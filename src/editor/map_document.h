#pragma once
// Verlustgefährdende Kartendaten vor dem Austausch des geladenen Baums prüfen.
#include "editor/json.h"
#include "editor/json_validation.h"

#include <exception>
#include <string>
#include <utility>

namespace rune::editor {

enum class MapParseStatus {
    Ok, InvalidJson, InvalidRoot, InvalidUtf8, Nonfinite, DuplicateObjects, InvalidObjects
};

// Doppelte Schlüssel innerhalb einzelner Objekte bleiben erhalten und werden
// im Eigenschaftenformular bei Bedarf gesperrt. Ein doppeltes objects auf
// Wurzelebene wäre hingegen eine mehrdeutige Auswahl des gesamten Arrays.
inline MapParseStatus map_parse_document(const std::string& text, JsonValue& out) {
    if (!editor_json_raw_strings_valid(text)) return MapParseStatus::InvalidJson;
    JsonValue loaded;
    try { loaded = json_parse(text); }
    catch (const std::exception&) { return MapParseStatus::InvalidJson; }
    if (!loaded.is_obj()) return MapParseStatus::InvalidRoot;
    if (!editor_json_utf8(loaded)) return MapParseStatus::InvalidUtf8;
    if (!editor_json_serializable(loaded)) return MapParseStatus::Nonfinite;
    unsigned count = 0;
    const JsonValue* objects = nullptr;
    for (const auto& item : loaded.obj) {
        if (item.first == "objects") { ++count; objects = &item.second; }
    }
    if (count > 1) return MapParseStatus::DuplicateObjects;
    if (objects && !objects->is_arr()) return MapParseStatus::InvalidObjects;
    out = std::move(loaded);
    return MapParseStatus::Ok;
}

} // namespace rune::editor
