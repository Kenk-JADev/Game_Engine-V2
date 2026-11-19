// ============================================================================
//  json_test.cpp — Kopfloser Test fuer die JSON-Schicht des Editors (19a).
//
//  Warum in C++ und nicht in Ruby? src/editor/json.cpp ist der einzige Weg,
//  auf dem der RuneEditor Karten liest und schreibt. Wenn dieser Parser oder
//  Schreiber etwas verschluckt (Reihenfolge, unbekannte Schluessel, int vs.
//  float, Umlaute), ist eine Karte nach dem Speichern kaputt — und der Fehler
//  faellt erst im Spiel auf. Der Test haengt an keiner Win32-/D3D11-/Ruby-
//  Abhaengigkeit und laeuft deshalb sowohl unter MSYS2 (build.ps1 -Test) als
//  auch in CI auf Linux. Bau in einer Zeile:
//     g++ -std=c++20 -O1 -Wall -Wextra -Isrc src/editor/json.cpp tools/json_test.cpp -o build/json_test
//
//  Zusatzmodus (fuer die Ruby-Vergleichspruefung tools/json_bridge_check.rb):
//     json_test --roundtrip <eingabe.json> <ausgabe.json>
//  Liest die Datei, parst sie und schreibt sie mit json_write neu — genau das,
//  was der Editor beim Speichern tut.
// ============================================================================

#include "editor/json.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

using rune::editor::JsonValue;
using rune::editor::json_parse;
using rune::editor::json_write;

namespace {

int g_fail = 0;
int g_count = 0;

void check(const char* name, bool ok) {
    ++g_count;
    if (!ok) ++g_fail;
    std::printf("  %s %s\n", ok ? "ok  " : "FAIL", name);
}

// Wirft json_parse bei diesem Text?
bool wirft(const std::string& text) {
    try {
        json_parse(text);
    } catch (const std::runtime_error&) {
        return true;
    } catch (...) {
        return true;
    }
    return false;
}

bool datei_lesen(const std::string& pfad, std::string& inhalt) {
    std::FILE* f = std::fopen(pfad.c_str(), "rb");
    if (!f) return false;
    char buf[8192];
    size_t n = 0;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) inhalt.append(buf, n);
    std::fclose(f);
    return true;
}

bool datei_schreiben(const std::string& pfad, const std::string& inhalt) {
    std::FILE* f = std::fopen(pfad.c_str(), "wb");
    if (!f) return false;
    const size_t n = std::fwrite(inhalt.data(), 1, inhalt.size(), f);
    std::fclose(f);
    return n == inhalt.size();
}

