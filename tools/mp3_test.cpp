#include "core/mp3.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

// ============================================================================
//  Portabler Test fuer den MP3-Leser (Etappe 22, T4).
//     g++ -std=c++20 -O2 -I src src/core/minimp3.cpp tools/mp3_test.cpp
//
//  Die Fixture ist mit lameenc (LAME, 128 kbit/s) erzeugt: 0,25 s, 44,1 kHz,
//  STEREO mit Sinus links (440 Hz, Amplitude 0,5) und Stille rechts. Damit
//  prueft der Test vier Dinge auf einmal: Dekodieren, Stereo->Mono mischen
//  (Mittelwert), Umrechnen auf die Engine-Rate 22050 und das Erkennen des
//  Formats. Dazu kommen ID3v2-Kopf, Muell, leere Daten und abgeschnittene
//  Dateien - alles ohne Absturz.
// ============================================================================

namespace wav = rune::core::wav;

static int fehler = 0;
static void check(const char* name, bool ok) {
    std::printf("%s  %s\n", ok ? "  ok  " : "  FAIL", name);
    if (!ok) ++fehler;
}

static std::vector<unsigned char> aus_base64(const std::string& text) {
    auto wert = [](char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+') return 62;
        if (c == '/') return 63;
        return -1;
    };
    std::vector<unsigned char> out;
    int puffer = 0, bits = 0;
    for (char c : text) {
        const int v = wert(c);
        if (v < 0) continue;
        puffer = (puffer << 6) | v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<unsigned char>((puffer >> bits) & 0xFF));
        }
    }
    return out;
}

