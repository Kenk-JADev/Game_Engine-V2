#include "ruby/vm.h"

#include <ruby.h>
#include <ruby/version.h>

#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>

#include "app.h"
#include "core/log.h"
#include "core/sound.h"

// In der DLL exportiert, aber nicht in ruby.h deklariert.
// Führt die eingebauten Ruby-Skripte aus (Integer#times, Array#last, ...).
extern "C" void rb_call_builtin_inits(void);

using namespace rune;

namespace {

VALUE g_rune = Qnil;
bool  g_ruby_ready = false;
int   g_tick_errors = 0;
bool  g_tick_disabled = false;

// ---------------------------------------------------------------- Helpers

std::string to_std(VALUE v) {
    VALUE s = rb_funcall(v, rb_intern("to_s"), 0);
    return std::string(StringValueCStr(s));
}

int vk_from_name(const char* n) {
    if (!n || !*n) return 0;

    if (n[1] == '\0') {
        const char c = n[0];
        if (c >= 'a' && c <= 'z') return static_cast<int>(std::toupper(static_cast<unsigned char>(c)));
        if (c >= 'A' && c <= 'Z') return static_cast<int>(c);
        if (c >= '0' && c <= '9') return static_cast<int>(c);
    }

    static const struct { const char* name; int vk; } table[] = {
        {"space", VK_SPACE},   {"shift", VK_SHIFT},   {"ctrl", VK_CONTROL},
        {"control", VK_CONTROL},{"alt", VK_MENU},     {"tab", VK_TAB},
        {"enter", VK_RETURN},  {"return", VK_RETURN}, {"escape", VK_ESCAPE},
        {"esc", VK_ESCAPE},    {"backspace", VK_BACK},{"up", VK_UP},
        {"down", VK_DOWN},     {"left", VK_LEFT},     {"right", VK_RIGHT},
        {"f1", VK_F1}, {"f2", VK_F2}, {"f3", VK_F3},  {"f4", VK_F4},
        {"f5", VK_F5}, {"f6", VK_F6}, {"f7", VK_F7},  {"f8", VK_F8},
        {"f9", VK_F9}, {"f10", VK_F10}, {"f11", VK_F11}, {"f12", VK_F12},
        // Etappe 18m: Pinselgrösse +/- (Zehnertastatur + normale Taste).
        {"add", VK_ADD},       {"plus", VK_OEM_PLUS},
        {"equals", VK_OEM_PLUS},{"subtract", VK_SUBTRACT},
        {"minus", VK_OEM_MINUS},
    };

    for (const auto& e : table) {
        if (std::strcmp(e.name, n) == 0) return e.vk;
    }
    return 0;
}

int vk_from_value(VALUE key) {
    const std::string name = to_std(key);
    return vk_from_name(name.c_str());
}

void report_error(const char* where) {
    VALUE err = rb_errinfo();
    if (NIL_P(err)) {
        core::log_error(std::string(where) + ": unbekannter Fehler");
        return;
    }

    const std::string cls = to_std(rb_funcall(rb_obj_class(err), rb_intern("name"), 0));
    const std::string msg = to_std(rb_funcall(err, rb_intern("message"), 0));
    core::log_error(std::string(where) + ": " + cls + ": " + msg);

    VALUE bt = rb_funcall(err, rb_intern("backtrace"), 0);
    if (!NIL_P(bt)) {
        const long n = RARRAY_LEN(bt);
        for (long i = 0; i < n && i < 15; ++i) {
            VALUE line = rb_ary_entry(bt, i);
            if (!NIL_P(line)) core::log_error("    " + to_std(line));
        }
    }
    rb_set_errinfo(Qnil);
}

// ---------------------------------------------------------------- Gfx

VALUE rune_gfx_clear_color_set(VALUE, VALUE v) {
    if (app().renderer) app().renderer->set_clear_color(static_cast<uint32_t>(NUM2UINT(v)));
    return v;
}

VALUE rune_gfx_box(int argc, VALUE* argv, VALUE) {
    if (argc < 7) rb_raise(rb_eArgError, "box braucht mind. 7 Argumente");

    if (!app().renderer) return Qnil;

    const math::Vec3 pos{static_cast<float>(NUM2DBL(argv[0])),
                         static_cast<float>(NUM2DBL(argv[1])),
                         static_cast<float>(NUM2DBL(argv[2]))};
    const math::Vec3 size{static_cast<float>(NUM2DBL(argv[3])),
                          static_cast<float>(NUM2DBL(argv[4])),
                          static_cast<float>(NUM2DBL(argv[5]))};

    math::Vec3 rot{};
    if (argc > 7 && !NIL_P(argv[7])) rot.x = static_cast<float>(NUM2DBL(argv[7]));
    if (argc > 8 && !NIL_P(argv[8])) rot.y = static_cast<float>(NUM2DBL(argv[8]));
    if (argc > 9 && !NIL_P(argv[9])) rot.z = static_cast<float>(NUM2DBL(argv[9]));

    // Optional: Texturname + Kachelgrösse in Metern.
    int tex = -1;
    float tile = 0.0f;
    if (argc > 10 && !NIL_P(argv[10])) tex = app().renderer->texture_index(to_std(argv[10]));
    if (argc > 11 && !NIL_P(argv[11])) tile = static_cast<float>(NUM2DBL(argv[11]));

    app().renderer->push_box(pos, size, rot, static_cast<uint32_t>(NUM2UINT(argv[6])), tex, tile);
    return Qnil;
}

// Textur aus Ruby anmelden: Rune::Gfx.texture('gras', 64, 64, pixel_string)
// Der String enthält w*h*4 Bytes RGBA (zeilenweise, oben links).
VALUE rune_gfx_texture(VALUE, VALUE name, VALUE w, VALUE h, VALUE pixels) {
    if (!app().renderer) return Qnil;
    const int width  = NUM2INT(w);
    const int height = NUM2INT(h);
    const long need  = static_cast<long>(width) * height * 4;
    if (width <= 0 || height <= 0 || RSTRING_LEN(pixels) != need) {
        rb_raise(rb_eArgError, "Textur '%s': erwartet %ld Bytes, bekommen %ld",
                 to_std(name).c_str(), need, RSTRING_LEN(pixels));
    }
    const int index = app().renderer->add_texture(to_std(name), width, height,
                                                  reinterpret_cast<const uint8_t*>(RSTRING_PTR(pixels)));
    return INT2NUM(index);
}

// Textur für das Gelände (dessen UV in den Vertices steckt).
// Etappe 18c: optionales 2. Argument = Kachelgrösse in Metern
// (Standard 2.0; kleinere Werte = grössere Kacheln).
VALUE rune_gfx_terrain_texture(int argc, VALUE* argv, VALUE) {
    if (!app().renderer) return Qnil;
    const std::string name = to_std(argv[0]);
    app().renderer->set_terrain_tex(app().renderer->texture_index(name));
    if (argc >= 2 && !NIL_P(argv[1])) {
        app().renderer->set_terrain_tile(static_cast<float>(NUM2DBL(argv[1])));
    }
    return Qnil;
}

// Etappe 18d: Boden-Flächen (Splats) der aktuellen Karte anwenden.
//   Rune::Gfx.terrain_splats('gras', [{ 'tex' => 'weg', 'x' => 10, 'z' => -5,
//                                       'w' => 30, 'h' => 12 }, ...])
// Baut den Atlas aus allen 64x64-Texturen und erzeugt das Terrain-Mesh mit
// pro-Quad-Atlas-UV neu (Höhenform bleibt: aktueller Terrain-Parameter).
// Ohne Flächen (leere Liste) wird Qfalse zurückgegeben und nichts geändert.
VALUE rune_gfx_terrain_splats(VALUE, VALUE v_base, VALUE v_list) {
    if (!app().renderer || !app().terrain) return Qfalse;
    if (!RB_TYPE_P(v_list, T_ARRAY)) return Qfalse;
    const int n = RARRAY_LEN(v_list);
    if (n <= 0) return Qfalse;

    std::vector<world::SplatRect> splats;
    splats.reserve(static_cast<size_t>(n));
    // Hash-Schlüssel: JSON.parse liefert Strings, Editor-Skripte können
    // auch Symbole nutzen - beides versuchen.
    auto hget = [&](VALUE h, const char* k) -> VALUE {
        VALUE v = rb_hash_aref(h, rb_str_new_cstr(k));
        if (NIL_P(v)) v = rb_hash_aref(h, ID2SYM(rb_intern(k)));
        return v;
    };
    for (int i = 0; i < n; ++i) {
        const VALUE e = rb_ary_entry(v_list, i);
        if (!RB_TYPE_P(e, T_HASH)) continue;
        VALUE v_tex = hget(e, "tex");
        VALUE v_x   = hget(e, "x");
        VALUE v_z   = hget(e, "z");
        VALUE v_w   = hget(e, "w");
        VALUE v_h   = hget(e, "h");
        world::SplatRect s;
        s.tex = to_std(v_tex);
        s.x = static_cast<float>(NIL_P(v_x) ? 0.0 : NUM2DBL(v_x));
        s.z = static_cast<float>(NIL_P(v_z) ? 0.0 : NUM2DBL(v_z));
        s.w = static_cast<float>(NIL_P(v_w) ? 0.0 : NUM2DBL(v_w));
        s.h = static_cast<float>(NIL_P(v_h) ? 0.0 : NUM2DBL(v_h));
        if (!s.tex.empty() && s.w > 0.0f && s.h > 0.0f) splats.push_back(s);
    }
    if (splats.empty()) return Qfalse;

    const world::AtlasInfo atlas = app().renderer->update_atlas();
    if (!atlas.valid) return Qfalse;

    app().terrain->generate_splat(app().terrain->width(), app().terrain->height(),
                                  app().terrain->resolution(),
                                  app().terrain->seed(), app().terrain->amplitude(),
                                  to_std(v_base), splats, atlas);
    const bool ok = app().renderer->set_terrain(app().terrain->vertices(),
                                               app().terrain->indices());
    if (ok) {
        // Atlas statt Einzeltextur anzeigen; tile-Skalierung ist bei der
        // pro-Quad-UV wirkungslos (faktor 1).
        app().renderer->set_terrain_tex(atlas.tex_index);
        app().renderer->set_terrain_tile(1.0f);
        core::log_info("[ruby] Splat-Boden: " + std::to_string(splats.size()) +
                       " Fläche(n), Atlas-Slot " + std::to_string(atlas.tex_index));
    }
    return ok ? Qtrue : Qfalse;
}

// Himmel-Verlauf: Rune::Gfx.sky(oben_hex, unten_hex)
VALUE rune_gfx_sky(VALUE, VALUE top, VALUE bottom) {
    if (!app().renderer) return Qnil;
    app().renderer->set_sky(NUM2UINT(top), NUM2UINT(bottom));
    return Qnil;
}

// Distanznebel: Rune::Gfx.fog(an, farbe, distanz) - Etappe 17b.
VALUE rune_gfx_fog(VALUE, VALUE on, VALUE rgb, VALUE far_dist) {
    if (!app().renderer) return Qnil;
    app().renderer->set_fog(RTEST(on), NUM2UINT(rgb), static_cast<float>(NUM2DBL(far_dist)));
    return Qnil;
}

// Sonnenlicht: Rune::Gfx.sun(dx, dy, dz, ambient) - Richtung wird normiert.
VALUE rune_gfx_sun(VALUE, VALUE dx, VALUE dy, VALUE dz, VALUE ambient) {
    if (!app().renderer) return Qnil;
    math::Vec3 dir{static_cast<float>(NUM2DBL(dx)),
                   static_cast<float>(NUM2DBL(dy)),
                   static_cast<float>(NUM2DBL(dz))};
    app().renderer->set_sun(dir, static_cast<float>(NUM2DBL(ambient)));
    return Qnil;
}

VALUE rune_gfx_shadows_set(VALUE, VALUE on) {
    if (app().renderer) app().renderer->set_shadows(RTEST(on));
    return Qnil;
}

VALUE rune_gfx_rect(int argc, VALUE* argv, VALUE) {
    VALUE x, y, w, h, color, alpha = Qnil;
    rb_scan_args(argc, argv, "51", &x, &y, &w, &h, &color, &alpha);
    if (!app().renderer) return Qnil;

    app().renderer->ui().rect(static_cast<float>(NUM2DBL(x)), static_cast<float>(NUM2DBL(y)),
                              static_cast<float>(NUM2DBL(w)), static_cast<float>(NUM2DBL(h)),
                              static_cast<uint32_t>(NUM2UINT(color)),
                              NIL_P(alpha) ? 1.0f : static_cast<float>(NUM2DBL(alpha)));
    return Qnil;
}

VALUE rune_gfx_text(int argc, VALUE* argv, VALUE) {
    VALUE x, y, str, color = Qnil, size = Qnil;
    rb_scan_args(argc, argv, "32", &x, &y, &str, &color, &size);
    if (!app().renderer) return Qnil;

    app().renderer->ui().text(static_cast<float>(NUM2DBL(x)), static_cast<float>(NUM2DBL(y)),
                              to_std(str),
                              NIL_P(color) ? 0xFFFFFFu : static_cast<uint32_t>(NUM2UINT(color)),
                              NIL_P(size) ? 18.0f : static_cast<float>(NUM2DBL(size)));
    return Qnil;
}

// ---------------------------------------------------------------- Camera

VALUE rune_camera_mode_set(VALUE, VALUE v) {
    const std::string m = to_std(v);
    if (!app().camera) return v;

    if (m == "first") {
        app().camera->mode = gfx::CameraMode::FirstPerson;
    } else if (m == "third") {
        app().camera->mode = gfx::CameraMode::ThirdPerson;
    } else {
        rb_raise(rb_eArgError, "Kameramodus muss :first oder :third sein (war: %s)", m.c_str());
    }
    return v;
}

VALUE rune_camera_mode(VALUE) {
    if (!app().camera) return rb_id2sym(rb_intern("first"));
    return rb_id2sym(rb_intern(app().camera->mode == gfx::CameraMode::FirstPerson ? "first" : "third"));
}

VALUE rune_camera_player_position(VALUE) {
    math::Vec3 p{};
    if (app().physics) p = app().physics->position();
    else if (app().camera) p = app().camera->target;
    return rb_ary_new_from_args(3, DBL2NUM(p.x), DBL2NUM(p.y), DBL2NUM(p.z));
}

VALUE rune_camera_distance_set(VALUE, VALUE v) {
    if (app().camera) app().camera->dist = static_cast<float>(NUM2DBL(v));
    return v;
}

VALUE rune_camera_distance(VALUE) {
    return DBL2NUM(app().camera ? app().camera->dist : 0.0);
}

// Kamera-Zustand als Kurzstring für HUD/Debug (Etappe 14a):
// "third d=7.0 a=3.2 k=3.5 l=0.5" (d=Soll, a=aktiv, k=Kollision, l=Anhebung)
VALUE rune_camera_info(VALUE) {
    if (!app().camera) return rb_str_new_cstr("keine Kamera");
    const gfx::Camera& c = *app().camera;
    char buf[96];
    snprintf(buf, sizeof(buf), "%s d=%.1f a=%.1f k=%.1f l=%.1f",
                  c.mode == gfx::CameraMode::ThirdPerson ? "third" : "first",
                  c.dist, c.applied_dist(), c.collision_dist, c.applied_lift());
    return rb_str_new_cstr(buf);
}

VALUE rune_camera_yaw_set(VALUE, VALUE v) {
    if (app().camera) app().camera->yaw = static_cast<float>(NUM2DBL(v));
    return v;
}

VALUE rune_camera_yaw(VALUE) {
    return DBL2NUM(app().camera ? app().camera->yaw : 0.0);
}

// Bildschirmpixel -> Weltstrahl. Rückgabe: [ox, oy, oz, dx, dy, dz]
VALUE rune_camera_ray(VALUE, VALUE sx, VALUE sy) {
    if (!app().camera || !app().window) return rb_ary_new();
    const gfx::Ray r = app().camera->pick_ray(app().window->width(), app().window->height(),
                                             static_cast<float>(NUM2DBL(sx)),
                                             static_cast<float>(NUM2DBL(sy)));
    VALUE a = rb_ary_new_capa(6);
    rb_ary_push(a, DBL2NUM(r.origin.x));
    rb_ary_push(a, DBL2NUM(r.origin.y));
    rb_ary_push(a, DBL2NUM(r.origin.z));
    rb_ary_push(a, DBL2NUM(r.dir.x));
    rb_ary_push(a, DBL2NUM(r.dir.y));
    rb_ary_push(a, DBL2NUM(r.dir.z));
    return a;
}

// ---------------------------------------------------------------- Input

VALUE rune_input_down(VALUE, VALUE key) {
    if (!app().window) return Qfalse;
    const int vk = vk_from_value(key);
    if (vk == 0) return Qfalse;
    return app().window->input().down(vk) ? Qtrue : Qfalse;
}

VALUE rune_input_pressed(VALUE, VALUE key) {
    if (!app().window) return Qfalse;
    const int vk = vk_from_value(key);
    if (vk == 0) return Qfalse;
    return app().window->input().pressed(vk) ? Qtrue : Qfalse;
}

VALUE rune_input_mouse_delta(VALUE) {
    float dx = 0.0f, dy = 0.0f;
    if (app().window) {
        dx = app().window->input().mouse_dx;
        dy = app().window->input().mouse_dy;
    }
    return rb_ary_new_from_args(2, DBL2NUM(dx), DBL2NUM(dy));
}

VALUE rune_input_wheel(VALUE) {
    return INT2NUM(app().window ? app().window->input().wheel : 0);
}

VALUE rune_input_captured(VALUE) {
    return (app().window && app().window->input().mouse_captured) ? Qtrue : Qfalse;
}

// Mausfang durch Ruby steuern (Editor schaltet ihn selbst um).
VALUE rune_input_captured_set(VALUE, VALUE v) {
    if (app().window) app().window->set_mouse_capture(RTEST(v));
    return v;
}

// Mausposition in Fensterpixeln: [x, y]
VALUE rune_input_mouse_position(VALUE) {
    VALUE a = rb_ary_new_capa(2);
    if (app().window) {
        rb_ary_push(a, DBL2NUM(app().window->input().mouse_x));
        rb_ary_push(a, DBL2NUM(app().window->input().mouse_y));
    } else {
        rb_ary_push(a, DBL2NUM(0.0));
        rb_ary_push(a, DBL2NUM(0.0));
    }
    return a;
}

// 0 = links, 1 = rechts, 2 = Mitte
VALUE rune_input_mouse_down(VALUE, VALUE idx) {
    if (!app().window) return Qfalse;
    return app().window->input().mouse_down(static_cast<int>(NUM2INT(idx))) ? Qtrue : Qfalse;
}

VALUE rune_input_mouse_pressed(VALUE, VALUE idx) {
    if (!app().window) return Qfalse;
    return app().window->input().mouse_pressed(static_cast<int>(NUM2INT(idx))) ? Qtrue : Qfalse;
}

// Zeichen, die seit dem letzten Frame gedruckt wurden (inkl. '\b' für
// Backspace). Die Eingabe wird beim Lesen geleert.
VALUE rune_input_typed_text(VALUE) {
    if (!app().window) return rb_str_new("", 0);
    const std::string text = app().window->input().take_typed();
    return rb_str_new(text.data(), static_cast<long>(text.size()));
}

// Fenster schliessen (z. B. 'Beenden' im Titelbildschirm).
VALUE rune_app_quit(VALUE) {
    if (app().window) PostMessage(app().window->hwnd(), WM_CLOSE, 0, 0);
    return Qnil;
}

// ---------------------------------------------------------------- Time / Window / Log

VALUE rune_time_fps(VALUE) { return DBL2NUM(app().fps); }
VALUE rune_time_dt(VALUE) { return DBL2NUM(app().dt); }

// Simulations-Pause (Etappe 16b): im Editor steht alles — Physik, Schwerkraft,
// Spielerbewegung. Die Kamera und die UI laufen weiter.
VALUE rune_sim_paused_set(VALUE, VALUE v) {
    app().sim_paused = RTEST(v) ? true : false;
    return v;
}

VALUE rune_sim_paused(VALUE) {
    return app().sim_paused ? Qtrue : Qfalse;
}

VALUE rune_window_width(VALUE) {
    return INT2NUM(app().window ? app().window->width() : 0);
}

VALUE rune_window_height(VALUE) {
    return INT2NUM(app().window ? app().window->height() : 0);
}

// UTF-8 (Ruby) -> Wide (Win32): sonst verstümmelt die ANSI-Codepage
// Umlaute im Fenstertitel.
static std::wstring utf8_to_wide_vm(const std::string& s) {
    if (s.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], n);
    if (!w.empty() && w.back() == L'\0') w.pop_back();
    return w;
}

