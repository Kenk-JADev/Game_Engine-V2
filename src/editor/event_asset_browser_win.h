#pragma once
// ============================================================================
//  Etappe 45: Asset-Ordnerwahl im Editor (Audio / Bilder).
//
//  Listet die Dateien eines Projekt-Asset-Ordners (audio\bgm, audio\se,
//  pictures, ...), gefiltert nach Endung (ec_asset_matches). Über "Ordner
//  wählen…" (SHBrowseForFolderW, shell32) kann ein beliebiger Ordner gewählt
//  werden; eine dort gewählte Datei wird in den Projekt-Asset-Ordner KOPIERT,
//  damit das Projekt in sich geschlossen bleibt. Zurück kommt der relative
//  Schlüssel (z. B. "audio/bgm/dorf.ogg"), den der Befehl speichert.
//
//  Modal über eine eigene Nachrichtenschleife mit deaktiviertem Besitzer.
//  Eingebunden in editor_main.cpp innerhalb namespace rune::editor, nachdem
//  ui_px/utf8_to_wide/g/file_path_to_wide definiert sind.
// ============================================================================

#include "editor/event_command_settings.h"

#include <shlobj.h>

enum {
    IDEAB_LIST   = 8400,
    IDEAB_DIR    = 8401,
    IDEAB_BROWSE = 8402,
    IDEAB_OK     = 8403,
    IDEAB_CANCEL = 8404,
};

struct EcBrowseState {
    HWND win = nullptr, list = nullptr, lbl_dir = nullptr;
    std::string collection, dir, result;
    bool done = false, ok = false;
};
static EcBrowseState g_eab;

static HWND eab_control(const wchar_t* cls, const wchar_t* text, DWORD style,
                        int x, int y, int w, int h, int id) {
    HWND c = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style,
                             ui_px(x), ui_px(y), ui_px(w), ui_px(h), g_eab.win,
                             reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                             GetModuleHandleW(nullptr), nullptr);
    SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(ui_dialog_font()), TRUE);
    return c;
}

static void eab_relist() {
    SendMessageW(g_eab.list, LB_RESETCONTENT, 0, 0);
    std::wstring pattern;
    if (!file_path_to_wide(g_eab.dir + "\\*.*", pattern)) return;
    WIN32_FIND_DATAW fd{};
    HANDLE find = FindFirstFileW(pattern.c_str(), &fd);
    if (find == INVALID_HANDLE_VALUE) return;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        std::string name;
        if (!core::win_path::to_utf8(fd.cFileName, name)) continue;
        if (!ec_asset_matches(g_eab.collection.c_str(), name)) continue;
        SendMessageW(g_eab.list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(utf8_to_wide(name).c_str()));
    } while (FindNextFileW(find, &fd));
    FindClose(find);
}

static void eab_set_dir(const std::string& dir) {
    g_eab.dir = dir;
    if (g_eab.lbl_dir) SetWindowTextW(g_eab.lbl_dir, utf8_to_wide(dir).c_str());
    eab_relist();
}

static void eab_browse_folder() {
    BROWSEINFOW bi{};
    bi.hwndOwner = g_eab.win;
    bi.lpszTitle = L"Asset-Ordner wählen";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    LPITEMIDLIST id = SHBrowseForFolderW(&bi);
    if (!id) return;
    wchar_t path[MAX_PATH] = {};
    if (SHGetPathFromIDListW(id, path)) {
        std::string utf8;
        if (core::win_path::to_utf8(path, utf8)) eab_set_dir(utf8);
    }
    CoTaskMemFree(id);
}

