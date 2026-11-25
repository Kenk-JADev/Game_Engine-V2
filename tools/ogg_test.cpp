#include "core/ogg.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

// ============================================================================
//  Portabler Test fuer den OGG-Vorbis-Leser (Etappe 21, T2).
//     g++ -std=c++20 -O2 -I src src/core/stb_vorbis.cpp tools/ogg_test.cpp
//
//  Die Fixture ist mit libsndfile (soundfile) erzeugt: 0,25 s, 44,1 kHz,
//  STEREO mit Sinus links (440 Hz, Amplitude 0,5) und Stille rechts. Damit
//  prueft der Test drei Dinge auf einmal: Dekodieren, Stereo->Mono mischen
//  (Mittelwert) und Umrechnen auf die Engine-Rate 22050.
// ============================================================================

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

static const char* const OGG_BASE64 =
    "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"
    "AEDSNMDyNEDzPAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAJE0DNM8DNM8DAAAAAAAAAM3zAE8UAU8UAQAAAAAAAMDyPMAT"
    "PcATRQAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
    "AAAAAAAAAAAAAAAAAAAAHE0DNM8DNM8DAAAAAAAAAMvzAE8UAc8TAQAAAAAAAEDzPMATRcATRQAA"
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAEAAAEOAAABFkKhISsC"
    "gDgBAIckQZIgSdA0gGRZ0DRoGkwTIFkWNA2aBtMEAAAAAAAAAAAAQPI0aBo0DaIIkDQPmgZNgygC"
    "AAAAAAAAAAAAIGkaNA2aBlEESJoGTYOmQRQBAAAAAAAAAAAA0EwToghRhGkCPNOEKEIUYZoAAAAA"
    "AAAAAAAAAAAAAAAAAAAAAAAAgAAAgAEHAIAAE8pAoSErAoA4AQCHolgWAAA4kmNZAADgOJJlAQCA"
    "ZVmiCAAAlqWJIgAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
    "AAAAAAAAAAAAAAAAAAAAAACAAACAAQcAgAATykChISsBgCgAAIeiWBZwHMsCjmNZQJIsC2BZAM0D"
    "aBpAFAGAAACAAgcAgAAbNCUWByg0ZCUAEAUA4FAUy9I0UeQ4lqVposiRLEvTRJFlaZrnmSY0zfNM"
    "EaLneaYJz/M804RpiqKqAlE0TQEAAAUOAAABNmhKLA5QaMhKACAkAMDhOJbleaLoeaJomqrKcSzL"
    "80RRFE1TVVWV42iW54miKJqmqqoqy9I0zxNFUTRNVVVdaJrniaIomqaqui48z/NEURRNU1VdF57n"
    "eaIoiqapqq4LURRF0zRNVVVV1wWiaJqmqaqq6rpAFEXTNFVVVV0XiKIomqaqqq7rAtM0TVVVVdeV"
    "XYBpqqqquq7rAlRVVV3XdWUZoKqq6rquK8sA13Vd15VlWQbguq7ryrIsAADgwAEAIMAIOsmosggb"
    "TbjwABQasiIAiAIAAIxhSjGlDGMSQgqhYUxCSCFkUlIqKaUKQiollVJBSKWkUjJKLaWWUgUhlZJK"
    "qSCkUlIpBQCAHTgAgB1YCIWGrAQA8gAACGOUYowx5yRCSjHmnHMSIaUYc845qRRjzjnnnJSSMeec"
    "c05K6ZhzzjknpWTMOeeck1I655xzzkkppXTOOeeklFJC6Bx0UkopnXMOQgEAQAUOAAABNopsTjAS"
    "VGjISgAgFQDA4DiWpWmeJ4qmaUmSpnme54mmqmqSpGmeJ4qmqao8z/NEURRNU1V5nueJoiiapqpy"
    "XVEURdM0TVUly6JoiqapqqoL0zRN01RV14VpmqZpqqrrwrZVVVVd13Vh26qqqq7rysB1Xdd1ZRnI"
    "ruu6riwLAABPcAAAKrBhdYSTorHAQkNWAgAZAACEMQgphBBSyCCkEEJIKYWQAACAAQcAgAATykCh"
    "ISsBgFQAAIAQa6211lprDWPWWmuttdYS56y11lprrbXWWmuttdZaa6211lprrbXWWmuttdZaa621"
    "1lprrbXWWmuttdZaa6211lprrbXWWmuttdZaa6211lprrbXWWmuttdZaa6211lprrbXWWmuttVYA"
    "IHaFA8BOhA2rI5wUjQUWGrISAAgHAACMQYgx6CSUUkqFEGPQSUiltRgrhBiDUEpKrbWYPOcchFJa"
    "ai3G5DnnIKTUWowxJtdCSCmllmKLsbgWQioptdZirMkYlVJqLbYYa+3FqJRKSzHGGGswxubUWowx"
    "1lqLMTq3EkuMMcZahBHGxRZjrLXXIowRssXSWq21BmOMsbm12GrNuRgjjK4ttVZrzQUAmDw4AEAl"
    "2DjDStJZ4WhwoSErAYDcAAACIaUYY8w555xzDkIIqVKMOecchBBCCKGUUlKlGHPOOQghhFBCKaWk"
    "jDHmHIQQQgillFJKaSllzDkIIYRQSimllNJS65xzEEIIpZRSSiklpdQ55yCEUEoppZRSSkothBBC"
    "KKGUUkoppZSUUkohhFBKKaWUUkopqaWUQgillFJKKaWUUlJKKYUQQimllFJKKaWklForpZRSSiml"
    "lFJKSS21lFIopZRSSimllJJaSimlUkoppZRSSiklpdRSSqWUUkoppZRSSkuppZRKKaWUUkoppZSU"
    "UkoppVRKKaWUUkopKaXUWkoppZRKKaWUUlprKaWWUiqllFJKKaW01FprLbWUSimllFJKaa21lFJK"
    "KZVSSimllFIAANCBAwBAgBGVFmKnGVcegSMKGSagQkNWAgBkAAAMo5RSSS1FgiKlGKSWQiUVc1BS"
    "iihzDlKsqULOIOYklYoxhJSDVDIHlVLMQQohZUwpBq2VGDrGmKOYaiqhYwwAAABBAACBkAkECqDA"
    "QAYAHCAkSAEAhQWGDhEiQIwCA+Pi0gYAIAiRGSIRsRgkJlQDRcV0ALC4wJAPABkaG2kXF9BlgAu6"
    "uOtACEEIQhCLAyggAQcn3PDEG55wgxN0ikodCAAAAACAAwA8AAAkG0BERDRzHB0eHyAhIiMkJSYn"
    "KAIAAAAA4AYAHwAASQoQERHNHEeHxwdIiMgISYnJCUoAACCAAAAAAAAIIAABAQEAAAAAgAAAAAAB"
    "AU9nZ1MABBErAAAAAAAAtHb7KgIAAAAn8Cq0DSFWLjEwMDEvLy8ylM/8or++/yAl6bL+4RdgMpAA"
    "AGT8r6p+Zmhm3XXXXXfddQH6iT5zN8n7LSTY48BRfPfdd999BwDQIAAAAAAAAAAAAAAAAADkMv7f"
    "7wWAkif7+MUfejkAtigvPezDm15cepVJPd3l7p7u///7/7////v5/vl+3B+zBj6KvvA3Kf1mBZzc"
    "KgAyAAAAAAAAAAAAAAAAAAAAAL/4qc8CEKxy3uYfBB8EAAA+ir5wNyntZgOc3BWAKzAGAAAAAAAA"
    "AAAAAAAAAABgv/ymswEAADYozffjS6vkAAAAPoq+cDcp7WYbnNwVgCsAAAAAAAAAAAAAAAAAAACA"
    "P3h+AQAAd13OK/+3yirlEAAAPoq+cDcp7WYTnNwWgCswDgAAAAAAAAAAAAAAAAAA4OxvdwAAgDXl"
    "oPGlhz4MAQAAPoq+8Dcp/WYDnNwvAFcAAAAAAAAAAAAAAAAAAAAAf+jznwIAgHUXe61yY/jSXgEA"
    "AD6KvnA3Ke1mCZzcKgAyYAwAAAAAAAAAAAAAAAAAAMB+/c++bABBz03lSU1NAQAAPoq+8Dcp/WYF"
    "nNyqADIAAAAAAAAAAAAAAAAAAAAAn/0XXjeAYNKDw3/8/6UQAAA+ir5wNyntZgmc3KoAMgAAAAAA"
    "AAAAAAAAAAAAAACf/UOfHwDhpIeWXmXYDyEAAD6KvnA3Ke1mDZzcVYArMA4AAAAAAAAAAAAAAAAA"
    "AOC8/rEvBwAAYMLyjfPe+GM2AAAAXmq+hdf/vti1MU+EwCEAgCbDMAzDMMgGAAAAAAAAAADA5/sF"
    "gGbzGZ9/3lfPbLbX23pmz9KkedmiZ/IqPXR3B+Wex3zfeTzW3Ie1mP3L7q9d5rXMtXvsMi/MtXvs"
    "Mhdz7cJiLga7sJjLHrt4Ya7dY5d5Ya5dvJa57K9dvJa57LHLvDDX7rHLvNhfu3gtc9ljFxcDAJ7m"
    "XDbh6UZTPA5MQGPzfwdmMWwlhmEYBgAAAAAAAADg8t/D5GGFf//z3mMe7+f7z3z/8P3/3H/u95/5"
    "nv+/+fnm53s9vh/rvx++f2Z+7vO487ivdR+el3673+Zt5pnpoYee7rc53OfBPtfhaQ73uexzHa45"
    "vJ5r6OmeZpp5Znrood/mbeaZeYZn6Lf7beZhnpln6KGnGXN4PZd9msM1B/tc9mkO1yz7XIdrDve5"
    "7JPDNQeeyz7N4Zpln8tec8As++RwzYHnspfDNYvnstcAAA==";

