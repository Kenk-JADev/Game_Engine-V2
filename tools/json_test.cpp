// ============================================================================
//  json_test.cpp — Kopfloser Test für die JSON-Schicht des Editors (19a).
//
//  Warum in C++ und nicht in Ruby? src/editor/json.cpp ist der einzige Weg,
//  auf dem der RuneEditor Karten liest und schreibt. Wenn dieser Parser oder
//  Schreiber etwas verschluckt (Reihenfolge, unbekannte Schlüssel, int vs.
//  float, Umlaute), ist eine Karte nach dem Speichern kaputt — und der Fehler
//  fällt erst im Spiel auf. Der Test hängt an keiner Win32-/D3D11-/Ruby-
//  Abhängigkeit und läuft deshalb sowohl unter MSYS2 (build.ps1 -Test) als
//  auch in CI auf Linux. Bau in einer Zeile:
//     g++ -std=c++20 -O1 -Wall -Wextra -Isrc src/editor/json.cpp tools/json_test.cpp -o build/json_test
//
//  Zusatzmodus (für die Ruby-Vergleichsprüfung tools/json_bridge_check.rb):
//     json_test --roundtrip <eingabe.json> <ausgabe.json>
//  Liest die Datei, parst sie und schreibt sie mit json_write neu — genau das,
//  was der Editor beim Speichern tut.
// ============================================================================

#include "editor/json.h"
#include "editor/encounter_check.h"
#include "editor/encounter_form.h"
#include "editor/map_props_form.h"
#include "editor/number_format.h"
#include "editor/placement_form.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

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

struct EncounterFinding {
    int severity;
    std::string where;
    std::string text;
};

