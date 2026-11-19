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
#include "editor/atomic_file_win.h"
#include "editor/database_form.h"
#include "editor/encounter_check.h"
#include "editor/encounter_form.h"
#include "editor/map_document.h"
#include "editor/map_props_form.h"
#include "editor/number_format.h"
#include "editor/object_props_form.h"
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

// Dieselben portablen Funktionen dienen dem F9-Dialog als Schreibkern.
void database_form_tests(const std::string& wurzel) {
    using namespace rune::editor;
    std::printf("== Editor: F9-Datenbank (Datenmodell) ==\n");
    const std::vector<DatabaseFieldSpec> specs = {
        {"id", DatabaseFieldKind::Text}, {"name", DatabaseFieldKind::Text},
        {"hp", DatabaseFieldKind::Integer}, {"drop_rate", DatabaseFieldKind::Decimal},
        {"enemies", DatabaseFieldKind::IdList}
    };
    JsonValue root = json_parse(R"({"actors":[{"id":"arik","name":"Arik",
      "hp":9007199254740993,"level":9007199254740995,"drop_rate":0.12345678901234567,
      "enemies":["wolf","wolf","bandit"],"extra":{"wichtig":true}},42],"andere":null})");
    const std::string original_root = json_write(root);
    const JsonValue original_entry = root.find("actors")->arr[0];
    DatabaseForm before = database_form_read(root, &root.find("actors")->arr[0], specs);
    std::string rate_text = json_write(*original_entry.find("drop_rate"));
    rate_text.pop_back();
    check("Anzeige zeigt große Ganzzahl und präzise Kommazahl unverändert",
          before.fields[2] == "9007199254740993" && before.fields[3] == rate_text && rate_text.size() > 5);
    check("Gegnerliste zeigt Reihenfolge und Duplikate", before.fields[4] == "wolf, wolf, bandit");
    check("fehlendes Gold wird als 0 angezeigt, aber nicht angelegt", before.gold == "0" && !root.find("gold"));
    DatabaseEdit edit = database_form_prepare(root, &root.find("actors")->arr[0], specs, before, before);
    check("unverändertes F9-Formular ändert auch fehlende Felder nicht", edit.ok && !edit.changed &&
          database_form_commit(root, "actors", 0, edit) && json_write(root) == original_root);

    DatabaseForm after = before;
    after.fields[1] = "Arik der Große";
    edit = database_form_prepare(root, &root.find("actors")->arr[0], specs, before, after);
    check("Entwurf bleibt ohne Commit unverändert", edit.ok && edit.entry_changed && !edit.gold_changed &&
          json_write(root) == original_root);
    check("Namensänderung übernommen", database_form_commit(root, "actors", 0, edit) &&
          root.find("actors")->arr[0].find("name")->str == "Arik der Große");
    const JsonValue& actor = root.find("actors")->arr[0];
    check("unberührte Integer, Kommazahlen, Level und Zusatzdaten bleiben exakt",
          json_write(*actor.find("hp")) == json_write(*original_entry.find("hp")) &&
          json_write(*actor.find("drop_rate")) == json_write(*original_entry.find("drop_rate")) &&
          json_write(*actor.find("level")) == json_write(*original_entry.find("level")) &&
          actor.find("extra")->find("wichtig")->b && !actor.find("exp") && !root.find("gold"));
    check("Nicht-Objekt in der Sammlung bleibt beim Ändern erhalten",
          root.find("actors")->arr[1].is_num() && root.find("actors")->arr[1].number() == 42);

    // Gold ist unabhängig von Selektion und Reiter. Root.set darf die
    // Speicheradresse der vorher bearbeiteten Sammlung verschieben.
    before = database_form_read(root, nullptr, specs);
    after = before;
    after.gold = "+0009007199254740993";
    const std::string actors_before = json_write(*root.find("actors"));
    edit = database_form_prepare(root, nullptr, specs, before, after);
    check("Gold ohne Auswahl unabhängig vom Eintrag übernehmen", edit.ok && !edit.entry_changed &&
          edit.gold_changed && database_form_commit(root, "items", -1, edit) &&
          json_write(*root.find("gold")) == "9007199254740993\n" &&
          json_write(*root.find("actors")) == actors_before && !root.find("items"));
    before = database_form_read(root, &root.find("actors")->arr[0], specs);
    after = before; after.gold = "-12"; after.fields[1] = "Neu";
    edit = database_form_prepare(root, &root.find("actors")->arr[0], specs, before, after);
    check("Gold und Eintrag gemeinsam übernehmen ohne veralteten Zeiger", edit.ok && edit.entry_changed &&
          edit.gold_changed && database_form_commit(root, "actors", 0, edit) &&
          root.find("actors")->arr[0].find("name")->str == "Neu" &&
          json_write(*root.find("gold")) == "-12\n");

    before = database_form_read(root, &root.find("actors")->arr[0], specs);
    const std::string before_bad = json_write(root);
    after = before; after.fields[1] = "Nicht übernehmen"; after.fields[2] = "12oops"; after.gold = "3";
    edit = database_form_prepare(root, &root.find("actors")->arr[0], specs, before, after);
    check("fehlerhaftes Feld verhindert alle Änderungen einschließlich Gold", !edit.ok && !edit.changed &&
          edit.field == "hp" && !database_form_commit(root, "actors", 0, edit) && json_write(root) == before_bad);
    after = before; after.fields[1] = "Nicht übernehmen"; after.gold = "1.5";
    edit = database_form_prepare(root, &root.find("actors")->arr[0], specs, before, after);
    check("ungültiges globales Gold wird gesondert gemeldet", !edit.ok && edit.gold_error &&
          json_write(root) == before_bad);
    after = before; after.fields[1] = "Name\nmit Zeile";
    edit = database_form_prepare(root, &root.find("actors")->arr[0], specs, before, after);
    check("Steuerzeichen in neuen Textfeldern werden abgelehnt", !edit.ok && edit.field == "name");
    after = before; after.fields[1] = std::string("A\0B", 3);
    check("eingebettetes NUL wird abgelehnt", !database_form_prepare(root, &root.find("actors")->arr[0],
                                                                     specs, before, after).ok);
    const std::vector<DatabaseFieldSpec> enemy_gold = {{"gold", DatabaseFieldKind::Integer}};
    const JsonValue enemy = json_parse(R"({"gold":5})");
    before = database_form_read(root, &enemy, enemy_gold); after = before; after.fields[0] = "falsch";
    edit = database_form_prepare(root, &enemy, enemy_gold, before, after);
    check("Fehler im Gegner-Gold ist nicht das globale Gold", !edit.ok && edit.field == "gold" && !edit.gold_error);

    JsonValue number;
    check("Ganzzahl-Eingabe normalisiert Vorzeichen und führende Nullen",
          database_integer(" +0000120 ", number) && json_write(number) == "120\n" &&
          database_integer("-000", number) && json_write(number) == "0\n");
    const std::string huge(400, '9');
    check("neue 400-stellige Ganzzahl wird nicht gerundet", database_integer(huge, number) &&
          json_write(number) == huge + "\n");
    for (const char* invalid : {"", "+", "--3", "1.2", "1e3", "7xyz", "NaN", "0x10"}) {
        const std::string label = std::string("ungültige Ganzzahl abgelehnt: ") + invalid;
        check(label.c_str(), !database_integer(invalid, number));
    }
    check("Kommazahl akzeptiert Dezimalkomma und Exponent", database_decimal("1,25e-1", number) &&
          !number.is_int && number.number() == 0.125);
    for (const char* invalid : {"", "NaN", "inf", "1e9999", "1e-9999", "0x1p2", "1,2,3", "8text"}) {
        const std::string label = std::string("ungültige Kommazahl abgelehnt: ") + invalid;
        check(label.c_str(), !database_decimal(invalid, number));
    }

    JsonValue list;
    check("Gegnerlisten akzeptieren Komma/Semikolon, Umlaute und Duplikate",
          database_ids_parse(" wolf, wölfchen; wolf ", list) && list.arr.size() == 3 &&
          list.arr[0].str == "wolf" && list.arr[1].str == "wölfchen" && list.arr[2].str == "wolf");
    check("Gegnerlisten lehnen Steuerzeichen ab", !database_ids_parse("wolf,\n bandit", list) &&
          !database_ids_parse(std::string("wolf\0bandit", 11), list));
    JsonValue mixed_root = json_parse(R"({"troops":[{"name":"Alt","enemies":["wolf",42," x ","a,b",""]}],"gold":0})");
    const std::vector<DatabaseFieldSpec> troop = {{"name", DatabaseFieldKind::Text},
                                                  {"enemies", DatabaseFieldKind::IdList}};
    const JsonValue* mixed = &mixed_root.find("troops")->arr[0];
    before = database_form_read(mixed_root, mixed, troop);
    check("nicht eindeutig darstellbare alte Gegnerliste erkannt",
          !database_ids_representable(mixed->find("enemies")) &&
          before.fields[1].find("[ungültiger Eintrag]") != std::string::npos);
    after = before; after.fields[0] = "Neu";
    edit = database_form_prepare(mixed_root, mixed, troop, before, after);
    check("andere Änderung erhält fehlerhafte alte Gegnerliste ohne Rückfrage", edit.ok &&
          !edit.replaces_invalid && json_write(*edit.entry.find("enemies")) == json_write(*mixed->find("enemies")));
    after = before; after.fields[1] = "wolf, bandit";
    const std::string mixed_before = json_write(mixed_root);
    edit = database_form_prepare(mixed_root, mixed, troop, before, after);
    check("Ersetzen fehlerhafter Gegnerdaten verlangt Rückfrage und verändert noch nichts",
          edit.ok && edit.replaces_invalid && json_write(mixed_root) == mixed_before);
    check("nach Bestätigung lassen sich Gegnerdaten ausdrücklich ersetzen",
          database_form_commit(mixed_root, "troops", 0, edit) &&
          mixed_root.find("troops")->arr[0].find("enemies")->arr.size() == 2);
    const JsonValue damaged = json_parse(R"({"enemies":"keine Liste","name":{"extra":1}})");
    check("falsche Typen sind sichtbar und als nicht darstellbar markiert",
          database_ids_text(damaged.find("enemies")) == "[ungültige Gegnerliste]" &&
          !database_ids_representable(damaged.find("enemies")) &&
          database_field_text(damaged.find("name"), DatabaseFieldKind::Text) == "[ungültig]");
    const JsonValue hidden_ids = json_parse(R"(["","Zeile\nneu"])");
    check("leere und nicht einzeilige IDs sind als Platzhalter sichtbar und reparierbar",
          database_ids_text(&hidden_ids) == "[leere ID], [nicht darstellbare ID]" &&
          !database_ids_representable(&hidden_ids));
    check("alte Zeilenumbrüche und NUL im Einzelfeld müssen gesperrt werden",
          !database_single_line("eine\nZeile") && !database_single_line(std::string("A\0B", 3)) &&
          database_single_line("Äpfel und Leerzeichen"));

    JsonValue reallocate = json_parse(R"({"actors":[{"name":"Alt"}]})");
    const std::vector<DatabaseFieldSpec> names = {{"name", DatabaseFieldKind::Text}};
    before = database_form_read(reallocate, &reallocate.find("actors")->arr[0], names);
    after = before; after.fields[0] = "Geändert"; after.gold = "100";
    edit = database_form_prepare(reallocate, &reallocate.find("actors")->arr[0], names, before, after);
    check("fehlendes Gold hinzufügen invalidiert keinen benötigten Eintragszeiger",
          database_form_commit(reallocate, "actors", 0, edit) &&
          reallocate.find("actors")->arr[0].find("name")->str == "Geändert" &&
          reallocate.find("gold")->number() == 100);
    const std::string before_stale = json_write(reallocate);
    check("veraltete Auswahl verhindert die gesamte Übernahme",
          !database_form_commit(reallocate, "actors", 9, edit) && json_write(reallocate) == before_stale);
    const JsonValue invalid_entry = json_parse("42");
    before = database_form_read(reallocate, &invalid_entry, names); after = before; after.gold = "101";
    edit = database_form_prepare(reallocate, &invalid_entry, names, before, after);
    check("Goldänderung auch mit ungültigem Eintrag möglich", edit.ok && edit.gold_changed && !edit.entry_changed);
    after.fields[0] = "unerlaubt";
    check("Nicht-Objekt lässt sich nicht still durch Objekt ersetzen",
          !database_form_prepare(reallocate, &invalid_entry, names, before, after).ok);
    check("nicht endliche Kommazahlen schützen die ganze Datenbank beim Laden",
          !database_document_serializable(json_parse(R"({"x":[1e400]})")) &&
          database_document_serializable(json_parse("{\"x\":" + huge + "}")));

    JsonValue loaded = json_parse(R"({"behalten":true})");
    const std::string saved_load = json_write(loaded);
    const std::string invalid_utf8 = std::string("{\"a\":\"") + std::string("\xc0\x80", 2) + "\"}";
    const std::vector<std::pair<std::string, DatabaseParseStatus>> bad_documents = {
        {"", DatabaseParseStatus::InvalidJson},
        {"{kaputt}", DatabaseParseStatus::InvalidJson},
        {"{\"name\":\"ungültig\nZeile\"}", DatabaseParseStatus::InvalidJson},
        {"[]", DatabaseParseStatus::InvalidRoot},
        {invalid_utf8, DatabaseParseStatus::InvalidUtf8},
        {R"({"a":"\uD800"})", DatabaseParseStatus::InvalidUtf8},
        {R"({"a":1,"a":2})", DatabaseParseStatus::DuplicateKey},
        {R"({"array":[{"a":1,"\u0061":2}]})", DatabaseParseStatus::DuplicateKey},
        {R"({"a":[1e999]})", DatabaseParseStatus::Nonfinite}
    };
    for (const auto& [text, status] : bad_documents) {
        check("defekte Datenbank blockiert Laden und erhält bisherigen Baum",
              database_parse_document(text, loaded) == status && json_write(loaded) == saved_load);
    }
    check("gültiges Unicode und maskierte Steuerzeichen bleiben ladbar",
          database_parse_document(R"({"name":"Äpfel 🍎","code":"a\n\u0000b","other":"\\\""})", loaded) ==
              DatabaseParseStatus::Ok && loaded.find("name")->str == "Äpfel 🍎" &&
          loaded.find("code")->str == std::string("a\n\0b", 4));
    check("UTF-8-Prüfung erkennt Überlänge, Surrogate, unvollständige und zu große Codepoints",
          database_valid_utf8("Äpfel 🍎") && !database_valid_utf8(std::string("\xc0\x80", 2)) &&
          !database_valid_utf8(std::string("\xed\xa0\x80", 3)) &&
          !database_valid_utf8(std::string("\xf0\x9f", 2)) &&
          !database_valid_utf8(std::string("\xf4\x90\x80\x80", 4)) &&
          !database_single_line(std::string("\xc0\x80", 2)));
    check("Dateikonfliktprüfung erkennt Änderungen, Neuanlage und Löschung",
          !database_disk_changed(false, "", false, "") &&
          !database_disk_changed(true, "original", true, "original") &&
          database_disk_changed(true, "original", true, "extern") &&
          database_disk_changed(true, "original", false, "") &&
          database_disk_changed(false, "", true, "neu"));
    for (const char* path : {"game/data/database.json", "projekte/dorf/data/database.json"}) {
        std::string text;
        JsonValue file;
        const bool ok = datei_lesen(wurzel + "/" + path, text) &&
                        database_parse_document(text, file) == DatabaseParseStatus::Ok;
        const std::string label = std::string("mitgelieferte Datenbank lässt sich sicher laden: ") + path;
        check(label.c_str(), ok);
    }

    std::string data;
    if (datei_lesen(wurzel + "/game/data/database.json", data)) {
        JsonValue demo = json_parse(data);
        const std::string initial = json_write(demo);
        const JsonValue* demo_actor = &demo.find("actors")->arr[0];
        before = database_form_read(demo, demo_actor, specs);
        edit = database_form_prepare(demo, demo_actor, specs, before, before);
        check("echte Demo-Datenbank bleibt bei unveränderter F9-Übernahme erhalten",
              edit.ok && !edit.changed && database_form_commit(demo, "actors", 0, edit) &&
              json_write(demo) == initial && json_write(json_parse(initial)) == initial);
    } else {
        check("Demo-Datenbank zum F9-Test lesbar", false);
    }
}