static void eab_accept() {
    const int sel = static_cast<int>(SendMessageW(g_eab.list, LB_GETCURSEL, 0, 0));
    if (sel < 0) {
        MessageBoxW(g_eab.win, L"Bitte eine Datei in der Liste auswählen.", L"Asset wählen", MB_OK | MB_ICONINFORMATION);
        return;
    }
    wchar_t name[MAX_PATH] = {};
    SendMessageW(g_eab.list, LB_GETTEXT, sel, reinterpret_cast<LPARAM>(name));
    std::string file;
    if (!core::win_path::to_utf8(name, file)) return;

    // Projekt-Asset-Ordner sicherstellen; externe Datei hineinkopieren.
    const std::string sub = ec_asset_subdir(g_eab.collection.c_str());
    const std::string target_dir = g.project_dir + "\\" + sub;
    std::wstring wtarget_dir, wsource, wtarget;
    if (!file_path_to_wide(target_dir, wtarget_dir)) return;
    core::win_path::create_directory(target_dir);
    if (!file_path_to_wide(g_eab.dir + "\\" + file, wsource) ||
        !file_path_to_wide(target_dir + "\\" + file, wtarget)) return;
    // Gleicher Ordner: keine Kopie nötig. Sonst kopieren (nicht überschreiben).
    wchar_t src_full[MAX_PATH] = {}, dst_full[MAX_PATH] = {};
    GetFullPathNameW(wsource.c_str(), MAX_PATH, src_full, nullptr);
    GetFullPathNameW(wtarget.c_str(), MAX_PATH, dst_full, nullptr);
    if (_wcsicmp(src_full, dst_full) != 0) {
        if (!CopyFileW(wsource.c_str(), wtarget.c_str(), TRUE)) {
            MessageBoxW(g_eab.win,
                L"Die Datei konnte nicht in den Projekt-Asset-Ordner kopiert werden\n"
                L"(Ziel existiert bereits oder ist schreibgeschützt).",
                L"Asset wählen", MB_OK | MB_ICONERROR);
            return;
        }
    }
    g_eab.result = ec_asset_key(g_eab.collection.c_str(), file);
    g_eab.ok = true;
    g_eab.done = true;
}

static LRESULT CALLBACK eab_proc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_COMMAND: {
            const int id = LOWORD(wp), code = HIWORD(wp);
            if (id == IDEAB_LIST && code == LBN_DBLCLK) { eab_accept(); return 0; }
            if (code != BN_CLICKED) return 0;
            if (id == IDEAB_BROWSE) { eab_browse_folder(); return 0; }
            if (id == IDEAB_OK)     { eab_accept(); return 0; }
            if (id == IDEAB_CANCEL) { g_eab.done = true; return 0; }
            return 0;
        }
        case WM_CLOSE:
            g_eab.done = true;
            return 0;
        case WM_DESTROY:
            if (g_eab.win == h) g_eab.win = nullptr;
            return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

// Liefert true und out_key, wenn der Nutzer eine Datei gewählt hat.
static bool ec_browse_asset(HWND owner, const char* collection, std::string& out_key) {
    WNDCLASSW wc{};
    wc.lpfnWndProc = eab_proc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"RuneEditorAssetDlg";
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
    RegisterClassW(&wc);

    g_eab = EcBrowseState{};
    g_eab.collection = collection ? collection : "";
    const std::string sub = ec_asset_subdir(collection);
    g_eab.dir = g.project_dir + "\\" + sub;

    g_eab.win = CreateWindowExW(WS_EX_DLGMODALFRAME, L"RuneEditorAssetDlg",
        L"Asset wählen - Rune3D", WS_POPUP | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, ui_px(520), ui_px(420),
        owner, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!g_eab.win) return false;

    g_eab.lbl_dir = eab_control(L"Static", utf8_to_wide(g_eab.dir).c_str(), 0, 12, 10, 400, 18, 0);
    eab_control(L"Button", L"Ordner wählen…", BS_PUSHBUTTON, 420, 8, 88, 24, IDEAB_BROWSE);
    g_eab.list = eab_control(L"ListBox", nullptr, WS_BORDER | WS_VSCROLL | LBS_NOTIFY | LBS_HASSTRINGS,
                             12, 36, 496, 300, IDEAB_LIST);
    eab_control(L"Button", L"Übernehmen", BS_PUSHBUTTON, 320, 344, 90, 26, IDEAB_OK);
    eab_control(L"Button", L"Abbrechen", BS_PUSHBUTTON, 418, 344, 90, 26, IDEAB_CANCEL);
    eab_relist();

    EnableWindow(owner, FALSE);
    ShowWindow(g_eab.win, SW_SHOW);
    SetForegroundWindow(g_eab.win);
    MSG m{};
    while (!g_eab.done && GetMessageW(&m, nullptr, 0, 0) > 0) {
        if (!g_eab.win || !IsDialogMessageW(g_eab.win, &m)) {
            TranslateMessage(&m);
            DispatchMessageW(&m);
        }
    }
    if (g_eab.win) DestroyWindow(g_eab.win);
    EnableWindow(owner, TRUE);
    SetForegroundWindow(owner);
    if (g_eab.ok) out_key = g_eab.result;
    return g_eab.ok;
}
