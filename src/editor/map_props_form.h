#pragma once
// ============================================================================
//  Allgemeine F2-Karteneigenschaften: nur geänderte Felder übernehmen.
//  Ohne Win32, damit Encounter-Änderungen und gleichzeitig bearbeitete
//  Eigenschaften keine unberührten Kartendaten überschreiben.
// ============================================================================

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

// Block-Zonen wie bisher aus den explizit bearbeiteten Textzeilen lesen.
// Leere/ungültige Zeilen werden überlesen; x/z sind die Mittelpunkte.
inline JsonValue map_props_blocked(const std::string& text) {
    JsonValue zones = JsonValue::make_arr();
    size_t pos = 0;
    while (pos <= text.size()) {
        const size_t nl = text.find_first_of("\r\n", pos);
        const std::string line = text.substr(pos, nl == std::string::npos ? nl : nl - pos);
        pos = (nl == std::string::npos) ? text.size() + 1 : nl + 1;
        std::vector<std::string> toks;
        size_t t0 = 0;
        for (size_t i = 0; i <= line.size(); ++i) {
            if (i == line.size() || line[i] == ',' || line[i] == ';') {
                toks.push_back(line.substr(t0, i - t0));
                t0 = i + 1;
            }
        }
        if (toks.size() < 4) continue;
        const double x = std::atof(toks[0].c_str());
        const double z = std::atof(toks[1].c_str());
        const double w = std::atof(toks[2].c_str());
        const double h = std::atof(toks[3].c_str());
        if (w <= 0.0 || h <= 0.0) continue;
        JsonValue zone = JsonValue::make_obj();
        zone.set("x", JsonValue::make_num(x, false));
        zone.set("z", JsonValue::make_num(z, false));
        zone.set("w", JsonValue::make_num(w, false));
        zone.set("h", JsonValue::make_num(h, false));
        zones.arr.push_back(std::move(zone));
    }
    return zones;
}

// Nur nach erfolgreicher Encounter-Validierung aufrufen. Ungeänderte Felder
// bleiben samt fehlenden/null-Werten und unbekannten Schlüsseln erhalten.
inline MapPropsChanges map_props_apply(JsonValue& root, const MapPropsForm& before,
                                       const MapPropsForm& after) {
    MapPropsChanges changes;
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

    if (before.blocked != after.blocked) {
        JsonValue zones = map_props_blocked(after.blocked);
        if (!zones.arr.empty()) root.set("blocked", std::move(zones));
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
