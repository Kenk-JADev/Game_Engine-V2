// ============================================================================
//  event_command_catalog_test.cpp — Etappe 44: portabler XP-Befehlskatalog.
//
//  Prüft ohne Win32, dass src/editor/event_command_catalog.h und
//  event_command_layout.h den Bauplan der Rpg-Schicht exakt abbilden:
//  90 Befehle, drei Tabs, je zwei Spalten und zwei Abschnitte, XP-Codes,
//  Feldzahl, Zuordnung zu den bestehenden Event-Strings (EV_TYPES) und eine
//  überlappungsfreie Geometrie. Quelle ist Rpg::EventCommands::Layout; die
//  Datei wird mit tools/gen_event_command_catalog.rb + .py erzeugt.
//     g++ -std=c++20 -O1 -Wall -Wextra -Isrc tools/event_command_catalog_test.cpp -o build/ec_test
// ============================================================================

#include "editor/event_command_catalog.h"
#include "editor/event_command_layout.h"
#include "editor/event_form.h"

#include <cstdio>
#include <cstring>
#include <string>

using namespace rune::editor;

namespace {
int g_fail = 0;
int g_count = 0;
void check(const char* name, bool ok) {
    ++g_count;
    if (!ok) ++g_fail;
    std::printf("  %s %s\n", ok ? "ok  " : "FAIL", name);
}

bool cmd_exists(const char* cmd) {
    for (int i = 0; i < ET_COUNT; ++i)
        if (std::strcmp(EV_TYPES[i], cmd) == 0) return true;
    return false;
}

int section_count(int tab, int col, const char* key) {
    int idx[64];
    return ec_section_commands(tab, col, key, idx, 64);
}
} // namespace

