#pragma once
// Etappe 38: reine Logik für die Event-/NPC-Liste im Inspektor.
// Die Liste zeigt Zähler und_position_ und springt bei Klick zur Markierung.
// Kein Win32: daher kopflos testbar und unabhängig von der 3D-Ansicht.
#include "editor/json.h"
#include "editor/map_interaction.h"
#include "editor/number_format.h"

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace rune::editor {

enum class EventNpcKind { Event, Npc };

struct EventNpcRow {
    EventNpcKind kind = EventNpcKind::Event;
    std::size_t index = 0;
    std::string art;
    std::string name;
    std::string x;
    std::string z;
};

struct EventNpcCounts {
    std::size_t events = 0;
    std::size_t npcs = 0;
};

// Die Zähler zählen alle Einträge, auch solche ohne gültige Position. Die
// Liste selbst zeigt nur darstellbare Marker (map_markers filtert sie).
inline EventNpcCounts event_npc_counts(const JsonValue& map) {
    EventNpcCounts out;
    if (!map.is_obj()) return out;
    if (map_field_unique(map, "events")) {
        if (const JsonValue* list = map.find("events"); list && list->is_arr())
            out.events = list->arr.size();
    }
    if (map_field_unique(map, "npcs")) {
        if (const JsonValue* list = map.find("npcs"); list && list->is_arr())
            out.npcs = list->arr.size();
    }
    return out;
}

inline std::string event_npc_name(const JsonValue& item, const std::string& fallback) {
    if (item.is_obj() && map_field_unique(item, "name")) {
        const JsonValue* value = item.find("name");
        if (value && value->is_str() && !value->str.empty()) return value->str;
    }
    return fallback;
}

inline std::string event_npc_summary(const EventNpcCounts& counts) {
    return "Events: " + std::to_string(counts.events) +
           "  |  NPCs: " + std::to_string(counts.npcs);
}

inline std::vector<EventNpcRow> event_npc_rows(const JsonValue& map) {
    std::vector<EventNpcRow> rows;
    for (const MapMarker& marker : map_markers(map)) {
        EventNpcRow row;
        row.kind = marker.kind == MapMarkerKind::Npc ? EventNpcKind::Npc : EventNpcKind::Event;
        row.index = marker.index;
        row.art = marker.kind == MapMarkerKind::Npc ? "NPC" : (marker.sign ? "Schild" : "Event");
        const std::string fallback = row.art + " #" + std::to_string(row.index + 1);
        if (const JsonValue* item = map_marker_item(map, marker.kind, marker.index))
            row.name = event_npc_name(*item, fallback);
        else
            row.name = fallback;
        row.x = editor_number_text(marker.x, 1);
        row.z = editor_number_text(marker.z, 1);
        rows.push_back(row);
    }
    return rows;
}

// Events sind negativ, NPCs positiv. 0 ist „keine Auswahl“. Damit bleibt die
// alte Objektliste (reine Indizes >= 0) eindeutig von Markern getrennt.
inline std::intptr_t event_npc_lparam(EventNpcKind kind, std::size_t index) {
    const std::int64_t value = static_cast<std::int64_t>(index) + 1;
    return static_cast<std::intptr_t>(kind == EventNpcKind::Event ? -value : value);
}

inline bool event_npc_decode(std::intptr_t raw, EventNpcKind& kind, std::size_t& index) {
    if (raw == 0) return false;
    const std::int64_t value = static_cast<std::int64_t>(raw);
    if (value < 0) {
        if (value == (std::numeric_limits<std::int64_t>::min)()) return false;
        kind = EventNpcKind::Event;
        index = static_cast<std::size_t>(-value - 1);
    } else {
        kind = EventNpcKind::Npc;
        index = static_cast<std::size_t>(value - 1);
    }
    return true;
}

} // namespace rune::editor
