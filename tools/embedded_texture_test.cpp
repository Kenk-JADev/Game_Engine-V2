// Windows-Integration: echtes eingebettetes CRuby, echter Materialstore und
// derselbe D3D11-Renderer auf WARP. Alle Projektdateien liegen in einer neu
// erzeugten temporären Engine-Wurzel; vorhandene Projekte bleiben unberührt.
#include "ruby/vm.h"
#include <ruby.h>

#include "app.h"
#include "core/project_assets_win.h"
#include "core/win_project_cli.h"

#include <windows.h>
#include <shellapi.h>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {
int checks = 0, failures = 0;

void check(const char* name, bool ok) {
    ++checks;
    if (!ok) ++failures;
    std::printf("  %s  %s\n", ok ? "ok  " : "FAIL", name);
    if (!ok && std::getenv("GITHUB_ACTIONS"))
        std::printf("::error title=Eingebettete Texturbrücke::%s\n", name);
}

bool ruby_true(const char* code) {
    int state = 0;
    const VALUE result = rb_eval_string_protect(code, &state);
    if (state) {
        const VALUE error = rb_errinfo();
        VALUE text = rb_obj_as_string(error);
        std::printf("  Ruby-Fehler: %s\n", StringValueCStr(text));
        rb_set_errinfo(Qnil);
        return false;
    }
    return RTEST(result);
}

std::string solid(unsigned char r, unsigned char g, unsigned char b) {
    std::string bytes(64 * 64 * 4, '\0');
    for (std::size_t i = 0; i < bytes.size(); i += 4) {
        bytes[i] = static_cast<char>(r);
        bytes[i + 1] = static_cast<char>(g);
        bytes[i + 2] = static_cast<char>(b);
        bytes[i + 3] = static_cast<char>(255);
    }
    return bytes;
}

bool write_new(const std::wstring& path, const std::string& data) {
    rune::assets::Handle h(CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                                       FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!h.valid()) return false;
    DWORD n = 0;
    return WriteFile(h.get(), data.data(), static_cast<DWORD>(data.size()), &n, nullptr) && n == data.size();
}

struct Fixture {
    std::string root, project;
    std::wstring wide, game, demo, engine, projects, a, adata, atex, amaps, b, bdata, btex;
    bool owned = false;

    bool create(const std::wstring& repo) {
        wchar_t temp[MAX_PATH + 1]{};
        const DWORD n = GetTempPathW(MAX_PATH, temp);
        if (!n || n >= MAX_PATH) return false;
        wide = std::wstring(temp, n) + L"rune-ruby-\u00e4-\u65e5-\U0001f680-" +
               std::to_wstring(GetCurrentProcessId());
        game = wide + L"\\game"; demo = game + L"\\data"; engine = game + L"\\engine";
        projects = wide + L"\\projekte";
        const std::wstring a_name = L" A_\u00e4_\u65e5_\U0001f680";
        a = projects + L"\\" + a_name; adata = a + L"\\data";
        atex = adata + L"\\textures"; amaps = adata + L"\\maps";
        b = projects + L"\\B"; bdata = b + L"\\data"; btex = bdata + L"\\textures";
        if (!rune::assets::to_utf8(wide, root) || !rune::assets::to_utf8(a_name, project) ||
            !CreateDirectoryW(wide.c_str(), nullptr)) return false;
        owned = true;
        for (const auto* d : {&game, &demo, &engine, &projects, &a, &adata, &atex, &amaps, &b, &bdata, &btex}) {
            if (!CreateDirectoryW(d->c_str(), nullptr)) return false;
        }
        for (const auto* name : {L"data.rb", L"project.rb", L"textures.rb"}) {
            if (!CopyFileW((repo + L"\\game\\engine\\" + name).c_str(),
                           (engine + L"\\" + name).c_str(), TRUE)) return false;
        }
        const std::string a_manifest = R"({"textures":[{"name":"gras","file":"rot.rgba","w":64,"h":64},{"name":"nur_a","file":"blau.rgba","w":64,"h":64}]})";
        const std::string b_manifest = R"({"textures":[{"name":"nur_b","file":"blau.rgba","w":64,"h":64}]})";
        return write_new(demo + L"\\textures.json", R"({"textures":[]})") &&
               write_new(adata + L"\\system.json", R"({"name":"Unicode-Projekt","start":{"map":"karte_\u65e5"}})") &&
               write_new(amaps + L"\\karte_\u65e5.json", R"({"name":"Unicode-Karte","objects":[]})") &&
               write_new(adata + L"\\textures.json", a_manifest) &&
               write_new(atex + L"\\rot.rgba", solid(255, 0, 0)) &&
               write_new(atex + L"\\blau.rgba", solid(0, 0, 255)) &&
               write_new(bdata + L"\\textures.json", b_manifest) &&
               write_new(btex + L"\\blau.rgba", solid(0, 0, 255));
    }

