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
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdlib>   // Etappe 19k.2: std::atof (Block-Zonen-Feld)
#include <cstring>
#include <cwchar>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <vector>

#include "app.h"
#include "core/log.h"
#include "core/project_assets_win.h"
#include "core/sound.h"
#include "core/win_path.h"
#include "core/win_project_cli.h"
#include "editor/json.h"
#include "editor/database_form.h"
#include "editor/document_store_win.h"
#include "editor/encounter_check.h"
#include "editor/encounter_form.h"
#include "editor/event_form.h"
#include "editor/event_view_win.h"
#include "editor/listbox_win.h"
#include "editor/main_layout.h"
#include "editor/main_panel_win.h"
#include "editor/main_tree_win.h"
#include "editor/map_document.h"
#include "editor/map_props_form.h"
#include "editor/material_document.h"
#include "editor/number_format.h"
#include "editor/object_props_form.h"
#include "editor/placement_form.h"
#include "editor/playtest_win.h"
#include "editor/project_scan_win.h"
#include "editor/terrain_preview.h"
#include "editor/texture_path.h"
#include "editor/texture_preview.h"
#include "editor/tile_form.h"
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
    ID_MENU_PLACEMENT = 117,   // Etappe 19n: NPCs, Schilder und Objekte
    ID_MENU_MAPS_MORE  = 118,   // Trennleiste auch ohne Maus verstellen
    ID_MENU_PALETTE_MORE = 119,
    ID_MENU_LAYOUT_RESET = 120,
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
    // Kartenbaum unter der Palette; die Karten bleiben direkte Fensterkinder.
    IDC_MAPTREE       = 3027,
    IDC_LBL_MAPS      = 3028,
    IDC_BTN_HEIGHT    = 3029,
    IDC_GRP_TOOL      = 3030,
    IDC_GRP_PREFAB    = 3031,
    IDC_GRP_ACT       = 3032,
    IDC_LBL_PROPS_HINT = 3033,
    IDC_PANEL_PALETTE = 3034,
    IDC_PANEL_INSPECTOR = 3035,
    IDC_MAP_NEW_BUTTON = 3036,
    IDC_MAP_PROPS_BUTTON = 3037,
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
    std::string tex;       // Name statt Slot: Projekt-Overrides können wechseln
    float      tile = 0.0f;
};

struct Prefab {
    std::string id;
    std::string name;
    std::vector<PrefabPart> parts;
    float radius = 0.0f;   // grober Bodenradius (Nahe-Löschung, Anzeige)
};

// Das vollständige Materialdokument und seine sicheren Ansichten liegen in
// editor/material_document.h; unbekannte Einträge bleiben im JSON erhalten.

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
enum class MapTreeActionKind { None, Properties, Context, New };

struct MapTreeAction {
    MapTreeActionKind kind = MapTreeActionKind::None;
    int index = -1;
    std::uint64_t project_epoch = 0;
    std::string file;
    LPARAM position = 0;
};

struct Editor {
    HWND main = nullptr;
    HWND view = nullptr;
    HWND status = nullptr;
    HWND list_prefab = nullptr;
    HWND map_tree = nullptr;      // links unter der Palette
    HWND list_objs = nullptr;
    MainPanel palette;
    MainPanel inspector;
    MainWindowLayout main_layout;
    unsigned main_dpi = 96;
    int wanted_map_height = MAIN_DEFAULT_MAP_HEIGHT;
    bool splitter_drag = false;
    int splitter_grab = 0;
    bool map_props_compact = false;
    HWND ed_x = nullptr;
    HWND ed_z = nullptr;
    HWND ed_rot = nullptr;
    HWND ed_tex = nullptr;
    HFONT font = nullptr;          // Schrift der bestehenden Dialoge
    HFONT main_font = nullptr;     // eigene DPI-skalierte Hauptfenster-Schrift
    HMENU menu = nullptr;
    HMENU menu_map = nullptr;      // Karten-Untermenü (dynamisch)
    HWND   props_dlg = nullptr;    // offener Dialog (moduslos)
    int    dlg_mode = 0;           // 1 = Eigenschaften, 2 = Neue Karte

    gfx::Renderer*  renderer = nullptr;
    world::Terrain* terrain = nullptr;
    std::string terrain_error;       // Fehler des letzten tatsächlichen Mesh-Aufbaus
    std::string terrain_warning;     // z.B. Einzeltextur statt Splat-Atlas
    bool terrain_memory_error = false; // setzt auch ohne String-Allokation einen sichtbaren Fehler
    bool terrain_field_warning = false;
    bool atmosphere_warning = false;

    std::string root;                 // Engine-Wurzel (übergeordnetes Verz.)
    std::string project_dir;          // z.B. projekte/dorf
    std::string project_name;

    std::vector<std::string> map_files;   // Dateinamen in maps/
    std::vector<std::string> map_names;   // Anzeigenamen
    int cur_map = -1;
    std::uint64_t map_epoch = 0, project_epoch = 0; // auch A → B → A erkennen

    JsonValue  map_root;              // ganze Karte (events/npcs bleiben drin)
    bool map_disk_present = false;
    std::string map_disk_text;        // gelesene/zuletzt gespeicherte Dateibytes
    JsonValue* objects = nullptr;     // zeigt in map_root

    std::vector<Prefab> prefabs;
    std::vector<std::string> tex_names;   // Texturnamen der Prefabs (+gras)
    int  sel_prefab = 0;
    int  sel_obj = -1;
    ObjectPropsForm object_form;
    bool object_field_editable[4] = {};
    bool syncing_object_list = false;
    bool object_selection_queued = false;
    bool object_selection_resolving = false;
    bool map_selection_queued = false;
    bool map_selection_resolving = false;
    MapTreeAction map_action;
    bool map_action_queued = false;
    bool data_prompt_active = false;
    bool object_selection_delayed = false;
    bool map_selection_delayed = false;
    bool map_menu_delayed = false;
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
    bool drag_undo_pushed = false; // erst bei der ersten echten Bewegung
    bool drag_pointer_moved = false;
    int  obj_drag = -1;            // Index des gezogenen Objekts
    int  drag_mouse_x = 0, drag_mouse_y = 0;
    float drag_y = 0.0f;           // Höhenebene des gezogenen Objekts
    float drag_anchor_x = 0.0f, drag_anchor_z = 0.0f, drag_grid = 1.0f;
    JsonValue drag_original;       // Werte vor dem ersten Drag-Schritt
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

// Benachrichtigungen von ListView/TreeView zunächst beenden; Rückfragen und
// komplette Listen-Neubauten dürfen nicht innerhalb ihrer Callbacks laufen.
constexpr UINT WM_RECONCILE_OBJECT_SELECTION = WM_APP + 41;
constexpr UINT WM_OBJECT_LIST_FOCUS = WM_APP + 42;
constexpr UINT WM_OBJECT_LIST_CONTEXT = WM_APP + 43;
constexpr UINT WM_RECONCILE_MAP_SELECTION = WM_APP + 44;
constexpr UINT WM_REBUILD_MAP_MENU = WM_APP + 45;
constexpr UINT WM_TREE_ACTION = WM_APP + 46;
constexpr UINT WM_OBJECT_LIST_SORT = WM_APP + 48;
constexpr UINT WM_TREE_RENAME_WARNING = WM_APP + 49;
constexpr UINT WM_EVENT_SELECT_EVENT = WM_APP + 50;
constexpr UINT WM_EVENT_SELECT_PAGE = WM_APP + 51;
constexpr UINT WM_EVENT_SELECT_COMMAND = WM_APP + 52;

void event_requeue_selections();
void maptree_schedule_action();

void cancel_splitter_drag() {
    const bool had_capture = g.splitter_drag;
    g.splitter_drag = false;
    if (had_capture && GetCapture() == g.main) ReleaseCapture();
}

void cancel_object_drag() {
    const bool had_capture = g.dragging;
    g.dragging = false;
    g.drag_undo_pushed = false;
    g.drag_pointer_moved = false;
    g.obj_drag = -1;
    g.drag_original = JsonValue{};
    if (had_capture && GetCapture() == g.view) ReleaseCapture();
}

struct DataPromptWindows {
    HWND owner = nullptr;
    std::vector<HWND> disabled;
    bool failed = false;
};

BOOL CALLBACK collect_data_prompt_windows(HWND window, LPARAM param) {
    auto& state = *reinterpret_cast<DataPromptWindows*>(param);
    if (window != state.owner && IsWindowEnabled(window) &&
        (window == g.main || GetWindow(window, GW_OWNER) == g.main)) {
        // Keine C++-Exception aus einem Win32-Callback herauslaufen lassen.
        try { state.disabled.push_back(window); }
        catch (...) { state.failed = true; return FALSE; }
    }
    return TRUE;
}

// F3, Projektfenster usw. sind moduslos. Eine Daten-Rückfrage muss deshalb
// auch diese Fenster sperren, nicht nur ihren jeweiligen MessageBox-Owner.
// Zuvor gesperrte Fenster bleiben gesperrt (z. B. beim modalen F5-Dialog).
int data_prompt(HWND owner, const wchar_t* text, const wchar_t* title, UINT flags) {
    if (g.data_prompt_active) return IDCANCEL;
    cancel_splitter_drag();
    if (!owner || !IsWindow(owner) || !IsWindowEnabled(owner)) owner = g.main;
    DataPromptWindows windows;
    windows.owner = owner;
    g.data_prompt_active = true;
    if (!EnumThreadWindows(GetCurrentThreadId(), collect_data_prompt_windows,
                           reinterpret_cast<LPARAM>(&windows)) || windows.failed) {
        g.data_prompt_active = false;
        try { core::log_warn("Datenrückfrage: Fenster konnten nicht sicher gesperrt werden"); } catch (...) {}
        return IDCANCEL;
    }
    for (HWND window : windows.disabled) EnableWindow(window, FALSE);
    const int result = MessageBoxW(owner, text, title, flags);
    for (HWND window : windows.disabled) if (IsWindow(window)) EnableWindow(window, TRUE);
    g.data_prompt_active = false;
    if (g.object_selection_delayed) {
        g.object_selection_delayed = false;
        if (!g.object_selection_queued)
            g.object_selection_queued = PostMessageW(g.main, WM_RECONCILE_OBJECT_SELECTION, 0, 0) != 0;
    }
    if (g.map_selection_delayed) {
        g.map_selection_delayed = false;
        if (!g.map_selection_queued)
            g.map_selection_queued = PostMessageW(g.main, WM_RECONCILE_MAP_SELECTION, 0, 0) != 0;
    }
    if (g.map_menu_delayed) {
        g.map_menu_delayed = false;
        PostMessageW(g.main, WM_REBUILD_MAP_MENU, 0, 0);
    }
    event_requeue_selections();
    maptree_schedule_action();
    return result;
}

// Vorwärtsdeklarationen (Definitionen weiter unten)
void apply_map_atmosphere_editor();
void rebuild_map_menu();
void open_map_props();
void open_map_new();
void delete_current_map();
bool apply_props_from_dlg();
bool apply_new_from_dlg();
void push_undo();
void undo_action();
void redo_action();
void restore_objects(const std::string& text);
void update_title();
void sync_object_list();
void sync_property_fields();
bool object_form_pending();
bool apply_property_fields(bool refresh_list = true);
bool resolve_object_edits(bool refresh_list = true);
bool prepare_map_departure();

// Etappe 18b: Projekt-Auswahl im Editor (bisher fix auf "dorf").
bool switch_project(const std::string& name, bool departure_checked = false);
void open_project_dialog();
void scan_maps();
void rebuild_map_menu();
void status_mode(const wchar_t* text);

// Etappe 18k: Kartenbaum (XP-Kartenliste). Item-LPARAM = Karten-Index.
void maptree_fill();
void maptree_select_current();
int maptree_selected();
void maptree_update_buttons();

// Etappe 18c: Tileset-Editor.
void open_tileset_dialog();

// Etappe 18d: Splat-Boden (Boden-Flächen pro Karte).
void regen_terrain();
bool splat_edit(float cx, float cz, float sw, float sh,
               const std::string& tex, bool erase, bool snap);   // Etappe 18e
// Etappe 18l: Berge/Senken malen (Höhen-Pinsel im Karten-JSON).
float height_step();
int  height_edit(float cx, float cz, bool sink);

// Etappe 17d: Event-Editor (eigenes modusloses Fenster).
void open_event_editor();
void close_event_editor();
bool event_editor_open();
bool event_editor_focused();
bool event_editor_pending();
bool resolve_event_edits();
bool resolve_event_editor();

// Etappe 19n: Platzier-Dialog für NPCs, Schilder und Objekte.
void open_placement_dialog();

// Etappe 17e: Script-Editor (scripts.json des Projekts).
void open_script_editor();
bool script_editor_focused();

// Etappe 17f: Grafik-Import + Materialbase.
void open_import_texture();
void open_material_dialog();
void close_material_dialog();
bool material_editor_focused();

// Etappe 17g: Datenbank-Dialog (Helden/Items/Gold).
void open_database_dialog();
bool database_editor_open();
bool database_editor_focused();
bool database_request_close();

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

// Ein Editfeld vollständig auslesen; feste 64-/128-Zeichen-Puffer würden
// lange bestehende Daten beim Übernehmen still abschneiden.
std::string control_text(HWND control) {
    if (!control) return {};
    const int length = GetWindowTextLengthW(control);
    std::wstring text(static_cast<size_t>(std::max(0, length)) + 1, L'\0');
    const int copied = GetWindowTextW(control, text.data(), static_cast<int>(text.size()));
    text.resize(static_cast<size_t>(std::max(0, copied)));
    return wide_to_utf8(text);
}

// Für Objekt-Eingaben zusätzlich ungültige UTF-16-Surrogate ablehnen statt
// sie beim Konvertieren unbemerkt durch ein Ersatzzeichen zu verändern.
bool control_text_strict(HWND control, std::string& result) {
    if (!control) return false;
    const int length = GetWindowTextLengthW(control);
    if (length < 0 || length == INT_MAX) return false;
    std::wstring text(static_cast<size_t>(length) + 1, L'\0');
    const int copied = GetWindowTextW(control, text.data(), length + 1);
    if (copied != length) return false;
    if (copied == 0) { result.clear(); return true; }
    const int bytes = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), copied,
                                          nullptr, 0, nullptr, nullptr);
    if (bytes <= 0) return false;
    std::string out(static_cast<size_t>(bytes), '\0');
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), copied,
                            out.data(), bytes, nullptr, nullptr) != bytes) return false;
    result = std::move(out);
    return true;
}

// Projekt-/Kartennamen und Engine-Wurzel sind intern UTF-8. Vor jedem
// Windows-Dateizugriff wird strikt zu UTF-16 konvertiert.
bool file_path_to_wide(const std::string& path, std::wstring& out) {
    return core::win_path::to_wide(path, out);
}

bool read_file(const std::string& path, std::string& out) {
    return core::win_path::read_file(path, out);
}

