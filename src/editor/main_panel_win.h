#pragma once
// Scrollbare Seitenleisten des Hauptfensters. Die Controls bleiben echte
// native Win32-Kinder; Benachrichtigungen werden unverändert an das
// Hauptfenster weitergereicht. Der Ausschnitt begrenzt auch ganz verborgene
// Controls, ohne ihre Texte, Auswahl oder JSON-Daten anzufassen.
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "editor/main_layout.h"

#include <algorithm>
#include <cstdint>

namespace rune::editor {

struct MainPanel;
using MainPanelMinimum = int (*)(unsigned dpi);
using MainPanelPlacement = void (*)(MainPanel& panel, int width, int content_height);

struct MainPanel {
    HWND window = nullptr;
    unsigned dpi = 96;
    int y = 0, wheel = 0, content_height = 0;
    bool ready = false, busy = false;
    MainPanelMinimum minimum = nullptr;
    MainPanelPlacement place = nullptr;
};

inline void main_panel_update(MainPanel& panel);

inline void main_panel_place(MainPanel& panel, HWND child, MainRect rect) {
    if (!child) return;
    SetWindowPos(child, nullptr, rect.x, rect.y - panel.y, rect.w, rect.h,
                 SWP_NOZORDER | SWP_NOACTIVATE);
}

inline void main_panel_size(HWND window, int& width, int& height) {
    RECT rect{};
    GetClientRect(window, &rect);
    width = std::max(0, static_cast<int>(rect.right - rect.left));
    height = std::max(0, static_cast<int>(rect.bottom - rect.top));
}

inline void main_panel_update(MainPanel& panel) {
    if (!panel.window || !panel.ready || panel.busy || !panel.minimum || !panel.place) return;
    panel.busy = true;
    int width = 0, height = 0;
    main_panel_size(panel.window, width, height);
    panel.content_height = std::max(height, panel.minimum(panel.dpi));
    const bool overflow = panel.content_height > height;
    // Die Leiste wird nur bei Bedarf gezeigt. WM_SIZE, das ShowScrollBar dabei
    // auslösen kann, darf nicht rekursiv in dieselbe Layoutberechnung laufen.
    ShowScrollBar(panel.window, SB_VERT, overflow ? TRUE : FALSE);
    main_panel_size(panel.window, width, height);
    panel.content_height = std::max(height, panel.minimum(panel.dpi));
    panel.y = event_scroll_clamp(panel.content_height, std::max(1, height), panel.y);
    SCROLLINFO info{};
    info.cbSize = sizeof(info);
    // Ohne SIF_DISABLENOSCROLL verschwindet die Leiste wieder, sobald der
    // Inhalt hineinpasst, statt dauerhaft deaktiviert Platz zu belegen.
    info.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    info.nMin = 0;
    info.nMax = std::max(0, panel.content_height - 1);
    info.nPage = static_cast<UINT>(std::max(1, height));
    info.nPos = panel.y;
    SetScrollInfo(panel.window, SB_VERT, &info, TRUE);
    panel.place(panel, width, panel.content_height);
    RedrawWindow(panel.window, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
    panel.busy = false;
}

inline void main_panel_scroll_to(MainPanel& panel, int position) {
    if (!panel.window || !panel.ready || panel.busy) return;
    int width = 0, height = 0;
    main_panel_size(panel.window, width, height);
    const int next = event_scroll_clamp(panel.content_height, std::max(1, height), position);
    if (next == panel.y) return;
    panel.y = next;
    main_panel_update(panel);
}

inline void main_panel_scrollbar(MainPanel& panel, WPARAM wp) {
    int width = 0, height = 0;
    main_panel_size(panel.window, width, height);
    const int page = std::max(1, height);
    const int line = std::max(1, main_layout_scale(32, panel.dpi));
    int next = panel.y;
    switch (LOWORD(wp)) {
        case SB_TOP: next = 0; break;
        case SB_BOTTOM: next = event_scroll_max(panel.content_height, page); break;
        case SB_LINEUP: next = event_scroll_move(panel.content_height, page, panel.y, -line); break;
        case SB_LINEDOWN: next = event_scroll_move(panel.content_height, page, panel.y, line); break;
        case SB_PAGEUP: next = event_scroll_move(panel.content_height, page, panel.y, -page); break;
        case SB_PAGEDOWN: next = event_scroll_move(panel.content_height, page, panel.y, page); break;
        case SB_THUMBPOSITION:
        case SB_THUMBTRACK: {
            SCROLLINFO info{};
            info.cbSize = sizeof(info);
            info.fMask = SIF_TRACKPOS;
            if (GetScrollInfo(panel.window, SB_VERT, &info)) next = info.nTrackPos;
            break;
        }
        default: return;
    }
    main_panel_scroll_to(panel, next);
}

inline void main_panel_wheel(MainPanel& panel, WPARAM wp) {
    const int total = panel.wheel + static_cast<short>(HIWORD(wp));
    const int detents = total / WHEEL_DELTA;
    panel.wheel = total % WHEEL_DELTA;
    if (!detents) return;
    int width = 0, height = 0;
    main_panel_size(panel.window, width, height);
    UINT lines = 3;
    if (!SystemParametersInfoW(SPI_GETWHEELSCROLLLINES, 0, &lines, 0)) lines = 3;
    if (!lines) return;
    const std::int64_t line = std::max(1, main_layout_scale(32, panel.dpi));
    const std::int64_t distance = lines == WHEEL_PAGESCROLL ? std::max(1, height)
        : std::min(static_cast<std::int64_t>(panel.content_height), line * lines);
    const int next = event_scroll_move(panel.content_height, std::max(1, height), panel.y,
                                       -static_cast<std::int64_t>(detents) * distance);
    main_panel_scroll_to(panel, next);
}

// Ein neu fokussiertes Feld bei Bedarf in den Ausschnitt holen. Ein großes,
// teilweise sichtbares Listenfeld bleibt stabil (event_scroll_reveal).
inline void main_panel_reveal(MainPanel& panel, HWND focus) {
    if (!panel.window || !panel.ready || !focus || !IsChild(panel.window, focus)) return;
    HWND child = focus;
    while (GetParent(child) && GetParent(child) != panel.window) child = GetParent(child);
    if (GetParent(child) != panel.window) return;
    RECT rect{};
    if (!GetWindowRect(child, &rect)) return;
    MapWindowPoints(nullptr, panel.window, reinterpret_cast<POINT*>(&rect), 2);
    int width = 0, height = 0;
    main_panel_size(panel.window, width, height);
    const int top = event_layout_int(static_cast<std::int64_t>(rect.top) + panel.y);
    const int bottom = event_layout_int(static_cast<std::int64_t>(rect.bottom) + panel.y);
    main_panel_scroll_to(panel, event_scroll_reveal(panel.content_height, std::max(1, height),
        panel.y, top, bottom, main_layout_scale(6, panel.dpi)));
}

inline void main_panel_set_dpi(MainPanel& panel, unsigned dpi) {
    const unsigned next = main_layout_dpi(dpi);
    panel.y = event_layout_rescale(panel.y, panel.dpi, next);
    panel.dpi = next;
    panel.wheel = 0;
}

inline LRESULT CALLBACK main_panel_proc(HWND window, UINT msg, WPARAM wp, LPARAM lp) {
    auto* panel = reinterpret_cast<MainPanel*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (msg == WM_NCCREATE) {
        const CREATESTRUCTW* create = reinterpret_cast<const CREATESTRUCTW*>(lp);
        panel = static_cast<MainPanel*>(create->lpCreateParams);
        if (!panel) return FALSE;
        panel->window = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(panel));
    }
    if (panel) {
        switch (msg) {
            case WM_SIZE:
                main_panel_update(*panel);
                return 0;
            case WM_VSCROLL:
                // Meldungen von Kind-Scrollbars gehören nicht zur Panel-Leiste.
                if (!lp) { main_panel_scrollbar(*panel, wp); return 0; }
                break;
            case WM_MOUSEWHEEL:
                main_panel_wheel(*panel, wp);
                return 0;
            case WM_COMMAND:
            case WM_NOTIFY:
            case WM_CONTEXTMENU:
                return SendMessageW(GetParent(window), msg, wp, lp);
            case WM_NCDESTROY:
                panel->window = nullptr;
                panel->ready = false;
                SetWindowLongPtrW(window, GWLP_USERDATA, 0);
                break;
            default: break;
        }
    }
    return DefWindowProcW(window, msg, wp, lp);
}

inline bool main_panel_create(MainPanel& panel, HWND parent, int id, unsigned dpi,
                              MainPanelMinimum minimum, MainPanelPlacement place) {
    const HINSTANCE instance = GetModuleHandleW(nullptr);
    WNDCLASSW wc{};
    wc.lpfnWndProc = main_panel_proc;
    wc.hInstance = instance;
    wc.lpszClassName = L"RuneEditorScrollPanel";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
    if (!RegisterClassW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;
    panel.dpi = main_layout_dpi(dpi);
    panel.minimum = minimum;
    panel.place = place;
    // Etappe 32: KEIN WS_CLIPCHILDREN. Gruppenrahmen (BS_GROUPBOX) löschen
    // ihren Innenraum nicht selbst; mit WS_CLIPCHILDREN löschte ihn auch das
    // Panel nicht. Nach Verschieben/Scrollen blieben alte Pixel stehen
    // (Geistertexte wie „Material“/„Redo“, schwarze Streifen rechts).
    const HWND window = CreateWindowExW(WS_EX_CONTROLPARENT, wc.lpszClassName, nullptr,
        WS_CHILD | WS_VISIBLE | WS_VSCROLL, 0, 0, 1, 1, parent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance, &panel);
    return window != nullptr;
}

} // namespace rune::editor
#endif // _WIN32
