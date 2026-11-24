// Portabel: Kartenmarker, Projektstart, Kartenbaum-Tiefen und verlustfreie
// RGSS-Scriptdokumente.
#include "editor/event_rules.h"
#include "editor/map_interaction.h"
#include "editor/map_tree.h"
#include "editor/script_document.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string>

using namespace rune::editor;

static int checks = 0, failures = 0;
static void check(const char* name, bool ok) {
    ++checks;
    if (!ok) ++failures;
    std::printf("  %s  %s\n", ok ? "ok  " : "FAIL", name);
    if (!ok && std::getenv("GITHUB_ACTIONS"))
        std::printf("::error title=Editor-Arbeitsablauf::%s\n", name);
}

int main() {
    const JsonValue map = json_parse(R"({"events":[{"name":"Gespräch","x":1,"z":2,"commands":[{"cmd":"text"}]},{"x":4,"z":5,"visual":"schild","rot":1}],"npcs":[{"name":"NPC","x":-2,"z":3,"y":0.5}],"objects":[]})");
    const auto markers = map_markers(map);
    check("NPC und alle Events (auch ohne Grafik) erhalten Marker", markers.size() == 3);
    check("Marker behält Art, Index und Bodenversatz", markers[0].kind == MapMarkerKind::Npc && markers[0].index == 0 && markers[0].y == 0.5);
    check("Schild wird als Event mit Drehung erkannt", markers[2].kind == MapMarkerKind::Event && markers[2].sign && markers[2].rotation == 1);
    check("ungültiger Markerindex wird abgelehnt", !map_marker_item(map, MapMarkerKind::Event, 22));
    check("Nicht-Marker kann nicht als Event oder NPC aufgefasst werden", !map_marker_item(map, MapMarkerKind::None, 0));
    const auto first = map_marker_select(map, MapMarkerKind::Event, 0, 5);
    check("derselbe Event kann nach dem ersten Klick geöffnet werden", map_marker_same(first, map, MapMarkerKind::Event, 0, 5));
    check("andere Art oder anderes Event öffnet nicht", !map_marker_same(first, map, MapMarkerKind::Npc, 0, 5) && !map_marker_same(first, map, MapMarkerKind::Event, 1, 5));
    check("Kartenwechsel zwischen den Klicks öffnet nicht", !map_marker_same(first, map, MapMarkerKind::Event, 0, 6));
    JsonValue replaced = map;
    replaced.find("events")->arr[0].set("name", JsonValue::make_str("anderes Event"));
    check("ersetzter Eintrag am selben Index öffnet nicht", !map_marker_same(first, replaced, MapMarkerKind::Event, 0, 5));
    const auto invalid = json_parse(R"({"events":[{"x":"kaputt","z":1},{"x":1,"x":2,"z":3}],"npcs":[]})");
    check("ungültige und doppelte Koordinaten erzeugen keine erfundenen Marker", map_markers(invalid).empty());
    check("mehrdeutige Eventliste wird nicht gepickt", map_markers(json_parse(R"({"events":[{"x":1,"z":2}],"events":[]})")).empty());

    double x = 0, z = 0;
    check("Startposition rastet am Kartenraster ein", map_start_point(2.4, -3.6, 1, 20, 20, x, z) && x == 2 && z == -4);
    check("Rundung am Rand bleibt innerhalb der Karte", map_start_point(4.9, -4.9, 2, 10, 10, x, z) && x == 4 && z == -4);
    check("außerhalb der Karte kein Start", !map_start_point(5.1, 0, 1, 10, 10, x, z));
    check("NaN und ungültiges Raster werden verworfen", !map_start_point(NAN, 0, 1, 10, 10, x, z) && !map_start_point(0, 0, 0, 10, 10, x, z));
    JsonValue system;
    check("gültiges system.json lesbar", project_start_parse(R"({"name":"Welt","start":{"map":"alt","x":1,"z":2,"yaw":0.5,"extra":17},"switches":{"a":true}})", system));
    const auto start = project_start_prepare(system, "Karte_ä", -4, 7);
    check("Projektstart trägt neue Karte und Position", start.ok && start.changed && start.value.find("start")->find("map")->str == "Karte_ä" && start.value.find("start")->find("x")->number() == -4);
    check("Richtung und unbekannte Felder bleiben erhalten", start.value.find("start")->find("yaw")->number() == 0.5 && start.value.find("start")->find("extra")->number() == 17 && start.value.find("switches")->find("a")->b);
    check("Vorschau nur auf aktiv markierter Projekt-Startkarte", project_start_marker(start.value, "Karte_ä", x, z) && x == -4 && z == 7 && !project_start_marker(start.value, "alt", x, z));
    check("alte Starts werden nicht still als expliziter Projektstart markiert", !project_start_marker(system, "alt", x, z));
    check("Startbearbeitung lässt Original unverändert", system.find("start")->find("map")->str == "alt" && !system.find("start")->find("use_project_position"));
    check("doppelte Startfelder werden nicht überschrieben", !project_start_prepare(json_parse(R"({"start":{"x":1,"x":2}})"), "a", 0, 0).ok);
    check("beschädigter Start und ungültige Richtung bleiben erhalten", !project_start_prepare(json_parse(R"({"start":[]})"), "a", 0, 0).ok && !project_start_prepare(json_parse(R"({"start":{"yaw":"?"}})"), "a", 0, 0).ok);
    check("kaputtes JSON nicht als leeres System lesen", !project_start_parse("{", system));

    JsonValue document;
    const std::string original = R"({"custom":{"keep":1},"scripts":[{"name":"eins","code":"a\r\nb","extra":true},17]})";
    check("Skriptdatei wird vollständig geladen", script_document_parse(original, document) == ScriptDocumentStatus::Ok && script_document_list(document)->arr.size() == 2);
    check("doppelte Skriptliste wird abgelehnt", script_document_parse(R"({"scripts":[],"scripts":[]})", system) == ScriptDocumentStatus::InvalidScripts);
    check("ungültige Liste wird nicht durch eine leere ersetzt", script_document_parse(R"({"scripts":42})", system) == ScriptDocumentStatus::InvalidScripts);
    check("beschädigte Datei wird nicht als leere Liste gelesen", script_document_parse("{", system) == ScriptDocumentStatus::InvalidJson);
    check("historisches Arrayformat bleibt ein Array", script_document_parse(R"([{"name":"a","code":""}])", system) == ScriptDocumentStatus::Ok && system.is_arr());
    JsonValue* list = script_document_list(document);
    const ScriptForm before = script_form_read(list->arr[0]);
    ScriptForm form = before;
    form.name = "neu";
    form.code = "a\nb";
    const ScriptEdit renamed = script_form_prepare(list->arr[0], before, form);
    check("Namensänderung lässt vorhandene CRLF im Code unverändert", renamed.ok && renamed.changed && renamed.value.find("code")->str == "a\r\nb");
    check("unbekannte Scriptfelder bleiben erhalten", renamed.value.find("extra")->b);
    form.autoload = true;
    form.code = "a\r\nneu";
    const ScriptEdit edited = script_form_prepare(list->arr[0], before, form);
    check("RGSS-Startscript speichert Flag und LF-Code", edited.ok && edited.value.find("autoload")->b && edited.value.find("code")->str == "a\nneu");
    check("Original-Script bleibt bis zur Übernahme unverändert", list->arr[0].find("name")->str == "eins" && !list->arr[0].find("autoload"));
    form.name.clear();
    check("leerer neuer Name wird abgelehnt", !script_form_prepare(list->arr[0], before, form).ok);
    const JsonValue unnamed = json_parse(R"({"code":"puts 1"})");
    const ScriptForm unnamed_before = script_form_read(unnamed);
    ScriptForm unnamed_start = unnamed_before;
    unnamed_start.autoload = true;
    check("ein namenloses Script kann nicht als wirkungsloser Autostart aktiviert werden",
          !script_form_prepare(unnamed, unnamed_before, unnamed_start).ok);
    const JsonValue ambiguous = json_parse(R"({"name":"a","code":"a","code":"b"})");
    ScriptForm protected_before = script_form_read(ambiguous), protected_input = protected_before;
    protected_input.code = "neu";
    check("doppelte Codefelder sind geschützt", !protected_before.code_editable && !script_form_prepare(ambiguous, protected_before, protected_input).ok);
    check("Name eines anderen Scripts gilt als belegt", script_name_used(json_parse(R"({"scripts":[{"name":"a"},{"name":"b"}]})"), "b", 0));
    const auto tab = script_indent(L"a\r\nb\r\nc", 0, 6, false);
    check("Tab rückt markierte Zeilen ein, behält CRLF und die nächste Zeile", tab.changed && tab.begin == 0 && tab.end == 6 && tab.replacement == L"  a\r\n  b\r\n");
    const auto untab = script_indent(L"  a\n\tb\n", 0, 7, true);
    check("Shift+Tab rückt Leerzeichen und Tab aus", untab.changed && untab.replacement == L"a\nb\n");
    const auto caret = script_indent(L"  abc", 4, 4, true);
    check("einzelner Cursor bleibt beim Ausrücken sinnvoll positioniert", caret.changed && caret.replacement == L"abc" && caret.selection_begin == 2);
    check("Tab ohne Auswahl fügt zwei Leerzeichen ein", script_indent(L"abc", 1, 1, false).replacement == L"  ");
    const auto edge = script_indent(L"a\r\nb\r\nc", 0, 3, false);
    check("Auswahl bis Zeilenanfang rückt nur die vorherige Zeile ein", edge.end == 3 && edge.replacement == L"  a\r\n");
    const auto noindent = script_indent(L"abc\n", 1, 1, true);
    check("Ausrücken ohne Einzug ändert nichts", !noindent.changed);
    check("leere letzte Zeile ohne Auswahl bleibt beim Ausrücken unverändert", !script_indent(L"a\n", 2, 2, true).changed);

    // Z8: Kartenbaum-Hierarchie (Einrücktiefen je Karte, wie im XP-Baum).
    {
        std::vector<int> tiefen{2, 0, 0};
        map_tree_normalize(tiefen);
        check("Normalisierung setzt die erste Karte auf Stufe 0", tiefen[0] == 0);

        std::vector<int> sprung{0, 3};
        map_tree_normalize(sprung);
        check("keine Karte steht tiefer als eine Stufe unter ihrer Vorgängerin",
              sprung[0] == 0 && sprung[1] == 1);

        std::vector<int> negativ{-3, -1};
        map_tree_normalize(negativ);
        check("negative Tiefen werden zu 0", negativ[0] == 0 && negativ[1] == 0);

        std::vector<int> baum{0, 0, 0};
        check("Einrücken macht die Karte zum Kind der Karte darüber",
              map_tree_shift(baum, 1, 1) && baum[0] == 0 && baum[1] == 1 && baum[2] == 0);

        std::vector<int> erste{0, 0};
        check("die erste Karte kann nicht einrücken (kein Elternteil)",
              !map_tree_shift(erste, 0, 1) && erste[0] == 0);

        std::vector<int> oben{0, 0};
        check("Stufe 0 kann nicht weiter ausrücken",
              !map_tree_shift(oben, 1, -1) && oben[1] == 0);

        std::vector<int> teilbaum{0, 1, 2, 0};
        check("Ausrücken nimmt den Teilbaum mit",
              map_tree_shift(teilbaum, 1, -1) && teilbaum[0] == 0 && teilbaum[1] == 0 &&
              teilbaum[2] == 1 && teilbaum[3] == 0);

        std::vector<int> tief{0, 1, 2, 3, 4, 5, 6, 7, MAP_TREE_MAX_DEPTH};
        check("Einrücken endet an der maximalen Tiefe",
              map_tree_shift(tief, 8, 1) == false && tief[8] == MAP_TREE_MAX_DEPTH);

        check("Tiefe aus JSON: ganze Zahl wird übernommen, Grenzen geklemmt",
              map_tree_depth_from(3.0, true) == 3 && map_tree_depth_from(99.0, true) == MAP_TREE_MAX_DEPTH &&
              map_tree_depth_from(-2.0, true) == 0 && map_tree_depth_from(2.0, false) == 0);

        std::vector<int> unveraendert{0, 1, 0};
        map_tree_normalize(unveraendert);
        check("gültige Tiefen bleiben beim Normalisieren unverändert",
              unveraendert[0] == 0 && unveraendert[1] == 1 && unveraendert[2] == 0);
    }

    // Z8: gemeinsame Regeln für Editor-Issue-Scan und MapCheck.
    {
        check("if_self_switch wird wie if_switch/if_variable rekursiv geprüft",
              event_cmd_has_branches("if_switch") && event_cmd_has_branches("if_variable") &&
              event_cmd_has_branches("if_self_switch"));
        check("Kommandos ohne Zweige werden nicht rekursiv geprüft",
              !event_cmd_has_branches("text") && !event_cmd_has_branches("choices") &&
              !event_cmd_has_branches(""));
        check("nur self_switch-Kommandos tragen einen Buchstaben",
              event_cmd_uses_self_switch_letter("self_switch") &&
              event_cmd_uses_self_switch_letter("if_self_switch") &&
              !event_cmd_uses_self_switch_letter("if_switch"));
        check("Self-Switch-Buchstaben sind A-D (wie GameState)",
              event_self_switch_letter_ok("A") && event_self_switch_letter_ok("D") &&
              !event_self_switch_letter_ok("E") && !event_self_switch_letter_ok("") &&
              !event_self_switch_letter_ok("a"));
    }

    std::printf("EDITOR-ARBEITSABLAUF: %d Prüfungen, %d Fehler\n", checks, failures);
    if (std::getenv("GITHUB_ACTIONS"))
        std::printf("::notice title=Editor-Arbeitsablauf::%d Prüfungen, %d Fehler\n", checks, failures);
    return failures ? 1 : 0;
}