    bool cleanup() {
        if (!owned) return false;
        DeleteFileW((wide + L"\\shot.bmp").c_str());
        DeleteFileW((demo + L"\\textures.json").c_str());
        DeleteFileW((engine + L"\\data.rb").c_str());
        DeleteFileW((engine + L"\\project.rb").c_str());
        DeleteFileW((engine + L"\\textures.rb").c_str());
        DeleteFileW((adata + L"\\system.json").c_str());
        DeleteFileW((amaps + L"\\karte_\u65e5.json").c_str());
        DeleteFileW((adata + L"\\textures.json").c_str());
        DeleteFileW((atex + L"\\rot.rgba").c_str());
        DeleteFileW((atex + L"\\blau.rgba").c_str());
        DeleteFileW((bdata + L"\\textures.json").c_str());
        DeleteFileW((btex + L"\\blau.rgba").c_str());
        RemoveDirectoryW(atex.c_str()); RemoveDirectoryW(amaps.c_str());
        RemoveDirectoryW(adata.c_str()); RemoveDirectoryW(a.c_str());
        RemoveDirectoryW(btex.c_str()); RemoveDirectoryW(bdata.c_str()); RemoveDirectoryW(b.c_str());
        RemoveDirectoryW(projects.c_str()); RemoveDirectoryW(demo.c_str());
        RemoveDirectoryW(engine.c_str()); RemoveDirectoryW(game.c_str());
        const bool removed = RemoveDirectoryW(wide.c_str()) != 0;
        owned = false;
        return removed;
    }
};

// Wie der separate WARP-Test den tatsächlichen Backbuffer-Mittelpunkt prüfen.
bool pixel_is(rune::gfx::Renderer& renderer, const std::string& shot, const char* texture, int color) {
    using namespace rune::math;
    const int index = renderer.texture_index(texture);
    if (index < 0) return false;
    const Mat4 view = look_at_lh(Vec3{0, 0, -3}, Vec3{0, 0, 0}, Vec3{0, 1, 0});
    const Mat4 proj = perspective_fov_lh(PI / 3, 1.0f, 0.1f, 20.0f);
    renderer.begin_frame(mul(view, proj), Vec3{0, 0, -3});
    renderer.push_box(Vec3{}, Vec3{1, 1, 1}, Vec3{}, 0xffffff, index);
    renderer.request_screenshot(shot);
    renderer.end_frame();
    std::wstring wide_shot;
    FILE* f = nullptr;
    if (!rune::core::win_path::to_wide(shot, wide_shot) ||
        _wfopen_s(&f, wide_shot.c_str(), L"rb") != 0 || !f) return false;
    BITMAPFILEHEADER fh{};
    BITMAPINFOHEADER ih{};
    bool ok = std::fread(&fh, sizeof(fh), 1, f) == 1 && std::fread(&ih, sizeof(ih), 1, f) == 1 &&
              fh.bfType == 0x4d42 && ih.biWidth == 64 && ih.biHeight == -64 && ih.biBitCount == 32;
    unsigned char bgra[4]{};
    if (ok) ok = std::fseek(f, static_cast<long>(fh.bfOffBits + (32 * 64 + 32) * 4), SEEK_SET) == 0 &&
                 std::fread(bgra, 1, 4, f) == 4;
    std::fclose(f);
    DeleteFileW(wide_shot.c_str());
    if (!ok) return false;
    return color == 0 ? bgra[2] > 100 && bgra[1] < 20 && bgra[0] < 20 :
           color == 1 ? bgra[1] > 100 && bgra[2] < 20 && bgra[0] < 20 :
                        bgra[0] > 100 && bgra[1] < 20 && bgra[2] < 20;
}

} // namespace

