#pragma once
// ============================================================================
//  WAV-Leser (Etappe 21, T1) — portabel, ohne Windows-Kopfzeilen.
//
//  Warum als eigene Kopfdatei? `src/core/sound.cpp` spricht WaveOut an und
//  lässt sich nur unter Windows bauen. Der Parser und die Abtastraten-
//  Umrechnung sind reine Rechnung und laufen deshalb auch im Linux-CI
//  (`tools/wav_test.cpp`).
//
//  Unterstützt: RIFF/WAVE, PCM (Format 1) und WAVE_FORMAT_EXTENSIBLE (0xFFFE)
//  mit 8, 16, 24 oder 32 Bit, mono oder stereo, 8–192 kHz.
//  Ergebnis ist immer mono, 16 Bit, Zielrate (Vorgabe 22050 wie WaveOut).
//
//  Grenzen: 60 Sekunden Zielpuffer je Datei (XP-BGM-Längen reichen), sonst
//  Fehler statt Absturz. Fehlertexte sind deutsch und landen im Log.
// ============================================================================

#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace rune::core::wav {

// Ein dekodierter Klang: mono, 16 Bit, `sample_rate` Abtastungen pro Sekunde.
struct Clip {
    int sample_rate = 0;
    std::vector<int16_t> samples;
    bool leer() const { return samples.empty(); }
};

constexpr int STANDARD_RATE = 22050;     // WaveOut-Format der Engine
constexpr double MAX_SEKUNDEN = 60.0;    // Obergrenze je Datei

namespace detail {

inline uint16_t lese_u16(const uint8_t* p) {
    return static_cast<uint16_t>(p[0] | (p[1] << 8));
}

inline uint32_t lese_u32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

// Ein Sample je Kanal auf -1..1 bringen (8 Bit ist vorzeichenlos).
inline double sample_wert(const uint8_t* p, int bits) {
    switch (bits) {
        case 8: return (static_cast<int>(p[0]) - 128) / 128.0;
        case 16: {
            const int16_t v = static_cast<int16_t>(lese_u16(p));
            return v / 32768.0;
        }
        case 24: {
            int32_t v = static_cast<int32_t>(p[0]) | (static_cast<int32_t>(p[1]) << 8) |
                        (static_cast<int32_t>(p[2]) << 16);
            if (v & 0x800000) v |= ~0xFFFFFF;   // Vorzeichen erweitern
            return v / 8388608.0;
        }
        case 32: {
            const int32_t v = static_cast<int32_t>(lese_u32(p));
            return v / 2147483648.0;
        }
        default: return 0.0;
    }
}

inline int16_t auf_i16(double wert) {
    const double skaliert = wert * 32767.0;
    if (skaliert > 32767.0) return 32767;
    if (skaliert < -32768.0) return -32768;
    return static_cast<int16_t>(std::lround(skaliert));
}

} // namespace detail

