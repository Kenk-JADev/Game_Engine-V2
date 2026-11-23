#pragma once
// F4-Kommando aus UTF-8-Projektname und Engine-Wurzel bauen. Beide üblichen
// Installationslayouts unterstützen: build/Rune3D.exe oder EXE neben game/.
#ifdef _WIN32
#include "core/project_assets_win.h"
#include "core/win_path.h"

#include <string>

namespace rune::editor {

inline bool playtest_command(const std::string& root, const std::string& project,
                             std::wstring& exe, std::wstring& command, std::wstring& cwd) {
    exe.clear(); command.clear(); cwd.clear();
    std::wstring wide_project;
    if (!core::win_path::to_wide(root, cwd) || !core::win_path::to_wide(project, wide_project) ||
        !assets::component(wide_project)) return false;
    const auto is_file = [](const std::wstring& path) {
        const DWORD attributes = GetFileAttributesW(path.c_str());
        return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
    };
    const std::wstring nested = cwd + L"\\build\\Rune3D.exe";
    const std::wstring adjacent = cwd + L"\\Rune3D.exe";
    if (is_file(nested)) exe = nested;
    else if (is_file(adjacent)) exe = adjacent;
    else return false;
    // Die validierte Projektkomponente enthält weder Anführungszeichen noch
    // Backslashes. Das erhält auch Leerzeichen und Nicht-ASCII-Zeichen exakt.
    command = L"\"" + exe + L"\" --project \"" + wide_project + L"\"";
    return true;
}

} // namespace rune::editor
#endif
