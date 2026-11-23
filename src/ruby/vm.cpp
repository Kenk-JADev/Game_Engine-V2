#include "ruby/vm.h"

#include <ruby.h>
#include <ruby/encoding.h>
#include <ruby/version.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>

#include "app.h"
#include "core/log.h"
#include "core/project_assets_win.h"
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

// Die in einer Spielsession geprüften Ordner bleiben bis F5/Projektwechsel
// gebunden; einzelne Ruby-Reads dürfen nicht plötzlich ein gleich benanntes,
// zwischenzeitlich ausgetauschtes Projektverzeichnis öffnen.
std::unique_ptr<assets::ProjectStore> g_asset_store;
std::string g_asset_root, g_asset_project;
bool g_asset_has_project = false;

void reset_project_assets() noexcept {
    g_asset_store.reset();
    g_asset_root.clear();
    g_asset_project.clear();
    g_asset_has_project = false;
}

// ---------------------------------------------------------------- Helpers

std::string to_std(VALUE v) {
    VALUE s = rb_funcall(v, rb_intern("to_s"), 0);
    return std::string(StringValueCStr(s));
}

// Keine NUM2INT/NUM2UINT-Konvertierung von ungeprüften Kartenwerten; riesige
// Ruby-Integer können bei NUM2DBL als Inf enden und werden danach verworfen.
// Strings/Arrays sind keine Karten-Zahlen (auch wenn sie to_f besitzen).
bool vm_number(VALUE value, double& out) {
    if (!RB_INTEGER_TYPE_P(value) && !RB_TYPE_P(value, T_FLOAT)) return false;
    out = NUM2DBL(value);
    return true;
}

// Kartentexte sind Strings; Symbole bleiben für direkte Ruby-Skripte erlaubt.
// Vor dem Kopieren begrenzen und NUL abweisen, statt StringValueCStr inmitten
// lebender C++-Container eine Ruby-Exception auslösen zu lassen.
bool vm_texture_name(VALUE value, std::string& out) {
    if (SYMBOL_P(value)) value = rb_sym2str(value);
    if (!RB_TYPE_P(value, T_STRING)) return false;
    const long n = RSTRING_LEN(value);
    if (n <= 0 || static_cast<unsigned long>(n) > world::TERRAIN_MAX_TEXTURE_BYTES ||
        std::memchr(RSTRING_PTR(value), '\0', static_cast<size_t>(n))) return false;
    out.assign(RSTRING_PTR(value), static_cast<size_t>(n));
    return true;
}

void vm_warn_safely(const char* message) noexcept {
    // Auch das Log baut Strings; im bad_alloc-Pfad darf es nicht erneut werfen.
    try { core::log_warn(message); } catch (...) {}
}

VALUE vm_hash_get(VALUE h, const char* key) {
    VALUE v = rb_hash_aref(h, rb_str_new_cstr(key));
    if (NIL_P(v)) v = rb_hash_aref(h, ID2SYM(rb_intern(key)));
    return v;
}

std::uint32_t vm_color(VALUE v, std::uint32_t fallback, const char* field) {
    double n;
    if (vm_number(v, n) && world::atmosphere_valid_color(n)) return static_cast<std::uint32_t>(n);
    core::log_warn(std::string("[ruby] ") + field + " ungültig; Standardfarbe als Ersatz");
    return fallback;
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
    Check_Type(pixels, T_STRING);
    // Vor dem Multiplizieren dieselben Grenzen wie Import und Renderer prüfen.
    std::size_t count = 0;
    if (!world::texture_byte_count(width, height, count))
        rb_raise(rb_eArgError, "Texturbreite und -höhe müssen zwischen 1 und 4096 liegen");
    const long need = static_cast<long>(count);
    if (RSTRING_LEN(pixels) != need)
        rb_raise(rb_eArgError, "Textur: erwartet %ld Bytes, bekommen %ld", need, RSTRING_LEN(pixels));
    try {
        std::string texture_name;
        if (!vm_texture_name(name, texture_name)) {
            vm_warn_safely("[ruby] Textur: ungültiger oder zu langer Name");
            return INT2NUM(-1);
        }
        const int index = app().renderer->add_texture(texture_name, width, height,
                                                      reinterpret_cast<const uint8_t*>(RSTRING_PTR(pixels)));
        return INT2NUM(index);
    } catch (const std::bad_alloc&) {
        vm_warn_safely("[ruby] Nicht genügend Speicher für die Textur");
        return INT2NUM(-1);
    } catch (const std::length_error&) {
        vm_warn_safely("[ruby] Textur überschreitet den Speicherbereich");
        return INT2NUM(-1);
    }
}

