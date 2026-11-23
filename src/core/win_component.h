#pragma once
// Gemeinsame lexikalische Prüfung einer einzelnen Windows-Dateikomponente.
// Keine Prüfung von Reparsepunkten, Hardlinks oder Dateikonflikten.
#ifdef _WIN32
#include "core/win_path.h"
#include "editor/texture_path.h"

#include <string>

namespace rune::core::win_path {

inline bool component(const std::wstring& name) {
    if (name.empty() || name.size() > 255 || name == L"." || name == L".." ||
        name.back() == L'.' || name.back() == L' ') return false;
    for (wchar_t c : name) {
        if (c < 0x20 || c == 0x7f || c == L'/' || c == L'\\' || c == L':' ||
            c == L'*' || c == L'?' || c == L'"' || c == L'<' || c == L'>' || c == L'|') return false;
    }
    std::string utf8;
    return to_utf8(name, utf8) && !editor::windows_device_name(utf8);
}

} // namespace rune::core::win_path
#endif
