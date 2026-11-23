// Windows-only: echter D3D11-Renderer auf WARP (Software), unsichtbares
// Fenster, echte Shader/Textures/Atlas und Backbuffer-Pixel. Kein GPU-/GUI-
// Live-Test und kein Ersatz für die manuelle Prüfung auf dem Zielrechner.
#include "gfx/renderer.h"
#include "core/log.h"

#include <windows.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace rune::core {
void log_info(const std::string& msg) { std::printf("  ... %s\n", msg.c_str()); }
void log_warn(const std::string& msg) { std::printf("  WARN %s\n", msg.c_str()); }
void log_error(const std::string& msg) { std::printf("  ERROR %s\n", msg.c_str()); }
}

namespace {
int failures = 0, checks = 0;
void check(const char* message, bool ok) {
    ++checks;
    if (!ok) ++failures;
    std::printf("  %s  %s\n", ok ? "ok  " : "FAIL", message);
}

std::vector<std::uint8_t> solid(std::uint8_t r, std::uint8_t g, std::uint8_t b) {
    std::vector<std::uint8_t> pixels(64 * 64 * 4);
    for (std::size_t i = 0; i < pixels.size(); i += 4) {
        pixels[i] = r; pixels[i + 1] = g; pixels[i + 2] = b; pixels[i + 3] = 255;
    }
    return pixels;
}

void begin_test_frame(rune::gfx::Renderer& renderer) {
    using namespace rune::math;
    const Mat4 view = look_at_lh(Vec3{0, 0, -3}, Vec3{0, 0, 0}, Vec3{0, 1, 0});
    const Mat4 proj = perspective_fov_lh(PI / 3, 1.0f, 0.1f, 20.0f);
    renderer.begin_frame(mul(view, proj), Vec3{0, 0, -3});
}

// color: 0 rot, 1 grün, 2 blau, 3 schwarz (gelöschtes Terrain).
bool finish_and_sample(rune::gfx::Renderer& renderer, const std::string& shot, int color) {
    DeleteFileA(shot.c_str());
    renderer.request_screenshot(shot);
    renderer.end_frame();
    std::FILE* f = std::fopen(shot.c_str(), "rb");
    if (!f) return false;
    BITMAPFILEHEADER fh{};
    BITMAPINFOHEADER ih{};
    bool ok = std::fread(&fh, sizeof(fh), 1, f) == 1 && std::fread(&ih, sizeof(ih), 1, f) == 1 &&
              fh.bfType == 0x4d42 && ih.biWidth == 64 && ih.biHeight == -64 && ih.biBitCount == 32;
    unsigned char bgra[4]{};
    if (ok) ok = std::fseek(f, static_cast<long>(fh.bfOffBits + (32 * 64 + 32) * 4), SEEK_SET) == 0 &&
                 std::fread(bgra, 1, 4, f) == 4;
    std::fclose(f);
    DeleteFileA(shot.c_str());
    if (!ok) return false;
    const bool matches = color == 0 ? bgra[2] > 100 && bgra[1] < 20 && bgra[0] < 20 :
                         color == 1 ? bgra[1] > 100 && bgra[2] < 20 && bgra[0] < 20 :
                         color == 2 ? bgra[0] > 100 && bgra[2] < 20 && bgra[1] < 20 :
                                      bgra[0] < 20 && bgra[1] < 20 && bgra[2] < 20;
    if (!matches) std::printf("  ... Pixel RGB=(%u,%u,%u), erwartet Farbe %d\n",
                              bgra[2], bgra[1], bgra[0], color);
    return matches;
}

bool pixel_is(rune::gfx::Renderer& renderer, const char* name, const std::string& shot, int color) {
    using namespace rune::math;
    const int index = renderer.texture_index(name);
    if (index < 0) return false;
    begin_test_frame(renderer);
    renderer.push_box(Vec3{}, Vec3{1, 1, 1}, Vec3{}, 0xffffff, index);
    return finish_and_sample(renderer, shot, color);
}

// Ein echtes Terrain-Mesh benutzt den internen Atlas-SRV und UVs auf die
// Zellmitte. Das Bild beweist sowohl Zuordnung als auch GPU-Ressourcenbindung.
bool terrain_pixel_is(rune::gfx::Renderer& renderer, const rune::world::AtlasInfo& atlas,
                      const std::string& name, const std::string& shot, int color) {
    if (!rune::world::atlas_has_texture(atlas, name)) return false;
    const int cell = atlas.cell_of.at(name);
    const float u = (static_cast<float>(cell % atlas.cols) + 0.5f) / atlas.cols;
    const float v = (static_cast<float>(cell / atlas.cols) + 0.5f) / atlas.rows;
    using rune::gfx::MeshVertex;
    const std::vector<MeshVertex> verts = {
        {{ 0.5f, -0.5f, 0}, {0, 0, -1}, {1, 1, 1}, {u, v}},
        {{-0.5f, -0.5f, 0}, {0, 0, -1}, {1, 1, 1}, {u, v}},
        {{-0.5f,  0.5f, 0}, {0, 0, -1}, {1, 1, 1}, {u, v}},
        {{ 0.5f,  0.5f, 0}, {0, 0, -1}, {1, 1, 1}, {u, v}}
    };
    if (!renderer.set_terrain(verts, {0, 1, 2, 0, 2, 3})) return false;
    renderer.set_terrain_tex(atlas.tex_index);
    renderer.set_terrain_tile(rune::world::ATLAS_NEUTRAL_TILE);
    begin_test_frame(renderer);
    return finish_and_sample(renderer, shot, color);
}

bool empty_terrain_frame(rune::gfx::Renderer& renderer, const std::string& shot) {
    begin_test_frame(renderer);
    return finish_and_sample(renderer, shot, 3);
}

} // namespace