VALUE rune_window_title_set(VALUE, VALUE v) {
    if (!app().window) return v;
    const std::string title = to_std(v);
    SetWindowTextW(app().window->hwnd(), utf8_to_wide_vm(title).c_str());
    return v;
}

VALUE rune_log_info(VALUE, VALUE v) {
    core::log_info("[ruby] " + to_std(v));
    return Qnil;
}

VALUE rune_log_warn(VALUE, VALUE v) {
    core::log_warn("[ruby] " + to_std(v));
    return Qnil;
}

VALUE rune_log_error(VALUE, VALUE v) {
    core::log_error("[ruby] " + to_std(v));
    return Qnil;
}

// ---------------------------------------------------------------- World / Player / Physics

VALUE rune_world_generate(int argc, VALUE* argv, VALUE) {
    VALUE v_width = Qnil, v_height = Qnil, v_res = Qnil, v_seed = Qnil, v_amp = Qnil;
    rb_scan_args(argc, argv, "05", &v_width, &v_height, &v_res, &v_seed, &v_amp);

    if (!app().terrain || !app().renderer) return Qfalse;

    const float width = NIL_P(v_width) ? 220.0f : static_cast<float>(NUM2DBL(v_width));
    const float height = NIL_P(v_height) ? width : static_cast<float>(NUM2DBL(v_height));
    const int res = NIL_P(v_res) ? 176 : NUM2INT(v_res);
    const uint32_t seed = NIL_P(v_seed) ? 7u : static_cast<uint32_t>(NUM2UINT(v_seed));
    const float amp = NIL_P(v_amp) ? 5.0f : static_cast<float>(NUM2DBL(v_amp));

    app().terrain->generate(width, height, res, seed, amp);
    const bool ok = app().renderer->set_terrain(app().terrain->vertices(), app().terrain->indices());

    core::log_info("[ruby] Gelände: " + std::to_string(static_cast<int>(width)) + " x " +
                   std::to_string(static_cast<int>(height)) + ", " + std::to_string(res) +
                   " Zellen, Seed " + std::to_string(seed) + ", Amplitude " +
                   std::to_string(amp));
    return ok ? Qtrue : Qfalse;
}

