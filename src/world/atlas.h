#pragma once
// ============================================================================
//  Etappe 18d: Atlas-Layout für Splat-Böden.
//  Portabler Basistyp für Renderer und Terrain, ohne Win32-Abhängigkeit.
// ============================================================================

#include <cstddef>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace rune::world {

// Der RGBA-Atlas wird im RAM und auf der GPU gehalten. Höchstens 1024
// 64×64-Zellen bedeuten ein Raster bis 2048×2048 (16 MiB pro Kopie).
inline constexpr std::size_t ATLAS_MAX_CELLS = 1024;
// Der Terrain-Shader multipliziert UV mit 2/tile. Atlas-UV dürfen nicht
// skaliert werden, sonst landen sie in anderen Zellen des Atlas.
inline constexpr float ATLAS_NEUTRAL_TILE = 2.0f;
struct AtlasPlan {
    int cols = 0, rows = 0;
    std::size_t rgba_bytes = 0;
    bool ok() const { return cols > 0 && rows > 0; }
};

inline AtlasPlan atlas_plan(std::size_t cells) {
    if (cells == 0 || cells > ATLAS_MAX_CELLS) return {};
    std::size_t cols = 1;
    while (cols * cols < cells) ++cols;
    const std::size_t rows = (cells + cols - 1) / cols;
    return {static_cast<int>(cols), static_cast<int>(rows), cols * rows * 64 * 64 * 4};
}

struct AtlasSources {
    std::vector<std::pair<std::string, int>> entries;
    bool too_many = false;
};

// Nur öffentliche Namens-Slots einsammeln. Der interne Atlas steht nicht in
// ids; eine echte Benutzertextur namens "atlas" ist dagegen ein normaler Tile.
inline AtlasSources atlas_sources(const std::map<std::string, int>& ids,
                                  const std::vector<std::pair<int, int>>& sizes) {
    AtlasSources result;
    for (const auto& [name, idx] : ids) {
        if (idx < 0 || static_cast<std::size_t>(idx) >= sizes.size()) continue;
        const auto& size = sizes[static_cast<std::size_t>(idx)];
        if (size.first != 64 || size.second != 64) continue;
        if (result.entries.size() >= ATLAS_MAX_CELLS) { result.too_many = true; break; }
        result.entries.emplace_back(name, idx);
    }
    return result;
}

// Rechteckige Boden-Fläche ("Splat") mit eigener Textur.
// x/z = Ecke (Weltmeter, Karte auf 0 zentriert), w/h = Ausdehnung.
// Der spätere Eintrag in der Liste gewinnt (Flächen stapeln).
struct SplatRect {
    float x = 0.0f, z = 0.0f, w = 0.0f, h = 0.0f;
    std::string tex;
};

// Atlas-Layout: alle 64x64-Texturen in einem Raster; Terrain-Quads zeigen
// damit je eine Zelle (per-Quad-UV mit halbem Pixel Inset gegen Bluten).
struct AtlasInfo {
    bool valid = false;
    int  tex_index = -1;                     // Renderer-Slot der Atlas-Textur
    int  cols = 1, rows = 1;                 // Zellraster
    std::map<std::string, int> cell_of;      // Texturname -> Zellindex
};

// Fehlt die Basiszelle, darf das Splat-Mesh keine Löcher erzeugen: Die
// Aufrufer zeigen dann das schon vorhandene Einzeltextur-Terrain an.
inline bool atlas_has_texture(const AtlasInfo& atlas, const std::string& name) {
    if (!atlas.valid || atlas.cols <= 0 || atlas.rows <= 0 ||
        atlas.cols > static_cast<int>(ATLAS_MAX_CELLS) ||
        atlas.rows > static_cast<int>(ATLAS_MAX_CELLS)) return false;
    const std::size_t cells = static_cast<std::size_t>(atlas.cols) * static_cast<std::size_t>(atlas.rows);
    if (cells > ATLAS_MAX_CELLS) return false;
    const auto it = atlas.cell_of.find(name);
    return it != atlas.cell_of.end() && it->second >= 0 && static_cast<std::size_t>(it->second) < cells;
}

} // namespace rune::world
