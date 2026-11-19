#pragma once
// Gemeinsame Schutzprüfungen für JSON-Dokumente, die der Editor neu schreibt.
#include "editor/json.h"
#include "editor/text_validation.h"

#include <cmath>
#include <string>
#include <unordered_set>

namespace rune::editor {

// Originalbytes statt neu formatiertes JSON vergleichen. Auch eine entfernte
// oder neu angelegte Datei ist ein Konflikt.
inline bool editor_disk_changed(bool was_present, const std::string& was,
                                bool is_present, const std::string& now) {
    return was_present != is_present || (was_present && was != now);
}

// Nicht endliche Kommazahlen schreibt json_write als null. Große Integer mit
// Originaltoken sind dagegen auch bei unendlicher double-Näherung speicherbar.
inline bool editor_json_serializable(const JsonValue& value) {
    if (value.is_num())
        return (value.is_int && !value.integer_text().empty()) || std::isfinite(value.number());
    if (value.is_arr()) {
        for (const JsonValue& item : value.arr)
            if (!editor_json_serializable(item)) return false;
    }
    if (value.is_obj()) {
        for (const auto& pair : value.obj)
            if (!editor_json_serializable(pair.second)) return false;
    }
    return true;
}

inline bool editor_json_utf8(const JsonValue& value) {
    if (value.is_str()) return editor_valid_utf8(value.str);
    if (value.is_arr()) {
        for (const JsonValue& item : value.arr)
            if (!editor_json_utf8(item)) return false;
    }
    if (value.is_obj()) {
        for (const auto& pair : value.obj)
            if (!editor_valid_utf8(pair.first) || !editor_json_utf8(pair.second)) return false;
    }
    return true;
}

inline bool editor_json_duplicates(const JsonValue& value) {
    if (value.is_arr()) {
        for (const JsonValue& item : value.arr)
            if (editor_json_duplicates(item)) return true;
    }
    if (value.is_obj()) {
        std::unordered_set<std::string> keys;
        for (const auto& pair : value.obj) {
            if (!keys.insert(pair.first).second || editor_json_duplicates(pair.second)) return true;
        }
    }
    return false;
}

// Der Parser akzeptiert rohe Steuerzeichen in Strings großzügig; das ist
// kein gültiges JSON und könnte sonst beim Speichern still normalisiert werden.
inline bool editor_json_raw_strings_valid(const std::string& text) {
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

} // namespace rune::editor