VALUE rune_world_height_at(VALUE, VALUE x, VALUE z) {
    if (!app().terrain) return DBL2NUM(0.0);
    return DBL2NUM(app().terrain->height_at(static_cast<float>(NUM2DBL(x)),
                                           static_cast<float>(NUM2DBL(z))));
}

VALUE rune_world_ready(VALUE) {
    return (app().terrain && app().terrain->ready()) ? Qtrue : Qfalse;
}

// Etappe 18l: gemalte Höhen-Pinsel setzen (vor World.generate).
// Ruby: Rune::World.height_brushes = [[x, z, r, h], ...]  (leere Liste/NIL löscht).
// Konvention wie im Terrain: r = Radius in m, h = Höhen-Änderung (negativ = Senke).
// Muss VOR dem generate()-Aufruf stehen, weil compute_heights() die Pinsel
// beim Generieren einbaut.
VALUE rune_world_height_brushes_set(VALUE, VALUE list) {
    if (!app().terrain) return Qfalse;
    std::vector<math::Vec4> brushes;
    if (!NIL_P(list)) {
        const long n = RARRAY_LEN(list);
        brushes.reserve(static_cast<size_t>(n > 0 ? n : 0));
        for (long i = 0; i < n; ++i) {
            VALUE e = rb_ary_entry(list, i);
            if (!RB_TYPE_P(e, T_ARRAY)) continue;
            if (RARRAY_LEN(e) < 4) continue;
            math::Vec4 b{};
            b.x = static_cast<float>(NUM2DBL(rb_ary_entry(e, 0)));
            b.z = static_cast<float>(NUM2DBL(rb_ary_entry(e, 1)));
            b.w = static_cast<float>(NUM2DBL(rb_ary_entry(e, 2)));   // Radius
            b.y = static_cast<float>(NUM2DBL(rb_ary_entry(e, 3)));   // Höhe
            brushes.push_back(b);
        }
    }
    app().terrain->set_height_brushes(brushes);
    return Qtrue;
}