// Projektwechsel und F5 räumen nur die projektspezifische Ebene auf;
// eingebaute Texturen und ihre Slots bleiben erhalten.
VALUE rune_gfx_begin_project_textures(VALUE) {
    reset_project_assets();
    if (app().renderer) app().renderer->begin_project_textures();
    return Qnil;
}

VALUE rune_gfx_end_project_textures(VALUE) {
    reset_project_assets();
    if (app().renderer) app().renderer->end_project_textures();
    return Qnil;
}

// Gemeinsamer, begrenzter Material-Lesepfad für Ruby und Editor. Die
// Rückgabe ist [Statussymbol, Bytes oder nil]; keine ungesicherten File.binread-
// Fallbacks in der eingebetteten Engine.
VALUE rune_assets_read_texture(VALUE, VALUE data_root, VALUE project, VALUE filename, VALUE expected) {
    if (!RB_TYPE_P(data_root, T_STRING) || (!NIL_P(project) && !RB_TYPE_P(project, T_STRING)) ||
        (!NIL_P(filename) && !RB_TYPE_P(filename, T_STRING)))
        return rb_ary_new_from_args(2, ID2SYM(rb_intern("invalid")), Qnil);
    const long root_len = RSTRING_LEN(data_root);
    const long project_len = NIL_P(project) ? 0 : RSTRING_LEN(project);
    const long filename_len = NIL_P(filename) ? 0 : RSTRING_LEN(filename);
    if (root_len <= 0 || root_len > 32760 || project_len < 0 || project_len > 1024 ||
        filename_len < 0 || filename_len > 255)
        return rb_ary_new_from_args(2, ID2SYM(rb_intern("invalid")), Qnil);
    const long byte_count = NIL_P(filename) ? 0 : NUM2LONG(expected);
    if (byte_count < 0 || static_cast<std::size_t>(byte_count) > world::TEXTURE_MAX_BYTES)
        return rb_ary_new_from_args(2, ID2SYM(rb_intern("invalid")), Qnil);

    assets::Result result = assets::Result::Invalid;
    std::string bytes;
    try {
        const std::string root(RSTRING_PTR(data_root), static_cast<std::size_t>(root_len));
        const bool has_project = !NIL_P(project);
        const std::string project_name = has_project
            ? std::string(RSTRING_PTR(project), static_cast<std::size_t>(project_len)) : std::string{};
        std::wstring project_wide;
        const std::wstring* project_ptr = has_project ? &project_wide : nullptr;
        const bool project_ok = !has_project ||
            (assets::to_wide(project_name, project_wide) && assets::component(project_wide));
        if (project_ok && assets::matches_data_root(app().root, root, project_ptr)) {
            if (!g_asset_store || g_asset_root != root || g_asset_project != project_name ||
                g_asset_has_project != has_project) {
                reset_project_assets();
                auto store = std::make_unique<assets::ProjectStore>();
                result = store->open(app().root, project_ptr);
                if (result == assets::Result::Ok) {
                    g_asset_root = root;
                    g_asset_project = project_name;
                    g_asset_has_project = has_project;
                    g_asset_store = std::move(store);
                }
            } else result = assets::Result::Ok;
            if (result == assets::Result::Ok && g_asset_store) {
                if (NIL_P(filename)) result = g_asset_store->read_manifest(bytes);
                else {
                    const std::string file(RSTRING_PTR(filename), static_cast<std::size_t>(filename_len));
                    result = g_asset_store->read_texture(file, static_cast<std::size_t>(byte_count), bytes);
                }
            }
        } else reset_project_assets();
    } catch (const std::bad_alloc&) {
        result = assets::Result::Io;
    } catch (const std::length_error&) {
        result = assets::Result::TooLarge;
    }
    const char* symbol = "io";
    switch (result) {
        case assets::Result::Ok: symbol = "ok"; break;
        case assets::Result::Missing: symbol = "missing"; break;
        case assets::Result::Conflict: symbol = "conflict"; break;
        case assets::Result::Invalid: symbol = "invalid"; break;
        case assets::Result::TooLarge: symbol = "too_large"; break;
        case assets::Result::Reparse: symbol = "reparse"; break;
        case assets::Result::Io: break;
    }
    const VALUE data = result == assets::Result::Ok ? rb_str_new(bytes.data(), static_cast<long>(bytes.size())) : Qnil;
    return rb_ary_new_from_args(2, ID2SYM(rb_intern(symbol)), data);
}

