#pragma once
// ============================================================================
//  Z8: Regeln für Event-Kommandos, die der Editor-Issue-Scan und MapCheck
//  (game/engine/mapcheck.rb) gleich auslegen müssen. Portabel, damit CI sie
//  prüfen kann (`tools/editor_workflow_test.cpp`) - der Scan selbst lebt in
//  `editor_main.cpp` und braucht Win32 + geöffnete Karte.
// ============================================================================

#include <string>

namespace rune::editor {

// Kommandos mit "then"/"else"-Zweigen: beide müssen Arrays sein und werden
// rekursiv geprüft. Bis Z8 fehlte hier `if_self_switch` - Fehler in seinen
// Zweigen erschienen im Editor-Backlog (F10) nicht, obwohl MapCheck sie fand.
// Etappe 20d: if_gold/if_item/if_script bringen dieselben Zweige mit.
// Etappe 20f: if_battle_result (XP 601-603) ebenso.
inline bool event_cmd_has_branches(const std::string& cmd) {
    return cmd == "if_switch" || cmd == "if_variable" || cmd == "if_self_switch" ||
           cmd == "if_gold" || cmd == "if_item" || cmd == "if_script" ||
           cmd == "if_battle_result";
}

// Kommandos, die einen Self-Switch-Buchstaben tragen (A-D).
inline bool event_cmd_uses_self_switch_letter(const std::string& cmd) {
    return cmd == "self_switch" || cmd == "if_self_switch";
}

// Gültige Buchstaben wie GameState.set_self_switch (Ruby normalisiert mit
// upcase; der Scan vergleicht die Rohdaten und meldet alles andere).
inline bool event_self_switch_letter_ok(const std::string& letter) {
    return letter == "A" || letter == "B" || letter == "C" || letter == "D";
}

} // namespace rune::editor