VALUE rune_player_position(VALUE) {
    math::Vec3 p{};
    if (app().physics) p = app().physics->position();
    return rb_ary_new_from_args(3, DBL2NUM(p.x), DBL2NUM(p.y), DBL2NUM(p.z));
}

VALUE rune_player_teleport(VALUE, VALUE x, VALUE y, VALUE z) {
    if (app().physics) {
        app().physics->teleport(math::Vec3{static_cast<float>(NUM2DBL(x)),
                                          static_cast<float>(NUM2DBL(y)),
                                          static_cast<float>(NUM2DBL(z))});
    }
    return Qnil;
}

VALUE rune_player_on_ground(VALUE) {
    return (app().physics && app().physics->on_ground()) ? Qtrue : Qfalse;
}

// Spieler aus einem Kreis herausdrücken (NPC-Kollision, Etappe 15).
VALUE rune_player_push_out(VALUE, VALUE cx, VALUE cz, VALUE radius) {
    if (app().physics) {
        app().physics->push_out(static_cast<float>(NUM2DBL(cx)),
                                static_cast<float>(NUM2DBL(cz)),
                                static_cast<float>(NUM2DBL(radius)));
    }
    return Qnil;
}

VALUE rune_player_speed_set(VALUE, VALUE v) {
    if (app().physics) app().physics->speed = static_cast<float>(NUM2DBL(v));
    return v;
}

