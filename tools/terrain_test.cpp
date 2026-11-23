// Kopfloser Test der echten Geländegenerierung (keine Win32/D3D-Abhängigkeit).
#include "world/terrain.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <limits>
#include <map>
#include <string>
#include <utility>
#include <vector>

using namespace rune;

namespace {
int count = 0, failures = 0;
void check(const char* name, bool ok) {
    ++count;
    if (!ok) ++failures;
    std::printf("  %s %s\n", ok ? "ok  " : "FAIL", name);
}

bool finite_mesh(const world::Terrain& terrain) {
    for (const gfx::MeshVertex& v : terrain.vertices()) {
        if (!std::isfinite(v.pos.x) || !std::isfinite(v.pos.y) || !std::isfinite(v.pos.z) ||
            !std::isfinite(v.nrm.x) || !std::isfinite(v.nrm.y) || !std::isfinite(v.nrm.z) ||
            !std::isfinite(v.col.x) || !std::isfinite(v.col.y) || !std::isfinite(v.col.z) ||
            !std::isfinite(v.uv[0]) || !std::isfinite(v.uv[1])) return false;
    }
    return std::all_of(terrain.indices().begin(), terrain.indices().end(), [&](std::uint32_t i) {
        return static_cast<size_t>(i) < terrain.vertices().size();
    });
}

bool empty(const world::Terrain& t) {
    return !t.ready() && t.width() == 0 && t.height() == 0 &&
           t.vertices().empty() && t.indices().empty();
}

// Die je zwei Dreiecke müssen die vier Ecken über eine Diagonale teilen und
// beide nach oben zeigen. Nur gültige Indizes/Anzahl erkennen weder ein Loch
// noch eine Überlappung oder verkehrte Wicklung.
bool splat_quads_cover_once(const world::Terrain& t) {
    const auto& verts = t.vertices();
    const auto& indices = t.indices();
    if (verts.empty() || verts.size() % 4 != 0 || indices.size() != verts.size() / 4 * 6) return false;
    for (std::size_t q = 0; q < verts.size() / 4; ++q) {
        unsigned masks[2] = {0, 0};
        for (int tri = 0; tri < 2; ++tri) {
            const std::size_t i = q * 6 + static_cast<std::size_t>(tri) * 3;
            for (int k = 0; k < 3; ++k) {
                const std::uint32_t index = indices[i + static_cast<std::size_t>(k)];
                if (index < q * 4 || index >= q * 4 + 4) return false;
                masks[tri] |= 1u << (index - static_cast<std::uint32_t>(q * 4));
            }
            const auto& a = verts[indices[i]].pos;
            const auto& b = verts[indices[i + 1]].pos;
            const auto& c = verts[indices[i + 2]].pos;
            const float up = (b.z - a.z) * (c.x - a.x) - (b.x - a.x) * (c.z - a.z);
            if (up <= 0.0f) return false;
        }
        const unsigned shared = masks[0] & masks[1];
        if ((masks[0] | masks[1]) != 15u || (shared != 0b1001u && shared != 0b0110u)) return false;
    }
    return true;
}
} // namespace