void object_props_tests(const std::string& wurzel) {
    using namespace rune::editor;
    std::printf("== Editor: Objekt-Eigenschaften datentreu ==\n");
    const JsonValue object = json_parse(R"({"type":"haus","x":9007199254740993,
      "z":1.2345678901234567,"rot":1.234567890123,"tex":"altes Material",
      "y":2.75,"extra":{"farbe":"blau"}})");
    const std::string original = json_write(object);
    const ObjectPropsForm before = object_props_read(object);
    check("Objektformular zeigt große Ganzzahl und präzise Kommazahl", before.x == "9007199254740993" &&
          before.z == placement_num_text(object.find("z")->number()));
    check("Material und Drehung werden geladen", before.material == "altes Material" && !before.rotation.empty());
    check("Snapshot-Gleichheit erkennt unveränderte Eingaben", object_props_equal(before, before));
    ObjectPropsForm pending = before; pending.material += "x";
    check("Snapshot-Gleichheit erkennt unübernommene Eingaben", !object_props_equal(before, pending));
    ObjectPropsEdit edit = object_props_prepare(object, before, before);
    check("unverändertes Formular erzeugt keine Mutation und keinen Undo-Bedarf",
          edit.ok && !edit.changed && json_write(edit.value) == original);

    ObjectPropsForm after = before;
    after.material = "  neues Material  ";
    edit = object_props_prepare(object, before, after);
    check("Materialänderung wird getrimmt und vorbereitet", edit.ok && edit.changed && !edit.replaces_invalid &&
          edit.value.find("tex")->str == "neues Material" && json_write(object) == original);
    check("nur Material ändern bewahrt exakte Koordinaten, Rotation, Höhe und Zusatzdaten",
          json_write(*edit.value.find("x")) == json_write(*object.find("x")) &&
          json_write(*edit.value.find("z")) == json_write(*object.find("z")) &&
          json_write(*edit.value.find("rot")) == json_write(*object.find("rot")) &&
          edit.value.find("y")->number() == 2.75 && edit.value.find("extra")->find("farbe")->str == "blau");
    JsonValue items = JsonValue::make_arr(); items.arr.push_back(object);
    const std::string undo = json_write(items);
    items.arr[0] = edit.value;
    const JsonValue restored = json_parse(undo);
    check("Objekt-Schnappschuss stellt bei Materialänderung alle Werte wieder her",
          json_write(restored.arr[0]) == original && json_write(items.arr[0]) != original);

    after = before; after.material.clear();
    edit = object_props_prepare(object, before, after);
    check("leeres Material entfernt nur tex", edit.ok && edit.changed && !edit.value.find("tex") &&
          json_write(*edit.value.find("x")) == json_write(*object.find("x")) && edit.value.find("y"));
    after = before; after.x = "2,75";
    edit = object_props_prepare(object, before, after);
    check("geändertes X akzeptiert Dezimalkomma, Z/Rotation bleiben exakt",
          edit.ok && edit.changed && edit.value.find("x")->number() == 2.75 &&
          json_write(*edit.value.find("z")) == json_write(*object.find("z")) &&
          json_write(*edit.value.find("rot")) == json_write(*object.find("rot")));
    after = before; after.rotation = "90";
    edit = object_props_prepare(object, before, after);
    check("Grad-Drehung wird als Radiant gespeichert und X bleibt als Integer erhalten",
          edit.ok && edit.changed && std::fabs(edit.value.find("rot")->number() - 1.5707963267948966) < 1e-14 &&
          json_write(*edit.value.find("x")) == "9007199254740993\n");
    after = before; after.x = "12xyz"; after.z = "4"; after.material = "wichtig";
    edit = object_props_prepare(object, before, after);
    check("ungültige Zahl blockiert sämtliche Änderungen", !edit.ok && !edit.changed && edit.field == "x" &&
          json_write(object) == original);
    after = before; after.x = "4"; after.rotation = "inf";
    edit = object_props_prepare(object, before, after);
    check("späterer Fehler verhindert auch vorher gültiges X", !edit.ok && !edit.changed && edit.field == "rotation" &&
          json_write(object) == original && json_write(edit.value) == original);
    for (const char* invalid : {"", "NaN", "1e999", "1000001", "-1000001", "2,3,4"}) {
        after = before; after.z = invalid;
        const std::string label = std::string("ungültige oder zu große Objektzahl abgelehnt: ") + invalid;
        check(label.c_str(), !object_props_prepare(object, before, after).ok);
    }
    after = before; after.x = std::string("1\0anderes", 9);
    check("NUL in Zahl wird nicht still abgeschnitten", !object_props_prepare(object, before, after).ok);
    after = before; after.x = "1\t";
    check("Steuerzeichen in Zahl werden abgelehnt", !object_props_prepare(object, before, after).ok);
    after = before; after.x = "1000000";
    check("erlaubter Zahlen-Grenzwert wird übernommen", object_props_prepare(object, before, after).ok);
    after = before; after.material = "Zeile\nneu";
    check("Zeilenumbruch in Material wird abgelehnt", !object_props_prepare(object, before, after).ok);
    after.material = std::string("a\0b", 3);
    check("NUL in Material wird abgelehnt", !object_props_prepare(object, before, after).ok);
    after.material = std::string("\xed\xa0\x80", 3);
    check("ungültiges UTF-8 in Material wird abgelehnt", !object_props_prepare(object, before, after).ok);

    const JsonValue empty = json_parse(R"({"type":"baum","extra":true})");
    const ObjectPropsForm empty_form = object_props_read(empty);
    edit = object_props_prepare(empty, empty_form, empty_form);
    check("fehlende x/z/rot/tex bleiben beim Übernehmen fehlend", edit.ok && !edit.changed &&
          !edit.value.find("x") && !edit.value.find("z") && !edit.value.find("rot") && !edit.value.find("tex"));
    after = empty_form; after.material = "  ";
    edit = object_props_prepare(empty, empty_form, after);
    check("nur Leerzeichen im bereits leeren Material erzeugen keinen Undo-Bedarf", edit.ok && !edit.changed);

    const JsonValue damaged = json_parse(R"({"x":{"sicher":true},"z":2,"tex":["alt"],"extra":5})");
    const ObjectPropsForm broken_form = object_props_read(damaged);
    after = broken_form; after.z = "3";
    edit = object_props_prepare(damaged, broken_form, after);
    check("andere Änderung erhält ungültige Altwerte ohne Rückfrage", edit.ok && !edit.replaces_invalid &&
          json_write(*edit.value.find("x")) == json_write(*damaged.find("x")) &&
          json_write(*edit.value.find("tex")) == json_write(*damaged.find("tex")));
    after = broken_form; after.x = "0";
    check("kaputte Koordinate ersetzen erfordert Bestätigung", object_props_prepare(damaged, broken_form, after).replaces_invalid);
    after = broken_form; after.material = "stein";
    check("kaputtes Material ersetzen erfordert Bestätigung", object_props_prepare(damaged, broken_form, after).replaces_invalid);
    after = broken_form; after.material.clear();
    check("kaputtes Material entfernen erfordert Bestätigung", object_props_prepare(damaged, broken_form, after).replaces_invalid);

    const JsonValue duplicates = json_parse(R"({"x":1,"x":2,"z":7,"tex":"a","tex":"b"})");
    const ObjectPropsForm duplicate_form = object_props_read(duplicates);
    check("doppelte editierbare Schlüssel werden erkannt", !object_props_field_unique(duplicates, "x") &&
          !object_props_field_unique(duplicates, "tex") && object_props_field_unique(duplicates, "z"));
    after = duplicate_form; after.x = "4";
    check("doppeltes X darf nicht mehrdeutig überschrieben werden",
          !object_props_prepare(duplicates, duplicate_form, after).ok);
    after = duplicate_form; after.material = "neu";
    check("doppeltes Material darf nicht mehrdeutig überschrieben werden",
          !object_props_prepare(duplicates, duplicate_form, after).ok);
    after = duplicate_form; after.z = "8";
    edit = object_props_prepare(duplicates, duplicate_form, after);
    check("andere Felder ändern erhält doppelte Altdaten", edit.ok && edit.changed &&
          edit.value.obj.size() == duplicates.obj.size() && edit.value.find("z")->number() == 8);

    const std::string huge(400, '9');
    const JsonValue huge_object = json_parse("{\"x\":" + huge + ",\"z\":0,\"tex\":\"alt\"}");
    const ObjectPropsForm huge_form = object_props_read(huge_object);
    after = huge_form; after.material = "neu";
    edit = object_props_prepare(huge_object, huge_form, after);
    check("400-stellige Altkoordinate bleibt bei Materialänderung exakt",
          huge_form.x == huge && edit.ok && json_write(*edit.value.find("x")) == huge + "\n");
    const JsonValue huge_rotation = json_parse(R"({"rot":1e308,"tex":"alt"})");
    const ObjectPropsForm rotation_form = object_props_read(huge_rotation);
    after = rotation_form; after.material = "neu";
    edit = object_props_prepare(huge_rotation, rotation_form, after);
    check("nicht darstellbare Grad-Anzeige verändert alte Radiant-Zahl nicht", rotation_form.rotation == "ungültig" &&
          edit.ok && edit.value.find("rot")->number() == huge_rotation.find("rot")->number());
    after = rotation_form; after.rotation = "30";
    check("nicht darstellbare Drehungsanzeige ersetzen erfordert Bestätigung",
          object_props_prepare(huge_rotation, rotation_form, after).replaces_invalid);
    const JsonValue broken_text = json_parse(R"({"tex":"A\nB","x":1})");
    const ObjectPropsForm text_form = object_props_read(broken_text);
    after = text_form; after.material = "neu";
    check("nicht einzeiligen Alt-Materialnamen ersetzen erfordert Bestätigung",
          object_props_prepare(broken_text, text_form, after).replaces_invalid);
    JsonValue long_material = json_parse(R"({"x":1,"z":2})");
    long_material.set("tex", JsonValue::make_str(std::string(9000, 'a')));
    const ObjectPropsForm long_form = object_props_read(long_material);
    after = long_form; after.x = "5";
    edit = object_props_prepare(long_material, long_form, after);
    check("langes unverändertes Material wird nicht abgeschnitten", long_form.material.size() == 9000 &&
          edit.ok && edit.value.find("tex")->str.size() == 9000);
    const JsonValue invalid = json_parse("42");
    check("Nicht-Objekt wird nicht still ersetzt", !object_props_prepare(invalid, ObjectPropsForm{}, ObjectPropsForm{}).ok);

    const JsonValue movable = json_parse(R"({"x":1,"z":2,"rot":0,"y":0.25})");
    check("normales Objekt ist für Vorschau und Picking geeignet", object_props_viewable(movable));
    check("extreme Ganzzahl, kaputter Typ und doppelte Koordinate bleiben aus der Vorschau",
          !object_props_viewable(object) && !object_props_viewable(huge_object) &&
          !object_props_viewable(damaged) && !object_props_viewable(duplicates));
    check("fehlende oder null-Koordinaten sind als Standardposition darstellbar",
          object_props_viewable(empty) && object_props_viewable(json_parse(R"({"x":null,"z":0})")));
    ObjectDragEdit drag = object_drag_prepare(movable, movable, 0.0, 0.0, 1.0);
    check("Klick ohne Bewegung erzeugt keinen Drag und keinen Undo-Bedarf",
          drag.ok && !drag.changed && json_write(drag.value) == json_write(movable));
    drag = object_drag_prepare(movable, movable, 0.3, -0.2, 1.0);
    check("Bewegung innerhalb einer halben Rasterzelle erzeugt keine Änderung", drag.ok && !drag.changed);
    drag = object_drag_prepare(movable, movable, 2.0, 0.0, 1.0);
    check("Drag auf einer Achse bewahrt den Integer-Token der anderen Achse",
          drag.ok && drag.changed && drag.value.find("x")->number() == 3.0 &&
          drag.value.find("z")->is_int && json_write(*drag.value.find("z")) == "2\n" &&
          json_write(*drag.value.find("y")) == json_write(*movable.find("y")));
    drag = object_drag_prepare(drag.value, movable, 0.0, 0.0, 1.0);
    check("Zurückziehen stellt ursprünglichen Zahlentyp und Position wieder her",
          drag.ok && drag.changed && json_write(drag.value) == json_write(movable));
    const JsonValue precise = json_parse(R"({"x":1.2345678901234567,"z":2.345678901234567,"y":1})");
    drag = object_drag_prepare(precise, precise, 1.0, 0.0, 1.0);
    check("Drag in X lässt präzises Z unverändert", drag.ok && drag.changed &&
          json_write(*drag.value.find("z")) == json_write(*precise.find("z")));
    drag = object_drag_prepare(drag.value, precise, 0.0, 0.0, 1.0);
    check("Zurückziehen erhält ursprüngliche präzise Zahl bitgenau",
          drag.ok && json_write(drag.value) == json_write(precise));
    drag = object_drag_prepare(empty, empty, 1.0, 0.0, 1.0);
    check("Drag legt nur die tatsächlich bewegte fehlende Achse an",
          drag.ok && drag.changed && drag.value.find("x") && !drag.value.find("z"));
    drag = object_drag_prepare(drag.value, empty, 0.0, 0.0, 1.0);
    check("Zurückziehen entfernt die ursprünglich fehlende Achse wieder",
          drag.ok && drag.changed && json_write(drag.value) == json_write(empty));
    const JsonValue null_axis = json_parse(R"({"x":null,"z":0})");
    drag = object_drag_prepare(null_axis, null_axis, 1.0, 0.0, 1.0);
    drag = object_drag_prepare(drag.value, null_axis, 0.0, 0.0, 1.0);
    check("Zurückziehen stellt ursprüngliches null wieder her",
          drag.ok && drag.changed && drag.value.find("x")->is_null());
    drag = object_drag_prepare(movable, movable, 2.0, 2000000.0, 1.0);
    check("ungültiges Drag-Ziel verwirft beide Achsen ohne Teiländerung",
          !drag.ok && !drag.changed && json_write(drag.value) == json_write(movable));
    check("ungültiges Raster wird abgelehnt", !object_drag_prepare(movable, movable, 1.0, 1.0, 0.0).ok);

    for (const char* path : {"game/data/maps/dorf.json", "projekte/dorf/data/maps/dorf.json"}) {
        std::string text;
        bool ok = datei_lesen(wurzel + "/" + path, text);
        JsonValue map;
        if (ok) ok = map_parse_document(text, map) == MapParseStatus::Ok;
        const JsonValue* objects = ok ? map.find("objects") : nullptr;
        ok = objects && objects->is_arr() && !objects->arr.empty();
        if (ok) {
            const std::string map_before = json_write(map);
            for (size_t i = 0; i < objects->arr.size(); ++i) {
                const JsonValue& item = objects->arr[i];
                if (!item.is_obj()) continue;
                const ObjectPropsForm form = object_props_read(item);
                const ObjectPropsEdit unchanged = object_props_prepare(item, form, form);
                if (!unchanged.ok || unchanged.changed || json_write(unchanged.value) != json_write(item)) ok = false;
            }
            ok = ok && json_write(map) == map_before;
        }
        const std::string label = std::string("Objekte echter Karte bleiben unverändert: ") + path;
        check(label.c_str(), ok);
    }
}

