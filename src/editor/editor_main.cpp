// ============================================================================
//  RuneEditor - eigenständiger Scene-Editor (Etappe 17, Variante B).
//
//  Der Editor ist KEIN Teil der Game-Engine: Er ist ein eigenes Programm mit
//  nativen Windows-Steuerelementen (Werkzeug-Palette, Objektliste,
//  Eigenschaften-Feldern) und einem 3D-Viewport als Vorschau. Er arbeitet
//  ausschliesslich auf den Kartendaten (JSON) — Spiellogik (Physik, Events,
//  NPCs, Interpreter) existiert hier nicht und kann daher nichts kaputtmachen.
//
//  Eingebettetes CRuby wird nur einmalig beim Start benutzt, um die
//  prozeduralen Texturen zu erzeugen (game/editor/boot.rb) — danach läuft
//  alles in C++.
//
//  Layout:
//    +--------------------------------------------------------------+
//    | Menüleiste (Datei / Karte / Ansicht / Hilfe)                |
//    | Werkzeuge |     3D-Viewport             | Objekte            |
//    | (Prefabs) |  freie Kamera, Klicken =    | (Liste)            |
//    | Buttons   |  Setzen/Auswählen          | Eigenschaften      |
//    |--------------------------------------------------------------|
//    | Statusleiste                                                 |
//    +--------------------------------------------------------------+
// ============================================================================

#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <windowsx.h>
#include <wincodec.h>
#include <shellapi.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>   // Etappe 19k.2: std::atof (Block-Zonen-Feld)
#include <cstring>
#include <string>
#include <vector>

#include "app.h"
#include "core/log.h"
#include "core/sound.h"
#include "editor/json.h"
#include "gfx/camera.h"      // nur fuer Kamera-Konstanten, falls noetig
#include "gfx/renderer.h"
#include "math/math.h"
#include "ruby/vm.h"
#include "world/terrain.h"

namespace rune::editor {

// ------------------------------------------------------------ Steuer-IDs
enum {
    ID_MENU_SAVE      = 100,
    ID_MENU_EXIT      = 101,
    ID_MENU_RESETCAM  = 102,
    ID_MENU_HELP      = 103,
    ID_MENU_MAPPROPS  = 104,
    ID_MENU_MAPNEW    = 105,
    ID_MENU_MAPDEL    = 106,
    ID_MENU_EVENTS    = 107,
    ID_MENU_SCRIPTS   = 108,
    ID_MENU_IMPORT_TEX = 109,
    ID_MENU_MATERIALS = 110,
    ID_MENU_DATABASE  = 111,
    ID_MENU_SOUNDTEST = 112,   // Etappe 17h
    ID_MENU_PLAYTEST  = 113,   // Etappe 17h
    ID_MENU_PROJECT   = 114,   // Etappe 18b: Projekt wechseln...
    ID_MENU_TILESET   = 115,   // Etappe 18c: Tileset-Editor...
    ID_MENU_PROBLEMS  = 116,   // Etappe 18h: Probleme-Dialog (Backlog)
    ID_MENU_MAP_BASE  = 200,   // dynamische Karten-Einträge danach

    IDC_PREFAB_LIST   = 3001,
    IDC_BTN_PLACE     = 3002,
    IDC_BTN_SELECT    = 3003,
    IDC_BTN_ROTATE    = 3004,
    IDC_BTN_DELETE    = 3005,
    IDC_BTN_DUPLICATE = 3006,
    IDC_BTN_MOVE      = 3007,
    IDC_BTN_UNDO      = 3008,
    IDC_BTN_REDO      = 3009,
    IDC_BTN_PAINT     = 3026,   // Etappe 18e: Boden malen (Splat-Pinsel)
    IDC_OBJ_LIST      = 3010,
    IDC_ED_X          = 3011,
    IDC_ED_Z          = 3012,
    IDC_ED_ROT        = 3013,
    IDC_BTN_APPLY     = 3014,
    IDC_VIEWPORT      = 3015,
    IDC_STATUSBAR     = 3016,
    // Etappe 18j: Timer, der während des interaktiven Resizes rendert.
    IDT_RESIZE        = 1,
    IDC_LBL_TOOLS     = 3017,
    IDC_LBL_OBJECTS   = 3018,
    IDC_GRP_PROPS     = 3019,
    IDC_LBL_X         = 3020,
    IDC_LBL_Z         = 3021,
    IDC_LBL_ROT       = 3022,
    IDC_ED_TEX        = 3023,
    IDC_LBL_TEX       = 3024,
    IDC_TOOLBAR       = 3025,
    // Etappe 18k: Kartenbaum links oben (RPG-Maker-XP-Kartenliste).
    IDC_MAPTREE       = 3027,
    IDC_LBL_MAPS      = 3028,
    // Etappe 18l: Höhen-Malwerkzeug (Berge/Senken)
    IDC_BTN_HEIGHT    = 3029,
    // Etappe 19c: Gruppenrahmen der linken Spalte + Kontextmenü Viewport/Liste
    IDC_GRP_TOOL      = 3030,
    IDC_GRP_PREFAB    = 3031,
    IDC_GRP_ACT       = 3032,
    ID_CTX_PLACE      = 3120,
    ID_CTX_FOCUS      = 3121,
    ID_CTX_DUP        = 3122,
    ID_CTX_DEL        = 3123,
    // Kontextmenü des Kartenbaums (postet als WM_COMMAND an main_proc)
    IDM_TREE_PROPS    = 3110,
    IDM_TREE_NEW      = 3111,
    IDM_TREE_RENAME   = 3112,
    IDM_TREE_DELETE   = 3113,
};

// ------------------------------------------------------------ Prefabs
struct PrefabPart {
    math::Vec3 d;          // Versatz relativ zur Standfläche
    math::Vec3 s;          // Grösse
    uint32_t   c = 0xFFFFFF;
    int        tex = -1;   // Index im Renderer (nach Texturen-Boot)
    float      tile = 0.0f;
};

struct Prefab {
    std::string id;
    std::string name;
    std::vector<PrefabPart> parts;
    float radius = 0.0f;   // grober Bodenradius (Nahe-Löschung, Anzeige)
};

// ------------------------------------------------------------ Materialien (17f)
// Importierte Grafiken liegen als rohe RGBA-Datei unter data/textures/, die
// Metadaten (Name, Grösse, Kachel, Quelle) in data/textures.json.
struct MaterialInfo {
    std::string name;
    std::string file;      // Dateiname in data/textures/
    std::string source;    // Originaldatei beim Import
    int w = 0, h = 0;
    double tile = 2.0;
    bool imported = false;
};

// ------------------------------------------------------------
//  Eigene Fenster-Icons (Etappe 18a): klein 16x16 und gross 32x32,
//  prozedural gezeichnet - kein Ressourcen-File, wie bei der Toolbar.
//  Editor: gelber Runenstein auf dunklem Grund; Spiel teilt sich das
//  Symbol (main.cpp ruft dieselbe Idee mit blauem Kristall).
// ------------------------------------------------------------

// Zeichnet das Editor-Icon (Runenstein) in ein HDC der Grösse sz.
void draw_icon_editor(HDC dc, int sz) {
    HBRUSH bg = CreateSolidBrush(RGB(24, 32, 46));
    RECT rc{ 0, 0, sz, sz };
    FillRect(dc, &rc, bg);
    DeleteObject(bg);

    // Runenstein: grauer Block mit gelber Rune.
    HBRUSH face  = CreateSolidBrush(RGB(150, 148, 140));
    HPEN   edge  = CreatePen(PS_SOLID, 1, RGB(70, 68, 64));
    HGDIOBJ op = SelectObject(dc, edge);
    HGDIOBJ ob = SelectObject(dc, face);
    Rectangle(dc, sz / 6, sz / 8, 5 * sz / 6, 7 * sz / 8);
    SelectObject(dc, op);
    SelectObject(dc, ob);
    DeleteObject(face);
    DeleteObject(edge);

    // Rune: Winkel-Schriftzug aus dicken gelben Linien.
    HPEN rune = CreatePen(PS_SOLID, std::max(1, sz / 8), RGB(255, 210, 70));
    HGDIOBJ orr = SelectObject(dc, rune);
    const int cx = sz / 2;
    const int cy = sz / 2;
    MoveToEx(dc, cx - sz / 5, cy + sz / 4, nullptr);
    LineTo(dc, cx - sz / 5, cy - sz / 4);          // Schaft
    LineTo(dc, cx + sz / 6, cy - sz / 12);         // Schräge oben
    MoveToEx(dc, cx - sz / 5, cy, nullptr);
    LineTo(dc, cx + sz / 6, cy + sz / 4);          // Bein
    SelectObject(dc, orr);
    DeleteObject(rune);
}

// Ein Icon einer Grösse aus einer Zeichenfunktion erzeugen.
// 18a-Fix: CreateIconIndirect verlangt eine ECHT monochrome Maske -
// CreateCompatibleBitmap erzeugt ein Farb-Bitmap, das als Maske
// undefiniert ist und den Titelzeichnungs-Pfad zum Absturz bringen
// kann. Zusätzlich wird das Farb-Bitmap als 32bpp-DIB mit Alpha=255
// angelegt: DWM wertet uninitialisierte Alpha-Bytes als Transparenz.
HICON make_icon(void (*draw)(HDC, int), int sz) {
    HDC screen = GetDC(nullptr);

    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = sz;
    bi.bmiHeader.biHeight = -sz;   // top-down
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP bmp = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    HDC mem = CreateCompatibleDC(screen);
    HGDIOBJ old = SelectObject(mem, bmp);
    draw(mem, sz);
    SelectObject(mem, old);
    // Alle Pixel opak (GDI schreibt kein Alpha in DIB-Sections).
    unsigned char* px = static_cast<unsigned char*>(bits);
    for (int i = 3; i < sz * sz * 4; i += 4) px[i] = 0xFF;
    DeleteDC(mem);

    HBITMAP mask = CreateBitmap(sz, sz, 1, 1, nullptr);   // echte Mono-Maske
    HDC mdc = CreateCompatibleDC(screen);
    HGDIOBJ om = SelectObject(mdc, mask);
    RECT rc{ 0, 0, sz, sz };
    FillRect(mdc, &rc, reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
    SelectObject(mdc, om);
    DeleteDC(mdc);

    ICONINFO ii{};
    ii.fIcon = TRUE;
    ii.hbmMask = mask;
    ii.hbmColor = bmp;
    HICON icon = CreateIconIndirect(&ii);

    DeleteObject(bmp);
    DeleteObject(mask);
    ReleaseDC(nullptr, screen);
    return icon;
}

// Beide Fenster-Icons (klein + gross) setzen.
void set_window_icons(HWND win, void (*draw)(HDC, int)) {
    HICON big = make_icon(draw, 32);
    HICON small = make_icon(draw, 16);
    SendMessageW(win, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(big));
    SendMessageW(win, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(small));
}

// ------------------------------------------------------------ Editor-Zustand
struct Editor {
    HWND main = nullptr;
    HWND view = nullptr;
    HWND status = nullptr;
    HWND list_prefab = nullptr;
    HWND map_tree = nullptr;      // Etappe 18k: Kartenbaum links oben
    HWND list_objs = nullptr;
    HWND ed_x = nullptr;
    HWND ed_z = nullptr;
    HWND ed_rot = nullptr;
    HWND ed_tex = nullptr;
    HFONT font = nullptr;
    HMENU menu = nullptr;
    HMENU menu_map = nullptr;      // Karten-Untermenü (dynamisch)
    HWND   props_dlg = nullptr;    // offener Dialog (moduslos)
    int    dlg_mode = 0;           // 1 = Eigenschaften, 2 = Neue Karte

    gfx::Renderer*  renderer = nullptr;
    world::Terrain* terrain = nullptr;

    std::string root;                 // Engine-Wurzel (übergeordnetes Verz.)
    std::string project_dir;          // z.B. projekte/dorf
    std::string project_name;

    std::vector<std::string> map_files;   // Dateinamen in maps/
    std::vector<std::string> map_names;   // Anzeigenamen
    int cur_map = -1;

    JsonValue  map_root;              // ganze Karte (events/npcs bleiben drin)
    JsonValue* objects = nullptr;     // zeigt in map_root

    std::vector<Prefab> prefabs;
    std::vector<std::string> tex_names;   // Texturnamen der Prefabs (+gras)
    int  sel_prefab = 0;
    int  sel_obj = -1;
    bool place_mode = true;
    bool move_mode = false;        // Verschieben-Werkzeug (Drag im Viewport)
    bool paint_mode = false;       // Etappe 18e: Boden-Malwerkzeug (Splats)
    bool height_mode = false;      // Etappe 18l: Berge/Senken malen
    float place_rot = 0.0f;        // Etappe 19c: Drehung der Platzier-Vorschau
    int  obj_sort_col = -1;        // Etappe 19c: Sortierspalte (-1 = Datenreihenfolge)
    bool obj_sort_asc = true;      // Etappe 19c: Sortierrichtung
    std::vector<int> obj_order;    // Etappe 19c: Listenzeile -> Index in objects
    HMENU ctx_obj = nullptr;       // Etappe 19c: Kontextmenü Viewport/Objektliste
    float paint_size = 8.0f;       // Brush-Grösse in Metern (Kantenlänge)
    std::string paint_tex = "weg"; // Textur des Pinsels
    bool dragging = false;         // gerade Objekt per Maus verschieben
    int  obj_drag = -1;            // Index des gezogenen Objekts
    float drag_y = 0.0f;           // Höhenebene des gezogenen Objekts
    std::vector<std::string> undo_stack;   // JSON-Schnappschüsse (vor Aktion)
    std::vector<std::string> redo_stack;
    float grid = 1.0f;
    bool dirty = false;

    // Freie Kamera
    math::Vec3 cam_pos{ 0.0f, 12.0f, 26.0f };
    float cam_yaw = 0.0f;
    float cam_pitch = -0.55f;
    bool  look_drag = false;
    POINT last_mouse{};

    // Maus im Viewport (Client-Koordinaten) + Terrain-Treffer für Status/Geist
    float mouse_x = 0.0f, mouse_y = 0.0f;
    bool  hover_valid = false;
    math::Vec3 hover_pos{};

    float fps = 0.0f;
    int resize_ticks = 0;   // Etappe 18j: Frames aus dem Resize-Timer
    std::wstring status_mode = L"Bereit";
};

static Editor g;

// Vorwärtsdeklarationen (Definitionen weiter unten)
void apply_map_atmosphere_editor();
void rebuild_map_menu();
void open_map_props();
void open_map_new();
void delete_current_map();
void apply_props_from_dlg();
void apply_new_from_dlg();
void push_undo();
void undo_action();
void redo_action();
void restore_objects(const std::string& text);
void update_title();
void sync_object_list();
void sync_property_fields();

// Etappe 18b: Projekt-Auswahl im Editor (bisher fix auf "dorf").
bool switch_project(const std::string& name);
void open_project_dialog();
void scan_maps();
void rebuild_map_menu();
void status_mode(const wchar_t* text);

// Etappe 18k: Kartenbaum (XP-Kartenliste). Item-LPARAM = Karten-Index.
void maptree_fill();
void maptree_select_current();

// Etappe 18c: Tileset-Editor.
void open_tileset_dialog();
uint32_t parse_hex_color(const std::string& s);

// Etappe 18d: Splat-Boden (Boden-Flächen pro Karte).
std::vector<world::SplatRect> read_splats(const JsonValue* t);
void regen_terrain();
void json_remove_key(JsonValue& v, const std::string& key);
bool splat_edit(float cx, float cz, float sw, float sh,
               const std::string& tex, bool erase, bool snap);   // Etappe 18e
// Etappe 18l: Berge/Senken malen (Höhen-Pinsel im Karten-JSON).
float height_step();
int  height_edit(float cx, float cz, bool sink);

// Etappe 17d: Event-Editor (eigenes modusloses Fenster).
void open_event_editor();
void close_event_editor();
bool event_editor_focused();

// Etappe 17e: Script-Editor (scripts.json des Projekts).
void open_script_editor();
bool script_editor_focused();

// Etappe 17f: Grafik-Import + Materialbase.
void open_import_texture();
void open_material_dialog();
bool material_editor_focused();

// Etappe 17g: Datenbank-Dialog (Helden/Items/Gold).
void open_database_dialog();
bool database_editor_focused();

// Etappe 17h: Sound-Test + Playtest.
void open_soundtest_dialog();
bool soundtest_focused();
void start_playtest();

// Etappe 17h: Werkzeugleiste mit prozeduralen Icons (kein Ressourcen-File).
void create_toolbar(HWND parent);
int  toolbar_height();

// ------------------------------------------------------------
//  Hilfsfunktionen: UTF-8 <-> UTF-16, Dateien
// ------------------------------------------------------------
std::wstring utf8_to_wide(const std::string& s) {
    if (s.empty()) return std::wstring();
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(static_cast<size_t>(n) > 0 ? n - 1 : 0, L'\0');
    if (n > 1) MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], n);
    return w;
}

std::string wide_to_utf8(const std::wstring& w) {
    if (w.empty()) return std::string();
    const int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string s(static_cast<size_t>(n) > 0 ? n - 1 : 0, '\0');
    if (n > 1) WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, &s[0], n, nullptr, nullptr);
    return s;
}

bool read_file(const std::string& path, std::string& out) {
    FILE* f = nullptr;
    if (fopen_s(&f, path.c_str(), "rb") != 0 || !f) return false;
    out.clear();
    char buf[65536];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) out.append(buf, n);
    std::fclose(f);
    return true;
}

bool write_file(const std::string& path, const std::string& data) {
    FILE* f = nullptr;
    if (fopen_s(&f, path.c_str(), "wb") != 0 || !f) return false;
    const size_t n = std::fwrite(data.data(), 1, data.size(), f);
    std::fclose(f);
    return n == data.size();
}

// Engine-Wurzel finden (Editor exe liegt in build/, Wurzel ist der Parent).
std::string find_root() {
    char exe[MAX_PATH];
    GetModuleFileNameA(nullptr, exe, MAX_PATH);
    std::string dir = exe;
    const size_t slash = dir.find_last_of("\\/");
    if (slash != std::string::npos) dir = dir.substr(0, slash);
    if (GetFileAttributesA((dir + "\\game").c_str()) != INVALID_FILE_ATTRIBUTES) return dir;
    if (GetFileAttributesA((dir + "\\..\\game").c_str()) != INVALID_FILE_ATTRIBUTES) return dir + "\\..";
    char cwd[MAX_PATH];
    GetCurrentDirectoryA(MAX_PATH, cwd);
    return cwd;
}

// ------------------------------------------------------------
//  Prefabs aus game/data/prefabs.json (eine Quelle mit dem Spiel)
// ------------------------------------------------------------
void load_prefabs() {
    g.prefabs.clear();
    std::string text;
    if (!read_file(g.root + "\\game\\data\\prefabs.json", text)) {
        core::log_error("prefabs.json nicht gefunden");
        return;
    }
    JsonValue j = json_parse(text);
    const JsonValue* list = j.find("prefabs");
    if (!list || !list->is_arr()) {
        core::log_error("prefabs.json: 'prefabs' fehlt");
        return;
    }
    for (const JsonValue& p : list->arr) {
        Prefab pre;
        if (const JsonValue* id = p.find("id")) pre.id = id->str;
        if (const JsonValue* nm = p.find("name")) pre.name = nm->str;
        if (const JsonValue* parts = p.find("parts")) {
            for (const JsonValue& q : parts->arr) {
                PrefabPart part;
                if (const JsonValue* d = q.find("d")) {
                    part.d.x = static_cast<float>(d->arr.size() > 0 ? d->arr[0].number() : 0);
                    part.d.y = static_cast<float>(d->arr.size() > 1 ? d->arr[1].number() : 0);
                    part.d.z = static_cast<float>(d->arr.size() > 2 ? d->arr[2].number() : 0);
                }
                if (const JsonValue* s = q.find("s")) {
                    part.s.x = static_cast<float>(s->arr.size() > 0 ? s->arr[0].number() : 1);
                    part.s.y = static_cast<float>(s->arr.size() > 1 ? s->arr[1].number() : 1);
                    part.s.z = static_cast<float>(s->arr.size() > 2 ? s->arr[2].number() : 1);
                }
                if (const JsonValue* c = q.find("c")) part.c = static_cast<uint32_t>(c->number());
                if (const JsonValue* t = q.find("tex")) {
                    part.tex = g.renderer->texture_index(t->str);
                    if (std::find(g.tex_names.begin(), g.tex_names.end(), t->str) ==
                        g.tex_names.end()) {
                        g.tex_names.push_back(t->str);
                    }
                }
                if (const JsonValue* ti = q.find("tile")) part.tile = static_cast<float>(ti->number());
                pre.parts.push_back(part);
                // Bodenradius: grösste horizontale Ausdehnung (wie Prefabs.radius_of)
                const float ext = std::sqrt(part.d.x * part.d.x + part.d.z * part.d.z) +
                                  std::max(part.s.x, part.s.z) * 0.5f;
                pre.radius = std::max(pre.radius, ext);
            }
        }
        g.prefabs.push_back(std::move(pre));
    }
    char buf[64];
    std::snprintf(buf, sizeof(buf), "Prefabs geladen: %d", static_cast<int>(g.prefabs.size()));
    core::log_info(buf);
}

// ------------------------------------------------------------
//  Etappe 17f: Grafik-Import + Materialbase
//
//  PNG/BMP werden per Windows Imaging Component (WIC) dekodiert und als rohe
//  RGBA-Datei unter projekte/<p>/data/textures/<name>.rgba gespeichert; die
//  Metadaten stehen in data/textures.json. Das Spiel liest beides in
//  Textures.register_project (Ruby braucht keinen Bilddecoder).
// ------------------------------------------------------------
static std::vector<MaterialInfo> g_materials;

std::string mat_dir() {
    return g.project_dir + "\\textures";
}

std::string mat_json_path() {
    return g.project_dir + "\\textures.json";
}

// Beliebige Bilddatei (PNG/BMP/JPG) -> 8-Bit-RGBA, Zeilen von oben nach unten.
bool decode_image_wic(const std::string& path, int& out_w, int& out_h,
                      std::vector<uint8_t>& out_rgba) {
    out_w = out_h = 0;
    out_rgba.clear();
    if (path.empty()) return false;

    const std::wstring wpath = utf8_to_wide(path);
    IWICImagingFactory* factory = nullptr;
    IWICBitmapDecoder* decoder = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICFormatConverter* conv = nullptr;
    bool ok = false;

    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&factory));
    if (SUCCEEDED(hr)) {
        hr = factory->CreateDecoderFromFilename(wpath.c_str(), nullptr, GENERIC_READ,
                                                WICDecodeMetadataCacheOnDemand, &decoder);
    }
    if (SUCCEEDED(hr)) hr = decoder->GetFrame(0, &frame);
    if (SUCCEEDED(hr)) hr = factory->CreateFormatConverter(&conv);
    if (SUCCEEDED(hr)) {
        hr = conv->Initialize(frame, GUID_WICPixelFormat32bppRGBA,
                              WICBitmapDitherTypeNone, nullptr, 0.0,
                              WICBitmapPaletteTypeCustom);
    }
    UINT w = 0, h = 0;
    if (SUCCEEDED(hr)) hr = conv->GetSize(&w, &h);
    if (SUCCEEDED(hr) && w > 0 && h > 0 && w <= 4096 && h <= 4096) {
        const UINT stride = w * 4;
        std::vector<uint8_t> px(static_cast<size_t>(stride) * h);
        hr = conv->CopyPixels(nullptr, stride, static_cast<UINT>(px.size()), px.data());
        if (SUCCEEDED(hr)) {
            out_w = static_cast<int>(w);
            out_h = static_cast<int>(h);
            out_rgba = std::move(px);
            ok = true;
        }
    }

    if (conv) conv->Release();
    if (frame) frame->Release();
    if (decoder) decoder->Release();
    if (factory) factory->Release();
    return ok;
}

void mat_register(const MaterialInfo& m) {
    if (!g.renderer) return;
    std::string bytes;
    if (!read_file(mat_dir() + "\\" + m.file, bytes)) return;
    if (static_cast<int>(bytes.size()) != m.w * m.h * 4) {
        core::log_warn("Textur '" + m.name + "': Grösse passt nicht zur Datei");
        return;
    }
    g.renderer->add_texture(m.name, m.w, m.h,
                            reinterpret_cast<const uint8_t*>(bytes.data()));
}

// textures.json + .rgba-Dateien laden; Texturen im Renderer anmelden.
void mat_load() {
    g_materials.clear();
    std::string text;
    if (!read_file(mat_json_path(), text) || text.empty()) return;

    JsonValue root;
    try {
        root = json_parse(text);
    } catch (const std::exception&) {
        core::log_error("textures.json: ungültiges JSON");
        return;
    }
    const JsonValue* arr = root.is_arr() ? &root : root.find("textures");
    if (!arr || !arr->is_arr()) return;

    for (const JsonValue& e : arr->arr) {
        if (!e.is_obj()) continue;
        MaterialInfo m;
        if (const JsonValue* v = e.find("name")) m.name = v->str;
        if (const JsonValue* v = e.find("file")) m.file = v->str;
        if (const JsonValue* v = e.find("source")) m.source = v->str;
        if (const JsonValue* v = e.find("w")) m.w = static_cast<int>(v->number());
        if (const JsonValue* v = e.find("h")) m.h = static_cast<int>(v->number());
        if (const JsonValue* v = e.find("tile")) m.tile = v->number(2.0);
        m.imported = true;
        if (m.name.empty()) continue;
        if (m.file.empty()) m.file = m.name + ".rgba";
        if (m.w <= 0) m.w = 64;
        if (m.h <= 0) m.h = 64;
        g_materials.push_back(m);
        mat_register(m);
    }
    core::log_info("Materialien geladen: " + std::to_string(g_materials.size()));
}

bool mat_save() {
    JsonValue root = JsonValue::make_obj();
    JsonValue arr = JsonValue::make_arr();
    for (const MaterialInfo& m : g_materials) {
        if (!m.imported) continue;
        JsonValue e = JsonValue::make_obj();
        e.set("name", JsonValue::make_str(m.name));
        e.set("file", JsonValue::make_str(m.file));
        e.set("w", JsonValue::make_num(m.w, true));
        e.set("h", JsonValue::make_num(m.h, true));
        e.set("tile", JsonValue::make_num(m.tile, false));
        e.set("source", JsonValue::make_str(m.source));
        arr.arr.push_back(std::move(e));
    }
    root.set("textures", std::move(arr));
    return write_file(mat_json_path(), json_write(root));
}

// Dateiname -> eindeutiger Materialname (klein, nur a-z/0-9/_).
std::string mat_slug(std::string base) {
    const size_t slash = base.find_last_of("\\/");
    if (slash != std::string::npos) base = base.substr(slash + 1);
    const size_t dot = base.find_last_of('.');
    if (dot != std::string::npos) base = base.substr(0, dot);

    std::string out;
    for (char c : base) {
        if (c >= 'A' && c <= 'Z') out += static_cast<char>(c - 'A' + 'a');
        else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_') out += c;
        else out += '_';
    }
    std::string cleaned;
    for (char c : out) {
        if (c == '_' && (cleaned.empty() || cleaned.back() == '_')) continue;
        cleaned += c;
    }
    if (!cleaned.empty() && cleaned.front() == '_') cleaned.erase(cleaned.begin());
    if (cleaned.empty()) cleaned = "textur";

    auto used = [&](const std::string& name) {
        for (const MaterialInfo& m : g_materials) {
            if (m.name == name) return true;
        }
        return g.renderer && g.renderer->texture_index(name) >= 0;
    };
    std::string candidate = cleaned;
    int n = 2;
    while (used(candidate)) candidate = cleaned + "_" + std::to_string(n++);
    return candidate;
}

// Bild importieren: dekodieren, .rgba + textures.json schreiben, anmelden.
bool mat_import_file(const std::string& src, std::string& out_name) {
    if (g.project_dir.empty()) {
        core::log_error("Import: kein Projektverzeichnis");
        return false;
    }
    int w = 0, h = 0;
    std::vector<uint8_t> rgba;
    if (!decode_image_wic(src, w, h, rgba)) {
        core::log_error("Import: Bild nicht lesbar: " + src);
        return false;
    }
    CreateDirectoryA(mat_dir().c_str(), nullptr);

    MaterialInfo m;
    m.name = out_name.empty() ? mat_slug(src) : out_name;
    m.file = m.name + ".rgba";
    m.source = src;
    m.w = w;
    m.h = h;
    m.tile = 2.0;
    m.imported = true;

    const std::string data(reinterpret_cast<const char*>(rgba.data()), rgba.size());
    if (!write_file(mat_dir() + "\\" + m.file, data)) {
        core::log_error("Import: Schreiben fehlgeschlagen: " + m.file);
        return false;
    }

    bool replaced = false;
    for (MaterialInfo& old : g_materials) {
        if (old.name == m.name) { old = m; replaced = true; break; }
    }
    if (!replaced) g_materials.push_back(m);

    mat_register(m);
    mat_save();
    out_name = m.name;
    core::log_info("Textur importiert: " + m.name + " (" + std::to_string(w) + "x" +
                   std::to_string(h) + ") aus " + src);
    return true;
}

// Verfügbare Materialnamen (Prefab-Texturen + importierte), sortiert.
std::vector<std::string> mat_all_names() {
    std::vector<std::string> names = g.tex_names;
    if (std::find(names.begin(), names.end(), "gras") == names.end()) names.push_back("gras");
    for (const MaterialInfo& m : g_materials) {
        if (std::find(names.begin(), names.end(), m.name) == names.end()) {
            names.push_back(m.name);
        }
    }
    std::sort(names.begin(), names.end());
    return names;
}

const MaterialInfo* mat_find(const std::string& name) {
    for (const MaterialInfo& m : g_materials) {
        if (m.name == name) return &m;
    }
    return nullptr;
}

// ------------------------------------------------------------
//  Karten laden/speichern (map_root bleibt komplett erhalten)
// ------------------------------------------------------------
// ============================================================================
//  Etappe 18h: Problem-Backlog — der Editor sammelt selbst gefundene Fehler
//  der Kartendaten (unbekannte Befehle/Trigger, kaputte Seiten, fehlende
//  Materialien) und zeigt sie im Probleme-Dialog (F10) an. Der Scan läuft
//  automatisch nach jedem Kartenladen und per Knopf im Dialog.
// ============================================================================
enum { IDPB_LIST = 5301, IDPB_SCAN = 5302, IDPB_CLOSE = 5303, IDPB_L_COUNT = 5304 };

struct EditorIssue {
    int severity;          // 1 = Warnung, 2 = Fehler
    std::string area;
    std::string text;
};
static std::vector<EditorIssue> g_issues;

static void issue_clear() {
    g_issues.clear();
}

static void issue_report(int severity, const std::string& area, const std::string& text) {
    g_issues.push_back({ severity, area, text });
    core::log_warn("[Backlog] " + area + ": " + text);
}

static bool issue_known_trigger(const std::string& t) {
    return t == "action" || t == "touch" || t == "auto" || t == "parallel";
}

// Bekannte Befehlstypen (eigene Liste, damit der Scan VOR der Event-Editor-
// Sektion stehen kann — EV_TYPES wird erst dort definiert).
const char* const ISSUE_CMD_TYPES[] = {
    "text", "choices", "log",
    "wait", "switch", "toggle_switch", "variable",
    "if_switch", "if_variable", "script",
    "teleport", "move_player", "heal",
    "weather", "effect", "sound",
    "item", "gold",
    "save_game", "load_game",
    "self_switch", "if_self_switch", "fade",
    "move_route", "input_number", "battle"
};
const int ISSUE_CMD_TYPE_COUNT =
    static_cast<int>(sizeof(ISSUE_CMD_TYPES) / sizeof(ISSUE_CMD_TYPES[0]));

// Befehlsliste rekursiv prüfen (inkl. Dann-/Sonst-/Choices-Zweige).
static void issues_scan_commands(const JsonValue& list, const std::string& where) {
    if (!list.is_arr()) return;
    for (int i = 0; i < static_cast<int>(list.arr.size()); ++i) {
        const JsonValue& c = list.arr[i];
        const std::string w = where + " Befehl #" + std::to_string(i + 1);
        if (!c.is_obj()) { issue_report(2, w, "Befehl ist kein Objekt"); continue; }
        const JsonValue* cmd = c.find("cmd");
        if (!cmd || !cmd->is_str()) { issue_report(2, w, "ohne 'cmd'"); continue; }
        bool known = false;
        for (int t = 0; t < ISSUE_CMD_TYPE_COUNT; ++t) {
            if (cmd->str == ISSUE_CMD_TYPES[t]) known = true;
        }
        if (!known) issue_report(2, w, "unbekannter Befehlstyp '" + cmd->str + "'");
        if (cmd->str == "if_switch" || cmd->str == "if_variable") {
            const JsonValue* th = c.find("then");
            const JsonValue* el = c.find("else");
            if (!th || !th->is_arr()) issue_report(1, w, "'then' fehlt/kein Array");
            if (!el || !el->is_arr()) issue_report(1, w, "'else' fehlt/kein Array");
            if (th && th->is_arr()) issues_scan_commands(*th, w + " (Dann)");
            if (el && el->is_arr()) issues_scan_commands(*el, w + " (Sonst)");
        }
        if (cmd->str == "choices") {
            const JsonValue* opts = c.find("options");
            const JsonValue* br = c.find("branches");
            if (!opts || !opts->is_arr() || opts->arr.empty())
                issue_report(2, w, "'options' fehlt/leer");
            if (!br || !br->is_arr()) {
                issue_report(2, w, "'branches' fehlt");
            } else if (opts && opts->is_arr() && br->arr.size() < opts->arr.size()) {
                issue_report(1, w, "weniger Zweige als Optionen");
            }
        }
        if (cmd->str == "text") {
            const JsonValue* lines = c.find("lines");
            if (!lines || !lines->is_arr() || lines->arr.empty())
                issue_report(1, w, "'lines' fehlt/leer");
        }
        // Etappe 19i: Zahleneingabe + Kampf brauchen 'name'.
        if (cmd->str == "input_number" || cmd->str == "battle") {
            const JsonValue* nm = c.find("name");
            if (!nm || !nm->is_str() || nm->str.empty())
                issue_report(2, w, "'name' fehlt oder ist leer");
        }
    }
}

// Etappe 19k.3: Liegt (x, z) in einer Block-Zone der aktuellen Karte?
// Gleiche Rechnung wie Passage.zone? (Ruby) und Physics::is_blocked (C++):
// x/z = Mitte, Rand gehört dazu, wirkungslose Zonen (w/h <= 0) zählen nicht.
static bool blocked_zone_at(float x, float z) {
    const JsonValue* bl = g.map_root.find("blocked");
    if (!bl || !bl->is_arr()) return false;
    for (const JsonValue& b : bl->arr) {
        if (!b.is_obj()) continue;
        auto num = [&b](const char* k) {
            const JsonValue* v = b.find(k);
            return v ? static_cast<float>(v->number(0.0)) : 0.0f;
        };
        const float w = num("w"), h = num("h");
        if (w <= 0.0f || h <= 0.0f) continue;
        if (std::fabs(x - num("x")) * 2.0f <= w &&
            std::fabs(z - num("z")) * 2.0f <= h) {
            return true;
        }
    }
    return false;
}

// Die geladene Karte komplett prüfen und den Backlog neu füllen.
void issues_scan_map() {
    issue_clear();
    if (g.cur_map < 0 || !g.map_root.is_obj()) return;
    const std::string mapname =
        (g.cur_map >= 0 && g.cur_map < static_cast<int>(g.map_files.size()))
            ? g.map_files[g.cur_map] : std::string("?");

    // Objekte: Prefab-Typ + Material-Override
    if (JsonValue* objs = g.map_root.find("objects"); objs && objs->is_arr()) {
        for (int i = 0; i < static_cast<int>(objs->arr.size()); ++i) {
            const JsonValue& o = objs->arr[i];
            const std::string w = mapname + " Objekt #" + std::to_string(i + 1);
            if (!o.is_obj()) { issue_report(2, w, "ist kein Objekt"); continue; }
            const JsonValue* type = o.find("type");
            if (!type || !type->is_str() || type->str.empty()) {
                issue_report(2, w, "ohne 'type'");
            } else {
                bool known = false;
                for (const Prefab& p : g.prefabs) {
                    if (p.id == type->str) known = true;
                }
                if (!known) issue_report(1, w, "unbekannter Prefab-Typ '" + type->str + "'");
            }
            const JsonValue* tex = o.find("tex");
            if (tex && tex->is_str() && !tex->str.empty()) {
                const std::vector<std::string> names = mat_all_names();
                if (std::find(names.begin(), names.end(), tex->str) == names.end()) {
                    issue_report(1, w, "Material '" + tex->str + "' ist nicht geladen");
                }
            }
        }
    }

    // Etappe 19k.2: Block-Zonen (Passierbarkeit) - wie MapCheck.pruefe_blocked.
    if (JsonValue* bl = g.map_root.find("blocked"); bl) {
        if (!bl->is_arr()) {
            issue_report(2, mapname + " blocked", "ist keine Liste");
        } else {
            for (int i = 0; i < static_cast<int>(bl->arr.size()); ++i) {
                const JsonValue& b = bl->arr[i];
                const std::string w = mapname + " blocked #" + std::to_string(i + 1);
                if (!b.is_obj()) { issue_report(2, w, "ist kein Objekt"); continue; }
                for (const char* k : { "x", "z", "w", "h" }) {
                    const JsonValue* v = b.find(k);
                    if (!v || !v->is_num()) {
                        issue_report(2, w, std::string("'") + k +
                                           "' fehlt oder ist keine Zahl");
                    } else if ((k[0] == 'w' || k[0] == 'h') && v->number(0.0) <= 0.0) {
                        issue_report(1, w, std::string("'") + k + "' muss > 0 sein");
                    }
                }
            }
        }
    }

    // Etappe 19k.3: Positionen in Block-Zonen (Parität zu MapCheck) - der
    // Spieler käme dort nicht hinein, NPCs stünden an der Wand und Wegpunkte
    // wären unerreichbar.
    if (const JsonValue* bl2 = g.map_root.find("blocked");
        bl2 && bl2->is_arr() && !bl2->arr.empty()) {
        auto punkt = [](const JsonValue& o, float& x, float& z) {
            const JsonValue* vx = o.find("x");
            const JsonValue* vz = o.find("z");
            if (!vx || !vx->is_num() || !vz || !vz->is_num()) return false;
            x = static_cast<float>(vx->number(0.0));
            z = static_cast<float>(vz->number(0.0));
            return true;
        };
        float px = 0.0f, pz = 0.0f;
        const JsonValue* st = g.map_root.find("start");
        if (st && st->is_obj() && punkt(*st, px, pz) && blocked_zone_at(px, pz)) {
            issue_report(1, mapname + " Startpunkt",
                         "liegt in einer Block-Zone (Spieler steckt fest)");
        }
        if (JsonValue* ev2 = g.map_root.find("events"); ev2 && ev2->is_arr()) {
            for (int i = 0; i < static_cast<int>(ev2->arr.size()); ++i) {
                const JsonValue& e = ev2->arr[i];
                if (!e.is_obj() || !punkt(e, px, pz)) continue;
                if (blocked_zone_at(px, pz)) {
                    issue_report(1, mapname + " Event #" + std::to_string(i + 1),
                                 "steht in einer Block-Zone (nicht erreichbar)");
                }
            }
        }
        if (JsonValue* np = g.map_root.find("npcs"); np && np->is_arr()) {
            for (int i = 0; i < static_cast<int>(np->arr.size()); ++i) {
                const JsonValue& n = np->arr[i];
                if (!n.is_obj()) continue;
                const std::string nw = mapname + " NPC #" + std::to_string(i + 1);
                if (punkt(n, px, pz) && blocked_zone_at(px, pz)) {
                    issue_report(1, nw, "steht in einer Block-Zone (kommt nicht heraus)");
                }
                const JsonValue* pfad = n.find("path");
                if (!pfad || !pfad->is_arr()) continue;
                for (int j = 0; j < static_cast<int>(pfad->arr.size()); ++j) {
                    const JsonValue& wp = pfad->arr[j];
                    if (!wp.is_arr() || wp.arr.size() != 2) continue;
                    if (!wp.arr[0].is_num() || !wp.arr[1].is_num()) continue;
                    if (blocked_zone_at(static_cast<float>(wp.arr[0].number(0.0)),
                                        static_cast<float>(wp.arr[1].number(0.0)))) {
                        issue_report(1, nw + " Wegpunkt #" + std::to_string(j + 1),
                                     "liegt in einer Block-Zone");
                    }
                }
            }
        }
    }

    // Events: Trigger, Seiten, Bedingungen, Befehle
    JsonValue* evs = g.map_root.find("events");
    if (evs && evs->is_arr()) {
        for (int i = 0; i < static_cast<int>(evs->arr.size()); ++i) {
            const JsonValue& e = evs->arr[i];
            std::string w = mapname + " Event #" + std::to_string(i + 1);
            const JsonValue* ename = e.find("name");
            if (ename && ename->is_str() && !ename->str.empty()) {
                w += " ('" + ename->str + "')";
            }
            if (!e.is_obj()) { issue_report(2, w, "ist kein Objekt"); continue; }
            const JsonValue* trig = e.find("trigger");
            if (trig && trig->is_str() && !issue_known_trigger(trig->str)) {
                issue_report(1, w, "unbekannter Trigger '" + trig->str + "'");
            }
            const JsonValue* pages = e.find("pages");
            if (pages && pages->is_arr()) {
                for (int pi = 0; pi < static_cast<int>(pages->arr.size()); ++pi) {
                    const JsonValue& pg = pages->arr[pi];
                    const std::string pw = w + " Seite " + std::to_string(pi + 1);
                    if (!pg.is_obj()) { issue_report(2, pw, "ist kein Objekt"); continue; }
                    const JsonValue* ptrig = pg.find("trigger");
                    if (ptrig && ptrig->is_str() && !issue_known_trigger(ptrig->str)) {
                        issue_report(1, pw, "unbekannter Trigger '" + ptrig->str + "'");
                    }
                    const JsonValue* cmds = pg.find("commands");
                    if (!cmds || !cmds->is_arr()) {
                        issue_report(2, pw, "'commands' fehlt/kein Array");
                    } else {
                        issues_scan_commands(*cmds, pw);
                    }
                    const JsonValue* cond = pg.find("conditions");
                    if (cond && cond->is_obj()) {
                        for (const auto& kv : cond->obj) {
                            if (kv.first != "switch" && kv.first != "variable") {
                                issue_report(1, pw, "unbekannte Bedingung '" +
                                             kv.first + "'");
                            }
                        }
                    }
                }
            } else {
                // Altes Format: Top-Level commands direkt am Event.
                const JsonValue* oldc = e.find("commands");
                if (oldc && oldc->is_arr()) issues_scan_commands(*oldc, w);
            }
        }
    }
}

// ---- Probleme-Dialog (moduslos, F10) --------------------------------------
struct ProblemsDlgState {
    HWND win = nullptr;
    HWND list = nullptr;
    HWND lbl = nullptr;
};
static ProblemsDlgState g_pb;

static void pb_refresh() {
    if (!g_pb.list) return;
    SendMessageW(g_pb.list, LB_RESETCONTENT, 0, 0);
    for (const EditorIssue& is : g_issues) {
        const std::string line = (is.severity >= 2 ? "[Fehler] " : "[Warnung] ") +
                                 is.area + ": " + is.text;
        const std::wstring w = utf8_to_wide(line);
        SendMessageW(g_pb.list, LB_ADDSTRING, 0,
                     reinterpret_cast<LPARAM>(w.c_str()));
    }
    if (g_pb.lbl) {
        const std::wstring c = std::to_wstring(g_issues.size()) +
                               L" Problem(e) — 'Neu scannen' prüft die aktuelle Karte";
        SetWindowTextW(g_pb.lbl, c.c_str());
    }
    if (g_pb.win) {
        SetWindowTextW(g_pb.win,
                       (L"Probleme (" + std::to_wstring(g_issues.size()) + L")").c_str());
    }
}

static HWND pb_control(const wchar_t* cls, const wchar_t* text, DWORD style,
                       int x, int y, int w, int h, int id) {
    HWND c = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style,
                             x, y, w, h, g_pb.win,
                             reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                             GetModuleHandleW(nullptr), nullptr);
    SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(g.font), TRUE);
    return c;
}

LRESULT CALLBACK problems_proc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_COMMAND: {
            const int id = LOWORD(wp);
            if (id == IDPB_SCAN)  { issues_scan_map(); pb_refresh(); return 0; }
            if (id == IDPB_CLOSE) { DestroyWindow(h); return 0; }
            return 0;
        }
        case WM_CLOSE:
            DestroyWindow(h);
            return 0;
        case WM_DESTROY:
            if (g_pb.win == h) g_pb.win = nullptr;
            return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

void open_problems_dialog() {
    if (g_pb.win) {
        SetForegroundWindow(g_pb.win);
        return;
    }
    WNDCLASSW wc{};
    wc.lpfnWndProc = problems_proc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"RuneEditorProblemsDlg";
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
    RegisterClassW(&wc);

    RECT mr;
    GetWindowRect(g.main, &mr);
    g_pb.win = CreateWindowExW(WS_EX_DLGMODALFRAME, L"RuneEditorProblemsDlg",
                               L"Probleme",
                               WS_POPUP | WS_CAPTION | WS_SYSMENU,
                               mr.left + 200, mr.top + 140, 780, 430,
                               g.main, nullptr, GetModuleHandleW(nullptr), nullptr);
    g_pb.lbl = pb_control(L"Static", L"", 0, 12, 8, 740, 20, IDPB_L_COUNT);
    g_pb.list = pb_control(L"ListBox", nullptr,
                           WS_VSCROLL | WS_BORDER | LBS_NOTIFY | LBS_HASSTRINGS,
                           12, 32, 744, 320, IDPB_LIST);
    pb_control(L"Button", L"Neu scannen", BS_PUSHBUTTON, 12, 360, 140, 28, IDPB_SCAN);
    pb_control(L"Button", L"Schließen", BS_PUSHBUTTON, 616, 360, 140, 28, IDPB_CLOSE);
    pb_refresh();
    ShowWindow(g_pb.win, SW_SHOW);
    SetForegroundWindow(g_pb.win);
}

bool load_map(int index) {
    if (index < 0 || index >= static_cast<int>(g.map_files.size())) return false;
    close_event_editor();   // Zeiger in map_root werden ungültig
    const std::string path = g.project_dir + "\\maps\\" + g.map_files[index];
    std::string text;
    if (!read_file(path, text)) {
        issue_report(2, "Karte", "nicht lesbar: " + path);
        core::log_error("Karte nicht lesbar: " + path);
        return false;
    }
    try {
        g.map_root = json_parse(text);
    } catch (const std::exception& e) {
        issue_report(2, "Karte", std::string("ungültiges JSON: ") + e.what());
        core::log_error(std::string("Karte ungültig: ") + e.what());
        return false;
    }
    g.objects = g.map_root.find("objects");
    if (!g.objects || !g.objects->is_arr()) {
        // Karte ohne Objekte: leeres Array anlegen.
        g.objects = &g.map_root.set("objects", JsonValue::make_arr());
    }
    g.cur_map = index;
    g.sel_obj = -1;
    g.dirty = false;

    // Terrain neu erzeugen (rechteckige Karten: width/height, alt: size).
    // Etappe 18d: regen_terrain behandelt Ein-Textur- und Splat-Boden.
    regen_terrain();

    // Kamera auf eine gute Übersichtsposition (Blick Richtung Kartenmitte).
    const float span = std::max(g.terrain->width(), g.terrain->height());
    g.cam_pos = math::Vec3{ 0.0f, span * 0.10f + 8.0f, span * 0.18f };
    g.cam_yaw = math::PI;
    g.cam_pitch = -0.5f;

    std::string name = "Karte";
    if (const JsonValue* n = g.map_root.find("name")) name = n->str;
    if (g.cur_map < static_cast<int>(g.map_names.size())) g.map_names[g.cur_map] = name;

    apply_map_atmosphere_editor();

    char buf[256];
    std::snprintf(buf, sizeof(buf), "Karte geladen: %s (%d Objekte)", name.c_str(),
                  static_cast<int>(g.objects->arr.size()));
    core::log_info(buf);
    issues_scan_map();   // Etappe 18h: Karte automatisch prüfen
    maptree_fill();      // Etappe 18k: Baum (Name + Auswahl) synchronisieren
    return true;
}

bool save_map() {
    if (g.cur_map < 0) return false;
    const std::string path = g.project_dir + "\\maps\\" + g.map_files[g.cur_map];
    if (!write_file(path, json_write(g.map_root))) {
        issue_report(2, "Karte", "nicht schreibbar: " + path);
        core::log_error("Karte nicht schreibbar: " + path);
        return false;
    }
    g.dirty = false;
    core::log_info("Karte gespeichert: " + path);
    return true;
}

// ----------------------------------------------------------------------------
//  Etappe 18c: Tileset-Editor (modusloses Fenster)
//
//  Zeigt alle verfügbaren Boden-/Kachel-Texturen (prozedurale 8 + Projekt-
//  Importe) als Vorschau, setzt Boden-Textur + Kachelgrösse der aktuellen
//  Karte (terrain.texture / terrain.tile im Karten-JSON, live im Viewport)
//  und erzeugt eigene Kacheln (Farbe + Muster) als Projekt-Texturen.
// ----------------------------------------------------------------------------
enum {
    IDTS_LIST   = 4301,   // Liste aller Texturen
    IDTS_TILE   = 4303,   // Kachelgrösse (m) Edit
    IDTS_APPLY  = 4304,   // "Übernehmen" (Karte + Viewport)
    IDTS_SAVE   = 4305,   // "Als Projekt-Textur speichern"
    IDTS_EXPORT = 4306,   // "Kachel erzeugen" (Farbe + Muster -> neue Textur)
    IDTS_COLR   = 4307,   // Basisfarbe (hex)
    IDTS_COLR2  = 4308,   // Zweitfarbe (hex)
    IDTS_PAT    = 4309,   // Muster (Combobox: kein/punkte/streifen/rauschen)
    IDTS_NAME   = 4310,   // Name der neuen Kachel
    IDTS_CLOSE  = 4311,
    IDTS_PREVIEW = 4312,  // Vorschau (owner-drawn)
    IDTS_SPX    = 4313,   // Etappe 18d: Fläche X (m)
    IDTS_SPZ    = 4314,   // Fläche Z (m)
    IDTS_SPW    = 4315,   // Fläche Breite (m)
    IDTS_SPH    = 4316,   // Fläche Tiefe (m)
    IDTS_SPADD  = 4317,   // "Fläche hinzufügen"
    IDTS_SPDEL  = 4318,   // "Fläche entfernen"
    IDTS_SPLIST = 4319,   // Liste der Flächen
};

struct TileDlgState {
    HWND win = nullptr;
    HWND list = nullptr;
    HWND preview = nullptr;
    HWND splist = nullptr;           // Etappe 18d: Flächen-Liste
    std::vector<std::string> names;   // Texturen in Listenreihenfolge
    std::string sel;                  // aktuell gewählte Textur
};
static TileDlgState g_ts;

// Text eines Kind-Controls (Edit) im Tileset-Fenster als UTF-8.
std::string ts_text(int id) {
    wchar_t buf[128];
    GetDlgItemTextW(g_ts.win, id, buf, 128);
    return wide_to_utf8(buf);
}

// Eigene Kachel prozedural erzeugen (Basisfarbe + Zweitfarbe + Muster),
// 64x64 RGBA - gleiche Grösse wie die Ruby-Texturen.
std::vector<uint8_t> make_pattern_tile(const std::string& base_hex,
                                       const std::string& pat,
                                       const std::string& sec_hex) {
    uint32_t base = parse_hex_color(base_hex);
    uint32_t sec  = parse_hex_color(sec_hex);
    if (base == 0) base = 0x88B055;   // Gras-Default bei Unsinn
    if (sec == 0)  sec  = 0x6A8A3F;
    const int R1 = static_cast<int>((base >> 16) & 0xFF);
    const int G1 = static_cast<int>((base >> 8) & 0xFF);
    const int B1 = static_cast<int>(base & 0xFF);
    const int R2 = static_cast<int>((sec >> 16) & 0xFF);
    const int G2 = static_cast<int>((sec >> 8) & 0xFF);
    const int B2 = static_cast<int>(sec & 0xFF);

    // Deterministischer Zufall (LCG) - gleiche Kachel bei gleicher Eingabe.
    uint32_t st = 12345u;
    auto rnd = [&]() {
        st = (st * 1103515245u + 12345u) & 0x7FFFFFFFu;
        return static_cast<float>(st) / 2147483647.0f;
    };

    std::vector<uint8_t> px(64 * 64 * 4, 255);
    for (int y = 0; y < 64; ++y) {
        for (int x = 0; x < 64; ++x) {
            float u = rnd() * 2.0f - 1.0f;
            int R = R1, G = G1, B = B1;
            if (pat == "punkte" && (x % 8) == 0 && (y % 8) == 0) {
                R = R2; G = G2; B = B2;
            } else if (pat == "streifen" && (((x + y) / 4) % 2) == 0) {
                R = R2; G = G2; B = B2;
            } else if (pat == "rauschen") {
                R = static_cast<int>((R + R2) * 0.5f + u * 22.0f);
                G = static_cast<int>((G + G2) * 0.5f + u * 22.0f);
                B = static_cast<int>((B + B2) * 0.5f + u * 22.0f);
            } else {
                R = static_cast<int>(R + u * 9.0f);
                G = static_cast<int>(G + u * 9.0f);
                B = static_cast<int>(B + u * 9.0f);
            }
            const int i = (y * 64 + x) * 4;
            px[i]     = static_cast<uint8_t>(std::min(255, std::max(0, R)));
            px[i + 1] = static_cast<uint8_t>(std::min(255, std::max(0, G)));
            px[i + 2] = static_cast<uint8_t>(std::min(255, std::max(0, B)));
            px[i + 3] = 255;
        }
    }
    return px;
}

// Vorschau zeichnen: RGBA-Puffer -> 32bpp-DIB -> StretchBlt (quadratisch).
void draw_tileset_preview(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(hwnd, &ps);
    RECT rc;
    GetClientRect(hwnd, &rc);
    FillRect(dc, &rc, reinterpret_cast<HBRUSH>(GetStockObject(DKGRAY_BRUSH)));

    if (!g_ts.sel.empty()) {
        std::vector<uint8_t> px;
        int w = 0, h = 0;
        if (g.renderer && g.renderer->texture_rgba(g_ts.sel, px, w, h) && !px.empty()) {
            BITMAPINFO bi{};
            bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bi.bmiHeader.biWidth = w;
            bi.bmiHeader.biHeight = -h;   // top-down
            bi.bmiHeader.biPlanes = 1;
            bi.bmiHeader.biBitCount = 32;
            bi.bmiHeader.biCompression = BI_RGB;
            void* bits = nullptr;
            HBITMAP dib = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
            if (dib && bits) {
                std::memcpy(bits, px.data(), px.size());
                HDC mem = CreateCompatibleDC(dc);
                HGDIOBJ old = SelectObject(mem, dib);
                const int W = rc.right - rc.left, H = rc.bottom - rc.top;
                const int side = std::max(1, std::min(W, H) - 8);
                StretchBlt(dc, (W - side) / 2, (H - side) / 2, side, side,
                           mem, 0, 0, w, h, SRCCOPY);
                SelectObject(mem, old);
                DeleteDC(mem);
                DeleteObject(dib);
            }
        }
    }
    EndPaint(hwnd, &ps);
}

// Tileset-Liste neu füllen (nach Kachelerzeugung).
void ts_fill_list() {
    SendMessageW(g_ts.list, LB_RESETCONTENT, 0, 0);
    g_ts.names = mat_all_names();
    for (const std::string& n : g_ts.names) {
        SendMessageW(g_ts.list, LB_ADDSTRING, 0,
                     reinterpret_cast<LPARAM>(utf8_to_wide(n).c_str()));
    }
    if (!g_ts.sel.empty()) {
        for (int i = 0; i < static_cast<int>(g_ts.names.size()); ++i) {
            if (g_ts.names[i] == g_ts.sel) {
                SendMessageW(g_ts.list, LB_SETCURSEL, i, 0);
                break;
            }
        }
    }
}

// Etappe 18d: Splat-Flächen-Liste im Tileset-Dialog neu füllen (Format:
// "textur @ (x, z) 30x12"). Auswahl merken.
void ts_fill_splats() {
    const int sel_before = static_cast<int>(
        SendMessageW(g_ts.splist, LB_GETCURSEL, 0, 0));
    SendMessageW(g_ts.splist, LB_RESETCONTENT, 0, 0);
    const JsonValue* t = g.map_root.find("terrain");
    const std::vector<world::SplatRect> splats = read_splats(t);
    for (const world::SplatRect& s : splats) {
        wchar_t buf[160];
        swprintf(buf, 160, L"%s @ (%g, %g) %gx%g",
                utf8_to_wide(s.tex).c_str(), s.x, s.z, s.w, s.h);
        SendMessageW(g_ts.splist, LB_ADDSTRING, 0,
                     reinterpret_cast<LPARAM>(buf));
    }
    if (sel_before >= 0 && sel_before < static_cast<int>(splats.size())) {
        SendMessageW(g_ts.splist, LB_SETCURSEL, sel_before, 0);
    }
}

LRESULT CALLBACK tileset_dlg_proc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_PAINT:
            if (h == g_ts.preview) { draw_tileset_preview(h); return 0; }
            break;
        case WM_COMMAND: {
            const int id = LOWORD(wp);
            const int code = HIWORD(wp);
            if (id == IDTS_LIST && (code == LBN_SELCHANGE || code == LBN_DBLCLK)) {
                const int sel = static_cast<int>(SendMessageW(g_ts.list, LB_GETCURSEL, 0, 0));
                if (sel >= 0 && sel < static_cast<int>(g_ts.names.size())) {
                    g_ts.sel = g_ts.names[sel];
                    // Etappe 18e: im Mal-Modus die Pinseltextur mitführen.
                    if (g.paint_mode) {
                        g.paint_tex = g_ts.sel;
                        status_mode((L"Boden malen: " + utf8_to_wide(g.paint_tex) +
                                     L" | Pinsel " + std::to_wstring(static_cast<int>(g.paint_size)) +
                                     L" m | Shift = Radierer").c_str());
                    }
                    InvalidateRect(g_ts.preview, nullptr, TRUE);
                }
                return 0;
            }
            if (id == IDTS_APPLY) {
                if (!g_ts.sel.empty() && g.cur_map >= 0) {
                    JsonValue* ter = g.map_root.find("terrain");
                    if (!ter) ter = &g.map_root.set("terrain", JsonValue::make_obj());
                    ter->set("texture", JsonValue::make_str(g_ts.sel));
                    const float tile = std::max(0.25f,
                        static_cast<float>(std::atof(ts_text(IDTS_TILE).c_str())));
                    ter->set("tile", JsonValue::make_num(static_cast<double>(tile), false));
                    regen_terrain();          // entscheidet Ein-Textur- oder Splat-Pfad
                    g.dirty = true;
                    update_title();
                    ts_fill_splats();
                    status_mode(L"Boden übernommen");
                    core::log_info("Boden umgestellt: " + g_ts.sel +
                                   " (Kachel " + std::to_string(tile) + " m, " +
                                   std::to_string(read_splats(ter).size()) + " Fläche(n))");
                } else {
                    core::log_warn("Tileset: keine Textur gewählt oder keine Karte offen");
                    status_mode(L"Keine Textur gewählt");
                }
                return 0;
            }
            // Etappe 18d: Fläche mit der gewählten Textur hinzufügen.
            if (id == IDTS_SPADD) {
                if (g_ts.sel.empty() || g.cur_map < 0) {
                    core::log_warn("Tileset: Fläche braucht eine gewählte Textur");
                    status_mode(L"Erst Textur wählen");
                    return 0;
                }
                const float x = static_cast<float>(std::atof(ts_text(IDTS_SPX).c_str()));
                const float z = static_cast<float>(std::atof(ts_text(IDTS_SPZ).c_str()));
                const float w = static_cast<float>(std::atof(ts_text(IDTS_SPW).c_str()));
                const float h = static_cast<float>(std::atof(ts_text(IDTS_SPH).c_str()));
                if (w <= 0.0f || h <= 0.0f) {
                    core::log_warn("Tileset: Fläche braucht Breite/Tiefe > 0");
                    status_mode(L"Breite/Tiefe ungültig");
                    return 0;
                }
                JsonValue* ter = g.map_root.find("terrain");
                if (!ter) ter = &g.map_root.set("terrain", JsonValue::make_obj());
                JsonValue* arr = ter->find("splats");
                if (!arr || !arr->is_arr()) arr = &ter->set("splats", JsonValue::make_arr());
                JsonValue e = JsonValue::make_obj();
                e.set("tex", JsonValue::make_str(g_ts.sel));
                e.set("x", JsonValue::make_num(static_cast<double>(x)));
                e.set("z", JsonValue::make_num(static_cast<double>(z)));
                e.set("w", JsonValue::make_num(static_cast<double>(w)));
                e.set("h", JsonValue::make_num(static_cast<double>(h)));
                arr->arr.push_back(e);
                regen_terrain();
                ts_fill_splats();
                g.dirty = true;
                update_title();
                status_mode((L"Fläche: " + utf8_to_wide(g_ts.sel)).c_str());
                core::log_info("Tileset: Fläche hinzugefügt " + g_ts.sel +
                               " @ (" + std::to_string(x) + ", " + std::to_string(z) + ") " +
                               std::to_string(w) + "x" + std::to_string(h));
                return 0;
            }
            // Etappe 18d: markierte Fläche entfernen.
            if (id == IDTS_SPDEL) {
                const int sel = static_cast<int>(SendMessageW(g_ts.splist, LB_GETCURSEL, 0, 0));
                JsonValue* ter = g.map_root.find("terrain");
                JsonValue* arr = ter ? ter->find("splats") : nullptr;
                if (sel < 0 || !arr || !arr->is_arr() || sel >= static_cast<int>(arr->arr.size())) {
                    core::log_warn("Tileset: keine Fläche markiert");
                    status_mode(L"Keine Fläche markiert");
                    return 0;
                }
                arr->arr.erase(arr->arr.begin() + sel);
                if (arr->arr.empty()) {
                    json_remove_key(*ter, "splats");   // leere Liste ganz raus
                }
                regen_terrain();
                ts_fill_splats();
                g.dirty = true;
                update_title();
                status_mode(L"Fläche entfernt");
                core::log_info("Tileset: Fläche entfernt (Index " + std::to_string(sel) + ")");
                return 0;
            }
            if (id == IDTS_EXPORT) {
                std::string name = ts_text(IDTS_NAME);
                for (char& c : name) if (c == ' ') c = '_';
                if (name.empty()) {
                    name = "kachel_" + std::to_string(GetTickCount() % 10000);
                }
                wchar_t wpat[32];
                GetDlgItemTextW(h, IDTS_PAT, wpat, 32);
                const std::vector<uint8_t> px = make_pattern_tile(
                    ts_text(IDTS_COLR), wide_to_utf8(wpat), ts_text(IDTS_COLR2));
                if (g.renderer) {
                    g.renderer->add_texture(name, 64, 64, px.data());
                }
                g_ts.sel = name;
                ts_fill_list();
                InvalidateRect(g_ts.preview, nullptr, TRUE);
                core::log_info("Tileset: neue Kachel '" + name +
                               "' erzeugt (speichern mit 'Als Projekt-Textur speichern')");
                status_mode((L"Neue Kachel: " + utf8_to_wide(name)).c_str());
                return 0;
            }
            if (id == IDTS_SAVE) {
                if (g_ts.sel.empty()) {
                    core::log_warn("Tileset: keine Textur gewählt");
                    return 0;
                }
                g_ts.sel = ts_text(IDTS_NAME).empty() ? g_ts.sel : ts_text(IDTS_NAME);
                std::vector<uint8_t> px;
                int w = 0, h = 0;
                if (!g.renderer || !g.renderer->texture_rgba(g_ts.sel, px, w, h)) {
                    core::log_error("Tileset: Pixel nicht abrufbar: " + g_ts.sel);
                    return 0;
                }
                MaterialInfo m;
                m.name = g_ts.sel;
                m.file = g_ts.sel + ".rgba";
                m.source = "tileset";
                m.w = w;
                m.h = h;
                m.tile = 2.0;
                m.imported = true;
                CreateDirectoryA(mat_dir().c_str(), nullptr);
                const std::string data(reinterpret_cast<const char*>(px.data()), px.size());
                if (!write_file(mat_dir() + "\\" + m.file, data)) {
                    core::log_error("Tileset: .rgba nicht schreibbar: " + m.file);
                    return 0;
                }
                bool replaced = false;
                for (MaterialInfo& old : g_materials) {
                    if (old.name == m.name) { old = m; replaced = true; break; }
                }
                if (!replaced) g_materials.push_back(m);
                mat_save();
                core::log_info("Tileset: Kachel als Projekt-Textur gespeichert: " + m.name);
                status_mode(L"Kachel als Projekt-Textur gespeichert");
                return 0;
            }
            if (id == IDTS_CLOSE) {
                DestroyWindow(h);
                return 0;
            }
            break;
        }
        case WM_CLOSE:
            DestroyWindow(h);
            return 0;
        case WM_DESTROY:
            if (g_ts.win == h) {
                g_ts.win = nullptr;
                g_ts.list = nullptr;
                g_ts.preview = nullptr;
                g_ts.splist = nullptr;
            }
            return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

void open_tileset_dialog() {
    if (g_ts.win) { SetForegroundWindow(g_ts.win); return; }

    WNDCLASSW wc{};
    wc.lpfnWndProc = tileset_dlg_proc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"RuneEditorTileDlg";
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
    RegisterClassW(&wc);

    const int W = 674, H = 470;
    RECT mr;
    GetWindowRect(g.main, &mr);
    g_ts.win = CreateWindowExW(WS_EX_DLGMODALFRAME, L"RuneEditorTileDlg",
                               L"Tileset-Editor",
                               WS_POPUP | WS_CAPTION | WS_SYSMENU,
                               mr.left + 180, mr.top + 90, W, H,
                               g.main, nullptr, GetModuleHandleW(nullptr), nullptr);

    // Control-Helfer auf dem Tileset-Fenster (wie prj_control).
    auto ts = [&](const wchar_t* cls, const wchar_t* text, DWORD style,
                  int x, int y, int w, int h, int id) {
        HWND c = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style,
                                 x, y, w, h, g_ts.win,
                                 reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                                 GetModuleHandleW(nullptr), nullptr);
        SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(g.font), TRUE);
        return c;
    };

    // Linke Spalte: Texturenliste.
    ts(L"Static", L"Boden-Texturen:", 0, 12, 12, 100, 20, 0);
    g_ts.list = ts(L"ListBox", nullptr,
                   LBS_NOTIFY | WS_BORDER | LBS_HASSTRINGS | WS_VSCROLL,
                   12, 36, 180, 380, IDTS_LIST);
    g_ts.names = mat_all_names();
    for (const std::string& n : g_ts.names) {
        SendMessageW(g_ts.list, LB_ADDSTRING, 0,
                     reinterpret_cast<LPARAM>(utf8_to_wide(n).c_str()));
    }
    // Aktuell in der Karte eingestellte Boden-Textur vorwählen.
    {
        const JsonValue* ter = g.map_root.find("terrain");
        const JsonValue* tx = ter ? ter->find("texture") : nullptr;
        const std::string cur = tx ? tx->str : std::string("gras");
        if (!cur.empty()) {
            g_ts.sel = cur;
            for (int i = 0; i < static_cast<int>(g_ts.names.size()); ++i) {
                if (g_ts.names[i] == cur) {
                    SendMessageW(g_ts.list, LB_SETCURSEL, i, 0);
                    break;
                }
            }
        }
    }

    // Rechte Spalte: Vorschau + Kachelgrösse + Eigene Kachel + Buttons.
    // Vorschau als eigenes Fenster der Tileset-Klasse (WM_PAINT landet so
    // in unserer Proc; ein Static-Kind würde selbst zeichnen).
    g_ts.preview = CreateWindowExW(0, L"RuneEditorTileDlg", nullptr,
                                   WS_CHILD | WS_VISIBLE,
                                   205, 36, 180, 180, g_ts.win,
                                   reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDTS_PREVIEW)),
                                   GetModuleHandleW(nullptr), nullptr);

    ts(L"Static", L"Kachelgröße (m):", 0, 205, 232, 120, 20, 0);
    ts(L"Edit", L"2.0", WS_BORDER | ES_LEFT, 330, 228, 44, 24, IDTS_TILE);

    ts(L"Static", L"Eigene Kachel:", 0, 205, 262, 100, 20, 0);
    ts(L"Static", L"Farbe:", 0, 205, 288, 60, 20, 0);
    ts(L"Edit", L"88B055", WS_BORDER | ES_LEFT, 260, 284, 70, 24, IDTS_COLR);
    ts(L"Static", L"Zweitfarbe:", 0, 205, 316, 100, 20, 0);
    ts(L"Edit", L"6A8A3F", WS_BORDER | ES_LEFT, 260, 312, 70, 24, IDTS_COLR2);
    HWND cb_pat = ts(L"ComboBox", nullptr, CBS_DROPDOWNLIST | WS_VSCROLL,
                     205, 344, 126, 100, IDTS_PAT);
    for (const wchar_t* p : { L"kein", L"punkte", L"streifen", L"rauschen" }) {
        SendMessageW(cb_pat, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(p));
    }
    SendMessageW(cb_pat, CB_SETCURSEL, 0, 0);
    ts(L"Static", L"Name:", 0, 340, 316, 60, 20, 0);
    ts(L"Edit", L"meine_kachel", WS_BORDER | ES_LEFT, 340, 342, 120, 24, IDTS_NAME);
    ts(L"Button", L"Kachel erzeugen", BS_PUSHBUTTON, 205, 376, 120, 26, IDTS_EXPORT);
    ts(L"Button", L"Als Projekt-Textur speichern", BS_PUSHBUTTON, 335, 376, 190, 26, IDTS_SAVE);
    ts(L"Button", L"Übernehmen (Karte)", BS_DEFPUSHBUTTON, 205, 410, 150, 28, IDTS_APPLY);
    ts(L"Button", L"Schließen", BS_PUSHBUTTON, 375, 410, 100, 28, IDTS_CLOSE);

    // Etappe 18d: rechte Spalte - Boden-Flächen (Splats).
    ts(L"Static", L"Boden-Flächen:", 0, 540, 12, 110, 20, 0);
    g_ts.splist = ts(L"ListBox", nullptr,
                     LBS_NOTIFY | WS_BORDER | LBS_HASSTRINGS | WS_VSCROLL,
                     540, 36, 160, 240, IDTS_SPLIST);
    ts_fill_splats();

    ts(L"Static", L"Fläche hinzufügen (m):", 0, 540, 284, 160, 20, 0);
    ts(L"Static", L"X:", 0, 540, 306, 16, 20, 0);
    ts(L"Edit", L"-30", WS_BORDER | ES_LEFT, 556, 302, 40, 24, IDTS_SPX);
    ts(L"Static", L"Z:", 0, 604, 306, 16, 20, 0);
    ts(L"Edit", L"-30", WS_BORDER | ES_LEFT, 620, 302, 40, 24, IDTS_SPZ);
    ts(L"Static", L"B:", 0, 540, 336, 16, 20, 0);
    ts(L"Edit", L"60", WS_BORDER | ES_LEFT, 556, 332, 40, 24, IDTS_SPW);
    ts(L"Static", L"T:", 0, 604, 336, 16, 20, 0);
    ts(L"Edit", L"40", WS_BORDER | ES_LEFT, 620, 332, 40, 24, IDTS_SPH);
    ts(L"Button", L"Hinzufügen", BS_PUSHBUTTON, 540, 366, 120, 26, IDTS_SPADD);
    ts(L"Button", L"Entfernen", BS_PUSHBUTTON, 540, 394, 120, 26, IDTS_SPDEL);

    ShowWindow(g_ts.win, SW_SHOW);
    core::log_info("Tileset-Editor geöffnet (" +
                   std::to_string(g_ts.names.size()) + " Texturen)");
}

// ------------------------------------------------------------
//  Etappe 18b: Projekt-Auswahl
//
//  Der Editor war bisher fest auf "dorf" eingestellt. Jetzt: Menü
//  "Projekt wechseln..." (Dialog mit Liste + Neuanlegen), CLI-Schalter
//  --project <name> und Merker log/last_project.txt (beim Start
//  automatisch laden, wenn vorhanden und gültig).
// ------------------------------------------------------------

// Alle Projekte unter <root>/projekte/ (Verzeichnisse mit data/system.json).
std::vector<std::string> scan_projects() {
    std::vector<std::string> names;
    const std::string dir = g.root + "\\projekte\\*";
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(dir.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return names;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
        if (fd.cFileName[0] == '.') continue;
        const std::string sys = g.root + "\\projekte\\" + fd.cFileName +
                                "\\data\\system.json";
        if (GetFileAttributesA(sys.c_str()) != INVALID_FILE_ATTRIBUTES) {
            names.push_back(fd.cFileName);
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    std::sort(names.begin(), names.end());
    return names;
}

// Neues Projekt anlegen: Verzeichnis-Gerüst + system.json + Startkarte.
bool create_project(const std::string& name) {
    for (char c : name) {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                        (c >= '0' && c <= '9') || c == '_';
        if (!ok) {
            core::log_error("Projektname ungültig (nur Buchstaben/Zahlen/_): " + name);
            return false;
        }
    }
    const std::string base = g.root + "\\projekte\\" + name;
    if (GetFileAttributesA(base.c_str()) != INVALID_FILE_ATTRIBUTES) {
        core::log_error("Projekt existiert bereits: " + name);
        return false;
    }
    CreateDirectoryA(base.c_str(), nullptr);
    CreateDirectoryA((base + "\\data").c_str(), nullptr);
    CreateDirectoryA((base + "\\data\\maps").c_str(), nullptr);
    CreateDirectoryA((base + "\\saves").c_str(), nullptr);

    // system.json: Titel + Startpunkt auf die neue Karte.
    JsonValue sys = JsonValue::make_obj();
    sys.set("name", JsonValue::make_str(name));
    JsonValue start = JsonValue::make_obj();
    start.set("map", JsonValue::make_str(name));
    start.set("x", JsonValue::make_num(0.0, false));
    start.set("z", JsonValue::make_num(0.0, false));
    start.set("yaw", JsonValue::make_num(3.14159, false));
    sys.set("start", start);
    sys.set("switches", JsonValue::make_obj());
    sys.set("variables", JsonValue::make_obj());
    if (!write_file(base + "\\data\\system.json", json_write(sys))) {
        core::log_error("system.json nicht schreibbar: " + name);
        return false;
    }

    // leere scripts.json + database.json mit Start-Held/Item.
    JsonValue scripts = JsonValue::make_obj();
    scripts.set("scripts", JsonValue::make_arr());
    write_file(base + "\\data\\scripts.json", json_write(scripts));
    JsonValue db = JsonValue::make_obj();
    db.set("gold", JsonValue::make_num(0, true));
    db.set("actors", JsonValue::make_arr());
    db.set("items", JsonValue::make_arr());
    write_file(base + "\\data\\database.json", json_write(db));

    // Startkarte (wie "Neue Karte", flach, 240x240).
    JsonValue map = JsonValue::make_obj();
    map.set("name", JsonValue::make_str(name));
    JsonValue ter = JsonValue::make_obj();
    ter.set("width", JsonValue::make_num(240.0, false));
    ter.set("height", JsonValue::make_num(240.0, false));
    ter.set("resolution", JsonValue::make_num(192, true));
    ter.set("seed", JsonValue::make_num(7, true));
    ter.set("amplitude", JsonValue::make_num(0.0, false));
    map.set("terrain", ter);
    map.set("objects", JsonValue::make_arr());
    map.set("events", JsonValue::make_arr());
    JsonValue ms = JsonValue::make_obj();
    ms.set("x", JsonValue::make_num(0.0, false));
    ms.set("z", JsonValue::make_num(0.0, false));
    map.set("start", ms);
    JsonValue fog = JsonValue::make_obj();
    fog.set("on", JsonValue::make_bool(false));
    fog.set("far", JsonValue::make_num(180.0, false));
    fog.set("color", JsonValue::make_num(0xBFD8EC, true));
    map.set("fog", fog);
    JsonValue pan = JsonValue::make_obj();
    pan.set("color", JsonValue::make_num(0x3D6FB5, true));
    map.set("panorama", pan);
    map.set("note", JsonValue::make_str(""));
    map.set("weather", JsonValue::make_obj());
    if (!write_file(base + "\\data\\maps\\" + name + ".json", json_write(map))) {
        core::log_error("Startkarte nicht schreibbar: " + name);
        return false;
    }
    core::log_info("Neues Projekt angelegt: " + name);
    return true;
}

// Auf ein Projekt umschalten (Karten/Texturen/Menüs/Listen komplett neu).
bool switch_project(const std::string& name) {
    const std::string data = g.root + "\\projekte\\" + name + "\\data";
    if (GetFileAttributesA((data + "\\system.json").c_str()) == INVALID_FILE_ATTRIBUTES) {
        core::log_error("Projekt nicht gefunden: " + name);
        return false;
    }
    // Offene Dialoge schliessen (Zeiger werden ungültig).
    if (g.props_dlg) { DestroyWindow(g.props_dlg); g.props_dlg = nullptr; }
    close_event_editor();

    g.project_dir = data;
    g.project_name = name;
    g.dirty = false;
    g.sel_obj = -1;
    g.undo_stack.clear();
    g.redo_stack.clear();

    // Objekt-/Eigenschaftsfelder und Terrain leeren.
    g.objects = nullptr;
    g.map_root = JsonValue{};
    g.cur_map = -1;
    g.renderer->clear_terrain();

    // Importierte Texturen des Projekts laden.
    g_materials.clear();
    mat_load();

    scan_maps();
    rebuild_map_menu();
    if (!g.map_files.empty()) {
        load_map(0);
    } else {
        core::log_warn("Projekt ohne Karten: " + name);
    }
    sync_object_list();
    sync_property_fields();
    update_title();

    // Merker für den nächsten Start schreiben.
    write_file(g.root + "\\log\\last_project.txt", name);
    core::log_info("Projekt gewechselt: " + name + " (" +
                   std::to_string(g.map_files.size()) + " Karten)");
    return true;
}

// Der Projekt-Dialog: Liste der Projekte + Neu-Textfeld + Buttons.
enum {
    IDPR_LIST  = 4201,
    IDPR_NEW   = 4202,
    IDPR_OPEN  = 4203,
    IDPR_CREATE = 4204,
    IDPR_CLOSE = 4205,
};

struct ProjectDlgState {
    HWND win = nullptr;
    HWND list = nullptr;
    HWND ed_new = nullptr;
};
static ProjectDlgState g_prj;

LRESULT CALLBACK project_dlg_proc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_COMMAND: {
            const int id = LOWORD(wp);
            const int code = HIWORD(wp);
            if (id == IDPR_OPEN || (id == IDPR_LIST && code == LBN_DBLCLK)) {
                const int sel = static_cast<int>(SendMessageW(g_prj.list, LB_GETCURSEL, 0, 0));
                if (sel >= 0) {
                    wchar_t buf[128];
                    SendMessageW(g_prj.list, LB_GETTEXT, sel, reinterpret_cast<LPARAM>(buf));
                    const std::string name = wide_to_utf8(buf);
                    DestroyWindow(h);
                    if (switch_project(name)) {
                        status_mode((L"Projekt: " + utf8_to_wide(name)).c_str());
                    } else {
                        status_mode(L"Projektwechsel fehlgeschlagen");
                    }
                }
                return 0;
            }
            if (id == IDPR_CREATE) {
                wchar_t wbuf[128];
                GetDlgItemTextW(g_prj.win, IDPR_NEW, wbuf, 128);
                const std::string name = wide_to_utf8(wbuf);
                if (name.empty()) { MessageBoxW(h, L"Bitte einen Namen eingeben.",
                                               L"Neues Projekt", MB_OK); return 0; }
                if (create_project(name)) {
                    DestroyWindow(h);
                    switch_project(name);
                    status_mode((L"Neues Projekt: " + utf8_to_wide(name)).c_str());
                } else {
                    MessageBoxW(h, L"Projekt konnte nicht angelegt werden (Name ungültig\n"
                                   L"oder bereits vorhanden?).", L"Neues Projekt", MB_OK);
                }
                return 0;
            }
            if (id == IDPR_CLOSE) {
                DestroyWindow(h);
                return 0;
            }
            break;
        }
        case WM_CLOSE:
            DestroyWindow(h);
            return 0;
        case WM_DESTROY:
            g_prj.win = nullptr;
            g_prj.list = nullptr;
            g_prj.ed_new = nullptr;
            return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

// Kleine Helfer: Controls direkt auf dem Projekt-Dialog anlegen -
// unabhängig vom globalen g_dlg (das gehört zu einem evtl. offenen
// Karten-Dialog).
HWND prj_control(const wchar_t* cls, const wchar_t* text, DWORD style,
                 int x, int y, int w, int h, int id) {
    HWND c = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style,
                             x, y, w, h, g_prj.win,
                             reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                             GetModuleHandleW(nullptr), nullptr);
    SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(g.font), TRUE);
    return c;
}

void prj_label(const wchar_t* text, int x, int y, int w) {
    prj_control(L"Static", text, 0, x, y, w, 20, 0);
}

void open_project_dialog() {
    if (g_prj.win) { SetForegroundWindow(g_prj.win); return; }   // schon offen

    WNDCLASSW wc{};
    wc.lpfnWndProc = project_dlg_proc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"RuneEditorProjectDlg";
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
    RegisterClassW(&wc);

    const int W = 360, H = 420;
    RECT mr;
    GetWindowRect(g.main, &mr);
    g_prj.win = CreateWindowExW(WS_EX_DLGMODALFRAME, L"RuneEditorProjectDlg",
                                L"Projekt wechseln",
                                WS_POPUP | WS_CAPTION | WS_SYSMENU,
                                mr.left + 240, mr.top + 100, W, H,
                                g.main, nullptr, GetModuleHandleW(nullptr), nullptr);

    prj_label(L"Projekt öffnen (Doppelklick):", 16, 14, 240);
    g_prj.list = prj_control(L"ListBox", nullptr,
                             LBS_NOTIFY | WS_BORDER | LBS_HASSTRINGS | WS_VSCROLL,
                             16, 38, 324, 200, IDPR_LIST);
    for (const std::string& p : scan_projects()) {
        SendMessageW(g_prj.list, LB_ADDSTRING, 0,
                     reinterpret_cast<LPARAM>(utf8_to_wide(p).c_str()));
    }

    prj_label(L"Neues Projekt:", 16, 260, 140);
    g_prj.ed_new = prj_control(L"Edit", nullptr, WS_BORDER | ES_LEFT,
                               130, 256, 210, 24, IDPR_NEW);

    prj_control(L"Button", L"Öffnen", BS_DEFPUSHBUTTON,   16, 300, 100, 30, IDPR_OPEN);
    prj_control(L"Button", L"Anlegen", BS_PUSHBUTTON,     130, 300, 100, 30, IDPR_CREATE);
    prj_control(L"Button", L"Schließen", BS_PUSHBUTTON,  240, 300, 100, 30, IDPR_CLOSE);

    prj_label(L"Hinweis: Der Editor startet mit dem zuletzt geöffneten Projekt.",
              16, 344, 330);

    ShowWindow(g_prj.win, SW_SHOW);
    SetFocus(g_prj.list);
}

void scan_maps() {
    g.map_files.clear();
    g.map_names.clear();
    const std::string dir = g.project_dir + "\\maps\\*.json";
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(dir.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (fd.cFileName[0] == '.') continue;
        std::string file = fd.cFileName;
        g.map_files.push_back(file);
        // Anzeigename: "name"-Feld, sonst Dateiname
        std::string text;
        std::string name = file.substr(0, file.size() - 5);
        if (read_file(g.project_dir + "\\maps\\" + file, text)) {
            try {
                const JsonValue parsed = json_parse(text);
                const JsonValue* n = parsed.find("name");
                if (n && n->is_str()) name = n->str;
            } catch (...) {
                // defekte Karte: Dateiname bleibt
            }
        }
        g.map_names.push_back(name);
    } while (FindNextFileA(h, &fd));
    FindClose(h);
}

// Karten-Untermenü neu aufbauen (nach Anlegen/Löschen/Laden).
void rebuild_map_menu() {
    if (!g.menu_map) return;
    while (DeleteMenu(g.menu_map, ID_MENU_MAP_BASE, MF_BYCOMMAND)) {}
    // Trennlinie entfernen, falls schon vorhanden, und frisch setzen.
    const int count = GetMenuItemCount(g.menu_map);
    for (int i = count - 1; i >= 0; --i) {
        MENUITEMINFOW mi{};
        mi.cbSize = sizeof(mi);
        mi.fMask = MIIM_FTYPE;
        GetMenuItemInfoW(g.menu_map, i, TRUE, &mi);
        if (mi.fType & MFT_SEPARATOR) DeleteMenu(g.menu_map, i, MF_BYPOSITION);
    }
    if (!g.map_names.empty()) {
        AppendMenuW(g.menu_map, MF_SEPARATOR, 0, nullptr);
        for (size_t i = 0; i < g.map_names.size(); ++i) {
            AppendMenuW(g.menu_map, MF_STRING,
                        static_cast<UINT_PTR>(ID_MENU_MAP_BASE + i),
                        utf8_to_wide(g.map_names[i]).c_str());
        }
        if (g.cur_map >= 0 && g.cur_map < static_cast<int>(g.map_names.size())) {
            CheckMenuRadioItem(g.menu_map, ID_MENU_MAP_BASE,
                               static_cast<UINT>(ID_MENU_MAP_BASE + g.map_names.size() - 1),
                               ID_MENU_MAP_BASE + g.cur_map, MF_BYCOMMAND);
        }
    }
    maptree_fill();   // Etappe 18k: Baum parallel zum Menü pflegen
}

// ----------------------------------------------------------------------------
//  Etappe 18k: Kartenbaum (RPG-Maker-XP-Kartenliste)
//
//  Links oben im Hauptfenster: ein TreeView mit dem Projektnamen als Wurzel
//  und allen Karten als Einträge (Item-LPARAM = Karten-Index in map_files).
//  Doppelklick öffnet die Eigenschaften, Rechtsklick gibt ein Kontextmenü
//  (Eigenschaften / Umbenennen / Neue Karte / Löschen). Umbenennen läuft
//  in-place (TVS_EDITLABELS, wie im RPG Maker XP); das Label-Edit deserialisiert
//  NICHT ins Karten-JSON - beim Wechsel wird gespeichert wie gehabt (Strg+S).
// ----------------------------------------------------------------------------
void maptree_fill() {
    if (!g.map_tree) return;
    SendMessageW(g.map_tree, TVM_DELETEITEM, 0,
                 reinterpret_cast<LPARAM>(TVI_ROOT));
    // Wurzel: Projektname (Karten hängen darunter, XP-Stil).
    TVINSERTSTRUCTW ins{};
    ins.hParent = TVI_ROOT;
    ins.item.mask = TVIF_TEXT | TVIF_CHILDREN;
    ins.hInsertAfter = TVI_FIRST;
    ins.item.pszText = const_cast<LPWSTR>(utf8_to_wide(g.project_name).c_str());
    ins.item.cChildren = 1;
    const HTREEITEM root = TreeView_InsertItem(g.map_tree, &ins);
    for (int i = 0; i < static_cast<int>(g.map_names.size()); ++i) {
        TVINSERTSTRUCTW mi{};
        mi.hParent = root;
        mi.hInsertAfter = TVI_LAST;
        mi.item.mask = TVIF_TEXT | TVIF_PARAM;
        mi.item.pszText = const_cast<LPWSTR>(utf8_to_wide(g.map_names[i]).c_str());
        mi.item.lParam = i;
        TreeView_InsertItem(g.map_tree, &mi);
    }
    // Wurzel aufklappen und aktuelle Karte markieren.
    SendMessageW(g.map_tree, TVM_EXPAND, TVE_EXPAND,
                 reinterpret_cast<LPARAM>(root));
    maptree_select_current();
}

// Markiert die aktuelle Karte im Baum (ohne Ladefehler auszulösen).
void maptree_select_current() {
    if (!g.map_tree) return;
    if (g.cur_map < 0) { TreeView_SelectItem(g.map_tree, nullptr); return; }
    const HTREEITEM root = TreeView_GetRoot(g.map_tree);
    if (!root) return;
    for (HTREEITEM it = TreeView_GetChild(g.map_tree, root); it;
         it = TreeView_GetNextSibling(g.map_tree, it)) {
        TVITEMEXW item{};
        item.mask = TVIF_PARAM;
        item.hItem = it;
        if (TreeView_GetItem(g.map_tree, &item) &&
            static_cast<int>(item.lParam) == g.cur_map) {
            TreeView_SelectItem(g.map_tree, it);
            return;
        }
    }
    TreeView_SelectItem(g.map_tree, nullptr);
}

// Karten-Index des markierten Baum-Eintrags (-1 = Wurzel/nichts markiert).
int maptree_selected() {
    if (!g.map_tree) return -1;
    const HTREEITEM sel = TreeView_GetSelection(g.map_tree);
    if (!sel) return -1;
    TVITEMEXW item{};
    item.mask = TVIF_PARAM | TVIF_CHILDREN;
    item.hItem = sel;
    if (!TreeView_GetItem(g.map_tree, &item)) return -1;
    if (item.cChildren) return -1;   // Wurzel projiziert keine Karte
    return static_cast<int>(item.lParam);
}

// Umbenennen: Namen der Karte in map_root + Liste aktualisieren
// (gespeichert wie üblich per Strg+S, g.dirty markiert die Karte).
void maptree_apply_rename(int index, const std::string& new_name) {
    if (index < 0 || index >= static_cast<int>(g.map_files.size())) return;
    if (new_name.empty()) return;
    if (index == g.cur_map) {
        g.map_root.set("name", JsonValue::make_str(new_name));
        g.dirty = true;
        update_title();
    }
    g.map_names[index] = new_name;
    rebuild_map_menu();
    core::log_info("Karte umbenannt: " + g.map_files[index] + " -> '" +
                   new_name + "' (Strg+S speichert)");
}

// ------------------------------------------------------------
//  Objekt-Operationen (arbeiten NUR auf den Kartendaten)
// ------------------------------------------------------------
float base_y_at(float x, float z) {
    return g.terrain->height_at(x, z);
}

const Prefab* prefab_of(const std::string& type) {
    for (const Prefab& p : g.prefabs) {
        if (p.id == type) return &p;
    }
    return nullptr;
}

std::string obj_type(int i) {
    if (!g.objects || i < 0 || i >= static_cast<int>(g.objects->arr.size())) return "";
    const JsonValue* t = g.objects->arr[i].find("type");
    return t ? t->str : "";
}

float obj_number(int i, const char* key, float fallback) {
    if (!g.objects || i < 0 || i >= static_cast<int>(g.objects->arr.size())) return fallback;
    const JsonValue* v = g.objects->arr[i].find(key);
    return v ? static_cast<float>(v->number(fallback)) : fallback;
}

void set_obj_number(int i, const char* key, float value) {
    if (!g.objects || i < 0 || i >= static_cast<int>(g.objects->arr.size())) return;
    g.objects->arr[i].set(key, JsonValue::make_num(value, false));
    g.dirty = true;
}

// Etappe 19c: zentrales Umschalten der fünf Modi. Die Knöpfe sind Radios
// (BS_AUTORADIOBUTTON); Prefab-Liste und Kontextmenü wechseln den Modus aber auch,
// deshalb wird der Haken hier gesetzt - genau einer ist immer aktiv.
void set_mode(int mode) {
    g.place_mode  = (mode == 1);
    g.move_mode   = (mode == 3);
    g.paint_mode  = (mode == 4);
    g.height_mode = (mode == 5);
    const int ids[6] = { 0, IDC_BTN_PLACE, IDC_BTN_SELECT, IDC_BTN_MOVE,
                         IDC_BTN_PAINT, IDC_BTN_HEIGHT };
    for (int m = 1; m <= 5; ++m)
        SendMessageW(GetDlgItem(g.main, ids[m]), BM_SETCHECK,
                     (m == mode) ? BST_CHECKED : BST_UNCHECKED, 0);
}

// Etappe 19c: Anzeigename/Material eines Objekts fuer Liste und Introspektion.
std::string obj_display_name(int i) {
    const std::string type = obj_type(i);
    if (const Prefab* pr = prefab_of(type)) return pr->name;
    return type;
}

std::string obj_material(int i) {
    if (!g.objects || i < 0 || i >= static_cast<int>(g.objects->arr.size())) return {};
    const JsonValue* t = g.objects->arr[i].find("tex");
    return (t && t->is_str()) ? t->str : std::string();
}

// Sortierschluessel der Objektliste (Spalte in g.obj_sort_col).
bool obj_sort_less(int a, int b) {
    switch (g.obj_sort_col) {
        case 1:  return obj_display_name(a) < obj_display_name(b);
        case 2:  return obj_number(a, "x", 0) < obj_number(b, "x", 0);
        case 3:  return obj_number(a, "z", 0) < obj_number(b, "z", 0);
        case 4:  return obj_number(a, "rot", 0) < obj_number(b, "rot", 0);
        case 5:  return obj_material(a) < obj_material(b);
        default: return a < b;
    }
}

int obj_row_of(int index) {
    for (int r = 0; r < static_cast<int>(g.obj_order.size()); ++r)
        if (g.obj_order[r] == index) return r;
    return -1;
}

// Etappe 19c: Objektliste als JSON-Schnappschuss nach log/editor_objects.json.
// Texte lassen sich prozessfremd nicht per SendMessage lesen (siehe BACKLOG);
// eine Datei im Log-Verzeichnis kann jedes Werkzeug - und hilft beim Debuggen.
void write_objects_introspection() {
    if (!g.objects) return;
    const int n = static_cast<int>(g.objects->arr.size());
    std::string js = "[\n";
    for (int i = 0; i < n; ++i) {
        char line[512];
        std::snprintf(line, sizeof(line),
                      "  {\"i\": %d, \"type\": \"%s\", \"name\": \"%s\", "
                      "\"x\": %.3f, \"z\": %.3f, \"rot\": %.4f, \"tex\": \"%s\"}%s\n",
                      i, obj_type(i).c_str(), obj_display_name(i).c_str(),
                      obj_number(i, "x", 0), obj_number(i, "z", 0),
                      obj_number(i, "rot", 0), obj_material(i).c_str(),
                      (i + 1 < n) ? "," : "");
        js += line;
    }
    js += "]\n";
    static std::string cache;
    if (js == cache) return;
    cache = js;
    write_file(g.root + "\\log\\editor_objects.json", js);
}

void sync_object_list() {
    if (!g.list_objs || !g.objects) return;
    // Etappe 19b: durchgehend W-Varianten - die Namen kommen als UTF-8 aus
    // dem Karten-JSON und dürfen nicht durch die ANSI-Codepage (CP1252)
    // laufen, sonst erscheint "Hütte" in der Liste doppelt kodiert (Ã-Form).
    // Zeilenreihenfolge: Datenreihenfolge oder Sortierung nach Spalte.
    const int n = static_cast<int>(g.objects->arr.size());
    g.obj_order.assign(static_cast<size_t>(n), 0);
    for (int i = 0; i < n; ++i) g.obj_order[static_cast<size_t>(i)] = i;
    if (g.obj_sort_col >= 0) {
        std::stable_sort(g.obj_order.begin(), g.obj_order.end(),
                         [](int a, int b) { return obj_sort_less(a, b); });
        if (!g.obj_sort_asc) std::reverse(g.obj_order.begin(), g.obj_order.end());
    }
    ListView_DeleteAllItems(g.list_objs);
    wchar_t buf[160];
    for (int row = 0; row < n; ++row) {
        const int i = g.obj_order[static_cast<size_t>(row)];
        LVITEMW it{};
        it.mask = LVIF_TEXT | LVIF_PARAM;
        it.iItem = row;
        it.iSubItem = 0;
        it.lParam = i;                      // Zeile -> Objektindex
        std::swprintf(buf, 160, L"%d", i + 1);
        it.pszText = buf;
        const int idx = ListView_InsertItem(g.list_objs, &it);
        const std::wstring name = utf8_to_wide(obj_display_name(i));
        ListView_SetItemText(g.list_objs, idx, 1, const_cast<wchar_t*>(name.c_str()));
        std::swprintf(buf, 160, L"%.1f", obj_number(i, "x", 0));
        ListView_SetItemText(g.list_objs, idx, 2, buf);
        std::swprintf(buf, 160, L"%.1f", obj_number(i, "z", 0));
        ListView_SetItemText(g.list_objs, idx, 3, buf);
        std::swprintf(buf, 160, L"%.0f\u00b0", obj_number(i, "rot", 0) * 180.0f / 3.14159265f);
        ListView_SetItemText(g.list_objs, idx, 4, buf);
        const std::wstring mat = utf8_to_wide(obj_material(i));
        ListView_SetItemText(g.list_objs, idx, 5, const_cast<wchar_t*>(mat.c_str()));
    }
    const int sel_row = obj_row_of(g.sel_obj);
    if (sel_row >= 0)
        ListView_SetItemState(g.list_objs, sel_row, LVIS_SELECTED | LVIS_FOCUSED,
                              LVIS_SELECTED | LVIS_FOCUSED);
    write_objects_introspection();
}

void json_remove_key(JsonValue& v, const std::string& key) {
    for (auto it = v.obj.begin(); it != v.obj.end(); ++it) {
        if (it->first == key) { v.obj.erase(it); return; }
    }
}

void sync_property_fields() {
    if (g.sel_obj < 0) {
        SetWindowTextW(g.ed_x, L"");
        SetWindowTextW(g.ed_z, L"");
        SetWindowTextW(g.ed_rot, L"");
        SetWindowTextW(g.ed_tex, L"");
        return;
    }
    wchar_t buf[32];
    std::swprintf(buf, 32, L"%.2f", obj_number(g.sel_obj, "x", 0));
    SetWindowTextW(g.ed_x, buf);
    std::swprintf(buf, 32, L"%.2f", obj_number(g.sel_obj, "z", 0));
    SetWindowTextW(g.ed_z, buf);
    std::swprintf(buf, 32, L"%.1f", obj_number(g.sel_obj, "rot", 0) * 180.0f / 3.14159265f);
    SetWindowTextW(g.ed_rot, buf);
    const JsonValue* tex = g.objects->arr[g.sel_obj].find("tex");
    SetWindowTextW(g.ed_tex, (tex && tex->is_str()) ? utf8_to_wide(tex->str).c_str() : L"");
}

void select_object(int i, bool from_list) {
    const int count = g.objects ? static_cast<int>(g.objects->arr.size()) : 0;
    g.sel_obj = (i >= 0 && i < count) ? i : -1;
    if (!from_list) sync_object_list();
    sync_property_fields();
}

void status_mode(const wchar_t* text) {
    g.status_mode = text;
}

// ------------------------------------------------------------
//  Undo/Redo (Etappe 17c): Objekt-Array als JSON-Schnappschuss.
//  push_undo() VOR jeder Änderung aufrufen.
// ------------------------------------------------------------
void push_undo() {
    if (!g.objects) return;
    g.undo_stack.push_back(json_write(*g.objects));
    if (g.undo_stack.size() > 50) g.undo_stack.erase(g.undo_stack.begin());
    g.redo_stack.clear();
}

void restore_objects(const std::string& text) {
    try {
        JsonValue v = json_parse(text);
        g.map_root.set("objects", v);
    } catch (...) {
        core::log_error("Undo: Schnappschuss ungültig");
        return;
    }
    g.objects = g.map_root.find("objects");
    g.sel_obj = -1;
    g.dirty = true;
    sync_object_list();
    sync_property_fields();
    update_title();
}

void undo_action() {
    if (g.undo_stack.empty()) { status_mode(L"Nichts rückgängig"); return; }
    if (g.objects) g.redo_stack.push_back(json_write(*g.objects));
    const std::string text = g.undo_stack.back();
    g.undo_stack.pop_back();
    restore_objects(text);
    status_mode(L"Rückgängig (Strg+Z)");
}

void redo_action() {
    if (g.redo_stack.empty()) { status_mode(L"Nichts wiederholen"); return; }
    if (g.objects) g.undo_stack.push_back(json_write(*g.objects));
    const std::string text = g.redo_stack.back();
    g.redo_stack.pop_back();
    restore_objects(text);
    status_mode(L"Wiederholt (Strg+Y)");
}

void place_object(float x, float z) {
    if (g.sel_prefab < 0 || g.sel_prefab >= static_cast<int>(g.prefabs.size())) return;
    push_undo();
    // Raster-Rasten (Shift-lose Standardbedienung, wie im alten Editor)
    x = std::round(x / g.grid) * g.grid;
    z = std::round(z / g.grid) * g.grid;

    JsonValue obj = JsonValue::make_obj();
    obj.set("type", JsonValue::make_str(g.prefabs[g.sel_prefab].id));
    obj.set("x", JsonValue::make_num(x, false));
    obj.set("z", JsonValue::make_num(z, false));
    obj.set("rot", JsonValue::make_num(g.place_rot, false));
    g.objects->arr.push_back(std::move(obj));
    g.dirty = true;

    char buf[128];
    std::snprintf(buf, sizeof(buf), "Gesetzt: %s bei %.1f/%.1f, Drehung %.0f Grad",
                  g.prefabs[g.sel_prefab].name.c_str(), x, z,
                  g.place_rot * 180.0 / 3.14159265);
    core::log_info(buf);
    sync_object_list();
}

void delete_selected() {
    if (g.sel_obj < 0 || !g.objects) return;
    push_undo();
    g.objects->arr.erase(g.objects->arr.begin() + g.sel_obj);
    g.dirty = true;
    core::log_info("Objekt gelöscht");
    select_object(-1, false);
}

void rotate_selected(float delta_rad) {
    if (g.sel_obj < 0) return;
    const float rot = obj_number(g.sel_obj, "rot", 0) + delta_rad;
    set_obj_number(g.sel_obj, "rot", rot);
    sync_property_fields();
}

void duplicate_selected() {
    if (g.sel_obj < 0 || !g.objects) return;
    push_undo();
    JsonValue copy = g.objects->arr[g.sel_obj];
    JsonValue* x = copy.find("x");
    JsonValue* z = copy.find("z");
    if (x) x->num += 1.0;
    if (z) z->num += 1.0;
    g.objects->arr.push_back(std::move(copy));
    g.dirty = true;
    core::log_info("Objekt dupliziert");
    select_object(static_cast<int>(g.objects->arr.size()) - 1, false);
}

void apply_property_fields() {
    if (g.sel_obj < 0) return;
    push_undo();
    wchar_t buf[64];
    auto get = [&](HWND h) {
        GetWindowTextW(h, buf, 64);
        std::string s = wide_to_utf8(buf);
        std::replace(s.begin(), s.end(), ',', '.');   // deutsches Komma erlauben
        return static_cast<float>(std::atof(s.c_str()));
    };
    set_obj_number(g.sel_obj, "x", get(g.ed_x));
    set_obj_number(g.sel_obj, "z", get(g.ed_z));
    set_obj_number(g.sel_obj, "rot", get(g.ed_rot) * 3.14159265f / 180.0f);

    // Etappe 17f: Materialname (leer = Prefab-Texturen behalten).
    wchar_t tbuf[128];
    GetWindowTextW(g.ed_tex, tbuf, 128);
    std::string tex = wide_to_utf8(tbuf);
    tex.erase(tex.begin(), std::find_if(tex.begin(), tex.end(),
              [](unsigned char c) { return !std::isspace(c); }));
    while (!tex.empty() && std::isspace(static_cast<unsigned char>(tex.back()))) tex.pop_back();
    JsonValue& obj = g.objects->arr[g.sel_obj];
    if (tex.empty()) json_remove_key(obj, "tex");
    else obj.set("tex", JsonValue::make_str(tex));

    sync_object_list();
}

// ------------------------------------------------------------
//  Picking: Strahl aus Kamera + Pixel, Terrain- und Objekt-Treffer
// ------------------------------------------------------------
math::Vec3 cam_forward() {
    return math::Vec3{ std::sin(g.cam_yaw) * std::cos(g.cam_pitch),
                       std::sin(g.cam_pitch),
                       std::cos(g.cam_yaw) * std::cos(g.cam_pitch) };
}

math::Vec3 ray_dir(float ndc_x, float ndc_y, float aspect) {
    const math::Vec3 fwd = cam_forward();
    const math::Vec3 right = math::normalize(math::cross(math::Vec3{ 0, 1, 0 }, fwd));
    const math::Vec3 up = math::cross(fwd, right);
    const float th = std::tan(3.14159265f / 6.0f);   // fov 60 Grad
    return math::normalize(
        math::add(math::add(fwd, math::scale(right, ndc_x * th * aspect)),
                  math::scale(up, ndc_y * th)));
}

// Ray-March übers Gelände (wie der alte Editor.terrain_hit).
bool terrain_hit(const math::Vec3& eye, const math::Vec3& dir, math::Vec3& out) {
    const float max_t = 400.0f;
    float t_prev = 0.0f;
    float d_prev = eye.y - g.terrain->height_at(eye.x, eye.z);
    for (float t = 0.5f; t < max_t; t += 0.5f) {
        const math::Vec3 p = math::add(eye, math::scale(dir, t));
        const float d = p.y - g.terrain->height_at(p.x, p.z);
        if (d <= 0.0f) {
            // Halbiere das Intervall für eine genaue Trefferstelle.
            float lo = t_prev, hi = t;
            for (int i = 0; i < 12; ++i) {
                const float mid = (lo + hi) * 0.5f;
                const math::Vec3 pm = math::add(eye, math::scale(dir, mid));
                if (pm.y - g.terrain->height_at(pm.x, pm.z) <= 0.0f) hi = mid; else lo = mid;
            }
            out = math::add(eye, math::scale(dir, hi));
            return true;
        }
        t_prev = t;
        d_prev = d;
    }
    (void)d_prev;
    return false;
}

// Ray-AABB im Objektraum (Slab-Methode); rot = Drehung um die Hochachse.
bool ray_box(const math::Vec3& eye, const math::Vec3& dir,
             const math::Vec3& center, const math::Vec3& half, float rot,
             float& out_t) {
    // Strahl in den lokalen Raum drehen (-rot um Y).
    const float c = std::cos(-rot), s = std::sin(-rot);
    const math::Vec3 o = math::sub(eye, center);
    const math::Vec3 l_o{ o.x * c + o.z * s, o.y, -o.x * s + o.z * c };
    const math::Vec3 l_d{ dir.x * c + dir.z * s, dir.y, -dir.x * s + dir.z * c };

    float tmin = -1e9f, tmax = 1e9f;
    const float* lo = &l_o.x;
    const float* ld = &l_d.x;
    const float* lh = &half.x;
    for (int a = 0; a < 3; ++a) {
        if (std::fabs(ld[a]) < 1e-8f) {
            if (std::fabs(lo[a]) > lh[a]) return false;
            continue;
        }
        float t1 = (-lh[a] - lo[a]) / ld[a];
        float t2 = ( lh[a] - lo[a]) / ld[a];
        if (t1 > t2) std::swap(t1, t2);
        tmin = std::max(tmin, t1);
        tmax = std::min(tmax, t2);
        if (tmin > tmax) return false;
    }
    if (tmax < 0.0f) return false;
    out_t = tmin > 0.0f ? tmin : tmax;
    return true;
}

// Nächstes Objekt unter dem Strahl (Teil-Kästen im Objektraum).
int object_hit(const math::Vec3& eye, const math::Vec3& dir, float& out_t) {
    int best = -1;
    float best_t = 1e9f;
    if (!g.objects) return -1;
    for (int i = 0; i < static_cast<int>(g.objects->arr.size()); ++i) {
        const Prefab* pre = prefab_of(obj_type(i));
        if (!pre) continue;
        const float x = obj_number(i, "x", 0);
        const float z = obj_number(i, "z", 0);
        const float rot = obj_number(i, "rot", 0);
        const float by = base_y_at(x, z);
        for (const PrefabPart& part : pre->parts) {
            const float cd = std::cos(rot), sd = std::sin(rot);
            const float dx = part.d.x, dz = part.d.z;
            const math::Vec3 center{ x + dx * cd + dz * sd,
                                     by + part.d.y,
                                     z - dx * sd + dz * cd };
            float t = 0.0f;
            if (ray_box(eye, dir, center, math::scale(part.s, 0.5f), rot, t) && t < best_t) {
                best_t = t;
                best = i;
            }
        }
    }
    out_t = best_t;
    return best;
}

// ------------------------------------------------------------
//  Win32: Steuerelemente und Fenster
// ------------------------------------------------------------
void apply_font(HWND h) {
    SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(g.font), TRUE);
}

HWND make_control(const wchar_t* cls, const wchar_t* text, DWORD style,
                  int x, int y, int w, int h, int id) {
    HWND ctrl = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style,
                                x, y, w, h, g.main,
                                reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                                GetModuleHandleW(nullptr), nullptr);
    apply_font(ctrl);
    return ctrl;
}

void renderer_resize(int w, int h);

void renderer_resize(int w, int h) {
    if (!g.renderer) return;
    static int last_w = -1, last_h = -1;
    if (w != last_w || h != last_h) {
        last_w = w;
        last_h = h;
        core::log_info("Viewport: " + std::to_string(w) + " x " + std::to_string(h));
    }
    if (w > 0 && h > 0) g.renderer->resize(w, h);
}

// ============================================================================
//  Etappe 17h: Werkzeugleiste mit prozeduralen Icons.
//
//  Die Icons werden zur Laufzeit mit GDI in 16x16-Bitmaps gezeichnet (wie die
//  Texturen/Sounds komplett im Code, kein Ressourcen-File). Die Buttons
//  schicken dieselben IDs wie das Menü (WM_COMMAND), deshalb verteilt sich
//  nichts doppelt.
// ============================================================================

// Ein 16x16-Icon zeichnen: draw() malt in ein frisches HDC (weisser Grund).
HICON make_toolbar_icon(void (*draw)(HDC)) {
    HDC screen = GetDC(nullptr);
    HDC mem = CreateCompatibleDC(screen);
    HBITMAP bmp = CreateCompatibleBitmap(screen, 16, 16);
    HBITMAP mask = CreateCompatibleBitmap(screen, 16, 16);
    HGDIOBJ old = SelectObject(mem, bmp);
    RECT rc{ 0, 0, 16, 16 };
    FillRect(mem, &rc, reinterpret_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
    draw(mem);
    SelectObject(mem, old);

    // Maske: alles ausser dem Weissen der Ecken bleibt sichtbar; für die
    // schlichten Formen reicht Schwarz (0) = opak.
    HDC mask_dc = CreateCompatibleDC(screen);
    HGDIOBJ oldm = SelectObject(mask_dc, mask);
    FillRect(mask_dc, &rc, reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
    SelectObject(mask_dc, oldm);
    DeleteDC(mask_dc);

    ICONINFO ii{};
    ii.fIcon = TRUE;
    ii.hbmMask = mask;
    ii.hbmColor = bmp;
    HICON icon = CreateIconIndirect(&ii);

    DeleteObject(bmp);
    DeleteObject(mask);
    DeleteDC(mem);
    ReleaseDC(nullptr, screen);
    return icon;
}

// Zeichen-Helfer für die einzelnen Icons.
namespace ico {

void diskette(HDC dc) {          // Speichern
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(40, 70, 110));
    HGDIOBJ op = SelectObject(dc, pen);
    HBRUSH br = CreateSolidBrush(RGB(70, 120, 180));
    HGDIOBJ ob = SelectObject(dc, br);
    Rectangle(dc, 2, 2, 14, 14);
    SelectObject(dc, GetStockObject(WHITE_BRUSH));
    Rectangle(dc, 5, 2, 11, 7);
    SelectObject(dc, br);
    Rectangle(dc, 4, 8, 12, 13);
    SelectObject(dc, GetStockObject(WHITE_BRUSH));
    Rectangle(dc, 5, 4, 10, 5);
    SelectObject(dc, op);
    SelectObject(dc, ob);
    DeleteObject(pen);
    DeleteObject(br);
}

void props(HDC dc) {             // Karten-Eigenschaften (Zahnrad)
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(110, 110, 110));
    HGDIOBJ op = SelectObject(dc, pen);
    HBRUSH br = CreateSolidBrush(RGB(190, 190, 190));
    HGDIOBJ ob = SelectObject(dc, br);
    Ellipse(dc, 4, 4, 13, 13);
    SelectObject(dc, GetStockObject(WHITE_BRUSH));
    Ellipse(dc, 6, 6, 11, 11);
    for (int a = 0; a < 8; ++a) {
        const double w = a * 0.7854 + 0.3927;
        const int x = static_cast<int>(8.5 + std::cos(w) * 6.5);
        const int y = static_cast<int>(8.5 + std::sin(w) * 6.5);
        MoveToEx(dc, x, y, nullptr);
        LineTo(dc, static_cast<int>(8.5 + std::cos(w) * 8.0),
               static_cast<int>(8.5 + std::sin(w) * 8.0));
    }
    SelectObject(dc, op);
    SelectObject(dc, ob);
    DeleteObject(pen);
    DeleteObject(br);
}

void events(HDC dc) {            // Event-Editor (Blitz/Kommandokette)
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(150, 90, 20));
    HGDIOBJ op = SelectObject(dc, pen);
    HBRUSH br = CreateSolidBrush(RGB(255, 200, 60));
    HGDIOBJ ob = SelectObject(dc, br);
    POINT bolt[] = { {10, 1}, {4, 9}, {8, 9}, {6, 15}, {13, 6}, {9, 6}, {11, 1} };
    Polygon(dc, bolt, 7);
    SelectObject(dc, op);
    SelectObject(dc, ob);
    DeleteObject(pen);
    DeleteObject(br);
}

void scripts(HDC dc) {           // Script-Editor (Blatt mit Zeilen)
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(60, 100, 60));
    HGDIOBJ op = SelectObject(dc, pen);
    Rectangle(dc, 3, 1, 13, 15);
    MoveToEx(dc, 5, 4, nullptr);  LineTo(dc, 11, 4);
    MoveToEx(dc, 5, 7, nullptr);  LineTo(dc, 11, 7);
    MoveToEx(dc, 5, 10, nullptr); LineTo(dc, 10, 10);
    SelectObject(dc, op);
    DeleteObject(pen);
}

void materials(HDC dc) {         // Materialbase (gemusterter Würfel)
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(60, 60, 60));
    HGDIOBJ op = SelectObject(dc, pen);
    HBRUSH br = CreateSolidBrush(RGB(120, 90, 60));
    HGDIOBJ ob = SelectObject(dc, br);
    Rectangle(dc, 2, 5, 14, 14);
    SelectObject(dc, GetStockObject(DKGRAY_BRUSH));
    Rectangle(dc, 4, 2, 12, 5);
    SelectObject(dc, op);
    SelectObject(dc, ob);
    DeleteObject(pen);
    DeleteObject(br);
}

void database(HDC dc) {          // Datenbank (Held + Karte)
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(90, 60, 120));
    HGDIOBJ op = SelectObject(dc, pen);
    HBRUSH br = CreateSolidBrush(RGB(180, 150, 220));
    HGDIOBJ ob = SelectObject(dc, br);
    Rectangle(dc, 2, 3, 14, 13);
    Rectangle(dc, 2, 12, 14, 13);   // Korpus
    SelectObject(dc, GetStockObject(WHITE_BRUSH));
    Ellipse(dc, 3, 1, 7, 5);       // Kopf
    Rectangle(dc, 4, 4, 6, 9);
    SelectObject(dc, op);
    SelectObject(dc, ob);
    DeleteObject(pen);
    DeleteObject(br);
}

void speaker(HDC dc) {           // Sound-Test (Lautsprecher)
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(30, 90, 30));
    HGDIOBJ op = SelectObject(dc, pen);
    HBRUSH br = CreateSolidBrush(RGB(110, 190, 110));
    HGDIOBJ ob = SelectObject(dc, br);
    POINT spk[] = { {2, 6}, {5, 6}, {9, 2}, {9, 14}, {5, 10}, {2, 10} };
    Polygon(dc, spk, 6);
    MoveToEx(dc, 11, 5, nullptr);
    LineTo(dc, 11, 11);
    SelectObject(dc, op);
    SelectObject(dc, ob);
    DeleteObject(pen);
    DeleteObject(br);
}

void playtest(HDC dc) {          // Playtest (grünes Dreieck)
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(20, 90, 20));
    HGDIOBJ op = SelectObject(dc, pen);
    HBRUSH br = CreateSolidBrush(RGB(80, 200, 80));
    HGDIOBJ ob = SelectObject(dc, br);
    POINT tri[] = { {4, 2}, {14, 8}, {4, 14} };
    Polygon(dc, tri, 3);
    SelectObject(dc, op);
    SelectObject(dc, ob);
    DeleteObject(pen);
    DeleteObject(br);
}

} // namespace ico

static HWND g_toolbar = nullptr;
static int  g_toolbar_h = 0;

int toolbar_height() {
    return g_toolbar_h;
}

void create_toolbar(HWND parent) {
    if (g_toolbar) return;

    InitCommonControls();
    g_toolbar = CreateWindowExW(0, TOOLBARCLASSNAMEW, nullptr,
                                WS_CHILD | WS_VISIBLE | TBSTYLE_TOOLTIPS |
                                CCS_NODIVIDER | CCS_NOPARENTALIGN,
                                0, 0, 0, 0, parent,
                                reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_TOOLBAR)),
                                GetModuleHandleW(nullptr), nullptr);
    SendMessageW(g_toolbar, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);
    SendMessageW(g_toolbar, TB_SETBITMAPSIZE, 0, MAKELPARAM(16, 16));

    // Imageliste aus den prozeduralen Icons.
    HIMAGELIST il = ImageList_Create(16, 16, ILC_COLOR32, 8, 0);
    struct { void (*draw)(HDC); } defs[] = {
        ico::diskette, ico::props,    ico::events,  ico::scripts,
        ico::materials, ico::database, ico::speaker, ico::playtest,
    };
    for (auto& d : defs) {
        HICON ic = make_toolbar_icon(d.draw);
        ImageList_AddIcon(il, ic);
        DestroyIcon(ic);
    }
    SendMessageW(g_toolbar, TB_SETIMAGELIST, 0, reinterpret_cast<LPARAM>(il));

    // Buttons: dieselben IDs wie das Menü - WM_COMMAND verteilt schon.
    TBBUTTON btns[8]{};
    for (int i = 0; i < 8; ++i) {
        btns[i].iBitmap = i;
        btns[i].fsState = TBSTATE_ENABLED;
        btns[i].fsStyle = BTNS_BUTTON;
    }
    btns[0].idCommand = ID_MENU_SAVE;
    btns[1].idCommand = ID_MENU_MAPPROPS;
    btns[2].idCommand = ID_MENU_EVENTS;
    btns[3].idCommand = ID_MENU_SCRIPTS;
    btns[4].idCommand = ID_MENU_MATERIALS;
    btns[5].idCommand = ID_MENU_DATABASE;
    btns[6].idCommand = ID_MENU_SOUNDTEST;
    btns[7].idCommand = ID_MENU_PLAYTEST;
    SendMessageW(g_toolbar, TB_ADDBUTTONS, 8, reinterpret_cast<LPARAM>(btns));

    // Tooltip-Texte direkt an die Buttons hängen.
    const wchar_t* tips[8] = {
        L"Karte speichern (Strg+S)", L"Karten-Eigenschaften (F2)",
        L"Event-Editor (F6)", L"Script-Editor (F11)",
        L"Materialbase (F3)", L"Datenbank: Helden/Items (F9)",
        L"Sound-Test (F8)", L"Playtest starten (F4)"
    };
    for (int i = 0; i < 8; ++i) {
        TBBUTTONINFO bi{};
        bi.cbSize = sizeof(bi);
        bi.dwMask = TBIF_TEXT;
        bi.pszText = const_cast<LPWSTR>(tips[i]);
        SendMessageW(g_toolbar, TB_SETBUTTONINFO, btns[i].idCommand,
                     reinterpret_cast<LPARAM>(&bi));
    }

    // Höhe ermitteln (TB_GETBUTTONSIZE liefert MAKELONG(breite, höhe)).
    const DWORD bs = static_cast<DWORD>(
        SendMessageW(g_toolbar, TB_GETBUTTONSIZE, 0, 0));
    g_toolbar_h = HIWORD(bs) > 0 ? HIWORD(bs) : 24;
    SendMessageW(g_toolbar, TB_AUTOSIZE, 0, 0);
}

void layout_children() {
    if (!g.view || !g.status) return;   // WM_SIZE kommt schon vor dem Aufbau
    RECT rc;
    GetClientRect(g.main, &rc);
    const int W = rc.right, H = rc.bottom;
    SendMessageW(g.status, WM_SIZE, 0, 0);

    RECT sb;
    GetWindowRect(g.status, &sb);
    const int sb_h = sb.bottom - sb.top;
    const int tb_h = toolbar_height();      // Etappe 17h: Werkzeugleiste
    // Etappe 18i: Die Spalten wachsen mit dem Fenster (begrenzt). Bei 1024x640
    // bleiben die alten Mindestbreiten, bei grossen Fenstern bekommen
    // Objektliste und Beschriftungen mehr Platz; der Viewport nimmt den Rest.
    const int left_w  = std::clamp(W * 16 / 100, 214, 280);
    const int right_w = std::clamp(W * 19 / 100, 258, 340);
    const int view_x = left_w;
    const int view_w = std::max(100, W - left_w - right_w);
    const int view_h = std::max(100, H - sb_h - tb_h);

    SetWindowPos(g.view, nullptr, view_x, tb_h, view_w, view_h,
                 SWP_NOZORDER | SWP_NOACTIVATE);

    // Statusleisten-Felder proportional zur Fensterbreite (früher fest 160/
    // 130/220 - bei schmalen Fenstern wurde das letzte Feld zu eng).
    const int sb_parts[4] = { std::max(120, W / 8), std::max(100, W / 9),
                              std::max(170, W / 6), -1 };
    SendMessageW(g.status, SB_SETPARTS, 4, reinterpret_cast<LPARAM>(sb_parts));

    // Linke Spalte: Etappe 18k teilt sie in Kartenbaum (oben) und Werkzeuge
    // (unten). Der Baum behält eine feste Höhe, Werkzeugliste + Bedienblock
    // verteilen sich daruber wie bisher (18i-Logik).
    const int lw = left_w - 16;
    const int bx = 8;
    const int bw2 = (lw - 8) / 2;
    const int tree_h = 150;                              // Kartenbaum
    // Etappe 19c: feste Höhen der Gruppen, Prefab-Liste bekommt den Rest.
    const int grp_tool_h = 5 * 26 + 26;    // 5 Radios + Rahmentitel
    const int grp_act_h  = 2 * 32 + 30;    // 2 Aktionszeilen + Rahmentitel
    const int undo_h     = 34;
    const int y_tool = tb_h + tree_h + 4;
    const int y_prefab = y_tool + grp_tool_h + 6;
    const int y_act = H - sb_h - 8 - undo_h - grp_act_h;
    const int y_undo = H - sb_h - 6 - undo_h;

    auto move_child = [](HWND hwnd, int x, int y, int w, int ht) {
        if (hwnd) SetWindowPos(hwnd, nullptr, x, y, w, ht,
                               SWP_NOZORDER | SWP_NOACTIVATE);
    };
    // Etappe 18k: Kartenbaum + KARTEN-Label oben.
    move_child(GetDlgItem(g.main, IDC_LBL_MAPS), bx, tb_h + 8, lw, 20);
    move_child(g.map_tree, bx, tb_h + 30, lw, tree_h - 30);
    // Modus-Gruppe: Radios untereinander, volle Breite.
    move_child(GetDlgItem(g.main, IDC_GRP_TOOL), bx - 4, y_tool, lw + 8, grp_tool_h);
    const int radios[5] = { IDC_BTN_PLACE, IDC_BTN_SELECT, IDC_BTN_MOVE,
                            IDC_BTN_PAINT, IDC_BTN_HEIGHT };
    for (int r = 0; r < 5; ++r)
        move_child(GetDlgItem(g.main, radios[r]), bx + 6, y_tool + 18 + r * 26, lw - 12, 24);
    // Prefab-Gruppe füllt die Mitte (kein toter Leerraum mehr).
    move_child(GetDlgItem(g.main, IDC_GRP_PREFAB), bx - 4, y_prefab, lw + 8,
               std::max(90, y_act - 6 - y_prefab));
    move_child(g.list_prefab, bx + 6, y_prefab + 18, lw - 12,
               std::max(60, y_act - 6 - y_prefab - 26));
    // Aktions-Gruppe + Undo/Redo am Spaltenende.
    move_child(GetDlgItem(g.main, IDC_GRP_ACT), bx - 4, y_act, lw + 8, grp_act_h);
    move_child(GetDlgItem(g.main, IDC_BTN_ROTATE), bx + 6, y_act + 18, bw2, 28);
    move_child(GetDlgItem(g.main, IDC_BTN_DELETE), bx + bw2 + 12, y_act + 18, bw2, 28);
    move_child(GetDlgItem(g.main, IDC_BTN_DUPLICATE), bx + 6, y_act + 50, lw - 12, 28);
    move_child(GetDlgItem(g.main, IDC_BTN_UNDO), bx, y_undo, bw2, 30);
    move_child(GetDlgItem(g.main, IDC_BTN_REDO), bx + bw2 + 8, y_undo, bw2, 30);

    // Rechte Spalte: Objektliste wächst nach oben, Eigenschaften bleiben
    // unten (fixe Höhe) und nutzen die neue Breite für die Eingabefelder.
    const int rx = W - right_w;
    const int rw = right_w - 16;
    const int py = H - sb_h - 242;
    move_child(GetDlgItem(g.main, IDC_LBL_OBJECTS), rx + 8, tb_h + 8, rw, 20);
    move_child(g.list_objs, rx + 8, tb_h + 30, rw,
               std::max(80, py - tb_h - 38));
    move_child(GetDlgItem(g.main, IDC_GRP_PROPS), rx + 4, py, right_w - 8, 238);
    move_child(GetDlgItem(g.main, IDC_LBL_X), rx + 14, py + 28, 60, 20);
    move_child(g.ed_x, rx + 80, py + 24, right_w - 100, 24);
    move_child(GetDlgItem(g.main, IDC_LBL_Z), rx + 14, py + 60, 60, 20);
    move_child(g.ed_z, rx + 80, py + 56, right_w - 100, 24);
    move_child(GetDlgItem(g.main, IDC_LBL_ROT), rx + 14, py + 92, 60, 20);
    move_child(g.ed_rot, rx + 80, py + 88, right_w - 100, 24);
    move_child(GetDlgItem(g.main, IDC_LBL_TEX), rx + 14, py + 124, 60, 20);
    move_child(g.ed_tex, rx + 80, py + 120, right_w - 100, 24);
    move_child(GetDlgItem(g.main, IDC_BTN_APPLY), rx + 14, py + 158,
               right_w - 28, 30);

    // Renderer auf die neue Viewport-Grösse bringen.
    renderer_resize(view_w, view_h);
}

void update_title() {
    std::wstring title = L"RuneEditor";
    if (!g.project_name.empty()) title += L" — " + utf8_to_wide(g.project_name);
    if (g.cur_map >= 0) {
        title += L" — " + (g.cur_map < static_cast<int>(g.map_names.size())
                               ? utf8_to_wide(g.map_names[g.cur_map]) : L"?");
        if (g.dirty) title += L" *";
    }
    SetWindowTextW(g.main, title.c_str());
}

void status_text(int part, const std::wstring& text) {
    SendMessageW(g.status, SB_SETTEXTW, part, reinterpret_cast<LPARAM>(text.c_str()));
}

void update_status() {
    wchar_t buf[128];
    status_text(0, g.status_mode);
    std::swprintf(buf, 128, L"Objekte: %d", g.objects ? static_cast<int>(g.objects->arr.size()) : 0);
    status_text(1, buf);
    if (g.hover_valid) {
        // Etappe 19c: im Setzen-Modus zeigt das Feld, WO der Geist einrastet
        // und wie er gedreht ist - genau das setzt der Klick.
        if (g.place_mode) {
            const float gx = std::round(g.hover_pos.x / g.grid) * g.grid;
            const float gz = std::round(g.hover_pos.z / g.grid) * g.grid;
            std::swprintf(buf, 128, L"Setzen @ %.1f / %.1f | Drehung %.0f° (R)",
                          gx, gz, g.place_rot * 180.0f / 3.14159265f);
        } else {
            std::swprintf(buf, 128, L"Cursor: %.1f / %.1f", g.hover_pos.x, g.hover_pos.z);
        }
    } else {
        std::swprintf(buf, 128, L"Cursor: —");
    }
    status_text(2, buf);
    std::swprintf(buf, 128, L"Raster %.2f m | %.0f FPS", g.grid, g.fps);
    status_text(3, buf);
}

// ------------------------------------------------------------
//  Viewport: Eingabe (Maus) + D3D-Ausgabe
// ------------------------------------------------------------
LRESULT CALLBACK view_proc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    // Während eines offenen Dialogs keine Kamera-/Editier-Eingaben.
    if (g.props_dlg && (msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST)) return 0;
    switch (msg) {
        case WM_SIZE:
            return 0;
        case WM_RBUTTONDOWN:
            g.look_drag = true;
            g.last_mouse.x = GET_X_LPARAM(lp);
            g.last_mouse.y = GET_Y_LPARAM(lp);
            SetCapture(h);
            return 0;
        case WM_RBUTTONUP:
            g.look_drag = false;
            ReleaseCapture();
            return 0;
        case WM_MOUSEMOVE: {
            const int mx = GET_X_LPARAM(lp);
            const int my = GET_Y_LPARAM(lp);
            if (g.look_drag) {
                g.cam_yaw   += static_cast<float>(mx - g.last_mouse.x) * 0.005f;
                g.cam_pitch -= static_cast<float>(my - g.last_mouse.y) * 0.005f;
                g.cam_pitch = std::clamp(g.cam_pitch, -1.45f, 1.45f);
            }
            g.last_mouse.x = mx;
            g.last_mouse.y = my;
            g.mouse_x = static_cast<float>(mx);
            g.mouse_y = static_cast<float>(my);
            if (g.dragging && g.obj_drag >= 0 && g.obj_drag < static_cast<int>(g.objects->arr.size())) {
                // Ray aus NDC (fov 60 Grad, Seitenverhältnis beachten) und
                // Schnitt mit der horizontalen Ebene y = drag_y.
                RECT rc;
                GetClientRect(h, &rc);
                const float vw = static_cast<float>(std::max(1, static_cast<int>(rc.right)));
                const float vh = static_cast<float>(std::max(1, static_cast<int>(rc.bottom)));
                const float ndc_x = (2.0f * static_cast<float>(mx) / vw) - 1.0f;
                const float ndc_y = 1.0f - (2.0f * static_cast<float>(my) / vh);
                const math::Vec3 dir = ray_dir(ndc_x, ndc_y, vw / vh);
                if (std::fabs(dir.y) > 1e-4f) {
                    const float t = (g.drag_y - g.cam_pos.y) / dir.y;
                    if (t > 0.0f) {
                        const math::Vec3 hit = math::add(g.cam_pos, math::scale(dir, t));
                        const float nx = std::round(hit.x / g.grid) * g.grid;
                        const float nz = std::round(hit.z / g.grid) * g.grid;
                        g.sel_obj = g.obj_drag;
                        set_obj_number(g.obj_drag, "x", nx);
                        set_obj_number(g.obj_drag, "z", nz);
                        g.dirty = true;
                        sync_property_fields();
                        sync_object_list();
                        update_title();
                    }
                }
            }
            return 0;
        }
        case WM_CONTEXTMENU: {
            // Etappe 19c: Rechtsklick im Viewport = Kontextmenü.
            POINT pt{ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
            if (pt.x == -1 && pt.y == -1) {
                pt.x = 0; pt.y = 0;
                ClientToScreen(h, &pt);
            }
            TrackPopupMenu(g.ctx_obj, TPM_RIGHTBUTTON, pt.x, pt.y, 0, g.main, nullptr);
            return 0;
        }
        case WM_MOUSEWHEEL: {
            // Mausrad = Zoom: Dolly entlang der Blickrichtung (Etappe 17h).
            // Shift zusätzlich = nur die Hohe ändern (alte Schnellübersicht).
            const float delta = static_cast<short>(GET_WHEEL_DELTA_WPARAM(wp)) / 120.0f;
            if (wp & MK_SHIFT) {
                g.cam_pos.y += delta * 2.0f;
            } else {
                const math::Vec3 fwd = cam_forward();
                g.cam_pos.x += fwd.x * delta * 4.0f;
                g.cam_pos.y += fwd.y * delta * 4.0f;
                g.cam_pos.z += fwd.z * delta * 4.0f;
                g.cam_pos.y = std::max(1.5f, g.cam_pos.y);   // nicht unter den Boden
            }
            return 0;
        }
        case WM_LBUTTONDOWN: {
            // Strahl aus Kamera und Pixel (Koordinaten direkt vom Ereignis).
            const float mx = static_cast<float>(GET_X_LPARAM(lp));
            const float my = static_cast<float>(GET_Y_LPARAM(lp));
            RECT rc;
            GetClientRect(h, &rc);
            const float w = static_cast<float>(std::max(1, static_cast<int>(rc.right)));
            const float hh = static_cast<float>(std::max(1, static_cast<int>(rc.bottom)));
            const float ndc_x = (2.0f * mx / w) - 1.0f;
            const float ndc_y = 1.0f - (2.0f * my / hh);
            const math::Vec3 dir = ray_dir(ndc_x, ndc_y, w / hh);

            if (g.height_mode) {
                // Etappe 18l: Berge malen - Linksklick = Hügel anheben,
                // Shift+Klick = Senke abtragen. Jeder Klick ist ein weicher
                // Pinsel (cos-Falloff) im Karten-JSON ("terrain.heights"),
                // Mehrfachklicken stapelt die Höhe.
                math::Vec3 hit{};
                if (terrain_hit(g.cam_pos, dir, hit)) {
                    const bool sink = (wp & MK_SHIFT) != 0;
                    const int n = height_edit(hit.x, hit.z, sink);
                    if (n > 0) {
                        regen_terrain();
                        g.dirty = true;
                        update_title();
                        wchar_t buf[128];
                        swprintf(buf, 128, L"%ls @ (%.1f, %.1f), %d Pinselfelder",
                                 sink ? L"Senke abgetragen" : L"Berg aufgehügelt",
                                 hit.x, hit.z, n);
                        status_mode(buf);
                        core::log_info("H\xC3\xB6hen-Pinsel: " +
                                       std::string(sink ? "-" : "+") +
                                       std::to_string(height_step()) + " m @ (" +
                                       std::to_string(hit.x) + ", " + std::to_string(hit.z) + ")");
                    } else {
                        status_mode(L"Kein Terrain hier");
                    }
                }
            } else if (g.paint_mode) {
                // Etappe 18e: Boden malen - Linksklick = Fläche mit der
                // Pinsel-Textur, Shift+Klick = Radierer an dieser Stelle.
                math::Vec3 hit{};
                if (terrain_hit(g.cam_pos, dir, hit)) {
                    const bool erase = (wp & MK_SHIFT) != 0;
                    if (splat_edit(hit.x, hit.z, g.paint_size, g.paint_size,
                                   g.paint_tex, erase, true)) {
                        regen_terrain();
                        if (g_ts.win) ts_fill_splats();
                        g.dirty = true;
                        update_title();
                        status_mode(erase ? L"Fläche entfernt" : L"Fläche gemalt");
                        core::log_info(std::string(erase ? "Malwerkzeug: Fläche entfernt @ ("
                                                         : "Malwerkzeug: Fläche gemalt @ (") +
                                       std::to_string(hit.x) + ", " + std::to_string(hit.z) + ")");
                    } else {
                        status_mode(L"Keine Fläche hier");
                    }
                }
            } else if (g.place_mode) {
                math::Vec3 hit{};
                if (terrain_hit(g.cam_pos, dir, hit)) {
                    place_object(hit.x, hit.z);   // push_undo() steckt in place_object
                    update_title();
                }
            } else {
                float t = 0.0f;
                const int idx = object_hit(g.cam_pos, dir, t);
                select_object(idx, false);
                status_mode(idx >= 0 ? L"Ausgewählt" : L"Kein Treffer");
                if (g.move_mode && idx >= 0) {
                    // Drag-Start: Undo-Schnappschuss genau einmal, dann Capture.
                    push_undo();
                    g.obj_drag = idx;
                    g.dragging = true;
                    g.drag_y = base_y_at(obj_number(idx, "x", 0), obj_number(idx, "z", 0));
                    SetCapture(h);
                    status_mode(L"Ziehen ...");
                }
            }
            return 0;
        }
        case WM_LBUTTONUP:
            if (g.dragging) {
                g.dragging = false;
                ReleaseCapture();
                if (g.obj_drag >= 0 && g.objects) {
                    core::log_info("Drag-Ende: idx=" + std::to_string(g.obj_drag) +
                                   " x=" + std::to_string(obj_number(g.obj_drag, "x", 0)) +
                                   " z=" + std::to_string(obj_number(g.obj_drag, "z", 0)));
                }
                g.obj_drag = -1;
                status_mode(L"Verschoben");
            }
            return 0;
        case WM_SETCURSOR:
            SetCursor(LoadCursorW(nullptr, IDC_ARROW));
            return TRUE;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

// ------------------------------------------------------------
//  Rendering: Objekte + Auswahl-Rahmen + Platzier-Geist
// ------------------------------------------------------------
void push_object(int i) {
    const Prefab* pre = prefab_of(obj_type(i));
    if (!pre) return;
    const float x = obj_number(i, "x", 0);
    const float z = obj_number(i, "z", 0);
    const float rot = obj_number(i, "rot", 0);
    const float by = base_y_at(x, z);
    const float cd = std::cos(rot), sd = std::sin(rot);
    // Etappe 17f: optionales Objekt-Material ("tex") überschreibt alle Teile.
    int override_tex = -1;
    if (g.objects && i >= 0 && i < static_cast<int>(g.objects->arr.size())) {
        if (const JsonValue* t = g.objects->arr[i].find("tex")) {
            if (t->is_str() && !t->str.empty()) {
                override_tex = g.renderer->texture_index(t->str);
            }
        }
    }
    for (const PrefabPart& part : pre->parts) {
        const float dx = part.d.x, dz = part.d.z;
        const math::Vec3 pos{ x + dx * cd + dz * sd,
                              by + part.d.y,
                              z - dx * sd + dz * cd };
        g.renderer->push_box(pos, part.s, math::Vec3{ 0, rot, 0 }, part.c,
                             override_tex >= 0 ? override_tex : part.tex,
                             part.tile);
    }
}

// Gelber Rahmen um das ausgewählte Objekt (12 dünne Kästen).
void push_selection_frame(int i) {
    const Prefab* pre = prefab_of(obj_type(i));
    if (!pre) return;
    const float x = obj_number(i, "x", 0);
    const float z = obj_number(i, "z", 0);
    const float rot = obj_number(i, "rot", 0);
    const float by = base_y_at(x, z);
    const float cd = std::cos(rot), sd = std::sin(rot);

    math::Vec3 mn{ 1e9f, 1e9f, 1e9f }, mx{ -1e9f, -1e9f, -1e9f };
    for (const PrefabPart& part : pre->parts) {
        const float dx = part.d.x, dz = part.d.z;
        const math::Vec3 c{ x + dx * cd + dz * sd, by + part.d.y, z - dx * sd + dz * cd };
        const float hx =  std::fabs(cd) * part.s.x * 0.5f + std::fabs(sd) * part.s.z * 0.5f;
        const float hz =  std::fabs(sd) * part.s.x * 0.5f + std::fabs(cd) * part.s.z * 0.5f;
        const float hy = part.s.y * 0.5f;
        mn.x = std::min(mn.x, c.x - hx); mx.x = std::max(mx.x, c.x + hx);
        mn.y = std::min(mn.y, c.y - hy); mx.y = std::max(mx.y, c.y + hy);
        mn.z = std::min(mn.z, c.z - hz); mx.z = std::max(mx.z, c.z + hz);
    }
    mn = math::sub(mn, math::Vec3{ 0.12f, 0.12f, 0.12f });
    mx = math::add(mx, math::Vec3{ 0.12f, 0.12f, 0.12f });
    const math::Vec3 size = math::sub(mx, mn);
    const math::Vec3 mid = math::scale(math::add(mn, mx), 0.5f);
    const uint32_t col = 0xFFC83C;
    const float th = 0.09f;

    // 4 Kanten unten, 4 oben, 4 vertikal
    const float ys[2] = { mn.y, mx.y };
    for (float y : ys) {
        g.renderer->push_box(math::Vec3{ mid.x, y, mn.z }, math::Vec3{ size.x, th, th },
                             math::Vec3{}, col);
        g.renderer->push_box(math::Vec3{ mid.x, y, mx.z }, math::Vec3{ size.x, th, th },
                             math::Vec3{}, col);
        g.renderer->push_box(math::Vec3{ mn.x, y, mid.z }, math::Vec3{ th, th, size.z },
                             math::Vec3{}, col);
        g.renderer->push_box(math::Vec3{ mx.x, y, mid.z }, math::Vec3{ th, th, size.z },
                             math::Vec3{}, col);
    }
    for (float xv : { mn.x, mx.x }) {
        for (float zv : { mn.z, mx.z }) {
            g.renderer->push_box(math::Vec3{ xv, mid.y, zv }, math::Vec3{ th, size.y, th },
                                 math::Vec3{}, col);
        }
    }
}

// Etappe 19c: Platzier-Vorschau ("Geist"). Der Renderer kennt kein Alpha,
// deshalb wird der Geist eingefärbt (Mischung Richtung Mintgrün) und ohne
// Textur gezeichnet - gleiche Masse, gleiche Drehung wie das gesetzte
// Objekt, aber klar als Vorschau erkennbar. Eingeraast auf dasselbe Raster,
// das place_object() benutzt: Was man sieht, setzt der Klick.
uint32_t mix_rgb(uint32_t a, uint32_t b, float t) {
    auto ch = [&](int shift) {
        const float av = static_cast<float>((a >> shift) & 0xFF);
        const float bv = static_cast<float>((b >> shift) & 0xFF);
        return static_cast<uint32_t>(av * (1.0f - t) + bv * t);
    };
    return (ch(16) << 16) | (ch(8) << 8) | ch(0);
}

void push_ghost_object() {
    if (!g.place_mode || !g.hover_valid) return;
    if (g.sel_prefab < 0 || g.sel_prefab >= static_cast<int>(g.prefabs.size())) return;
    const Prefab& pre = g.prefabs[static_cast<size_t>(g.sel_prefab)];
    const float gx = std::round(g.hover_pos.x / g.grid) * g.grid;
    const float gz = std::round(g.hover_pos.z / g.grid) * g.grid;
    const float rot = g.place_rot;
    const float by = base_y_at(gx, gz);
    const float cd = std::cos(rot), sd = std::sin(rot);
    for (const PrefabPart& part : pre.parts) {
        const float dx = part.d.x, dz = part.d.z;
        const math::Vec3 pos{ gx + dx * cd + dz * sd, by + part.d.y,
                              gz - dx * sd + dz * cd };
        g.renderer->push_box(pos, part.s, math::Vec3{ 0, rot, 0 },
                             mix_rgb(part.c, 0x63F58A, 0.55f));
    }
}

// Gelber Platzier-Marker am Cursor (Raster-gross). Im Mal-Modus (Etappe 18e)
// stattdessen ein flaches Quadrat in Pinselgrösse als Brush-Vorschau.
void push_hover_marker() {
    if (!g.hover_valid) return;
    if (g.paint_mode) {
        const float s = g.paint_size * 0.98f;
        g.renderer->push_box(math::Vec3{ g.hover_pos.x, g.hover_pos.y + 0.05f, g.hover_pos.z },
                             math::Vec3{ s, 0.04f, s }, math::Vec3{}, 0x66CCFF);
        return;
    }
    // Etappe 18l: Berge-Maler - orange Markierung in Pinselgrösse am Cursor.
    if (g.height_mode) {
        const float s = g.paint_size * 0.98f;
        g.renderer->push_box(math::Vec3{ g.hover_pos.x, g.hover_pos.y + 0.05f, g.hover_pos.z },
                             math::Vec3{ s, 0.04f, s }, math::Vec3{}, 0xFF8830);
        return;
    }
    if (!g.place_mode) return;
    const float s = g.grid * 0.9f;
    g.renderer->push_box(math::Vec3{ g.hover_pos.x, g.hover_pos.y + 0.06f, g.hover_pos.z },
                         math::Vec3{ s, 0.06f, s }, math::Vec3{}, 0xFFE27A);
}

// Etappe 19k.2: Block-Zonen der Karte als rote Flach-Quader sichtbar machen
// (Daten: „blocked" im Karten-JSON, x/z = Mitte, w/h = Größe in Metern).
void push_blocked_zones() {
    const JsonValue* bl = g.map_root.find("blocked");
    if (!bl || !bl->is_arr()) return;
    for (const JsonValue& b : bl->arr) {
        if (!b.is_obj()) continue;
        auto num = [&b](const char* k) {
            const JsonValue* v = b.find(k);
            return v ? static_cast<float>(v->number(0.0)) : 0.0f;
        };
        const float cx = num("x"), cz = num("z"), w = num("w"), h = num("h");
        if (w <= 0.0f || h <= 0.0f) continue;
        const float y = base_y_at(cx, cz);
        g.renderer->push_box(math::Vec3{ cx, y + 0.07f, cz },
                             math::Vec3{ w, 0.14f, h }, math::Vec3{}, 0xFF5A5A);
    }
}

void render_frame() {
    RECT rc;
    GetClientRect(g.view, &rc);
    const int w = std::max(1, static_cast<int>(rc.right));
    const int h = std::max(1, static_cast<int>(rc.bottom));
    const float aspect = static_cast<float>(w) / static_cast<float>(h);

    const math::Mat4 proj = math::perspective_fov_lh(math::PI / 3.0f, aspect, 0.1f, 800.0f);
    const math::Mat4 view = math::look_to_lh(g.cam_pos, cam_forward(), math::Vec3{ 0, 1, 0 });
    g.renderer->begin_frame(math::mul(view, proj), g.cam_pos);

    if (g.objects) {
        for (int i = 0; i < static_cast<int>(g.objects->arr.size()); ++i) push_object(i);
    }
    if (g.sel_obj >= 0) push_selection_frame(g.sel_obj);
    push_ghost_object();   // Etappe 19c: Vorschau vor dem Marker
    push_hover_marker();
    push_blocked_zones();  // Etappe 19k.2: Passierbarkeit sichtbar machen

    g.renderer->end_frame();
}

// ------------------------------------------------------------
//  Hauptschleife
// ------------------------------------------------------------
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

void update_camera(float dt) {
    const bool fast = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
    const float speed = fast ? 30.0f : 10.0f;
    const math::Vec3 fwd{ std::sin(g.cam_yaw), 0.0f, std::cos(g.cam_yaw) };
    const math::Vec3 right{ std::cos(g.cam_yaw), 0.0f, -std::sin(g.cam_yaw) };
    math::Vec3 move{};
    if (GetAsyncKeyState('W') & 0x8000) move = math::add(move, fwd);
    if (GetAsyncKeyState('S') & 0x8000) move = math::sub(move, fwd);
    if (GetAsyncKeyState('A') & 0x8000) move = math::sub(move, right);
    if (GetAsyncKeyState('D') & 0x8000) move = math::add(move, right);
    if (GetAsyncKeyState('E') & 0x8000) move.y += 1.0f;
    if (GetAsyncKeyState('Q') & 0x8000) move.y -= 1.0f;
    if (move.x != 0.0f || move.y != 0.0f || move.z != 0.0f) {
        move = math::scale(math::normalize(move), speed * dt);
        g.cam_pos = math::add(g.cam_pos, move);
    }
    // Nie unter dem Gelände stecken.
    const float ground = g.terrain->ready() ? g.terrain->height_at(g.cam_pos.x, g.cam_pos.z) : 0.0f;
    g.cam_pos.y = std::max(g.cam_pos.y, ground + 1.5f);
}

void update_hover() {
    RECT rc;
    GetClientRect(g.view, &rc);
    const float w = static_cast<float>(std::max(1, static_cast<int>(rc.right)));
    const float h = static_cast<float>(std::max(1, static_cast<int>(rc.bottom)));
    const float ndc_x = (2.0f * g.mouse_x / w) - 1.0f;
    const float ndc_y = 1.0f - (2.0f * g.mouse_y / h);
    const math::Vec3 dir = ray_dir(ndc_x, ndc_y, w / h);
    math::Vec3 hit{};
    g.hover_valid = terrain_hit(g.cam_pos, dir, hit);
    if (g.hover_valid) g.hover_pos = hit;
}

void handle_shortcuts() {
    if (g.props_dlg) return;   // kein Editieren hinter einem offenen Dialog
    if (event_editor_focused()) return;   // Tippen im Event-Editor soll nichts auslösen
    if (script_editor_focused()) return;  // dito für den Script-Editor
    if (material_editor_focused()) return;
    if (database_editor_focused()) return;
    if (soundtest_focused()) return;
    // Etappe 18k: Umbenennen im Kartenbaum - Edit-Control im TreeView aktiv?
    if (g.map_tree && TreeView_GetEditControl(g.map_tree)) return;
    // F12: Screenshot für Tests/Doku (einmal pro Tastendruck)
    {
        static bool f12_was = false;
        const bool f12_now = (GetAsyncKeyState(VK_F12) & 0x8000) != 0;
        if (f12_now && !f12_was) {
            static int shot_no = 0;
            ++shot_no;
            g.renderer->request_screenshot("log\\editor_shot_" + std::to_string(shot_no) + ".bmp");
            core::log_info("Screenshot angefordert (#" + std::to_string(shot_no) + ")");
        }
        f12_was = f12_now;
    }
    // Strg+S: Karte speichern
    if ((GetAsyncKeyState(VK_CONTROL) & 0x8000) && (GetAsyncKeyState('S') & 0x8000)) {
        static double last_save = 0.0;
        const double now = qpc_now();
        if (now - last_save > 0.5) {
            last_save = now;
            if (save_map()) {
                status_mode(L"Gespeichert");
                update_title();
            }
        }
    }
    // Strg+Z / Strg+Y: Rückgängig und Wiederholen
    {
        const bool ctrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
        static bool z_was = false, y_was = false;
        const bool z_now = ctrl && (GetAsyncKeyState('Z') & 0x8000) != 0;
        const bool y_now = ctrl && (GetAsyncKeyState('Y') & 0x8000) != 0;
        if (z_now && !z_was) undo_action();
        if (y_now && !y_was) redo_action();
        z_was = z_now;
        y_was = y_now;
    }

    // Etappe 18e: Pinselgrösse im Mal-Modus mit +/- (auch Zehnertastatur).
    // Etappe 18l: auch im Höhen-Modus (Pinsel ist dort der Berg-Radius).
    if (g.paint_mode || g.height_mode) {
        static bool plus_was = false, minus_was = false;
        const bool plus_now  = (GetAsyncKeyState(VK_ADD) & 0x8000) ||
                               (GetAsyncKeyState(VK_OEM_PLUS) & 0x8000);
        const bool minus_now = (GetAsyncKeyState(VK_SUBTRACT) & 0x8000) ||
                               (GetAsyncKeyState(VK_OEM_MINUS) & 0x8000);
        if (plus_now && !plus_was) {
            g.paint_size = std::min(64.0f, g.paint_size + std::max(1.0f, g.grid));
            status_mode((g.height_mode
                             ? L"Höhe malen: Pinsel " +
                                   std::to_wstring(static_cast<int>(g.paint_size)) +
                                   L" m | Shift = Senke"
                             : L"Boden malen: " + utf8_to_wide(g.paint_tex) +
                                   L" | Pinsel " +
                                   std::to_wstring(static_cast<int>(g.paint_size)) +
                                   L" m | Shift = Radierer")
                            .c_str());
        }
        if (minus_now && !minus_was) {
            g.paint_size = std::max(g.grid, g.paint_size - std::max(1.0f, g.grid));
            status_mode((g.height_mode
                             ? L"Höhe malen: Pinsel " +
                                   std::to_wstring(static_cast<int>(g.paint_size)) +
                                   L" m | Shift = Senke"
                             : L"Boden malen: " + utf8_to_wide(g.paint_tex) +
                                   L" | Pinsel " +
                                   std::to_wstring(static_cast<int>(g.paint_size)) +
                                   L" m | Shift = Radierer")
                            .c_str());
        }
        plus_was = (GetAsyncKeyState(VK_ADD) & 0x8000) != 0 ||
                   (GetAsyncKeyState(VK_OEM_PLUS) & 0x8000) != 0;
        minus_was = (GetAsyncKeyState(VK_SUBTRACT) & 0x8000) != 0 ||
                    (GetAsyncKeyState(VK_OEM_MINUS) & 0x8000) != 0;
    }

    // Etappe 17h: F-Tasten für die Werkzeugleiste (wie im Menü vermerkt).
    {
        static bool f1w = false, f2w = false, f3w = false, f4w = false;
        static bool f6w = false, f8w = false, f9w = false, f11w = false;
        struct { int vk; bool* was; } fkeys[] = {
            { VK_F1,  &f1w },  { VK_F2,  &f2w },  { VK_F3,  &f3w },
            { VK_F4,  &f4w },  { VK_F6,  &f6w },  { VK_F8,  &f8w },
            { VK_F9,  &f9w },  { VK_F11, &f11w },
        };
        for (auto& k : fkeys) {
            const bool now = (GetAsyncKeyState(k.vk) & 0x8000) != 0;
            if (now && !*k.was) {
                switch (k.vk) {
                    case VK_F1:  PostMessageW(g.main, WM_COMMAND, ID_MENU_HELP, 0); break;
                    case VK_F2:
                        if (g.cur_map >= 0) PostMessageW(g.main, WM_COMMAND, ID_MENU_MAPPROPS, 0);
                        break;
                    case VK_F3:  PostMessageW(g.main, WM_COMMAND, ID_MENU_MATERIALS, 0); break;
                    case VK_F4:  PostMessageW(g.main, WM_COMMAND, ID_MENU_PLAYTEST, 0); break;
                    case VK_F6:  PostMessageW(g.main, WM_COMMAND, ID_MENU_EVENTS, 0); break;
                    case VK_F7:  PostMessageW(g.main, WM_COMMAND, ID_MENU_TILESET, 0); break;
                    case VK_F8:  PostMessageW(g.main, WM_COMMAND, ID_MENU_SOUNDTEST, 0); break;
                    case VK_F9:  PostMessageW(g.main, WM_COMMAND, ID_MENU_DATABASE, 0); break;
                    case VK_F10: PostMessageW(g.main, WM_COMMAND, ID_MENU_PROBLEMS, 0); break;
                    case VK_F11: PostMessageW(g.main, WM_COMMAND, ID_MENU_SCRIPTS, 0); break;
                }
            }
            *k.was = now;
        }
    }
    // Etappe 19c: R dreht im Setzen-Modus (ohne Auswahl) die Vorschau in
    // 15-Grad-Schritten, sonst wie bisher das ausgewählte Objekt.
    {
        static bool r_was = false;
        const bool r_now = (GetAsyncKeyState('R') & 0x8000) != 0;
        if (r_now && !r_was) {
            if (g.place_mode && g.sel_obj < 0) {
                g.place_rot += math::PI / 12.0f;
                if (g.place_rot >= 2.0f * math::PI) g.place_rot -= 2.0f * math::PI;
                status_mode((L"Vorschau-Drehung " +
                             std::to_wstring(static_cast<int>(
                                 g.place_rot * 180.0f / 3.14159265f)) +
                             L"° | R = weiter drehen | Klick = setzen").c_str());
            } else if (g.sel_obj >= 0) {
                rotate_selected(math::PI / 6.0f);
            }
        }
        r_was = r_now;
    }
    if (g.sel_obj >= 0) {
        static bool del_was = false;
        const bool del_now = (GetAsyncKeyState(VK_DELETE) & 0x8000) != 0;
        if (del_now && !del_was) delete_selected();
        del_was = del_now;
    }

    // Etappe 18a: Esc = Auswahl aufheben, Enter = Eigenschaften übernehmen.
    {
        static bool esc_was = false, ent_was = false;
        const bool esc_now = (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;
        const bool ent_now = (GetAsyncKeyState(VK_RETURN) & 0x8000) != 0;
        if (esc_now && !esc_was && g.sel_obj >= 0) {
            select_object(-1, false);
            status_mode(L"Auswahl aufgehoben");
            core::log_info("Auswahl aufgehoben (Esc)");
        }
        if (ent_now && !ent_was && g.sel_obj >= 0) {
            apply_property_fields();
            status_mode(L"Eigenschaften übernommen");
            core::log_info("Eigenschaften übernommen (Enter)");
        }
        esc_was = esc_now;
        ent_was = ent_now;
    }

    // Etappe 18a: G schaltet das Raster um (1.0 / 0.5 / 0.25 / aus 0.01).
    {
        static bool g_was = false;
        const bool g_now = (GetAsyncKeyState('G') & 0x8000) != 0;
        if (g_now && !g_was) {
            const float steps[4] = { 1.0f, 0.5f, 0.25f, 0.01f };
            int idx = 3;
            for (int i = 0; i < 4; ++i) {
                if (std::abs(g.grid - steps[i]) < 1e-4f) { idx = i; break; }
            }
            g.grid = steps[(idx + 1) % 4];
            wchar_t buf[64];
            std::swprintf(buf, 64, L"Raster: %.2f m", g.grid);
            status_mode(buf);
            core::log_info("Raster umgeschaltet: " +
                           std::to_string(static_cast<int>(g.grid * 100)) + " cm");
        }
        g_was = g_now;
    }
}

// ------------------------------------------------------------
//  Hauptfenster
// ------------------------------------------------------------
LRESULT CALLBACK main_proc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_GETMINMAXINFO: {
            // Etappe 18a: Mindestgrösse, damit das Layout nicht bricht.
            MINMAXINFO* mmi = reinterpret_cast<MINMAXINFO*>(lp);
            mmi->ptMinTrackSize.x = 1024;
            mmi->ptMinTrackSize.y = 640;
            return 0;
        }
        case WM_SIZE:
            if (wp != SIZE_MINIMIZED) {
                layout_children();
                // Etappe 18j: sofort einen frischen Frame zeichnen. Sonst zeigt
                // DWM den alten (falsch skalierten) Frame oder undefinierten
                // Backbuffer-Inhalt, bis die Hauptschleife wieder läuft.
                static bool in_size = false;
                if (!in_size) {
                    in_size = true;
                    render_frame();
                    in_size = false;
                }
            }
            return 0;

        // Etappe 18j: Beim Ziehen der Fensterkante/Vollbild-Umschalten läuft
        // die Modalschleife von Windows - unsere Render-Schleife pausiert dann.
        // Neue Flächen blieben schwarz oder zeigten das Fenster dahinter
        // (im Test: das Spielfenster). Ein 16-ms-Timer rendert weiter.
        case WM_ENTERSIZEMOVE:
            SetTimer(h, IDT_RESIZE, 16, nullptr);
            g.resize_ticks = 0;
            return 0;

        case WM_EXITSIZEMOVE:
            KillTimer(h, IDT_RESIZE);
            layout_children();
            render_frame();
            // Beleg für Live-Tests: hat der Timer während der Windows-
            // Modalschleife tatsächlich Bilder geliefert?
            core::log_info("Resize-Timer: " + std::to_string(g.resize_ticks) +
                           " Frames während des Ziehens gerendert");
            return 0;

        case WM_TIMER:
            if (wp == IDT_RESIZE && !g.props_dlg) {
                // Das Layout wird bereits durch WM_SIZE aktualisiert. Hier nur
                // zeichnen, damit die Swapchain nicht bei jedem Tick neu
                // erzeugt wird, obwohl sich ihre Grösse gar nicht geändert hat.
                render_frame();
                ++g.resize_ticks;
                return 0;
            }
            break;

        case WM_DROPFILES: {
            // Etappe 18a: PNG/BMP ins Fenster ziehen = Textur importieren.
            HDROP drop = reinterpret_cast<HDROP>(wp);
            wchar_t wpath[MAX_PATH]{};
            const UINT n = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
            int imports = 0;
            for (UINT i = 0; i < n; ++i) {
                if (DragQueryFileW(drop, i, wpath, MAX_PATH) == 0) continue;
                std::wstring wv(wpath);
                const std::string path(wv.begin(), wv.end());
                std::string lower = path;
                for (char& c : lower) c = static_cast<char>(std::tolower(c));
                const bool ok_ext = lower.size() >= 4 &&
                    (lower.compare(lower.size() - 4, 4, ".png") == 0 ||
                     lower.compare(lower.size() - 4, 4, ".bmp") == 0);
                if (!ok_ext) {
                    core::log_warn("Drag&Drop übersprungen (kein PNG/BMP): " + path);
                    continue;
                }
                std::string name;
                if (mat_import_file(path, name)) {
                    ++imports;
                    status_mode((L"Importiert: " + utf8_to_wide(name)).c_str());
                }
            }
            DragFinish(drop);
            if (imports > 0) core::log_info("Drag&Drop: " +
                                  std::to_string(imports) + " Textur(en) importiert");
            return 0;
        }

        case WM_SETFOCUS:
            SetFocus(g.view);
            return 0;

        case WM_COMMAND: {
            if (g.props_dlg) return 0;   // kein Menü-Kommando hinter Dialog
            const int id = LOWORD(wp);
            const int code = HIWORD(wp);
            switch (id) {
                case ID_MENU_SAVE:
                    if (save_map()) {
                        status_mode(L"Gespeichert");
                        update_title();
                    }
                    return 0;
                case ID_MENU_EXIT:
                    PostMessageW(h, WM_CLOSE, 0, 0);
                    return 0;
                case ID_MENU_RESETCAM:
                    if (g.cur_map >= 0) {
                        const JsonValue* t = g.map_root.find("terrain");
                        auto tnum = [&](const char* key, double fallback) {
                            if (!t) return fallback;
                            const JsonValue* v = t->find(key);
                            return v ? v->number(fallback) : fallback;
                        };
                        const double size_fallback = tnum("size", 240);
                        const float span = static_cast<float>(std::max(tnum("width", size_fallback),
                                                                       tnum("height", size_fallback)));
                        g.cam_pos = math::Vec3{ 0.0f, span * 0.10f + 8.0f, span * 0.18f };
                        g.cam_yaw = math::PI;
                        g.cam_pitch = -0.5f;
                    }
                    return 0;
                case ID_MENU_MAPPROPS:
                    open_map_props();
                    return 0;
                case ID_MENU_MAPNEW:
                    open_map_new();
                    return 0;
                case ID_MENU_MAPDEL:
                    delete_current_map();
                    return 0;
                case ID_MENU_EVENTS:
                    open_event_editor();
                    return 0;
                case ID_MENU_SCRIPTS:
                    open_script_editor();
                    return 0;
                case ID_MENU_IMPORT_TEX:
                    open_import_texture();
                    return 0;
                case ID_MENU_MATERIALS:
                    open_material_dialog();
                    return 0;
                case ID_MENU_DATABASE:
                    open_database_dialog();
                    return 0;
                case ID_MENU_SOUNDTEST:
                    open_soundtest_dialog();
                    return 0;
                case ID_MENU_PLAYTEST:
                    start_playtest();
                    return 0;
                case ID_MENU_PROJECT:
                    open_project_dialog();
                    return 0;
                case ID_MENU_TILESET:
                    open_tileset_dialog();
                    return 0;
                case ID_MENU_PROBLEMS:
                    open_problems_dialog();
                    return 0;
                case ID_MENU_HELP:
                    MessageBoxW(h,
                                L"Maus (rechts halten): Umsehen\n"
                                L"W/A/S/D: Bewegen    Q/E: Runter/Hoch\n"
                                L"Mausrad: Zoom (Dolly)    Shift+Rad: Höhe    Shift: schnell\n\n"
                                L"Linksklick: Setzen (im Setzen-Modus) / Auswählen\n"
                                L"Verschieben-Modus: Objekt mit Linksklick greifen und ziehen\n"
                                L"Boden malen: Linksklick = Fläche, Shift+Klick = Radierer\n"
                                L"Höhe malen: Linksklick = Berg, Shift+Klick = Senke,\n"
                                L"             mehrfach klicken stapelt, +/- = Pinselgröße\n"
                                L"Doppelklick in Objektliste: Kamera aufs Objekt richten\n"
                                L"R: Auswahl drehen (+30 Grad)    Entf: löschen\n"
                                L"Strg+Z: Rückgängig    Strg+Y: Wiederholen\n"
                                L"Strg+S: Karte speichern\n"
                                L"G: Raster umschalten    Esc: Auswahl aufheben\n"
                                L"Enter: Eigenschaften übernehmen\n"
                                L"PNG/BMP einfach ins Fenster ziehen = Textur importieren\n\n"
                                L"Werkzeugleiste (F-Tasten):\n"
                                L"  F2 Karten-Eigenschaften (u. a. Wetter)\n"
                                L"  F3 Materialbase    F4 Playtest starten\n"
                                L"  F6 Event-Editor    F8 Sound-Test\n"
                                L"  F9 Datenbank (Helden/Items/Gegner/Truppen)\n"
                                L"  F11 Script-Editor\n\n"
                                L"Menü Karte: neue Karten anlegen, zwischen ihnen wechseln\n"
                                L"Playtest: startet Rune3D.exe mit diesem Projekt\n"
                                L"(--project, ohne Titelbildschirm).\n\n"
                                L"Der Editor arbeitet nur auf den Kartendaten - "
                                L"die Game-Engine bleibt unberührt.",
                                L"RuneEditor - Bedienung", MB_OK | MB_ICONINFORMATION);
                    return 0;
                case IDC_PREFAB_LIST:
                    if (code == LBN_SELCHANGE) {
                        g.sel_prefab = static_cast<int>(SendMessageW(g.list_prefab, LB_GETCURSEL, 0, 0));
                        set_mode(1);
                        status_mode((L"Setzen: " + utf8_to_wide(
                            g.sel_prefab < static_cast<int>(g.prefabs.size())
                                ? g.prefabs[g.sel_prefab].name : std::string("?"))).c_str());
                    }
                    return 0;
                case ID_CTX_PLACE:
                    if (g.hover_valid) {
                        set_mode(1);
                        place_object(g.hover_pos.x, g.hover_pos.z);
                        update_title();
                    }
                    return 0;
                case ID_CTX_FOCUS:
                    if (g.sel_obj >= 0 && g.objects &&
                        g.sel_obj < static_cast<int>(g.objects->arr.size())) {
                        const float ox = obj_number(g.sel_obj, "x", 0);
                        const float oz = obj_number(g.sel_obj, "z", 0);
                        g.cam_pos = math::Vec3{ ox, base_y_at(ox, oz) + 12.0f, oz + 18.0f };
                        g.cam_yaw = math::PI;
                        g.cam_pitch = -0.45f;
                        status_mode(L"Kamera auf Objekt gerichtet");
                    }
                    return 0;
                case ID_CTX_DUP:
                    duplicate_selected();
                    sync_object_list();
                    update_title();
                    return 0;
                case ID_CTX_DEL:
                    delete_selected();
                    sync_object_list();
                    update_title();
                    return 0;
                case IDC_BTN_PLACE:
                    set_mode(1);
                    status_mode(L"Setzen: Vorschau folgt dem Cursor, Klick setzt | R = drehen");
                    return 0;
                case IDC_BTN_SELECT:
                    set_mode(2);
                    status_mode(L"Auswahl-Modus");
                    return 0;
                case IDC_BTN_MOVE:
                    set_mode(3);
                    status_mode(L"Verschieben: Objekt ziehen (Linksklick halten)");
                    return 0;
                case IDC_BTN_PAINT: {
                    // Etappe 18e: Boden-Malwerkzeug. Pinseltextur = aktuelle
                    // Tileset-Auswahl (oder Default weg), Grösse per +/- im
                    // Mal-Modus, Shift+Klick radiert.
                    set_mode(4);
                    if (g_ts.win && !g_ts.sel.empty()) g.paint_tex = g_ts.sel;
                    status_mode((L"Boden malen: " + utf8_to_wide(g.paint_tex) +
                                 L" | +/- Grösse | Shift = Radierer").c_str());
                    core::log_info("Malwerkzeug aktiviert (Textur '" + g.paint_tex +
                                   "', Grösse " + std::to_string(g.paint_size) + " m)");
                    return 0;
                }
                case IDC_BTN_HEIGHT: {
                    // Etappe 18l: Höhen-Malwerkzeug. Berge/Senken direkt an
                    // der angeklickten Stelle, Default-Karten sind flach
                    // (Amplitude 0) - Erhöhen/Lesen ohne Seed-Fummelei.
                    set_mode(5);
                    status_mode((L"Höhe malen: +" + std::to_wstring(static_cast<int>(height_step())) +
                                 L" m pro Klick | +/- Grösse | Shift = Senke").c_str());
                    core::log_info("H\xC3\xB6hen-Malwerkzeug aktiviert (Schritt " +
                                   std::to_string(height_step()) + " m, Grösse " +
                                   std::to_string(g.paint_size) + " m)");
                    return 0;
                }
                case IDC_BTN_UNDO:
                    undo_action();
                    return 0;
                case IDC_BTN_REDO:
                    redo_action();
                    return 0;
                case IDC_BTN_ROTATE:
                    rotate_selected(math::PI / 6.0f);
                    return 0;
                case IDC_BTN_DELETE:
                    delete_selected();
                    update_title();
                    return 0;
                case IDC_BTN_DUPLICATE:
                    duplicate_selected();
                    update_title();
                    return 0;
                case IDC_BTN_APPLY:
                    apply_property_fields();
                    update_title();
                    return 0;
                default:
                    if (id >= ID_MENU_MAP_BASE &&
                        id < ID_MENU_MAP_BASE + static_cast<int>(g.map_files.size())) {
                        if (load_map(id - ID_MENU_MAP_BASE)) {
                            sync_object_list();
                            sync_property_fields();
                            update_title();
                            status_mode(L"Karte geladen");
                        }
                    }
                    return 0;
            }
        }

        case WM_CLOSE:
            if (g.dirty) {
                const int r = MessageBoxW(h, L"Ungespeicherte Änderungen verwerfen?",
                                          L"RuneEditor", MB_YESNO | MB_ICONQUESTION);
                if (r != IDYES) return 0;
            }
            DestroyWindow(h);
            return 0;

        // Etappe 18k: Kartenbaum - Auswahl laden, Doppelklick = Eigenschaften,
        // Umbenennen per Inplace-Edit (TVS_EDITLABELS).
        case WM_NOTIFY: {
            const NMHDR* nm = reinterpret_cast<const NMHDR*>(lp);
            // Etappe 19c: Objektliste (ListView) - Auswahl, Sortierung,
            // Doppelklick (Kamera) und Rechtsklick (Kontextmenü).
            if (nm->idFrom == IDC_OBJ_LIST && nm->hwndFrom) {
                if (nm->code == LVN_ITEMCHANGED) {
                    const NMLISTVIEW* lv = reinterpret_cast<const NMLISTVIEW*>(lp);
                    if ((lv->uNewState & LVIS_SELECTED) &&
                        !(lv->uOldState & LVIS_SELECTED) && lv->iItem >= 0) {
                        LVITEMW it{};
                        it.mask = LVIF_PARAM;
                        it.iItem = lv->iItem;
                        if (ListView_GetItem(g.list_objs, &it)) {
                            select_object(static_cast<int>(it.lParam), true);
                            status_mode(L"Ausgewählt (Liste)");
                        }
                    }
                    return 0;
                }
                if (nm->code == LVN_COLUMNCLICK) {
                    const NMLISTVIEW* lv = reinterpret_cast<const NMLISTVIEW*>(lp);
                    if (g.obj_sort_col == lv->iSubItem) g.obj_sort_asc = !g.obj_sort_asc;
                    else { g.obj_sort_col = lv->iSubItem; g.obj_sort_asc = true; }
                    sync_object_list();
                    status_mode(g.obj_sort_asc ? L"Objektliste aufsteigend sortiert"
                                               : L"Objektliste absteigend sortiert");
                    return 0;
                }
                if (nm->code == NM_DBLCLK) {
                    // Doppelklick = Kamera auf das Objekt richten (wie 18a).
                    const int i = g.sel_obj;
                    if (i >= 0 && g.objects &&
                        i < static_cast<int>(g.objects->arr.size())) {
                        const float ox = obj_number(i, "x", 0);
                        const float oz = obj_number(i, "z", 0);
                        g.cam_pos = math::Vec3{ ox, base_y_at(ox, oz) + 12.0f, oz + 18.0f };
                        g.cam_yaw = math::PI;
                        g.cam_pitch = -0.45f;
                        status_mode(L"Kamera auf Objekt gerichtet");
                    }
                    return 0;
                }
                if (nm->code == NM_RCLICK) {
                    POINT pt{};
                    GetCursorPos(&pt);
                    TrackPopupMenu(g.ctx_obj, TPM_RIGHTBUTTON, pt.x, pt.y, 0, g.main, nullptr);
                    return 0;
                }
                return 0;
            }
            if (nm->idFrom != IDC_MAPTREE || !nm->hwndFrom) break;
            switch (nm->code) {
                case TVN_SELCHANGED: {
                    const NMTREEVIEWW* tv = reinterpret_cast<const NMTREEVIEWW*>(lp);
                    if (tv->action == TVC_BYMOUSE || tv->action == TVC_BYKEYBOARD) {
                        TVITEMEXW it{};
                        it.hItem = tv->itemNew.hItem;
                        it.mask = TVIF_PARAM | TVIF_CHILDREN;
                        if (TreeView_GetItem(g.map_tree, &it) && !it.cChildren &&
                            it.lParam >= 0 &&
                            static_cast<int>(it.lParam) < static_cast<int>(g.map_files.size()) &&
                            static_cast<int>(it.lParam) != g.cur_map) {
                            // Unspeichere-Karte vorher sichern (wie Kartenwechsel
                            // im Menü 17b: change_map sichert schmutzig).
                            if (g.dirty) save_map();
                            if (load_map(static_cast<int>(it.lParam))) {
                                sync_object_list();
                                sync_property_fields();
                                update_title();
                                status_mode(L"Karte geladen");
                            }
                        }
                    }
                    return 0;
                }
                case NM_DBLCLK:
                    // Doppelklick auf eine Karte = Eigenschaften öffnen (F2).
                    if (maptree_selected() >= 0) {
                        PostMessageW(h, WM_COMMAND, ID_MENU_MAPPROPS, 0);
                    }
                    return 0;
                case TVN_BEGINLABELEDIT: {
                    // Nur Karten (nicht die Wurzel) sind umbenennbar.
                    const NMTVDISPINFOW* di =
                        reinterpret_cast<const NMTVDISPINFOW*>(lp);
                    TVITEMEXW it{};
                    it.hItem = di->item.hItem;
                    it.mask = TVIF_CHILDREN;
                    TreeView_GetItem(g.map_tree, &it);
                    if (it.cChildren) return TRUE;   // Wurzel: kein Edit
                    return FALSE;
                }
                case TVN_ENDLABELEDIT: {
                    const NMTVDISPINFOW* di =
                        reinterpret_cast<const NMTVDISPINFOW*>(lp);
                    if (di->item.pszText && di->item.pszText[0]) {
                        TVITEMEXW it{};
                        it.hItem = di->item.hItem;
                        it.mask = TVIF_PARAM;
                        TreeView_GetItem(g.map_tree, &it);
                        maptree_apply_rename(static_cast<int>(it.lParam),
                                             wide_to_utf8(di->item.pszText));
                        return TRUE;   // Text übernehmen
                    }
                    return FALSE;      // leer: Edit verwerfen
                }
                default: break;
            }
            break;
        }

        // Etappe 18k: Rechtsklick auf eine Karte = Kontextmenü.
        case WM_CONTEXTMENU: {
            if (reinterpret_cast<HWND>(wp) != g.map_tree) break;
            const int idx = maptree_selected();
            POINT pt{ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
            HMENU popup = CreatePopupMenu();
            AppendMenuW(popup, MF_STRING | (idx >= 0 ? MF_ENABLED : MF_GRAYED),
                        IDM_TREE_PROPS, L"Eigenschaften...\tF2");
            AppendMenuW(popup, MF_STRING | (idx >= 0 ? MF_ENABLED : MF_GRAYED),
                        IDM_TREE_RENAME, L"Umbenennen");
            AppendMenuW(popup, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(popup, MF_STRING, IDM_TREE_NEW, L"Neue Karte...");
            AppendMenuW(popup, MF_STRING | (idx >= 0 ? MF_ENABLED : MF_GRAYED),
                        IDM_TREE_DELETE, L"Löschen");
            const int cmd = TrackPopupMenu(popup, TPM_RIGHTBUTTON | TPM_RETURNCMD,
                                           pt.x, pt.y, 0, h, nullptr);
            DestroyMenu(popup);
            switch (cmd) {
                case IDM_TREE_PROPS:
                    if (idx >= 0) PostMessageW(h, WM_COMMAND, ID_MENU_MAPPROPS, 0);
                    break;
                case IDM_TREE_RENAME:
                    if (idx >= 0) {
                        // Karte laden, falls noch nicht aktuell (Rename wirkt
                        // sonst auf die falsche Karte).
                        if (idx != g.cur_map) {
                            if (g.dirty) save_map();
                            load_map(idx);
                        }
                        HTREEITEM sel = TreeView_GetSelection(g.map_tree);
                        if (sel) TreeView_EditLabel(g.map_tree, sel);
                    }
                    break;
                case IDM_TREE_NEW:   PostMessageW(h, WM_COMMAND, ID_MENU_MAPNEW, 0); break;
                case IDM_TREE_DELETE:
                    if (idx >= 0) {
                        if (idx != g.cur_map) {
                            if (g.dirty) save_map();
                            load_map(idx);
                        }
                        PostMessageW(h, WM_COMMAND, ID_MENU_MAPDEL, 0);
                    }
                    break;
                default: break;
            }
            return 0;
        }

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

// ------------------------------------------------------------
//  Start: Fenster, Texturen-Boot (Ruby), Karte, Schleife
// ------------------------------------------------------------
// ------------------------------------------------------------
//  Karten-Verwaltung (Etappe 17b): Eigenschaften-Dialog, Neue Karte,
//  Karte löschen. Alle Änderungen landen direkt im Karten-JSON.
// ------------------------------------------------------------
enum {
    IDLG_OK          = 4100,
    IDLG_CANCEL      = 4101,
    IDLG_NAME        = 4102,
    IDLG_WIDTH       = 4103,
    IDLG_HEIGHT      = 4104,
    IDLG_BERGE       = 4105,
    IDLG_SEED        = 4106,
    IDLG_AMP         = 4107,
    IDLG_FOG         = 4108,
    IDLG_FOGFAR      = 4109,
    IDLG_FOGCOL      = 4110,
    IDLG_PANCOL      = 4111,
    IDLG_NOTE        = 4112,
    IDLG_WTYPE       = 4113,   // Etappe 17h: Wetter-Typ (Combobox)
    IDLG_WINT        = 4114,   // Etappe 17h: Wetter-Intensität
    IDLG_BLOCKED     = 4115,   // Etappe 19k.2: Block-Zonen (x,z,w,h pro Zeile)
};

static HWND g_dlg = nullptr;

HWND dlg_control(const wchar_t* cls, const wchar_t* text, DWORD style,
                 int x, int y, int w, int h, int id) {
    HWND c = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style,
                             x, y, w, h, g_dlg,
                             reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                             GetModuleHandleW(nullptr), nullptr);
    SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(g.font), TRUE);
    return c;
}

void dlg_label(const wchar_t* text, int x, int y, int w) {
    dlg_control(L"Static", text, 0, x, y, w, 20, 0);
}

std::string dlg_text(int id) {
    wchar_t buf[512];
    GetDlgItemTextW(g_dlg, id, buf, 512);
    return wide_to_utf8(buf);
}

// Etappe 19k.2: Text eines Dialogfelds OHNE 512-Zeichen-Limit. Mehrzeiler
// (Kommentar, Block-Zonen) können länger sein - dlg_text würde dort still
// abschneiden und beim Speichern Zeilen verlieren.
std::string dlg_text_long(int id) {
    HWND h = GetDlgItem(g_dlg, id);
    if (!h) return std::string();
    const int len = GetWindowTextLengthW(h);
    if (len <= 0) return std::string();
    std::wstring buf(static_cast<size_t>(len) + 1, L'\0');
    GetWindowTextW(h, &buf[0], static_cast<int>(buf.size()));
    return wide_to_utf8(buf.c_str());
}

void dlg_set_text(int id, const std::string& s) {
    SetDlgItemTextW(g_dlg, id, utf8_to_wide(s).c_str());
}

// Hexfarbe "BFD8EC" oder "0xBFD8EC" -> 0x00BFD8EC (0 bei Unsinn).
uint32_t parse_hex_color(const std::string& s) {
    std::string t = s;
    if (t.rfind("0x", 0) == 0 || t.rfind("0X", 0) == 0) t = t.substr(2);
    if (t.size() != 6) return 0;
    uint32_t v = 0;
    for (char c : t) {
        v <<= 4;
        if (c >= '0' && c <= '9') v |= static_cast<uint32_t>(c - '0');
        else if (c >= 'a' && c <= 'f') v |= static_cast<uint32_t>(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') v |= static_cast<uint32_t>(c - 'A' + 10);
        else return 0;
    }
    return v;
}

LRESULT CALLBACK dlg_proc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_COMMAND:
            if (LOWORD(wp) == IDLG_OK) {
                if (g.dlg_mode == 1) apply_props_from_dlg();
                if (g.dlg_mode == 2) apply_new_from_dlg();
                DestroyWindow(h);
                return 0;
            }
            if (LOWORD(wp) == IDLG_CANCEL) {
                DestroyWindow(h);
                return 0;
            }
            break;
        case WM_CLOSE:
            DestroyWindow(h);
            return 0;
        case WM_DESTROY:
            if (g.props_dlg == h) g.props_dlg = nullptr;
            return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

void modal_loop(HWND dlg) {
    EnableWindow(g.main, FALSE);
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (msg.message == WM_QUIT) break;
        if (IsDialogMessageW(dlg, &msg)) continue;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
        if (!IsWindow(dlg)) break;
    }
    EnableWindow(g.main, TRUE);
    SetForegroundWindow(g.main);
}

// Flächen-Liste (terrain.splats) aus dem Karten-JSON lesen.
std::vector<world::SplatRect> read_splats(const JsonValue* t) {
    std::vector<world::SplatRect> splats;
    const JsonValue* arr = t ? t->find("splats") : nullptr;
    if (!arr || !arr->is_arr()) return splats;
    for (const JsonValue& e : arr->arr) {
        if (!e.is_obj()) continue;
        const JsonValue* tex = e.find("tex");
        const JsonValue* x = e.find("x");
        const JsonValue* z = e.find("z");
        const JsonValue* w = e.find("w");
        const JsonValue* h = e.find("h");
        world::SplatRect s;
        s.tex = tex ? tex->str : std::string();
        s.x = x ? static_cast<float>(x->number(0)) : 0.0f;
        s.z = z ? static_cast<float>(z->number(0)) : 0.0f;
        s.w = w ? static_cast<float>(w->number(0)) : 0.0f;
        s.h = h ? static_cast<float>(h->number(0)) : 0.0f;
        if (!s.tex.empty() && s.w > 0.0f && s.h > 0.0f) splats.push_back(s);
    }
    return splats;
}

// Etappe 18e: Boden-Fläche anlegen (rev = false) oder Punkt-löschen (true).
// Zentriert x/z, Ausdehnung w/h; legt terrain.splats an, falls fehlt.
// Rückgabe: true wenn die Karte geändert wurde.
bool splat_edit(float cx, float cz, float sw, float sh,
               const std::string& tex, bool erase, bool snap) {
    if (g.cur_map < 0) return false;
    if (snap) {   // aufs Raster ziehen (wie Objekte)
        cx = std::round(cx / g.grid) * g.grid;
        cz = std::round(cz / g.grid) * g.grid;
        sw = std::max(g.grid, std::round(sw / g.grid) * g.grid);
        sh = std::max(g.grid, std::round(sh / g.grid) * g.grid);
    }
    JsonValue* ter = g.map_root.find("terrain");
    if (!ter) ter = &g.map_root.set("terrain", JsonValue::make_obj());
    JsonValue* arr = ter->find("splats");
    if (!arr || !arr->is_arr()) arr = &ter->set("splats", JsonValue::make_arr());

    if (!erase) {
        JsonValue e = JsonValue::make_obj();
        e.set("tex", JsonValue::make_str(tex));
        e.set("x", JsonValue::make_num(static_cast<double>(cx - sw * 0.5f)));
        e.set("z", JsonValue::make_num(static_cast<double>(cz - sh * 0.5f)));
        e.set("w", JsonValue::make_num(static_cast<double>(sw)));
        e.set("h", JsonValue::make_num(static_cast<double>(sh)));
        arr->arr.push_back(e);
        return true;
    }
    // Löschen: letzte (späteste) Fläche entfernen, die den Punkt abdeckt.
    for (int i = static_cast<int>(arr->arr.size()) - 1; i >= 0; --i) {
        const JsonValue& e = arr->arr[i];
        if (!e.is_obj()) continue;
        const JsonValue* ex = e.find("x");
        const JsonValue* ez = e.find("z");
        const JsonValue* ew = e.find("w");
        const JsonValue* eh = e.find("h");
        const float x0 = ex ? static_cast<float>(ex->number(0)) : 0.0f;
        const float z0 = ez ? static_cast<float>(ez->number(0)) : 0.0f;
        const float w0 = ew ? static_cast<float>(ew->number(0)) : 0.0f;
        const float h0 = eh ? static_cast<float>(eh->number(0)) : 0.0f;
        if (cx >= x0 && cx < x0 + w0 && cz >= z0 && cz < z0 + h0) {
            arr->arr.erase(arr->arr.begin() + i);
            if (arr->arr.empty()) json_remove_key(*ter, "splats");
            return true;
        }
    }
    return false;
}

// Etappe 18l: Höhen-Schritt pro Klick (Meter). Ein Klick Klick stapelt.
float height_step() { return 2.0f; }

// Etappe 18l: Berg/Senke an (cx,cz) malen. Legt terrain.heights an, hängt
// einen weichen Pinsel {x,z,r,h} an (r = halbe Pinselgrösse, h = +-Schritt).
// Rückgabe: Anzahl Pinsel-Einträge nach dem Malen (0 = kein Kartenkontext).
int height_edit(float cx, float cz, bool sink) {
    if (g.cur_map < 0) return 0;
    // aufs Raster ziehen wie Objekte/Splats, Radius mind. 1 Rasterzelle
    cx = std::round(cx / g.grid) * g.grid;
    cz = std::round(cz / g.grid) * g.grid;
    const float r = std::max(g.grid, g.paint_size * 0.5f);
    const float hgt = sink ? -height_step() : height_step();

    JsonValue* ter = g.map_root.find("terrain");
    if (!ter) ter = &g.map_root.set("terrain", JsonValue::make_obj());
    JsonValue* arr = ter->find("heights");
    if (!arr || !arr->is_arr()) arr = &ter->set("heights", JsonValue::make_arr());

    // Aufeinanderfolgende Klicks an derselben Stelle (gleiche Kachel) zum
    // selben Pinsel addieren, damit die JSON-Liste beim "Höhen-Stapeln"
    // nicht endlos wächst.
    const double cell = static_cast<double>(g.grid);
    for (JsonValue& e : arr->arr) {
        if (!e.is_obj()) continue;
        const JsonValue* ex = e.find("x");
        const JsonValue* ez = e.find("z");
        const JsonValue* er = e.find("r");
        if (!ex || !ez || !er) continue;
        const double dx = ex->number(0) - static_cast<double>(cx);
        const double dz = ez->number(0) - static_cast<double>(cz);
        const double rr = er->number(0);
        if (dx * dx + dz * dz <= cell * cell * 0.25 && rr == static_cast<double>(r)) {
            const JsonValue* eh = e.find("h");
            e.set("h", JsonValue::make_num((eh ? eh->number(0) : 0.0) +
                                            static_cast<double>(hgt)));
            return static_cast<int>(arr->arr.size());
        }
    }

    JsonValue e = JsonValue::make_obj();
    e.set("x", JsonValue::make_num(static_cast<double>(cx)));
    e.set("z", JsonValue::make_num(static_cast<double>(cz)));
    e.set("r", JsonValue::make_num(static_cast<double>(r)));
    e.set("h", JsonValue::make_num(static_cast<double>(hgt)));
    arr->arr.push_back(e);
    return static_cast<int>(arr->arr.size());
}

void regen_terrain() {
    const JsonValue* t = g.map_root.find("terrain");
    auto tnum = [&](const char* key, double fallback) {
        if (!t) return fallback;
        const JsonValue* v = t->find(key);
        return v ? v->number(fallback) : fallback;
    };
    const double size_fallback = tnum("size", 240);
    const float w = static_cast<float>(tnum("width", size_fallback));
    const float h = static_cast<float>(tnum("height", size_fallback));
    const int res = static_cast<int>(tnum("resolution", 192));
    const uint32_t seed = static_cast<uint32_t>(tnum("seed", 7));
    const float amp = static_cast<float>(tnum("amplitude", 0.0));
    // Etappe 18l: gemalte Berge/Senken (terrain.heights) vor dem Generieren
    // ans Terrain geben - compute_heights() baut sie nach dem FBM ein.
    std::vector<math::Vec4> brushes;
    const JsonValue* heights = t ? t->find("heights") : nullptr;
    if (heights && heights->is_arr()) {
        for (const JsonValue& e : heights->arr) {
            if (!e.is_obj()) continue;
            const JsonValue* bx = e.find("x");
            const JsonValue* bz = e.find("z");
            const JsonValue* br = e.find("r");
            const JsonValue* bh = e.find("h");
            math::Vec4 b{};
            b.x = bx ? static_cast<float>(bx->number(0)) : 0.0f;
            b.z = bz ? static_cast<float>(bz->number(0)) : 0.0f;
            b.w = br ? static_cast<float>(br->number(0)) : 0.0f;   // Radius
            b.y = bh ? static_cast<float>(bh->number(0)) : 0.0f;   // Höhe
            if (b.w > 0.0f && b.y != 0.0f) brushes.push_back(b);
        }
    }
    g.terrain->set_height_brushes(brushes);
    // Etappe 18d: mit Boden-Flächen Atlas-Pfad (pro-Quad-UV), sonst der
    // alte Ein-Textur-Pfad inkl. Kachelgrösse (live-kompatibel).
    const std::vector<world::SplatRect> splats = read_splats(t);
    const JsonValue* tx = t ? t->find("texture") : nullptr;
    const std::string base_tex = tx ? tx->str : std::string("gras");
    if (!splats.empty()) {
        const world::AtlasInfo atlas = g.renderer->update_atlas();
        if (atlas.valid) {
            g.terrain->generate_splat(w, h, res, seed, amp, base_tex, splats, atlas);
            g.renderer->set_terrain(g.terrain->vertices(), g.terrain->indices());
            g.renderer->set_terrain_tex(atlas.tex_index);
            g.renderer->set_terrain_tile(1.0f);
            core::log_info("Splat-Boden aktiv: " + std::to_string(splats.size()) +
                           " Fläche(n), Atlas-Slot " + std::to_string(atlas.tex_index));
            return;
        }
        core::log_warn("Splat-Boden: kein Atlas möglich (keine 64x64-Texturen?)");
    }
    g.terrain->generate(w, h, res, seed, amp);
    g.renderer->set_terrain(g.terrain->vertices(), g.terrain->indices());
    g.renderer->set_terrain_tex(g.renderer->texture_index(base_tex));
    const JsonValue* tl = t ? t->find("tile") : nullptr;
    g.renderer->set_terrain_tile(tl ? static_cast<float>(tl->number(2.0)) : 2.0f);
}

// Himmel/Nebel im Editor-Viewport aus der Karte übernehmen.
void apply_map_atmosphere_editor() {
    const JsonValue* pan = g.map_root.find("panorama");
    const JsonValue* pc = pan ? pan->find("color") : nullptr;
    const uint32_t pan_col = pc ? static_cast<uint32_t>(pc->number(0x3D6FB5)) : 0x3D6FB5;
    g.renderer->set_sky(pan_col, pan_col);

    const JsonValue* fog = g.map_root.find("fog");
    const JsonValue* on = fog ? fog->find("on") : nullptr;
    const JsonValue* fc = fog ? fog->find("color") : nullptr;
    const JsonValue* ff = fog ? fog->find("far") : nullptr;
    g.renderer->set_fog(on && on->b,
                        fc ? static_cast<uint32_t>(fc->number(0xBFD8EC)) : 0xBFD8EC,
                        static_cast<float>(ff ? ff->number(180.0) : 180.0));
}

void open_map_props() {
    if (g.cur_map < 0) return;
    if (g.props_dlg) { SetForegroundWindow(g.props_dlg); return; }   // schon offen

    WNDCLASSW wc{};
    wc.lpfnWndProc = dlg_proc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"RuneEditorDlg";
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
    RegisterClassW(&wc);

    const int W = 460, H = 660;   // 19k.2: Platz für das Block-Zonen-Feld
    RECT mr;
    GetWindowRect(g.main, &mr);
    g_dlg = CreateWindowExW(WS_EX_DLGMODALFRAME, L"RuneEditorDlg",
                            L"Karten-Eigenschaften",
                            WS_POPUP | WS_CAPTION | WS_SYSMENU,
                            mr.left + 200, mr.top + 80, W, H,
                            g.main, nullptr, GetModuleHandleW(nullptr), nullptr);

    dlg_label(L"Name", 16, 16, 100);
    dlg_control(L"Edit", nullptr, WS_BORDER | ES_LEFT, 120, 12, 310, 24, IDLG_NAME);

    dlg_label(L"Breite (m)", 16, 48, 100);
    dlg_control(L"Edit", nullptr, WS_BORDER | ES_LEFT, 120, 44, 90, 24, IDLG_WIDTH);
    dlg_label(L"Höhe (m)", 230, 48, 80);
    dlg_control(L"Edit", nullptr, WS_BORDER | ES_LEFT, 320, 44, 90, 24, IDLG_HEIGHT);

    dlg_control(L"Button", L"Berge (Gelände-Rauschen)", BS_AUTOCHECKBOX,
                16, 80, 300, 22, IDLG_BERGE);
    dlg_label(L"Seed", 16, 110, 100);
    dlg_control(L"Edit", nullptr, WS_BORDER | ES_LEFT, 120, 106, 90, 24, IDLG_SEED);
    dlg_label(L"Berg-Höhe (m)", 230, 110, 110);
    dlg_control(L"Edit", nullptr, WS_BORDER | ES_LEFT, 350, 106, 80, 24, IDLG_AMP);

    dlg_control(L"Button", L"Nebel (Distanznebel)", BS_AUTOCHECKBOX,
                16, 142, 300, 22, IDLG_FOG);
    dlg_label(L"Sichtweite (m)", 16, 172, 100);
    dlg_control(L"Edit", nullptr, WS_BORDER | ES_LEFT, 120, 168, 90, 24, IDLG_FOGFAR);
    dlg_label(L"Nebelfarbe (hex)", 230, 172, 110);
    dlg_control(L"Edit", nullptr, WS_BORDER | ES_LEFT, 350, 168, 80, 24, IDLG_FOGCOL);

    dlg_label(L"Panorama-Farbe (hex)", 16, 204, 140);
    dlg_control(L"Edit", nullptr, WS_BORDER | ES_LEFT, 170, 200, 110, 24, IDLG_PANCOL);

    // Etappe 17h: Wetter (kein/regen/schnee) + Intensität.
    dlg_label(L"Wetter", 16, 236, 100);
    HWND cb_wtype = dlg_control(L"ComboBox", nullptr, CBS_DROPDOWNLIST | WS_VSCROLL,
                                120, 232, 110, 200, IDLG_WTYPE);
    for (const wchar_t* wt : { L"kein", L"regen", L"schnee" }) {
        SendMessageW(cb_wtype, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(wt));
    }
    dlg_label(L"Intensität", 240, 236, 80);
    dlg_control(L"Edit", nullptr, WS_BORDER | ES_LEFT, 330, 232, 60, 24, IDLG_WINT);

    dlg_label(L"Kommentar", 16, 268, 140);
    dlg_control(L"Edit", nullptr,
                WS_BORDER | WS_VSCROLL | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL,
                16, 290, 414, 110, IDLG_NOTE);

    // Etappe 19k.2: Block-Zonen (Passierbarkeit) - „x,z,w,h" pro Zeile,
    // x/z = Mitte in Metern, w/h = Größe. Wirkt im Spiel als unsichtbare
    // Wand über alle Höhen (Physik + RGSS passable?).
    dlg_label(L"Block-Zonen (x,z,w,h pro Zeile)", 16, 408, 300);
    dlg_control(L"Edit", nullptr,
                WS_BORDER | WS_VSCROLL | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL,
                16, 428, 414, 150, IDLG_BLOCKED);

    dlg_control(L"Button", L"OK", BS_DEFPUSHBUTTON, 240, 590, 90, 30, IDLG_OK);
    dlg_control(L"Button", L"Abbrechen", BS_PUSHBUTTON, 340, 590, 90, 30, IDLG_CANCEL);

    // Felder füllen
    const JsonValue* t = g.map_root.find("terrain");
    auto tnum = [&](const char* key, double fallback) {
        if (!t) return fallback;
        const JsonValue* v = t->find(key);
        return v ? v->number(fallback) : fallback;
    };
    const double size_fallback = tnum("size", 240);
    char buf[64];
    dlg_set_text(IDLG_NAME, g.map_root.find("name") ? g.map_root.find("name")->str : "");
    std::snprintf(buf, sizeof(buf), "%d", static_cast<int>(tnum("width", size_fallback)));
    dlg_set_text(IDLG_WIDTH, buf);
    std::snprintf(buf, sizeof(buf), "%d", static_cast<int>(tnum("height", size_fallback)));
    dlg_set_text(IDLG_HEIGHT, buf);
    const double amp = tnum("amplitude", 0.0);
    CheckDlgButton(g_dlg, IDLG_BERGE, amp > 0.0 ? BST_CHECKED : BST_UNCHECKED);
    std::snprintf(buf, sizeof(buf), "%u", static_cast<uint32_t>(tnum("seed", 7)));
    dlg_set_text(IDLG_SEED, buf);
    std::snprintf(buf, sizeof(buf), "%.1f", amp);
    dlg_set_text(IDLG_AMP, buf);

    const JsonValue* fog = g.map_root.find("fog");
    const JsonValue* fon = fog ? fog->find("on") : nullptr;
    const JsonValue* ffar = fog ? fog->find("far") : nullptr;
    const JsonValue* fcol = fog ? fog->find("color") : nullptr;
    CheckDlgButton(g_dlg, IDLG_FOG, (fon && fon->b) ? BST_CHECKED : BST_UNCHECKED);
    std::snprintf(buf, sizeof(buf), "%d", static_cast<int>(ffar ? ffar->number(180.0) : 180.0));
    dlg_set_text(IDLG_FOGFAR, buf);
    std::snprintf(buf, sizeof(buf), "%06X",
                  static_cast<uint32_t>(fcol ? fcol->number(0xBFD8EC) : 0xBFD8EC));
    dlg_set_text(IDLG_FOGCOL, buf);

    const JsonValue* pan = g.map_root.find("panorama");
    const JsonValue* pcol = pan ? pan->find("color") : nullptr;
    std::snprintf(buf, sizeof(buf), "%06X",
                  static_cast<uint32_t>(pcol ? pcol->number(0x3D6FB5) : 0x3D6FB5));
    dlg_set_text(IDLG_PANCOL, buf);

    // Etappe 17h: Wetter-Typ + Intensität aus dem Karten-JSON.
    {
        const JsonValue* w = g.map_root.find("weather");
        const std::string wt = w ? (w->find("type") ? w->find("type")->str : "kein")
                                : std::string("kein");
        int sel = 0;
        if (wt == "regen")  sel = 1;
        if (wt == "schnee") sel = 2;
        SendMessageW(GetDlgItem(g_dlg, IDLG_WTYPE), CB_SETCURSEL, sel, 0);
        std::snprintf(buf, sizeof(buf), "%.1f",
                      w && w->find("intensity") ? w->find("intensity")->number(1.0) : 1.0);
        dlg_set_text(IDLG_WINT, buf);
    }

    const JsonValue* note = g.map_root.find("note");
    dlg_set_text(IDLG_NOTE, note ? note->str : "");

    // Etappe 19k.2: Block-Zonen als Textzeilen „x,z,w,h" anzeigen.
    {
        std::string zeilen;
        const JsonValue* bl = g.map_root.find("blocked");
        if (bl && bl->is_arr()) {
            char zb[96];
            for (const JsonValue& b : bl->arr) {
                if (!b.is_obj()) continue;
                auto num = [&b](const char* k) {
                    const JsonValue* v = b.find(k);
                    return v ? v->number(0.0) : 0.0;
                };
                std::snprintf(zb, sizeof(zb), "%.10g,%.10g,%.10g,%.10g",
                              num("x"), num("z"), num("w"), num("h"));
                if (!zeilen.empty()) zeilen += "\r\n";
                zeilen += zb;
            }
        }
        dlg_set_text(IDLG_BLOCKED, zeilen);
    }

    ShowWindow(g_dlg, SW_SHOW);
    SetFocus(GetDlgItem(g_dlg, IDLG_NAME));
    g.props_dlg = g_dlg;      // moduslos: Hauptschleife rendert weiter
    g.dlg_mode = 1;
}

// Werte aus dem offenen Eigenschaften-Dialog in die Karte übernehmen (OK).
void apply_props_from_dlg() {
    HWND dlg = g.props_dlg;
    if (!dlg) return;
    {
        JsonValue& root = g.map_root;
        root.set("name", JsonValue::make_str(dlg_text(IDLG_NAME)));

        const JsonValue* t = g.map_root.find("terrain");
        JsonValue ter = t ? *t : JsonValue::make_obj();
        ter.set("width", JsonValue::make_num(std::atof(dlg_text(IDLG_WIDTH).c_str()), false));
        ter.set("height", JsonValue::make_num(std::atof(dlg_text(IDLG_HEIGHT).c_str()), false));
        const bool berge = IsDlgButtonChecked(dlg, IDLG_BERGE) == BST_CHECKED;
        ter.set("seed", JsonValue::make_num(std::atof(dlg_text(IDLG_SEED).c_str()), true));
        ter.set("amplitude", JsonValue::make_num(
            berge ? std::atof(dlg_text(IDLG_AMP).c_str()) : 0.0, false));
        ter.set("resolution", JsonValue::make_num(
            t && t->find("resolution") ? t->find("resolution")->number(192) : 192, true));
        root.set("terrain", ter);

        JsonValue fogv = JsonValue::make_obj();
        fogv.set("on", JsonValue::make_bool(
            IsDlgButtonChecked(dlg, IDLG_FOG) == BST_CHECKED));
        fogv.set("far", JsonValue::make_num(std::atof(dlg_text(IDLG_FOGFAR).c_str()), false));
        fogv.set("color", JsonValue::make_num(
            static_cast<double>(parse_hex_color(dlg_text(IDLG_FOGCOL))), true));
        root.set("fog", fogv);

        JsonValue panv = JsonValue::make_obj();
        panv.set("color", JsonValue::make_num(
            static_cast<double>(parse_hex_color(dlg_text(IDLG_PANCOL))), true));
        root.set("panorama", panv);

        // Etappe 17h: Wetter in das Karten-JSON schreiben.
        {
            LRESULT sel = SendMessageW(GetDlgItem(dlg, IDLG_WTYPE), CB_GETCURSEL, 0, 0);
            JsonValue wv = JsonValue::make_obj();
            if (sel == 1)      wv.set("type", JsonValue::make_str("regen"));
            else if (sel == 2) wv.set("type", JsonValue::make_str("schnee"));
            else               wv.set("type", JsonValue::make_str("kein"));
            double wint = std::atof(dlg_text(IDLG_WINT).c_str());
            if (wint <= 0.0) wint = 1.0;
            wv.set("intensity", JsonValue::make_num(wint, false));
            root.set("weather", wv);
        }

        root.set("note", JsonValue::make_str(dlg_text_long(IDLG_NOTE)));

        // Etappe 19k.2: Block-Zonen - „x,z,w,h" pro Zeile -> JSON-Array.
        // Wirkungslose Zeilen (w/h <= 0, zu wenige Zahlen) werden überlesen;
        // eine leere Liste entfernt den Schlüssel ganz.
        {
            JsonValue zones = JsonValue::make_arr();
            const std::string text = dlg_text_long(IDLG_BLOCKED);
            size_t pos = 0;
            while (pos <= text.size()) {
                const size_t nl = text.find_first_of("\r\n", pos);
                const std::string line = text.substr(
                    pos, nl == std::string::npos ? std::string::npos : nl - pos);
                pos = (nl == std::string::npos) ? text.size() + 1 : nl + 1;
                std::vector<std::string> toks;
                size_t t0 = 0;
                for (size_t i = 0; i <= line.size(); ++i) {
                    if (i == line.size() || line[i] == ',' || line[i] == ';') {
                        toks.push_back(line.substr(t0, i - t0));
                        t0 = i + 1;
                    }
                }
                if (toks.size() < 4) continue;
                const double zx = std::atof(toks[0].c_str());
                const double zz = std::atof(toks[1].c_str());
                const double zw = std::atof(toks[2].c_str());
                const double zh = std::atof(toks[3].c_str());
                if (zw <= 0.0 || zh <= 0.0) continue;
                JsonValue zone = JsonValue::make_obj();
                zone.set("x", JsonValue::make_num(zx, false));
                zone.set("z", JsonValue::make_num(zz, false));
                zone.set("w", JsonValue::make_num(zw, false));
                zone.set("h", JsonValue::make_num(zh, false));
                zones.arr.push_back(std::move(zone));
            }
            if (zones.arr.empty()) json_remove_key(root, "blocked");
            else root.set("blocked", zones);
        }

        g.dirty = true;
        regen_terrain();
        apply_map_atmosphere_editor();
        if (g.cur_map < static_cast<int>(g.map_names.size())) {
            g.map_names[g.cur_map] = dlg_text(IDLG_NAME);
            rebuild_map_menu();
        }
        update_title();
        core::log_info("Karten-Eigenschaften geändert");
    }
}

// Neue Karte: kleiner Dialog (Name, Breite, Höhe, Berge) -> JSON-Datei.
void open_map_new() {
    if (g.props_dlg) { SetForegroundWindow(g.props_dlg); return; }   // schon offen

    WNDCLASSW wc{};
    wc.lpfnWndProc = dlg_proc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"RuneEditorDlgNew";
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
    RegisterClassW(&wc);

    const int W = 380, H = 250;
    RECT mr;
    GetWindowRect(g.main, &mr);
    g_dlg = CreateWindowExW(WS_EX_DLGMODALFRAME, L"RuneEditorDlgNew", L"Neue Karte",
                            WS_POPUP | WS_CAPTION | WS_SYSMENU,
                            mr.left + 240, mr.top + 160, W, H,
                            g.main, nullptr, GetModuleHandleW(nullptr), nullptr);

    dlg_label(L"Name", 16, 16, 100);
    dlg_control(L"Edit", L"Neue Welt", WS_BORDER | ES_LEFT, 120, 12, 230, 24, IDLG_NAME);
    dlg_label(L"Breite (m)", 16, 48, 100);
    dlg_control(L"Edit", L"240", WS_BORDER | ES_LEFT, 120, 44, 90, 24, IDLG_WIDTH);
    dlg_label(L"Höhe (m)", 16, 80, 100);
    dlg_control(L"Edit", L"240", WS_BORDER | ES_LEFT, 120, 76, 90, 24, IDLG_HEIGHT);
    dlg_control(L"Button", L"Berge erzeugen", BS_AUTOCHECKBOX,
                16, 110, 240, 22, IDLG_BERGE);
    dlg_control(L"Button", L"OK", BS_DEFPUSHBUTTON, 160, 170, 90, 30, IDLG_OK);
    dlg_control(L"Button", L"Abbrechen", BS_PUSHBUTTON, 260, 170, 90, 30, IDLG_CANCEL);

    ShowWindow(g_dlg, SW_SHOW);
    SetFocus(GetDlgItem(g_dlg, IDLG_NAME));
    g.props_dlg = g_dlg;      // moduslos: Hauptschleife rendert weiter
    g.dlg_mode = 2;
}

// Neue Karte aus dem Dialog anlegen (OK).
void apply_new_from_dlg() {
    // nächste freie ID: welt01, welt02, ...
    int n = 1;
    std::string id;
    char buf[32];
    do {
        std::snprintf(buf, sizeof(buf), "welt%02d", n++);
        id = buf;
    } while (std::find(g.map_files.begin(), g.map_files.end(), id + ".json") != g.map_files.end());

    const float w = static_cast<float>(std::max(60.0, std::atof(dlg_text(IDLG_WIDTH).c_str())));
    const float h = static_cast<float>(std::max(60.0, std::atof(dlg_text(IDLG_HEIGHT).c_str())));
    const bool berge = IsDlgButtonChecked(g_dlg, IDLG_BERGE) == BST_CHECKED;

        JsonValue map = JsonValue::make_obj();
        map.set("name", JsonValue::make_str(dlg_text(IDLG_NAME).empty() ? "Neue Welt" : dlg_text(IDLG_NAME)));
        JsonValue ter = JsonValue::make_obj();
        ter.set("width", JsonValue::make_num(w, false));
        ter.set("height", JsonValue::make_num(h, false));
        ter.set("resolution", JsonValue::make_num(192, true));
        ter.set("seed", JsonValue::make_num(static_cast<double>(GetTickCount() % 9999 + 1), true));
        ter.set("amplitude", JsonValue::make_num(berge ? 18.0 : 0.0, false));
        map.set("terrain", ter);
        map.set("objects", JsonValue::make_arr());
        map.set("events", JsonValue::make_arr());
        JsonValue start = JsonValue::make_obj();
        start.set("x", JsonValue::make_num(0.0, false));
        start.set("z", JsonValue::make_num(0.0, false));
        map.set("start", start);
        JsonValue fog = JsonValue::make_obj();
        fog.set("on", JsonValue::make_bool(false));
        fog.set("far", JsonValue::make_num(180.0, false));
        fog.set("color", JsonValue::make_num(0xBFD8EC, true));
        map.set("fog", fog);
        JsonValue pan = JsonValue::make_obj();
        pan.set("color", JsonValue::make_num(0x3D6FB5, true));
        map.set("panorama", pan);
        map.set("note", JsonValue::make_str(""));

        const std::string path = g.project_dir + "\\maps\\" + id + ".json";
        if (write_file(path, json_write(map))) {
            core::log_info("Neue Karte angelegt: " + id);
            scan_maps();
            rebuild_map_menu();
            // geladen wird gleich - Index der neuen Karte suchen
            for (int i = 0; i < static_cast<int>(g.map_files.size()); ++i) {
                if (g.map_files[i] == id + ".json") { load_map(i); break; }
            }
            sync_object_list();
            sync_property_fields();
            update_title();
            status_mode(L"Neue Karte angelegt");
        } else {
            core::log_error("Neue Karte nicht schreibbar: " + path);
        }
}

void delete_current_map() {
    if (g.cur_map < 0) return;
    const std::string name = g.cur_map < static_cast<int>(g.map_names.size())
                                 ? g.map_names[g.cur_map] : "?";
    const std::wstring text = L"Karte \"" + utf8_to_wide(name) +
                              L"\" wirklich löschen?";
    const int r = MessageBoxW(g.main, text.c_str(), L"Karte löschen",
                              MB_YESNO | MB_ICONWARNING);
    if (r != IDYES) return;
    const std::string path = g.project_dir + "\\maps\\" + g.map_files[g.cur_map];
    DeleteFileA(path.c_str());
    core::log_info("Karte gelöscht: " + path);
    scan_maps();
    rebuild_map_menu();
    if (!g.map_files.empty()) {
        load_map(0);
    } else {
        g.cur_map = -1;
        g.objects = nullptr;
        g.renderer->clear_terrain();
    }
    sync_object_list();
    sync_property_fields();
    update_title();
}

// ============================================================================
//  Etappe 17d: Event-Editor
//
//  Eigenes modusloses Win32-Fenster (Klasse "RuneEditorEventDlg") parallel zum
//  Editor. Es arbeitet direkt auf g.map_root["events"]; gespeichert wird mit
//  Strg+S im Hauptfenster. Befehle werden als Baum mit Dann-/Sonst-Zweigen
//  angezeigt; das Formular unten bearbeitet genau einen Befehl.
// ============================================================================
enum {
    IDEV_LIST     = 5001,
    IDEV_NEW      = 5002,
    IDEV_DEL      = 5003,
    IDEV_DUP      = 5004,
    IDEV_CLOSE    = 5005,
    IDEV_ID       = 5010,
    IDEV_NAME     = 5011,
    IDEV_X        = 5012,
    IDEV_Z        = 5013,
    IDEV_RADIUS   = 5014,
    IDEV_TRIGGER  = 5015,
    IDEV_APPLY_EV = 5016,
    IDEV_CMDS     = 5020,
    IDEV_CETYPE   = 5021,
    IDEV_INSERT   = 5022,
    IDEV_APPLYCMD = 5023,
    IDEV_DELCMD   = 5024,
    IDEV_UP       = 5025,
    IDEV_DOWN     = 5026,
    IDEV_CE_NAME  = 5030,
    IDEV_CE_VAL1  = 5031,
    IDEV_CE_VAL2  = 5032,
    IDEV_CE_VAL3  = 5033,
    IDEV_CE_TEXT  = 5034,
    IDEV_CE_OP    = 5035,
    IDEV_CE_BOOL  = 5036,
    IDEV_L_NAME   = 5040,
    IDEV_L_VAL1   = 5041,
    IDEV_L_VAL2   = 5042,
    IDEV_L_VAL3   = 5043,
    IDEV_L_TEXT   = 5044,
    IDEV_L_INSERT = 5045,
    // Etappe 18g: Event-Seiten (RPG-Maker-XP-Feature)
    IDEV_PAGES     = 5046,
    IDEV_NEWPAGE   = 5047,
    IDEV_DELPAGE   = 5048,
    IDEV_APPLY_PAGE = 5049,
    IDEV_CSWITCH   = 5050,   // Bedingung: Schalter-Name
    IDEV_CSWVAL    = 5051,   // Bedingung: Schalter an/aus
    IDEV_CVAR      = 5052,   // Bedingung: Variablen-Name
    IDEV_CVOP      = 5053,   // Bedingung: Vergleichsoperator
    IDEV_CVVAL     = 5054,   // Bedingung: Variablenwert
    // Etappe 19d: Self-Switch-Bedingung der Seite (Buchstabe + an/aus)
    IDEV_CSELF     = 5055,
    IDEV_CSELFVAL  = 5056,
};

// Ein Eintrag der flachen Befehlsliste. list/index zeigen in den JSON-Baum;
// header = "Dann:"/"Sonst:"-Zeile (list ist dann das Ziel-Array).
struct EventCmdRef {
    JsonValue* list = nullptr;
    int  index = -1;
    int  depth = 0;
    bool header = false;
    std::string label;
};

struct EventDlgState {
    HWND win = nullptr;
    HWND list_events = nullptr;
    HWND list_cmds = nullptr;
    HWND ed_id = nullptr;
    HWND ed_name = nullptr;
    HWND ed_x = nullptr;
    HWND ed_z = nullptr;
    HWND ed_radius = nullptr;
    HWND cb_trigger = nullptr;
    HWND cb_ce_type = nullptr;
    HWND ed_ce_name = nullptr;
    HWND ed_ce_val1 = nullptr;
    HWND ed_ce_val2 = nullptr;
    HWND ed_ce_val3 = nullptr;
    HWND ed_ce_text = nullptr;
    HWND cb_ce_op = nullptr;
    HWND cb_ce_bool = nullptr;
    HWND lbl_name = nullptr;
    HWND lbl_val1 = nullptr;
    HWND lbl_val2 = nullptr;
    HWND lbl_val3 = nullptr;
    HWND lbl_text = nullptr;
    HWND lbl_insert = nullptr;
    // Etappe 18g: Seiten
    HWND list_pages = nullptr;
    HWND ed_cswitch = nullptr;
    HWND cb_cswval = nullptr;
    HWND ed_cvar = nullptr;
    HWND cb_cvop = nullptr;
    HWND ed_cvval = nullptr;
    // Etappe 19d: Self-Switch-Bedingung (RPG Maker XP)
    HWND cb_cself = nullptr;      // keins/A/B/C/D
    HWND cb_cselfval = nullptr;   // an/aus
    int sel_event = -1;
    int sel_page = -1;
    int sel_cmd = -1;
    std::vector<EventCmdRef> refs;
    bool syncing = false;
};

static EventDlgState g_ev;

// Etappe 18f: Befehlstypen mit Kategorien (RPG-Maker-XP-Gruppen).
// Das Enum ersetzt die früheren magischen Indexzahlen überall.
enum EvType {
    ET_TEXT = 0, ET_CHOICES, ET_LOG,
    ET_WAIT, ET_SWITCH, ET_TOGGLE_SWITCH, ET_VARIABLE,
    ET_IF_SWITCH, ET_IF_VARIABLE, ET_SCRIPT,
    ET_TELEPORT, ET_MOVE_PLAYER, ET_HEAL,
    ET_WEATHER, ET_EFFECT, ET_SOUND,
    ET_ITEM, ET_GOLD,
    ET_SAVE_GAME, ET_LOAD_GAME,
    ET_SELF_SWITCH, ET_IF_SELF_SWITCH, ET_FADE,   // Etappe 19d (XP-Features)
    ET_MOVE_ROUTE,                                // Etappe 19g (XP-Feature)
    ET_INPUT_NUMBER,                              // Etappe 19i (XP-Feature)
    ET_BATTLE,                                    // Etappe 19i (XP-Feature)
    ET_COUNT
};
const char* const EV_TYPES[ET_COUNT] = {
    "text", "choices", "log",
    "wait", "switch", "toggle_switch", "variable",
    "if_switch", "if_variable", "script",
    "teleport", "move_player", "heal",
    "weather", "effect", "sound",
    "item", "gold",
    "save_game", "load_game",
    "self_switch", "if_self_switch", "fade",
    "move_route", "input_number", "battle"
};
const wchar_t* const EV_TYPE_LABELS[ET_COUNT] = {
    L"[Nachricht] Text", L"[Nachricht] Wahlmöglichkeiten", L"[Nachricht] Log",
    L"[Steuerung] Warte", L"[Steuerung] Schalter setzen",
    L"[Steuerung] Schalter umschalten", L"[Steuerung] Variable",
    L"[Steuerung] Wenn Schalter", L"[Steuerung] Wenn Variable",
    L"[Steuerung] Script",
    L"[Spieler] Teleport", L"[Spieler] Spieler bewegen", L"[Spieler] Heilen",
    L"[Welt] Wetter", L"[Welt] Effekt", L"[Welt] Sound",
    L"[Party] Item geben/nehmen", L"[Party] Gold +/-",
    L"[Spiel] Spiel speichern", L"[Spiel] Spiel laden",
    L"[Steuerung] Self-Switch", L"[Steuerung] Wenn Self-Switch",
    L"[Welt] Bild aus-/einblenden",
    L"[Bewegung] Bewegungsroute",
    L"[Steuerung] Zahleneingabe",
    L"[Kampf] Kampf starten"
};
const wchar_t* const EV_TRIGGERS[4] = { L"action", L"touch", L"auto", L"parallel" };
const wchar_t* const EV_OPS[6] = { L"==", L"!=", L"<", L"<=", L">", L">=" };

std::string ev_num_str(double v, int prec = 2) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.*f", prec, v);
    std::string s = buf;
    while (!s.empty() && s.back() == '0') s.pop_back();
    if (!s.empty() && s.back() == '.') s.pop_back();
    if (s.empty() || s == "-") s = "0";
    return s;
}

std::string ev_text(HWND h) {
    wchar_t buf[8192];
    buf[0] = 0;
    GetWindowTextW(h, buf, 8192);
    return wide_to_utf8(buf);
}

void ev_set_text(HWND h, const std::string& s) {
    SetWindowTextW(h, utf8_to_wide(s).c_str());
}

double ev_number(HWND h, double fallback) {
    std::string s = ev_text(h);
    std::replace(s.begin(), s.end(), ',', '.');
    if (s.empty()) return fallback;
    return std::atof(s.c_str());
}

std::string ev_str_of(const JsonValue& o, const char* key, const std::string& def = "") {
    const JsonValue* v = o.find(key);
    return (v && v->is_str()) ? v->str : def;
}

double ev_num_of(const JsonValue& o, const char* key, double def = 0.0) {
    const JsonValue* v = o.find(key);
    return (v && v->is_num()) ? v->num : def;
}

bool ev_bool_of(const JsonValue& o, const char* key, bool def = false) {
    const JsonValue* v = o.find(key);
    return (v && v->type == JsonValue::Type::Bool) ? v->b : def;
}

JsonValue* ev_events() {
    JsonValue* e = g.map_root.find("events");
    if (!e || !e->is_arr()) e = &g.map_root.set("events", JsonValue::make_arr());
    return e;
}

// Etappe 18g: Seiten-Array eines Events holen und notfalls aus dem alten
// Format migrieren (Top-Level trigger/commands werden zu Seite 0 umgebaut).
JsonValue* ev_ensure_pages(int ev_index) {
    JsonValue* evs = ev_events();
    if (ev_index < 0 || ev_index >= static_cast<int>(evs->arr.size())) return nullptr;
    JsonValue& e = evs->arr[ev_index];
    JsonValue* pages = e.find("pages");
    if (pages && pages->is_arr() && !pages->arr.empty()) return pages;

    JsonValue migrated = JsonValue::make_arr();
    JsonValue page0 = JsonValue::make_obj();
    page0.set("conditions", JsonValue::make_obj());
    const JsonValue* old_trig = e.find("trigger");
    page0.set("trigger", JsonValue::make_str(
        (old_trig && old_trig->is_str()) ? old_trig->str : "action"));
    const JsonValue* old_cmds = e.find("commands");
    page0.set("commands",
              (old_cmds && old_cmds->is_arr()) ? *old_cmds : JsonValue::make_arr());
    migrated.arr.push_back(std::move(page0));
    e.set("pages", std::move(migrated));
    json_remove_key(e, "trigger");
    json_remove_key(e, "commands");
    return e.find("pages");
}

// Objekt der gewählten Seite (oder nullptr, wenn es sie nicht gibt).
JsonValue* ev_page(int ev_index, int page_index) {
    JsonValue* pages = ev_ensure_pages(ev_index);
    if (!pages) return nullptr;
    if (page_index < 0 || page_index >= static_cast<int>(pages->arr.size())) return nullptr;
    return &pages->arr[page_index];
}

JsonValue* ev_current_page() {
    return ev_page(g_ev.sel_event, g_ev.sel_page);
}

// Befehlsliste der AKTÜLLEN SEITE (früher: Top-Level 'commands').
JsonValue* ev_commands(int ev_index) {
    JsonValue* pg = ev_page(ev_index, g_ev.sel_page);
    if (!pg) return nullptr;
    JsonValue* c = pg->find("commands");
    if (!c || !c->is_arr()) c = &pg->set("commands", JsonValue::make_arr());
    return c;
}

std::vector<std::string> ev_split_lines(const std::string& text) {
    std::vector<std::string> out;
    std::string cur;
    for (char ch : text) {
        if (ch == '\r') continue;
        if (ch == '\n') { out.push_back(cur); cur.clear(); }
        else cur.push_back(ch);
    }
    out.push_back(cur);
    return out;
}

std::string ev_join_lines(const JsonValue& cmd) {
    const JsonValue* lines = cmd.find("lines");
    if (!lines || !lines->is_arr()) return "";
    std::string out;
    for (size_t i = 0; i < lines->arr.size(); ++i) {
        if (i) out += "\r\n";
        out += lines->arr[i].str;
    }
    return out;
}

std::string ev_cmd_label(const JsonValue& c) {
    const std::string type = ev_str_of(c, "cmd");
    if (type == "text") {
        const JsonValue* lines = c.find("lines");
        std::string first;
        int n = 0;
        if (lines && lines->is_arr()) {
            n = static_cast<int>(lines->arr.size());
            if (n > 0) first = lines->arr[0].str;
        }
        std::string s = "Text: \"" + first + "\"";
        if (n > 1) s += " (+" + std::to_string(n - 1) + ")";
        return s;
    }
    if (type == "wait") return "Warte " + ev_num_str(ev_num_of(c, "seconds"), 2) + " s";
    if (type == "switch") {
        return std::string("Schalter \"") + ev_str_of(c, "name") + "\" = " +
               (ev_bool_of(c, "value") ? "an" : "aus");
    }
    if (type == "variable") {
        const JsonValue* add = c.find("add");
        if (add && (add->is_num())) {
            return "Variable \"" + ev_str_of(c, "name") + "\" += " + ev_num_str(add->num, 2);
        }
        return "Variable \"" + ev_str_of(c, "name") + "\" = " +
               ev_num_str(ev_num_of(c, "value"), 2);
    }
    if (type == "if_switch") {
        return "Wenn Schalter \"" + ev_str_of(c, "name") + "\" = " +
               (ev_bool_of(c, "value") ? "an" : "aus");
    }
    if (type == "if_variable") {
        return "Wenn \"" + ev_str_of(c, "name") + "\" " + ev_str_of(c, "op", "==") + " " +
               ev_num_str(ev_num_of(c, "value"), 2);
    }
    if (type == "self_switch") {
        return "Self-Switch " + ev_str_of(c, "letter") + " = " +
               (ev_bool_of(c, "value") ? "an" : "aus");
    }
    if (type == "if_self_switch") {
        return "Wenn Self-Switch " + ev_str_of(c, "letter") + " = " +
               (ev_bool_of(c, "value") ? "an" : "aus");
    }
    if (type == "fade") {
        const bool ein = ev_str_of(c, "direction", "aus") == "ein";
        return std::string(ein ? "Bild einblenden (" : "Bild ausblenden (") +
               ev_num_str(ev_num_of(c, "seconds"), 2) + " s)";
    }
    if (type == "move_route") {
        const JsonValue* steps = c.find("steps");
        const int n = (steps && steps->is_arr())
                          ? static_cast<int>(steps->arr.size()) : 0;
        return "Bewegungsroute: " + ev_str_of(c, "target") + " (" +
               std::to_string(n) + (n == 1 ? " Schritt" : " Schritte") +
               (ev_bool_of(c, "repeat") ? ", wiederholend)" : ")");
    }
    if (type == "input_number") {
        return "Zahleneingabe -> " + ev_str_of(c, "name") + " (" +
               ev_num_str(ev_num_of(c, "digits", 1.0), 0) + " Ziffern)";
    }
    if (type == "battle") {
        return std::string("Kampf: ") + ev_str_of(c, "name") +
               (ev_bool_of(c, "can_escape", true) ? "" : " (keine Flucht)");
    }
    if (type == "teleport") {
        return "Teleport nach (" + ev_num_str(ev_num_of(c, "x"), 1) + ", " +
               ev_num_str(ev_num_of(c, "z"), 1) + ")";
    }
    if (type == "log") return "Log: " + ev_str_of(c, "text");
    if (type == "sound") {
        return "Sound \"" + ev_str_of(c, "name") + "\" " +
               ev_num_str(ev_num_of(c, "volume", 1.0), 2);
    }
    if (type == "effect") {
        return "Effekt \"" + ev_str_of(c, "name", "burst") + "\"";
    }
    if (type == "script") {
        return "Script \"" + ev_str_of(c, "name") + "\"";
    }
    if (type == "item") {
        const JsonValue* take = c.find("take");
        if (take && take->is_num()) {
            return "Item \"" + ev_str_of(c, "name") + "\" -" + ev_num_str(take->num, 0);
        }
        return "Item \"" + ev_str_of(c, "name") + "\" +" +
               ev_num_str(ev_num_of(c, "count", 1.0), 0);
    }
    if (type == "gold") {
        const double amount = ev_num_of(c, "amount");
        const std::string sign = (amount >= 0) ? "+" : "";
        return "Gold " + sign + ev_num_str(amount, 0);
    }
    if (type == "heal") {
        const JsonValue* amt = c.find("amount");
        return amt ? "Heilen +" + ev_num_str(amt->num, 0) : "Heilen (voll)";
    }
    if (type == "choices") {
        const JsonValue* opts = c.find("options");
        std::string s = "Wahl: ";
        if (opts && opts->is_arr()) {
            for (size_t i = 0; i < opts->arr.size(); ++i) {
                if (i) s += " / ";
                s += opts->arr[i].str;
            }
        }
        return s;
    }
    if (type == "toggle_switch") {
        return "Schalter \"" + ev_str_of(c, "name") + "\" umschalten";
    }
    if (type == "move_player") {
        const bool rel = ev_bool_of(c, "relative");
        return std::string("Spieler bewegen ") + (rel ? "relativ (" : "nach (") +
               ev_num_str(ev_num_of(c, "x"), 1) + ", " +
               ev_num_str(ev_num_of(c, "z"), 1) + ")";
    }
    if (type == "weather") {
        return "Wetter \"" + ev_str_of(c, "name", "kein") + "\" (Stärke " +
               ev_num_str(ev_num_of(c, "intensity", 1.0), 2) + ")";
    }
    if (type == "save_game") return "Spiel speichern (Slot " + ev_num_str(ev_num_of(c, "slot", 1.0), 0) + ")";
    if (type == "load_game") return "Spiel laden (Slot " + ev_num_str(ev_num_of(c, "slot", 1.0), 0) + ")";
    return type;
}

// Befehlsbaum in die flache Liste bringen (Dann/Sonst als Kopfzeilen).
void ev_flatten(JsonValue& list, int depth) {
    for (int i = 0; i < static_cast<int>(list.arr.size()); ++i) {
        JsonValue& c = list.arr[i];
        EventCmdRef r;
        r.list = &list;
        r.index = i;
        r.depth = depth;
        r.label = ev_cmd_label(c);
        g_ev.refs.push_back(r);

        const std::string type = ev_str_of(c, "cmd");
        if (type == "choices") {
            // Wahlmöglichkeiten: pro Option ein Zweig (wie Dann/Sonst).
            JsonValue* opts = c.find("options");
            JsonValue* branches = c.find("branches");
            if (opts && opts->is_arr() && branches && branches->is_arr()) {
                for (size_t oi = 0; oi < opts->arr.size(); ++oi) {
                    if (oi >= branches->arr.size()) break;
                    JsonValue& br = branches->arr[oi];
                    if (!br.is_arr() || br.arr.empty()) continue;
                    EventCmdRef h;
                    h.list = &br;
                    h.index = -1;
                    h.depth = depth + 1;
                    h.header = true;
                    h.label = "Wenn [" + opts->arr[oi].str + "]:";
                    g_ev.refs.push_back(h);
                    ev_flatten(br, depth + 2);
                }
            }
            continue;
        }
        if (type != "if_switch" && type != "if_variable" &&
            type != "if_self_switch") continue;

        JsonValue* th = c.find("then");
        if (th && th->is_arr() && !th->arr.empty()) {
            EventCmdRef h;
            h.list = th;
            h.index = -1;
            h.depth = depth + 1;
            h.header = true;
            h.label = "Dann:";
            g_ev.refs.push_back(h);
            ev_flatten(*th, depth + 2);
        }
        JsonValue* el = c.find("else");
        if (el && el->is_arr() && !el->arr.empty()) {
            EventCmdRef h;
            h.list = el;
            h.index = -1;
            h.depth = depth + 1;
            h.header = true;
            h.label = "Sonst:";
            g_ev.refs.push_back(h);
            ev_flatten(*el, depth + 2);
        }
    }
}

void ev_refresh_events() {
    if (!g_ev.list_events) return;
    SendMessageW(g_ev.list_events, LB_RESETCONTENT, 0, 0);
    JsonValue* evs = ev_events();
    for (int i = 0; i < static_cast<int>(evs->arr.size()); ++i) {
        const JsonValue& e = evs->arr[i];
        const JsonValue* id = e.find("id");
        std::string id_s;
        if (id && id->is_num()) id_s = ev_num_str(id->num, 0);
        else if (id && id->is_str()) id_s = id->str;
        else id_s = "?";
        const std::string line = id_s + ". " + ev_str_of(e, "name", "Event") + "  (" +
                                 ev_num_str(ev_num_of(e, "x"), 1) + " / " +
                                 ev_num_str(ev_num_of(e, "z"), 1) + ")";
        const std::wstring w = utf8_to_wide(line);
        const int idx = static_cast<int>(
            SendMessageW(g_ev.list_events, LB_ADDSTRING, 0,
                         reinterpret_cast<LPARAM>(w.c_str())));
        SendMessageW(g_ev.list_events, LB_SETITEMDATA, idx,
                     static_cast<LPARAM>(static_cast<INT_PTR>(i)));
    }
    if (g_ev.sel_event >= 0 &&
        g_ev.sel_event < static_cast<int>(evs->arr.size())) {
        SendMessageW(g_ev.list_events, LB_SETCURSEL, g_ev.sel_event, 0);
    }
}

void ev_refresh_fields() {
    if (!g_ev.ed_name) return;
    JsonValue* evs = ev_events();
    if (g_ev.sel_event < 0 || g_ev.sel_event >= static_cast<int>(evs->arr.size())) {
        SetWindowTextW(g_ev.ed_id, L"");
        SetWindowTextW(g_ev.ed_name, L"");
        SetWindowTextW(g_ev.ed_x, L"");
        SetWindowTextW(g_ev.ed_z, L"");
        SetWindowTextW(g_ev.ed_radius, L"");
        EnableWindow(g_ev.ed_name, FALSE);
        EnableWindow(g_ev.ed_x, FALSE);
        EnableWindow(g_ev.ed_z, FALSE);
        EnableWindow(g_ev.ed_radius, FALSE);
        return;
    }
    EnableWindow(g_ev.ed_name, TRUE);
    EnableWindow(g_ev.ed_x, TRUE);
    EnableWindow(g_ev.ed_z, TRUE);
    EnableWindow(g_ev.ed_radius, TRUE);

    const JsonValue& e = evs->arr[g_ev.sel_event];
    const JsonValue* id = e.find("id");
    std::string id_s;
    if (id && id->is_num()) id_s = ev_num_str(id->num, 0);
    else if (id && id->is_str()) id_s = id->str;
    ev_set_text(g_ev.ed_id, id_s);
    ev_set_text(g_ev.ed_name, ev_str_of(e, "name", "Event"));
    ev_set_text(g_ev.ed_x, ev_num_str(ev_num_of(e, "x"), 2));
    ev_set_text(g_ev.ed_z, ev_num_str(ev_num_of(e, "z"), 2));
    ev_set_text(g_ev.ed_radius, ev_num_str(ev_num_of(e, "radius", 2.0), 2));
}

// Etappe 18g: Seitenliste füllen (Seite 1 = Standard, Bedingungen als Hinweis).
void ev_refresh_pages() {
    if (!g_ev.list_pages) return;
    SendMessageW(g_ev.list_pages, LB_RESETCONTENT, 0, 0);
    JsonValue* pages = ev_ensure_pages(g_ev.sel_event);
    if (!pages) {
        g_ev.sel_page = -1;
        return;
    }
    if (g_ev.sel_page < 0 || g_ev.sel_page >= static_cast<int>(pages->arr.size())) {
        g_ev.sel_page = 0;
    }
    for (int i = 0; i < static_cast<int>(pages->arr.size()); ++i) {
        const JsonValue& p = pages->arr[i];
        std::string line = "Seite " + std::to_string(i + 1);
        const JsonValue* cond = p.find("conditions");
        if (cond && cond->is_obj()) {
            const JsonValue* sw = cond->find("switch");
            const JsonValue* va = cond->find("variable");
            if (sw && sw->is_obj()) {
                line += "  [Schalter '" + ev_str_of(*sw, "name") + "' = " +
                        (ev_bool_of(*sw, "value") ? "an" : "aus") + "]";
            }
            if (va && va->is_obj()) {
                line += "  [Var '" + ev_str_of(*va, "name") + "' " +
                        ev_str_of(*va, "op", "==") + " " +
                        ev_num_str(ev_num_of(*va, "value"), 0) + "]";
            }
            if (!sw && !va) line += "  [immer]";
        } else {
            line += "  [immer]";
        }
        const std::wstring w = utf8_to_wide(line);
        const int idx = static_cast<int>(
            SendMessageW(g_ev.list_pages, LB_ADDSTRING, 0,
                         reinterpret_cast<LPARAM>(w.c_str())));
        SendMessageW(g_ev.list_pages, LB_SETITEMDATA, idx,
                     static_cast<LPARAM>(static_cast<INT_PTR>(i)));
    }
    SendMessageW(g_ev.list_pages, LB_SETCURSEL, g_ev.sel_page, 0);
}

// Etappe 18g: Trigger + Bedingungsfelder der aktuellen Seite laden.
void ev_refresh_page_fields() {
    if (!g_ev.cb_trigger) return;
    JsonValue* pg = ev_current_page();
    if (!pg) {
        SendMessageW(g_ev.cb_trigger, CB_SETCURSEL, static_cast<WPARAM>(-1), 0);
        ev_set_text(g_ev.ed_cswitch, "");
        ev_set_text(g_ev.ed_cvar, "");
        ev_set_text(g_ev.ed_cvval, "");
        SendMessageW(g_ev.cb_cswval, CB_SETCURSEL, 0, 0);
        SendMessageW(g_ev.cb_cvop, CB_SETCURSEL, 0, 0);
        SendMessageW(g_ev.cb_cself, CB_SETCURSEL, 0, 0);
        SendMessageW(g_ev.cb_cselfval, CB_SETCURSEL, 0, 0);
        EnableWindow(g_ev.cb_trigger, FALSE);
        EnableWindow(g_ev.ed_cswitch, FALSE);
        EnableWindow(g_ev.cb_cswval, FALSE);
        EnableWindow(g_ev.ed_cvar, FALSE);
        EnableWindow(g_ev.cb_cvop, FALSE);
        EnableWindow(g_ev.ed_cvval, FALSE);
        EnableWindow(g_ev.cb_cself, FALSE);
        EnableWindow(g_ev.cb_cselfval, FALSE);
        EnableWindow(GetDlgItem(g_ev.win, IDEV_APPLY_PAGE), FALSE);
        return;
    }
    EnableWindow(g_ev.cb_trigger, TRUE);
    EnableWindow(g_ev.ed_cswitch, TRUE);
    EnableWindow(g_ev.cb_cswval, TRUE);
    EnableWindow(g_ev.ed_cvar, TRUE);
    EnableWindow(g_ev.cb_cvop, TRUE);
    EnableWindow(g_ev.ed_cvval, TRUE);
    EnableWindow(g_ev.cb_cself, TRUE);
    EnableWindow(g_ev.cb_cselfval, TRUE);
    EnableWindow(GetDlgItem(g_ev.win, IDEV_APPLY_PAGE), TRUE);

    const std::string trig = ev_str_of(*pg, "trigger", "action");
    int ti = 0;
    for (int i = 0; i < 4; ++i) {
        if (trig == wide_to_utf8(EV_TRIGGERS[i])) ti = i;
    }
    SendMessageW(g_ev.cb_trigger, CB_SETCURSEL, ti, 0);

    // Bedingungen ins Formular laden (leer = keine Bedingung).
    ev_set_text(g_ev.ed_cswitch, "");
    ev_set_text(g_ev.ed_cvar, "");
    ev_set_text(g_ev.ed_cvval, "");
    SendMessageW(g_ev.cb_cswval, CB_SETCURSEL, 0, 0);
    SendMessageW(g_ev.cb_cvop, CB_SETCURSEL, 0, 0);
    SendMessageW(g_ev.cb_cself, CB_SETCURSEL, 0, 0);
    SendMessageW(g_ev.cb_cselfval, CB_SETCURSEL, 0, 0);
    const JsonValue* cond = pg->find("conditions");
    if (cond && cond->is_obj()) {
        const JsonValue* sw = cond->find("switch");
        if (sw && sw->is_obj()) {
            ev_set_text(g_ev.ed_cswitch, ev_str_of(*sw, "name"));
            SendMessageW(g_ev.cb_cswval, CB_SETCURSEL, ev_bool_of(*sw, "value") ? 0 : 1, 0);
        }
        const JsonValue* va = cond->find("variable");
        if (va && va->is_obj()) {
            ev_set_text(g_ev.ed_cvar, ev_str_of(*va, "name"));
            const std::string op = ev_str_of(*va, "op", "==");
            for (int i = 0; i < 6; ++i) {
                if (op == wide_to_utf8(EV_OPS[i])) SendMessageW(g_ev.cb_cvop, CB_SETCURSEL, i, 0);
            }
            ev_set_text(g_ev.ed_cvval, ev_num_str(ev_num_of(*va, "value"), 2));
        }
        const JsonValue* ss = cond->find("self_switch");
        if (ss && ss->is_obj()) {
            // Buchstabe A-D -> Combo-Index 1-4 (0 = "keins").
            const std::string letter = ev_str_of(*ss, "letter");
            if (!letter.empty() && letter[0] >= 'A' && letter[0] <= 'D') {
                SendMessageW(g_ev.cb_cself, CB_SETCURSEL,
                             static_cast<WPARAM>(letter[0] - 'A' + 1), 0);
            }
            SendMessageW(g_ev.cb_cselfval, CB_SETCURSEL,
                         ev_bool_of(*ss, "value") ? 0 : 1, 0);
        }
    }
}

// Formularfelder je Befehlstyp ein-/ausblenden und beschriften.
void ev_cmd_visibility() {
    const int t = static_cast<int>(SendMessageW(g_ev.cb_ce_type, CB_GETCURSEL, 0, 0));
    auto show = [](HWND h, bool v) {
        if (!h) return;
        ShowWindow(h, v ? SW_SHOW : SW_HIDE);
        EnableWindow(h, v ? TRUE : FALSE);
    };
    const bool is_text = (t == ET_TEXT || t == ET_LOG || t == ET_CHOICES ||
                         t == ET_MOVE_ROUTE);
    const bool has_name = (t == ET_SWITCH || t == ET_TOGGLE_SWITCH || t == ET_VARIABLE ||
                          t == ET_IF_SWITCH || t == ET_IF_VARIABLE || t == ET_SCRIPT ||
                          t == ET_SOUND || t == ET_EFFECT || t == ET_ITEM || t == ET_WEATHER ||
                          t == ET_SELF_SWITCH || t == ET_IF_SELF_SWITCH || t == ET_FADE ||
                          t == ET_MOVE_ROUTE || t == ET_INPUT_NUMBER || t == ET_BATTLE);
    const bool has_val1 = (t == ET_WAIT || t == ET_VARIABLE || t == ET_IF_VARIABLE ||
                          t == ET_TELEPORT || t == ET_MOVE_PLAYER || t == ET_SOUND ||
                          t == ET_EFFECT || t == ET_GOLD || t == ET_HEAL ||
                          t == ET_WEATHER || t == ET_SAVE_GAME || t == ET_LOAD_GAME ||
                          t == ET_FADE || t == ET_MOVE_ROUTE || t == ET_INPUT_NUMBER);
    const bool has_val2 = (t == ET_VARIABLE || t == ET_TELEPORT || t == ET_MOVE_PLAYER ||
                          t == ET_EFFECT || t == ET_ITEM || t == ET_MOVE_ROUTE ||
                          t == ET_INPUT_NUMBER || t == ET_BATTLE);
    const bool has_val3 = (t == ET_TELEPORT || t == ET_MOVE_ROUTE);
    const bool has_op = (t == ET_IF_VARIABLE);
    const bool has_bool = (t == ET_SWITCH || t == ET_IF_SWITCH ||
                          t == ET_SELF_SWITCH || t == ET_IF_SELF_SWITCH ||
                          t == ET_MOVE_ROUTE || t == ET_INPUT_NUMBER || t == ET_BATTLE);

    show(g_ev.lbl_name, has_name);
    show(g_ev.ed_ce_name, has_name);
    show(g_ev.lbl_val1, has_val1);
    show(g_ev.ed_ce_val1, has_val1);
    show(g_ev.lbl_val2, has_val2);
    show(g_ev.ed_ce_val2, has_val2);
    show(g_ev.lbl_val3, has_val3);
    show(g_ev.ed_ce_val3, has_val3);
    show(g_ev.cb_ce_op, has_op);
    show(g_ev.cb_ce_bool, has_bool);
    show(g_ev.lbl_text, is_text);
    show(g_ev.ed_ce_text, is_text);

    SetWindowTextW(g_ev.lbl_name, L"Name");
    SetWindowTextW(g_ev.lbl_val1, L"Wert");
    SetWindowTextW(g_ev.lbl_val2, L"Add");
    SetWindowTextW(g_ev.lbl_val3, L"Yaw (Grad)");
    SetWindowTextW(g_ev.lbl_text, L"Zeilen (eine pro Zeile)");

    if (t == ET_SWITCH || t == ET_TOGGLE_SWITCH || t == ET_IF_SWITCH)
        SetWindowTextW(g_ev.lbl_name, L"Schalter");
    if (t == ET_SELF_SWITCH || t == ET_IF_SELF_SWITCH)
        SetWindowTextW(g_ev.lbl_name, L"Self-Switch (A-D)");
    if (t == ET_FADE) { SetWindowTextW(g_ev.lbl_name, L"Richtung (aus = dunkel, ein = hell)");
                  SetWindowTextW(g_ev.lbl_val1, L"Sekunden"); }
    if (t == ET_MOVE_ROUTE) {
        SetWindowTextW(g_ev.lbl_name, L"Ziel (NPC-Name oder 'spieler')");
        SetWindowTextW(g_ev.lbl_val1, L"Tempo m/s (leer = Standard)");
        SetWindowTextW(g_ev.lbl_val2, L"Warten bis fertig (1 = ja)");
        // Etappe 19h.2: Schritte = XP-Vokabular (move_up, turn_left, wait,
        // XP-Codes 11..57, deutsche Kurznamen gehen weiter) - mover.rb-Kopf.
        SetWindowTextW(g_ev.lbl_val3, L"Code-Parameter (switch_on/change_speed/script)");
        SetWindowTextW(g_ev.lbl_text, L"Schritte, einer pro Zeile (an/aus = wiederholen)");
    }
    if (t == ET_INPUT_NUMBER) {
        // Etappe 19i: XP "Input Number Processing".
        SetWindowTextW(g_ev.lbl_name, L"Variable");
        SetWindowTextW(g_ev.lbl_val1, L"Ziffern (1-6)");
        SetWindowTextW(g_ev.lbl_val2, L"Startwert");
        SetWindowTextW(g_ev.lbl_text, L"Zahleneingabe: an/aus = Abbrechen erlaubt (Esc -> 0)");
    }
    if (t == ET_BATTLE) {
        SetWindowTextW(g_ev.lbl_name, L"Gegnergruppe (troops-ID aus database.json)");
        SetWindowTextW(g_ev.lbl_val2, L"Niederlage ohne Game Over (1 = ja)");
        SetWindowTextW(g_ev.lbl_text, L"Kampf: an/aus = Flucht möglich (Standard an)");
    }
    if (t == ET_VARIABLE) { SetWindowTextW(g_ev.lbl_name, L"Variable");
                  SetWindowTextW(g_ev.lbl_val1, L"Wert (leer = lassen)");
                  SetWindowTextW(g_ev.lbl_val2, L"Add (optional)"); }
    if (t == ET_IF_VARIABLE) { SetWindowTextW(g_ev.lbl_name, L"Variable");
                  SetWindowTextW(g_ev.lbl_val1, L"Wert"); }
    if (t == ET_SOUND) { SetWindowTextW(g_ev.lbl_name, L"Sound-Name");
                  SetWindowTextW(g_ev.lbl_val1, L"Lautstärke"); }
    if (t == ET_EFFECT) { SetWindowTextW(g_ev.lbl_name, L"Effekt (sparkle/dust/burst)");
                  SetWindowTextW(g_ev.lbl_val1, L"Höhe");
                  SetWindowTextW(g_ev.lbl_val2, L"Farbe (hex)"); }
    if (t == ET_SCRIPT) SetWindowTextW(g_ev.lbl_name, L"Script-Name");
    if (t == ET_ITEM) { SetWindowTextW(g_ev.lbl_name, L"Item-ID");
                   SetWindowTextW(g_ev.lbl_val2, L"Menge (negativ = nehmen)"); }
    if (t == ET_GOLD) SetWindowTextW(g_ev.lbl_val1, L"Gold +/- (z.B. 50 oder -20)");
    if (t == ET_HEAL) SetWindowTextW(g_ev.lbl_val1, L"HP (leer = voll heilen)");
    if (t == ET_TELEPORT) { SetWindowTextW(g_ev.lbl_val1, L"X");
                  SetWindowTextW(g_ev.lbl_val2, L"Z"); }
    if (t == ET_MOVE_PLAYER) { SetWindowTextW(g_ev.lbl_val1, L"X");
                  SetWindowTextW(g_ev.lbl_val2, L"Z (relativ, wenn befüllt)"); }
    if (t == ET_WEATHER) { SetWindowTextW(g_ev.lbl_name, L"Wetter (kein/regen/schnee)");
                  SetWindowTextW(g_ev.lbl_val1, L"Intensität"); }
    if (t == ET_SAVE_GAME || t == ET_LOAD_GAME)
        SetWindowTextW(g_ev.lbl_val1, L"Slot (1-3)");
    if (t == ET_WAIT) SetWindowTextW(g_ev.lbl_val1, L"Sekunden");
    if (t == ET_LOG) SetWindowTextW(g_ev.lbl_text, L"Log-Text");
    if (t == ET_CHOICES) SetWindowTextW(g_ev.lbl_text, L"Optionen (eine pro Zeile)");
}

void ev_clear_cmd_form() {
    SetWindowTextW(g_ev.ed_ce_name, L"");
    SetWindowTextW(g_ev.ed_ce_val1, L"");
    SetWindowTextW(g_ev.ed_ce_val2, L"");
    SetWindowTextW(g_ev.ed_ce_val3, L"");
    SetWindowTextW(g_ev.ed_ce_text, L"");
    SendMessageW(g_ev.cb_ce_op, CB_SETCURSEL, 0, 0);
    SendMessageW(g_ev.cb_ce_bool, CB_SETCURSEL, 0, 0);
}

void ev_load_cmd_form() {
    if (!g_ev.cb_ce_type) return;
    g_ev.syncing = true;
    if (g_ev.sel_cmd < 0 || g_ev.sel_cmd >= static_cast<int>(g_ev.refs.size()) ||
        g_ev.refs[g_ev.sel_cmd].header) {
        ev_clear_cmd_form();
        SendMessageW(g_ev.cb_ce_type, CB_SETCURSEL, 0, 0);
        ev_cmd_visibility();
        EnableWindow(GetDlgItem(g_ev.win, IDEV_APPLYCMD), FALSE);
        EnableWindow(GetDlgItem(g_ev.win, IDEV_DELCMD), FALSE);
        EnableWindow(GetDlgItem(g_ev.win, IDEV_UP), FALSE);
        EnableWindow(GetDlgItem(g_ev.win, IDEV_DOWN), FALSE);
        g_ev.syncing = false;
        return;
    }
    EnableWindow(GetDlgItem(g_ev.win, IDEV_APPLYCMD), TRUE);
    EnableWindow(GetDlgItem(g_ev.win, IDEV_DELCMD), TRUE);
    EnableWindow(GetDlgItem(g_ev.win, IDEV_UP), TRUE);
    EnableWindow(GetDlgItem(g_ev.win, IDEV_DOWN), TRUE);

    const EventCmdRef& ref = g_ev.refs[g_ev.sel_cmd];
    const JsonValue& c = ref.list->arr[ref.index];
    const std::string type = ev_str_of(c, "cmd");
    int ti = 0;
    for (int i = 0; i < ET_COUNT; ++i) {
        if (type == EV_TYPES[i]) ti = i;
    }
    ev_clear_cmd_form();
    SendMessageW(g_ev.cb_ce_type, CB_SETCURSEL, ti, 0);

    ev_set_text(g_ev.ed_ce_name, ev_str_of(c, "name"));
    if (type == "text") {
        ev_set_text(g_ev.ed_ce_text, ev_join_lines(c));
    } else if (type == "choices") {
        const JsonValue* opts = c.find("options");
        std::string joined;
        if (opts && opts->is_arr()) {
            for (size_t i = 0; i < opts->arr.size(); ++i) {
                if (i) joined += "\r\n";
                joined += opts->arr[i].str;
            }
        }
        ev_set_text(g_ev.ed_ce_text, joined);
    } else if (type == "log") {
        ev_set_text(g_ev.ed_ce_text, ev_str_of(c, "text"));
    } else if (type == "wait") {
        ev_set_text(g_ev.ed_ce_val1, ev_num_str(ev_num_of(c, "seconds"), 2));
    } else if (type == "switch" || type == "if_switch") {
        SendMessageW(g_ev.cb_ce_bool, CB_SETCURSEL, ev_bool_of(c, "value") ? 0 : 1, 0);
    } else if (type == "self_switch" || type == "if_self_switch") {
        // Etappe 19d: Buchstabe im Namensfeld, an/aus im Bool-Combo.
        ev_set_text(g_ev.ed_ce_name, ev_str_of(c, "letter"));
        SendMessageW(g_ev.cb_ce_bool, CB_SETCURSEL, ev_bool_of(c, "value") ? 0 : 1, 0);
    } else if (type == "fade") {
        ev_set_text(g_ev.ed_ce_name, ev_str_of(c, "direction", "aus"));
        ev_set_text(g_ev.ed_ce_val1, ev_num_str(ev_num_of(c, "seconds"), 2));
    } else if (type == "move_route") {
        // Etappe 19g: Schritte als Zeilenliste, wie bei text/choices.
        ev_set_text(g_ev.ed_ce_name, ev_str_of(c, "target"));
        const JsonValue* steps = c.find("steps");
        std::string joined;
        if (steps && steps->is_arr()) {
            for (size_t i = 0; i < steps->arr.size(); ++i) {
                if (i) joined += "\r\n";
                joined += steps->arr[i].str;
            }
        }
        ev_set_text(g_ev.ed_ce_text, joined);
        if (c.find("speed"))
            ev_set_text(g_ev.ed_ce_val1, ev_num_str(ev_num_of(c, "speed"), 2));
        if (ev_bool_of(c, "wait")) ev_set_text(g_ev.ed_ce_val2, "1");
        if (c.find("code")) ev_set_text(g_ev.ed_ce_val3, ev_str_of(c, "code"));
        SendMessageW(g_ev.cb_ce_bool, CB_SETCURSEL, ev_bool_of(c, "repeat") ? 0 : 1, 0);
    } else if (type == "input_number") {
        // Etappe 19i: Variable + Ziffern + Startwert; an/aus = Abbruch erlaubt.
        if (c.find("digits"))
            ev_set_text(g_ev.ed_ce_val1, ev_num_str(ev_num_of(c, "digits", 1.0), 0));
        if (c.find("initial"))
            ev_set_text(g_ev.ed_ce_val2, ev_num_str(ev_num_of(c, "initial", 0.0), 0));
        SendMessageW(g_ev.cb_ce_bool, CB_SETCURSEL,
                     ev_bool_of(c, "cancel", true) ? 0 : 1, 0);
    } else if (type == "battle") {
        // Etappe 19i: Truppe im Namensfeld; an/aus = Flucht, Wert 2 = can_lose.
        if (ev_bool_of(c, "can_lose")) ev_set_text(g_ev.ed_ce_val2, "1");
        SendMessageW(g_ev.cb_ce_bool, CB_SETCURSEL,
                     ev_bool_of(c, "can_escape", true) ? 0 : 1, 0);
    } else if (type == "variable") {
        if (c.find("value")) ev_set_text(g_ev.ed_ce_val1, ev_num_str(ev_num_of(c, "value"), 2));
        if (c.find("add"))   ev_set_text(g_ev.ed_ce_val2, ev_num_str(ev_num_of(c, "add"), 2));
    } else if (type == "if_variable") {
        const std::string op = ev_str_of(c, "op", "==");
        for (int i = 0; i < 6; ++i) {
            if (op == wide_to_utf8(EV_OPS[i])) SendMessageW(g_ev.cb_ce_op, CB_SETCURSEL, i, 0);
        }
        ev_set_text(g_ev.ed_ce_val1, ev_num_str(ev_num_of(c, "value"), 2));
    } else if (type == "teleport") {
        ev_set_text(g_ev.ed_ce_val1, ev_num_str(ev_num_of(c, "x"), 2));
        ev_set_text(g_ev.ed_ce_val2, ev_num_str(ev_num_of(c, "z"), 2));
        ev_set_text(g_ev.ed_ce_val3, ev_num_str(ev_num_of(c, "yaw", 3.14159265), 3));
    } else if (type == "sound") {
        ev_set_text(g_ev.ed_ce_val1, ev_num_str(ev_num_of(c, "volume", 1.0), 2));
    } else if (type == "effect") {
        ev_set_text(g_ev.ed_ce_val1, ev_num_str(ev_num_of(c, "height", 1.0), 2));
        char col[16];
        std::snprintf(col, sizeof(col), "%06X",
                      static_cast<uint32_t>(ev_num_of(c, "color", 0xFFD866)));
        ev_set_text(g_ev.ed_ce_val2, col);
    } else if (type == "item") {
        const JsonValue* take = c.find("take");
        const JsonValue* count = c.find("count");
        if (count) ev_set_text(g_ev.ed_ce_val2, ev_num_str(count->num, 0));
        if (take)  ev_set_text(g_ev.ed_ce_val2, ev_num_str(-take->num, 0));
    } else if (type == "gold") {
        ev_set_text(g_ev.ed_ce_val1, ev_num_str(ev_num_of(c, "amount"), 0));
    } else if (type == "heal") {
        if (c.find("amount")) ev_set_text(g_ev.ed_ce_val1, ev_num_str(ev_num_of(c, "amount"), 0));
    } else if (type == "move_player") {
        ev_set_text(g_ev.ed_ce_val1, ev_num_str(ev_num_of(c, "x"), 2));
        ev_set_text(g_ev.ed_ce_val2, ev_num_str(ev_num_of(c, "z"), 2));
    } else if (type == "weather") {
        ev_set_text(g_ev.ed_ce_val1, ev_num_str(ev_num_of(c, "intensity", 1.0), 2));
    } else if (type == "save_game" || type == "load_game") {
        ev_set_text(g_ev.ed_ce_val1, ev_num_str(ev_num_of(c, "slot", 1.0), 0));
    }
    ev_cmd_visibility();
    g_ev.syncing = false;
}

void ev_refresh_cmds() {
    if (!g_ev.list_cmds) return;
    SendMessageW(g_ev.list_cmds, LB_RESETCONTENT, 0, 0);
    g_ev.refs.clear();
    JsonValue* cmds = ev_commands(g_ev.sel_event);
    if (cmds) ev_flatten(*cmds, 0);
    for (int i = 0; i < static_cast<int>(g_ev.refs.size()); ++i) {
        const EventCmdRef& r = g_ev.refs[i];
        std::string text(static_cast<size_t>(r.depth) * 2, ' ');
        if (r.header) text += "-> ";
        text += r.label;
        const std::wstring w = utf8_to_wide(text);
        const int idx = static_cast<int>(
            SendMessageW(g_ev.list_cmds, LB_ADDSTRING, 0,
                         reinterpret_cast<LPARAM>(w.c_str())));
        SendMessageW(g_ev.list_cmds, LB_SETITEMDATA, idx,
                     static_cast<LPARAM>(static_cast<INT_PTR>(i)));
    }
    if (g_ev.sel_cmd >= 0 && g_ev.sel_cmd < static_cast<int>(g_ev.refs.size())) {
        SendMessageW(g_ev.list_cmds, LB_SETCURSEL, g_ev.sel_cmd, 0);
    } else {
        g_ev.sel_cmd = -1;
    }

    // Ziel für neue Befehle anzeigen.
    std::wstring target = L"Ziel: Wurzel";
    if (g_ev.sel_cmd >= 0 && g_ev.sel_cmd < static_cast<int>(g_ev.refs.size())) {
        const EventCmdRef& r = g_ev.refs[g_ev.sel_cmd];
        JsonValue* root = ev_commands(g_ev.sel_event);
        if (r.header) {
            target = L"Ziel: " + utf8_to_wide(r.label);
            // "Dann:" / "Sonst:" / "Wenn [x]:" ohne Doppelpunkt hinten
            if (!target.empty() && target.back() == L':') target.pop_back();
            target += L"-Zweig";
        } else if (r.list != root) {
            target = L"Ziel: Zweig-Ebene";
        }
    }
    SetWindowTextW(g_ev.lbl_insert, target.c_str());
}

void ev_refresh() {
    g_ev.syncing = true;
    ev_refresh_events();
    ev_refresh_fields();
    ev_refresh_pages();      // Etappe 18g
    ev_refresh_page_fields(); // Etappe 18g
    ev_refresh_cmds();
    ev_load_cmd_form();
    g_ev.syncing = false;
}

// Event-Felder aus dem Formular ins JSON schreiben.
void ev_apply_event_fields(bool mark_dirty) {
    JsonValue* evs = ev_events();
    if (g_ev.sel_event < 0 || g_ev.sel_event >= static_cast<int>(evs->arr.size())) return;
    JsonValue& e = evs->arr[g_ev.sel_event];
    e.set("name", JsonValue::make_str(ev_text(g_ev.ed_name)));
    e.set("x", JsonValue::make_num(ev_number(g_ev.ed_x, 0.0), false));
    e.set("z", JsonValue::make_num(ev_number(g_ev.ed_z, 0.0), false));
    e.set("radius", JsonValue::make_num(ev_number(g_ev.ed_radius, 2.0), false));
    // Trigger steht seit 18g pro Seite (ev_apply_page_fields).
    if (mark_dirty) g.dirty = true;
    ev_refresh_events();
    if (mark_dirty) update_title();
}

// Etappe 18g: Trigger + Bedingungen der aktuellen Seite aus dem Formular
// ins JSON schreiben. Leere Bedingungsfelder entfernen die Bedingung.
void ev_apply_page_fields(bool mark_dirty) {
    JsonValue* pg = ev_current_page();
    if (!pg) return;
    std::string trig = ev_text(g_ev.cb_trigger);
    if (trig.empty()) trig = "action";
    pg->set("trigger", JsonValue::make_str(trig));

    const std::string sw_name = ev_text(g_ev.ed_cswitch);
    const std::string va_name = ev_text(g_ev.ed_cvar);
    // Etappe 19d: Self-Switch-Auswahl (Index 0 = "keins" = Bedingung aus).
    const int self_sel =
        static_cast<int>(SendMessageW(g_ev.cb_cself, CB_GETCURSEL, 0, 0));
    const bool has_self = (self_sel >= 1 && self_sel <= 4);
    if (sw_name.empty() && va_name.empty() && !has_self) {
        json_remove_key(*pg, "conditions");
    } else {
        JsonValue cond = pg->find("conditions") && pg->find("conditions")->is_obj()
                             ? *pg->find("conditions") : JsonValue::make_obj();
        if (sw_name.empty()) {
            json_remove_key(cond, "switch");
        } else {
            JsonValue sw = JsonValue::make_obj();
            sw.set("name", JsonValue::make_str(sw_name));
            const int b = static_cast<int>(SendMessageW(g_ev.cb_cswval, CB_GETCURSEL, 0, 0));
            sw.set("value", JsonValue::make_bool(b == 0));
            cond.set("switch", std::move(sw));
        }
        if (va_name.empty()) {
            json_remove_key(cond, "variable");
        } else {
            JsonValue va = JsonValue::make_obj();
            va.set("name", JsonValue::make_str(va_name));
            const int op = static_cast<int>(SendMessageW(g_ev.cb_cvop, CB_GETCURSEL, 0, 0));
            va.set("op", JsonValue::make_str(wide_to_utf8(EV_OPS[(op >= 0 && op < 6) ? op : 0])));
            va.set("value", JsonValue::make_num(ev_number(g_ev.ed_cvval, 0.0), false));
            cond.set("variable", std::move(va));
        }
        if (!has_self) {
            json_remove_key(cond, "self_switch");
        } else {
            JsonValue ss = JsonValue::make_obj();
            const char letter = static_cast<char>('A' + self_sel - 1);
            ss.set("letter", JsonValue::make_str(std::string(1, letter)));
            const int b =
                static_cast<int>(SendMessageW(g_ev.cb_cselfval, CB_GETCURSEL, 0, 0));
            ss.set("value", JsonValue::make_bool(b == 0));
            cond.set("self_switch", std::move(ss));
        }
        pg->set("conditions", std::move(cond));
    }
    if (mark_dirty) g.dirty = true;
    ev_refresh_pages();
    if (mark_dirty) update_title();
}

// Etappe 18g: neue (leere) Seite ans Event hängen.
void ev_new_page() {
    JsonValue* pages = ev_ensure_pages(g_ev.sel_event);
    if (!pages) return;
    JsonValue pg = JsonValue::make_obj();
    pg.set("conditions", JsonValue::make_obj());
    pg.set("trigger", JsonValue::make_str("action"));
    pg.set("commands", JsonValue::make_arr());
    pages->arr.push_back(std::move(pg));
    g_ev.sel_page = static_cast<int>(pages->arr.size()) - 1;
    g_ev.sel_cmd = -1;
    g.dirty = true;
    ev_refresh_pages();
    ev_refresh_page_fields();
    ev_refresh_cmds();
    ev_load_cmd_form();
    update_title();
}

// Etappe 18g: gewählte Seite löschen (die letzte bleibt).
void ev_delete_page() {
    JsonValue* pages = ev_ensure_pages(g_ev.sel_event);
    if (!pages) return;
    if (pages->arr.size() <= 1) {
        core::log_warn("Event-Editor: die letzte Seite kann nicht gelöscht werden");
        return;
    }
    if (g_ev.sel_page < 0 || g_ev.sel_page >= static_cast<int>(pages->arr.size())) return;
    pages->arr.erase(pages->arr.begin() + g_ev.sel_page);
    if (g_ev.sel_page >= static_cast<int>(pages->arr.size())) {
        g_ev.sel_page = static_cast<int>(pages->arr.size()) - 1;
    }
    g_ev.sel_cmd = -1;
    g.dirty = true;
    ev_refresh_pages();
    ev_refresh_page_fields();
    ev_refresh_cmds();
    ev_load_cmd_form();
    update_title();
}

void ev_new_event() {
    JsonValue* evs = ev_events();
    int max_id = 0;
    for (const JsonValue& e : evs->arr) {
        const JsonValue* id = e.find("id");
        if (id && id->is_num()) max_id = std::max(max_id, static_cast<int>(id->num));
    }
    const int id = max_id + 1;
    JsonValue e = JsonValue::make_obj();
    e.set("id", JsonValue::make_num(id, true));
    e.set("name", JsonValue::make_str("Event " + std::to_string(id)));
    const float x = g.hover_valid ? g.hover_pos.x : 0.0f;
    const float z = g.hover_valid ? g.hover_pos.z : 0.0f;
    e.set("x", JsonValue::make_num(std::round(x / g.grid) * g.grid, false));
    e.set("z", JsonValue::make_num(std::round(z / g.grid) * g.grid, false));
    e.set("radius", JsonValue::make_num(2.0, false));
    // Etappe 18g: neue Events starten mit einer Standard-Seite.
    JsonValue pages = JsonValue::make_arr();
    JsonValue pg = JsonValue::make_obj();
    pg.set("conditions", JsonValue::make_obj());
    pg.set("trigger", JsonValue::make_str("action"));
    pg.set("commands", JsonValue::make_arr());
    pages.arr.push_back(std::move(pg));
    e.set("pages", std::move(pages));
    evs->arr.push_back(std::move(e));
    g_ev.sel_event = static_cast<int>(evs->arr.size()) - 1;
    g_ev.sel_page = 0;
    g_ev.sel_cmd = -1;
    g.dirty = true;
    ev_refresh();
    update_title();
}

void ev_delete_event() {
    JsonValue* evs = ev_events();
    if (g_ev.sel_event < 0 || g_ev.sel_event >= static_cast<int>(evs->arr.size())) return;
    evs->arr.erase(evs->arr.begin() + g_ev.sel_event);
    if (g_ev.sel_event >= static_cast<int>(evs->arr.size())) {
        g_ev.sel_event = static_cast<int>(evs->arr.size()) - 1;
    }
    g_ev.sel_page = 0;
    g_ev.sel_cmd = -1;
    g.dirty = true;
    ev_refresh();
    update_title();
}

void ev_duplicate_event() {
    JsonValue* evs = ev_events();
    if (g_ev.sel_event < 0 || g_ev.sel_event >= static_cast<int>(evs->arr.size())) return;
    ev_apply_event_fields(true);
    int max_id = 0;
    for (const JsonValue& e : evs->arr) {
        const JsonValue* id = e.find("id");
        if (id && id->is_num()) max_id = std::max(max_id, static_cast<int>(id->num));
    }
    JsonValue copy = evs->arr[g_ev.sel_event];
    copy.set("id", JsonValue::make_num(max_id + 1, true));
    copy.set("name", JsonValue::make_str(ev_str_of(copy, "name", "Event") + " (Kopie)"));
    JsonValue* x = copy.find("x");
    JsonValue* z = copy.find("z");
    if (x) x->num += 1.0;
    if (z) z->num += 1.0;
    evs->arr.push_back(std::move(copy));
    g_ev.sel_event = static_cast<int>(evs->arr.size()) - 1;
    g_ev.sel_page = 0;
    g_ev.sel_cmd = -1;
    g.dirty = true;
    ev_refresh();
    update_title();
}

// Befehl aus dem Formular bauen. existing = alter Befehl (für Dann/Sonst).
JsonValue ev_build_command(const JsonValue* existing) {
    const int t = static_cast<int>(SendMessageW(g_ev.cb_ce_type, CB_GETCURSEL, 0, 0));
    const int ti = (t >= 0 && t < ET_COUNT) ? t : 0;
    JsonValue cmd = JsonValue::make_obj();
    cmd.set("cmd", JsonValue::make_str(EV_TYPES[ti]));

    if (ti == ET_TEXT) {
        const std::vector<std::string> lines = ev_split_lines(ev_text(g_ev.ed_ce_text));
        JsonValue arr = JsonValue::make_arr();
        for (const std::string& line : lines) arr.arr.push_back(JsonValue::make_str(line));
        cmd.set("lines", arr);
    } else if (ti == ET_CHOICES) {
        // Wahlmöglichkeiten: Optionen aus den Zeilen; Zweige aus dem alten
        // Befehl übernehmen (sonst leere Arrays für jede Option).
        const std::vector<std::string> lines = ev_split_lines(ev_text(g_ev.ed_ce_text));
        JsonValue opts = JsonValue::make_arr();
        JsonValue branches = JsonValue::make_arr();
        const JsonValue* old_branches = existing ? existing->find("branches") : nullptr;
        for (size_t i = 0; i < lines.size(); ++i) {
            if (lines[i].empty()) continue;
            opts.arr.push_back(JsonValue::make_str(lines[i]));
            if (old_branches && old_branches->is_arr() &&
                branches.arr.size() < old_branches->arr.size()) {
                branches.arr.push_back(old_branches->arr[branches.arr.size()]);
            } else {
                branches.arr.push_back(JsonValue::make_arr());
            }
        }
        cmd.set("options", opts);
        cmd.set("branches", branches);
    } else if (ti == ET_WAIT) {
        cmd.set("seconds", JsonValue::make_num(ev_number(g_ev.ed_ce_val1, 1.0), false));
    } else if (ti == ET_SWITCH || ti == ET_IF_SWITCH) {
        cmd.set("name", JsonValue::make_str(ev_text(g_ev.ed_ce_name)));
        const int b = static_cast<int>(SendMessageW(g_ev.cb_ce_bool, CB_GETCURSEL, 0, 0));
        cmd.set("value", JsonValue::make_bool(b == 0));
    } else if (ti == ET_SELF_SWITCH || ti == ET_IF_SELF_SWITCH) {
        // Etappe 19d: Buchstabe A-D; klein wird groß, Ungültiges/Leeres -> A
        // (wie GameState.set_self_switch bzw. der Interpreter es erwarten).
        std::string letter = ev_text(g_ev.ed_ce_name);
        char c0 = letter.empty() ? 'A'
                                 : static_cast<char>(std::toupper(
                                       static_cast<unsigned char>(letter[0])));
        if (c0 < 'A' || c0 > 'D') c0 = 'A';
        cmd.set("letter", JsonValue::make_str(std::string(1, c0)));
        const int b = static_cast<int>(SendMessageW(g_ev.cb_ce_bool, CB_GETCURSEL, 0, 0));
        cmd.set("value", JsonValue::make_bool(b == 0));
    } else if (ti == ET_FADE) {
        std::string dir = ev_text(g_ev.ed_ce_name);
        std::transform(dir.begin(), dir.end(), dir.begin(),
                       [](unsigned char ch) {
                           return static_cast<char>(std::tolower(ch));
                       });
        cmd.set("direction", JsonValue::make_str(dir == "ein" ? "ein" : "aus"));
        cmd.set("seconds", JsonValue::make_num(ev_number(g_ev.ed_ce_val1, 0.5), false));
    } else if (ti == ET_MOVE_ROUTE) {
        // Etappe 19g: Ziel + Schritte (Zeilen); Tempo/Warten optional,
        // "an/aus"-Combo = wiederholen (Default "an" wie im XP-Dialog).
        cmd.set("target", JsonValue::make_str(ev_text(g_ev.ed_ce_name)));
        const std::vector<std::string> lines = ev_split_lines(ev_text(g_ev.ed_ce_text));
        JsonValue steps = JsonValue::make_arr();
        for (const std::string& line : lines) {
            if (line.empty()) continue;
            steps.arr.push_back(JsonValue::make_str(line));
        }
        cmd.set("steps", steps);
        if (!ev_text(g_ev.ed_ce_val1).empty()) {
            const double sp = ev_number(g_ev.ed_ce_val1, 0.0);
            if (sp > 0) cmd.set("speed", JsonValue::make_num(sp, false));
        }
        if (ev_number(g_ev.ed_ce_val2, 0.0) != 0.0) {
            cmd.set("wait", JsonValue::make_bool(true));
        }
        if (!ev_text(g_ev.ed_ce_val3).empty()) {
            cmd.set("code", JsonValue::make_str(ev_text(g_ev.ed_ce_val3)));
        }
        const int b = static_cast<int>(SendMessageW(g_ev.cb_ce_bool, CB_GETCURSEL, 0, 0));
        if (b == 0) cmd.set("repeat", JsonValue::make_bool(true));
    } else if (ti == ET_INPUT_NUMBER) {
        // Etappe 19i: Variable (Namensfeld), Ziffern/Startwert, an/aus = cancel.
        cmd.set("name", JsonValue::make_str(ev_text(g_ev.ed_ce_name)));
        const int digits = static_cast<int>(ev_number(g_ev.ed_ce_val1, 1.0));
        cmd.set("digits", JsonValue::make_num(digits < 1 ? 1 : (digits > 6 ? 6 : digits), false));
        const int initial = static_cast<int>(ev_number(g_ev.ed_ce_val2, 0.0));
        if (initial > 0) cmd.set("initial", JsonValue::make_num(initial, false));
        const int cb = static_cast<int>(SendMessageW(g_ev.cb_ce_bool, CB_GETCURSEL, 0, 0));
        if (cb != 0) cmd.set("cancel", JsonValue::make_bool(false));
    } else if (ti == ET_BATTLE) {
        // Etappe 19i: Truppe (Namensfeld), an/aus = Flucht möglich,
        // Wert 2 = Niederlage ohne Game Over.
        cmd.set("name", JsonValue::make_str(ev_text(g_ev.ed_ce_name)));
        const int cb = static_cast<int>(SendMessageW(g_ev.cb_ce_bool, CB_GETCURSEL, 0, 0));
        if (cb != 0) cmd.set("can_escape", JsonValue::make_bool(false));
        if (ev_number(g_ev.ed_ce_val2, 0.0) != 0.0)
            cmd.set("can_lose", JsonValue::make_bool(true));
    } else if (ti == ET_TOGGLE_SWITCH) {
        cmd.set("name", JsonValue::make_str(ev_text(g_ev.ed_ce_name)));
    } else if (ti == ET_VARIABLE) {
        cmd.set("name", JsonValue::make_str(ev_text(g_ev.ed_ce_name)));
        const std::string v1 = ev_text(g_ev.ed_ce_val1);
        const std::string v2 = ev_text(g_ev.ed_ce_val2);
        if (!v1.empty() || v2.empty()) {
            cmd.set("value", JsonValue::make_num(ev_number(g_ev.ed_ce_val1, 0.0), false));
        }
        if (!v2.empty()) {
            std::string add = v2;
            std::replace(add.begin(), add.end(), ',', '.');
            cmd.set("add", JsonValue::make_num(std::atof(add.c_str()), false));
        }
    } else if (ti == ET_IF_VARIABLE) {
        cmd.set("name", JsonValue::make_str(ev_text(g_ev.ed_ce_name)));
        const int op = static_cast<int>(SendMessageW(g_ev.cb_ce_op, CB_GETCURSEL, 0, 0));
        cmd.set("op", JsonValue::make_str(wide_to_utf8(EV_OPS[(op >= 0 && op < 6) ? op : 0])));
        cmd.set("value", JsonValue::make_num(ev_number(g_ev.ed_ce_val1, 0.0), false));
    } else if (ti == ET_TELEPORT) {
        cmd.set("x", JsonValue::make_num(ev_number(g_ev.ed_ce_val1, 0.0), false));
        cmd.set("z", JsonValue::make_num(ev_number(g_ev.ed_ce_val2, 0.0), false));
        const std::string yaw = ev_text(g_ev.ed_ce_val3);
        cmd.set("yaw", JsonValue::make_num(yaw.empty() ? 3.14159265
                                                     : ev_number(g_ev.ed_ce_val3, 3.14159265),
                                          false));
    } else if (ti == ET_MOVE_PLAYER) {
        cmd.set("x", JsonValue::make_num(ev_number(g_ev.ed_ce_val1, 0.0), false));
        cmd.set("z", JsonValue::make_num(ev_number(g_ev.ed_ce_val2, 0.0), false));
        cmd.set("relative", JsonValue::make_bool(true));   // Standard: relativ
    } else if (ti == ET_LOG) {
        cmd.set("text", JsonValue::make_str(ev_text(g_ev.ed_ce_text)));
    } else if (ti == ET_SOUND) {
        cmd.set("name", JsonValue::make_str(ev_text(g_ev.ed_ce_name)));
        cmd.set("volume", JsonValue::make_num(ev_number(g_ev.ed_ce_val1, 1.0), false));
    } else if (ti == ET_EFFECT) {
        cmd.set("name", JsonValue::make_str(ev_text(g_ev.ed_ce_name)));
        cmd.set("height", JsonValue::make_num(ev_number(g_ev.ed_ce_val1, 1.0), false));
        const uint32_t parsed = parse_hex_color(ev_text(g_ev.ed_ce_val2));
        cmd.set("color", JsonValue::make_num(
            static_cast<double>(parsed ? parsed : 0xFFD866), true));
    } else if (ti == ET_SCRIPT) {
        cmd.set("name", JsonValue::make_str(ev_text(g_ev.ed_ce_name)));
    } else if (ti == ET_ITEM) {
        // Item geben (positive Menge) oder nehmen (negative Menge).
        cmd.set("name", JsonValue::make_str(ev_text(g_ev.ed_ce_name)));
        const double n = ev_number(g_ev.ed_ce_val2, 1.0);
        if (n < 0) {
            cmd.set("take", JsonValue::make_num(-n, false));
        } else {
            cmd.set("count", JsonValue::make_num(n, false));
        }
    } else if (ti == ET_GOLD) {
        cmd.set("amount", JsonValue::make_num(ev_number(g_ev.ed_ce_val1, 0.0), false));
    } else if (ti == ET_HEAL) {
        const std::string amt = ev_text(g_ev.ed_ce_val1);
        if (!amt.empty()) {
            cmd.set("amount", JsonValue::make_num(ev_number(g_ev.ed_ce_val1, 0.0), false));
        }
    } else if (ti == ET_WEATHER) {
        cmd.set("name", JsonValue::make_str(ev_text(g_ev.ed_ce_name)));
        cmd.set("intensity", JsonValue::make_num(ev_number(g_ev.ed_ce_val1, 1.0), false));
    } else if (ti == ET_SAVE_GAME || ti == ET_LOAD_GAME) {
        cmd.set("slot", JsonValue::make_num(ev_number(g_ev.ed_ce_val1, 1.0), false));
    }

    const std::string type = EV_TYPES[ti];
    if (type == "if_switch" || type == "if_variable" || type == "if_self_switch") {
        // Zweige erhalten, wenn der Befehl schon ein Wenn war.
        const std::string old_type = existing ? ev_str_of(*existing, "cmd") : "";
        if (existing && (old_type == "if_switch" || old_type == "if_variable" ||
                         old_type == "if_self_switch")) {
            const JsonValue* th = existing->find("then");
            const JsonValue* el = existing->find("else");
            cmd.set("then", th ? *th : JsonValue::make_arr());
            cmd.set("else", el ? *el : JsonValue::make_arr());
        } else {
            cmd.set("then", JsonValue::make_arr());
            cmd.set("else", JsonValue::make_arr());
        }
    }
    return cmd;
}

void ev_insert_command() {
    JsonValue cmd = ev_build_command(nullptr);
    JsonValue* target = nullptr;
    if (g_ev.sel_cmd >= 0 && g_ev.sel_cmd < static_cast<int>(g_ev.refs.size())) {
        target = g_ev.refs[g_ev.sel_cmd].list;
    }
    if (!target) target = ev_commands(g_ev.sel_event);
    if (!target) return;
    target->arr.push_back(std::move(cmd));
    g.dirty = true;
    g_ev.sel_cmd = -1;
    ev_refresh_cmds();
    ev_load_cmd_form();
    update_title();
}

void ev_update_command() {
    if (g_ev.sel_cmd < 0 || g_ev.sel_cmd >= static_cast<int>(g_ev.refs.size())) return;
    const EventCmdRef ref = g_ev.refs[g_ev.sel_cmd];
    if (ref.header) return;
    const JsonValue* old = &ref.list->arr[ref.index];
    JsonValue cmd = ev_build_command(old);
    ref.list->arr[ref.index] = std::move(cmd);
    g.dirty = true;
    ev_refresh_cmds();
    ev_load_cmd_form();
    update_title();
}

void ev_delete_command() {
    if (g_ev.sel_cmd < 0 || g_ev.sel_cmd >= static_cast<int>(g_ev.refs.size())) return;
    const EventCmdRef ref = g_ev.refs[g_ev.sel_cmd];
    if (ref.header) return;
    ref.list->arr.erase(ref.list->arr.begin() + ref.index);
    g_ev.sel_cmd = -1;
    g.dirty = true;
    ev_refresh_cmds();
    ev_load_cmd_form();
    update_title();
}

void ev_move_command(int delta) {
    if (g_ev.sel_cmd < 0 || g_ev.sel_cmd >= static_cast<int>(g_ev.refs.size())) return;
    const EventCmdRef ref = g_ev.refs[g_ev.sel_cmd];
    if (ref.header) return;
    const int n = static_cast<int>(ref.list->arr.size());
    const int to = ref.index + delta;
    if (to < 0 || to >= n) return;
    std::swap(ref.list->arr[ref.index], ref.list->arr[to]);
    g_ev.sel_cmd = -1;
    g.dirty = true;
    ev_refresh_cmds();
    // denselben Befehl wieder markieren (seine Position hat sich verschoben)
    for (int i = 0; i < static_cast<int>(g_ev.refs.size()); ++i) {
        const EventCmdRef& r = g_ev.refs[i];
        if (!r.header && r.list == ref.list && r.index == to) {
            g_ev.sel_cmd = i;
            break;
        }
    }
    if (g_ev.sel_cmd >= 0) {
        SendMessageW(g_ev.list_cmds, LB_SETCURSEL, g_ev.sel_cmd, 0);
    }
    ev_load_cmd_form();
    update_title();
}

HWND ev_control(const wchar_t* cls, const wchar_t* text, DWORD style,
                int x, int y, int w, int h, int id) {
    HWND c = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style,
                             x, y, w, h, g_ev.win,
                             reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                             GetModuleHandleW(nullptr), nullptr);
    SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(g.font), TRUE);
    return c;
}

void ev_label(const wchar_t* text, int x, int y, int w) {
    ev_control(L"Static", text, 0, x, y, w, 20, 0);
}

LRESULT CALLBACK event_proc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_COMMAND: {
            const int id = LOWORD(wp);
            const int code = HIWORD(wp);
            if (g_ev.syncing) return 0;

            if (id == IDEV_LIST && code == LBN_SELCHANGE) {
                const int sel = static_cast<int>(
                    SendMessageW(g_ev.list_events, LB_GETCURSEL, 0, 0));
                if (sel == g_ev.sel_event) return 0;
                if (g_ev.sel_event >= 0) ev_apply_event_fields(false);
                g_ev.sel_event = sel;
                g_ev.sel_page = 0;   // Etappe 18g: Seitenwechsel beim Eventwechsel
                g_ev.sel_cmd = -1;
                ev_refresh_events();   // Auswahl nach dem Neuaufbau wiederherstellen
                ev_refresh_fields();
                ev_refresh_pages();
                ev_refresh_page_fields();
                ev_refresh_cmds();
                ev_load_cmd_form();
                return 0;
            }
            // Etappe 18g: Seitenliste
            if (id == IDEV_PAGES && code == LBN_SELCHANGE) {
                const int sel = static_cast<int>(
                    SendMessageW(g_ev.list_pages, LB_GETCURSEL, 0, 0));
                if (sel == g_ev.sel_page) return 0;
                if (g_ev.sel_page >= 0) ev_apply_page_fields(false);
                g_ev.sel_page = sel;
                g_ev.sel_cmd = -1;
                ev_refresh_pages();
                ev_refresh_page_fields();
                ev_refresh_cmds();
                ev_load_cmd_form();
                return 0;
            }
            if (id == IDEV_NEWPAGE)   { ev_new_page(); return 0; }
            if (id == IDEV_DELPAGE)   { ev_delete_page(); return 0; }
            if (id == IDEV_APPLY_PAGE) { ev_apply_page_fields(true); return 0; }
            if (id == IDEV_CMDS && code == LBN_SELCHANGE) {
                g_ev.sel_cmd = static_cast<int>(
                    SendMessageW(g_ev.list_cmds, LB_GETCURSEL, 0, 0));
                ev_refresh_cmds();
                ev_load_cmd_form();
                return 0;
            }
            if (id == IDEV_NEW)      { ev_new_event(); return 0; }
            if (id == IDEV_DEL)      { ev_delete_event(); return 0; }
            if (id == IDEV_DUP)      { ev_duplicate_event(); return 0; }
            if (id == IDEV_CLOSE)    { DestroyWindow(h); return 0; }
            if (id == IDEV_APPLY_EV) { ev_apply_event_fields(true); return 0; }
            if (id == IDEV_CETYPE && code == CBN_SELCHANGE) {
                ev_cmd_visibility();
                return 0;
            }
            if (id == IDEV_INSERT)   { ev_insert_command(); return 0; }
            if (id == IDEV_APPLYCMD) { ev_update_command(); return 0; }
            if (id == IDEV_DELCMD)   { ev_delete_command(); return 0; }
            if (id == IDEV_UP)       { ev_move_command(-1); return 0; }
            if (id == IDEV_DOWN)     { ev_move_command(+1); return 0; }
            return 0;
        }
        case WM_CLOSE:
            DestroyWindow(h);
            return 0;
        case WM_DESTROY:
            if (g_ev.win == h) {
                g_ev.win = nullptr;
                g_ev.refs.clear();
                g_ev.sel_event = -1;
                g_ev.sel_page = -1;
                g_ev.sel_cmd = -1;
            }
            return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

void open_event_editor() {
    if (g_ev.win) {
        SetForegroundWindow(g_ev.win);
        return;
    }
    if (g.cur_map < 0) return;

    WNDCLASSW wc{};
    wc.lpfnWndProc = event_proc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"RuneEditorEventDlg";
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
    RegisterClassW(&wc);

    const int W = 1000, H = 730;   // Etappe 18f: mehr Platz + Gruppenboxen
    RECT mr;
    GetWindowRect(g.main, &mr);
    g_ev.win = CreateWindowExW(WS_EX_DLGMODALFRAME, L"RuneEditorEventDlg",
                               L"Event-Editor",
                               WS_POPUP | WS_CAPTION | WS_SYSMENU,
                               mr.left + 120, mr.top + 60, W, H,
                               g.main, nullptr, GetModuleHandleW(nullptr), nullptr);

    // Linke Spalte: Event-Liste + Seiten (Etappe 18g)
    ev_label(L"EVENTS", 12, 8, 200);
    g_ev.list_events = ev_control(L"ListBox", nullptr,
                                  WS_VSCROLL | WS_BORDER | LBS_NOTIFY | LBS_HASSTRINGS,
                                  12, 30, 280, 330, IDEV_LIST);
    ev_control(L"Button", L"Neu", BS_PUSHBUTTON, 12, 366, 88, 28, IDEV_NEW);
    ev_control(L"Button", L"Löschen", BS_PUSHBUTTON, 106, 366, 88, 28, IDEV_DEL);
    ev_control(L"Button", L"Duplizieren", BS_PUSHBUTTON, 200, 366, 92, 28, IDEV_DUP);

    ev_label(L"SEITEN (höchste erfüllte gewinnt)", 12, 402, 280);
    g_ev.list_pages = ev_control(L"ListBox", nullptr,
                                 WS_VSCROLL | WS_BORDER | LBS_NOTIFY | LBS_HASSTRINGS,
                                 12, 424, 280, 130, IDEV_PAGES);
    ev_control(L"Button", L"Neue Seite", BS_PUSHBUTTON, 12, 560, 136, 28, IDEV_NEWPAGE);
    ev_control(L"Button", L"Seite löschen", BS_PUSHBUTTON, 156, 560, 136, 28, IDEV_DELPAGE);
    ev_control(L"Button", L"Schließen", BS_PUSHBUTTON, 12, 640, 280, 30, IDEV_CLOSE);

    // Rechte Seite oben: Event-Eigenschaften (Trigger steht jetzt pro Seite)
    ev_control(L"Button", L"Event-Eigenschaften", BS_GROUPBOX, 304, 12, 684, 80, 0);
    ev_label(L"ID", 316, 38, 24);
    g_ev.ed_id = ev_control(L"Edit", nullptr, WS_BORDER | ES_READONLY,
                            342, 34, 52, 24, IDEV_ID);
    ev_label(L"Name", 406, 38, 40);
    g_ev.ed_name = ev_control(L"Edit", nullptr, WS_BORDER | ES_LEFT,
                              450, 34, 180, 24, IDEV_NAME);

    ev_label(L"X", 316, 68, 16);
    g_ev.ed_x = ev_control(L"Edit", nullptr, WS_BORDER | ES_RIGHT,
                           334, 64, 70, 24, IDEV_X);
    ev_label(L"Z", 416, 68, 16);
    g_ev.ed_z = ev_control(L"Edit", nullptr, WS_BORDER | ES_RIGHT,
                           434, 64, 70, 24, IDEV_Z);
    ev_label(L"Radius", 516, 68, 54);
    g_ev.ed_radius = ev_control(L"Edit", nullptr, WS_BORDER | ES_RIGHT,
                                574, 64, 62, 24, IDEV_RADIUS);
    ev_control(L"Button", L"Event übernehmen", BS_PUSHBUTTON,
               740, 62, 190, 28, IDEV_APPLY_EV);

    // Etappe 18g: Gruppenbox der aktuellen Seite (Trigger + Bedingungen)
    ev_control(L"Button", L"Seite", BS_GROUPBOX, 304, 100, 684, 124, 0);
    ev_label(L"Trigger", 316, 128, 50);
    g_ev.cb_trigger = ev_control(L"ComboBox", nullptr,
                                 CBS_DROPDOWNLIST | WS_VSCROLL,
                                 372, 124, 110, 200, IDEV_TRIGGER);
    for (const wchar_t* t : EV_TRIGGERS) {
        SendMessageW(g_ev.cb_trigger, CB_ADDSTRING, 0,
                     reinterpret_cast<LPARAM>(t));
    }
    ev_label(L"Schalter", 496, 128, 56);
    g_ev.ed_cswitch = ev_control(L"Edit", nullptr, WS_BORDER | ES_LEFT,
                                 556, 124, 120, 24, IDEV_CSWITCH);
    g_ev.cb_cswval = ev_control(L"ComboBox", nullptr, CBS_DROPDOWNLIST | WS_VSCROLL,
                                682, 124, 60, 180, IDEV_CSWVAL);
    SendMessageW(g_ev.cb_cswval, CB_ADDSTRING, 0,
                 reinterpret_cast<LPARAM>(L"an"));
    SendMessageW(g_ev.cb_cswval, CB_ADDSTRING, 0,
                 reinterpret_cast<LPARAM>(L"aus"));
    SendMessageW(g_ev.cb_cswval, CB_SETCURSEL, 0, 0);

    // Etappe 19d: Self-Switch-Bedingung (XP) rechts daneben. Erstes Item
    // ("keins") = Bedingung aus; A-D wählt den Buchstaben des Events.
    ev_label(L"Self-Switch", 748, 128, 76);
    g_ev.cb_cself = ev_control(L"ComboBox", nullptr, CBS_DROPDOWNLIST | WS_VSCROLL,
                               828, 124, 68, 180, IDEV_CSELF);
    for (const wchar_t* b : { L"keins", L"A", L"B", L"C", L"D" }) {
        SendMessageW(g_ev.cb_cself, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(b));
    }
    SendMessageW(g_ev.cb_cself, CB_SETCURSEL, 0, 0);
    g_ev.cb_cselfval = ev_control(L"ComboBox", nullptr, CBS_DROPDOWNLIST | WS_VSCROLL,
                                  902, 124, 60, 180, IDEV_CSELFVAL);
    SendMessageW(g_ev.cb_cselfval, CB_ADDSTRING, 0,
                 reinterpret_cast<LPARAM>(L"an"));
    SendMessageW(g_ev.cb_cselfval, CB_ADDSTRING, 0,
                 reinterpret_cast<LPARAM>(L"aus"));
    SendMessageW(g_ev.cb_cselfval, CB_SETCURSEL, 0, 0);

    ev_label(L"Variable", 316, 158, 56);
    g_ev.ed_cvar = ev_control(L"Edit", nullptr, WS_BORDER | ES_LEFT,
                              376, 154, 110, 24, IDEV_CVAR);
    g_ev.cb_cvop = ev_control(L"ComboBox", nullptr, CBS_DROPDOWNLIST | WS_VSCROLL,
                              492, 154, 60, 180, IDEV_CVOP);
    for (const wchar_t* o : EV_OPS) {
        SendMessageW(g_ev.cb_cvop, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(o));
    }
    SendMessageW(g_ev.cb_cvop, CB_SETCURSEL, 0, 0);
    ev_label(L"Wert", 558, 158, 36);
    g_ev.ed_cvval = ev_control(L"Edit", nullptr, WS_BORDER | ES_RIGHT,
                               596, 154, 70, 24, IDEV_CVVAL);
    ev_control(L"Button", L"Seite übernehmen", BS_PUSHBUTTON,
               740, 150, 190, 28, IDEV_APPLY_PAGE);

    // Befehlsliste + Formular (arbeitet auf der gewählten Seite)
    ev_control(L"Button", L"Befehle", BS_GROUPBOX, 304, 232, 684, 458, 0);
    g_ev.list_cmds = ev_control(L"ListBox", nullptr,
                                WS_VSCROLL | WS_BORDER | LBS_NOTIFY | LBS_HASSTRINGS,
                                316, 252, 660, 230, IDEV_CMDS);

    ev_label(L"Neuer Befehl:", 316, 494, 110);
    g_ev.cb_ce_type = ev_control(L"ComboBox", nullptr,
                                 CBS_DROPDOWNLIST | WS_VSCROLL,
                                 424, 490, 250, 220, IDEV_CETYPE);
    for (const wchar_t* t : EV_TYPE_LABELS) {
        SendMessageW(g_ev.cb_ce_type, CB_ADDSTRING, 0,
                     reinterpret_cast<LPARAM>(t));
    }
    SendMessageW(g_ev.cb_ce_type, CB_SETCURSEL, 0, 0);
    g_ev.lbl_insert = ev_control(L"Static", L"Ziel: Wurzel", 0,
                                 686, 494, 180, 20, IDEV_L_INSERT);
    ev_control(L"Button", L"Einfügen", BS_PUSHBUTTON, 872, 488, 104, 28, IDEV_INSERT);

    g_ev.lbl_name = ev_control(L"Static", L"Name", 0, 316, 524, 105, 20, IDEV_L_NAME);
    g_ev.ed_ce_name = ev_control(L"Edit", nullptr, WS_BORDER | ES_LEFT,
                                 424, 520, 220, 24, IDEV_CE_NAME);
    g_ev.lbl_val1 = ev_control(L"Static", L"Wert", 0, 654, 524, 106, 20, IDEV_L_VAL1);
    g_ev.ed_ce_val1 = ev_control(L"Edit", nullptr, WS_BORDER | ES_RIGHT,
                                 764, 520, 172, 24, IDEV_CE_VAL1);

    g_ev.lbl_val2 = ev_control(L"Static", L"Add", 0, 316, 554, 105, 20, IDEV_L_VAL2);
    g_ev.ed_ce_val2 = ev_control(L"Edit", nullptr, WS_BORDER | ES_RIGHT,
                                 424, 550, 100, 24, IDEV_CE_VAL2);
    g_ev.cb_ce_op = ev_control(L"ComboBox", nullptr, CBS_DROPDOWNLIST | WS_VSCROLL,
                               528, 550, 70, 180, IDEV_CE_OP);
    for (const wchar_t* o : EV_OPS) {
        SendMessageW(g_ev.cb_ce_op, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(o));
    }
    SendMessageW(g_ev.cb_ce_op, CB_SETCURSEL, 0, 0);
    g_ev.lbl_val3 = ev_control(L"Static", L"Yaw (Grad)", 0, 606, 554, 90, 20, IDEV_L_VAL3);
    g_ev.ed_ce_val3 = ev_control(L"Edit", nullptr, WS_BORDER | ES_RIGHT,
                                 698, 550, 70, 24, IDEV_CE_VAL3);
    g_ev.cb_ce_bool = ev_control(L"ComboBox", nullptr, CBS_DROPDOWNLIST | WS_VSCROLL,
                                 776, 550, 90, 180, IDEV_CE_BOOL);
    SendMessageW(g_ev.cb_ce_bool, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"an"));
    SendMessageW(g_ev.cb_ce_bool, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"aus"));
    SendMessageW(g_ev.cb_ce_bool, CB_SETCURSEL, 0, 0);

    g_ev.lbl_text = ev_control(L"Static", L"Zeilen (eine pro Zeile)", 0,
                               316, 582, 220, 20, IDEV_L_TEXT);
    g_ev.ed_ce_text = ev_control(L"Edit", nullptr,
                                 WS_BORDER | WS_VSCROLL | ES_LEFT | ES_MULTILINE |
                                 ES_AUTOVSCROLL | ES_WANTRETURN,
                                 316, 602, 620, 50, IDEV_CE_TEXT);

    ev_control(L"Button", L"Befehl übernehmen", BS_PUSHBUTTON,
               686, 660, 250, 28, IDEV_APPLYCMD);
    ev_control(L"Button", L"Befehl löschen", BS_PUSHBUTTON,
               316, 660, 150, 28, IDEV_DELCMD);
    ev_control(L"Button", L"Hoch", BS_PUSHBUTTON, 476, 660, 96, 28, IDEV_UP);
    ev_control(L"Button", L"Runter", BS_PUSHBUTTON, 580, 660, 96, 28, IDEV_DOWN);

    // Startzustand: erstes Event auswählen.
    JsonValue* evs = ev_events();
    g_ev.sel_event = evs->arr.empty() ? -1 : 0;
    g_ev.sel_page = 0;
    g_ev.sel_cmd = -1;
    ev_refresh();
    ev_cmd_visibility();

    ShowWindow(g_ev.win, SW_SHOW);
    SetForegroundWindow(g_ev.win);
}

void close_event_editor() {
    if (g_ev.win) DestroyWindow(g_ev.win);
}

bool event_editor_focused() {
    if (!g_ev.win) return false;
    HWND fg = GetForegroundWindow();
    return fg == g_ev.win || (fg && IsChild(g_ev.win, fg));
}

// ============================================================================
//  Etappe 17e: Script-Editor
//
//  Bearbeitet projekte/<name>/data/scripts.json ({"scripts":[{name,code}]}) in
//  einem eigenen moduslosen Fenster. Das Spiel lädt die Datei beim Setup
//  (Scripts.load_project); Events rufen Scripts über {"cmd":"script",...}.
// ============================================================================
enum {
    IDSC_LIST   = 6001,
    IDSC_NEW    = 6002,
    IDSC_DEL    = 6003,
    IDSC_NAME   = 6010,
    IDSC_CODE   = 6011,
    IDSC_APPLY  = 6012,
    IDSC_SAVE   = 6013,
    IDSC_RELOAD = 6014,
    IDSC_CLOSE  = 6015,
    IDSC_STATUS = 6016,
};

struct ScriptDlgState {
    HWND win = nullptr;
    HWND list = nullptr;
    HWND ed_name = nullptr;
    HWND ed_code = nullptr;
    HWND lbl_status = nullptr;
    JsonValue root;
    int sel = -1;
    bool dirty = false;
    bool syncing = false;
};

static ScriptDlgState g_sc;
static HFONT g_sc_code_font = nullptr;

std::string sc_path() {
    return g.project_dir + "\\scripts.json";
}

JsonValue* sc_scripts() {
    JsonValue* s = g_sc.root.find("scripts");
    if (!s || !s->is_arr()) s = &g_sc.root.set("scripts", JsonValue::make_arr());
    return s;
}

void sc_status(const std::string& text) {
    if (g_sc.lbl_status) ev_set_text(g_sc.lbl_status, text);
}

void sc_load() {
    g_sc.root = JsonValue::make_obj();
    std::string text;
    if (read_file(sc_path(), text) && !text.empty()) {
        try {
            g_sc.root = json_parse(text);
        } catch (const std::exception&) {
            g_sc.root = JsonValue::make_obj();
        }
    }
    JsonValue* arr = g_sc.root.find("scripts");
    if (!arr || !arr->is_arr()) {
        if (g_sc.root.is_arr()) {
            JsonValue wrapped = JsonValue::make_obj();
            wrapped.set("scripts", g_sc.root);
            g_sc.root = std::move(wrapped);
        } else {
            g_sc.root.set("scripts", JsonValue::make_arr());
        }
    }
    g_sc.dirty = false;
}

void sc_refresh_list() {
    if (!g_sc.list) return;
    SendMessageW(g_sc.list, LB_RESETCONTENT, 0, 0);
    const JsonValue* arr = sc_scripts();
    for (int i = 0; i < static_cast<int>(arr->arr.size()); ++i) {
        const std::wstring w = utf8_to_wide(ev_str_of(arr->arr[i], "name", "?"));
        const int idx = static_cast<int>(
            SendMessageW(g_sc.list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(w.c_str())));
        SendMessageW(g_sc.list, LB_SETITEMDATA, idx,
                     static_cast<LPARAM>(static_cast<INT_PTR>(i)));
    }
    if (g_sc.sel >= 0 && g_sc.sel < static_cast<int>(arr->arr.size())) {
        SendMessageW(g_sc.list, LB_SETCURSEL, g_sc.sel, 0);
    } else {
        g_sc.sel = -1;
    }
}

void sc_refresh_fields() {
    if (!g_sc.ed_name) return;
    const JsonValue* arr = sc_scripts();
    const bool valid = g_sc.sel >= 0 && g_sc.sel < static_cast<int>(arr->arr.size());
    EnableWindow(g_sc.ed_name, valid ? TRUE : FALSE);
    EnableWindow(g_sc.ed_code, valid ? TRUE : FALSE);
    EnableWindow(GetDlgItem(g_sc.win, IDSC_APPLY), valid ? TRUE : FALSE);
    EnableWindow(GetDlgItem(g_sc.win, IDSC_DEL), valid ? TRUE : FALSE);
    if (!valid) {
        SetWindowTextW(g_sc.ed_name, L"");
        SetWindowTextW(g_sc.ed_code, L"");
        return;
    }
    const JsonValue& s = arr->arr[g_sc.sel];
    ev_set_text(g_sc.ed_name, ev_str_of(s, "name"));
    ev_set_text(g_sc.ed_code, ev_str_of(s, "code"));
}

void sc_apply(bool mark_dirty) {
    JsonValue* arr = sc_scripts();
    if (g_sc.sel < 0 || g_sc.sel >= static_cast<int>(arr->arr.size())) return;
    JsonValue& s = arr->arr[g_sc.sel];
    s.set("name", JsonValue::make_str(ev_text(g_sc.ed_name)));
    s.set("code", JsonValue::make_str(ev_text(g_sc.ed_code)));
    if (mark_dirty) {
        g_sc.dirty = true;
        sc_refresh_list();
        sc_status("Geändert - noch nicht gespeichert");
    }
}

void sc_add() {
    sc_apply(true);
    JsonValue* arr = sc_scripts();
    int n = 1;
    std::string name;
    for (;;) {
        name = "script_" + std::to_string(n);
        bool used = false;
        for (const JsonValue& s : arr->arr) {
            if (ev_str_of(s, "name") == name) { used = true; break; }
        }
        if (!used) break;
        ++n;
    }
    JsonValue s = JsonValue::make_obj();
    s.set("name", JsonValue::make_str(name));
    s.set("code", JsonValue::make_str("# Neues Script\n# Helfer: say, switch, set_switch, variable,\n"
                                      "#         set_variable, add_variable, teleport, sound, effect, log\n"));
    arr->arr.push_back(std::move(s));
    g_sc.sel = static_cast<int>(arr->arr.size()) - 1;
    g_sc.dirty = true;
    sc_refresh_list();
    sc_refresh_fields();
    sc_status("Neues Script angelegt - Speichern nicht vergessen");
}

void sc_delete() {
    JsonValue* arr = sc_scripts();
    if (g_sc.sel < 0 || g_sc.sel >= static_cast<int>(arr->arr.size())) return;
    arr->arr.erase(arr->arr.begin() + g_sc.sel);
    if (g_sc.sel >= static_cast<int>(arr->arr.size())) {
        g_sc.sel = static_cast<int>(arr->arr.size()) - 1;
    }
    g_sc.dirty = true;
    sc_refresh_list();
    sc_refresh_fields();
    sc_status("Script gelöscht - Speichern nicht vergessen");
}

void sc_save() {
    sc_apply(false);
    if (write_file(sc_path(), json_write(g_sc.root))) {
        g_sc.dirty = false;
        sc_status("Gespeichert: scripts.json (" +
                  std::to_string(sc_scripts()->arr.size()) + " Scripts)");
    } else {
        sc_status("FEHLER: scripts.json konnte nicht geschrieben werden");
    }
}

void sc_reload() {
    sc_load();
    g_sc.sel = sc_scripts()->arr.empty() ? -1 : 0;
    sc_refresh_list();
    sc_refresh_fields();
    sc_status("Vom Datenträger neu geladen");
}

HWND sc_control(const wchar_t* cls, const wchar_t* text, DWORD style,
                int x, int y, int w, int h, int id) {
    HWND c = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style,
                             x, y, w, h, g_sc.win,
                             reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                             GetModuleHandleW(nullptr), nullptr);
    SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(g.font), TRUE);
    return c;
}

LRESULT CALLBACK script_proc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_COMMAND: {
            const int id = LOWORD(wp);
            const int code = HIWORD(wp);
            if (g_sc.syncing) return 0;

            if (id == IDSC_LIST && code == LBN_SELCHANGE) {
                const int sel = static_cast<int>(
                    SendMessageW(g_sc.list, LB_GETCURSEL, 0, 0));
                if (sel == g_sc.sel) return 0;
                sc_apply(true);
                g_sc.sel = sel;
                sc_refresh_list();
                sc_refresh_fields();
                return 0;
            }
            if (id == IDSC_NEW)    { sc_add(); return 0; }
            if (id == IDSC_DEL)    { sc_delete(); return 0; }
            if (id == IDSC_APPLY)  { sc_apply(true); return 0; }
            if (id == IDSC_SAVE)   { sc_save(); return 0; }
            if (id == IDSC_RELOAD) { sc_reload(); return 0; }
            if (id == IDSC_CLOSE)  { DestroyWindow(h); return 0; }
            return 0;
        }
        case WM_CLOSE:
            DestroyWindow(h);
            return 0;
        case WM_DESTROY:
            if (g_sc.win == h) {
                g_sc.win = nullptr;
                g_sc.list = nullptr;
                g_sc.ed_name = nullptr;
                g_sc.ed_code = nullptr;
                g_sc.lbl_status = nullptr;
                g_sc.sel = -1;
            }
            return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

void open_script_editor() {
    if (g_sc.win) {
        SetForegroundWindow(g_sc.win);
        return;
    }
    if (g.cur_map < 0) return;

    if (!g_sc_code_font) {
        g_sc_code_font = CreateFontW(-15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                     DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                     CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                     FIXED_PITCH | FF_MODERN, L"Consolas");
    }

    WNDCLASSW wc{};
    wc.lpfnWndProc = script_proc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"RuneEditorScriptDlg";
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
    RegisterClassW(&wc);

    RECT mr;
    GetWindowRect(g.main, &mr);
    g_sc.win = CreateWindowExW(WS_EX_DLGMODALFRAME, L"RuneEditorScriptDlg",
                               L"Script-Editor", WS_POPUP | WS_CAPTION | WS_SYSMENU,
                               mr.left + 140, mr.top + 70, 920, 700,
                               g.main, nullptr, GetModuleHandleW(nullptr), nullptr);

    sc_load();

    sc_control(L"Static", L"SCRIPTS", 0, 12, 8, 200, 20, 0);
    g_sc.list = sc_control(L"ListBox", nullptr,
                           WS_VSCROLL | WS_BORDER | LBS_NOTIFY | LBS_HASSTRINGS,
                           12, 30, 240, 500, IDSC_LIST);
    sc_control(L"Button", L"Neu", BS_PUSHBUTTON, 12, 538, 72, 30, IDSC_NEW);
    sc_control(L"Button", L"Löschen", BS_PUSHBUTTON, 88, 538, 84, 30, IDSC_DEL);
    sc_control(L"Button", L"Schließen", BS_PUSHBUTTON, 12, 578, 240, 30, IDSC_CLOSE);

    sc_control(L"Static", L"Name", 0, 268, 32, 44, 20, 0);
    g_sc.ed_name = sc_control(L"Edit", nullptr, WS_BORDER | ES_LEFT,
                              316, 28, 380, 24, IDSC_NAME);
    sc_control(L"Button", L"Übernehmen", BS_PUSHBUTTON, 706, 26, 140, 28, IDSC_APPLY);
    sc_control(L"Static", L"Code (Ruby)", 0, 268, 62, 200, 20, 0);
    g_sc.ed_code = sc_control(L"Edit", nullptr,
                              WS_BORDER | WS_VSCROLL | WS_HSCROLL | ES_LEFT |
                              ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_WANTRETURN,
                              268, 84, 640, 500, IDSC_CODE);
    SendMessageW(g_sc.ed_code, WM_SETFONT,
                 reinterpret_cast<WPARAM>(g_sc_code_font ? g_sc_code_font : g.font), TRUE);

    g_sc.lbl_status = sc_control(L"Static", L"", 0, 268, 590, 500, 20, IDSC_STATUS);
    sc_control(L"Button", L"Speichern", BS_PUSHBUTTON, 268, 616, 140, 30, IDSC_SAVE);
    sc_control(L"Button", L"Neu laden", BS_PUSHBUTTON, 416, 616, 140, 30, IDSC_RELOAD);
    sc_control(L"Button", L"Schließen", BS_PUSHBUTTON, 564, 616, 140, 30, IDSC_CLOSE);

    g_sc.sel = sc_scripts()->arr.empty() ? -1 : 0;
    g_sc.syncing = true;
    sc_refresh_list();
    sc_refresh_fields();
    g_sc.syncing = false;
    sc_status("scripts.json: " + sc_path());

    ShowWindow(g_sc.win, SW_SHOW);
    SetForegroundWindow(g_sc.win);
}

bool script_editor_focused() {
    if (!g_sc.win) return false;
    HWND fg = GetForegroundWindow();
    return fg == g_sc.win || (fg && IsChild(g_sc.win, fg));
}

// ============================================================================
//  Etappe 17f: Materialbase + Import-Dialog (moduslos)
// ============================================================================
enum {
    IDMA_LIST    = 7001,
    IDMA_IMPORT  = 7002,
    IDMA_DEL     = 7003,
    IDMA_APPLY   = 7004,
    IDMA_CLEAR   = 7005,
    IDMA_CLOSE   = 7006,
    IDMA_TILE    = 7010,
    IDMA_TILESET = 7011,
    IDMA_INFO    = 7012,
    IDMA_STATUS  = 7013,
};

struct MaterialDlgState {
    HWND win = nullptr;
    HWND list = nullptr;
    HWND ed_tile = nullptr;
    HWND lbl_info = nullptr;
    HWND lbl_status = nullptr;
    std::vector<std::string> names;
    std::string sel;
};

static MaterialDlgState g_md;

void ma_status(const std::string& text) {
    if (g_md.lbl_status) ev_set_text(g_md.lbl_status, text);
}

void ma_refresh_info() {
    if (!g_md.win) return;
    const MaterialInfo* m = g_md.sel.empty() ? nullptr : mat_find(g_md.sel);
    const bool has_sel = !g_md.sel.empty();
    const bool is_imported = m != nullptr;

    EnableWindow(GetDlgItem(g_md.win, IDMA_TILE), is_imported ? TRUE : FALSE);
    EnableWindow(GetDlgItem(g_md.win, IDMA_TILESET), is_imported ? TRUE : FALSE);
    EnableWindow(GetDlgItem(g_md.win, IDMA_DEL), is_imported ? TRUE : FALSE);
    EnableWindow(GetDlgItem(g_md.win, IDMA_APPLY),
                 (has_sel && g.sel_obj >= 0) ? TRUE : FALSE);
    EnableWindow(GetDlgItem(g_md.win, IDMA_CLEAR), g.sel_obj >= 0 ? TRUE : FALSE);

    if (!has_sel) {
        ev_set_text(g_md.lbl_info,
                    "Kein Material ausgewählt.\n\nLinks eine Textur anklicken.");
        SetWindowTextW(g_md.ed_tile, L"");
        return;
    }
    if (is_imported) {
        std::string info = "Importiert: " + m->name + "\n" +
                           "Datei: textures\\" + m->file + "\n" +
                           "Grösse: " + std::to_string(m->w) + " x " +
                           std::to_string(m->h) + " Pixel\n" +
                           "Quelle: " + m->source;
        ev_set_text(g_md.lbl_info, info);
        wchar_t buf[32];
        std::swprintf(buf, 32, L"%.2f", m->tile);
        SetWindowTextW(g_md.ed_tile, buf);
    } else {
        ev_set_text(g_md.lbl_info,
                    "Prozedurale Textur (fest eingebaut):\n" + g_md.sel +
                    "\n\nKann nur als Objekt-Material verwendet werden.");
        SetWindowTextW(g_md.ed_tile, L"");
    }
}

void ma_refresh_list() {
    if (!g_md.list) return;
    g_md.names = mat_all_names();
    SendMessageW(g_md.list, LB_RESETCONTENT, 0, 0);
    int select = -1;
    for (int i = 0; i < static_cast<int>(g_md.names.size()); ++i) {
        const bool imported = mat_find(g_md.names[i]) != nullptr;
        const std::wstring line = utf8_to_wide(g_md.names[i]) +
                                  (imported ? L"  (importiert)" : L"");
        const int idx = static_cast<int>(
            SendMessageW(g_md.list, LB_ADDSTRING, 0,
                         reinterpret_cast<LPARAM>(line.c_str())));
        if (g_md.names[i] == g_md.sel) select = idx;
    }
    if (select >= 0) SendMessageW(g_md.list, LB_SETCURSEL, select, 0);
    ma_refresh_info();
}

void ma_apply_tile() {
    MaterialInfo* m = const_cast<MaterialInfo*>(mat_find(g_md.sel));
    if (!m || !m->imported) return;
    wchar_t buf[32];
    GetWindowTextW(g_md.ed_tile, buf, 32);
    std::string s = wide_to_utf8(buf);
    std::replace(s.begin(), s.end(), ',', '.');
    const double tile = std::atof(s.c_str());
    if (tile <= 0.0) { ma_status("Kachel muss grösser als 0 sein"); return; }
    m->tile = tile;
    mat_save();
    ma_status("Kachel gespeichert: " + g_md.sel);
}

void ma_apply_to_object() {
    if (g_md.sel.empty() || g.sel_obj < 0 || !g.objects) return;
    push_undo();
    g.objects->arr[g.sel_obj].set("tex", JsonValue::make_str(g_md.sel));
    g.dirty = true;
    sync_property_fields();
    update_title();
    ma_status("Objekt " + std::to_string(g.sel_obj + 1) + " -> " + g_md.sel);
}

void ma_clear_object() {
    if (g.sel_obj < 0 || !g.objects) return;
    push_undo();
    json_remove_key(g.objects->arr[g.sel_obj], "tex");
    g.dirty = true;
    sync_property_fields();
    update_title();
    ma_status("Objekt-Material entfernt (Prefab-Texturen gelten wieder)");
}

void ma_delete() {
    const MaterialInfo* m = mat_find(g_md.sel);
    if (!m || !m->imported) return;
    DeleteFileA((mat_dir() + "\\" + m->file).c_str());
    const std::string name = m->name;
    for (size_t i = 0; i < g_materials.size(); ++i) {
        if (g_materials[i].name == name) {
            g_materials.erase(g_materials.begin() + i);
            break;
        }
    }
    mat_save();
    g_md.sel.clear();
    ma_refresh_list();
    ma_status("Material gelöscht: " + name);
}

HWND ma_control(const wchar_t* cls, const wchar_t* text, DWORD style,
                int x, int y, int w, int h, int id) {
    HWND c = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style,
                             x, y, w, h, g_md.win,
                             reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                             GetModuleHandleW(nullptr), nullptr);
    SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(g.font), TRUE);
    return c;
}

LRESULT CALLBACK material_proc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_COMMAND: {
            const int id = LOWORD(wp);
            const int code = HIWORD(wp);
            if (id == IDMA_LIST && code == LBN_SELCHANGE) {
                const int idx = static_cast<int>(
                    SendMessageW(g_md.list, LB_GETCURSEL, 0, 0));
                if (idx >= 0 && idx < static_cast<int>(g_md.names.size())) {
                    g_md.sel = g_md.names[idx];
                    ma_refresh_info();
                }
                return 0;
            }
            if (id == IDMA_IMPORT)  { open_import_texture(); return 0; }
            if (id == IDMA_DEL)     { ma_delete(); return 0; }
            if (id == IDMA_APPLY)   { ma_apply_to_object(); return 0; }
            if (id == IDMA_CLEAR)   { ma_clear_object(); return 0; }
            if (id == IDMA_TILESET) { ma_apply_tile(); return 0; }
            if (id == IDMA_CLOSE)   { DestroyWindow(h); return 0; }
            return 0;
        }
        case WM_CLOSE:
            DestroyWindow(h);
            return 0;
        case WM_DESTROY:
            if (g_md.win == h) {
                g_md.win = nullptr;
                g_md.list = nullptr;
                g_md.ed_tile = nullptr;
                g_md.lbl_info = nullptr;
                g_md.lbl_status = nullptr;
            }
            return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

void open_import_texture() {
    wchar_t file[MAX_PATH] = L"";
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g.main;
    ofn.lpstrFilter = L"Bilder (*.png;*.bmp)\0*.png;*.bmp\0Alle Dateien (*.*)\0*.*\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    ofn.lpstrTitle = L"Grafik importieren (PNG/BMP)";
    if (!GetOpenFileNameW(&ofn)) return;

    std::string name;
    if (mat_import_file(wide_to_utf8(file), name)) {
        status_mode(L"Textur importiert");
        if (g_md.win) {
            g_md.sel = name;
            ma_refresh_list();
            ma_status("Importiert: " + name);
        }
        // Vorschau-Objekte neu zeichnen lassen (nächster Frame genügt).
    } else {
        MessageBoxW(g.main, L"Das Bild konnte nicht gelesen werden.",
                    L"Grafik-Import", MB_OK | MB_ICONWARNING);
    }
}

void open_material_dialog() {
    if (g_md.win) {
        SetForegroundWindow(g_md.win);
        return;
    }
    if (g.project_dir.empty()) return;

    WNDCLASSW wc{};
    wc.lpfnWndProc = material_proc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"RuneEditorMatDlg";
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
    RegisterClassW(&wc);

    RECT mr;
    GetWindowRect(g.main, &mr);
    g_md.win = CreateWindowExW(WS_EX_DLGMODALFRAME, L"RuneEditorMatDlg",
                               L"Materialbase", WS_POPUP | WS_CAPTION | WS_SYSMENU,
                               mr.left + 220, mr.top + 110, 660, 580,
                               g.main, nullptr, GetModuleHandleW(nullptr), nullptr);

    ma_control(L"Static", L"MATERIALIEN", 0, 12, 8, 200, 20, 0);
    g_md.list = ma_control(L"ListBox", nullptr,
                           WS_VSCROLL | WS_BORDER | LBS_NOTIFY | LBS_HASSTRINGS,
                           12, 30, 280, 460, IDMA_LIST);
    ma_control(L"Button", L"Importieren (PNG/BMP)...", BS_PUSHBUTTON,
               12, 500, 190, 30, IDMA_IMPORT);
    ma_control(L"Button", L"Löschen", BS_PUSHBUTTON,
               208, 500, 84, 30, IDMA_DEL);

    g_md.lbl_info = ma_control(L"Static", L"", SS_LEFT,
                               308, 30, 320, 110, IDMA_INFO);
    ma_control(L"Static", L"Kachel (m)", 0, 308, 152, 70, 20, 0);
    g_md.ed_tile = ma_control(L"Edit", nullptr, WS_BORDER | ES_RIGHT,
                              380, 148, 90, 24, IDMA_TILE);
    ma_control(L"Button", L"Kachel setzen", BS_PUSHBUTTON,
               480, 146, 148, 28, IDMA_TILESET);

    ma_control(L"Button", L"Auf ausgewähltes Objekt anwenden", BS_PUSHBUTTON,
               308, 190, 320, 30, IDMA_APPLY);
    ma_control(L"Button", L"Objekt-Material entfernen", BS_PUSHBUTTON,
               308, 226, 320, 30, IDMA_CLEAR);
    ma_control(L"Static",
               L"Das Material überschreibt alle Teile des Objekts.\n"
               L"Im Eigenschaften-Panel steht es als Feld \"Material\".",
               SS_LEFT, 308, 266, 320, 60, 0);
    g_md.lbl_status = ma_control(L"Static", L"", SS_LEFT,
                                 308, 330, 320, 40, IDMA_STATUS);

    ma_control(L"Button", L"Schließen", BS_PUSHBUTTON,
               308, 500, 320, 30, IDMA_CLOSE);

    g_md.sel.clear();
    for (const MaterialInfo& m : g_materials) {
        if (m.imported) { g_md.sel = m.name; break; }
    }
    if (g_md.sel.empty() && !g.tex_names.empty()) g_md.sel = g.tex_names.front();
    ma_refresh_list();
    ShowWindow(g_md.win, SW_SHOW);
    SetForegroundWindow(g_md.win);
}

bool material_editor_focused() {
    if (!g_md.win) return false;
    HWND fg = GetForegroundWindow();
    return fg == g_md.win || (fg && IsChild(g_md.win, fg));
}

// ============================================================================
//  Etappe 17g, 19k.1: Datenbank-Dialog (Helden / Items / Gegner / Truppen)
//
//  Bearbeitet projekte/<name>/data/database.json
//  ({"gold","actors","items","enemies","troops"}).
//  Das Spiel liest sie via Database.database und baut daraus die Party
//  (Party.setup) und die Kämpfe (Battle.start, Etappe 19i); Gold/Inventar
//  wandern mit den Spielständen.
//  Vier Reiter-Umschalter oben + eine Liste links + Formular rechts.
//  Das Formular ist feldgesteuert (DB_FIELDS_*): pro Reiter eine Tabelle aus
//  JSON-Schlüssel, Beschriftung, Zahlen-Flag und Präzision. Neue Felder
//  brauchen nur einen Tabellen-Eintrag (Platz: DB_MAX_ROWS Zeilen).
//  tools/sync_test.rb prüft die Tabellen gegen die echte database.json.
// ============================================================================
enum {
    IDDB_LIST     = 8001,   // Eintrags-Liste (alle Reiter)
    IDDB_TAB_H    = 8010,   // Umschalter: Helden
    IDDB_TAB_I    = 8011,   // Umschalter: Items
    IDDB_GOLD     = 8012,   // Gold-Edit
    IDDB_TAB_E    = 8013,   // Umschalter: Gegner (19k.1)
    IDDB_TAB_T    = 8014,   // Umschalter: Truppen (19k.1)
    IDDB_F1       = 8020,   // Formularfelder 1..13 (IDDB_F1 + Zeile)
    IDDB_NEW      = 8050,
    IDDB_DEL      = 8051,
    IDDB_SAVE     = 8052,
    IDDB_RELOAD   = 8053,
    IDDB_CLOSE    = 8054,
    IDDB_STATUS   = 8055,
    IDDB_L1       = 8060,   // Labels 1..13 (IDDB_L1 + Zeile)
};

constexpr int DB_MAX_ROWS = 13;   // Gegner brauchen 12 Felder, eine Zeile Luft

// Ein Formularfeld: JSON-Schlüssel, Beschriftung, Zahl (sonst Text),
// Nachkommastellen. Sonderfall Truppen-"enemies": Array aus Gegner-IDs,
// im Edit komma-getrennt (db_join_ids/db_split_ids).
struct DbField {
    const char*    key;
    const wchar_t* label;
    bool           num;
    int            prec;
};

const DbField DB_FIELDS_ACTOR[9] = {
    { "id",    L"ID",       false, 0 },
    { "name",  L"Name",     false, 0 },
    { "class", L"Klasse",   false, 0 },
    { "hp",    L"HP (max)", true,  0 },
    { "mp",    L"MP (max)", true,  0 },
    { "atk",   L"ATK",      true,  0 },
    { "def",   L"DEF",      true,  0 },
    { "spi",   L"SPI",      true,  0 },
    { "agi",   L"AGI",      true,  0 },
};
const DbField DB_FIELDS_ITEM[7] = {
    { "id",    L"ID",                          false, 0 },
    { "name",  L"Name",                        false, 0 },
    { "kind",  L"Art (heal/mana/quest/money)", false, 0 },
    { "power", L"Stärke",                      true,  0 },
    { "price", L"Preis",                       true,  0 },
    { "start", L"Startmenge",                  true,  0 },
    { "desc",  L"Beschreibung",                false, 0 },
};
const DbField DB_FIELDS_ENEMY[12] = {
    { "id",        L"ID",                           false, 0 },
    { "name",      L"Name",                         false, 0 },
    { "hp",        L"HP",                           true,  0 },
    { "mp",        L"MP",                           true,  0 },
    { "atk",       L"ATK",                          true,  0 },
    { "def",       L"DEF",                          true,  0 },
    { "spi",       L"SPI",                          true,  0 },
    { "agi",       L"AGI",                          true,  0 },
    { "exp",       L"Exp (Belohnung)",              true,  0 },
    { "gold",      L"Gold (Beute)",                 true,  0 },
    { "drop",      L"Drop (Item-ID, leer = keins)", false, 0 },
    { "drop_rate", L"Drop-Chance (0..1)",           true,  2 },
};
const DbField DB_FIELDS_TROOP[4] = {
    { "id",      L"ID",                           false, 0 },
    { "name",    L"Name",                         false, 0 },
    { "enemies", L"Gegner (IDs, komma-getrennt)", false, 0 },
    { "desc",    L"Beschreibung",                 false, 0 },
};

// Reiter -> Feldtabelle. 0 = Helden, 1 = Items, 2 = Gegner, 3 = Truppen.
const DbField* db_fields(int tab, int& count) {
    switch (tab) {
        case 1:  count = 7;  return DB_FIELDS_ITEM;
        case 2:  count = 12; return DB_FIELDS_ENEMY;
        case 3:  count = 4;  return DB_FIELDS_TROOP;
        default: count = 9;  return DB_FIELDS_ACTOR;
    }
}

// Reiter -> Schlüssel des JSON-Arrays in database.json.
const char* db_tab_key(int tab) {
    switch (tab) {
        case 1:  return "items";
        case 2:  return "enemies";
        case 3:  return "troops";
        default: return "actors";
    }
}

struct DatabaseDlgState {
    HWND win = nullptr;
    HWND list = nullptr;
    HWND ed_gold = nullptr;
    HWND ed[DB_MAX_ROWS] = {};    // Formularfelder
    HWND lbl[DB_MAX_ROWS] = {};   // zugehörige Labels
    HWND lbl_status = nullptr;
    JsonValue root;      // komplette database.json
    int  tab = 0;        // 0 Helden, 1 Items, 2 Gegner, 3 Truppen
    int  sel = -1;
    bool dirty = false;
    bool syncing = false;
};

static DatabaseDlgState g_db;

std::string db_path() {
    return g.project_dir + "\\database.json";
}

JsonValue* db_array(const char* key) {
    JsonValue* a = g_db.root.find(key);
    if (!a || !a->is_arr()) a = &g_db.root.set(key, JsonValue::make_arr());
    return a;
}

void db_status(const std::string& text) {
    if (g_db.lbl_status) ev_set_text(g_db.lbl_status, text);
}

void db_load() {
    g_db.root = JsonValue::make_obj();
    std::string text;
    if (read_file(db_path(), text) && !text.empty()) {
        try {
            g_db.root = json_parse(text);
        } catch (const std::exception&) {
            g_db.root = JsonValue::make_obj();
        }
    }
    db_array("actors");
    db_array("items");
    db_array("enemies");   // Etappe 19i: Kämpfe
    db_array("troops");
    if (!g_db.root.find("gold")) g_db.root.set("gold", JsonValue::make_num(0, true));
    g_db.dirty = false;
}

// Truppen-Feld "enemies": Array aus IDs -> komma-getrennter Text.
std::string db_join_ids(const JsonValue& e) {
    const JsonValue* a = e.find("enemies");
    if (!a || !a->is_arr()) return "";
    std::string out;
    for (const JsonValue& v : a->arr) {
        if (!v.is_str()) continue;
        if (!out.empty()) out += ", ";
        out += v.str;
    }
    return out;
}

// komma-getrennter Text -> Array aus IDs (Whitespace wird getrimmt).
JsonValue db_split_ids(const std::string& text) {
    JsonValue arr = JsonValue::make_arr();
    std::string tok;
    for (char c : text) {
        if (c == ',' || c == ';') {
            const size_t b = tok.find_first_not_of(" \t\r\n");
            if (b != std::string::npos)
                arr.arr.push_back(JsonValue::make_str(
                    tok.substr(b, tok.find_last_not_of(" \t\r\n") - b + 1)));
            tok.clear();
        } else {
            tok += c;
        }
    }
    const size_t b = tok.find_first_not_of(" \t\r\n");
    if (b != std::string::npos)
        arr.arr.push_back(JsonValue::make_str(
            tok.substr(b, tok.find_last_not_of(" \t\r\n") - b + 1)));
    return arr;
}

// Formular je Reiter beschriften (Feldanzahl sichtbar, Rest versteckt).
void db_form_layout() {
    int count = 0;
    const DbField* fields = db_fields(g_db.tab, count);
    for (int i = 0; i < DB_MAX_ROWS; ++i) {
        const bool used = i < count;
        ShowWindow(g_db.lbl[i], used ? SW_SHOW : SW_HIDE);
        ShowWindow(g_db.ed[i], used ? SW_SHOW : SW_HIDE);
        EnableWindow(g_db.ed[i], used ? TRUE : FALSE);
        if (used) SetWindowTextW(g_db.lbl[i], fields[i].label);
    }
}

void db_refresh_lists() {
    if (!g_db.win) return;
    wchar_t buf[200];
    SendMessageW(g_db.list, LB_RESETCONTENT, 0, 0);
    const JsonValue* arr = db_array(db_tab_key(g_db.tab));
    for (int i = 0; i < static_cast<int>(arr->arr.size()); ++i) {
        const JsonValue& e = arr->arr[i];
        switch (g_db.tab) {
            case 1:
                std::swprintf(buf, 200, L"%d. %ls  [%ls]", i + 1,
                              utf8_to_wide(ev_str_of(e, "name", "?")).c_str(),
                              utf8_to_wide(ev_str_of(e, "kind", "-")).c_str());
                break;
            case 2:
                std::swprintf(buf, 200, L"%d. %ls  (HP %ls)", i + 1,
                              utf8_to_wide(ev_str_of(e, "name", "?")).c_str(),
                              utf8_to_wide(ev_num_str(ev_num_of(e, "hp"), 0)).c_str());
                break;
            case 3: {
                const JsonValue* en = e.find("enemies");
                const int n = (en && en->is_arr())
                                  ? static_cast<int>(en->arr.size()) : 0;
                std::swprintf(buf, 200, L"%d. %ls  (%d Gegner)", i + 1,
                              utf8_to_wide(ev_str_of(e, "name", "?")).c_str(), n);
                break;
            }
            default:
                std::swprintf(buf, 200, L"%d. %ls (%ls)", i + 1,
                              utf8_to_wide(ev_str_of(e, "name", "?")).c_str(),
                              utf8_to_wide(ev_str_of(e, "class", "-")).c_str());
                break;
        }
        const int idx = static_cast<int>(
            SendMessageW(g_db.list, LB_ADDSTRING, 0,
                        reinterpret_cast<LPARAM>(buf)));
        SendMessageW(g_db.list, LB_SETITEMDATA, idx,
                     static_cast<LPARAM>(static_cast<INT_PTR>(i)));
    }

    // Auswahl wiederherstellen.
    if (g_db.sel >= 0 && g_db.sel < static_cast<int>(arr->arr.size())) {
        SendMessageW(g_db.list, LB_SETCURSEL, g_db.sel, 0);
    } else {
        g_db.sel = -1;
    }
}

void db_refresh_fields() {
    if (!g_db.win) return;
    int count = 0;
    const DbField* fields = db_fields(g_db.tab, count);
    const JsonValue* arr = db_array(db_tab_key(g_db.tab));
    const bool valid = g_db.sel >= 0 && g_db.sel < static_cast<int>(arr->arr.size());
    for (int i = 0; i < DB_MAX_ROWS; ++i) {
        EnableWindow(g_db.ed[i], (valid && i < count) ? TRUE : FALSE);
        if (!valid) SetWindowTextW(g_db.ed[i], L"");
    }
    EnableWindow(GetDlgItem(g_db.win, IDDB_DEL), valid ? TRUE : FALSE);

    if (g_db.ed_gold) {
        SetWindowTextW(g_db.ed_gold,
                       utf8_to_wide(ev_num_str(ev_num_of(g_db.root, "gold"), 0)).c_str());
    }
    if (!valid) return;

    const JsonValue& e = arr->arr[g_db.sel];
    for (int i = 0; i < count; ++i) {
        const DbField& f = fields[i];
        if (std::strcmp(f.key, "enemies") == 0) {
            ev_set_text(g_db.ed[i], db_join_ids(e));
        } else if (f.num) {
            ev_set_text(g_db.ed[i], ev_num_str(ev_num_of(e, f.key), f.prec));
        } else {
            ev_set_text(g_db.ed[i], ev_str_of(e, f.key));
        }
    }
}

void db_refresh() {
    g_db.syncing = true;
    db_form_layout();
    db_refresh_lists();
    db_refresh_fields();
    g_db.syncing = false;
}

// Formular in den Eintrag zurückschreiben.
void db_apply(bool mark_dirty) {
    JsonValue* arrp = db_array(db_tab_key(g_db.tab));
    if (g_db.sel < 0 || g_db.sel >= static_cast<int>(arrp->arr.size())) return;
    JsonValue& e = arrp->arr[g_db.sel];
    int count = 0;
    const DbField* fields = db_fields(g_db.tab, count);
    for (int i = 0; i < count; ++i) {
        const DbField& f = fields[i];
        if (std::strcmp(f.key, "enemies") == 0) {
            e.set("enemies", db_split_ids(ev_text(g_db.ed[i])));
        } else if (f.num) {
            e.set(f.key, JsonValue::make_num(ev_number(g_db.ed[i], ev_num_of(e, f.key)),
                                             f.prec == 0));
        } else {
            e.set(f.key, JsonValue::make_str(ev_text(g_db.ed[i])));
        }
    }
    if (g_db.tab == 0) {
        // Felder ohne Formularzeile behalten (level/exp wie seit 17g).
        e.set("level", JsonValue::make_num(
            ev_num_of(e, "level") > 0 ? ev_num_of(e, "level") : 1.0, true));
        if (!e.find("exp")) e.set("exp", JsonValue::make_num(0, true));
    }
    if (g_db.ed_gold) {
        g_db.root.set("gold", JsonValue::make_num(
            ev_number(g_db.ed_gold, ev_num_of(g_db.root, "gold")), true));
    }
    if (mark_dirty) {
        g_db.dirty = true;
        db_refresh_lists();
        db_status("Geändert - noch nicht gespeichert");
    }
}

void db_add() {
    db_apply(true);
    JsonValue* arr = db_array(db_tab_key(g_db.tab));
    const char* prefix = "held";
    if (g_db.tab == 1) prefix = "item";
    if (g_db.tab == 2) prefix = "gegner";
    if (g_db.tab == 3) prefix = "trupp";
    int n = 1;
    std::string id;
    for (;;) {
        id = std::string(prefix) + "_" + std::to_string(n);
        bool used = false;
        for (const JsonValue& e : arr->arr) {
            if (ev_str_of(e, "id") == id) { used = true; break; }
        }
        if (!used) break;
        ++n;
    }
    JsonValue e = JsonValue::make_obj();
    e.set("id", JsonValue::make_str(id));
    e.set("name", JsonValue::make_str(id));
    switch (g_db.tab) {
        case 1:
            e.set("kind", JsonValue::make_str("heal"));
            e.set("power", JsonValue::make_num(20, true));
            e.set("price", JsonValue::make_num(10, true));
            e.set("start", JsonValue::make_num(0, true));
            e.set("desc", JsonValue::make_str(""));
            break;
        case 2:   // Gegner (19i-Felder; color/desc bleiben optional im JSON)
            e.set("hp", JsonValue::make_num(30, true));
            e.set("mp", JsonValue::make_num(0, true));
            e.set("atk", JsonValue::make_num(10, true));
            e.set("def", JsonValue::make_num(8, true));
            e.set("spi", JsonValue::make_num(5, true));
            e.set("agi", JsonValue::make_num(8, true));
            e.set("exp", JsonValue::make_num(10, true));
            e.set("gold", JsonValue::make_num(0, true));
            e.set("drop", JsonValue::make_str(""));
            e.set("drop_rate", JsonValue::make_num(0.0, false));
            break;
        case 3:   // Truppen
            e.set("enemies", JsonValue::make_arr());
            e.set("desc", JsonValue::make_str(""));
            break;
        default:  // Helden
            e.set("class", JsonValue::make_str("Krieger"));
            e.set("level", JsonValue::make_num(1, true));
            e.set("hp", JsonValue::make_num(100, true));
            e.set("mp", JsonValue::make_num(20, true));
            e.set("atk", JsonValue::make_num(10, true));
            e.set("def", JsonValue::make_num(8, true));
            e.set("spi", JsonValue::make_num(6, true));
            e.set("agi", JsonValue::make_num(8, true));
            e.set("exp", JsonValue::make_num(0, true));
            break;
    }
    arr->arr.push_back(std::move(e));
    g_db.sel = static_cast<int>(arr->arr.size()) - 1;
    g_db.dirty = true;
    db_refresh();
    db_status("Neuen Eintrag angelegt - Speichern nicht vergessen");
}

void db_delete() {
    JsonValue* arr = db_array(db_tab_key(g_db.tab));
    if (g_db.sel < 0 || g_db.sel >= static_cast<int>(arr->arr.size())) return;
    arr->arr.erase(arr->arr.begin() + g_db.sel);
    if (g_db.sel >= static_cast<int>(arr->arr.size())) {
        g_db.sel = static_cast<int>(arr->arr.size()) - 1;
    }
    g_db.dirty = true;
    db_refresh();
    db_status("Eintrag gelöscht - Speichern nicht vergessen");
}

void db_save() {
    db_apply(false);
    if (write_file(db_path(), json_write(g_db.root))) {
        g_db.dirty = false;
        db_status("Gespeichert: database.json");
    } else {
        db_status("FEHLER: database.json konnte nicht geschrieben werden");
    }
}

HWND db_control(const wchar_t* cls, const wchar_t* text, DWORD style,
                int x, int y, int w, int h, int id) {
    HWND c = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style,
                             x, y, w, h, g_db.win,
                             reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                             GetModuleHandleW(nullptr), nullptr);
    SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(g.font), TRUE);
    return c;
}

LRESULT CALLBACK database_proc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_COMMAND: {
            const int id = LOWORD(wp);
            const int code = HIWORD(wp);
            if (g_db.syncing) return 0;

            if (code == LBN_SELCHANGE && id == IDDB_LIST) {
                if (g_db.sel >= 0) db_apply(true);   // alten Eintrag sichern
                g_db.sel = static_cast<int>(SendMessageW(g_db.list, LB_GETCURSEL, 0, 0));
                db_refresh_lists();
                db_refresh_fields();
                return 0;
            }
            if (id == IDDB_TAB_H || id == IDDB_TAB_I ||
                id == IDDB_TAB_E || id == IDDB_TAB_T) {
                db_apply(true);
                g_db.tab = (id == IDDB_TAB_I) ? 1
                         : (id == IDDB_TAB_E) ? 2
                         : (id == IDDB_TAB_T) ? 3 : 0;
                g_db.sel = -1;
                db_refresh();
                return 0;
            }
            if (id == IDDB_NEW)    { db_add(); return 0; }
            if (id == IDDB_DEL)    { db_delete(); return 0; }
            if (id == IDDB_SAVE)   { db_save(); return 0; }
            if (id == IDDB_RELOAD) {
                db_load();
                g_db.sel = -1;
                db_refresh();
                db_status("Vom Datenträger neu geladen");
                return 0;
            }
            if (id == IDDB_CLOSE)  { DestroyWindow(h); return 0; }
            return 0;
        }
        case WM_CLOSE:
            DestroyWindow(h);
            return 0;
        case WM_DESTROY:
            if (g_db.win == h) {
                g_db.win = nullptr;
                g_db.list = nullptr;
                g_db.ed_gold = nullptr;
                for (int i = 0; i < DB_MAX_ROWS; ++i) {
                    g_db.ed[i] = nullptr;
                    g_db.lbl[i] = nullptr;
                }
                g_db.lbl_status = nullptr;
                g_db.sel = -1;
            }
            return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

void open_database_dialog() {
    if (g_db.win) {
        SetForegroundWindow(g_db.win);
        return;
    }
    if (g.project_dir.empty()) return;

    WNDCLASSW wc{};
    wc.lpfnWndProc = database_proc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"RuneEditorDbDlg";
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
    RegisterClassW(&wc);

    RECT mr;
    GetWindowRect(g.main, &mr);
    g_db.win = CreateWindowExW(WS_EX_DLGMODALFRAME, L"RuneEditorDbDlg",
                               L"Datenbank (Helden / Items / Gegner / Truppen)",
                               WS_POPUP | WS_CAPTION | WS_SYSMENU,
                               mr.left + 140, mr.top + 40, 716, 768,
                               g.main, nullptr, GetModuleHandleW(nullptr), nullptr);

    db_load();

    // Reiter + Gold
    db_control(L"Button", L"Helden", BS_AUTORADIOBUTTON | WS_GROUP,
               12, 10, 90, 24, IDDB_TAB_H);
    db_control(L"Button", L"Items", BS_AUTORADIOBUTTON,
               104, 10, 80, 24, IDDB_TAB_I);
    db_control(L"Button", L"Gegner", BS_AUTORADIOBUTTON,
               186, 10, 90, 24, IDDB_TAB_E);
    db_control(L"Button", L"Truppen", BS_AUTORADIOBUTTON,
               278, 10, 90, 24, IDDB_TAB_T);
    db_control(L"Static", L"Gold:", 0, 396, 13, 40, 20, 0);
    g_db.ed_gold = db_control(L"Edit", L"0", WS_BORDER | ES_RIGHT,
                              438, 10, 80, 24, IDDB_GOLD);
    SendMessageW(GetDlgItem(g_db.win, IDDB_TAB_H), BM_SETCHECK, BST_CHECKED, 0);

    // Liste links
    g_db.list = db_control(L"ListBox", nullptr,
                           WS_VSCROLL | WS_BORDER | LBS_NOTIFY | LBS_HASSTRINGS,
                           12, 44, 260, 560, IDDB_LIST);
    db_control(L"Button", L"Neu", BS_PUSHBUTTON, 12, 612, 80, 28, IDDB_NEW);
    db_control(L"Button", L"Löschen", BS_PUSHBUTTON, 96, 612, 90, 28, IDDB_DEL);
    db_control(L"Button", L"Schließen", BS_PUSHBUTTON, 192, 612, 80, 28, IDDB_CLOSE);

    // Formular rechts: DB_MAX_ROWS Zeilen Label + Edit (Tabelle je Reiter)
    for (int i = 0; i < DB_MAX_ROWS; ++i) {
        const int y = 44 + i * 46;
        g_db.lbl[i] = db_control(L"Static", L"", 0, 292, y, 250, 18, IDDB_L1 + i);
        g_db.ed[i] = db_control(L"Edit", nullptr, WS_BORDER | ES_LEFT,
                                292, y + 18, 380, 24, IDDB_F1 + i);
    }

    g_db.lbl_status = db_control(L"Static", L"", 0, 292, 648, 396, 20, IDDB_STATUS);
    db_control(L"Button", L"Speichern", BS_PUSHBUTTON, 292, 674, 120, 30, IDDB_SAVE);
    db_control(L"Button", L"Neu laden", BS_PUSHBUTTON, 420, 674, 120, 30, IDDB_RELOAD);

    g_db.tab = 0;
    g_db.sel = db_array("actors")->arr.empty() ? -1 : 0;
    db_refresh();
    db_status("database.json: " + db_path());

    ShowWindow(g_db.win, SW_SHOW);
    SetForegroundWindow(g_db.win);
}

bool database_editor_focused() {
    if (!g_db.win) return false;
    HWND fg = GetForegroundWindow();
    return fg == g_db.win || (fg && IsChild(g_db.win, fg));
}

// ============================================================================
//  Etappe 17h: Sound-Test + Playtest
//
//  Der Editor linkt die Sound-Engine (waveOut) mit; die Spiel-Sounds werden
//  beim Boot in Ruby angemeldet (game/editor/boot.rb -> Sounds.register_all).
//  Der Sound-Test-Dialog listet sie und spielt sie ab. "Playtest starten"
//  speichert die Karte und startet Rune3D.exe als eigenen Prozess.
// ============================================================================
enum {
    IDSND_LIST     = 8101,
    IDSND_PLAY     = 8102,
    IDSND_LOOP     = 8103,   // Musik als Dauerschleife an/aus
    IDSND_STOP     = 8104,
    IDSND_VOL      = 8105,
    IDSND_CLOSE    = 8106,
    IDSND_STATUS   = 8107,
};

// Dieselben Namen wie game/engine/sounds.rb (Sounds.register_all).
const wchar_t* const SND_NAMES[7] = {
    L"schritt", L"klick", L"jingle", L"ablehnen", L"sprung", L"landung", L"musik"
};

struct SoundDlgState {
    HWND win = nullptr;
    HWND list = nullptr;
    HWND ed_vol = nullptr;
    HWND lbl_status = nullptr;
    bool musik_loop = false;   // Loop-Zustand lässt sich nicht abfragen
};
static SoundDlgState g_snd;

void snd_status(const std::string& text) {
    if (g_snd.lbl_status)
        SetWindowTextW(g_snd.lbl_status, utf8_to_wide(text).c_str());
}

float snd_volume() {
    if (!g_snd.ed_vol) return 1.0f;
    wchar_t buf[32];
    buf[0] = 0;
    GetWindowTextW(g_snd.ed_vol, buf, 32);
    const std::string s8 = wide_to_utf8(buf);
    const float v = static_cast<float>(std::atof(s8.c_str()));
    return (v > 0.0f && v <= 1.0f) ? v : 1.0f;
}

void snd_play_selected() {
    const int sel = static_cast<int>(
        SendMessageW(g_snd.list, LB_GETCURSEL, 0, 0));
    if (sel < 0 || sel >= 7) {
        snd_status("Kein Sound ausgewählt");
        return;
    }
    wchar_t wname[32];
    wname[0] = 0;
    SendMessageW(g_snd.list, LB_GETTEXT, sel,
                 reinterpret_cast<LPARAM>(wname));
    const std::string name = wide_to_utf8(wname);
    core::sound::play(name, snd_volume());
    snd_status(std::string("Spiele: ") + name);
}

void snd_toggle_loop() {
    // Musik ist der einzige Loop-Sound des Spiels.
    if (g_snd.musik_loop) {
        core::sound::stop("musik");
        g_snd.musik_loop = false;
        snd_status("Musik gestoppt");
    } else {
        core::sound::loop("musik", snd_volume() * 0.6f);
        g_snd.musik_loop = true;
        snd_status("Musik als Schleife");
    }
}

LRESULT CALLBACK soundtest_proc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_COMMAND: {
            const int id = LOWORD(wp);
            if (id == IDSND_PLAY) snd_play_selected();
            else if (id == IDSND_LOOP) snd_toggle_loop();
            else if (id == IDSND_STOP) {
                core::sound::stop("musik");
                snd_status("Gestoppt");
            }
            else if (id == IDSND_CLOSE) DestroyWindow(h);
            return 0;
        }
        case WM_CLOSE:
            DestroyWindow(h);
            return 0;
        case WM_DESTROY:
            if (g_snd.win == h) {
                g_snd.win = nullptr;
                g_snd.list = nullptr;
                g_snd.ed_vol = nullptr;
                g_snd.lbl_status = nullptr;
                core::sound::stop("musik");
            }
            return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

void open_soundtest_dialog() {
    if (g_snd.win) {
        SetForegroundWindow(g_snd.win);
        return;
    }

    WNDCLASSW wc{};
    wc.lpfnWndProc = soundtest_proc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"RuneEditorSoundDlg";
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
    RegisterClassW(&wc);

    RECT mr;
    GetWindowRect(g.main, &mr);
    g_snd.win = CreateWindowExW(WS_EX_DLGMODALFRAME, L"RuneEditorSoundDlg",
                                L"Sound-Test", WS_POPUP | WS_CAPTION | WS_SYSMENU,
                                mr.left + 260, mr.top + 120, 360, 380,
                                g.main, nullptr, GetModuleHandleW(nullptr), nullptr);

    auto ctl = [&](const wchar_t* cls, const wchar_t* text, DWORD style,
                   int x, int y, int w, int h, int id) {
        HWND c = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style,
                                 x, y, w, h, g_snd.win,
                                 reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                                 GetModuleHandleW(nullptr), nullptr);
        SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(g.font), TRUE);
        return c;
    };

    ctl(L"Static", L"SOUND-DATEIEN", 0, 12, 8, 200, 20, 0);
    g_snd.list = ctl(L"ListBox", nullptr,
                     WS_VSCROLL | WS_BORDER | LBS_NOTIFY | LBS_HASSTRINGS,
                     12, 30, 200, 240, IDSND_LIST);
    for (const wchar_t* n : SND_NAMES) {
        SendMessageW(g_snd.list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(n));
    }
    SendMessageW(g_snd.list, LB_SETCURSEL, 2, 0);   // jingle

    ctl(L"Static", L"Lautstärke (0.0-1.0)", 0, 224, 33, 120, 20, 0);
    g_snd.ed_vol = ctl(L"Edit", L"1.0", WS_BORDER | ES_LEFT,
                       224, 54, 70, 24, IDSND_VOL);

    ctl(L"Button", L"Abspielen", BS_PUSHBUTTON, 224, 100, 110, 30, IDSND_PLAY);
    ctl(L"Button", L"Musik an/aus", BS_PUSHBUTTON, 224, 136, 110, 30, IDSND_LOOP);
    ctl(L"Button", L"Stopp", BS_PUSHBUTTON, 224, 172, 110, 30, IDSND_STOP);
    ctl(L"Button", L"Schließen", BS_PUSHBUTTON, 224, 300, 110, 28, IDSND_CLOSE);

    g_snd.lbl_status = ctl(L"Static", L"", 0, 12, 340, 330, 20, IDSND_STATUS);
    snd_status("Sound-Engine: " +
               std::string(core::sound::ready() ? "bereit" : "NICHT verfügbar"));

    ShowWindow(g_snd.win, SW_SHOW);
    SetForegroundWindow(g_snd.win);
}

bool soundtest_focused() {
    if (!g_snd.win) return false;
    HWND fg = GetForegroundWindow();
    return fg == g_snd.win || (fg && IsChild(g_snd.win, fg));
}

// Karte speichern und das Spiel als eigenen Prozess starten (Etappe 17h).
void start_playtest() {
    if (g.cur_map >= 0) {
        if (!save_map()) {
            MessageBoxW(g.main, L"Karte konnte nicht gespeichert werden - "
                                L"Playtest abgebrochen.",
                        L"RuneEditor", MB_OK | MB_ICONERROR);
            return;
        }
    }

    const std::wstring exe = utf8_to_wide(g.root) + L"\\build\\Rune3D.exe";
    std::wstring args = L"\"build\\Rune3D.exe\" --project " +
                        utf8_to_wide(g.project_name);

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(exe.c_str(), args.data(), nullptr, nullptr, FALSE,
                        0, nullptr, nullptr, &si, &pi)) {
        MessageBoxW(g.main, L"build\\Rune3D.exe konnte nicht gestartet werden. "
                            L"Zürst build.ps1 ausführen.",
                    L"RuneEditor", MB_OK | MB_ICONERROR);
        return;
    }
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    core::log_info("Playtest gestartet: build/Rune3D.exe --project " + g.project_name);
}

} // namespace rune::editor

int main(int argc, char** argv) {
    using namespace rune::editor;
    using namespace rune;

    const HINSTANCE hInst = GetModuleHandleW(nullptr);

    // Etappe 18a: scharfe Darstellung auf hohen DPI-Einstellungen
    // (ohne das skaliert Windows Fonts und Viewport unscharb).
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    // Etappe 18b: --project <name> wie beim Spiel; ohne Angabe wird der
    // Merker log/last_project.txt geladen, sonst "dorf".
    std::string cli_project;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--project" && i + 1 < argc) {
            cli_project = argv[++i];
        }
    }

    g.root = find_root();
    SetCurrentDirectoryA(g.root.c_str());
    CreateDirectoryA("log", nullptr);
    core::log_open("log\\editor.log");
    core::log_info("=== RuneEditor startet ===");
    core::log_info("Wurzel: " + g.root);

    // Etappe 19f: Daten-Check mit klarem Hinweis (siehe main.cpp). Ohne
    // game/-Ordner fehlt dem Editor boot.rb (Ruby-Szene), prefabs.json und
    // das Projekt dorf - die drei Folgefehler wirken sonst unerklärlich.
    if (GetFileAttributesA((g.root + "\\game").c_str()) == INVALID_FILE_ATTRIBUTES) {
        core::log_error(
            "Engine-Daten fehlen: kein game/-Ordner unter der Wurzel. "
            "Bitte den KOMPLETTEN Build entpacken (EXEs + game/ + projekte/ "
            "in einen Ordner) - einzelne EXEs finden ihre Daten nicht.");
    }

    // WIC (Bildimport) braucht COM; D3D und Ruby stören sich nicht daran.
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    // Kopfloser Import für Tests/Werkzeuge:
    //   RuneEditor.exe --import-tex <bild> [name]
    if (argc >= 3 && std::string(argv[1]) == "--import-tex") {
        std::string tex_project = "dorf";
        for (int i = 3; i + 1 < argc; ++i) {
            if (std::string(argv[i]) == "--project") tex_project = argv[i + 1];
        }
        g.project_dir = g.root + "\\projekte\\" + tex_project + "\\data";
        g.project_name = tex_project;
        mat_load();
        std::string name = argc >= 4 ? argv[3] : std::string();
        const bool ok = mat_import_file(argv[2], name);
        if (ok) std::printf("OK: Textur '%s' importiert\n", name.c_str());
        else    std::printf("FEHLER: Import fehlgeschlagen (%s)\n", argv[2]);
        return ok ? 0 : 1;
    }

    // Gemeinsame Steuerelemente (Win32-Design)
    INITCOMMONCONTROLSEX icc{ sizeof(icc), ICC_STANDARD_CLASSES | ICC_BAR_CLASSES |
                                             ICC_TREEVIEW_CLASSES |
                                             ICC_LISTVIEW_CLASSES };
    InitCommonControlsEx(&icc);

    g.font = CreateFontW(-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                         DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                         CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    // Menü
    g.menu = CreateMenu();
    HMENU file_menu = CreatePopupMenu();
    AppendMenuW(file_menu, MF_STRING, ID_MENU_SAVE, L"Karte speichern\tStrg+S");
    AppendMenuW(file_menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(file_menu, MF_STRING, ID_MENU_PROJECT, L"Projekt wechseln...");
    AppendMenuW(file_menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(file_menu, MF_STRING, ID_MENU_EXIT, L"Beenden");
    HMENU map_menu = CreatePopupMenu();     // Karten werden dynamisch angehängt
    g.menu_map = map_menu;
    AppendMenuW(map_menu, MF_STRING, ID_MENU_MAPNEW, L"Neue Karte...");
    AppendMenuW(map_menu, MF_STRING, ID_MENU_MAPPROPS, L"Eigenschaften...\tF2");
    AppendMenuW(map_menu, MF_STRING, ID_MENU_MAPDEL, L"Karte löschen");
    AppendMenuW(map_menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(map_menu, MF_STRING | MF_GRAYED, 0,
                L"Karten weiter unten in diesem Menü");
    HMENU ev_menu = CreatePopupMenu();
    AppendMenuW(ev_menu, MF_STRING, ID_MENU_EVENTS, L"Event-Editor...\tF6");
    HMENU sc_menu = CreatePopupMenu();
    AppendMenuW(sc_menu, MF_STRING, ID_MENU_SCRIPTS, L"Script-Editor...\tF11");
    HMENU tex_menu = CreatePopupMenu();
    AppendMenuW(tex_menu, MF_STRING, ID_MENU_IMPORT_TEX, L"Grafik importieren...");
    AppendMenuW(tex_menu, MF_STRING, ID_MENU_TILESET, L"Tileset-Editor...\tF7");
    AppendMenuW(tex_menu, MF_STRING, ID_MENU_MATERIALS, L"Materialbase...\tF3");
    HMENU db_menu = CreatePopupMenu();
    AppendMenuW(db_menu, MF_STRING, ID_MENU_DATABASE,
                L"Datenbank (Helden/Items/Gegner/Truppen)...\tF9");
    HMENU extra_menu = CreatePopupMenu();     // Etappe 17h
    AppendMenuW(extra_menu, MF_STRING, ID_MENU_SOUNDTEST, L"Sound-Test...\tF8");
    AppendMenuW(extra_menu, MF_STRING, ID_MENU_PLAYTEST, L"Playtest starten...\tF4");
    AppendMenuW(extra_menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(extra_menu, MF_STRING, ID_MENU_PROBLEMS, L"Probleme...\tF10");
    HMENU view_menu = CreatePopupMenu();
    AppendMenuW(view_menu, MF_STRING, ID_MENU_RESETCAM, L"Kamera zurücksetzen");
    AppendMenuW(view_menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(view_menu, MF_STRING | MF_GRAYED, 0,
                L"Mausrad = Zoom (Dolly) | Shift+Rad = Höhe");
    AppendMenuW(view_menu, MF_STRING | MF_GRAYED, 0,
                L"Rechte Maustaste ziehen = Umsehen");
    AppendMenuW(view_menu, MF_STRING | MF_GRAYED, 0,
                L"G = Raster | Esc = Auswahl frei | Entf = löschen");
    HMENU help_menu = CreatePopupMenu();
    AppendMenuW(help_menu, MF_STRING, ID_MENU_HELP, L"Bedienung...\tF1");
    AppendMenuW(g.menu, MF_POPUP, reinterpret_cast<UINT_PTR>(file_menu), L"&Datei");
    AppendMenuW(g.menu, MF_POPUP, reinterpret_cast<UINT_PTR>(map_menu), L"&Karte");
    AppendMenuW(g.menu, MF_POPUP, reinterpret_cast<UINT_PTR>(ev_menu), L"&Events");
    AppendMenuW(g.menu, MF_POPUP, reinterpret_cast<UINT_PTR>(sc_menu), L"&Scripts");
    AppendMenuW(g.menu, MF_POPUP, reinterpret_cast<UINT_PTR>(tex_menu), L"&Grafik");
    AppendMenuW(g.menu, MF_POPUP, reinterpret_cast<UINT_PTR>(db_menu), L"&Datenbank");
    AppendMenuW(g.menu, MF_POPUP, reinterpret_cast<UINT_PTR>(extra_menu), L"E&xtras");
    AppendMenuW(g.menu, MF_POPUP, reinterpret_cast<UINT_PTR>(view_menu), L"&Ansicht");
    AppendMenuW(g.menu, MF_POPUP, reinterpret_cast<UINT_PTR>(help_menu), L"&Hilfe");

    // Etappe 19c: Kontextmenü für Viewport und Objektliste (Rechtsklick).
    g.ctx_obj = CreatePopupMenu();
    AppendMenuW(g.ctx_obj, MF_STRING, ID_CTX_PLACE, L"Objekt hier setzen");
    AppendMenuW(g.ctx_obj, MF_STRING, ID_CTX_FOCUS, L"Kamera darauf richten");
    AppendMenuW(g.ctx_obj, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(g.ctx_obj, MF_STRING, ID_CTX_DUP, L"Objekt duplizieren");
    AppendMenuW(g.ctx_obj, MF_STRING, ID_CTX_DEL, L"Objekt löschen");

    // Hauptfenster
    WNDCLASSW wc{};
    wc.lpfnWndProc = main_proc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"RuneEditorMain";
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    // Fix Fullbild-Resize: Ohne Hintergrund-Pinsel löscht Windows den Fenster-
    // Hintergrund NIE - beim Maximieren/ Vergröessern neu aufgedeckte Stellen
    // (Ränder zwischen den Controls) blieben schwarz/altes Bild stehen, weil
    // kein Control sie bemalt. BTNFACE passt zu den Controls (wie in den
    // Dialog-Klassen des Editors).
    wc.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
    RegisterClassW(&wc);

    WNDCLASSW wcv{};
    wcv.lpfnWndProc = view_proc;
    wcv.hInstance = hInst;
    wcv.lpszClassName = L"RuneEditorView";
    wcv.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wcv.hbrBackground = CreateSolidBrush(RGB(16, 24, 32));
    RegisterClassW(&wcv);

    RECT adj{ 0, 0, 1440, 880 };
    AdjustWindowRect(&adj, WS_OVERLAPPEDWINDOW, TRUE);
    g.main = CreateWindowExW(0, L"RuneEditorMain", L"RuneEditor",
                             WS_OVERLAPPEDWINDOW,
                             CW_USEDEFAULT, CW_USEDEFAULT,
                             adj.right - adj.left, adj.bottom - adj.top,
                             nullptr, g.menu, hInst, nullptr);
    if (!g.main) return 1;

    // Etappe 18a: eigenes Fenster-Icon (Runenstein, prozedural gezeichnet).
    set_window_icons(g.main, draw_icon_editor);

    // Etappe 17h: Werkzeugleiste unter dem Menü.
    create_toolbar(g.main);

    // Etappe 18k: Kartenbaum links oben (XP-Kartenliste).
    make_control(L"Static", L"KARTEN", 0, 8, 8, 180, 20, IDC_LBL_MAPS);
    g.map_tree = make_control(WC_TREEVIEWW, nullptr,
                              WS_BORDER | TVS_HASBUTTONS | TVS_HASLINES |
                              TVS_LINESATROOT | TVS_EDITLABELS |
                              TVS_SHOWSELALWAYS | WS_TABSTOP,
                              8, 30, 198, 130, IDC_MAPTREE);

    // Etappe 19c: linke Spalte als drei benannte Gruppen wie in den anderen
    // Dialogen des Editors: erst der Modus (Radios, genau einer aktiv), dann
    // der Objekt-Typ (Prefab-Liste füllt den Rest), dann die Aktionen.
    make_control(L"Button", L"Werkzeug (Modus)", BS_GROUPBOX,
                 4, 168, 206, 156, IDC_GRP_TOOL);
    make_control(L"Button", L"Setzen", BS_AUTORADIOBUTTON | WS_GROUP,
                 14, 186, 186, 24, IDC_BTN_PLACE);
    make_control(L"Button", L"Auswählen", BS_AUTORADIOBUTTON,
                 14, 212, 186, 24, IDC_BTN_SELECT);
    make_control(L"Button", L"Verschieben", BS_AUTORADIOBUTTON,
                 14, 238, 186, 24, IDC_BTN_MOVE);
    make_control(L"Button", L"Boden malen", BS_AUTORADIOBUTTON,
                 14, 264, 186, 24, IDC_BTN_PAINT);
    // Etappe 18l: Berge/Senken direkt an der Stelle malen (Default flach, da
    // neue Karten Amplitude 0 haben); Shift+Klick trägt ab.
    make_control(L"Button", L"Höhe malen", BS_AUTORADIOBUTTON,
                 14, 290, 186, 24, IDC_BTN_HEIGHT);
    make_control(L"Button", L"Objekt-Typ (Prefab)", BS_GROUPBOX | WS_GROUP,
                 4, 330, 206, 300, IDC_GRP_PREFAB);
    g.list_prefab = make_control(L"ListBox", nullptr,
                                 WS_VSCROLL | WS_BORDER | LBS_NOTIFY | LBS_HASSTRINGS,
                                 14, 348, 186, 272, IDC_PREFAB_LIST);
    make_control(L"Button", L"Objekt-Aktionen", BS_GROUPBOX,
                 4, 636, 206, 96, IDC_GRP_ACT);
    make_control(L"Button", L"Drehen (R)", BS_PUSHBUTTON | WS_GROUP,
                 14, 654, 88, 28, IDC_BTN_ROTATE);
    make_control(L"Button", L"Löschen", BS_PUSHBUTTON,
                 108, 654, 92, 28, IDC_BTN_DELETE);
    make_control(L"Button", L"Duplizieren", BS_PUSHBUTTON,
                 14, 686, 186, 28, IDC_BTN_DUPLICATE);
    make_control(L"Button", L"Undo (Strg+Z)", BS_PUSHBUTTON,
                 8, 738, 96, 30, IDC_BTN_UNDO);
    make_control(L"Button", L"Redo (Strg+Y)", BS_PUSHBUTTON,
                 110, 738, 96, 30, IDC_BTN_REDO);

    // Rechte Spalte: Objektliste + Eigenschaften
    make_control(L"Static", L"OBJEKTE", 0, 1188, 8, 200, 20, IDC_LBL_OBJECTS);
    // Etappe 19c: Objektliste als ListView im Bericht-Modus: Spalten statt
    // einzeiliger Kurztexte, sortierbar per Kopflick, ganze Zeile markierbar.
    g.list_objs = make_control(WC_LISTVIEWW, nullptr,
                               WS_BORDER | LVS_REPORT | LVS_SINGLESEL |
                               LVS_SHOWSELALWAYS | WS_TABSTOP,
                               1188, 30, 242, 400, IDC_OBJ_LIST);
    ListView_SetExtendedListViewStyle(g.list_objs,
                                      LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES |
                                      LVS_EX_DOUBLEBUFFER);
    {
        struct { const wchar_t* titel; int breite; } cols[] = {
            { L"#", 34 },       { L"Objekt", 118 }, { L"X", 52 },
            { L"Z", 52 },       { L"Drehung", 56 }, { L"Material", 78 },
        };
        for (int c = 0; c < 6; ++c) {
            LVCOLUMNW lc{};
            lc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
            lc.fmt = LVCFMT_LEFT;
            lc.pszText = const_cast<wchar_t*>(cols[c].titel);
            lc.cx = cols[c].breite;
            ListView_InsertColumn(g.list_objs, c, &lc);
        }
    }
    make_control(L"Button", L"Eigenschaften", BS_GROUPBOX,
                 1184, 613, 250, 238, IDC_GRP_PROPS);
    make_control(L"Static", L"X", 0, 1194, 648, 60, 20, IDC_LBL_X);
    g.ed_x = make_control(L"Edit", nullptr, WS_BORDER | ES_RIGHT,
                          1264, 644, 158, 24, IDC_ED_X);
    make_control(L"Static", L"Z", 0, 1194, 680, 60, 20, IDC_LBL_Z);
    g.ed_z = make_control(L"Edit", nullptr, WS_BORDER | ES_RIGHT,
                          1264, 676, 158, 24, IDC_ED_Z);
    make_control(L"Static", L"Drehung", 0, 1194, 712, 60, 20, IDC_LBL_ROT);
    g.ed_rot = make_control(L"Edit", nullptr, WS_BORDER | ES_RIGHT,
                            1264, 708, 158, 24, IDC_ED_ROT);
    make_control(L"Static", L"Material", 0, 1194, 744, 60, 20, IDC_LBL_TEX);
    g.ed_tex = make_control(L"Edit", nullptr, WS_BORDER | ES_LEFT,
                            1264, 740, 158, 24, IDC_ED_TEX);
    make_control(L"Button", L"Übernehmen", BS_PUSHBUTTON,
                 1194, 774, 228, 30, IDC_BTN_APPLY);

    // Viewport + Statusleiste
    g.view = CreateWindowExW(WS_EX_CLIENTEDGE, L"RuneEditorView", nullptr,
                             WS_CHILD | WS_VISIBLE,
                             214, toolbar_height(), 960, 830, g.main,
                             reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_VIEWPORT)),
                             hInst, nullptr);
    g.status = CreateWindowExW(0, STATUSCLASSNAMEW, nullptr,
                               WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
                               0, 0, 0, 0, g.main,
                               reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_STATUSBAR)),
                               hInst, nullptr);
    apply_font(g.status);
    const int sb_parts[4] = { 160, 130, 220, -1 };
    SendMessageW(g.status, SB_SETPARTS, 4, reinterpret_cast<LPARAM>(sb_parts));

    // Renderer auf den Viewport
    static gfx::Renderer renderer;
    static world::Terrain terrain;
    g.renderer = &renderer;
    g.terrain = &terrain;
    RECT vr;
    GetClientRect(g.view, &vr);
    if (!renderer.init(g.view, std::max(1, static_cast<int>(vr.right)),
                       std::max(1, static_cast<int>(vr.bottom)))) {
        core::log_error("Renderer konnte nicht initialisiert werden");
        MessageBoxW(g.main, L"3D-Renderer konnte nicht gestartet werden.",
                    L"RuneEditor", MB_OK | MB_ICONERROR);
        return 1;
    }
    rune::app().renderer = &renderer;
    rune::app().terrain = &terrain;
    renderer.set_sky(0x3D6FB5, 0xBFD8EC);
    renderer.set_sun(math::Vec3{ -0.55f, 0.75f, 0.25f }, 0.35f);

    // Etappe 17h: Sound-Engine für den Sound-Test-Dialog (waveOut).
    if (!core::sound::init()) {
        core::log_warn("Starte ohne Sound weiter (Sound-Test deaktiviert)");
    }

    // Texturen: einmalig per eingebettetem Ruby erzeugen (game/editor/boot.rb).
    if (!rune::ruby::init(0, nullptr, g.root)) {
        core::log_warn("Ruby nicht gestartet - Editor läuft mit untexturierten Objekten");
    } else {
        rune::ruby::load_script("game\\editor\\boot.rb");
    }
    load_prefabs();
    renderer.set_terrain_tex(renderer.texture_index("gras"));

    // Projekt + Karten (Etappe 18b: Auswahl statt festes "dorf").
    {
        std::string startup = "dorf";
        if (!cli_project.empty()) {
            startup = cli_project;   // explizite Vorgabe gewinnt
        } else {
            std::string last;
            if (read_file(g.root + "\\log\\last_project.txt", last)) {
                // Zeilenumbrüche abschneiden
                while (!last.empty() && (last.back() == '\r' || last.back() == '\n')) {
                    last.pop_back();
                }
                if (!last.empty()) startup = last;
            }
        }
        if (!switch_project(startup)) {
            // Rückfall: dorf, sonst erstes gefundenes Projekt.
            if (startup != "dorf" && switch_project("dorf")) {
                core::log_warn("Projekt '" + startup + "' fehlt, zurück auf 'dorf'");
            } else if (g.project_dir.empty()) {
                const std::vector<std::string> all = scan_projects();
                if (!all.empty()) switch_project(all.front());
            }
        }
    }

    // Prefab-Liste füllen
    for (const Prefab& p : g.prefabs) {
        const std::wstring label = utf8_to_wide(p.name);
        SendMessageW(g.list_prefab, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
    }
    if (!g.prefabs.empty()) SendMessageW(g.list_prefab, LB_SETCURSEL, 0, 0);
    set_mode(1);   // Etappe 19c: Startmodus Setzen (Radio-Haken zentral)
    status_mode(L"Setzen-Modus");
    sync_object_list();
    sync_property_fields();
    update_title();

    // Etappe 18a: Drag&Drop von PNG/BMP erlauben (Import als Textur).
    DragAcceptFiles(g.main, TRUE);

    ShowWindow(g.main, SW_SHOWDEFAULT);
    UpdateWindow(g.main);

    // ------------------------------------------------------ Hauptschleife
    double last = qpc_now();
    double fps_accum = 0.0;
    int fps_frames = 0;
    bool running = true;
    while (running) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) { running = false; break; }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (!running) break;

        const double now = qpc_now();
        float dt = static_cast<float>(now - last);
        last = now;
        if (dt > 0.1f) dt = 0.1f;
        if (dt < 0.0f) dt = 0.0f;

        update_camera(dt);
        handle_shortcuts();
        update_hover();
        render_frame();
        core::sound::update();   // Etappe 17h: Puffer des Sound-Tests füllen

        fps_accum += dt;
        ++fps_frames;
        if (fps_accum >= 0.5) {
            g.fps = static_cast<float>(fps_frames / fps_accum);
            fps_accum = 0.0;
            fps_frames = 0;
            update_status();
        }
    }

    rune::ruby::shutdown();
    renderer.shutdown();
    core::sound::shutdown();
    core::log_info("RuneEditor beendet");
    return 0;
}