std::vector<EncounterFinding> encounter_funde(const std::string& json) {
    const JsonValue root = json_parse(json);
    std::vector<EncounterFinding> funde;
    rune::editor::issue_scan_encounters(root.find("encounters"), "Karte encounters",
        [&](int severity, const std::string& where, const std::string& text) {
            funde.push_back({severity, where, text});
        });
    return funde;
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

void placement_form_tests() {
    using namespace rune::editor;
    std::printf("== Editor: NPCs, Schilder und Objekte ==\n");
    JsonValue map = json_parse(R"({"objects":[{"type":"haus","x":0,"z":0}],
      "events":[{"id":1,"name":"NPC 1"},{"id":3,"name":"Schild 1"}],
      "npcs":[{"name":"Mara","x":1.2345678901234567,"z":2,"speed":1.2,
        "path":[[1,2],[3,4]],"extra":{"wichtig":true},
        "commands":[{"cmd":"text","lines":["Guten Tag","Äpfel"],"extra":42},
                    {"cmd":"switch","name":"auftrag","value":true}]}]})");
    const JsonValue original = map.find("npcs")->arr[0];
    const std::string original_text = json_write(original);
    const PlacementForm before = placement_form_read(original, PlacementKind::Npc);
    check("NPC-Formular liest Wegpunkte und Text", before.name == "Mara" &&
          before.path == "1; 2\r\n3; 4" && before.text == "Guten Tag\r\nÄpfel" &&
          before.path_valid && before.text_editable && before.y == "0");
    const PlacementEdit unchanged = placement_form_prepare(original, PlacementKind::Npc, before, before);
    check("unverändertes Formular verändert keine NPC-Daten", unchanged.ok && !unchanged.changed &&
          json_write(unchanged.value) == original_text);
    PlacementForm changed = before;
    changed.y = "1,75";
    changed.path = "1,5; -2,25\r\n3; 4";
    changed.text = "Hallo\r\n\r\nWelt";
    const PlacementEdit edit = placement_form_prepare(original, PlacementKind::Npc, before, changed);
    const JsonValue* commands = edit.value.find("commands");
    check("NPC-Höhe, Dezimalkomma und Wegpunkte validiert", edit.ok && edit.changed &&
          edit.value.find("y")->number() == 1.75 && edit.value.find("path")->arr.size() == 2 &&
          edit.value.find("path")->arr[0].arr[0].number() == 1.5 &&
          edit.value.find("path")->arr[0].arr[1].number() == -2.25);
    check("Textänderung erhält Leerzeilen, Zusatzfelder und weitere Befehle",
          commands && commands->arr.size() == 2 && commands->arr[0].find("extra")->number() == 42 &&
          commands->arr[0].find("lines")->arr.size() == 3 &&
          commands->arr[0].find("lines")->arr[1].str.empty() &&
          commands->arr[1].find("cmd")->str == "switch");
    check("NPC-Vorbereitung ist transaktional und erhält präzise Werte", json_write(original) == original_text &&
          json_write(*edit.value.find("x")) == json_write(*original.find("x")) &&
          edit.value.find("extra")->find("wichtig")->b);
    changed = before; changed.path = "3; 4\r\n1; 2\r\n9; 8";
    const PlacementEdit reordered = placement_form_prepare(original, PlacementKind::Npc, before, changed);
    check("umgeordnete Wegpunkte behalten ihre ursprünglichen Zahlentypen", reordered.ok &&
          reordered.value.find("path")->arr[0].arr[0].is_int &&
          reordered.value.find("path")->arr[1].arr[1].is_int &&
          !reordered.value.find("path")->arr[2].arr[0].is_int);
    changed = before;
    changed.x = "12xyz";
    changed.text = "Änderung";
    const PlacementEdit bad_x = placement_form_prepare(original, PlacementKind::Npc, before, changed);
    check("ungültige Zahl verhindert die gesamte Änderung", !bad_x.ok && bad_x.field == "x" &&
          json_write(original) == original_text);
    changed = before; changed.speed = "0";
    check("Tempo muss positiv sein", !placement_form_prepare(original, PlacementKind::Npc, before, changed).ok);
    changed = before; changed.radius = "nan";
    check("NaN wird zurückgewiesen", !placement_form_prepare(original, PlacementKind::Npc, before, changed).ok);
    changed = before; changed.path = "1; 2; 3";
    check("kaputter Wegpunkt wird zurückgewiesen", !placement_form_prepare(original, PlacementKind::Npc, before, changed).ok);
    changed = before; changed.name = "  ";
    check("leerer Name wird zurückgewiesen", !placement_form_prepare(original, PlacementKind::Npc, before, changed).ok);
    changed.name = "A\tB";
    check("Steuerzeichen im Namen werden zurückgewiesen", !placement_form_prepare(original, PlacementKind::Npc, before, changed).ok);

    const JsonValue invalid = json_parse(R"({"name":"Alt","x":0,"z":0,"path":[[1,2,3]],
                                              "commands":{"fremd":true}})");
    const PlacementForm old_form = placement_form_read(invalid, PlacementKind::Npc);
    check("ungültige Alt-Wegpunkte und Kommandos werden angezeigt bzw. geschützt",
          !old_form.path_valid && !old_form.text_editable);
    changed = old_form; changed.x = "3";
    const PlacementEdit old_edit = placement_form_prepare(invalid, PlacementKind::Npc, old_form, changed);
    check("andere Änderungen erhalten ungültige Altdaten", old_edit.ok &&
          json_write(*old_edit.value.find("path")) == json_write(*invalid.find("path")) &&
          json_write(*old_edit.value.find("commands")) == json_write(*invalid.find("commands")));
    changed.path = "3; 4";
    const PlacementEdit replacement = placement_form_prepare(invalid, PlacementKind::Npc, old_form, changed);
    check("Ersetzen ungültiger Wegpunkte erfordert Rückfrage", replacement.ok && replacement.replaces_invalid_path);
    changed.text = "Gefahr";
    check("gesperrter Text kann nicht versehentlich ersetzt werden",
          !placement_form_prepare(invalid, PlacementKind::Npc, old_form, changed).ok);

    const JsonValue npc = placement_new(map, PlacementKind::Npc, "");
    const JsonValue sign = placement_new(map, PlacementKind::Sign, "");
    const JsonValue object = placement_new(map, PlacementKind::Object, "baum");
    check("neuer NPC bekommt eindeutigen Namen und bleibt ohne Wegpunkte stehen",
          npc.find("name")->str == "NPC 2" && npc.find("path")->arr.empty());
    check("Schild bekommt freie Event-ID, eigenen Namen und Textbefehl",
          sign.find("id")->number() == 2 && sign.find("name")->str == "Schild 2" &&
          sign.find("visual")->str == "schild" && sign.find("trigger")->str == "action" &&
          sign.find("commands")->arr[0].find("cmd")->str == "text");
    check("Namenskollisionen zwischen Events und NPCs erkennbar",
          placement_name_used(map, "NPC 1", PlacementKind::Npc, 0) &&
          !placement_name_used(map, "Mara", PlacementKind::Npc, 0));
    const JsonValue grosse_ids = json_parse(R"({"events":[{"id":9223372036854775808},{"id":1}]})");
    check("Event-ID-Vergabe überläuft nicht bei grossen Alt-IDs",
          placement_next_event_id(grosse_ids) == 2);
    const JsonValue grosse_position = json_parse(R"({"name":"Test","x":9007199254740993,"z":0})");
    const PlacementForm grosse_form = placement_form_read(grosse_position, PlacementKind::Npc);
    changed = grosse_form; changed.name = "Neu";
    const PlacementEdit grosse_edit = placement_form_prepare(grosse_position, PlacementKind::Npc, grosse_form, changed);
    check("F5 zeigt Ganzzahl exakt und bewahrt sie bei Namensänderung",
          grosse_form.x == "9007199254740993" && grosse_edit.ok &&
          json_write(grosse_edit.value).find("\"x\": 9007199254740993") != std::string::npos);
    check("neues Prefab-Objekt hat den gewählten Typ", object.find("type")->str == "baum");
    const PlacementForm so = placement_form_read(object, PlacementKind::Object);
    changed = so; changed.y = "4"; changed.rotation = "90"; changed.type = "haus";
    const PlacementEdit eo = placement_form_prepare(object, PlacementKind::Object, so, changed);
    check("Objekt mit Höhe und Grad-Drehung gespeichert", eo.ok && eo.value.find("y")->number() == 4 &&
          std::fabs(eo.value.find("rot")->number() - 1.5707963267948966) < 1e-14 &&
          eo.value.find("type")->str == "haus");
    const JsonValue pages = json_parse(R"({"name":"Schild","x":0,"z":0,"visual":"schild",
       "pages":[{"commands":[{"cmd":"text","lines":["A"]}]},
                {"commands":[{"cmd":"text","lines":["B"]}]}]})");
    const PlacementForm pf = placement_form_read(pages, PlacementKind::Sign);
    changed = pf; changed.x = "5";
    const PlacementEdit pe = placement_form_prepare(pages, PlacementKind::Sign, pf, changed);
    check("mehrseitiges Schild schützt Text, Positionsänderung bewahrt Seiten", !pf.text_editable && pe.ok &&
          json_write(*pe.value.find("pages")) == json_write(*pages.find("pages")));
    map.find("npcs")->arr[0] = edit.value;
    map.find("events")->arr.push_back(sign);
    map.find("objects")->arr.push_back(eo.value);
    const JsonValue roundtrip = json_parse(json_write(map));
    check("alle drei Platzierungsarten überleben einen Karten-Roundtrip",
          roundtrip.find("npcs")->arr[0].find("commands")->arr.size() == 2 &&
          roundtrip.find("events")->arr[2].find("visual")->str == "schild" &&
          roundtrip.find("objects")->arr[1].find("y")->number() == 4);
    const JsonValue bad_heights = json_parse(R"({"objects":[{"y":"hoch"}],
        "events":[{"y":null},{"y":[]}],"npcs":[{"y":1},{"y":false}]})");
    std::vector<EncounterFinding> heights;
    issue_scan_placement_heights(bad_heights, "Karte", [&](int severity, const std::string& where,
                                                            const std::string& text) {
        heights.push_back({severity, where, text});
    });
    check("F10 meldet ungültige Höhen aller drei Arten, null ist erlaubt",
          heights.size() == 3 && heights[0].severity == 1 && heights[0].where == "Karte objects #1" &&
          heights[1].where == "Karte events #2" && heights[2].where == "Karte npcs #2");
}

