#pragma once
// ============================================================================
//  Allgemeine F2-Karteneigenschaften: nur geänderte Felder übernehmen.
//  Ohne Win32, damit Encounter-Änderungen und gleichzeitig bearbeitete
//  Eigenschaften keine unberührten Kartendaten überschreiben.
// ============================================================================

#include "editor/blocked_form.h"
#include "editor/json.h"
#include "editor/terrain_preview.h"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdio>
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
    // Auch bei neu hinzukommenden Feldern darf eine während der Rückfrage
    // geänderte Formularansicht nicht unbemerkt übernommen werden.
    bool operator==(const MapPropsForm&) const = default;
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

inline std::string map_props_number_display(const JsonValue* value, double fallback) {
    if (!value || value->is_null()) return placement_num_text(fallback);
    if (!value->is_num()) return "ungültig";
    if (value->is_int && !value->integer_text().empty()) return value->integer_text();
    return std::isfinite(value->number()) ? placement_num_text(value->number()) : "ungültig";
}

inline std::string map_props_color_display(const JsonValue* value, std::uint32_t fallback) {
    double n = fallback;
    if (value && !value->is_null()) {
        if (!value->is_num()) return "ungültig";
        n = value->number();
    }
    if (!world::atmosphere_valid_color(n)) return "ungültig";
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%06X", static_cast<unsigned int>(n));
    return buf;
}

struct MapPropsCheck {
    bool ok = true;
    std::string field;
    std::string error;
};

// Vollständig gelesene Dezimalzahl; kein atof-Teilresultat, kein NaN/Inf.
inline bool map_props_number(std::string s, double& out) {
    if (s.empty()) return false;
    std::replace(s.begin(), s.end(), ',', '.');
    for (const unsigned char c : s) {
        if ((c < '0' || c > '9') && c != '.' && c != '+' && c != '-' &&
            c != 'e' && c != 'E' && c != ' ' && c != '\t') return false;
    }
    errno = 0;
    char* end = nullptr;
    const double n = std::strtod(s.c_str(), &end);
    if (end == s.c_str() || errno == ERANGE || !std::isfinite(n)) return false;
    while (*end == ' ' || *end == '\t') ++end;
    if (*end) return false;
    out = n;
    return true;
}

// Nur nach map_props_check verwenden; kein abweichender atof-Teilparse.
inline double map_props_value(const std::string& text) {
    double value = 0.0;
    (void)map_props_number(text, value);
    return value;
}

inline bool map_props_hex_valid(const std::string& s) {
    std::string t = s;
    if (t.rfind("0x", 0) == 0 || t.rfind("0X", 0) == 0) t = t.substr(2);
    if (t.size() != 6) return false;
    for (char c : t)
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
              (c >= 'A' && c <= 'F'))) return false;
    return true;
}