int main() {
    std::printf("== XP-Befehlskatalog (Etappe 44) ==\n");
    check("90 Befehle", ec_command_count() == 90);
    check("drei Tabs", EC_TAB_COUNT == 3);
    check("zwoelf Abschnitte", EC_SECTION_COUNT == 12);

    // Abschnittsgrößen exakt wie im Bauplan (Layout).
    check("Tab1 links dialog=7", section_count(1, 0, "dialog") == 7);
    check("Tab1 links flow=8", section_count(1, 0, "flow") == 8);
    check("Tab1 rechts state=9", section_count(1, 1, "state") == 9);
    check("Tab1 rechts system=6", section_count(1, 1, "system") == 6);
    check("Tab2 links map=10", section_count(2, 0, "map") == 10);
    check("Tab2 links screen=5", section_count(2, 0, "screen") == 5);
    check("Tab2 rechts picture=6", section_count(2, 1, "picture") == 6);
    check("Tab2 rechts audio=9", section_count(2, 1, "audio") == 9);
    check("Tab3 links battle=3", section_count(3, 0, "battle") == 3);
    check("Tab3 links actor=12", section_count(3, 0, "actor") == 12);
    check("Tab3 rechts enemy=10", section_count(3, 1, "enemy") == 10);
    check("Tab3 rechts system=5", section_count(3, 1, "system") == 5);

    // Je Tab zwei Spalten, je Spalte zwei Abschnitte.
    for (int tab = 1; tab <= 3; ++tab) {
        for (int col = 0; col < 2; ++col) {
            int sections = 0;
            for (int i = 0; i < EC_SECTION_COUNT; ++i)
                if (EC_SECTIONS[i].tab == tab && EC_SECTIONS[i].column == col) ++sections;
            char name[64];
            std::snprintf(name, sizeof name, "Tab%d Spalte%d hat zwei Abschnitte", tab, col);
            check(name, sections == 2);
        }
    }

    // XP-Codes je Tab und eindeutige Typen.
    bool codes_ok = true, types_ok = true, fields_total = true;
    int field_sum = 0;
    for (int i = 0; i < ec_command_count(); ++i) {
        const EcCommand& c = ec_command(i);
        field_sum += c.field_count;
        if (c.tab == 1 && (c.code < 101 || c.code > 136)) codes_ok = false;
        if (c.tab == 2 && (c.code < 201 || c.code > 251)) codes_ok = false;
        if (c.tab == 3 && (c.code < 301 || c.code > 355)) codes_ok = false;
        if (ec_find(c.type) != &c) types_ok = false;
        for (int j = i + 1; j < ec_command_count(); ++j)
            if (std::strcmp(ec_command(j).type, c.type) == 0) types_ok = false;
    }
    check("XP-Codes je Tab im Bereich", codes_ok);
    check("Typen eindeutig und auffindbar", types_ok);
    check("235 Felder gesamt", field_sum == 235);
    (void)fields_total;

    // Jeder zugeordnete Event-String existiert im bestehenden Register.
    bool cmd_ok = true;
    int wired = 0;
    for (int i = 0; i < ec_command_count(); ++i) {
        const EcCommand& c = ec_command(i);
        if (c.cmd) { ++wired; if (!cmd_exists(c.cmd)) cmd_ok = false; }
    }
    check("alle cmd-Zuordnungen existieren in EV_TYPES", cmd_ok);
    check("mehr als die Haelfte ist verdrahtet", wired >= 45);
    std::printf("  info %d von 90 Befehlen haben einen bestehenden cmd-String\n", wired);

    // Pflichtfelder haben einen Typ; Enum-Felder tragen ihre Auswahl.
    bool enum_ok = true;
    for (int i = 0; i < ec_command_count(); ++i) {
        const EcCommand& c = ec_command(i);
        for (int f = 0; f < c.field_count; ++f) {
            const EcField& field = c.fields[f];
            if (field.kind == EcFieldKind::Enum && field.value_count <= 0) enum_ok = false;
            if (field.kind == EcFieldKind::Enum && !field.values) enum_ok = false;
        }
    }
    check("Enum-Felder tragen ihre Auswahlwerte", enum_ok);

    // Geometrie: keine Überlappungen, alles im Fenster.
    bool layout_ok = true;
    for (int tab = 1; tab <= 3; ++tab) {
        EcDialogLayout l = ec_dialog_layout(tab);
        for (int col = 0; col < 2; ++col) {
            const EcColumnLayout& cl = l.column[col];
            for (int r = 0; r < cl.rows; ++r) {
                if (cl.row[r].w <= 0 || cl.row[r].h <= 0) layout_ok = false;
                if (cl.row[r].bottom() > l.hint.y) layout_ok = false;
                for (int s = r + 1; s < cl.rows; ++s)
                    if (ec_rect_overlap(cl.row[r], cl.row[s])) layout_ok = false;
            }
            for (int s = 0; s < 2; ++s)
                for (int r = 0; r < cl.rows; ++r)
                    if (ec_rect_overlap(cl.title[s], cl.row[r])) layout_ok = false;
        }
        if (ec_rect_overlap(l.ok, l.cancel)) layout_ok = false;
        if (l.ok.bottom() > EC_DIALOG_HEIGHT || l.cancel.bottom() > EC_DIALOG_HEIGHT) layout_ok = false;
        if (l.content_bottom > l.hint.y) layout_ok = false;
    }
    check("Geometrie ueberlappungsfrei und im Fenster", layout_ok);

    // Höchste Spalte passt unter die Hinweiszeile.
    check("Fensterhoehe reicht fuer die hoechste Spalte",
          ec_dialog_content_height() + 48 < EC_DIALOG_HEIGHT - 60);

    // Arbeitsfläche begrenzt das Fenster (auch negative Koordinaten).
    EcRect work = {-1920, 0, 1920, 1080};
    EcRect fitted = ec_dialog_fit(work, {-3000, -200, EC_DIALOG_WIDTH, EC_DIALOG_HEIGHT});
    check("Fit haelt Fenster in der Arbeitsflaeche",
          fitted.x >= work.x && fitted.y >= work.y &&
          fitted.right() <= work.right() && fitted.bottom() <= work.bottom());

    // DPI-Skalierung rundet wie MulDiv.
    check("96 DPI bleibt unveraendert", ec_dialog_scale(100, 96) == 100);
    check("144 DPI skaliert 100 auf 150", ec_dialog_scale(100, 144) == 150);

    std::printf("\n%d Checks, %d Fehler\n", g_count, g_fail);
    std::printf(g_fail ? "EVENT-COMMAND-KATALOG FEHLGESCHLAGEN\n" : "EVENT-COMMAND-KATALOG OK\n");
    return g_fail ? 1 : 0;
}
