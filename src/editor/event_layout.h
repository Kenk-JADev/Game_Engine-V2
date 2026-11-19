#pragma once
// Geometrie des F6-Formulars in 96-DPI-Einheiten. Ohne Win32, damit auch
// kleine Arbeitsflächen, negative Monitorkoordinaten und Scrollgrenzen in CI
// geprüft werden können. Die Höhe der Combo-Dropdowns zählt nicht zur Fläche:
// Windows zeichnet ihre aufgeklappten Listen als eigene Popups.

#include <algorithm>
#include <cstdint>
#include <limits>

namespace rune::editor {

constexpr int EVENT_LAYOUT_WIDTH = 1000;
constexpr int EVENT_LAYOUT_HEIGHT = 702;
constexpr unsigned EVENT_LAYOUT_DPI = 96;

struct EventLayoutRect {
    int left = 0, top = 0, right = 0, bottom = 0;
};

inline int event_layout_int(std::int64_t value) {
    return static_cast<int>(std::clamp(value,
        static_cast<std::int64_t>(std::numeric_limits<int>::min()),
        static_cast<std::int64_t>(std::numeric_limits<int>::max())));
}

// Wie MulDiv gerundet, aber auch bei sehr großen Eingaben ohne Überlauf.
inline int event_layout_rescale(int value, unsigned from_dpi, unsigned to_dpi) {
    if (!from_dpi) from_dpi = EVENT_LAYOUT_DPI;
    if (!to_dpi) to_dpi = EVENT_LAYOUT_DPI;
    std::int64_t product = static_cast<std::int64_t>(value) * to_dpi;
    product += product < 0 ? -static_cast<std::int64_t>(from_dpi / 2)
                           : static_cast<std::int64_t>(from_dpi / 2);
    return event_layout_int(product / from_dpi);
}

inline int event_layout_scale(int value, unsigned dpi) {
    return event_layout_rescale(value, EVENT_LAYOUT_DPI, dpi);
}

// Größe und Ursprung eines gewünschten äußeren Fensters auf die Arbeitsfläche
// begrenzen. Bei ungültiger Arbeitsfläche den Vorschlag unverändert lassen.
inline EventLayoutRect event_layout_fit(EventLayoutRect work, EventLayoutRect wanted) {
    const std::int64_t ww = static_cast<std::int64_t>(work.right) - work.left;
    const std::int64_t wh = static_cast<std::int64_t>(work.bottom) - work.top;
    if (ww <= 0 || wh <= 0) return wanted;
    const std::int64_t width = std::clamp(static_cast<std::int64_t>(wanted.right) - wanted.left,
                                          std::int64_t{1}, ww);
    const std::int64_t height = std::clamp(static_cast<std::int64_t>(wanted.bottom) - wanted.top,
                                           std::int64_t{1}, wh);
    const std::int64_t left = std::clamp(static_cast<std::int64_t>(wanted.left),
                                         static_cast<std::int64_t>(work.left), work.right - width);
    const std::int64_t top = std::clamp(static_cast<std::int64_t>(wanted.top),
                                        static_cast<std::int64_t>(work.top), work.bottom - height);
    return {static_cast<int>(left), static_cast<int>(top),
            static_cast<int>(left + width), static_cast<int>(top + height)};
}

inline EventLayoutRect event_layout_center(EventLayoutRect work, EventLayoutRect owner,
                                           int desired_width, int desired_height) {
    const std::int64_t ww = static_cast<std::int64_t>(work.right) - work.left;
    const std::int64_t wh = static_cast<std::int64_t>(work.bottom) - work.top;
    if (ww <= 0 || wh <= 0) return work;
    const std::int64_t width = std::clamp(static_cast<std::int64_t>(desired_width), std::int64_t{1}, ww);
    const std::int64_t height = std::clamp(static_cast<std::int64_t>(desired_height), std::int64_t{1}, wh);
    const std::int64_t left = std::clamp((static_cast<std::int64_t>(owner.left) + owner.right - width) / 2,
                                         static_cast<std::int64_t>(work.left), work.right - width);
    const std::int64_t top = std::clamp((static_cast<std::int64_t>(owner.top) + owner.bottom - height) / 2,
                                        static_cast<std::int64_t>(work.top), work.bottom - height);
    return {static_cast<int>(left), static_cast<int>(top),
            static_cast<int>(left + width), static_cast<int>(top + height)};
}

inline int event_scroll_max(int content, int viewport) {
    return std::max(0, std::max(0, content) - std::max(1, viewport));
}

inline int event_scroll_clamp(int content, int viewport, int position) {
    return std::clamp(position, 0, event_scroll_max(content, viewport));
}

// Auch bei sehr großen/negativen Schritten nicht über int64 hinaus addieren.
inline int event_scroll_move(int content, int viewport, int position, std::int64_t delta) {
    const int maximum = event_scroll_max(content, viewport);
    const int current = std::clamp(position, 0, maximum);
    if (delta >= static_cast<std::int64_t>(maximum) - current) return maximum;
    if (delta <= -static_cast<std::int64_t>(current)) return 0;
    return static_cast<int>(current + delta);
}

// Ein fokussiertes Feld sichtbar machen, ohne bei einem übergroßen Listfeld
// unnötig zu springen, wenn davon bereits ein Teil im Ausschnitt liegt.
inline int event_scroll_reveal(int content, int viewport, int position,
                               int start, int end, int margin) {
    viewport = std::max(1, viewport);
    const int current = event_scroll_clamp(content, viewport, position);
    if (end <= start) return current;
    const int pad = std::clamp(margin, 0, (viewport - 1) / 2);
    const std::int64_t left = start, right = end;
    std::int64_t wanted = current;
    if (right - left > static_cast<std::int64_t>(viewport) - 2 * pad) {
        if (right <= current || left >= static_cast<std::int64_t>(current) + viewport)
            wanted = left - pad;
    } else if (left < static_cast<std::int64_t>(current) + pad) {
        wanted = left - pad;
    } else if (right > static_cast<std::int64_t>(current) + viewport - pad) {
        wanted = right - viewport + pad;
    }
    return static_cast<int>(std::clamp(wanted, std::int64_t{0},
                                       static_cast<std::int64_t>(event_scroll_max(content, viewport))));
}

} // namespace rune::editor
