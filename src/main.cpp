#include <windows.h>

#include <cstdio>
#include <string>

#include "app.h"

#include <algorithm>
#include <cmath>

namespace {

// Freie Sichtlinie vom Spielerkopf zur Third-Person-Kamera: Ray-March in
// 32 Schritten; Terrain (Höhe + Rand) und alle festen Hindernisse (um
// 0.3 m aufgeblasen) blocken. Rückgabe: erlaubter Abstand, oder -1 = frei.
// lift (Ausgabe): > 0, wenn die Kamera lieber über das Hindernis gehoben
// wird (niedrige Zaune, Brunnen) statt ganz ranzuziehen.
float camera_clearance(rune::world::Terrain& terrain,
                       const rune::world::Physics& physics,
                       const rune::gfx::Camera& camera,
                       float& lift) {
    using rune::math::Vec3;
    lift = 0.0f;

    const Vec3 head{camera.target.x, camera.target.y + camera.eye_height, camera.target.z};
    const Vec3 fwd = camera.forward();
    const Vec3 desired = rune::math::sub(
        Vec3{camera.target.x, camera.target.y + camera.eye_height + 0.6f, camera.target.z},
        rune::math::scale(fwd, camera.dist));

    const Vec3 delta = rune::math::sub(desired, head);
    const float len = std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
    if (len < 0.01f) return -1.0f;

    const Vec3 dir = rune::math::scale(delta, 1.0f / len);
    const int steps = 32;
    const float pad = 0.3f;

    const auto blocked = [&](const Vec3& p) {
        if (terrain.ready() && p.y < terrain.height_at(p.x, p.z) + 0.3f) return true;
        for (const rune::world::Aabb& s : physics.solids()) {
            if (p.x > s.min.x - pad && p.x < s.max.x + pad &&
                p.y > s.min.y - pad && p.y < s.max.y + pad &&
                p.z > s.min.z - pad && p.z < s.max.z + pad) {
                return true;
            }
        }
        return false;
    };

    int hit = -1;
    for (int i = 1; i <= steps; ++i) {
        const float t = len * static_cast<float>(i) / static_cast<float>(steps);
        if (blocked(rune::math::add(head, rune::math::scale(dir, t)))) {
            hit = i;
            break;
        }
    }
    if (hit < 0) return -1.0f;

    const float t = len * static_cast<float>(hit) / static_cast<float>(steps);
    float d = t - pad;
    if (d < 1.2f) {
        // Sehr nah dran: lieber über das Hindernis heben (Zaun, Brunnen ...).
        for (float l = 0.5f; l <= 2.5f; l += 0.5f) {
            if (!blocked(Vec3{desired.x, desired.y + l, desired.z})) {
                lift = l;
                return -1.0f;   // volle Distanz, nur höher
            }
        }
        d = 1.2f;
    }
    return std::max(0.8f, d);
}

} // namespace
#include "core/log.h"
#include "core/sound.h"
#include "core/win_path.h"
#include "core/win_project_cli.h"
#include "core/window.h"
#include "gfx/camera.h"
#include "gfx/renderer.h"
#include "ruby/vm.h"
#include "world/physics.h"
#include "world/terrain.h"

namespace {

// Spiel bevorzugt das aktuelle Arbeitsverzeichnis, falls es game/ enthält.
// Die gemeinsame Suche nutzt Windows-UTF-16 und liefert intern UTF-8.
std::string find_root() {
    std::string root;
    rune::core::win_path::find_engine_root(true, root);
    return root;
}

double qpc_now() {
    static LARGE_INTEGER freq = [] {
        LARGE_INTEGER f{};
        QueryPerformanceFrequency(&f);
        return f;
    }();
    LARGE_INTEGER now{};
    QueryPerformanceCounter(&now);
    return static_cast<double>(now.QuadPart) / static_cast<double>(freq.QuadPart);
}

} // namespace

