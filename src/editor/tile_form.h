#pragma once
// Tileset-Formular auf einer Kopie vorbereiten. Eine reine Texturänderung
// darf ein unberührtes, möglicherweise extrem großes tile nicht ersetzen.
#include "editor/map_props_form.h"

#include <cstddef>

namespace rune::editor {

// Zeilen bleiben auch bei ungültigen Altdaten an ihren Originalindex im
// JSON gebunden; das UI darf nicht mit Indizes einer gefilterten Liste löschen.
struct SplatListRow {
    std::size_t original_index = 0;
    bool valid = false;
    world::SplatRect splat;
};

struct SplatListModel {
    std::vector<SplatListRow> rows;
    std::size_t omitted = 0;
};

inline SplatListModel splat_list_model(const JsonValue* list) {
    SplatListModel model;
    if (!list || !list->is_arr()) return model;
    const std::size_t count = std::min(list->arr.size(), world::TERRAIN_MAX_SPLATS);
    model.rows.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        SplatListRow row;
        row.original_index = i;
        row.valid = preview_splat(list->arr[i], row.splat);
        model.rows.push_back(std::move(row));
    }
    model.omitted = list->arr.size() - count;
    return model;
}

enum class SplatDeleteState { Unavailable, Ready, NeedsConfirmation };

// Ausschließlich sichtbare Originalindizes zulassen, nicht den Sentinel oder
// einen mehrdeutigen JSON-Pfad. Die Prüfung und die Mutation arbeiten auf dem
// aktuellen Baum, sodass nach einer Rückfrage kein alter JSON-Zeiger lebt.
inline SplatDeleteState splat_delete_state(const JsonValue& root, std::ptrdiff_t original) {
    if (original < 0 || static_cast<std::size_t>(original) >= world::TERRAIN_MAX_SPLATS ||
        !root.is_obj() || !preview_unique(root, "terrain")) return SplatDeleteState::Unavailable;
    const JsonValue* terrain = root.find("terrain");
    if (!terrain || !terrain->is_obj() || !preview_unique(*terrain, "splats"))
        return SplatDeleteState::Unavailable;
    const JsonValue* list = terrain->find("splats");
    if (!list || !list->is_arr() || static_cast<std::size_t>(original) >= list->arr.size())
        return SplatDeleteState::Unavailable;
    world::SplatRect splat;
    return preview_splat(list->arr[static_cast<std::size_t>(original)], splat)
        ? SplatDeleteState::Ready : SplatDeleteState::NeedsConfirmation;
}

inline bool splat_delete_apply(JsonValue& root, std::ptrdiff_t original, bool confirmed_invalid) {
    const SplatDeleteState state = splat_delete_state(root, original);
    if (state == SplatDeleteState::Unavailable ||
        (state == SplatDeleteState::NeedsConfirmation && !confirmed_invalid)) return false;
    JsonValue* list = root.find("terrain")->find("splats");
    list->arr.erase(list->arr.begin() + original);
    // Eine absichtlich vorhandene Liste bleibt auch nach dem letzten Löschen
    // erhalten; nur die ausgewählte Zeile wurde bearbeitet.
    return true;
}

struct TileFormEdit {
    bool ok = true;
    bool changed = false;
    bool replaces_invalid = false;
    std::string error;
    JsonValue value;
};

inline TileFormEdit tile_form_prepare(const JsonValue* old, const std::string& texture_before,
                                      const std::string& texture_after, const std::string& tile_before,
                                      const std::string& tile_after) {
    TileFormEdit edit;
    const bool texture_changed = texture_before != texture_after;
    const bool tile_changed = tile_before != tile_after;
    if (texture_changed && !world::terrain_valid_texture_name(texture_after)) {
        edit.ok = false; edit.error = "Ungültiger oder zu langer Texturname."; return edit;
    }
    if (old && !old->is_obj()) {
        edit.ok = false; edit.error = "terrain ist kein Objekt; Altdaten bleiben erhalten."; return edit;
    }
    if (!texture_changed && !tile_changed) return edit;
    const JsonValue* old_texture = old ? old->find("texture") : nullptr;
    const JsonValue* old_tile = old ? old->find("tile") : nullptr;
    if ((texture_changed && old && !preview_unique(*old, "texture")) ||
        (tile_changed && old && !preview_unique(*old, "tile"))) {
        edit.ok = false; edit.error = "Mehrfach vorhandene texture-/tile-Schlüssel bleiben geschützt."; return edit;
    }
    double tile = 0.0;
    if (tile_changed && (!map_props_number(tile_after, tile) || !world::terrain_valid_tile(tile))) {
        edit.ok = false; edit.error = "Kachelgröße muss zwischen 0.01 und 10000 liegen."; return edit;
    }
    edit.replaces_invalid =
        (texture_changed && old_texture && !old_texture->is_null() &&
         (!old_texture->is_str() || !world::terrain_valid_texture_name(old_texture->str))) ||
        (tile_changed && old_tile && !old_tile->is_null() &&
         (!old_tile->is_num() || !world::terrain_valid_tile(old_tile->number())));
    edit.value = old ? *old : JsonValue::make_obj();
    if (texture_changed) edit.value.set("texture", JsonValue::make_str(texture_after));
    if (tile_changed) edit.value.set("tile", JsonValue::make_num(tile, false));
    edit.changed = texture_changed || tile_changed;
    return edit;
}

} // namespace rune::editor