void map_document_tests(const std::string& wurzel) {
    using namespace rune::editor;
    std::printf("== Editor: sichere Karten-Dokumente ==\n");
    JsonValue loaded = json_parse(R"({"alt":true})");
    const std::string old = json_write(loaded);
    const std::string huge(400, '9');
    check("Karten-Dateischnappschuss: unveränderte Bytes sind kein Konflikt",
          !editor_disk_changed(true, "alt", true, "alt"));
    check("Karten-Dateischnappschuss: externe Änderung wird erkannt",
          editor_disk_changed(true, "alt", true, "neu"));
    check("Karten-Dateischnappschuss: externe Löschung wird erkannt",
          editor_disk_changed(true, "alt", false, ""));
    check("Karten-Dateischnappschuss: Neuanlage wird erkannt",
          editor_disk_changed(false, "", true, "neu"));
    check("Karten-Dateischnappschuss: zwei fehlende Dateien sind gleich",
          !editor_disk_changed(false, "", false, ""));
    const std::vector<std::pair<std::string, MapParseStatus>> invalid = {
        {"{", MapParseStatus::InvalidJson},
        {"[]", MapParseStatus::InvalidRoot},
        {"{\"name\":\"Zeile\nnein\"}", MapParseStatus::InvalidJson},
        {R"({"name":"\ud800\u0041"})", MapParseStatus::InvalidUtf8},
        {std::string("{\"name\":\"") + std::string("\xc0\x80", 2) + "\"}", MapParseStatus::InvalidUtf8},
        {R"({"objects":[{"x":1e999}]})", MapParseStatus::Nonfinite},
        {R"({"objects":[],"objects":[]})", MapParseStatus::DuplicateObjects},
        {R"({"objects":{"wert":42}})", MapParseStatus::InvalidObjects},
        {R"({"objects":null})", MapParseStatus::InvalidObjects}
    };
    for (const auto& [text, expected] : invalid) {
        check("nicht sicher ladbare Karte erhält bisherigen Baum",
              map_parse_document(text, loaded) == expected && json_write(loaded) == old);
    }
    const std::string valid = "{\"objects\":[{\"x\":" + huge +
                              ",\"z\":2,\"z\":3,\"tex\":\"Äpfel 🍎\"}]}";
    check("große Integer und doppelte Objekt-Felder werden datentreu geladen",
          map_parse_document(valid, loaded) == MapParseStatus::Ok &&
          json_write(*loaded.find("objects")->arr[0].find("x")) == huge + "\n" &&
          !object_props_field_unique(loaded.find("objects")->arr[0], "z"));
    check("fehlende Objektliste darf weiterhin als leer angelegt werden",
          map_parse_document(R"({"name":"Leer"})", loaded) == MapParseStatus::Ok && !loaded.find("objects"));
    check("gültiges Surrogatpaar und maskiertes NUL werden erhalten",
          map_parse_document(R"({"name":"\ud83c\udf4e","note":"a\u0000b"})", loaded) == MapParseStatus::Ok &&
          loaded.find("name")->str == "🍎" && loaded.find("note")->str == std::string("a\0b", 3));
    for (const char* path : {"game/data/maps/dorf.json", "projekte/dorf/data/maps/dorf.json"}) {
        std::string text;
        const bool ok = datei_lesen(wurzel + "/" + path, text) && map_parse_document(text, loaded) == MapParseStatus::Ok;
        const std::string label = std::string("mitgelieferte Karte lässt sich sicher laden: ") + path;
        check(label.c_str(), ok);
    }
}

