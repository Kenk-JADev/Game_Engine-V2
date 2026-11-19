#include "core/window.h"
#include "core/log.h"

#include <cmath>
#include <cstdio>
#include <windowsx.h> // GET_X_LPARAM / GET_Y_LPARAM

namespace rune::core {
namespace {

const wchar_t* kClassName = L"Rune3DWindowClass";

// Etappe 18a: eigenes Fenster-Icon (blauer Kristall), prozedural gezeichnet -
// kein Ressourcen-File, wie bei Texturen/Sounds. Groesse sz (16 oder 32).
void draw_game_icon(HDC dc, int sz) {
    HBRUSH bg = CreateSolidBrush(RGB(18, 26, 38));
    RECT rc{ 0, 0, sz, sz };
    FillRect(dc, &rc, bg);
    DeleteObject(bg);

    HBRUSH cry = CreateSolidBrush(RGB(90, 160, 230));
    HPEN edge = CreatePen(PS_SOLID, 1, RGB(220, 240, 255));
    HGDIOBJ op = SelectObject(dc, edge);
    HGDIOBJ ob = SelectObject(dc, cry);
    // Kristall: sechs Zacken um einen Kern.
    const int cx = sz / 2;
    const int cy = sz / 2;
    POINT star[7]{};
    for (int i = 0; i < 7; ++i) {
        const bool long_arm = (i % 2 == 0);
        const double w = i * 0.8976;   // 360/7 Grad, passend gerundet
        const float r = long_arm ? 0.46f : 0.20f;
        star[i].x = cx + static_cast<LONG>(std::sin(w * 6.2832 / 5.7) * sz * r);
        star[i].y = cy - static_cast<LONG>(std::cos(w * 6.2832 / 5.7) * sz * r);
    }
    Polygon(dc, star, 7);
    SelectObject(dc, op);
    SelectObject(dc, ob);
    DeleteObject(cry);
    DeleteObject(edge);

    // Kern hell akzentuieren.
    HBRUSH core_br = CreateSolidBrush(RGB(200, 235, 255));
    HGDIOBJ oc = SelectObject(dc, core_br);
    Ellipse(dc, cx - sz / 5, cy - sz / 5, cx + sz / 5, cy + sz / 5);
    SelectObject(dc, oc);
    DeleteObject(core_br);
}

HICON make_icon(void (*draw)(HDC, int), int sz) {
    // 18a-Fix: CreateIconIndirect verlangt eine ECHT monochrome Maske
    // (CreateCompatibleBitmap = Farb-Bitmap = undefiniert und kann die
    // Titelzeichnung zum Absturz bringen) und ein 32bpp-DIB mit Alpha=255
    // (DWM wertet uninitialisierte Alpha-Bytes als Transparenz).
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

} // namespace

Window::~Window() { destroy(); }

LRESULT CALLBACK Window::wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    Window* self = nullptr;

    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        self = reinterpret_cast<Window*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->m_hwnd = hwnd;
    } else {
        self = reinterpret_cast<Window*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (self) return self->handle(hwnd, msg, wp, lp);
    return DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT Window::handle(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CLOSE:
        case WM_DESTROY:
            m_should_close = true;
            m_input.quit_requested = true;
            PostQuitMessage(0);
            return 0;

        case WM_SIZE:
            if (wp != SIZE_MINIMIZED) {
                m_width = LOWORD(lp);
                m_height = HIWORD(lp);
                m_resized = true;
            }
            return 0;

        case WM_SYSKEYDOWN:
            if (wp == VK_F4) { // Alt+F4
                m_should_close = true;
                m_input.quit_requested = true;
                return 0;
            }
            [[fallthrough]];

        case WM_KEYDOWN:
        case WM_KEYUP: {
            int vk = static_cast<int>(wp);
            if (vk >= 0 && vk < 256) {
                m_input.keys[vk] = (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN);
            }
            return 0;
        }

        case WM_CHAR: {
            // Texteingabe: druckbare ASCII-Zeichen sammeln, Backspace als '\b'.
            const int ch = static_cast<int>(wp);
            if (ch == 8 || (ch >= 32 && ch < 127)) m_input.push_char(static_cast<char>(ch));
            return 0;
        }

        case WM_MOUSEMOVE: {
            if (!m_input.mouse_captured) {
                m_input.mouse_x = static_cast<float>(GET_X_LPARAM(lp));
                m_input.mouse_y = static_cast<float>(GET_Y_LPARAM(lp));
            }
            return 0;
        }

        case WM_LBUTTONDOWN: m_input.mouse_buttons[0] = true;  return 0;
        case WM_LBUTTONUP:   m_input.mouse_buttons[0] = false; return 0;
        case WM_RBUTTONDOWN: m_input.mouse_buttons[1] = true;  return 0;
        case WM_RBUTTONUP:   m_input.mouse_buttons[1] = false; return 0;
        case WM_MBUTTONDOWN:
            // Mittlere Maustaste schaltet den Mausfang um (Esc gibt ebenfalls frei).
            toggle_mouse_capture();
            m_input.mouse_buttons[2] = true;
            return 0;
        case WM_MBUTTONUP:   m_input.mouse_buttons[2] = false; return 0;

        case WM_MOUSEWHEEL:
            m_input.wheel += GET_WHEEL_DELTA_WPARAM(wp);
            return 0;

        case WM_KILLFOCUS:
            ZeroMemory(m_input.keys, sizeof(m_input.keys));
            if (m_input.mouse_captured) set_mouse_capture(false);
            return 0;

        case WM_ACTIVATEAPP:
            if (!wp) {
                ZeroMemory(m_input.keys, sizeof(m_input.keys));
                if (m_input.mouse_captured) set_mouse_capture(false);
            }
            return 0;

        default:
            break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

bool Window::create(const wchar_t* title, int width, int height) {
    HINSTANCE inst = GetModuleHandleW(nullptr);

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc = &Window::wnd_proc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = kClassName;

    if (!RegisterClassExW(&wc)) {
        log_error("RegisterClassExW fehlgeschlagen");
        return false;
    }

    RECT r{0, 0, width, height};
    DWORD style = WS_OVERLAPPEDWINDOW;
    AdjustWindowRect(&r, style, FALSE);

    m_hwnd = CreateWindowExW(0, kClassName, title, style, CW_USEDEFAULT, CW_USEDEFAULT,
                             r.right - r.left, r.bottom - r.top, nullptr, nullptr, inst, this);
    if (!m_hwnd) {
        log_error("CreateWindowExW fehlgeschlagen");
        return false;
    }

    // Etappe 18a: eigenes Fenster-Icon (Kristall, prozedural gezeichnet).
    SendMessageW(m_hwnd, WM_SETICON, ICON_BIG,
                 reinterpret_cast<LPARAM>(make_icon(draw_game_icon, 32)));
    SendMessageW(m_hwnd, WM_SETICON, ICON_SMALL,
                 reinterpret_cast<LPARAM>(make_icon(draw_game_icon, 16)));

    m_width = width;
    m_height = height;
    m_dc = GetDC(m_hwnd);

    // DPI-Skalierung soll intern einheitlich bleiben.
    SetProcessDpiAwarenessContext((DPI_AWARENESS_CONTEXT)-4); // PER_MONITOR_AWARE_V2

    ShowWindow(m_hwnd, SW_SHOW);
    UpdateWindow(m_hwnd);
    SetForegroundWindow(m_hwnd);

    set_mouse_capture(true);

    char buf[160];
    std::snprintf(buf, sizeof(buf), "Fenster erstellt: %dx%d", width, height);
    log_info(buf);
    return true;
}

void Window::destroy() {
    if (m_input.mouse_captured) set_mouse_capture(false);
    if (m_dc && m_hwnd) {
        ReleaseDC(m_hwnd, m_dc);
        m_dc = nullptr;
    }
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
    UnregisterClassW(kClassName, GetModuleHandleW(nullptr));
}

bool Window::pump() {
    MSG msg{};
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) m_should_close = true;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    update_mouse_delta();
    return !m_should_close;
}

void Window::update_mouse_delta() {
    if (!m_hwnd) return;

    POINT p{};
    if (!GetCursorPos(&p)) return;

    POINT client = p;
    ScreenToClient(m_hwnd, &client);

    if (!m_input.mouse_captured) {
        // Ohne Fang: echter Zeiger, keine Kameradrehung.
        m_input.mouse_x = static_cast<float>(client.x);
        m_input.mouse_y = static_cast<float>(client.y);
        return;
    }

    RECT r{};
    GetClientRect(m_hwnd, &r);
    POINT center{(r.right - r.left) / 2, (r.bottom - r.top) / 2};
    POINT center_screen = center;
    ClientToScreen(m_hwnd, &center_screen);

    m_input.mouse_dx += static_cast<float>(p.x - center_screen.x);
    m_input.mouse_dy += static_cast<float>(p.y - center_screen.y);

    // Im Fang-Modus steht das Fadenkreuz in der Bildschirmmitte.
    m_input.mouse_x = static_cast<float>(center.x);
    m_input.mouse_y = static_cast<float>(center.y);

    SetCursorPos(center_screen.x, center_screen.y);
}

void Window::set_mouse_capture(bool on) {
    if (m_input.mouse_captured == on) return;
    m_input.mouse_captured = on;

    if (on) {
        ShowCursor(FALSE);
        RECT r{};
        GetClientRect(m_hwnd, &r);
        POINT tl{r.left, r.top};
        POINT br{r.right, r.bottom};
        ClientToScreen(m_hwnd, &tl);
        ClientToScreen(m_hwnd, &br);
        RECT clip{tl.x, tl.y, br.x, br.y};
        ClipCursor(&clip);
        POINT center{(tl.x + br.x) / 2, (tl.y + br.y) / 2};
        SetCursorPos(center.x, center.y);
    } else {
        ClipCursor(nullptr);
        ShowCursor(TRUE);
    }
}

void Window::toggle_mouse_capture() { set_mouse_capture(!m_input.mouse_captured); }

bool Window::consume_resize() {
    bool r = m_resized;
    m_resized = false;
    return r;
}

} // namespace rune::core