bool file_path_exists(const std::string& path) {
    std::wstring wide_path;
    return file_path_to_wide(path, wide_path) &&
           GetFileAttributesW(wide_path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

// Die Transaktion nennt ihre eigenen, eventuell erhaltenen Dateinamen. Ein
// Backup darf nicht still verschwinden, wenn ein fremder Neuanleger die
// Rücksetzung des ursprünglichen Namens verhindert hat.
void document_result_notice(HWND owner, const wchar_t* title, assets::Result result,
                            const DocumentRecovery& recovery, const std::string& directory) {
    if (result == assets::Result::Ok && recovery.backup.empty() && recovery.temp.empty()) return;
    std::wstring message;
    if (result == assets::Result::Ok) {
        message = L"Der Vorgang wurde abgeschlossen, aber eine Sicherungsdatei blieb erhalten.";
    } else if (result == assets::Result::Conflict) {
        message = L"Die Datei wurde nach der Prüfung erneut geändert, entfernt oder neu angelegt. "
                  L"Es wurde keine fremde Datei überschrieben oder gelöscht. Bitte erneut prüfen.";
    } else if (result == assets::Result::Missing) {
        message = L"Die Datei oder ein benötigter Projektordner fehlt. Der Vorgang wurde abgebrochen.";
    } else if (result == assets::Result::Reparse) {
        message = L"Die Datei oder ein Projektordner ist ein Reparsepunkt (z. B. eine Junction). "
                  L"Dieser Zugriff wird zum Schutz der Daten abgebrochen.";
    } else if (result == assets::Result::Invalid) {
        message = L"Ungültiger Pfad oder Dateityp; Hardlinks sind für diese Aktion nicht erlaubt. "
                  L"Der Vorgang wurde abgebrochen.";
    } else if (result == assets::Result::TooLarge) {
        message = L"Für den vollständigen Dateivergleich steht nicht genügend Speicher zur Verfügung. "
                  L"Der Vorgang wurde abgebrochen.";
    } else {
        message = L"Der Dateizugriff ist gesperrt, fehlgeschlagen oder wird vom Dateisystem nicht unterstützt. "
                  L"Der Vorgang wurde abgebrochen.";
    }
    std::wstring wide_directory;
    const bool has_directory = file_path_to_wide(directory, wide_directory);
    const auto append_path = [&](const wchar_t* label, const std::wstring& name) {
        if (name.empty()) return;
        message += label;
        if (has_directory) message += wide_directory + L"\\";
        message += name;
        std::string utf8;
        if (assets::to_utf8(has_directory ? wide_directory + L"\\" + name : name, utf8))
            core::log_warn("Dokument-Wiederherstellungsdatei: " + utf8);
    };
    append_path(L"\r\n\r\nAlte Datei als Sicherung (bitte prüfen und ggf. wiederherstellen):\r\n", recovery.backup);
    append_path(L"\r\n\r\nNicht entfernte temporäre Datei:\r\n", recovery.temp);
    if (result != assets::Result::Ok)
        core::log_error(std::string("Dokument-Dateiaktion: ") + assets::result_text(result));
    data_prompt(owner, message.c_str(), title,
                MB_OK | (result == assets::Result::Ok ? MB_ICONWARNING : MB_ICONERROR));
}

bool write_file(const std::string& path, const std::string& data) {
    return core::win_path::write_file(path, data);
}

// Editor bevorzugt die Installation neben bzw. über seiner EXE.
std::string find_root() {
    std::string root;
    core::win_path::find_engine_root(false, root);
    return root;
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
                    part.tex = t->str;
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

// Beliebige Bilddatei (PNG/BMP/JPG) -> 8-Bit-RGBA, Zeilen von oben nach unten.
// Auch die komprimierte Eingabe ist begrenzt. WIC sieht die stabil gelesenen
// Bytes, und die Bilddimensionen werden vor Konverter und Pixelallokation geprüft.
bool decode_image_wic(const std::string& path, int& out_w, int& out_h,
                      std::vector<uint8_t>& out_rgba) {
    out_w = out_h = 0;
    out_rgba.clear();
    IWICImagingFactory* factory = nullptr;
    IWICStream* stream = nullptr;
    IWICBitmapDecoder* decoder = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICFormatConverter* conv = nullptr;
    std::string source; // bleibt bis nach dem Freigeben des WIC-Streams gültig
    bool ok = false;
    try {
        std::wstring wpath;
        if (assets::to_wide(path, wpath)) {
            assets::Handle input(CreateFileW(wpath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                             OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
            if (input.valid() && assets::read_locked(input.get(), world::TEXTURE_MAX_SOURCE_BYTES, source) == assets::Result::Ok &&
                !source.empty()) {
                HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                              IID_PPV_ARGS(&factory));
                if (SUCCEEDED(hr)) hr = factory->CreateStream(&stream);
                if (SUCCEEDED(hr)) hr = stream->InitializeFromMemory(
                    reinterpret_cast<BYTE*>(source.data()), static_cast<DWORD>(source.size()));
                if (SUCCEEDED(hr)) hr = factory->CreateDecoderFromStream(
                    stream, nullptr, WICDecodeMetadataCacheOnDemand, &decoder);
                if (SUCCEEDED(hr)) hr = decoder->GetFrame(0, &frame);
                UINT w = 0, h = 0;
                if (SUCCEEDED(hr)) hr = frame->GetSize(&w, &h);
                std::size_t bytes = 0;
                if (SUCCEEDED(hr) && w <= static_cast<UINT>(world::TEXTURE_MAX_DIMENSION) &&
                    h <= static_cast<UINT>(world::TEXTURE_MAX_DIMENSION) &&
                    world::texture_byte_count(static_cast<int>(w), static_cast<int>(h), bytes)) {
                    hr = factory->CreateFormatConverter(&conv);
                    if (SUCCEEDED(hr)) hr = conv->Initialize(frame, GUID_WICPixelFormat32bppRGBA,
                        WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);
                    if (SUCCEEDED(hr)) {
                        std::vector<uint8_t> pixels(bytes);
                        hr = conv->CopyPixels(nullptr, w * 4, static_cast<UINT>(pixels.size()), pixels.data());
                        if (SUCCEEDED(hr)) {
                            out_w = static_cast<int>(w);
                            out_h = static_cast<int>(h);
                            out_rgba = std::move(pixels);
                            ok = true;
                        }
                    }
                }
            }
        }
    } catch (const std::bad_alloc&) {
        core::log_error("Import: nicht genügend Speicher für das Bild");
    } catch (const std::length_error&) {
        core::log_error("Import: Bild überschreitet den Speicherbereich");
    }
    if (conv) conv->Release();
    if (frame) frame->Release();
    if (decoder) decoder->Release();
    if (stream) stream->Release();
    if (factory) factory->Release();
    return ok;
}

// Der vollständige Baum und die Originalbytes bleiben bis zu einem
// erfolgreichen, konfliktgeprüften Commit erhalten.
static JsonValue g_material_document;
static bool g_material_writable = false;
static bool g_material_disk_present = false;
static std::string g_material_disk_text;
static std::string g_material_error;
// Die beim Laden geprüften Ordnerhandles bleiben bis zum Projektwechsel offen.
// So kann ein zwischen Lesen und Speichern ausgetauschtes Projektverzeichnis
// nicht trotz identischer Manifestbytes als anderes Ziel akzeptiert werden.
static std::unique_ptr<assets::ProjectStore> g_material_store;
static std::uint64_t g_material_store_epoch = 0;
// Nur der kopflose Import erhält seinen Projektnamen direkt aus der UTF-16-CLI.
static std::wstring g_material_cli_project;

assets::Result mat_open_store(assets::ProjectStore& store) {
    std::wstring project = g_material_cli_project;
    if (project.empty() && !assets::to_wide(g.project_name, project)) return assets::Result::Invalid;
    if (!assets::component(project)) return assets::Result::Invalid;
    return store.open(g.root, &project);
}

void mat_error(const std::string& message) {
    g_material_error = message;
    core::log_warn("Material: " + message);
}

const char* mat_parse_problem(MaterialParse status) {
    switch (status) {
        case MaterialParse::InvalidJson: return "ungültiges JSON";
        case MaterialParse::InvalidRoot: return "textures ist keine Liste";
        case MaterialParse::InvalidUtf8: return "ungültiges UTF-8";
        case MaterialParse::Duplicate: return "doppelte Schlüssel oder Materialnamen";
        case MaterialParse::Nonfinite: return "nicht darstellbare Kommazahl";
        case MaterialParse::TooLarge: return "Datei oder Materialliste zu gross";
        case MaterialParse::TooDeep: return "JSON zu tief verschachtelt";
        case MaterialParse::Ok: return "";
    }
    return "unbekannter Fehler";
}

void mat_register(assets::ProjectStore& store, const MaterialInfo& m) {
    if (!g.renderer) return;
    std::size_t need = 0;
    if (!world::texture_byte_count(m.w, m.h, need)) return;
    std::string bytes;
    const assets::Result result = store.read_texture(m.file, need, bytes);
    if (result != assets::Result::Ok) {
        core::log_warn("Textur '" + m.name + "' nicht sicher lesbar: " + assets::result_text(result));
        return;
    }
    if (g.renderer->add_texture(m.name, m.w, m.h, reinterpret_cast<const uint8_t*>(bytes.data())) < 0)
        core::log_warn("Textur '" + m.name + "' konnte nicht im Renderer angemeldet werden");
}

// Fehlende Datei ist ein leerer NEUER Katalog; eine unlesbare oder defekte
// vorhandene Datei ist dagegen schreibgeschützt und wird nie ersetzt.
void mat_load() {
    g_material_store.reset();
    g_materials.clear();
    g_material_document = JsonValue{};
    g_material_writable = false;
    g_material_disk_present = false;
    g_material_disk_text.clear();
    g_material_error.clear();
    try {
        auto store = std::make_unique<assets::ProjectStore>();
        assets::Result result = mat_open_store(*store);
        if (result != assets::Result::Ok) {
            mat_error(std::string("Projektpfad nicht sicher: ") + assets::result_text(result));
            return;
        }
        std::string text;
        result = store->read_manifest(text);
        if (result == assets::Result::Missing) {
            g_material_document = JsonValue::make_obj();
            g_material_document.set("textures", JsonValue::make_arr());
            g_material_store_epoch = g.project_epoch;
            g_material_store = std::move(store);
            g_material_writable = true;
            return;
        }
        if (result != assets::Result::Ok) {
            mat_error(std::string("textures.json nicht sicher lesbar: ") + assets::result_text(result));
            return;
        }
        JsonValue parsed;
        const MaterialParse status = material_parse(text, parsed);
        if (status != MaterialParse::Ok) {
            mat_error(std::string("textures.json bleibt unverändert: ") + mat_parse_problem(status));
            return;
        }
        g_material_document = std::move(parsed);
        g_material_disk_present = true;
        g_material_disk_text = std::move(text);
        g_materials = material_entries(g_material_document);
        const JsonValue* list = material_array(g_material_document);
        if (list && list->arr.size() != g_materials.size())
            core::log_warn("Material: ungültige Einträge bleiben im Dokument erhalten und werden nicht geladen");
        for (const MaterialInfo& m : g_materials) mat_register(*store, m);
        core::log_info("Materialien geladen: " + std::to_string(g_materials.size()));
        g_material_store_epoch = g.project_epoch;
        g_material_store = std::move(store);
        g_material_writable = true;
    } catch (const std::bad_alloc&) {
        g_material_writable = false;
        mat_error("nicht genügend Speicher beim Laden");
    } catch (const std::length_error&) {
        g_material_writable = false;
        mat_error("Materialdatei überschreitet den Speicherbereich");
    }
}

bool mat_commit_with_store(assets::ProjectStore& store, JsonValue candidate) {
    if (!g_material_writable) {
        mat_error("Materialdatei ist schreibgeschützt; Projekt erneut öffnen und extern prüfen");
        return false;
    }
    std::string output = json_write(candidate);
    // Vor dem Commit alles Allokierende vorbereiten. Nach dem Dateitausch
    // dürfen weder ein OOM noch ein Rückbau die referenzierte RGBA entfernen.
    std::vector<MaterialInfo> prepared = material_entries(candidate);
    if (output.size() > world::TEXTURE_MAX_MANIFEST_BYTES) {
        mat_error("textures.json würde die Grössengrenze überschreiten");
        return false;
    }
    std::wstring recovery;
    const assets::Result result = store.save_manifest(g_material_disk_present, g_material_disk_text, output, recovery);
    if (result != assets::Result::Ok) {
        if (result == assets::Result::Conflict) g_material_writable = false;
        std::string message = std::string("nicht gespeichert: ") + assets::result_text(result);
        std::string recovery_utf8;
        if (!recovery.empty() && assets::to_utf8(recovery, recovery_utf8))
            message += "; Sicherung unter data\\" + recovery_utf8 + " erhalten";
        mat_error(message);
        return false;
    }
    g_material_document = std::move(candidate);
    g_material_disk_text = std::move(output);
    g_material_disk_present = true;
    g_materials = std::move(prepared);
    g_material_error.clear();
    if (!recovery.empty()) {
        try {
            std::string recovery_utf8;
            if (assets::to_utf8(recovery, recovery_utf8))
                core::log_warn("Material: Sicherung konnte nicht entfernt werden: data\\" + recovery_utf8);
        } catch (...) {} // erfolgreiche Änderung darf nicht nachträglich scheitern
    }
    return true;
}

bool mat_commit(JsonValue candidate) {
    try {
        if (!g_material_store || g_material_store_epoch != g.project_epoch) {
            mat_error("Projektverzeichnis nicht mehr gebunden; Projekt erneut öffnen");
            return false;
        }
        return mat_commit_with_store(*g_material_store, std::move(candidate));
    } catch (const std::bad_alloc&) {
        mat_error("nicht genügend Speicher; keine Änderung übernommen");
    } catch (const std::length_error&) {
        mat_error("Materialdaten überschreiten den Speicherbereich");
    }
    return false;
}

// Ein OOM zwischen Dateianlage und Katalog-Commit darf keine eigene
// unreferenzierte Datei still zurücklassen; niemals fremde Pfade löschen.
struct MatNewFileGuard {
    assets::Handle& file;
    const std::string& name;
    bool committed = false;
    ~MatNewFileGuard() {
        if (!committed && file.valid() && !assets::delete_owned_handle(file.get())) {
            try { core::log_warn("Material: unbenutzte neue Datei erhalten: textures\\" + name); }
            catch (...) {}
        }
    }
};

// Eine neue RGBA-Datei wird exklusiv angelegt und bleibt gesperrt, bis der
// Katalog erfolgreich gespeichert ist. Beim Fehlschlag nur das eigene Handle
// löschen; niemals einen fremden Pfad überschreiben oder löschen.
bool mat_store_texture(MaterialInfo material, const std::string& data, bool replace) {
    try {
        if (!g_material_writable) {
            if (g_material_error.empty()) mat_error("textures.json ist schreibgeschützt");
            return false;
        }
        g_material_error.clear();
        std::size_t count = 0;
        if (!material_name_valid(material.name) || !texture_file_is_flat(material.file) ||
            !world::texture_byte_count(material.w, material.h, count) || count != data.size()) {
            mat_error("Texturname, Dateiname oder Bildgrösse ungültig");
            return false;
        }
        const JsonValue* list = material_array(g_material_document);
        if (!list) { mat_error("Materialliste nicht verfügbar"); return false; }
        for (const JsonValue& entry : list->arr) {
            const JsonValue* name = entry.is_obj() ? entry.find("name") : nullptr;
            if (name && name->is_str() && name->str == material.name && !replace) {
                mat_error("Materialname bereits vorhanden; bestehende Daten bleiben erhalten");
                return false;
            }
        }
        if (!g_material_store || g_material_store_epoch != g.project_epoch) {
            mat_error("Projektverzeichnis nicht mehr gebunden; Projekt erneut öffnen");
            return false;
        }
        assets::ProjectStore& store = *g_material_store;
        assets::Handle created;
        std::string actual;
        assets::Result r = store.create_texture(material.file, data, actual, created);
        if (r != assets::Result::Ok) {
            mat_error(std::string("RGBA-Datei nicht angelegt: ") + assets::result_text(r));
            return false;
        }
        MatNewFileGuard guard{created, actual};
        material.file = actual;
        JsonValue candidate = g_material_document;
        if (!material_upsert(candidate, material) || !mat_commit_with_store(store, std::move(candidate))) {
            if (g_material_error.empty()) mat_error("ungültiger Alteintrag wurde nicht ersetzt");
            return false;
        }
        guard.committed = true;
        created.reset();
        try {
            if (g.renderer && g.renderer->add_texture(material.name, material.w, material.h,
                    reinterpret_cast<const uint8_t*>(data.data())) < 0)
                core::log_warn("Material gespeichert, aber im Renderer nicht angemeldet: " + material.name);
        } catch (...) {} // der Dateicommit ist bereits erfolgreich
        return true;
    } catch (const std::bad_alloc&) {
        mat_error("nicht genügend Speicher beim Import");
    } catch (const std::length_error&) {
        mat_error("Textur überschreitet den Speicherbereich");
    }
    return false;
}

// Dateiname -> eindeutiger Materialname (klein, nur a-z/0-9/_).
std::string mat_slug(std::string base) {
    const size_t slash = base.find_last_of("\\/");
    if (slash != std::string::npos) base = base.substr(slash + 1);
    const size_t dot = base.find_last_of('.');
    if (dot != std::string::npos) base = base.substr(0, dot);
    std::string cleaned;
    for (char c : base) {
        if (cleaned.size() >= 96) break;
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
        else if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) c = '_';
        if (c == '_' && (cleaned.empty() || cleaned.back() == '_')) continue;
        cleaned += c;
    }
    if (cleaned.empty()) cleaned = "textur";
    auto used = [&](const std::string& name) {
        const JsonValue* list = material_array(g_material_document);
        if (list) for (const JsonValue& entry : list->arr) {
            const JsonValue* n = entry.is_obj() ? entry.find("name") : nullptr;
            if (n && n->is_str() && n->str == name) return true;
        }
        return g.renderer && g.renderer->texture_index(name) >= 0;
    };
    std::string candidate = cleaned;
    int n = 2;
    while (used(candidate)) candidate = cleaned + "_" + std::to_string(n++);
    return candidate;
}

// Bild importieren: decode, neue Datei und nichtdestruktiven Katalog committen.
bool mat_import_file(const std::string& src, std::string& out_name) {
    if (g.project_dir.empty() || !g_material_writable) {
        if (g_material_error.empty())
            mat_error("kein sicher geöffnetes Projekt oder schreibgeschützte Materialdatei");
        return false;
    }
    int w = 0, h = 0;
    std::vector<uint8_t> rgba;
    if (!decode_image_wic(src, w, h, rgba)) {
        mat_error("Bild nicht sicher lesbar, zu gross oder ungültig: " + src);
        return false;
    }
    MaterialInfo m;
    m.name = out_name.empty() ? mat_slug(src) : out_name;
    m.file = m.name + ".rgba";
    m.source = src;
    m.w = w;
    m.h = h;
    m.tile = 2.0;
    m.imported = true;
    const std::string data(reinterpret_cast<const char*>(rgba.data()), rgba.size());
    if (!mat_store_texture(m, data, false)) return false;
    out_name = m.name;
    core::log_info("Textur importiert: " + m.name + " (" + std::to_string(w) + "x" +
                   std::to_string(h) + ") aus " + src);
    if (g.renderer && g.cur_map >= 0) regen_terrain();
    return true;
}

// Verfügbare Materialnamen (Prefab-Texturen + importierte), sortiert.
std::vector<std::string> mat_all_names() {
    std::vector<std::string> names = g.tex_names;
    if (std::find(names.begin(), names.end(), "gras") == names.end()) names.push_back("gras");
    // Auch noch nicht gespeicherte F7-Kacheln gehören zur aktuellen
    // Projekt-Ebene; nach Projektwechsel verschwinden sie automatisch.
    if (g.renderer) for (const std::string& name : g.renderer->texture_names()) {
        if (std::find(names.begin(), names.end(), name) == names.end()) names.push_back(name);
    }
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
            if (!object_props_viewable(o))
                issue_report(1, w, "Position/Drehung ungültig, mehrfach oder extrem; keine Vorschau");
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

    // Etappe 19l: Zufallskämpfe (auch im kopflosen C++-Test geprüft).
    // Die Existenz der Truppen prüft erst Battle.start gegen die Datenbank.
    issue_scan_encounters(g.map_root.find("encounters"), mapname + " encounters", issue_report);
    issue_scan_placement_heights(g.map_root, mapname, issue_report);
    const TerrainPreview terrain = terrain_preview_read(g.map_root);
    if (!terrain.ok())
        issue_report(1, mapname + " terrain", terrain.error + "; keine Gelände-Vorschau");
    else if (g.terrain_memory_error)
        issue_report(1, mapname + " terrain", "Speicher für die Gelände-Vorschau nicht verfügbar; kein Gelände");
    else if (!g.terrain_error.empty())
        issue_report(1, mapname + " terrain", g.terrain_error + "; keine Gelände-Vorschau");
    if (!g.terrain_warning.empty()) issue_report(1, mapname + " terrain", g.terrain_warning);
    for (const std::string& warning : terrain.warnings) issue_report(1, mapname + " terrain", warning);
    const AtmospherePreview atmosphere = atmosphere_preview_read(g.map_root);
    for (const std::string& warning : atmosphere.warnings) issue_report(1, mapname + " Atmosphäre", warning);

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
    // Auch ein inzwischen reparierter Vorschaufehler muss verschwinden:
    // der letzte Scan kann sowohl älter als ein Fehler als auch älter als
    // dessen Behebung sein. F10 liest deshalb immer den aktuellen Zustand.
    issues_scan_map();
    if (g_pb.win) {
        pb_refresh();
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

bool load_map(int index, bool departure_checked = false) {
    if (index < 0 || index >= static_cast<int>(g.map_files.size())) return false;
    if (!departure_checked && g.cur_map >= 0 && !prepare_map_departure()) return false;
    const std::string directory = g.project_dir + "\\maps";
    const std::string path = directory + "\\" + g.map_files[index];
    DocumentStore store;
    DocumentSnapshot snapshot;
    assets::Result result = store.open(g.root, g.project_name, true, g.map_files[index]);
    if (result == assets::Result::Ok) result = store.observe(snapshot);
    if (result == assets::Result::Ok && !snapshot.present) result = assets::Result::Missing;
    if (result != assets::Result::Ok) {
        issue_report(2, "Karte", "nicht sicher lesbar: " + path);
        core::log_error("Karte nicht sicher lesbar: " + path);
        document_result_notice(GetActiveWindow(), L"Karte nicht geladen", result, {}, directory);
        return false;
    }
    JsonValue loaded;
    const MapParseStatus parsed = map_parse_document(snapshot.bytes, loaded);
    if (parsed != MapParseStatus::Ok) {
        const wchar_t* reason = L"Ungültiges JSON.";
        switch (parsed) {
            case MapParseStatus::InvalidRoot: reason = L"Die Kartenwurzel ist kein Objekt."; break;
            case MapParseStatus::InvalidUtf8: reason = L"Die Karte enthält ungültiges UTF-8 oder Unicode."; break;
            case MapParseStatus::Nonfinite: reason = L"Eine Kommazahl ist nicht darstellbar und würde zu null werden."; break;
            case MapParseStatus::DuplicateObjects: reason = L"Der objects-Schlüssel ist mehrfach vorhanden."; break;
            case MapParseStatus::InvalidObjects: reason = L"objects ist keine Liste und würde sonst überschrieben."; break;
            default: break;
        }
        const std::wstring message = std::wstring(reason) +
            L"\r\nDie Karte wurde nicht geladen oder überschrieben. Bitte die Datei extern prüfen.";
        data_prompt(GetActiveWindow(), message.c_str(), L"Karte nicht geladen", MB_OK | MB_ICONERROR);
        issue_report(2, "Karte", "nicht sicher lesbar: " + path);
        core::log_error("Karte nicht sicher lesbar: " + path);
        return false;
    }
    cancel_object_drag();
    close_event_editor();   // Zeiger in map_root werden ungültig
    g.map_root = std::move(loaded);
    ++g.map_epoch;
    g.objects = g.map_root.find("objects");
    if (!g.objects || !g.objects->is_arr()) {
        // Karte ohne Objekte: leeres Array anlegen.
        g.objects = &g.map_root.set("objects", JsonValue::make_arr());
    }
    g.cur_map = index;
    g.sel_obj = -1;
    g.dirty = false;
    g.map_disk_present = true;
    g.map_disk_text = std::move(snapshot.bytes);
    // Schnappschüsse gehören zur vorherigen Karte und dürfen deren Objekte
    // niemals in die neu geladene Karte zurückschreiben.
    g.undo_stack.clear();
    g.redo_stack.clear();

    // Terrain neu erzeugen (rechteckige Karten: width/height, alt: size).
    // Etappe 18d: regen_terrain behandelt Ein-Textur- und Splat-Boden.
    regen_terrain();

    // Kamera auf eine gute Übersichtsposition (Blick Richtung Kartenmitte).
    const float span = g.terrain->ready() ? std::max(g.terrain->width(), g.terrain->height()) : 240.0f;
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
    if (g.cur_map < 0 || g.cur_map >= static_cast<int>(g.map_files.size())) return false;
    cancel_object_drag();
    // Moduslose Formulare und gepostete F6-Auswahlwechsel dürfen beim
    // Speichern nicht still ausgelassen werden.
    if (!resolve_event_editor()) return false;
    if (object_form_pending() && !apply_property_fields()) return false;
    if (!editor_json_utf8(g.map_root) || !editor_json_serializable(g.map_root)) {
        data_prompt(GetActiveWindow(), L"Die Karte enthält ungültiges Unicode oder eine nicht darstellbare Kommazahl. "
                                       L"Sie wird nicht überschrieben.",
                    L"Karte nicht gespeichert", MB_OK | MB_ICONERROR);
        return false;
    }
    const std::string directory = g.project_dir + "\\maps";
    const std::string path = directory + "\\" + g.map_files[g.cur_map];
    std::string output = json_write(g.map_root);
    DocumentStore store;
    DocumentRecovery recovery;
    assets::Result result = store.open(g.root, g.project_name, true, g.map_files[g.cur_map]);
    if (result == assets::Result::Ok) {
        const DocumentOutcome outcome = document_save_confirmed(
            store, g.map_disk_present, g.map_disk_text, output, [] {
                return data_prompt(GetActiveWindow(),
                    L"Die Kartendatei wurde seit dem Laden oder Speichern extern geändert oder entfernt. "
                    L"Trotzdem mit dem Editorstand überschreiben?",
                    L"Karten-Dateikonflikt", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) == IDYES;
            }, recovery);
        if (outcome.cancelled) return false;
        result = outcome.result;
    }
    if (result != assets::Result::Ok) {
        issue_report(2, "Karte", "nicht sicher schreibbar: " + path);
        core::log_error("Karte nicht sicher schreibbar: " + path);
        document_result_notice(GetActiveWindow(), L"Karte nicht gespeichert", result, recovery, directory);
        return false;
    }
    g.map_disk_present = true;
    g.map_disk_text = std::move(output);
    g.dirty = false;
    core::log_info("Karte gespeichert: " + path);
    document_result_notice(GetActiveWindow(), L"Karte gespeichert", result, recovery, directory);
    return true;
}

// Vor Karten-/Projektwechsel zuerst das Formular klären, danach den
// gespeicherten Kartenstand. Auch bei fehlgeschlagenem Speichern wird nicht
// einfach die nächste Karte geladen.
bool prepare_map_departure() {
    cancel_object_drag();
    if (!resolve_event_editor() || !resolve_object_edits()) return false;
    if (!g.dirty) return true;
    HWND owner = GetActiveWindow();
    if (!owner) owner = g.main;
    const int choice = data_prompt(owner,
        L"Änderungen an der aktuellen Karte speichern?\r\n\r\n"
        L"Ja: speichern   Nein: verwerfen   Abbrechen: zurück",
        L"Karte wechseln", MB_YESNOCANCEL | MB_ICONQUESTION | MB_DEFBUTTON3);
    if (choice == IDNO) return true;
    if (choice != IDYES) return false;
    if (save_map()) return true;
    data_prompt(owner, L"Die Karte konnte nicht gespeichert werden. Der Wechsel wurde abgebrochen.",
                L"Karte wechseln", MB_OK | MB_ICONERROR);
    return false;
}

// terrain an der Wurzel nur eindeutig und als Objekt bearbeiten. Das erstmalige
// Anhängen kann sowohl den Objektzeiger als auch F6-Befehlszeiger invalidieren.
JsonValue* writable_terrain() {
    HWND owner = GetActiveWindow();
    if (!owner) owner = g.main;
    if (!object_props_field_unique(g.map_root, "terrain")) {
        MessageBoxW(owner, L"terrain ist im Karten-JSON mehrfach vorhanden und kann hier nicht eindeutig geändert werden.",
                    L"Terrain", MB_OK | MB_ICONWARNING);
        return nullptr;
    }
    JsonValue* terrain = g.map_root.find("terrain");
    if (terrain && !terrain->is_obj()) {
        MessageBoxW(owner, L"terrain ist kein Objekt. Die Altdaten bleiben erhalten; bitte die Karte extern prüfen.",
                    L"Terrain", MB_OK | MB_ICONWARNING);
        return nullptr;
    }
    if (!terrain) {
        if (event_editor_open()) {
            MessageBoxW(owner, L"Bitte zuerst den Event-Editor schließen, bevor terrain angelegt wird.",
                        L"Terrain", MB_OK | MB_ICONINFORMATION);
            return nullptr;
        }
        terrain = &g.map_root.set("terrain", JsonValue::make_obj());
        g.objects = g.map_root.find("objects");
    }
    return terrain;
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
    std::string project, map_file, texture_before, tile_before;
    std::uint64_t map_epoch = 0, project_epoch = 0;
    bool texture_indices_safe = true, splat_indices_safe = true;
};
static TileDlgState g_ts;

// Text eines Kind-Controls (Edit) im Tileset-Fenster als UTF-8.
std::string ts_text(int id) {
    HWND control = GetDlgItem(g_ts.win, id);
    if (!control) return {};
    const int length = GetWindowTextLengthW(control);
    if (length <= 0) return {};
    std::wstring buf(static_cast<size_t>(length) + 1, L'\0');
    GetWindowTextW(control, &buf[0], static_cast<int>(buf.size()));
    return wide_to_utf8(buf.c_str());
}

bool ts_same_project() {
    if (g_ts.project_epoch == g.project_epoch && g_ts.project == g.project_dir) return true;
    MessageBoxW(g_ts.win, L"Das Projekt wurde gewechselt. Bitte das Tileset-Fenster "
                          L"schließen und erneut öffnen, bevor Daten geändert werden.",
                L"Terrain", MB_OK | MB_ICONWARNING);
    return false;
}

bool ts_same_map() {
    if (!ts_same_project()) return false;
    if (g_ts.map_epoch == g.map_epoch && g.cur_map >= 0 &&
        static_cast<size_t>(g.cur_map) < g.map_files.size() &&
        g_ts.map_file == g.map_files[static_cast<size_t>(g.cur_map)]) return true;
    MessageBoxW(g_ts.win, L"Die Karte wurde gewechselt oder neu geladen. Bitte das Tileset-Fenster "
                          L"schließen und erneut öffnen, bevor Daten geändert werden.",
                L"Terrain", MB_OK | MB_ICONWARNING);
    return false;
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
        std::size_t expected = 0;
        if (g.renderer && g.renderer->texture_rgba(g_ts.sel, px, w, h) &&
            world::texture_byte_count(w, h, expected) && px.size() == expected) {
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
                texture_rgba_to_bgra(px.data(), static_cast<std::uint8_t*>(bits), px.size() / 4);
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
    g_ts.texture_indices_safe = true;
    LRESULT selected = -1;
    for (std::size_t i = 0; i < g_ts.names.size(); ++i) {
        const std::wstring label = utf8_to_wide(g_ts.names[i]);
        const LRESULT item = SendMessageW(g_ts.list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
        if (item == LB_ERR || item == LB_ERRSPACE ||
            SendMessageW(g_ts.list, LB_SETITEMDATA, static_cast<WPARAM>(item), static_cast<LPARAM>(i)) == LB_ERR) {
            g_ts.texture_indices_safe = false;
            continue;
        }
        if (g_ts.names[i] == g_ts.sel) selected = item;
    }
    if (selected >= 0) SendMessageW(g_ts.list, LB_SETCURSEL, static_cast<WPARAM>(selected), 0);
    if (!g_ts.texture_indices_safe)
        status_mode(L"Texturliste unvollständig — Auswahl gesperrt; F7 erneut öffnen");
}

// Etappe 18d: Splat-Flächen-Liste im Tileset-Dialog neu füllen (Format:
// "textur @ (x, z) 30x12"). Auswahl merken.
void ts_fill_splats() {
    const int sel_before = static_cast<int>(
        SendMessageW(g_ts.splist, LB_GETCURSEL, 0, 0));
    SendMessageW(g_ts.splist, LB_RESETCONTENT, 0, 0);
    g_ts.splat_indices_safe = true;
    const JsonValue* t = g.map_root.find("terrain");
    const JsonValue* list = t && t->is_obj() ? t->find("splats") : nullptr;
    const SplatListModel model = splat_list_model(list);
    // Auch ungültige Einträge sichtbar lassen. Der Listeneintrag trägt den
    // Originalindex; gefilterte Vorschauen dürfen nie einen anderen löschen.
    for (const SplatListRow& row : model.rows) {
        std::wstring label;
        if (row.valid) {
            const world::SplatRect& s = row.splat;
            std::wstring name = utf8_to_wide(s.tex);
            if (name.size() > 60) name = name.substr(0, 57) + L"...";
            wchar_t buf[160] = {};
            swprintf(buf, 160, L"%ls @ (%g, %g) %gx%g", name.c_str(), s.x, s.z, s.w, s.h);
            label = buf;
        } else label = L"#" + std::to_wstring(row.original_index + 1) + L" [ungültige Fläche]";
        const LRESULT item = SendMessageW(g_ts.splist, LB_ADDSTRING, 0,
                                           reinterpret_cast<LPARAM>(label.c_str()));
        if (item == LB_ERR || item == LB_ERRSPACE ||
            SendMessageW(g_ts.splist, LB_SETITEMDATA, static_cast<WPARAM>(item),
                         static_cast<LPARAM>(row.original_index)) == LB_ERR)
            g_ts.splat_indices_safe = false;
    }
    if (model.omitted) {
        const std::wstring more = L"[" + std::to_wstring(model.omitted) + L" weitere ausgeblendet]";
        const LRESULT item = SendMessageW(g_ts.splist, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(more.c_str()));
        if (item == LB_ERR || item == LB_ERRSPACE ||
            SendMessageW(g_ts.splist, LB_SETITEMDATA, static_cast<WPARAM>(item), static_cast<LPARAM>(-1)) == LB_ERR)
            g_ts.splat_indices_safe = false;
    }
    if (!g_ts.splat_indices_safe)
        status_mode(L"Flächenliste unvollständig — Löschen gesperrt; F7 erneut öffnen");
    if (sel_before >= 0 && sel_before < SendMessageW(g_ts.splist, LB_GETCOUNT, 0, 0))
        SendMessageW(g_ts.splist, LB_SETCURSEL, sel_before, 0);
}

LRESULT CALLBACK tileset_dlg_proc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    // Auch bereits gepostete Aktionen dürfen während einer Datenrückfrage
    // keine Karte oder Liste ändern (MessageBox pumpt Nachrichten weiter).
    if (g.data_prompt_active) {
        if (msg == WM_COMMAND) {
            if (LOWORD(wp) == IDTS_LIST &&
                (HIWORD(wp) == LBN_SELCHANGE || HIWORD(wp) == LBN_DBLCLK)) {
                const auto it = std::find(g_ts.names.begin(), g_ts.names.end(), g_ts.sel);
                const LPARAM original = g_ts.texture_indices_safe && it != g_ts.names.end()
                    ? static_cast<LPARAM>(it - g_ts.names.begin()) : -1;
                listbox_select_data(g_ts.list, original);
            }
            return 0;
        }
        if (msg == WM_CLOSE) return 0;
    }
    switch (msg) {
        case WM_PAINT:
            if (h == g_ts.preview) { draw_tileset_preview(h); return 0; }
            break;
        case WM_COMMAND: {
            const int id = LOWORD(wp);
            const int code = HIWORD(wp);
            // F5 arbeitet auf einer vollständigen Kartenkopie; eine bereits
            // gepostete F7-Aktion darf diese Kopie nicht überholen und später
            // vom F5-OK wieder überschrieben werden. F2 bearbeitet dagegen
            // nur seine Felder auf dem aktuellen Baum.
            if (g.props_dlg && g.dlg_mode != 1 &&
                (id == IDTS_APPLY || id == IDTS_SPADD || id == IDTS_SPDEL ||
                 id == IDTS_EXPORT || id == IDTS_SAVE)) return 0;
            if (id == IDTS_LIST && (code == LBN_SELCHANGE || code == LBN_DBLCLK)) {
                if (!g_ts.texture_indices_safe) {
                    SendMessageW(g_ts.list, LB_SETCURSEL, static_cast<WPARAM>(-1), 0);
                    status_mode(L"Texturliste unvollständig — Auswahl gesperrt; F7 erneut öffnen");
                    return 0;
                }
                const LRESULT sel = SendMessageW(g_ts.list, LB_GETCURSEL, 0, 0);
                const LRESULT original = sel == LB_ERR ? LB_ERR :
                    SendMessageW(g_ts.list, LB_GETITEMDATA, static_cast<WPARAM>(sel), 0);
                if (original >= 0 && static_cast<std::size_t>(original) < g_ts.names.size()) {
                    g_ts.sel = g_ts.names[static_cast<std::size_t>(original)];
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
                if (g.cur_map >= 0) {
                    if (!ts_same_map()) return 0;
                    const std::string tile_text = ts_text(IDTS_TILE);
                    TileFormEdit edit;
                    try {
                        edit = tile_form_prepare(g.map_root.find("terrain"), g_ts.texture_before,
                                                 g_ts.sel, g_ts.tile_before, tile_text);
                    } catch (const std::bad_alloc&) {
                        MessageBoxW(g_ts.win, L"Nicht genügend Speicher; die Kartendaten wurden nicht geändert.",
                                    L"Terrain", MB_OK | MB_ICONERROR);
                        return 0;
                    } catch (const std::length_error&) {
                        MessageBoxW(g_ts.win, L"Terrain-Daten zu groß; die Kartendaten wurden nicht geändert.",
                                    L"Terrain", MB_OK | MB_ICONERROR);
                        return 0;
                    }
                    if (!edit.ok) {
                        MessageBoxW(g_ts.win, utf8_to_wide(edit.error).c_str(), L"Terrain",
                                    MB_OK | MB_ICONWARNING);
                        return 0;
                    }
                    if (!edit.changed) { status_mode(L"Boden unverändert"); return 0; }
                    if (edit.replaces_invalid && data_prompt(g_ts.win,
                        L"Die Änderung ersetzt ungültige vorhandene Terrain-Daten. Trotzdem übernehmen?",
                        L"Terrain", MB_YESNO | MB_DEFBUTTON2 | MB_ICONWARNING) != IDYES) return 0;
                    if (!ts_same_map()) return 0;
                    JsonValue* ter = writable_terrain();
                    if (!ter) return 0;
                    *ter = std::move(edit.value);
                    g_ts.texture_before = g_ts.sel;
                    g_ts.tile_before = tile_text;
                    regen_terrain();          // entscheidet Ein-Textur- oder Splat-Pfad
                    g.dirty = true;
                    update_title();
                    ts_fill_splats();
                    status_mode(L"Boden übernommen");
                    core::log_info("Boden umgestellt: " + g_ts.sel);
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
                if (!ts_same_map()) return 0;
                double xd, zd, wd, hd;
                if (!map_props_number(ts_text(IDTS_SPX), xd) || !map_props_number(ts_text(IDTS_SPZ), zd) ||
                    !map_props_number(ts_text(IDTS_SPW), wd) || !map_props_number(ts_text(IDTS_SPH), hd) ||
                    !world::terrain_valid_splat(xd, zd, wd, hd)) {
                    MessageBoxW(g_ts.win, L"Fläche: endliche Koordinaten bis ±1.000.000 und "
                                           L"Breite/Tiefe von 0,001 bis 1.000.000 eingeben.",
                                L"Terrain", MB_OK | MB_ICONWARNING);
                    return 0;
                }
                TerrainPreview candidate = terrain_preview_read(g.map_root);
                if (!candidate.ok()) {
                    MessageBoxW(g_ts.win, L"Terrain-Vorschau ungültig. Bitte zuerst die F10-Probleme prüfen.",
                                L"Terrain", MB_OK | MB_ICONWARNING); return 0;
                }
                const float x = static_cast<float>(xd), z = static_cast<float>(zd);
                const float w = static_cast<float>(wd), h = static_cast<float>(hd);
                candidate.splats.push_back(world::SplatRect{x, z, w, h, g_ts.sel});
                if (world::terrain_check_splats(candidate.plan, candidate.splats) != world::TerrainProblem::None) {
                    MessageBoxW(g_ts.win, L"Diese Fläche würde das Terrain-Budget überschreiten.",
                                L"Terrain", MB_OK | MB_ICONWARNING); return 0;
                }
                JsonValue* ter = writable_terrain();
                if (!ter || !preview_unique(*ter, "splats")) return 0;
                JsonValue* arr = ter->find("splats");
                if (arr && !arr->is_arr()) {
                    MessageBoxW(g_ts.win, L"terrain.splats ist keine Liste; die Altdaten bleiben erhalten.",
                                L"Terrain", MB_OK | MB_ICONWARNING);
                    return 0;
                }
                if (!arr) arr = &ter->set("splats", JsonValue::make_arr());
                JsonValue e = JsonValue::make_obj();
                e.set("tex", JsonValue::make_str(g_ts.sel));
                e.set("x", JsonValue::make_num(xd));
                e.set("z", JsonValue::make_num(zd));
                e.set("w", JsonValue::make_num(wd));
                e.set("h", JsonValue::make_num(hd));
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
                if (!ts_same_map()) return 0;
                if (!g_ts.splat_indices_safe) {
                    MessageBoxW(g_ts.win, L"Die Flächenliste konnte nicht vollständig aufgebaut werden. "
                                           L"Löschen ist gesperrt; bitte F7 erneut öffnen.",
                                L"Terrain", MB_OK | MB_ICONWARNING);
                    return 0;
                }
                const int sel = static_cast<int>(SendMessageW(g_ts.splist, LB_GETCURSEL, 0, 0));
                const LRESULT original = sel < 0 ? LB_ERR :
                    SendMessageW(g_ts.splist, LB_GETITEMDATA, static_cast<WPARAM>(sel), 0);
                const SplatDeleteState state = splat_delete_state(g.map_root, static_cast<std::ptrdiff_t>(original));
                if (state == SplatDeleteState::Unavailable) {
                    core::log_warn("Tileset: keine eindeutige Fläche markiert");
                    status_mode(L"Keine eindeutige Fläche markiert");
                    return 0;
                }
                const bool needs_confirmation = state == SplatDeleteState::NeedsConfirmation;
                if (needs_confirmation && data_prompt(g_ts.win,
                    L"Dieser Eintrag enthält ungültige Altdaten. Wirklich vollständig löschen?", L"Terrain",
                    MB_YESNO | MB_DEFBUTTON2 | MB_ICONWARNING) != IDYES) return 0;
                // Nach der modalen Rückfrage nur auf dem aktuellen Baum und
                // derselben Karten-/Projektgeneration arbeiten, nie mit einem
                // zuvor gehaltenen JSON-Zeiger.
                if (!ts_same_map() || splat_delete_state(g.map_root, static_cast<std::ptrdiff_t>(original)) != state ||
                    !splat_delete_apply(g.map_root, static_cast<std::ptrdiff_t>(original), needs_confirmation)) return 0;
                regen_terrain();
                ts_fill_splats();
                g.dirty = true;
                update_title();
                status_mode(L"Fläche entfernt");
                core::log_info("Tileset: Fläche entfernt (Index " + std::to_string(sel) + ")");
                return 0;
            }
            if (id == IDTS_EXPORT) {
                if (!ts_same_project()) return 0;
                std::string name = ts_text(IDTS_NAME);
                for (char& c : name) if (c == ' ') c = '_';
                if (name.empty()) {
                    name = "kachel_" + std::to_string(GetTickCount() % 10000);
                }
                if (!world::terrain_valid_texture_name(name)) {
                    MessageBoxW(g_ts.win, L"Der Texturname ist ungültig oder zu lang.",
                                L"Terrain", MB_OK | MB_ICONWARNING);
                    return 0;
                }
                if (g.renderer && g.renderer->texture_index(name) >= 0 && data_prompt(g_ts.win,
                        L"Dieser Name ist bereits geladen. Die Vorschau wird bis zum Projektwechsel ersetzt; "
                        L"vorhandene Dateien bleiben unverändert. Fortfahren?",
                        L"Kachel erzeugen", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES) return 0;
                if (!ts_same_project()) return 0;
                wchar_t wpat[32];
                GetDlgItemTextW(h, IDTS_PAT, wpat, 32);
                const std::vector<uint8_t> px = make_pattern_tile(
                    ts_text(IDTS_COLR), wide_to_utf8(wpat), ts_text(IDTS_COLR2));
                if (!g.renderer || g.renderer->add_texture(name, 64, 64, px.data()) < 0) {
                    MessageBoxW(g_ts.win, L"Die Kachel konnte nicht im Renderer angelegt werden.",
                                L"Terrain", MB_OK | MB_ICONERROR);
                    status_mode(L"Kachel nicht angelegt");
                    return 0;
                }
                g_ts.sel = name;
                ts_fill_list();
                InvalidateRect(g_ts.preview, nullptr, TRUE);
                if (g.cur_map >= 0) regen_terrain();
                core::log_info("Tileset: neue Kachel '" + name +
                               "' erzeugt (speichern mit 'Als Projekt-Textur speichern')");
                status_mode((L"Neue Kachel: " + utf8_to_wide(name)).c_str());
                return 0;
            }
            if (id == IDTS_SAVE) {
                if (!ts_same_project()) return 0;
                if (g_ts.sel.empty()) {
                    core::log_warn("Tileset: keine Textur gewählt");
                    return 0;
                }
                const std::string typed = ts_text(IDTS_NAME);
                const std::string selected = typed.empty() ? g_ts.sel : typed;
                const std::string filename = selected + ".rgba";
                if (!world::terrain_valid_texture_name(selected) || !texture_file_is_flat(filename)) {
                    MessageBoxW(g_ts.win, L"Bitte einen einfachen Dateinamen ohne Pfad, Geräte- oder Sonderzeichen eingeben.",
                                L"Terrain", MB_OK | MB_ICONWARNING);
                    return 0;
                }
                std::vector<uint8_t> px;
                int w = 0, h = 0;
                if (!g.renderer || !g.renderer->texture_rgba(selected, px, w, h)) {
                    core::log_error("Tileset: Pixel nicht abrufbar: " + selected);
                    return 0;
                }
                MaterialInfo m;
                m.name = selected;
                m.file = filename;
                m.source = "tileset";
                m.w = w;
                m.h = h;
                m.tile = 2.0;
                m.imported = true;
                const bool replace = mat_find(selected) != nullptr;
                if (replace && data_prompt(g_ts.win,
                        L"Das vorhandene Material erhält neue Bilddaten. Die alte RGBA-Datei bleibt erhalten. Fortfahren?",
                        L"Projekt-Textur speichern", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES) return 0;
                if (!ts_same_project()) return 0;
                const std::string data(reinterpret_cast<const char*>(px.data()), px.size());
                if (!mat_store_texture(m, data, replace)) {
                    data_prompt(g_ts.win, utf8_to_wide("Textur nicht gespeichert: " + g_material_error).c_str(),
                                L"Projekt-Textur", MB_OK | MB_ICONWARNING);
                    status_mode(L"Kachel nicht gespeichert");
                    return 0;
                }
                g_ts.sel = selected;
                ts_fill_list();
                InvalidateRect(g_ts.preview, nullptr, TRUE);
                if (g.cur_map >= 0) regen_terrain();
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
    g_ts.project = g.project_dir;
    g_ts.project_epoch = g.project_epoch;
    g_ts.map_epoch = g.map_epoch;
    g_ts.map_file = g.cur_map >= 0 && static_cast<size_t>(g.cur_map) < g.map_files.size()
        ? g.map_files[static_cast<size_t>(g.cur_map)] : std::string();
    g_ts.sel.clear();

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
    // Aktuell in der Karte eingestellte Boden-Textur vorwählen.
    {
        const JsonValue* ter = g.map_root.find("terrain");
        const JsonValue* tx = ter && ter->is_obj() ? ter->find("texture") : nullptr;
        const std::string cur = !tx || tx->is_null() ? "gras"
            : tx->is_str() && world::terrain_valid_texture_name(tx->str) ? tx->str : std::string();
        if (!cur.empty()) g_ts.sel = cur;
    }
    ts_fill_list();
    g_ts.texture_before = g_ts.sel;

    // Rechte Spalte: Vorschau + Kachelgrösse + Eigene Kachel + Buttons.
    // Vorschau als eigenes Fenster der Tileset-Klasse (WM_PAINT landet so
    // in unserer Proc; ein Static-Kind würde selbst zeichnen).
    g_ts.preview = CreateWindowExW(0, L"RuneEditorTileDlg", nullptr,
                                   WS_CHILD | WS_VISIBLE,
                                   205, 36, 180, 180, g_ts.win,
                                   reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDTS_PREVIEW)),
                                   GetModuleHandleW(nullptr), nullptr);

    ts(L"Static", L"Kachelgröße (m):", 0, 205, 232, 120, 20, 0);
    HWND tile_edit = ts(L"Edit", L"2.0", WS_BORDER | ES_LEFT, 330, 228, 44, 24, IDTS_TILE);
    const JsonValue* tile_terrain = g.map_root.find("terrain");
    const JsonValue* old_tile = tile_terrain && tile_terrain->is_obj() ? tile_terrain->find("tile") : nullptr;
    SetWindowTextW(tile_edit, utf8_to_wide(map_props_number_display(old_tile, 2.0)).c_str());
    g_ts.tile_before = ts_text(IDTS_TILE);

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
    return project_names_on_disk(g.root);
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
    // CREATE_DIRECTORY ist exklusiv: ein inzwischen angelegtes Projekt
    // niemals mit den folgenden Dateien überschreiben.
    if (!core::win_path::create_directory(base)) {
        core::log_error("Projektordner existiert bereits oder ist nicht anlegbar: " + name);
        return false;
    }
    if (!core::win_path::create_directory(base + "\\data") ||
        !core::win_path::create_directory(base + "\\data\\maps") ||
        !core::win_path::create_directory(base + "\\saves")) {
        core::log_error("Projektgerüst unvollständig; bitte prüfen: " + name);
        return false; // nur die eigene, möglicherweise unvollständige Anlage bleibt zurück
    }

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
bool switch_project(const std::string& name, bool departure_checked) {
    // F9 hält eine eigene Kopie der Datenbank. Ein Projektwechsel bei
    // offenem F9 würde diese sonst unter dem Pfad des neuen Projekts speichern.
    if (database_editor_open()) return false;
    std::wstring project_component;
    if (!core::win_path::to_wide(name, project_component) || !assets::component(project_component)) {
        core::log_error("Ungültiger Projektname: " + name);
        return false;
    }
    const std::string data = g.root + "\\projekte\\" + name + "\\data";
    if (!core::win_path::path_exists(data + "\\system.json")) {
        core::log_error("Projekt nicht gefunden: " + name);
        return false;
    }
    if (name == g.project_name && g.cur_map >= 0) return true;
    if (!departure_checked && g.cur_map >= 0 && !prepare_map_departure()) return false;
    cancel_object_drag();
    // Offene Dialoge schliessen (Zeiger werden ungültig).
    if (g.props_dlg) { DestroyWindow(g.props_dlg); g.props_dlg = nullptr; }
    close_event_editor();
    close_material_dialog(); // kein alter F3-Entwurf im neuen Projekt

    g.project_dir = data;
    g.project_name = name;
    ++g.project_epoch;
    ++g.map_epoch;
    g.dirty = false;
    g.sel_obj = -1;
    g.undo_stack.clear();
    g.redo_stack.clear();

    // Objekt-/Eigenschaftsfelder, Dateischnappschuss und Terrain leeren.
    g.objects = nullptr;
    g.map_root = JsonValue{};
    g.map_disk_present = false;
    g.map_disk_text.clear();
    g.cur_map = -1;
    g.terrain_error.clear();
    g.terrain_warning.clear();
    g.terrain_memory_error = false;
    g.terrain_field_warning = false;
    g.atmosphere_warning = false;
    g.terrain->clear();
    g.renderer->clear_terrain();

    // Vor dem Laden alle alten Projekt-Slots und den Atlas freigeben;
    // eingebaute Texturen (auch bei gleichnamigen Overrides) bleiben erhalten.
    g.renderer->begin_project_textures();
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
    // Bereits gepostete Befehle werden auch an deaktivierte Fenster zugestellt.
    if (g.data_prompt_active && (msg == WM_COMMAND || msg == WM_CLOSE)) return 0;
    switch (msg) {
        case WM_COMMAND: {
            const int id = LOWORD(wp);
            const int code = HIWORD(wp);
            const bool project_action = id == IDPR_OPEN || id == IDPR_CREATE ||
                                        (id == IDPR_LIST && code == LBN_DBLCLK);
            if (project_action && g.props_dlg) {
                MessageBoxW(h, L"Bitte zuerst den Karten-Dialog bestätigen oder abbrechen.",
                            L"Projekt wechseln", MB_OK | MB_ICONINFORMATION);
                SetForegroundWindow(g.props_dlg);
                return 0;
            }
            if (project_action && database_editor_open()) {
                MessageBoxW(h, L"Bitte zuerst die Datenbank speichern und schließen.",
                            L"Projekt wechseln", MB_OK | MB_ICONINFORMATION);
                return 0;
            }
            if (id == IDPR_OPEN || (id == IDPR_LIST && code == LBN_DBLCLK)) {
                const int sel = static_cast<int>(SendMessageW(g_prj.list, LB_GETCURSEL, 0, 0));
                if (sel >= 0) {
                    // Von außen angelegte Namen können länger als 127
                    // Zeichen sein; vollständig und ohne Ersatzzeichen holen.
                    std::string name;
                    if (!listbox_text_utf8(g_prj.list, sel, name) || name.empty()) return 0;
                    if (switch_project(name)) {
                        DestroyWindow(h);
                        status_mode((L"Projekt: " + utf8_to_wide(name)).c_str());
                    } else {
                        status_mode(L"Projektwechsel abgebrochen oder fehlgeschlagen");
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
                if (g.cur_map >= 0 && !prepare_map_departure()) return 0;
                if (create_project(name)) {
                    if (switch_project(name, true)) {
                        DestroyWindow(h);
                        status_mode((L"Neues Projekt: " + utf8_to_wide(name)).c_str());
                    } else {
                        MessageBoxW(h, L"Projekt wurde angelegt, konnte aber nicht geöffnet werden.",
                                    L"Neues Projekt", MB_OK | MB_ICONWARNING);
                    }
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
    g.map_files = map_files_on_disk(g.project_dir);
    g.map_names.clear();
    for (const std::string& file : g.map_files) {
        // Anzeigename: "name"-Feld, sonst UTF-8-Dateiname
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
    }
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
//  Links unter der Palette: ein TreeView mit dem Projektnamen als Wurzel
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
    // Die Zeichenkette muss bis nach TreeView_InsertItem leben: ein direktes
    // utf8_to_wide(...).c_str() wäre bereits am Semikolon freigegeben.
    std::wstring project_label = utf8_to_wide(g.project_name);
    ins.item.pszText = project_label.data();
    ins.item.cChildren = 1;
    const HTREEITEM root = TreeView_InsertItem(g.map_tree, &ins);
    for (int i = 0; i < static_cast<int>(g.map_names.size()); ++i) {
        TVINSERTSTRUCTW mi{};
        mi.hParent = root;
        mi.hInsertAfter = TVI_LAST;
        mi.item.mask = TVIF_TEXT | TVIF_PARAM;
        std::wstring label = utf8_to_wide(g.map_names[i]);
        mi.item.pszText = label.data();
        mi.item.lParam = i;
        TreeView_InsertItem(g.map_tree, &mi);
    }
    // Wurzel aufklappen und aktuelle Karte markieren.
    SendMessageW(g.map_tree, TVM_EXPAND, TVE_EXPAND,
                 reinterpret_cast<LPARAM>(root));
    maptree_select_current();
    maptree_update_buttons();
}

// Markiert die aktuelle Karte im Baum (ohne Ladefehler auszulösen).
void maptree_select_current() {
    if (!g.map_tree) return;
    if (g.cur_map < 0) { TreeView_SelectItem(g.map_tree, nullptr); maptree_update_buttons(); return; }
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
            maptree_update_buttons();
            return;
        }
    }
    TreeView_SelectItem(g.map_tree, nullptr);
    maptree_update_buttons();
}

// Karten-Index des markierten Baum-Eintrags (-1 = Wurzel/nichts markiert).
int maptree_selected() {
    return main_tree_selected_index(g.map_tree);
}

// Aktionen warten, bis ein bereits angeforderter Kartenwechsel einschließlich
// aller Datenrückfragen beendet ist. Nur die letzte Geste bleibt vorgemerkt.
void maptree_schedule_action() {
    if (!g.main || !main_map_action_can_post(g.map_action.kind != MapTreeActionKind::None,
        g.map_action_queued, g.data_prompt_active, g.map_selection_queued, g.map_selection_resolving)) return;
    g.map_action_queued = PostMessageW(g.main, WM_TREE_ACTION, 0, 0) != 0;
    if (!g.map_action_queued) g.map_action = {};
}

void maptree_queue_action(MapTreeActionKind kind, int index, LPARAM position = 0) {
    if (g.project_dir.empty() || (index >= 0 && static_cast<size_t>(index) >= g.map_files.size())) {
        g.map_action = {};
        return;
    }
    MapTreeAction action;
    action.kind = kind;
    action.index = index;
    action.position = position;
    action.project_epoch = g.project_epoch;
    try {
        if (index >= 0) action.file = g.map_files[static_cast<size_t>(index)];
    } catch (...) {
        g.map_action = {}; // keine Ausnahme aus einem Win32-Callback
        return;
    }
    g.map_action = std::move(action);
    if (index >= 0 && index != g.cur_map && !g.map_selection_queued && !g.map_selection_resolving) {
        g.map_selection_queued = PostMessageW(g.main, WM_RECONCILE_MAP_SELECTION, 0, 0) != 0;
        if (!g.map_selection_queued) {
            g.map_action = {};
            maptree_select_current();
            return;
        }
    }
    maptree_schedule_action();
}

bool maptree_action_matches(const MapTreeAction& action) {
    const std::string_view current_file = g.cur_map >= 0 &&
        static_cast<size_t>(g.cur_map) < g.map_files.size()
        ? std::string_view(g.map_files[static_cast<size_t>(g.cur_map)]) : std::string_view{};
    return main_map_action_target_matches(action.index, maptree_selected(), g.cur_map,
        action.project_epoch, g.project_epoch, action.file, current_file);
}

void maptree_update_buttons() {
    if (!g.main) return;
    const bool available = !g.props_dlg && !g.project_dir.empty();
    const int selected = maptree_selected();
    EnableWindow(GetDlgItem(g.main, IDC_MAP_NEW_BUTTON), available ? TRUE : FALSE);
    // Während eines noch nicht abgeglichenen Wechsels keine Aktion auf der
    // vorherigen Karte anbieten. Nach erfolgreichem Laden wird erneut gesetzt.
    EnableWindow(GetDlgItem(g.main, IDC_MAP_PROPS_BUTTON),
                 available && selected >= 0 && selected == g.cur_map ? TRUE : FALSE);
}

// Umbenennen: Namen der Karte in map_root + Liste aktualisieren
// (gespeichert wie üblich per Strg+S, g.dirty markiert die Karte).
bool maptree_apply_rename(int index, const std::string& new_name) {
    if (index < 0 || index >= static_cast<int>(g.map_files.size()) ||
        index != g.cur_map || new_name.empty()) return false;
    if (!object_props_field_unique(g.map_root, "name")) {
        // Keine modale Nachrichtenschleife innerhalb von TVN_ENDLABELEDIT.
        PostMessageW(g.main, WM_TREE_RENAME_WARNING, 1, 0);
        return false;
    }
    if (!g.map_root.find("name") && event_editor_open()) {
        PostMessageW(g.main, WM_TREE_RENAME_WARNING, 2, 0);
        return false;
    }
    g.map_root.set("name", JsonValue::make_str(new_name));
    // Ein fehlendes name kann die Wurzel-Paarliste umallozieren.
    g.objects = g.map_root.find("objects");
    g.dirty = true;
    update_title();
    g.map_names[index] = new_name;
    // Nicht den Baum mitten in TVN_ENDLABELEDIT löschen und neu aufbauen.
    PostMessageW(g.main, WM_REBUILD_MAP_MENU, 0, 0);
    core::log_info("Karte umbenannt: " + g.map_files[index] + " -> '" +
                   new_name + "' (Strg+S speichert)");
    return true;
}

// ------------------------------------------------------------
//  Objekt-Operationen (arbeiten NUR auf den Kartendaten)
// ------------------------------------------------------------
float base_y_at(float x, float z) {
    if (!g.terrain || !std::isfinite(x) || !std::isfinite(z)) return 0.0f;
    const float half_w = g.terrain->width() * 0.5f, half_h = g.terrain->height() * 0.5f;
    if (!std::isfinite(half_w) || !std::isfinite(half_h) || half_w <= 0.0f || half_h <= 0.0f)
        return 0.0f;
    // Terrain::height_at klemmt zwar Indizes, castet aber vorher nach int.
    // Positionen vorher auf die Kartengrenze begrenzen, um auch große
    // unveränderte Altkoordinaten sicher zu behandeln.
    return g.terrain->height_at(std::clamp(x, -half_w, half_w), std::clamp(z, -half_h, half_h));
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
    if (!v || !v->is_num() || !std::isfinite(v->number()) || std::fabs(v->number()) > 1000000.0)
        return fallback;
    return static_cast<float>(v->number());
}

bool object_viewable(int i) {
    return g.objects && i >= 0 && static_cast<size_t>(i) < g.objects->arr.size() &&
           object_props_viewable(g.objects->arr[static_cast<size_t>(i)]);
}

// Etappe 19c: zentrales Umschalten der fünf Modi. Die Knöpfe sind Radios
// (BS_AUTORADIOBUTTON); Prefab-Liste und Kontextmenü wechseln den Modus aber auch,
// deshalb wird der Haken hier gesetzt - genau einer ist immer aktiv.
void set_mode(int mode) {
    cancel_object_drag();
    g.place_mode  = (mode == 1);
    g.move_mode   = (mode == 3);
    g.paint_mode  = (mode == 4);
    g.height_mode = (mode == 5);
    const int ids[6] = { 0, IDC_BTN_PLACE, IDC_BTN_SELECT, IDC_BTN_MOVE,
                         IDC_BTN_PAINT, IDC_BTN_HEIGHT };
    for (int m = 1; m <= 5; ++m)
        SendMessageW(GetDlgItem(g.palette.window, ids[m]), BM_SETCHECK,
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

// Sortiert nach der double-Näherung, auch außerhalb des Viewport-Bereichs.
// NaN darf die strikte Ordnung von stable_sort nicht verletzen.
double obj_sort_number(int i, const char* key) {
    if (!g.objects || i < 0 || static_cast<size_t>(i) >= g.objects->arr.size()) return 0.0;
    const JsonValue* v = g.objects->arr[static_cast<size_t>(i)].find(key);
    const double n = v ? v->number(0.0) : 0.0;
    return std::isnan(n) ? 0.0 : n;
}

// Sortierschluessel der Objektliste (Spalte in g.obj_sort_col).
bool obj_sort_less(int a, int b) {
    switch (g.obj_sort_col) {
        case 1:  return obj_display_name(a) < obj_display_name(b);
        case 2:  return obj_sort_number(a, "x") < obj_sort_number(b, "x");
        case 3:  return obj_sort_number(a, "z") < obj_sort_number(b, "z");
        case 4:  return obj_sort_number(a, "rot") < obj_sort_number(b, "rot");
        case 5:  return obj_material(a) < obj_material(b);
        default: return a < b;
    }
}

int obj_row_of(int index) {
    for (int r = 0; r < static_cast<int>(g.obj_order.size()); ++r)
        if (g.obj_order[r] == index) return r;
    return -1;
}

int object_list_selected() {
    if (!g.list_objs) return -1;
    const int row = ListView_GetNextItem(g.list_objs, -1, LVNI_SELECTED);
    if (row < 0) return -1;
    LVITEMW item{};
    item.mask = LVIF_PARAM;
    item.iItem = row;
    return ListView_GetItem(g.list_objs, &item) ? static_cast<int>(item.lParam) : -1;
}

// Etappe 19c: Objektliste als JSON-Schnappschuss nach log/editor_objects.json.
// Texte lassen sich prozessfremd nicht per SendMessage lesen (siehe BACKLOG);
// eine Datei im Log-Verzeichnis kann jedes Werkzeug - und hilft beim Debuggen.
void write_objects_introspection() {
    JsonValue list = JsonValue::make_arr();
    const size_t count = g.objects ? g.objects->arr.size() : 0;
    for (size_t i = 0; i < count; ++i) {
        const int index = static_cast<int>(i);
        const JsonValue& object = g.objects->arr[i];
        JsonValue entry = JsonValue::make_obj();
        entry.set("i", JsonValue::make_integer(std::to_string(i)));
        entry.set("type", JsonValue::make_str(obj_type(index)));
        entry.set("name", JsonValue::make_str(obj_display_name(index)));
        for (const char* key : {"x", "z", "rot"}) {
            const JsonValue* v = object.find(key);
            entry.set(key, v && v->is_num() ? *v : JsonValue::make_num(0.0));
        }
        entry.set("tex", JsonValue::make_str(obj_material(index)));
        list.arr.push_back(std::move(entry));
    }
    const std::string js = json_write(list);
    static std::string cache;
    if (js == cache) return;
    cache = js;
    write_file(g.root + "\\log\\editor_objects.json", js);
}

void sync_object_list() {
    if (!g.list_objs) return;
    const bool was_syncing = g.syncing_object_list;
    g.syncing_object_list = true;
    if (!g.objects) {
        ListView_DeleteAllItems(g.list_objs);
        g.obj_order.clear();
        g.syncing_object_list = was_syncing;
        write_objects_introspection();
        return;
    }
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
        const ObjectPropsForm values = object_props_read(g.objects->arr[static_cast<size_t>(i)]);
        const std::wstring x = utf8_to_wide(values.x), z = utf8_to_wide(values.z);
        const std::wstring rotation = utf8_to_wide(values.rotation) + L"°";
        ListView_SetItemText(g.list_objs, idx, 2, const_cast<wchar_t*>(x.c_str()));
        ListView_SetItemText(g.list_objs, idx, 3, const_cast<wchar_t*>(z.c_str()));
        ListView_SetItemText(g.list_objs, idx, 4, const_cast<wchar_t*>(rotation.c_str()));
        const std::wstring mat = utf8_to_wide(obj_material(i));
        ListView_SetItemText(g.list_objs, idx, 5, const_cast<wchar_t*>(mat.c_str()));
    }
    const int sel_row = obj_row_of(g.sel_obj);
    if (sel_row >= 0)
        ListView_SetItemState(g.list_objs, sel_row, LVIS_SELECTED | LVIS_FOCUSED,
                              LVIS_SELECTED | LVIS_FOCUSED);
    g.syncing_object_list = was_syncing;
    write_objects_introspection();
}

void sync_property_fields() {
    const JsonValue* object = nullptr;
    if (g.objects && g.sel_obj >= 0 && static_cast<size_t>(g.sel_obj) < g.objects->arr.size())
        object = &g.objects->arr[static_cast<size_t>(g.sel_obj)];
    const bool valid = object && object->is_obj();
    g.object_form = valid ? object_props_read(*object) : ObjectPropsForm{};
    const HWND controls[4] = {g.ed_x, g.ed_z, g.ed_rot, g.ed_tex};
    const char* keys[4] = {"x", "z", "rot", "tex"};
    const std::string values[4] = {g.object_form.x, g.object_form.z,
                                    g.object_form.rotation, g.object_form.material};
    bool protected_field = false;
    for (int i = 0; i < 4; ++i) {
        SetWindowTextW(controls[i], utf8_to_wide(values[i]).c_str());
        // Nur Felder freigeben, die das Win32-Control unverändert zurückgeben
        // kann. Alte Texte mit NUL/Zeilenumbruch oder Überlänge bleiben so
        // auch bei Änderungen an anderen Feldern unangetastet.
        std::string returned;
        g.object_field_editable[i] = valid && controls[i] && object_props_field_unique(*object, keys[i]) &&
                                     editor_single_line(values[i]) && control_text_strict(controls[i], returned) &&
                                     returned == values[i];
        if (valid && !g.object_field_editable[i]) protected_field = true;
        EnableWindow(controls[i], g.object_field_editable[i] ? TRUE : FALSE);
    }
    EnableWindow(GetDlgItem(g.inspector.window, IDC_BTN_APPLY), valid ? TRUE : FALSE);
    const wchar_t* hint = !object ? L"Objekt auswählen."
        : !valid ? L"Ungültiger Eintrag: Eigenschaften gesperrt."
        : protected_field ? L"Feld gesperrt: mehrfach oder nicht darstellbar."
        : !object_props_viewable(*object) ? L"Ungültige/extreme Position: keine Vorschau."
        : L"Änderungen übernehmen, dann mit Strg+S speichern.";
    SetWindowTextW(GetDlgItem(g.inspector.window, IDC_LBL_PROPS_HINT), hint);
}

bool object_current_form(ObjectPropsForm& form, HWND* invalid = nullptr) {
    form = g.object_form;
    const HWND controls[4] = {g.ed_x, g.ed_z, g.ed_rot, g.ed_tex};
    std::string* values[4] = {&form.x, &form.z, &form.rotation, &form.material};
    for (int i = 0; i < 4; ++i) {
        if (g.object_field_editable[i] && !control_text_strict(controls[i], *values[i])) {
            if (invalid) *invalid = controls[i];
            return false;
        }
    }
    return true;
}

bool object_form_pending() {
    if (!g.objects || g.sel_obj < 0 || static_cast<size_t>(g.sel_obj) >= g.objects->arr.size() ||
        !g.objects->arr[static_cast<size_t>(g.sel_obj)].is_obj()) return false;
    ObjectPropsForm current;
    return !object_current_form(current) || !object_props_equal(current, g.object_form);
}

// Bei Aktionen, die die Felder ersetzen, darf nichts still verschwinden.
// Ja übernimmt, Nein verwirft nur die Formulareingaben, Abbrechen bleibt hier.
bool resolve_object_edits(bool refresh_list) {
    if (!object_form_pending()) return true;
    HWND owner = GetActiveWindow();
    if (!owner) owner = g.main;
    const int choice = data_prompt(owner,
        L"Nicht übernommene Objekt-Eingaben übernehmen?\r\n\r\n"
        L"Ja: übernehmen   Nein: verwerfen   Abbrechen: zurück",
        L"Objekteigenschaften", MB_YESNOCANCEL | MB_ICONQUESTION | MB_DEFBUTTON3);
    if (choice == IDYES) return apply_property_fields(refresh_list);
    if (choice != IDNO) return false;
    sync_property_fields();
    return true;
}

void restore_object_list_selection() {
    if (!g.list_objs) return;
    const bool was_syncing = g.syncing_object_list;
    g.syncing_object_list = true;
    ListView_SetItemState(g.list_objs, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
    const int row = obj_row_of(g.sel_obj);
    if (row >= 0)
        ListView_SetItemState(g.list_objs, row, LVIS_SELECTED | LVIS_FOCUSED,
                              LVIS_SELECTED | LVIS_FOCUSED);
    g.syncing_object_list = was_syncing;
}

bool select_object(int i) {
    const int count = g.objects ? static_cast<int>(g.objects->arr.size()) : 0;
    const int next = (i >= 0 && i < count) ? i : -1;
    if (next == g.sel_obj) return true;
    if (!resolve_object_edits(false)) return false;
    cancel_object_drag();
    g.sel_obj = next;
    sync_object_list();
    sync_property_fields();
    return true;
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
    cancel_object_drag();
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
    cancel_object_drag();
    if (!resolve_object_edits()) return;
    if (g.undo_stack.empty()) { status_mode(L"Nichts rückgängig"); return; }
    if (g.objects) g.redo_stack.push_back(json_write(*g.objects));
    const std::string text = g.undo_stack.back();
    g.undo_stack.pop_back();
    restore_objects(text);
    status_mode(L"Rückgängig (Strg+Z)");
}

void redo_action() {
    cancel_object_drag();
    if (!resolve_object_edits()) return;
    if (g.redo_stack.empty()) { status_mode(L"Nichts wiederholen"); return; }
    if (g.objects) g.undo_stack.push_back(json_write(*g.objects));
    const std::string text = g.redo_stack.back();
    g.redo_stack.pop_back();
    restore_objects(text);
    status_mode(L"Wiederholt (Strg+Y)");
}

void place_object(float x, float z) {
    if (!g.objects || !g.objects->is_arr() || g.cur_map < 0 ||
        g.sel_prefab < 0 || g.sel_prefab >= static_cast<int>(g.prefabs.size())) return;
    cancel_object_drag();
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
    if (g.sel_obj < 0 || !g.objects || static_cast<size_t>(g.sel_obj) >= g.objects->arr.size()) return;
    if (!resolve_object_edits()) return;
    cancel_object_drag();
    push_undo();
    g.objects->arr.erase(g.objects->arr.begin() + g.sel_obj);
    g.dirty = true;
    core::log_info("Objekt gelöscht");
    select_object(-1);
}

void rotate_selected(float delta_rad) {
    if (!g.objects || g.sel_obj < 0 || static_cast<size_t>(g.sel_obj) >= g.objects->arr.size()) return;
    if (!resolve_object_edits()) return;
    const JsonValue& object = g.objects->arr[static_cast<size_t>(g.sel_obj)];
    const JsonValue* old = object.find("rot");
    if (!object.is_obj() || !object_props_field_unique(object, "rot") ||
        (old && !old->is_null() && (!old->is_num() || !std::isfinite(old->number()) ||
                                    std::fabs(old->number()) > 1000000.0))) {
        MessageBoxW(g.main, L"Die vorhandene Drehung ist ungültig oder mehrdeutig. "
                            L"Bitte zuerst im Formular prüfen.",
                    L"Objekt drehen", MB_OK | MB_ICONWARNING);
        return;
    }
    const double rot = (old ? old->number(0.0) : 0.0) + static_cast<double>(delta_rad);
    if (!std::isfinite(rot) || std::fabs(rot) > 1000000.0) {
        MessageBoxW(g.main, L"Die Drehung liegt außerhalb des darstellbaren Bereichs.",
                    L"Objekt drehen", MB_OK | MB_ICONWARNING);
        return;
    }
    cancel_object_drag();
    push_undo();
    g.objects->arr[static_cast<size_t>(g.sel_obj)].set("rot", JsonValue::make_num(rot, false));
    g.dirty = true;
    sync_object_list();
    sync_property_fields();
    update_title();
}

void duplicate_selected() {
    if (g.sel_obj < 0 || !g.objects || static_cast<size_t>(g.sel_obj) >= g.objects->arr.size()) return;
    if (!resolve_object_edits()) return;
    const JsonValue& original = g.objects->arr[static_cast<size_t>(g.sel_obj)];
    if (!object_props_field_unique(original, "x") || !object_props_field_unique(original, "z")) {
        MessageBoxW(g.main, L"Mehrfach vorhandene X-/Z-Schlüssel verhindern eindeutiges Duplizieren.",
                    L"Objekt duplizieren", MB_OK | MB_ICONWARNING);
        return;
    }
    cancel_object_drag();
    push_undo();
    JsonValue copy = original;
    JsonValue* x = copy.find("x");
    JsonValue* z = copy.find("z");
    if (x) x->increment();
    if (z) z->increment();
    g.objects->arr.push_back(std::move(copy));
    g.dirty = true;
    core::log_info("Objekt dupliziert");
    select_object(static_cast<int>(g.objects->arr.size()) - 1);
}

bool apply_property_fields(bool refresh_list) {
    if (!g.objects || g.sel_obj < 0 || static_cast<size_t>(g.sel_obj) >= g.objects->arr.size()) return false;
    HWND owner = GetActiveWindow();
    if (!owner) owner = g.main;
    const JsonValue& object = g.objects->arr[static_cast<size_t>(g.sel_obj)];
    ObjectPropsForm after;
    HWND invalid = nullptr;
    if (!object_current_form(after, &invalid)) {
        data_prompt(owner, L"Das Feld enthält ungültiges Unicode oder konnte nicht vollständig gelesen werden.",
                    L"Objekteigenschaften", MB_OK | MB_ICONWARNING);
        if (invalid) { SetForegroundWindow(g.main); SetFocus(invalid); }
        return false;
    }
    ObjectPropsEdit edit = object_props_prepare(object, g.object_form, after);
    if (!edit.ok) {
        const std::string message = (edit.field.empty() ? std::string() : edit.field + ": ") + edit.error;
        data_prompt(owner, utf8_to_wide(message).c_str(), L"Objekteigenschaften", MB_OK | MB_ICONWARNING);
        HWND field = edit.field == "x" ? g.ed_x : edit.field == "z" ? g.ed_z
                   : edit.field == "rotation" ? g.ed_rot : edit.field == "material" ? g.ed_tex : nullptr;
        if (field) { SetForegroundWindow(g.main); SetFocus(field); }
        return false;
    }
    if (edit.changed && edit.replaces_invalid && data_prompt(
            owner, L"Diese Änderung ersetzt fehlerhafte oder nicht darstellbare Altdaten. Fortfahren?",
            L"Objekteigenschaften", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES) return false;
    if (edit.changed) {
        cancel_object_drag();
        push_undo();
        g.objects->arr[static_cast<size_t>(g.sel_obj)] = std::move(edit.value);
        g.dirty = true;
        if (refresh_list) sync_object_list();
        update_title();
    }
    sync_property_fields();
    return true;
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
    // Kein fiktiver Nullboden zum Malen/Picken, wenn die Vorschau ausfiel.
    if (!g.terrain || !g.terrain->ready()) return false;
    const float max_t = 400.0f;
    float t_prev = 0.0f;
    float d_prev = eye.y - base_y_at(eye.x, eye.z);
    for (float t = 0.5f; t < max_t; t += 0.5f) {
        const math::Vec3 p = math::add(eye, math::scale(dir, t));
        const float d = p.y - base_y_at(p.x, p.z);
        if (d <= 0.0f) {
            // Halbiere das Intervall für eine genaue Trefferstelle.
            float lo = t_prev, hi = t;
            for (int i = 0; i < 12; ++i) {
                const float mid = (lo + hi) * 0.5f;
                const math::Vec3 pm = math::add(eye, math::scale(dir, mid));
                if (pm.y - base_y_at(pm.x, pm.z) <= 0.0f) hi = mid; else lo = mid;
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
        if (!object_viewable(i)) continue;
        const Prefab* pre = prefab_of(obj_type(i));
        if (!pre) continue;
        const float x = obj_number(i, "x", 0);
        const float z = obj_number(i, "z", 0);
        const float rot = obj_number(i, "rot", 0);
        const float by = base_y_at(x, z) + obj_number(i, "y", 0);
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
    SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(g.main_font ? g.main_font : g.font), TRUE);
}

HWND make_control(const wchar_t* cls, const wchar_t* text, DWORD style,
                  int x, int y, int w, int h, int id, HWND parent = nullptr) {
    HWND ctrl = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style,
                                x, y, w, h, parent ? parent : g.main,
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

// Die Zeichner verwenden 16x16 logische Einheiten; GDI skaliert die Formen
// für die tatsächliche Monitor-DPI. Die Icon-Maske muss monochrom sein.
HICON make_toolbar_icon(void (*draw)(HDC), int size) {
    HDC screen = GetDC(nullptr);
    HDC mem = CreateCompatibleDC(screen);
    HBITMAP bmp = CreateCompatibleBitmap(screen, size, size);
    HBITMAP mask = CreateBitmap(size, size, 1, 1, nullptr);
    if (!screen || !mem || !bmp || !mask) {
        if (bmp) DeleteObject(bmp);
        if (mask) DeleteObject(mask);
        if (mem) DeleteDC(mem);
        if (screen) ReleaseDC(nullptr, screen);
        return nullptr;
    }
    HGDIOBJ old = SelectObject(mem, bmp);
    RECT rc{ 0, 0, size, size };
    FillRect(mem, &rc, reinterpret_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
    SetMapMode(mem, MM_ANISOTROPIC);
    SetWindowExtEx(mem, 16, 16, nullptr);
    SetViewportExtEx(mem, size, size, nullptr);
    draw(mem);
    SelectObject(mem, old);

    HDC mask_dc = CreateCompatibleDC(screen);
    if (!mask_dc) {
        DeleteObject(bmp);
        DeleteObject(mask);
        DeleteDC(mem);
        ReleaseDC(nullptr, screen);
        return nullptr;
    }
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

void new_map(HDC dc) {           // Neue Karte
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(60, 85, 115));
    HGDIOBJ op = SelectObject(dc, pen);
    HBRUSH paper = CreateSolidBrush(RGB(240, 246, 252));
    HGDIOBJ ob = SelectObject(dc, paper);
    Rectangle(dc, 2, 1, 12, 14);
    MoveToEx(dc, 4, 4, nullptr); LineTo(dc, 10, 4);
    MoveToEx(dc, 4, 6, nullptr); LineTo(dc, 9, 6);
    HPEN plus = CreatePen(PS_SOLID, 2, RGB(45, 145, 65));
    SelectObject(dc, plus);
    MoveToEx(dc, 10, 8, nullptr); LineTo(dc, 10, 15);
    MoveToEx(dc, 7, 11, nullptr); LineTo(dc, 14, 11);
    SelectObject(dc, op);
    SelectObject(dc, ob);
    DeleteObject(plus);
    DeleteObject(paper);
    DeleteObject(pen);
}

void placement(HDC dc) {         // NPCs und Objekte platzieren
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(95, 55, 40));
    HGDIOBJ op = SelectObject(dc, pen);
    HBRUSH fill = CreateSolidBrush(RGB(215, 151, 98));
    HGDIOBJ ob = SelectObject(dc, fill);
    Ellipse(dc, 5, 1, 11, 7);
    Rectangle(dc, 4, 7, 12, 14);
    MoveToEx(dc, 8, 7, nullptr); LineTo(dc, 8, 14);
    SelectObject(dc, op);
    SelectObject(dc, ob);
    DeleteObject(fill);
    DeleteObject(pen);
}

void tiles(HDC dc) {             // Tileset-Editor
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(58, 90, 51));
    HGDIOBJ op = SelectObject(dc, pen);
    HBRUSH grass = CreateSolidBrush(RGB(122, 183, 95));
    HBRUSH earth = CreateSolidBrush(RGB(188, 156, 112));
    HGDIOBJ ob = SelectObject(dc, grass);
    Rectangle(dc, 1, 1, 8, 8);
    Rectangle(dc, 8, 8, 15, 15);
    SelectObject(dc, earth);
    Rectangle(dc, 8, 1, 15, 8);
    Rectangle(dc, 1, 8, 8, 15);
    SelectObject(dc, op);
    SelectObject(dc, ob);
    DeleteObject(grass);
    DeleteObject(earth);
    DeleteObject(pen);
}

void camera(HDC dc) {            // Kamera zurücksetzen
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(50, 71, 96));
    HGDIOBJ op = SelectObject(dc, pen);
    HBRUSH body = CreateSolidBrush(RGB(145, 175, 205));
    HGDIOBJ ob = SelectObject(dc, body);
    Rectangle(dc, 1, 4, 15, 13);
    Rectangle(dc, 4, 2, 9, 5);
    SelectObject(dc, GetStockObject(WHITE_BRUSH));
    Ellipse(dc, 5, 5, 12, 12);
    SelectObject(dc, op);
    SelectObject(dc, ob);
    DeleteObject(body);
    DeleteObject(pen);
}

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
static HIMAGELIST g_toolbar_images = nullptr;
static int g_toolbar_h = 0;

struct ToolbarItem {
    int command;
    void (*draw)(HDC);
    const wchar_t* tip;
};

// Trennzeichen gliedern die Leiste wie in einem klassischen Karteneditor.
// Jeder sichtbare Knopf führt zu einer bereits vorhandenen Funktion.
static const ToolbarItem TOOLBAR_ITEMS[] = {
    {ID_MENU_MAPNEW,    ico::new_map,   L"Neue Karte"},
    {ID_MENU_SAVE,      ico::diskette,  L"Karte speichern (Strg+S)"},
    {0, nullptr, nullptr},
    {ID_MENU_MAPPROPS,  ico::props,     L"Karten-Eigenschaften (F2)"},
    {ID_MENU_PLACEMENT, ico::placement, L"NPCs, Schilder und Objekte (F5)"},
    {ID_MENU_EVENTS,    ico::events,    L"Event-Editor (F6)"},
    {ID_MENU_TILESET,   ico::tiles,     L"Tileset-Editor (F7)"},
    {0, nullptr, nullptr},
    {ID_MENU_MATERIALS, ico::materials, L"Materialbase (F3)"},
    {ID_MENU_DATABASE,  ico::database,  L"Datenbank (F9)"},
    {ID_MENU_SCRIPTS,   ico::scripts,   L"Script-Editor (F11)"},
    {ID_MENU_SOUNDTEST, ico::speaker,   L"Sound-Test (F8)"},
    {0, nullptr, nullptr},
    {ID_MENU_RESETCAM,  ico::camera,    L"Kamera zurücksetzen"},
    {ID_MENU_PLAYTEST,  ico::playtest,  L"Playtest starten (F4)"},
};

int toolbar_height() {
    return g_toolbar_h;
}

void toolbar_set_dpi(unsigned dpi) {
    if (!g_toolbar) return;
    const int icon_size = std::max(1, main_layout_scale(16, dpi));
    HIMAGELIST images = ImageList_Create(icon_size, icon_size, ILC_COLOR32 | ILC_MASK, 12, 0);
    bool complete = images != nullptr;
    for (const ToolbarItem& item : TOOLBAR_ITEMS) {
        if (!item.draw || !complete) continue;
        HICON icon = make_toolbar_icon(item.draw, icon_size);
        if (!icon || ImageList_AddIcon(images, icon) < 0) complete = false;
        if (icon) DestroyIcon(icon);
    }
    if (complete) {
        SendMessageW(g_toolbar, TB_SETBITMAPSIZE, 0, MAKELPARAM(icon_size, icon_size));
        HIMAGELIST old = reinterpret_cast<HIMAGELIST>(
            SendMessageW(g_toolbar, TB_SETIMAGELIST, 0, reinterpret_cast<LPARAM>(images)));
        g_toolbar_images = images;
        if (old) ImageList_Destroy(old);
    } else if (images) {
        ImageList_Destroy(images);
    }
    const int button_w = main_layout_scale(26, dpi), button_h = main_layout_scale(24, dpi);
    SendMessageW(g_toolbar, TB_SETBUTTONSIZE, 0, MAKELPARAM(button_w, button_h));
    for (size_t i = 0; i < sizeof(TOOLBAR_ITEMS) / sizeof(TOOLBAR_ITEMS[0]); ++i) {
        if (TOOLBAR_ITEMS[i].command) continue;
        TBBUTTONINFOW info{};
        info.cbSize = sizeof(info);
        info.dwMask = TBIF_BYINDEX | TBIF_SIZE;
        info.cx = static_cast<WORD>(main_layout_scale(8, dpi));
        SendMessageW(g_toolbar, TB_SETBUTTONINFOW, static_cast<WPARAM>(i), reinterpret_cast<LPARAM>(&info));
    }
    const DWORD size = static_cast<DWORD>(SendMessageW(g_toolbar, TB_GETBUTTONSIZE, 0, 0));
    g_toolbar_h = std::max(main_layout_scale(28, dpi), static_cast<int>(HIWORD(size)));
    SendMessageW(g_toolbar, TB_AUTOSIZE, 0, 0);
}

void create_toolbar(HWND parent) {
    if (g_toolbar) return;
    g_toolbar = CreateWindowExW(0, TOOLBARCLASSNAMEW, nullptr,
                                WS_CHILD | WS_VISIBLE | TBSTYLE_FLAT | TBSTYLE_TOOLTIPS |
                                CCS_NODIVIDER | CCS_NOPARENTALIGN | CCS_NORESIZE,
                                0, 0, 1, 1, parent,
                                reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_TOOLBAR)),
                                GetModuleHandleW(nullptr), nullptr);
    if (!g_toolbar) return;
    SendMessageW(g_toolbar, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);
    SendMessageW(g_toolbar, TB_SETBITMAPSIZE, 0, MAKELPARAM(16, 16));
    // Texte bleiben für Tooltips/Barrierefreiheit verfügbar, vergrößern aber
    // die klassischen Icon-Knöpfe nicht zu einer breiten Textleiste.
    SendMessageW(g_toolbar, TB_SETEXTENDEDSTYLE, 0, TBSTYLE_EX_MIXEDBUTTONS);
    TBBUTTON buttons[sizeof(TOOLBAR_ITEMS) / sizeof(TOOLBAR_ITEMS[0])]{};
    int image = 0;
    for (size_t i = 0; i < sizeof(TOOLBAR_ITEMS) / sizeof(TOOLBAR_ITEMS[0]); ++i) {
        const ToolbarItem& item = TOOLBAR_ITEMS[i];
        buttons[i].fsState = TBSTATE_ENABLED;
        buttons[i].idCommand = item.command;
        buttons[i].iString = -1;
        if (item.command) {
            buttons[i].iBitmap = image++;
            buttons[i].fsStyle = BTNS_BUTTON;
        } else {
            buttons[i].iBitmap = main_layout_scale(8, g.main_dpi);
            buttons[i].fsStyle = BTNS_SEP;
        }
    }
    SendMessageW(g_toolbar, TB_ADDBUTTONS,
                 sizeof(buttons) / sizeof(buttons[0]), reinterpret_cast<LPARAM>(buttons));
    for (const ToolbarItem& item : TOOLBAR_ITEMS) {
        if (!item.command) continue;
        TBBUTTONINFOW info{};
        info.cbSize = sizeof(info);
        info.dwMask = TBIF_TEXT;
        info.pszText = const_cast<LPWSTR>(item.tip);
        SendMessageW(g_toolbar, TB_SETBUTTONINFOW, item.command, reinterpret_cast<LPARAM>(&info));
    }
    toolbar_set_dpi(g.main_dpi);
}

void palette_place(MainPanel& panel, int width, int content_height) {
    const MainPaletteLayout p = main_palette_layout(width, content_height, panel.dpi);
    auto put = [&](int id, MainRect rect) { main_panel_place(panel, GetDlgItem(panel.window, id), rect); };
    put(IDC_GRP_TOOL, p.tools);
    const int radios[5] = {IDC_BTN_PLACE, IDC_BTN_SELECT, IDC_BTN_MOVE, IDC_BTN_PAINT, IDC_BTN_HEIGHT};
    for (int i = 0; i < 5; ++i) put(radios[i], p.radios[i]);
    put(IDC_GRP_PREFAB, p.prefabs);
    put(IDC_PREFAB_LIST, p.prefab_list);
    put(IDC_GRP_ACT, p.actions);
    put(IDC_BTN_ROTATE, p.rotate);
    put(IDC_BTN_DELETE, p.remove);
    put(IDC_BTN_DUPLICATE, p.duplicate);
    put(IDC_BTN_UNDO, p.undo);
    put(IDC_BTN_REDO, p.redo);
}

void inspector_place(MainPanel& panel, int width, int content_height) {
    const MainInspectorLayout p = main_inspector_layout(width, content_height, panel.dpi);
    auto put = [&](int id, MainRect rect) { main_panel_place(panel, GetDlgItem(panel.window, id), rect); };
    put(IDC_LBL_OBJECTS, p.objects_label);
    put(IDC_OBJ_LIST, p.objects);
    put(IDC_GRP_PROPS, p.props);
    const int labels[4] = {IDC_LBL_X, IDC_LBL_Z, IDC_LBL_ROT, IDC_LBL_TEX};
    const int fields[4] = {IDC_ED_X, IDC_ED_Z, IDC_ED_ROT, IDC_ED_TEX};
    for (int i = 0; i < 4; ++i) { put(labels[i], p.labels[i]); put(fields[i], p.fields[i]); }
    put(IDC_BTN_APPLY, p.apply);
    put(IDC_LBL_PROPS_HINT, p.hint);
}

BOOL CALLBACK main_font_child(HWND child, LPARAM param) {
    const HWND parent = GetParent(child);
    if ((parent == g.main && child != g.view && child != g_toolbar &&
         child != g.palette.window && child != g.inspector.window) ||
        parent == g.palette.window || parent == g.inspector.window) {
        SendMessageW(child, WM_SETFONT, static_cast<WPARAM>(param), TRUE);
    }
    return TRUE;
}

void main_set_dpi(unsigned dpi) {
    const unsigned old_dpi = g.main_dpi;
    g.main_dpi = main_layout_dpi(dpi);
    main_panel_set_dpi(g.palette, g.main_dpi);
    main_panel_set_dpi(g.inspector, g.main_dpi);
    if (HFONT font = event_view_make_font(g.main_dpi)) {
        const HFONT old = g.main_font;
        g.main_font = font;
        if (g.main) EnumChildWindows(g.main, main_font_child, reinterpret_cast<LPARAM>(font));
        if (old) DeleteObject(old);
    }
    if (g.list_prefab)
        SendMessageW(g.list_prefab, LB_SETITEMHEIGHT, 0, main_layout_scale(22, g.main_dpi));
    if (g.map_tree) SendMessageW(g.map_tree, TVM_SETITEMHEIGHT, static_cast<WPARAM>(-1), 0);
    if (g.list_objs && old_dpi != g.main_dpi) {
        // Von Nutzern angepasste Spaltenbreiten beim Monitorwechsel bewahren.
        for (int i = 0; i < 6; ++i) {
            const int old_width = ListView_GetColumnWidth(g.list_objs, i);
            ListView_SetColumnWidth(g.list_objs, i,
                std::max(1, event_layout_rescale(old_width, old_dpi, g.main_dpi)));
        }
    }
    toolbar_set_dpi(g.main_dpi);
}

void layout_children(bool resize_renderer = true) {
    if (!g.main || !g.view || !g.status || !g.palette.window || !g.inspector.window) return;
    RECT client{};
    GetClientRect(g.main, &client);
    const int width = std::max(0, static_cast<int>(client.right));
    const int height = std::max(0, static_cast<int>(client.bottom));
    SendMessageW(g.status, WM_SIZE, 0, 0);
    RECT status{};
    GetWindowRect(g.status, &status);
    const int status_h = std::max(0, static_cast<int>(status.bottom - status.top));
    const int toolbar_h = toolbar_height();
    const MainRect old_split = g.main_layout.splitter;
    const MainWindowLayout layout = main_window_layout(width, height, toolbar_h, status_h,
                                                       g.wanted_map_height, g.main_dpi);
    g.main_layout = layout;
    auto move = [](HWND child, MainRect r) {
        if (child) SetWindowPos(child, nullptr, r.x, r.y, r.w, r.h,
                                SWP_NOZORDER | SWP_NOACTIVATE);
    };
    move(g_toolbar, {0, 0, width, toolbar_h});
    move(g.palette.window, layout.palette);
    move(g.inspector.window, layout.inspector);
    move(GetDlgItem(g.main, IDC_LBL_MAPS), layout.maps_label);
    move(GetDlgItem(g.main, IDC_MAP_NEW_BUTTON), layout.maps_new);
    const HWND map_props = GetDlgItem(g.main, IDC_MAP_PROPS_BUTTON);
    move(map_props, layout.maps_props);
    const bool compact_props = main_map_props_compact(layout.maps_props.w, g.main_dpi);
    if (map_props && compact_props != g.map_props_compact) {
        SetWindowTextW(map_props, compact_props ? L"F2" : L"Eigenschaften");
        g.map_props_compact = compact_props;
    }
    move(g.map_tree, layout.maps_tree);
    move(g.view, layout.viewport);
    main_panel_update(g.palette);
    main_panel_update(g.inspector);

    // Das erste Feld enthält Bedienhinweise und Warnungen; die restlichen
    // Felder zeigen Objektzahl, Cursor und Raster/FPS.
    const int parts[4] = {width * 40 / 100, width * 52 / 100, width * 77 / 100, -1};
    SendMessageW(g.status, SB_SETPARTS, 4, reinterpret_cast<LPARAM>(parts));
    // Auch die frühere Leistenposition löschen, damit beim Ziehen keine
    // verbliebenen Linien auf dem Hauptfenster-Hintergrund stehen bleiben.
    RECT previous{old_split.x, old_split.y, old_split.x + old_split.w, old_split.y + old_split.h};
    RECT splitter{layout.splitter.x, layout.splitter.y,
                  layout.splitter.x + layout.splitter.w, layout.splitter.y + layout.splitter.h};
    InvalidateRect(g.main, &previous, TRUE);
    InvalidateRect(g.main, &splitter, TRUE);

    if (resize_renderer) {
        // Der Viewport hat WS_EX_CLIENTEDGE: die Swapchain muss zum inneren
        // Client-Rechteck passen, nicht zur äußeren Fenstergröße.
        RECT view{};
        GetClientRect(g.view, &view);
        renderer_resize(static_cast<int>(view.right - view.left),
                        static_cast<int>(view.bottom - view.top));
    }
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
    // Andere Aktionen überschreiben status_mode. Ein ausgefallenes Terrain
    // muss trotzdem sichtbar bleiben, bis ein Neuaufbau wieder gelingt.
    if (g.terrain_memory_error || !g.terrain_error.empty()) status_text(0, L"Keine Gelände-Vorschau — F10");
    else if (!g.terrain_warning.empty()) status_text(0, L"Boden-Vorschau eingeschränkt — F10");
    else if (g.terrain_field_warning || g.atmosphere_warning) status_text(0, L"Terrain-/Atmosphäre-Ersatz — F10");
    else status_text(0, g.status_mode);
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
    // Bereits gepostete Mausaktionen können eine MessageBox-Schleife noch
    // erreichen. Keine JSON-Bearbeitung während einer Datenentscheidung;
    // Mouse-Up bleibt für Capture-/Drag-Aufräumen zulässig.
    if (g.data_prompt_active &&
        ((msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST && msg != WM_LBUTTONUP && msg != WM_RBUTTONUP) ||
         msg == WM_CONTEXTMENU)) return 0;
    // Während eines offenen Dialogs keine Kamera-/Editier-Eingaben.
    if (g.props_dlg && (msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST)) return 0;
    switch (msg) {
        case WM_SIZE:
            return 0;
        case WM_RBUTTONDOWN:
            cancel_object_drag();
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
            if (g.dragging && g.objects && g.obj_drag >= 0 && g.sel_obj == g.obj_drag &&
                g.obj_drag < static_cast<int>(g.objects->arr.size())) {
                // Ein künstliches WM_MOUSEMOVE an der Klickposition darf das
                // Objekt noch nicht verschieben (auch nicht ans Raster).
                if (!g.drag_pointer_moved && mx == g.drag_mouse_x && my == g.drag_mouse_y) return 0;
                g.drag_pointer_moved = true;
                // Ray-Schnitt auf der ursprünglichen horizontalen Ebene. Der
                // Versatz zum Griffpunkt verhindert ein Springen beim Klick.
                RECT rc;
                GetClientRect(h, &rc);
                const float vw = static_cast<float>(std::max(1, static_cast<int>(rc.right)));
                const float vh = static_cast<float>(std::max(1, static_cast<int>(rc.bottom)));
                const float ndc_x = (2.0f * static_cast<float>(mx) / vw) - 1.0f;
                const float ndc_y = 1.0f - (2.0f * static_cast<float>(my) / vh);
                const math::Vec3 dir = ray_dir(ndc_x, ndc_y, vw / vh);
                if (std::fabs(dir.y) > 1e-4f) {
                    const float t = (g.drag_y - g.cam_pos.y) / dir.y;
                    if (t > 0.0f && std::isfinite(t)) {
                        const math::Vec3 hit = math::add(g.cam_pos, math::scale(dir, t));
                        JsonValue& object = g.objects->arr[static_cast<size_t>(g.obj_drag)];
                        ObjectDragEdit edit = object_drag_prepare(object, g.drag_original,
                            static_cast<double>(hit.x) - g.drag_anchor_x,
                            static_cast<double>(hit.z) - g.drag_anchor_z, g.drag_grid);
                        if (edit.ok && edit.changed) {
                            if (!g.drag_undo_pushed) { push_undo(); g.drag_undo_pushed = true; }
                            object = std::move(edit.value);
                            g.dirty = true;
                            sync_property_fields();
                            sync_object_list();
                            update_title();
                        }
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
            cancel_object_drag();
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
            // Nach einem Viewport-Klick landen Tastatureingaben nicht weiter
            // im zuvor aktiven Eigenschaftenfeld.
            SetFocus(h);
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
                if (!select_object(idx)) return 0;
                status_mode(idx >= 0 ? L"Ausgewählt" : L"Kein Treffer");
                if (g.move_mode && idx >= 0) {
                    // Auch bei erneutem Klick auf dasselbe Objekt dürfen nicht
                    // übernommene Felder beim ersten Drag-Schritt nicht verschwinden.
                    if (!resolve_object_edits() || !object_viewable(idx)) return 0;
                    const float drag_y = base_y_at(obj_number(idx, "x", 0), obj_number(idx, "z", 0)) +
                                         obj_number(idx, "y", 0);
                    if (!std::isfinite(drag_y) || std::fabs(dir.y) <= 1e-4f) return 0;
                    const float t_plane = (drag_y - g.cam_pos.y) / dir.y;
                    if (!std::isfinite(t_plane) || t_plane <= 0.0f) return 0;
                    const math::Vec3 anchor = math::add(g.cam_pos, math::scale(dir, t_plane));
                    if (!std::isfinite(anchor.x) || !std::isfinite(anchor.z)) return 0;
                    // Capture sofort; Undo erst beim ersten Positionswechsel.
                    g.obj_drag = idx;
                    g.drag_undo_pushed = false;
                    g.drag_pointer_moved = false;
                    g.drag_mouse_x = GET_X_LPARAM(lp);
                    g.drag_mouse_y = GET_Y_LPARAM(lp);
                    g.drag_y = drag_y;
                    g.drag_anchor_x = anchor.x;
                    g.drag_anchor_z = anchor.z;
                    g.drag_grid = g.grid;
                    g.drag_original = g.objects->arr[static_cast<size_t>(idx)];
                    g.dragging = true;
                    SetCapture(h);
                    if (GetCapture() != h) { cancel_object_drag(); return 0; }
                    status_mode(L"Ziehen ...");
                }
            }
            return 0;
        }
        case WM_LBUTTONUP:
            if (g.dragging) {
                const bool moved = g.drag_undo_pushed;
                if (g.obj_drag >= 0 && g.objects) {
                    core::log_info("Drag-Ende: idx=" + std::to_string(g.obj_drag) +
                                   " x=" + std::to_string(obj_number(g.obj_drag, "x", 0)) +
                                   " z=" + std::to_string(obj_number(g.obj_drag, "z", 0)));
                }
                cancel_object_drag();
                status_mode(moved ? L"Verschoben" : L"Ausgewählt");
            }
            return 0;
        case WM_CAPTURECHANGED:
            if (g.dragging) cancel_object_drag();
            g.look_drag = false;
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
    if (!object_viewable(i)) return;
    const Prefab* pre = prefab_of(obj_type(i));
    if (!pre) return;
    const float x = obj_number(i, "x", 0);
    const float z = obj_number(i, "z", 0);
    const float rot = obj_number(i, "rot", 0);
    const float by = base_y_at(x, z) + obj_number(i, "y", 0);
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
                             override_tex >= 0 ? override_tex : g.renderer->texture_index(part.tex),
                             part.tile);
    }
}

// Gelber Rahmen um das ausgewählte Objekt (12 dünne Kästen).
void push_selection_frame(int i) {
    if (!object_viewable(i)) return;
    const Prefab* pre = prefab_of(obj_type(i));
    if (!pre) return;
    const float x = obj_number(i, "x", 0);
    const float z = obj_number(i, "z", 0);
    const float rot = obj_number(i, "rot", 0);
    const float by = base_y_at(x, z) + obj_number(i, "y", 0);
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

// NPCs und Schild-Events zeigen dieselben Maße und Bodenversätze wie das
// Spiel. Wegpunkte werden als kleine blaue Bodenmarken sichtbar.
void push_placements() {
    auto number = [](const JsonValue& o, const char* key, float fallback = 0.0f) {
        const JsonValue* v = o.find(key);
        if (!v || !v->is_num() || !std::isfinite(v->number()) || std::fabs(v->number()) > 1000000.0)
            return fallback;
        return static_cast<float>(v->number());
    };
    const JsonValue* npcs = g.map_root.find("npcs");
    if (npcs && npcs->is_arr()) {
        for (const JsonValue& npc : npcs->arr) {
            if (!npc.is_obj()) continue;
            const float x = number(npc, "x"), z = number(npc, "z"), y = number(npc, "y");
            uint32_t color = 0xC49A6C;
            if (const JsonValue* c = npc.find("color"); c && c->is_num() &&
                std::isfinite(c->number()) && c->number() >= 0.0 && c->number() <= 0xFFFFFF)
                color = static_cast<uint32_t>(c->number());
            g.renderer->push_box(math::Vec3{x, base_y_at(x, z) + y + 0.9f, z},
                                 math::Vec3{0.7f, 1.8f, 0.7f}, math::Vec3{}, color);
            const JsonValue* path = npc.find("path");
            if (!path || !path->is_arr()) continue;
            for (const JsonValue& p : path->arr) {
                if (!p.is_arr() || p.arr.size() != 2 || !p.arr[0].is_num() || !p.arr[1].is_num() ||
                    !std::isfinite(p.arr[0].number()) || !std::isfinite(p.arr[1].number()) ||
                    std::fabs(p.arr[0].number()) > 1000000.0 || std::fabs(p.arr[1].number()) > 1000000.0) continue;
                const float px = static_cast<float>(p.arr[0].number()), pz = static_cast<float>(p.arr[1].number());
                g.renderer->push_box(math::Vec3{px, base_y_at(px, pz) + y + 0.12f, pz},
                                     math::Vec3{0.35f, 0.24f, 0.35f}, math::Vec3{}, 0x45B7F3);
            }
        }
    }
    const JsonValue* events = g.map_root.find("events");
    if (events && events->is_arr()) {
        for (const JsonValue& event : events->arr) {
            if (!event.is_obj()) continue;
            const JsonValue* visual = event.find("visual");
            if (!visual || !visual->is_str() || visual->str != "schild") continue;
            const float x = number(event, "x"), z = number(event, "z");
            const float y = base_y_at(x, z) + number(event, "y");
            const math::Vec3 rot{0, number(event, "rot"), 0};
            g.renderer->push_box(math::Vec3{x, y + 0.65f, z}, math::Vec3{0.18f, 1.3f, 0.18f}, rot, 0x76502F);
            g.renderer->push_box(math::Vec3{x, y + 1.55f, z}, math::Vec3{1.35f, 0.65f, 0.15f}, rot, 0xD5AB69);
        }
    }
}

void render_frame() {
    // WM_SIZE kann schon beim Anlegen/Positionieren des Hauptfensters kommen,
    // bevor Viewport und D3D-Renderer bereit sind.
    if (!g.view || !g.renderer) return;
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
    push_placements();
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

bool object_field_focused() {
    const HWND focus = GetFocus();
    return focus && (focus == g.ed_x || focus == g.ed_z || focus == g.ed_rot || focus == g.ed_tex);
}

void update_camera(float dt) {
    // Eingaben in anderen Programmen oder moduslosen Editorfenstern gehören
    // nicht zur Kamera. Auch Objektfelder und ein Drag halten sie still.
    if (GetForegroundWindow() != g.main || object_field_focused() || g.dragging) return;
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
    const float ground = base_y_at(g.cam_pos.x, g.cam_pos.z);
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
    // GetAsyncKeyState sieht systemweite Tasten. Projekt-/Tileset-Dialoge und
    // andere Programme dürfen damit nicht versehentlich Objekte verändern.
    if (GetForegroundWindow() != g.main) return;
    if (g.props_dlg) return;   // kein Editieren hinter einem offenen Dialog
    if (event_editor_focused()) return;   // Tippen im Event-Editor soll nichts auslösen
    if (script_editor_focused()) return;  // dito für den Script-Editor
    if (material_editor_focused()) return;
    if (database_editor_focused()) return;
    if (soundtest_focused()) return;
    // Etappe 18k: Umbenennen im Kartenbaum - Edit-Control im TreeView aktiv?
    if (g.map_tree && TreeView_GetEditControl(g.map_tree)) return;
    const bool object_editing = object_field_focused();
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
    // Strg+S auch im Objektfeld; bei Fehler nicht mehrfach pro gehaltenem
    // Tastendruck denselben Validierungsdialog öffnen.
    {
        static bool save_was = false;
        const bool save_now = (GetAsyncKeyState(VK_CONTROL) & 0x8000) &&
                              (GetAsyncKeyState('S') & 0x8000);
        if (save_now && !save_was && save_map()) {
            status_mode(L"Gespeichert");
            update_title();
        }
        save_was = save_now;
    }
    // Strg+Z / Strg+Y: Rückgängig und Wiederholen
    {
        const bool ctrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
        static bool z_was = false, y_was = false;
        const bool z_now = ctrl && (GetAsyncKeyState('Z') & 0x8000) != 0;
        const bool y_now = ctrl && (GetAsyncKeyState('Y') & 0x8000) != 0;
        // Im Editfeld gehört Strg+Z/Y dem Text-Control, nicht der Karte.
        if (!object_editing && z_now && !z_was) undo_action();
        if (!object_editing && y_now && !y_was) redo_action();
        z_was = z_now;
        y_was = y_now;
    }

    // Etappe 18e: Pinselgrösse im Mal-Modus mit +/- (auch Zehnertastatur).
    // Etappe 18l: auch im Höhen-Modus (Pinsel ist dort der Berg-Radius).
    if ((g.paint_mode || g.height_mode) && !object_editing) {
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
        static bool f1w = false, f2w = false, f3w = false, f4w = false, f5w = false;
        static bool f6w = false, f7w = false, f8w = false, f9w = false, f10w = false, f11w = false;
        struct { int vk; bool* was; } fkeys[] = {
            { VK_F1,  &f1w },  { VK_F2,  &f2w },  { VK_F3,  &f3w },
            { VK_F4,  &f4w },  { VK_F5,  &f5w },  { VK_F6,  &f6w },
            { VK_F7,  &f7w },  { VK_F8,  &f8w },  { VK_F9,  &f9w },
            { VK_F10, &f10w }, { VK_F11, &f11w },
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
                    case VK_F5:  PostMessageW(g.main, WM_COMMAND, ID_MENU_PLACEMENT, 0); break;
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
        if (!object_editing && r_now && !r_was) {
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
        if (!object_editing && del_now && !del_was) delete_selected();
        del_was = del_now;
    }

    // Etappe 18a: Esc = Auswahl aufheben, Enter = Eigenschaften übernehmen.
    {
        static bool esc_was = false, ent_was = false;
        const bool esc_now = (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;
        const bool ent_now = (GetAsyncKeyState(VK_RETURN) & 0x8000) != 0;
        if (esc_now && !esc_was && g.sel_obj >= 0 && select_object(-1)) {
            status_mode(L"Auswahl aufgehoben");
            core::log_info("Auswahl aufgehoben (Esc)");
        }
        if (ent_now && !ent_was && g.sel_obj >= 0 && apply_property_fields()) {
            update_title();
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
        if (!object_editing && g_now && !g_was) {
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
    // MessageBox pumpt eine verschachtelte Nachrichtenschleife. Bereits
    // gepostete Auswahl-/Menüaktionen dürfen während der Datenentscheidung
    // weder die Karte wechseln noch das vorbereitete Objekt ersetzen.
    if (g.data_prompt_active) {
        switch (msg) {
            case WM_RECONCILE_OBJECT_SELECTION:
                g.object_selection_queued = false;
                g.object_selection_delayed = true;
                return 0;
            case WM_RECONCILE_MAP_SELECTION:
                g.map_selection_queued = false;
                g.map_selection_delayed = true;
                return 0;
            case WM_REBUILD_MAP_MENU:
                g.map_menu_delayed = true;
                return 0;
            case WM_TREE_ACTION:
                // Nach der Rückfrage erneut zustellen; Identität und Auswahl
                // werden erst nach dem Kartenabgleich geprüft.
                g.map_action_queued = false;
                return 0;
            case WM_OBJECT_LIST_FOCUS: case WM_OBJECT_LIST_CONTEXT: case WM_OBJECT_LIST_SORT:
            case WM_TREE_RENAME_WARNING:
            case WM_COMMAND: case WM_CONTEXTMENU: case WM_CLOSE:
            case WM_LBUTTONDOWN: case WM_MOUSEMOVE:
                return 0;
            case WM_NOTIFY: {
                const NMHDR* nm = reinterpret_cast<const NMHDR*>(lp);
                if (!nm) return 0;
                if (nm->idFrom == IDC_MAPTREE && nm->hwndFrom == g.map_tree) {
                    if (nm->code == TVN_SELCHANGING || nm->code == TVN_BEGINLABELEDIT) return TRUE;
                    if (nm->code == TVN_SELCHANGED) g.map_selection_delayed = true;
                } else if (nm->idFrom == IDC_OBJ_LIST && nm->code == LVN_ITEMCHANGED) {
                    g.object_selection_delayed = true;
                }
                return 0;
            }
            case WM_DROPFILES:
                DragFinish(reinterpret_cast<HDROP>(wp));
                return 0;
            default: break;
        }
    }
    switch (msg) {
        case WM_TREE_RENAME_WARNING:
            if (wp == 1)
                data_prompt(g.main, L"Der Kartenname ist im JSON mehrfach vorhanden und kann hier nicht eindeutig geändert werden.",
                            L"Karte umbenennen", MB_OK | MB_ICONWARNING);
            else if (wp == 2)
                data_prompt(g.main, L"Bitte zuerst den Event-Editor schließen; der Kartenname fehlt im JSON.",
                            L"Karte umbenennen", MB_OK | MB_ICONINFORMATION);
            return 0;
        case WM_RECONCILE_OBJECT_SELECTION: {
            g.object_selection_queued = false;
            if (g.syncing_object_list || g.object_selection_resolving) return 0;
            // Alle LVN_ITEMCHANGED-Meldungen derselben Geste sind vorbei.
            // Auch MessageBox und Listen-Neubau laufen erst jetzt, außerhalb
            // des ListView-Callbacks. Bei Abbruch die alte Auswahl herstellen.
            g.object_selection_resolving = true;
            const int next = object_list_selected();
            if (next != g.sel_obj) {
                if (select_object(next)) status_mode(next >= 0 ? L"Ausgewählt (Liste)" : L"Auswahl aufgehoben");
                else restore_object_list_selection();
            }
            g.object_selection_resolving = false;
            return 0;
        }
        case WM_OBJECT_LIST_FOCUS:
            if (static_cast<int>(wp) - 1 == g.sel_obj && g.sel_obj >= 0)
                SendMessageW(h, WM_COMMAND, ID_CTX_FOCUS, 0);
            return 0;
        case WM_OBJECT_LIST_CONTEXT:
            if (static_cast<int>(wp) - 1 == g.sel_obj)
                TrackPopupMenu(g.ctx_obj, TPM_RIGHTBUTTON, GET_X_LPARAM(lp), GET_Y_LPARAM(lp), 0, h, nullptr);
            return 0;
        case WM_OBJECT_LIST_SORT: {
            const int column = static_cast<int>(wp);
            if (column < 0 || column > 5) return 0;
            if (g.obj_sort_col == column) g.obj_sort_asc = !g.obj_sort_asc;
            else { g.obj_sort_col = column; g.obj_sort_asc = true; }
            sync_object_list();
            status_mode(g.obj_sort_asc ? L"Objektliste aufsteigend sortiert"
                                       : L"Objektliste absteigend sortiert");
            return 0;
        }
        case WM_RECONCILE_MAP_SELECTION: {
            g.map_selection_queued = false;
            if (g.map_selection_resolving) return 0;
            if (g.props_dlg) {
                g.map_action = {};
                maptree_select_current();
                return 0;
            }
            const int next = maptree_selected();
            if (next >= 0 && next != g.cur_map) {
                g.map_selection_resolving = true;
                if (load_map(next)) {
                    sync_object_list();
                    sync_property_fields();
                    update_title();
                    status_mode(L"Karte geladen");
                } else {
                    maptree_select_current();
                }
                g.map_selection_resolving = false;
            }
            maptree_schedule_action();
            return 0;
        }
        case WM_REBUILD_MAP_MENU:
            rebuild_map_menu();
            return 0;
        case WM_GETMINMAXINFO: {
            if (!lp) break;
            const unsigned dpi = main_layout_dpi(GetDpiForWindow(h));
            DWORD style = static_cast<DWORD>(GetWindowLongPtrW(h, GWL_STYLE));
            const DWORD ex_style = static_cast<DWORD>(GetWindowLongPtrW(h, GWL_EXSTYLE));
            if (!style) style = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN;
            RECT frame{0, 0, main_layout_scale(700, dpi), main_layout_scale(360, dpi)};
            if (!AdjustWindowRectExForDpi(&frame, style, TRUE, ex_style, dpi))
                AdjustWindowRectEx(&frame, style, TRUE, ex_style);
            int min_w = std::max(1, static_cast<int>(frame.right - frame.left));
            int min_h = std::max(1, static_cast<int>(frame.bottom - frame.top));
            EventLayoutRect work{};
            if (event_view_work_area(h, nullptr, work)) {
                min_w = std::min(min_w, std::max(1, work.right - work.left));
                min_h = std::min(min_h, std::max(1, work.bottom - work.top));
            }
            MINMAXINFO* mmi = reinterpret_cast<MINMAXINFO*>(lp);
            mmi->ptMinTrackSize.x = min_w;
            mmi->ptMinTrackSize.y = min_h;
            return 0;
        }
        case WM_DPICHANGED: {
            if (!lp) break;
            cancel_splitter_drag();
            main_set_dpi(LOWORD(wp));
            EventLayoutRect target = event_view_rect(*reinterpret_cast<const RECT*>(lp)), work{};
            if (event_view_work_area(h, reinterpret_cast<const RECT*>(lp), work))
                target = event_layout_fit(work, target);
            SetWindowPos(h, nullptr, target.left, target.top, target.right - target.left,
                         target.bottom - target.top, SWP_NOZORDER | SWP_NOACTIVATE);
            layout_children();
            main_panel_reveal(g.palette, GetFocus());
            main_panel_reveal(g.inspector, GetFocus());
            return 0;
        }
        case WM_PAINT: {
            PAINTSTRUCT paint{};
            HDC dc = BeginPaint(h, &paint);
            const MainRect& split = g.main_layout.splitter;
            if (split.w > 0 && split.h > 0) {
                RECT line{split.x, split.y, split.x + split.w, split.y + split.h};
                DrawEdge(dc, &line, EDGE_ETCHED, BF_TOP | BF_BOTTOM);
                const int half = std::min(split.w / 3, main_layout_scale(18, g.main_dpi));
                const int cx = split.x + split.w / 2, cy = split.y + split.h / 2;
                HPEN pen = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_BTNSHADOW));
                HGDIOBJ old = SelectObject(dc, pen);
                MoveToEx(dc, cx - half, cy, nullptr); LineTo(dc, cx + half, cy);
                SelectObject(dc, old);
                DeleteObject(pen);
            }
            EndPaint(h, &paint);
            return 0;
        }
        case WM_SETCURSOR: {
            if (LOWORD(lp) != HTCLIENT) break;
            POINT point{};
            GetCursorPos(&point);
            ScreenToClient(h, &point);
            if (g.splitter_drag || main_rect_contains(g.main_layout.splitter, point.x, point.y)) {
                SetCursor(LoadCursorW(nullptr, IDC_SIZENS));
                return TRUE;
            }
            break;
        }
        case WM_LBUTTONDOWN: {
            const int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
            if (!g.props_dlg && main_rect_contains(g.main_layout.splitter, x, y)) {
                cancel_object_drag();
                g.splitter_drag = true;
                g.splitter_grab = y - g.main_layout.splitter.y;
                SetCapture(h);
                if (GetCapture() != h) g.splitter_drag = false;
                return 0;
            }
            break;
        }
        case WM_MOUSEMOVE:
            if (g.splitter_drag) {
                if (GetCapture() != h) { g.splitter_drag = false; return 0; }
                RECT client{}, status{};
                GetClientRect(h, &client);
                GetWindowRect(g.status, &status);
                g.wanted_map_height = main_map_height_for_split(
                    static_cast<int>(client.right), static_cast<int>(client.bottom), toolbar_height(),
                    static_cast<int>(status.bottom - status.top), GET_Y_LPARAM(lp) - g.splitter_grab,
                    g.main_dpi);
                // Nur die linke Spalte ändert sich; keine D3D-Swapchain neu anlegen.
                layout_children(false);
                return 0;
            }
            break;
        case WM_LBUTTONUP:
            if (g.splitter_drag) { cancel_splitter_drag(); return 0; }
            break;
        case WM_CAPTURECHANGED:
            g.splitter_drag = false;
            return 0;
        case WM_CANCELMODE:
            cancel_splitter_drag();
            break;
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
                const std::string path = wide_to_utf8(wv);
                std::string lower = path;
                for (char& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
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
                        const float span = g.terrain && g.terrain->ready()
                            ? std::max(g.terrain->width(), g.terrain->height()) : 240.0f;
                        g.cam_pos = math::Vec3{ 0.0f, span * 0.10f + 8.0f, span * 0.18f };
                        g.cam_yaw = math::PI;
                        g.cam_pitch = -0.5f;
                    }
                    return 0;
                case ID_MENU_MAPS_MORE:
                case ID_MENU_PALETTE_MORE:
                case ID_MENU_LAYOUT_RESET: {
                    if (id == ID_MENU_LAYOUT_RESET) {
                        g.wanted_map_height = MAIN_DEFAULT_MAP_HEIGHT;
                    } else {
                        RECT client{}, status{};
                        GetClientRect(h, &client);
                        GetWindowRect(g.status, &status);
                        const int step = main_layout_scale(40, g.main_dpi);
                        const int split = g.main_layout.splitter.y +
                            (id == ID_MENU_MAPS_MORE ? -step : step);
                        g.wanted_map_height = main_map_height_for_split(
                            static_cast<int>(client.right), static_cast<int>(client.bottom),
                            toolbar_height(), static_cast<int>(status.bottom - status.top), split, g.main_dpi);
                    }
                    layout_children(false);
                    main_panel_reveal(g.palette, GetFocus());
                    main_panel_reveal(g.inspector, GetFocus());
                    return 0;
                }
                case ID_MENU_MAPPROPS:
                    open_map_props();
                    return 0;
                case IDC_MAP_PROPS_BUTTON: {
                    const int selected = maptree_selected();
                    if (selected >= 0) maptree_queue_action(MapTreeActionKind::Properties, selected);
                    return 0;
                }
                case IDC_MAP_NEW_BUTTON:
                    maptree_queue_action(MapTreeActionKind::New, maptree_selected());
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
                case ID_MENU_PLACEMENT:
                    open_placement_dialog();
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
                                L"  F5 NPCs, Schilder und Objekte platzieren\n"
                                L"  F6 Event-Editor    F7 Tileset    F8 Sound-Test\n"
                                L"  F9 Datenbank (Helden/Items/Gegner/Truppen)\n"
                                L"  F11 Script-Editor\n\n"
                                L"Links: Palette über dem Kartenbaum; Trennleiste ziehen.\n"
                                L"Ansicht: Bereiche vergrößern oder Aufteilung zurücksetzen.\n"
                                L"Karten: Neu, Eigenschaften (F2) oder Rechtsklick auf eine Karte.\n"
                                L"Bei wenig Platz lassen sich die Seitenleisten scrollen.\n"
                                L"Playtest: speichert die gewählte Karte und startet\n"
                                L"Rune3D.exe mit Projekt und Karte (ohne Titelbildschirm).\n\n"
                                L"Der Editor arbeitet nur auf den Kartendaten - "
                                L"die Game-Engine bleibt unberührt.",
                                L"RuneEditor - Bedienung", MB_OK | MB_ICONINFORMATION);
                    return 0;
                case IDC_PREFAB_LIST:
                    if (code == LBN_SELCHANGE) {
                        g.sel_prefab = static_cast<int>(SendMessageW(g.list_prefab, LB_GETCURSEL, 0, 0));
                        set_mode(1);
                        status_mode((L"Setzen: " + utf8_to_wide(
                            g.sel_prefab >= 0 && g.sel_prefab < static_cast<int>(g.prefabs.size())
                                ? g.prefabs[static_cast<size_t>(g.sel_prefab)].name : std::string("?"))).c_str());
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
                    if (object_viewable(g.sel_obj)) {
                        cancel_object_drag();
                        const float ox = obj_number(g.sel_obj, "x", 0);
                        const float oz = obj_number(g.sel_obj, "z", 0);
                        g.cam_pos = math::Vec3{ ox, base_y_at(ox, oz) + obj_number(g.sel_obj, "y", 0) + 12.0f, oz + 18.0f };
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
                    if (apply_property_fields()) update_title();
                    return 0;
                default:
                    if (id >= ID_MENU_MAP_BASE &&
                        id < ID_MENU_MAP_BASE + static_cast<int>(g.map_files.size())) {
                        if (id - ID_MENU_MAP_BASE == g.cur_map) return 0;
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
            if (g.props_dlg) {
                SetForegroundWindow(g.props_dlg);
                return 0;   // erst den offenen Dialog bestätigen oder abbrechen
            }
            cancel_object_drag();
            if (!resolve_event_editor() || !resolve_object_edits()) return 0;
            if (g.dirty) {
                const int r = data_prompt(h, L"Ungespeicherte Änderungen verwerfen?",
                                          L"RuneEditor", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);
                if (r != IDYES) return 0;
            }
            if (database_editor_open() && !database_request_close()) return 0;
            DestroyWindow(h);
            return 0;

        // Etappe 18k: Kartenbaum - Auswahl laden, Doppelklick = Eigenschaften,
        // Umbenennen per Inplace-Edit (TVS_EDITLABELS).
        case WM_NOTIFY: {
            const NMHDR* nm = reinterpret_cast<const NMHDR*>(lp);
            if (!nm) break;
            if (g_toolbar && nm->code == TTN_GETDISPINFOW && nm->hwndFrom == reinterpret_cast<HWND>(
                    SendMessageW(g_toolbar, TB_GETTOOLTIPS, 0, 0))) {
                auto* tip = reinterpret_cast<NMTTDISPINFOW*>(lp);
                for (const ToolbarItem& item : TOOLBAR_ITEMS) {
                    if (item.command && static_cast<UINT_PTR>(item.command) == nm->idFrom) {
                        tip->lpszText = const_cast<LPWSTR>(item.tip);
                        return 0;
                    }
                }
            }
            // Etappe 19c: Objektliste (ListView) - Auswahl, Sortierung,
            // Doppelklick (Kamera) und Rechtsklick (Kontextmenü).
            if (nm->idFrom == IDC_OBJ_LIST && nm->hwndFrom) {
                if (nm->code == LVN_ITEMCHANGED) {
                    if (g.syncing_object_list) return 0;
                    const NMLISTVIEW* lv = reinterpret_cast<const NMLISTVIEW*>(lp);
                    if (((lv->uNewState ^ lv->uOldState) & LVIS_SELECTED) && !g.object_selection_queued) {
                        g.object_selection_queued = PostMessageW(h, WM_RECONCILE_OBJECT_SELECTION, 0, 0) != 0;
                    }
                    return 0;
                }
                if (nm->code == LVN_COLUMNCLICK) {
                    const NMLISTVIEW* lv = reinterpret_cast<const NMLISTVIEW*>(lp);
                    PostMessageW(h, WM_OBJECT_LIST_SORT, static_cast<WPARAM>(lv->iSubItem), 0);
                    return 0;
                }
                if (nm->code == NM_DBLCLK) {
                    // Nach dem Auswahlabgleich fokussieren, nicht versehentlich
                    // das vorherige Objekt, falls die Rückfrage abgebrochen wird.
                    const int selected = object_list_selected();
                    if (selected >= 0) PostMessageW(h, WM_OBJECT_LIST_FOCUS, static_cast<WPARAM>(selected + 1), 0);
                    return 0;
                }
                if (nm->code == NM_RCLICK) {
                    POINT pt{};
                    GetCursorPos(&pt);
                    PostMessageW(h, WM_OBJECT_LIST_CONTEXT, static_cast<WPARAM>(object_list_selected() + 1),
                                 MAKELPARAM(pt.x, pt.y));
                    return 0;
                }
                return 0;
            }
            if (nm->idFrom != IDC_MAPTREE || !nm->hwndFrom) break;
            switch (nm->code) {
                case TVN_SELCHANGING: {
                    // Der Dialog ist moduslos. Maus/Tastatur dürfen nicht zu
                    // einer anderen Karte wechseln, bevor er geschlossen ist.
                    // Programmgesteuerte Auswahl (z.B. maptree_fill) bleibt ok.
                    const NMTREEVIEWW* tv = reinterpret_cast<const NMTREEVIEWW*>(lp);
                    if (g.props_dlg && (tv->action == TVC_BYMOUSE ||
                                        tv->action == TVC_BYKEYBOARD)) return TRUE;
                    return FALSE;
                }
                case TVN_SELCHANGED: {
                    const NMTREEVIEWW* tv = reinterpret_cast<const NMTREEVIEWW*>(lp);
                    maptree_update_buttons();
                    if ((tv->action == TVC_BYMOUSE || tv->action == TVC_BYKEYBOARD) &&
                        !g.map_selection_queued) {
                        g.map_selection_queued = PostMessageW(h, WM_RECONCILE_MAP_SELECTION, 0, 0) != 0;
                    }
                    return 0;
                }
                case NM_DBLCLK: {
                    // Erst den Kartenwechsel abgleichen; nach Abbruch nicht
                    // versehentlich Eigenschaften der alten Karte öffnen.
                    const int selected = maptree_selected();
                    if (selected >= 0) maptree_queue_action(MapTreeActionKind::Properties, selected);
                    return 0;
                }
                case TVN_BEGINLABELEDIT: {
                    if (g.props_dlg) return TRUE;
                    // Nur Karten (nicht die Wurzel) sind umbenennbar.
                    const NMTVDISPINFOW* di =
                        reinterpret_cast<const NMTVDISPINFOW*>(lp);
                    TVITEMEXW it{};
                    it.hItem = di->item.hItem;
                    it.mask = TVIF_CHILDREN | TVIF_PARAM;
                    TreeView_GetItem(g.map_tree, &it);
                    if (it.cChildren || static_cast<int>(it.lParam) != g.cur_map) return TRUE;
                    return FALSE;
                }
                case TVN_ENDLABELEDIT: {
                    if (g.props_dlg) return FALSE;
                    const NMTVDISPINFOW* di =
                        reinterpret_cast<const NMTVDISPINFOW*>(lp);
                    if (di->item.pszText && di->item.pszText[0]) {
                        TVITEMEXW it{};
                        it.hItem = di->item.hItem;
                        it.mask = TVIF_PARAM;
                        TreeView_GetItem(g.map_tree, &it);
                        return maptree_apply_rename(static_cast<int>(it.lParam),
                                                    wide_to_utf8(di->item.pszText)) ? TRUE : FALSE;
                    }
                    return FALSE;      // leer: Edit verwerfen
                }
                default: break;
            }
            break;
        }

        // Ein Rechtsklick wählt den tatsächlich getroffenen Kartenknoten.
        // Den Wechsel wie bei Linksklick erst nach der TreeView-Nachricht und
        // mit denselben Rückfragen abgleichen; bei Abbruch kein Menü auf der
        // alten Karte öffnen. Shift+F10 behält die Tastaturauswahl.
        case WM_CONTEXTMENU: {
            if (reinterpret_cast<HWND>(wp) != g.map_tree) break;
            if (g.props_dlg) { SetForegroundWindow(g.props_dlg); return 0; }
            const POINT screen{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            if (!(screen.x == -1 && screen.y == -1)) {
                bool changed = false;
                main_tree_select_at(g.map_tree, screen, changed);
            }
            maptree_queue_action(MapTreeActionKind::Context, maptree_selected(), lp);
            return 0;
        }
        case WM_TREE_ACTION: {
            g.map_action_queued = false;
            // Eine neue Auswahl kann zwischen PostMessage und Zustellung
            // eingetroffen sein. Der Auswahlabgleich stellt die Aktion erneut zu.
            if (g.map_selection_queued || g.map_selection_resolving) return 0;
            MapTreeAction action = std::move(g.map_action);
            g.map_action = {};
            if (g.props_dlg || !maptree_action_matches(action)) return 0;
            if (action.kind == MapTreeActionKind::Properties) {
                if (action.index >= 0) open_map_props();
                return 0;
            }
            if (action.kind == MapTreeActionKind::New) {
                open_map_new();
                return 0;
            }
            if (action.kind != MapTreeActionKind::Context) return 0;
            const int idx = action.index;
            const std::uint64_t map_epoch = g.map_epoch;
            POINT pt{ GET_X_LPARAM(action.position), GET_Y_LPARAM(action.position) };
            if (pt.x == -1 && pt.y == -1) {
                RECT rect{};
                GetWindowRect(g.map_tree, &rect);
                pt.x = rect.left + 16;
                pt.y = rect.top + 16;
            }
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
            // TrackPopupMenu pumpt Nachrichten: Karte, Projekt und Auswahl
            // müssen auch nach dem Schließen noch dieselben sein.
            if (g.props_dlg || g.map_epoch != map_epoch || !maptree_action_matches(action)) return 0;
            switch (cmd) {
                case IDM_TREE_PROPS:  if (idx >= 0) open_map_props(); break;
                case IDM_TREE_RENAME:
                    if (idx >= 0) {
                        HTREEITEM sel = TreeView_GetSelection(g.map_tree);
                        if (sel) TreeView_EditLabel(g.map_tree, sel);
                    }
                    break;
                case IDM_TREE_NEW:    open_map_new(); break;
                case IDM_TREE_DELETE: if (idx >= 0) delete_current_map(); break;
                default: break;
            }
            return 0;
        }

        case WM_DESTROY:
            cancel_splitter_drag();
            PostQuitMessage(0);
            return 0;
        case WM_NCDESTROY:
            // Kinder und Toolbar sind jetzt zerstört und nutzen Schrift/Icons
            // nicht mehr. Die Dialogschrift g.font gehört weiterhin dem Editor.
            if (g.main_font) { DeleteObject(g.main_font); g.main_font = nullptr; }
            if (g_toolbar_images) { ImageList_Destroy(g_toolbar_images); g_toolbar_images = nullptr; }
            g_toolbar = nullptr;
            break;
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
    IDLG_ENC_STEPS   = 4116,   // Etappe 19m: mittlere Schrittzahl
    IDLG_ENC_TROOPS  = 4117,   // eine Truppen-ID pro Zeile
    IDLG_ENC_COMBO   = 4118,   // vorhandene Projekt-Truppen
    IDLG_ENC_ADD     = 4119,
    IDLG_ENC_RELOAD  = 4120,
    IDLG_ENC_STATUS  = 4121,
    IDLG_ENC_DB      = 4122,   // Datenbank auch bei offenem F2 öffnen
    IDLG_BLOCK_LABEL = 4123,
};

static HWND g_dlg = nullptr;

struct EncounterDlgState {
    EncounterForm before;
    MapPropsForm props_before;
    std::vector<EncounterTroopChoice> choices;
    std::string project;
    std::string map_file;
    int map_index = -1;
    bool syncing = false;
    bool troops_touched = false;
    bool steps_touched = false;
};
static EncounterDlgState g_enc_dlg;

void encounter_refresh_choices();
void encounter_add_choice();

HWND dlg_control(const wchar_t* cls, const wchar_t* text, DWORD style,
                 int x, int y, int w, int h, int id) {
    if (id != 0 && std::wcscmp(cls, L"Static") != 0) style |= WS_TABSTOP;
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

// Alle angezeigten allgemeinen Werte als Vergleichsstand aufnehmen. So
// schreibt ein reines Encounter-Edit weder gerundete Terrain-Werte noch
// unberührte Block-Zonen oder unbekannte Eigenschaften zurück.
MapPropsForm dlg_map_props_form() {
    MapPropsForm form;
    form.name = dlg_text_long(IDLG_NAME);
    form.width = dlg_text_long(IDLG_WIDTH);
    form.height = dlg_text_long(IDLG_HEIGHT);
    form.seed = dlg_text_long(IDLG_SEED);
    form.amplitude = dlg_text_long(IDLG_AMP);
    form.mountains = IsDlgButtonChecked(g_dlg, IDLG_BERGE) == BST_CHECKED;
    form.fog_on = IsDlgButtonChecked(g_dlg, IDLG_FOG) == BST_CHECKED;
    form.fog_far = dlg_text_long(IDLG_FOGFAR);
    form.fog_color = dlg_text_long(IDLG_FOGCOL);
    form.panorama_color = dlg_text_long(IDLG_PANCOL);
    form.weather_type = static_cast<int>(SendMessageW(GetDlgItem(g_dlg, IDLG_WTYPE),
                                                       CB_GETCURSEL, 0, 0));
    form.weather_intensity = dlg_text_long(IDLG_WINT);
    form.note = dlg_text_long(IDLG_NOTE);
    form.blocked = dlg_text_long(IDLG_BLOCKED);
    return form;
}

// Nur gespeicherte Projekt-Datenbank lesen; F9 kann als eigenes modusloses
// Fenster offen sein. Nicht gespeicherte F9-Änderungen gelten im Spiel nicht.
bool encounter_read_database(JsonValue& db, std::wstring& status) {
    std::string text;
    if (!read_file(g.project_dir + "\\database.json", text)) {
        status = L"Datenbank fehlt oder ist nicht lesbar.\r\nF9: anlegen und speichern.";
        return false;
    }
    try {
        db = json_parse(text);
    } catch (const std::exception&) {
        status = L"Datenbank: ungültiges JSON.\r\nDatei vor F9-Speichern prüfen.";
        return false;
    }
    const JsonValue* troops = db.is_obj() ? db.find("troops") : nullptr;
    if (!db.is_obj() || (troops && !troops->is_null() && !troops->is_arr())) {
        status = L"Ungültige Truppenliste.\r\nDatei vor F9-Speichern prüfen.";
        return false;
    }
    status.clear();
    return true;
}

void encounter_refresh_choices() {
    HWND combo = GetDlgItem(g_dlg, IDLG_ENC_COMBO);
    SendMessageW(combo, CB_RESETCONTENT, 0, 0);
    g_enc_dlg.choices.clear();
    JsonValue db;
    std::wstring status;
    if (encounter_read_database(db, status)) {
        g_enc_dlg.choices = encounter_database_choices(db);
        status = g_enc_dlg.choices.empty()
            ? L"Keine Truppen: in F9 anlegen,\r\nspeichern und hier neu laden."
            : L"Nur gespeicherte F9-Truppen\r\nwerden hier angezeigt.";
        for (size_t i = 0; i < g_enc_dlg.choices.size(); ++i) {
            const EncounterTroopChoice& choice = g_enc_dlg.choices[i];
            std::wstring label = utf8_to_wide(choice.id);
            if (encounter_id_editable(choice.name) && choice.name != choice.id) {
                label += L" — " + utf8_to_wide(choice.name);
            }
            const LRESULT item = SendMessageW(combo, CB_ADDSTRING, 0,
                                              reinterpret_cast<LPARAM>(label.c_str()));
            if (item != CB_ERR && item != CB_ERRSPACE) {
                SendMessageW(combo, CB_SETITEMDATA, static_cast<WPARAM>(item),
                             static_cast<LPARAM>(i));
            }
        }
    }
    const LRESULT count = SendMessageW(combo, CB_GETCOUNT, 0, 0);
    if (count > 0) SendMessageW(combo, CB_SETCURSEL, 0, 0);
    EnableWindow(GetDlgItem(g_dlg, IDLG_ENC_ADD), count > 0 ? TRUE : FALSE);
    SetDlgItemTextW(g_dlg, IDLG_ENC_STATUS, status.c_str());
}

void encounter_add_choice() {
    HWND combo = GetDlgItem(g_dlg, IDLG_ENC_COMBO);
    const LRESULT selected = SendMessageW(combo, CB_GETCURSEL, 0, 0);
    if (selected == CB_ERR) return;
    const LRESULT index = SendMessageW(combo, CB_GETITEMDATA,
                                       static_cast<WPARAM>(selected), 0);
    if (index < 0 || static_cast<size_t>(index) >= g_enc_dlg.choices.size()) return;
    std::string lines = dlg_text_long(IDLG_ENC_TROOPS);
    if (!lines.empty() && lines.back() != '\r' && lines.back() != '\n') lines += "\r\n";
    else if (!lines.empty() && lines.back() == '\r') lines += '\n';
    lines += g_enc_dlg.choices[static_cast<size_t>(index)].id;
    dlg_set_text(IDLG_ENC_TROOPS, lines);
    g_enc_dlg.troops_touched = true;
    HWND edit = GetDlgItem(g_dlg, IDLG_ENC_TROOPS);
    const int len = GetWindowTextLengthW(edit);
    SendMessageW(edit, EM_SETSEL, len, len);
    SendMessageW(edit, EM_SCROLLCARET, 0, 0);
    SetFocus(edit);
}

LRESULT CALLBACK dlg_proc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    if (g.data_prompt_active) {
        // Ein Edit-Control kann eine bereits zugestellte Textänderung noch
        // melden. Die Berührungsmarke behalten, aber keine Aktion ausführen.
        if (msg == WM_COMMAND && g.dlg_mode == 1 && !g_enc_dlg.syncing && HIWORD(wp) == EN_CHANGE) {
            if (LOWORD(wp) == IDLG_ENC_TROOPS) g_enc_dlg.troops_touched = true;
            if (LOWORD(wp) == IDLG_ENC_STEPS) g_enc_dlg.steps_touched = true;
        }
        if (msg == WM_COMMAND || msg == WM_CLOSE) return 0;
    }
    switch (msg) {
        // Das Fenster nutzt eine eigene Klasse statt eines Dialog-Templates;
        // IsDialogMessage muss trotzdem den Standard-Button ermitteln können.
        case DM_GETDEFID:
            return MAKELRESULT(IDLG_OK, DC_HASDEFID);
        case WM_COMMAND: {
            const int id = LOWORD(wp);
            const int code = HIWORD(wp);
            if (g.dlg_mode == 1 && !g_enc_dlg.syncing) {
                if (code == EN_CHANGE && id == IDLG_ENC_TROOPS) g_enc_dlg.troops_touched = true;
                if (code == EN_CHANGE && id == IDLG_ENC_STEPS) g_enc_dlg.steps_touched = true;
                if (code == BN_CLICKED && id == IDLG_ENC_ADD) { encounter_add_choice(); return 0; }
                if (code == BN_CLICKED && id == IDLG_ENC_RELOAD) { encounter_refresh_choices(); return 0; }
                if (code == BN_CLICKED && id == IDLG_ENC_DB) { open_database_dialog(); return 0; }
            }
            if (id == IDLG_OK || id == IDOK) {
                if (g.dlg_mode == 1 && !apply_props_from_dlg()) return 0;
                if (g.dlg_mode == 2 && !apply_new_from_dlg()) return 0;
                DestroyWindow(h);
                return 0;
            }
            if (id == IDLG_CANCEL || id == IDCANCEL) {
                DestroyWindow(h);
                return 0;
            }
            break;
        }
        case WM_CLOSE:
            DestroyWindow(h);
            return 0;
        case WM_DESTROY:
            if (g.props_dlg == h) {
                g.props_dlg = nullptr;
                g.dlg_mode = 0;
                if (g.map_tree) EnableWindow(g.map_tree, TRUE);
                maptree_update_buttons();
            }
            if (g_dlg == h) g_dlg = nullptr;
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
    if (erase && !g.map_root.find("terrain")) return false;
    if (!erase) {
        TerrainPreview candidate = terrain_preview_read(g.map_root);
        if (!candidate.ok()) return false;
        candidate.splats.push_back(world::SplatRect{cx - sw * 0.5f, cz - sh * 0.5f, sw, sh, tex});
        if (world::terrain_check_splats(candidate.plan, candidate.splats) != world::TerrainProblem::None) {
            MessageBoxW(g.main, L"Diese Fläche würde das Terrain-Budget überschreiten.",
                        L"Terrain", MB_OK | MB_ICONWARNING); return false;
        }
    }
    JsonValue* ter = writable_terrain();
    if (!ter || !preview_unique(*ter, "splats")) return false;
    JsonValue* arr = ter->find("splats");
    if (erase && (!arr || !arr->is_arr())) return false;
    if (arr && !arr->is_arr()) {
        MessageBoxW(g.main, L"terrain.splats ist keine Liste; die Altdaten bleiben erhalten.",
                    L"Terrain", MB_OK | MB_ICONWARNING);
        return false;
    }
    if (!arr) arr = &ter->set("splats", JsonValue::make_arr());

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
    // Ein Radierer darf bei extrem langen Listen keine unbegrenzte Suche
    // auslösen; einzelne sichtbare Einträge können stattdessen in F7 weg.
    if (arr->arr.size() > world::TERRAIN_MAX_SPLATS) {
        MessageBoxW(g.main, L"Zu viele Bodenflächen für den Radierer. Bitte die Liste in F7 prüfen.",
                    L"Terrain", MB_OK | MB_ICONWARNING);
        return false;
    }
    // Letzte (späteste) gültige Fläche unter dem Punkt entfernen.
    for (std::size_t i = arr->arr.size(); i-- > 0;) {
        const JsonValue& e = arr->arr[i];
        if (!e.is_obj()) continue;
        world::SplatRect s;
        if (!preview_splat(e, s)) continue;
        if (cx >= s.x && cx < s.x + s.w && cz >= s.z && cz < s.z + s.h) {
            arr->arr.erase(arr->arr.begin() + static_cast<std::ptrdiff_t>(i));
            return true; // eine vorhandene leere Liste bleibt erhalten
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
    TerrainPreview candidate = terrain_preview_read(g.map_root);
    if (!candidate.ok()) return 0;
    auto within_budget = [&candidate]() {
        if (world::terrain_check_brushes(candidate.plan, candidate.brushes) == world::TerrainProblem::None)
            return true;
        MessageBoxW(g.main, L"Dieser Höhenpinsel würde den sicheren Bereich oder das Rechenbudget überschreiten.",
                    L"Terrain", MB_OK | MB_ICONWARNING);
        return false;
    };

    // Vor dem erstmaligen Anlegen von terrain/heights prüfen, damit ein
    // abgelehnter Pinsel auch keine leeren Felder in der Karte hinterlässt.
    const JsonValue* old_terrain = g.map_root.find("terrain");
    const JsonValue* old_heights = old_terrain && old_terrain->is_obj() ? old_terrain->find("heights") : nullptr;
    if (!old_heights) {
        candidate.brushes.push_back(math::Vec4{cx, hgt, cz, r});
        if (!within_budget()) return 0;
    }
    JsonValue* ter = writable_terrain();
    if (!ter || !preview_unique(*ter, "heights")) return 0;
    JsonValue* arr = ter->find("heights");
    if (arr && !arr->is_arr()) {
        MessageBoxW(g.main, L"terrain.heights ist keine Liste; die Altdaten bleiben erhalten.",
                    L"Terrain", MB_OK | MB_ICONWARNING);
        return 0;
    }
    if (!arr) arr = &ter->set("heights", JsonValue::make_arr());

    // Aufeinanderfolgende Klicks an derselben Stelle (gleiche Kachel) zum
    // selben Pinsel addieren, damit die JSON-Liste beim "Höhen-Stapeln"
    // nicht endlos wächst.
    const double cell = static_cast<double>(g.grid);
    for (size_t i = 0; i < arr->arr.size(); ++i) {
        JsonValue& e = arr->arr[i];
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
            const double next = (eh ? eh->number(0) : 0.0) + static_cast<double>(hgt);
            if (!world::terrain_valid_brush(cx, cz, r, next)) return 0;
            candidate.brushes[i].y = static_cast<float>(next);
            if (!within_budget()) return 0;
            e.set("h", JsonValue::make_num(next));
            return static_cast<int>(arr->arr.size());
        }
    }

    // Bei einem bereits vorhandenen Array ist der neue Pinsel noch nicht in
    // candidate enthalten; das neu angelegte Array wurde oben schon geprüft.
    if (candidate.brushes.size() == arr->arr.size()) {
        candidate.brushes.push_back(math::Vec4{cx, hgt, cz, r});
        if (!within_budget()) return 0;
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
    if (!g.terrain || !g.renderer) return;
    g.terrain_error.clear();
    g.terrain_warning.clear();
    g.terrain_memory_error = false;
    g.terrain_field_warning = false;
    auto fail = [](const std::string& reason) {
        g.terrain->clear();
        g.renderer->clear_terrain();
        g.terrain_error = reason;
        core::log_warn("Terrain: " + reason + "; keine Vorschau, Kartendaten unverändert");
        status_mode(L"Keine Gelände-Vorschau — F10 zeigt die Ursache");
    };
    try {
        const TerrainPreview preview = terrain_preview_read(g.map_root);
        if (!preview.ok()) { fail(preview.error); return; }
        g.terrain_field_warning = !preview.warnings.empty();
        for (const std::string& warning : preview.warnings) core::log_warn("Terrain: " + warning);
        if (!g.terrain->set_height_brushes(preview.brushes)) {
            fail("Höhenpinsel konnten nicht übernommen werden"); return;
        }
        const world::TerrainPlan& p = preview.plan;
        if (!preview.splats.empty()) {
            const world::AtlasInfo atlas = g.renderer->update_atlas();
            if (world::atlas_has_texture(atlas, preview.texture)) {
                if (std::any_of(preview.splats.begin(), preview.splats.end(),
                    [&atlas](const world::SplatRect& s) { return !world::atlas_has_texture(atlas, s.tex); })) {
                    g.terrain_warning = "Bodenflächen-Textur fehlt im Atlas; betroffene Flächen zeigen die Basis";
                    core::log_warn("Splat-Boden: " + g.terrain_warning);
                }
                if (!g.terrain->generate_splat(p.width, p.height, p.res_x, p.seed, p.amplitude,
                                               preview.texture, preview.splats, atlas) ||
                    !g.renderer->set_terrain(g.terrain->vertices(), g.terrain->indices())) {
                    fail("Splat-Gelände konnte nicht aufgebaut werden"); return;
                }
                g.renderer->set_terrain_tex(atlas.tex_index);
                g.renderer->set_terrain_tile(world::ATLAS_NEUTRAL_TILE);
                core::log_info("Splat-Boden aktiv: " + std::to_string(preview.splats.size()) +
                               " Fläche(n), Atlas-Slot " + std::to_string(atlas.tex_index));
                return;
            }
            g.terrain_warning = atlas.valid
                ? "Basistextur fehlt im Atlas; Bodenflächen fehlen in der Einzeltextur-Vorschau"
                : "Kein Atlas möglich; Bodenflächen fehlen in der Einzeltextur-Vorschau";
            core::log_warn("Splat-Boden: " + g.terrain_warning);
        }
        if (!g.terrain->generate(p.width, p.height, p.res_x, p.seed, p.amplitude) ||
            !g.renderer->set_terrain(g.terrain->vertices(), g.terrain->indices())) {
            fail("Gelände oder GPU-Puffer konnte nicht aufgebaut werden"); return;
        }
        const int texture = g.renderer->texture_index(preview.texture);
        if (texture < 0) {
            if (!g.terrain_warning.empty()) g.terrain_warning += "; ";
            g.terrain_warning += "Basistextur nicht geladen; Gelände ohne Textur";
        }
        g.renderer->set_terrain_tex(texture);
        g.renderer->set_terrain_tile(preview.tile);
    } catch (const std::bad_alloc&) {
        g.terrain->clear();
        g.renderer->clear_terrain();
        g.terrain_memory_error = true;
        try { core::log_warn("Nicht genügend Speicher für die Gelände-Vorschau"); } catch (...) {}
    } catch (const std::length_error&) {
        g.terrain->clear();
        g.renderer->clear_terrain();
        g.terrain_memory_error = true;
        try { core::log_warn("Gelände-Vorschau überschreitet den Speicherbereich"); } catch (...) {}
    }
}

// Himmel/Nebel im Editor-Viewport aus der Karte übernehmen. Ersatzwerte
// betreffen nur den Renderer; die JSON-Daten bleiben unverändert.
void apply_map_atmosphere_editor() {
    const AtmospherePreview preview = atmosphere_preview_read(g.map_root);
    g.atmosphere_warning = !preview.warnings.empty();
    g.renderer->set_sky(preview.sky, preview.sky);
    g.renderer->set_fog(preview.fog_on, preview.fog_color, preview.fog_far);
    for (const std::string& warning : preview.warnings) core::log_warn("Atmosphäre: " + warning);
}

void open_map_props() {
    if (g.cur_map < 0 || g.cur_map >= static_cast<int>(g.map_files.size())) return;
    if (g.props_dlg) { SetForegroundWindow(g.props_dlg); return; }   // schon offen
    if (event_editor_open()) {
        MessageBoxW(g.main, L"Bitte zuerst den Event-Editor schließen.\n"
                            L"Karten-Eigenschaften können sonst dessen Datenzeiger ungültig machen.",
                    L"Karten-Eigenschaften", MB_OK | MB_ICONINFORMATION);
        return;
    }
    cancel_object_drag();

    g_enc_dlg = EncounterDlgState{};
    g_enc_dlg.syncing = true;
    g_enc_dlg.before = encounter_form_read(g.map_root.find("encounters"));
    g_enc_dlg.project = g.project_dir;
    g_enc_dlg.map_index = g.cur_map;
    g_enc_dlg.map_file = g.map_files[g.cur_map];

    WNDCLASSW wc{};
    wc.lpfnWndProc = dlg_proc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"RuneEditorDlg";
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
    RegisterClassW(&wc);

    // Zwei Spalten statt eines noch höheren Dialogs. Aus der benötigten
    // Clientfläche die Fenstergröße mit DPI-abhängigem Rahmen berechnen,
    // damit auch bei hohen DPI die unteren Knöpfe nicht abgeschnitten sind.
    const DWORD window_style = WS_POPUP | WS_CAPTION | WS_SYSMENU;
    const DWORD window_ex_style = WS_EX_DLGMODALFRAME;
    RECT frame{0, 0, 770, 504};
    const UINT dpi = GetDpiForWindow(g.main);
    if (!AdjustWindowRectExForDpi(&frame, window_style, FALSE, window_ex_style, dpi)) {
        AdjustWindowRectEx(&frame, window_style, FALSE, window_ex_style);
    }
    const int W = static_cast<int>(frame.right - frame.left);
    const int H = static_cast<int>(frame.bottom - frame.top);
    // Auf dem aktuellen Bildschirm zentrieren und dessen Arbeitsfläche nutzen.
    RECT mr{};
    GetWindowRect(g.main, &mr);
    int x = static_cast<int>((mr.left + mr.right - W) / 2);
    int y = static_cast<int>((mr.top + mr.bottom - H) / 2);
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    if (GetMonitorInfoW(MonitorFromWindow(g.main, MONITOR_DEFAULTTONEAREST), &mi)) {
        const int left = static_cast<int>(mi.rcWork.left);
        const int top = static_cast<int>(mi.rcWork.top);
        const int right = std::max(left, static_cast<int>(mi.rcWork.right) - W);
        const int bottom = std::max(top, static_cast<int>(mi.rcWork.bottom) - H);
        x = std::clamp(x, left, right);
        y = std::clamp(y, top, bottom);
    }
    g_dlg = CreateWindowExW(window_ex_style, L"RuneEditorDlg",
                            L"Karten-Eigenschaften", window_style,
                            x, y, W, H,
                            g.main, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!g_dlg) return;

    // Der Rahmen muss vor den darin liegenden Steuerelementen entstehen.
    dlg_control(L"Button", L"Zufallskämpfe", BS_GROUPBOX, 446, 12, 314, 440, 0);

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
    HWND note_edit = dlg_control(L"Edit", nullptr,
                WS_BORDER | WS_VSCROLL | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN,
                16, 288, 414, 50, IDLG_NOTE);
    SendMessageW(note_edit, EM_SETLIMITTEXT, 0, 0);

    // #Nummern bleiben beim Editieren und Umordnen an der jeweiligen Zone;
    // neue Zeilen ohne Nummer anlegen. Zusatzdaten bleiben so zugeordnet.
    dlg_control(L"Static", L"Block-Zonen (# behalten; neu: x,z,w,h)", 0,
                16, 346, 414, 20, IDLG_BLOCK_LABEL);
    HWND blocked_edit = dlg_control(L"Edit", nullptr,
                WS_BORDER | WS_VSCROLL | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN,
                16, 366, 414, 85, IDLG_BLOCKED);
    SendMessageW(blocked_edit, EM_SETLIMITTEXT, 0, 0);

    // Etappe 19m: IDs dürfen auch direkt eingegeben, mehrfach verwendet oder
    // durch Löschen aller Zeilen deaktiviert werden. F9-Auswahl ist optional.
    dlg_label(L"Schritte (Mittelwert)", 460, 38, 150);
    dlg_control(L"Edit", nullptr, WS_BORDER | ES_LEFT, 614, 34, 128, 24, IDLG_ENC_STEPS);
    dlg_label(L"Leeres Feld = 30", 460, 64, 260);
    dlg_label(L"Projekt-Truppen (F9)", 460, 92, 260);
    dlg_control(L"ComboBox", nullptr, CBS_DROPDOWNLIST | WS_VSCROLL,
                460, 112, 282, 180, IDLG_ENC_COMBO);
    dlg_control(L"Button", L"Hinzufügen", BS_PUSHBUTTON, 460, 145, 102, 27, IDLG_ENC_ADD);
    dlg_control(L"Button", L"Neu laden", BS_PUSHBUTTON, 567, 145, 91, 27, IDLG_ENC_RELOAD);
    dlg_control(L"Button", L"F9 öffnen", BS_PUSHBUTTON, 663, 145, 79, 27, IDLG_ENC_DB);
    dlg_control(L"Static", L"", 0, 460, 180, 282, 40, IDLG_ENC_STATUS);
    dlg_label(L"Kartenliste (eine ID pro Zeile)", 460, 226, 282);
    HWND troops_edit = dlg_control(L"Edit", nullptr,
                WS_BORDER | WS_VSCROLL | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN,
                460, 248, 282, 106, IDLG_ENC_TROOPS);
    SendMessageW(troops_edit, EM_SETLIMITTEXT, 0, 0);
    dlg_control(L"Static", L"Leere Liste = keine Zufallskämpfe.\r\n"
                           L"Mehrfach: Truppe wird häufiger gewählt.",
                0, 460, 362, 282, 44, 0);
    const bool bad_encounter = g_enc_dlg.before.bad_object || g_enc_dlg.before.bad_troops ||
                               g_enc_dlg.before.bad_steps;
    dlg_control(L"Static", bad_encounter
        ? L"Altdaten ungültig/nicht darstellbar;\r\nÄnderung nur nach Rückfrage."
        : L"Nach OK mit Strg+S speichern.",
        0, 460, 409, 282, 38, 0);

    dlg_control(L"Button", L"OK", BS_DEFPUSHBUTTON, 562, 464, 90, 30, IDLG_OK);
    dlg_control(L"Button", L"Abbrechen", BS_PUSHBUTTON, 662, 464, 90, 30, IDLG_CANCEL);

    // Originalzahlen ohne int-/uint-Casts anzeigen. Nicht editierte Werte
    // (auch große Integer) bleiben beim OK bytegetreu im JSON-Baum erhalten.
    const JsonValue* t = g.map_root.find("terrain");
    if (t && !t->is_obj()) t = nullptr;
    const JsonValue* size = t ? t->find("size") : nullptr;
    const JsonValue* width = t ? t->find("width") : nullptr;
    const JsonValue* height = t ? t->find("height") : nullptr;
    if (!width || width->is_null()) width = size;
    if (!height || height->is_null()) height = size;
    dlg_set_text(IDLG_NAME, g.map_root.find("name") ? g.map_root.find("name")->str : "");
    dlg_set_text(IDLG_WIDTH, map_props_number_display(width, 240));
    dlg_set_text(IDLG_HEIGHT, map_props_number_display(height, 240));
    const JsonValue* amplitude = t ? t->find("amplitude") : nullptr;
    const double amp = amplitude ? amplitude->number(0.0) : 0.0;
    CheckDlgButton(g_dlg, IDLG_BERGE, amp != 0.0 ? BST_CHECKED : BST_UNCHECKED);
    dlg_set_text(IDLG_SEED, map_props_number_display(t ? t->find("seed") : nullptr, 7));
    dlg_set_text(IDLG_AMP, map_props_number_display(amplitude, 0.0));

    const JsonValue* fog = g.map_root.find("fog");
    if (fog && !fog->is_obj()) fog = nullptr;
    const JsonValue* fon = fog ? fog->find("on") : nullptr;
    CheckDlgButton(g_dlg, IDLG_FOG, (fon && fon->type == JsonValue::Type::Bool && fon->b)
                                     ? BST_CHECKED : BST_UNCHECKED);
    dlg_set_text(IDLG_FOGFAR, map_props_number_display(fog ? fog->find("far") : nullptr, 180.0));
    dlg_set_text(IDLG_FOGCOL, map_props_color_display(fog ? fog->find("color") : nullptr, 0xBFD8EC));
    const JsonValue* pan = g.map_root.find("panorama");
    if (pan && !pan->is_obj()) pan = nullptr;
    dlg_set_text(IDLG_PANCOL, map_props_color_display(pan ? pan->find("color") : nullptr, 0x3D6FB5));

    // Etappe 17h: Wetter-Typ + Intensität aus dem Karten-JSON.
    {
        const JsonValue* w = g.map_root.find("weather");
        const std::string wt = w ? (w->find("type") ? w->find("type")->str : "kein")
                                : std::string("kein");
        int sel = 0;
        if (wt == "regen")  sel = 1;
        if (wt == "schnee") sel = 2;
        SendMessageW(GetDlgItem(g_dlg, IDLG_WTYPE), CB_SETCURSEL, sel, 0);
        dlg_set_text(IDLG_WINT, map_props_number_display(w && w->is_obj() ? w->find("intensity") : nullptr, 1.0));
    }

    const JsonValue* note = g.map_root.find("note");
    dlg_set_text(IDLG_NOTE, note ? note->str : "");

    // Unlesbare/lange Altwerte niemals durch das Windows-Editlimit
    // abschneiden. Die Textform enthält für jede alte Zone eine Marke.
    {
        const std::string zeilen = blocked_form_read(g.map_root.find("blocked"));
        dlg_set_text(IDLG_BLOCKED, zeilen);
        if (dlg_text_long(IDLG_BLOCKED) != zeilen) {
            EnableWindow(GetDlgItem(g_dlg, IDLG_BLOCKED), FALSE);
            SetDlgItemTextW(g_dlg, IDLG_BLOCK_LABEL,
                            L"Block-Zonen zu lang/nicht darstellbar — unverändert geschützt");
        }
    }

    dlg_set_text(IDLG_ENC_STEPS, g_enc_dlg.before.steps);
    dlg_set_text(IDLG_ENC_TROOPS, g_enc_dlg.before.troops);
    encounter_refresh_choices();
    g_enc_dlg.props_before = dlg_map_props_form();
    g_enc_dlg.syncing = false;
    g.props_dlg = g_dlg;      // moduslos: Hauptschleife rendert weiter
    g.dlg_mode = 1;
    if (g.map_tree) EnableWindow(g.map_tree, FALSE);
    maptree_update_buttons();
    ShowWindow(g_dlg, SW_SHOW);
    SetFocus(GetDlgItem(g_dlg, IDLG_NAME));
}

// Werte aus dem offenen Eigenschaften-Dialog in die Karte übernehmen (OK).
// Bei Fehler oder abgelehnter Rückfrage bleibt der Dialog geöffnet und die
// gesamte Karte unverändert.
bool apply_props_from_dlg() {
    HWND dlg = g.props_dlg;
    if (!dlg) return false;
    if (g_enc_dlg.project != g.project_dir || g_enc_dlg.map_index != g.cur_map ||
        g.cur_map < 0 || g.cur_map >= static_cast<int>(g.map_files.size()) ||
        g_enc_dlg.map_file != g.map_files[g.cur_map]) {
        MessageBoxW(dlg, L"Die Karte oder das Projekt hat sich geändert. "
                         L"Bitte den Dialog abbrechen und erneut öffnen.",
                    L"Karten-Eigenschaften", MB_OK | MB_ICONWARNING);
        return false;
    }

    // Erst ALLE neuen Felder prüfen und Rückfragen stellen, bevor irgendein
    // Kartenwert geändert wird. Abbrechen lässt auch Name/Terrain unangetastet.
    const std::uint64_t project_epoch = g.project_epoch, map_epoch = g.map_epoch;
    const std::string troops_text = dlg_text_long(IDLG_ENC_TROOPS);
    const std::string steps_text = dlg_text_long(IDLG_ENC_STEPS);
    const bool troops_touched = g_enc_dlg.troops_touched, steps_touched = g_enc_dlg.steps_touched;
    EncounterEdit enc = encounter_form_prepare(g.map_root.find("encounters"), g_enc_dlg.before,
        troops_text, steps_text, troops_touched, steps_touched);
    if (!enc.ok) {
        MessageBoxW(dlg, utf8_to_wide(enc.error).c_str(), L"Zufallskämpfe",
                    MB_OK | MB_ICONWARNING);
        SetFocus(GetDlgItem(dlg, enc.troops_error ? IDLG_ENC_TROOPS : IDLG_ENC_STEPS));
        return false;
    }
    const MapPropsForm props = dlg_map_props_form();
    const MapPropsCheck general_check = map_props_check(g.map_root, g_enc_dlg.props_before, props);
    if (!general_check.ok) {
        MessageBoxW(dlg, utf8_to_wide(general_check.error).c_str(), L"Karten-Eigenschaften",
                    MB_OK | MB_ICONWARNING);
        int focus = IDLG_NAME;
        if (general_check.field == "width") focus = IDLG_WIDTH;
        else if (general_check.field == "height") focus = IDLG_HEIGHT;
        else if (general_check.field == "seed") focus = IDLG_SEED;
        else if (general_check.field == "amplitude") focus = IDLG_AMP;
        else if (general_check.field == "fog_on") focus = IDLG_FOG;
        else if (general_check.field == "fog_far") focus = IDLG_FOGFAR;
        else if (general_check.field == "fog_color") focus = IDLG_FOGCOL;
        else if (general_check.field == "panorama_color") focus = IDLG_PANCOL;
        else if (general_check.field == "weather_type") focus = IDLG_WTYPE;
        else if (general_check.field == "weather_intensity") focus = IDLG_WINT;
        else if (general_check.field == "note") focus = IDLG_NOTE;
        SetFocus(GetDlgItem(dlg, focus));
        return false;
    }
    const BlockedEdit blocked = blocked_form_prepare(g.map_root.find("blocked"),
                                                     g_enc_dlg.props_before.blocked, props.blocked);
    if (!blocked.ok) {
        MessageBoxW(dlg, utf8_to_wide(blocked.error).c_str(), L"Block-Zonen",
                    MB_OK | MB_ICONWARNING);
        SetFocus(GetDlgItem(dlg, IDLG_BLOCKED));
        return false;
    }
    if (blocked.changed && (blocked.removed || blocked.replaces_invalid_entries ||
                            blocked.replaces_invalid_list)) {
        std::wstring text = L"Beim Übernehmen gehen vorhandene Block-Zonen oder ungültige Altdaten verloren:";
        if (blocked.removed) text += L"\r\n• Entfernte Einträge: " + std::to_wstring(blocked.removed);
        if (blocked.replaces_invalid_entries)
            text += L"\r\n• Ersetzte ungültige Einträge: " + std::to_wstring(blocked.replaces_invalid_entries);
        if (blocked.replaces_invalid_list) text += L"\r\n• Ungültige alte Liste wird ersetzt";
        text += L"\r\n\r\nZusatzdaten entfernter/ersetzter Einträge gehen verloren. Trotzdem übernehmen?";
        if (data_prompt(dlg, text.c_str(), L"Block-Zonen",
                        MB_YESNO | MB_DEFBUTTON2 | MB_ICONWARNING) != IDYES) return false;
    }
    if (enc.replaces_invalid && data_prompt(dlg,
        L"Die vorhandenen Begegnungsdaten sind ungültig oder nicht vollständig "
        L"darstellbar. Das bearbeitete Feld bzw. Begegnungsobjekt wird ersetzt.\r\n\r\n"
        L"Trotzdem übernehmen?", L"Zufallskämpfe",
        MB_YESNO | MB_DEFBUTTON2 | MB_ICONWARNING) != IDYES) return false;

    const JsonValue* ids = enc.troops_changed ? enc.value.find("troops") : nullptr;
    if (ids && ids->is_arr() && !ids->arr.empty()) {
        // Während F2 offen war, kann die F9-Datenbank geändert worden sein.
        // Vor der Warnung stets noch einmal den gespeicherten Stand lesen.
        JsonValue db;
        std::wstring status;
        if (!encounter_read_database(db, status)) {
            const std::wstring text = status +
                L"\r\n\r\nDie Truppen können nicht geprüft werden. Trotzdem übernehmen?";
            if (data_prompt(dlg, text.c_str(), L"Zufallskämpfe",
                            MB_YESNO | MB_DEFBUTTON2 | MB_ICONWARNING) != IDYES) return false;
        } else {
            const std::vector<std::string> unknown = encounter_unknown_troops(*ids, db);
            if (!unknown.empty()) {
                std::wstring text = L"Diese Truppen fehlen in der gespeicherten Datenbank:\r\n";
                const size_t limit = std::min<size_t>(unknown.size(), 5);
                for (size_t i = 0; i < limit; ++i) {
                    std::wstring id = utf8_to_wide(unknown[i]);
                    if (id.size() > 80) id = id.substr(0, 80) + L"…";
                    text += L"\r\n• " + id;
                }
                if (unknown.size() > limit) text += L"\r\n… und weitere.";
                text += L"\r\n\r\nIm Spiel kann dafür möglicherweise kein Kampf starten. "
                        L"Trotzdem übernehmen?";
                if (data_prompt(dlg, text.c_str(), L"Zufallskämpfe",
                                MB_YESNO | MB_DEFBUTTON2 | MB_ICONWARNING) != IDYES) return false;
            }
        }
    }

    // MessageBox-Schleifen können bereits zugestellte Nachrichten verarbeiten.
    // Ein inzwischen geänderter Entwurf darf nicht durch den alten Snapshot
    // überschrieben und beim Schließen des Dialogs verloren werden.
    if (!IsWindow(dlg) || g.props_dlg != dlg || g.project_epoch != project_epoch || g.map_epoch != map_epoch)
        return false;
    if (!(dlg_map_props_form() == props) || dlg_text_long(IDLG_ENC_TROOPS) != troops_text ||
        dlg_text_long(IDLG_ENC_STEPS) != steps_text || g_enc_dlg.troops_touched != troops_touched ||
        g_enc_dlg.steps_touched != steps_touched) {
        data_prompt(dlg, L"Das Formular wurde während der Rückfrage geändert. "
                         L"Bitte die Eingaben erneut prüfen und bestätigen.",
                    L"Karten-Eigenschaften", MB_OK | MB_ICONWARNING);
        return false;
    }

    {
        JsonValue& root = g.map_root;
        const MapPropsChanges general = map_props_apply(root, g_enc_dlg.props_before, props, blocked);

        // Fehlende/null/unbearbeitete Encounter-Felder bleiben unverändert;
        // beim Ändern behält enc auch unbekannte Eigenschaften und Duplikate.
        if (enc.changed) root.set("encounters", std::move(enc.value));
        if (!general.changed && !enc.changed) return true;

        // set()/erase() können die Wurzel-Vector-Einträge verschieben. Der
        // Objektzeiger muss vor dem nächsten Rendern neu geholt werden.
        g.objects = root.find("objects");
        if (!g.objects || !g.objects->is_arr()) {
            g.objects = &root.set("objects", JsonValue::make_arr());
        }
        g.dirty = true;
        if (general.terrain) regen_terrain();
        if (general.atmosphere) apply_map_atmosphere_editor();
        issues_scan_map();
        if (general.name && g.cur_map < static_cast<int>(g.map_names.size())) {
            g.map_names[g.cur_map] = props.name;
            rebuild_map_menu();
        }
        update_title();
        core::log_info("Karten-Eigenschaften geändert");
    }
    return true;
}

// Neue Karte: kleiner Dialog (Name, Breite, Höhe, Berge) -> JSON-Datei.
void open_map_new() {
    if (g.props_dlg) { SetForegroundWindow(g.props_dlg); return; }   // schon offen
    cancel_object_drag();
    if (!resolve_object_edits()) return;

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
    if (!g_dlg) return;

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

    g.props_dlg = g_dlg;      // moduslos: Hauptschleife rendert weiter
    g.dlg_mode = 2;
    if (g.map_tree) EnableWindow(g.map_tree, FALSE);
    maptree_update_buttons();
    ShowWindow(g_dlg, SW_SHOW);
    SetFocus(GetDlgItem(g_dlg, IDLG_NAME));
}

// Neue Karte aus dem Dialog anlegen (OK).
bool apply_new_from_dlg() {
    if (g.cur_map >= 0 && !prepare_map_departure()) return false;
    const std::string previous = g.cur_map >= 0 && g.cur_map < static_cast<int>(g.map_files.size())
                                     ? g.map_files[static_cast<size_t>(g.cur_map)] : std::string();
    // nächste freie ID: welt01, welt02, ...
    int n = 1;
    std::string id;
    char buf[32];
    do {
        std::snprintf(buf, sizeof(buf), "welt%02d", n++);
        id = buf;
    } while (std::find(g.map_files.begin(), g.map_files.end(), id + ".json") != g.map_files.end() ||
             file_path_exists(g.project_dir + "\\maps\\" + id + ".json"));

    double width, height;
    if (!map_props_number(dlg_text(IDLG_WIDTH), width) ||
        !map_props_number(dlg_text(IDLG_HEIGHT), height) ||
        width < 60.0 || height < 60.0 ||
        !world::terrain_plan(width, height, 192, 7, 0).ok()) {
        MessageBoxW(g_dlg, L"Breite/Höhe müssen mindestens 60 und höchstens 10000 betragen; "
                            L"auch das Seitenverhältnis muss zum Terrain-Raster passen.",
                    L"Neue Karte", MB_OK | MB_ICONWARNING);
        return false;
    }
    const bool berge = IsDlgButtonChecked(g_dlg, IDLG_BERGE) == BST_CHECKED;

    JsonValue map = JsonValue::make_obj();
    map.set("name", JsonValue::make_str(dlg_text(IDLG_NAME).empty() ? "Neue Welt" : dlg_text(IDLG_NAME)));
    JsonValue ter = JsonValue::make_obj();
    ter.set("width", JsonValue::make_num(width, false));
    ter.set("height", JsonValue::make_num(height, false));
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

    if (!editor_json_utf8(map) || !editor_json_serializable(map)) {
        data_prompt(g_dlg, L"Die neue Karte enthält ungültiges Unicode oder eine nicht darstellbare Zahl.",
                    L"Neue Karte", MB_OK | MB_ICONERROR);
        return false;
    }
    const std::string directory = g.project_dir + "\\maps";
    const std::string path = directory + "\\" + id + ".json";
    DocumentStore store;
    DocumentSnapshot missing; // Keine bestehende Karte darf ersetzt werden.
    DocumentRecovery recovery;
    assets::Result result = store.open(g.root, g.project_name, true, id + ".json");
    if (result == assets::Result::Ok) result = store.save(missing, json_write(map), recovery);
    if (result != assets::Result::Ok) {
        core::log_error("Neue Karte nicht sicher schreibbar oder bereits vorhanden: " + path);
        document_result_notice(g_dlg, L"Neue Karte nicht angelegt", result, recovery, directory);
        return false;
    }
    document_result_notice(g_dlg, L"Neue Karte angelegt", result, recovery, directory);
    core::log_info("Neue Karte angelegt: " + id);
    scan_maps();
    const auto old = std::find(g.map_files.begin(), g.map_files.end(), previous);
    g.cur_map = old == g.map_files.end() ? -1 : static_cast<int>(old - g.map_files.begin());
    rebuild_map_menu();
    const auto created = std::find(g.map_files.begin(), g.map_files.end(), id + ".json");
    if (created == g.map_files.end() ||
        !load_map(static_cast<int>(created - g.map_files.begin()), true)) {
        MessageBoxW(g_dlg, L"Die Karte wurde angelegt, konnte aber nicht geladen werden.",
                    L"Neue Karte", MB_OK | MB_ICONERROR);
        maptree_select_current();
        return false;
    }
    sync_object_list();
    sync_property_fields();
    update_title();
    status_mode(L"Neue Karte angelegt");
    return true;
}

void delete_current_map() {
    if (g.cur_map < 0 || g.cur_map >= static_cast<int>(g.map_files.size())) return;
    const std::string filename = g.map_files[g.cur_map];
    const std::uint64_t map_epoch = g.map_epoch, project_epoch = g.project_epoch;
    const std::string name = g.cur_map < static_cast<int>(g.map_names.size())
                                 ? g.map_names[g.cur_map] : "?";
    std::wstring text = L"Karte \"" + utf8_to_wide(name) + L"\" wirklich löschen?";
    if (g.dirty || object_form_pending() || event_editor_pending())
        text += L"\r\nAuch ungespeicherte Änderungen und Formulareingaben gehen verloren.";
    const int r = data_prompt(g.main, text.c_str(), L"Karte löschen",
                              MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);
    if (r != IDYES) return;
    if (g.project_epoch != project_epoch || g.map_epoch != map_epoch || g.cur_map < 0 ||
        g.cur_map >= static_cast<int>(g.map_files.size()) || g.map_files[g.cur_map] != filename) return;
    const std::string directory = g.project_dir + "\\maps";
    const std::string path = directory + "\\" + filename;
    DocumentStore store;
    DocumentRecovery recovery;
    assets::Result result = store.open(g.root, g.project_name, true, filename);
    if (result == assets::Result::Ok) {
        const DocumentOutcome outcome = document_erase_confirmed(
            store, g.map_disk_present, g.map_disk_text, [] {
                return data_prompt(g.main, L"Die Kartendatei wurde seit dem Laden oder Speichern extern geändert. "
                                           L"Auch die externen Änderungen wirklich löschen?",
                                   L"Karten-Dateikonflikt", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) == IDYES;
            }, recovery);
        if (outcome.cancelled) return;
        result = outcome.result;
    }
    if (result != assets::Result::Ok) {
        document_result_notice(g.main, L"Karte nicht gelöscht", result, recovery, directory);
        return;
    }
    core::log_info("Karte gelöscht: " + path);
    document_result_notice(g.main, L"Karte gelöscht", result, recovery, directory);
    cancel_object_drag();
    close_event_editor();
    ++g.map_epoch;
    g.cur_map = -1;
    g.sel_obj = -1;
    g.dirty = false;
    g.undo_stack.clear();
    g.redo_stack.clear();
    g.objects = nullptr;
    g.map_root = JsonValue{};
    g.map_disk_present = false;
    g.map_disk_text.clear();
    g.terrain_error.clear();
    g.terrain_warning.clear();
    g.terrain_memory_error = false;
    g.terrain_field_warning = false;
    g.atmosphere_warning = false;
    g.terrain->clear();
    g.renderer->clear_terrain();
    scan_maps();
    rebuild_map_menu();
    if (!g.map_files.empty()) load_map(0, true);
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

// Pfade statt langlebiger Zeiger: jede Strukturänderung löst die Referenz auf
// der vorbereiteten Event-Kopie neu auf.
struct EventCmdRef {
    EventCommandPath path;
    int index = -1;
    int depth = 0;
    bool header = false;
    bool insertable = true;
    std::string label;
};

struct EventDlgState {
    HWND win = nullptr;
    EventViewState view;
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
    HWND lbl_status = nullptr;
    HWND list_pages = nullptr;
    HWND ed_cswitch = nullptr;
    HWND cb_cswval = nullptr;
    HWND ed_cvar = nullptr;
    HWND cb_cvop = nullptr;
    HWND ed_cvval = nullptr;
    HWND cb_cself = nullptr;
    HWND cb_cselfval = nullptr;
    int sel_event = -1;
    int sel_page = -1;
    int sel_cmd = -1;
    std::vector<EventCmdRef> refs;
    EventForm event_before;
    EventPageForm page_before;
    EventCommandForm command_before;
    bool event_active = false, page_active = false, command_active = false;
    bool event_editable[4] = {};
    bool page_editable[3] = {};
    bool command_editable[5] = {};
    bool syncing = false;
    bool select_queued[3] = {};
    bool select_delayed[3] = {};
};

static EventDlgState g_ev;

struct EventSyncGuard {
    bool previous;
    EventSyncGuard() : previous(g_ev.syncing) { g_ev.syncing = true; }
    ~EventSyncGuard() { g_ev.syncing = previous; }
};

// Etappe 18f: Befehlstypen mit Kategorien (Enum und JSON-Namen im Modell).
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
    return editor_number_text(v, prec);
}

std::string ev_text(HWND h) { return control_text(h); }

void ev_set_text(HWND h, const std::string& s) {
    SetWindowTextW(h, utf8_to_wide(s).c_str());
}

std::string ev_str_of(const JsonValue& o, const char* key, const std::string& def = "") {
    const JsonValue* v = o.find(key);
    return (v && v->is_str()) ? v->str : def;
}

double ev_num_of(const JsonValue& o, const char* key, double def = 0.0) {
    const JsonValue* v = o.find(key);
    return (v && v->is_num()) ? v->number() : def;
}

bool ev_bool_of(const JsonValue& o, const char* key, bool def = false) {
    const JsonValue* v = o.find(key);
    return (v && v->type == JsonValue::Type::Bool) ? v->b : def;
}

// Lesen legt weder eine fehlende Liste an noch überschreibt es beschädigte
// Daten. Doppelte Root-Schlüssel sind wegen Ruby-Last-Wins nicht eindeutig.
JsonValue* ev_events() {
    if (!g.map_root.is_obj() || !event_unique(g.map_root, "events")) return nullptr;
    JsonValue* e = g.map_root.find("events");
    return e && e->is_arr() ? e : nullptr;
}

JsonValue* ev_selected_event() {
    JsonValue* events = ev_events();
    if (!events || g_ev.sel_event < 0 || static_cast<size_t>(g_ev.sel_event) >= events->arr.size()) return nullptr;
    return &events->arr[static_cast<size_t>(g_ev.sel_event)];
}

const JsonValue* ev_selected_commands() {
    const JsonValue* e = ev_selected_event();
    if (!e || g_ev.sel_page < 0) return nullptr;
    return event_commands_view(*e, static_cast<size_t>(g_ev.sel_page));
}

// Ein überlanges oder von Win32 nicht unverändert darstellbares Feld wird
// gesperrt. Seine Snapshot-Version wird beim Auslesen unverändert verwendet.
void ev_install_field(HWND control, const std::string& text, bool active, bool& editable) {
    ev_set_text(control, text);
    std::string returned;
    editable = active && control_text_strict(control, returned) && returned == text;
    if (active && !editable) SetWindowTextW(control, L"[nicht vollständig darstellbar – geschützt]");
    EnableWindow(control, editable ? TRUE : FALSE);
}

bool ev_read_field(HWND control, bool editable, std::string& value) {
    return !editable || control_text_strict(control, value);
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
            return "Variable \"" + ev_str_of(c, "name") + "\" += " + ev_num_str(add->number(), 2);
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
            return "Item \"" + ev_str_of(c, "name") + "\" -" + ev_num_str(take->number(), 0);
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
        return amt ? "Heilen +" + ev_num_str(amt->number(), 0) : "Heilen (voll)";
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

// Keine Zweige beim Anzeigen anlegen. Auch leere oder fehlende Zweige bekommen
// eine auswählbare Kopfzeile; ihr Array entsteht erst bei gültigem Einfügen.
void ev_flatten(const JsonValue& list, int depth, const EventCommandPath& path) {
    if (depth > 128) {
        EventCmdRef r; r.path = path; r.depth = depth; r.header = true; r.insertable = false;
        r.label = "[weitere Ebenen nicht angezeigt]";
        g_ev.refs.push_back(std::move(r));
        return;
    }
    for (size_t i = 0; i < list.arr.size(); ++i) {
        const JsonValue& c = list.arr[i];
        EventCmdRef r;
        r.path = path; r.index = static_cast<int>(i); r.depth = depth;
        r.label = c.is_obj() ? ev_cmd_label(c) : "[ungültiger Befehl]";
        if (r.label.empty()) r.label = "[unbekannter Befehl]";
        g_ev.refs.push_back(std::move(r));
        if (!c.is_obj() || !event_unique(c, "cmd")) continue;
        const std::string type = ev_str_of(c, "cmd");
        if (type == "choices") {
            const JsonValue* opts = event_unique(c, "options") ? c.find("options") : nullptr;
            if (!opts || !opts->is_arr()) continue;
            const bool unique = event_unique(c, "branches");
            const JsonValue* branches = unique ? c.find("branches") : nullptr;
            for (size_t oi = 0; oi < opts->arr.size(); ++oi) {
                const JsonValue* br = branches && branches->is_arr() && oi < branches->arr.size()
                                          ? &branches->arr[oi] : nullptr;
                EventCmdRef h; h.path = path; h.path.push_back({i, "branches", oi});
                h.depth = depth + 1; h.header = true;
                h.insertable = unique && (!branches || branches->is_arr()) && (!br || br->is_arr());
                h.label = "Wenn [" + event_string_view(&opts->arr[oi], "[leer]") + "]:";
                if (!h.insertable) h.label += " [ungültig/mehrfach]";
                else if (!br) h.label += " [fehlt]";
                const EventCommandPath child_path = h.path;
                g_ev.refs.push_back(std::move(h));
                if (br && br->is_arr() && unique) ev_flatten(*br, depth + 2, child_path);
            }
            continue;
        }
        if (type != "if_switch" && type != "if_variable" && type != "if_self_switch") continue;
        for (const char* key : {"then", "else"}) {
            const bool unique = event_unique(c, key);
            const JsonValue* branch = unique ? c.find(key) : nullptr;
            EventCmdRef h; h.path = path; h.path.push_back({i, key, 0});
            h.depth = depth + 1; h.header = true;
            h.insertable = unique && (!branch || branch->is_arr());
            h.label = std::string(key) == "then" ? "Dann:" : "Sonst:";
            if (!h.insertable) h.label += " [ungültig/mehrfach]";
            else if (!branch) h.label += " [fehlt]";
            const EventCommandPath child_path = h.path;
            g_ev.refs.push_back(std::move(h));
            if (branch && branch->is_arr() && unique) ev_flatten(*branch, depth + 2, child_path);
        }
    }
}

void ev_refresh_events() {
    if (!g_ev.list_events) return;
    EventSyncGuard sync;
    SendMessageW(g_ev.list_events, LB_RESETCONTENT, 0, 0);
    JsonValue* events = ev_events();
    const bool root_ok = g.map_root.is_obj() && event_unique(g.map_root, "events") &&
                         (!g.map_root.find("events") || events);
    EnableWindow(GetDlgItem(g_ev.win, IDEV_NEW), root_ok ? TRUE : FALSE);
    if (!events) {
        g_ev.sel_event = -1;
        if (!root_ok) {
            const int row = static_cast<int>(SendMessageW(g_ev.list_events, LB_ADDSTRING, 0,
                reinterpret_cast<LPARAM>(L"[events ungültig oder mehrfach – extern prüfen]")));
            SendMessageW(g_ev.list_events, LB_SETITEMDATA, row, static_cast<LPARAM>(-1));
        }
        EnableWindow(GetDlgItem(g_ev.win, IDEV_DEL), FALSE);
        EnableWindow(GetDlgItem(g_ev.win, IDEV_DUP), FALSE);
        return;
    }
    for (size_t i = 0; i < events->arr.size(); ++i) {
        const JsonValue& e = events->arr[i];
        std::string line;
        if (!e.is_obj()) line = "[ungültiger Event-Eintrag]";
        else {
            const JsonValue* id = e.find("id");
            const std::string id_s = id && id->is_str() ? event_string_view(id, "?") : event_number_view(id, "?");
            line = id_s + ". " + event_string_view(e.find("name"), "Event") + "  (" +
                   event_number_view(e.find("x"), "0") + " / " + event_number_view(e.find("z"), "0") + ")";
        }
        const std::wstring wide = utf8_to_wide(line);
        const int row = static_cast<int>(SendMessageW(g_ev.list_events, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(wide.c_str())));
        SendMessageW(g_ev.list_events, LB_SETITEMDATA, row, static_cast<LPARAM>(i));
    }
    if (g_ev.sel_event < 0 || static_cast<size_t>(g_ev.sel_event) >= events->arr.size()) g_ev.sel_event = -1;
    SendMessageW(g_ev.list_events, LB_SETCURSEL, static_cast<WPARAM>(g_ev.sel_event), 0);
    EnableWindow(GetDlgItem(g_ev.win, IDEV_DEL), g_ev.sel_event >= 0 ? TRUE : FALSE);
    EnableWindow(GetDlgItem(g_ev.win, IDEV_DUP), g_ev.sel_event >= 0 && events->arr[g_ev.sel_event].is_obj() ? TRUE : FALSE);
}

void ev_refresh_fields() {
    EventSyncGuard sync;
    JsonValue* e = ev_selected_event();
    g_ev.event_active = e && e->is_obj();
    g_ev.event_before = g_ev.event_active ? event_form_read(*e) : EventForm{};
    std::string id;
    if (g_ev.event_active) {
        const JsonValue* v = e->find("id");
        id = v && v->is_str() ? event_string_view(v) : event_number_view(v);
    }
    ev_set_text(g_ev.ed_id, id);
    ev_install_field(g_ev.ed_name, g_ev.event_before.name, g_ev.event_active, g_ev.event_editable[0]);
    ev_install_field(g_ev.ed_x, g_ev.event_before.x, g_ev.event_active, g_ev.event_editable[1]);
    ev_install_field(g_ev.ed_z, g_ev.event_before.z, g_ev.event_active, g_ev.event_editable[2]);
    ev_install_field(g_ev.ed_radius, g_ev.event_before.radius, g_ev.event_active, g_ev.event_editable[3]);
    EnableWindow(GetDlgItem(g_ev.win, IDEV_APPLY_EV), g_ev.event_active ? TRUE : FALSE);
}

void ev_refresh_pages() {
    EventSyncGuard sync;
    SendMessageW(g_ev.list_pages, LB_RESETCONTENT, 0, 0);
    const JsonValue* e = ev_selected_event();
    const size_t count = e ? event_page_count(*e) : 0;
    if (!count) {
        g_ev.sel_page = -1;
        if (e && e->is_obj() && !event_unique(*e, "pages")) {
            const int row = static_cast<int>(SendMessageW(g_ev.list_pages, LB_ADDSTRING, 0,
                reinterpret_cast<LPARAM>(L"[pages mehrfach – extern prüfen]")));
            SendMessageW(g_ev.list_pages, LB_SETITEMDATA, row, static_cast<LPARAM>(-1));
        }
        EnableWindow(GetDlgItem(g_ev.win, IDEV_NEWPAGE), FALSE);
        EnableWindow(GetDlgItem(g_ev.win, IDEV_DELPAGE), FALSE);
        return;
    }
    if (g_ev.sel_page < 0 || static_cast<size_t>(g_ev.sel_page) >= count) g_ev.sel_page = 0;
    for (size_t i = 0; i < count; ++i) {
        const JsonValue* page = event_page_view(*e, i);
        std::string line = "Seite " + std::to_string(i + 1);
        if (!page) line += "  [ungültiger Eintrag]";
        else if (event_legacy_page(*e)) line += "  [implizit / Altformat]";
        else {
            const JsonValue* cond = page->find("conditions");
            if (cond && cond->is_obj()) {
                const JsonValue* sw = cond->find("switch");
                const JsonValue* va = cond->find("variable");
                const JsonValue* ss = cond->find("self_switch");
                if (sw && sw->is_obj()) line += "  [Schalter '" + event_string_view(sw->find("name")) + "']";
                if (va && va->is_obj()) line += "  [Var '" + event_string_view(va->find("name")) + "']";
                if (ss && ss->is_obj()) line += "  [Self-Switch '" + event_string_view(ss->find("letter")) + "']";
                if (!sw && !va && !ss) line += "  [immer/weitere Bedingungen]";
            } else if (cond && !cond->is_null()) line += "  [Bedingungen ungültig]";
            else line += "  [immer]";
        }
        const std::wstring wide = utf8_to_wide(line);
        const int row = static_cast<int>(SendMessageW(g_ev.list_pages, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(wide.c_str())));
        SendMessageW(g_ev.list_pages, LB_SETITEMDATA, row, static_cast<LPARAM>(i));
    }
    SendMessageW(g_ev.list_pages, LB_SETCURSEL, g_ev.sel_page, 0);
    EnableWindow(GetDlgItem(g_ev.win, IDEV_NEWPAGE), TRUE);
    EnableWindow(GetDlgItem(g_ev.win, IDEV_DELPAGE), count > 1 ? TRUE : FALSE);
}

void ev_refresh_page_fields() {
    EventSyncGuard sync;
    const JsonValue* e = ev_selected_event();
    g_ev.page_active = e && g_ev.sel_page >= 0 && event_page_view(*e, static_cast<size_t>(g_ev.sel_page));
    g_ev.page_before = g_ev.page_active ? event_page_form_read(*e, static_cast<size_t>(g_ev.sel_page)) : EventPageForm{};
    if (!g_ev.page_active) g_ev.page_before.trigger = -1;
    SendMessageW(g_ev.cb_trigger, CB_SETCURSEL, static_cast<WPARAM>(g_ev.page_before.trigger), 0);
    SendMessageW(g_ev.cb_cswval, CB_SETCURSEL, static_cast<WPARAM>(g_ev.page_before.switch_value), 0);
    SendMessageW(g_ev.cb_cvop, CB_SETCURSEL, static_cast<WPARAM>(g_ev.page_before.variable_op), 0);
    SendMessageW(g_ev.cb_cself, CB_SETCURSEL, static_cast<WPARAM>(g_ev.page_before.self_letter), 0);
    SendMessageW(g_ev.cb_cselfval, CB_SETCURSEL, static_cast<WPARAM>(g_ev.page_before.self_value), 0);
    ev_install_field(g_ev.ed_cswitch, g_ev.page_before.switch_name, g_ev.page_active, g_ev.page_editable[0]);
    ev_install_field(g_ev.ed_cvar, g_ev.page_before.variable_name, g_ev.page_active, g_ev.page_editable[1]);
    ev_install_field(g_ev.ed_cvval, g_ev.page_before.variable_value, g_ev.page_active, g_ev.page_editable[2]);
    for (HWND control : {g_ev.cb_trigger, g_ev.cb_cswval, g_ev.cb_cvop, g_ev.cb_cself, g_ev.cb_cselfval})
        EnableWindow(control, g_ev.page_active ? TRUE : FALSE);
    EnableWindow(GetDlgItem(g_ev.win, IDEV_APPLY_PAGE), g_ev.page_active ? TRUE : FALSE);
}

void ev_update_status() {
    if (!g_ev.lbl_status) return;
    bool protected_field = false;
    if (g_ev.event_active) for (bool editable : g_ev.event_editable) if (!editable) protected_field = true;
    if (g_ev.page_active) for (bool editable : g_ev.page_editable) if (!editable) protected_field = true;
    if (g_ev.command_active) for (bool editable : g_ev.command_editable) if (!editable) protected_field = true;
    const JsonValue* e = ev_selected_event();
    const wchar_t* status = protected_field ? L"Zu lange/nicht darstellbare Felder sind geschützt."
        : e && e->is_obj() && event_legacy_page(*e) ? L"Altformat: Seiten entstehen erst bei Bedarf."
        : L"Übernehmen, dann Karte im Hauptfenster speichern.";
    SetWindowTextW(g_ev.lbl_status, status);
}

// Formularfelder je Befehlstyp ein-/ausblenden und beschriften.
void ev_cmd_visibility() {
    const int t = static_cast<int>(SendMessageW(g_ev.cb_ce_type, CB_GETCURSEL, 0, 0));
    auto show = [](HWND h, bool v) {
        if (!h) return;
        ShowWindow(h, v ? SW_SHOW : SW_HIDE);
        bool editable = g_ev.command_active;
        if (h == g_ev.ed_ce_name) editable &= g_ev.command_editable[0];
        if (h == g_ev.ed_ce_val1) editable &= g_ev.command_editable[1];
        if (h == g_ev.ed_ce_val2) editable &= g_ev.command_editable[2];
        if (h == g_ev.ed_ce_val3) editable &= g_ev.command_editable[3];
        if (h == g_ev.ed_ce_text) editable &= g_ev.command_editable[4];
        EnableWindow(h, v && editable ? TRUE : FALSE);
    };
    EnableWindow(g_ev.cb_ce_type, g_ev.command_active ? TRUE : FALSE);
    const bool has_name = event_command_uses_field(t, 0);
    const bool has_val1 = event_command_uses_field(t, 1);
    const bool has_val2 = event_command_uses_field(t, 2);
    const bool has_val3 = event_command_uses_field(t, 3);
    const bool is_text = event_command_uses_field(t, 4);
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
    const bool has_hint = t == ET_INPUT_NUMBER || t == ET_BATTLE || t == ET_FADE ||
                          t == ET_EFFECT || t == ET_WEATHER || t == ET_ITEM ||
                          t == ET_HEAL || t == ET_MOVE_PLAYER || t == ET_VARIABLE ||
                          t == ET_SELF_SWITCH || t == ET_IF_SELF_SWITCH;
    show(g_ev.lbl_text, is_text || has_hint);
    show(g_ev.ed_ce_text, is_text);

    SetWindowTextW(g_ev.lbl_name, L"Name");
    SetWindowTextW(g_ev.lbl_val1, L"Wert");
    SetWindowTextW(g_ev.lbl_val2, L"Add");
    SetWindowTextW(g_ev.lbl_val3, L"Yaw (Radiant)");
    SetWindowTextW(g_ev.lbl_text, L"Zeilen (eine pro Zeile)");

    if (t == ET_SWITCH || t == ET_TOGGLE_SWITCH || t == ET_IF_SWITCH)
        SetWindowTextW(g_ev.lbl_name, L"Schalter");
    if (t == ET_SELF_SWITCH || t == ET_IF_SELF_SWITCH) {
        SetWindowTextW(g_ev.lbl_name, L"Self-Switch");
        SetWindowTextW(g_ev.lbl_text, L"Buchstabe A bis D; an/aus = Wert.");
    }
    if (t == ET_FADE) { SetWindowTextW(g_ev.lbl_name, L"Richtung");
                  SetWindowTextW(g_ev.lbl_val1, L"Sekunden");
                  SetWindowTextW(g_ev.lbl_text, L"Richtung: aus = dunkel, ein = hell."); }
    if (t == ET_MOVE_ROUTE) {
        SetWindowTextW(g_ev.lbl_name, L"Ziel");
        SetWindowTextW(g_ev.lbl_val1, L"Tempo m/s");
        SetWindowTextW(g_ev.lbl_val2, L"Warten (1/0)");
        // Etappe 19h.2: Schritte = XP-Vokabular (move_up, turn_left, wait,
        // XP-Codes 11..57, deutsche Kurznamen gehen weiter) - mover.rb-Kopf.
        SetWindowTextW(g_ev.lbl_val3, L"Parameter");
        SetWindowTextW(g_ev.lbl_text, L"Schritte pro Zeile; Ziel: NPC-Name/spieler; an = wiederholen.");
    }
    if (t == ET_INPUT_NUMBER) {
        // Etappe 19i: XP "Input Number Processing".
        SetWindowTextW(g_ev.lbl_name, L"Variable");
        SetWindowTextW(g_ev.lbl_val1, L"Ziffern (1-6)");
        SetWindowTextW(g_ev.lbl_val2, L"Startwert");
        SetWindowTextW(g_ev.lbl_text, L"Zahleneingabe: an/aus = Abbrechen erlaubt (Esc -> 0)");
    }
    if (t == ET_BATTLE) {
        SetWindowTextW(g_ev.lbl_name, L"Truppen-ID");
        SetWindowTextW(g_ev.lbl_val2, L"Verlust (1/0)");
        SetWindowTextW(g_ev.lbl_text, L"an/aus = Flucht; Verlust 1 verhindert Game Over.");
    }
    if (t == ET_VARIABLE) { SetWindowTextW(g_ev.lbl_name, L"Variable");
                  SetWindowTextW(g_ev.lbl_val1, L"Wert");
                  SetWindowTextW(g_ev.lbl_val2, L"Add");
                  SetWindowTextW(g_ev.lbl_text, L"Leerer Wert = nicht setzen; Add ist optional."); }
    if (t == ET_IF_VARIABLE) { SetWindowTextW(g_ev.lbl_name, L"Variable");
                  SetWindowTextW(g_ev.lbl_val1, L"Wert"); }
    if (t == ET_SOUND) { SetWindowTextW(g_ev.lbl_name, L"Sound-Name");
                  SetWindowTextW(g_ev.lbl_val1, L"Lautstärke"); }
    if (t == ET_EFFECT) { SetWindowTextW(g_ev.lbl_name, L"Effekt");
                  SetWindowTextW(g_ev.lbl_val1, L"Höhe");
                  SetWindowTextW(g_ev.lbl_val2, L"Farbe (hex)");
                  SetWindowTextW(g_ev.lbl_text, L"Effekt (sparkle/dust/burst)"); }
    if (t == ET_SCRIPT) SetWindowTextW(g_ev.lbl_name, L"Script-Name");
    if (t == ET_ITEM) { SetWindowTextW(g_ev.lbl_name, L"Item-ID");
                   SetWindowTextW(g_ev.lbl_val2, L"Menge (+/-)");
                   SetWindowTextW(g_ev.lbl_text, L"Negative Menge = nehmen, positive Menge = geben."); }
    if (t == ET_GOLD) SetWindowTextW(g_ev.lbl_val1, L"Gold (+/-)");
    if (t == ET_HEAL) { SetWindowTextW(g_ev.lbl_val1, L"HP");
                        SetWindowTextW(g_ev.lbl_text, L"Leeres HP-Feld heilt vollständig."); }
    if (t == ET_TELEPORT) { SetWindowTextW(g_ev.lbl_val1, L"X");
                  SetWindowTextW(g_ev.lbl_val2, L"Z"); }
    if (t == ET_MOVE_PLAYER) { SetWindowTextW(g_ev.lbl_val1, L"X");
                  SetWindowTextW(g_ev.lbl_val2, L"Z");
                  SetWindowTextW(g_ev.lbl_text, L"Neue Bewegungen sind relativ; bestehende behalten ihren Modus."); }
    if (t == ET_WEATHER) { SetWindowTextW(g_ev.lbl_name, L"Wetter");
                  SetWindowTextW(g_ev.lbl_val1, L"Intensität");
                  SetWindowTextW(g_ev.lbl_text, L"Wetter: kein, regen oder schnee."); }
    if (t == ET_SAVE_GAME || t == ET_LOAD_GAME)
        SetWindowTextW(g_ev.lbl_val1, L"Slot (1-3)");
    if (t == ET_WAIT) SetWindowTextW(g_ev.lbl_val1, L"Sekunden");
    if (t == ET_LOG) SetWindowTextW(g_ev.lbl_text, L"Log-Text");
    if (t == ET_CHOICES) SetWindowTextW(g_ev.lbl_text, L"Optionen (eine pro Zeile)");
}

void ev_refresh_cmds() {
    if (!g_ev.list_cmds) return;
    EventSyncGuard sync;
    SendMessageW(g_ev.list_cmds, LB_RESETCONTENT, 0, 0);
    g_ev.refs.clear();
    const JsonValue* cmds = ev_selected_commands();
    if (cmds) ev_flatten(*cmds, 0, {});
    for (size_t i = 0; i < g_ev.refs.size(); ++i) {
        const EventCmdRef& r = g_ev.refs[i];
        std::string text(static_cast<size_t>(r.depth) * 2, ' ');
        if (r.header) text += "-> ";
        text += r.label;
        const std::wstring wide = utf8_to_wide(text);
        const int row = static_cast<int>(SendMessageW(g_ev.list_cmds, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(wide.c_str())));
        SendMessageW(g_ev.list_cmds, LB_SETITEMDATA, row, static_cast<LPARAM>(i));
    }
    if (g_ev.sel_cmd < 0 || static_cast<size_t>(g_ev.sel_cmd) >= g_ev.refs.size()) g_ev.sel_cmd = -1;
    SendMessageW(g_ev.list_cmds, LB_SETCURSEL, static_cast<WPARAM>(g_ev.sel_cmd), 0);
    std::wstring target = L"Ziel: Wurzel";
    if (g_ev.sel_cmd >= 0) {
        const EventCmdRef& r = g_ev.refs[static_cast<size_t>(g_ev.sel_cmd)];
        if (r.header) target = r.insertable ? L"Ziel: ausgewählter Zweig" : L"Zweig nicht bearbeitbar";
        else if (!r.path.empty()) target = L"Ziel: Zweig-Ebene";
    }
    SetWindowTextW(g_ev.lbl_insert, target.c_str());
}

void ev_load_cmd_form() {
    EventSyncGuard sync;
    const JsonValue* command = nullptr;
    bool leaf = false;
    if (g_ev.page_active && g_ev.sel_cmd >= 0 && static_cast<size_t>(g_ev.sel_cmd) < g_ev.refs.size()) {
        const EventCmdRef& ref = g_ev.refs[static_cast<size_t>(g_ev.sel_cmd)];
        JsonValue* event = ev_selected_event();
        std::string error;
        JsonValue* list = ref.header || !event ? nullptr : event_command_list_mutable(
            *event, static_cast<size_t>(g_ev.sel_page), ref.path, false, error);
        if (list && ref.index >= 0 && static_cast<size_t>(ref.index) < list->arr.size()) {
            command = &list->arr[static_cast<size_t>(ref.index)];
            leaf = true;
        }
    }
    g_ev.command_active = g_ev.page_active;
    g_ev.command_before = command ? event_command_form_read(*command) : EventCommandForm{};
    SendMessageW(g_ev.cb_ce_type, CB_SETCURSEL, static_cast<WPARAM>(g_ev.command_before.type), 0);
    SendMessageW(g_ev.cb_ce_op, CB_SETCURSEL, static_cast<WPARAM>(g_ev.command_before.op), 0);
    SendMessageW(g_ev.cb_ce_bool, CB_SETCURSEL, static_cast<WPARAM>(g_ev.command_before.flag), 0);
    ev_install_field(g_ev.ed_ce_name, g_ev.command_before.name, g_ev.command_active, g_ev.command_editable[0]);
    ev_install_field(g_ev.ed_ce_val1, g_ev.command_before.val1, g_ev.command_active, g_ev.command_editable[1]);
    ev_install_field(g_ev.ed_ce_val2, g_ev.command_before.val2, g_ev.command_active, g_ev.command_editable[2]);
    ev_install_field(g_ev.ed_ce_val3, g_ev.command_before.val3, g_ev.command_active, g_ev.command_editable[3]);
    ev_install_field(g_ev.ed_ce_text, g_ev.command_before.text, g_ev.command_active, g_ev.command_editable[4]);
    for (int id : {IDEV_APPLYCMD, IDEV_DELCMD, IDEV_UP, IDEV_DOWN}) EnableWindow(GetDlgItem(g_ev.win, id), leaf ? TRUE : FALSE);
    const bool selected = g_ev.sel_cmd >= 0 && static_cast<size_t>(g_ev.sel_cmd) < g_ev.refs.size();
    const bool can_insert = g_ev.command_active && (!selected ||
        !g_ev.refs[static_cast<size_t>(g_ev.sel_cmd)].header || g_ev.refs[static_cast<size_t>(g_ev.sel_cmd)].insertable);
    EnableWindow(GetDlgItem(g_ev.win, IDEV_INSERT), can_insert ? TRUE : FALSE);
    ev_cmd_visibility();
    ev_update_status();
}

void ev_refresh() {
    EventSyncGuard sync;
    ev_refresh_events();
    ev_refresh_fields();
    ev_refresh_pages();
    ev_refresh_page_fields();
    ev_refresh_cmds();
    ev_load_cmd_form();
}

bool ev_current_event_form(EventForm& form) {
    form = g_ev.event_before;
    if (!g_ev.event_active) return true;
    return ev_read_field(g_ev.ed_name, g_ev.event_editable[0], form.name) &&
           ev_read_field(g_ev.ed_x, g_ev.event_editable[1], form.x) &&
           ev_read_field(g_ev.ed_z, g_ev.event_editable[2], form.z) &&
           ev_read_field(g_ev.ed_radius, g_ev.event_editable[3], form.radius);
}

bool ev_current_page_form(EventPageForm& form) {
    form = g_ev.page_before;
    if (!g_ev.page_active) return true;
    form.trigger = static_cast<int>(SendMessageW(g_ev.cb_trigger, CB_GETCURSEL, 0, 0));
    form.switch_value = static_cast<int>(SendMessageW(g_ev.cb_cswval, CB_GETCURSEL, 0, 0));
    form.variable_op = static_cast<int>(SendMessageW(g_ev.cb_cvop, CB_GETCURSEL, 0, 0));
    form.self_letter = static_cast<int>(SendMessageW(g_ev.cb_cself, CB_GETCURSEL, 0, 0));
    form.self_value = static_cast<int>(SendMessageW(g_ev.cb_cselfval, CB_GETCURSEL, 0, 0));
    return ev_read_field(g_ev.ed_cswitch, g_ev.page_editable[0], form.switch_name) &&
           ev_read_field(g_ev.ed_cvar, g_ev.page_editable[1], form.variable_name) &&
           ev_read_field(g_ev.ed_cvval, g_ev.page_editable[2], form.variable_value);
}

bool ev_current_command_form(EventCommandForm& form) {
    form = g_ev.command_before;
    if (!g_ev.command_active) return true;
    form.type = static_cast<int>(SendMessageW(g_ev.cb_ce_type, CB_GETCURSEL, 0, 0));
    form.op = static_cast<int>(SendMessageW(g_ev.cb_ce_op, CB_GETCURSEL, 0, 0));
    form.flag = static_cast<int>(SendMessageW(g_ev.cb_ce_bool, CB_GETCURSEL, 0, 0));
    return ev_read_field(g_ev.ed_ce_name, g_ev.command_editable[0], form.name) &&
           ev_read_field(g_ev.ed_ce_val1, g_ev.command_editable[1], form.val1) &&
           ev_read_field(g_ev.ed_ce_val2, g_ev.command_editable[2], form.val2) &&
           ev_read_field(g_ev.ed_ce_val3, g_ev.command_editable[3], form.val3) &&
           ev_read_field(g_ev.ed_ce_text, g_ev.command_editable[4], form.text);
}

bool event_editor_pending() {
    if (!g_ev.win || !IsWindow(g_ev.win)) return false;
    EventForm ev; EventPageForm page; EventCommandForm command;
    return !ev_current_event_form(ev) || !ev_current_page_form(page) || !ev_current_command_form(command) ||
           (g_ev.event_active && !event_form_equal(ev, g_ev.event_before)) ||
           (g_ev.page_active && !event_page_form_equal(page, g_ev.page_before)) ||
           (g_ev.command_active && !event_command_form_equal(command, g_ev.command_before));
}

void ev_error(const EventEdit& edit) {
    const std::wstring message = utf8_to_wide(edit.error + "\nEs wurde nichts übernommen.");
    data_prompt(g_ev.win, message.c_str(), L"Event-Editor", MB_OK | MB_ICONWARNING);
    HWND focus = nullptr;
    if (edit.field == "name") focus = g_ev.ed_name;
    else if (edit.field == "x") focus = g_ev.ed_x;
    else if (edit.field == "z") focus = g_ev.ed_z;
    else if (edit.field == "radius") focus = g_ev.ed_radius;
    else if (edit.field == "trigger") focus = g_ev.cb_trigger;
    else if (edit.field == "switch_name") focus = g_ev.ed_cswitch;
    else if (edit.field == "switch_value") focus = g_ev.cb_cswval;
    else if (edit.field == "variable_name") focus = g_ev.ed_cvar;
    else if (edit.field == "variable_op") focus = g_ev.cb_cvop;
    else if (edit.field == "variable_value") focus = g_ev.ed_cvval;
    else if (edit.field == "self_letter") focus = g_ev.cb_cself;
    else if (edit.field == "self_value") focus = g_ev.cb_cselfval;
    else if (edit.field == "cmd_type") focus = g_ev.cb_ce_type;
    else if (edit.field == "cmd_name") focus = g_ev.ed_ce_name;
    else if (edit.field == "cmd_val1") focus = g_ev.ed_ce_val1;
    else if (edit.field == "cmd_val2") focus = g_ev.ed_ce_val2;
    else if (edit.field == "cmd_val3") focus = g_ev.ed_ce_val3;
    else if (edit.field == "cmd_text") focus = g_ev.ed_ce_text;
    else if (edit.field == "cmd_op") focus = g_ev.cb_ce_op;
    else if (edit.field == "cmd_flag") focus = g_ev.cb_ce_bool;
    if (focus && IsWindowEnabled(focus) && IsWindowVisible(focus)) {
        SetFocus(focus);
        // Bei Fehlern kann das Feld auch nach einer MessageBox außerhalb des
        // gerade sichtbaren Ausschnitts liegen.
        event_view_reveal(g_ev.view, g_ev.win, focus);
        g_ev.view.last_focus = GetFocus();
    }
}

bool ev_confirm_edit(const EventEdit& edit) {
    std::wstring text;
    if (edit.converts_command)
        text += L"Der Befehlstyp wird ersetzt. Alte Felder, Zusatzdaten und Zweige dieses Befehls gehen verloren.\r\n";
    if (edit.removes_condition)
        text += L"Eine Bedingung wird einschließlich ihrer Zusatzfelder entfernt.\r\n";
    if (edit.replaces_invalid)
        text += L"Fehlerhafte/nicht darstellbare oder nicht mehr verwendete Altdaten werden ersetzt oder entfernt.\r\n";
    if (edit.renames_event)
        text += L"Der Eventname wird geändert. Bestehende Spielstände und Self-Switches verwenden den alten Namen und werden nicht umbenannt.\r\n";
    if (edit.reassigns_choices)
        text += L"Wahlzweige bleiben nach Index erhalten. Umordnen/Entfernen von Optionen ändert ihre Zuordnung; zusätzliche Zweige bleiben im JSON.\r\n";
    if (text.empty()) return true;
    text += L"\r\nTrotzdem übernehmen?";
    return data_prompt(g_ev.win, text.c_str(), L"Event-Daten ändern",
                       MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) == IDYES;
}

enum class EvCommandApply { None, Auto, Update, Insert };

// Beliebige Kombination der drei Formulare zuerst gemeinsam auf einer Kopie
// validieren. Bei Fehler oder abgelehnter Warnung bleibt auch g.dirty gleich.
bool ev_apply(bool event_fields, bool page_fields, EvCommandApply command_mode) {
    JsonValue* current = ev_selected_event();
    if (!current) return !event_fields && !page_fields && command_mode == EvCommandApply::None;
    EventBatch batch;
    batch.page = g_ev.sel_page >= 0 ? static_cast<size_t>(g_ev.sel_page) : 0;
    batch.event_fields = event_fields && g_ev.event_active;
    batch.page_fields = page_fields && g_ev.page_active;
    batch.event_before = g_ev.event_before; batch.page_before = g_ev.page_before;
    batch.command_before = g_ev.command_before;
    if ((batch.event_fields && !ev_current_event_form(batch.event_after)) ||
        (batch.page_fields && !ev_current_page_form(batch.page_after)) ||
        (command_mode != EvCommandApply::None && g_ev.command_active && !ev_current_command_form(batch.command_after))) {
        ev_error(event_failure(*current, "", "Ein Formular enthält ungültiges Unicode oder konnte nicht vollständig gelesen werden."));
        return false;
    }
    if (command_mode != EvCommandApply::None && g_ev.command_active) {
        if (command_mode == EvCommandApply::Auto && event_command_form_equal(batch.command_after, g_ev.command_before))
            command_mode = EvCommandApply::None;
        if (command_mode != EvCommandApply::None) {
            const bool selected = g_ev.sel_cmd >= 0 && static_cast<size_t>(g_ev.sel_cmd) < g_ev.refs.size();
            const EventCmdRef ref = selected ? g_ev.refs[static_cast<size_t>(g_ev.sel_cmd)] : EventCmdRef{};
            const bool leaf = selected && !ref.header && ref.index >= 0;
            if (command_mode == EvCommandApply::Auto) command_mode = leaf ? EvCommandApply::Update : EvCommandApply::Insert;
            if (command_mode == EvCommandApply::Update && !leaf) {
                ev_error(event_failure(*current, "commands", "Bitte einen Befehl auswählen.")); return false;
            }
            if (command_mode == EvCommandApply::Insert && selected && ref.header && !ref.insertable) {
                ev_error(event_failure(*current, "commands", "Dieser Zweig ist beschädigt oder nicht eindeutig.")); return false;
            }
            const bool fresh = command_mode == EvCommandApply::Insert ||
                               batch.command_before.type != batch.command_after.type;
            if (fresh && !event_command_fields_available(batch.command_after.type, g_ev.command_editable)) {
                ev_error(event_failure(*current, "commands",
                    "Ein für diesen neuen Befehl benötigtes Feld ist wegen seiner alten Länge/Darstellung geschützt. "
                    "Bitte den Editor erneut öffnen und einen leeren Befehlsentwurf verwenden."));
                return false;
            }
            batch.command = command_mode == EvCommandApply::Update ? EventCommandAction::Update : EventCommandAction::Insert;
            if (selected) batch.path = ref.path;
            if (leaf) batch.command_index = static_cast<size_t>(ref.index);
        }
    }
    const std::string original = json_write(*current);
    EventEdit edit = event_batch_prepare(*current, batch);
    if (!edit.ok) { ev_error(edit); return false; }
    if (batch.event_fields && batch.event_before.name != batch.event_after.name &&
        event_name_used(g.map_root, batch.event_after.name, g_ev.sel_event)) {
        ev_error(event_failure(*current, "name",
            "Dieser Name wird bereits von einem Event oder NPC verwendet. "
            "Bitte einen eindeutigen Namen wählen (Spielstände und Self-Switches)."));
        return false;
    }
    if (edit.changed && !ev_confirm_edit(edit)) return false;
    // Eine modale Rückfrage pumpt Nachrichten. Das Ziel vor dem Commit neu
    // prüfen, statt einen möglicherweise veralteten Zeiger zu verwenden.
    current = ev_selected_event();
    if (!current || json_write(*current) != original) {
        ev_error(event_failure(JsonValue{}, "event", "Das Event wurde während der Rückfrage verändert.")); return false;
    }
    if (edit.changed) {
        *current = std::move(edit.value);
        g.dirty = true;
    }
    if (batch.event_fields) g_ev.event_before = batch.event_after;
    if (batch.page_fields) g_ev.page_before = batch.page_after;
    EventSyncGuard sync;
    ev_refresh_events();
    ev_refresh_pages();
    if (batch.command == EventCommandAction::Insert) g_ev.sel_cmd = -1;
    ev_refresh_cmds();
    if (batch.command != EventCommandAction::None) ev_load_cmd_form();
    ev_update_status();
    if (edit.changed) update_title();
    return true;
}

bool resolve_event_edits() {
    if (!event_editor_pending()) return true;
    const int answer = data_prompt(g_ev.win,
        L"Nicht übernommene Event-, Seiten- oder Befehlseingaben übernehmen?\r\n"
        L"Ja ändert den ausgewählten Befehl; ohne Auswahl wird ein Entwurf eingefügt.\r\n\r\n"
        L"Ja: übernehmen   Nein: Eingaben verwerfen   Abbrechen: zurück",
        L"Event-Editor", MB_YESNOCANCEL | MB_ICONQUESTION | MB_DEFBUTTON3);
    if (answer == IDYES) return ev_apply(true, true, EvCommandApply::Auto);
    if (answer != IDNO) return false;
    ev_refresh();
    return true;
}

bool ev_confirm_delete(const wchar_t* what) {
    std::wstring message = std::wstring(what) + L" wirklich löschen?\r\n"
        L"Auch enthaltene Befehle, Zweige und Zusatzdaten gehen verloren. F6-Löschen ist nicht rückgängig.";
    return data_prompt(g_ev.win, message.c_str(), L"Event-Daten löschen",
                       MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) == IDYES;
}

bool ev_commit_structural(EventEdit edit) {
    if (!edit.ok) { ev_error(edit); return false; }
    JsonValue* event = ev_selected_event();
    if (!event) return false;
    if (edit.changed) {
        *event = std::move(edit.value);
        g.dirty = true;
        update_title();
    }
    return true;
}

void ev_new_page() {
    if (!resolve_event_edits()) return;
    const JsonValue* event = ev_selected_event();
    if (!event) return;
    EventEdit edit = event_add_page_prepare(*event);
    if (!ev_commit_structural(std::move(edit))) return;
    event = ev_selected_event();
    g_ev.sel_page = static_cast<int>(event_page_count(*event)) - 1;
    g_ev.sel_cmd = -1;
    ev_refresh();
}

void ev_delete_page() {
    if (!resolve_event_edits()) return;
    const JsonValue* event = ev_selected_event();
    if (!event || g_ev.sel_page < 0 || event_page_count(*event) <= 1) return;
    if (!ev_confirm_delete(L"Diese Seite")) return;
    event = ev_selected_event();
    if (!event) return;
    EventEdit edit = event_delete_page_prepare(*event, static_cast<size_t>(g_ev.sel_page));
    if (!ev_commit_structural(std::move(edit))) return;
    event = ev_selected_event();
    if (static_cast<size_t>(g_ev.sel_page) >= event_page_count(*event))
        g_ev.sel_page = static_cast<int>(event_page_count(*event)) - 1;
    g_ev.sel_cmd = -1;
    ev_refresh();
}

void ev_new_event() {
    if (!resolve_event_edits()) return;
    if (!g.map_root.is_obj() || !event_unique(g.map_root, "events")) {
        ev_error(event_failure(g.map_root, "events", "events ist mehrfach vorhanden.")); return;
    }
    JsonValue* events = g.map_root.find("events");
    if (events && !events->is_arr()) {
        ev_error(event_failure(g.map_root, "events", "events ist keine Liste; die Altdaten bleiben erhalten.")); return;
    }
    const int id = event_next_id(g.map_root);
    if (id <= 0) { ev_error(event_failure(g.map_root, "events", "Keine freie Event-ID gefunden.")); return; }
    JsonValue e = JsonValue::make_obj();
    e.set("id", JsonValue::make_integer(std::to_string(id)));
    e.set("name", JsonValue::make_str(event_available_name(g.map_root, "Event " + std::to_string(id))));
    const double grid = std::isfinite(g.grid) && g.grid > 0 ? g.grid : 1.0;
    const double x = g.hover_valid && std::isfinite(g.hover_pos.x) ? g.hover_pos.x : 0.0;
    const double z = g.hover_valid && std::isfinite(g.hover_pos.z) ? g.hover_pos.z : 0.0;
    const double snapped_x = std::round(x / grid) * grid, snapped_z = std::round(z / grid) * grid;
    if (!std::isfinite(snapped_x) || !std::isfinite(snapped_z) ||
        std::fabs(snapped_x) > 1000000.0 || std::fabs(snapped_z) > 1000000.0) {
        ev_error(event_failure(g.map_root, "events", "Die Rasterposition des neuen Events ist nicht darstellbar.")); return;
    }
    e.set("x", JsonValue::make_num(snapped_x, false));
    e.set("z", JsonValue::make_num(snapped_z, false));
    e.set("radius", JsonValue::make_num(2.0, false));
    JsonValue page = JsonValue::make_obj();
    page.set("conditions", JsonValue::make_obj());
    page.set("trigger", JsonValue::make_str("action"));
    page.set("commands", JsonValue::make_arr());
    JsonValue pages = JsonValue::make_arr(); pages.arr.push_back(std::move(page));
    e.set("pages", std::move(pages));
    if (!events) {
        g_ev.refs.clear();
        events = &g.map_root.set("events", JsonValue::make_arr());
        g.objects = g.map_root.find("objects");
    }
    events->arr.push_back(std::move(e));
    g_ev.sel_event = static_cast<int>(events->arr.size()) - 1;
    g_ev.sel_page = 0; g_ev.sel_cmd = -1;
    g.dirty = true;
    ev_refresh(); update_title();
}

void ev_delete_event() {
    if (!resolve_event_edits() || !ev_selected_event()) return;
    if (!ev_confirm_delete(L"Dieses Event")) return;
    JsonValue* events = ev_events();
    if (!events || g_ev.sel_event < 0 || static_cast<size_t>(g_ev.sel_event) >= events->arr.size()) return;
    events->arr.erase(events->arr.begin() + g_ev.sel_event);
    if (g_ev.sel_event >= static_cast<int>(events->arr.size())) g_ev.sel_event = static_cast<int>(events->arr.size()) - 1;
    g_ev.sel_page = 0; g_ev.sel_cmd = -1;
    g.dirty = true; ev_refresh(); update_title();
}

void ev_duplicate_event() {
    if (!resolve_event_edits()) return;
    const JsonValue* event = ev_selected_event();
    if (!event) return;
    const int id = event_next_id(g.map_root);
    if (id <= 0) { ev_error(event_failure(*event, "events", "Keine freie Event-ID gefunden.")); return; }
    EventEdit edit = event_duplicate_prepare(*event, id, &g.map_root);
    if (!edit.ok) { ev_error(edit); return; }
    if (edit.unshifted_coordinate && data_prompt(g_ev.win,
            L"Mindestens eine Koordinate fehlt, ist ungültig oder kann nicht um eins verschoben werden. "
            L"Sie bleibt in der Kopie unverändert; die Kopie kann daher am selben Ort liegen. Fortfahren?",
            L"Event duplizieren", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES) return;
    JsonValue* events = ev_events();
    if (!events || g_ev.sel_event < 0 || static_cast<size_t>(g_ev.sel_event) >= events->arr.size()) return;
    events->arr.push_back(std::move(edit.value));
    g_ev.sel_event = static_cast<int>(events->arr.size()) - 1;
    g_ev.sel_page = 0; g_ev.sel_cmd = -1;
    g.dirty = true; ev_refresh(); update_title();
}

void ev_insert_command() { ev_apply(false, false, EvCommandApply::Insert); }
void ev_update_command() { ev_apply(false, false, EvCommandApply::Update); }

void ev_delete_command() {
    if (!resolve_event_edits()) return;
    if (g_ev.sel_cmd < 0 || static_cast<size_t>(g_ev.sel_cmd) >= g_ev.refs.size()) return;
    const EventCmdRef ref = g_ev.refs[static_cast<size_t>(g_ev.sel_cmd)];
    if (ref.header || ref.index < 0 || !ev_confirm_delete(L"Diesen Befehl")) return;
    const JsonValue* event = ev_selected_event();
    if (!event || g_ev.sel_page < 0) return;
    EventEdit edit = event_command_remove_or_move(*event, static_cast<size_t>(g_ev.sel_page), ref.path,
                                                 static_cast<size_t>(ref.index), 0);
    if (!ev_commit_structural(std::move(edit))) return;
    g_ev.sel_cmd = -1;
    ev_refresh_cmds(); ev_load_cmd_form();
}

bool ev_same_path(const EventCommandPath& a, const EventCommandPath& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (a[i].command != b[i].command || a[i].key != b[i].key || a[i].choice != b[i].choice) return false;
    return true;
}

void ev_move_command(int delta) {
    if (!resolve_event_edits()) return;
    if (g_ev.sel_cmd < 0 || static_cast<size_t>(g_ev.sel_cmd) >= g_ev.refs.size()) return;
    const EventCmdRef ref = g_ev.refs[static_cast<size_t>(g_ev.sel_cmd)];
    const JsonValue* event = ev_selected_event();
    if (ref.header || ref.index < 0 || !event || g_ev.sel_page < 0) return;
    EventEdit edit = event_command_remove_or_move(*event, static_cast<size_t>(g_ev.sel_page), ref.path,
                                                 static_cast<size_t>(ref.index), delta);
    if (!edit.ok) { ev_error(edit); return; }
    if (!edit.changed || !ev_commit_structural(std::move(edit))) return;
    const int to = ref.index + (delta < 0 ? -1 : 1);
    g_ev.sel_cmd = -1;
    ev_refresh_cmds();
    for (size_t i = 0; i < g_ev.refs.size(); ++i) {
        const EventCmdRef& candidate = g_ev.refs[i];
        if (!candidate.header && candidate.index == to && ev_same_path(candidate.path, ref.path)) {
            g_ev.sel_cmd = static_cast<int>(i); break;
        }
    }
    SendMessageW(g_ev.list_cmds, LB_SETCURSEL, static_cast<WPARAM>(g_ev.sel_cmd), 0);
    ev_load_cmd_form();
}

HWND ev_control(const wchar_t* cls, const wchar_t* text, DWORD style,
                int x, int y, int w, int h, int id) {
    if (id != 0 && std::wcscmp(cls, L"Static") != 0) style |= WS_TABSTOP;
    return event_view_add(g_ev.view, g_ev.win, cls, text, style, x, y, w, h, id);
}

void ev_label(const wchar_t* text, int x, int y, int w) {
    ev_control(L"Static", text, 0, x, y, w, 20, 0);
}

void ev_queue_selection(int which) {
    if (!g_ev.win || which < 0 || which > 2 || g_ev.select_queued[which]) return;
    const UINT msg = which == 0 ? WM_EVENT_SELECT_EVENT : which == 1 ? WM_EVENT_SELECT_PAGE : WM_EVENT_SELECT_COMMAND;
    g_ev.select_queued[which] = PostMessageW(g_ev.win, msg, 0, 0) != 0;
    if (!g_ev.select_queued[which]) {
        HWND list = which == 0 ? g_ev.list_events : which == 1 ? g_ev.list_pages : g_ev.list_cmds;
        const int previous = which == 0 ? g_ev.sel_event : which == 1 ? g_ev.sel_page : g_ev.sel_cmd;
        listbox_select_data(list, static_cast<LPARAM>(previous));
    }
}

void event_requeue_selections() {
    if (!g_ev.win || !IsWindow(g_ev.win)) return;
    for (int i = 0; i < 3; ++i) if (g_ev.select_delayed[i]) {
        g_ev.select_delayed[i] = false;
        ev_queue_selection(i);
    }
}

int ev_list_selection(HWND list) {
    const LRESULT row = SendMessageW(list, LB_GETCURSEL, 0, 0);
    if (row == LB_ERR) return -1;
    const LRESULT item = SendMessageW(list, LB_GETITEMDATA, static_cast<WPARAM>(row), 0);
    return item == LB_ERR ? -1 : static_cast<int>(item);
}

bool ev_reconcile_selection(int which) {
    g_ev.select_queued[which] = false;
    HWND list = which == 0 ? g_ev.list_events : which == 1 ? g_ev.list_pages : g_ev.list_cmds;
    const int previous = which == 0 ? g_ev.sel_event : which == 1 ? g_ev.sel_page : g_ev.sel_cmd;
    const int next = ev_list_selection(list);
    if (next == previous) return true;
    EventCmdRef target;
    const bool has_target = which == 2 && next >= 0 && static_cast<size_t>(next) < g_ev.refs.size();
    if (has_target) target = g_ev.refs[static_cast<size_t>(next)];
    if (!resolve_event_edits()) {
        listbox_select_data(list, static_cast<LPARAM>(previous));
        return false;
    }
    if (which == 0) {
        g_ev.sel_event = next; g_ev.sel_page = 0; g_ev.sel_cmd = -1;
        ev_refresh();
    } else if (which == 1) {
        g_ev.sel_page = next; g_ev.sel_cmd = -1;
        ev_refresh();
    } else {
        // Eine bestätigte Änderung kann Kopfzeilen einfügen/entfernen; die
        // angeklickte Zeile deshalb anhand ihres Pfades wiederfinden.
        g_ev.sel_cmd = -1;
        if (has_target) for (size_t i = 0; i < g_ev.refs.size(); ++i) {
            const EventCmdRef& r = g_ev.refs[i];
            if (r.header == target.header && r.index == target.index && ev_same_path(r.path, target.path)) {
                g_ev.sel_cmd = static_cast<int>(i); break;
            }
        }
        ev_refresh_cmds();
        ev_load_cmd_form();
    }
    return true;
}

// Ein schneller Button- oder Schließen-Aufruf darf keine noch gepostete
// Listenauswahl übergehen und dadurch die vorherige Zeile bearbeiten.
// Dies läuft nur außerhalb von LBN_SELCHANGE, nie im Listen-Callback selbst.
bool ev_flush_queued_selections() {
    for (int i = 0; i < 3; ++i)
        if (g_ev.select_queued[i] && !ev_reconcile_selection(i)) return false;
    return true;
}

// Auch Aktionen des Haupt-/Projektfensters dürfen keine noch gepostete
// Auswahl ignorieren. Der Auswahlabgleich selbst ruft nur den Kern auf, damit
// diese Funktion nicht rekursiv denselben Listenwechsel bearbeitet.
bool resolve_event_editor() {
    // Andere moduslose Fenster können bereits gepostete Befehle während einer
    // MessageBox zustellen, selbst wenn sie inzwischen deaktiviert wurden.
    if (g.data_prompt_active) return false;
    return ev_flush_queued_selections() && resolve_event_edits();
}

LRESULT CALLBACK event_proc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    if (g.data_prompt_active) {
        const int which = msg == WM_EVENT_SELECT_EVENT ? 0 : msg == WM_EVENT_SELECT_PAGE ? 1 :
                          msg == WM_EVENT_SELECT_COMMAND ? 2 : -1;
        if (which >= 0) {
            if (g_ev.select_queued[which]) {
                g_ev.select_queued[which] = false;
                g_ev.select_delayed[which] = true;
            }
            return 0;
        }
        if (msg == WM_COMMAND) {
            if (HIWORD(wp) == LBN_SELCHANGE) {
                const int id = LOWORD(wp);
                HWND list = id == IDEV_LIST ? g_ev.list_events : id == IDEV_PAGES ? g_ev.list_pages
                           : id == IDEV_CMDS ? g_ev.list_cmds : nullptr;
                const int previous = id == IDEV_LIST ? g_ev.sel_event : id == IDEV_PAGES ? g_ev.sel_page
                                   : id == IDEV_CMDS ? g_ev.sel_cmd : -1;
                if (list) listbox_select_data(list, static_cast<LPARAM>(previous));
            }
            return 0;
        }
        if (msg == WM_CLOSE) return 0;
    }
    if (event_view_message(g_ev.view, h, msg, wp, lp)) return 0;
    switch (msg) {
        // Ein bereits synchron abgeglichener Post ist nur noch eine veraltete
        // Nachricht und darf keine weitere Auswahländerung auslösen.
        case WM_EVENT_SELECT_EVENT: if (g_ev.select_queued[0]) ev_reconcile_selection(0); return 0;
        case WM_EVENT_SELECT_PAGE: if (g_ev.select_queued[1]) ev_reconcile_selection(1); return 0;
        case WM_EVENT_SELECT_COMMAND: if (g_ev.select_queued[2]) ev_reconcile_selection(2); return 0;
        case WM_COMMAND: {
            const int id = LOWORD(wp), code = HIWORD(wp);
            if (g_ev.syncing) return 0;
            if (id == IDEV_LIST && code == LBN_SELCHANGE) { ev_queue_selection(0); return 0; }
            if (id == IDEV_PAGES && code == LBN_SELCHANGE) { ev_queue_selection(1); return 0; }
            if (id == IDEV_CMDS && code == LBN_SELCHANGE) { ev_queue_selection(2); return 0; }
            if (id == IDEV_CETYPE && code == CBN_SELCHANGE) { ev_cmd_visibility(); return 0; }
            if (code != BN_CLICKED) return 0;
            if (!ev_flush_queued_selections()) return 0;
            if (id == IDEV_NEWPAGE)   { ev_new_page(); return 0; }
            if (id == IDEV_DELPAGE)   { ev_delete_page(); return 0; }
            if (id == IDEV_APPLY_PAGE) { ev_apply(false, true, EvCommandApply::None); return 0; }
            if (id == IDEV_NEW)      { ev_new_event(); return 0; }
            if (id == IDEV_DEL)      { ev_delete_event(); return 0; }
            if (id == IDEV_DUP)      { ev_duplicate_event(); return 0; }
            if (id == IDEV_CLOSE || id == IDCANCEL) { if (resolve_event_edits()) DestroyWindow(h); return 0; }
            if (id == IDEV_APPLY_EV) { ev_apply(true, false, EvCommandApply::None); return 0; }
            if (id == IDEV_INSERT)   { ev_insert_command(); return 0; }
            if (id == IDEV_APPLYCMD) { ev_update_command(); return 0; }
            if (id == IDEV_DELCMD)   { ev_delete_command(); return 0; }
            if (id == IDEV_UP)       { ev_move_command(-1); return 0; }
            if (id == IDEV_DOWN)     { ev_move_command(+1); return 0; }
            return 0;
        }
        case WM_CLOSE:
            if (resolve_event_editor()) DestroyWindow(h);
            return 0;
        case WM_DESTROY:
            if (g_ev.win == h) g_ev.view.ready = false;
            return 0;
        case WM_NCDESTROY:
            if (g_ev.win == h) {
                event_view_dispose(g_ev.view);
                g_ev = EventDlgState{};
            }
            break;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

void open_event_editor() {
    if (g_ev.win) {
        event_view_fit_existing(g_ev.win);
        SetForegroundWindow(g_ev.win);
        event_view_reveal(g_ev.view, g_ev.win, GetFocus());
        return;
    }
    // F2 schützt den umgekehrten Weg ebenfalls. Sonst könnte ein bereits
    // offenes F2 beim Übernehmen während F6 den JSON-Baum verschieben.
    if (g.props_dlg) {
        MessageBoxW(g.main, L"Bitte zuerst den Karten-Dialog schließen, bevor der Event-Editor geöffnet wird.",
                    L"Event-Editor", MB_OK | MB_ICONINFORMATION);
        return;
    }
    if (g.cur_map < 0) return;
    cancel_object_drag();
    g_ev = EventDlgState{};
    g_ev.syncing = true;

    WNDCLASSW wc{};
    wc.lpfnWndProc = event_proc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"RuneEditorEventDlg";
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
    RegisterClassW(&wc);

    // Die ursprüngliche Formularfläche wird DPI-gerecht skaliert, das äußere
    // Fenster aber auf die Arbeitsfläche des aktuellen Monitors begrenzt.
    // Bei Platzmangel bleiben beide Fenster-Scrollleisten und Tab-Navigation.
    // Kein WS_CLIPCHILDREN: Die transparenten Gruppenboxen brauchen den vom
    // Elternfenster gezeichneten Hintergrund auch beim Verschieben.
    const DWORD style = WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME |
                        WS_HSCROLL | WS_VSCROLL;
    const DWORD ex_style = WS_EX_DLGMODALFRAME;
    UINT dpi = GetDpiForWindow(g.main);
    if (!dpi) dpi = EVENT_LAYOUT_DPI;
    const EventLayoutRect bounds = event_view_initial_rect(g.main, style, ex_style, dpi);
    g_ev.win = CreateWindowExW(ex_style, L"RuneEditorEventDlg", L"Event-Editor", style,
                               bounds.left, bounds.top,
                               event_layout_int(static_cast<std::int64_t>(bounds.right) - bounds.left),
                               event_layout_int(static_cast<std::int64_t>(bounds.bottom) - bounds.top),
                               g.main, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!g_ev.win) { g_ev = EventDlgState{}; return; }
    event_view_init(g_ev.view, dpi, g.font);

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
    g_ev.lbl_status = ev_control(L"Static", L"", 0, 12, 600, 280, 36, 0);
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

    ev_label(L"Befehlstyp:", 316, 494, 110);
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
    g_ev.lbl_val3 = ev_control(L"Static", L"Yaw (Radiant)", 0, 606, 554, 90, 20, IDEV_L_VAL3);
    g_ev.ed_ce_val3 = ev_control(L"Edit", nullptr, WS_BORDER | ES_RIGHT,
                                 698, 550, 70, 24, IDEV_CE_VAL3);
    g_ev.cb_ce_bool = ev_control(L"ComboBox", nullptr, CBS_DROPDOWNLIST | WS_VSCROLL,
                                 776, 550, 90, 180, IDEV_CE_BOOL);
    SendMessageW(g_ev.cb_ce_bool, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"an"));
    SendMessageW(g_ev.cb_ce_bool, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"aus"));
    SendMessageW(g_ev.cb_ce_bool, CB_SETCURSEL, 0, 0);

    g_ev.lbl_text = ev_control(L"Static", L"Zeilen (eine pro Zeile)", 0,
                               316, 582, 620, 20, IDEV_L_TEXT);
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

    // Lange Eingaben nicht am Win32-Standardlimit abschneiden. Noch längere
    // Bestandsdaten werden durch den Rücklesevergleich feldweise geschützt.
    for (HWND field : {g_ev.ed_name, g_ev.ed_x, g_ev.ed_z, g_ev.ed_radius,
                       g_ev.ed_cswitch, g_ev.ed_cvar, g_ev.ed_cvval,
                       g_ev.ed_ce_name, g_ev.ed_ce_val1, g_ev.ed_ce_val2,
                       g_ev.ed_ce_val3, g_ev.ed_ce_text})
        SendMessageW(field, EM_SETLIMITTEXT, 1000000, 0);
    // Ein fehlendes oder ungültiges events wird beim Öffnen nicht angelegt.
    JsonValue* evs = ev_events();
    g_ev.sel_event = evs && !evs->arr.empty() ? 0 : -1;
    g_ev.sel_page = 0;
    g_ev.sel_cmd = -1;
    ev_refresh();
    g_ev.syncing = false;
    g_ev.view.ready = true;
    event_view_update(g_ev.view, g_ev.win);

    ShowWindow(g_ev.win, SW_SHOW);
    SetForegroundWindow(g_ev.win);
}

void close_event_editor() {
    if (g_ev.win) DestroyWindow(g_ev.win);
}

bool event_editor_open() {
    return g_ev.win && IsWindow(g_ev.win);
}

bool event_editor_focused() {
    if (!g_ev.win) return false;
    HWND fg = GetForegroundWindow();
    return fg == g_ev.win || (fg && IsChild(g_ev.win, fg));
}

#include "editor/placement_dialog.h"

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
    if (g.data_prompt_active || (g.props_dlg && g.dlg_mode != 1)) {
        if (msg == WM_CLOSE) return 0;
        if (msg == WM_COMMAND) {
            if (LOWORD(wp) == IDSC_LIST && HIWORD(wp) == LBN_SELCHANGE && g_sc.list)
                listbox_select_data(g_sc.list, static_cast<LPARAM>(g_sc.sel));
            return 0;
        }
    }
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
    std::string tile_before;
    bool tile_editable = false;
    bool indices_safe = true;
};

static MaterialDlgState g_md;

void ma_status(const std::string& text) {
    if (g_md.lbl_status) ev_set_text(g_md.lbl_status, text);
}

void ma_refresh_info() {
    if (!g_md.win) return;
    const MaterialInfo* m = g_md.sel.empty() ? nullptr : mat_find(g_md.sel);
    const bool has_sel = g_md.indices_safe && !g_md.sel.empty();
    const bool is_imported = g_md.indices_safe && m != nullptr;

    EnableWindow(GetDlgItem(g_md.win, IDMA_TILE), is_imported && g_material_writable ? TRUE : FALSE);
    EnableWindow(GetDlgItem(g_md.win, IDMA_TILESET), is_imported && g_material_writable ? TRUE : FALSE);
    EnableWindow(GetDlgItem(g_md.win, IDMA_DEL), is_imported && g_material_writable ? TRUE : FALSE);
    g_md.tile_before.clear();
    g_md.tile_editable = false;
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
        if (!m->tile_valid) info += "\nKachelwert ungültig; ein Ersatz erfordert Bestätigung.";
        ev_set_text(g_md.lbl_info, info);
        if (m->tile_valid) {
            const JsonValue* list = material_array(g_material_document);
            const JsonValue* original = list && m->document_index < list->arr.size()
                ? list->arr[m->document_index].find("tile") : nullptr;
            g_md.tile_before = original ? json_write(*original) : "2.0";
            if (!g_md.tile_before.empty() && g_md.tile_before.back() == '\n') g_md.tile_before.pop_back();
        }
        ev_set_text(g_md.ed_tile, g_md.tile_before);
        std::string displayed;
        g_md.tile_editable = g_material_writable && control_text_strict(g_md.ed_tile, displayed) &&
                             displayed == g_md.tile_before;
        EnableWindow(g_md.ed_tile, g_md.tile_editable ? TRUE : FALSE);
        EnableWindow(GetDlgItem(g_md.win, IDMA_TILESET), g_md.tile_editable ? TRUE : FALSE);
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
    g_md.indices_safe = true;
    LRESULT select = LB_ERR;
    for (std::size_t i = 0; i < g_md.names.size(); ++i) {
        const bool imported = mat_find(g_md.names[i]) != nullptr;
        const std::wstring line = utf8_to_wide(g_md.names[i]) +
                                  (imported ? L"  (importiert)" : L"");
        const LRESULT row = SendMessageW(g_md.list, LB_ADDSTRING, 0,
                                         reinterpret_cast<LPARAM>(line.c_str()));
        if (row == LB_ERR || row == LB_ERRSPACE ||
            SendMessageW(g_md.list, LB_SETITEMDATA, static_cast<WPARAM>(row), static_cast<LPARAM>(i)) == LB_ERR) {
            g_md.indices_safe = false;
            continue;
        }
        if (g_md.names[i] == g_md.sel) select = row;
    }
    if (g_md.indices_safe && select != LB_ERR) SendMessageW(g_md.list, LB_SETCURSEL, static_cast<WPARAM>(select), 0);
    ma_refresh_info();
    if (!g_md.indices_safe) ma_status("Materialliste unvollständig; Auswahl gesperrt. Bitte F3 erneut öffnen.");
}

void ma_apply_tile() {
    const MaterialInfo* selected = mat_find(g_md.sel);
    if (!selected || !selected->imported || !g_md.tile_editable || !g_material_writable) return;
    const std::string name = selected->name;
    const std::size_t index = selected->document_index;
    const bool old_valid = selected->tile_valid;
    const std::uint64_t epoch = g.project_epoch;
    std::string text;
    if (!control_text_strict(g_md.ed_tile, text)) { ma_status("Kachelwert nicht lesbar"); return; }
    if (text == g_md.tile_before) { ma_status("Kachel unverändert"); return; }
    JsonValue number;
    if (!database_decimal(text, number) || !world::terrain_valid_tile(number.number())) {
        ma_status("Kachel muss zwischen 0,01 und 10000 liegen");
        return;
    }
    if (!old_valid && data_prompt(g_md.win,
            L"Der bisherige Kachelwert ist ungültig. Soll nur dieser Wert ersetzt werden?",
            L"Material-Kachel", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES) return;
    const MaterialInfo* current = mat_find(name);
    if (g.project_epoch != epoch || g_md.sel != name || !current || current->document_index != index) return;
    JsonValue candidate = g_material_document;
    if (!material_set_tile(candidate, index, number.number()) || !mat_commit(std::move(candidate))) {
        ma_status("Nicht gespeichert: " + g_material_error);
        ma_refresh_info();
        return;
    }
    ma_refresh_info();
    ma_status("Kachel gespeichert: " + name);
}

// Die Auswahl vor einer möglichen Rückfrage als Wert sichern.
void ma_set_object_material(std::string material) {
    if (!g.objects || g.sel_obj < 0 || static_cast<size_t>(g.sel_obj) >= g.objects->arr.size()) return;
    if (!resolve_object_edits()) return;
    const JsonValue& object = g.objects->arr[static_cast<size_t>(g.sel_obj)];
    const ObjectPropsForm before = object_props_read(object);
    ObjectPropsForm after = before;
    after.material = material;
    ObjectPropsEdit edit = object_props_prepare(object, before, after);
    if (!edit.ok) {
        data_prompt(g_md.win, utf8_to_wide(edit.error).c_str(), L"Objekt-Material", MB_OK | MB_ICONWARNING);
        return;
    }
    const JsonValue* old = object.find("tex");
    const bool protected_old = old && old->is_str() && !g.object_field_editable[3];
    if (edit.changed && (edit.replaces_invalid || protected_old) && data_prompt(
            g_md.win, L"Das vorhandene Material ist fehlerhaft oder nicht vollständig darstellbar. "
                      L"Trotzdem ersetzen?",
            L"Objekt-Material", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES) return;
    if (edit.changed) {
        cancel_object_drag();
        push_undo();
        g.objects->arr[static_cast<size_t>(g.sel_obj)] = std::move(edit.value);
        g.dirty = true;
        sync_object_list();
        sync_property_fields();
        update_title();
    }
    ma_status(edit.changed ? "Objekt-Material geändert (Strg+S speichert)"
                           : "Objekt-Material unverändert");
}

void ma_apply_to_object() {
    if (!g_md.sel.empty()) ma_set_object_material(g_md.sel);
}

void ma_clear_object() { ma_set_object_material(""); }

void ma_delete() {
    const MaterialInfo* selected = mat_find(g_md.sel);
    if (!selected || !selected->imported || !g_material_writable) return;
    const std::string name = selected->name;
    const std::size_t index = selected->document_index;
    const std::uint64_t epoch = g.project_epoch;
    if (data_prompt(g_md.win,
            L"Materialeintrag entfernen? Die RGBA-Datei bleibt zur Sicherheit erhalten. "
            L"Karten und Objekte können weiterhin auf den Namen verweisen.",
            L"Material entfernen", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES) return;
    const MaterialInfo* current = mat_find(name);
    if (g.project_epoch != epoch || g_md.sel != name || !current || current->document_index != index) return;
    JsonValue candidate = g_material_document;
    if (!material_remove(candidate, index) || !mat_commit(std::move(candidate))) {
        ma_status("Nicht entfernt: " + g_material_error);
        ma_refresh_info();
        return;
    }
    if (g.renderer) {
        g.renderer->remove_project_texture(name);
        if (g.cur_map >= 0) regen_terrain();
    }
    g_md.sel.clear();
    ma_refresh_list();
    ma_status("Materialeintrag entfernt; RGBA-Datei erhalten: " + name);
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
    // Auch eine bereits an die Listbox zugestellte Auswahl darf das Ziel
    // einer Rückfrage nicht ändern; die angezeigte Auswahl zurücksetzen.
    const bool data_modal = g.data_prompt_active || (g.props_dlg && g.dlg_mode != 1);
    if (data_modal) {
        if (msg == WM_COMMAND) {
            if (LOWORD(wp) == IDMA_LIST && HIWORD(wp) == LBN_SELCHANGE) {
                const auto it = std::find(g_md.names.begin(), g_md.names.end(), g_md.sel);
                const LPARAM original = g_md.indices_safe && it != g_md.names.end()
                    ? static_cast<LPARAM>(it - g_md.names.begin()) : -1;
                listbox_select_data(g_md.list, original);
            }
            return 0;
        }
        if (msg == WM_CLOSE) return 0;
    }
    switch (msg) {
        case WM_COMMAND: {
            const int id = LOWORD(wp);
            const int code = HIWORD(wp);
            if (id == IDMA_LIST && code == LBN_SELCHANGE) {
                if (!g_md.indices_safe) {
                    listbox_select_data(g_md.list, -1);
                    ma_status("Materialliste unvollständig; Auswahl gesperrt. Bitte F3 erneut öffnen.");
                    return 0;
                }
                const LRESULT row = SendMessageW(g_md.list, LB_GETCURSEL, 0, 0);
                const LRESULT idx = row == LB_ERR ? LB_ERR :
                    SendMessageW(g_md.list, LB_GETITEMDATA, static_cast<WPARAM>(row), 0);
                if (idx >= 0 && static_cast<std::size_t>(idx) < g_md.names.size()) {
                    g_md.sel = g_md.names[static_cast<std::size_t>(idx)];
                    ma_refresh_info();
                }
                return 0;
            }
            if (id == IDMA_IMPORT)  { open_import_texture(); return 0; }
            if (!g_md.indices_safe && (id == IDMA_DEL || id == IDMA_APPLY || id == IDMA_TILESET)) {
                ma_status("Materialliste unvollständig; Auswahl gesperrt. Bitte F3 erneut öffnen.");
                return 0;
            }
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
    const std::uint64_t project_epoch = g.project_epoch;
    wchar_t file[MAX_PATH] = L"";
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g.main;
    ofn.lpstrFilter = L"Bilder (*.png;*.bmp)\0*.png;*.bmp\0Alle Dateien (*.*)\0*.*\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
    ofn.lpstrTitle = L"Grafik importieren (PNG/BMP)";
    if (!GetOpenFileNameW(&ofn)) return;
    if (project_epoch != g.project_epoch) {
        data_prompt(g.main, L"Das Projekt wurde während der Dateiauswahl gewechselt. Import abgebrochen.",
                    L"Grafik-Import", MB_OK | MB_ICONWARNING);
        return;
    }

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
        data_prompt(g.main, utf8_to_wide("Import abgebrochen: " + g_material_error).c_str(),
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
    SendMessageW(g_md.ed_tile, EM_SETLIMITTEXT, 1024, 0);
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
    if (!g_material_error.empty()) ma_status(g_material_error);
    ShowWindow(g_md.win, SW_SHOW);
    SetForegroundWindow(g_md.win);
}

void close_material_dialog() {
    if (g_md.win) DestroyWindow(g_md.win);
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
// im Edit komma-getrennt (siehe database_form.h).
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
    DatabaseForm before;
    bool field_editable[DB_MAX_ROWS] = {};
    bool gold_editable = false;
    bool hint_active = false;
    bool disk_present = false;
    std::string disk_text;   // Originalbytes für Konfliktprüfung beim Speichern
};

static DatabaseDlgState g_db;

std::string db_path() {
    return g.project_dir + "\\database.json";
}

// Das bloße Öffnen/Wechseln eines Reiters darf fehlende oder fehlerhafte
// Sammlungen nicht durch leere Listen ersetzen.
JsonValue* db_array(const char* key) {
    JsonValue* a = g_db.root.find(key);
    return a && a->is_arr() ? a : nullptr;
}

JsonValue* db_entry() {
    JsonValue* a = db_array(db_tab_key(g_db.tab));
    if (!a || g_db.sel < 0 || static_cast<size_t>(g_db.sel) >= a->arr.size()) return nullptr;
    return &a->arr[static_cast<size_t>(g_db.sel)];
}

std::vector<DatabaseFieldSpec> db_specs() {
    int count = 0;
    const DbField* fields = db_fields(g_db.tab, count);
    std::vector<DatabaseFieldSpec> result;
    for (int i = 0; i < count; ++i) {
        const DbField& f = fields[i];
        const DatabaseFieldKind kind = std::strcmp(f.key, "enemies") == 0 ? DatabaseFieldKind::IdList
            : !f.num ? DatabaseFieldKind::Text
            : f.prec == 0 ? DatabaseFieldKind::Integer : DatabaseFieldKind::Decimal;
        result.push_back({f.key, kind});
    }
    return result;
}

std::string db_text(HWND control) { return control_text(control); }

void db_status(const std::string& text) {
    if (g_db.lbl_status) ev_set_text(g_db.lbl_status, text);
}

bool db_load() {
    const auto fail = [](const wchar_t* message) {
        data_prompt(g_db.win, message, L"Datenbank nicht geladen", MB_OK | MB_ICONERROR);
        return false;
    };
    DocumentStore store;
    DocumentSnapshot snapshot;
    assets::Result result = store.open(g.root, g.project_name, false, "database.json");
    if (result == assets::Result::Ok) result = store.observe(snapshot);
    if (result != assets::Result::Ok) {
        document_result_notice(g_db.win, L"Datenbank nicht geladen", result, {}, g.project_dir);
        return false;
    }
    JsonValue loaded = JsonValue::make_obj();
    if (snapshot.present) {
        switch (database_parse_document(snapshot.bytes, loaded)) {
            case DatabaseParseStatus::InvalidJson:
                return fail(L"database.json enthält ungültiges JSON. Sie wird nicht überschrieben.");
            case DatabaseParseStatus::InvalidRoot:
                return fail(L"Die Datenbankwurzel ist kein Objekt. Sie wird nicht überschrieben.");
            case DatabaseParseStatus::InvalidUtf8:
                return fail(L"Die Datenbank enthält ungültiges UTF-8 oder Unicode. Sie wird nicht überschrieben.");
            case DatabaseParseStatus::DuplicateKey:
                return fail(L"Die Datenbank enthält doppelte Schlüssel. Sie wird nicht überschrieben.");
            case DatabaseParseStatus::Nonfinite:
                return fail(L"Die Datenbank enthält eine nicht darstellbare Kommazahl. Sie wird nicht überschrieben.");
            case DatabaseParseStatus::Ok: break;
        }
    }
    // Eine noch nicht vorhandene Datei entsteht erst durch explizites Speichern.
    g_db.root = std::move(loaded);
    g_db.disk_present = snapshot.present;
    g_db.disk_text = std::move(snapshot.bytes);
    g_db.dirty = false;
    return true;
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
    SendMessageW(g_db.list, LB_RESETCONTENT, 0, 0);
    const JsonValue* arr = db_array(db_tab_key(g_db.tab));
    if (!arr) { g_db.sel = -1; return; }
    int selected_row = -1;
    for (int i = 0; i < static_cast<int>(arr->arr.size()); ++i) {
        const JsonValue& e = arr->arr[static_cast<size_t>(i)];
        std::wstring label = std::to_wstring(i + 1) + L". ";
        if (!e.is_obj()) {
            label += L"[ungültiger Eintrag]";
        } else {
            label += utf8_to_wide(ev_str_of(e, "name", "?"));
            switch (g_db.tab) {
                case 1:
                    label += L"  [" + utf8_to_wide(ev_str_of(e, "kind", "-")) + L"]";
                    break;
                case 2:
                    label += L"  (HP " + utf8_to_wide(database_number_text(e.find("hp"))) + L")";
                    break;
                case 3: {
                    const JsonValue* en = e.find("enemies");
                    const size_t n = en && en->is_arr() ? en->arr.size() : 0;
                    label += L"  (" + std::to_wstring(n) + L" Gegner)";
                    break;
                }
                default:
                    label += L" (" + utf8_to_wide(ev_str_of(e, "class", "-")) + L")";
                    break;
            }
        }
        const int idx = static_cast<int>(SendMessageW(
            g_db.list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str())));
        if (idx >= 0) {
            SendMessageW(g_db.list, LB_SETITEMDATA, idx, static_cast<LPARAM>(static_cast<INT_PTR>(i)));
            if (i == g_db.sel) selected_row = idx;
        }
    }

    if (selected_row >= 0) SendMessageW(g_db.list, LB_SETCURSEL, selected_row, 0);
    else g_db.sel = -1;
}

void db_refresh_fields() {
    if (!g_db.win) return;
    const bool was_syncing = g_db.syncing;
    g_db.syncing = true;
    const std::vector<DatabaseFieldSpec> specs = db_specs();
    const JsonValue* entry = db_entry();
    const bool valid = entry && entry->is_obj();
    g_db.before = database_form_read(g_db.root, entry, specs);
    bool protected_field = false;
    for (int i = 0; i < DB_MAX_ROWS; ++i) {
        const bool used = static_cast<size_t>(i) < specs.size();
        const std::string text = used ? g_db.before.fields[static_cast<size_t>(i)] : std::string();
        ev_set_text(g_db.ed[i], text);
        g_db.field_editable[i] = valid && used && database_single_line(text) && db_text(g_db.ed[i]) == text;
        if (valid && used && !g_db.field_editable[i]) protected_field = true;
        EnableWindow(g_db.ed[i], g_db.field_editable[i] ? TRUE : FALSE);
    }
    EnableWindow(GetDlgItem(g_db.win, IDDB_DEL), entry ? TRUE : FALSE);
    ev_set_text(g_db.ed_gold, g_db.before.gold);
    g_db.gold_editable = database_single_line(g_db.before.gold) && db_text(g_db.ed_gold) == g_db.before.gold;
    EnableWindow(g_db.ed_gold, g_db.gold_editable ? TRUE : FALSE);
    if (!g_db.gold_editable) protected_field = true;
    g_db.hint_active = protected_field || (entry && !valid);
    if (protected_field)
        db_status("Nicht darstellbares Feld gesperrt; seine Daten bleiben erhalten.");
    else if (entry && !valid)
        db_status("Ungültiger Eintrag: unverändert lassen oder ausdrücklich löschen.");
    else if (g_db.dirty)
        db_status("Geändert - noch nicht gespeichert");
    else
        db_status("database.json: " + db_path());
    g_db.syncing = was_syncing;
}

void db_refresh() {
    const bool was_syncing = g_db.syncing;
    g_db.syncing = true;
    db_form_layout();
    db_refresh_lists();
    db_refresh_fields();
    g_db.syncing = was_syncing;
}

DatabaseForm db_current_form() {
    DatabaseForm current = g_db.before;
    if (g_db.gold_editable) current.gold = db_text(g_db.ed_gold);
    for (size_t i = 0; i < current.fields.size(); ++i)
        if (g_db.field_editable[i]) current.fields[i] = db_text(g_db.ed[i]);
    return current;
}

bool db_pending() {
    const DatabaseForm current = db_current_form();
    return current.gold != g_db.before.gold || current.fields != g_db.before.fields;
}

// Validiert Gold und Eintrag gemeinsam auf Kopien; erst nach Bestätigung
// werden nur die geänderten Felder übernommen. Beim Löschen eines Eintrags
// kann das Eintragsformular übersprungen werden, Gold bleibt aber erhalten.
bool db_apply(bool include_entry = true) {
    const std::vector<DatabaseFieldSpec> specs = db_specs();
    DatabaseForm after = db_current_form();
    if (!include_entry) after.fields = g_db.before.fields;
    const DatabaseEdit edit = database_form_prepare(g_db.root, db_entry(), specs, g_db.before, after);
    if (!edit.ok) {
        const std::string message = (edit.field.empty() ? std::string() : edit.field + ": ") + edit.error;
        data_prompt(g_db.win, utf8_to_wide(message).c_str(), L"Ungültige Eingabe", MB_OK | MB_ICONWARNING);
        if (edit.gold_error) SetFocus(g_db.ed_gold);
        else {
            for (size_t i = 0; i < specs.size(); ++i)
                if (edit.field == specs[i].key) { SetFocus(g_db.ed[i]); break; }
        }
        return false;
    }
    if (edit.replaces_invalid && data_prompt(
            g_db.win,
            L"Diese Änderung ersetzt fehlerhafte oder nicht eindeutig darstellbare Altdaten. Fortfahren?",
            L"Datenbank", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES) return false;
    if (!database_form_commit(g_db.root, db_tab_key(g_db.tab), g_db.sel, edit)) {
        data_prompt(g_db.win, L"Der Eintrag ist nicht mehr verfügbar. Es wurde nichts übernommen.",
                    L"Datenbank", MB_OK | MB_ICONERROR);
        return false;
    }
    if (edit.changed) {
        g_db.dirty = true;
        db_status("Geändert - noch nicht gespeichert");
    }
    g_db.before = std::move(after);
    return true;
}

void db_add() {
    if (!db_apply()) return;
    const char* key = db_tab_key(g_db.tab);
    JsonValue* arr = db_array(key);
    if (!arr) {
        if (g_db.root.find(key) && data_prompt(
                g_db.win, L"Die Sammlung ist keine Liste. Durch Neu wird sie ersetzt. Fortfahren?",
                L"Datenbank", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES) return;
        arr = &g_db.root.set(key, JsonValue::make_arr());
    }
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
    if (!g_db.hint_active) db_status("Neuen Eintrag angelegt - Speichern nicht vergessen");
}

void db_delete() {
    JsonValue* arr = db_array(db_tab_key(g_db.tab));
    if (!arr || g_db.sel < 0 || static_cast<size_t>(g_db.sel) >= arr->arr.size()) return;
    if (data_prompt(g_db.win, L"Diesen Eintrag und seine Zusatzdaten wirklich löschen?",
                    L"Datenbank", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES) return;
    if (!db_apply(false)) return;
    // Gold kann beim Setzen die Wurzel-Paarliste und damit arr verschieben.
    arr = db_array(db_tab_key(g_db.tab));
    arr->arr.erase(arr->arr.begin() + g_db.sel);
    if (g_db.sel >= static_cast<int>(arr->arr.size())) {
        g_db.sel = static_cast<int>(arr->arr.size()) - 1;
    }
    g_db.dirty = true;
    db_refresh();
    if (!g_db.hint_active) db_status("Eintrag gelöscht - Speichern nicht vergessen");
}

bool db_save() {
    if (!db_apply()) return false;
    if (!database_document_utf8(g_db.root) || !database_document_serializable(g_db.root) ||
        database_document_duplicates(g_db.root)) {
        data_prompt(g_db.win, L"Die Datenbank enthält ungültiges Unicode, doppelte Schlüssel "
                              L"oder eine nicht darstellbare Zahl. Sie wird nicht überschrieben.",
                    L"Datenbank nicht gespeichert", MB_OK | MB_ICONERROR);
        return false;
    }
    std::string output = json_write(g_db.root);
    DocumentStore store;
    DocumentRecovery recovery;
    assets::Result result = store.open(g.root, g.project_name, false, "database.json");
    if (result == assets::Result::Ok) {
        const DocumentOutcome outcome = document_save_confirmed(
            store, g_db.disk_present, g_db.disk_text, output, [] {
                return data_prompt(g_db.win,
                    L"database.json wurde außerhalb dieses Dialogs geändert, angelegt oder entfernt. "
                    L"Trotzdem mit dem Dialogstand überschreiben?",
                    L"Datenbank-Konflikt", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) == IDYES;
            }, recovery);
        if (outcome.cancelled) return false;
        result = outcome.result;
    }
    if (result != assets::Result::Ok) {
        db_status("FEHLER: database.json konnte nicht sicher geschrieben werden");
        document_result_notice(g_db.win, L"Datenbank nicht gespeichert", result, recovery, g.project_dir);
        return false;
    }
    g_db.disk_present = true;
    g_db.disk_text = std::move(output);
    g_db.dirty = false;
    db_refresh();
    if (!g_db.hint_active) db_status("Gespeichert: database.json");
    document_result_notice(g_db.win, L"Datenbank gespeichert", result, recovery, g.project_dir);
    return true;
}

bool database_request_close() {
    if (!g_db.win) return true;
    if (g_db.dirty || db_pending()) {
        const int result = data_prompt(g_db.win, L"Änderungen an der Datenbank speichern?",
                                       L"Datenbank", MB_YESNOCANCEL | MB_ICONQUESTION | MB_DEFBUTTON3);
        if ((result != IDYES && result != IDNO) || (result == IDYES && !db_save())) return false;
    }
    DestroyWindow(g_db.win);
    return true;
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
    if (g.data_prompt_active || (g.props_dlg && g.dlg_mode != 1)) {
        if (msg == WM_CLOSE) return 0;
        if (msg == WM_COMMAND) {
            const int id = LOWORD(wp), code = HIWORD(wp);
            // Falls eine Listen-/Reiterauswahl schon vor der Sperre im nativen
            // Control ankam, auf den noch gültigen Modellzustand zurücksetzen.
            if (id == IDDB_LIST && code == LBN_SELCHANGE && g_db.list)
                listbox_select_data(g_db.list, static_cast<LPARAM>(g_db.sel));
            if (code == BN_CLICKED && (id == IDDB_TAB_H || id == IDDB_TAB_I ||
                                       id == IDDB_TAB_E || id == IDDB_TAB_T)) {
                const int current = g_db.tab == 1 ? IDDB_TAB_I : g_db.tab == 2 ? IDDB_TAB_E
                                  : g_db.tab == 3 ? IDDB_TAB_T : IDDB_TAB_H;
                for (const int button : {IDDB_TAB_H, IDDB_TAB_I, IDDB_TAB_E, IDDB_TAB_T})
                    SendMessageW(GetDlgItem(g_db.win, button), BM_SETCHECK,
                                 button == current ? BST_CHECKED : BST_UNCHECKED, 0);
            }
            return 0;
        }
    }
    switch (msg) {
        case WM_COMMAND: {
            const int id = LOWORD(wp);
            const int code = HIWORD(wp);
            if (g_db.syncing) return 0;

            if (code == LBN_SELCHANGE && id == IDDB_LIST) {
                const LRESULT row = SendMessageW(g_db.list, LB_GETCURSEL, 0, 0);
                const LRESULT item = row == LB_ERR ? LB_ERR : SendMessageW(g_db.list, LB_GETITEMDATA, row, 0);
                if (!db_apply()) {
                    g_db.syncing = true;
                    db_refresh_lists();   // alte Auswahl wiederherstellen; Eingaben behalten
                    g_db.syncing = false;
                    return 0;
                }
                g_db.sel = item == LB_ERR ? -1 : static_cast<int>(item);
                db_refresh();
                return 0;
            }
            if (code == BN_CLICKED && (id == IDDB_TAB_H || id == IDDB_TAB_I ||
                                       id == IDDB_TAB_E || id == IDDB_TAB_T)) {
                if (!db_apply()) {
                    const int old_id = g_db.tab == 1 ? IDDB_TAB_I : g_db.tab == 2 ? IDDB_TAB_E
                                     : g_db.tab == 3 ? IDDB_TAB_T : IDDB_TAB_H;
                    for (const int button : {IDDB_TAB_H, IDDB_TAB_I, IDDB_TAB_E, IDDB_TAB_T})
                        SendMessageW(GetDlgItem(g_db.win, button), BM_SETCHECK,
                                     button == old_id ? BST_CHECKED : BST_UNCHECKED, 0);
                    return 0;
                }
                g_db.tab = (id == IDDB_TAB_I) ? 1
                         : (id == IDDB_TAB_E) ? 2
                         : (id == IDDB_TAB_T) ? 3 : 0;
                g_db.sel = -1;
                db_refresh();
                return 0;
            }
            if (code == EN_CHANGE && (id == IDDB_GOLD || (id >= IDDB_F1 && id < IDDB_F1 + DB_MAX_ROWS))) {
                db_status("Eingabe geändert - noch nicht gespeichert");
                return 0;
            }
            if (code != BN_CLICKED) return 0;
            if (id == IDDB_NEW)    { db_add(); return 0; }
            if (id == IDDB_DEL)    { db_delete(); return 0; }
            if (id == IDDB_SAVE)   { db_save(); return 0; }
            if (id == IDDB_RELOAD) {
                if ((g_db.dirty || db_pending()) && data_prompt(
                        h, L"Ungespeicherte Datenbankänderungen verwerfen und neu laden?",
                        L"Datenbank", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES) return 0;
                if (db_load()) {
                    g_db.sel = -1;
                    db_refresh();
                    if (!g_db.hint_active) db_status("Vom Datenträger neu geladen");
                }
                return 0;
            }
            if (id == IDDB_CLOSE)  { database_request_close(); return 0; }
            return 0;
        }
        case WM_CLOSE:
            database_request_close();
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
                g_db.before = DatabaseForm{};
                g_db.gold_editable = false;
                for (bool& editable : g_db.field_editable) editable = false;
                g_db.dirty = false;
                g_db.syncing = false;
                g_db.hint_active = false;
                g_db.disk_present = false;
                g_db.disk_text.clear();
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

    if (!g_db.win) return;
    if (!db_load()) { DestroyWindow(g_db.win); return; }

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
    g_db.ed_gold = db_control(L"Edit", L"0", WS_BORDER | ES_RIGHT | ES_AUTOHSCROLL,
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
        g_db.ed[i] = db_control(L"Edit", nullptr, WS_BORDER | ES_LEFT | ES_AUTOHSCROLL,
                                292, y + 18, 380, 24, IDDB_F1 + i);
    }

    g_db.lbl_status = db_control(L"Static", L"", 0, 292, 648, 396, 20, IDDB_STATUS);
    db_control(L"Button", L"Speichern", BS_PUSHBUTTON, 292, 674, 120, 30, IDDB_SAVE);
    db_control(L"Button", L"Neu laden", BS_PUSHBUTTON, 420, 674, 120, 30, IDDB_RELOAD);

    g_db.tab = 0;
    const JsonValue* actors = db_array("actors");
    g_db.sel = actors && !actors->arr.empty() ? 0 : -1;
    db_refresh();

    ShowWindow(g_db.win, SW_SHOW);
    SetForegroundWindow(g_db.win);
}

bool database_editor_open() {
    return g_db.win != nullptr;
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

// Ausgewählte Karte speichern und im eigenen Spielprozess testen.
void start_playtest() {
    std::string map_id;
    if (g.cur_map >= 0) {
        const int map_index = g.cur_map;
        if (map_index >= static_cast<int>(g.map_files.size()) ||
            !playtest_map_id(g.map_files[map_index], map_id)) {
            data_prompt(g.main, L"Die ausgewählte Karte hat keine gültige Karten-ID. "
                                L"Playtest abgebrochen.",
                        L"RuneEditor", MB_OK | MB_ICONERROR);
            return;
        }
        const std::string map_file = g.map_files[map_index];
        const std::uint64_t map_epoch = g.map_epoch, project_epoch = g.project_epoch;
        if (!save_map()) {
            data_prompt(g.main, L"Karte konnte nicht gespeichert werden - "
                                L"Playtest abgebrochen.",
                        L"RuneEditor", MB_OK | MB_ICONERROR);
            return;
        }
        // Rückfragen pumpen Nachrichten. Nur die tatsächlich gespeicherte
        // Karte des ursprünglichen Projekts darf an den Kindprozess gehen.
        if (g.project_epoch != project_epoch || g.map_epoch != map_epoch ||
            g.cur_map != map_index || map_index >= static_cast<int>(g.map_files.size()) ||
            g.map_files[map_index] != map_file) {
            data_prompt(g.main, L"Projekt oder Karte haben sich beim Speichern geändert. "
                                L"Playtest abgebrochen.",
                        L"RuneEditor", MB_OK | MB_ICONERROR);
            return;
        }
    }

    std::wstring exe, args, cwd;
    if (!playtest_command(g.root, g.project_name, map_id, exe, args, cwd)) {
        data_prompt(g.main, L"Rune3D.exe wurde nicht gefunden oder Projekt-/Kartenname ist ungültig. "
                            L"Bitte den Build und die Auswahl prüfen.",
                    L"RuneEditor", MB_OK | MB_ICONERROR);
        return;
    }
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    // Auch nach einem Dateidialog explizit mit der Engine-Wurzel starten.
    if (!CreateProcessW(exe.c_str(), args.data(), nullptr, nullptr, FALSE,
                        0, nullptr, cwd.c_str(), &si, &pi)) {
        data_prompt(g.main, L"Rune3D.exe konnte nicht gestartet werden. "
                            L"Zuerst build.ps1 ausführen.",
                    L"RuneEditor", MB_OK | MB_ICONERROR);
        return;
    }
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    core::log_info("Playtest gestartet: Projekt " + g.project_name +
                   (map_id.empty() ? " (Projekt-Startkarte)" : ", Karte " + map_id));
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
    if (core::win_project_cli::current_project(cli_project) == core::win_project_cli::ProjectArgument::Invalid) {
        std::fprintf(stderr, "FEHLER: Ungültiges oder fehlendes --project-Argument\n");
        return 1;
    }

    g.root = find_root();
    if (g.root.empty() || !core::win_path::set_current_directory(g.root)) {
        std::fprintf(stderr, "FEHLER: Engine-Wurzel nicht zugänglich\n");
        return 1;
    }
    CreateDirectoryW(L"log", nullptr);
    core::log_open("log\\editor.log");
    core::log_info("=== RuneEditor startet ===");
    core::log_info("Wurzel: " + g.root);

    // Etappe 19f: Daten-Check mit klarem Hinweis (siehe main.cpp). Ohne
    // game/-Ordner fehlt dem Editor boot.rb (Ruby-Szene), prefabs.json und
    // das Projekt dorf - die drei Folgefehler wirken sonst unerklärlich.
    if (!core::win_path::path_exists(g.root + "\\game")) {
        core::log_error(
            "Engine-Daten fehlen: kein game/-Ordner unter der Wurzel. "
            "Bitte den KOMPLETTEN Build entpacken (EXEs + game/ + projekte/ "
            "in einen Ordner) - einzelne EXEs finden ihre Daten nicht.");
    }

    // WIC (Bildimport) braucht COM; D3D und Ruby stören sich nicht daran.
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    // Kopfloser Import für Tests/Werkzeuge:
    //   RuneEditor.exe --import-tex <bild> [name]
    if (argc >= 2 && std::string(argv[1]) == "--import-tex") {
        int wide_argc = 0;
        wchar_t** wide_argv = CommandLineToArgvW(GetCommandLineW(), &wide_argc);
        bool valid = wide_argv && wide_argc >= 3;
        std::wstring project = L"dorf";
        std::string source, name;
        bool named = false;
        if (valid) valid = assets::to_utf8(wide_argv[2], source);
        for (int i = 3; valid && i < wide_argc; ++i) {
            const std::wstring arg = wide_argv[i];
            if (arg == L"--project") {
                if (++i >= wide_argc) { valid = false; break; }
                project = wide_argv[i];
            } else if (!named && arg.rfind(L"--", 0) != 0) {
                valid = assets::to_utf8(arg, name);
                named = true;
            } else valid = false;
        }
        if (wide_argv) LocalFree(wide_argv);
        valid = valid && assets::component(project);
        if (!valid) {
            std::fprintf(stderr, "FEHLER: --import-tex <bild> [name] [--project <projekt>]\n");
            CoUninitialize();
            core::log_close();
            return 1;
        }
        g_material_cli_project = project;
        assets::to_utf8(project, g.project_name);
        g.project_dir = g.root + "\\projekte\\" + g.project_name + "\\data";
        mat_load();
        const bool ok = mat_import_file(source, name);
        if (ok) std::printf("OK: Textur '%s' importiert\n", name.c_str());
        else    std::printf("FEHLER: Import fehlgeschlagen: %s\n", g_material_error.c_str());
        CoUninitialize();
        core::log_close();
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
    AppendMenuW(ev_menu, MF_STRING, ID_MENU_PLACEMENT, L"NPCs, Schilder und Objekte...\tF5");
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
    AppendMenuW(view_menu, MF_STRING, ID_MENU_MAPS_MORE, L"Kartenbereich vergrößern");
    AppendMenuW(view_menu, MF_STRING, ID_MENU_PALETTE_MORE, L"Palette vergrößern");
    AppendMenuW(view_menu, MF_STRING, ID_MENU_LAYOUT_RESET, L"Aufteilung zurücksetzen");
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

    const DWORD main_style = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN;
    RECT adj{ 0, 0, 1440, 880 };
    AdjustWindowRect(&adj, main_style, TRUE);
    g.main = CreateWindowExW(0, L"RuneEditorMain", L"RuneEditor",
                             main_style, CW_USEDEFAULT, CW_USEDEFAULT,
                             adj.right - adj.left, adj.bottom - adj.top,
                             nullptr, g.menu, hInst, nullptr);
    if (!g.main) return 1;
    main_set_dpi(GetDpiForWindow(g.main));

    // Startgröße in logischen Einheiten, aber niemals größer als die
    // Arbeitsfläche des Monitors, auf dem Windows das Fenster platziert hat.
    RECT frame{0, 0, main_layout_scale(1440, g.main_dpi), main_layout_scale(880, g.main_dpi)};
    if (!AdjustWindowRectExForDpi(&frame, main_style, TRUE, 0, g.main_dpi))
        AdjustWindowRectEx(&frame, main_style, TRUE, 0);
    RECT initial{};
    GetWindowRect(g.main, &initial);
    EventLayoutRect wanted{static_cast<int>(initial.left), static_cast<int>(initial.top),
        static_cast<int>(initial.left + frame.right - frame.left),
        static_cast<int>(initial.top + frame.bottom - frame.top)};
    EventLayoutRect work{};
    if (event_view_work_area(g.main, nullptr, work)) wanted = event_layout_fit(work, wanted);
    SetWindowPos(g.main, nullptr, wanted.left, wanted.top, wanted.right - wanted.left,
                 wanted.bottom - wanted.top, SWP_NOZORDER | SWP_NOACTIVATE);

    // Etappe 18a: eigenes Fenster-Icon (Runenstein, prozedural gezeichnet).
    set_window_icons(g.main, draw_icon_editor);

    // Klassische Werkzeugleiste und scrollbare Palette oberhalb des Kartenbaums.
    create_toolbar(g.main);
    if (!g_toolbar || !main_panel_create(g.palette, g.main, IDC_PANEL_PALETTE, g.main_dpi,
                                         main_palette_min_height, palette_place)) {
        MessageBoxW(g.main, L"Die Werkzeugpalette konnte nicht geöffnet werden.",
                    L"RuneEditor", MB_OK | MB_ICONERROR);
        DestroyWindow(g.main);
        return 1;
    }
    const HWND palette = g.palette.window;
    make_control(L"Button", L"Werkzeug (Modus)", BS_GROUPBOX,
                 0, 0, 1, 1, IDC_GRP_TOOL, palette);
    make_control(L"Button", L"Setzen", BS_AUTORADIOBUTTON | WS_GROUP | WS_TABSTOP,
                 0, 0, 1, 1, IDC_BTN_PLACE, palette);
    make_control(L"Button", L"Auswählen", BS_AUTORADIOBUTTON,
                 0, 0, 1, 1, IDC_BTN_SELECT, palette);
    make_control(L"Button", L"Verschieben", BS_AUTORADIOBUTTON,
                 0, 0, 1, 1, IDC_BTN_MOVE, palette);
    make_control(L"Button", L"Boden malen", BS_AUTORADIOBUTTON,
                 0, 0, 1, 1, IDC_BTN_PAINT, palette);
    make_control(L"Button", L"Höhe malen", BS_AUTORADIOBUTTON,
                 0, 0, 1, 1, IDC_BTN_HEIGHT, palette);
    make_control(L"Button", L"Objekt-Typ (Prefab)", BS_GROUPBOX | WS_GROUP,
                 0, 0, 1, 1, IDC_GRP_PREFAB, palette);
    g.list_prefab = make_control(L"ListBox", nullptr,
                                 WS_VSCROLL | WS_BORDER | WS_TABSTOP | LBS_NOTIFY | LBS_HASSTRINGS,
                                 0, 0, 1, 1, IDC_PREFAB_LIST, palette);
    make_control(L"Button", L"Objekt-Aktionen", BS_GROUPBOX,
                 0, 0, 1, 1, IDC_GRP_ACT, palette);
    make_control(L"Button", L"Drehen (R)", BS_PUSHBUTTON | WS_GROUP | WS_TABSTOP,
                 0, 0, 1, 1, IDC_BTN_ROTATE, palette);
    make_control(L"Button", L"Löschen", BS_PUSHBUTTON | WS_TABSTOP,
                 0, 0, 1, 1, IDC_BTN_DELETE, palette);
    make_control(L"Button", L"Duplizieren", BS_PUSHBUTTON | WS_TABSTOP,
                 0, 0, 1, 1, IDC_BTN_DUPLICATE, palette);
    make_control(L"Button", L"Undo", BS_PUSHBUTTON | WS_TABSTOP,
                 0, 0, 1, 1, IDC_BTN_UNDO, palette);
    make_control(L"Button", L"Redo", BS_PUSHBUTTON | WS_TABSTOP,
                 0, 0, 1, 1, IDC_BTN_REDO, palette);
    g.palette.ready = true;

    // Der Kartenbaum bleibt ein direktes Hauptfensterkind; alle bestehenden
    // Auswahl-, Umbenennungs- und Speicherprüfungen bleiben dadurch bestehen.
    make_control(L"Static", L"Karten", 0, 0, 0, 1, 1, IDC_LBL_MAPS);
    make_control(L"Button", L"Neu", BS_PUSHBUTTON | WS_TABSTOP,
                 0, 0, 1, 1, IDC_MAP_NEW_BUTTON);
    make_control(L"Button", L"Eigenschaften", BS_PUSHBUTTON | WS_TABSTOP,
                 0, 0, 1, 1, IDC_MAP_PROPS_BUTTON);
    g.map_tree = make_control(WC_TREEVIEWW, nullptr,
                              WS_BORDER | TVS_HASBUTTONS | TVS_HASLINES |
                              TVS_LINESATROOT | TVS_EDITLABELS |
                              TVS_SHOWSELALWAYS | WS_TABSTOP,
                              0, 0, 1, 1, IDC_MAPTREE);
    maptree_update_buttons();

    // Auch der Inspektor scrollt bei sehr niedrigen Fenstern, statt Liste und
    // Eingabefelder übereinander oder außerhalb des Fensters zu platzieren.
    if (!main_panel_create(g.inspector, g.main, IDC_PANEL_INSPECTOR, g.main_dpi,
                           main_inspector_min_height, inspector_place)) {
        MessageBoxW(g.main, L"Der Objektinspektor konnte nicht geöffnet werden.",
                    L"RuneEditor", MB_OK | MB_ICONERROR);
        DestroyWindow(g.main);
        return 1;
    }
    const HWND inspector = g.inspector.window;
    make_control(L"Static", L"Objekte", 0, 0, 0, 1, 1, IDC_LBL_OBJECTS, inspector);
    g.list_objs = make_control(WC_LISTVIEWW, nullptr,
                               WS_BORDER | LVS_REPORT | LVS_SINGLESEL |
                               LVS_SHOWSELALWAYS | WS_TABSTOP,
                               0, 0, 1, 1, IDC_OBJ_LIST, inspector);
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
            lc.cx = main_layout_scale(cols[c].breite, g.main_dpi);
            ListView_InsertColumn(g.list_objs, c, &lc);
        }
    }
    make_control(L"Button", L"Eigenschaften", BS_GROUPBOX,
                 0, 0, 1, 1, IDC_GRP_PROPS, inspector);
    make_control(L"Static", L"X", 0, 0, 0, 1, 1, IDC_LBL_X, inspector);
    g.ed_x = make_control(L"Edit", nullptr, WS_BORDER | WS_TABSTOP | ES_RIGHT | ES_AUTOHSCROLL,
                          0, 0, 1, 1, IDC_ED_X, inspector);
    make_control(L"Static", L"Z", 0, 0, 0, 1, 1, IDC_LBL_Z, inspector);
    g.ed_z = make_control(L"Edit", nullptr, WS_BORDER | WS_TABSTOP | ES_RIGHT | ES_AUTOHSCROLL,
                          0, 0, 1, 1, IDC_ED_Z, inspector);
    make_control(L"Static", L"Drehung", 0, 0, 0, 1, 1, IDC_LBL_ROT, inspector);
    g.ed_rot = make_control(L"Edit", nullptr, WS_BORDER | WS_TABSTOP | ES_RIGHT | ES_AUTOHSCROLL,
                            0, 0, 1, 1, IDC_ED_ROT, inspector);
    make_control(L"Static", L"Material", 0, 0, 0, 1, 1, IDC_LBL_TEX, inspector);
    g.ed_tex = make_control(L"Edit", nullptr, WS_BORDER | WS_TABSTOP | ES_LEFT | ES_AUTOHSCROLL,
                            0, 0, 1, 1, IDC_ED_TEX, inspector);
    for (HWND field : {g.ed_x, g.ed_z, g.ed_rot, g.ed_tex})
        SendMessageW(field, EM_SETLIMITTEXT, 1000000, 0);
    make_control(L"Button", L"Übernehmen", BS_PUSHBUTTON | WS_TABSTOP,
                 0, 0, 1, 1, IDC_BTN_APPLY, inspector);
    make_control(L"Static", L"Objekt auswählen.", 0,
                 0, 0, 1, 1, IDC_LBL_PROPS_HINT, inspector);
    g.inspector.ready = true;

    // Viewport + Statusleiste
    g.view = CreateWindowExW(WS_EX_CLIENTEDGE, L"RuneEditorView", nullptr,
                             WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                             0, 0, 1, 1, g.main,
                             reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_VIEWPORT)),
                             hInst, nullptr);
    g.status = CreateWindowExW(0, STATUSCLASSNAMEW, nullptr,
                               WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
                               0, 0, 0, 0, g.main,
                               reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_STATUSBAR)),
                               hInst, nullptr);
    if (!g.view || !g.status) {
        MessageBoxW(g.main, L"Die 3D-Arbeitsfläche konnte nicht geöffnet werden.",
                    L"RuneEditor", MB_OK | MB_ICONERROR);
        DestroyWindow(g.main);
        return 1;
    }
    apply_font(g.status);
    SendMessageW(g.list_prefab, LB_SETITEMHEIGHT, 0, main_layout_scale(22, g.main_dpi));
    layout_children(false); // vor Renderer::init auf die echte Clientgröße setzen

    // Renderer auf den Viewport
    static gfx::Renderer renderer;
    static world::Terrain terrain;
    RECT vr;
    GetClientRect(g.view, &vr);
    if (!renderer.init(g.view, std::max(1, static_cast<int>(vr.right)),
                       std::max(1, static_cast<int>(vr.bottom)))) {
        core::log_error("Renderer konnte nicht initialisiert werden");
        MessageBoxW(g.main, L"3D-Renderer konnte nicht gestartet werden.",
                    L"RuneEditor", MB_OK | MB_ICONERROR);
        return 1;
    }
    g.renderer = &renderer;
    g.terrain = &terrain;
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
        MessageBoxW(g.main, L"Die Ruby-Laufzeit konnte nicht gestartet werden. "
                            L"Der Editor läuft ohne Ruby-Texturen und Sounds weiter.\n\n"
                            L"Bitte log\\editor.log prüfen und den vollständigen Build erneut "
                            L"erstellen oder entpacken.",
                    L"RuneEditor: Ruby-Start", MB_OK | MB_ICONWARNING);
    } else if (!rune::ruby::load_script("game\\editor\\boot.rb")) {
        core::log_error("Editor-Texturen und Sounds konnten nicht vollständig registriert werden");
        MessageBoxW(g.main, L"Die Ruby-Skripte für Editor-Texturen und Sounds konnten nicht "
                            L"vollständig geladen werden. Der Editor läuft ohne diese Inhalte weiter.\n\n"
                            L"Bitte log\\editor.log prüfen und sicherstellen, dass die zur EXE "
                            L"gehörige Standardbibliothek neben ihr unter lib\\ruby vorhanden ist.",
                    L"RuneEditor: Ruby-Start", MB_OK | MB_ICONWARNING);
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
                std::wstring checked;
                if (!last.empty() && core::win_path::to_wide(last, checked)) startup = last;
                else if (!last.empty())
                    core::log_warn("Alter Projektmerker ist kein UTF-8; Projekt erneut auswählen");
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
    core::log_info("Editor-Hauptschleife läuft");

    // ------------------------------------------------------ Hauptschleife
    double last = qpc_now();
    double fps_accum = 0.0;
    int fps_frames = 0;
    bool running = true;
    while (running) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) { running = false; break; }
            // Auch moduslose Dialoge brauchen Tab/Esc/Enter; Mehrzeiler
            // behalten Enter dank ES_WANTRETURN als Zeilenumbruch.
            if (g.props_dlg && IsDialogMessageW(g.props_dlg, &msg)) continue;
            if (event_editor_open() && IsDialogMessageW(g_ev.win, &msg)) {
                event_view_reveal_new_focus(g_ev.view, g_ev.win);
                continue;
            }
            const HWND old_focus = GetFocus();
            const bool main_child = msg.hwnd == g.main || IsChild(g.main, msg.hwnd);
            const int focused_id = old_focus ? GetDlgCtrlID(old_focus) : 0;
            const bool radio_arrow = focused_id == IDC_BTN_PLACE || focused_id == IDC_BTN_SELECT ||
                                     focused_id == IDC_BTN_MOVE || focused_id == IDC_BTN_PAINT ||
                                     focused_id == IDC_BTN_HEIGHT;
            const bool navigation = msg.message == WM_KEYDOWN &&
                (msg.wParam == VK_TAB || (radio_arrow &&
                 (msg.wParam == VK_UP || msg.wParam == VK_DOWN ||
                  msg.wParam == VK_LEFT || msg.wParam == VK_RIGHT)));
            if (!g.props_dlg && !g.data_prompt_active && main_child && navigation &&
                IsDialogMessageW(g.main, &msg)) {
                main_panel_reveal(g.palette, GetFocus());
                main_panel_reveal(g.inspector, GetFocus());
                continue;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            if (GetFocus() != old_focus) {
                main_panel_reveal(g.palette, GetFocus());
                main_panel_reveal(g.inspector, GetFocus());
            }
            if (event_editor_open()) event_view_reveal_new_focus(g_ev.view, g_ev.win);
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
