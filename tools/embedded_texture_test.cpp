// Windows-Integration: echtes eingebettetes CRuby, echter Materialstore und
// derselbe D3D11-Renderer auf WARP. Alle Projektdateien liegen in einer neu
// erzeugten temporären Engine-Wurzel; vorhandene Projekte bleiben unberührt.
#include "ruby/vm.h"
#include <ruby.h>

#include "app.h"
#include "core/project_assets_win.h"
#include "core/win_project_cli.h"
#include "editor/document_store_win.h"

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
        const std::string message = StringValueCStr(text);
        std::printf("  Ruby-Fehler: %s\n", message.c_str());
        if (std::getenv("GITHUB_ACTIONS")) {
            std::string escaped;
            for (char c : message) {
                if (c == '%') escaped += "%25";
                else if (c == '\r') escaped += "%0D";
                else if (c == '\n') escaped += "%0A";
                else escaped += c;
            }
            std::printf("::error title=Ruby-Ausnahme::%s\n", escaped.c_str());
        }
        rb_set_errinfo(Qnil);
        return false;
    }
    return RTEST(result);
}

struct TouchRace {
    bool invoked = false;
    bool skipped = false;
};

// Der Editor hält die Systemdatei in DocumentStore::save bereits gesperrt.
// Der Ruby-Start darf dann weder aus einem älteren Stand zurückschreiben noch
// warten oder das Spiel wegen eines optionalen Zeitstempels abbrechen.
void touch_while_editor_locked(void* context) noexcept {
    auto& race = *static_cast<TouchRace*>(context);
    race.invoked = true;
    try { race.skipped = ruby_true("!Project.touch($expected_project)"); }
    catch (...) { race.skipped = false; }
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
    std::wstring wide, game, demo, engine, editor, projects, a, adata, atex, amaps, b, bdata, btex;
    bool owned = false;

    bool create(const std::wstring& repo) {
        wchar_t temp[MAX_PATH + 1]{};
        const DWORD n = GetTempPathW(MAX_PATH, temp);
        if (!n || n >= MAX_PATH) return false;
        wide = std::wstring(temp, n) + L"rune-ruby-\u00e4-\u65e5-\U0001f680-" +
               std::to_wstring(GetCurrentProcessId());
        game = wide + L"\\game"; demo = game + L"\\data"; engine = game + L"\\engine";
        editor = game + L"\\editor"; projects = wide + L"\\projekte";
        const std::wstring a_name = L" A_\u00e4_\u65e5_\U0001f680";
        a = projects + L"\\" + a_name; adata = a + L"\\data";
        atex = adata + L"\\textures"; amaps = adata + L"\\maps";
        b = projects + L"\\B"; bdata = b + L"\\data"; btex = bdata + L"\\textures";
        if (!rune::assets::to_utf8(wide, root) || !rune::assets::to_utf8(a_name, project) ||
            !CreateDirectoryW(wide.c_str(), nullptr)) return false;
        owned = true;
        for (const auto* d : {&game, &demo, &engine, &editor, &projects, &a, &adata, &atex, &amaps, &b, &bdata, &btex}) {
            if (!CreateDirectoryW(d->c_str(), nullptr)) return false;
        }
        for (const auto* name : {L"data.rb", L"project.rb", L"textures.rb", L"sounds.rb"}) {
            if (!CopyFileW((repo + L"\\game\\engine\\" + name).c_str(),
                           (engine + L"\\" + name).c_str(), TRUE)) return false;
        }
        if (!CopyFileW((repo + L"\\game\\editor\\boot.rb").c_str(),
                       (editor + L"\\boot.rb").c_str(), TRUE)) return false;
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
        DeleteFileW((engine + L"\\sounds.rb").c_str());
        DeleteFileW((editor + L"\\boot.rb").c_str());
        DeleteFileW((adata + L"\\system.json").c_str());
        DeleteFileW((a + L"\\saves\\slot1.json").c_str());
        DeleteFileW((amaps + L"\\karte_\u65e5.json").c_str());
        DeleteFileW((adata + L"\\textures.json").c_str());
        DeleteFileW((atex + L"\\rot.rgba").c_str());
        DeleteFileW((atex + L"\\blau.rgba").c_str());
        DeleteFileW((bdata + L"\\textures.json").c_str());
        DeleteFileW((btex + L"\\blau.rgba").c_str());
        RemoveDirectoryW(atex.c_str()); RemoveDirectoryW(amaps.c_str());
        RemoveDirectoryW((a + L"\\saves").c_str());
        RemoveDirectoryW(adata.c_str()); RemoveDirectoryW(a.c_str());
        RemoveDirectoryW(btex.c_str()); RemoveDirectoryW(bdata.c_str()); RemoveDirectoryW(b.c_str());
        RemoveDirectoryW(projects.c_str()); RemoveDirectoryW(demo.c_str());
        RemoveDirectoryW(editor.c_str()); RemoveDirectoryW(engine.c_str()); RemoveDirectoryW(game.c_str());
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
    const std::wstring mode = wide_argv && wide_argc == 3 ? wide_argv[2] : L"";
    const bool expect_incomplete = mode == L"--expect-incomplete";
    const bool expect_foreign = mode == L"--expect-foreign";
    if (!wide_argv || (wide_argc != 2 && !(wide_argc == 3 && (expect_incomplete || expect_foreign)))) {
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
        // Wie Rune3D.exe vor dem VM-Start setzen; Ruby liest die Wide-Umgebung.
        const bool env_set = rune::core::win_project_cli::set_project_environment(f.project);
        check("Projektname verlustfrei in Windows-Umgebung gesetzt", env_set);
        std::string expected_map;
        const bool map_env_set = rune::core::win_path::to_utf8(L" Karte_\u65e5_\U0001f680 ", expected_map) &&
            rune::core::win_project_cli::set_start_map_environment(expected_map);
        check("Karten-ID verlustfrei in Windows-Umgebung gesetzt", map_env_set);
        char* ruby_argv[] = {argv[0], nullptr};
        ruby_started = env_set && map_env_set && rune::ruby::init(1, ruby_argv, f.root);
        check(expect_incomplete ? "unvollständiges Paket abgelehnt" : "echte eingebettete Ruby-VM gestartet",
              expect_incomplete ? !ruby_started && env_set && map_env_set : ruby_started);
        if (ruby_started && !expect_incomplete) {
            std::string syntax_message;
            rb_gv_set("$syntax_probe", Qfalse);
            check("RGSS-Syntaxprüfung kompiliert, ohne Ruby-Code auszuführen",
                  rune::ruby::check_syntax("$syntax_probe = true\nclass Probe; end\n", syntax_message) &&
                  ruby_true("$syntax_probe == false && !defined?(Probe)"));
            check("Syntaxfehler nennt die Editor-Datei und setzt Ruby-Fehlerzustand zurück",
                  !rune::ruby::check_syntax("x = 1\nif\n", syntax_message) &&
                  syntax_message.find("F11:") != std::string::npos && NIL_P(rb_errinfo()));
            check("Syntaxprüfung weist NUL im Code ab",
                  !rune::ruby::check_syntax(std::string("a\0b", 3), syntax_message));
            rb_gv_set("$expected_root", rb_utf8_str_new(f.root.data(), static_cast<long>(f.root.size())));
            rb_gv_set("$expected_project", rb_utf8_str_new(f.project.data(), static_cast<long>(f.project.size())));
            rb_gv_set("$expected_map", rb_utf8_str_new(expected_map.data(), static_cast<long>(expected_map.size())));
            check("eingebettetes Ruby initialisiert externe und Dateisystem-Kodierung als UTF-8", ruby_true(
                "Encoding.default_external == Encoding::UTF_8 && Encoding.find('filesystem') == Encoding::UTF_8"));
            check("Rune::ROOT und RUNE_PROJECT erreichen Ruby exakt als UTF-8", ruby_true(
                "Rune::ROOT.encoding == Encoding::UTF_8 && Rune::ROOT == $expected_root && "
                "ENV['RUNE_PROJECT'] == $expected_project && File.directory?(Rune::ROOT)"));
            check("RUNE_PLAYTEST_MAP erreicht eingebettetes Ruby samt Unicode und Leerzeichen", ruby_true(
                "ENV['RUNE_PLAYTEST_MAP'] == $expected_map && ENV['RUNE_PLAYTEST_MAP'].encoding == Encoding::UTF_8"));
            check("Ruby übernimmt und löscht die einmalige Kartenumgebung exakt", ruby_true(
                "ENV.delete('RUNE_PLAYTEST_MAP') == $expected_map && !ENV.key?('RUNE_PLAYTEST_MAP')"));

            // Der beim Nutzer fehlgeschlagene echte Editor-Boot läuft hier
            // aus der Kopie mit den echten Bindings und WARP, nicht als Stub.
            const bool boot_loaded = rune::ruby::load_script("game\\editor\\boot.rb");
            check("echter Editor-Boot lädt Texturen und Sounds", boot_loaded &&
                  renderer.texture_index("gras") >= 0 && renderer.texture_index("dach") >= 0);
            check("JSON-Parser, Generator und fileutils funktionieren", ruby_true(
                "require 'json'; require 'fileutils'; "
                "JSON.parse(JSON.generate({'zahl' => 9007199254740993}))['zahl'] == 9007199254740993 && "
                "defined?(FileUtils)"));
            std::wstring exe_dir;
            std::string runtime_root;
            const bool runtime_path = rune::core::win_path::executable_directory(exe_dir) &&
                rune::core::win_path::to_utf8(exe_dir + L"\\lib\\ruby", runtime_root);
            if (runtime_path)
                rb_gv_set("$test_runtime_root", rb_utf8_str_new(runtime_root.data(), static_cast<long>(runtime_root.size())));
            check("JSON und fileutils stammen aus der kopierten Standardbibliothek", runtime_path && ruby_true(
                "prefix = File.expand_path($test_runtime_root).tr(92.chr, '/').downcase + '/'; "
                "files = $LOADED_FEATURES.filter_map { |entry| "
                "p = entry.dup.force_encoding(Encoding::UTF_8); "
                "File.expand_path(p).tr(92.chr, '/').downcase if p.valid_encoding? }; "
                "%w[json.rb json/ext/parser.so json/ext/generator.so fileutils.rb].all? { |part| "
                "files.any? { |p| p.start_with?(prefix) && p.end_with?('/' + part) } }"));
            check(expect_foreign ? "zusätzliches RUBYLIB bleibt erhalten" : "kein RUBYLIB für den isolierten Start",
                  expect_foreign ? ruby_true(
                "p = ENV['RUNE_TEST_RUBYLIB']; p && !p.empty? && $LOAD_PATH.any? { |entry| "
                "s = entry.dup.force_encoding(Encoding::UTF_8); s.valid_encoding? && "
                "File.expand_path(s).tr(92.chr, '/').downcase == File.expand_path(p).tr(92.chr, '/').downcase }") :
                  ruby_true("[ENV['RUBYLIB'], ENV['RUNE_TEST_RUBYLIB']].all? { |p| !p || p.empty? }"));

            // Die weitere Texturprüfung nutzt eine bewusst grüne Basis, damit
            // A/B-Overrides eindeutig über echte Pixel geprüft werden können.
            const std::string green = solid(0, 255, 0);
            const int base = renderer.add_texture("gras", 64, 64, reinterpret_cast<const std::uint8_t*>(green.data()));
            check("eingebaute Basis registriert", base >= 0);
            const bool data_loaded = rune::ruby::load_script("game\\engine\\data.rb");
            const bool textures_loaded = ruby_true("defined?(Textures)");
            const bool project_loaded = ruby_true(
                "$LOAD_PATH.unshift(File.join(Rune::ROOT, 'game', 'engine')); require 'project'; defined?(Project)");
            check("echte Database-/Textures-/Project-Module aus Unicode-Wurzel geladen",
                  data_loaded && textures_loaded && project_loaded);
            if (data_loaded && textures_loaded && project_loaded) {
                check("Ruby-Verzeichnissuche liefert den Unicode-Projektnamen exakt als UTF-8", ruby_true(
                    "names = Project.list; names == [$expected_project] && names[0].encoding == Encoding::UTF_8"));
                check("Ruby aktualisiert Systemdaten des Unicode-Projekts", ruby_true(
                    "Project.touch($expected_project) && Project.read_system($expected_project)['last_played'].is_a?(String)"));
                {
                    rune::editor::DocumentStore store;
                    rune::editor::DocumentSnapshot snapshot;
                    rune::editor::DocumentRecovery recovery;
                    TouchRace race;
                    rune::editor::DocumentTestHooks hooks{};
                    hooks.after_compare = touch_while_editor_locked;
                    hooks.context = &race;
                    auto result = store.open(f.root, f.project, false, "system.json");
                    if (result == rune::assets::Result::Ok) result = store.observe(snapshot);
                    if (result == rune::assets::Result::Ok && snapshot.present)
                        result = store.save(snapshot, snapshot.bytes, recovery, &hooks);
                    check("Ruby-Zeitstempel überspringt die vom Editor gesperrte Systemdatei",
                          result == rune::assets::Result::Ok && race.invoked && race.skipped &&
                          ruby_true("Project.read_system($expected_project).dig('start', 'map') == \"karte_\\u65e5\""));
                }
                {
                    const bool held = ruby_true(
                        "$touch_test_file = File.open(File.join(Project.data_path($expected_project), 'system.json'), 'r+b'); "
                        "$touch_test_file.flock(File::LOCK_EX | File::LOCK_NB)");
                    rune::editor::DocumentStore store;
                    rune::editor::DocumentSnapshot snapshot;
                    auto result = store.open(f.root, f.project, false, "system.json");
                    if (result == rune::assets::Result::Ok) result = store.observe(snapshot);
                    const bool released = ruby_true("$touch_test_file.close if $touch_test_file; $touch_test_file = nil; true");
                    check("Editor beobachtet kein system.json während Ruby es zum Schreiben hält",
                          held && result != rune::assets::Result::Ok && released);
                }
                check("Ruby schreibt und liest Spielstand im Unicode-Projekt", ruby_true(
                    "Project.save($expected_project, 1, {'map' => \"karte_\\u65e5\"}); "
                    "Project.load_save($expected_project, 1)['map'] == \"karte_\\u65e5\""));
                check("Ruby liest Startkarten-ID aus Unicode-Projekt", ruby_true(
                    "$project_name = ENV['RUNE_PROJECT']; $project_root = Project.data_path($project_name); Database.reset!; "
                    "Database.system.dig('start', 'map') == \"karte_\\u65e5\""));
                check("Ruby-Verzeichnissuche liefert Unicode-Karten-ID exakt als UTF-8", ruby_true(
                    "ids = Database.map_ids; ids == [\"karte_\\u65e5\"] && ids[0].encoding == Encoding::UTF_8 && "
                    "Project.info($expected_project)['maps'] == 1"));
                check("Ruby liest Datei mit Unicode-Karten-ID", ruby_true(
                    "Database.map(\"karte_\\u65e5\")['name'] == 'Unicode-Karte'"));
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
        }
        if (ruby_started) rune::ruby::shutdown();
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