int roundtrip_modus(int argc, char** argv) {
    if (argc < 4) {
        std::fprintf(stderr, "Aufruf: json_test --roundtrip <ein.json> <aus.json>\n");
        return 2;
    }
    std::string inhalt;
    if (!datei_lesen(argv[2], inhalt)) {
        std::fprintf(stderr, "Kann %s nicht lesen\n", argv[2]);
        return 2;
    }
    try {
        const JsonValue v = json_parse(inhalt);
        const std::string out = json_write(v);
        if (!datei_schreiben(argv[3], out)) {
            std::fprintf(stderr, "Kann %s nicht schreiben\n", argv[3]);
            return 2;
        }
    } catch (const std::runtime_error& e) {
        std::fprintf(stderr, "JSON-Fehler in %s: %s\n", argv[2], e.what());
        return 1;
    }
    std::printf("roundtrip: %s -> %s\n", argv[2], argv[3]);
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (argc >= 2 && std::strcmp(argv[1], "--roundtrip") == 0) return roundtrip_modus(argc, argv);

    // Repo-Wurzel fuer die Daten-Pruefung: argv[1] oder aktuelles Verzeichnis.
    const std::string wurzel = (argc >= 2) ? argv[1] : ".";

    std::printf("== JSON: Grundlagen ==\n");
    {
        const JsonValue v = json_parse("{\"a\": 1, \"b\": 2.5, \"c\": \"text\", "
                                       "\"d\": true, \"e\": false, \"f\": null}");
        check("Objekt erkannt", v.is_obj() && v.obj.size() == 6);
        check("Ganzzahl bleibt Ganzzahl", v.find("a")->is_num() && v.find("a")->is_int);
        check("Kommazahl bleibt Kommazahl", v.find("b")->is_num() && !v.find("b")->is_int);
        check("Zahlenwert stimmt", v.find("a")->number() == 1.0 && v.find("b")->number() == 2.5);
        check("String gelesen", v.find("c")->is_str() && v.find("c")->text() == "text");
        check("Bool gelesen", v.find("d")->b && !v.find("e")->b);
        check("null gelesen", v.find("f")->is_null());
        check("number() mit Fallback", v.find("zzz") == nullptr &&
                                       v.find("a")->number(-9.0) == 1.0);
    }

    std::printf("== JSON: Schluessel-Reihenfolge und Unbekanntes ==\n");
    {
        const JsonValue v = json_parse("{\"z\": 1, \"a\": 2, \"events\": [1, 2], "
                                       "\"neu\": {\"tief\": \"wert\"}}");
        check("Reihenfolge bleibt (z vor a)", v.obj[0].first == "z" && v.obj[1].first == "a");
        check("unbekannter Schluessel ueberlebt", v.find("neu") != nullptr);
        check("Array gelesen", v.find("events")->is_arr() && v.find("events")->arr.size() == 2);
        check("verschachteltes Objekt", v.find("neu")->find("tief")->text() == "wert");

        JsonValue kopie = v;
        kopie.set("z", JsonValue::make_num(42.0, true));
        check("set() ersetzt an Ort und Stelle", kopie.obj[0].first == "z" &&
                                                kopie.find("z")->number() == 42.0);
        kopie.set("hinten", JsonValue::make_str("neu"));
        check("set() haengt neuen Schluessel an", kopie.obj.back().first == "hinten");
        check("Original unveraendert", v.find("z")->number() == 1.0 && v.find("hinten") == nullptr);
    }

    std::printf("== JSON: doppelte Schluessel ==\n");
    {
        const JsonValue v = json_parse("{\"a\": 1, \"a\": 2}");
        check("beide Treffer bleiben stehen", v.obj.size() == 2);
        check("find() liefert den ersten", v.find("a")->number() == 1.0);
    }

    std::printf("== JSON: Strings und Umlaute ==\n");
    {
        const JsonValue v = json_parse("{\"t\": \"Zeile\\nTab\\tEnde\", \"q\": \"\\\"zitiert\\\"\", "
                                       "\"s\": \"Pfad\\\\Datei\", \"slash\": \"a\\/b\", "
                                       "\"u\": \"\\u00fcber\", \"emoji\": \"\\uD83D\\uDE00\"}");
        check("Escape \\n und \\t", v.find("t")->text() == "Zeile\nTab\tEnde");
        check("Escape Anfuehrung", v.find("q")->text() == "\"zitiert\"");
        check("Escape Backslash", v.find("s")->text() == "Pfad\\Datei");
        check("Escape Slash", v.find("slash")->text() == "a/b");
        const std::string u = v.find("u")->text();
        check("\\u00fc wird UTF-8 ('ueber' = 5 Bytes)", u.size() == 5 &&
                                              static_cast<unsigned char>(u[0]) == 0xC3 &&
                                              static_cast<unsigned char>(u[1]) == 0xBC &&
                                              u.substr(2) == "ber");
        const std::string e = v.find("emoji")->text();
        check("Surrogatpaar wird UTF-8 (4 Bytes)", e.size() == 4 &&
                                                   static_cast<unsigned char>(e[0]) == 0xF0 &&
                                                   static_cast<unsigned char>(e[1]) == 0x9F &&
                                                   static_cast<unsigned char>(e[2]) == 0x98 &&
                                                   static_cast<unsigned char>(e[3]) == 0x80);

        // Geschrieben wird roh (UTF-8 durchreichen), Steuerzeichen als Escape.
        const std::string out = json_write(v);
        check("Ausgabe enthaelt \\n-Escape", out.find("\\n") != std::string::npos);
        check("Ausgabe enthaelt rohes Umlaut-Byte", out.find("\xC3\xBC") != std::string::npos);
        check("Ausgabe enthaelt rohes Emoji", out.find("\xF0\x9F\x98\x80") != std::string::npos);
    }

    std::printf("== JSON: Fehlerfaelle ==\n");
    check("leere Eingabe wirft", wirft(""));
    check("nur Leerzeichen wirft", wirft("   \n "));
    check("offenes Objekt wirft", wirft("{\"a\": 1"));
    check("offenes Array wirft", wirft("[1, 2"));
    check("Komma zu viel wirft", wirft("[1, 2,]"));
    check("Doppelpunkt fehlt wirft", wirft("{\"a\" 1}"));
    check("String ohne Ende wirft", wirft("\"ohne ende"));
    check("unbekanntes Escape wirft", wirft("\"\\q\""));
    check("kaputtes \\u wirft", wirft("\"\\uZZZZ\""));
    check("true-Fragment wirft", wirft("tru"));
    check("false-Fragment wirft", wirft("fals"));
    check("null-Fragment wirft", wirft("nul"));
    check("Muell wirft", wirft("json"));
    check("Zeichen hinter dem Wert wirft", wirft("1 2"));
    check("gueltige Werte werfen nicht", !wirft("{}") && !wirft("[]") && !wirft("0") &&
                                         !wirft("-1.5e3") && !wirft("true") && !wirft("null"));
    {
        bool meldung_ok = false;
        try {
            json_parse("{\"a\": }");
        } catch (const std::runtime_error& ex) {
            // Fehlermeldung nennt die Position (Zeichen-Offset) — wichtig fuer
            // den Probleme-Dialog und fuer Fehlersuche in grossen Karten.
            meldung_ok = std::strstr(ex.what(), "Zeichen") != nullptr;
        }
        check("Fehlermeldung nennt die Position", meldung_ok);
    }

    std::printf("== JSON: Schreiben ==\n");
    {
        JsonValue v = json_parse("{\"liste\": [1, 2], \"leer\": [], \"obj\": {}, "
                                 "\"g\": 5, \"f\": 5.0, \"neg\": -3.25}");
        const std::string out = json_write(v);
        check("Ein Element pro Zeile (2 Leerzeichen Einrueckung)",
              out.find("[\n    1,\n    2\n  ]") != std::string::npos);
        check("leeres Array bleibt []", out.find("\"leer\": []") != std::string::npos);
        check("leeres Objekt bleibt {}", out.find("\"obj\": {}") != std::string::npos);
        check("Ganzzahl ohne Nachkomma", out.find("\"g\": 5,") != std::string::npos);
        check("Kommazahl mit .0", out.find("\"f\": 5.0,") != std::string::npos);
        check("negative Kommazahl", out.find("\"neg\": -3.25") != std::string::npos);
        check("Datei endet mit Zeilenumbruch", !out.empty() && out.back() == '\n');

        // Idempotenz: Nochmal schreiben darf nichts mehr veraendern, sonst
        // produzieren Editor und Spiel bei jedem Speichern neue Diffs.
        const std::string out2 = json_write(json_parse(out));
        check("schreiben ist idempotent", out == out2);
    }

    std::printf("== JSON: Zahlen-Treue (Etappe 19a) ==\n");
    {
        // Eine Zahl gilt als treu, wenn sie nach parse -> write -> parse
        // bitgleich ist. %.12g (alter Stand) war das nicht: pi wurde zu
        // 3.14159265359, Spielstaende zu 2.00403410417e-05.
        auto bitgleich = [](double a, double b) {
            return a == b && std::memcmp(&a, &b, sizeof(double)) == 0;
        };
        auto treu = [&](const char* zahl) {
            const std::string text = std::string("{\"v\": ") + zahl + "}";
            const JsonValue v = json_parse(text);
            const double vorher = v.find("v")->number();
            const std::string out = json_write(v);
            const double nachher = json_parse(out).find("v")->number();
            return bitgleich(vorher, nachher);
        };

        check("pi (Startblickwinkel) bleibt exakt", treu("3.141592653589793"));
        check("pi wird vollstaendig geschrieben",
              json_write(json_parse("{\"yaw\": 3.141592653589793}"))
                  .find("3.141592653589793") != std::string::npos);
        check("kleine Spielerkoordinate bleibt exakt", treu("2.0040341041749343e-05"));
        check("Spielzeit bleibt exakt", treu("9.91547869797796"));
        check("0.1+0.2-Ergebnis bleibt exakt", treu("0.30000000000000004"));
        check("negative Kommazahl bleibt exakt", treu("-0.09510689973831177"));
        check("Ganzzahl 0 bleibt exakt", treu("0"));
        check("Kommazahl 0.0 bleibt Kommazahl",
              json_write(json_parse("{\"v\": 0.0}")).find("\"v\": 0.0") != std::string::npos);
        check("grosse Ganzzahl (19 Stellen) bleibt exakt", treu("1234567890123456789"));
        check("uebergrosse Ganzzahl (25 Stellen) stuerzt nicht",
              treu("1234567890123456789012345"));
        check("Exponentenschreibweise bleibt exakt", treu("1.5e-7"));

        // Festkomma statt Exponentenschreibweise: %g macht aus 240.0 sonst
        // "2.4e+02" — Ruby/Python schreiben "240.0", und genau das muss der
        // Editor auch tun (sonst kippt beim Speichern das Format der Karten).
        auto geschrieben = [](const char* zahl) {
            return json_write(json_parse((std::string("{\"v\": ") + zahl + "}").c_str()));
        };
        check("240.0 bleibt Festkomma", geschrieben("240.0").find("\"v\": 240.0") != std::string::npos);
        check("20.0 bleibt Festkomma", geschrieben("20.0").find("\"v\": 20.0") != std::string::npos);
        check("-10.0 bleibt Festkomma", geschrieben("-10.0").find("\"v\": -10.0") != std::string::npos);
        check("0.0001 bleibt Festkomma", geschrieben("0.0001").find("\"v\": 0.0001") != std::string::npos);
        check("1e15 bleibt Festkomma",
              geschrieben("1000000000000000.0").find("1000000000000000.0") != std::string::npos);
        check("1e-5 darf wissenschaftlich bleiben", treu("0.00001"));
        check("kein e+ in Kartenwerten",
              geschrieben("120.0").find("e+") == std::string::npos &&
              geschrieben("3.5").find("e+") == std::string::npos);

        JsonValue nan = JsonValue::make_obj();
        nan.set("kaputt", JsonValue::make_num(std::nan("")));
        nan.set("ok", JsonValue::make_num(1.5, false));
        const std::string out = json_write(nan);
        check("NaN wird zu null (Datei bleibt lesbar)",
              out.find("\"kaputt\": null") != std::string::npos);
        check("NaN-Ausgabe ist parsebar", json_parse(out).find("ok")->number() == 1.5);
    }

    std::printf("== JSON: Grosse/tiefe Dokumente ==\n");
    {
        std::string text = "[";
        for (int i = 0; i < 500; ++i) {
            if (i) text += ",";
            text += "{\"x\": " + std::to_string(i) + ".5, \"t\": \"objekt\"}";
        }
        text += "]";
        const JsonValue v = json_parse(text);
        check("500 Objekte gelesen", v.arr.size() == 500);
        check("Inhalt stimmt", v.arr[499].find("x")->number() == 499.5);
        const std::string out = json_write(v);
        check("neu geschrieben und wieder lesbar", json_parse(out).arr.size() == 500);
    }

    std::printf("== JSON: mitgelieferte Karten ==\n");
    {
        const std::string karte = wurzel + "/projekte/dorf/data/maps/dorf.json";
        std::string inhalt;
        if (!datei_lesen(karte, inhalt)) {
            std::printf("  --   %s nicht gefunden (uebersprungen)\n", karte.c_str());
        } else {
            const JsonValue v = json_parse(inhalt);
            const JsonValue* objs = v.find("objects");
            const JsonValue* evs = v.find("events");
            const JsonValue* npcs = v.find("npcs");
            const JsonValue* terrain = v.find("terrain");
            check("Dorf-Karte geparst", v.is_obj());
            check("Objekte vorhanden", objs && objs->arr.size() >= 30);
            check("Events vorhanden", evs && evs->arr.size() == 6);
            check("NPCs vorhanden", npcs && npcs->arr.size() == 1);
            check("Terrain vorhanden", terrain && terrain->is_obj());
            check("Kartenname lesbar", v.find("name") && v.find("name")->text() == "Dorf");

            // Das ist der eigentliche Speichervorgang des Editors.
            const std::string out = json_write(v);
            const JsonValue zurueck = json_parse(out);
            check("Roundtrip: Objektzahl gleich",
                  zurueck.find("objects")->arr.size() == objs->arr.size());
            check("Roundtrip: Eventzahl gleich",
                  zurueck.find("events")->arr.size() == evs->arr.size());
            check("Roundtrip: Schluessel der Wurzel in gleicher Reihenfolge",
                  zurueck.obj.size() == v.obj.size() &&
                  zurueck.obj.front().first == v.obj.front().first &&
                  zurueck.obj.back().first == v.obj.back().first);
            check("Roundtrip: Event-Seiten ueberleben",
                  zurueck.find("events")->arr[0].find("pages") != nullptr);
            check("Roundtrip: Hoehe-Pinsel ueberleben",
                  zurueck.find("terrain")->find("heights") != nullptr);
            check("Roundtrip ist idempotent", json_write(zurueck) == out);
        }
    }

    std::printf("== JSON: Projekt-Systemdaten ==\n");
    {
        // Genau hier schlug der alte %.12g-Schreiber zu: der Startblickwinkel
        // (pi) wurde beim Speichern im Editor beschnitten.
        const std::string pfad = wurzel + "/projekte/meine_welt/data/system.json";
        std::string inhalt;
        if (!datei_lesen(pfad, inhalt)) {
            std::printf("  --   %s nicht gefunden (uebersprungen)\n", pfad.c_str());
        } else {
            const JsonValue v = json_parse(inhalt);
            const JsonValue* start = v.find("start");
            check("start-Block vorhanden", start && start->is_obj());
            const double yaw = start->find("yaw")->number();
            const std::string out = json_write(v);
            const double yaw_neu = json_parse(out).find("start")->find("yaw")->number();
            check("Startblickwinkel ueberlebt das Speichern bitgleich",
                  yaw == yaw_neu && std::memcmp(&yaw, &yaw_neu, sizeof(double)) == 0);
            check("Kartenname des Startpunkts bleibt", json_parse(out).find("start")->
                  find("map")->text() == "welt01");
        }
    }

    std::printf("\n%d Checks, %d Fehler\n", g_count, g_fail);
    std::printf(g_fail == 0 ? "JSON-TEST OK\n" : "JSON-TEST FEHLGESCHLAGEN\n");
    return g_fail == 0 ? 0 : 1;
}