// Textur für das Gelände (dessen UV in den Vertices steckt).
// Etappe 18c: optionales 2. Argument = Kachelgrösse in Metern
// (Standard 2.0; kleinere Werte = grössere Kacheln).
VALUE rune_gfx_terrain_texture(int argc, VALUE* argv, VALUE) {
    rb_check_arity(argc, 1, 2);
    if (!app().renderer) return Qnil;
    try {
        std::string name;
        if (!vm_texture_name(argv[0], name)) {
            vm_warn_safely("[ruby] terrain.texture ungültig; Gras als Ersatz");
            name = "gras";
        }
        const int index = app().renderer->texture_index(name);
        if (index < 0) vm_warn_safely("[ruby] terrain.texture nicht geladen; Gelände ohne Textur");
        app().renderer->set_terrain_tex(index);
        if (argc >= 2 && !NIL_P(argv[1])) {
            double tile;
            if (!vm_number(argv[1], tile) || !world::terrain_valid_tile(tile)) {
                vm_warn_safely("[ruby] terrain.tile ungültig; 2 m als Ersatz");
                tile = 2.0;
            }
            app().renderer->set_terrain_tile(static_cast<float>(tile));
        }
        return Qnil;
    } catch (const std::bad_alloc&) {
        app().renderer->set_terrain_tex(-1);
        app().renderer->set_terrain_tile(2.0f);
        vm_warn_safely("[ruby] Nicht genügend Speicher für die Bodentextur");
        return Qfalse;
    } catch (const std::length_error&) {
        app().renderer->set_terrain_tex(-1);
        app().renderer->set_terrain_tile(2.0f);
        vm_warn_safely("[ruby] Texturname überschreitet den Speicherbereich");
        return Qfalse;
    }
}

