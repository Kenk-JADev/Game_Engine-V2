#pragma once
// Etappe 28: XP-Defaultscripts im Script-Editor (F11). Neue Projekte bekommen
// alle 90 Dateien aus XP-Defaultscript/ als Einträge in data/scripts.json:
//   {"name":"000 - Game_Temp","xp_default":"000","autoload":true,"code":"..."}
// Die Laufzeit (game/engine/scripts.rb) lädt sie vor dem Spielstart im
// Namensraum XP. Reine Logik ohne Win32, damit kopflos testbar.
// Die Autoload-Vorgabe muss zu game/engine/xp_manifest.rb passen
// (NEEDS_RGSS / BLOCKED); tools/xp_scripts_test.rb vergleicht beides.

#include "editor/json.h"

#include <string>
#include <string_view>

namespace rune::editor {

// Nummer aus "NNN - Name.rb"; -1 bei fremden Dateinamen.
inline int xp_default_number(std::string_view file) {
    if (file.size() < 10 || file.substr(3, 3) != " - " || file.substr(file.size() - 3) != ".rb") return -1;
    int n = 0;
    for (int i = 0; i < 3; ++i) {
        if (file[i] < '0' || file[i] > '9') return -1;
        n = n * 10 + (file[i] - '0');
    }
    return n;
}

// Standardmäßig aktiv: nur Dateien, deren Klassenrumpf ohne RGSS-Grafik
// lädt. 023-026/029-062 brauchen Sprite/Window/RPG::Sprite, 089 Main läuft nie.
inline bool xp_default_autoload(int n) {
    if (n == 89) return false;
    if (n >= 23 && n <= 26) return false;
    if (n >= 29 && n <= 62) return false;
    return n >= 0 && n <= 88;
}

inline JsonValue xp_default_entry(std::string_view file, std::string code) {
    const int n = xp_default_number(file);
    if (code.size() >= 3 && static_cast<unsigned char>(code[0]) == 0xEF &&
        static_cast<unsigned char>(code[1]) == 0xBB && static_cast<unsigned char>(code[2]) == 0xBF)
        code.erase(0, 3); // UTF-8-BOM entfernen
    JsonValue e = JsonValue::make_obj();
    e.set("name", JsonValue::make_str(std::string(file.substr(0, file.size() - 3))));
    e.set("xp_default", JsonValue::make_str(std::string(file.substr(0, 3))));
    e.set("autoload", JsonValue::make_bool(xp_default_autoload(n)));
    e.set("code", JsonValue::make_str(std::move(code)));
    return e;
}

} // namespace rune::editor
