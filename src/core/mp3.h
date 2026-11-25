#pragma once
// ============================================================================
//  MP3-Leser (Etappe 22, T4) — duenne Huelle um minimp3.
//
//  Warum minimp3? XP-Projekte importieren Musik oft als MP3. Bis Etappe 21
//  warnte die Engine nur ("MP3 wird nicht unterstuetzt"). Der Dekoder ist
//  mitgeliefert (`src/core/minimp3.h` + `.cpp`, CC0 / public domain), braucht
//  weder Windows noch externe Bibliotheken — er laeuft im Linux-CI genauso
//  wie im Spiel (Vorbild: `src/core/ogg.h`).
//
//  Schnittstelle wie beim WAV-/OGG-Leser: Bytes rein, `wav::Clip` (mono,
//  16 Bit, `STANDARD_RATE`) raus. Alles darueber (Datei, Pitch, Stimmen)
//  macht `src/core/sound.cpp`; Fehlertexte sind deutsch und landen im Log.
//
//  Grenzen: ID3v2 am Anfang wird uebersprungen, ID3v1 am Ende ignoriert;
//  max. 60 Sekunden wie bei WAV/OGG (XP-Musik ist laenger, laeuft hier aber
//  ohnehin in Schleife - gekuerzt wird nur der Puffer). Kaputte oder
//  abgeschnittene Daten liefern false statt eines Absturzes.
// ============================================================================

#include "core/minimp3.h"
#include "core/wav.h"

#include <cstdint>
#include <string>
#include <vector>

namespace rune::core::mp3 {

// Erkennt MP3 an einer ID3v2-Kennung ("ID3") oder am Frame-Sync (11 Einsen,
// danach eine gueltige Version/Layer-Kombination). Eine blosse 0xFF reicht
// nicht - sonst wuerde jeder Zufallspuffer als MP3 gelten.
inline bool ist_mp3(const unsigned char* bytes, size_t size) {
    if (!bytes || size < 4) return false;
    if (bytes[0] == 'I' && bytes[1] == 'D' && bytes[2] == '3') return true;
    // Frame-Sync: 0xFF 0xEx; Version != reserviert (01), Layer != reserviert (00).
    if (bytes[0] != 0xFF || (bytes[1] & 0xE0) != 0xE0) return false;
    const int version = (bytes[1] >> 3) & 0x03;
    const int layer = (bytes[1] >> 1) & 0x03;
    return version != 1 && layer != 0;
}

// ID3v2-Kopf ueberspringen (10 Byte Kopf + Groesse, ggf. Fusszeile).
inline size_t id3_laenge(const unsigned char* bytes, size_t size) {
    if (size < 10 || bytes[0] != 'I' || bytes[1] != 'D' || bytes[2] != '3') return 0;
    const size_t groesse = (static_cast<size_t>(bytes[6] & 0x7F) << 21) |
                           (static_cast<size_t>(bytes[7] & 0x7F) << 14) |
                           (static_cast<size_t>(bytes[8] & 0x7F) << 7) |
                           static_cast<size_t>(bytes[9] & 0x7F);
    const bool fusszeile = (bytes[5] & 0x10) != 0;
    const size_t gesamt = 10 + groesse + (fusszeile ? 10 : 0);
    return gesamt <= size ? gesamt : 0;
}

// MP3 nach mono/16 Bit/STANDARD_RATE wandeln. `fehler` (optional) bekommt
// einen deutschen Text, wenn false zurueckkommt.
inline bool parse(const unsigned char* bytes, size_t size, wav::Clip& clip,
                  std::string* fehler = nullptr) {
    auto melde = [fehler](const char* text) {
        if (fehler) *fehler = text;
    };
    if (!bytes || size == 0) {
        melde("keine Daten");
        return false;
    }
    if (!ist_mp3(bytes, size)) {
        melde("kein MP3 (ID3-Kennung oder Frame-Sync fehlt)");
        return false;
    }
    if (size > 0x7FFFFFFF) {
        melde("Datei zu gross");
        return false;
    }

    mp3dec_t dekoder;
    mp3dec_init(&dekoder);
    mp3dec_frame_info_t info{};
    std::vector<int16_t> pcm(MINIMP3_MAX_SAMPLES_PER_FRAME);
    std::vector<int16_t> mono;

    size_t offset = id3_laenge(bytes, size);
    int kanaele = 0;
    int rate = 0;
    bool rahmen_gesehen = false;
    while (offset + 4 <= size) {
        const int abtastwerte = mp3dec_decode_frame(
            &dekoder, bytes + offset, static_cast<int>(size - offset), pcm.data(), &info);
        if (info.frame_bytes <= 0) break;   // kein Sync mehr -> Ende
        offset += static_cast<size_t>(info.frame_bytes);
        if (abtastwerte <= 0) {
            if (info.frame_bytes > 0) continue;   // Rahmen ohne Ton (Resync)
            break;
        }
        rahmen_gesehen = true;
        if (info.channels <= 0 || info.hz <= 0) {
            melde("Kopfdaten ohne Kanaele oder Abtastrate");
            return false;
        }
        if (info.hz > 192000) {
            melde("Abtastrate ueber 192 kHz");
            return false;
        }
        kanaele = info.channels;
        rate = info.hz;
        const double max_samples = wav::MAX_SEKUNDEN * rate;
        for (int i = 0; i < abtastwerte; ++i) {
            int summe = 0;
            for (int k = 0; k < kanaele; ++k)
                summe += pcm[static_cast<std::size_t>(i) * kanaele + k];
            mono.push_back(static_cast<int16_t>(summe / kanaele));
            if (static_cast<double>(mono.size()) > max_samples) {
                melde("laenger als 60 Sekunden");
                return false;
            }
        }
    }
    if (!rahmen_gesehen || mono.empty()) {
        melde("keine Abtastwerte dekodiert");
        return false;
    }

    wav::Clip roh;
    roh.sample_rate = rate;
    roh.samples = std::move(mono);
    clip = wav::resample(roh, wav::STANDARD_RATE);
    if (clip.leer()) {
        melde("Umrechnung auf die Engine-Rate lieferte nichts");
        return false;
    }
    return true;
}

} // namespace rune::core::mp3