// Etappe 18d: Boden-Flächen (Splats) der aktuellen Karte anwenden.
//   Rune::Gfx.terrain_splats('gras', [{ 'tex' => 'weg', 'x' => 10, 'z' => -5,
//                                       'w' => 30, 'h' => 12 }, ...])
// Baut den Atlas aus allen 64x64-Texturen und erzeugt das Terrain-Mesh mit
// pro-Quad-Atlas-UV neu (Höhenform bleibt: aktueller Terrain-Parameter).
// Ohne Flächen (leere Liste) wird Qfalse zurückgegeben und nichts geändert.
VALUE rune_gfx_terrain_splats(VALUE, VALUE v_base, VALUE v_list) {
    if (!app().renderer || !app().terrain || !app().terrain->ready()) return Qfalse;
    auto fail = [](const char* reason) -> VALUE {
        app().terrain->clear();
        app().renderer->clear_terrain();
        // Auch bei erneutem Allokationsfehler muss das Binding Qfalse liefern.
        try { core::log_warn(std::string("[ruby] Splat-Boden: ") + reason + "; kein Gelände"); }
        catch (...) {}
        return Qfalse;
    };
    try {
    if (!RB_TYPE_P(v_list, T_ARRAY)) return fail("terrain.splats ist keine Liste");
    const long n = RARRAY_LEN(v_list);
    if (n <= 0) return Qfalse;
    if (static_cast<unsigned long>(n) > world::TERRAIN_MAX_SPLATS)
        return fail("zu viele Bodenflächen");
    const world::TerrainPlan plan = world::terrain_plan(app().terrain->width(), app().terrain->height(),
        app().terrain->resolution(), app().terrain->seed(), app().terrain->amplitude());
    if (!plan.ok() || plan.quads() > world::TERRAIN_MAX_SPLAT_CHECKS / static_cast<size_t>(n))
        return fail("Rechenbudget überschritten");

    std::vector<world::SplatRect> splats;
    splats.reserve(static_cast<size_t>(n));
    for (long i = 0; i < n; ++i) {
        const VALUE e = rb_ary_entry(v_list, i);
        if (!RB_TYPE_P(e, T_HASH)) return fail("ungültiger Eintrag");
        const VALUE v_tex = vm_hash_get(e, "tex");
        double x, z, w, h;
        if ((!RB_TYPE_P(v_tex, T_STRING) && !SYMBOL_P(v_tex)) ||
            !vm_number(vm_hash_get(e, "x"), x) || !vm_number(vm_hash_get(e, "z"), z) ||
            !vm_number(vm_hash_get(e, "w"), w) || !vm_number(vm_hash_get(e, "h"), h) ||
            !world::terrain_valid_splat(x, z, w, h)) return fail("ungültige Fläche");
        world::SplatRect s;
        if (!vm_texture_name(v_tex, s.tex)) return fail("ungültiger oder zu langer Texturname");
        s.x = static_cast<float>(x); s.z = static_cast<float>(z);
        s.w = static_cast<float>(w); s.h = static_cast<float>(h);
        splats.push_back(std::move(s));
    }
    if (world::terrain_check_splats(plan, splats) != world::TerrainProblem::None)
        return fail("ungültige Fläche oder Rechenbudget überschritten");
    std::string base;
    if (!vm_texture_name(v_base, base)) {
        vm_warn_safely("[ruby] terrain.texture ungültig; Gras als Ersatz");
        base = "gras";
    }
    const world::AtlasInfo atlas = app().renderer->update_atlas();
    if (!world::atlas_has_texture(atlas, base)) {
        vm_warn_safely("[ruby] Splat-Boden: Atlas oder Basistextur fehlt; Einzeltextur bleibt");
        return Qfalse;
    }
    if (std::any_of(splats.begin(), splats.end(),
        [&atlas](const world::SplatRect& s) { return !world::atlas_has_texture(atlas, s.tex); }))
        vm_warn_safely("[ruby] Splat-Boden: Flächentextur fehlt im Atlas; Basis als Ersatz");
    if (!app().terrain->generate_splat(plan.width, plan.height, plan.res_x, plan.seed, plan.amplitude,
                                        base, splats, atlas) ||
        !app().renderer->set_terrain(app().terrain->vertices(), app().terrain->indices()))
        return fail("Mesh oder GPU-Puffer konnte nicht aufgebaut werden");
    app().renderer->set_terrain_tex(atlas.tex_index);
    app().renderer->set_terrain_tile(world::ATLAS_NEUTRAL_TILE);
    try {
        core::log_info("[ruby] Splat-Boden: " + std::to_string(splats.size()) +
                       " Fläche(n), Atlas-Slot " + std::to_string(atlas.tex_index));
    } catch (...) {}
    return Qtrue;
    } catch (const std::bad_alloc&) {
        return fail("Nicht genügend Speicher");
    } catch (const std::length_error&) {
        return fail("Speicherbereich überschritten");
    }
}

// Himmel-Verlauf: Rune::Gfx.sky(oben_hex, unten_hex)
VALUE rune_gfx_sky(VALUE, VALUE top, VALUE bottom) {
    if (!app().renderer) return Qnil;
    app().renderer->set_sky(vm_color(top, 0x3D6FB5, "Himmelfarbe"),
                            vm_color(bottom, 0xBFD8EC, "Himmelfarbe"));
    return Qnil;
}