int main(int argc, char** argv) {
    const std::string root = find_root();
    if (root.empty() || !rune::core::win_path::set_current_directory(root)) {
        std::fprintf(stderr, "FEHLER: Engine-Wurzel nicht zugänglich\n");
        return 1;
    }
    CreateDirectoryW(L"log", nullptr);

    // Etappe 18a: scharfe Darstellung auf skalierten Displays.
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    rune::core::log_open("log\\engine.log");
    rune::core::log_info("=== Rune3D startet ===");
    rune::core::log_info("Projektwurzel: " + root);

    // Etappe 19f: Daten-Check mit klarem Hinweis. Ohne game/-Ordner neben der
    // EXE (oder eine Ebene höher) fehlen boot.rb, prefabs.json und Projekte;
    // die Folgefehler (LoadError/„nicht gefunden") sind sonst schwer zu deuten
    // - genau das passierte mit einem Artefakt-Download, der nur EXEs enthielt.
    if (!rune::core::win_path::path_exists(root + "\\game")) {
        rune::core::log_error(
            "Engine-Daten fehlen: kein game/-Ordner unter der Projektwurzel. "
            "Bitte den KOMPLETTEN Build entpacken (EXEs + game/ + projekte/ "
            "in einen Ordner) - einzelne EXEs finden ihre Daten nicht.");
    }

    // Playtest: Rune3D.exe --project <name> [--map <karten-id>].
    // Die UTF-16-Argumente gelangen verlustfrei in die Ruby-Umgebung;
    // ohne --map wird ein möglicherweise geerbter alter Kartenwert gelöscht.
    rune::core::win_project_cli::GameArguments launch;
    if (!rune::core::win_project_cli::current_game(launch) ||
        !rune::core::win_project_cli::apply_game_environment(launch)) {
        rune::core::log_error("Ungültige --project-/--map-Argumente oder Umgebung");
        return 1;
    }
    if (launch.has_project) rune::core::log_info("Projekt-Argument: " + launch.project);
    if (launch.has_map) rune::core::log_info("Playtest-Karte: " + launch.map);

    rune::App& app = rune::app();

    rune::core::Window window;
    if (!window.create(L"Rune3D - RPG Engine", 1280, 720)) {
        rune::core::log_error("Fenster konnte nicht erstellt werden");
        return 1;
    }

    rune::gfx::Renderer renderer;
    if (!renderer.init(window.hwnd(), window.width(), window.height())) {
        rune::core::log_error("Renderer konnte nicht initialisiert werden");
        return 1;
    }

    rune::gfx::Camera camera;
    rune::world::Terrain terrain;
    rune::world::Physics physics;

    if (!rune::core::sound::init()) {
        rune::core::log_warn("Starte ohne Sound weiter");
    }

    app.window = &window;
    app.renderer = &renderer;
    app.camera = &camera;
    app.terrain = &terrain;
    app.physics = &physics;

    if (!rune::ruby::init(argc, argv, root)) {
        rune::core::log_error("Ruby konnte nicht gestartet werden");
        return 1;
    }

    if (!rune::ruby::load_script("game\\engine\\boot.rb")) {
        rune::core::log_error("Bootstrap fehlgeschlagen - starte ohne Spiel weiter");
    }

    rune::core::log_info("Hauptschleife läuft");
    rune::core::log_info("F1 = First-Person, F3 = Third-Person, F2 = Debug, F5 = Ruby neu laden");

    double last = qpc_now();
    double fps_accum = 0.0;
    int fps_frames = 0;

    while (true) {
        if (!window.pump()) break;

        const double now = qpc_now();
        float dt = static_cast<float>(now - last);
        last = now;
        if (dt > 0.1f) dt = 0.1f;
        if (dt < 0.0f) dt = 0.0f;

        rune::core::InputState& in = window.input();

        // Engine-Tasten
        if (in.pressed(VK_F1)) {
            camera.mode = rune::gfx::CameraMode::FirstPerson;
            rune::core::log_info("Kamera: First-Person");
        }
        if (in.pressed(VK_F3)) {
            camera.mode = rune::gfx::CameraMode::ThirdPerson;
            rune::core::log_info("Kamera: Third-Person");
        }
        if (in.pressed(VK_F5)) {
            rune::core::log_info("Ruby-Skripte werden neu geladen ...");
            rune::ruby::load_script("game\\main.rb");
        }
        if (in.pressed(VK_ESCAPE)) window.set_mouse_capture(false);

        if (in.pressed(VK_F12)) {
            static int shot_no = 0;
            ++shot_no;
            renderer.request_screenshot("log\\screenshot_" + std::to_string(shot_no) + ".bmp");
            rune::core::log_info("Screenshot angefordert (#" + std::to_string(shot_no) + ")");
        }

        if (window.consume_resize()) renderer.resize(window.width(), window.height());

        // Erst Physik (Spieler bewegen), dann Kamera daran ausrichten.
        // Bei sim_paused, etwa während eines Kampfes, steht die Physik still;
        // Kamera und Ruby-Oberfläche laufen weiter.
        if (!app.sim_paused) {
            physics.update(dt, in, camera.yaw, terrain);
        }
        camera.target = physics.position();
        camera.eye_height = physics.eye_height;
        camera.update(in, dt);

        // Kamera-Kollision (Etappe 14a): freie Sichtlinie vom Spielerkopf zur
        // Wunschposition suchen; Hindernis -> Kamera ranziehen oder heben.
        float lift = 0.0f;
        if (camera.mode == rune::gfx::CameraMode::ThirdPerson) {
            camera.collision_dist = camera_clearance(terrain, physics, camera, lift);
            camera.collision_lift = lift;
            static bool logged = false;
            if (!logged && (camera.collision_dist >= 0.0f || lift > 0.0f)) {
                logged = true;
                char buf[128];
                std::snprintf(buf, sizeof(buf),
                              "Kamera-Kollision aktiv: dist=%.2f lift=%.1f",
                              camera.collision_dist < 0.0f ? camera.dist : camera.collision_dist, lift);
                rune::core::log_info(buf);
            }
        } else {
            camera.collision_dist = -1.0f;
            camera.collision_lift = 0.0f;
        }

        app.dt = dt;
        fps_accum += dt;
        ++fps_frames;
        if (fps_accum >= 0.5) {
            app.fps = static_cast<float>(fps_frames / fps_accum);
            fps_accum = 0.0;
            fps_frames = 0;

            wchar_t title[160];
            std::swprintf(title, 160, L"Rune3D - RPG Engine   |   %.0f FPS   |   %d Draws",
                          app.fps, renderer.draw_count());
            SetWindowTextW(window.hwnd(), title);
        }

        renderer.begin_frame(camera.view_proj(window.width(), window.height()),
                             camera.eye_position());
        rune::ruby::tick(dt);
        renderer.end_frame();
        rune::core::sound::update();

        in.end_frame();
    }

    rune::core::log_info("Fenster geschlossen, fahre herunter");
    rune::ruby::shutdown();
    renderer.shutdown();
    rune::core::sound::shutdown();
    window.destroy();
    rune::core::log_info("=== Rune3D beendet ===");
    rune::core::log_close();
    return 0;
}
