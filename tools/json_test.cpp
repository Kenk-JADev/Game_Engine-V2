// ============================================================================
//  json_test.cpp — Kopfloser Test für die JSON-Schicht des Editors (19a).
//
//  Warum in C++ und nicht in Ruby? src/editor/json.cpp ist der einzige Weg,
//  auf dem der RuneEditor Karten liest und schreibt. Wenn dieser Parser oder
//  Schreiber etwas verschluckt (Reihenfolge, unbekannte Schlüssel, int vs.
//  float, Umlaute), ist eine Karte nach dem Speichern kaputt — und der Fehler
//  fällt erst im Spiel auf. Der Test hängt an keiner Win32-/D3D11-/Ruby-
// Abhängigkeit und läuft deshalb sowohl unter MSYS2 (build.ps1 -Test) als
// auch in CI auf Linux. Unter Windows kommen unsichtbare Win32-Controltests
// hinzu; sie ersetzen keinen grafischen Live-Test. Bau in einer Zeile:
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
#include "editor/event_form.h"
#include "editor/event_layout.h"
#include "editor/event_view_win.h"
#include "editor/listbox_win.h"
#include "editor/main_layout.h"
#include "editor/main_panel_win.h"
#include "editor/main_tree_win.h"
#include "editor/map_document.h"
#include "editor/map_props_form.h"
#include "editor/number_format.h"
#include "editor/object_props_form.h"
#include "editor/placement_form.h"
#include "editor/terrain_preview.h"
#include "editor/texture_path.h"
#include "editor/tile_form.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <limits>
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

