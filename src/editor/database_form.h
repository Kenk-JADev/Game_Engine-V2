#pragma once
// ============================================================================
//  F9-Datenbankformular: verlustfreie Anzeige und transaktionale Übernahme.
//  Ohne Win32: nur tatsächlich geänderte Felder werden ersetzt; Gold ist
//  unabhängig von der aktuellen Auswahl. Ungültige Eingaben ändern nichts.
// ============================================================================

#include "editor/json.h"
#include "editor/json_validation.h"
#include "editor/text_validation.h"

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

namespace rune::editor {

// Etappe 41: zusätzlich Auswahlfelder (XP-Dropdowns wie scope/occasion) und
// ein JSON-Feld für verschachtelte Daten (Common Events: commands). Beide
// bleiben verlustfrei: unbekannte Altdaten werden angezeigt, nicht ersetzt.
enum class DatabaseFieldKind { Text, Integer, Decimal, IdList, Choice, Json };

struct DatabaseFieldSpec {
    const char* key;
    DatabaseFieldKind kind;
    std::vector<std::string> choices;   // nur Choice: erlaubte Werte
    const char* label = nullptr;        // Beschriftung (UTF-8), sonst Schlüssel
    bool optional = false;              // darf in alten Projekten fehlen
};

struct DatabaseForm {
    std::vector<std::string> fields;
    std::string gold;
};

struct DatabaseEdit {
    bool ok = true;
    bool changed = false;
    bool entry_changed = false;
    bool gold_changed = false;
    bool gold_error = false;
    bool replaces_invalid = false;
    std::string field;
    std::string error;
    JsonValue entry;
    JsonValue gold;
};

inline std::string database_trim(const std::string& text) {
    size_t first = 0, last = text.size();
    while (first < last && std::isspace(static_cast<unsigned char>(text[first]))) ++first;
    while (last > first && std::isspace(static_cast<unsigned char>(text[last - 1]))) --last;
    return text.substr(first, last - first);
}

// Einzeilige Win32-Editfelder können Steuerzeichen und ungültiges UTF-8
// nicht verlässlich anzeigen oder verlustfrei zurückgeben.
inline bool database_single_line(const std::string& text) { return editor_single_line(text); }

inline std::string database_number_text(const JsonValue* value) {
    if (!value || value->is_null()) return "0";
    if (!value->is_num()) return "[ungültig]";
    if ((!value->is_int || value->integer_text().empty()) && !std::isfinite(value->number()))
        return "[nicht darstellbar]";
    std::string text = json_write(*value);
    if (!text.empty() && text.back() == '\n') text.pop_back();
    return text;
}

inline bool database_ids_representable(const JsonValue* value) {
    if (!value || value->is_null()) return true;
    if (!value->is_arr()) return false;
    for (const JsonValue& id : value->arr) {
        if (!id.is_str() || id.str.empty() || !database_single_line(id.str) ||
            database_trim(id.str) != id.str || id.str.find_first_of(",;") != std::string::npos)
            return false;
    }
    return true;
}

inline std::string database_ids_text(const JsonValue* value) {
    if (!value || value->is_null()) return {};
    if (!value->is_arr()) return "[ungültige Gegnerliste]";
    std::string result;
    for (size_t i = 0; i < value->arr.size(); ++i) {
        if (i) result += ", ";
        const JsonValue& id = value->arr[i];
        if (!id.is_str()) result += "[ungültiger Eintrag]";
        else if (id.str.empty()) result += "[leere ID]";
        else if (!database_single_line(id.str)) result += "[nicht darstellbare ID]";
        else result += id.str;
    }
    return result;
}

// Verschachtelte Daten (Common-Event-Befehle) zeigt F9 als JSON-Text. Die
// Anzeige ist die kanonische Schreibweise des Editors; ein unverändertes
// Feld schreibt deshalb nichts zurück. Nicht darstellbare Altdaten werden
// gesperrt statt ersetzt.
inline bool database_json_representable(const JsonValue* value) {
    if (!value || value->is_null()) return true;
    if (!value->is_arr() && !value->is_obj()) return false;
    return editor_json_utf8(*value) && editor_json_serializable(*value) &&
           !editor_json_duplicates(*value);
}

inline std::string database_json_text(const JsonValue* value) {
    if (!value || value->is_null()) return "[]";
    if (!value->is_arr() && !value->is_obj()) return "[ungültig]";
    if (!database_json_representable(value)) return "[nicht darstellbar]";
    std::string text = json_write(*value);
    if (!text.empty() && text.back() == '\n') text.pop_back();
    return text;
}

// Win32-Mehrzeiler liefern CRLF. Für den Vergleich und für JSON zählt LF.
inline std::string database_lines_lf(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\r') {
            if (i + 1 < text.size() && text[i + 1] == '\n') continue;
            out += '\n';
        } else out += text[i];
    }
    return out;
}

