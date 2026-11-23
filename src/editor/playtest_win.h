#pragma once
// F4-Kommando aus UTF-8-Projektname, Karten-ID und Engine-Wurzel bauen.
// Beide üblichen Layouts unterstützen: build/Rune3D.exe oder EXE neben game/.
#ifdef _WIN32
#include "core/project_assets_win.h"
#include "core/win_path.h"
#include "core/win_project_cli.h"

#include <algorithm>
#include <string>

namespace rune::editor {

// Der Editor speichert den vollständigen Dateinamen; die Spiel-API benutzt
// die Karten-ID ohne .json. Auch .JSON und Unicode bleiben exakt zugeordnet.
inline bool playtest_map_id(const std::string& filename, std::string& id) {
    id.clear();
    if (filename.size() <= 5) return false;
    std::string ext = filename.substr(filename.size() - 5);
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
        return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : static_cast<char>(c);
    });
    std::wstring wide_file, wide_id;
    const std::string candidate = filename.substr(0, filename.size() - 5);
    if (ext != ".json" || !core::win_path::to_wide(filename, wide_file) ||
        !assets::component(wide_file) || !core::win_path::to_wide(candidate, wide_id) ||
        !core::win_project_cli::valid_map_id(wide_id)) return false;
    id = candidate;
    return true;
}

// Eine leere Karten-ID bedeutet: keine Karte gewählt, Projekt-Start verwenden.
inline bool playtest_command(const std::string& root, const std::string& project,
                             const std::string& map, std::wstring& exe,
                             std::wstring& command, std::wstring& cwd) {
    exe.clear(); command.clear(); cwd.clear();
    std::wstring wide_project, wide_map;
    if (!core::win_path::to_wide(root, cwd) || !core::win_path::to_wide(project, wide_project) ||
        !assets::component(wide_project) ||
        (!map.empty() && (!core::win_path::to_wide(map, wide_map) ||
                          !core::win_project_cli::valid_map_id(wide_map)))) return false;
    const auto is_file = [](const std::wstring& path) {
        const DWORD attributes = GetFileAttributesW(path.c_str());
        return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
    };
    const std::wstring nested = cwd + L"\\build\\Rune3D.exe";
    const std::wstring adjacent = cwd + L"\\Rune3D.exe";
    if (is_file(nested)) exe = nested;
    else if (is_file(adjacent)) exe = adjacent;
    else return false;
    // Die validierten Komponenten enthalten weder Anführungszeichen noch
    // Backslashes. Leerzeichen und Nicht-ASCII-Zeichen bleiben exakt erhalten.
    command = L"\"" + exe + L"\" --project \"" + wide_project + L"\"";
    if (!map.empty()) command += L" --map \"" + wide_map + L"\"";
    return true;
}

} // namespace rune::editor
#endif