// Der Win32-Dialog benutzt dieselben Funktionen. So können wir auch ohne
// Fenster nachweisen, dass Eingaben vor dem Anwenden geprüft werden und
// unbekannte/optionale Kartendaten nicht nebenbei verschwinden.
void encounter_form_tests(const std::string& wurzel) {
    using namespace rune::editor;
    std::printf("== Editor: F2-Zufallskämpfe (Datenmodell) ==\n");

    for (const char* text : {R"({})", R"({"encounters":null})", R"({"encounters":{}})",
                             R"({"encounters":{"troops":null,"steps":null}})",
                             R"({"encounters":{"troops":[]}})"}) {
        const JsonValue root = json_parse(text);
        const EncounterForm form = encounter_form_read(root.find("encounters"));
        const EncounterEdit unchanged = encounter_form_prepare(root.find("encounters"), form,
                                                                form.troops, form.steps, false, false);
        check("optional: leer, Default 30 und unverändert",
              form.troops.empty() && form.steps == "30" && !form.bad_object && !form.bad_troops &&
              !form.bad_steps && unchanged.ok && !unchanged.changed);
    }
    {
        JsonValue root = json_parse(R"({"objects":[{"type":"haus"}],"events":[{"id":1}],)"
                                    R"("encounters":{"troops":["waldweg"," banditen ","waldweg"],)"
                                    R"("steps":30.0,"extra":{"wetter":"Regen"}}})");
        const std::string before_json = json_write(root);
        const JsonValue* old = root.find("encounters");
        const EncounterForm form = encounter_form_read(old);
        check("Duplikate, Leerzeichen und ganzzahligen Float lesen",
              form.troops == "waldweg\r\n banditen \r\nwaldweg" && form.steps == "30" &&
              !form.bad_object && !form.bad_troops && !form.bad_steps);
        const EncounterEdit same = encounter_form_prepare(old, form, form.troops, form.steps, true, true);
        check("angefasst, aber gleich: Float und JSON bleiben unangetastet",
              same.ok && !same.changed && json_write(root) == before_json);

        const EncounterEdit step = encounter_form_prepare(old, form, form.troops, "45", false, true);
        check("nur Schrittzahl ändern bewahrt Liste und unbekannte Eigenschaften",
              step.ok && step.changed && !step.troops_changed && !step.replaces_invalid &&
              step.value.find("steps")->is_int && step.value.find("steps")->number() == 45 &&
              json_write(*step.value.find("troops")) == json_write(*old->find("troops")) &&
              step.value.find("extra")->find("wetter")->str == "Regen");
        check("Vorbereitung verändert die Karte nicht", json_write(root) == before_json);

        const EncounterEdit list = encounter_form_prepare(old, form,
            "waldweg\r\n banditen \nwaldweg\r  \t\r\ntrupp_ä", form.steps, true, false);
        const JsonValue* ids = list.value.find("troops");
        check("CRLF/LF/CR, Leerzeilen, Umlaute und Gewichte",
              list.ok && list.troops_changed && ids && ids->arr.size() == 4 &&
              ids->arr[0].str == "waldweg" && ids->arr[1].str == " banditen " &&
              ids->arr[2].str == "waldweg" && ids->arr[3].str == "trupp_ä");
        check("nur Liste ändern bewahrt 30.0 und Zusatzfelder",
              list.value.find("steps")->number() == 30 && !list.value.find("steps")->is_int &&
              list.value.find("extra")->find("wetter")->str == "Regen");
        root.set("encounters", list.value);
        const JsonValue saved = json_parse(json_write(root));
        check("übernehmen und JSON-Roundtrip erhält Begegnungen und andere Kartenfelder",
              saved.find("encounters")->find("troops")->arr.size() == 4 &&
              saved.find("encounters")->find("extra")->find("wetter")->str == "Regen" &&
              saved.find("objects")->arr[0].find("type")->str == "haus" &&
              saved.find("events")->arr[0].find("id")->number() == 1);

        const EncounterForm again = encounter_form_read(root.find("encounters"));
        const EncounterEdit empty = encounter_form_prepare(root.find("encounters"), again,
                                                            "\r\n  \t\n", again.steps, true, false);
        check("leere Liste deaktiviert Kämpfe ohne Zusatzdaten zu entfernen",
              empty.ok && empty.changed && empty.value.find("troops")->is_arr() &&
              empty.value.find("troops")->arr.empty() && empty.value.find("steps")->number() == 30 &&
              empty.value.find("extra") != nullptr);
    }
    {
        JsonValue root = json_parse(R"({"objects":[],"events":[]})");
        const EncounterForm form = encounter_form_read(nullptr);
        EncounterEdit list = encounter_form_prepare(nullptr, form, "wolf\r\nwolf", "30", true, false);
        check("erste Truppenliste ohne steps anlegen: Default bleibt implizit",
              list.ok && list.changed && list.value.find("steps") == nullptr &&
              list.value.find("troops")->arr.size() == 2);
        root.obj.shrink_to_fit(); // Das nächste set() darf die Objektadresse verschieben.
        root.set("encounters", std::move(list.value));
        JsonValue* objects = root.find("objects");
        objects->arr.push_back(JsonValue::make_obj());
        check("nach neuem Wurzelfeld erneut geholter Objektzeiger ist benutzbar",
              json_parse(json_write(root)).find("objects")->arr.size() == 1);
        const EncounterEdit step = encounter_form_prepare(nullptr, form, "", "21", false, true);
        check("Schrittzahl allein anlegen aktiviert keine Begegnungen",
              step.ok && step.changed && step.value.find("steps")->number() == 21 &&
              step.value.find("troops") == nullptr);
    }
    {
        JsonValue ids;
        const std::string mit_nul("wolf\0bandit", 11);
        check("NUL in IDs wird nicht still abgeschnitten", !encounter_parse_troops(mit_nul, ids));
        check("Steuerzeichen in Datenbank-ID nicht auswählbar",
              !encounter_id_editable(mit_nul) && !encounter_id_editable("zeile\nneu") &&
              !encounter_id_editable("trupp\tneu"));
        const EncounterForm form = encounter_form_read(nullptr);
        const EncounterEdit bad_id = encounter_form_prepare(nullptr, form, "wolf\tbandit", "30", true, false);
        check("Steuerzeichen in Eingaben melden das betroffene Truppenfeld",
              !bad_id.ok && bad_id.troops_error && !bad_id.changed);
        std::string many;
        for (int i = 0; i < 7000; ++i) many += "truppe_" + std::to_string(i) + "\r\n";
        check("lange Liste ohne 512- oder 32767-Zeichen-Limit parsen",
              many.size() > 32767 && encounter_parse_troops(many, ids) &&
              ids.arr.size() == 7000 && ids.arr.back().str == "truppe_6999");
    }
    {
        std::uint64_t n = 0;
        check("leere Schrittzahl und Leerraum ergeben 30",
              encounter_parse_steps(" \t", n) && n == 30);
        check("positive ganze Zahl mit Rand-Leerraum",
              encounter_parse_steps(" 42 ", n) && n == 42);
        check("Grenzwert exakt darstellbar",
              encounter_parse_steps("9007199254740991", n) && n == ENCOUNTER_MAX_STEPS);
        for (const char* text : {"0", "-1", "+3", "1.5", "1e2", "12x", "1 2",
                                 "9007199254740992", "999999999999999999999999999999999"}) {
            check("ungültige oder zu große Schrittzahl abweisen", !encounter_parse_steps(text, n));
        }
        const JsonValue original = json_parse(R"({"troops":["wald"],"steps":9,"zukunft":true})");
        const EncounterForm form = encounter_form_read(&original);
        const std::string snapshot = json_write(original);
        const EncounterEdit invalid = encounter_form_prepare(&original, form, "neu", "0", true, true);
        check("ungültige Eingabe erzeugt keinen Teil-Commit",
              !invalid.ok && !invalid.changed && json_write(original) == snapshot);
        const EncounterEdit defaults = encounter_form_prepare(&original, form, form.troops, "", false, true);
        check("Schrittzahl leeren setzt Default 30",
              defaults.ok && defaults.value.find("steps")->number() == 30);
        const EncounterEdit maximum = encounter_form_prepare(&original, form, form.troops,
                                                               "9007199254740991", false, true);
        check("Grenzwert bleibt beim JSON-Speichern exakt",
              maximum.ok && json_write(maximum.value).find("9007199254740991") != std::string::npos);
    }
    {
        const JsonValue bad = json_parse(R"(["nicht","objekt"])" );
        const EncounterForm form = encounter_form_read(&bad);
        const EncounterEdit same = encounter_form_prepare(&bad, form, form.troops, form.steps, false, false);
        const EncounterEdit replace = encounter_form_prepare(&bad, form, "wolf", form.steps, true, false);
        check("ungültiges encounters-Objekt unberührt behalten", form.bad_object && same.ok && !same.changed);
        check("Objekt ersetzen erfordert Bestätigung",
              replace.ok && replace.changed && replace.replaces_invalid && replace.value.is_obj());

        const JsonValue mixed = json_parse(R"({"troops":["wolf",42,"  ","zeile\nneu"],)"
                                           R"("steps":15,"extra":"bewahren"})");
        const EncounterForm f = encounter_form_read(&mixed);
        check("nicht darstellbare Truppen als Altdaten markieren", f.bad_troops && f.troops == "wolf");
        const EncounterEdit only_steps = encounter_form_prepare(&mixed, f, f.troops, "16", false, true);
        check("andere Felder ändern bewahrt kaputte Liste verlustfrei",
              only_steps.ok && !only_steps.replaces_invalid &&
              json_write(*only_steps.value.find("troops")) == json_write(*mixed.find("troops")) &&
              only_steps.value.find("extra")->str == "bewahren");
        const EncounterEdit replace_list = encounter_form_prepare(&mixed, f, f.troops, f.steps, true, false);
        check("auch gleiche sichtbare Liste ersetzt Altdaten nur mit Rückfrage",
              replace_list.ok && replace_list.replaces_invalid &&
              replace_list.value.find("troops")->arr.size() == 1);

        const JsonValue invalid_steps = json_parse(R"({"troops":["wolf"],"steps":-5,"extra":1})");
        const EncounterForm fs = encounter_form_read(&invalid_steps);
        const EncounterEdit list_only = encounter_form_prepare(&invalid_steps, fs, "wolf\nbandit", fs.steps,
                                                                true, false);
        check("ungültige steps bleiben bei Listenänderung erhalten",
              fs.bad_steps && list_only.ok && !list_only.replaces_invalid &&
              list_only.value.find("steps")->number() == -5);
        const EncounterEdit replace_steps = encounter_form_prepare(&invalid_steps, fs, fs.troops, "30",
                                                                    false, true);
        check("ungültige steps nur nach Bestätigung ersetzen",
              replace_steps.ok && replace_steps.replaces_invalid &&
              replace_steps.value.find("steps")->number() == 30);
        const JsonValue too_large = json_parse(R"({"steps":9007199254740992})");
        check("bestehende zu große Zahl wird als nicht darstellbar markiert",
              encounter_form_read(&too_large).bad_steps);
    }
    {
        const JsonValue db = json_parse(R"({"troops":[)"
            R"({"id":"wald","name":"Wälder"},{"id":"wald","name":"Zweiter Name"},)"
            R"({"id":"band","name":"Banditen"},{"id":"","name":"Nur Name"},)"
            R"({"id":"zeile\nneu","name":"Neu"},42,{"id":null}]})");
        const auto choices = encounter_database_choices(db);
        check("F9-Auswahl filtert defekte und doppelte IDs",
              choices.size() == 2 && choices[0].id == "wald" && choices[0].name == "Wälder" &&
              choices[1].id == "band");
        JsonValue ids;
        encounter_parse_troops("wald\nWälder\nZweiter Name\nNur Name\nfehlt\nfehlt\nband", ids);
        const auto unknown = encounter_unknown_troops(ids, db);
        check("Prüfung akzeptiert ID und alle Namen, fasst unbekannte Duplikate zusammen",
              unknown.size() == 1 && unknown[0] == "fehlt");
        check("fehlende Datenbank ergibt keine Auswahl", encounter_database_choices(JsonValue{}).empty());
    }
    {
        std::string map_text, db_text;
        const bool read_map = datei_lesen(wurzel + "/game/data/maps/dorf.json", map_text);
        const bool read_db = datei_lesen(wurzel + "/game/data/database.json", db_text);
        check("Demo-Karte und Projektvorlage lesbar", read_map && read_db);
        if (read_map && read_db) {
            JsonValue map = json_parse(map_text);
            const JsonValue db = json_parse(db_text);
            const EncounterForm form = encounter_form_read(map.find("encounters"));
            const auto choices = encounter_database_choices(db);
            check("F2 liest echte Demo-Begegnungen und Datenbank",
                  form.troops == "waldweg\r\nbanditen" && form.steps == "30" && choices.size() >= 3);
            const EncounterEdit edit = encounter_form_prepare(map.find("encounters"), form,
                form.troops + "\r\nbrunnen", "12", true, true);
            const JsonValue* ids = edit.value.find("troops");
            const auto unknown = ids ? encounter_unknown_troops(*ids, db) : std::vector<std::string>{};
            check("Demo-Truppen sind bekannt und Auswahl bleibt gewichtet",
                  edit.ok && edit.changed && ids && unknown.empty() && ids->arr.size() == 3);
            map.set("encounters", edit.value);
            const JsonValue roundtrip = json_parse(json_write(map));
            const auto validation = encounter_funde(json_write(roundtrip));
            check("F2-Daten der Demo bestehen JSON-Roundtrip und Editorprüfung",
                  validation.empty() && roundtrip.find("encounters")->find("steps")->number() == 12 &&
                  roundtrip.find("encounters")->find("troops")->arr[2].str == "brunnen");
        }
    }
}