inline bool database_json_parse(const std::string& input, JsonValue& value) {
    const std::string text = database_lines_lf(input);
    if (text.empty() || !editor_json_raw_strings_valid(text)) return false;
    JsonValue parsed;
    try {
        parsed = json_parse(text);
    } catch (const std::exception&) {
        return false;
    }
    if (!parsed.is_arr() && !parsed.is_obj()) return false;
    if (!editor_json_utf8(parsed) || editor_json_duplicates(parsed) ||
        !editor_json_serializable(parsed)) return false;
    value = std::move(parsed);
    return true;
}

// Auswahlfelder prüfen nur echte Änderungen: ein unbekannter Altwert bleibt
// stehen, solange ihn niemand anfasst (wie bei allen anderen Feldarten).
inline bool database_choice_valid(const DatabaseFieldSpec& spec, const std::string& text) {
    if (!database_single_line(text)) return false;
    for (const std::string& choice : spec.choices)
        if (choice == text) return true;
    return false;
}

inline bool database_choice_known(const DatabaseFieldSpec& spec, const std::string& text) {
    for (const std::string& choice : spec.choices)
        if (choice == text) return true;
    return false;
}

inline std::string database_choice_list(const DatabaseFieldSpec& spec) {
    std::string text;
    for (const std::string& choice : spec.choices) {
        if (!text.empty()) text += "/";
        text += choice;
    }
    return text;
}

inline std::string database_field_text(const JsonValue* value, DatabaseFieldKind kind) {
    if (kind == DatabaseFieldKind::IdList) return database_ids_text(value);
    if (kind == DatabaseFieldKind::Json) return database_json_text(value);
    if (kind == DatabaseFieldKind::Integer || kind == DatabaseFieldKind::Decimal)
        return database_number_text(value);
    if (!value || value->is_null()) return {};
    return value->is_str() ? value->str : "[ungültig]";
}

inline DatabaseForm database_form_read(const JsonValue& root, const JsonValue* entry,
                                        const std::vector<DatabaseFieldSpec>& specs) {
    DatabaseForm form;
    form.gold = database_number_text(root.find("gold"));
    for (const DatabaseFieldSpec& spec : specs)
        form.fields.push_back(entry && entry->is_obj()
            ? database_field_text(entry->find(spec.key), spec.kind) : std::string());
    return form;
}

// Beliebig große neue Ganzzahlen bleiben als Dezimaltext exakt; führende
// Nullen und ein optionales Plus werden für gültiges JSON normalisiert.
inline bool database_integer(const std::string& input, JsonValue& value) {
    std::string s = database_trim(input);
    if (s.empty()) return false;
    const bool negative = s[0] == '-';
    size_t first = (s[0] == '-' || s[0] == '+') ? 1 : 0;
    if (first == s.size()) return false;
    for (size_t i = first; i < s.size(); ++i)
        if (s[i] < '0' || s[i] > '9') return false;
    while (first + 1 < s.size() && s[first] == '0') ++first;
    std::string token = s.substr(first);
    if (negative && token != "0") token.insert(token.begin(), '-');
    value = JsonValue::make_integer(std::move(token));
    return true;
}

