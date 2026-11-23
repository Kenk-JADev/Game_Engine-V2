#pragma once
// Grenzen für Gelände und Atmosphäre in Editor und Engine. Diese Prüfungen
// betreffen nur Darstellung/Simulation, niemals die gespeicherten Kartendaten.

#include "math/math.h"
#include "world/atlas.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace rune::world {

inline constexpr double TERRAIN_MIN_SIZE = 1.0;
inline constexpr double TERRAIN_MAX_SIZE = 10000.0;
inline constexpr int TERRAIN_MIN_RES = 8;
inline constexpr int TERRAIN_MAX_RES = 1024;
inline constexpr std::size_t TERRAIN_MAX_QUADS = 262144; // auch für 4 Vertices pro Splat-Quad
inline constexpr double TERRAIN_MAX_AMPLITUDE = 10000.0;
inline constexpr double TERRAIN_MAX_COORD = 1000000.0;
inline constexpr double TERRAIN_MIN_RADIUS = 0.001;
inline constexpr std::size_t TERRAIN_MAX_BRUSHES = 2048;
inline constexpr std::size_t TERRAIN_MAX_BRUSH_SAMPLES = 4000000;
inline constexpr std::size_t TERRAIN_MAX_SPLATS = 2048;
inline constexpr std::size_t TERRAIN_MAX_SPLAT_CHECKS = 8000000;
inline constexpr std::size_t TERRAIN_MAX_TEXTURE_BYTES = 1024;
inline constexpr double TERRAIN_MIN_TILE = 0.01;
inline constexpr double TERRAIN_MAX_TILE = 10000.0;
inline constexpr double FOG_MIN_FAR = 0.1;
inline constexpr double FOG_MAX_FAR = 100000.0;
inline constexpr double WEATHER_MAX_INTENSITY = 10.0;

inline bool terrain_between(double value, double low, double high) {
    return std::isfinite(value) && value >= low && value <= high;
}

enum class TerrainProblem {
    None, Size, Resolution, Aspect, GridBudget, Seed, Amplitude,
    Brushes, BrushBudget, Splats, SplatBudget
};

inline const char* terrain_problem_text(TerrainProblem problem) {
    switch (problem) {
        case TerrainProblem::Size: return "Breite/Höhe müssen zwischen 1 und 10000 liegen";
        case TerrainProblem::Resolution: return "Auflösung muss zwischen 8 und 1024 liegen";
        case TerrainProblem::Aspect: return "Seitenverhältnis ergibt mehr als 1024 Zellen in Z";
        case TerrainProblem::GridBudget: return "Geländeraster überschreitet 262144 Quads";
        case TerrainProblem::Seed: return "Seed muss zwischen 0 und 4294967295 liegen";
        case TerrainProblem::Amplitude: return "Amplitude muss zwischen -10000 und 10000 liegen";
        case TerrainProblem::Brushes: return "Höhenpinsel ungültig oder mehr als 2048 Einträge";
        case TerrainProblem::BrushBudget: return "Höhenpinsel überschreiten das Rechenbudget";
        case TerrainProblem::Splats: return "Bodenflächen ungültig oder mehr als 2048 Einträge";
        case TerrainProblem::SplatBudget: return "Bodenflächen überschreiten das Rechenbudget";
        case TerrainProblem::None: return "";
    }
    return "ungültiges Gelände";
}

struct TerrainPlan {
    // Ein noch nicht berechneter Plan darf niemals als gültiges Raster gelten.
    TerrainProblem problem = TerrainProblem::Size;
    float width = 0.0f, height = 0.0f, amplitude = 0.0f;
    int res_x = 0, res_z = 0;
    std::uint32_t seed = 0;
    bool ok() const { return problem == TerrainProblem::None; }
    std::size_t quads() const {
        return static_cast<std::size_t>(res_x) * static_cast<std::size_t>(res_z);
    }
};

// Vor jeder float-/Integerkonvertierung prüfen. Die bisherige Abschneidung
// gebrochener Auflösungen/Seeds bleibt innerhalb des gültigen Bereichs erhalten.
inline TerrainPlan terrain_plan(double width, double height, double resolution,
                                double seed, double amplitude) {
    TerrainPlan p;
    if (!terrain_between(width, TERRAIN_MIN_SIZE, TERRAIN_MAX_SIZE) ||
        !terrain_between(height, TERRAIN_MIN_SIZE, TERRAIN_MAX_SIZE)) {
        p.problem = TerrainProblem::Size; return p;
    }
    if (!terrain_between(resolution, TERRAIN_MIN_RES, TERRAIN_MAX_RES)) {
        p.problem = TerrainProblem::Resolution; return p;
    }
    if (!terrain_between(seed, 0.0, 4294967295.0)) {
        p.problem = TerrainProblem::Seed; return p;
    }
    if (!terrain_between(amplitude, -TERRAIN_MAX_AMPLITUDE, TERRAIN_MAX_AMPLITUDE)) {
        p.problem = TerrainProblem::Amplitude; return p;
    }
    p.width = static_cast<float>(width);
    p.height = static_cast<float>(height);
    p.res_x = static_cast<int>(resolution);
    p.seed = static_cast<std::uint32_t>(seed);
    p.amplitude = static_cast<float>(amplitude);
    // Wie bisher quadratische Zellen anstreben. Vor lround den Wertebereich
    // kontrollieren; die float-Rechnung entspricht dem bisherigen Terrain.
    const float z = static_cast<float>(p.res_x) * p.height / std::max(1.0f, p.width);
    if (!std::isfinite(z) || z >= static_cast<float>(TERRAIN_MAX_RES) + 0.5f) {
        p.problem = TerrainProblem::Aspect; return p;
    }
    p.res_z = std::max(TERRAIN_MIN_RES, static_cast<int>(std::lround(z)));
    p.problem = p.quads() > TERRAIN_MAX_QUADS ? TerrainProblem::GridBudget : TerrainProblem::None;
    return p;
}