VALUE rune_player_jump_set(VALUE, VALUE v) {
    if (app().physics) app().physics->jump_speed = static_cast<float>(NUM2DBL(v));
    return v;
}

VALUE rune_physics_clear_solids(VALUE) {
    if (app().physics) app().physics->clear_solids();
    return Qnil;
}

VALUE rune_physics_add_solid(int argc, VALUE* argv, VALUE) {
    VALUE x, y, z, sx, sy, sz;
    rb_scan_args(argc, argv, "33", &x, &y, &z, &sx, &sy, &sz);
    if (app().physics) {
        app().physics->add_solid(
            math::Vec3{static_cast<float>(NUM2DBL(x)), static_cast<float>(NUM2DBL(y)),
                       static_cast<float>(NUM2DBL(z))},
            math::Vec3{static_cast<float>(NUM2DBL(sx)), static_cast<float>(NUM2DBL(sy)),
                       static_cast<float>(NUM2DBL(sz))});
    }
    return Qnil;
}

VALUE rune_physics_solid_count(VALUE) {
    return INT2NUM(app().physics ? app().physics->solid_count() : 0);
}

// Etappe 19k.2: Block-Zonen (Passierbarkeit). Rechtecke in der XZ-Ebene,
// über alle Höhen blockierend (unsichtbare Wände). Datenquelle: das
// „blocked"-Array im Karten-JSON ({x,z,w,h}, x/z = Mitte, Meter).
VALUE rune_physics_clear_blocked(VALUE) {
    if (app().physics) app().physics->clear_blocked();
    return Qnil;
}

VALUE rune_physics_add_blocked(VALUE, VALUE x, VALUE z, VALUE w, VALUE h) {
    if (app().physics) {
        app().physics->add_blocked(static_cast<float>(NUM2DBL(x)),
                                   static_cast<float>(NUM2DBL(z)),
                                   static_cast<float>(NUM2DBL(w)),
                                   static_cast<float>(NUM2DBL(h)));
    }
    return Qnil;
}

VALUE rune_physics_blocked_count(VALUE) {
    return INT2NUM(app().physics ? app().physics->blocked_count() : 0);
}

