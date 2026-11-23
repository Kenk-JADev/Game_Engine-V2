#pragma once
// Gemeinsame Prüfung für UTF-8 und einzeilige Win32-Formularfelder.
#include <algorithm>
#include <cstddef>
#include <string>

namespace rune::editor {

inline bool editor_valid_utf8(const std::string& text) {
    for (std::size_t i = 0; i < text.size();) {
        const auto first = static_cast<unsigned char>(text[i]);
        if (first < 0x80) { ++i; continue; }
        std::size_t count = 0;
        unsigned codepoint = 0, minimum = 0;
        if (first >= 0xc2 && first <= 0xdf) { count = 2; codepoint = first & 0x1f; minimum = 0x80; }
        else if (first >= 0xe0 && first <= 0xef) { count = 3; codepoint = first & 0x0f; minimum = 0x800; }
        else if (first >= 0xf0 && first <= 0xf4) { count = 4; codepoint = first & 0x07; minimum = 0x10000; }
        else return false;
        if (text.size() - i < count) return false;
        for (std::size_t j = 1; j < count; ++j) {
            const auto c = static_cast<unsigned char>(text[i + j]);
            if ((c & 0xc0) != 0x80) return false;
            codepoint = (codepoint << 6) | (c & 0x3f);
        }
        if (codepoint < minimum || codepoint > 0x10ffff ||
            (codepoint >= 0xd800 && codepoint <= 0xdfff)) return false;
        i += count;
    }
    return true;
}

inline bool editor_single_line(const std::string& text) {
    return editor_valid_utf8(text) && std::none_of(text.begin(), text.end(), [](unsigned char c) {
        return c < 0x20 || c == 0x7f;
    });
}

} // namespace rune::editor
