#pragma once
// WIC/Renderer speichern RGBA, Win32-DIBs mit BI_RGB erwarten BGRA.
#include <cstddef>
#include <cstdint>

namespace rune::editor {
inline void texture_rgba_to_bgra(const std::uint8_t* source, std::uint8_t* destination,
                                 std::size_t pixels) noexcept {
    for (std::size_t i = 0; i < pixels; ++i) {
        const std::size_t at = i * 4;
        destination[at] = source[at + 2];
        destination[at + 1] = source[at + 1];
        destination[at + 2] = source[at];
        destination[at + 3] = source[at + 3];
    }
}
} // namespace rune::editor
