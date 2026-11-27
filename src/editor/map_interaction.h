#pragma once
// Kartenmarkierungen und Projektstart ohne Win32; Lesen verändert keine Daten.
#include "editor/json.h"
#include "editor/json_validation.h"

#include <cmath>
#include <cstddef>
#include <exception>
#include <string>
#include <utility>
#include <vector>

namespace rune::editor {

enum class MapMarkerKind { None, Event, Npc };

struct MapMarker {
    MapMarkerKind kind = MapMarkerKind::None;
    std::size_t index = 0;
    double x = 0, z = 0, y = 0, rotation = 0;
    bool sign = false;
};

inline bool map_field_unique(const JsonValue& object, const char* key) {
    if (!object.is_obj()) return false;
    unsigned count = 0;
    for (const auto& pair : object.obj) if (pair.first == key && ++count > 1) return false;
    return true;
}

// Nicht darstellbare bzw. mehrdeutige Positionen nicht auf (0,0) erfinden.
inline bool map_marker_number(const JsonValue& item, const char* key, double& out, bool required) {
    if (!map_field_unique(item, key)) return false;
    const JsonValue* value = item.find(key);
    if (!value) { out = 0; return !required; }
    if (!value->is_num() || !std::isfinite(value->number()) || std::fabs(value->number()) > 1000000.0)
        return false;
    out = value->number();
    return true;
}

inline bool map_marker_read(const JsonValue& map, MapMarkerKind kind, std::size_t index, MapMarker& out) {
    const char* key = kind == MapMarkerKind::Event ? "events" : kind == MapMarkerKind::Npc ? "npcs" : nullptr;
    if (!key || !map_field_unique(map, key)) return false;
    const JsonValue* array = map.find(key);
    if (!array || !array->is_arr() || index >= array->arr.size()) return false;
    const JsonValue& item = array->arr[index];
    if (!item.is_obj()) return false;
    MapMarker marker;
    marker.kind = kind;
    marker.index = index;
    if (!map_marker_number(item, "x", marker.x, true) ||
        !map_marker_number(item, "z", marker.z, true) ||
        !map_marker_number(item, "y", marker.y, false) ||
        !map_marker_number(item, "rot", marker.rotation, false) ||
        !map_field_unique(item, "visual")) return false;
    const JsonValue* visual = item.find("visual");
    marker.sign = kind == MapMarkerKind::Event && visual && visual->is_str() && visual->str == "schild";
    out = marker;
    return true;
}

inline std::vector<MapMarker> map_markers(const JsonValue& map) {
    std::vector<MapMarker> result;
    for (const auto kind : {MapMarkerKind::Npc, MapMarkerKind::Event}) {
        const char* key = kind == MapMarkerKind::Npc ? "npcs" : "events";
        if (!map_field_unique(map, key)) continue;
        const JsonValue* array = map.find(key);
        if (!array || !array->is_arr()) continue;
        for (std::size_t i = 0; i < array->arr.size(); ++i) {
            MapMarker marker;
            if (map_marker_read(map, kind, i, marker)) result.push_back(marker);
        }
    }
    return result;
}

inline const JsonValue* map_marker_item(const JsonValue& map, MapMarkerKind kind, std::size_t index) {
    MapMarker checked;
    if (!map_marker_read(map, kind, index, checked)) return nullptr;
    const JsonValue* array = map.find(kind == MapMarkerKind::Npc ? "npcs" : "events");
    return &array->arr[index];
}

// Etappe 34: Marker (Event/NPC) verschieben. Nur x/z werden ersetzt; alle
// anderen Felder (Seiten, Befehle, Pfad, fremde Erweiterungen) bleiben.
// Abgelehnt werden mehrdeutige/ungültige Einträge und nicht darstellbare
// Zielwerte. Rückgabe: true = geändert.
inline bool map_marker_move(JsonValue& map, MapMarkerKind kind, std::size_t index, double x, double z) {
    MapMarker checked;
    if (!map_marker_read(map, kind, index, checked)) return false;
    if (!std::isfinite(x) || !std::isfinite(z) || std::fabs(x) > 1000000.0 || std::fabs(z) > 1000000.0)
        return false;
    if (checked.x == x && checked.z == z) return false;
    JsonValue* array = map.find(kind == MapMarkerKind::Npc ? "npcs" : "events");
    JsonValue& item = array->arr[index];
    item.set("x", JsonValue::make_num(x, false));
    item.set("z", JsonValue::make_num(z, false));
    return true;
}

// Der zweite Klick darf kein anderes, inzwischen ersetztes Element öffnen.
struct MapMarkerSelection {
    MapMarkerKind kind = MapMarkerKind::None;
    std::size_t index = 0;
    std::uint64_t map_epoch = 0;
    std::string snapshot;
};

inline MapMarkerSelection map_marker_select(const JsonValue& map, MapMarkerKind kind,
                                            std::size_t index, std::uint64_t epoch) {
    const JsonValue* item = map_marker_item(map, kind, index);
    if (!item) return {};
    return {kind, index, epoch, json_write(*item)};
}

inline bool map_marker_same(const MapMarkerSelection& previous, const JsonValue& map,
                            MapMarkerKind kind, std::size_t index, std::uint64_t epoch) {
    if (kind == MapMarkerKind::None || previous.kind != kind || previous.index != index ||
        previous.map_epoch != epoch) return false;
    const JsonValue* item = map_marker_item(map, kind, index);
    return item && previous.snapshot == json_write(*item);
}

// Das gesetzte Raster wird auf die tatsächlichen Kartenmaße begrenzt; ein
// Treffer außerhalb der Karte oder ein nicht endlicher Wert ist kein Start.
inline bool map_start_point(double x, double z, double grid, double width, double height,
                            double& snapped_x, double& snapped_z) {
    if (!std::isfinite(x) || !std::isfinite(z) || !std::isfinite(grid) || grid <= 0 ||
        !std::isfinite(width) || !std::isfinite(height) || width <= 0 || height <= 0 ||
        std::fabs(x) > width * 0.5 || std::fabs(z) > height * 0.5) return false;
    snapped_x = std::round(x / grid) * grid;
    snapped_z = std::round(z / grid) * grid;
    // An der Außenkante darf Rundung nicht außerhalb des Spielfelds landen.
    if (!std::isfinite(snapped_x) || !std::isfinite(snapped_z)) return false;
    if (std::fabs(snapped_x) > width * 0.5) snapped_x = std::copysign(std::floor(width * 0.5 / grid) * grid, x);
    if (std::fabs(snapped_z) > height * 0.5) snapped_z = std::copysign(std::floor(height * 0.5 / grid) * grid, z);
    return std::fabs(snapped_x) <= 1000000.0 && std::fabs(snapped_z) <= 1000000.0;
}

struct ProjectStartEdit {
    bool ok = false;
    bool changed = false;
    JsonValue value;
    std::string error;
};

inline bool project_start_parse(const std::string& bytes, JsonValue& out) {
    if (!editor_json_raw_strings_valid(bytes)) return false;
    try {
        JsonValue value = json_parse(bytes);
        if (!value.is_obj() || !editor_json_utf8(value) || !editor_json_serializable(value)) return false;
        out = std::move(value);
        return true;
    } catch (const std::exception&) { return false; }
}

// Patch nur auf einer Kopie. Alte Felder und fremde Erweiterungen bleiben
// erhalten; mehrdeutige bzw. beschädigte Startfelder werden nicht ersetzt.
inline ProjectStartEdit project_start_prepare(const JsonValue& system, const std::string& map_id,
                                               double x, double z) {
    ProjectStartEdit result;
    if (!system.is_obj() || !editor_json_utf8(system) || !editor_json_serializable(system) ||
        !map_field_unique(system, "start") || map_id.empty() || !editor_valid_utf8(map_id) ||
        map_id.find('\0') != std::string::npos || !std::isfinite(x) || !std::isfinite(z) ||
        std::fabs(x) > 1000000.0 || std::fabs(z) > 1000000.0) {
        result.error = "system.json oder Startposition ist ungültig bzw. mehrdeutig.";
        return result;
    }
    const JsonValue* old_start = system.find("start");
    if (old_start && !old_start->is_obj()) {
        result.error = "start ist kein Objekt; die vorhandenen Daten bleiben erhalten.";
        return result;
    }
    if (old_start) {
        for (const char* key : {"map", "x", "z", "yaw", "use_project_position"}) {
            if (!map_field_unique(*old_start, key)) {
                result.error = "Ein Startfeld ist mehrfach vorhanden; bitte system.json extern prüfen.";
                return result;
            }
        }
        const JsonValue* yaw = old_start->find("yaw");
        if (yaw && (!yaw->is_num() || !std::isfinite(yaw->number()))) {
            result.error = "Die vorhandene Startrichtung ist ungültig und wird nicht ersetzt.";
            return result;
        }
    }
    result.value = system;
    JsonValue start = old_start ? *old_start : JsonValue::make_obj();
    start.set("map", JsonValue::make_str(map_id));
    start.set("x", JsonValue::make_num(x, false));
    start.set("z", JsonValue::make_num(z, false));
    if (!start.find("yaw")) start.set("yaw", JsonValue::make_num(3.141592653589793, false));
    start.set("use_project_position", JsonValue::make_bool(true));
    result.value.set("start", std::move(start));
    result.ok = true;
    result.changed = json_write(result.value) != json_write(system);
    return result;
}

inline bool project_start_marker(const JsonValue& system, const std::string& map_id, double& x, double& z) {
    if (!map_field_unique(system, "start")) return false;
    const JsonValue* start = system.find("start");
    if (!start || !start->is_obj() || !map_field_unique(*start, "map") ||
        !map_field_unique(*start, "x") || !map_field_unique(*start, "z") ||
        !map_field_unique(*start, "use_project_position")) return false;
    const JsonValue* map = start->find("map");
    const JsonValue* flag = start->find("use_project_position");
    if (!map || !map->is_str() || map->str != map_id || !flag || flag->type != JsonValue::Type::Bool || !flag->b)
        return false;
    return map_marker_number(*start, "x", x, true) && map_marker_number(*start, "z", z, true);
}

} // namespace rune::editor
