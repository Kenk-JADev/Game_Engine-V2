#pragma once
// ============================================================================
//  F9-Datenbankformular: verlustfreie Anzeige und transaktionale Übernahme.
//  Ohne Win32: nur tatsächlich geänderte Felder werden ersetzt; Gold ist
//  unabhängig von der aktuellen Auswahl. Ungültige Eingaben ändern nichts.
// ============================================================================

#include "editor/json.h"

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace rune::editor {

enum class DatabaseFieldKind { Text, Integer, Decimal, IdList };

struct DatabaseFieldSpec {
    const char* key;
    DatabaseFieldKind kind;
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

inline bool database_valid_utf8(const std::string& text);

// Einzeilige Win32-Editfelder können Steuerzeichen und ungültiges UTF-8
// nicht verlässlich anzeigen oder verlustfrei zurückgeben.
inline bool database_single_line(const std::string& text) {
    return database_valid_utf8(text) && std::none_of(text.begin(), text.end(), [](unsigned char c) {
        return c < 0x20 || c == 0x7f;
    });
}

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

inline std::string database_field_text(const JsonValue* value, DatabaseFieldKind kind) {
    if (kind == DatabaseFieldKind::IdList) return database_ids_text(value);
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
    if (kind == DatabaseFieldKind::Text) return old->is_str();
    return old->is_num();
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
    for (size_t i = 0; i < specs.size(); ++i) {
        if (before.fields[i] == after.fields[i]) continue;
        const DatabaseFieldSpec& spec = specs[i];
        if (!entry || !entry->is_obj()) {
            fail(spec.key, "Kein bearbeitbarer Eintrag ausgewählt.");
            return result;
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
        }
        if (!valid) {
            const char* message = spec.kind == DatabaseFieldKind::Integer ? "Eine ganze Zahl eingeben."
                : spec.kind == DatabaseFieldKind::Decimal ? "Eine endliche Zahl eingeben."
                : "Einzeiligen Text ohne Steuerzeichen eingeben.";
            fail(spec.key, message);
            return result;
        }
        if (!database_value_compatible(entry->find(spec.key), spec.kind)) result.replaces_invalid = true;
        result.entry.set(spec.key, std::move(replacement));
        result.entry_changed = result.changed = true;
    }
    return result;
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

// Nicht endliche Kommazahlen würden vom JSON-Schreiber zu null werden.
// Große Integer mit Originaltoken sind dagegen verlustfrei speicherbar.
inline bool database_document_serializable(const JsonValue& value) {
    if (value.is_num())
        return (value.is_int && !value.integer_text().empty()) || std::isfinite(value.number());
    if (value.is_arr()) {
        for (const JsonValue& item : value.arr)
            if (!database_document_serializable(item)) return false;
    }
    if (value.is_obj()) {
        for (const auto& pair : value.obj)
            if (!database_document_serializable(pair.second)) return false;
    }
    return true;
}

inline bool database_valid_utf8(const std::string& text) {
    for (size_t i = 0; i < text.size();) {
        const auto first = static_cast<unsigned char>(text[i]);
        if (first < 0x80) { ++i; continue; }
        size_t count = 0;
        unsigned codepoint = 0, minimum = 0;
        if (first >= 0xc2 && first <= 0xdf) { count = 2; codepoint = first & 0x1f; minimum = 0x80; }
        else if (first >= 0xe0 && first <= 0xef) { count = 3; codepoint = first & 0x0f; minimum = 0x800; }
        else if (first >= 0xf0 && first <= 0xf4) { count = 4; codepoint = first & 0x07; minimum = 0x10000; }
        else return false;
        if (text.size() - i < count) return false;
        for (size_t j = 1; j < count; ++j) {
            const auto c = static_cast<unsigned char>(text[i + j]);
            if ((c & 0xc0) != 0x80) return false;
            codepoint = (codepoint << 6) | (c & 0x3f);
        }
        if (codepoint < minimum || codepoint > 0x10ffff ||
            (codepoint >= 0xd800 && codepoint <= 0xdfff)) return false;
        i += count;
    }
    return true;
}

inline bool database_document_utf8(const JsonValue& value) {
    if (value.is_str()) return database_valid_utf8(value.str);
    if (value.is_arr()) {
        for (const JsonValue& item : value.arr)
            if (!database_document_utf8(item)) return false;
    }
    if (value.is_obj()) {
        for (const auto& pair : value.obj)
            if (!database_valid_utf8(pair.first) || !database_document_utf8(pair.second)) return false;
    }
    return true;
}

inline bool database_document_duplicates(const JsonValue& value) {
    if (value.is_arr()) {
        for (const JsonValue& item : value.arr)
            if (database_document_duplicates(item)) return true;
    }
    if (value.is_obj()) {
        std::unordered_set<std::string> keys;
        for (const auto& pair : value.obj) {
            if (!keys.insert(pair.first).second || database_document_duplicates(pair.second)) return true;
        }
    }
    return false;
}

// json_parse erhält alte Daten großzügig, nimmt aber rohe Steuerzeichen in
// Strings an. In F9 wäre das keine gültige JSON-Datei: lieber nicht laden.
inline bool database_raw_strings_valid(const std::string& text) {
    bool inside = false, escape = false;
    for (const unsigned char c : text) {
        if (!inside) { if (c == '"') inside = true; continue; }
        if (escape) { escape = false; continue; }
        if (c == '\\') escape = true;
        else if (c == '"') inside = false;
        else if (c < 0x20) return false;
    }
    return !inside && !escape;
}

enum class DatabaseParseStatus { Ok, InvalidJson, InvalidRoot, InvalidUtf8, DuplicateKey, Nonfinite };

// Der Vergleich verwendet Dateibytes, nicht das neu formatierte JSON. Auch
// Neuanlage und Löschung der Datei zählen als externe Änderung.
inline bool database_disk_changed(bool was_present, const std::string& was,
                                  bool is_present, const std::string& now) {
    return was_present != is_present || (was_present && was != now);
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
