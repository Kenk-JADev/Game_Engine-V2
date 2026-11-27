#pragma once
// Etappe 37: neuer Objekt-Typ aus einem vorhandenen Prefab und einem
// importierten Material. Reine JSON-Arbeit, ohne Win32. Unbekannte Felder des
// Prefabs und seiner Teile bleiben erhalten; nur id, name und tex ändern sich.

#include "editor/json.h"

#include <algorithm>
#include <cctype>
#include <string>

namespace rune::editor {

inline bool prefab_custom_text_ok(const std::string& s, bool allow_empty = false) {
    if (s.empty()) return allow_empty;
    if (s.size() > 128 || s.find('\0') != std::string::npos) return false;
    return std::none_of(s.begin(), s.end(), [](unsigned char c) {
        return c < 0x20 || c == 0x7f;
    });
}

inline std::string prefab_custom_slug(const std::string& text) {
    // Materialnamen tragen meist eine Dateiendung. Die Typ-ID soll aber
    // "baum_wasser" und nicht "baum_wasser_png" heissen.
    std::string quelle = text;
    const std::size_t punkt = quelle.find_last_of('.');
    if (punkt != std::string::npos) {
        std::string endung = quelle.substr(punkt + 1);
        for (char& c : endung) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (endung == "png" || endung == "bmp" || endung == "jpg" ||
            endung == "jpeg" || endung == "gif" || endung == "tga") {
            quelle.resize(punkt);
        }
    }
    std::string out;
    for (unsigned char c : quelle) {
        if (out.size() >= 28) break;
        if (std::isalnum(c)) out += static_cast<char>(std::tolower(c));
        else if (!out.empty() && out.back() != '_') out += '_';
    }
    while (!out.empty() && out.back() == '_') out.pop_back();
    return out.empty() ? "typ" : out;
}

// Liefert ein neues Prefab-Objekt oder ein Null-Json, wenn Basis/Material
// fehlen. `out_id` ist die vergebene, eindeutige Typ-ID.
inline JsonValue prefab_custom_type(const JsonValue& root, const std::string& base_id,
                                    const std::string& material, const std::string& wanted_name,
                                    std::string& out_id) {
    out_id.clear();
    const JsonValue* list = root.is_obj() ? root.find("prefabs") : nullptr;
    if (!list || !list->is_arr() || !prefab_custom_text_ok(material) ||
        !prefab_custom_text_ok(wanted_name, true)) return JsonValue{};
    const JsonValue* base = nullptr;
    for (const JsonValue& entry : list->arr) {
        const JsonValue* id = entry.is_obj() ? entry.find("id") : nullptr;
        if (id && id->is_str() && id->str == base_id) { base = &entry; break; }
    }
    if (!base) return JsonValue{};
    const JsonValue* base_name = base->find("name");
    const std::string name = wanted_name.empty()
        ? (base_name && base_name->is_str() ? base_name->str : base_id) + " (" + material + ")"
        : wanted_name;
    if (!prefab_custom_text_ok(name)) return JsonValue{};
    const std::string stem = prefab_custom_slug(base_id) + "_" + prefab_custom_slug(material);
    auto id_used = [&](const std::string& id) {
        for (const JsonValue& entry : list->arr) {
            const JsonValue* candidate = entry.is_obj() ? entry.find("id") : nullptr;
            if (candidate && candidate->is_str() && candidate->str == id) return true;
        }
        return false;
    };
    std::string id = stem;
    for (int n = 2; id_used(id) && n < 10000; ++n)
        id = stem + "_" + std::to_string(n);
    if (id_used(id)) return JsonValue{};
    JsonValue copy = *base;
    copy.set("id", JsonValue::make_str(id));
    copy.set("name", JsonValue::make_str(name));
    if (JsonValue* parts = copy.find("parts"); parts && parts->is_arr()) {
        for (JsonValue& part : parts->arr) {
            if (part.is_obj()) part.set("tex", JsonValue::make_str(material));
        }
    }
    out_id = id;
    return copy;
}

} // namespace rune::editor
