#pragma once
// Etappe 27a: kontextabhängiger Inspektor (rechte Spalte des Hauptfensters).
// Reine Logik ohne Win32: Aus Karte und 3D-Auswahl wird bestimmt, welcher
// Arbeitskontext gilt und welche Überschrift/Hinweise der Inspektor zeigt.
// Events und NPCs werden NICHT doppelt bearbeitet - der Inspektor verweist
// auf die vorhandenen Dialoge F5 (Events/NPCs) und F6 (Befehle).

#include "editor/json.h"

#include <cstddef>
#include <string>

namespace rune::editor {

enum class InspectorContext { Map, Object, Invalid };

struct InspectorSummary {
    InspectorContext context = InspectorContext::Map;
    std::string title;    // UTF-8, Gruppenüberschrift
    std::string overview; // UTF-8, kurze Kartenübersicht
};

inline std::size_t inspector_array_count(const JsonValue& map, const char* key) {
    const JsonValue* v = map.is_obj() ? map.find(key) : nullptr;
    return v && v->is_arr() ? v->arr.size() : 0;
}

inline InspectorContext inspector_context_for(const JsonValue* objects, int selected) {
    if (!objects || !objects->is_arr() || selected < 0 ||
        static_cast<std::size_t>(selected) >= objects->arr.size())
        return InspectorContext::Map;
    return objects->arr[static_cast<std::size_t>(selected)].is_obj() ? InspectorContext::Object
                                                                     : InspectorContext::Invalid;
}

// map_name/object_name sind UTF-8; leere Namen bekommen einen Ersatztext.
inline InspectorSummary inspector_summary(const JsonValue& map, const JsonValue* objects,
                                          int selected, const std::string& map_name,
                                          const std::string& object_name) {
    InspectorSummary out;
    out.context = inspector_context_for(objects, selected);
    const std::size_t n_obj = objects && objects->is_arr() ? objects->arr.size() : 0;
    const std::size_t n_ev = inspector_array_count(map, "events");
    const std::size_t n_npc = inspector_array_count(map, "npcs");
    out.overview = std::to_string(n_obj) + " Objekte · " + std::to_string(n_ev) +
                   " Events · " + std::to_string(n_npc) + " NPCs";
    switch (out.context) {
    case InspectorContext::Map:
        out.title = "Karte: " + (map_name.empty() ? std::string("(ohne Namen)") : map_name);
        break;
    case InspectorContext::Object:
        out.title = "Objekt " + std::to_string(selected + 1) + ": " +
                    (object_name.empty() ? std::string("(ohne Namen)") : object_name);
        break;
    case InspectorContext::Invalid:
        out.title = "Objekt " + std::to_string(selected + 1) + ": ungültiger Eintrag";
        break;
    }
    return out;
}

// Hinweiszeile für die Kartensicht (nichts ausgewählt): Übersicht plus Wege
// zu den bestehenden Dialogen statt eigener Doppel-Bearbeitung.
inline std::string inspector_map_hint(const InspectorSummary& s) {
    // Etappe 32: kurz halten - F5/F6 stehen als Knöpfe direkt darunter,
    // die lange Zeile wurde bei normaler Breite abgeschnitten.
    return s.overview + "\nObjekt in 3D oder Liste wählen.";
}

// Etappe 27b: Inspektor-Knöpfe sind reine Wegweiser. Sie liefern die
// vorhandene Menü-ID (F5 = Platzierung/NPCs, F6 = Event-Editor).
enum class InspectorButton { Placement, Events };

inline int inspector_button_command(InspectorButton b, int placement_cmd, int events_cmd) {
    return b == InspectorButton::Events ? events_cmd : placement_cmd;
}

} // namespace rune::editor