int main() {
    std::puts("== D3D11-WARP: Renderer und Projekttexturen ==");
    HWND window = CreateWindowExW(0, L"STATIC", L"Rune WARP Test", WS_OVERLAPPEDWINDOW,
                                  0, 0, 64, 64, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    check("unsichtbares Testfenster", window != nullptr);
    if (!window) return 1;
    rune::gfx::Renderer renderer;
    const bool ready = renderer.init(window, 64, 64, true);
    check("D3D11 WARP, Swapchain und echte Pipeline initialisiert", ready);
    if (!ready) { renderer.shutdown(); DestroyWindow(window); return 1; }
    renderer.set_shadows(false);
    renderer.set_clear_color(0);
    renderer.set_sun(rune::math::Vec3{0, 1, 0}, 1.0f);
    char temp[MAX_PATH + 1]{};
    const DWORD n = GetTempPathA(MAX_PATH, temp);
    const std::string shot = n && n < MAX_PATH
        ? std::string(temp, n) + "rune-warp-" + std::to_string(GetCurrentProcessId()) + ".bmp"
        : "rune-warp-test.bmp";

    const auto green = solid(0, 255, 0);
    const auto red = solid(255, 0, 0);
    const auto blue = solid(0, 0, 255);
    const int base = renderer.add_texture("gras", 64, 64, green.data());
    check("Basistextur registriert", base >= 0);
    check("Basistextur wirklich grün gerendert", pixel_is(renderer, "gras", shot, 1));
    const auto base_atlas = renderer.update_atlas();
    check("Basisatlas hat privaten Slot", base_atlas.valid && base_atlas.tex_index != base &&
          base_atlas.cell_of.count("gras") == 1);
    check("Basis-Atlaszelle wird auf Terrain grün gerendert", terrain_pixel_is(renderer, base_atlas, "gras", shot, 1));

    renderer.begin_project_textures();
    check("Projektbeginn räumt alte Terrain-Referenz ab", empty_terrain_frame(renderer, shot));
    const int red_slot = renderer.add_texture("gras", 64, 64, red.data());
    const int a_slot = renderer.add_texture("nur_a", 64, 64, blue.data());
    const int named_atlas = renderer.add_texture("atlas", 64, 64, red.data());
    check("Projekt-Override liegt getrennt von Basis", red_slot >= 0 && red_slot != base && a_slot >= 0 && named_atlas >= 0);
    check("Projekt-Override wirklich rot gerendert", pixel_is(renderer, "gras", shot, 0));
    const auto atlas_a = renderer.update_atlas();
    check("Projektname atlas kollidiert nicht mit internem Atlas", atlas_a.valid && atlas_a.tex_index != named_atlas &&
          atlas_a.cell_of.count("atlas") == 1 && atlas_a.cell_of.count("nur_a") == 1);
    std::vector<std::uint8_t> read;
    int w = 0, h = 0;
    check("öffentliche atlas-Pixel nicht vom internen Atlas ersetzt", renderer.texture_rgba("atlas", read, w, h) && read == red);
    check("Atlaszelle des Gras-Overrides ist auf Terrain rot", terrain_pixel_is(renderer, atlas_a, "gras", shot, 0));
    check("benachbarte Atlaszelle ist auf Terrain blau", terrain_pixel_is(renderer, atlas_a, "nur_a", shot, 2));
    const std::size_t highwater = renderer.texture_storage_slots();

    renderer.push_box({}, {1, 1, 1}, {}, 0xffffff, a_slot);
    check("alter Draw ist für Wechsel vorgemerkt", renderer.draw_count() > 0);
    renderer.begin_project_textures();
    check("A → B verwirft alte Draw- und Atlas-Terrain-Referenzen", renderer.draw_count() == 0 && empty_terrain_frame(renderer, shot));
    check("A → B entfernt fremde Namen und stellt Basis her", renderer.texture_index("nur_a") < 0 &&
          renderer.texture_index("atlas") < 0 && renderer.texture_index("gras") == base);
    const auto names_b = renderer.texture_names();
    check("F7-Liste enthält nach Wechsel keine A-Namen und gras genau einmal",
          std::find(names_b.begin(), names_b.end(), "nur_a") == names_b.end() &&
          std::find(names_b.begin(), names_b.end(), "atlas") == names_b.end() &&
          std::count(names_b.begin(), names_b.end(), "gras") == 1);
    check("nach A → B wieder grün auf WARP", pixel_is(renderer, "gras", shot, 1));
    check("B-Textur angelegt", renderer.add_texture("nur_b", 64, 64, blue.data()) >= 0);
    const auto atlas_b = renderer.update_atlas();
    check("Atlas nach Wechsel ohne alte A-Namen", atlas_b.valid && atlas_b.cell_of.count("nur_b") == 1 &&
          atlas_b.cell_of.count("nur_a") == 0 && atlas_b.cell_of.count("atlas") == 0);
    check("B-Atlas referenziert wieder grünes Basis-Gras", terrain_pixel_is(renderer, atlas_b, "gras", shot, 1));
    renderer.begin_project_textures();
    check("B → A entfernt B und altes Terrain", renderer.texture_index("nur_b") < 0 && empty_terrain_frame(renderer, shot));
    check("A erneut laden", renderer.add_texture("gras", 64, 64, red.data()) >= 0 &&
          renderer.add_texture("nur_a", 64, 64, blue.data()) >= 0 && pixel_is(renderer, "gras", shot, 0));
    const auto atlas_a_again = renderer.update_atlas();
    check("A-Atlas erneut rot", terrain_pixel_is(renderer, atlas_a_again, "gras", shot, 0));
    check("Löschen eines Overrides entfernt veraltetes Atlas-Terrain", renderer.remove_project_texture("gras") &&
          renderer.texture_index("gras") == base && empty_terrain_frame(renderer, shot));
    check("nach Entfernen wieder Basis-Gras gerendert", pixel_is(renderer, "gras", shot, 1));
    check("unbekannter Projektname nicht löschbar", !renderer.remove_project_texture("fehlt"));
    const auto atlas_before_edit = renderer.update_atlas();
    check("Atlas vor Neuanmeldung aufgebaut", terrain_pixel_is(renderer, atlas_before_edit, "gras", shot, 1));
    check("Neuanmeldung räumt veraltete Atlas-UV ab", renderer.add_texture("nur_a", 64, 64, red.data()) >= 0 &&
          empty_terrain_frame(renderer, shot));

    bool bounded = true;
    for (int i = 0; i < 20; ++i) {
        renderer.begin_project_textures();
        bounded = bounded && renderer.add_texture("gras", 64, 64, red.data()) >= 0 &&
                  renderer.add_texture("nur_a", 64, 64, blue.data()) >= 0 && renderer.update_atlas().valid &&
                  renderer.texture_storage_slots() <= highwater;
    }
    check("20 Projektwechsel/F5 ohne Slotwachstum", bounded);
    const std::size_t resident = renderer.texture_resident_bytes();
    for (int i = 0; i < 20; ++i) {
        const auto& color = i % 2 ? red : blue;
        bounded = bounded && renderer.add_texture("nur_a", 64, 64, color.data()) >= 0 && renderer.update_atlas().valid &&
                  renderer.texture_storage_slots() <= highwater && renderer.texture_resident_bytes() == resident;
    }
    check("gleichnamige Neuanmeldung ersetzt statt Speicher zu häufen", bounded);
    check("überdimensionierte Textur vor GPU-Allokation abgewiesen", renderer.add_texture("riesig", 4097, 1, red.data()) < 0);
    renderer.end_project_textures();
    check("Launcher: nur Basis bleibt, Atlas/Projekt freigegeben", renderer.texture_index("nur_a") < 0 &&
          renderer.texture_index("gras") == base && renderer.texture_resident_bytes() == green.size());
    check("Launcher-Basis wirklich grün gerendert", pixel_is(renderer, "gras", shot, 1));
    renderer.shutdown();
    DestroyWindow(window);
    std::printf("WARP-TEST: %d Prüfungen, %d Fehler\n", checks, failures);
    return failures ? 1 : 0;
}