// Liest WAV-Bytes und mischt auf mono herunter (Zielrate bleibt die Datei-Rate).
// Rückgabe false bei unbekanntem Format/Grenzverletzung; `fehler` erklärt warum.
inline bool parse(const uint8_t* daten, std::size_t groesse, Clip& out, std::string* fehler = nullptr) {
    auto melde = [&](const char* text) {
        if (fehler) *fehler = text;
        return false;
    };
    out = Clip{};
    if (!daten || groesse < 12) return melde("WAV: Datei ist zu kurz");
    if (std::memcmp(daten, "RIFF", 4) != 0 || std::memcmp(daten + 8, "WAVE", 4) != 0)
        return melde("WAV: keine RIFF/WAVE-Datei");

    int format = 0;
    int kanaele = 0;
    int rate = 0;
    int bits = 0;
    const uint8_t* pcm = nullptr;
    std::size_t pcm_bytes = 0;

    std::size_t pos = 12;
    while (pos + 8 <= groesse) {
        const uint8_t* kopf = daten + pos;
        const std::size_t laenge = detail::lese_u32(kopf + 4);
        const uint8_t* inhalt = kopf + 8;
        if (pos + 8 + laenge > groesse) {
            // Abgeschnittene letzte Chunk-Daten: bei 'data' so viel nehmen, wie da ist.
            if (std::memcmp(kopf, "data", 4) == 0 && groesse > pos + 8) {
                pcm = inhalt;
                pcm_bytes = groesse - (pos + 8);
            }
            break;
        }
        if (std::memcmp(kopf, "fmt ", 4) == 0 && laenge >= 16) {
            format = detail::lese_u16(inhalt);
            kanaele = detail::lese_u16(inhalt + 2);
            rate = static_cast<int>(detail::lese_u32(inhalt + 4));
            bits = detail::lese_u16(inhalt + 14);
            if (format == 0xFFFE && laenge >= 26) {
                // WAVE_FORMAT_EXTENSIBLE: Unterformat-GUID beginnt bei 24;
                // die ersten zwei Bytes sind die eigentliche Format-Kennung.
                format = detail::lese_u16(inhalt + 24);
            }
        } else if (std::memcmp(kopf, "data", 4) == 0) {
            pcm = inhalt;
            pcm_bytes = laenge;
        }
        pos += 8 + laenge + (laenge & 1);    // Chunks stehen auf geraden Grenzen
    }

    if (format != 1) return melde("WAV: nur PCM wird unterstützt (kein IEEE-Float/ADPCM)");
    if (kanaele < 1 || kanaele > 2) return melde("WAV: nur mono oder stereo");
    if (rate < 8000 || rate > 192000) return melde("WAV: Abtastrate außerhalb 8–192 kHz");
    if (bits != 8 && bits != 16 && bits != 24 && bits != 32)
        return melde("WAV: nur 8/16/24/32 Bit");
    if (!pcm || pcm_bytes == 0) return melde("WAV: keine Audiodaten (data-Chunk fehlt)");

    const std::size_t rahmen = static_cast<std::size_t>(kanaele) * static_cast<std::size_t>(bits / 8);
    if (rahmen == 0) return melde("WAV: ungültige Rahmenlänge");
    const std::size_t anzahl = pcm_bytes / rahmen;
    if (anzahl == 0) return melde("WAV: keine vollständigen Rahmen");
    if (anzahl > static_cast<std::size_t>(MAX_SEKUNDEN * rate) + 1)
        return melde("WAV: länger als 60 Sekunden");

    out.sample_rate = rate;
    out.samples.reserve(anzahl);
    for (std::size_t i = 0; i < anzahl; ++i) {
        const uint8_t* rahmen_start = pcm + i * rahmen;
        double summe = 0.0;
        for (int k = 0; k < kanaele; ++k)
            summe += detail::sample_wert(rahmen_start + static_cast<std::size_t>(k) * (bits / 8), bits);
        out.samples.push_back(detail::auf_i16(summe / kanaele));
    }
    return true;
}

// Lineare Umrechnung auf eine Zielrate (WaveOut läuft mit 22050 Hz).
inline Clip resample(const Clip& quelle, int ziel_rate) {
    Clip out;
    out.sample_rate = ziel_rate;
    if (quelle.samples.empty() || ziel_rate <= 0 || quelle.sample_rate <= 0) return out;

    if (quelle.sample_rate == ziel_rate) {
        out.samples = quelle.samples;
        return out;
    }

    const double verhaeltnis = static_cast<double>(quelle.sample_rate) / ziel_rate;
    const std::size_t laenge = static_cast<std::size_t>(
        std::llround(quelle.samples.size() / verhaeltnis));
    out.samples.reserve(laenge);
    for (std::size_t i = 0; i < laenge; ++i) {
        const double pos = i * verhaeltnis;
        const std::size_t links = static_cast<std::size_t>(pos);
        const std::size_t rechts = links + 1 < quelle.samples.size() ? links + 1 : links;
        const double rest = pos - static_cast<double>(links);
        const double wert = quelle.samples[links] * (1.0 - rest) + quelle.samples[rechts] * rest;
        out.samples.push_back(detail::auf_i16(wert / 32768.0));
    }
    return out;
}

// Pitch in Prozent (XP: 50–150) als Zielrate für `resample`.
inline int rate_fuer_pitch(int rate, int pitch_prozent) {
    const int p = pitch_prozent < 50 ? 50 : (pitch_prozent > 150 ? 150 : pitch_prozent);
    return static_cast<int>(rate * (100.0 / p));
}

} // namespace rune::core::wav