// Distanznebel: Rune::Gfx.fog(an, farbe, distanz) - Etappe 17b.
VALUE rune_gfx_fog(VALUE, VALUE on, VALUE rgb, VALUE far_dist) {
    if (!app().renderer) return Qnil;
    double distance;
    if (!vm_number(far_dist, distance) || !world::atmosphere_valid_fog_far(distance)) {
        core::log_warn("[ruby] fog.far ungültig; 180 m als Ersatz");
        distance = 180.0;
    }
    app().renderer->set_fog(RTEST(on), vm_color(rgb, 0xBFD8EC, "Nebelfarbe"), static_cast<float>(distance));
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

// Mausfang durch Ruby steuern (z. B. für Launcher und Spielmenü).
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

// Simulations-Pause: Physik, Schwerkraft und Spielerbewegung anhalten;
// Kamera und UI laufen weiter. Die Spiellogik nutzt dies z. B. im Kampf.
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
    double width = 220.0, height = 220.0, res = 176, seed = 7, amp = 5.0;
    const bool valid = (NIL_P(v_width) || vm_number(v_width, width)) &&
                       (NIL_P(v_height) || vm_number(v_height, height)) &&
                       (NIL_P(v_res) || vm_number(v_res, res)) &&
                       (NIL_P(v_seed) || vm_number(v_seed, seed)) &&
                       (NIL_P(v_amp) || vm_number(v_amp, amp));
    if (NIL_P(v_height)) height = width;
    world::TerrainPlan p;
    if (valid) p = world::terrain_plan(width, height, res, seed, amp);
    else p.problem = world::TerrainProblem::Size;
    if (!valid || !p.ok()) {
        app().terrain->clear(); app().renderer->clear_terrain();
        try { core::log_warn(std::string("[ruby] Gelände ungültig: ") + world::terrain_problem_text(p.problem)); }
        catch (...) {}
        return Qfalse;
    }
    if (!app().terrain->generate(p.width, p.height, p.res_x, p.seed, p.amplitude) ||
        !app().renderer->set_terrain(app().terrain->vertices(), app().terrain->indices())) {
        app().terrain->clear(); app().renderer->clear_terrain();
        vm_warn_safely("[ruby] Gelände: Höhenpinsel/Rechenbudget oder GPU-Puffer ungültig; kein Gelände");
        return Qfalse;
    }
    try {
        core::log_info("[ruby] Gelände: " + std::to_string(p.width) + " x " +
                       std::to_string(p.height) + ", " + std::to_string(p.res_x) +
                       " Zellen, Seed " + std::to_string(p.seed) + ", Amplitude " +
                       std::to_string(p.amplitude));
    } catch (...) {}
    return Qtrue;
}