void blocked_form_tests() {
    using namespace rune::editor;
    std::printf("== Editor: Block-Zonen datentreu bearbeiten (19o.2) ==\n");
    const JsonValue old = json_parse(R"([
      {"x":1.2345678901234567,"z":2,"w":3,"h":4,"extra":{"nr":9007199254740993}},
      {"x":1.2345678901234567,"z":2,"w":3,"h":4,"extra":{"nr":7}}])");
    const std::string before = blocked_form_read(&old);
    check("alte Zonen haben eindeutige Sitzungsnummern und volle Präzision",
          before == "#1: 1.2345678901234567; 2; 3; 4\r\n#2: 1.2345678901234567; 2; 3; 4");
    const BlockedEdit unchanged = blocked_form_prepare(&old, before, before);
    check("unveränderte Liste erzeugt keine Änderung", unchanged.ok && !unchanged.changed);
    const std::string text = "#2: 1.2345678901234567; 2; 3; 4\r\n"
                             "#1: 1.2345678901234567; 9; 3; 4\r\n"
                             "1,5; -2,25; 3; 4\r\n5,6,7,8";
    const BlockedEdit edit = blocked_form_prepare(&old, before, text);
    check("Umordnung, Koordinatenänderung und neue Zonen sind gültig",
          edit.ok && edit.changed && edit.removed == 0 && edit.value.arr.size() == 4);
    if (edit.ok && edit.value.arr.size() == 4) {
        const JsonValue& a = edit.value.arr[0];
        const JsonValue& b = edit.value.arr[1];
        check("identische Geometrie behält bei Umordnung ihre eigenen Zusatzfelder",
              a.find("extra")->find("nr")->number() == 7 &&
              json_write(*b.find("extra")->find("nr")) == "9007199254740993\n");
        check("nur tatsächlich geänderte Koordinate wird ersetzt",
              json_write(*b.find("x")) == "1.2345678901234567\n" &&
              b.find("w")->is_int && b.find("z")->number() == 9);
        check("Dezimalkomma und alte Komma-Trennung für neue Zonen",
              edit.value.arr[2].find("x")->number() == 1.5 &&
              edit.value.arr[2].find("z")->number() == -2.25 &&
              edit.value.arr[3].find("x")->number() == 5);
        check("keine Zusatzfelder werden auf neue Zeilen übertragen",
              edit.value.arr[2].find("extra") == nullptr && edit.value.arr[3].find("extra") == nullptr);
        check("bearbeitete Zonen überstehen einen JSON-Roundtrip exakt",
              json_write(json_parse(json_write(edit.value))) == json_write(edit.value));
    }
    const BlockedEdit equivalent = blocked_form_prepare(&old, before,
        "#1: 1.2345678901234567; 2.0; 3; 4\r\n#2: 1.2345678901234567; 2; 3; 4\r\n");
    check("gleichwertige Schreibweise und Leerzeile erhalten die alten Zahlentypen",
          equivalent.ok && !equivalent.changed);
    const BlockedEdit deletion = blocked_form_prepare(&old, before,
        "#2: 1.2345678901234567; 2; 3; 4");
    check("entfernte Zone wird gezählt und verbleibende Metadaten sind richtig",
          deletion.ok && deletion.changed && deletion.removed == 1 &&
          deletion.value.arr[0].find("extra")->find("nr")->number() == 7);
    const BlockedEdit untagged = blocked_form_prepare(&old, before, "1,2,3,4");
    check("Zeile ohne Nummer ist neu und warnt vor Entfernung beider Altzonen",
          untagged.ok && untagged.removed == 2 && untagged.value.arr[0].find("extra") == nullptr);
    for (const char* invalid : {
        "#3: 1; 2; 3; 4", "#0: 1; 2; 3; 4", "#99999999999999999999999999: 1; 2; 3; 4",
        "#1 1; 2; 3; 4",
        "#1: 1; 2; 3; 4\r\n#1: 1; 2; 3; 4", "#1: 1; 2; 0; 4",
        "#1: 1; 2; -1; 4", "#1: 1; 2; inf; 4", "1;2;3", "1,2,3,4,5", "1;2;3x;4"}) {
        const BlockedEdit bad = blocked_form_prepare(&old, before, invalid);
        const std::string name = std::string("ungültige Eingabe wird ohne Teiländerung abgelehnt: ") + invalid;
        check(name.c_str(), !bad.ok && !bad.error.empty() && old.arr.size() == 2);
    }
    const JsonValue malformed = json_parse(R"([{"x":"kaputt","z":2,"w":3,"h":null,"extra":"bleibt"},42])");
    const std::string malformed_before = blocked_form_read(&malformed);
    check("ungültige Alt-Einträge und fehlende Felder sind sichtbar",
          malformed_before == "#1: ?; 2; 3; ?\r\n#2: [ungültiger Eintrag]");
    const BlockedEdit kept = blocked_form_prepare(&malformed, malformed_before,
        "#1: ?; 2.5; 3; ?\r\n#2: [ungültiger Eintrag]");
    check("andere Änderung erhält kaputte Felder und vollständigen Nicht-Objekt-Eintrag",
          kept.ok && kept.removed == 0 && kept.replaces_invalid_entries == 0 &&
          kept.value.arr[0].find("x")->str == "kaputt" && kept.value.arr[0].find("h")->is_null() &&
          kept.value.arr[0].find("extra")->str == "bleibt" && kept.value.arr[1].number() == 42);
    const BlockedEdit replace_invalid = blocked_form_prepare(&malformed, malformed_before,
        "#1: ?; 2; 3; ?\r\n#2: 1; 2; 3; 4");
    check("Ersetzen eines ungültigen Eintrags erfordert Bestätigung",
          replace_invalid.ok && replace_invalid.replaces_invalid_entries == 1 && replace_invalid.removed == 0);
    const JsonValue bad_list = json_parse(R"({"fremd":true})");
    const std::string bad_list_before = blocked_form_read(&bad_list);
    const BlockedEdit new_list = blocked_form_prepare(&bad_list, bad_list_before, "1,2,3,4");
    check("Ersetzen einer ungültigen Liste erfordert Bestätigung",
          new_list.ok && new_list.replaces_invalid_list && new_list.changed);
    const JsonValue large = json_parse(R"([{"x":9007199254740993,"z":0,"w":1,"h":2,"extra":"ja"}])");
    const std::string large_before = blocked_form_read(&large);
    const BlockedEdit large_edit = blocked_form_prepare(&large, large_before,
                                                        "#1: 9007199254740993; 3; 1; 2");
    check("große Alt-Ganzzahl bleibt bei anderer Feldänderung exakt",
          large_edit.ok && json_write(*large_edit.value.arr[0].find("x")) == "9007199254740993\n");
    const std::string very_big(400, '9');
    const JsonValue riesig = json_parse("[{\"x\":" + very_big + ",\"z\":0,\"w\":1,\"h\":2}]");
    const BlockedEdit riesig_edit = blocked_form_prepare(&riesig, blocked_form_read(&riesig),
                                                         "#1: " + very_big + "; 3; 1; 2");
    check("Ganzzahl jenseits des double-Bereichs bleibt bei anderer Feldänderung exakt",
          riesig_edit.ok && json_write(*riesig_edit.value.arr[0].find("x")) == very_big + "\n");
    const BlockedEdit none = blocked_form_prepare(nullptr, "", "  \r\n");
    check("leeres Feld ohne alte Liste legt keine neue Liste an", none.ok && !none.changed);
}

