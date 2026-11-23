#pragma once

#include <cstdint>
#include <vector>

#include "gfx/mesh_vertex.h"
#include "math/math.h"
#include "world/atlas.h"
#include "world/terrain_limits.h"

namespace rune::world {

// Heightmap-Gelände. Wird komplett im Speicher erzeugt (kein Asset nötig),
// damit ein Projekt ohne Artwork sofort begehbar ist. Ruby kann es mit
// Seed/Grösse/Amplitude neu erzeugen - F5 lädt also auch die Landschaft neu.
class Terrain {
public:
    // width/height = Kantenlängen in Welteinheiten (rechteckige Karten,
    // Etappe 17b), resolution = Zellen entlang der Breite.
    // Bei ungültigen Parametern, übermäßigem Aufwand oder Speichermangel:
    // false und kein altes/teilweises Terrain. Die Karte wird nicht verändert.
    bool generate(float width, float height, int resolution, uint32_t seed,
                  float amplitude);

    // Etappe 18d: wie generate(), aber mit Boden-Flächen: Jedes Terrain-Quad
    // zeigt dann genau eine Atlas-Zelle (per-Quad-UV). base = Textur für
    // nicht überdeckte Quads. Ohne Flächen verhält sich exakt wie generate().
    bool generate_splat(float width, float height, int resolution, uint32_t seed,
                        float amplitude, const std::string& base,
                        const std::vector<SplatRect>& splats,
                        const AtlasInfo& atlas);
    void clear();

    bool  ready() const { return !m_heights.empty(); }
    float width() const { return m_width; }
    float height() const { return m_height; }
    int   resolution() const { return m_res_x; }
    // Etappe 18l: weiche Höhen-Pinsel aus dem Karten-JSON ({x,z,r,h}) -
    // nach dem FBM-Rauschen addiert, damit Berge "gemalt" werden können,
    // ohne dass der Editor die gesamte Heightmap im JSON ablegen muss.
    bool set_height_brushes(const std::vector<math::Vec4>& brushes);
    void invalidate_height_brushes();
    void clear_height_brushes();
    // Etappe 18d: Parameter des letzten generate()-Laufs (für Regenerierung
    // mit Splats aus Ruby ohne die alten Argumente neu übergeben zu müssen).
    uint32_t seed() const { return m_seed; }
    float amplitude() const { return m_amplitude; }
    // Etappe 18l: Karten-Grenzfreqünz? Nein - halbe Kantenlänge für den
    // Physik-Grenzklemm-Wert (Spieler darf die Karte nicht verlassen).
    float half_w() const { return m_width * 0.5f; }
    float half_h() const { return m_height * 0.5f; }

    // Höhe an einer Weltposition (bilinear interpoliert, ausserhalb geklemmt).
    float height_at(float x, float z) const;
    math::Vec3 normal_at(float x, float z) const;

    const std::vector<gfx::MeshVertex>& vertices() const { return m_verts; }
    const std::vector<uint32_t>&        indices() const { return m_idx; }

private:
    float sample(int ix, int iz) const;
    void clear_mesh();
    bool prepare(const TerrainPlan& plan);

    // Etappe 18d: shared Teil von generate/generate_splat.
    void compute_heights(uint32_t seed, float amplitude);
    gfx::MeshVertex make_vertex(int ix, int iz, uint32_t seed);

    // Etappe 18l: gemalte Höhen-Pinsel (x, z, r, h) - weiche Kegel, die
    // in compute_heights() auf das FBM addiert werden.
    std::vector<math::Vec4> m_brushes;
    bool m_brushes_valid = true;

    float                       m_width = 0.0f;
    float                       m_height = 0.0f;
    int                         m_res_x = 0;
    int                         m_res_z = 0;
    uint32_t                    m_seed = 7u;        // Etappe 18d
    float                       m_amplitude = 0.0f;  // Etappe 18d
    std::vector<float>          m_heights;
    std::vector<gfx::MeshVertex> m_verts;
    std::vector<uint32_t>       m_idx;
};

} // namespace rune::world