int main() {
    using namespace world;
    std::puts("== Terrain-Parameter und Budgets ==");
    const TerrainPlan p = terrain_plan(240, 120, 64, 7, 18);
    check("uninitialisierter Plan ist vor Pinsel-/Splat-Prüfung ungültig",
          !TerrainPlan{}.ok() && terrain_check_brushes(TerrainPlan{}, {}) != TerrainProblem::None &&
          terrain_check_splats(TerrainPlan{}, {}) != TerrainProblem::None);
    check("rechteckiger Plan berechnet quadratische Zellen", p.ok() && p.res_x == 64 && p.res_z == 32 && p.quads() == 2048);
    check("Legacy-Größe und Seed bleiben gültig", terrain_plan(240, 240, 192, 7, 18).ok());
    check("gebrochene Auflösung/Seed wie bisher abschneiden",
          terrain_plan(240, 120, 64.9, 7.9, 0).res_x == 64 && terrain_plan(240, 120, 64.9, 7.9, 0).seed == 7);
    check("gültige Maximalzahl von Quads", terrain_plan(240, 240, 512, 4294967295.0, -10000).ok());
    check("Obergrenze der Quads", terrain_plan(240, 240, 513, 7, 0).problem == TerrainProblem::GridBudget);
    check("zulässiges extremes Seitenverhältnis", terrain_plan(240, 1920, 128, 7, 0).res_z == 1024);
    check("zu großes Seitenverhältnis vor lround abweisen", terrain_plan(1, 10000, 1024, 7, 0).problem == TerrainProblem::Aspect);
    check("negative und verschwindende Maße abweisen", !terrain_plan(0, 240, 192, 7, 0).ok() && !terrain_plan(-1, 240, 192, 7, 0).ok());
    check("NaN und Unendlich in Maßen abweisen", !terrain_plan(NAN, 240, 192, 7, 0).ok() && !terrain_plan(INFINITY, 240, 192, 7, 0).ok());
    check("extreme Auflösung vor Integer-Cast abweisen", !terrain_plan(240, 240, 1e300, 7, 0).ok() && !terrain_plan(240, 240, INFINITY, 7, 0).ok());
    check("Seed außerhalb uint32 vor Cast abweisen", !terrain_plan(240, 240, 192, -1, 0).ok() && !terrain_plan(240, 240, 192, 4294967296.0, 0).ok());
    check("Amplitude vor float-Cast abweisen", !terrain_plan(240, 240, 192, 7, 1e300).ok() && !terrain_plan(240, 240, 192, 7, NAN).ok());
    check("Kachel/Nebel/Wetter/Farben begrenzt", terrain_valid_tile(0.01) && !terrain_valid_tile(1e-300) &&
          atmosphere_valid_color(0xFFFFFF) && !atmosphere_valid_color(1e100) &&
          atmosphere_valid_fog_far(180) && !atmosphere_valid_fog_far(INFINITY) &&
          atmosphere_valid_weather(10) && !atmosphere_valid_weather(10.1));
    check("untere Tile-Grenze bleibt auch nach float-Konvertierung gültig",
          terrain_valid_tile_float(static_cast<float>(TERRAIN_MIN_TILE)) &&
          !terrain_valid_tile_float(0.009f) && !terrain_valid_tile_float(INFINITY));
    check("Atlas-UV bleiben im Shader unverändert", terrain_uv_scale(ATLAS_NEUTRAL_TILE) == 1.0f &&
          std::abs(terrain_uv_scale(static_cast<float>(TERRAIN_MIN_TILE)) - 200.0f) < 0.01f &&
          terrain_uv_scale(0.0f) == 1.0f);
    check("Texturnamen vor Kopien begrenzt und ohne NUL",
          terrain_valid_texture_name(std::string(TERRAIN_MAX_TEXTURE_BYTES, 'x')) &&
          !terrain_valid_texture_name(std::string(TERRAIN_MAX_TEXTURE_BYTES + 1, 'x')) &&
          !terrain_valid_texture_name(std::string("a\0b", 3)) && !terrain_valid_texture_name(""));
    const AtlasPlan atlas_one = atlas_plan(1), atlas_max = atlas_plan(ATLAS_MAX_CELLS);
    check("Atlasplan für einzelne Textur und Obergrenze", atlas_one.ok() && atlas_one.cols == 1 &&
          atlas_one.rows == 1 && atlas_one.rgba_bytes == 16384 && atlas_max.ok() &&
          atlas_max.cols == 32 && atlas_max.rows == 32 && atlas_max.rgba_bytes == 16u * 1024u * 1024u);
    check("leerer und übergroßer Atlas vor Allokation abgewiesen", !atlas_plan(0).ok() &&
          !atlas_plan(ATLAS_MAX_CELLS + 1).ok() && !atlas_plan(std::numeric_limits<std::size_t>::max()).ok());
    const std::vector<std::pair<int, int>> sizes{{64, 64}, {128, 64}, {64, 64}};
    const std::map<std::string, int> named{{"atlas", 0}, {"gross", 1}, {"weg", 2}, {"defekt", 99}};
    const AtlasSources sources = atlas_sources(named, sizes);
    check("echte Textur atlas bleibt Eingabe; Fremdgröße und kaputte Slots fehlen",
          !sources.too_many && sources.entries.size() == 2 && sources.entries[0].first == "atlas" &&
          sources.entries[1].first == "weg");
    std::map<std::string, int> excessive;
    for (std::size_t i = 0; i <= ATLAS_MAX_CELLS; ++i) excessive["tex_" + std::to_string(i)] = 0;
    const AtlasSources limited = atlas_sources(excessive, sizes);
    check("Atlas-Eingaben werden vor der 1025. Kopie abgeschnitten",
          limited.too_many && limited.entries.size() == ATLAS_MAX_CELLS);

    std::puts("== Echtes Terrain und Höhenpinsel ==");
    Terrain t;
    check("rechteckiges Terrain erzeugen", t.generate(240, 120, 64, 7, 18) && t.ready());
    check("korrekte Vertex- und Indexzahl", t.vertices().size() == 65u * 33u && t.indices().size() == 64u * 32u * 6u);
    check("alle Meshwerte/Indizes endlich und gültig", finite_mesh(t));
    Terrain t2;
    check("deterministische Erzeugung", t2.generate(240, 120, 64, 7, 18) &&
          t.height_at(80, 40) == t2.height_at(80, 40) && t.indices() == t2.indices());
    check("riesige endliche Abfrage vor int-Cast geklemmt",
          t.height_at(std::numeric_limits<float>::max(), -std::numeric_limits<float>::max()) == t.height_at(120, -60));
    check("NaN/Inf-Abfragen sicher", t.height_at(NAN, 0) == 0 && t.height_at(INFINITY, 0) == 0 &&
          t.normal_at(NAN, 0).y == 1.0f);
    const float flat = t.height_at(0, 0);
    const std::vector<math::Vec4> one{{0, 5, 0, 8}};
    check("Höhenpinsel akzeptiert und aufgebaut", t.set_height_brushes(one) && t.generate(240, 120, 64, 7, 18));
    check("Höhenpinsel wirkt in der Mitte", t.height_at(0, 0) > flat + 4.9f && finite_mesh(t));
    std::vector<math::Vec4> outside{{1000000.0f, 2, -1000000.0f, 0.01f}};
    check("weit außerhalb liegender Pinsel ohne überlaufenden Cast",
          t.set_height_brushes(outside) && t.generate(240, 120, 64, 7, 18) && finite_mesh(t));
    std::vector<math::Vec4> invalid{{INFINITY, 2, 0, 8}};
    check("ungültiger Pinsel wird nicht übernommen", !t.set_height_brushes(invalid));
    check("ungültiger Pinsel verhindert Aufbau und entfernt altes Mesh", !t.generate(240, 120, 64, 7, 18) && empty(t));
    t.clear_height_brushes();
    check("nach explizitem Löschen wieder aufbaubar", t.generate(240, 120, 64, 7, 18));
    std::vector<math::Vec4> over_count(TERRAIN_MAX_BRUSHES + 1, math::Vec4{0, 1, 0, 1});
    check("Pinselanzahl beschränkt", !t.set_height_brushes(over_count) && !t.generate(240, 120, 64, 7, 0) && empty(t));
    std::vector<math::Vec4> costly(200, math::Vec4{0, 1, 0, 10000});
    check("Pinselwertliste gültig, aber Rechenbudget vor Allokation abgewiesen",
          t.set_height_brushes(costly) && !t.generate(240, 240, 192, 7, 0) && empty(t));
    t.clear_height_brushes();
    check("kleine Karte mit steilen Werten bleibt endlich",
          t.set_height_brushes({math::Vec4{0, 10000, 0, 1}}) &&
          t.generate(1, 1, 128, 4294967295u, 10000) && finite_mesh(t));
    check("schlechtes Format leert altes Terrain", !t.generate(0, 240, 192, 7, 0) && empty(t));
    check("Inf/NaN und INT_MAX an direkter C++-API sicher",
          !t.generate(INFINITY, 240, 192, 7, 0) && !t.generate(240, 240, 2147483647, 7, 0) &&
          !t.generate(240, 240, 192, 7, NAN) && empty(t));

    std::puts("== Splat-Mesh und Ausfallbehandlung ==");
    t.clear();
    check("Boden vor Splat-Aufbau", t.set_height_brushes(one) && t.generate(240, 120, 64, 7, 18));
    const float painted_height = t.height_at(0, 0);
    AtlasInfo atlas;
    atlas.valid = true; atlas.cols = 2; atlas.rows = 1;
    atlas.cell_of["gras"] = 0; atlas.cell_of["weg"] = 1; atlas.cell_of["ungültig"] = 2;
    check("Atlas erkennt Basis, fehlende und unzulässige Zellen",
          atlas_has_texture(atlas, "gras") && !atlas_has_texture(atlas, "fehlt") &&
          !atlas_has_texture(atlas, "ungültig") && !atlas_has_texture(AtlasInfo{}, "gras"));
    const std::vector<SplatRect> splats{{-10, -10, 20, 20, "weg"}};
    check("Splat-Mesh erzeugen", t.generate_splat(240, 120, 64, 7, 18, "gras", splats, atlas));
    check("vier Vertices pro Quad, sichere Indizes", t.vertices().size() == 64u * 32u * 4u &&
          t.indices().size() == 64u * 32u * 6u && finite_mesh(t));
    check("Splat-Dreiecke bedecken jedes Quad einmal und zeigen nach oben", splat_quads_cover_once(t));
    const std::size_t middle_vertex = (16u * 64u + 32u) * 4u;
    check("bekannte Fläche nutzt ihre Atlaszelle", t.vertices()[middle_vertex].uv[0] > 0.5f);
    check("Höhenform beim Umtexturieren erhalten", t.height_at(0, 0) == painted_height);
    check("fehlende Atlas-Basis erzeugt kein löchriges Mesh",
          !t.generate_splat(240, 120, 64, 7, 18, "fehlt", splats, atlas) && empty(t));
    const std::vector<SplatRect> unknown_splat{{-10, -10, 20, 20, "ungültig"}};
    check("ungültige Flächen-Zelle fällt auf die Basis zurück, ohne Löcher",
          t.generate_splat(240, 120, 64, 7, 18, "gras", unknown_splat, atlas) &&
          t.indices().size() == 64u * 32u * 6u && finite_mesh(t));
    const std::vector<SplatRect> overlap{{-10, -10, 20, 20, "weg"}, {-10, -10, 20, 20, "fehlt"}};
    check("letzte fehlende Flächentextur fällt auch über älterem Splat auf die Basis zurück",
          t.generate_splat(240, 120, 64, 7, 18, "gras", overlap, atlas) &&
          t.vertices()[middle_vertex].uv[0] < 0.5f);
    const std::vector<SplatRect> invalid_splat{{0, 0, INFINITY, 1, "weg"}};
    check("ungültige Splat-Werte leeren altes Terrain",
          !t.generate_splat(240, 120, 64, 7, 18, "gras", invalid_splat, atlas) && empty(t));
    std::vector<SplatRect> many_splats(40, SplatRect{0, 0, 1, 1, "weg"});
    check("Splat-Rechenbudget vor GPU-/Mesh-Allokation", terrain_check_splats(
          terrain_plan(240, 240, 512, 7, 0), many_splats) == TerrainProblem::SplatBudget &&
          !t.generate_splat(240, 240, 512, 7, 0, "gras", many_splats, atlas) && empty(t));
    t.clear();
    check("clear entfernt Terrain und Pinsel", empty(t) && t.generate(240, 120, 64, 7, 0) && t.height_at(0, 0) == 0);

    std::printf("\n%d Terrain-Checks, %d Fehler\n", count, failures);
    std::puts(failures ? "TERRAIN-TEST FEHLGESCHLAGEN" : "TERRAIN-TEST OK");
    return failures ? 1 : 0;
}