// Etappe 19k.3: Punkt-Test gegen Objekt-Hindernisse. Ruby tastet damit
// über der Steiggrenze (kMaxStep) ab, ob an einer Stelle eine Wand steht -
// niedrige Kästen bleiben begehbar (Stufen), genau wie in der Physik.
VALUE rune_physics_solid_at(VALUE, VALUE x, VALUE y, VALUE z) {
    if (!app().physics) return Qfalse;
    return app().physics->solid_at(static_cast<float>(NUM2DBL(x)),
                                   static_cast<float>(NUM2DBL(y)),
                                   static_cast<float>(NUM2DBL(z))) ? Qtrue : Qfalse;
}

VALUE rune_physics_is_blocked(VALUE, VALUE x, VALUE z) {
    if (!app().physics) return Qfalse;
    return app().physics->is_blocked(static_cast<float>(NUM2DBL(x)),
                                     static_cast<float>(NUM2DBL(z))) ? Qtrue : Qfalse;
}

// ---------------------------------------------------------------- Registration


// Sound: PCM (16 Bit mono, little endian) anmelden und abspielen.
VALUE rune_sound_register(VALUE, VALUE name, VALUE rate, VALUE samples) {
    Check_Type(samples, T_STRING);
    const long len = RSTRING_LEN(samples);
    if (len % 2 != 0) rb_raise(rb_eArgError, "Sound '%s': Bytezahl muss gerade sein", to_std(name).c_str());
    core::sound::register_sound(to_std(name), NUM2INT(rate),
                                reinterpret_cast<const int16_t*>(RSTRING_PTR(samples)),
                                static_cast<size_t>(len) / 2);
    return Qnil;
}

VALUE rune_sound_play(int argc, VALUE* argv, VALUE) {
    VALUE name, volume = Qnil;
    rb_scan_args(argc, argv, "11", &name, &volume);
    const float vol = NIL_P(volume) ? 1.0f : static_cast<float>(NUM2DBL(volume));
    core::sound::play(to_std(name), vol);
    return Qnil;
}

VALUE rune_sound_ready(VALUE) {
    return core::sound::ready() ? Qtrue : Qfalse;
}

VALUE rune_sound_loop(int argc, VALUE* argv, VALUE) {
    VALUE name, volume = Qnil;
    rb_scan_args(argc, argv, "11", &name, &volume);
    const float vol = NIL_P(volume) ? 1.0f : static_cast<float>(NUM2DBL(volume));
    core::sound::loop(to_std(name), vol);
    return Qnil;
}

VALUE rune_sound_stop(VALUE, VALUE name) {
    core::sound::stop(to_std(name));
    return Qnil;
}