static const char* const MP3_BASE64 =
    "//uQRAAAAe0IVL08IAgAAA0goAABC8ytvfmm0FAAADSDAAAAACHfbJcGrE3IWtm4DYAWAIAsCFsavV6vfvwfBAEA"
    "QOVg/wQBDWD78oGJT38p5/lPP9Hv6PB99YEDGgH+j3hMBLv/KAKAKAYDAYDAcDgRIhHetrjSk+cmOerR563RtXDQ"
    "uv1sJqCu+CsifCZfiXEiPUcP+O4YYS4kR6/+OEyHsPYxLv/5dMi8XkS6Xf6gaEoSBr/iUJA0JQke/8sVMCEA2zAj"
    "ANswfIJBMBiB+DA/QrI4H/51NmUPIzH5hRcw9YLFMHTCTDCFQggwkEG8MDDAsERlvpgxWVCRFy6fR9v3////7///"
    "////+hAAA7bfa2xyO2gEXeWcMFw3NMpjOVjcwwAi46w67EbwA3PAgJZMWXOL3QVfa+7RdX/f99Q6vdR+z/dq7hP3"
    "XK3f/1pMQU1FNC4wqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqq"
    "qqqqqqqqqqqqqqqqqqqqqqqqqqqq//uSRHCA8mIJxAd8QAAAAA0g4AABCEwpQa7xhLAAADSAAAAEqqqqqqqqqqqq"
    "qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqq"
    "qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqDAADAjwJ4wJwDTMDVBSjBZgn0wzcemNV47UjU6R3owvQ"
    "IlNfpIykZTaq0OIWc1wazE4AStcaW+pl/p/9/nqdAwz//////V//9n///eI7ttY201BDGizWLjy9zEGGcNynOo2/"
    "BnjELCJPhUNg6NSzNRmMgQZfSP/GKcHKt7ZqNDnl+r+z7vHWu/p/7/7f+//+qkxBTUU0LjCqqqqqqqqqqqqqqqqq"
    "qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqq"
    "qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqv/7kkSJCPJjGkST/BGwAAANIAAAAQiE"
    "JSete0JgAAA0gAAABKqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqq"
    "qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqoh"
    "ADEneG//+suIKJMPkBSBgZM33s9RdMItqsZxVhmDTvXVnlHba5fop4bRoW2caxkCs+x7Pu96v/9mn/T94A62mUUD"
    "DYHMuFA1usTrV2MMtElTU4I0U1GESbMM2BqDu8lNft01IyDUNfMwmcBBRib+SukBOvdn4zTq66fU3/q/1//u/s//"
    "/+tMQU1FNC4wqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqq"
    "qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqr/"
    "+5JEhADx6wjS+fzQjAAADSAAAAEJNCUW7n+CQAAANIAAAASqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqq"
    "qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqq"
    "qqqqqqqqqqqqqqqqqqqqqqqqqqqqAAIFuuusbbjWlyo3AiJguNRpd9hyczGEAWhKYiuov6021x+p3xarYMUIsdn0"
    "Kd9Fn/9VltHyX/+v+/u//6DEgFDLkjzU5LDmebzFgh3A49vjQOMAHfjFkwsk6Ofo1QYg0hW00Um4ysMQwjBNAIw9"
    "2H/p7Dx2/7/X1Uf///9DP9/9nb13//1VTEFNRTQuMFVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV"
    "VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV"
    "VVVVVVVVVVVVVVVVVVVVVVVVVVVV//uSRIQA8e4JT2sd4IwAAA0gAAABCRQpDg7/okAAADSAAAAEVVVVVVVVVVVV"
    "VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV"
    "VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVTIgoTI83DLxXzTmpjCziCE0uH7tNJeH9zChwlgz8wTOY8N1"
    "Jc5LODYpaAxVSSdWRKb1/KfM23zv///////////6DGEDzNwmjYlRzs6pjGZyBc6Y72pOiRIKzGdAxI7xj01/cc1A"
    "ag0xtUy/OkwoBouWuxrb+Ri3e9ecZxw31ddv/+6rbu8a2/W/dqrz/Z7nf7pnWkxBTUU0LjCqqqqqqqqqqqqqqqqq"
    "qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqq"
    "qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqv/7kkSKD/IKCMSDv+CYAAANIAAAAQos"
    "LQwO/6JAAAA0gAAABKqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqq"
    "qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqhgAAyHK"
    "AyKOEyzWQ0orMwqkhENHw/+DRdSBwwl4JnM0s4z4LzeaKOVSc2QVQUVkkXWkInZ1I8iz0O//6fvX////////+lAA"
    "A7a7a2NuOYXTWIBgDMJAwNh4oNfAwMJQBAEAhAk4DwJI/wSs5E+BiH09dEr2jolX3wd9X49//j93q77/36vR2/f/"
    "/VVMQU1FNC4wVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV"
    "VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVX/"
    "+5JEhgDyKgjEk7/gkAAADSAAAAEIpGk/rrxG8AAANIAAAARVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV"
    "VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV"
    "VVVgDHAizHEwjJBOzPGZDCMB10zuPkFM56HOTB7QiQyAvzOYWNrG04O8DV5CCCKn08s8KN6/qR4Z+4iBCzBEAJQw"
    "dUFNMNQCtjHhyEU8Mr7bO+fIYzHjg0A9ln02rg01UfQ051kzBRUwoCIEgCrYyt9JZb511fgqf2jNmM8zb1O06HIy"
    "UgxVNLJKsq3+js2INarWtcgV+mhiu2YrTEFNRTQuMFVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV"
    "VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV"
    "VVVVVVVVVVVVVVVVVVVVVVVVVVVV//uSRI2N8cIIxQu/4JQAAA0gAAABDDhTCg/0RwAAADSAAAAEVVVVVVVVVVVV"
    "VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV"
    "VVVVVVVVVVVVVVVVVVVVVVVVVVVVLAADG8ijGwxTIlPjOCbzCDR3gzf7nOM2dHVDByAjExSyDPQHNtmI4U4TWQ9C"
    "CUpF5pCLivV9hmXW4k7//////YBkdNFyDN3E0PW6LMegHwjxb+vI8D4fiMelDHT2acDaKDjUd+zSbYjLJGDCUJQY"
    "ACt7KH3jdi/z9c//r/IsRzrHWKftr+nbnasxoio2jb07WaZHK02tT/2KN7KIukxBTUU0LjCqqqqqqqqqqqqqqqqq"
    "qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqoALAGNhFmNJjGPyhmaM7GDzDyJmifUOZloO3mDQhHhgxqhkGNwFk4i"
    "zjWo2DCSpJ5Z7Qr4bf6fVd0MAAS77faREAyNIsx9JEOB8wqF4xyOw/O0s//7kkSPDfIDCMUTv+CQAAANIAAAAQuI"
    "OwoO/6JAAAA0gAAABN9XMy9H0xNAcwpEcw/DMwvCsxBG0w2FwxWEYxiC5IUcAY11rNpWzSTsycFMSXjmMA9rYO4a"
    "AScnCRhvy4JKhh5GaujjyOYwaGbCxg5qb1CHzXxxCgYYKmgs5vsOcO8mwl4INjPT8zMZLrGICgKHBUNMfJzHwcCD"
    "RpaeaSamOgIwBGHCxiwgYYDF6IBQcbUveWjh5DAwAAL0JbmGBxhQErxgaK664v8rYYqRiECPGl+gHV2zAAgJdt82"
    "sMQk0NxeUM7a/D9qIOw5EUm3IdyrG6e5GJZK2cNcsbp6enz1nnrDmdPbqRuX38KSxh////+GHN0gYL4IVh8u/h//"
    "DFVMQU1FNC4wVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV"
    "VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVX/"
    "+5JE6IDxywlFK7/gkAAADSAAAAEiuTU1td2AMAAANIKAAARVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV"
    "VVVVVVVVVVVVVVUAACwWiwWi0WCwRhgIAx2DkznX5Spi3+arlUYgiMnK0n/NL45NpGIFhlMCQC/wN7CsDlZ2AwiK"
    "gCgfga0MYGoGGBFKBIADlFfgZDQIGHRCDmKDk0IVC10XIGO+BkMhAYSH4CBwCYeAxGBRZpOFsZYp/AxIBAMOBEDC"
    "YWAWCoAgVAwECCOJpMwRV+Bg0DgNA4AIKg2DgAQUCEDgCAQomKB3Mv40A+wn0MiiMhCMdYtgt6quuv/jnEBJQjiL"
    "kEJ0ixMkNJj///y+5qkZHTBA+eMTI7EVTEFNRTQuMFVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV"
    "VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV"
    "VVVVVVVVVVVVVVVVVVVVVVVVVVVV//uSRKoABpJWUO52oAYAAA0gwAAAAAABpBwAACAAADSDgAAEVVVVVVVVVVVV"
    "VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV"
    "VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV"
    "VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV"
    "VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV"
    "VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV"
    "VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVQ==";