inline bool terrain_valid_brush(double x, double z, double radius, double height) {
    return terrain_between(x, -TERRAIN_MAX_COORD, TERRAIN_MAX_COORD) &&
           terrain_between(z, -TERRAIN_MAX_COORD, TERRAIN_MAX_COORD) &&
           terrain_between(radius, TERRAIN_MIN_RADIUS, TERRAIN_MAX_COORD) &&
           terrain_between(height, -TERRAIN_MAX_AMPLITUDE, TERRAIN_MAX_AMPLITUDE);
}

struct TerrainBrushBounds { int x0 = 0, x1 = -1, z0 = 0, z1 = -1; };

// Weltpositionen zuerst in double schneiden, DANN nach int konvertieren.
// Für vollständig außerhalb liegende Pinsel bleibt der Bereich leer.
inline TerrainBrushBounds terrain_brush_bounds(const TerrainPlan& p, const math::Vec4& b) {
    TerrainBrushBounds result;
    const double hw = static_cast<double>(p.width) * 0.5;
    const double hh = static_cast<double>(p.height) * 0.5;
    const double x = b.x, z = b.z, r = b.w;
    if (x - r > hw || x + r < -hw || z - r > hh || z + r < -hh) return result;
    const auto low = [](double v, int n) {
        return static_cast<int>(std::floor(std::clamp(v, 0.0, static_cast<double>(n))));
    };
    const auto high = [](double v, int n) {
        return static_cast<int>(std::ceil(std::clamp(v, 0.0, static_cast<double>(n))));
    };
    result.x0 = low((x - r + hw) * p.res_x / p.width, p.res_x);
    result.x1 = high((x + r + hw) * p.res_x / p.width, p.res_x);
    result.z0 = low((z - r + hh) * p.res_z / p.height, p.res_z);
    result.z1 = high((z + r + hh) * p.res_z / p.height, p.res_z);
    return result;
}

inline TerrainProblem terrain_check_brushes(const TerrainPlan& p, const std::vector<math::Vec4>& brushes) {
    if (!p.ok()) return p.problem;
    if (brushes.size() > TERRAIN_MAX_BRUSHES) return TerrainProblem::Brushes;
    std::size_t samples = 0;
    for (const math::Vec4& b : brushes) {
        if (!terrain_valid_brush(b.x, b.z, b.w, b.y)) return TerrainProblem::Brushes;
        if (b.y == 0.0f) continue;
        const TerrainBrushBounds r = terrain_brush_bounds(p, b);
        if (r.x1 < r.x0 || r.z1 < r.z0) continue;
        samples += static_cast<std::size_t>(r.x1 - r.x0 + 1) *
                   static_cast<std::size_t>(r.z1 - r.z0 + 1);
        if (samples > TERRAIN_MAX_BRUSH_SAMPLES) return TerrainProblem::BrushBudget;
    }
    return TerrainProblem::None;
}

inline bool terrain_valid_splat(double x, double z, double width, double height) {
    return terrain_between(x, -TERRAIN_MAX_COORD, TERRAIN_MAX_COORD) &&
           terrain_between(z, -TERRAIN_MAX_COORD, TERRAIN_MAX_COORD) &&
           terrain_between(width, TERRAIN_MIN_RADIUS, TERRAIN_MAX_COORD) &&
           terrain_between(height, TERRAIN_MIN_RADIUS, TERRAIN_MAX_COORD);
}

inline bool terrain_valid_texture_name(const std::string& name) {
    return !name.empty() && name.size() <= TERRAIN_MAX_TEXTURE_BYTES && name.find('\0') == std::string::npos;
}

inline TerrainProblem terrain_check_splats(const TerrainPlan& p, const std::vector<SplatRect>& splats) {
    if (!p.ok()) return p.problem;
    if (splats.size() > TERRAIN_MAX_SPLATS) return TerrainProblem::Splats;
    if (!splats.empty() && p.quads() > TERRAIN_MAX_SPLAT_CHECKS / splats.size())
        return TerrainProblem::SplatBudget;
    for (const SplatRect& s : splats)
        if (!terrain_valid_texture_name(s.tex) || !terrain_valid_splat(s.x, s.z, s.w, s.h))
            return TerrainProblem::Splats;
    return TerrainProblem::None;
}

inline bool terrain_valid_tile(double n) { return terrain_between(n, TERRAIN_MIN_TILE, TERRAIN_MAX_TILE); }
// 0.01 ist als float minimal kleiner als der double-Grenzwert. Nach einer
// bereits validierten Konversion muss die untere Grenze weiterhin gelten.
inline bool terrain_valid_tile_float(float n) {
    return std::isfinite(n) && n >= static_cast<float>(TERRAIN_MIN_TILE) &&
           n <= static_cast<float>(TERRAIN_MAX_TILE);
}
inline float terrain_uv_scale(float tile) {
    return 2.0f / (terrain_valid_tile_float(tile) ? tile : ATLAS_NEUTRAL_TILE);
}
inline bool atmosphere_valid_color(double n) { return terrain_between(n, 0.0, 16777215.0); }
inline bool atmosphere_valid_fog_far(double n) { return terrain_between(n, FOG_MIN_FAR, FOG_MAX_FAR); }
inline bool atmosphere_valid_weather(double n) { return std::isfinite(n) && n > 0.0 && n <= WEATHER_MAX_INTENSITY; }

} // namespace rune::world
