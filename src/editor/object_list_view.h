#pragma once
// Etappe 37: Zustand eines Objekts für die Inspector-Liste. Reine Logik, damit
// Icons und Materialanzeige kopflos geprüft werden können.

#include "editor/json.h"

#include <algorithm>
#include <string>
#include <vector>

namespace rune::editor {

enum ObjectListImage : int {
    ObjectImageKnown = 0,
    ObjectImageUnknown = 1,
    ObjectImageOverride = 2,
    ObjectImageCount = 3,
};

struct ObjectListInfo {
    int image = ObjectImageUnknown;
    std::string type;
    std::string material;
    bool known = false;
    bool override_material = false;
};

inline ObjectListInfo object_list_info(const JsonValue* object,
                                       const std::vector<std::string>& known_types) {
    ObjectListInfo info;
    if (!object || !object->is_obj()) return info;
    if (const JsonValue* t = object->find("type"); t && t->is_str()) info.type = t->str;
    if (const JsonValue* m = object->find("tex"); m && m->is_str()) info.material = m->str;
    info.known = !info.type.empty() &&
                 std::find(known_types.begin(), known_types.end(), info.type) != known_types.end();
    info.override_material = !info.material.empty();
    info.image = info.override_material ? ObjectImageOverride
               : info.known ? ObjectImageKnown : ObjectImageUnknown;
    return info;
}

} // namespace rune::editor