void texture_path_tests() {
    using rune::editor::texture_file_is_flat;
    std::printf("== Editor: RGBA-Dateinamen ohne Pfadwechsel ==\n");
    check("normale, Unicode- und versteckte Dateinamen zulässig",
          texture_file_is_flat("gras.rgba") && texture_file_is_flat("Fels-ä.rgba") &&
          texture_file_is_flat(".kachel.rgba"));
    check("relative Pfade mit beiden Trennern abgewiesen",
          !texture_file_is_flat("../database.json") && !texture_file_is_flat("..\\database.json"));
    check("absolute und Netzwerkpfade abgewiesen",
          !texture_file_is_flat("/data/file.rgba") && !texture_file_is_flat("\\\\server\\datei.rgba"));
    check("Laufwerke, Datenströme und Windows-Sonderzeichen abgewiesen",
          !texture_file_is_flat("C:bild.rgba") && !texture_file_is_flat("bild.rgba:stream") &&
          !texture_file_is_flat("bild?.rgba") && !texture_file_is_flat("bild|.rgba"));
    check("NUL und Steuerzeichen abgewiesen",
          !texture_file_is_flat(std::string("bild\0.rgba", 10)) && !texture_file_is_flat("a\nb.rgba"));
    check("Windows-Gerätenamen unabhängig von Großschreibung und Erweiterung abgewiesen",
          !texture_file_is_flat("con.rgba") && !texture_file_is_flat("NUL.txt.rgba") &&
          !texture_file_is_flat("Com9.rgba") && !texture_file_is_flat("lpt1.x") &&
          !texture_file_is_flat("CON .rgba") && !texture_file_is_flat("com\xC2\xB9.rgba") &&
          texture_file_is_flat("com9grafik.rgba"));
    check("leere Namen, Punktpfade und normalisierte Enden abgewiesen",
          !texture_file_is_flat("") && !texture_file_is_flat(".") && !texture_file_is_flat("..") &&
          !texture_file_is_flat("bild.") && !texture_file_is_flat("bild "));
    check("Dateiname auf 255 Bytes begrenzt",
          texture_file_is_flat(std::string(255, 'a')) && !texture_file_is_flat(std::string(256, 'a')));
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

void terrain_preview_tests(const std::string& wurzel) {
    using namespace rune::editor;
    using namespace rune::world;
    std::printf("== Editor: sichere Terrain-/Atmosphären-Vorschau ==\n");
    const std::string huge(400, '9');
    JsonValue root = json_parse("{\"name\":\"Alt\",\"objects\":[],\"terrain\":{\"width\":" + huge +
        ",\"height\":240,\"seed\":" + huge + ",\"amplitude\":1e300,\"heights\":[]},"
        "\"fog\":{\"on\":true,\"far\":1e300,\"color\":" + huge + "},"
        "\"panorama\":{\"color\":-999999999999999999999},"
        "\"weather\":{\"type\":\"regen\",\"intensity\":1e300}}");
    const std::string old = json_write(root);
    const TerrainPreview terrain = terrain_preview_read(root);
    check("extremes Terrain wird vor allen Casts abgewiesen", !terrain.ok() && !terrain.error.empty());
    const AtmospherePreview atmosphere = atmosphere_preview_read(root);
    check("Nebel/Himmel mit sicheren Ersatzwerten und Warnungen",
          atmosphere.sky == 0x3D6FB5 && atmosphere.fog_on && atmosphere.fog_color == 0xBFD8EC &&
          atmosphere.fog_far == 180.0f && atmosphere.warnings.size() == 4);
    check("Vorschau verändert auch übergroße Ganzzahlen niemals", json_write(root) == old &&
          json_write(*root.find("terrain")->find("width")) == huge + "\n");
    check("F2-Zahlenanzeige erhält extremen Originaltoken",
          map_props_number_display(root.find("terrain")->find("width"), 240) == huge &&
          map_props_number_display(root.find("terrain")->find("seed"), 7) == huge &&
          map_props_color_display(root.find("fog")->find("color"), 0) == "ungültig");
    MapPropsForm before;
    before.name = "Alt"; before.width = huge; before.height = "240"; before.seed = huge;
    before.amplitude = "1e300"; before.mountains = true; before.fog_on = true;
    before.fog_far = "1e300"; before.fog_color = "ungültig";
    before.panorama_color = "ungültig"; before.weather_intensity = "1e300";
    MapPropsForm after = before;
    after.name = "Neu";
    const BlockedEdit blocked = blocked_form_prepare(nullptr, "", "");
    check("nur Name ändern erlaubt und Altdaten bleiben exakt",
          map_props_check(root, before, after).ok && map_props_apply(root, before, after, blocked).changed &&
          json_write(*root.find("terrain")->find("width")) == huge + "\n" &&
          json_write(*root.find("fog")->find("color")) == huge + "\n");
    const std::string after_name = json_write(root);
    after = before; after.width = "1e300"; after.name = "darf nicht gespeichert werden";
    check("ungültige F2-Zahl blockiert alle Änderungen atomar",
          !map_props_check(root, before, after).ok && !map_props_apply(root, before, after, blocked).changed &&
          json_write(root) == after_name);
    after = before; after.seed = "4294967296";
    check("Seed über uint32 abgewiesen", !map_props_check(root, before, after).ok);
    after.seed = "4294967295";
    check("oberer Seed im Formular erlaubt", map_props_check(root, before, after).ok);
    after = before; after.fog_far = "nan";
    check("NaN-Feld wird abgewiesen", !map_props_check(root, before, after).ok);
    after = before; after.weather_intensity = "1e300"; after.width = "nan";
    check("unsichere Eingabe trotz anderer unveränderter Altwerte erkannt", !map_props_check(root, before, after).ok);
    after = before; after.weather_intensity = "0";
    check("Wetterintensität null wird abgewiesen", !map_props_check(root, before, after).ok);
    after.weather_intensity = "0,00000001";
    const MapPropsCheck tiny_weather = map_props_check(root, before, after);
    check("kleine positive Wetterintensität bleibt zulässig", tiny_weather.ok);
    after = before; after.panorama_color = "kaputt";
    check("ungültige Hexfarbe erzeugt kein stilles Schwarz", !map_props_check(root, before, after).ok);
    after = before; after.fog_on = false;
    check("nur Nebelschalter ändert keine extreme Farbe oder Entfernung",
          map_props_apply(root, before, after, blocked).changed &&
          json_write(*root.find("fog")->find("color")) == huge + "\n" &&
          root.find("fog")->find("far")->number() == 1e300);

    JsonValue duplicate = json_parse(R"({"terrain":{"width":240,"width":30,"height":120}})");
    check("doppelte Rasterfelder sperren Vorschau", !terrain_preview_read(duplicate).ok());
    MapPropsForm d0, d1; d0.width = "240"; d1 = d0; d1.width = "100";
    check("doppelte Rasterfelder durch F2 nicht überschreiben", !map_props_check(duplicate, d0, d1).ok);
    JsonValue wrong = json_parse(R"({"terrain":[1,2,3],"name":"X"})");
    check("nichtobjektartiges Terrain nicht durch F2 ersetzen", !map_props_check(wrong, d0, d1).ok);
    JsonValue unused_size = json_parse("{\"terrain\":{\"size\":" + huge + ",\"width\":240,\"height\":120}}");
    check("ungültiges, nicht verwendetes Legacy-size stört gültige width/height nicht",
          terrain_preview_read(unused_size).ok());
    const TerrainPreview fallback_size = terrain_preview_read(json_parse(R"({"terrain":{"size":150,"resolution":64}})"));
    check("alte size-Karten bleiben unterstützt", fallback_size.ok() && fallback_size.plan.width == 150 &&
          fallback_size.plan.height == 150 && fallback_size.plan.res_x == 64);
    const TerrainPreview bad_ratio = terrain_preview_read(json_parse(R"({"terrain":{"width":1,"height":10000,"resolution":192}})"));
    check("Seitenverhältnis wird schon im Editor abgewiesen", !bad_ratio.ok());
    const TerrainPreview bad_brush = terrain_preview_read(json_parse(
        "{\"terrain\":{\"heights\":[{\"x\":" + huge + ",\"z\":0,\"r\":1,\"h\":1}]}}"));
    check("extremer Höhenpinsel verhindert statt verfälscht die Vorschau", !bad_brush.ok());
    const TerrainPreview bad_splat = terrain_preview_read(json_parse(
        "{\"terrain\":{\"splats\":[{\"tex\":\"weg\",\"x\":0,\"z\":0,\"w\":1,\"h\":" + huge + "}]}}"));
    check("extreme Bodenfläche verhindert statt verfälscht die Vorschau", !bad_splat.ok());
    SplatRect s;
    check("ungültige Listenzeile nicht mit gültiger verwechseln",
          !preview_splat(json_parse(R"({"tex":"weg","x":1,"z":2,"w":0,"h":1})"), s) &&
          preview_splat(json_parse(R"({"tex":"weg","x":1,"z":2,"w":2,"h":1})"), s) && s.x == 1);
    const JsonValue splat_list = json_parse(R"([false,{"tex":"weg","x":1,"z":2,"w":2,"h":1},)"
        R"({"tex":"gras","x":3,"x":4,"z":0,"w":1,"h":1},)"
        R"({"tex":"stein","x":5,"z":0,"w":1,"h":1}])");
    const SplatListModel rows = splat_list_model(&splat_list);
    check("Splat-Zeilen einschließlich ungültiger behalten die Originalindizes",
          rows.rows.size() == 4 && !rows.rows[0].valid && rows.rows[0].original_index == 0 &&
          rows.rows[1].valid && rows.rows[1].original_index == 1 && rows.rows[1].splat.tex == "weg" &&
          !rows.rows[2].valid && rows.rows[2].original_index == 2 &&
          rows.rows[3].valid && rows.rows[3].original_index == 3 && rows.omitted == 0);
    JsonValue removed_root = JsonValue::make_obj();
    JsonValue removed_terrain = JsonValue::make_obj();
    removed_terrain.set("splats", splat_list);
    removed_root.set("terrain", std::move(removed_terrain));
    check("ungültige Splat-Zeile benötigt Bestätigung und bleibt bei Abbruch erhalten",
          splat_delete_state(removed_root, 0) == SplatDeleteState::NeedsConfirmation &&
          !splat_delete_apply(removed_root, 0, false) &&
          removed_root.find("terrain")->find("splats")->arr.size() == 4);
    const auto original = static_cast<std::ptrdiff_t>(rows.rows[3].original_index);
    const bool removed = splat_delete_apply(removed_root, original, false);
    const JsonValue* after_delete = removed_root.find("terrain")->find("splats");
    check("Löschen über Originalindex entfernt nicht den ungültigen Vorgänger",
          removed && after_delete->arr.size() == 3 && !after_delete->arr[0].is_obj() &&
          after_delete->arr[1].find("tex")->str == "weg" && !preview_unique(after_delete->arr[2], "x"));
    check("bestätigtes Löschen entfernt nur die gewählte ungültige Zeile",
          splat_delete_apply(removed_root, 0, true) &&
          removed_root.find("terrain")->find("splats")->arr.size() == 2 &&
          removed_root.find("terrain")->find("splats")->arr[0].find("tex")->str == "weg");
    JsonValue singleton = json_parse(R"({"terrain":{"extra":42,"splats":[false]}})");
    check("nach letzter Löschung bleibt die vorhandene leere Liste samt Nachbarfeldern erhalten",
          splat_delete_apply(singleton, 0, true) && singleton.find("terrain")->find("splats")->is_arr() &&
          singleton.find("terrain")->find("splats")->arr.empty() && singleton.find("terrain")->find("extra")->number() == 42);
    JsonValue ambiguous = json_parse(R"({"terrain":{"splats":[false],"splats":[true]}})");
    const std::string ambiguous_before = json_write(ambiguous);
    check("doppelte Splat-Schlüssel werden nicht bearbeitet",
          splat_delete_state(ambiguous, 0) == SplatDeleteState::Unavailable &&
          !splat_delete_apply(ambiguous, 0, true) && json_write(ambiguous) == ambiguous_before);
    JsonValue many_rows = JsonValue::make_arr();
    many_rows.arr.resize(rune::world::TERRAIN_MAX_SPLATS + 3);
    const SplatListModel limited = splat_list_model(&many_rows);
    check("überlange Splat-Liste begrenzt anzeigen und Auslassung melden",
          limited.rows.size() == rune::world::TERRAIN_MAX_SPLATS && limited.omitted == 3 &&
          limited.rows.back().original_index == rune::world::TERRAIN_MAX_SPLATS - 1);
    JsonValue many_root = JsonValue::make_obj(), many_terrain = JsonValue::make_obj();
    many_terrain.set("splats", std::move(many_rows));
    many_root.set("terrain", std::move(many_terrain));
    check("Auslassungszeile und ausgeblendete Indizes sind nicht löschbar",
          splat_delete_state(many_root, -1) == SplatDeleteState::Unavailable &&
          splat_delete_state(many_root, static_cast<std::ptrdiff_t>(TERRAIN_MAX_SPLATS)) == SplatDeleteState::Unavailable &&
          !splat_delete_apply(many_root, -1, true) &&
          !splat_delete_apply(many_root, static_cast<std::ptrdiff_t>(TERRAIN_MAX_SPLATS), true) &&
          many_root.find("terrain")->find("splats")->arr.size() == TERRAIN_MAX_SPLATS + 3);
    const TerrainPreview bad_tile = terrain_preview_read(json_parse(R"({"terrain":{"tile":1e-300}})"));
    check("extreme Kachelgröße nur für Vorschau ersetzen und warnen", bad_tile.ok() &&
          bad_tile.tile == 2 && bad_tile.warnings.size() == 1);
    const JsonValue nul_texture = json_parse(R"({"terrain":{"texture":"a\u0000b"}})");
    const TerrainPreview nul_view = terrain_preview_read(nul_texture);
    check("NUL im Basisnamen warnt, zeigt Ersatz und bewahrt JSON", nul_view.ok() &&
          nul_view.texture == "gras" && !nul_view.warnings.empty() &&
          nul_texture.find("terrain")->find("texture")->str.size() == 3);
    const JsonValue nul_splat = json_parse(R"({"tex":"a\u0000b","x":0,"z":0,"w":1,"h":1})");
    check("NUL im Flächennamen verhindert unsichere Ruby-/UI-Übergabe", !preview_splat(nul_splat, s));
    JsonValue long_name = json_parse(R"({"terrain":{"texture":""}})");
    long_name.find("terrain")->find("texture")->str = std::string(TERRAIN_MAX_TEXTURE_BYTES + 1, 'x');
    const std::string long_before = json_write(long_name);
    const TerrainPreview long_view = terrain_preview_read(long_name);
    check("übergroßer Basisname wird nicht kopiert oder im JSON ersetzt", long_view.ok() &&
          long_view.texture == "gras" && !long_view.warnings.empty() && json_write(long_name) == long_before);
    const JsonValue old_tile = json_parse("{\"texture\":\"gras\",\"tile\":" + huge + ",\"extra\":true}");
    const TileFormEdit tex_only = tile_form_prepare(&old_tile, "gras", "weg", huge, huge);
    check("Tileset-Texturwechsel bewahrt extremen, unberührten tile-Token und Zusatzfelder",
          tex_only.ok && tex_only.changed && json_write(*tex_only.value.find("tile")) == huge + "\n" &&
          tex_only.value.find("extra")->b && tex_only.value.find("texture")->str == "weg");
    check("Tileset ohne Änderung schreibt nichts", !tile_form_prepare(&old_tile, "gras", "gras", huge, huge).changed);
    const JsonValue invalid_tile = json_parse(R"({"texture":"gras","tile":"kaputt","extra":5})");
    const TileFormEdit invalid_preserved = tile_form_prepare(&invalid_tile, "gras", "weg", "ungültig", "ungültig");
    check("Texturänderung bewahrt auch einen nichtnumerischen tile-Altwert",
          invalid_preserved.ok && invalid_preserved.value.find("tile")->str == "kaputt" &&
          invalid_preserved.value.find("extra")->number() == 5);
    const TileFormEdit repair = tile_form_prepare(&old_tile, "gras", "gras", huge, "2.5");
    check("bewusst bearbeitete Kachelgröße lässt sich nach Rückfrage reparieren", repair.ok && repair.changed &&
          repair.replaces_invalid && repair.value.find("tile")->number() == 2.5);
    const TileFormEdit comma = tile_form_prepare(&old_tile, "gras", "gras", huge, "2,75");
    check("Dezimalkomma wird validiert und korrekt gespeichert", comma.ok && comma.value.find("tile")->number() == 2.75);
    check("ungültige Kachelgröße blockiert auch Texturänderung",
          !tile_form_prepare(&old_tile, "gras", "weg", huge, "1e300").ok &&
          json_write(*old_tile.find("tile")) == huge + "\n");
    const JsonValue duplicates = json_parse(R"({"texture":"gras","tile":2,"tile":3})");
    check("doppeltes tile beim gezielten Ändern geschützt",
          !tile_form_prepare(&duplicates, "gras", "weg", "2", "4").ok &&
          tile_form_prepare(&duplicates, "gras", "weg", "2", "2").ok);
    const JsonValue missing_texture = json_parse(R"({"tile":2,"extra":true})");
    const TileFormEdit only_size = tile_form_prepare(&missing_texture, "gras", "gras", "2", "3");
    check("Kacheländerung legt fehlende Basistextur nicht unbemerkt an", only_size.ok && only_size.changed &&
          !only_size.value.find("texture") && only_size.value.find("tile")->number() == 3);
    const JsonValue invalid_texture = json_parse(R"({"texture":{"alt":true},"tile":2})");
    const TileFormEdit preserve_texture = tile_form_prepare(&invalid_texture, "", "", "2", "3");
    check("Kacheländerung erhält ungültiges Texturobjekt vollständig", preserve_texture.ok &&
          preserve_texture.value.find("texture")->find("alt")->b);
    const TileFormEdit replace_texture = tile_form_prepare(&invalid_texture, "", "gras", "2", "2");
    check("gezieltes Ersetzen ungültiger Textur verlangt Rückfrage", replace_texture.ok &&
          replace_texture.replaces_invalid && replace_texture.value.find("texture")->str == "gras");
    const AtmospherePreview malformed = atmosphere_preview_read(json_parse(
        R"({"fog":{"on":"ja","far":-1,"color":1e300},"weather":{"intensity":-2}})"));
    check("fehlerhafte Atmosphäre mit definierten Ersatzwerten", !malformed.fog_on &&
          malformed.fog_far == 180 && malformed.fog_color == 0xBFD8EC && malformed.warnings.size() == 4);
    std::string file;
    if (datei_lesen(wurzel + "/projekte/dorf/data/maps/dorf.json", file)) {
        const JsonValue demo = json_parse(file);
        const TerrainPreview demo_view = terrain_preview_read(demo);
        check("mitgelieferte Dorfkarte bleibt vollständig darstellbar", demo_view.ok() &&
              demo_view.plan.res_x == 192 && demo_view.brushes.size() == 35 && json_write(demo).find("\"heights\"") != std::string::npos);
    } else check("mitgelieferte Dorfkarte lesbar", false);
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
    MapPropsForm prompt_snapshot = before;
    check("F2-Rückfrage erkennt denselben Formularstand", prompt_snapshot == before);
    prompt_snapshot.blocked += "\nEntwurf";
    check("F2-Rückfrage erkennt währenddessen geänderte Felder", !(prompt_snapshot == before));
    prompt_snapshot = before;
    prompt_snapshot.fog_on = !before.fog_on;
    check("F2-Rückfrage erkennt geänderte Schalter", !(prompt_snapshot == before));
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

// C++ erzeugt reale F6-Edits; tools/event_bridge_check.rb liest sie mit der
// tatsächlichen Ruby-Event-/Interpreter-Laufzeit, nicht mit einem Nachbau.
int event_bridge_modus(int argc, char** argv) {
    using namespace rune::editor;
    if (argc != 3) { std::fprintf(stderr, "Aufruf: json_test --event-bridge <ausgabe.json>\n"); return 2; }
    const JsonValue old = json_parse(R"({"id":7,"name":"Brücke","x":1.23456789012345,"z":9007199254740993,"radius":2.75,"trigger":"touch","commands":[{"cmd":"if_switch","name":"quest","value":true,"then":[{"cmd":"text","lines":["Alt"],"extra":{"integer":9007199254740993}}],"else":[{"cmd":"text","lines":["Zu"]}],"condition_meta":42}],"visual":"schild","extra":{"keep":true}})");
    EventBatch batch;
    batch.page = 0;
    batch.event_fields = true; batch.event_before = event_form_read(old); batch.event_after = batch.event_before;
    batch.event_after.name = "Brücke neu";
    batch.page_fields = true; batch.page_before = event_page_form_read(old, 0); batch.page_after = batch.page_before;
    batch.page_after.switch_name = "offen"; batch.page_after.switch_value = 0;
    batch.command = EventCommandAction::Update; batch.path = {{0, "then", 0}}; batch.command_index = 0;
    const JsonValue& text = old.find("commands")->arr[0].find("then")->arr[0];
    batch.command_before = event_command_form_read(text); batch.command_after = batch.command_before;
    batch.command_after.text = "Neu";
    const EventEdit migrated = event_batch_prepare(old, batch);
    const EventEdit added = event_add_page_prepare(old);
    const EventEdit copied = event_duplicate_prepare(old, 8);
    JsonValue root = JsonValue::make_obj(), events = JsonValue::make_arr(), npcs = JsonValue::make_arr();
    events.arr.push_back(old); events.arr.push_back(copied.value);
    JsonValue npc = JsonValue::make_obj(); npc.set("name", JsonValue::make_str("Brücke (Kopie) 2"));
    npcs.arr.push_back(std::move(npc));
    root.set("events", std::move(events)); root.set("npcs", std::move(npcs));
    const EventEdit unique_copy = event_duplicate_prepare(old, 9, &root);
    EventCommandForm number_form; number_form.type = ET_INPUT_NUMBER; number_form.name = "anzahl";
    number_form.val1 = "2"; number_form.val2 = "99"; number_form.flag = 1;
    const EventEdit number = event_command_prepare(nullptr, EventCommandForm{}, number_form);
    EventPageForm trigger_before = event_page_form_read(old, 0), trigger_after = trigger_before;
    trigger_after.trigger = 3;
    const EventEdit trigger = event_page_prepare(old, 0, trigger_before, trigger_after);
    const JsonValue choice = json_parse(R"({"cmd":"choices","options":["Ja","Nein"],"branches":[[{"cmd":"log","text":"eins"}],[{"cmd":"log","text":"zwei"}],[{"extra":123}]],"meta":{"keep":true},"cancel":"verzweigung","cancel_branch":[{"cmd":"log","text":"abgebrochen"}]})");
    EventCommandForm cb = event_command_form_read(choice), ca = cb; ca.text = "Nein\r\nJa";
    const EventEdit reordered = event_command_prepare(&choice, cb, ca);
    if (!migrated.ok || !migrated.renames_event || !added.ok || !copied.ok || !unique_copy.ok || !number.ok ||
        !trigger.ok || !reordered.ok || !reordered.reassigns_choices) {
        std::fprintf(stderr, "F6-Brückenbeispiele konnten nicht vorbereitet werden.\n"); return 1;
    }
    JsonValue result = JsonValue::make_obj();
    result.set("old", old); result.set("migrated", migrated.value); result.set("added", added.value);
    result.set("copy", copied.value); result.set("unique_copy", unique_copy.value);
    result.set("number", number.value); result.set("trigger", trigger.value);
    result.set("choice_old", choice); result.set("choice_reordered", reordered.value);
    if (!datei_schreiben(argv[2], json_write(result))) { std::fprintf(stderr, "F6-Brückendatei nicht schreibbar.\n"); return 1; }
    return 0;
}

void event_form_tests(const std::string& wurzel) {
    using namespace rune::editor;
    std::printf("== Editor: F6-Datentreue und Transaktionen ==\n");
    const JsonValue legacy = json_parse(R"({"id":5,"name":"Alt","x":1.23456789012345,"z":9007199254740993,"radius":2.123456789,"trigger":"touch","commands":[{"cmd":"if_switch","name":"quest","value":true,"meta":{"n":9007199254740993},"then":[{"cmd":"choices","options":["Ja","Nein"],"branches":[[{"cmd":"move_player","x":1.23456789,"z":9007199254740993,"relative":false,"custom":{"keep":7}}],[]],"extra":42}],"else":[]}],"visual":"schild","unknown":{"a":1}})");
    const std::string saved = json_write(legacy);
    check("Altformat zeigt eine implizite Seite ohne Migration", event_page_count(legacy) == 1 &&
          event_legacy_page(legacy) && event_page_view(legacy, 0) == &legacy && json_write(legacy) == saved);
    check("alte Befehle und Trigger werden nicht beim Lesen erzeugt/verschoben",
          event_commands_view(legacy, 0) && event_commands_view(legacy, 0)->arr.size() == 1 &&
          event_page_form_read(legacy, 0).trigger == 1 && json_write(legacy) == saved);
    const JsonValue empty = json_parse(R"({"name":"Leer"})");
    check("fehlende Felder und Befehle werden beim Lesen nicht angelegt",
          event_page_count(empty) == 1 && !event_commands_view(empty, 0) && json_write(empty) == json_write(json_parse(R"({"name":"Leer"})")));
    const JsonValue broken_pages = json_parse(R"({"pages":{"fremd":42},"trigger":"touch","commands":[]})");
    check("ungültiges pages wird wie die Laufzeit als implizite Seite angesehen, aber erhalten",
          event_legacy_page(broken_pages) && event_page_count(broken_pages) == 1 &&
          !event_add_page_prepare(broken_pages).ok && broken_pages.find("pages")->is_obj());
    const JsonValue duplicates = json_parse(R"({"pages":[],"pages":[{}],"x":1,"x":2})");
    check("mehrfaches pages wird nicht anhand des ersten Treffers bearbeitet", event_page_count(duplicates) == 0 &&
          !event_add_page_prepare(duplicates).ok);
    const JsonValue bad_page = json_parse(R"({"pages":[null,"fremd",{"commands":[]}]})");
    check("kaputte Seiten bleiben einzeln erhalten und nicht editierbar", event_page_count(bad_page) == 3 &&
          !event_page_view(bad_page, 0) && !event_page_view(bad_page, 1) && event_page_view(bad_page, 2));

    EventForm ev_before = event_form_read(legacy), ev_after = ev_before;
    check("Event zeigt exakte Ganzzahl und ausreichend präzise Kommazahl",
          ev_before.z == "9007199254740993" && ev_before.x == database_number_text(legacy.find("x")));
    EventEdit edit = event_form_prepare(legacy, ev_before, ev_before);
    check("unverändertes Event bleibt bytegleich im JSON-Modell", edit.ok && !edit.changed && json_write(edit.value) == saved);
    ev_after.name = "Neu";
    edit = event_form_prepare(legacy, ev_before, ev_after);
    check("nur Name geändert: Position, Radius, Seiten und Zusatzdaten unverändert",
          edit.ok && edit.changed && json_write(*edit.value.find("x")) == json_write(*legacy.find("x")) &&
          json_write(*edit.value.find("z")) == json_write(*legacy.find("z")) &&
          json_write(*edit.value.find("commands")) == json_write(*legacy.find("commands")) &&
          json_write(*edit.value.find("unknown")) == json_write(*legacy.find("unknown")) && !edit.value.find("pages"));
    check("Event-Umbenennung warnt vor Änderungen an Spielstand-/Self-Switch-Schlüsseln", edit.renames_event);
    EventForm position_only = ev_before; position_only.x = "4";
    check("reine Positionsänderung löst keine Umbenennungswarnung aus",
          event_form_prepare(legacy, ev_before, position_only).ok &&
          !event_form_prepare(legacy, ev_before, position_only).renames_event);
    ev_after.x = "1,25"; ev_after.z = "kaputt";
    edit = event_form_prepare(legacy, ev_before, ev_after);
    check("ungültige Koordinate rollt auch vorherige Feldänderungen zurück", !edit.ok && !edit.changed && json_write(edit.value) == saved);
    for (const std::string& invalid : {std::string(""), std::string("NaN"), std::string("1e999"), std::string("1x"),
                                      std::string("0x1"), std::string("1000001"), std::string("2\0x", 3)}) {
        ev_after = ev_before; ev_after.x = invalid;
        check("ungültige Event-Position wird vollständig abgelehnt", !event_form_prepare(legacy, ev_before, ev_after).ok);
    }
    ev_after = ev_before; ev_after.radius = "0";
    check("Radius muss positiv sein", !event_form_prepare(legacy, ev_before, ev_after).ok);
    const JsonValue bad_event = json_parse(R"({"name":42,"x":"kaputt","z":1})");
    ev_before = event_form_read(bad_event); ev_after = ev_before; ev_after.x = "3";
    check("Ersetzen ungültiger Positionsdaten erfordert Bestätigung", event_form_prepare(bad_event, ev_before, ev_after).replaces_invalid);
    ev_after = ev_before; ev_after.name = "Heilung";
    check("Ersetzen ungültiger Namensdaten erfordert Bestätigung", event_form_prepare(bad_event, ev_before, ev_after).replaces_invalid);
    const JsonValue dup_x = json_parse(R"({"x":1,"x":2,"name":"n"})");
    ev_before = event_form_read(dup_x); ev_after = ev_before; ev_after.x = "3";
    check("mehrfaches bearbeitetes Event-Feld wird abgelehnt", !event_form_prepare(dup_x, ev_before, ev_after).ok);
    ev_after = ev_before; ev_after.name = "m";
    check("unberührte doppelte Event-Felder bleiben erhalten", event_form_prepare(dup_x, ev_before, ev_after).ok &&
          !event_unique(event_form_prepare(dup_x, ev_before, ev_after).value, "x"));
    const JsonValue nul_name = json_parse(R"({"name":"a\u0000b"})");
    ev_before = event_form_read(nul_name); ev_after = ev_before; ev_after.name = "sicher";
    check("nicht darstellbarer Name kann nur mit Warnung ersetzt werden", event_form_prepare(nul_name, ev_before, ev_after).replaces_invalid);

    EventPageForm pg_before = event_page_form_read(legacy, 0), pg_after = pg_before;
    pg_after.trigger = 3;
    edit = event_page_prepare(legacy, 0, pg_before, pg_after);
    check("Trigger im Altformat bleibt Top-Level und erzeugt kein pages", edit.ok && edit.changed &&
          !edit.value.find("pages") && edit.value.find("trigger")->str == "parallel" &&
          json_write(*edit.value.find("commands")) == json_write(*legacy.find("commands")));
    pg_after = pg_before; pg_after.switch_name = "tor"; pg_after.switch_value = 0;
    edit = event_page_prepare(legacy, 0, pg_before, pg_after);
    check("neue Bedingung migriert erst beim Bearbeiten und erhält alte Felder",
          edit.ok && edit.value.find("pages") && edit.value.find("pages")->arr.size() == 1 &&
          json_write(*edit.value.find("commands")) == json_write(*legacy.find("commands")) &&
          json_write(*event_commands_view(edit.value, 0)) == json_write(*legacy.find("commands")) &&
          edit.value.find("trigger")->str == "touch" &&
          event_page_view(edit.value, 0)->find("conditions")->find("switch")->find("name")->str == "tor");
    const EventEdit added = event_add_page_prepare(legacy);
    check("Neue Seite migriert und erhält den alten Auslöser und alle Befehle",
          added.ok && event_page_count(added.value) == 2 && added.value.find("trigger")->str == "touch" &&
          json_write(*event_commands_view(added.value, 0)) == json_write(*legacy.find("commands")));
    check("Seite löschen erhält Event- und Seitenmetadaten", added.ok && event_delete_page_prepare(added.value, 1).ok &&
          event_page_count(event_delete_page_prepare(added.value, 1).value) == 1 &&
          json_write(*event_delete_page_prepare(added.value, 1).value.find("unknown")) == json_write(*legacy.find("unknown")));
    check("letzte Seite kann nicht gelöscht werden", !event_delete_page_prepare(legacy, 0).ok);
    for (const char* text : {R"({"trigger":"unbekannt","commands":[]})", R"({"trigger":3,"commands":[]})",
                             R"({"trigger":"touch","commands":{"alt":1}})", R"({"trigger":"touch","trigger":"auto"})"}) {
        const JsonValue v = json_parse(text);
        const EventEdit bad = event_add_page_prepare(v);
        check("nicht äquivalente/mehrdeutige Altdaten verhindern Migration", !bad.ok && json_write(bad.value) == json_write(v));
    }
    const JsonValue empty_pages = json_parse(R"({"pages":[],"trigger":"auto","commands":[{"cmd":"wait","seconds":1}]})");
    check("leeres pages-Array migriert wie das implizite Altformat", event_add_page_prepare(empty_pages).ok &&
          event_page_count(event_add_page_prepare(empty_pages).value) == 2);

    const JsonValue rich = json_parse(R"({"pages":[{"trigger":"action","conditions":{"other":{"keep":7},"switch":{"name":"a","value":true,"meta":123},"variable":{"name":"v","op":">=","value":12345678901234567890,"units":"gold"},"self_switch":{"letter":"B","value":true,"extra":9}},"commands":[],"page_extra":5}]})");
    pg_before = event_page_form_read(rich, 0); pg_after = pg_before; pg_after.switch_name = "b";
    edit = event_page_prepare(rich, 0, pg_before, pg_after);
    const JsonValue* rich_cond = edit.ok ? event_page_view(edit.value, 0)->find("conditions") : nullptr;
    check("Schaltername ändert nur sein Feld und erhält Metadaten und andere Bedingungen",
          rich_cond && rich_cond->find("switch")->find("meta")->number() == 123 &&
          json_write(*rich_cond->find("variable")) == json_write(*event_page_view(rich, 0)->find("conditions")->find("variable")) &&
          rich_cond->find("other") && rich_cond->find("self_switch"));
    pg_after = pg_before; pg_after.variable_value = "3,5"; pg_after.self_value = 1;
    edit = event_page_prepare(rich, 0, pg_before, pg_after);
    rich_cond = edit.ok ? event_page_view(edit.value, 0)->find("conditions") : nullptr;
    check("Variablen-/Self-Switch-Änderung erhält Unterobjekt-Zusatzdaten",
          rich_cond && rich_cond->find("variable")->find("units")->str == "gold" &&
          rich_cond->find("self_switch")->find("extra")->number() == 9 &&
          rich_cond->find("variable")->find("value")->number() == 3.5);
    pg_after = pg_before; pg_after.switch_name.clear();
    edit = event_page_prepare(rich, 0, pg_before, pg_after);
    check("Entfernen einer Bedingung warnt und erhält fremde Bedingungen", edit.ok && edit.removes_condition &&
          !event_page_view(edit.value, 0)->find("conditions")->find("switch") &&
          event_page_view(edit.value, 0)->find("conditions")->find("other"));
    pg_after = pg_before; pg_after.trigger = 1; pg_after.variable_value = "ungültig";
    edit = event_page_prepare(rich, 0, pg_before, pg_after);
    check("ungültiger Bedingungswert verwirft Trigger und alle übrigen Seitenänderungen",
          !edit.ok && !edit.changed && json_write(edit.value) == json_write(rich));
    const JsonValue dup_cond = json_parse(R"({"pages":[{"conditions":{"switch":{"name":"a","name":"b","value":true}}}]})");
    pg_before = event_page_form_read(dup_cond, 0); pg_after = pg_before; pg_after.switch_name = "c";
    check("mehrfacher Bedingungsname blockiert nur diese Änderung", !event_page_prepare(dup_cond, 0, pg_before, pg_after).ok);
    pg_after = pg_before; pg_after.trigger = 1;
    check("Triggeränderung lässt doppelte Bedingungen unangetastet", event_page_prepare(dup_cond, 0, pg_before, pg_after).ok);
    const JsonValue bad_cond = json_parse(R"({"pages":[{"conditions":{"fremd":1},"trigger":"seltsam"}]})");
    pg_before = event_page_form_read(bad_cond, 0); pg_after = pg_before; pg_after.trigger = 2;
    check("unbekannter Trigger wird nicht als action überschrieben", pg_before.trigger == -1 &&
          event_page_prepare(bad_cond, 0, pg_before, pg_after).replaces_invalid);

    const std::vector<std::string> samples = {
        R"({"cmd":"text","lines":["a","b"]})", R"({"cmd":"choices","options":["a"],"branches":[[]]})",
        R"({"cmd":"log","text":"a\nb"})", R"({"cmd":"wait","seconds":1.23456789})",
        R"({"cmd":"switch","name":"a","value":true})", R"({"cmd":"toggle_switch","name":"a"})",
        R"({"cmd":"variable","name":"a","value":9007199254740993,"add":0.123456789})",
        R"({"cmd":"if_switch","name":"a","value":false,"then":[{"cmd":"log","text":"x"}],"else":[]})",
        R"({"cmd":"if_variable","name":"a","value":1.23456789,"op":">=","then":[],"else":[]})",
        R"({"cmd":"script","name":"a","code":"side_effect"})", R"({"cmd":"teleport","x":1,"z":2,"yaw":3.141592653589793})",
        R"({"cmd":"move_player","x":1.23456789,"z":2,"relative":false})", R"({"cmd":"heal","amount":10})",
        R"({"cmd":"weather","name":"regen","intensity":0.123456789})", R"({"cmd":"effect","name":"burst","height":1,"color":0})",
        R"({"cmd":"sound","name":"a","volume":0.123456789})", R"({"cmd":"item","name":"a","take":9007199254740993})",
        R"({"cmd":"gold","amount":9007199254740993})", R"({"cmd":"save_game","slot":2})", R"({"cmd":"load_game","slot":3})",
        R"({"cmd":"self_switch","letter":"A","value":true})", R"({"cmd":"if_self_switch","letter":"B","value":false,"then":[],"else":[]})",
        R"({"cmd":"fade","direction":"ein","seconds":0.123456789})", R"({"cmd":"move_route","target":"x","steps":["up"],"speed":2.123456789,"repeat":false,"wait":true,"code":"a"})",
        R"({"cmd":"input_number","name":"a","digits":3,"initial":99,"cancel":false})", R"({"cmd":"battle","name":"a","can_escape":false,"can_lose":true})"
    };
    check("Beispiel für jeden der 26 Befehlstypen vorhanden", samples.size() == ET_COUNT);
    for (size_t i = 0; i < samples.size(); ++i) {
        JsonValue c = json_parse(samples[i]); c.set("extra_meta", json_parse(R"({"n":9007199254740993})"));
        const EventCommandForm f = event_command_form_read(c);
        const EventEdit noop = event_command_prepare(&c, f, f);
        const std::string label = "unberührter Befehl inkl. Zusatzdaten: " + std::string(EV_TYPES[i]);
        check(label.c_str(), f.type == static_cast<int>(i) && noop.ok && !noop.changed && json_write(noop.value) == json_write(c));
        const EventEdit created = event_command_prepare(nullptr, EventCommandForm{}, f);
        const std::string create_label = "Befehl aus gültigem Formular neu erzeugen: " + std::string(EV_TYPES[i]);
        check(create_label.c_str(), created.ok && created.changed && created.value.find("cmd") &&
              created.value.find("cmd")->str == EV_TYPES[i]);
        EventCommandForm changed = f;
        switch (i) {
            case ET_TEXT: case ET_CHOICES: case ET_LOG: changed.text = "neuer Text"; break;
            case ET_WAIT: case ET_TELEPORT: case ET_MOVE_PLAYER: case ET_HEAL: case ET_GOLD: changed.val1 = "4"; break;
            case ET_SAVE_GAME: case ET_LOAD_GAME: changed.val1 = "1"; break;
            case ET_SELF_SWITCH: case ET_IF_SELF_SWITCH: changed.name = "C"; break;
            case ET_FADE: changed.name = "aus"; break;
            default: changed.name = "neuer_name"; break;
        }
        const EventEdit patched = event_command_prepare(&c, f, changed);
        const std::string patch_label = "Teiländerung erhält zusätzliche Befehlsdaten: " + std::string(EV_TYPES[i]);
        check(patch_label.c_str(), patched.ok && patched.changed && patched.value.find("extra_meta") &&
              json_write(*patched.value.find("extra_meta")) == json_write(*c.find("extra_meta")));
    }
    JsonValue move = json_parse(samples[ET_MOVE_PLAYER]);
    move.set("extra", json_parse(R"({"data":[1,2]})"));
    EventCommandForm cmd_before = event_command_form_read(move), cmd_after = cmd_before;
    cmd_after.val1 = "4,25";
    edit = event_command_prepare(&move, cmd_before, cmd_after);
    check("Spielerbewegung behält relative=false, Z-Präzision und Zusatzdaten", edit.ok &&
          edit.value.find("relative") && !edit.value.find("relative")->b && edit.value.find("extra") &&
          json_write(*edit.value.find("z")) == json_write(*move.find("z")));
    JsonValue script = json_parse(samples[ET_SCRIPT]);
    cmd_before = event_command_form_read(script); cmd_after = cmd_before; cmd_after.name = "b";
    edit = event_command_prepare(&script, cmd_before, cmd_after);
    check("Script-Änderung erhält unsichtbaren Inline-Code", edit.ok && edit.value.find("code")->str == "side_effect");
    JsonValue var = json_parse(samples[ET_VARIABLE]);
    cmd_before = event_command_form_read(var); cmd_after = cmd_before; cmd_after.name = "b";
    edit = event_command_prepare(&var, cmd_before, cmd_after);
    check("Variable behält große Ganzzahl und präzises Add", edit.ok &&
          json_write(*edit.value.find("value")) == json_write(*var.find("value")) &&
          json_write(*edit.value.find("add")) == json_write(*var.find("add")));
    cmd_after.val1 = "42"; cmd_after.val2 = "falsch";
    edit = event_command_prepare(&var, cmd_before, cmd_after);
    check("ungültiger Befehl rollt alle Befehl-Felder zurück", !edit.ok && json_write(edit.value) == json_write(var));
    const JsonValue huge_color = json_parse("{\"cmd\":\"effect\",\"name\":\"burst\",\"color\":" + std::string(400, '9') + "}");
    check("extreme Farbe wird ohne unsicheren Integer-Cast angezeigt", event_command_form_read(huge_color).val2 == "[ungültig]");
    cmd_before = event_command_form_read(huge_color); cmd_after = cmd_before; cmd_after.val2 = "000000";
    edit = event_command_prepare(&huge_color, cmd_before, cmd_after);
    check("Schwarz ist gültige Farbe und Ersetzen extremer Farbe wird bestätigt", edit.ok && edit.replaces_invalid && edit.value.find("color")->number() == 0);
    cmd_after = cmd_before; cmd_after.val2 = "XX";
    check("ungültige Hex-Farbe wird nicht zu Standardfarbe", !event_command_prepare(&huge_color, cmd_before, cmd_after).ok);
    const JsonValue malformed_list = json_parse(R"({"cmd":"text","lines":["a",{"extra":7},"b"]})");
    cmd_before = event_command_form_read(malformed_list); cmd_after = cmd_before; cmd_after.text = "neu";
    check("Ersetzen nicht darstellbarer Textliste erfordert Warnung", event_command_prepare(&malformed_list, cmd_before, cmd_after).replaces_invalid);
    const JsonValue unknown = json_parse(R"({"cmd":"fremd","then":[{"x":1}],"extra":2})");
    cmd_before = event_command_form_read(unknown); cmd_after = cmd_before; cmd_after.type = ET_TEXT; cmd_after.text = "neu";
    edit = event_command_prepare(&unknown, cmd_before, cmd_after);
    check("Befehlstypwechsel verlangt explizite Bestätigung", edit.ok && edit.converts_command);
    const JsonValue dup_cmd = json_parse(R"({"cmd":"text","cmd":"log","lines":["alt"]})");
    cmd_before = event_command_form_read(dup_cmd); cmd_after = cmd_before; cmd_after.type = ET_TEXT;
    check("mehrfacher cmd-Schlüssel verhindert Umwandlung", !event_command_prepare(&dup_cmd, cmd_before, cmd_after).ok);
    JsonValue choices = json_parse(R"({"cmd":"choices","options":["a","b"],"branches":[[{"cmd":"log","text":"a"}],[{"cmd":"log","text":"b"}],[{"extra":7}]],"meta":42})");
    cmd_before = event_command_form_read(choices); cmd_after = cmd_before; cmd_after.text = "b\r\na";
    edit = event_command_prepare(&choices, cmd_before, cmd_after);
    check("Optionen umordnen warnt vor Indexzuordnung und erhält sämtliche Zweige",
          edit.ok && edit.reassigns_choices && json_write(*edit.value.find("branches")) == json_write(*choices.find("branches")) && edit.value.find("meta"));
    cmd_after.text = "nur eins"; edit = event_command_prepare(&choices, cmd_before, cmd_after);
    check("weniger Optionen verwerfen keinen zusätzlichen Zweig", edit.ok && edit.reassigns_choices && edit.value.find("branches")->arr.size() == 3);
    cmd_after.text = "a\r\n\r\nb";
    check("leere Optionen werden nicht still entfernt", !event_command_prepare(&choices, cmd_before, cmd_after).ok);
    // Z5: cancel (XP "When Cancel") - lesen, setzen, prüfen, erhalten.
    const JsonValue cancel_json = json_parse(
        R"({"cmd":"choices","options":["Ja","Nein"],"branches":[[],[]],"cancel":"verzweigung","cancel_branch":[{"cmd":"log","text":"abgebrochen"}]})");
    cmd_before = event_command_form_read(cancel_json);
    check("cancel=verzweigung wird im Formular als val1 angezeigt", cmd_before.val1 == "verzweigung");
    cmd_after = cmd_before; cmd_after.val1 = "1";
    edit = event_command_prepare(&cancel_json, cmd_before, cmd_after);
    check("cancel auf Optionsnummer umstellen schreibt Zahl und erhält cancel_branch",
          edit.ok && edit.value.find("cancel") && edit.value.find("cancel")->is_num() &&
          edit.value.find("cancel")->number() == 1.0 &&
          edit.value.find("cancel_branch") && edit.value.find("branches"));
    cmd_after = cmd_before; cmd_after.val1 = "";
    edit = event_command_prepare(&cancel_json, cmd_before, cmd_after);
    check("cancel leeren entfernt nur den cancel-Schlüssel",
          edit.ok && !edit.value.find("cancel") &&
          edit.value.find("cancel_branch") && edit.value.find("options"));
    cmd_after = cmd_before; cmd_after.val1 = "7";
    check("cancel-Index außerhalb der Optionen wird abgelehnt",
          !event_command_prepare(&cancel_json, cmd_before, cmd_after).ok);
    cmd_after = cmd_before; cmd_after.val1 = "vielleicht";
    check("unbekannter cancel-Wert wird abgelehnt",
          !event_command_prepare(&cancel_json, cmd_before, cmd_after).ok);
    cmd_after = cmd_before; cmd_after.text = "Nein\r\nJa";
    edit = event_command_prepare(&cancel_json, cmd_before, cmd_after);
    check("unverändertes cancel-Feld bleibt beim Optionenumordnen unberührt",
          edit.ok && edit.reassigns_choices &&
          edit.value.find("cancel") && edit.value.find("cancel")->is_str() &&
          edit.value.find("cancel")->str == "verzweigung" &&
          edit.value.find("cancel_branch"));
    const JsonValue cancel_aktiv = json_parse(
        R"({"cmd":"choices","options":["Ja"],"branches":[[]],"cancel":true})");
    check("cancel=true zeigt das Formular als verzweigung an",
          event_command_form_read(cancel_aktiv).val1 == "verzweigung");
    JsonValue item = json_parse(samples[ET_ITEM]);
    cmd_before = event_command_form_read(item);
    check("große Item-Entnahme wird exakt mit negativem Vorzeichen angezeigt", cmd_before.val2 == "-9007199254740993");
    cmd_after = cmd_before; cmd_after.val2 = "5";
    edit = event_command_prepare(&item, cmd_before, cmd_after);
    check("Wechsel von take zu count warnt vor Entfernen des alten Feldes", edit.ok && edit.replaces_invalid &&
          !edit.value.find("take") && edit.value.find("count")->integer_text() == "5");

    EventBatch batch;
    batch.page = 0; batch.event_fields = true; batch.event_before = event_form_read(legacy); batch.event_after = batch.event_before;
    batch.event_after.name = "Geändert";
    batch.page_fields = true; batch.page_before = event_page_form_read(legacy, 0); batch.page_after = batch.page_before;
    batch.page_after.variable_name = "score"; batch.page_after.variable_value = "kaputt";
    edit = event_batch_prepare(legacy, batch);
    check("mehrere Formulare sind zusammen atomar: kein Eventname bei ungültiger Seite", !edit.ok && json_write(edit.value) == saved);
    batch.page_after.variable_value = "3.5";
    batch.command = EventCommandAction::Update;
    batch.path = {{0, "then", 0}, {0, "branches", 0}}; batch.command_index = 0;
    const JsonValue* old_nested = &legacy.find("commands")->arr[0].find("then")->arr[0].find("branches")->arr[0].arr[0];
    batch.command_before = event_command_form_read(*old_nested); batch.command_after = batch.command_before;
    batch.command_after.val1 = "8"; batch.command_after.val2 = "oops";
    edit = event_batch_prepare(legacy, batch);
    check("ungültiger verschachtelter Befehl rollt Event, Seitenmigration und Befehl zurück", !edit.ok && json_write(edit.value) == saved);
    batch.command_after.val2 = batch.command_before.val2;
    edit = event_batch_prepare(legacy, batch);
    check("Event, Migration und verschachtelter Befehl werden gemeinsam vorbereitet", edit.ok && edit.changed &&
          edit.value.find("pages") && edit.value.find("name")->str == "Geändert" && !edit.converts_command);
    check("Umbenennungswarnung bleibt beim gemeinsamen Batch erhalten", edit.ok && edit.renames_event);
    std::string error;
    JsonValue* nested = edit.ok ? event_command_list_mutable(edit.value, 0, batch.path, false, error) : nullptr;
    check("Pfad bleibt nach Migration gültig; relative und Metadaten bleiben", nested && nested->arr[0].find("x")->number() == 8 &&
          !nested->arr[0].find("relative")->b && nested->arr[0].find("custom") &&
          nested->arr[0].find("z")->integer_text() == "9007199254740993");
    EventBatch ins;
    ins.page = 0; ins.command = EventCommandAction::Insert; ins.path = {{0, "else", 0}};
    ins.command_after.type = ET_LOG; ins.command_after.text = "neuer Zweig";
    edit = event_batch_prepare(legacy, ins);
    nested = edit.ok ? event_command_list_mutable(edit.value, 0, ins.path, false, error) : nullptr;
    check("Einfügen in leeren Wenn-Zweig erhält andere Zweige", nested && nested->arr.size() == 1 &&
          json_write(*edit.value.find("commands")->arr[0].find("then")) == json_write(*legacy.find("commands")->arr[0].find("then")));
    const JsonValue missing_branch = json_parse(R"({"commands":[{"cmd":"if_switch","name":"x","value":true}]})");
    ins.path = {{0, "then", 0}};
    edit = event_batch_prepare(missing_branch, ins);
    check("fehlender Zweig entsteht erst beim Einfügen, nicht beim Anzeigen", edit.ok &&
          !missing_branch.find("commands")->arr[0].find("then") && edit.value.find("commands")->arr[0].find("then"));
    const JsonValue invalid_branch = json_parse(R"({"commands":[{"cmd":"if_switch","then":{"meta":1}}]})");
    edit = event_batch_prepare(invalid_branch, ins);
    check("beschädigter Zweig wird nicht überschrieben", !edit.ok && json_write(edit.value) == json_write(invalid_branch));
    const JsonValue missing_choices = json_parse(R"({"commands":[{"cmd":"choices","options":["a","b"]}]})");
    ins.path = {{0, "branches", 1}};
    edit = event_batch_prepare(missing_choices, ins);
    nested = edit.ok ? event_command_list_mutable(edit.value, 0, ins.path, false, error) : nullptr;
    check("fehlende Wahlzweige werden bei Einfügen bis zum Ziel angelegt", nested && nested->arr.size() == 1 &&
          edit.value.find("commands")->arr[0].find("branches")->arr.size() == 2);
    const JsonValue invalid_commands = json_parse(R"({"commands":{"custom":42}})");
    ins.path.clear(); edit = event_batch_prepare(invalid_commands, ins);
    check("ungültige Befehlsliste wird beim Einfügen nicht ersetzt", !edit.ok && json_write(edit.value) == json_write(invalid_commands));
    ins.command_after.type = ET_WAIT; ins.command_after.val1 = "unsinn";
    check("ungültiger neuer Befehl wird nicht eingefügt", !event_batch_prepare(empty, ins).ok);
    ins.command_after.val1 = "2";
    edit = event_batch_prepare(empty, ins);
    check("fehlende Befehlsliste entsteht erst beim gültigen Einfügen", edit.ok && !empty.find("commands") &&
          event_commands_view(edit.value, 0)->arr.size() == 1 && !edit.value.find("pages"));
    edit = event_command_remove_or_move(legacy, 0, {{0, "then", 0}, {0, "branches", 0}}, 0, 0);
    check("verschachteltes Löschen lässt andere Metadaten und Zweige stehen", edit.ok && edit.changed &&
          edit.value.find("commands")->arr[0].find("meta") &&
          edit.value.find("commands")->arr[0].find("then")->arr[0].find("branches")->arr[1].is_arr());

    edit = event_duplicate_prepare(legacy, 77);
    check("Duplikat erhält Altformat und Zusatzdaten; Original bleibt unverändert", edit.ok &&
          !edit.value.find("pages") && edit.value.find("id")->integer_text() == "77" &&
          edit.value.find("z")->integer_text() == "9007199254740994" &&
          edit.value.find("name")->str == "Alt (Kopie)" && edit.value.find("unknown") && json_write(legacy) == saved);
    check("Duplizieren mehrfacher Positionsschlüssel wird verhindert", !event_duplicate_prepare(dup_x, 8).ok);
    check("Duplizieren eines fremden Namensobjekts wird verhindert", !event_duplicate_prepare(bad_event, 8).ok);
    check("Duplikat benötigt eine positive Event-ID", !event_duplicate_prepare(legacy, 0).ok);
    check("normal verschobene Koordinaten erfordern keine Kopierwarnung", !event_duplicate_prepare(legacy, 8).unshifted_coordinate);
    const JsonValue missing_position = json_parse(R"({"name":"Leer","z":null})");
    const EventEdit missing_copy = event_duplicate_prepare(missing_position, 8);
    check("fehlende und null-Koordinaten bleiben erhalten, Kopie warnt vor gleicher Position",
          missing_copy.ok && missing_copy.unshifted_coordinate && !missing_copy.value.find("x") &&
          missing_copy.value.find("z")->is_null());
    const JsonValue invalid_position = json_parse(R"({"name":"Defekt","x":"alt","z":2})");
    const EventEdit invalid_copy = event_duplicate_prepare(invalid_position, 8);
    check("ungültige Koordinate bleibt in der Kopie erhalten und löst Warnung aus",
          invalid_copy.ok && invalid_copy.unshifted_coordinate && invalid_copy.value.find("x")->str == "alt" &&
          invalid_copy.value.find("z")->number() == 3);
    const JsonValue imprecise_position = json_parse(R"({"name":"Weit","x":1e100,"z":0})");
    const EventEdit imprecise_copy = event_duplicate_prepare(imprecise_position, 8);
    check("nicht darstellbares Float-Inkrement bleibt unverändert und wird gemeldet",
          imprecise_copy.ok && imprecise_copy.unshifted_coordinate &&
          json_write(*imprecise_copy.value.find("x")) == json_write(*imprecise_position.find("x")));
    const std::string enormous(400, '9');
    const JsonValue huge_position = json_parse("{\"name\":\"Gross\",\"x\":" + enormous + ",\"z\":0}");
    const EventEdit huge_copy = event_duplicate_prepare(huge_position, 8);
    check("400-stellige Ganzzahl wird trotz unendlicher double-Näherung exakt inkrementiert",
          huge_copy.ok && !huge_copy.unshifted_coordinate &&
          huge_copy.value.find("x")->integer_text() == "1" + std::string(400, '0'));

    const JsonValue names = json_parse(R"json({"events":[{"name":"Alt"},{"name":"Alt (Kopie)"},{"name":"Alt (Kopie) 2"},{"name":null},{"name":"erstes","name":"letztes"},{"name":12}],"npcs":[{"name":"Event 1"},{"name":"NPC-Name"}]})json");
    check("Namensprüfung berücksichtigt NPCs, Fallback und doppelte Schlüssel",
          event_name_used(names, "NPC-Name") && event_name_used(names, "Event") &&
          event_name_used(names, "erstes") && event_name_used(names, "letztes") && event_name_used(names, "12"));
    check("der eigene Eventname darf unverändert bleiben", !event_name_used(names, "Alt", 0));
    check("neuer Eventname weicht NPC-Namen aus", event_available_name(names, "Event 1") == "Event 1 2");
    const EventEdit named_copy = event_duplicate_prepare(names.find("events")->arr[0], 8, &names);
    check("wiederholtes Duplizieren erzeugt einen eindeutigen Namen für Self-Switches",
          named_copy.ok && named_copy.value.find("name")->str == "Alt (Kopie) 3" &&
          !event_name_used(names, named_copy.value.find("name")->str));

    const JsonValue duplicate_ids = json_parse(R"({"events":[{"id":1,"id":2},{"id":3},{"id":9007199254740993}]})");
    check("neue ID beachtet alle mehrfachen ID-Schlüssel", event_next_id(duplicate_ids) == 4);
    const JsonValue mixed_ids = json_parse("{\"events\":[{\"id\":1.0},{\"id\":2.5},{\"id\":-1},{\"id\":" + enormous + "}]}");
    check("ID-Vergabe ignoriert nicht-ganzzahlige und extreme Werte sicher", event_next_id(mixed_ids) == 2);
    check("uneindeutige oder ungültige Eventliste erhält keine neue ID",
          event_next_id(json_parse(R"({"events":[],"events":[]})")) == 0 &&
          event_next_id(json_parse(R"({"events":{}})")) == 0);
    JsonValue many_events = JsonValue::make_obj();
    JsonValue id_list = JsonValue::make_arr();
    for (int i = 1; i <= 12000; ++i) {
        JsonValue item = JsonValue::make_obj();
        item.set("id", JsonValue::make_integer(std::to_string(i)));
        id_list.arr.push_back(std::move(item));
    }
    many_events.set("events", std::move(id_list));
    check("ID-Vergabe bleibt auch bei 12000 lückenlosen Events handhabbar", event_next_id(many_events) == 12001);

    const bool all_fields[] = {true, true, true, true, true};
    const bool protected_text[] = {true, true, true, true, false};
    const bool protected_name[] = {false, true, true, true, true};
    check("Sichtbarkeit und Schutz nutzen dieselbe F6-Feldzuordnung",
          event_command_fields_available(ET_MOVE_ROUTE, all_fields) &&
          !event_command_fields_available(ET_TEXT, protected_text) &&
          event_command_fields_available(ET_WAIT, protected_text) &&
          !event_command_fields_available(ET_SWITCH, protected_name) &&
          !event_command_uses_field(-1, 0) && !event_command_uses_field(ET_TEXT, 5));
    const JsonValue duplicate_relative = json_parse(R"({"cmd":"move_player","x":1,"z":2,"relative":true,"relative":false})");
    cmd_before = event_command_form_read(duplicate_relative); cmd_after = cmd_before; cmd_after.val1 = "3";
    check("mehrdeutiges verstecktes relative blockiert die Befehlsänderung", !event_command_prepare(&duplicate_relative, cmd_before, cmd_after).ok);
    const JsonValue duplicate_num = json_parse(R"({"cmd":"wait","seconds":1,"seconds":2})");
    check("mehrfaches Befehlsfeld wird nicht als eindeutiger erster Wert angezeigt",
          event_command_form_read(duplicate_num).val1 == "[mehrfach vorhanden]");
    const JsonValue long_text = json_parse("{\"cmd\":\"text\",\"lines\":[\"" + std::string(20000, 'a') + "\"]}");
    cmd_before = event_command_form_read(long_text); cmd_after = cmd_before;
    check("mehr als 8192 Textzeichen bleiben beim Lesen und unverändertem Übernehmen erhalten",
          cmd_before.text.size() == 20000 && json_write(event_command_prepare(&long_text, cmd_before, cmd_after).value) == json_write(long_text));
    EventCommandForm invalid_new; invalid_new.type = ET_INPUT_NUMBER; invalid_new.name = "n"; invalid_new.val1 = "7";
    check("Ziffernzahl außerhalb 1–6 wird nicht geklemmt", !event_command_prepare(nullptr, EventCommandForm{}, invalid_new).ok);
    invalid_new.val1 = "3"; invalid_new.val2 = "-1";
    check("negativer Startwert wird nicht still verworfen", !event_command_prepare(nullptr, EventCommandForm{}, invalid_new).ok);
    invalid_new.val1 = "2"; invalid_new.val2 = "100";
    check("Startwert außerhalb der Ziffernzahl wird nicht still im Spiel geklemmt",
          !event_command_prepare(nullptr, EventCommandForm{}, invalid_new).ok);
    invalid_new.val2 = "99";
    check("größter passender Startwert wird zugelassen", event_command_prepare(nullptr, EventCommandForm{}, invalid_new).ok);
    invalid_new.val1.clear(); invalid_new.val2 = "10";
    check("fehlende Ziffernzahl verwendet für die Prüfung den Ruby-Standard 1",
          !event_command_prepare(nullptr, EventCommandForm{}, invalid_new).ok);
    const JsonValue old_number = json_parse(R"({"cmd":"input_number","name":"zahl","digits":3,"initial":999,"extra":42})");
    cmd_before = event_command_form_read(old_number); cmd_after = cmd_before; cmd_after.val1 = "2";
    check("kleinere Ziffernzahl verlangt passenden Startwert statt stiller Klemmen",
          !event_command_prepare(&old_number, cmd_before, cmd_after).ok);
    cmd_after.val2 = "99";
    check("Ziffernzahl und passend korrigierter Startwert werden gemeinsam übernommen",
          event_command_prepare(&old_number, cmd_before, cmd_after).ok);
    const JsonValue old_clamped = json_parse(R"({"cmd":"input_number","name":"alt","digits":1,"initial":999})");
    cmd_before = event_command_form_read(old_clamped); cmd_after = cmd_before; cmd_after.name = "neu";
    edit = event_command_prepare(&old_clamped, cmd_before, cmd_after);
    check("unberührter historischer Startwert bleibt bei Namensänderung erhalten",
          edit.ok && edit.value.find("initial")->integer_text() == "999");
    invalid_new.type = ET_SAVE_GAME; invalid_new.val1 = "4";
    check("ungültiger Speicher-Slot wird nicht übernommen", !event_command_prepare(nullptr, EventCommandForm{}, invalid_new).ok);
    invalid_new.type = ET_MOVE_ROUTE; invalid_new.name = "spieler"; invalid_new.val1 = "0"; invalid_new.val2 = "1";
    check("ungültiges Routentempo wird nicht still entfernt", !event_command_prepare(nullptr, EventCommandForm{}, invalid_new).ok);
    invalid_new.val1 = "2"; invalid_new.val2 = "2";
    check("Warten akzeptiert nur 0, 1 oder leer", !event_command_prepare(nullptr, EventCommandForm{}, invalid_new).ok);
    invalid_new.type = ET_SELF_SWITCH; invalid_new.name = "Z";
    check("ungültiger Self-Switch wird nicht zu A umgewandelt", !event_command_prepare(nullptr, EventCommandForm{}, invalid_new).ok);
    invalid_new.type = ET_FADE; invalid_new.name = "falsch"; invalid_new.val1 = "1";
    check("ungültige Fade-Richtung wird nicht zu aus umgewandelt", !event_command_prepare(nullptr, EventCommandForm{}, invalid_new).ok);
    invalid_new.type = ET_LOG; invalid_new.text = std::string("a\0b", 3);
    check("NUL in neuem Logtext wird abgelehnt", !event_command_prepare(nullptr, EventCommandForm{}, invalid_new).ok);
    invalid_new.type = ET_VARIABLE; invalid_new.name = "x"; invalid_new.val1 = std::string(400, '9'); invalid_new.val2.clear();
    check("nicht endlicher Variablenwert wird abgelehnt", !event_command_prepare(nullptr, EventCommandForm{}, invalid_new).ok);

    for (const char* path : {"game/data/maps/dorf.json", "projekte/dorf/data/maps/dorf.json"}) {
        std::string file;
        bool ok = datei_lesen(wurzel + "/" + path, file);
        if (ok) {
            const JsonValue root = json_parse(file);
            const JsonValue* events = root.find("events");
            ok = events && events->is_arr();
            if (ok) for (const JsonValue& e : events->arr) {
                if (!e.is_obj()) continue;
                const std::string original = json_write(e);
                const EventForm f = event_form_read(e);
                const EventEdit no = event_form_prepare(e, f, f);
                if (!no.ok || no.changed || json_write(no.value) != original) ok = false;
                for (size_t i = 0; i < event_page_count(e); ++i) if (event_page_view(e, i)) {
                    const EventPageForm p = event_page_form_read(e, i);
                    const EventEdit same = event_page_prepare(e, i, p, p);
                    if (!same.ok || same.changed || json_write(same.value) != original) ok = false;
                }
            }
        }
        const std::string label = std::string("mitgelieferte Events/Seiten lesend unverändert: ") + path;
        check(label.c_str(), ok);
    }
}

// Die echten F6-Controlkoordinaten aus der UI lesen, damit eine spätere
// Erweiterung nicht unbemerkt über die virtuelle Formularfläche hinausragt.
struct F6TestControl { int x, y, width, height; };

bool f6_test_int(const std::string& token, int& result) {
    const char* start = token.c_str();
    char* end = nullptr;
    errno = 0;
    const long value = std::strtol(start, &end, 10);
    if (end == start || errno == ERANGE || value < INT_MIN || value > INT_MAX) return false;
    while (*end && std::isspace(static_cast<unsigned char>(*end))) ++end;
    if (*end) return false;
    result = static_cast<int>(value);
    return true;
}

bool f6_test_controls(const std::string& source, std::vector<F6TestControl>& controls) {
    size_t cursor = source.find("void open_event_editor() {");
    if (cursor == std::string::npos) return false;
    const size_t limit = source.find("\nvoid close_event_editor()", cursor);
    if (limit == std::string::npos) return false;
    while (true) {
        const size_t control = source.find("ev_control(", cursor);
        const size_t label = source.find("ev_label(", cursor);
        const size_t next = std::min(control, label);
        if (next == std::string::npos || next >= limit) break;
        const bool is_label = next == label;
        cursor = next + (is_label ? std::strlen("ev_label(") : std::strlen("ev_control("));
        size_t arg_start = cursor;
        int depth = 1;
        bool quoted = false, escaped = false;
        std::vector<std::string> args;
        for (; cursor < limit; ++cursor) {
            const char c = source[cursor];
            if (quoted) {
                if (escaped) escaped = false;
                else if (c == '\\') escaped = true;
                else if (c == '"') quoted = false;
                continue;
            }
            if (c == '"') quoted = true;
            else if (c == '(') ++depth;
            else if (c == ')') {
                if (--depth == 0) {
                    args.push_back(source.substr(arg_start, cursor - arg_start));
                    ++cursor;
                    break;
                }
            } else if (c == ',' && depth == 1) {
                args.push_back(source.substr(arg_start, cursor - arg_start));
                arg_start = cursor + 1;
            }
        }
        if (depth != 0 || args.size() != (is_label ? 4u : 8u)) return false;
        const size_t first = is_label ? 1 : 3;
        F6TestControl c{};
        if (!f6_test_int(args[first], c.x) || !f6_test_int(args[first + 1], c.y) ||
            !f6_test_int(args[first + 2], c.width)) return false;
        if (is_label) c.height = 20;
        else if (args[0].find("L\"ComboBox\"") != std::string::npos) {
            // Das hohe CreateWindow-Maß ist der Popup-Umfang, nicht die
            // geschlossene Auswahlzeile. Deren Win32-Größe prüft der Windows-Test.
            c.height = 24;
        } else if (!f6_test_int(args[first + 3], c.height)) return false;
        controls.push_back(c);
    }
    return controls.size() >= 50;
}

bool main_test_inside(rune::editor::MainRect child, rune::editor::MainRect parent) {
    return child.w >= 0 && child.h >= 0 && child.x >= parent.x && child.y >= parent.y &&
        static_cast<std::int64_t>(child.x) + child.w <= static_cast<std::int64_t>(parent.x) + parent.w &&
        static_cast<std::int64_t>(child.y) + child.h <= static_cast<std::int64_t>(parent.y) + parent.h;
}

void main_layout_tests() {
    using namespace rune::editor;
    std::printf("== Hauptfenster: Palette, Kartenbaum und 3D-Ansicht ==\n");
    const MainWindowLayout normal = main_window_layout(1440, 880, 28, 24, 180, 96);
    check("Palette über Trennleiste und Kartenbaum", normal.palette.y == 28 &&
          normal.palette.y + normal.palette.h == normal.splitter.y &&
          normal.splitter.y + normal.splitter.h <= normal.maps_label.y &&
          normal.maps_label.y + normal.maps_label.h <= normal.maps_tree.y &&
          normal.maps_tree.y + normal.maps_tree.h <= normal.content_bottom);
    check("3D-Ansicht zentral, Inspektor rechts und Status unten",
          normal.viewport.x == normal.palette.w &&
          normal.viewport.x + normal.viewport.w == normal.inspector.x &&
          normal.inspector.x + normal.inspector.w == 1440 &&
          normal.viewport.y + normal.viewport.h == 856 && normal.content_bottom == 856);
    check("Kartenaktionen liegen vollständig neben der Überschrift",
          main_test_inside(normal.maps_label, {0, 28, normal.palette.w, 828}) &&
          main_test_inside(normal.maps_new, {0, 28, normal.palette.w, 828}) &&
          main_test_inside(normal.maps_props, {0, 28, normal.palette.w, 828}) &&
          normal.maps_label.x + normal.maps_label.w <= normal.maps_new.x &&
          normal.maps_new.x + normal.maps_new.w <= normal.maps_props.x &&
          normal.maps_props.x + normal.maps_props.w <= normal.palette.w);

    const int larger = main_map_height_for_split(1024, 640, 28, 24, -500, 96);
    const int smaller = main_map_height_for_split(1024, 640, 28, 24, 5000, 96);
    const MainWindowLayout upper = main_window_layout(1024, 640, 28, 24, larger, 96);
    const MainWindowLayout lower = main_window_layout(1024, 640, 28, 24, smaller, 96);
    check("Splitter begrenzt beide Bereiche auch bei extremem Ziehen",
          larger > smaller && upper.palette.h >= 140 && lower.map_height >= 90 &&
          upper.maps_tree.h > 0 && lower.palette.h > 0);
    check("Splitter-Treffer nur innerhalb der Leiste",
          main_rect_contains(normal.splitter, normal.splitter.x + 1, normal.splitter.y + 1) &&
          !main_rect_contains(normal.splitter, normal.splitter.x + normal.splitter.w, normal.splitter.y) &&
          !main_rect_contains(normal.splitter, normal.splitter.x, normal.splitter.y - 1));
    const MainWindowLayout squeezed = main_window_layout(800, 260, 28, 24, 320, 96);
    const MainWindowLayout restored = main_window_layout(800, 800, 28, 24, 320, 96);
    check("Verkleinern klemmt nur den Istwert, nicht den späteren Wunsch",
          squeezed.map_height < 320 && restored.map_height == 320);
    check("Kartenaktion nach Abbruch oder anderer Auswahl verworfen",
          main_map_action_matches(2, 2, 2) && !main_map_action_matches(2, 2, 1) &&
          !main_map_action_matches(2, 1, 1) && main_map_action_matches(-1, -1, 1) &&
          !main_map_action_matches(-1, 0, 0));
    check("Kartenaktion wartet auf Rückfrage und vollständigen Auswahlabgleich",
          main_map_action_can_post(true, false, false, false, false) &&
          !main_map_action_can_post(false, false, false, false, false) &&
          !main_map_action_can_post(true, true, false, false, false) &&
          !main_map_action_can_post(true, false, true, false, false) &&
          !main_map_action_can_post(true, false, false, true, false) &&
          !main_map_action_can_post(true, false, false, false, true));
    check("Kartenaktion bleibt an Projekt und Dateinamen gebunden",
          main_map_action_target_matches(1, 1, 1, 8, 8, "wald.json", "wald.json") &&
          !main_map_action_target_matches(1, 1, 1, 8, 9, "wald.json", "wald.json") &&
          !main_map_action_target_matches(1, 1, 1, 8, 8, "wald.json", "dorf.json") &&
          !main_map_action_target_matches(1, 1, 1, 8, 8, "", "") &&
          main_map_action_target_matches(-1, -1, 1, 8, 8, "", "dorf.json"));
    const MainWindowLayout narrow = main_window_layout(1024, 768, 56, 48, 180, 192);
    check("schmaler Monitor erhält Kartenüberschrift und kompakten F2-Knopf",
          narrow.maps_label.w >= main_layout_scale(52, 192) &&
          narrow.maps_new.w == main_layout_scale(42, 192) &&
          narrow.maps_props.w > 0 && main_map_props_compact(narrow.maps_props.w, 192) &&
          !main_map_props_compact(normal.maps_props.w, 96));

    const MainWindowLayout twice = main_window_layout(2880, 1760, 56, 48, 180, 192);
    check("200 Prozent skalieren Spalten, Splitter und Kartenbereich",
          twice.palette.w == 2 * normal.palette.w && twice.splitter.h == 2 * normal.splitter.h &&
          twice.map_height == 2 * normal.map_height && twice.maps_tree.h == 2 * normal.maps_tree.h);
    check("DPI-Fallback und extreme Eingaben begrenzt",
          main_layout_dpi(0) == 96 && main_layout_dpi(UINT_MAX) == 768 &&
          main_window_layout(-1, -1, INT_MAX, INT_MAX, INT_MAX, UINT_MAX).viewport.w == 0);

    bool windows_fit = true, palette_fit = true, inspector_fit = true;
    const unsigned dpis[] = {96, 120, 144, 192};
    const int widths[] = {320, 700, 1024, 1440};
    const int heights[] = {180, 360, 640, 880};
    for (unsigned dpi : dpis) for (int w : widths) for (int h : heights) {
        const int px_w = main_layout_scale(w, dpi), px_h = main_layout_scale(h, dpi);
        const int tb = main_layout_scale(28, dpi), sb = main_layout_scale(24, dpi);
        const MainWindowLayout x = main_window_layout(px_w, px_h, tb, sb, 180, dpi);
        const MainRect client{0, 0, px_w, px_h};
        windows_fit = windows_fit && main_test_inside(x.palette, client) &&
            main_test_inside(x.splitter, client) && main_test_inside(x.maps_label, client) &&
            main_test_inside(x.maps_new, client) && main_test_inside(x.maps_props, client) &&
            main_test_inside(x.maps_tree, client) && main_test_inside(x.viewport, client) &&
            main_test_inside(x.inspector, client) &&
            x.palette.x + x.palette.w <= x.viewport.x &&
            x.viewport.x + x.viewport.w <= x.inspector.x &&
            x.palette.y + x.palette.h <= x.splitter.y &&
            x.splitter.y + x.splitter.h <= x.maps_tree.y &&
            x.maps_tree.y + x.maps_tree.h <= x.content_bottom;
    }
    for (unsigned dpi : dpis) for (int h : {0, 400, 800}) {
        const int width = main_layout_scale(214, dpi);
        const int content = std::max(main_layout_scale(h, dpi), main_palette_min_height(dpi));
        const MainPaletteLayout p = main_palette_layout(width, main_layout_scale(h, dpi), dpi);
        const MainRect bounds{0, 0, width, content};
        palette_fit = palette_fit && main_test_inside(p.tools, bounds) &&
            main_test_inside(p.prefabs, bounds) && main_test_inside(p.prefab_list, p.prefabs) &&
            main_test_inside(p.actions, bounds) && main_test_inside(p.rotate, p.actions) &&
            main_test_inside(p.remove, p.actions) && main_test_inside(p.duplicate, p.actions) &&
            main_test_inside(p.undo, bounds) && main_test_inside(p.redo, bounds) &&
            p.tools.y + p.tools.h <= p.prefabs.y && p.prefabs.y + p.prefabs.h <= p.actions.y &&
            p.actions.y + p.actions.h <= p.undo.y;
        for (const MainRect& radio : p.radios) palette_fit = palette_fit && main_test_inside(radio, p.tools);
        const int iw = main_layout_scale(258, dpi);
        const int ih = std::max(main_layout_scale(h, dpi), main_inspector_min_height(dpi));
        const MainInspectorLayout q = main_inspector_layout(iw, main_layout_scale(h, dpi), dpi);
        const MainRect ib{0, 0, iw, ih};
        inspector_fit = inspector_fit && main_test_inside(q.objects_label, ib) &&
            main_test_inside(q.objects, ib) && main_test_inside(q.props, ib) &&
            q.objects.y + q.objects.h <= q.props.y && main_test_inside(q.apply, q.props) &&
            main_test_inside(q.hint, q.props);
        for (int i = 0; i < 4; ++i)
            inspector_fit = inspector_fit && main_test_inside(q.labels[i], q.props) &&
                main_test_inside(q.fields[i], q.props);
    }
    check("kleine und große Fenster bei 100 bis 200 Prozent ohne Spaltenüberschneidung", windows_fit);
    check("Palette bleibt im scrollbaren Inhalt ohne Überlappung", palette_fit);
    check("Inspektor bleibt im scrollbaren Inhalt ohne Überlappung", inspector_fit);
}

void event_layout_tests(const std::string& root) {
    using namespace rune::editor;
    std::printf("== F6: Arbeitsfläche, DPI und Scrollen ==\n");
    check("96 DPI unverändert", event_layout_scale(1000, 96) == 1000);
    check("125/150/200 Prozent skalieren", event_layout_scale(1000, 120) == 1250 &&
          event_layout_scale(702, 144) == 1053 && event_layout_scale(24, 192) == 48);
    check("Skalierung rundet auch negative Werte", event_layout_scale(-3, 144) == -5 &&
          event_layout_scale(3, 144) == 5);
    check("ungültige DPI fällt auf 96 zurück", event_layout_scale(16, 0) == 16 &&
          event_layout_rescale(240, 0, 144) == 360);
    check("Skalierung begrenzt große Zahlen", event_layout_scale(INT_MAX, UINT_MAX) == INT_MAX &&
          event_layout_scale(INT_MIN, UINT_MAX) == INT_MIN);
    check("DPI-Wechsel erhält logische Scrollposition", event_layout_rescale(200, 96, 144) == 300 &&
          event_layout_rescale(300, 144, 96) == 200);

    const EventLayoutRect full{0, 40, 1024, 600}; // Arbeitsfläche unter einer Taskleiste
    const EventLayoutRect owner{80, 60, 980, 590};
    const EventLayoutRect small = event_layout_center(full, owner, 1500, 1050);
    check("zu großes Fenster passt vollständig in die Arbeitsfläche", small.left == 0 && small.top == 40 &&
          small.right == 1024 && small.bottom == 600);
    const EventLayoutRect normal = event_layout_center({0, 0, 1920, 1040}, {200, 100, 1200, 800}, 1000, 700);
    check("passendes Fenster am Besitzer zentriert", normal.left == 200 && normal.top == 100 &&
          normal.right == 1200 && normal.bottom == 800);
    const EventLayoutRect negative = event_layout_center({-1920, -200, 0, 840},
                                                          {-1800, -100, -300, 700}, 900, 700);
    check("negative Monitorkoordinaten bleiben auf diesem Monitor", negative.left == -1500 &&
          negative.top == -50 && negative.right == -600 && negative.bottom == 650);
    const EventLayoutRect moved = event_layout_fit({-1280, 20, 0, 1000}, {-1500, 800, -600, 1400});
    check("DPI-Vorschlag wird links und unten begrenzt", moved.left == -1280 && moved.top == 400 &&
          moved.right == -380 && moved.bottom == 1000);
    const EventLayoutRect shrunk = event_layout_fit({0, 0, 800, 560}, {-20, -30, 1100, 900});
    check("DPI-Vorschlag wird bei Platzmangel verkleinert", shrunk.left == 0 && shrunk.top == 0 &&
          shrunk.right == 800 && shrunk.bottom == 560);
    const EventLayoutRect invalid = event_layout_fit({10, 0, 10, 100}, {3, 4, 33, 44});
    check("ungültige Arbeitsfläche verändert den Vorschlag nicht", invalid.left == 3 && invalid.bottom == 44);
    const EventLayoutRect huge = event_layout_fit({INT_MIN, INT_MIN, INT_MAX, INT_MAX},
                                                   {INT_MIN, INT_MIN, INT_MAX, INT_MAX});
    check("Rechteckrechnung überläuft nicht an int-Grenzen", huge.left == INT_MIN && huge.right == INT_MAX &&
          huge.top == INT_MIN && huge.bottom == INT_MAX);

    check("Scrollbereich entspricht Inhalt minus Ausschnitt", event_scroll_max(1000, 600) == 400 &&
          event_scroll_max(702, 900) == 0 && event_scroll_max(1000, 0) == 999);
    check("Scrollpositionen beidseitig begrenzt", event_scroll_clamp(1000, 600, -1) == 0 &&
          event_scroll_clamp(1000, 600, 500) == 400);
    check("extreme Scrollschritte ohne Überlauf", event_scroll_move(1000, 600, 200, INT64_MAX) == 400 &&
          event_scroll_move(1000, 600, 200, INT64_MIN) == 0);
    check("größerer Ausschnitt setzt Überhang zurück", event_scroll_clamp(1000, 1200, 400) == 0);
    check("Fokus rechts wird vollständig sichtbar", event_scroll_reveal(1000, 300, 0, 764, 936, 8) == 644);
    check("Fokus links wird vollständig sichtbar", event_scroll_reveal(1000, 300, 500, 12, 100, 8) == 4);
    check("sichtbarer Fokus lässt Ausschnitt stehen", event_scroll_reveal(1000, 300, 400, 450, 500, 8) == 400);
    check("große sichtbare Liste springt nicht", event_scroll_reveal(1000, 200, 200, 100, 500, 8) == 200);
    check("große unsichtbare Liste wird erreichbar", event_scroll_reveal(1000, 200, 600, 100, 500, 8) == 92);
    check("leeres Fokusrechteck ändert nichts", event_scroll_reveal(1000, 300, 50, 20, 20, 8) == 50);
    check("auch winziger Ausschnitt bleibt innerhalb der Grenzen",
          event_scroll_reveal(1000, 1, 0, 500, 524, 100) == 500);

    std::string source;
    std::vector<F6TestControl> controls;
    const bool parsed = datei_lesen(root + "/src/editor/editor_main.cpp", source) &&
                        f6_test_controls(source, controls);
    check("aktuelle F6-Controls aus dem Quelltext erfasst", parsed);
    if (!parsed) return;
    bool contained = true;
    for (const F6TestControl& c : controls)
        contained = contained && c.x >= 0 && c.y >= 0 && c.width > 0 && c.height > 0 &&
                    c.x + c.width <= EVENT_LAYOUT_WIDTH && c.y + c.height <= EVENT_LAYOUT_HEIGHT;
    check("alle F6-Controls liegen innerhalb der virtuellen Fläche", contained);

    struct Scenario { unsigned dpi; int width, height; const char* name; };
    const Scenario scenarios[] = {
        {96, 984, 560, "1024x640, 100 Prozent"},
        {120, 780, 450, "kleine Arbeitsfläche, 125 Prozent"},
        {144, 600, 360, "kleine Arbeitsfläche, 150 Prozent"},
        {192, 420, 240, "kleiner Ausschnitt, 200 Prozent"},
        {288, 300, 150, "sehr kleiner Ausschnitt, 300 Prozent"},
    };
    for (const Scenario& s : scenarios) {
        const int canvas_w = event_layout_scale(EVENT_LAYOUT_WIDTH, s.dpi);
        const int canvas_h = event_layout_scale(EVENT_LAYOUT_HEIGHT, s.dpi);
        const int max_x = event_scroll_max(canvas_w, s.width);
        const int max_y = event_scroll_max(canvas_h, s.height);
        const int margin = event_layout_scale(8, s.dpi);
        bool edges = true, focus = true;
        for (const F6TestControl& c : controls) {
            const int left = event_layout_scale(c.x, s.dpi);
            const int top = event_layout_scale(c.y, s.dpi);
            const int right = left + event_layout_scale(c.width, s.dpi);
            const int bottom = top + event_layout_scale(c.height, s.dpi);
            // Auch bei Controls größer als der Ausschnitt sind beide Enden
            // separat per Scrollleiste erreichbar.
            const int x_start = event_scroll_clamp(canvas_w, s.width, left);
            const int x_end = event_scroll_clamp(canvas_w, s.width, right - s.width);
            const int y_start = event_scroll_clamp(canvas_h, s.height, top);
            const int y_end = event_scroll_clamp(canvas_h, s.height, bottom - s.height);
            edges = edges && left >= x_start && left < x_start + s.width &&
                    right - 1 >= x_end && right - 1 < x_end + s.width &&
                    top >= y_start && top < y_start + s.height &&
                    bottom - 1 >= y_end && bottom - 1 < y_end + s.height;
            for (int mode = 0; mode < 3; ++mode) {
                const int old_x = mode == 0 ? 0 : mode == 1 ? max_x : max_x / 2;
                const int old_y = mode == 0 ? 0 : mode == 1 ? max_y : max_y / 2;
                const int x = event_scroll_reveal(canvas_w, s.width, old_x, left, right, margin);
                const int y = event_scroll_reveal(canvas_h, s.height, old_y, top, bottom, margin);
                focus = focus && x >= 0 && x <= max_x && y >= 0 && y <= max_y &&
                        right > x && left < x + s.width && bottom > y && top < y + s.height;
                if (right - left <= s.width - 2 * margin) focus = focus && left >= x && right <= x + s.width;
                if (bottom - top <= s.height - 2 * margin) focus = focus && top >= y && bottom <= y + s.height;
            }
        }
        std::string name = std::string(s.name) + ": alle Controlränder scrollbar";
        check(name.c_str(), edges);
        name = std::string(s.name) + ": Fokus wird sichtbar";
        check(name.c_str(), focus);
    }
}

#ifdef _WIN32
LRESULT CALLBACK f6_test_window_proc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    auto* view = reinterpret_cast<rune::editor::EventViewState*>(GetWindowLongPtrW(h, GWLP_USERDATA));
    if (msg == WM_NCCREATE) {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lp);
        view = static_cast<rune::editor::EventViewState*>(create->lpCreateParams);
        SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(view));
    }
    if (view && rune::editor::event_view_message(*view, h, msg, wp, lp)) return 0;
    return DefWindowProcW(h, msg, wp, lp);
}

