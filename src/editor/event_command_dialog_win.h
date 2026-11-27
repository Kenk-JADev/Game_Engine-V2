#pragma once
// ============================================================================
//  Etappe 44: XP-Befehlsauswahl fuer F6 — drei Registerkarten wie RPG Maker XP.
//
//  Der Dialog zeigt die 90 Befehle der Rpg-Schicht in drei Tabs, je zwei
//  Spalten und zwei Abschnitten (Bauplan: event_command_catalog.h, Geometrie:
//  event_command_layout.h). Ein Klick waehlt den Befehl; OK uebernimmt ihn in
//  das Befehlstyp-Feld des Event-Editors. Befehle ohne bestehenden cmd-String
//  sind als "vorbereitet" gekennzeichnet (die Rpg-Schicht fuehrt sie, das
//  Karten-Event-System noch nicht — siehe docs/ETAPPE_43_ENGINE_PORTS.md).
//
//  Diese Datei wird in editor_main.cpp eingebunden, NACHDEM ui_px,
//  utf8_to_wide, ui_dialog_font, g_ev und event_layout_center definiert sind.
//  Sicht-/DPI-Test nur auf Windows; die Geometrie ist kopflos geprueft
//  (tools/event_command_catalog_test.cpp).
// ============================================================================

#include "editor/event_command_catalog.h"
#include "editor/event_command_layout.h"


enum {
    IDEC_TABS     = 8200,
    IDEC_ROW_BASE = 8210,   // + Befehlsindex (0..89)
    IDEC_OK       = 8310,
    IDEC_CANCEL   = 8311,
    IDEC_HINT     = 8312,
    IDEC_TITLE_BASE = 8320, // + Abschnitt (0..3)
};

struct EcDlgState {
    HWND win = nullptr;
    HWND tabs = nullptr;
    HWND hint = nullptr;
    HWND ok = nullptr;
    HWND row[EC_COMMAND_COUNT] = {};
    HWND title[4] = {};
    int tab = 1;          // 1..3
    int selected = -1;    // Befehlsindex
};
static EcDlgState g_ec;

static HWND ec_control(const wchar_t* cls, const wchar_t* text, DWORD style,
                       int x, int y, int w, int h, int id) {
    HWND c = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style,
                             ui_px(x), ui_px(y), ui_px(w), ui_px(h), g_ec.win,
                             reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                             GetModuleHandleW(nullptr), nullptr);
    SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(ui_dialog_font()), TRUE);
    return c;
}

static void ec_hint(const std::string& text) {
    if (g_ec.hint) SetWindowTextW(g_ec.hint, utf8_to_wide(text).c_str());
}

