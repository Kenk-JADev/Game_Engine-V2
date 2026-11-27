#pragma once
// Etappe 37: Icon- und Fettwahl für den XP-Kartenbaum. Reine Logik ohne Win32.

#include <cstddef>
#include <vector>

namespace rune::editor {

enum MapTreeIcon : int {
    MapIconRoot = 0,
    MapIconBranch = 1,
    MapIconLeaf = 2,
    MapIconStart = 3,
    MapIconCount = 4,
};

inline bool map_tree_has_children(const std::vector<int>& depths, std::size_t i) {
    return i + 1 < depths.size() && depths[i + 1] > depths[i];
}

inline int map_tree_icon(int depth, bool has_children, bool is_start) {
    (void)depth; // die Baumstruktur kommt aus den Tiefen, das Icon aus dem Zustand
    if (is_start) return MapIconStart;
    return has_children ? MapIconBranch : MapIconLeaf;
}

} // namespace rune::editor