bool f6_test_visible_rect(HWND parent, HWND child) {
    RECT client{}, bounds{};
    if (!GetClientRect(parent, &client) || !GetWindowRect(child, &bounds)) return false;
    MapWindowPoints(nullptr, parent, reinterpret_cast<POINT*>(&bounds), 2);
    return bounds.left >= 0 && bounds.top >= 0 &&
           bounds.right <= client.right && bounds.bottom <= client.bottom;
}

// Verdeckte Standardcontrols: prüft die echten Win32-Nachrichten und dass
// Verschieben/Skalieren keine Formularinhalte oder Auswahlen verändert.
// Kein Fenster wird gezeigt; Tastatur, Maus und reale Monitor-DPI bleiben ein
// separater Live-Test.
void event_view_windows_tests() {
    using namespace rune::editor;
    std::printf("== Windows: F6-Scroll-Controls (ohne sichtbare GUI) ==\n");
    const HINSTANCE instance = GetModuleHandleW(nullptr);
    const wchar_t* cls = L"RuneF6LayoutTest";
    WNDCLASSW wc{};
    wc.hInstance = instance;
    wc.lpfnWndProc = f6_test_window_proc;
    wc.lpszClassName = cls;
    const ATOM registered = RegisterClassW(&wc);
    check("Testfensterklasse registriert", registered != 0);
    if (!registered) return;

    EventViewState view;
    const DWORD style = WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME |
                        WS_HSCROLL | WS_VSCROLL;
    HWND window = CreateWindowExW(WS_EX_DLGMODALFRAME, cls, L"F6-Test", style,
                                   0, 0, 400, 270, nullptr, nullptr, instance, &view);
    check("unsichtbares F6-Testfenster erzeugt", window != nullptr);
    if (!window) { UnregisterClassW(cls, instance); return; }
    EventLayoutRect work{};
    const bool has_work = event_view_work_area(window, nullptr, work);
    const EventLayoutRect initial = event_view_initial_rect(window, style, WS_EX_DLGMODALFRAME, 96);
    check("Startrechteck berücksichtigt die aktuelle Arbeitsfläche", has_work &&
          initial.left >= work.left && initial.top >= work.top &&
          initial.right <= work.right && initial.bottom <= work.bottom &&
          initial.right > initial.left && initial.bottom > initial.top);
    event_view_init(view, 96, static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT)));
    HWND list = event_view_add(view, window, L"ListBox", nullptr,
        WS_VSCROLL | WS_BORDER | LBS_NOTIFY | LBS_HASSTRINGS | WS_TABSTOP, 12, 30, 280, 130, 1);
    HWND edit = event_view_add(view, window, L"Edit", L"Entwurf Ä", WS_BORDER | ES_LEFT | WS_TABSTOP,
                               764, 520, 172, 24, 2);
    HWND combo = event_view_add(view, window, L"ComboBox", nullptr,
        CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 776, 550, 90, 180, 3);
    HWND button = event_view_add(view, window, L"Button", L"Übernehmen", BS_PUSHBUTTON | WS_TABSTOP,
                                 686, 660, 250, 28, 4);
    check("Listen-, Edit-, Combo- und Button-Control erzeugt", list && edit && combo && button);
    if (!list || !edit || !combo || !button) {
        DestroyWindow(window);
        event_view_dispose(view);
        UnregisterClassW(cls, instance);
        return;
    }
    SendMessageW(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Erstes Event"));
    SendMessageW(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Zweites Event"));
    SendMessageW(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"[weitere ausgeblendet]"));
    SendMessageW(list, LB_SETITEMDATA, 0, 9);
    SendMessageW(list, LB_SETITEMDATA, 1, 4);
    SendMessageW(list, LB_SETITEMDATA, 2, static_cast<LPARAM>(-1));
    check("Listbox stellt Auswahl anhand des Originalindex wieder her",
          listbox_select_data(list, 4) && SendMessageW(list, LB_GETCURSEL, 0, 0) == 1 &&
          listbox_select_data(list, 9) && SendMessageW(list, LB_GETCURSEL, 0, 0) == 0);
    check("unbekannter Originalindex löscht die Auswahl",
          !listbox_select_data(list, 5) && SendMessageW(list, LB_GETCURSEL, 0, 0) == LB_ERR);
    SendMessageW(list, LB_SETCURSEL, 2, 0);
    check("negativer Sentinel wählt keine Auslassungszeile",
          listbox_select_data(list, -1) && SendMessageW(list, LB_GETCURSEL, 0, 0) == LB_ERR);
    listbox_select_data(list, 4);
    SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"an"));
    SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"aus"));
    SendMessageW(combo, CB_SETCURSEL, 1, 0);
    SendMessageW(edit, EM_SETSEL, 1, 4);
    view.ready = true;
    event_view_update(view, window);
    int width = 1, height = 1;
    event_view_size(window, width, height);
    check("kleines Testfenster benötigt beide Scrollachsen", width < EVENT_LAYOUT_WIDTH &&
          height < EVENT_LAYOUT_HEIGHT && view.x == 0 && view.y == 0);

    SendMessageW(window, WM_HSCROLL, SB_BOTTOM, 0);
    SendMessageW(window, WM_VSCROLL, SB_BOTTOM, 0);
    SCROLLINFO si{};
    si.cbSize = sizeof(si); si.fMask = SIF_POS;
    const bool scroll_info = GetScrollInfo(window, SB_HORZ, &si) && si.nPos == view.x;
    check("Scrollleisten erreichen das rechte und untere Ende", scroll_info &&
          view.x == event_scroll_max(EVENT_LAYOUT_WIDTH, width) &&
          view.y == event_scroll_max(EVENT_LAYOUT_HEIGHT, height));
    check("zuvor verborgene Controls am Ende sichtbar", f6_test_visible_rect(window, edit) &&
          f6_test_visible_rect(window, combo) && f6_test_visible_rect(window, button));
    wchar_t value[64]{};
    DWORD selection_start = 0, selection_end = 0;
    GetWindowTextW(edit, value, static_cast<int>(sizeof(value) / sizeof(value[0])));
    SendMessageW(edit, EM_GETSEL, reinterpret_cast<WPARAM>(&selection_start), reinterpret_cast<LPARAM>(&selection_end));
    check("Scrollen erhält Text und Textauswahl", std::wcscmp(value, L"Entwurf Ä") == 0 &&
          selection_start == 1 && selection_end == 4);
    check("Scrollen erhält Listen- und Comboauswahl", SendMessageW(list, LB_GETCURSEL, 0, 0) == 1 &&
          SendMessageW(combo, CB_GETCURSEL, 0, 0) == 1);
    const int old_x = view.x, old_y = view.y;
    check("Scrollmeldung eines Kindes wird nicht als Fensterleiste behandelt",
          !event_view_message(view, window, WM_VSCROLL, SB_LINEUP, reinterpret_cast<LPARAM>(list)) &&
          view.x == old_x && view.y == old_y);

    event_view_scroll_to(view, window, 0, 0);
    event_view_reveal(view, window, edit);
    check("Fokus-Nachführung erreicht ein verborgenes Editfeld", f6_test_visible_rect(window, edit));
    event_view_scroll_to(view, window, 0, 0);
    SendMessageW(window, WM_MOUSEWHEEL, MAKEWPARAM(0, static_cast<WORD>(-60)), 0);
    check("Mausrad sammelt halbe Schritte", view.wheel_y == -60 && view.y == 0);
    UINT lines = 3;
    if (!SystemParametersInfoW(SPI_GETWHEELSCROLLLINES, 0, &lines, 0)) lines = 3;
    SendMessageW(window, WM_MOUSEWHEEL, MAKEWPARAM(0, static_cast<WORD>(-60)), 0);
    check("voller Mausradschritt bewegt die vertikale Ansicht", view.wheel_y == 0 && (!lines || view.y > 0));
    SendMessageW(window, WM_MOUSEWHEEL, MAKEWPARAM(MK_SHIFT, static_cast<WORD>(-120)), 0);
    check("Umschalt-Mausrad bewegt die horizontale Ansicht", !lines || view.x > 0);

    RECT suggested{};
    GetWindowRect(window, &suggested);
    SendMessageW(window, WM_DPICHANGED, MAKEWPARAM(144, 144), reinterpret_cast<LPARAM>(&suggested));
    event_view_scroll_to(view, window, 0, 0);
    RECT edit_bounds{};
    GetWindowRect(edit, &edit_bounds);
    MapWindowPoints(nullptr, window, reinterpret_cast<POINT*>(&edit_bounds), 2);
    check("DPI-Nachricht skaliert die ursprünglichen Controlkoordinaten", view.dpi == 144 &&
          edit_bounds.left == event_layout_scale(764, 144) && edit_bounds.top == event_layout_scale(520, 144));
    event_view_reveal(view, window, combo);
    RECT collapsed{}, dropped{};
    GetWindowRect(combo, &collapsed);
    SendMessageW(combo, CB_GETDROPPEDCONTROLRECT, 0, reinterpret_cast<LPARAM>(&dropped));
    check("Combo bleibt als Auswahlzeile erreichbar, Popup-Maß bleibt erhalten",
          f6_test_visible_rect(window, combo) && collapsed.bottom - collapsed.top < event_layout_scale(180, 144) &&
          dropped.bottom > collapsed.bottom && SendMessageW(combo, CB_GETCOUNT, 0, 0) == 2);
    GetWindowTextW(edit, value, static_cast<int>(sizeof(value) / sizeof(value[0])));
    check("DPI-Layout erhält Entwurf und Auswahlen", std::wcscmp(value, L"Entwurf Ä") == 0 &&
          SendMessageW(list, LB_GETCURSEL, 0, 0) == 1 && SendMessageW(combo, CB_GETCURSEL, 0, 0) == 1);
    SetWindowPos(window, nullptr, 0, 0, 600, 400, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    SendMessageW(window, WM_SIZE, SIZE_RESTORED, 0);
    event_view_size(window, width, height);
    check("Resize begrenzt beide Scrollpositionen neu", view.x <= event_scroll_max(event_view_content_width(view), width) &&
          view.y <= event_scroll_max(event_view_content_height(view), height));
    // Ein Verlassen des Monitors durch Fensterziehen muss auch ohne DPI-Wechsel
    // auf der dann aktuellen Arbeitsfläche landen.
    SetWindowPos(window, nullptr, work.right + 100, work.top, 0, 0,
                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    SendMessageW(window, WM_EXITSIZEMOVE, 0, 0);
    RECT after_move{};
    EventLayoutRect current_work{};
    check("Fenster nach Ziehen wieder auf seiner Monitor-Arbeitsfläche", GetWindowRect(window, &after_move) &&
          event_view_work_area(window, nullptr, current_work) &&
          after_move.left >= current_work.left && after_move.top >= current_work.top &&
          after_move.right <= current_work.right && after_move.bottom <= current_work.bottom);
    check("F6-Testfenster zerstört", DestroyWindow(window) != 0);
    event_view_dispose(view);
    UnregisterClassW(cls, instance);
}

static int main_test_last_command = 0, main_test_last_code = 0;
static HWND main_test_last_sender = nullptr, main_test_context_sender = nullptr;
static LPARAM main_test_context_position = 0;
static bool main_test_veto_selection = false;

LRESULT CALLBACK main_test_owner_proc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_COMMAND) {
        main_test_last_command = LOWORD(wp);
        main_test_last_code = HIWORD(wp);
        main_test_last_sender = reinterpret_cast<HWND>(lp);
        return 42;
    }
    if (msg == WM_CONTEXTMENU) {
        main_test_context_sender = reinterpret_cast<HWND>(wp);
        main_test_context_position = lp;
        return 33;
    }
    if (msg == WM_NOTIFY && lp) {
        const NMHDR* nm = reinterpret_cast<const NMHDR*>(lp);
        if (nm->idFrom == 9201 && (nm->code == TVN_SELCHANGINGW || nm->code == TVN_SELCHANGINGA))
            return main_test_veto_selection ? TRUE : FALSE;
        if (nm->idFrom == 9999) return 77;
        return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

void main_test_panel_place(rune::editor::MainPanel& panel, int width, int height) {
    const auto layout = rune::editor::main_palette_layout(width, height, panel.dpi);
    rune::editor::main_panel_place(panel, GetDlgItem(panel.window, 9101), layout.prefab_list);
    rune::editor::main_panel_place(panel, GetDlgItem(panel.window, 9102), layout.undo);
}

HTREEITEM main_test_tree_add(HWND tree, HTREEITEM parent, const wchar_t* text, LPARAM index) {
    TVINSERTSTRUCTW insert{};
    insert.hParent = parent;
    insert.hInsertAfter = TVI_LAST;
    insert.item.mask = TVIF_TEXT | TVIF_PARAM;
    insert.item.pszText = const_cast<wchar_t*>(text);
    insert.item.lParam = index;
    return reinterpret_cast<HTREEITEM>(SendMessageW(tree, TVM_INSERTITEMW, 0,
                                                    reinterpret_cast<LPARAM>(&insert)));
}

bool main_test_tree_point(HWND tree, HTREEITEM item, POINT& point) {
    RECT rect{};
    std::memcpy(&rect, &item, sizeof(item));
    if (!SendMessageW(tree, TVM_GETITEMRECT, TRUE, reinterpret_cast<LPARAM>(&rect))) return false;
    point.x = (rect.left + rect.right) / 2;
    point.y = (rect.top + rect.bottom) / 2;
    return ClientToScreen(tree, &point) != 0;
}

// Echte, unsichtbare Win32-Panels und TreeView-Nachrichten. Sie prüfen keine
// Darstellung, Hardware-GPU oder tatsächliche Mehrmonitor-DPI-Interaktion.
void main_window_windows_tests() {
    using namespace rune::editor;
    std::printf("== Windows: Hauptfenster-Panels und Kartenbaum (unsichtbar) ==\n");
    INITCOMMONCONTROLSEX common{};
    common.dwSize = sizeof(common);
    common.dwICC = ICC_TREEVIEW_CLASSES;
    const bool common_ready = InitCommonControlsEx(&common) != 0;
    check("TreeView-Klasse initialisiert", common_ready);
    if (!common_ready) return;
    const HINSTANCE instance = GetModuleHandleW(nullptr);
    const wchar_t* cls = L"RuneMainLayoutTest";
    WNDCLASSW wc{};
    wc.hInstance = instance;
    wc.lpfnWndProc = main_test_owner_proc;
    wc.lpszClassName = cls;
    const ATOM registered = RegisterClassW(&wc);
    check("Testfensterklasse registriert", registered != 0);
    if (!registered) return;
    HWND owner = CreateWindowExW(0, cls, L"Hauptfenster-Test", WS_OVERLAPPED,
                                 0, 0, 700, 600, nullptr, nullptr, instance, nullptr);
    check("unsichtbares Testfenster angelegt", owner != nullptr);
    if (!owner) { UnregisterClassW(cls, instance); return; }

    MainPanel panel;
    const bool created = main_panel_create(panel, owner, 9100, 96,
                                           main_palette_min_height, main_test_panel_place);
    check("scrollbare Palette angelegt", created);
    if (!created) { DestroyWindow(owner); UnregisterClassW(cls, instance); return; }
    HWND list = CreateWindowExW(0, L"ListBox", nullptr,
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY | LBS_HASSTRINGS,
        0, 0, 10, 10, panel.window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(9101)), instance, nullptr);
    HWND edit = CreateWindowExW(0, L"Edit", L"Entwurf Ä", WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP,
        0, 0, 10, 10, panel.window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(9102)), instance, nullptr);
    check("native Panel-Controls angelegt", list && edit);
    if (!list || !edit) { DestroyWindow(owner); UnregisterClassW(cls, instance); return; }
    SendMessageW(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Baum"));
    SendMessageW(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Haus"));
    SendMessageW(list, LB_SETCURSEL, 1, 0);
    SendMessageW(edit, EM_SETSEL, 1, 4);
    panel.ready = true;
    SetWindowPos(panel.window, nullptr, 0, 0, 230, 160, SWP_NOZORDER | SWP_NOACTIVATE);
    main_panel_update(panel);
    int width = 0, height = 0;
    main_panel_size(panel.window, width, height);
    check("kleiner Ausschnitt erhält vertikalen Überhang", panel.content_height > height && panel.y == 0);
    check("Scrollleiste erscheint bei Überhang", width < 230);
    main_panel_scroll_to(panel, INT_MAX);
    check("Scrollen erreicht das begrenzte Ende", panel.y == event_scroll_max(panel.content_height, height) &&
          f6_test_visible_rect(panel.window, edit));
    wchar_t text[64]{};
    DWORD begin = 0, end = 0;
    GetWindowTextW(edit, text, static_cast<int>(sizeof(text) / sizeof(text[0])));
    SendMessageW(edit, EM_GETSEL, reinterpret_cast<WPARAM>(&begin), reinterpret_cast<LPARAM>(&end));
    check("Scrollen erhält Entwurf, Text- und Prefab-Auswahl", std::wcscmp(text, L"Entwurf Ä") == 0 &&
          begin == 1 && end == 4 && SendMessageW(list, LB_GETCURSEL, 0, 0) == 1);
    main_panel_scroll_to(panel, 0);
    main_panel_reveal(panel, edit);
    check("Fokus-Nachführung erreicht verborgenes Feld", panel.y > 0 && f6_test_visible_rect(panel.window, edit));
    const int old_y = panel.y;
    SendMessageW(panel.window, WM_VSCROLL, SB_LINEUP, reinterpret_cast<LPARAM>(list));
    check("Scrollmeldung eines Kindes verändert nicht den Panel-Ausschnitt", panel.y == old_y);
    main_test_last_command = 0;
    const LRESULT command_result = SendMessageW(panel.window, WM_COMMAND,
        MAKEWPARAM(9102, BN_CLICKED), reinterpret_cast<LPARAM>(edit));
    NMHDR notify{};
    notify.hwndFrom = list;
    notify.idFrom = 9999;
    const LRESULT notify_result = SendMessageW(panel.window, WM_NOTIFY, 9999, reinterpret_cast<LPARAM>(&notify));
    check("Befehle und Benachrichtigungen unverändert weitergereicht", command_result == 42 &&
          main_test_last_command == 9102 && main_test_last_code == BN_CLICKED &&
          main_test_last_sender == edit && notify_result == 77);
    const LPARAM position = MAKELPARAM(24, 35);
    const LRESULT context_result = SendMessageW(panel.window, WM_CONTEXTMENU,
                                               reinterpret_cast<WPARAM>(list), position);
    check("Kontextmenü-Nachricht unverändert weitergereicht", context_result == 33 &&
          main_test_context_sender == list && main_test_context_position == position);

    main_panel_scroll_to(panel, 0);
    SendMessageW(panel.window, WM_MOUSEWHEEL, MAKEWPARAM(0, static_cast<WORD>(-60)), 0);
    const bool half_step = panel.wheel == -60 && panel.y == 0;
    SendMessageW(panel.window, WM_MOUSEWHEEL, MAKEWPARAM(0, static_cast<WORD>(-60)), 0);
    UINT lines = 3;
    if (!SystemParametersInfoW(SPI_GETWHEELSCROLLLINES, 0, &lines, 0)) lines = 3;
    check("Mausrad sammelt Teilschritte", half_step && panel.wheel == 0 && (!lines || panel.y > 0));
    main_panel_set_dpi(panel, 144);
    main_panel_update(panel);
    main_panel_size(panel.window, width, height);
    const MainPaletteLayout scaled = main_palette_layout(width, panel.content_height, 144);
    RECT actual{};
    GetWindowRect(edit, &actual);
    MapWindowPoints(nullptr, panel.window, reinterpret_cast<POINT*>(&actual), 2);
    check("DPI-Wechsel skaliert Position und Größe des Inhalts", panel.dpi == 144 &&
          actual.top == scaled.undo.y - panel.y && actual.bottom - actual.top == scaled.undo.h);
    GetWindowTextW(edit, text, static_cast<int>(sizeof(text) / sizeof(text[0])));
    check("DPI-Wechsel erhält Text und Listenauswahl", std::wcscmp(text, L"Entwurf Ä") == 0 &&
          SendMessageW(list, LB_GETCURSEL, 0, 0) == 1);
    SetWindowPos(panel.window, nullptr, 0, 0, 400, 1200, SWP_NOZORDER | SWP_NOACTIVATE);
    main_panel_update(panel);
    main_panel_size(panel.window, width, height);
    check("größerer Ausschnitt begrenzt Scrollposition zurück", panel.y == 0 &&
          f6_test_visible_rect(panel.window, edit));
    check("Scrollleiste verschwindet ohne Überhang", width == 400 && panel.content_height == height);

    HWND tree = CreateWindowExW(0, WC_TREEVIEWW, nullptr,
        WS_CHILD | WS_VISIBLE | WS_BORDER | TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT,
        250, 0, 230, 200, owner, reinterpret_cast<HMENU>(static_cast<INT_PTR>(9201)), instance, nullptr);
    check("Kartenbaum angelegt", tree != nullptr);
    if (tree) {
        SendMessageW(tree, CCM_SETUNICODEFORMAT, TRUE, 0);
        const HTREEITEM root = main_test_tree_add(tree, TVI_ROOT, L"Projekt", 0);
        SendMessageW(tree, TVM_SELECTITEM, TVGN_CARET, reinterpret_cast<LPARAM>(root));
        check("leere Projektwurzel wird nicht für Karte 0 gehalten", root && main_tree_selected_index(tree) == -1);
        const HTREEITEM a = main_test_tree_add(tree, root, L"Erste", 0);
        const HTREEITEM b = main_test_tree_add(tree, root, L"Zweite", 1);
        SendMessageW(tree, TVM_EXPAND, TVE_EXPAND, reinterpret_cast<LPARAM>(root));
        SendMessageW(tree, TVM_SELECTITEM, TVGN_CARET, reinterpret_cast<LPARAM>(a));
        POINT pa{}, pb{};
        const bool positions = a && b && main_test_tree_point(tree, a, pa) && main_test_tree_point(tree, b, pb);
        bool changed = false;
        const bool clicked = positions && main_tree_select_at(tree, pb, changed);
        check("Rechtsklick wählt die tatsächlich getroffene Karte", clicked && changed &&
              main_tree_selected_index(tree) == 1);
        main_test_veto_selection = true;
        changed = false;
        const bool vetoed = positions && !main_tree_select_at(tree, pa, changed);
        main_test_veto_selection = false;
        check("abgelehnte Auswahl bleibt unverändert", vetoed && !changed && main_tree_selected_index(tree) == 1);
        POINT blank{220, 190};
        ClientToScreen(tree, &blank);
        changed = false;
        check("Klick auf freie Fläche ändert keine Kartenauswahl",
              !main_tree_select_at(tree, blank, changed) && !changed && main_tree_selected_index(tree) == 1);
    }
    check("Testfenster und Panel zerstört", DestroyWindow(owner) != 0 && panel.window == nullptr);
    UnregisterClassW(cls, instance);
}

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
    if (argc >= 2 && std::strcmp(argv[1], "--event-bridge") == 0) return event_bridge_modus(argc, argv);

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
    terrain_preview_tests(wurzel);
    texture_path_tests();
    placement_form_tests();
    database_form_tests(wurzel);
    map_document_tests(wurzel);
    object_props_tests(wurzel);
    event_form_tests(wurzel);
    main_layout_tests();
    event_layout_tests(wurzel);
#ifdef _WIN32
    main_window_windows_tests();
    event_view_windows_tests();
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