VALUE rune_world_height_at(VALUE, VALUE x, VALUE z) {
    if (!app().terrain || !app().terrain->ready()) return DBL2NUM(0.0);
    double dx, dz;
    if (!vm_number(x, dx) || !vm_number(z, dz) || !std::isfinite(dx) || !std::isfinite(dz))
        return DBL2NUM(0.0);
    // Auch endliche Double-Werte können jenseits des float-Bereichs liegen.
    dx = std::clamp(dx, -static_cast<double>(app().terrain->half_w()),
                         static_cast<double>(app().terrain->half_w()));
    dz = std::clamp(dz, -static_cast<double>(app().terrain->half_h()),
                         static_cast<double>(app().terrain->half_h()));
    return DBL2NUM(app().terrain->height_at(static_cast<float>(dx), static_cast<float>(dz)));
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
    if (NIL_P(list)) { app().terrain->clear_height_brushes(); return Qtrue; }
    auto fail = []() -> VALUE {
        app().terrain->invalidate_height_brushes();
        vm_warn_safely("[ruby] Höhenpinsel ungültig oder zu zahlreich; kein Gelände beim nächsten Aufbau");
        return Qfalse;
    };
    if (!RB_TYPE_P(list, T_ARRAY)) return fail();
    const long n = RARRAY_LEN(list);
    if (static_cast<unsigned long>(n) > world::TERRAIN_MAX_BRUSHES) return fail();
    try {
    std::vector<math::Vec4> brushes;
    brushes.reserve(static_cast<size_t>(n));
    for (long i = 0; i < n; ++i) {
        const VALUE e = rb_ary_entry(list, i);
        VALUE x, z, r, h;
        if (RB_TYPE_P(e, T_ARRAY) && RARRAY_LEN(e) >= 4) {
            x = rb_ary_entry(e, 0); z = rb_ary_entry(e, 1);
            r = rb_ary_entry(e, 2); h = rb_ary_entry(e, 3);
        } else if (RB_TYPE_P(e, T_HASH)) {
            // Karten-JSON enthält Hashes; direkte Ruby-API auch [x,z,r,h].
            x = vm_hash_get(e, "x"); z = vm_hash_get(e, "z");
            r = vm_hash_get(e, "r"); h = vm_hash_get(e, "h");
        } else return fail();
        double dx, dz, dr, dh;
        if (!vm_number(x, dx) || !vm_number(z, dz) || !vm_number(r, dr) || !vm_number(h, dh) ||
            !world::terrain_valid_brush(dx, dz, dr, dh)) return fail();
        brushes.push_back(math::Vec4{static_cast<float>(dx), static_cast<float>(dh),
                                     static_cast<float>(dz), static_cast<float>(dr)});
    }
    return app().terrain->set_height_brushes(brushes) ? Qtrue : Qfalse;
    } catch (const std::bad_alloc&) {
        return fail();
    } catch (const std::length_error&) {
        return fail();
    }
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
    // Die Engine-Wurzel ist bereits gültiges UTF-8; Ruby erhält denselben
    // Pfad ohne Umweg über die Windows-Systemcodepage.
    rb_define_const(mRune, "ROOT", rb_utf8_str_new(app().root.data(), static_cast<long>(app().root.size())));

    VALUE mGfx = rb_define_module_under(mRune, "Gfx");
    rb_define_module_function(mGfx, "clear_color=", RUBY_METHOD_FUNC(rune_gfx_clear_color_set), 1);
    rb_define_module_function(mGfx, "box", RUBY_METHOD_FUNC(rune_gfx_box), -1);
    rb_define_module_function(mGfx, "texture", RUBY_METHOD_FUNC(rune_gfx_texture), 4);
    rb_define_module_function(mGfx, "begin_project_textures", RUBY_METHOD_FUNC(rune_gfx_begin_project_textures), 0);
    rb_define_module_function(mGfx, "end_project_textures", RUBY_METHOD_FUNC(rune_gfx_end_project_textures), 0);
    rb_define_module_function(mGfx, "terrain_texture", RUBY_METHOD_FUNC(rune_gfx_terrain_texture), -1);
    rb_define_module_function(mGfx, "terrain_splats", RUBY_METHOD_FUNC(rune_gfx_terrain_splats), 2);
    rb_define_module_function(mGfx, "sky", RUBY_METHOD_FUNC(rune_gfx_sky), 2);
    rb_define_module_function(mGfx, "fog", RUBY_METHOD_FUNC(rune_gfx_fog), 3);
    rb_define_module_function(mGfx, "sun", RUBY_METHOD_FUNC(rune_gfx_sun), 4);
    rb_define_module_function(mGfx, "shadows=", RUBY_METHOD_FUNC(rune_gfx_shadows_set), 1);
    rb_define_module_function(mGfx, "rect", RUBY_METHOD_FUNC(rune_gfx_rect), -1);
    rb_define_module_function(mGfx, "text", RUBY_METHOD_FUNC(rune_gfx_text), -1);

    VALUE mAssets = rb_define_module_under(mRune, "Assets");
    rb_define_module_function(mAssets, "read_texture", RUBY_METHOD_FUNC(rune_assets_read_texture), 4);

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

    // Simulations-Schalter: true = Physik eingefroren.
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

enum class RubyBundle { Missing, Incomplete, Ready };

// Die mit der EXE ausgelieferte Standardbibliothek gehört zur verlinkten
// Ruby-DLL. Sie muss auch dann Vorrang haben, wenn ein fremdes RUBYLIB auf
// eine andere Ruby-Version zeigt. Andere zusätzliche Load-Pfade bleiben
// erhalten; es wird keine Umgebungsvariable verändert.
RubyBundle prefer_bundled_ruby_library() {
    std::wstring exe, version, platform;
    const std::string api = std::to_string(RUBY_API_VERSION_MAJOR) + "." +
                            std::to_string(RUBY_API_VERSION_MINOR) + "." +
                            std::to_string(RUBY_API_VERSION_TEENY);
    if (!core::win_path::executable_directory(exe) ||
        !core::win_path::to_wide(api, version) ||
        !core::win_path::to_wide(ruby_platform, platform)) return RubyBundle::Incomplete;
    const std::wstring library = exe + L"\\lib\\ruby\\" + version;
    const std::wstring native = library + L"\\" + platform;
    const DWORD attrs = GetFileAttributesW(library.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        const DWORD error = GetLastError();
        return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND ?
            RubyBundle::Missing : RubyBundle::Incomplete;
    }
    if ((attrs & FILE_ATTRIBUTE_DIRECTORY) == 0 || !core::win_path::directory_exists(native))
        return RubyBundle::Incomplete;
    // Ein halb kopiertes Paket darf nicht unbemerkt mit einer fremden Ruby-
    // Installation vermischt werden. Windows-Ruby benutzt .so-Erweiterungen.
    for (const auto& file : {library + L"\\json.rb", library + L"\\fileutils.rb",
                             native + L"\\json\\ext\\parser.so", native + L"\\json\\ext\\generator.so",
                             native + L"\\enc\\encdb.so"}) {
        const DWORD file_attrs = GetFileAttributesW(file.c_str());
        if (file_attrs == INVALID_FILE_ATTRIBUTES || (file_attrs & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            std::string name;
            if (core::win_path::to_utf8(file, name)) core::log_error("Ruby-Laufzeitdatei fehlt: " + name);
            return RubyBundle::Incomplete;
        }
    }

    std::string library_utf8, native_utf8;
    if (!core::win_path::to_utf8(library, library_utf8) ||
        !core::win_path::to_utf8(native, native_utf8)) return RubyBundle::Incomplete;
    std::replace(library_utf8.begin(), library_utf8.end(), '\\', '/');
    std::replace(native_utf8.begin(), native_utf8.end(), '\\', '/');
    const VALUE paths = rb_gv_get("$LOAD_PATH");
    if (!RB_TYPE_P(paths, T_ARRAY)) return RubyBundle::Incomplete;
    rb_ary_unshift(paths, rb_utf8_str_new(library_utf8.data(), static_cast<long>(library_utf8.size())));
    rb_ary_unshift(paths, rb_utf8_str_new(native_utf8.data(), static_cast<long>(native_utf8.size())));
    core::log_info("Ruby-VM: mitgelieferte Standardbibliothek " + library_utf8);
    return RubyBundle::Ready;
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
    std::wstring root_wide;
    if (!core::win_path::to_wide(root, root_wide)) {
        core::log_error("Ruby-VM: ungültiger UTF-8-Installationspfad");
        return false;
    }
    app().root = root;

    ruby_sysinit(&argc, &argv);

    {
        RUBY_INIT_STACK;
        ruby_init();
    }

    // ruby_init() allein durchläuft nicht ruby_process_options(), das beim
    // normalen Windows-Ruby die externe und Dateisystem-Kodierung setzt.
    // Ohne diesen Schritt kann Dir.children die System-Codepage benutzen
    // und nicht darstellbare Projekt-/Kartennamen durch '?' ersetzen.
    rb_enc_set_default_external(rb_enc_from_encoding(rb_utf8_encoding()));
    if (rb_filesystem_encindex() != rb_utf8_encindex()) {
        core::log_error("Ruby-VM: Dateisystem-Kodierung ist nicht UTF-8");
        ruby_finalize();
        return false;
    }

    ruby_init_loadpath();
    const RubyBundle bundle = prefer_bundled_ruby_library();
    if (bundle == RubyBundle::Incomplete) {
        core::log_error("Ruby-VM: Mitgelieferte Standardbibliothek ist unvollständig; "
                        "bitte den vollständigen Build erneut erstellen oder entpacken.");
        ruby_finalize();
        return false;
    }
    if (bundle == RubyBundle::Missing) {
        core::log_warn("Ruby-VM: Mitgelieferte Standardbibliothek fehlt. "
                       "Bitte build.ps1 erneut ausführen und lib/ruby neben die EXE kopieren; "
                       "alternativ ein passendes RUBYLIB setzen.");
    }
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
    const bool drive_absolute = p.size() >= 3 && p[1] == ':' && p[2] == '/';
    const bool unc_absolute = p.size() >= 2 && p[0] == '/' && p[1] == '/';
    if (!drive_absolute && !unc_absolute) {
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

    std::wstring checked;
    if (!core::win_path::to_wide(p, checked)) {
        core::log_error("Ruby-Skriptpfad ist kein gültiges UTF-8");
        return false;
    }
    int state = 0;
    rb_load_protect(rb_utf8_str_new(p.data(), static_cast<long>(p.size())), 0, &state);

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
    reset_project_assets();
    if (!g_ruby_ready) return;
    g_ruby_ready = false;
    ruby_finalize();
}

} // namespace rune::ruby