void register_bindings() {
    VALUE mRune = rb_define_module("Rune");
    g_rune = mRune;

    rb_define_const(mRune, "VERSION", rb_str_new_cstr("0.1.0"));
    rb_define_const(mRune, "RUBY_VERSION_EMBEDDED", rb_str_new_cstr(ruby_version));
    rb_define_const(mRune, "ROOT", rb_str_new_cstr(app().root.c_str()));

    VALUE mGfx = rb_define_module_under(mRune, "Gfx");
    rb_define_module_function(mGfx, "clear_color=", RUBY_METHOD_FUNC(rune_gfx_clear_color_set), 1);
    rb_define_module_function(mGfx, "box", RUBY_METHOD_FUNC(rune_gfx_box), -1);
    rb_define_module_function(mGfx, "texture", RUBY_METHOD_FUNC(rune_gfx_texture), 4);
    rb_define_module_function(mGfx, "terrain_texture", RUBY_METHOD_FUNC(rune_gfx_terrain_texture), -1);
    rb_define_module_function(mGfx, "terrain_splats", RUBY_METHOD_FUNC(rune_gfx_terrain_splats), 2);
    rb_define_module_function(mGfx, "sky", RUBY_METHOD_FUNC(rune_gfx_sky), 2);
    rb_define_module_function(mGfx, "fog", RUBY_METHOD_FUNC(rune_gfx_fog), 3);
    rb_define_module_function(mGfx, "sun", RUBY_METHOD_FUNC(rune_gfx_sun), 4);
    rb_define_module_function(mGfx, "shadows=", RUBY_METHOD_FUNC(rune_gfx_shadows_set), 1);
    rb_define_module_function(mGfx, "rect", RUBY_METHOD_FUNC(rune_gfx_rect), -1);
    rb_define_module_function(mGfx, "text", RUBY_METHOD_FUNC(rune_gfx_text), -1);

    VALUE mCam = rb_define_module_under(mRune, "Camera");
    rb_define_module_function(mCam, "mode", RUBY_METHOD_FUNC(rune_camera_mode), 0);
    rb_define_module_function(mCam, "mode=", RUBY_METHOD_FUNC(rune_camera_mode_set), 1);
    rb_define_module_function(mCam, "player_position", RUBY_METHOD_FUNC(rune_camera_player_position), 0);
    rb_define_module_function(mCam, "distance", RUBY_METHOD_FUNC(rune_camera_distance), 0);
    rb_define_module_function(mCam, "distance=", RUBY_METHOD_FUNC(rune_camera_distance_set), 1);
    rb_define_module_function(mCam, "info", RUBY_METHOD_FUNC(rune_camera_info), 0);
    rb_define_module_function(mCam, "yaw", RUBY_METHOD_FUNC(rune_camera_yaw), 0);
    rb_define_module_function(mCam, "yaw=", RUBY_METHOD_FUNC(rune_camera_yaw_set), 1);
    rb_define_module_function(mCam, "ray", RUBY_METHOD_FUNC(rune_camera_ray), 2);

    VALUE mIn = rb_define_module_under(mRune, "Input");
    rb_define_module_function(mIn, "down?", RUBY_METHOD_FUNC(rune_input_down), 1);
    rb_define_module_function(mIn, "pressed?", RUBY_METHOD_FUNC(rune_input_pressed), 1);
    rb_define_module_function(mIn, "mouse_delta", RUBY_METHOD_FUNC(rune_input_mouse_delta), 0);
    rb_define_module_function(mIn, "wheel", RUBY_METHOD_FUNC(rune_input_wheel), 0);
    rb_define_module_function(mIn, "captured?", RUBY_METHOD_FUNC(rune_input_captured), 0);
    rb_define_module_function(mIn, "capture=", RUBY_METHOD_FUNC(rune_input_captured_set), 1);
    rb_define_module_function(mIn, "mouse_position", RUBY_METHOD_FUNC(rune_input_mouse_position), 0);
    rb_define_module_function(mIn, "mouse_down?", RUBY_METHOD_FUNC(rune_input_mouse_down), 1);
    rb_define_module_function(mIn, "mouse_pressed?", RUBY_METHOD_FUNC(rune_input_mouse_pressed), 1);
    rb_define_module_function(mIn, "typed_text", RUBY_METHOD_FUNC(rune_input_typed_text), 0);

    VALUE mSound = rb_define_module_under(mRune, "Sound");
    rb_define_module_function(mSound, "register", RUBY_METHOD_FUNC(rune_sound_register), 3);
    rb_define_module_function(mSound, "play", RUBY_METHOD_FUNC(rune_sound_play), -1);
    rb_define_module_function(mSound, "loop", RUBY_METHOD_FUNC(rune_sound_loop), -1);
    rb_define_module_function(mSound, "stop", RUBY_METHOD_FUNC(rune_sound_stop), 1);
    rb_define_module_function(mSound, "ready?", RUBY_METHOD_FUNC(rune_sound_ready), 0);

    VALUE mApp = rb_define_module_under(mRune, "App");
    rb_define_module_function(mApp, "quit", RUBY_METHOD_FUNC(rune_app_quit), 0);

    VALUE mTime = rb_define_module_under(mRune, "Time");
    rb_define_module_function(mTime, "fps", RUBY_METHOD_FUNC(rune_time_fps), 0);
    rb_define_module_function(mTime, "dt", RUBY_METHOD_FUNC(rune_time_dt), 0);

    // Simulations-Schalter (Etappe 16b): true = Physik eingefroren (Editor).
    VALUE mSim = rb_define_module_under(mRune, "Simulation");
    rb_define_module_function(mSim, "paused=", RUBY_METHOD_FUNC(rune_sim_paused_set), 1);
    rb_define_module_function(mSim, "paused?", RUBY_METHOD_FUNC(rune_sim_paused), 0);

    VALUE mWin = rb_define_module_under(mRune, "Window");
    rb_define_module_function(mWin, "width", RUBY_METHOD_FUNC(rune_window_width), 0);
    rb_define_module_function(mWin, "height", RUBY_METHOD_FUNC(rune_window_height), 0);
    rb_define_module_function(mWin, "title=", RUBY_METHOD_FUNC(rune_window_title_set), 1);
    VALUE mLog = rb_define_module_under(mRune, "Log");
    rb_define_module_function(mLog, "info", RUBY_METHOD_FUNC(rune_log_info), 1);
    rb_define_module_function(mLog, "warn", RUBY_METHOD_FUNC(rune_log_warn), 1);
    rb_define_module_function(mLog, "error", RUBY_METHOD_FUNC(rune_log_error), 1);

    VALUE mWorld = rb_define_module_under(mRune, "World");
    rb_define_module_function(mWorld, "generate_raw", RUBY_METHOD_FUNC(rune_world_generate), -1);
    rb_define_module_function(mWorld, "height_at", RUBY_METHOD_FUNC(rune_world_height_at), 2);
    rb_define_module_function(mWorld, "ready?", RUBY_METHOD_FUNC(rune_world_ready), 0);
    // Etappe 18l: gemalte Höhen-Pinsel (Berge/Senken aus dem Editor).
    rb_define_module_function(mWorld, "height_brushes=", RUBY_METHOD_FUNC(rune_world_height_brushes_set), 1);

    VALUE mPlayer = rb_define_module_under(mRune, "Player");
    rb_define_module_function(mPlayer, "position", RUBY_METHOD_FUNC(rune_player_position), 0);
    rb_define_module_function(mPlayer, "teleport", RUBY_METHOD_FUNC(rune_player_teleport), 3);
    rb_define_module_function(mPlayer, "push_out", RUBY_METHOD_FUNC(rune_player_push_out), 3);
    rb_define_module_function(mPlayer, "on_ground?", RUBY_METHOD_FUNC(rune_player_on_ground), 0);
    rb_define_module_function(mPlayer, "speed=", RUBY_METHOD_FUNC(rune_player_speed_set), 1);
    rb_define_module_function(mPlayer, "jump_speed=", RUBY_METHOD_FUNC(rune_player_jump_set), 1);

    VALUE mPhys = rb_define_module_under(mRune, "Physics");
    rb_define_module_function(mPhys, "clear_solids", RUBY_METHOD_FUNC(rune_physics_clear_solids), 0);
    rb_define_module_function(mPhys, "add_solid", RUBY_METHOD_FUNC(rune_physics_add_solid), -1);
    rb_define_module_function(mPhys, "solid_count", RUBY_METHOD_FUNC(rune_physics_solid_count), 0);
    // Etappe 19k.2: Block-Zonen (Passierbarkeit).
    rb_define_module_function(mPhys, "clear_blocked", RUBY_METHOD_FUNC(rune_physics_clear_blocked), 0);
    rb_define_module_function(mPhys, "add_blocked", RUBY_METHOD_FUNC(rune_physics_add_blocked), 4);
    rb_define_module_function(mPhys, "blocked_count", RUBY_METHOD_FUNC(rune_physics_blocked_count), 0);
    rb_define_module_function(mPhys, "blocked?", RUBY_METHOD_FUNC(rune_physics_is_blocked), 2);
    // Etappe 19k.3: Objekt-Hindernisse abfragen (RGSS passable?, Routen).
    rb_define_module_function(mPhys, "solid_at?", RUBY_METHOD_FUNC(rune_physics_solid_at), 3);
}

