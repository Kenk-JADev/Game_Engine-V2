#pragma once
// ============================================================================
//  Objekt-Eigenschaften im Hauptfenster: reine Formularlogik für X/Z,
//  Drehung (Grad) und Material. Vorbereitung verändert das Objekt nicht;
//  unberührte und unbekannte Werte bleiben erhalten.
// ============================================================================

#include "editor/json.h"
#include "editor/placement_form.h" // exakte Anzeige und endliche Zahlen
#include "editor/text_validation.h"

#include <cmath>
#include <string>
#include <utility>

namespace rune::editor {

struct ObjectPropsForm {
    std::string x, z, rotation, material;
};

inline bool object_props_equal(const ObjectPropsForm& a, const ObjectPropsForm& b) {
    return a.x == b.x && a.z == b.z && a.rotation == b.rotation && a.material == b.material;
}

struct ObjectPropsEdit {
    bool ok = true;
    bool changed = false;
    bool replaces_invalid = false;
    std::string field;
    std::string error;
    JsonValue value;
};

inline ObjectPropsForm object_props_read(const JsonValue& object) {
    ObjectPropsForm form;
    if (!object.is_obj()) return form;
    form.x = placement_number_field(object, "x", 0.0);
    form.z = placement_number_field(object, "z", 0.0);
    const JsonValue* rot = object.find("rot");
    if (!rot || rot->is_null()) {
        form.rotation = "0";
    } else if (!rot->is_num() || !std::isfinite(rot->number())) {
        form.rotation = "ungültig";
    } else {
        constexpr double degrees = 180.0 / 3.14159265358979323846;
        const double value = rot->number() * degrees;
        form.rotation = std::isfinite(value) ? placement_num_text(value) : "ungültig";
    }
    const JsonValue* material = object.find("tex");
    if (material && material->is_str()) form.material = material->str;
    else if (material && !material->is_null()) form.material = "[ungültiges Material]";
    return form;
}

inline bool object_props_field_unique(const JsonValue& object, const char* key) {
    unsigned found = 0;
    for (const auto& item : object.obj)
        if (item.first == key && ++found > 1) return false;
    return true;
}

// Beschädigte oder extreme Werte unverändert im JSON lassen, aber nicht in
// float-Rendering, Picking und Terrainberechnung einspeisen. Der Eintrag ist
// weiterhin über die Liste erreichbar und kann im Formular repariert werden.
inline bool object_props_viewable(const JsonValue& object) {
    if (!object.is_obj()) return false;
    for (const char* key : {"x", "y", "z", "rot"}) {
        if (!object_props_field_unique(object, key)) return false;
        const JsonValue* value = object.find(key);
        if (!value || value->is_null()) continue;
        if (!value->is_num() || !std::isfinite(value->number()) ||
            std::fabs(value->number()) > 1000000.0) return false;
    }
    return true;
}

// Drag auf einer Kopie vorbereiten. dx/dz sind die Bewegung des angefassten
// Punktes gegenüber dem Beginn, nicht seine absolute Position. Erst wenn eine
// Achse mindestens eine halbe Rasterzelle bewegt wurde, wird sie eingerastet.
// Beim Zurückziehen auf den Anfang bleiben Typ, Präzision und fehlende/null-
// Felder wie zuvor; eine Bewegung nur in X darf Z nicht mitrunden.
struct ObjectDragEdit {
    bool ok = false;
    bool changed = false;
    JsonValue value;
};

inline ObjectDragEdit object_drag_prepare(const JsonValue& current, const JsonValue& original,
                                           double dx, double dz, double grid) {
    ObjectDragEdit result;
    result.value = current;
    if (!object_props_viewable(current) || !object_props_viewable(original) ||
        !std::isfinite(dx) || !std::isfinite(dz) || !std::isfinite(grid) || grid <= 0.0)
        return result;

    auto axis = [&](const char* key, double delta) {
        const JsonValue* start = original.find(key);
        const double start_number = start ? start->number(0.0) : 0.0;
        const double cells = delta / grid;
        if (!std::isfinite(cells)) return false;
        const bool restore = std::round(cells) == 0.0;
        double target = start_number;
        if (!restore) {
            target = std::round((start_number + delta) / grid) * grid;
            if (!std::isfinite(target) || std::fabs(target) > 1000000.0) return false;
        }
        const JsonValue* previous = result.value.find(key);
        // Die ursprüngliche Darstellung auch dann beibehalten, wenn das
        // eingerastete Ziel dem ursprünglichen Zahlenwert entspricht.
        if (restore || target == start_number) {
            if (!start && previous) {
                for (auto it = result.value.obj.begin(); it != result.value.obj.end(); ++it) {
                    if (it->first == key) { result.value.obj.erase(it); break; }
                }
                result.changed = true;
            } else if (start && (!previous || json_write(*previous) != json_write(*start))) {
                result.value.set(key, *start);
                result.changed = true;
            }
        } else if (!previous || !previous->is_num() || previous->number() != target) {
            result.value.set(key, JsonValue::make_num(target, false));
            result.changed = true;
        }
        return true;
    };
    if (!axis("x", dx) || !axis("z", dz)) {
        result.value = current;
        result.changed = false;
        return result;
    }
    result.ok = true;
    return result;
}

inline std::string object_props_trim(const std::string& text) {
    const size_t first = text.find_first_not_of(' ');
    if (first == std::string::npos) return {};
    return text.substr(first, text.find_last_not_of(' ') - first + 1);
}

inline ObjectPropsEdit object_props_prepare(const JsonValue& object,
                                             const ObjectPropsForm& before,
                                             const ObjectPropsForm& after) {
    ObjectPropsEdit result;
    result.value = object;
    auto fail = [&result, &object](const char* field, const char* message) {
        result.ok = false;
        result.changed = false;
        result.replaces_invalid = false;
        result.value = object;
        result.field = field;
        result.error = message;
    };
    if (!object.is_obj()) {
        fail("", "Kein bearbeitbares Objekt ausgewählt.");
        return result;
    }
    auto number = [&](const char* key, const char* field, const std::string& was,
                      const std::string& now, double factor) {
        if (was == now) return true;
        if (!object_props_field_unique(object, key)) {
            fail(field, "Mehrfach vorhandene JSON-Schlüssel können hier nicht bearbeitet werden.");
            return false;
        }
        double value = 0.0;
        if (!editor_single_line(now) || !placement_number(now, value)) {
            fail(field, "Eine endliche Zahl von -1.000.000 bis 1.000.000 eingeben.");
            return false;
        }
        const JsonValue* old = object.find(key);
        if (old && !old->is_null() &&
            (!old->is_num() || !std::isfinite(old->number()) ||
             (factor != 1.0 && !std::isfinite(old->number() / factor))))
            result.replaces_invalid = true;
        result.value.set(key, JsonValue::make_num(value * factor, false));
        return true;
    };
    constexpr double radians = 3.14159265358979323846 / 180.0;
    if (!number("x", "x", before.x, after.x, 1.0) ||
        !number("z", "z", before.z, after.z, 1.0) ||
        !number("rot", "rotation", before.rotation, after.rotation, radians)) return result;

    if (before.material != after.material) {
        if (!object_props_field_unique(object, "tex")) {
            fail("material", "Mehrfach vorhandene JSON-Schlüssel können hier nicht bearbeitet werden.");
            return result;
        }
        if (!editor_single_line(after.material)) {
            fail("material", "Einzeiligen UTF-8-Materialnamen ohne Steuerzeichen eingeben.");
            return result;
        }
        const JsonValue* old = object.find("tex");
        if (old && !old->is_null() &&
            (!old->is_str() || !editor_single_line(old->str))) result.replaces_invalid = true;
        const std::string material = object_props_trim(after.material);
        if (material.empty()) {
            for (auto it = result.value.obj.begin(); it != result.value.obj.end(); ++it) {
                if (it->first == "tex") { result.value.obj.erase(it); break; }
            }
        } else {
            result.value.set("tex", JsonValue::make_str(material));
        }
    }
    result.changed = json_write(result.value) != json_write(object);
    return result;
}

} // namespace rune::editor
