#pragma once
// Verlustfreies Modell für textures.json. Ungültige Einträge bleiben im Baum;
// nur eindeutige, sicher ladbare Einträge erscheinen als Materialien.
#include "editor/json.h"
#include "editor/json_validation.h"
#include "editor/texture_path.h"
#include "world/terrain_limits.h"
#include "world/texture_limits.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace rune::editor {

struct MaterialInfo {
    std::string name;
    std::string file;
    std::string source;
    int w = 0, h = 0;
    double tile = 2.0;
    bool tile_valid = true;
    bool imported = false;
    std::size_t document_index = 0;
};

// Etappe 38: Materialien kommen aus Projektordnern. Die Anzeige ergänzt den
// übergeordneten Ordner, ohne den gespeicherten Materialnamen zu verändern.
inline std::string material_display_folder(const MaterialInfo& material) {
    std::string path = material.file.empty() ? material.source : material.file;
    if (path.empty()) return {};
    std::replace(path.begin(), path.end(), '\\', '/');
    const std::size_t slash = path.find_last_of('/');
    if (slash == std::string::npos) return {};
    const std::string folder = path.substr(0, slash);
    const std::size_t previous = folder.find_last_of('/');
    return previous == std::string::npos ? folder : folder.substr(previous + 1);
}

inline std::string material_display_name(const MaterialInfo& material) {
    const std::string folder = material_display_folder(material);
    return folder.empty() ? material.name : folder + "/" + material.name;
}

enum class MaterialParse { Ok, InvalidJson, InvalidRoot, InvalidUtf8, Duplicate, Nonfinite, TooLarge, TooDeep };

// Begrenzt VOR dem rekursiven Parser die Verschachtelung und berücksichtigt
// dabei Klammern und Escapes in Strings. Ruby JSON nutzt dieselbe Grenze 100.
inline bool material_nesting_safe(const std::string& text) {
    std::size_t depth = 0;
    bool inside = false, escape = false;
    for (unsigned char c : text) {
        if (inside) {
            if (escape) escape = false;
            else if (c == '\\') escape = true;
            else if (c == '"') inside = false;
        } else if (c == '"') inside = true;
        else if (c == '{' || c == '[') {
            if (++depth > 100) return false;
        } else if (c == '}' || c == ']') {
            if (depth == 0) return false;
            --depth;
        }
    }
    return depth == 0 && !inside && !escape;
}

inline const JsonValue* material_array(const JsonValue& root) {
    if (root.is_arr()) return &root;
    if (!root.is_obj()) return nullptr;
    const JsonValue* list = root.find("textures");
    return list && list->is_arr() ? list : nullptr;
}
inline JsonValue* material_array(JsonValue& root) {
    return const_cast<JsonValue*>(material_array(static_cast<const JsonValue&>(root)));
}

inline bool material_name_valid(const std::string& name) {
    return world::terrain_valid_texture_name(name) && editor_single_line(name);
}

inline bool material_dimension(const JsonValue* value, int& out) {
    if (!value) { out = 64; return true; }
    if (!value->is_num()) return false;
    const double n = value->number();
    if (!std::isfinite(n) || n < 1 || n > world::TEXTURE_MAX_DIMENSION || std::floor(n) != n) return false;
    out = static_cast<int>(n);
    return true;
}

inline bool material_entry(const JsonValue& entry, std::size_t index, MaterialInfo& out) {
    if (!entry.is_obj()) return false;
    const JsonValue* name = entry.find("name");
    const JsonValue* file = entry.find("file");
    if (!name || !name->is_str() || !material_name_valid(name->str) ||
        !file || !file->is_str() || !texture_file_is_flat(file->str) || !editor_valid_utf8(file->str)) return false;
    MaterialInfo m;
    m.name = name->str;
    m.file = file->str;
    if (!material_dimension(entry.find("w"), m.w) || !material_dimension(entry.find("h"), m.h)) return false;
    std::size_t bytes = 0;
    if (!world::texture_byte_count(m.w, m.h, bytes)) return false;
    if (const JsonValue* source = entry.find("source")) {
        if (source->is_str() && editor_single_line(source->str)) m.source = source->str;
        else m.source = "[nicht darstellbar]";
    }
    if (const JsonValue* tile = entry.find("tile")) {
        m.tile_valid = tile->is_num() && world::terrain_valid_tile(tile->number());
        if (m.tile_valid) m.tile = tile->number();
    }
    m.imported = true;
    m.document_index = index;
    out = std::move(m);
    return true;
}

