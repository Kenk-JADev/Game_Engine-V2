#pragma once
// Kleine Win32-Helfer für die Kartenliste. Die Wurzel ist ausdrücklich kein
// Kartenindex, auch wenn ein leeres Projekt keine Kindknoten besitzt.
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commctrl.h>

#include <limits>

namespace rune::editor {

inline HTREEITEM main_tree_selection(HWND tree) {
    return tree ? reinterpret_cast<HTREEITEM>(SendMessageW(tree, TVM_GETNEXTITEM, TVGN_CARET, 0)) : nullptr;
}

inline int main_tree_selected_index(HWND tree) {
    const HTREEITEM selected = main_tree_selection(tree);
    const HTREEITEM root = tree ? reinterpret_cast<HTREEITEM>(
        SendMessageW(tree, TVM_GETNEXTITEM, TVGN_ROOT, 0)) : nullptr;
    if (!selected || selected == root) return -1;
    TVITEMW item{};
    item.mask = TVIF_PARAM;
    item.hItem = selected;
    if (!SendMessageW(tree, TVM_GETITEMW, 0, reinterpret_cast<LPARAM>(&item)) ||
        item.lParam < 0 || item.lParam > std::numeric_limits<int>::max()) return -1;
    return static_cast<int>(item.lParam);
}

// Rechtsklick auf einen Knoten wählt genau diesen aus. Die Entscheidung über
// Laden/Speichern/Abbrechen trifft anschließend unverändert der Aufrufer.
// Klicks auf freie Fläche und Tastatur-Kontextmenü ändern die Auswahl nicht.
inline bool main_tree_select_at(HWND tree, POINT screen, bool& changed) {
    changed = false;
    if (!tree || !ScreenToClient(tree, &screen)) return false;
    TVHITTESTINFO hit{};
    hit.pt = screen;
    SendMessageW(tree, TVM_HITTEST, 0, reinterpret_cast<LPARAM>(&hit));
    if (!hit.hItem || !(hit.flags & (TVHT_ONITEM | TVHT_ONITEMBUTTON))) return false;
    if (hit.hItem == main_tree_selection(tree)) return true;
    if (!SendMessageW(tree, TVM_SELECTITEM, TVGN_CARET, reinterpret_cast<LPARAM>(hit.hItem)))
        return false;
    changed = true;
    return true;
}

} // namespace rune::editor
#endif // _WIN32
