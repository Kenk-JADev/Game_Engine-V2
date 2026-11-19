#pragma once
// ============================================================================
//  Etappe 18d: Atlas-Layout für Splat-Böden.
//  Basistyp ohne Abhängigkeiten, damit gfx/renderer.h und world/terrain.h
//  ihn beide nutzen können (terrain.h inkludiert renderer.h).
// ============================================================================

#include <map>
#include <string>

namespace rune::world {

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

} // namespace rune::world
