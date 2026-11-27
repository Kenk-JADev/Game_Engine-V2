#pragma once
// ============================================================================
//  Z8: Kartenbaum-Hierarchie wie im RPG Maker XP - Einrücktiefen je Karte.
//
//  Reine Rechnung ohne Win32, damit CI sie portabel prüfen kann
//  (`tools/editor_workflow_test.cpp`). Der Editor speichert die Tiefen als
//  "map_tree": { "<karten-id>": tiefe } in der system.json des Projekts; die
//  Reihenfolge der Dateiliste bleibt unverändert, damit Karten-Indizes stabil
//  bleiben. Die Engine liest den Schlüssel nicht.
//
//  Invariante (Normalisierung): die erste Karte steht auf Stufe 0 und keine
//  Karte tiefer als eine Stufe unter ihrer Vorgängerin - sonst gäbe es Knoten
//  ohne Elternteil.
// ============================================================================

#include <algorithm>
#include <cstddef>
#include <vector>

namespace rune::editor {

inline constexpr int MAP_TREE_MAX_DEPTH = 8;

// Einrücktiefe aus einem JSON-Wert: nur echte Zahlen > 0, geklemmt auf 0..MAX.
inline int map_tree_depth_from(double wert, bool ist_zahl) {
    if (!ist_zahl || !(wert > 0.0)) return 0;
    return static_cast<int>(std::min(wert, static_cast<double>(MAP_TREE_MAX_DEPTH)));
}

// Tiefen normalisieren (siehe Invariante oben).
inline void map_tree_normalize(std::vector<int>& tiefen) {
    int vorgaenger = 0;
    for (std::size_t i = 0; i < tiefen.size(); ++i) {
        const int grenze = i == 0 ? 0 : vorgaenger + 1;
        tiefen[i] = std::clamp(tiefen[i], 0, grenze);
        vorgaenger = tiefen[i];
    }
}

// Einrücken (+1) oder Ausrücken (-1) für einen Knoten. Der Teilbaum (der
// zusammenhängende Block tieferer Stufen) wandert mit, damit die Invariante
// erhalten bleibt. Rückgabe false, wenn die Bewegung nicht möglich ist:
// die erste Karte kann nicht einrücken (kein Elternteil), Stufe 0 nicht
// weiter ausrücken, Stufe MAX nicht weiter einrücken.
inline bool map_tree_shift(std::vector<int>& tiefen, int index, int delta) {
    if (delta == 0 || index < 0 || index >= static_cast<int>(tiefen.size())) return false;
    const std::size_t pos = static_cast<std::size_t>(index);
    if (delta > 0 && (index == 0 || tiefen[pos] >= MAP_TREE_MAX_DEPTH)) return false;
    if (delta < 0 && tiefen[pos] == 0) return false;
    const int wurzel = tiefen[pos];
    for (std::size_t i = pos; i < tiefen.size(); ++i) {
        if (i != pos && tiefen[i] <= wurzel) break;   // Teilbaum endet hier
        tiefen[i] = std::clamp(tiefen[i] + delta, 0, MAP_TREE_MAX_DEPTH);
    }
    return true;
}

} // namespace rune::editor
