#pragma once
#include <cstdint>
#include <string>

#include <windows.h>

namespace rune::core {

// Tastatur/Maus-Zustand eines Frames. Wird von der Engine und von Ruby gelesen.
struct InputState {
    bool keys[256] = {};
    bool keys_prev[256] = {};

    float mouse_dx = 0.0f;
    float mouse_dy = 0.0f;
    int   wheel = 0;

    // Mausposition in Fensterpixeln. Im Fang-Modus steht sie in der Mitte
    // (Fadenkreuz), sonst folgt sie dem echten Zeiger.
    float mouse_x = 0.0f;
    float mouse_y = 0.0f;

    // Maustasten: 0 = links, 1 = rechts, 2 = Mitte
    bool mouse_buttons[3] = {};
    bool mouse_buttons_prev[3] = {};

    bool mouse_captured = false;
    bool quit_requested = false;

    // Druckbare Zeichen (und '\b' für Backspace), die seit dem letzten Frame
    // eingegangen sind - für Texteingaben im Titelbildschirm/Editor.
    std::string typed;
    void push_char(char c) { if (typed.size() < 64) typed.push_back(c); }
    std::string take_typed() { std::string out; out.swap(typed); return out; }

    bool down(int vk) const { return vk > 0 && vk < 256 && keys[vk]; }
    bool pressed(int vk) const { return vk > 0 && vk < 256 && keys[vk] && !keys_prev[vk]; }

    bool mouse_down(int i) const { return i >= 0 && i < 3 && mouse_buttons[i]; }
    bool mouse_pressed(int i) const {
        return i >= 0 && i < 3 && mouse_buttons[i] && !mouse_buttons_prev[i];
    }

    void end_frame() {
        for (int i = 0; i < 256; ++i) keys_prev[i] = keys[i];
        for (int i = 0; i < 3; ++i) mouse_buttons_prev[i] = mouse_buttons[i];
        mouse_dx = 0.0f;
        mouse_dy = 0.0f;
        wheel = 0;
    }
};

class Window {
public:
    Window() = default;
    ~Window();

    bool create(const wchar_t* title, int width, int height);
    void destroy();

    // Verarbeitet Nachrichten, aktualisiert Input. Rückgabe false = Fenster zu.
    bool pump();

    void toggle_mouse_capture();
    void set_mouse_capture(bool on);

    HWND  hwnd() const { return m_hwnd; }
    int   width() const { return m_width; }
    int   height() const { return m_height; }

    // true, wenn sich die Fenstergrösse seit dem letzten Aufruf geändert hat.
    bool consume_resize();

    InputState& input() { return m_input; }
    const InputState& input() const { return m_input; }

private:
    static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT handle(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    void update_mouse_delta();

    HWND       m_hwnd = nullptr;
    HDC        m_dc = nullptr;
    int        m_width = 0;
    int        m_height = 0;
    bool       m_should_close = false;
    bool       m_resized = false;
    InputState m_input;
};

} // namespace rune::core
