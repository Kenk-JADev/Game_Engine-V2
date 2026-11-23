#pragma once
// Lexikalischer Schutz für einzelne RGBA-Dateinamen im Projektordner.
// Das ersetzt keine Prüfung von Windows-Reparsepunkten oder Dateikonflikten.
#include <algorithm>
#include <cstddef>
#include <string>

namespace rune::editor {

inline bool texture_file_is_flat(const std::string& name) {
    // Konservatives Byte-Limit; es vermeidet auch Komponenten über dem
    // üblichen Windows-Namenslimit. Logische Texturnamen dürfen länger sein.
    if (name.empty() || name.size() > 255 || name.back() == '.' || name.back() == ' ') return false;
    for (const unsigned char c : name) {
        if (c < 0x20 || c == 0x7f || c == '/' || c == '\\' || c == ':' || c == '*' ||
            c == '?' || c == '"' || c == '<' || c == '>' || c == '|') return false;
    }
    // Windows behandelt Gerätenamen auch mit Erweiterung als Geräte statt
    // Dateien. Vor dem ersten Punkt unabhängig von der System-Locale vergleichen.
    std::string stem = name.substr(0, name.find('.'));
    while (!stem.empty() && stem.back() == ' ') stem.pop_back();
    std::transform(stem.begin(), stem.end(), stem.begin(), [](unsigned char c) {
        return c >= 'a' && c <= 'z' ? static_cast<char>(c - 'a' + 'A') : static_cast<char>(c);
    });
    if (stem == "CON" || stem == "PRN" || stem == "AUX" || stem == "NUL" ||
        stem == "CONIN$" || stem == "CONOUT$") return false;
    if (stem.size() >= 4 && (stem.compare(0, 3, "COM") == 0 || stem.compare(0, 3, "LPT") == 0)) {
        const std::string suffix = stem.substr(3);
        if ((suffix.size() == 1 && suffix[0] >= '1' && suffix[0] <= '9') ||
            suffix == "\xC2\xB9" || suffix == "\xC2\xB2" || suffix == "\xC2\xB3") return false;
    }
    return true;
}

} // namespace rune::editor
