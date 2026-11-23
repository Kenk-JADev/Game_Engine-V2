#pragma once
// Windows-Kommandozeile: Projektname ohne Umweg über die CRT-Codepage lesen.
#ifdef _WIN32
#include "core/win_path.h"
#include <shellapi.h>

#include <string>

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

inline bool set_project_environment(const std::string& project) {
    std::wstring wide;
    return win_path::to_wide(project, wide) && SetEnvironmentVariableW(L"RUNE_PROJECT", wide.c_str());
}

} // namespace rune::core::win_project_cli
#endif
