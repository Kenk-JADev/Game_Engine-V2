#pragma once
// Etappe 37: reine Geometrie für die F5-Objekttyp-Vorschau. Kein Win32, kein
// Renderer: die Prefab-Teile werden als überlappende 2D-Kästen projiziert,
// ähnlich dem Vorschaubild eines Tilesets. Kopflos testbar.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace rune::editor {

struct PrefabPreviewPart {
    double dx = 0.0, dy = 0.0, dz = 0.0;
    double sx = 0.0, sy = 0.0, sz = 0.0;
    std::uint32_t color = 0xFFFFFFu;
    std::string tex;
};

struct PrefabPreviewBox {
    int left = 0, top = 0, right = 0, bottom = 0;
    std::uint32_t color = 0xFFFFFFu;
    std::string tex;
};

inline bool prefab_preview_finite(double v) {
    return std::isfinite(v) && std::fabs(v) < 1.0e7;
}

inline bool prefab_preview_valid(const PrefabPreviewPart& p) {
    return prefab_preview_finite(p.dx) && prefab_preview_finite(p.dy) && prefab_preview_finite(p.dz) &&
           prefab_preview_finite(p.sx) && prefab_preview_finite(p.sy) && prefab_preview_finite(p.sz) &&
           p.sx >= 0.0 && p.sy >= 0.0 && p.sz >= 0.0;
}

// Maßtabsvorschlag in logischen Pixeln pro Meter. Sehr kleine und sehr große
// Prefabs bleiben dadurch sichtbar, ohne dass ein Cast überläuft.
inline double prefab_preview_scale(const std::vector<PrefabPreviewPart>& parts,
                                   int area_w, int area_h) {
    double span_x = 1.0, span_y = 1.0;
    for (const PrefabPreviewPart& p : parts) {
        if (!prefab_preview_valid(p)) continue;
        span_x = std::max(span_x, std::fabs(p.dx) + p.sx * 0.5 + std::fabs(p.dz) + p.sz * 0.5);
        span_y = std::max(span_y, p.dy + p.sy * 0.5 + (std::fabs(p.dx) + std::fabs(p.dz)) * 0.35);
    }
    const double w = area_w > 8 ? static_cast<double>(area_w) : 96.0;
    const double h = area_h > 8 ? static_cast<double>(area_h) : 96.0;
    const double fit = std::min(w / (2.6 * span_x), h / (1.8 * span_y));
    return std::clamp(fit, 3.0, 64.0);
}

// Projiziert die Teile in die Vorschafläche. hints = relative Tiefe; weiter
// entfernte Teile werden zuerst gezeichnet, damit nahe Teile oben liegen.
inline std::vector<PrefabPreviewBox> prefab_preview_boxes(
        const std::vector<PrefabPreviewPart>& parts, double center_x, double center_y,
        double scale, int area_w, int area_h) {
    std::vector<PrefabPreviewBox> out;
    if (area_w <= 0 || area_h <= 0) return out;
    if (!prefab_preview_finite(center_x) || !prefab_preview_finite(center_y) ||
        !std::isfinite(scale)) return out;
    // Sehr große, aber endliche Maßstäbe werden begrenzt statt verworfen;
    // nur NaN/Inf sind kein gültiger Maßstab.
    scale = std::clamp(scale, 0.5, 256.0);
    std::vector<std::pair<double, PrefabPreviewBox>> sorted;
    sorted.reserve(parts.size());
    for (const PrefabPreviewPart& p : parts) {
        if (!prefab_preview_valid(p)) continue;
        const double cx = center_x + (p.dx - p.dz) * scale;
        const double cy = center_y + (p.dx + p.dz) * scale * 0.5 - p.dy * scale;
        const double hw = (p.sx + p.sz) * scale * 0.35;
        const double hh = p.sy * scale * 0.5 + (p.sx + p.sz) * scale * 0.18;
        if (!prefab_preview_finite(cx) || !prefab_preview_finite(cy) ||
            !prefab_preview_finite(hw) || !prefab_preview_finite(hh)) continue;
        PrefabPreviewBox box;
        box.left = static_cast<int>(std::clamp(std::round(cx - hw), -1.0e7, 1.0e7));
        box.top = static_cast<int>(std::clamp(std::round(cy - hh), -1.0e7, 1.0e7));
        box.right = static_cast<int>(std::clamp(std::round(cx + hw), -1.0e7, 1.0e7));
        box.bottom = static_cast<int>(std::clamp(std::round(cy + hh), -1.0e7, 1.0e7));
        box.left = std::clamp(box.left, 0, area_w - 1);
        box.top = std::clamp(box.top, 0, area_h - 1);
        box.right = std::clamp(box.right, box.left + 1, area_w - 1);
        box.bottom = std::clamp(box.bottom, box.top + 1, area_h - 1);
        box.color = p.color;
        box.tex = p.tex;
        sorted.emplace_back(p.dx + p.dz + p.dy * 0.01, box);
    }
    std::stable_sort(sorted.begin(), sorted.end(),
                     [](const auto& a, const auto& b) { return a.first < b.first; });
    out.reserve(sorted.size());
    for (auto& entry : sorted) out.push_back(std::move(entry.second));
    return out;
}

} // namespace rune::editor
