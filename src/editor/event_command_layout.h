#pragma once
// ============================================================================
//  Etappe 44: Geometrie des XP-Befehlsdialogs (F6) in 96-DPI-Einheiten.
//  Ohne Win32, damit Spalten, Abschnitte, Überlappungen und die Grenzen der
//  Arbeitsfläche kopflos geprüft werden können
//  (tools/event_command_catalog_test.cpp).
//
//  Aufbau wie in RPG Maker XP: Registerleiste oben (drei Tabs), darunter je
//  Tab zwei Spalten mit je zwei Abschnitten. Jeder Abschnitt hat eine
//  Überschrift und darunter seine Befehle als Zeilen. Unten: Hinweis, OK und
//  Abbrechen. Die Zeilenhöhe ist fest; die Fensterhöhe richtet sich nach der
//  höchsten Spalte über alle drei Tabs.
// ============================================================================

#include "editor/event_command_catalog.h"

#include <algorithm>
#include <cstdint>
#include <limits>

namespace rune::editor {

constexpr int EC_DIALOG_WIDTH = 780;
constexpr int EC_DIALOG_HEIGHT = 560;
constexpr unsigned EC_DIALOG_DPI = 96;
constexpr int EC_MARGIN = 12;
constexpr int EC_TAB_HEIGHT = 26;
constexpr int EC_SECTION_TITLE_HEIGHT = 18;
constexpr int EC_ROW_HEIGHT = 22;
constexpr int EC_ROW_GAP = 2;
constexpr int EC_SECTION_GAP = 10;
constexpr int EC_MAX_ROWS_PER_COLUMN = 16;

struct EcRect {
    int x = 0, y = 0, w = 0, h = 0;
    int right() const { return x + w; }
    int bottom() const { return y + h; }
};

inline bool ec_rect_overlap(const EcRect& a, const EcRect& b) {
    return a.w > 0 && a.h > 0 && b.w > 0 && b.h > 0 &&
           a.x < b.right() && b.x < a.right() && a.y < b.bottom() && b.y < a.bottom();
}

struct EcColumnLayout {
    EcRect title[2];                       // Überschrift je Abschnitt
    EcRect row[EC_MAX_ROWS_PER_COLUMN];    // Befehlszeilen (beide Abschnitte)
    int row_section[EC_MAX_ROWS_PER_COLUMN]; // zu welchem Abschnitt (0/1) die Zeile gehört
    int rows = 0;
    int bottom = 0;
};

struct EcDialogLayout {
    EcRect tabs;
    EcColumnLayout column[2];
    EcRect hint, ok, cancel;
    int content_bottom = 0;
};

inline int ec_dialog_scale(int value, unsigned dpi) {
    if (!dpi) dpi = EC_DIALOG_DPI;
    std::int64_t product = static_cast<std::int64_t>(value) * dpi;
    product += static_cast<std::int64_t>(EC_DIALOG_DPI / 2);
    return static_cast<int>(product / EC_DIALOG_DPI);
}

// Spaltenhöhe: zwei Abschnittsüberschriften + alle Zeilen + eine Lücke.
inline int ec_column_height(int tab, int column) {
    int rows = ec_column_count(tab, column);
    return 2 * (EC_SECTION_TITLE_HEIGHT + 4) + rows * (EC_ROW_HEIGHT + EC_ROW_GAP) + EC_SECTION_GAP;
}

// Höchste Spalte über alle Tabs bestimmt die Fensterhöhe.
inline int ec_dialog_content_height() {
    int highest = 0;
    for (int tab = 1; tab <= EC_TAB_COUNT; ++tab)
        for (int col = 0; col < EC_COLUMN_COUNT; ++col)
            highest = std::max(highest, ec_column_height(tab, col));
    return highest;
}

inline EcDialogLayout ec_dialog_layout(int tab) {
    EcDialogLayout out;
    const int width = EC_DIALOG_WIDTH;
    out.tabs = {EC_MARGIN, EC_MARGIN, width - 2 * EC_MARGIN, EC_TAB_HEIGHT};
    const int content_top = out.tabs.bottom() + 10;
    const int col_width = (width - 3 * EC_MARGIN) / 2;

    for (int col = 0; col < EC_COLUMN_COUNT; ++col) {
        EcColumnLayout& cl = out.column[col];
        const int x = EC_MARGIN + col * (col_width + EC_MARGIN);
        int y = content_top;
        // Zwei Abschnitte je Spalte, in Katalog-Reihenfolge.
        const EcSectionInfo* sections[2] = {nullptr, nullptr};
        int found = 0;
        for (int i = 0; i < EC_SECTION_COUNT && found < 2; ++i) {
            if (EC_SECTIONS[i].tab == tab && EC_SECTIONS[i].column == col)
                sections[found++] = &EC_SECTIONS[i];
        }
        for (int s = 0; s < 2; ++s) {
            cl.title[s] = {x, y, col_width, EC_SECTION_TITLE_HEIGHT};
            y += EC_SECTION_TITLE_HEIGHT + 4;
            if (!sections[s]) continue;
            int indices[EC_MAX_ROWS_PER_COLUMN];
            int n = ec_section_commands(tab, col, sections[s]->key, indices, EC_MAX_ROWS_PER_COLUMN);
            for (int r = 0; r < n && cl.rows < EC_MAX_ROWS_PER_COLUMN; ++r) {
                cl.row[cl.rows] = {x, y, col_width, EC_ROW_HEIGHT};
                cl.row_section[cl.rows] = s;
                ++cl.rows;
                y += EC_ROW_HEIGHT + EC_ROW_GAP;
            }
            y += EC_SECTION_GAP;
        }
        cl.bottom = y;
        out.content_bottom = std::max(out.content_bottom, y);
    }

    const int button_y = EC_DIALOG_HEIGHT - EC_MARGIN - 26;
    out.hint = {EC_MARGIN, button_y - 22, width - 2 * EC_MARGIN, 18};
    out.cancel = {width - EC_MARGIN - 90, button_y, 90, 26};
    out.ok = {out.cancel.x - 10 - 90, button_y, 90, 26};
    return out;
}

// Fenster auf die Arbeitsfläche begrenzen (negative Koordinaten erlaubt).
inline EcRect ec_dialog_fit(EcRect work, EcRect wanted) {
    const std::int64_t ww = static_cast<std::int64_t>(work.right()) - work.x;
    const std::int64_t wh = static_cast<std::int64_t>(work.bottom()) - work.y;
    if (ww <= 0 || wh <= 0) return wanted;
    const int width = static_cast<int>(std::clamp(static_cast<std::int64_t>(wanted.w), std::int64_t{1}, ww));
    const int height = static_cast<int>(std::clamp(static_cast<std::int64_t>(wanted.h), std::int64_t{1}, wh));
    const int x = static_cast<int>(std::clamp(static_cast<std::int64_t>(wanted.x),
        static_cast<std::int64_t>(work.x), static_cast<std::int64_t>(work.right()) - width));
    const int y = static_cast<int>(std::clamp(static_cast<std::int64_t>(wanted.y),
        static_cast<std::int64_t>(work.y), static_cast<std::int64_t>(work.bottom()) - height));
    return {x, y, width, height};
}

} // namespace rune::editor
