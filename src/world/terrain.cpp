#include "world/terrain.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <new>
#include <stdexcept>

using namespace rune::math;

namespace rune::world {
namespace {

// Deterministischer Wert-Noise: gleicher Seed -> gleiches Gelände auf jedem Rechner.
uint32_t hash2(int x, int z, uint32_t seed) {
    uint32_t h = seed;
    h ^= static_cast<uint32_t>(x) * 0x9E3779B1u;
    h ^= static_cast<uint32_t>(z) * 0x85EBCA77u;
    h ^= h >> 15;
    h *= 0x2545F491u;
    h ^= h >> 13;
    h *= 0xC2B2AE3Du;
    h ^= h >> 16;
    return h;
}

float rnd01(int x, int z, uint32_t seed) {
    return static_cast<float>(hash2(x, z, seed) & 0xFFFFFF) / 16777215.0f;
}

float smooth01(float t) { return t * t * (3.0f - 2.0f * t); }

float value_noise(float x, float z, uint32_t seed) {
    const int x0 = static_cast<int>(std::floor(x));
    const int z0 = static_cast<int>(std::floor(z));
    const float fx = smooth01(x - static_cast<float>(x0));
    const float fz = smooth01(z - static_cast<float>(z0));

    const float a = rnd01(x0, z0, seed);
    const float b = rnd01(x0 + 1, z0, seed);
    const float c = rnd01(x0, z0 + 1, seed);
    const float d = rnd01(x0 + 1, z0 + 1, seed);

    const float top = a * (1.0f - fx) + b * fx;
    const float bot = c * (1.0f - fx) + d * fx;
    return top * (1.0f - fz) + bot * fz;
}

float fbm(float x, float z, uint32_t seed, int octaves) {
    float sum = 0.0f, amp = 1.0f, freq = 1.0f, norm = 0.0f;
    for (int i = 0; i < octaves; ++i) {
        sum += value_noise(x * freq, z * freq, seed + static_cast<uint32_t>(i) * 7919u) * amp;
        norm += amp;
        amp *= 0.5f;
        freq *= 2.03f;
    }
    return sum / norm; // 0..1
}

Vec3 rgb(uint32_t c) {
    return Vec3{((c >> 16) & 0xFF) / 255.0f, ((c >> 8) & 0xFF) / 255.0f, (c & 0xFF) / 255.0f};
}

Vec3 lerp(Vec3 a, Vec3 b, float t) {
    return Vec3{a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t};
}

} // namespace

void Terrain::clear_mesh() {
    m_width = m_height = 0.0f;
    m_res_x = m_res_z = 0;
    std::vector<float>().swap(m_heights);
    std::vector<gfx::MeshVertex>().swap(m_verts);
    std::vector<uint32_t>().swap(m_idx);
}

void Terrain::clear() {
    clear_mesh();
    clear_height_brushes();
}

bool Terrain::prepare(const TerrainPlan& plan) {
    if (!plan.ok() || !m_brushes_valid ||
        terrain_check_brushes(plan, m_brushes) != TerrainProblem::None) {
        clear_mesh();
        return false;
    }
    m_width = plan.width;
    m_height = plan.height;
    m_res_x = plan.res_x;
    m_res_z = plan.res_z;
    m_seed = plan.seed;
    m_amplitude = plan.amplitude;
    return true;
}

// Gemeinsamer Höhen-Teil: Raster + fbm + flächige Mitte (Dorf-Platz).
void Terrain::compute_heights(uint32_t seed, float amplitude) {
    const float step_x = m_width / static_cast<float>(m_res_x);
    const float step_z = m_height / static_cast<float>(m_res_z);

    m_heights.assign((static_cast<size_t>(m_res_z) + 1) * (static_cast<size_t>(m_res_x) + 1), 0.0f);

    for (int iz = 0; iz <= m_res_z; ++iz) {
        for (int ix = 0; ix <= m_res_x; ++ix) {
            const float x = -m_width * 0.5f + static_cast<float>(ix) * step_x;
            const float z = -m_height * 0.5f + static_cast<float>(iz) * step_z;

            // Mehrere Rausch-Oktaven, um -0.5..0.5 zentriert.
            float h = (fbm(x * 0.020f, z * 0.020f, seed, 4) - 0.5f) * 2.0f * amplitude;

            // Rund um das Dorf (x,z nahe 0) flach halten - die Häuser stehen auf 0.
            // Etappe 18l: Nur das Rauschen wird abgeflacht - gemalte Pinsel
            // wirken auch im Dorfkern (dort will man ebenfalls Hügel/Senken).
            const float d = std::sqrt(x * x + z * z);
            float flat = (d - 16.0f) / 26.0f;
            flat = std::clamp(flat, 0.0f, 1.0f);
            flat = smooth01(flat);
            h *= flat;

            m_heights[static_cast<size_t>(iz) * (m_res_x + 1) + ix] = h;
        }
    }

    // Etappe 18l: gemalte Höhen-Pinsel nachtragen. Konvention: x/z = Welt-
    // position, w = Radius (m), y = Höhen-Änderung (negativ = Senke).
    // Weicher Kegel mit cos-Falloff: Mitte volle Höhe, Rand 0 - dadurch
    // keine harte Kante im Gelände. Wirkt auch im Dorfkern.
    TerrainPlan plan;
    plan.width = m_width; plan.height = m_height;
    plan.res_x = m_res_x; plan.res_z = m_res_z;
    for (const Vec4& b : m_brushes) {
        const float r = b.w;
        const float h_add = b.y;
        if (h_add == 0.0f) continue;
        const TerrainBrushBounds bounds = terrain_brush_bounds(plan, b);
        for (int iz = bounds.z0; iz <= bounds.z1; ++iz) {
            for (int ix = bounds.x0; ix <= bounds.x1; ++ix) {
                const float x = -m_width * 0.5f + static_cast<float>(ix) * step_x;
                const float z = -m_height * 0.5f + static_cast<float>(iz) * step_z;
                const float dist = std::sqrt((x - b.x) * (x - b.x) +
                                             (z - b.z) * (z - b.z));
                if (dist >= r) continue;
                const float t = dist / r;                      // 0 Mitte .. 1 Rand
                const float falloff = 0.5f * (1.0f + std::cos(t * PI));
                m_heights[static_cast<size_t>(iz) * (m_res_x + 1) + ix] +=
                    h_add * falloff;
            }
        }
    }
}

// Vertexdaten (Position/Normale/Farbe) an einem Gitterpunkt; der UV-Part
// unterscheidet sich je nach Mesh-Typ und wird vom Aufrufer gefüllt.
gfx::MeshVertex Terrain::make_vertex(int ix, int iz, uint32_t seed) {
    const float step_x = m_width / static_cast<float>(m_res_x);
    const float step_z = m_height / static_cast<float>(m_res_z);
    const size_t i = static_cast<size_t>(iz) * (m_res_x + 1) + ix;
    const float x = -m_width * 0.5f + static_cast<float>(ix) * step_x;
    const float z = -m_height * 0.5f + static_cast<float>(iz) * step_z;
    const float h = m_heights[i];

    // Normale per zentralem Differenzenquotient (2 Zellen Abstand).
    const int p = std::min(ix + 2, m_res_x);
    const int m = std::max(ix - 2, 0);
    const int pt = std::min(iz + 2, m_res_z);
    const int mb = std::max(iz - 2, 0);
    const float dhx = (m_heights[static_cast<size_t>(iz) * (m_res_x + 1) + p] -
                       m_heights[static_cast<size_t>(iz) * (m_res_x + 1) + m]) /
                      (2.0f * step_x);
    const float dhz = (m_heights[static_cast<size_t>(pt) * (m_res_x + 1) + ix] -
                       m_heights[static_cast<size_t>(mb) * (m_res_x + 1) + ix]) /
                      (2.0f * step_z);
    Vec3 nrm = normalize(Vec3{-dhx, 1.0f, -dhz});

    // Farbe nach Höhe und Steigung - nur noch als leichte Tönung,
    // die Grass-Textur trägt die Grundfarbe.
    const uint32_t grass_a = 0x39512B;
    const uint32_t grass_b = 0x4C6A38;
    const uint32_t rock    = 0x6E6455;
    const uint32_t sand    = 0x6F6A45;
    const float n = fbm(x * 0.05f, z * 0.05f, seed ^ 0xA5A5A5u, 2);
    Vec3 col = lerp(rgb(grass_a), rgb(grass_b), n);
    if (h < -1.2f) col = lerp(col, rgb(sand), 0.6f);
    const float slope = 1.0f - nrm.y;
    if (slope > 0.28f) col = lerp(col, rgb(rock), (slope - 0.28f) / 0.4f);
    col = Vec3{col.x * 0.35f + 0.65f, col.y * 0.35f + 0.65f, col.z * 0.35f + 0.65f};

    return gfx::MeshVertex{Vec3{x, h, z}, nrm, col, {0.0f, 0.0f}};
}

bool Terrain::generate(float width, float height, int resolution, uint32_t seed,
                       float amplitude) {
    const TerrainPlan plan = terrain_plan(width, height, resolution, seed, amplitude);
    if (!prepare(plan)) return false;
    try {
    compute_heights(seed, amplitude);

    // Vertices + Indices. UV in Weltmetern (2 m pro Kachel), damit das Gras
    // überall gleich gross kachelt.
    m_verts.clear();
    m_idx.clear();
    m_verts.reserve((static_cast<size_t>(m_res_z) + 1) * (static_cast<size_t>(m_res_x) + 1));
    m_idx.reserve(plan.quads() * 6);

    for (int iz = 0; iz <= m_res_z; ++iz) {
        for (int ix = 0; ix <= m_res_x; ++ix) {
            gfx::MeshVertex v = make_vertex(ix, iz, seed);
            v.uv[0] = v.pos.x / 2.0f;
            v.uv[1] = v.pos.z / 2.0f;
            m_verts.push_back(v);
        }
    }

    const size_t stride = static_cast<size_t>(m_res_x) + 1;
    for (int iz = 0; iz < m_res_z; ++iz) {
        for (int ix = 0; ix < m_res_x; ++ix) {
            const uint32_t a = static_cast<uint32_t>(iz * stride + ix);
            const uint32_t b = static_cast<uint32_t>(iz * stride + ix + 1);
            const uint32_t c = static_cast<uint32_t>((iz + 1) * stride + ix);
            const uint32_t d = static_cast<uint32_t>((iz + 1) * stride + ix + 1);
            m_idx.push_back(c); m_idx.push_back(d); m_idx.push_back(b);
            m_idx.push_back(c); m_idx.push_back(b); m_idx.push_back(a);
        }
    }
    return true;
    } catch (const std::bad_alloc&) {
        clear_mesh(); return false;
    } catch (const std::length_error&) {
        clear_mesh(); return false;
    }
}

bool Terrain::generate_splat(float width, float height, int resolution, uint32_t seed,
                             float amplitude, const std::string& base,
                             const std::vector<SplatRect>& splats,
                             const AtlasInfo& atlas) {
    const TerrainPlan plan = terrain_plan(width, height, resolution, seed, amplitude);
    if (!prepare(plan)) return false;
    if (terrain_check_splats(plan, splats) != TerrainProblem::None || !atlas_has_texture(atlas, base)) {
        clear_mesh(); return false;
    }
    try {
    compute_heights(seed, amplitude);

    m_verts.clear();
    m_idx.clear();
    // Pro Quad 4 eigene Vertices (nicht geteilt) - jedes Quad bekommt UVs
    // innerhalb genau einer Atlas-Zelle.
    m_verts.reserve(plan.quads() * 4);
    m_idx.reserve(plan.quads() * 6);

    // Basiszelle; skaliert die localen Quad-UVs in die Zelle (halb Pixel
    // Inset pro Rand, damit bilineares Filtering nicht in die Nachbar-
    // Zelle blutet).
    const float inset = 0.5f / 64.0f;         // im Zell-UV-Raum 0..1
    const int cell_cols = std::max(1, atlas.cols);

    // Textur eines Punkts: letzter (spätester) Splat, der ihn überdeckt,
    // sonst Basis. Ein unbekannter Name fällt auch über älteren Flächen auf
    // die Basis zurück, statt versehentlich eine ältere Fläche durchzulassen.
    const int base_cell = atlas.cell_of.find(base)->second; // oben validiert
    const std::size_t atlas_cells = static_cast<std::size_t>(atlas.cols) * static_cast<std::size_t>(atlas.rows);
    auto tex_at = [&](float px, float pz) -> int {
        int cell = base_cell;
        for (const SplatRect& s : splats) {
            if (px >= s.x && px < s.x + s.w && pz >= s.z && pz < s.z + s.h) {
                const auto c = atlas.cell_of.find(s.tex);
                cell = c != atlas.cell_of.end() && c->second >= 0 &&
                       static_cast<std::size_t>(c->second) < atlas_cells ? c->second : base_cell;
            }
        }
        return cell;
    };

    const float step_x = m_width / static_cast<float>(m_res_x);
    const float step_z = m_height / static_cast<float>(m_res_z);

    for (int iz = 0; iz < m_res_z; ++iz) {
        for (int ix = 0; ix < m_res_x; ++ix) {
            // Quad-Mitte entscheidet über die Zelle (kein Misch-Quad).
            const float cx = -m_width * 0.5f + (static_cast<float>(ix) + 0.5f) * step_x;
            const float cz = -m_height * 0.5f + (static_cast<float>(iz) + 0.5f) * step_z;
            const int cell = tex_at(cx, cz); // immer gültig: fehlende Zellen nutzen die Basis
            const int col = cell % cell_cols;
            const int row = cell / cell_cols;

            // Vier Eckvertices mit UV in der Zelle (v-Richtung: erster Pixel-
            // oben-links wie die UI-Vorschau).
            const float u0 = (static_cast<float>(col) + inset) / static_cast<float>(cell_cols);
            const float u1 = (static_cast<float>(col) + 1.0f - inset) / static_cast<float>(cell_cols);
            const float v0 = (static_cast<float>(row) + inset) / static_cast<float>(std::max(1, atlas.rows));
            const float v1 = (static_cast<float>(row) + 1.0f - inset) / static_cast<float>(std::max(1, atlas.rows));

            std::array<float, 8> uv_quad = { u0, v0,  u1, v0,  u0, v1,  u1, v1 };
            const int corners[4][2] = { {ix, iz}, {ix + 1, iz}, {ix, iz + 1}, {ix + 1, iz + 1} };

            const uint32_t base_index = static_cast<uint32_t>(m_verts.size());
            for (int k = 0; k < 4; ++k) {
                gfx::MeshVertex v = make_vertex(corners[k][0], corners[k][1], seed);
                v.uv[0] = uv_quad[k * 2];
                v.uv[1] = uv_quad[k * 2 + 1];
                m_verts.push_back(v);
            }
            // Wie beim Einzeltextur-Mesh: gemeinsame Diagonale 2–1 und beide
            // Dreiecke nach oben. Eine Außenkante als Teilung überlappt sonst.
            m_idx.push_back(base_index + 2); m_idx.push_back(base_index + 3); m_idx.push_back(base_index + 1);
            m_idx.push_back(base_index + 2); m_idx.push_back(base_index + 1); m_idx.push_back(base_index + 0);
        }
    }
    return true;
    } catch (const std::bad_alloc&) {
        clear_mesh(); return false;
    } catch (const std::length_error&) {
        clear_mesh(); return false;
    }
}

float Terrain::sample(int ix, int iz) const {
    ix = std::clamp(ix, 0, m_res_x);
    iz = std::clamp(iz, 0, m_res_z);
    return m_heights[static_cast<size_t>(iz) * (m_res_x + 1) + ix];
}

float Terrain::height_at(float x, float z) const {
    if (!ready() || !std::isfinite(x) || !std::isfinite(z)) return 0.0f;
    const float step_x = m_width / static_cast<float>(m_res_x);
    const float step_z = m_height / static_cast<float>(m_res_z);
    // Vor floor/int auf die Kartengrenzen klemmen, auch für FLT_MAX.
    x = std::clamp(x, -m_width * 0.5f, m_width * 0.5f);
    z = std::clamp(z, -m_height * 0.5f, m_height * 0.5f);
    const float gx = (x + m_width * 0.5f) / step_x;
    const float gz = (z + m_height * 0.5f) / step_z;

    const int ix = std::clamp(static_cast<int>(std::floor(gx)), 0, m_res_x - 1);
    const int iz = std::clamp(static_cast<int>(std::floor(gz)), 0, m_res_z - 1);
    const float fx = std::clamp(gx - static_cast<float>(ix), 0.0f, 1.0f);
    const float fz = std::clamp(gz - static_cast<float>(iz), 0.0f, 1.0f);

    const float h00 = sample(ix, iz);
    const float h10 = sample(ix + 1, iz);
    const float h01 = sample(ix, iz + 1);
    const float h11 = sample(ix + 1, iz + 1);

    const float top = h00 * (1.0f - fx) + h10 * fx;
    const float bot = h01 * (1.0f - fx) + h11 * fx;
    return top * (1.0f - fz) + bot * fz;
}

Vec3 Terrain::normal_at(float x, float z) const {
    if (!std::isfinite(x) || !std::isfinite(z)) return Vec3{0.0f, 1.0f, 0.0f};
    x = std::clamp(x, -m_width * 0.5f, m_width * 0.5f);
    z = std::clamp(z, -m_height * 0.5f, m_height * 0.5f);
    const float e = 0.25f;
    const Vec3 p = Vec3{x, height_at(x, z), z};
    const Vec3 dx = Vec3{x + e, height_at(x + e, z), z};
    const Vec3 dz = Vec3{x, height_at(x, z + e), z + e};
    return normalize(cross(sub(dz, p), sub(dx, p)));
}

// Etappe 18l: gemalte Höhen-Pinsel setzen (vor dem nächsten generate()).
// Konvention: x/z = Weltposition, w = Radius in Metern, y = Höhen-Änderung
// (negativ = Senke). Die Pinsel werden in compute_heights() nach dem FBM
// addiert - ein erneutes generate() baut sie also reproduzierbar ein.
bool Terrain::set_height_brushes(const std::vector<Vec4>& brushes) {
    if (brushes.size() > TERRAIN_MAX_BRUSHES ||
        std::any_of(brushes.begin(), brushes.end(), [](const Vec4& b) {
            return !terrain_valid_brush(b.x, b.z, b.w, b.y);
        })) {
        invalidate_height_brushes();
        return false;
    }
    try {
        m_brushes = brushes;
        m_brushes_valid = true;
        return true;
    } catch (const std::bad_alloc&) {
        invalidate_height_brushes();
        return false;
    } catch (const std::length_error&) {
        invalidate_height_brushes();
        return false;
    }
}

void Terrain::invalidate_height_brushes() {
    std::vector<Vec4>().swap(m_brushes);
    m_brushes_valid = false;
}

void Terrain::clear_height_brushes() {
    std::vector<Vec4>().swap(m_brushes);
    m_brushes_valid = true;
}

} // namespace rune::world