void map_props_form_tests() {
    using namespace rune::editor;
    std::printf("== Editor: F2 erhält unberührte Karteneigenschaften ==\n");
    JsonValue root = json_parse(R"({"name":"Alt","terrain":{"width":240.25,"height":120.75,)"
        R"("resolution":192.0,"seed":7,"amplitude":2.345,"extra":"Berg"},)"
        R"("fog":{"on":false,"far":123.456,"color":42,"extra":"Nebel"},)"
        R"("panorama":{"color":11,"extra":"Himmel"},)"
        R"("weather":{"type":"regen","intensity":0.55,"extra":"Wolken"},)"
        R"("blocked":[{"x":1.234567890123,"z":2,"w":3,"h":4,"extra":"Wand"}],)"
        R"("objects":[{"type":"haus"}],"events":[{"id":1}],)"
        R"("encounters":{"troops":["wolf","wolf"],"steps":30.0,"extra":true}})");
    MapPropsForm before;
    before.name = "Alt";
    before.width = "240"; before.height = "120"; before.seed = "7"; before.amplitude = "2.3";
    before.mountains = true; before.fog_far = "123"; before.fog_color = "00002A";
    before.panorama_color = "00000B"; before.weather_type = 1; before.weather_intensity = "0.6";
    before.blocked = blocked_form_read(root.find("blocked"));
    auto apply = [&root](const MapPropsForm& was, const MapPropsForm& now) {
        const BlockedEdit blocked = blocked_form_prepare(root.find("blocked"), was.blocked, now.blocked);
        return map_props_apply(root, was, now, blocked);
    };
    const std::string untouched = json_write(root);
    MapPropsChanges changes = apply(before, before);
    check("unveränderte F2-Felder schreiben nichts, trotz gerundeter Anzeige",
          !changes.changed && json_write(root) == untouched);

    // Derselbe Ablauf wie im Dialog: allgemeine Felder vorbereiten, dann
    // Encounter übernehmen. Gerundete oder versteckte Daten bleiben erhalten.
    const EncounterForm enc = encounter_form_read(root.find("encounters"));
    const EncounterEdit only_enc = encounter_form_prepare(root.find("encounters"), enc,
                                                           "wolf\nbandit", "30", true, false);
    root.set("encounters", only_enc.value);
    const JsonValue saved = json_parse(json_write(root));
    check("nur Begegnungen ändern bewahrt Gelände, Nebel, Wetter und Zonen exakt",
          saved.find("terrain")->find("width")->number() == 240.25 &&
          saved.find("terrain")->find("resolution")->is_int == false &&
          saved.find("fog")->find("far")->number() == 123.456 &&
          saved.find("weather")->find("intensity")->number() == 0.55 &&
          saved.find("blocked")->arr[0].find("x")->number() == 1.234567890123 &&
          saved.find("blocked")->arr[0].find("extra")->str == "Wand");

    const std::string original_terrain = json_write(*root.find("terrain"));
    MapPropsForm after = before;
    after.name = "Neu";
    changes = apply(before, after);
    check("nur Namen ändern lässt andere Felder und Encounter-Zusätze stehen",
          changes.changed && changes.name && !changes.terrain && !changes.atmosphere &&
          root.find("name")->str == "Neu" && json_write(*root.find("terrain")) == original_terrain &&
          root.find("encounters")->find("extra")->b && root.find("note") == nullptr);

    after = before;
    after.width = "300";
    changes = apply(before, after);
    const JsonValue* terrain = root.find("terrain");
    check("Terrain nur gezielt ändern und übrige Präzision erhalten",
          changes.changed && changes.terrain && !changes.atmosphere && terrain->find("width")->number() == 300 &&
          terrain->find("height")->number() == 120.75 && terrain->find("amplitude")->number() == 2.345 &&
          !terrain->find("resolution")->is_int && terrain->find("extra")->str == "Berg");

    after = before;
    after.fog_on = true; after.fog_color = "BFD8EC"; after.panorama_color = "0x123456";
    after.weather_type = 2; after.note = "Hallo";
    changes = apply(before, after);
    check("Atmosphäre/Wetter behalten nicht bearbeitete Schlüssel",
          changes.changed && changes.atmosphere && root.find("fog")->find("on")->b &&
          root.find("fog")->find("color")->number() == 0xBFD8EC &&
          root.find("fog")->find("far")->number() == 123.456 && root.find("fog")->find("extra")->str == "Nebel" &&
          root.find("panorama")->find("color")->number() == 0x123456 &&
          root.find("panorama")->find("extra")->str == "Himmel" &&
          root.find("weather")->find("type")->str == "schnee" &&
          root.find("weather")->find("intensity")->number() == 0.55 &&
          root.find("weather")->find("extra")->str == "Wolken" && root.find("note")->str == "Hallo");
    check("Farbparser wie im übrigen Editor",
          parse_hex_color("0X12Ab56") == 0x12AB56 && parse_hex_color("kaputt") == 0);

    after = before;
    after.blocked = "#1: 1.234567890123; 7; 5; 6\r\n1,2,3,4";
    changes = apply(before, after);
    check("bearbeitete Block-Zone behält Zusatzfeld und präzises X", changes.changed &&
          root.find("blocked")->arr.size() == 2 &&
          json_write(*root.find("blocked")->arr[0].find("x")) == "1.234567890123\n" &&
          root.find("blocked")->arr[0].find("extra")->str == "Wand" &&
          root.find("blocked")->arr[0].find("z")->number() == 7);
    before.blocked = blocked_form_read(root.find("blocked"));
    after = before;
    after.name = "darf nicht übernommen werden";
    after.blocked = "#1: 1; 2; 0; 4";
    const std::string before_bad = json_write(root);
    changes = apply(before, after);
    check("ungültige Zone verhindert Änderungen am ganzen F2-Formular",
          !changes.changed && json_write(root) == before_bad);
    after = before;
    after.blocked.clear();
    const BlockedEdit removing = blocked_form_prepare(root.find("blocked"), before.blocked, after.blocked);
    changes = map_props_apply(root, before, after, removing);
    check("explizit geleerte Block-Zonen zählen alle gelöschten Zeilen",
          removing.ok && removing.removed == 2 && changes.changed && root.find("blocked") == nullptr);
}

} // namespace