inline bool database_decimal(std::string input, JsonValue& value) {
    input = database_trim(input);
    if (input.empty() || input.find('\0') != std::string::npos) return false;
    std::replace(input.begin(), input.end(), ',', '.');
    // strtod akzeptiert auch Hexadezimalzahlen; das Formular tut es nicht.
    for (const char c : input) {
        if ((c < '0' || c > '9') && c != '.' && c != '+' && c != '-' && c != 'e' && c != 'E')
            return false;
    }
    errno = 0;
    char* end = nullptr;
    const double n = std::strtod(input.c_str(), &end);
    if (end == input.c_str() || *end || errno == ERANGE || !std::isfinite(n)) return false;
    value = JsonValue::make_num(n, false);
    return true;
}

inline bool database_ids_parse(const std::string& text, JsonValue& value) {
    if (!database_single_line(text)) return false;
    value = JsonValue::make_arr();
    size_t start = 0;
    while (true) {
        const size_t end = text.find_first_of(",;", start);
        const std::string id = database_trim(text.substr(start, end == std::string::npos ? end : end - start));
        if (!database_single_line(id)) return false;
        if (!id.empty()) value.arr.push_back(JsonValue::make_str(id));
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return true;
}

inline bool database_value_compatible(const JsonValue* old, DatabaseFieldKind kind) {
    if (!old || old->is_null()) return true;
    if (kind == DatabaseFieldKind::IdList) return database_ids_representable(old);
    if (kind == DatabaseFieldKind::Json)
        return (old->is_arr() || old->is_obj()) && editor_json_utf8(*old) &&
               editor_json_serializable(*old) && !editor_json_duplicates(*old);
    if (kind == DatabaseFieldKind::Text || kind == DatabaseFieldKind::Choice) return old->is_str();
    return old->is_num();
}

// Ein Auswahlfeld mit unbekanntem Altwert gilt zusätzlich als Ersatz alter
// Daten: der Nutzer soll die Rückfrage sehen, bevor er ihn überschreibt.
inline bool database_spec_compatible(const JsonValue* old, const DatabaseFieldSpec& spec) {
    if (!database_value_compatible(old, spec.kind)) return false;
    if (spec.kind == DatabaseFieldKind::Choice && old && old->is_str() &&
        !database_choice_known(spec, old->str)) return false;
    return true;
}

// Feldweise Änderung eines Eintrags. Wird von der Listen- und der
// System-Variante benutzt; bei einem Fehler bleibt `result` unverändert nutzlos
// (ok = false) und der Aufrufer schreibt nichts.
inline bool database_entry_fields(const JsonValue* entry,
                                  const std::vector<DatabaseFieldSpec>& specs,
                                  const DatabaseForm& before, const DatabaseForm& after,
                                  DatabaseEdit& result) {
    for (size_t i = 0; i < specs.size(); ++i) {
        if (before.fields[i] == after.fields[i]) continue;
        const DatabaseFieldSpec& spec = specs[i];
        if (!entry || !entry->is_obj()) {
            result.ok = false;
            result.changed = result.entry_changed = result.gold_changed = false;
            result.field = spec.key;
            result.error = "Kein bearbeitbarer Eintrag ausgewählt.";
            return false;
        }
        JsonValue replacement;
        bool valid = true;
        switch (spec.kind) {
            case DatabaseFieldKind::Text:
                valid = database_single_line(after.fields[i]);
                replacement = JsonValue::make_str(after.fields[i]);
                break;
            case DatabaseFieldKind::Integer:
                valid = database_integer(after.fields[i], replacement);
                break;
            case DatabaseFieldKind::Decimal:
                valid = database_decimal(after.fields[i], replacement);
                break;
            case DatabaseFieldKind::IdList:
                valid = database_ids_parse(after.fields[i], replacement);
                break;
            case DatabaseFieldKind::Choice:
                valid = database_choice_valid(spec, after.fields[i]);
                replacement = JsonValue::make_str(after.fields[i]);
                break;
            case DatabaseFieldKind::Json:
                valid = database_json_parse(after.fields[i], replacement);
                break;
        }
        if (!valid) {
            const char* message = spec.kind == DatabaseFieldKind::Integer ? "Eine ganze Zahl eingeben."
                : spec.kind == DatabaseFieldKind::Decimal ? "Eine endliche Zahl eingeben."
                : spec.kind == DatabaseFieldKind::Choice ? "Einen der angebotenen Werte eingeben."
                : spec.kind == DatabaseFieldKind::Json ? "Gültiges JSON (Liste oder Objekt) eingeben."
                : "Einzeiligen Text ohne Steuerzeichen eingeben.";
            result.ok = false;
            result.changed = result.entry_changed = result.gold_changed = false;
            result.field = spec.key;
            result.error = spec.kind == DatabaseFieldKind::Choice
                ? std::string(message) + " Erlaubt: " + database_choice_list(spec)
                : std::string(message);
            return false;
        }
        if (!database_spec_compatible(entry->find(spec.key), spec)) result.replaces_invalid = true;
        result.entry.set(spec.key, std::move(replacement));
        result.entry_changed = result.changed = true;
    }
    return true;
}

// Erst vorbereiten und gegebenenfalls bestätigen, danach database_form_commit.
// Auch wenn Gold oder ein späteres Feld fehlschlägt, wird nichts verändert.
inline DatabaseEdit database_form_prepare(const JsonValue& root, const JsonValue* entry,
                                            const std::vector<DatabaseFieldSpec>& specs,
                                            const DatabaseForm& before, const DatabaseForm& after) {
    DatabaseEdit result;
    auto fail = [&result](const std::string& field, const std::string& error) {
        result.ok = false;
        result.changed = result.entry_changed = result.gold_changed = false;
        result.field = field;
        result.error = error;
    };
    if (!root.is_obj() || before.fields.size() != specs.size() || after.fields.size() != specs.size()) {
        fail("", "Das Datenbankformular ist nicht mehr gültig.");
        return result;
    }
    if (before.gold != after.gold) {
        if (!database_integer(after.gold, result.gold)) {
            fail("gold", "Gold muss eine ganze Zahl sein.");
            result.gold_error = true;
            return result;
        }
        result.gold_changed = result.changed = true;
        if (!database_value_compatible(root.find("gold"), DatabaseFieldKind::Integer))
            result.replaces_invalid = true;
    }
    if (entry && entry->is_obj()) result.entry = *entry;
    database_entry_fields(entry, specs, before, after, result);
    return result;
}

// System ist wie in XP ein einzelner Datensatz statt einer Liste. Er liegt
// als Objekt unter "system" in database.json; fehlende Schlüssel entstehen
// erst, wenn ein Feld wirklich geändert wird.
inline DatabaseEdit database_system_prepare(const JsonValue& root, const JsonValue* entry,
                                              const std::vector<DatabaseFieldSpec>& specs,
                                              const DatabaseForm& before, const DatabaseForm& after) {
    DatabaseEdit result;
    if (!root.is_obj() || before.fields.size() != specs.size() || after.fields.size() != specs.size()) {
        result.ok = false;
        result.error = "Das Datenbankformular ist nicht mehr gültig.";
        return result;
    }
    if (before.gold != after.gold) {
        if (!database_integer(after.gold, result.gold)) {
            result.ok = false;
            result.gold_error = true;
            result.field = "gold";
            result.error = "Gold muss eine ganze Zahl sein.";
            return result;
        }
        result.gold_changed = result.changed = true;
        if (!database_value_compatible(root.find("gold"), DatabaseFieldKind::Integer))
            result.replaces_invalid = true;
    }
    const bool present = entry && !entry->is_null();
    if (present && !entry->is_obj()) result.replaces_invalid = true;
    if (entry && entry->is_obj()) result.entry = *entry;
    else result.entry = JsonValue::make_obj();
    // Ein fehlendes oder kaputtes Objekt darf Felder aufnehmen; die Rückfrage
    // über replaces_invalid deckt das Ersetzen alter Daten ab. Jedes Feld wird
    // genau einmal geprüft, deshalb liest die Prüfung hier noch den Altwert.
    database_entry_fields(&result.entry, specs, before, after, result);
    return result;
}

inline bool database_system_commit(JsonValue& root, const char* key, const DatabaseEdit& edit) {
    if (!edit.ok || !root.is_obj()) return false;
    if (edit.entry_changed) {
        if (!edit.entry.is_obj()) return false;
        root.set(key, edit.entry);
    }
    if (edit.gold_changed) root.set("gold", edit.gold);
    return true;
}

inline bool database_form_commit(JsonValue& root, const char* key, int index, const DatabaseEdit& edit) {
    if (!edit.ok || !root.is_obj()) return false;
    JsonValue* entry = nullptr;
    if (edit.entry_changed) {
        JsonValue* array = root.find(key);
        if (!array || !array->is_arr() || index < 0 || static_cast<size_t>(index) >= array->arr.size() ||
            !array->arr[static_cast<size_t>(index)].is_obj()) return false;
        entry = &array->arr[static_cast<size_t>(index)];
    }
    if (entry) *entry = edit.entry;
    // set() kann die Wurzel-Paarliste verschieben; den alten Eintragszeiger
    // danach nicht mehr verwenden (insbesondere bei zuvor fehlendem Gold).
    if (edit.gold_changed) root.set("gold", edit.gold);
    return true;
}

// F9 benutzt dieselben UTF-8-/Serialisierbarkeitsprüfungen wie Karten;
// doppelte Schlüssel sind in Datenbanken darüber hinaus generell verboten.
inline bool database_valid_utf8(const std::string& text) { return editor_valid_utf8(text); }
inline bool database_document_serializable(const JsonValue& value) { return editor_json_serializable(value); }
inline bool database_document_utf8(const JsonValue& value) { return editor_json_utf8(value); }
inline bool database_document_duplicates(const JsonValue& value) { return editor_json_duplicates(value); }
inline bool database_raw_strings_valid(const std::string& text) { return editor_json_raw_strings_valid(text); }

enum class DatabaseParseStatus { Ok, InvalidJson, InvalidRoot, InvalidUtf8, DuplicateKey, Nonfinite };

// Der Vergleich verwendet Dateibytes, nicht das neu formatierte JSON. Auch
// Neuanlage und Löschung der Datei zählen als externe Änderung.
inline bool database_disk_changed(bool was_present, const std::string& was,
                                  bool is_present, const std::string& now) {
    return editor_disk_changed(was_present, was, is_present, now);
}

// Bei jedem Fehler bleibt der bisherige Ausgabebaum unangetastet. Das gilt
// auch für mehrdeutige doppelte Schlüssel und nicht speicherbare Zahlen.
inline DatabaseParseStatus database_parse_document(const std::string& text, JsonValue& out) {
    if (!database_raw_strings_valid(text)) return DatabaseParseStatus::InvalidJson;
    JsonValue loaded;
    try {
        loaded = json_parse(text);
    } catch (const std::exception&) {
        return DatabaseParseStatus::InvalidJson;
    }
    if (!loaded.is_obj()) return DatabaseParseStatus::InvalidRoot;
    if (!database_document_utf8(loaded)) return DatabaseParseStatus::InvalidUtf8;
    if (database_document_duplicates(loaded)) return DatabaseParseStatus::DuplicateKey;
    if (!database_document_serializable(loaded)) return DatabaseParseStatus::Nonfinite;
    out = std::move(loaded);
    return DatabaseParseStatus::Ok;
}

} // namespace rune::editor
