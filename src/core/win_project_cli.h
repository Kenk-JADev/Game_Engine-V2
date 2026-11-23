#pragma once
// Windows-Kommandozeile: Projekt- und Kartenname ohne Umweg über die
// CRT-Codepage lesen. Die Editor-CLI behält ihren bisherigen Projektparser.
#ifdef _WIN32
#include "core/win_component.h"
#include "core/win_path.h"
#include <shellapi.h>

#include <string>
#include <utility>

namespace rune::core::win_project_cli {

enum class ProjectArgument { Absent, Present, Invalid };

inline ProjectArgument parse_project(int argc, wchar_t* const* argv, std::string& project) {
    project.clear();
    bool found = false;
    for (int i = 1; i < argc; ++i) {
        if (std::wstring(argv[i]) != L"--project") continue;
        // Mehrdeutige oder nicht verlustfrei konvertierbare Angaben ablehnen.
        if (found || ++i >= argc || !win_path::to_utf8(argv[i], project)) {
            project.clear();
            return ProjectArgument::Invalid;
        }
        found = true;
    }
    return found ? ProjectArgument::Present : ProjectArgument::Absent;
}

inline ProjectArgument current_project(std::string& project) {
    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) { project.clear(); return ProjectArgument::Invalid; }
    const ProjectArgument result = parse_project(argc, argv, project);
    LocalFree(argv);
    return result;
}

// Eine Karten-ID ist der Dateiname ohne die Erweiterung. Sie darf z. B. auf
// einen Punkt oder ein Leerzeichen enden, wenn die tatsächliche Komponente
// durch das angehängte .json gültig ist. Pfade und Gerätenamen sind ungültig.
inline bool valid_map_id(const std::wstring& map) {
    return !map.empty() && win_path::component(map + L".json");
}

struct GameArguments {
    bool has_project = false;
    bool has_map = false;
    std::string project;
    std::string map;
};

// Für Rune3D sind --project und --map die einzigen Optionen. Ein Wert wird
// immer als nächstes Argument gelesen, auch wenn er selbst wie eine Option
// heißt: Ein Projekt namens "--map" bleibt so eindeutig darstellbar.
inline bool parse_game(int argc, wchar_t* const* argv, GameArguments& out) {
    out = {};
    GameArguments parsed;
    for (int i = 1; i < argc; ++i) {
        const std::wstring option(argv[i]);
        const bool is_project = option == L"--project";
        const bool is_map = option == L"--map";
        if (!is_project && !is_map) return false;
        if ((is_project && parsed.has_project) || (is_map && parsed.has_map) || ++i >= argc)
            return false;
        const std::wstring value(argv[i]);
        std::string utf8;
        if (!win_path::to_utf8(value, utf8) || (is_map && !valid_map_id(value))) return false;
        if (is_project) {
            parsed.has_project = true;
            parsed.project = std::move(utf8);
        } else {
            parsed.has_map = true;
            parsed.map = std::move(utf8);
        }
    }
    if (parsed.has_map && !parsed.has_project) return false;
    out = std::move(parsed);
    return true;
}

inline bool current_game(GameArguments& out) {
    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) { out = {}; return false; }
    const bool ok = parse_game(argc, argv, out);
    LocalFree(argv);
    return ok;
}

inline bool set_project_environment(const std::string& project) {
    std::wstring wide;
    return win_path::to_wide(project, wide) && SetEnvironmentVariableW(L"RUNE_PROJECT", wide.c_str());
}

inline bool set_start_map_environment(const std::string& map) {
    std::wstring wide;
    return win_path::to_wide(map, wide) && valid_map_id(wide) &&
           SetEnvironmentVariableW(L"RUNE_PLAYTEST_MAP", wide.c_str());
}

inline bool clear_start_map_environment() {
    if (SetEnvironmentVariableW(L"RUNE_PLAYTEST_MAP", nullptr)) return true;
    return GetLastError() == ERROR_ENVVAR_NOT_FOUND;
}

// Keine alte Kartenoption aus der Elternumgebung in einen normalen Start
// übernehmen. RUNE_PROJECT ohne CLI-Option bleibt aus Kompatibilität erhalten.
inline bool apply_game_environment(const GameArguments& args) {
    if (args.has_map && !args.has_project) return false;
    if (args.has_project && !set_project_environment(args.project)) return false;
    return args.has_map ? set_start_map_environment(args.map) : clear_start_map_environment();
}

} // namespace rune::core::win_project_cli
#endif