int main(int argc, char** argv) {
    if (argc >= 2 && std::strcmp(argv[1], "--roundtrip") == 0) return roundtrip_modus(argc, argv);

    // Repo-Wurzel für die Daten-Prüfung: argv[1] oder aktuelles Verzeichnis.
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

    std::printf("== JSON: Schlüssel-Reihenfolge und Unbekanntes ==\n");
    {
        const JsonValue v = json_parse("{\"z\": 1, \"a\": 2, \"events\": [1, 2], "
                                       "\"neu\": {\"tief\": \"wert\"}}");
        check("Reihenfolge bleibt (z vor a)", v.obj[0].first == "z" && v.obj[1].first == "a");
        check("unbekannter Schlüssel überlebt", v.find("neu") != nullptr);
        check("Array gelesen", v.find("events")->is_arr() && v.find("events")->arr.size() == 2);
        check("verschachteltes Objekt", v.find("neu")->find("tief")->text() == "wert");

        JsonValue kopie = v;
        kopie.set("z", JsonValue::make_num(42.0, true));
        check("set() ersetzt an Ort und Stelle", kopie.obj[0].first == "z" &&
                                                kopie.find("z")->number() == 42.0);
        kopie.set("hinten", JsonValue::make_str("neu"));
        check("set() hängt neuen Schlüssel an", kopie.obj.back().first == "hinten");
        check("Original unverändert", v.find("z")->number() == 1.0 && v.find("hinten") == nullptr);
    }

    std::printf("== JSON: doppelte Schlüssel ==\n");
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
        check("Escape Anführung", v.find("q")->text() == "\"zitiert\"");
        check("Escape Backslash", v.find("s")->text() == "Pfad\\Datei");
        check("Escape Slash", v.find("slash")->text() == "a/b");
        const std::string u = v.find("u")->text();
        check("\\u00fc wird UTF-8 ('über' = 5 Bytes)", u.size() == 5 &&
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
        check("Ausgabe enthält \\n-Escape", out.find("\\n") != std::string::npos);
        check("Ausgabe enthält rohes Umlaut-Byte", out.find("\xC3\xBC") != std::string::npos);
        check("Ausgabe enthält rohes Emoji", out.find("\xF0\x9F\x98\x80") != std::string::npos);
    }

    std::printf("== JSON: Fehlerfälle ==\n");
    check("leere Eingabe wirft", wirft(""));
    check("nur Leerzeichen wirft", wirft("   \n "));
    check("offenes Objekt wirft", wirft("{\"a\": 1"));
    check("offenes Array wirft", wirft("[1, 2"));
    check("Komma zu viel wirft", wirft("[1, 2,]"));
    check("Doppelpunkt fehlt wirft", wirft("{\"a\" 1}"));
    check("String ohne Ende wirft", wirft("\"ohne ende"));
    check("unbekanntes Escape wirft", wirft("\"\\q\""));
    check("kaputtes \\u wirft", wirft("\"\\uZZZZ\""));
    check("trü-Fragment wirft", wirft("tru"));
    check("false-Fragment wirft", wirft("fals"));
    check("null-Fragment wirft", wirft("nul"));
    check("Müll wirft", wirft("json"));
    check("Zeichen hinter dem Wert wirft", wirft("1 2"));
    for (const char* text : {"+1", "01", "-01", "-", ".1", "1.", "1e", "1e+", "1e-", "1.2.3"}) {
        const std::string name = std::string("ungültiger Zahlentoken wird abgelehnt: ") + text;
        check(name.c_str(), wirft(text));
    }
    check("gültige Werte werfen nicht", !wirft("{}") && !wirft("[]") && !wirft("0") &&
                                         !wirft("-1.5e3") && !wirft("true") && !wirft("null"));
    {
        bool meldung_ok = false;
        try {
            json_parse("{\"a\": }");
        } catch (const std::runtime_error& ex) {
            // Fehlermeldung nennt die Position (Zeichen-Offset) — wichtig für
            // den Probleme-Dialog und für Fehlersuche in grossen Karten.
            meldung_ok = std::strstr(ex.what(), "Zeichen") != nullptr;
        }
        check("Fehlermeldung nennt die Position", meldung_ok);
    }

    std::printf("== JSON: Schreiben ==\n");
    {
        JsonValue v = json_parse("{\"liste\": [1, 2], \"leer\": [], \"obj\": {}, "
                                 "\"g\": 5, \"f\": 5.0, \"neg\": -3.25}");
        const std::string out = json_write(v);
        check("Ein Element pro Zeile (2 Leerzeichen Einrückung)",
              out.find("[\n    1,\n    2\n  ]") != std::string::npos);
        check("leeres Array bleibt []", out.find("\"leer\": []") != std::string::npos);
        check("leeres Objekt bleibt {}", out.find("\"obj\": {}") != std::string::npos);
        check("Ganzzahl ohne Nachkomma", out.find("\"g\": 5,") != std::string::npos);
        check("Kommazahl mit .0", out.find("\"f\": 5.0,") != std::string::npos);
        check("negative Kommazahl", out.find("\"neg\": -3.25") != std::string::npos);
        check("Datei endet mit Zeilenumbruch", !out.empty() && out.back() == '\n');

        // Idempotenz: Nochmal schreiben darf nichts mehr verändern, sonst
        // produzieren Editor und Spiel bei jedem Speichern neue Diffs.
        const std::string out2 = json_write(json_parse(out));
        check("schreiben ist idempotent", out == out2);
    }

    std::printf("== JSON: Zahlen-Treue (Etappe 19a) ==\n");
    {
        // Eine Zahl gilt als treu, wenn sie nach parse -> write -> parse
        // bitgleich ist. %.12g (alter Stand) war das nicht: pi wurde zu
        // 3.14159265359, Spielstände zu 2.00403410417e-05.
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
        check("pi wird vollständig geschrieben",
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
        check("übergrosse Ganzzahl (25 Stellen) stürzt nicht",
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

    std::printf("== JSON: exakte Ganzzahlen und Zahlenanzeige (19o.1) ==\n");
    {
        const std::vector<std::string> tokens = {
            "0", "1", "-1", "9007199254740991", "9007199254740992", "9007199254740993",
            "-9007199254740993", "9223372036854775807", "9223372036854775808",
            "-9223372036854775808", "-9223372036854775809", "18446744073709551615",
            "123456789012345678901234567890123456789012345678901234567890",
            std::string(400, '9'), "-" + std::string(400, '9')
        };
        for (const std::string& token : tokens) {
            const JsonValue value = json_parse(token);
            const std::string out = json_write(value);
            const std::string name = "Ganzzahl exakt und idempotent: " +
                                     (token.size() > 65 ? token.substr(0, 30) + "..." : token);
            check(name.c_str(), value.is_int && value.integer_text() == token &&
                  out == token + "\n" && json_write(json_parse(out)) == out);
        }
        check("negative ganzzahlige Null wie Ruby normalisiert", json_write(json_parse("-0")) == "0\n");
        check("sehr grosse Ganzzahl bleibt trotz double-Unendlich erhalten",
              !std::isfinite(json_parse(std::string(400, '9')).number()) &&
              json_write(json_parse(std::string(400, '9'))) == std::string(400, '9') + "\n");
        JsonValue root = json_parse(R"({"a":9007199254740993,"nested":[-9223372036854775809,{"n":42}]})");
        JsonValue copy = root;
        root.set("andere_aenderung", JsonValue::make_str("neu"));
        root.find("nested")->arr[1].set("n", JsonValue::make_num(0.5));
        const std::string out = json_write(root);
        check("Kopie und unberührte verschachtelte Zahlen bleiben exakt",
              out.find("9007199254740993") != std::string::npos &&
              out.find("-9223372036854775809") != std::string::npos &&
              json_write(copy).find("\"n\": 42") != std::string::npos);
        root.set("a", JsonValue::make_num(3, true));
        check("expliziter Ersatz verwirft den ursprünglichen Token",
              json_write(root).find("\"a\": 3,") != std::string::npos);
        check("neu erzeugtes 2^63 wird ohne undefinierten int-Cast geschrieben",
              json_write(JsonValue::make_num(9223372036854775808.0, true)) == "9223372036854775808\n");
        check("negatives neues 2^63 bleibt Ganzzahl",
              json_write(JsonValue::make_num(-9223372036854775808.0, true)) == "-9223372036854775808\n");
        const std::vector<std::pair<std::string, std::string>> increment = {
            {"0", "1"}, {"-1", "0"}, {"-10", "-9"}, {"99", "100"},
            {"9007199254740993", "9007199254740994"},
            {"-9007199254740993", "-9007199254740992"},
            {std::string(400, '9'), "1" + std::string(400, '0')}
        };
        for (const auto& [before, after] : increment) {
            JsonValue n = json_parse(before);
            n.increment();
            const std::string name = "exaktes Inkrement: " +
                                     (before.size() > 65 ? before.substr(0, 30) + "..." : before);
            check(name.c_str(), json_write(n) == after + "\n");
        }
        JsonValue kommazahl = json_parse("1.5");
        kommazahl.increment();
        check("Kommazahl-Inkrement bleibt Kommazahl", json_write(kommazahl) == "2.5\n");
        check("Integeranzeige bewahrt Endnullen",
              rune::editor::editor_number_text(100, 0) == "100" &&
              rune::editor::editor_number_text(-20, 0) == "-20");
        check("Anzeige kürzt nur Kommanullen",
              rune::editor::editor_number_text(10.5, 2) == "10.5" &&
              rune::editor::editor_number_text(10, 2) == "10");
    }

    std::printf("== Editor: Zufallskampf-Prüfung ==\n");
    {
        check("encounters fehlt", encounter_funde(R"({})").empty());
        check("encounters null", encounter_funde(R"({"encounters":null})").empty());
        check("optionale Felder null",
              encounter_funde(R"({"encounters":{"troops":null,"steps":null}})").empty());
        check("gültige Truppen und ganzzahlige Kommazahl",
              encounter_funde(R"({"encounters":{"troops":["waldweg","banditen"],"steps":2.0}})").empty());
        const auto objekt = encounter_funde(R"({"encounters":[]})");
        check("Liste statt Objekt: Fehler", objekt.size() == 1 && objekt[0].severity == 2 &&
              objekt[0].text == "ist kein Objekt");
        const auto liste = encounter_funde(R"({"encounters":{"troops":"waldweg"}})");
        check("Truppen keine Liste: Fehler", liste.size() == 1 && liste[0].severity == 2 &&
              liste[0].text == "'troops' ist keine Liste");
        const auto ids = encounter_funde(R"({"encounters":{"troops":["waldweg",42,"  "]}})");
        check("nur gültige IDs: zwei Fehler mit Index", ids.size() == 2 &&
              ids[0].severity == 2 && ids[0].where == "Karte encounters troops #2" &&
              ids[1].severity == 2 && ids[1].where == "Karte encounters troops #3");
        const auto null = encounter_funde(R"({"encounters":{"steps":0}})");
        const auto bruch = encounter_funde(R"({"encounters":{"steps":0.5}})");
        const auto text = encounter_funde(R"({"encounters":{"steps":"30"}})");
        check("0, Bruch und Text als Schrittzahl: Warnungen",
              null.size() == 1 && bruch.size() == 1 && text.size() == 1 &&
              null[0].severity == 1 && bruch[0].severity == 1 && text[0].severity == 1 &&
              bruch[0].text == "'steps' muss eine ganze Zahl > 0 sein");
    }

    encounter_form_tests(wurzel);
    blocked_form_tests();
    map_props_form_tests();
    placement_form_tests();

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
            std::printf("  --   %s nicht gefunden (übersprungen)\n", karte.c_str());
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
            check("Roundtrip: Schlüssel der Wurzel in gleicher Reihenfolge",
                  zurueck.obj.size() == v.obj.size() &&
                  zurueck.obj.front().first == v.obj.front().first &&
                  zurueck.obj.back().first == v.obj.back().first);
            check("Roundtrip: Event-Seiten überleben",
                  zurueck.find("events")->arr[0].find("pages") != nullptr);
            check("Roundtrip: Höhe-Pinsel überleben",
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
            std::printf("  --   %s nicht gefunden (übersprungen)\n", pfad.c_str());
        } else {
            const JsonValue v = json_parse(inhalt);
            const JsonValue* start = v.find("start");
            check("start-Block vorhanden", start && start->is_obj());
            const double yaw = start->find("yaw")->number();
            const std::string out = json_write(v);
            const double yaw_neu = json_parse(out).find("start")->find("yaw")->number();
            check("Startblickwinkel überlebt das Speichern bitgleich",
                  yaw == yaw_neu && std::memcmp(&yaw, &yaw_neu, sizeof(double)) == 0);
            check("Kartenname des Startpunkts bleibt", json_parse(out).find("start")->
                  find("map")->text() == "welt01");
        }
    }

    std::printf("\n%d Checks, %d Fehler\n", g_count, g_fail);
    std::printf(g_fail == 0 ? "JSON-TEST OK\n" : "JSON-TEST FEHLGESCHLAGEN\n");
    return g_fail == 0 ? 0 : 1;
}
