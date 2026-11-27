#pragma once
// ============================================================================
//  Etappe 41: Geometrie des Datenbank-Dialogs (F9) in 96-DPI-Einheiten.
//  Ohne Win32, damit Spalten, Überlappungen und die Grenzen der Arbeitsfläche
//  kopflos geprüft werden können (tools/json_test.cpp).
//
//  Aufbau wie im XP-Datenbankfenster: Registerleiste oben, Liste links,
//  Formular rechts. Das Formular hat zwei Spalten mit je acht Zeilen; ein
//  großes JSON-Feld (Befehle gemeinsamer Events) liegt darunter.
// ============================================================================

#include <algorithm>

namespace rune::editor {

constexpr int DATABASE_WINDOW_WIDTH = 980;
constexpr int DATABASE_WINDOW_HEIGHT = 786;
constexpr int DATABASE_CLIENT_WIDTH = 964;
constexpr int DATABASE_CLIENT_HEIGHT = 744;
constexpr int DATABASE_FORM_ROWS = 8;                              // je Spalte
constexpr int DATABASE_FORM_FIELDS = DATABASE_FORM_ROWS * 2;       // 16 Felder

struct DatabaseRect {
    int x = 0, y = 0, w = 0, h = 0;
    int right() const { return x + w; }
    int bottom() const { return y + h; }
};

struct DatabaseLayout {
    DatabaseRect tabs, info, gold_label, gold;
    DatabaseRect list, button_new, button_delete, button_close;
    DatabaseRect label[DATABASE_FORM_FIELDS];
    DatabaseRect field[DATABASE_FORM_FIELDS];
    DatabaseRect big_label, big, hint, status, button_save, button_reload;
    int used = 0;             // sichtbare Formularfelder
    bool has_big = false;     // großes JSON-Feld sichtbar
    bool has_list = true;     // System hat keine Liste
};

inline bool database_rect_overlap(const DatabaseRect& a, const DatabaseRect& b) {
    return a.w > 0 && a.h > 0 && b.w > 0 && b.h > 0 &&
           a.x < b.right() && b.x < a.right() && a.y < b.bottom() && b.y < a.bottom();
}

inline DatabaseLayout database_layout(int field_count, bool has_big, bool list_visible = true) {
    DatabaseLayout out;
    out.used = std::clamp(field_count, 0, DATABASE_FORM_FIELDS);
    out.has_big = has_big;
    out.has_list = list_visible;

    out.tabs = { 8, 8, 948, 46 };
    out.info = { 12, 60, 660, 20 };
    out.gold_label = { 700, 62, 72, 18 };
    out.gold = { 776, 58, 176, 24 };

    out.list = { 12, 88, 250, 520 };
    out.button_new = { 12, 616, 78, 28 };
    out.button_delete = { 96, 616, 88, 28 };
    out.button_close = { 190, 616, 72, 28 };

    // Spalte A links, Spalte B rechts; Zeilen von oben nach unten.
    for (int i = 0; i < DATABASE_FORM_FIELDS; ++i) {
        const int column = i / DATABASE_FORM_ROWS;
        const int row = i % DATABASE_FORM_ROWS;
        const int x = column == 0 ? 278 : 622;
        const int y = 88 + row * 44;
        out.label[i] = { x, y, 330, 16 };
        out.field[i] = { x, y + 18, 330, 24 };
    }

    out.big_label = { 278, 448, 674, 16 };
    out.big = { 278, 466, 674, 140 };
    // Ohne großes Feld darf der Hinweis die freie Fläche nutzen.
    out.hint = has_big ? DatabaseRect{ 278, 614, 674, 44 } : DatabaseRect{ 278, 448, 674, 158 };
    out.status = { 12, 666, 940, 20 };
    out.button_save = { 278, 692, 130, 30 };
    out.button_reload = { 416, 692, 130, 30 };
    return out;
}

} // namespace rune::editor
