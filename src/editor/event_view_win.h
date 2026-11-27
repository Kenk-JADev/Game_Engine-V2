#pragma once
// Win32-Ausschnitt für das F6-Formular. Die Kinder bleiben direkte Kinder des
// Dialogs: WM_COMMAND, GetDlgItem und IsDialogMessage behalten ihr Verhalten.
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "editor/event_layout.h"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace rune::editor {

struct EventViewControl {
    HWND window = nullptr;
    int x = 0, y = 0, width = 0, height = 0; // 96-DPI-Koordinaten
};

struct EventViewState {
    unsigned dpi = EVENT_LAYOUT_DPI;
    int x = 0, y = 0;
    int wheel_x = 0, wheel_y = 0;
    HFONT font = nullptr;                 // eigene DPI-Schrift, falls erzeugt
    HFONT fallback_font = nullptr;        // gehört weiterhin dem Editor
    HWND last_focus = nullptr;
    bool ready = false, busy = false;
    std::vector<EventViewControl> controls;
};

inline EventLayoutRect event_view_rect(const RECT& r) {
    return {static_cast<int>(r.left), static_cast<int>(r.top),
            static_cast<int>(r.right), static_cast<int>(r.bottom)};
}

inline bool event_view_work_area(HWND window, const RECT* near_rect, EventLayoutRect& work) {
    const HMONITOR monitor = near_rect ? MonitorFromRect(near_rect, MONITOR_DEFAULTTONEAREST)
                                       : MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    if (monitor && GetMonitorInfoW(monitor, &info)) {
        work = event_view_rect(info.rcWork);
        return true;
    }
    RECT primary{};
    if (SystemParametersInfoW(SPI_GETWORKAREA, 0, &primary, 0)) {
        work = event_view_rect(primary);
        return true;
    }
    return false;
}

inline EventLayoutRect event_view_initial_rect(HWND owner, DWORD style, DWORD ex_style, unsigned dpi) {
    RECT frame{0, 0, event_layout_scale(EVENT_LAYOUT_WIDTH, dpi),
                    event_layout_scale(EVENT_LAYOUT_HEIGHT, dpi)};
    if (!AdjustWindowRectExForDpi(&frame, style, FALSE, ex_style, dpi))
        AdjustWindowRectEx(&frame, style, FALSE, ex_style);
    // AdjustWindowRectExForDpi berücksichtigt die Fenster-Scrollleisten nicht.
    const int scroll_w = (style & WS_VSCROLL) ? GetSystemMetricsForDpi(SM_CXVSCROLL, dpi) : 0;
    const int scroll_h = (style & WS_HSCROLL) ? GetSystemMetricsForDpi(SM_CYHSCROLL, dpi) : 0;
    const int width = event_layout_int(static_cast<std::int64_t>(frame.right) - frame.left + scroll_w);
    const int height = event_layout_int(static_cast<std::int64_t>(frame.bottom) - frame.top + scroll_h);
    EventLayoutRect work{};
    if (!event_view_work_area(owner, nullptr, work))
        work = {0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)};
    RECT owner_rect{};
    const EventLayoutRect anchor = GetWindowRect(owner, &owner_rect) ? event_view_rect(owner_rect) : work;
    return event_layout_center(work, anchor, width, height);
}

inline void event_view_fit_existing(HWND window) {
    RECT current{};
    EventLayoutRect work{};
    if (!GetWindowRect(window, &current) || !event_view_work_area(window, nullptr, work)) return;
    const EventLayoutRect r = event_layout_fit(work, event_view_rect(current));
    if (r.left == current.left && r.top == current.top &&
        r.right == current.right && r.bottom == current.bottom) return;
    SetWindowPos(window, nullptr, r.left, r.top,
                 event_layout_int(static_cast<std::int64_t>(r.right) - r.left),
                 event_layout_int(static_cast<std::int64_t>(r.bottom) - r.top),
                 SWP_NOZORDER | SWP_NOACTIVATE);
}