int main() {
    const std::vector<unsigned char> ogg = aus_base64(OGG_BASE64);
    check("Fixture dekodiert (nicht leer)", !ogg.empty());

    rune::core::wav::Clip clip;
    std::string text;
    const bool ok = rune::core::ogg::parse(ogg.data(), ogg.size(), clip, &text);
    check("OGG wird erkannt und dekodiert", ok && text.empty());
    check("Zielrate ist die Engine-Rate 22050 Hz", clip.sample_rate == 22050);
    // 0,25 s bei 22050 Hz -> rund 5512 Werte (Vorbis-Granularitaet erlaubt Abweichung).
    const long erwartet = 5512;
    const long ist = static_cast<long>(clip.samples.size());
    check("Laenge passt zu 0,25 s", std::labs(ist - erwartet) <= 200);
    int spitze = 0;
    for (int16_t v : clip.samples)
        if (std::abs(static_cast<int>(v)) > spitze) spitze = std::abs(static_cast<int>(v));
    // Links 0,5 und rechts 0 -> Mono-Mittelwert 0,25 -> Spitze um 8192.
    check("Stereo->Mono halbiert die Amplitude (Spitze um 8192)",
          spitze > 6000 && spitze < 11000);
    check("Ton ist nicht still", spitze > 1000);

    rune::core::wav::Clip leer;
    std::string grund;
    check("keine Daten -> false mit Begruendung",
          !rune::core::ogg::parse(nullptr, 0, leer, &grund) && !grund.empty());
    const unsigned char mist[] = {'R', 'I', 'F', 'F', 0, 0, 0, 0};
    check("kein OggS -> false mit Begruendung",
          !rune::core::ogg::parse(mist, sizeof(mist), leer, &grund) && !grund.empty());
    const unsigned char kaputt[] = {'O', 'g', 'g', 'S', 0, 2, 0, 0, 0, 0, 0, 0,
                                    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    check("OggS ohne lesbare Kopfdaten -> false",
          !rune::core::ogg::parse(kaputt, sizeof(kaputt), leer, &grund));
    check("abgeschnittene Datei liefert keine Absturzwerte",
          !rune::core::ogg::parse(ogg.data(), 64, leer, &grund));
    check("echte Datei bleibt nach den Fehlerfaellen lesbar",
          rune::core::ogg::parse(ogg.data(), ogg.size(), leer, &grund) && !leer.leer());

    std::printf("%s\n", fehler == 0 ? "OGG-TEST: alle Pruefungen bestanden"
                                     : "OGG-TEST FEHLGESCHLAGEN");
    return fehler == 0 ? 0 : 1;
}
