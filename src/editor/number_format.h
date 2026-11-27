#pragma once
// Anzeige für numerische Felder im Event-/Datenbankeditor. Nicht zum
// Speichern von JSON verwenden: die Anzeige kann Kommazahlen runden.
#include <algorithm>
#include <cstdio>
#include <limits>
#include <string>

namespace rune::editor {

inline std::string editor_number_text(double value, int precision = 2) {
    char buf[std::numeric_limits<double>::max_exponent10 + 64];
    std::snprintf(buf, sizeof(buf), "%.*f", std::clamp(precision, 0, 17), value);
    std::string s = buf;
    // Nur Nullen HINTER dem Dezimalpunkt entfernen! Bei Präzision 0 würde
    // z. B. 100 sonst fälschlich als 1 angezeigt und beim Übernehmen zerstört.
    if (s.find('.') != std::string::npos) {
        while (!s.empty() && s.back() == '0') s.pop_back();
        if (!s.empty() && s.back() == '.') s.pop_back();
    }
    if (s.empty() || s == "-" || s == "-0") s = "0";
    return s;
}

} // namespace rune::editor
