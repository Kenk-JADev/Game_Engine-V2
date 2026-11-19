#pragma once
// ============================================================================
//  Etappe 18d: Atlas-Layout fuer Splat-Boeden.
//  Basistyp ohne Abhaengigkeiten, damit gfx/renderer.h und world/terrain.h
//  ihn beide nutzen koennen (terrain.h inkludiert renderer.h).
// ============================================================================

#include <map>
#include <string>

namespace rune::world {

// Rechteckige Boden-Flaeche ("Splat") mit eigener Textur.
// x/z = Ecke (Weltmeter, Karte auf 0 zentriert), w/h = Ausdehnung.
// Der spaetere Eintrag in der Liste gewinnt (Flaechen stapeln).
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
