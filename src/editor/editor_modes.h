#pragma once
// Etappe 30: XP-Menüs „Modus“ und „Maßstab“ in 3D übersetzt
// (Recherche: docs/XP_EDITOR_RECHERCHE.md). Reine Logik ohne Win32.
//
//   XP Ebene 1 (Boden)      -> Modus Gelände (Boden malen / Höhe)
//   XP Ebene 2/3 (Objekte)  -> Modus Objekte (Setzen / Auswahl / Verschieben)
//   XP Ereignisebene        -> Modus Events  (Auswahl + Wege zu F5/F6)
//   XP Maßstab 1/1,1/2,1/4  -> Kamerahöhe nah / mittel / weit
//
// Die Werkzeugnummern entsprechen set_mode() in editor_main.cpp:
// 1 Setzen, 2 Auswahl, 3 Verschieben, 4 Boden malen, 5 Höhe malen.

namespace rune::editor {

enum class EditorLayer { Terrain = 0, Objects = 1, Events = 2 };

// Werkzeug, mit dem ein Modus startet.
inline int editor_layer_start_tool(EditorLayer layer) {
    switch (layer) {
    case EditorLayer::Terrain: return 4;
    case EditorLayer::Objects: return 1;
    case EditorLayer::Events:  return 2;
    }
    return 2;
}

// Welcher Modus zu einem direkt gewählten Werkzeug gehört. Etappe 37: Das
// Auswahlwerkzeug (2) ist neutral und verlässt die aktive Ebene nicht – sonst
// sprang ein Klick auf „Auswählen“ im Geländemodus nach „Objekte“, obwohl nur
// markiert werden sollte. Setzen/Verschieben gehören zu den Objektebenen,
// Boden/Höhe zum Gelände.
inline EditorLayer editor_layer_for_tool(int tool, EditorLayer current) {
    if (tool == 4 || tool == 5) return EditorLayer::Terrain;
    if (tool == 2) return current;
    return EditorLayer::Objects;
}

// Etappe 33: Zusätzlicher Trefferrand (Meter) für Event-/NPC-Marker. Im
// Modus Events (XP-Ereignisebene) sollen auch kleine Marker leicht zu
// treffen sein; in den anderen Modi bleibt ein kleiner Rand, damit
// Objekte daneben weiter wählbar sind.
inline float editor_marker_pick_pad(EditorLayer layer) {
    return layer == EditorLayer::Events ? 0.45f : 0.10f;
}

// Was ein Doppelklick auf FREIEN Boden bewirkt (kein Marker getroffen).
// XP: Ereignisebene = neues Event an dieser Stelle; sonst zählt der zweite
// Klick wie bisher als normaler Klick des Werkzeugs.
enum class EditorEmptyDoubleClick { NewEvent, ToolClick };

inline EditorEmptyDoubleClick editor_empty_double_click(EditorLayer layer) {
    return layer == EditorLayer::Events ? EditorEmptyDoubleClick::NewEvent
                                        : EditorEmptyDoubleClick::ToolClick;
}

struct EditorScaleView {
    float height = 0.0f; // Kamerahöhe über Grund in Metern
    float pitch = 0.0f;  // Blickneigung (negativ = nach unten)
};

// Maßstab 0 = 1/1 (nah), 1 = 1/2, 2 = 1/4 (weit). span = größere Kartenseite.
inline EditorScaleView editor_scale_view(int scale, float span) {
    if (!(span > 0.0f) || span > 1.0e6f) span = 240.0f;
    switch (scale) {
    case 0:  return {span * 0.04f + 4.0f, -0.45f};
    case 2:  return {span * 0.30f + 16.0f, -1.05f};
    default: return {span * 0.12f + 8.0f, -0.70f};
    }
}

} // namespace rune::editor