inline HFONT event_view_make_font(unsigned dpi) {
    const int height = std::max(1, event_layout_scale(16, dpi));
    return CreateFontW(-height, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                       CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
}

inline void event_view_init(EventViewState& view, unsigned dpi, HFONT fallback_font) {
    view.dpi = dpi ? dpi : EVENT_LAYOUT_DPI;
    view.fallback_font = fallback_font;
    view.font = event_view_make_font(view.dpi);
}

// Erst nach WM_NCDESTROY aufrufen: dann verwendet kein Kind die Schrift mehr.
inline void event_view_dispose(EventViewState& view) {
    if (view.font) DeleteObject(view.font);
    view = EventViewState{};
}

inline HWND event_view_add(EventViewState& view, HWND parent, const wchar_t* cls,
                           const wchar_t* text, DWORD style,
                           int x, int y, int width, int height, int id) {
    HWND child = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style,
        event_layout_scale(x, view.dpi), event_layout_scale(y, view.dpi),
        event_layout_scale(width, view.dpi), event_layout_scale(height, view.dpi),
        parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), GetModuleHandleW(nullptr), nullptr);
    if (child) {
        view.controls.push_back({child, x, y, width, height});
        SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(view.font ? view.font : view.fallback_font), TRUE);
    }
    return child;
}

inline void event_view_size(HWND window, int& width, int& height) {
    RECT client{};
    GetClientRect(window, &client);
    width = std::max(1, static_cast<int>(client.right - client.left));
    height = std::max(1, static_cast<int>(client.bottom - client.top));
}

inline int event_view_content_width(const EventViewState& view) {
    return event_layout_scale(EVENT_LAYOUT_WIDTH, view.dpi);
}

inline int event_view_content_height(const EventViewState& view) {
    return event_layout_scale(EVENT_LAYOUT_HEIGHT, view.dpi);
}