// Bei jedem Fehler bleibt der Ausgabebaum unangetastet. Doppelte JSON-Keys,
// doppelte logische Namen und nicht erneut speicherbare Zahlen sind gesperrt.
inline MaterialParse material_parse(const std::string& text, JsonValue& out) {
    if (text.size() > world::TEXTURE_MAX_MANIFEST_BYTES) return MaterialParse::TooLarge;
    if (!editor_valid_utf8(text)) return MaterialParse::InvalidUtf8;
    if (!editor_json_raw_strings_valid(text)) return MaterialParse::InvalidJson;
    if (!material_nesting_safe(text)) return MaterialParse::TooDeep;
    JsonValue parsed;
    try { parsed = json_parse(text); }
    catch (const std::exception&) { return MaterialParse::InvalidJson; }
    if (!editor_json_utf8(parsed)) return MaterialParse::InvalidUtf8;
    if (editor_json_duplicates(parsed)) return MaterialParse::Duplicate;
    if (!editor_json_serializable(parsed)) return MaterialParse::Nonfinite;
    if (parsed.is_obj() && !parsed.find("textures")) parsed.set("textures", JsonValue::make_arr());
    const JsonValue* list = material_array(parsed);
    if (!list) return MaterialParse::InvalidRoot;
    if (list->arr.size() > world::TEXTURE_MAX_MATERIALS) return MaterialParse::TooLarge;
    std::set<std::string> names;
    for (const JsonValue& item : list->arr) {
        if (!item.is_obj()) continue;
        const JsonValue* name = item.find("name");
        if (name && name->is_str() && material_name_valid(name->str) && !names.insert(name->str).second)
            return MaterialParse::Duplicate;
    }
    out = std::move(parsed);
    return MaterialParse::Ok;
}

inline std::vector<MaterialInfo> material_entries(const JsonValue& root) {
    std::vector<MaterialInfo> result;
    const JsonValue* list = material_array(root);
    if (!list) return result;
    for (std::size_t i = 0; i < list->arr.size(); ++i) {
        MaterialInfo m;
        if (material_entry(list->arr[i], i, m)) result.push_back(std::move(m));
    }
    return result;
}

inline bool material_set_tile(JsonValue& root, std::size_t index, double tile) {
    JsonValue* list = material_array(root);
    if (!list || index >= list->arr.size() || !list->arr[index].is_obj() || !world::terrain_valid_tile(tile))
        return false;
    list->arr[index].set("tile", JsonValue::make_num(tile, false));
    return true;
}

inline bool material_remove(JsonValue& root, std::size_t index) {
    JsonValue* list = material_array(root);
    if (!list || index >= list->arr.size()) return false;
    list->arr.erase(list->arr.begin() + static_cast<std::ptrdiff_t>(index));
    return true;
}

// Ein Ersatz ändert nur die grafikspezifischen Felder. Vorhandene tile- und
// alle unbekannten Felder bleiben unverändert, auch wenn sie ungültig sind.
inline bool material_upsert(JsonValue& root, const MaterialInfo& material, std::size_t* replaced_index = nullptr) {
    JsonValue* list = material_array(root);
    if (!list || !material_name_valid(material.name) || !texture_file_is_flat(material.file)) return false;
    MaterialInfo checked;
    std::size_t target = list->arr.size();
    for (std::size_t i = 0; i < list->arr.size(); ++i) {
        const JsonValue* name = list->arr[i].is_obj() ? list->arr[i].find("name") : nullptr;
        if (name && name->is_str() && name->str == material.name) { target = i; break; }
    }
    if (target == list->arr.size() && list->arr.size() >= world::TEXTURE_MAX_MATERIALS) return false;
    // Ein ungültiger Alteintrag wird nicht als Nebeneffekt eines Imports repariert.
    if (target < list->arr.size() && !material_entry(list->arr[target], target, checked)) return false;
    JsonValue entry = target == list->arr.size() ? JsonValue::make_obj() : list->arr[target];
    if (!entry.is_obj()) return false;
    entry.set("name", JsonValue::make_str(material.name));
    entry.set("file", JsonValue::make_str(material.file));
    entry.set("w", JsonValue::make_num(material.w, true));
    entry.set("h", JsonValue::make_num(material.h, true));
    entry.set("source", JsonValue::make_str(material.source));
    if (target == list->arr.size()) entry.set("tile", JsonValue::make_num(material.tile, false));
    if (!material_entry(entry, target, checked)) return false;
    if (target == list->arr.size()) list->arr.push_back(std::move(entry));
    else list->arr[target] = std::move(entry);
    if (replaced_index) *replaced_index = target;
    return true;
}

} // namespace rune::editor
