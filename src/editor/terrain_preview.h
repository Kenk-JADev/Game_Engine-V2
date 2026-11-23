#pragma once
// Nichtdestruktive Karteninterpretation für Terrain und Atmosphäre. Ungültige
// Altdaten bleiben im JSON; niemals unsichere Zahlen in Renderer/Casts reichen.

#include "editor/json.h"
#include "world/terrain_limits.h"

#include <cstdint>
#include <string>
#include <vector>

namespace rune::editor {

inline bool preview_unique(const JsonValue& obj, const char* key) {
    unsigned count = 0;
    for (const auto& item : obj.obj) if (item.first == key && ++count > 1) return false;
    return true;
}

inline bool preview_number(const JsonValue* obj, const char* key, double fallback, double& out) {
    out = fallback;
    if (!obj) return true;
    if (!obj->is_obj() || !preview_unique(*obj, key)) return false;
    const JsonValue* value = obj->find(key);
    if (!value || value->is_null()) return true;
    if (!value->is_num()) return false;
    out = value->number();
    return true;
}

inline bool preview_required_number(const JsonValue& obj, const char* key, double& out) {
    if (!obj.is_obj() || !preview_unique(obj, key)) return false;
    const JsonValue* v = obj.find(key);
    if (!v || !v->is_num()) return false;
    out = v->number();
    return true;
}

inline bool preview_splat(const JsonValue& entry, world::SplatRect& out) {
    if (!entry.is_obj() || !preview_unique(entry, "tex")) return false;
    const JsonValue* tex = entry.find("tex");
    double x, z, w, h;
    if (!tex || !tex->is_str() || !world::terrain_valid_texture_name(tex->str) ||
        !preview_required_number(entry, "x", x) || !preview_required_number(entry, "z", z) ||
        !preview_required_number(entry, "w", w) || !preview_required_number(entry, "h", h) ||
        !world::terrain_valid_splat(x, z, w, h)) return false;
    out.tex = tex->str;
    out.x = static_cast<float>(x); out.z = static_cast<float>(z);
    out.w = static_cast<float>(w); out.h = static_cast<float>(h);
    return true;
}

struct TerrainPreview {
    world::TerrainPlan plan;
    std::vector<math::Vec4> brushes;
    std::vector<world::SplatRect> splats;
    std::string texture = "gras";
    float tile = 2.0f;
    std::string error;
    std::vector<std::string> warnings;
    bool ok() const { return error.empty(); }
};

inline TerrainPreview terrain_preview_read(const JsonValue& root) {
    TerrainPreview result;
    auto fail = [&result](const std::string& why) { result.error = why; };
    if (!root.is_obj() || !preview_unique(root, "terrain")) {
        fail("terrain fehlt eindeutig oder ist mehrfach vorhanden"); return result;
    }
    const JsonValue* t = root.find("terrain");
    if (t && t->is_null()) t = nullptr;
    if (t && !t->is_obj()) { fail("terrain ist kein Objekt"); return result; }

    double size = 240, width, height, res, seed, amp;
    const JsonValue* w_value = t ? t->find("width") : nullptr;
    const JsonValue* h_value = t ? t->find("height") : nullptr;
    const bool needs_size = !w_value || w_value->is_null() || !h_value || h_value->is_null();
    if ((needs_size && !preview_number(t, "size", 240, size)) ||
        !preview_number(t, "width", size, width) || !preview_number(t, "height", size, height) ||
        !preview_number(t, "resolution", 192, res) || !preview_number(t, "seed", 7, seed) ||
        !preview_number(t, "amplitude", 0, amp)) {
        fail("terrain enthält ein nichtnumerisches oder mehrfaches Grundfeld"); return result;
    }
    result.plan = world::terrain_plan(width, height, res, seed, amp);
    if (!result.plan.ok()) { fail(world::terrain_problem_text(result.plan.problem)); return result; }

    if (t && !preview_unique(*t, "heights")) { fail("terrain.heights ist mehrfach vorhanden"); return result; }
    const JsonValue* heights = t ? t->find("heights") : nullptr;
    if (heights && !heights->is_null()) {
        if (!heights->is_arr() || heights->arr.size() > world::TERRAIN_MAX_BRUSHES) {
            fail(world::terrain_problem_text(world::TerrainProblem::Brushes)); return result;
        }
        result.brushes.reserve(heights->arr.size());
        for (const JsonValue& entry : heights->arr) {
            double x, z, r, h;
            if (!preview_required_number(entry, "x", x) || !preview_required_number(entry, "z", z) ||
                !preview_required_number(entry, "r", r) || !preview_required_number(entry, "h", h) ||
                !world::terrain_valid_brush(x, z, r, h)) {
                fail(world::terrain_problem_text(world::TerrainProblem::Brushes)); return result;
            }
            result.brushes.push_back(math::Vec4{static_cast<float>(x), static_cast<float>(h),
                                                static_cast<float>(z), static_cast<float>(r)});
        }
        const world::TerrainProblem problem = world::terrain_check_brushes(result.plan, result.brushes);
        if (problem != world::TerrainProblem::None) { fail(world::terrain_problem_text(problem)); return result; }
    }

    if (t && !preview_unique(*t, "splats")) { fail("terrain.splats ist mehrfach vorhanden"); return result; }
    const JsonValue* splats = t ? t->find("splats") : nullptr;
    if (splats && !splats->is_null()) {
        if (!splats->is_arr() || splats->arr.size() > world::TERRAIN_MAX_SPLATS) {
            fail(world::terrain_problem_text(world::TerrainProblem::Splats)); return result;
        }
        // Auch bei vielen gültigen Flächen nicht unbeschränkt pro Quad suchen.
        if (!splats->arr.empty() && result.plan.quads() >
            world::TERRAIN_MAX_SPLAT_CHECKS / splats->arr.size()) {
            fail(world::terrain_problem_text(world::TerrainProblem::SplatBudget)); return result;
        }
        result.splats.reserve(splats->arr.size());
        for (const JsonValue& entry : splats->arr) {
            world::SplatRect s;
            if (!preview_splat(entry, s)) {
                fail(world::terrain_problem_text(world::TerrainProblem::Splats)); return result;
            }
            result.splats.push_back(std::move(s));
        }
    }
    if (t && preview_unique(*t, "texture")) {
        const JsonValue* texture = t->find("texture");
        if (texture && texture->is_str() && world::terrain_valid_texture_name(texture->str))
            result.texture = texture->str;
        else if (texture && !texture->is_null()) result.warnings.push_back("terrain.texture ungültig; Gras als Ersatz");
    } else if (t) result.warnings.push_back("terrain.texture mehrfach; Gras als Ersatz");
    double tile;
    if (!preview_number(t, "tile", 2.0, tile) || !world::terrain_valid_tile(tile))
        result.warnings.push_back("terrain.tile ungültig; 2 m als Ersatz");
    else result.tile = static_cast<float>(tile);
    return result;
}

struct AtmospherePreview {
    std::uint32_t sky = 0x3D6FB5;
    bool fog_on = false;
    std::uint32_t fog_color = 0xBFD8EC;
    float fog_far = 180.0f;
    std::vector<std::string> warnings;
};

inline AtmospherePreview atmosphere_preview_read(const JsonValue& root) {
    AtmospherePreview result;
    auto object = [&root, &result](const char* key) -> const JsonValue* {
        if (!root.is_obj() || !preview_unique(root, key)) {
            result.warnings.push_back(std::string(key) + " mehrfach; Standard als Ersatz"); return nullptr;
        }
        const JsonValue* v = root.find(key);
        if (!v || v->is_null()) return nullptr;
        if (!v->is_obj()) {
            result.warnings.push_back(std::string(key) + " ist kein Objekt; Standard als Ersatz"); return nullptr;
        }
        return v;
    };
    const JsonValue* pan = object("panorama");
    double color;
    if (!preview_number(pan, "color", result.sky, color) || !world::atmosphere_valid_color(color))
        result.warnings.push_back("panorama.color ungültig; Standard als Ersatz");
    else result.sky = static_cast<std::uint32_t>(color);

    const JsonValue* fog = object("fog");
    if (fog && !preview_unique(*fog, "on")) result.warnings.push_back("fog.on mehrfach; Nebel aus");
    else if (fog) {
        const JsonValue* on = fog->find("on");
        if (on && !on->is_null() && on->type != JsonValue::Type::Bool)
            result.warnings.push_back("fog.on ist kein Bool; Nebel aus");
        else if (on && on->type == JsonValue::Type::Bool) result.fog_on = on->b;
    }
    if (!preview_number(fog, "color", result.fog_color, color) || !world::atmosphere_valid_color(color))
        result.warnings.push_back("fog.color ungültig; Standard als Ersatz");
    else result.fog_color = static_cast<std::uint32_t>(color);
    double distance;
    if (!preview_number(fog, "far", 180.0, distance) || !world::atmosphere_valid_fog_far(distance))
        result.warnings.push_back("fog.far ungültig; 180 m als Ersatz");
    else result.fog_far = static_cast<float>(distance);

    const JsonValue* weather = object("weather");
    double intensity;
    if (!preview_number(weather, "intensity", 1.0, intensity) || !world::atmosphere_valid_weather(intensity))
        result.warnings.push_back("weather.intensity ungültig; 1 als Ersatz im Spiel");
    return result;
}

} // namespace rune::editor