static double spitze(const std::vector<int16_t>& samples) {
    double max = 0.0;
    for (int16_t s : samples) {
        const double a = std::fabs(static_cast<double>(s));
        if (a > max) max = a;
    }
    return max;
}

int main() {
    namespace mp3 = rune::core::mp3;
    const std::vector<unsigned char> datei = aus_base64(MP3_BASE64);

    check("Fixture entpackt (> 1000 Bytes)", datei.size() > 1000);
    check("ist_mp3 erkennt die Fixture", mp3::ist_mp3(datei.data(), datei.size()));

    const std::vector<unsigned char> id3kopf = {'I', 'D', '3', 3, 0, 0, 0, 0, 0x02, 0x00};
    check("ist_mp3 erkennt ID3v2-Kennung", mp3::ist_mp3(id3kopf.data(), id3kopf.size()));

    const std::vector<unsigned char> wavkopf = {'R', 'I', 'F', 'F', 0x24, 0, 0, 0, 'W', 'A', 'V', 'E'};
    check("ist_mp3 lehnt WAV-Kopf ab", !mp3::ist_mp3(wavkopf.data(), wavkopf.size()));

    const std::vector<unsigned char> nullen(4000, 0x00);
    check("ist_mp3 lehnt Nullbytes ab", !mp3::ist_mp3(nullen.data(), nullen.size()));
    check("ist_mp3 lehnt kurze Daten ab", !mp3::ist_mp3(datei.data(), 3));

    wav::Clip clip;
    std::string meldung;
    check("parse dekodiert die Fixture", mp3::parse(datei.data(), datei.size(), clip, &meldung));
    check("Engine-Rate 22050", clip.sample_rate == wav::STANDARD_RATE);
    const double dauer = static_cast<double>(clip.samples.size()) / wav::STANDARD_RATE;
    // MP3 hat Encoder-Vorlauf/-Nachlauf: nicht exakt 0,25 s.
    check("Dauer ~0,25 s (0,20 .. 0,40)", dauer > 0.20 && dauer < 0.40);

    // Links 0,5, rechts Stille -> Mono-Mittel 0,25 -> ca. 8192 von 32767.
    const double peak = spitze(clip.samples);
    check("Amplitude ~25 % (Stereo->Mono Mittelwert)", peak > 5000.0 && peak < 11000.0);

    // ID3v2-Kopf (256 Byte Nutzlast) davorsetzen: muss uebersprungen werden.
    std::vector<unsigned char> mit_id3 = id3kopf;
    mit_id3.resize(10 + 256, 0x00);
    mit_id3.insert(mit_id3.end(), datei.begin(), datei.end());
    wav::Clip clip_id3;
    std::string meldung_id3;
    const bool ok_id3 = mp3::parse(mit_id3.data(), mit_id3.size(), clip_id3, &meldung_id3);
    check("ID3v2-Kopf wird uebersprungen", ok_id3 && clip_id3.samples.size() == clip.samples.size());

    check("leere Daten -> false", !mp3::parse(nullptr, 0, clip, &meldung));
    meldung.clear();
    wav::Clip muell_clip;
    check("Muell -> false mit deutschem Grund",
          !mp3::parse(nullen.data(), nullen.size(), muell_clip, &meldung) && !meldung.empty());

    // Abgeschnitten: darf nicht abstuerzen, notfalls weniger/keine Samples.
    std::vector<unsigned char> halb(datei.begin(), datei.begin() + static_cast<long>(datei.size() / 2));
    wav::Clip clip_halb;
    std::string meldung_halb;
    const bool ok_halb = mp3::parse(halb.data(), halb.size(), clip_halb, &meldung_halb);
    check("abgeschnittene Datei: kein Absturz, hoechstens so lang",
          ok_halb ? clip_halb.samples.size() <= clip.samples.size() : !meldung_halb.empty());

    // Nur ein ID3-Kopf ohne Rahmen -> keine Abtastwerte.
    std::vector<unsigned char> nur_kopf(id3kopf.begin(), id3kopf.end());
    wav::Clip kopf_clip;
    check("nur ID3-Kopf -> false", !mp3::parse(nur_kopf.data(), nur_kopf.size(), kopf_clip, &meldung));

    if (fehler == 0) {
        std::printf("MP3-TEST OK (0 Fehler)\n");
    } else {
        std::printf("MP3-TEST FEHLGESCHLAGEN (%d Fehler)\n", fehler);
    }
    return fehler == 0 ? 0 : 1;
}
