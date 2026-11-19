#pragma once
// ============================================================================
//  Allgemeine F2-Karteneigenschaften: nur geänderte Felder übernehmen.
//  Ohne Win32, damit Encounter-Änderungen und gleichzeitig bearbeitete
//  Eigenschaften keine unberührten Kartendaten überschreiben.
// ============================================================================

#include "editor/blocked_form.h"
#include "editor/json.h"

#include <cstdint>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

namespace rune::editor {

struct MapPropsForm {
    std::string name, width, height, seed, amplitude;
    std::string fog_far, fog_color, panorama_color, weather_intensity;
    std::string note, blocked;
    bool mountains = false;
    bool fog_on = false;
    int weather_type = 0;
};

struct MapPropsChanges {
    bool changed = false;
    bool name = false;
    bool terrain = false;
    bool atmosphere = false;
};

// Hexfarbe "BFD8EC" oder "0xBFD8EC" -> 0x00BFD8EC (0 bei Unsinn).
inline std::uint32_t parse_hex_color(const std::string& s) {
    std::string t = s;
    if (t.rfind("0x", 0) == 0 || t.rfind("0X", 0) == 0) t = t.substr(2);
    if (t.size() != 6) return 0;
    std::uint32_t v = 0;
    for (char c : t) {
        v <<= 4;
        if (c >= '0' && c <= '9') v |= static_cast<std::uint32_t>(c - '0');
        else if (c >= 'a' && c <= 'f') v |= static_cast<std::uint32_t>(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') v |= static_cast<std::uint32_t>(c - 'A' + 10);
        else return 0;
    }
    return v;
}

// Nur nach erfolgreicher Encounter-/Block-Zonen-Validierung und Rückfragen
// aufrufen. Bei ungültigen Block-Zonen wird NICHTS an der Karte geändert.
inline MapPropsChanges map_props_apply(JsonValue& root, const MapPropsForm& before,
                                       const MapPropsForm& after, const BlockedEdit& blocked) {
    MapPropsChanges changes;
    if (!blocked.ok) return changes;
    auto object_copy = [&root](const char* key) {
        const JsonValue* v = root.find(key);
        return v && v->is_obj() ? *v : JsonValue::make_obj();
    };

    if (before.name != after.name) {
        root.set("name", JsonValue::make_str(after.name));
        changes.name = changes.changed = true;
    }

    if (before.width != after.width || before.height != after.height ||
        before.seed != after.seed || before.amplitude != after.amplitude ||
        before.mountains != after.mountains) {
        JsonValue terrain = object_copy("terrain");
        if (before.width != after.width)
            terrain.set("width", JsonValue::make_num(std::atof(after.width.c_str()), false));
        if (before.height != after.height)
            terrain.set("height", JsonValue::make_num(std::atof(after.height.c_str()), false));
        if (before.seed != after.seed)
            terrain.set("seed", JsonValue::make_num(std::atof(after.seed.c_str()), true));
        if (before.amplitude != after.amplitude || before.mountains != after.mountains) {
            terrain.set("amplitude", JsonValue::make_num(
                after.mountains ? std::atof(after.amplitude.c_str()) : 0.0, false));
        }
        root.set("terrain", std::move(terrain));
        changes.terrain = changes.changed = true;
    }

    if (before.fog_on != after.fog_on || before.fog_far != after.fog_far ||
        before.fog_color != after.fog_color) {
        JsonValue fog = object_copy("fog");
        if (before.fog_on != after.fog_on)
            fog.set("on", JsonValue::make_bool(after.fog_on));
        if (before.fog_far != after.fog_far)
            fog.set("far", JsonValue::make_num(std::atof(after.fog_far.c_str()), false));
        if (before.fog_color != after.fog_color)
            fog.set("color", JsonValue::make_num(static_cast<double>(parse_hex_color(after.fog_color)), true));
        root.set("fog", std::move(fog));
        changes.atmosphere = changes.changed = true;
    }

    if (before.panorama_color != after.panorama_color) {
        JsonValue panorama = object_copy("panorama");
        panorama.set("color", JsonValue::make_num(
            static_cast<double>(parse_hex_color(after.panorama_color)), true));
        root.set("panorama", std::move(panorama));
        changes.atmosphere = changes.changed = true;
    }

    if (before.weather_type != after.weather_type ||
        before.weather_intensity != after.weather_intensity) {
        JsonValue weather = object_copy("weather");
        if (before.weather_type != after.weather_type) {
            const char* type = after.weather_type == 1 ? "regen"
                             : after.weather_type == 2 ? "schnee" : "kein";
            weather.set("type", JsonValue::make_str(type));
        }
        if (before.weather_intensity != after.weather_intensity) {
            double intensity = std::atof(after.weather_intensity.c_str());
            if (intensity <= 0.0) intensity = 1.0;
            weather.set("intensity", JsonValue::make_num(intensity, false));
        }
        root.set("weather", std::move(weather));
        changes.changed = true;
    }

    if (before.note != after.note) {
        root.set("note", JsonValue::make_str(after.note));
        changes.changed = true;
    }

    if (blocked.changed) {
        if (!blocked.value.arr.empty()) root.set("blocked", blocked.value);
        else {
            for (auto it = root.obj.begin(); it != root.obj.end(); ++it) {
                if (it->first == "blocked") { root.obj.erase(it); break; }
            }
        }
        changes.changed = true;
    }
    return changes;
}

} // namespace rune::editor
