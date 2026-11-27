#pragma once
// Gemeinsame Grenzen für importierte und per Ruby registrierte RGBA-Texturen.
// Neben der CPU-Kopie hält der Renderer typischerweise eine weitere GPU-Kopie.
#include <cstddef>

namespace rune::world {

inline constexpr int TEXTURE_MAX_DIMENSION = 4096;
inline constexpr std::size_t TEXTURE_MAX_BYTES = 64u * 1024u * 1024u;
inline constexpr std::size_t TEXTURE_MAX_RESIDENT_BYTES = 128u * 1024u * 1024u;
inline constexpr std::size_t TEXTURE_MAX_SLOTS = 2048;
inline constexpr std::size_t TEXTURE_MAX_MATERIALS = 1024;
inline constexpr std::size_t TEXTURE_MAX_MANIFEST_BYTES = 2u * 1024u * 1024u;
inline constexpr std::size_t TEXTURE_MAX_SOURCE_BYTES = 64u * 1024u * 1024u;

inline bool texture_byte_count(int width, int height, std::size_t& bytes) {
    bytes = 0;
    if (width < 1 || height < 1 || width > TEXTURE_MAX_DIMENSION ||
        height > TEXTURE_MAX_DIMENSION) return false;
    // Die geprüften Dimensionen machen die Multiplikation auch auf 32 Bit sicher.
    bytes = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u;
    return bytes <= TEXTURE_MAX_BYTES;
}

} // namespace rune::world