// Befehlszeilen und Abschnittsüberschriften des aktuellen Tabs aufbauen.
static void ec_build_tab() {
    for (int i = 0; i < EC_COMMAND_COUNT; ++i) {
        if (g_ec.row[i]) { DestroyWindow(g_ec.row[i]); g_ec.row[i] = nullptr; }
    }
    for (int i = 0; i < 4; ++i) {
        if (g_ec.title[i]) { DestroyWindow(g_ec.title[i]); g_ec.title[i] = nullptr; }
    }
    const EcDialogLayout layout = ec_dialog_layout(g_ec.tab);
    int title_index = 0;
    for (int col = 0; col < EC_COLUMN_COUNT; ++col) {
        const EcColumnLayout& cl = layout.column[col];
        for (int s = 0; s < 2; ++s) {
            // Abschnittsüberschrift aus dem Katalog suchen.
            const EcSectionInfo* info = nullptr;
            int seen = 0;
            for (int i = 0; i < EC_SECTION_COUNT; ++i) {
                if (EC_SECTIONS[i].tab == g_ec.tab && EC_SECTIONS[i].column == col) {
                    if (seen == s) { info = &EC_SECTIONS[i]; break; }
                    ++seen;
                }
            }
            const std::wstring title = info ? utf8_to_wide(info->title) : L"";
            g_ec.title[title_index] = ec_control(L"Static", title.c_str(), 0,
                cl.title[s].x, cl.title[s].y, cl.title[s].w, cl.title[s].h,
                IDEC_TITLE_BASE + title_index);
            ++title_index;
        }
        for (int r = 0; r < cl.rows; ++r) {
            // Befehlsindex über Abschnitt und Position auflösen.
            int indices[EC_MAX_ROWS_PER_COLUMN];
            const EcSectionInfo* info = nullptr;
            int seen = 0;
            for (int i = 0; i < EC_SECTION_COUNT; ++i) {
                if (EC_SECTIONS[i].tab == g_ec.tab && EC_SECTIONS[i].column == col) {
                    if (seen == cl.row_section[r]) { info = &EC_SECTIONS[i]; break; }
                    ++seen;
                }
            }
            if (!info) continue;
            // Wievielte Zeile in diesem Abschnitt?
            int within = 0;
            for (int k = 0; k < r; ++k)
                if (cl.row_section[k] == cl.row_section[r]) ++within;
            int n = ec_section_commands(g_ec.tab, col, info->key, indices, EC_MAX_ROWS_PER_COLUMN);
            if (within >= n) continue;
            const int cmd_index = indices[within];
            const EcCommand& c = ec_command(cmd_index);
            std::wstring label = utf8_to_wide(c.label);
            if (!c.cmd) label += L"  (vorbereitet)";
            g_ec.row[cmd_index] = ec_control(L"Button", label.c_str(),
                BS_PUSHBUTTON | BS_LEFT | WS_TABSTOP,
                cl.row[r].x, cl.row[r].y, cl.row[r].w, cl.row[r].h,
                IDEC_ROW_BASE + cmd_index);
        }
    }
    EnableWindow(g_ec.ok, g_ec.selected >= 0 ? TRUE : FALSE);
}

static void ec_select(int index) {
    g_ec.selected = index;
    if (index < 0 || index >= EC_COMMAND_COUNT) { ec_hint("Kein Befehl gewählt."); return; }
    const EcCommand& c = ec_command(index);
    std::string text = std::to_string(c.code) + "  " + c.label + "  (" + c.xp_name + ")";
    text += c.cmd ? std::string("  —  cmd: ") + c.cmd
                  : std::string("  —  vorbereitet: die Rpg-Schicht führt ihn, das Karten-Event noch nicht.");
    ec_hint(text);
    EnableWindow(g_ec.ok, TRUE);
}

// Übernahme in das Befehlstyp-Feld des Event-Editors.
static void ec_apply() {
    if (g_ec.selected < 0) return;
    const EcCommand& c = ec_command(g_ec.selected);
    if (!c.cmd) {
        MessageBoxW(g_ec.win,
            L"Dieser Befehl ist in der Rpg-Schicht vorbereitet, aber das Karten-Event-System\n"
            L"führt ihn noch nicht. Er kann deshalb nicht in eine Karte eingefügt werden.\n"
            L"Siehe docs/ETAPPE_43_ENGINE_PORTS.md (Abschnitt „Ehrliche Grenzen“).",
            L"Befehl vorbereitet", MB_OK | MB_ICONINFORMATION);
        return;
    }
    if (g_ev.cb_ce_type) {
        for (int i = 0; i < ET_COUNT; ++i) {
            if (std::strcmp(EV_TYPES[i], c.cmd) == 0) {
                SendMessageW(g_ev.cb_ce_type, CB_SETCURSEL, i, 0);
                SendMessageW(g_ev.win, WM_COMMAND,
                             MAKEWPARAM(IDEV_CETYPE, CBN_SELCHANGE),
                             reinterpret_cast<LPARAM>(g_ev.cb_ce_type));
                break;
            }
        }
    }
    DestroyWindow(g_ec.win);
}