int main(int argc, char** argv) {
    std::puts("== Eingebettetes Ruby: Projekttexturen und D3D11-WARP ==");
    int wide_argc = 0;
    wchar_t** wide_argv = CommandLineToArgvW(GetCommandLineW(), &wide_argc);
    if (!wide_argv || wide_argc != 2) {
        if (wide_argv) LocalFree(wide_argv);
        check("Repository-Pfad übergeben", false);
        return 1;
    }
    const std::wstring repo = wide_argv[1];
    LocalFree(wide_argv);
    Fixture f;
    if (!f.create(repo)) {
        check("isolierte temporäre Testprojekte", false);
        if (f.owned) f.cleanup();
        return 1;
    }
    check("isolierte temporäre Testprojekte inklusive Unicode-Name", true);
    HWND window = CreateWindowExW(0, L"STATIC", L"Rune Ruby Texturtest", WS_OVERLAPPEDWINDOW,
                                  0, 0, 64, 64, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    check("unsichtbares Testfenster", window != nullptr);
    rune::gfx::Renderer renderer;
    const bool ready = window && renderer.init(window, 64, 64, true);
    check("WARP-Renderer initialisiert", ready);
    bool ruby_started = false;
    if (ready) {
        renderer.set_shadows(false);
        renderer.set_clear_color(0);
        renderer.set_sun(rune::math::Vec3{0, 1, 0}, 1.0f);
        rune::app().renderer = &renderer;
        const std::string green = solid(0, 255, 0);
        const int base = renderer.add_texture("gras", 64, 64, reinterpret_cast<const std::uint8_t*>(green.data()));
        check("eingebaute Basis registriert", base >= 0);
        // Wie Rune3D.exe vor dem VM-Start setzen; Ruby liest die Wide-Umgebung.
        const bool env_set = rune::core::win_project_cli::set_project_environment(f.project);
        check("Projektname verlustfrei in Windows-Umgebung gesetzt", env_set);
        char* ruby_argv[] = {argv[0], nullptr};
        ruby_started = env_set && rune::ruby::init(1, ruby_argv, f.root);
        check("echte eingebettete Ruby-VM gestartet", ruby_started);
        if (ruby_started) {
            rb_gv_set("$expected_root", rb_utf8_str_new(f.root.data(), static_cast<long>(f.root.size())));
            rb_gv_set("$expected_project", rb_utf8_str_new(f.project.data(), static_cast<long>(f.project.size())));
            check("Rune::ROOT und RUNE_PROJECT erreichen Ruby exakt als UTF-8", ruby_true(
                "Rune::ROOT.encoding == Encoding::UTF_8 && Rune::ROOT == $expected_root && "
                "ENV['RUNE_PROJECT'] == $expected_project && File.directory?(Rune::ROOT)"));
            const bool data_loaded = rune::ruby::load_script("game\\engine\\data.rb");
            const bool textures_loaded = rune::ruby::load_script(f.root + "\\game\\engine\\textures.rb");
            const bool project_loaded = ruby_true(
                "$LOAD_PATH.unshift(File.join(Rune::ROOT, 'game', 'engine')); require 'project'; defined?(Project)");
            check("echte Database-/Textures-/Project-Module aus Unicode-Wurzel geladen",
                  data_loaded && textures_loaded && project_loaded);
            if (data_loaded && textures_loaded && project_loaded) {
                check("Ruby findet und aktualisiert den Unicode-Projektnamen exakt", ruby_true(
                    "Project.list.include?($expected_project) && Project.touch($expected_project) && "
                    "Project.read_system($expected_project)['last_played'].is_a?(String)"));
                check("Ruby liest Unicode-Karten-ID und Systemdaten", ruby_true(
                    "$project_name = ENV['RUNE_PROJECT']; $project_root = Project.data_path($project_name); Database.reset!; "
                    "Database.system.dig('start', 'map') == \"karte_\\u65e5\" && "
                    "Database.map_ids == [\"karte_\\u65e5\"] && Database.map(\"karte_\\u65e5\")['name'] == 'Unicode-Karte'"));
                check("Ruby liest Unicode-Projekt A und meldet zwei echte Texturen an", ruby_true(
                    "Textures.begin_project; Textures.register_project == 2"));
                check("A überschreibt Basis nur in der Projektebene", renderer.texture_index("gras") != base &&
                      renderer.texture_index("nur_a") >= 0 && renderer.texture_index("nur_b") < 0);
                check("A-Override über Ruby tatsächlich rot gerendert", pixel_is(renderer, f.root + "\\shot.bmp", "gras", 0));
                check("A-Textur über Ruby tatsächlich blau gerendert", pixel_is(renderer, f.root + "\\shot.bmp", "nur_a", 2));
                const std::wstring moved = f.a + L"\\data-moved";
                const bool moved_a = MoveFileW(f.adata.c_str(), moved.c_str()) != 0;
                if (moved_a) MoveFileW(moved.c_str(), f.adata.c_str());
                check("Ruby hält die geprüfte Projekt-Datenwurzel gebunden", !moved_a);
                check("fremde Datenwurzel und Pfadflucht werden vom echten Binding abgewiesen", ruby_true(
                    "Rune::Assets.read_texture(File.join(Rune::ROOT, 'projekte', 'B', 'data'), $project_name, nil, nil)[0] == :invalid && "
                    "Rune::Assets.read_texture($project_root, $project_name, '../a.rgba', 4)[0] == :invalid"));
                check("Neuladepfad leert den Cache und lädt A erneut", ruby_true("Textures.begin_project; Textures.register_project == 2") &&
                      renderer.texture_index("nur_a") >= 0 && pixel_is(renderer, f.root + "\\shot.bmp", "gras", 0));
                check("Ruby wechselt zu B", ruby_true(
                    "$project_name = 'B'; $project_root = File.join(Rune::ROOT, 'projekte', $project_name, 'data'); "
                    "Textures.begin_project; Textures.register_project == 1"));
                check("B entfernt A-Namen und stellt Basis wieder her", renderer.texture_index("nur_a") < 0 &&
                      renderer.texture_index("gras") == base && renderer.texture_index("nur_b") >= 0);
                check("B rendert eingebautes Gras grün", pixel_is(renderer, f.root + "\\shot.bmp", "gras", 1));
                check("Ruby wechselt zurück nach A", ruby_true(
                    "$project_name = ENV['RUNE_PROJECT']; $project_root = File.join(Rune::ROOT, 'projekte', $project_name, 'data'); "
                    "Textures.begin_project; Textures.register_project == 2") &&
                      renderer.texture_index("nur_b") < 0 && pixel_is(renderer, f.root + "\\shot.bmp", "gras", 0));
                check("Launcher-Ende räumt Projekttexturen, Demo-Manifest bleibt lesbar", ruby_true(
                    "Textures.end_project; $project_root = nil; $project_name = nil; Textures.read_asset[0] == :ok") &&
                      renderer.texture_index("nur_a") < 0 && renderer.texture_index("gras") == base &&
                      pixel_is(renderer, f.root + "\\shot.bmp", "gras", 1));
            }
            rune::ruby::shutdown();
        }
        rune::app().renderer = nullptr;
    }
    renderer.shutdown();
    if (window) DestroyWindow(window);
    check("nur eigene Testdateien vollständig entfernt", f.cleanup());
    std::printf("EINGEBETTETE-TEXTURBRÜCKE: %d Prüfungen, %d Fehler\n", checks, failures);
    if (std::getenv("GITHUB_ACTIONS"))
        std::printf("::notice title=Eingebettete Texturbrücke::%d Prüfungen, %d Fehler\n", checks, failures);
    return failures ? 1 : 0;
}
