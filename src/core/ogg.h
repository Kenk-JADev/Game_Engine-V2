#pragma once
// ============================================================================
//  OGG-Vorbis-Leser (Etappe 21, T2) — duenne Huelle um stb_vorbis.
//
//  Warum stb_vorbis? XP-Projekte liefern Musik als `Audio/BGM/*.ogg`. Der
//  Dekoder ist mitgeliefert (`src/core/stb_vorbis.cpp`, v1.25, public domain /
//  MIT) und braucht weder Windows noch externe Bibliotheken — damit laeuft er
//  im Linux-CI genauso wie im Spiel (Vorbild: `src/core/wav.h`).
//
//  Schnittstelle wie beim WAV-Leser: Bytes rein, `wav::Clip` (mono, 16 Bit,
//  `STANDARD_RATE`) raus. Alles darueber (Datei, Pitch, Stimmen) macht
//  `src/core/sound.cpp`; Fehlertexte sind deutsch und landen im Log.
//
//  Grenzen: nur EIN Vorbis-Strom (kein Chaining), max. 60 Sekunden wie beim
//  WAV-Leser. MP3 liest seit Etappe 22 `src/core/mp3.h` (minimp3).
//  Fehlerhafte oder abgeschnittene Daten liefern false
//  statt eines Absturzes.
// ============================================================================

// stb_vorbis hat keine eigene Kopfdatei: der obere Teil der .cpp ist die
// Deklaration (stb-Muster "HEADER_ONLY"), der Rest die Implementierung, die
// beim Bauen von src/core/stb_vorbis.cpp entsteht.
#define STB_VORBIS_HEADER_ONLY
#include "core/stb_vorbis.cpp"
#undef STB_VORBIS_HEADER_ONLY

#include "core/wav.h"

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace rune::core::ogg {

// Erkennt OGG-Vorbis an den ersten vier Bytes ("OggS").
inline bool ist_ogg(const unsigned char* bytes, size_t size) {
    return bytes && size >= 4 && bytes[0] == 'O' && bytes[1] == 'g' &&
           bytes[2] == 'g' && bytes[3] == 'S';
}

// OGG-Vorbis nach mono/16 Bit/STANDARD_RATE wandeln. `fehler` (optional)
// bekommt einen deutschen Text, wenn false zurueckkommt.
inline bool parse(const unsigned char* bytes, size_t size, wav::Clip& clip,
                  std::string* fehler = nullptr) {
    auto melde = [fehler](const char* text) {
        if (fehler) *fehler = text;
    };
    if (!bytes || size == 0) {
        melde("keine Daten");
        return false;
    }
    if (!ist_ogg(bytes, size)) {
        melde("kein OGG-Container (Kennung 'OggS' fehlt)");
        return false;
    }
    if (size > 0x7FFFFFFF) {
        melde("Datei zu gross");
        return false;
    }

    int fehler_code = 0;
    stb_vorbis* datei = stb_vorbis_open_memory(bytes, static_cast<int>(size),
                                               &fehler_code, nullptr);
    if (!datei) {
        melde(fehler_code == VORBIS_outofmem ? "Speicher voll"
                                            : "OGG-Daten nicht lesbar (kaputt oder kein Vorbis)");
        return false;
    }
    struct Schliesser {
        stb_vorbis* d;
        ~Schliesser() { stb_vorbis_close(d); }
    } schliesser{datei};

    const stb_vorbis_info info = stb_vorbis_get_info(datei);
    if (info.channels <= 0 || info.sample_rate <= 0) {
        melde("Kopfdaten ohne Kanaele oder Abtastrate");
        return false;
    }
    if (info.sample_rate > 192000) {
        melde("Abtastrate ueber 192 kHz");
        return false;
    }

    const double max_samples = wav::MAX_SEKUNDEN * info.sample_rate;
    std::vector<int16_t> puffer(static_cast<std::size_t>(info.channels) * 4096);
    std::vector<int16_t> mono;
    mono.reserve(static_cast<std::size_t>(max_samples + 1));

    for (;;) {
        const int rahmen = stb_vorbis_get_frame_short_interleaved(
            datei, info.channels, puffer.data(),
            static_cast<int>(puffer.size() / static_cast<std::size_t>(info.channels)));
        if (rahmen <= 0) break;
        for (int i = 0; i < rahmen; ++i) {
            int summe = 0;
            for (int k = 0; k < info.channels; ++k)
                summe += puffer[static_cast<std::size_t>(i) * info.channels + k];
            mono.push_back(static_cast<int16_t>(summe / info.channels));
            if (static_cast<double>(mono.size()) > max_samples) {
                melde("laenger als 60 Sekunden");
                return false;
            }
        }
    }
    if (mono.empty()) {
        melde("keine Abtastwerte dekodiert");
        return false;
    }

    wav::Clip roh;
    roh.sample_rate = info.sample_rate;
    roh.samples = std::move(mono);
    clip = wav::resample(roh, wav::STANDARD_RATE);
    if (clip.leer()) {
        melde("Umrechnung auf die Engine-Rate lieferte nichts");
        return false;
    }
    return true;
}

} // namespace rune::core::ogg
