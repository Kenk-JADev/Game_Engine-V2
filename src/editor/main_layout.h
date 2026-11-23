#pragma once
// Geometrie des RuneEditor-Hauptfensters. Alle festen Maße sind 96-DPI-
// Einheiten; das Ergebnis sind Pixel. Keine Win32-Abhängigkeit, damit die
// Aufteilung und die Grenzen der Seitenleisten auch kopflos geprüft werden.

#include "editor/event_layout.h"

#include <algorithm>
#include <cstdint>
#include <string_view>

namespace rune::editor {

struct MainRect {
    int x = 0, y = 0, w = 0, h = 0;
};

inline unsigned main_layout_dpi(unsigned dpi) {
    // Auch bei beschädigten/testweise extremen Werten bleiben alle Maße sicher.
    return dpi ? std::clamp(dpi, 48u, 768u) : 96u;
}

inline int main_layout_scale(int value, unsigned dpi) {
    return event_layout_scale(value, main_layout_dpi(dpi));
}

inline int main_layout_size(int value) {
    // Fenstergrößen sind in der Praxis wesentlich kleiner; begrenzt zugleich
    // Summen und Prozentrechnungen bei ungültigen Eingaben.
    return std::clamp(value, 0, 1000000);
}

inline bool main_rect_contains(const MainRect& rect, int x, int y) {
    return rect.w > 0 && rect.h > 0 && x >= rect.x && y >= rect.y &&
           static_cast<std::int64_t>(x) < static_cast<std::int64_t>(rect.x) + rect.w &&
           static_cast<std::int64_t>(y) < static_cast<std::int64_t>(rect.y) + rect.h;
}

constexpr int MAIN_DEFAULT_MAP_HEIGHT = 180; // inklusive Kartenüberschrift

// Gepostete Kartenaktionen dürfen nach einem abgebrochenen Wechsel oder einer
// neuen Auswahl nicht versehentlich auf die zuvor geladene Karte wirken.
inline bool main_map_action_matches(int expected, int selected, int loaded) {
    return expected == selected && (expected < 0 || expected == loaded);
}

inline bool main_map_action_can_post(bool pending, bool posted, bool prompting,
                                     bool selection_queued, bool selection_resolving) {
    return pending && !posted && !prompting && !selection_queued && !selection_resolving;
}

// Gleiche Listenindizes können nach Projektwechsel oder Listenumbau eine
// andere Datei bezeichnen. Eine ausstehende Aktion bleibt an beides gebunden.
inline bool main_map_action_target_matches(int expected, int selected, int loaded,
                                           std::uint64_t project_before, std::uint64_t project_now,
                                           std::string_view file_before, std::string_view file_now) {
    return project_before == project_now && main_map_action_matches(expected, selected, loaded) &&
           (expected < 0 || (!file_before.empty() && file_before == file_now));
}

struct MainWindowLayout {
    MainRect palette, splitter, maps_label, maps_new, maps_props, maps_tree;
    MainRect viewport, inspector;
    int content_bottom = 0;
    int map_height = 0; // tatsächlich verfügbare Höhe in Pixeln
};

inline MainWindowLayout main_window_layout(int width, int height, int toolbar_height,
                                           int status_height, int wanted_map_height,
                                           unsigned dpi) {
    MainWindowLayout out;
    const int w = main_layout_size(width), h = main_layout_size(height);
    const auto s = [dpi](int n) { return main_layout_scale(n, dpi); };
    const int top = std::clamp(toolbar_height, 0, h);
    const int bottom = std::clamp(h - std::max(0, status_height), top, h);
    const int available = bottom - top;
    out.content_bottom = bottom;

    int left = std::clamp(w * 16 / 100, s(214), s(280));
    int right = std::clamp(w * 19 / 100, s(258), s(340));
    const int min_view = s(140);
    if (static_cast<std::int64_t>(left) + right + min_view > w) {
        // Wenn der Monitor kleiner als die bevorzugten Mindestmaße ist,
        // bleiben alle drei Spalten innerhalb des Fensters und überlappen nie.
        const int budget = std::max(0, w - min_view);
        const std::int64_t sum = static_cast<std::int64_t>(left) + right;
        left = sum ? static_cast<int>(static_cast<std::int64_t>(budget) * left / sum) : 0;
        right = budget - left;
    }
    out.viewport = {left, top, std::max(0, w - left - right), available};
    out.inspector = {w - right, top, right, available};

    const int splitter = std::min(s(8), available);
    const int remaining = available - splitter;
    const int min_maps = std::min(s(90), remaining / 2);
    const int min_palette = std::min(s(140), remaining - min_maps);
    const int max_maps = std::max(min_maps, remaining - min_palette);
    const int desired = s(std::clamp(wanted_map_height, 0, 1000000));
    const int maps = std::clamp(desired, min_maps, max_maps);
    const int palette = remaining - maps;
    out.map_height = maps;
    out.palette = {0, top, left, palette};
    out.splitter = {0, top + palette, left, splitter};

    const int maps_top = out.splitter.y + splitter;
    const int padding = std::min(s(8), left / 2);
    const int inner_w = std::max(0, left - 2 * padding);
    const int gap = std::min(s(4), inner_w / 4);
    // Auf sehr schmalen Arbeitsflächen erst die Überschrift sichern und den
    // Eigenschaftenknopf verkürzen; die Oberfläche beschriftet ihn dann „F2“.
    const int label_min = std::min(s(52), inner_w);
    const int new_w = std::min(s(42), std::max(0, inner_w - label_min - 2 * gap));
    const int props_w = std::min(s(96), std::max(0, inner_w - label_min - new_w - 2 * gap));
    const int label_w = std::max(0, inner_w - new_w - props_w - 2 * gap);
    const int header_h = std::min(s(26), maps);
    const int button_h = std::min(s(24), header_h);
    const int button_y = maps_top + (header_h - button_h) / 2;
    out.maps_label = {padding, maps_top + (header_h - std::min(s(20), header_h)) / 2,
                      label_w, std::min(s(20), header_h)};
    out.maps_new = {padding + label_w + gap, button_y, new_w, button_h};
    out.maps_props = {out.maps_new.x + new_w + gap, button_y, props_w, button_h};
    const int tree_gap = std::min(s(4), maps - header_h);
    const int tree_y = maps_top + header_h + tree_gap;
    const int tree_bottom = std::max(tree_y, bottom - std::min(s(6), bottom - tree_y));
    out.maps_tree = {padding, tree_y, inner_w, tree_bottom - tree_y};
    return out;
}

// Die Wunschhöhe bleibt bei Fensterverkleinerung erhalten. Nur ein echter
// Splitterzug ändert sie; beim Loslassen kann man beide Bereiche nicht ganz
// verstecken. map_height im Ergebnis ist der begrenzte Istwert.
inline int main_map_height_for_split(int width, int height, int toolbar_height,
                                     int status_height, int splitter_top, unsigned dpi) {
    const MainWindowLayout initial = main_window_layout(width, height, toolbar_height,
        status_height, MAIN_DEFAULT_MAP_HEIGHT, dpi);
    const std::int64_t raw = static_cast<std::int64_t>(initial.content_bottom) -
                             splitter_top - initial.splitter.h;
    const int pixels = event_layout_int(std::clamp(raw, std::int64_t{0}, std::int64_t{1000000}));
    const int wanted = event_layout_rescale(pixels, main_layout_dpi(dpi), 96);
    const MainWindowLayout bounded = main_window_layout(width, height, toolbar_height,
        status_height, wanted, dpi);
    return event_layout_rescale(bounded.map_height, main_layout_dpi(dpi), 96);
}

inline bool main_map_props_compact(int width, unsigned dpi) {
    return width < main_layout_scale(90, dpi);
}

struct MainPaletteLayout {
    MainRect tools, radios[5], prefabs, prefab_list, actions;
    MainRect rotate, remove, duplicate, undo, redo;
};

inline int main_palette_min_height(unsigned dpi) {
    const auto s = [dpi](int n) { return main_layout_scale(n, dpi); };
    return s(4) + s(154) + s(6) + s(98) + s(6) + s(96) + s(6) + s(30) + s(4);
}

inline MainPaletteLayout main_palette_layout(int width, int height, unsigned dpi) {
    MainPaletteLayout out;
    const int w = main_layout_size(width);
    const auto s = [dpi](int n) { return main_layout_scale(n, dpi); };
    const int content = std::max(main_layout_size(height), main_palette_min_height(dpi));
    const int outer_x = std::min(s(4), w / 2);
    const int inner_x = std::min(s(14), w / 2);
    const int inner_w = std::max(0, w - 2 * inner_x);
    const int gap = std::min(s(6), inner_w);
    const int half = std::max(0, (inner_w - gap) / 2);
    const int top = s(4), tool_h = s(154);
    out.tools = {outer_x, top, std::max(0, w - 2 * outer_x), tool_h};
    for (int i = 0; i < 5; ++i)
        out.radios[i] = {inner_x, top + s(20) + i * s(26), inner_w, s(24)};
    const int fixed = main_palette_min_height(dpi) - s(98);
    const int prefab_h = std::max(s(98), content - fixed);
    const int prefab_y = top + tool_h + s(6);
    out.prefabs = {outer_x, prefab_y, std::max(0, w - 2 * outer_x), prefab_h};
    out.prefab_list = {inner_x, prefab_y + s(20), inner_w, std::max(0, prefab_h - s(28))};
    const int action_y = prefab_y + prefab_h + s(6);
    out.actions = {outer_x, action_y, std::max(0, w - 2 * outer_x), s(96)};
    out.rotate = {inner_x, action_y + s(20), half, s(28)};
    out.remove = {inner_x + half + gap, action_y + s(20), inner_w - half - gap, s(28)};
    out.duplicate = {inner_x, action_y + s(52), inner_w, s(28)};
    const int undo_y = action_y + s(96) + s(6);
    out.undo = {inner_x, undo_y, half, s(30)};
    out.redo = {inner_x + half + gap, undo_y, inner_w - half - gap, s(30)};
    return out;
}

struct MainInspectorLayout {
    MainRect objects_label, objects, props, labels[4], fields[4], apply, hint;
};

inline int main_inspector_min_height(unsigned dpi) {
    const auto s = [dpi](int n) { return main_layout_scale(n, dpi); };
    return s(30) + s(80) + s(8) + s(242);
}

inline MainInspectorLayout main_inspector_layout(int width, int height, unsigned dpi) {
    MainInspectorLayout out;
    const int w = main_layout_size(width);
    const auto s = [dpi](int n) { return main_layout_scale(n, dpi); };
    const int content = std::max(main_layout_size(height), main_inspector_min_height(dpi));
    const int py = content - s(242);
    out.objects_label = {s(8), s(8), std::max(0, w - s(16)), s(20)};
    out.objects = {s(8), s(30), std::max(0, w - s(16)), std::max(0, py - s(38))};
    out.props = {s(4), py, std::max(0, w - s(8)), s(238)};
    for (int i = 0; i < 4; ++i) {
        out.labels[i] = {s(14), py + s(28) + i * s(32), s(60), s(20)};
        out.fields[i] = {s(80), py + s(24) + i * s(32), std::max(0, w - s(100)), s(24)};
    }
    out.apply = {s(14), py + s(158), std::max(0, w - s(28)), s(30)};
    out.hint = {s(14), py + s(192), std::max(0, w - s(28)), s(36)};
    return out;
}

} // namespace rune::editor