struct TickArgs {
    VALUE receiver;
    VALUE dt;
};

VALUE tick_body(VALUE arg) {
    auto* a = reinterpret_cast<TickArgs*>(arg);
    return rb_funcall(a->receiver, rb_intern("tick"), 1, a->dt);
}

} // namespace

namespace rune::ruby {


bool init(int argc, char** argv, const std::string& root) {
    app().root = root;

    // XCopy-Deployment: Liegt die Ruby-DLL neben der EXE (build/), berechnet
    // sie ihr lib-Präfix relativ zu ihrem eigenen Speicherort und findet die
    // stdlib ('json', Gems) der Ruby-Installation nicht mehr. RUBYLIB zeigt
    // den Load-Pfad zurück dorthin; ruby_init_loadpath() liest die Variable.
    // Nur setzen, wenn nicht schon vom Nutzer belegt.
    {
        size_t len = 0;
        getenv_s(&len, nullptr, 0, "RUBYLIB");
        if (len == 0) {
            const char* dirs[] = {
                "C:/Ruby34-x64/lib/ruby/site_ruby/3.4.0",
                "C:/Ruby34-x64/lib/ruby/site_ruby/3.4.0/x64-ucrt",
                "C:/Ruby34-x64/lib/ruby/site_ruby",
                "C:/Ruby34-x64/lib/ruby/vendor_ruby/3.4.0",
                "C:/Ruby34-x64/lib/ruby/vendor_ruby/3.4.0/x64-ucrt",
                "C:/Ruby34-x64/lib/ruby/vendor_ruby",
                "C:/Ruby34-x64/lib/ruby/3.4.0",
                "C:/Ruby34-x64/lib/ruby/3.4.0/x64-mingw-ucrt",
            };
            std::string val;
            for (const char* d : dirs) {
                if (!val.empty()) val += ';';
                val += d;
            }
            _putenv_s("RUBYLIB", val.c_str());
        }
    }

    ruby_sysinit(&argc, &argv);

    {
        RUBY_INIT_STACK;
        ruby_init();
    }

    ruby_init_loadpath();
    ruby_script("rune3d");

    // WICHTIG: ruby_init() allein führt die eingebauten Ruby-Skripte nicht aus.
    // Ohne diesen Aufruf fehlen Kernmethoden wie Integer#times, Integer#to_i
    // oder Array#last (Ruby 3.4 definiert sie in Builtin-Skripten).
    rb_call_builtin_inits();

    // Encodings registrieren (Encoding::UTF_8 usw.). Ohne encdb fehlen diese
    // Konstanten und jedes `require` von stdlib (z. B. 'json') bricht ab.
    {
        int state = 0;
        rb_protect([](VALUE) -> VALUE { return rb_require("enc/encdb"); }, Qnil, &state);
        if (state) {
            core::log_warn("enc/encdb konnte nicht geladen werden - stdlib-require evtl. defekt");
            rb_set_errinfo(Qnil);
        }
    }

    g_ruby_ready = true;

    register_bindings();

    // UTF-8 als Default für Skripte
    rb_eval_string("Rune::Log.info('Ruby ' + RUBY_VERSION + ' eingebettet (' + RUBY_PLATFORM + ')')");

    core::log_info("Ruby-VM gestartet: " + std::string(ruby_version));
    return true;
}

bool load_script(const std::string& path) {
    if (!g_ruby_ready) return false;

    // Ruby erwartet Forward-Slashes und mag relative Pfade nicht immer:
    // wir bauen einen absoluten Pfad ab der Projektwurzel.
    std::string p = path;
    for (char& c : p) {
        if (c == '\\') c = '/';
    }
    if (p.size() < 2 || p[1] != ':') {
        p = app().root;
        for (char& c : p) {
            if (c == '\\') c = '/';
        }
        if (!p.empty() && p.back() != '/') p += '/';
        p += path;
        for (char& c : p) {
            if (c == '\\') c = '/';
        }
    }

    int state = 0;
    rb_load_protect(rb_str_new_cstr(p.c_str()), 0, &state);

    if (state != 0) {
        report_error(("Ruby-Fehler in " + path).c_str());
        return false;
    }

    g_tick_errors = 0;
    g_tick_disabled = false;
    return true;
}

void tick(float dt) {
    if (!g_ruby_ready || NIL_P(g_rune) || g_tick_disabled) return;
    if (!rb_respond_to(g_rune, rb_intern("tick"))) return;

    TickArgs args{g_rune, DBL2NUM(dt)};
    int state = 0;
    rb_protect(tick_body, reinterpret_cast<VALUE>(&args), &state);

    if (state != 0) {
        report_error("Ruby-Fehler in Rune.tick");
        // Nicht 60x pro Sekunde dieselbe Meldung ins Log schreiben: nach drei
        // Fehlern in Folge wird der Tick angehalten, bis F5 neu lädt.
        if (++g_tick_errors >= 3) {
            g_tick_disabled = true;
            core::log_warn("Ruby-Fehler häufen sich - Tick gestoppt. F5 lädt die Skripte neu.");
        }
    } else {
        g_tick_errors = 0;
    }
}

void shutdown() {
    if (!g_ruby_ready) return;
    g_ruby_ready = false;
    ruby_finalize();
}

} // namespace rune::ruby