static LRESULT CALLBACK ec_proc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_NOTIFY: {
            const NMHDR* nh = reinterpret_cast<const NMHDR*>(lp);
            if (nh && nh->idFrom == IDEC_TABS && nh->code == TCN_SELCHANGE) {
                g_ec.tab = TabCtrl_GetCurSel(g_ec.tabs) + 1;
                ec_build_tab();
                return 0;
            }
            return 0;
        }
        case WM_COMMAND: {
            const int id = LOWORD(wp);
            const int code = HIWORD(wp);
            if (id >= IDEC_ROW_BASE && id < IDEC_ROW_BASE + EC_COMMAND_COUNT && code == BN_CLICKED) {
                ec_select(id - IDEC_ROW_BASE);
                return 0;
            }
            if (code != BN_CLICKED) return 0;
            if (id == IDEC_OK)     { ec_apply(); return 0; }
            if (id == IDEC_CANCEL) { DestroyWindow(h); return 0; }
            return 0;
        }
        case WM_CLOSE:
            DestroyWindow(h);
            return 0;
        case WM_DESTROY:
            if (g_ec.win == h) {
                g_ec.win = nullptr;
                g_ec.tabs = nullptr;
                g_ec.hint = nullptr;
                g_ec.ok = nullptr;
                for (int i = 0; i < EC_COMMAND_COUNT; ++i) g_ec.row[i] = nullptr;
                for (int i = 0; i < 4; ++i) g_ec.title[i] = nullptr;
                g_ec.selected = -1;
            }
            return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

void open_event_command_picker() {
    if (g_ec.win) { SetForegroundWindow(g_ec.win); return; }

    WNDCLASSW wc{};
    wc.lpfnWndProc = ec_proc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"RuneEditorEcDlg";
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
    RegisterClassW(&wc);

    EventLayoutRect work{};
    EventLayoutRect owner{};
    RECT mr;
    GetWindowRect(g.main, &mr);
    owner = { mr.left, mr.top, mr.right, mr.bottom };
    HMONITOR monitor = MonitorFromWindow(g.main, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{ sizeof(mi), {}, {}, 0 };
    if (monitor && GetMonitorInfoW(monitor, &mi))
        work = { mi.rcWork.left, mi.rcWork.top, mi.rcWork.right, mi.rcWork.bottom };
    else
        work = owner;
    const EventLayoutRect fit = event_layout_center(work, owner,
        ui_px(EC_DIALOG_WIDTH), ui_px(EC_DIALOG_HEIGHT));

    g_ec.win = CreateWindowExW(WS_EX_DLGMODALFRAME, L"RuneEditorEcDlg",
                               L"Befehl wählen - Rune3D (drei Tabs wie RPG Maker XP)",
                               WS_POPUP | WS_CAPTION | WS_SYSMENU,
                               fit.left, fit.top, fit.right - fit.left, fit.bottom - fit.top,
                               g.main, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!g_ec.win) return;

    g_ec.tab = 1;
    g_ec.selected = -1;
    const EcDialogLayout layout = ec_dialog_layout(g_ec.tab);
    // TCS_MULTILINE bricht die langen Registerbeschriftungen bei wenig Breite
    // (oder hoher DPI) in mehrere Zeilen um, statt sie abzuschneiden.
    g_ec.tabs = ec_control(WC_TABCONTROLW, nullptr, WS_CLIPSIBLINGS | TCS_MULTILINE | TCS_FOCUSNEVER,
                           layout.tabs.x, layout.tabs.y, layout.tabs.w, layout.tabs.h, IDEC_TABS);
    for (int i = 0; i < EC_TAB_COUNT; ++i) {
        const std::wstring label = utf8_to_wide(ec_tab(i + 1).title);
        TCITEMW item{};
        item.mask = TCIF_TEXT;
        item.pszText = const_cast<wchar_t*>(label.c_str());
        TabCtrl_InsertItem(g_ec.tabs, i, &item);
    }
    g_ec.hint = ec_control(L"Static", L"Kein Befehl gewählt.", 0,
                           layout.hint.x, layout.hint.y, layout.hint.w, layout.hint.h, IDEC_HINT);
    g_ec.ok = ec_control(L"Button", L"Übernehmen", BS_PUSHBUTTON | WS_TABSTOP,
                         layout.ok.x, layout.ok.y, layout.ok.w, layout.ok.h, IDEC_OK);
    ec_control(L"Button", L"Abbrechen", BS_PUSHBUTTON | WS_TABSTOP,
               layout.cancel.x, layout.cancel.y, layout.cancel.w, layout.cancel.h, IDEC_CANCEL);
    ec_build_tab();
    ShowWindow(g_ec.win, SW_SHOW);
    SetForegroundWindow(g_ec.win);
}
