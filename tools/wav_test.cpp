// ============================================================================
//  WAV-Leser (Etappe 21, T1) — portabler Test ohne Windows.
//
//     g++ -std=c++20 -O2 -Wall -Wextra -Werror -Isrc tools/wav_test.cpp -o wav_test
//     ./wav_test
//
//  Die Testdateien sind als Base64 eingebettet (kein Binärblob im Baum):
//    - PCM 16 Bit stereo 22050 Hz, 8 Rahmen
//    - PCM 8 Bit mono 8000 Hz, 1 Rahmen
//    - PCM 24 Bit mono 44100 Hz, 4 Rahmen
//    - WAVE_FORMAT_EXTENSIBLE 16 Bit stereo 48000 Hz, 4 Rahmen
//  Dazu Fehlerfälle (kein RIFF, Float-Format, ADPCM, abgeschnitten) und die
//  Umrechnung auf die WaveOut-Rate inklusive Pitch.
// ============================================================================

#include <cmath>
#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>

#include "core/wav.h"

namespace {

int checks = 0;
int failures = 0;

void check(const char* name, bool ok) {
    ++checks;
    if (!ok) ++failures;
    std::printf("  %s %s\n", ok ? "ok  " : "FAIL", name);
}

std::vector<std::uint8_t> base64(const char* text) {
    static const char* tabelle =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::vector<std::uint8_t> out;
    int puffer = 0;
    int bits = 0;
    for (const char* p = text; *p; ++p) {
        if (*p == '=') break;
        if (*p == '\n' || *p == '\r' || *p == ' ') continue;
        const char* stelle = std::strchr(tabelle, *p);
        if (!stelle) continue;
        puffer = (puffer << 6) | static_cast<int>(stelle - tabelle);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<std::uint8_t>((puffer >> bits) & 0xFF));
        }
    }
    return out;
}

const char* PCM16_STEREO =
    "UklGRkQAAABXQVZFZm10IBAAAAABAAIAIlYAAIhYAQAEABAAZGF0YSAAAABI9Kz0uAscDEj0rPS4CxwM"
    "SPSs9LgLHAxI9Kz0uAscDA==";
const char* PCM8_MONO =
    "UklGRiwAAABXQVZFZm10IBAAAAABAAEAQB8AAEAfAAABAAgAZGF0YQgAAAB0i3SLdIt0iw==";
const char* PCM24_MONO =
    "UklGRjAAAABXQVZFZm10IBAAAAABAAEARKwAAMwEAgADABgAZGF0YQwAAAAASPQAuAsASPQAuAs=";
const char* EXT16_STEREO =
    "UklGRkwAAABXQVZFZm10ICgAAAD+/wIAgLsAAADuAgAEABAAFgAQAAAAAAABAAAAAAAQAIAAAKoAOJtx"
    "ZGF0YRAAAABI9Kz0uAscDEj0rPS4CxwM";

} // namespace

int main() {
    using namespace rune::core::wav;
    std::puts("== WAV-Leser: Formate ==");

    {
        Clip clip;
        std::string fehler;
        const bool ok = parse(base64(PCM16_STEREO).data(), base64(PCM16_STEREO).size(), clip, &fehler);
        check("PCM 16 Bit stereo gelesen", ok && clip.sample_rate == 22050 && clip.samples.size() == 8);
        check("Stereo wird gemittelt (erster Rahmen -3000/-2900 -> -2950)", !clip.samples.empty() &&
              std::abs(clip.samples.front() - (-2950)) <= 1);
    }
    {
        const auto bytes = base64(PCM8_MONO);
        Clip clip;
        check("PCM 8 Bit mono gelesen", parse(bytes.data(), bytes.size(), clip) &&
              clip.sample_rate == 8000 && clip.samples.size() == 8);
        // 8 Bit ist vorzeichenlos: 0x74 = 116 -> (116-128)/128 -> etwa -3072.
        check("8 Bit wird vorzeichenrichtig (0x74 -> rund -3072)", !clip.samples.empty() &&
              std::abs(clip.samples.front() - (-3072)) <= 40);
    }
    {
        const auto bytes = base64(PCM24_MONO);
        Clip clip;
        check("PCM 24 Bit mono gelesen", parse(bytes.data(), bytes.size(), clip) &&
              clip.sample_rate == 44100 && clip.samples.size() == 4);
    }
    {
        const auto bytes = base64(EXT16_STEREO);
        Clip clip;
        check("WAVE_FORMAT_EXTENSIBLE wird als PCM erkannt", parse(bytes.data(), bytes.size(), clip) &&
              clip.sample_rate == 48000 && clip.samples.size() == 4);
    }

    std::puts("== WAV-Leser: Fehlerfälle ==");
    {
        Clip clip;
        std::string fehler;
        const auto bytes = base64(PCM16_STEREO);
        check("kein RIFF -> Fehler", !parse(nullptr, 0, clip, &fehler) && !fehler.empty());
        check("kaputter Kopf -> Fehler", !parse(bytes.data(), 12, clip, &fehler));
        std::vector<std::uint8_t> falsch(bytes.begin(), bytes.end());
        falsch[8] = 'X';    // "WAVE" zerstören
        check("falsche Wellen-Kennung -> Fehler", !parse(falsch.data(), falsch.size(), clip, &fehler));
        std::vector<std::uint8_t> float_format(bytes.begin(), bytes.end());
        float_format[20] = 3;    // IEEE-Float
        check("IEEE-Float (Format 3) -> Fehler", !parse(float_format.data(), float_format.size(), clip, &fehler));
        check("abgeschnittene data -> so viel wie da ist", parse(bytes.data(), bytes.size() - 4, clip));
    }

    std::puts("== Umrechnung ==");
    {
        // 100 Rahmen bei 1000 Hz auf 500 Hz -> 50 Rahmen (Seitenverhältnis 2:1).
        Clip quelle;
        quelle.sample_rate = 1000;
        for (int i = 0; i < 100; ++i) quelle.samples.push_back(static_cast<int16_t>(i * 100));
        const Clip ziel = resample(quelle, 500);
        check("Halbierte Rate halbiert die Rahmenzahl", ziel.samples.size() == 50 &&
              ziel.sample_rate == 500);
        check("gleiche Rate bleibt unverändert", resample(quelle, 1000).samples == quelle.samples);
        check("leerer Klang bleibt leer", resample(Clip{}, 22050).leer());
    }
    {
        check("Pitch 100 lässt die Rate gleich", rate_fuer_pitch(22050, 100) == 22050);
        check("Pitch 50 halbiert die Rate (doppelt so hoch)", rate_fuer_pitch(22050, 50) == 44100);
        check("Pitch 150 hebt die Rate (tiefer)", rate_fuer_pitch(22050, 150) == 14700);
        check("Pitch wird auf 50..150 geklemmt", rate_fuer_pitch(22050, 10) == 44100 &&
              rate_fuer_pitch(22050, 900) == 14700);
    }

    std::printf("\nWAV-TEST: %d Prüfungen, %d Fehler\n", checks, failures);
    return failures ? 1 : 0;
}
