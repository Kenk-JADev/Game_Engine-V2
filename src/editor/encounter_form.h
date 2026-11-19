#pragma once
// ============================================================================
//  Datenmodell für Zufallskämpfe im Karten-Eigenschaften-Dialog (Etappe 19m).
//  Reines C++ ohne Win32: Laden, Validieren und Vorbereiten sind kopflos
//  testbar. Die Karte wird erst nach Bestätigung im Dialog geändert.
// ============================================================================

#include "editor/json.h"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <string>
#include <system_error>
#include <vector>

namespace rune::editor {

// Der JSON-Editor speichert Zahlen als double; bis hier sind ganze Zahlen
// exakt. So geht eine im Dialog eingegebene Schrittzahl nicht beim Speichern
// oder beim Ruby-Roundtrip verloren.
inline constexpr std::uint64_t ENCOUNTER_MAX_STEPS = 9007199254740991ULL;

inline bool encounter_text_present(const std::string& text) {
    return std::any_of(text.begin(), text.end(), [](unsigned char c) {
        return c != '\0' && !std::isspace(c);
    });
}

// Steuerzeichen (einschließlich Zeilenumbrüchen, Tab und NUL) lassen sich im
// zeilenweisen Win32-Editfeld nicht zuverlässig bearbeiten. Normale
// Leerzeichen dagegen gehören zur ID und bleiben erhalten.
inline bool encounter_id_editable(const std::string& id) {
    return encounter_text_present(id) && std::none_of(id.begin(), id.end(), [](unsigned char c) {
        return c < 0x20 || c == 0x7f;
    });
}

struct EncounterForm {
    std::string troops;          // eine ID je Zeile, inklusive Duplikaten
    std::string steps = "30";    // Default, falls Feld fehlt oder null ist
    bool bad_object = false;     // encounters ist weder Objekt noch null
    bool bad_troops = false;     // nicht verlustfrei im Textfeld darstellbar
    bool bad_steps = false;      // nicht als positive ganze Schrittzahl darstellbar
};

inline EncounterForm encounter_form_read(const JsonValue* enc) {
    EncounterForm form;
    if (!enc || enc->is_null()) return form;
    if (!enc->is_obj()) {
        form.bad_object = true;
        return form;
    }

    const JsonValue* troops = enc->find("troops");
    if (troops && !troops->is_null()) {
        if (!troops->is_arr()) {
            form.bad_troops = true;
        } else {
            for (const JsonValue& id : troops->arr) {
                if (!id.is_str() || !encounter_id_editable(id.str)) {
                    form.bad_troops = true;
                    continue;
                }
                if (!form.troops.empty()) form.troops += "\r\n";
                form.troops += id.str;
            }
        }
    }

    const JsonValue* steps = enc->find("steps");
    if (steps && !steps->is_null()) {
        const double n = steps->number(0.0);
        if (steps->is_num() && std::isfinite(n) && n >= 1.0 &&
            n <= static_cast<double>(ENCOUNTER_MAX_STEPS) && n == std::floor(n)) {
            form.steps = std::to_string(static_cast<std::uint64_t>(n));
        } else {
            form.bad_steps = true;
        }
    }
    return form;
}

// Zeilen nicht trimmen: Leerzeichen können Teil einer gültigen Truppen-ID
// sein. Nur wirklich leere/whitespace-only Zeilen werden ignoriert.
inline bool encounter_parse_troops(const std::string& lines, JsonValue& out) {
    out = JsonValue::make_arr();
    size_t start = 0;
    while (start <= lines.size()) {
        const size_t end = lines.find_first_of("\r\n", start);
        const std::string id = lines.substr(start, end == std::string::npos ? end : end - start);
        if (id.find('\0') != std::string::npos) return false;
        if (encounter_text_present(id)) {
            if (!encounter_id_editable(id)) return false;
            out.arr.push_back(JsonValue::make_str(id));
        }
        if (end == std::string::npos) break;
        start = end + 1;
        if (lines[end] == '\r' && start < lines.size() && lines[start] == '\n') ++start;
    }
    return true;
}

inline bool encounter_parse_steps(const std::string& input, std::uint64_t& steps) {
    size_t first = 0;
    size_t last = input.size();
    while (first < last && std::isspace(static_cast<unsigned char>(input[first]))) ++first;
    while (last > first && std::isspace(static_cast<unsigned char>(input[last - 1]))) --last;
    if (first == last) {
        steps = 30;
        return true;
    }
    std::uint64_t value = 0;
    const char* begin = input.data() + first;
    const char* end = input.data() + last;
    const auto parsed = std::from_chars(begin, end, value, 10);
    if (parsed.ec != std::errc() || parsed.ptr != end || value == 0 ||
        value > ENCOUNTER_MAX_STEPS) return false;
    steps = value;
    return true;
}

struct EncounterEdit {
    bool ok = true;
    bool changed = false;
    bool troops_changed = false;
    bool replaces_invalid = false;
    bool troops_error = false;
    std::string error;
    JsonValue value;
};

// Bereitet eine Änderung vor, ohne die Karte zu mutieren. Unbearbeitete
// Felder und unbekannte Schlüssel bleiben exakt erhalten. Ungültige Altwerte
// werden nur auf ausdrücklichen Wunsch (UI-Bestätigung) ersetzt.
inline EncounterEdit encounter_form_prepare(const JsonValue* original, const EncounterForm& before,
                                             const std::string& troops, const std::string& steps,
                                             bool troops_touched, bool steps_touched) {
    EncounterEdit result;
    const bool change_troops = troops_touched &&
        (before.bad_object || before.bad_troops || troops != before.troops);
    const bool change_steps = steps_touched &&
        (before.bad_object || before.bad_steps || steps != before.steps);
    if (!change_troops && !change_steps) return result;

    JsonValue parsed_troops;
    std::uint64_t parsed_steps = 30;
    if (change_troops && !encounter_parse_troops(troops, parsed_troops)) {
        result.ok = false;
        result.troops_error = true;
        result.error = "Truppen-IDs dürfen keine Steuerzeichen enthalten.";
        return result;
    }
    if (change_steps && !encounter_parse_steps(steps, parsed_steps)) {
        result.ok = false;
        result.error = "Bitte eine ganze Schrittzahl von 1 bis 9007199254740991 eingeben "
                       "(leeres Feld = 30).";
        return result;
    }

    result.value = (original && original->is_obj()) ? *original : JsonValue::make_obj();
    if (change_troops) result.value.set("troops", std::move(parsed_troops));
    if (change_steps) result.value.set("steps", JsonValue::make_num(static_cast<double>(parsed_steps), true));
    result.changed = true;
    result.troops_changed = change_troops;
    result.replaces_invalid = before.bad_object || (change_troops && before.bad_troops) ||
                              (change_steps && before.bad_steps);
    return result;
}

struct EncounterTroopChoice {
    std::string id;
    std::string name;
};

// Nur tatsächlich verwendbare IDs anbieten; Duplikate in der Datenbank
// zusammenfassen. Duplikate in der Begegnungsliste bleiben dagegen erlaubt.
inline std::vector<EncounterTroopChoice> encounter_database_choices(const JsonValue& db) {
    std::vector<EncounterTroopChoice> choices;
    const JsonValue* troops = db.find("troops");
    if (!db.is_obj() || !troops || !troops->is_arr()) return choices;
    for (const JsonValue& troop : troops->arr) {
        if (!troop.is_obj()) continue;
        const JsonValue* id = troop.find("id");
        if (!id || !id->is_str() || !encounter_id_editable(id->str)) continue;
        if (std::any_of(choices.begin(), choices.end(), [&](const EncounterTroopChoice& c) {
            return c.id == id->str;
        })) continue;
        const JsonValue* name = troop.find("name");
        choices.push_back({id->str, name && name->is_str() ? name->str : std::string()});
    }
    return choices;
}

// Battle.start akzeptiert neben IDs auch Truppennamen. Die Prüfung schaut
// deshalb in ALLE Datensätze (auch solche mit doppelter oder fehlender ID),
// nicht nur in die deduplizierte Auswahlliste. Unbekanntes bleibt erlaubt;
// der Dialog warnt lediglich vor dem Übernehmen.
inline std::vector<std::string> encounter_unknown_troops(const JsonValue& troops,
                                                          const JsonValue& db) {
    std::vector<std::string> unknown;
    if (!troops.is_arr()) return unknown;
    const JsonValue* definitions = db.is_obj() ? db.find("troops") : nullptr;
    for (const JsonValue& id : troops.arr) {
        if (!id.is_str()) continue;
        bool known = false;
        if (definitions && definitions->is_arr()) {
            for (const JsonValue& def : definitions->arr) {
                if (!def.is_obj()) continue;
                const JsonValue* ref_id = def.find("id");
                const JsonValue* ref_name = def.find("name");
                if ((ref_id && ref_id->is_str() && ref_id->str == id.str) ||
                    (ref_name && ref_name->is_str() && ref_name->str == id.str)) {
                    known = true;
                    break;
                }
            }
        }
        if (!known && std::find(unknown.begin(), unknown.end(), id.str) == unknown.end()) {
            unknown.push_back(id.str);
        }
    }
    return unknown;
}

} // namespace rune::editor
