// ============================================================================
//  event_command_settings_test.cpp — Etappe 45: nummerierte Register +
//  Einstell-Schema + Asset-Filter, kopflos (ohne Win32).
//     g++ -std=c++20 -O1 -Wall -Wextra -Isrc src/editor/json.cpp
//         tools/event_command_settings_test.cpp -o build/ec_settings_test
// ============================================================================

#include "editor/event_numbered_registry.h"
#include "editor/event_command_settings.h"
#include "editor/json.h"

#include <cstdio>
#include <string>

using namespace rune::editor;

namespace {
int g_fail = 0, g_count = 0;
void check(const char* name, bool ok) {
    ++g_count;
    if (!ok) ++g_fail;
    std::printf("  %s %s\n", ok ? "ok  " : "FAIL", name);
}
} // namespace

int main() {
    std::printf("== Nummerierte Register + Einstell-Schema (Etappe 45) ==\n");

    // Register: Grenzen, Namen, Nummer-Suche, Beschriftung.
    NumberedRegistry r;
    check("Register hat 999 Plaetze", (int)r.names.size() == EC_NUMBERED_MAX);
    check("Nummer 0 ungueltig", !r.valid(0));
    check("Nummer 1000 ungueltig", !r.valid(1000));
    check("Nummer 999 gueltig", r.valid(999));
    r.set_name(101, "Tor_Offen");
    r.set_name(202, "Quest_Phase");
    check("Name ruecklesbar", r.name_of(101) == "Tor_Offen");
    check("Nummer rueckwaerts", r.number_of("Quest_Phase") == 202);
    check("unbenannt leer", r.name_of(1).empty());
    check("Beschriftung vierstellig", r.label(101) == "0101: Tor_Offen");
    check("unbenannte Beschriftung", r.label(1) == "0001: (unbenannt)");

    // Roundtrip Objekt und Liste.
    JsonValue obj = json_parse("{\"101\":\"Tor_Offen\",\"202\":\"Quest_Phase\"}");
    NumberedRegistry a = NumberedRegistry::from_json(&obj);
    check("Objekt gelesen", a.name_of(101) == "Tor_Offen" && a.name_of(202) == "Quest_Phase");
    JsonValue back = a.to_json();
    check("Roundtrip erhaelt Namen", back.find("101") && back.find("101")->str == "Tor_Offen");
    check("Roundtrip nur benannte", back.obj.size() == 2);
    JsonValue arr = json_parse("[\"Eins\",\"Zwei\"]");
    NumberedRegistry b = NumberedRegistry::from_json(&arr);
    check("Liste gelesen (Index=Nummer-1)", b.name_of(1) == "Eins" && b.name_of(2) == "Zwei");
    NumberedRegistry empty = NumberedRegistry::from_json(nullptr);
    check("fehlender Schluessel => leer", empty.number_of("x") == 0);

    // Einstell-Schema: Selektoren statt Freitext fuer Schalter/Variable.
    EcSetField f[EC_SET_MAX_FIELDS];
    int n = ec_settings_fields(ET_SWITCH, f, EC_SET_MAX_FIELDS);
    bool has_num = false, has_bool = false;
    for (int i = 0; i < n; ++i) {
        if (f[i].slot == 0 && f[i].control == EcSetControl::NumberedSwitch) has_num = true;
        if (f[i].slot == 6 && f[i].control == EcSetControl::Bool) has_bool = true;
    }
    check("Schalter: nummerierte Liste + Bool", has_num && has_bool);
    n = ec_settings_fields(ET_VARIABLE, f, EC_SET_MAX_FIELDS);
    has_num = false;
    for (int i = 0; i < n; ++i)
        if (f[i].slot == 0 && f[i].control == EcSetControl::NumberedVariable) has_num = true;
    check("Variable: nummerierte Liste", has_num);
    n = ec_settings_fields(ET_ITEM, f, EC_SET_MAX_FIELDS);
    bool ref = false;
    for (int i = 0; i < n; ++i)
        if (f[i].control == EcSetControl::Ref && std::string(f[i].collection) == "items") ref = true;
    check("Item: Datenbank-Auswahl", ref);
    n = ec_settings_fields(ET_PLAY_BGM, f, EC_SET_MAX_FIELDS);
    bool asset = false;
    for (int i = 0; i < n; ++i)
        if (f[i].control == EcSetControl::Asset && std::string(f[i].collection) == "bgms") asset = true;
    check("BGM: Asset-Ordnerwahl", asset);
    n = ec_settings_fields(ET_TEXT, f, EC_SET_MAX_FIELDS);
    check("Text: Mehrzeiler", n >= 1 && f[0].control == EcSetControl::Multiline);

    // Jeder Typ liefert 1..8 sinnvolle Felder, kein Hidden, Slots gueltig.
    bool all_ok = true;
    for (int t = 0; t < ET_COUNT; ++t) {
        int m = ec_settings_fields(t, f, EC_SET_MAX_FIELDS);
        if (m < 1 || m > EC_SET_MAX_FIELDS) all_ok = false;
        for (int i = 0; i < m; ++i) {
            if (f[i].control == EcSetControl::Hidden) all_ok = false;
            if (f[i].slot < 0 || f[i].slot > 7) all_ok = false;
            if (f[i].control == EcSetControl::Combo && f[i].option_count <= 0 && f[i].slot != 5 && f[i].slot != 7)
                all_ok = false;
        }
    }
    check("alle Typen: 1..8 Felder, kein Hidden, Slots gueltig", all_ok);

    // Asset-Filter und Schlüssel.
    check("Audio-Unterordner", std::string(ec_asset_subdir("bgms")) == "audio\\bgm");
    check("Bild-Unterordner", std::string(ec_asset_subdir("pictures")) == "pictures");
    check("Audio erkennt ogg", ec_asset_matches("bgms", "dorf.OGG"));
    check("Audio erkennt wav", ec_asset_matches("ses", "door.wav"));
    check("Audio lehnt png", !ec_asset_matches("bgms", "bild.png"));
    check("Bild erkennt png", ec_asset_matches("pictures", "held.PNG"));
    check("Bild lehnt ogg", !ec_asset_matches("pictures", "song.ogg"));
    check("ohne Endung abgelehnt", !ec_asset_matches("bgms", "datei"));
    check("Schluessel mit Slash", ec_asset_key("bgms", "dorf.ogg") == "audio/bgm/dorf.ogg");

    std::printf("\n%d Checks, %d Fehler\n", g_count, g_fail);
    std::printf(g_fail ? "EINSTELL-SCHEMA FEHLGESCHLAGEN\n" : "EINSTELL-SCHEMA OK\n");
    return g_fail ? 1 : 0;
}