#ifdef _WIN32
// Echter Dateitest ohne GUI: Die gemeinsame Karte/F9-Schreibfunktion arbeitet
// mit Unicode-Pfaden und darf bestehende Daten bei Fehlern nicht beschädigen.
bool win_test_read(const std::wstring& path, std::string& out) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ,
                              FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                              nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    std::string result;
    char buffer[8192];
    bool ok = true;
    for (;;) {
        DWORD count = 0;
        if (!ReadFile(file, buffer, sizeof(buffer), &count, nullptr)) { ok = false; break; }
        if (!count) break;
        result.append(buffer, count);
    }
    if (!CloseHandle(file)) ok = false;
    if (ok) out = std::move(result);
    return ok;
}

void atomic_file_windows_tests() {
    using namespace rune::editor;
    std::printf("== Windows: sicheres Datei-Ersetzen ==\n");
    wchar_t base[MAX_PATH + 1]{};
    const DWORD length = GetTempPathW(MAX_PATH, base);
    if (!length || length >= MAX_PATH) {
        check("temporäres Testverzeichnis verfügbar", false);
        return;
    }
    std::wstring dir;
    bool created = false;
    for (int i = 0; i < 100; ++i) {
        dir = std::wstring(base, length) + L"rune-dateitest-" +
              std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(i);
        if (CreateDirectoryW(dir.c_str(), nullptr)) { created = true; break; }
        if (GetLastError() != ERROR_ALREADY_EXISTS) break;
    }
    check("temporäres Testverzeichnis verfügbar", created);
    if (!created) return;
    const std::wstring path = dir + L"\\karte-\u00c4.json";
    const std::string original("alt\0wert", 8);
    std::string read;
    const bool first = editor_write_file_atomic(path, original, false);
    check("neue Datei unter Unicode-Pfad wird vollständig erstellt",
          first && win_test_read(path, read) && read == original);
    const bool existing = editor_write_file_atomic(path, "nicht überschreiben", false);
    check("Neuanlage überschreibt keine inzwischen vorhandene Datei",
          !existing && win_test_read(path, read) && read == original);

    // Eine offene Datei ohne FILE_SHARE_DELETE erzwingt einen Fehlschlag beim
    // Ersetzen, nachdem die temporäre Datei bereits geschrieben wurde.
    HANDLE lock = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    const bool blocked = lock != INVALID_HANDLE_VALUE && !editor_write_file_atomic(path, "neu");
    if (lock != INVALID_HANDLE_VALUE) CloseHandle(lock);
    check("fehlgeschlagenes Ersetzen erhält ursprüngliche Dateibytes",
          blocked && win_test_read(path, read) && read == original);
    WIN32_FIND_DATAW found{};
    HANDLE search = FindFirstFileW((dir + L"\\*.tmp").c_str(), &found);
    const bool no_temporary = search == INVALID_HANDLE_VALUE && GetLastError() == ERROR_FILE_NOT_FOUND;
    if (search != INVALID_HANDLE_VALUE) FindClose(search);
    check("fehlgeschlagenes Ersetzen räumt die temporäre Datei auf", no_temporary);

    std::string large(1024 * 1024 + 17, 'x');
    large[10] = '\0';
    large.back() = 'z';
    check("Ersetzen schreibt auch mehrere Blöcke vollständig",
          editor_write_file_atomic(path, large) && win_test_read(path, read) && read == large);
    DeleteFileW(path.c_str());
    check("Testverzeichnis nach Dateioperationen entfernbar", RemoveDirectoryW(dir.c_str()) != 0);
}
#endif

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
    database_form_tests(wurzel);
    map_document_tests(wurzel);
    object_props_tests(wurzel);
#ifdef _WIN32
    atomic_file_windows_tests();
#endif

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
