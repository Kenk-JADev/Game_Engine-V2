#pragma once
// Gemeinsame Windows-Suche für die Projektliste und Kartenliste des Editors.
// Rückgaben und Eingaben sind UTF-8; die Dateisuche selbst ist UTF-16.
#ifdef _WIN32
#include "core/win_path.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace rune::editor {

inline std::vector<std::string> project_names_on_disk(const std::string& root) {
    std::vector<std::string> names;
    std::vector<core::win_path::DirectoryEntry> entries;
    if (!core::win_path::list(root + "\\projekte\\*", entries)) return names;
    for (const auto& entry : entries) {
        if (!(entry.attributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
        if (core::win_path::path_exists(root + "\\projekte\\" + entry.name + "\\data\\system.json"))
            names.push_back(entry.name);
    }
    std::sort(names.begin(), names.end());
    return names;
}

inline std::vector<std::string> map_files_on_disk(const std::string& project_data) {
    std::vector<std::string> names;
    std::vector<core::win_path::DirectoryEntry> entries;
    if (!core::win_path::list(project_data + "\\maps\\*.json", entries)) return names;
    for (auto& entry : entries) {
        if ((entry.attributes & FILE_ATTRIBUTE_DIRECTORY) || entry.name.size() < 5) continue;
        // FindFirstFileW kann auch über 8.3-Kurznamen ein Muster treffen.
        // Die eigentliche Erweiterung der langen Datei muss wirklich .json sein.
        std::string ext = entry.name.substr(entry.name.size() - 5);
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
            return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : static_cast<char>(c);
        });
        if (ext == ".json") names.push_back(std::move(entry.name));
    }
    return names;
}

} // namespace rune::editor
#endif