inline void event_view_place(EventViewState& view, HWND window, bool resize) {
    // ScrollWindowEx(SW_SCROLLCHILDREN) bewegt nur Kinder, die den aktuellen
    // Ausschnitt schneiden. Auch vollständig verborgene Kinder müssen aber
    // mitwandern; deshalb stets aus den logischen Originalkoordinaten setzen.
    const UINT flags = SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER | (resize ? 0 : SWP_NOSIZE);
    HDWP batch = BeginDeferWindowPos(static_cast<int>(view.controls.size()));
    for (const EventViewControl& control : view.controls) {
        if (!batch) break;
        batch = DeferWindowPos(batch, control.window, nullptr,
            event_layout_scale(control.x, view.dpi) - view.x,
            event_layout_scale(control.y, view.dpi) - view.y,
            event_layout_scale(control.width, view.dpi), event_layout_scale(control.height, view.dpi), flags);
    }
    const bool placed = batch && EndDeferWindowPos(batch);
    if (!placed) {
        for (const EventViewControl& control : view.controls)
            SetWindowPos(control.window, nullptr,
                event_layout_scale(control.x, view.dpi) - view.x,
                event_layout_scale(control.y, view.dpi) - view.y,
                event_layout_scale(control.width, view.dpi), event_layout_scale(control.height, view.dpi), flags);
    }
    RedrawWindow(window, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
}

inline void event_view_update(EventViewState& view, HWND window, bool resize_controls = false) {
    if (!view.ready || view.busy) return;
    view.busy = true;
    int width = 1, height = 1;
    event_view_size(window, width, height);
    const int content_w = event_view_content_width(view), content_h = event_view_content_height(view);
    view.x = event_scroll_clamp(content_w, width, view.x);
    view.y = event_scroll_clamp(content_h, height, view.y);
    SCROLLINFO info{};
    info.cbSize = sizeof(info);
    // Die Leisten bleiben bei ausreichend Platz deaktiviert sichtbar. So ändert
    // ihr Ein-/Ausblenden nicht die Clientgröße und es gibt keine WM_SIZE-Schleife.
    info.fMask = SIF_RANGE | SIF_PAGE | SIF_POS | SIF_DISABLENOSCROLL;
    info.nMin = 0;
    info.nMax = std::max(0, content_w - 1);
    info.nPage = static_cast<UINT>(width);
    info.nPos = view.x;
    SetScrollInfo(window, SB_HORZ, &info, TRUE);
    info.nMax = std::max(0, content_h - 1);
    info.nPage = static_cast<UINT>(height);
    info.nPos = view.y;
    SetScrollInfo(window, SB_VERT, &info, TRUE);
    event_view_place(view, window, resize_controls);
    view.busy = false;
}

inline void event_view_scroll_to(EventViewState& view, HWND window, int x, int y) {
    if (!view.ready || view.busy) return;
    int width = 1, height = 1;
    event_view_size(window, width, height);
    const int next_x = event_scroll_clamp(event_view_content_width(view), width, x);
    const int next_y = event_scroll_clamp(event_view_content_height(view), height, y);
    if (next_x == view.x && next_y == view.y) return;
    view.x = next_x;
    view.y = next_y;
    event_view_update(view, window);
}

inline void event_view_reveal(EventViewState& view, HWND window, HWND focus) {
    if (!view.ready || !focus || !IsChild(window, focus)) return;
    // Combo-Box- und andere Unterkinder auf das direkte Dialogkind zurückführen.
    HWND child = focus;
    while (GetParent(child) && GetParent(child) != window) child = GetParent(child);
    if (GetParent(child) != window) return;
    RECT r{};
    if (!GetWindowRect(child, &r)) return;
    MapWindowPoints(nullptr, window, reinterpret_cast<POINT*>(&r), 2);
    int width = 1, height = 1;
    event_view_size(window, width, height);
    const int margin = event_layout_scale(8, view.dpi);
    const int left = event_layout_int(static_cast<std::int64_t>(r.left) + view.x);
    const int right = event_layout_int(static_cast<std::int64_t>(r.right) + view.x);
    const int top = event_layout_int(static_cast<std::int64_t>(r.top) + view.y);
    const int bottom = event_layout_int(static_cast<std::int64_t>(r.bottom) + view.y);
    event_view_scroll_to(view, window,
        event_scroll_reveal(event_view_content_width(view), width, view.x, left, right, margin),
        event_scroll_reveal(event_view_content_height(view), height, view.y, top, bottom, margin));
}

// Nur bei echtem Fokuswechsel nachführen. Sonst würde manuelles Scrollen
// sofort wieder zum bislang fokussierten, nun verborgenen Feld zurückspringen.
inline void event_view_reveal_new_focus(EventViewState& view, HWND window) {
    if (!view.ready) return;
    HWND focus = GetFocus();
    if (focus == view.last_focus) return;
    view.last_focus = focus;
    event_view_reveal(view, window, focus);
}

inline void event_view_scrollbar(EventViewState& view, HWND window, bool horizontal, WPARAM wp) {
    int width = 1, height = 1;
    event_view_size(window, width, height);
    const int viewport = horizontal ? width : height;
    const int content = horizontal ? event_view_content_width(view) : event_view_content_height(view);
    const int old = horizontal ? view.x : view.y;
    const int line = std::max(1, event_layout_scale(32, view.dpi));
    const int page = std::max(1, viewport - line);
    int next = old;
    switch (LOWORD(wp)) {
        case SB_LINEUP:   next = event_scroll_move(content, viewport, old, -static_cast<std::int64_t>(line)); break;
        case SB_LINEDOWN: next = event_scroll_move(content, viewport, old, line); break;
        case SB_PAGEUP:   next = event_scroll_move(content, viewport, old, -static_cast<std::int64_t>(page)); break;
        case SB_PAGEDOWN: next = event_scroll_move(content, viewport, old, page); break;
        case SB_TOP:      next = 0; break;
        case SB_BOTTOM:   next = event_scroll_max(content, viewport); break;
        case SB_THUMBTRACK:
        case SB_THUMBPOSITION: {
            SCROLLINFO info{};
            info.cbSize = sizeof(info);
            info.fMask = SIF_TRACKPOS;
            if (GetScrollInfo(window, horizontal ? SB_HORZ : SB_VERT, &info)) next = info.nTrackPos;
            break;
        }
        default: return;
    }
    event_view_scroll_to(view, window, horizontal ? next : view.x, horizontal ? view.y : next);
}

inline void event_view_wheel(EventViewState& view, HWND window, bool horizontal, bool horizontal_message, WPARAM wp) {
    int& remainder = horizontal ? view.wheel_x : view.wheel_y;
    const int total = remainder + static_cast<short>(HIWORD(wp));
    const int detents = total / WHEEL_DELTA;
    remainder = total % WHEEL_DELTA;
    if (!detents) return;
    int width = 1, height = 1;
    event_view_size(window, width, height);
    const int viewport = horizontal ? width : height;
    const int content = horizontal ? event_view_content_width(view) : event_view_content_height(view);
    const int old = horizontal ? view.x : view.y;
    UINT lines = 3;
    if (!SystemParametersInfoW(SPI_GETWHEELSCROLLLINES, 0, &lines, 0)) lines = 3;
    if (!lines) return;
    const std::int64_t line = std::max(1, event_layout_scale(32, view.dpi));
    const std::int64_t distance = lines == WHEEL_PAGESCROLL ? viewport
        : std::min(static_cast<std::int64_t>(std::max(0, content)), line * lines);
    // Vertikales Rad: positiv = oben/links. WM_MOUSEHWHEEL: positiv = rechts.
    const std::int64_t delta = static_cast<std::int64_t>(detents) * distance * (horizontal_message ? 1 : -1);
    const int next = event_scroll_move(content, viewport, old, delta);
    event_view_scroll_to(view, window, horizontal ? next : view.x, horizontal ? view.y : next);
}

inline void event_view_dpi_changed(EventViewState& view, HWND window, unsigned dpi, RECT suggested) {
    if (!dpi) dpi = EVENT_LAYOUT_DPI;
    const unsigned old_dpi = view.dpi;
    view.x = event_layout_rescale(view.x, old_dpi, dpi);
    view.y = event_layout_rescale(view.y, old_dpi, dpi);
    view.dpi = dpi;
    view.wheel_x = view.wheel_y = 0;
    view.busy = true;
    if (HFONT font = event_view_make_font(dpi)) {
        const HFONT old_font = view.font;
        view.font = font;
        for (const EventViewControl& control : view.controls)
            SendMessageW(control.window, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        if (old_font) DeleteObject(old_font);
    }
    EventLayoutRect target = event_view_rect(suggested), work{};
    if (event_view_work_area(window, &suggested, work)) target = event_layout_fit(work, target);
    SetWindowPos(window, nullptr, target.left, target.top,
                 event_layout_int(static_cast<std::int64_t>(target.right) - target.left),
                 event_layout_int(static_cast<std::int64_t>(target.bottom) - target.top),
                 SWP_NOZORDER | SWP_NOACTIVATE);
    view.busy = false;
    event_view_update(view, window, true);
    event_view_reveal(view, window, GetFocus());
}

// Nur Fenster-Scrollleisten behandeln (lParam == 0), nicht Meldungen eines
// untergeordneten Controls. Unverarbeitete Mausräder dürfen von deren eigener
// DefWindowProc nach oben gelangen; Listen und Combos behalten Vorrang.
inline bool event_view_message(EventViewState& view, HWND window, UINT msg, WPARAM wp, LPARAM lp) {
    if (!view.ready || view.busy) return false;
    switch (msg) {
        case WM_SIZE:
            if (wp != SIZE_MINIMIZED) event_view_update(view, window);
            return true;
        case WM_HSCROLL:
        case WM_VSCROLL:
            if (lp) return false;
            event_view_scrollbar(view, window, msg == WM_HSCROLL, wp);
            return true;
        case WM_MOUSEWHEEL:
            event_view_wheel(view, window, (LOWORD(wp) & MK_SHIFT) != 0, false, wp);
            return true;
        case WM_MOUSEHWHEEL:
            event_view_wheel(view, window, true, true, wp);
            return true;
        case WM_DPICHANGED:
            if (!lp) return false;
            event_view_dpi_changed(view, window, LOWORD(wp), *reinterpret_cast<const RECT*>(lp));
            return true;
        case WM_DISPLAYCHANGE:
        case WM_SETTINGCHANGE:
        case WM_EXITSIZEMOVE:
            // Erst nach dem Ziehen begrenzen, damit ein Wechsel auf einen
            // anderen Monitor auch bei gleicher DPI nicht verhindert wird.
            event_view_fit_existing(window);
            return false; // die Standardverarbeitung ebenfalls zulassen
        case WM_GETMINMAXINFO: {
            EventLayoutRect work{};
            if (!lp || !event_view_work_area(window, nullptr, work)) return false;
            const int width = event_layout_int(static_cast<std::int64_t>(work.right) - work.left);
            const int height = event_layout_int(static_cast<std::int64_t>(work.bottom) - work.top);
            if (width <= 0 || height <= 0) return false;
            MINMAXINFO* info = reinterpret_cast<MINMAXINFO*>(lp);
            info->ptMinTrackSize.x = std::min(event_layout_scale(240, view.dpi), std::max(1, width / 2));
            info->ptMinTrackSize.y = std::min(event_layout_scale(160, view.dpi), std::max(1, height / 2));
            info->ptMaxTrackSize.x = width;
            info->ptMaxTrackSize.y = height;
            return true;
        }
        default: return false;
    }
}

} // namespace rune::editor
#endif // _WIN32
