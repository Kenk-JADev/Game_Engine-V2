#pragma once
// Etappe 31 (Grafik): Toolbar-Icons mit echtem transparentem Hintergrund.
// Vorher wurde jedes Icon auf Weiß gezeichnet und mit voll deckender Maske
// erzeugt - auf der grauen Toolbar erschienen dadurch weiße Kästchen.
// Jetzt: Zeichnen auf eine Schlüsselfarbe, danach wird diese Farbe in einem
// 32-Bit-BGRA-Puffer durchsichtig, alles andere voll deckend.
// Reine Logik ohne Win32, kopflos testbar (tools/json_test.cpp).

#include <cstddef>
#include <cstdint>

namespace rune::editor {

// Schlüsselfarbe im BGRA-Speicherformat (0x00RRGGBB): Magenta kommt in
// keinem Icon vor.
constexpr std::uint32_t ICON_KEY_COLOR = 0x00FF00FFu;

// Liefert die Anzahl deckender Pixel (0 = Icon leer gezeichnet).
inline std::size_t icon_key_to_alpha(std::uint32_t* pixels, std::size_t count,
                                     std::uint32_t key = ICON_KEY_COLOR) {
    if (!pixels) return 0;
    std::size_t opaque = 0;
    for (std::size_t i = 0; i < count; ++i) {
        const std::uint32_t rgb = pixels[i] & 0x00FFFFFFu;
        if (rgb == (key & 0x00FFFFFFu)) {
            pixels[i] = 0; // vormultipliziert: durchsichtig = schwarz, Alpha 0
        } else {
            pixels[i] = rgb | 0xFF000000u;
            ++opaque;
        }
    }
    return opaque;
}

} // namespace rune::editor