// Nur geänderte Felder prüfen: Auch ungültige, riesige oder unbekannte
// Altwerte dürfen bei Änderungen an anderen Feldern nicht ersetzt werden.
inline MapPropsCheck map_props_check(const JsonValue& root, const MapPropsForm& before,
                                     const MapPropsForm& after) {
    MapPropsCheck check;
    auto fail = [&check](const char* field, const char* message) {
        check.ok = false; check.field = field; check.error = message; return false;
    };
    auto member = [&](const char* parent, const char* key, const char* field) {
        if (!root.is_obj() || !preview_unique(root, parent))
            return fail(field, "Mehrfach vorhandene oder ungültige JSON-Felder können hier nicht bearbeitet werden.");
        const JsonValue* p = root.find(parent);
        if (p && !p->is_null() && !p->is_obj())
            return fail(field, "Das vorhandene Kartenfeld ist kein Objekt; bitte extern prüfen.");
        if (p && p->is_obj() && !preview_unique(*p, key))
            return fail(field, "Mehrfach vorhandene JSON-Schlüssel können hier nicht bearbeitet werden.");
        return true;
    };
    auto number = [&](const char* parent, const char* key, const char* field,
                      const std::string& text, double low, double high, bool integer) {
        if (!member(parent, key, field)) return false;
        double value;
        if (!map_props_number(text, value) || !world::terrain_between(value, low, high) ||
            (integer && std::floor(value) != value)) {
            check.ok = false;
            check.field = field;
            check.error = std::string("Bitte eine endliche ") + (integer ? "ganze " : "") +
                          "Zahl zwischen " + placement_num_text(low) + " und " + placement_num_text(high) + " eingeben.";
            return false;
        }
        return true;
    };
    if (before.name != after.name && (!root.is_obj() || !preview_unique(root, "name")))
        fail("name", "Der Kartenname ist mehrfach vorhanden und kann hier nicht bearbeitet werden.");
    if (!check.ok) return check;
    if (before.width != after.width && !number("terrain", "width", "width", after.width,
                                                world::TERRAIN_MIN_SIZE, world::TERRAIN_MAX_SIZE, false)) return check;
    if (before.height != after.height && !number("terrain", "height", "height", after.height,
                                                  world::TERRAIN_MIN_SIZE, world::TERRAIN_MAX_SIZE, false)) return check;
    if (before.seed != after.seed && !number("terrain", "seed", "seed", after.seed, 0, 4294967295.0, true)) return check;
    if (before.amplitude != after.amplitude || before.mountains != after.mountains) {
        if (after.mountains) {
            if (!number("terrain", "amplitude", "amplitude", after.amplitude,
                        -world::TERRAIN_MAX_AMPLITUDE, world::TERRAIN_MAX_AMPLITUDE, false)) return check;
        } else if (!member("terrain", "amplitude", "amplitude")) return check;
    }
    if (before.fog_on != after.fog_on && !member("fog", "on", "fog_on")) return check;
    if (before.fog_far != after.fog_far && !number("fog", "far", "fog_far", after.fog_far,
                                                    world::FOG_MIN_FAR, world::FOG_MAX_FAR, false)) return check;
    if (before.fog_color != after.fog_color &&
        (!member("fog", "color", "fog_color") ||
         (!map_props_hex_valid(after.fog_color) && !fail("fog_color", "Sechs hexadezimale Farbzeichen eingeben.")))) return check;
    if (before.panorama_color != after.panorama_color &&
        (!member("panorama", "color", "panorama_color") ||
         (!map_props_hex_valid(after.panorama_color) && !fail("panorama_color", "Sechs hexadezimale Farbzeichen eingeben.")))) return check;
    if (before.weather_type != after.weather_type && !member("weather", "type", "weather_type")) return check;
    if (before.weather_intensity != after.weather_intensity) {
        if (!member("weather", "intensity", "weather_intensity")) return check;
        double intensity;
        if (!map_props_number(after.weather_intensity, intensity) || !world::atmosphere_valid_weather(intensity)) {
            fail("weather_intensity", "Die Wetterintensität muss größer als 0 und höchstens 10 sein.");
            return check;
        }
    }
    if (before.note != after.note && (!root.is_obj() || !preview_unique(root, "note")))
        fail("note", "Die Kartennotiz ist mehrfach vorhanden und kann hier nicht bearbeitet werden.");
    return check;
}

// Nur nach erfolgreicher Encounter-/Block-Zonen-Validierung und Rückfragen
// aufrufen. Bei ungültigen Block-Zonen wird NICHTS an der Karte geändert.
inline MapPropsChanges map_props_apply(JsonValue& root, const MapPropsForm& before,
                                       const MapPropsForm& after, const BlockedEdit& blocked) {
    MapPropsChanges changes;
    if (!blocked.ok || !map_props_check(root, before, after).ok) return changes;
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
            terrain.set("width", JsonValue::make_num(map_props_value(after.width), false));
        if (before.height != after.height)
            terrain.set("height", JsonValue::make_num(map_props_value(after.height), false));
        if (before.seed != after.seed)
            terrain.set("seed", JsonValue::make_num(map_props_value(after.seed), true));
        if (before.amplitude != after.amplitude || before.mountains != after.mountains) {
            terrain.set("amplitude", JsonValue::make_num(
                after.mountains ? map_props_value(after.amplitude) : 0.0, false));
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
            fog.set("far", JsonValue::make_num(map_props_value(after.fog_far), false));
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
            weather.set("intensity", JsonValue::make_num(map_props_value(after.weather_intensity), false));
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
