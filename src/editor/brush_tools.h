#pragma once
// Etappe 34: Werkzeuge Verschieben, Boden malen, Höhe malen verbessert.
// Reine Rechenlogik ohne Win32/Renderer, kopflos getestet (json_test.cpp).
//
//  - Pinselstriche: Malen/Höhe bei gedrückter Maus entlang des Wegs, mit
//    Mindestabstand zwischen zwei Stempeln (kein Budget-Verbrauch durch
//    hunderte gleiche Einträge) und EINEM Undo-Schritt pro Strich.
//  - Sichtbare Pinselform: Ring für den runden Höhenpinsel, Rahmen für den
//    quadratischen Bodenpinsel, lokales Raster um den Cursor.
//  - Marker (Events/NPCs) ziehen mit Rasterfang wie Objekte.

#include <algorithm>
#include <cmath>
#include <vector>

namespace rune::editor {

struct BrushPoint {
    float x = 0.0f, z = 0.0f;
};

inline bool brush_finite(float v) { return std::isfinite(v) && std::fabs(v) <= 1.0e6f; }

// Auf das Raster runden (wie splat_edit/height_edit es intern tun).
inline float brush_snap(float v, float grid) {
    if (!brush_finite(v)) return 0.0f;
    if (!(grid > 0.0f) || !std::isfinite(grid)) return v;
    return std::round(v / grid) * grid;
}

// Abstand zwischen zwei Stempeln eines Strichs: halbe Pinselgröße (beim
// Höhenpinsel = sein Radius, beim Bodenpinsel = halbe Kante), nie kleiner
// als eine Rasterzelle. So überlappen die Stempel lückenlos.
inline float brush_stroke_spacing(float brush_size, float grid) {
    const float g = (grid > 0.0f && std::isfinite(grid)) ? grid : 1.0f;
    const float s = (brush_size > 0.0f && std::isfinite(brush_size)) ? brush_size : g;
    return std::max(g, s * 0.5f);
}

// Radius des Höhenpinsels wie height_edit: max(Raster, Pinselgröße/2).
inline float brush_height_radius(float brush_size, float grid) {
    return brush_stroke_spacing(brush_size, grid);
}

// Neuer Stempel nötig? Vergleicht die GERASTERTEN Mittelpunkte, damit
// kleine Mausbewegungen innerhalb derselben Zelle nichts doppelt anlegen.
inline bool brush_stroke_should_stamp(const BrushPoint& last, const BrushPoint& now,
                                      float spacing, float grid) {
    if (!brush_finite(now.x) || !brush_finite(now.z)) return false;
    const float lx = brush_snap(last.x, grid), lz = brush_snap(last.z, grid);
    const float nx = brush_snap(now.x, grid), nz = brush_snap(now.z, grid);
    if (lx == nx && lz == nz) return false;
    const float dx = nx - lx, dz = nz - lz;
    return std::sqrt(dx * dx + dz * dz) + 1e-4f >= spacing;
}

// Gelände-Neuaufbau während des Strichs drosseln (ms seit letztem Aufbau).
constexpr unsigned BRUSH_REGEN_INTERVAL_MS = 60;

inline bool brush_regen_due(unsigned long long now_ms, unsigned long long last_ms) {
    return now_ms < last_ms || now_ms - last_ms >= BRUSH_REGEN_INTERVAL_MS;
}

// Punkte auf einem Kreis (Ring-Vorschau des Höhenpinsels). Anzahl wächst
// mit dem Radius, begrenzt auf 16..64 Segmente.
inline std::vector<BrushPoint> brush_ring_points(float cx, float cz, float radius) {
    std::vector<BrushPoint> out;
    if (!brush_finite(cx) || !brush_finite(cz) || !(radius > 0.0f) || !brush_finite(radius)) return out;
    const int count = std::clamp(static_cast<int>(radius * 4.0f), 16, 64);
    out.reserve(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) {
        const float a = 6.2831853f * static_cast<float>(i) / static_cast<float>(count);
        out.push_back({cx + std::cos(a) * radius, cz + std::sin(a) * radius});
    }
    return out;
}

// Linien eines lokalen Rasters um den Cursor: je Richtung `cells` Zellen.
// Jede Linie als Start/Ende. Bei sehr feinem Raster (unter 0,25 m) leer,
// damit die Ansicht nicht zugekleistert wird.
struct BrushLine {
    BrushPoint a, b;
};

inline std::vector<BrushLine> brush_grid_lines(float cx, float cz, float grid, int cells) {
    std::vector<BrushLine> out;
    if (!(grid >= 0.25f) || !std::isfinite(grid) || cells <= 0 || cells > 32 ||
        !brush_finite(cx) || !brush_finite(cz)) return out;
    const float ox = brush_snap(cx, grid), oz = brush_snap(cz, grid);
    const float half = grid * static_cast<float>(cells);
    for (int i = -cells; i <= cells; ++i) {
        const float o = grid * static_cast<float>(i);
        out.push_back({{ox + o, oz - half}, {ox + o, oz + half}}); // entlang z
        out.push_back({{ox - half, oz + o}, {ox + half, oz + o}}); // entlang x
    }
    return out;
}

// Marker ziehen: Zielposition mit Rasterfang. Solange die Bewegung unter
// einer halben Zelle bleibt, bleibt der Startwert exakt erhalten (wie
// object_drag_prepare), damit ein Klick ohne Bewegung nichts verändert.
// double, nicht float: Der unveränderte Startwert muss BITGENAU erhalten
// bleiben, sonst würde 3.3 still zu 3.2999999… umgeschrieben.
inline double brush_drag_axis(double start, double delta, double grid) {
    if (!std::isfinite(start) || !std::isfinite(delta)) return start;
    if (!(grid > 0.0) || !std::isfinite(grid)) return start + delta;
    if (std::round(delta / grid) == 0.0) return start;
    return std::round((start + delta) / grid) * grid;
}

// Pfeiltasten: Objekt um eine Rasterzelle schieben (ebenfalls gerastert).
inline float brush_nudge(double value, int direction, double grid) {
    const double g = (grid > 0.0 && std::isfinite(grid)) ? grid : 1.0;
    if (!std::isfinite(value) || direction == 0) return static_cast<float>(value);
    return static_cast<float>(std::round(value / g) * g + (direction > 0 ? g : -g));
}

} // namespace rune::editor
