#pragma once
// ============================================================================
//  Datenmodell für den Platzier-Editor: NPCs, Schild-Events und Prefab-Objekte.
//  Ohne Win32: Eingaben werden auf einer Kopie validiert, unveränderte und
//  unbekannte Felder bleiben erhalten. Ein y-Wert ist ein Bodenversatz.
// ============================================================================

#include "editor/json.h"

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace rune::editor {

enum class PlacementKind { Npc, Sign, Object };

struct PlacementForm {
    std::string name, type, x, y, z, speed, radius, rotation, path, text;
    bool path_valid = true;
    bool text_editable = true;
};

struct PlacementEdit {
    bool ok = true;
    bool changed = false;
    bool replaces_invalid_path = false;
    std::string field;
    std::string error;
    JsonValue value;
};

inline bool placement_nonempty(const std::string& s) {
    return std::any_of(s.begin(), s.end(), [](unsigned char c) { return !std::isspace(c); });
}

inline bool placement_single_line(const std::string& s) {
    return placement_nonempty(s) && std::none_of(s.begin(), s.end(), [](unsigned char c) {
        return c < 0x20 || c == 0x7f;
    });
}

inline std::string placement_num_text(double n) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.17g", n);
    return buf;
}

inline std::string placement_number_field(const JsonValue& o, const char* key, double fallback) {
    const JsonValue* v = o.find(key);
    if (!v || v->is_null()) return placement_num_text(fallback);
    if (!v->is_num()) return "ungültig";
    if (v->is_int && !v->integer_text().empty()) return v->integer_text();
    if (!std::isfinite(v->number())) return "ungültig";
    return placement_num_text(v->number());
}

// Ein Zahlenfeld darf Dezimalkomma oder Dezimalpunkt benutzen. Es muss
// vollständig gelesen werden; NaN, Unendlich und Überläufe sind unzulässig.
inline bool placement_number(std::string s, double& out) {
    std::replace(s.begin(), s.end(), ',', '.');
    const char* first = s.c_str();
    while (*first && std::isspace(static_cast<unsigned char>(*first))) ++first;
    if (!*first) return false;
    errno = 0;
    char* end = nullptr;
    const double n = std::strtod(first, &end);
    if (first == end || errno == ERANGE || !std::isfinite(n)) return false;
    while (*end && std::isspace(static_cast<unsigned char>(*end))) ++end;
    // 3D-Viewport und Physik verwenden float; extreme Werte wären dort nicht
    // sinnvoll darstellbar und könnten das Rendering destabilisieren.
    if (*end || std::fabs(n) > 1000000.0) return false;
    out = n;
    return true;
}

// CR/LF/CRLF als Zeilenende behandeln, leere Innenzeilen nicht verlieren.
inline std::vector<std::string> placement_lines(const std::string& s) {
    std::vector<std::string> lines;
    size_t start = 0;
    while (true) {
        const size_t end = s.find_first_of("\r\n", start);
        lines.push_back(s.substr(start, end == std::string::npos ? end : end - start));
        if (end == std::string::npos) break;
        start = end + 1;
        if (s[end] == '\r' && start < s.size() && s[start] == '\n') ++start;
    }
    return lines;
}

inline bool placement_path_parse(const std::string& text, JsonValue& result) {
    result = JsonValue::make_arr();
    for (const std::string& line : placement_lines(text)) {
        if (!placement_nonempty(line)) continue;
        // Semikolon erlaubt auch deutsche Dezimalkommas. Alternativ ist bei
        // Punktzahlen ein einzelnes Komma als Trenner möglich.
        size_t sep = line.find(';');
        if (sep == std::string::npos) sep = line.find(',');
        if (sep == std::string::npos || line.find(';', sep + 1) != std::string::npos)
            return false;
        if (line[sep] == ',' && line.find(',', sep + 1) != std::string::npos) return false;
        double x = 0.0, z = 0.0;
        if (!placement_number(line.substr(0, sep), x) ||
            !placement_number(line.substr(sep + 1), z)) return false;
        JsonValue point = JsonValue::make_arr();
        point.arr.push_back(JsonValue::make_num(x, false));
        point.arr.push_back(JsonValue::make_num(z, false));
        result.arr.push_back(std::move(point));
    }
    return true;
}

// Unveränderte Punkte auch nach Einfügen, Löschen oder Umordnen samt ihrem
// ursprünglichen JSON-Zahlentyp bewahren. Doppelte Punkte werden höchstens
// einmal zugeordnet; neu eingegebene Werte bleiben neue Kommazahlen.
inline JsonValue placement_path_merge(const JsonValue* old, JsonValue parsed) {
    if (!old || !old->is_arr()) return parsed;
    std::vector<bool> used(old->arr.size(), false);
    auto same = [](double a, double b) {
        return a == b && (a != 0.0 || std::signbit(a) == std::signbit(b));
    };
    for (JsonValue& point : parsed.arr) {
        for (size_t i = 0; i < old->arr.size(); ++i) {
            const JsonValue& previous = old->arr[i];
            if (used[i] || !previous.is_arr() || previous.arr.size() != 2 ||
                !previous.arr[0].is_num() || !previous.arr[1].is_num()) continue;
            if (same(previous.arr[0].number(), point.arr[0].number()) &&
                same(previous.arr[1].number(), point.arr[1].number())) {
                point = previous;
                used[i] = true;
                break;
            }
        }
    }
    return parsed;
}

inline std::string placement_path_text(const JsonValue* path, bool& valid) {
    valid = true;
    if (!path || path->is_null()) return {};
    if (!path->is_arr()) { valid = false; return "[ungültige Wegpunkt-Liste]"; }
    std::string result;
    for (size_t i = 0; i < path->arr.size(); ++i) {
        if (i) result += "\r\n";
        const JsonValue& p = path->arr[i];
        if (!p.is_arr() || p.arr.size() != 2 || !p.arr[0].is_num() ||
            !p.arr[1].is_num() || !std::isfinite(p.arr[0].number()) ||
            !std::isfinite(p.arr[1].number())) {
            valid = false;
            result += "[ungültiger Wegpunkt]";
        } else {
            result += placement_num_text(p.arr[0].number()) + "; " + placement_num_text(p.arr[1].number());
        }
    }
    return result;
}

// Nur die erste einfache Textanweisung editieren, andere Kommandos und
// Zusatzfelder bleiben bestehen. Bei mehreren Event-Seiten oder kaputten
// Kommandodaten ist das Textfeld gesperrt; F6 bearbeitet komplexe Events.
inline const JsonValue* placement_commands(const JsonValue& o, PlacementKind kind, bool& editable) {
    editable = true;
    const JsonValue* owner = &o;
    if (kind == PlacementKind::Sign) {
        if (const JsonValue* pages = o.find("pages"); pages && !pages->is_null()) {
            if (!pages->is_arr() || pages->arr.size() != 1 || !pages->arr[0].is_obj()) {
                editable = false;
                return nullptr;
            }
            owner = &pages->arr[0];
        }
    }
    const JsonValue* commands = owner->find("commands");
    if (!commands || commands->is_null()) return nullptr;
    if (!commands->is_arr()) { editable = false; return nullptr; }
    return commands;
}

inline JsonValue* placement_commands_mutable(JsonValue& o, PlacementKind kind) {
    JsonValue* owner = &o;
    if (kind == PlacementKind::Sign) {
        JsonValue* pages = o.find("pages");
        if (pages && !pages->is_null()) owner = &pages->arr[0];
    }
    JsonValue* cmds = owner->find("commands");
    if (!cmds || cmds->is_null()) cmds = &owner->set("commands", JsonValue::make_arr());
    return cmds;
}

inline std::string placement_text_read(const JsonValue& o, PlacementKind kind, bool& editable) {
    const JsonValue* commands = placement_commands(o, kind, editable);
    if (!editable || !commands) return {};
    for (const JsonValue& c : commands->arr) {
        const JsonValue* cmd = c.is_obj() ? c.find("cmd") : nullptr;
        if (!cmd || !cmd->is_str() || cmd->str != "text") continue;
        const JsonValue* lines = c.find("lines");
        if (!lines || !lines->is_arr()) { editable = false; return {}; }
        std::string result;
        for (size_t i = 0; i < lines->arr.size(); ++i) {
            const JsonValue& line = lines->arr[i];
            if (!line.is_str() || line.str.find_first_of("\r\n\0", 0, 3) != std::string::npos) {
                editable = false;
                return {};
            }
            if (i) result += "\r\n";
            result += line.str;
        }
        return result;
    }
    return {};
}

inline PlacementForm placement_form_read(const JsonValue& o, PlacementKind kind) {
    PlacementForm f;
    const JsonValue* name = o.find("name");
    f.name = name && name->is_str() ? name->str : "";
    const JsonValue* type = o.find("type");
    f.type = type && type->is_str() ? type->str : "";
    f.x = placement_number_field(o, "x", 0.0);
    f.y = placement_number_field(o, "y", 0.0);
    f.z = placement_number_field(o, "z", 0.0);
    f.speed = placement_number_field(o, "speed", 1.5);
    f.radius = placement_number_field(o, "radius", kind == PlacementKind::Npc ? 2.5 : 2.0);
    const JsonValue* rot = o.find("rot");
    constexpr double to_degrees = 180.0 / 3.14159265358979323846;
    if (!rot || rot->is_null()) f.rotation = "0";
    else if (!rot->is_num() || !std::isfinite(rot->number())) f.rotation = "ungültig";
    else f.rotation = placement_num_text(rot->number() * to_degrees);
    f.path = placement_path_text(o.find("path"), f.path_valid);
    if (kind != PlacementKind::Object) f.text = placement_text_read(o, kind, f.text_editable);
    return f;
}

inline bool placement_form_equal(const PlacementForm& a, const PlacementForm& b) {
    return a.name == b.name && a.type == b.type && a.x == b.x && a.y == b.y &&
           a.z == b.z && a.speed == b.speed && a.radius == b.radius &&
           a.rotation == b.rotation && a.path == b.path && a.text == b.text;
}

inline PlacementEdit placement_form_prepare(const JsonValue& old, PlacementKind kind,
                                             const PlacementForm& before,
                                             const PlacementForm& after) {
    PlacementEdit r;
    r.value = old;
    auto fail = [&r](const char* field, const char* error) {
        r.ok = false;
        r.field = field;
        r.error = error;
    };
    if (!old.is_obj()) { fail("list", "Dieser Eintrag ist kein Objekt."); return r; }
    if (kind != PlacementKind::Object && before.name != after.name) {
        if (!placement_single_line(after.name)) {
            fail("name", "Bitte einen Namen ohne Steuerzeichen eingeben."); return r;
        }
        r.value.set("name", JsonValue::make_str(after.name));
        r.changed = true;
    }
    if (kind == PlacementKind::Object && before.type != after.type) {
        if (!placement_single_line(after.type)) {
            fail("type", "Bitte einen Objekt-Typ auswählen."); return r;
        }
        r.value.set("type", JsonValue::make_str(after.type));
        r.changed = true;
    }
    auto number = [&](const char* key, const std::string& was, const std::string& now,
                      bool positive, double factor = 1.0) {
        if (was == now) return true;
        double v = 0.0;
        if (!placement_number(now, v) || (positive && v <= 0.0)) {
            fail(key, positive ? "Bitte eine gültige Zahl größer als 0 eingeben."
                               : "Bitte eine gültige Zahl zwischen -1000000 und 1000000 eingeben.");
            return false;
        }
        r.value.set(key, JsonValue::make_num(v * factor, false));
        r.changed = true;
        return true;
    };
    if (!number("x", before.x, after.x, false) ||
        !number("y", before.y, after.y, false) ||
        !number("z", before.z, after.z, false)) return r;
    if (kind == PlacementKind::Npc && !number("speed", before.speed, after.speed, true)) return r;
    if (kind != PlacementKind::Object && !number("radius", before.radius, after.radius, true)) return r;
    if (kind != PlacementKind::Npc &&
        !number("rot", before.rotation, after.rotation, false, 3.14159265358979323846 / 180.0)) return r;
    if (kind == PlacementKind::Npc && before.path != after.path) {
        JsonValue path;
        if (!placement_path_parse(after.path, path)) {
            fail("path", "Wegpunkte: je Zeile zwei Zahlen, z. B. 4; -1 oder 1,5; 2,0.");
            return r;
        }
        r.replaces_invalid_path = !before.path_valid;
        r.value.set("path", placement_path_merge(old.find("path"), std::move(path)));
        r.changed = true;
    }
    if (kind != PlacementKind::Object && before.text != after.text) {
        if (!before.text_editable || after.text.find('\0') != std::string::npos) {
            fail("text", "Dieser Text kann hier nicht bearbeitet werden."); return r;
        }
        JsonValue* cmds = placement_commands_mutable(r.value, kind);
        JsonValue* text_cmd = nullptr;
        for (JsonValue& c : cmds->arr) {
            const JsonValue* cmd = c.is_obj() ? c.find("cmd") : nullptr;
            if (cmd && cmd->is_str() && cmd->str == "text") { text_cmd = &c; break; }
        }
        if (!text_cmd && !after.text.empty()) {
            JsonValue cmd = JsonValue::make_obj();
            cmd.set("cmd", JsonValue::make_str("text"));
            cmds->arr.push_back(std::move(cmd));
            text_cmd = &cmds->arr.back();
        }
        if (text_cmd) {
            JsonValue lines = JsonValue::make_arr();
            for (const std::string& line : placement_lines(after.text))
                lines.arr.push_back(JsonValue::make_str(line));
            text_cmd->set("lines", std::move(lines));
            r.changed = true;
        }
    }
    return r;
}

// Der numerische Event-Identifier soll auch bei alten/fehlenden IDs eindeutig
// sein, ohne übergrosse double-Werte in int zu casten.
inline int placement_next_event_id(const JsonValue& root) {
    const JsonValue* events = root.find("events");
    for (int candidate = 1; candidate < std::numeric_limits<int>::max(); ++candidate) {
        bool used = false;
        if (events && events->is_arr()) {
            for (const JsonValue& e : events->arr) {
                const JsonValue* id = e.is_obj() ? e.find("id") : nullptr;
                if (id && id->is_num() && id->number() == static_cast<double>(candidate)) {
                    used = true;
                    break;
                }
            }
        }
        if (!used) return candidate;
    }
    return 0;
}

inline bool placement_name_used(const JsonValue& root, const std::string& name,
                                PlacementKind skip_kind, int skip_index) {
    for (const char* key : {"npcs", "events"}) {
        const JsonValue* list = root.find(key);
        if (!list || !list->is_arr()) continue;
        for (size_t i = 0; i < list->arr.size(); ++i) {
            const bool skip = static_cast<int>(i) == skip_index &&
                ((skip_kind == PlacementKind::Npc && std::string(key) == "npcs") ||
                 (skip_kind == PlacementKind::Sign && std::string(key) == "events"));
            if (skip) continue;
            const JsonValue& item = list->arr[i];
            const JsonValue* n = item.is_obj() ? item.find("name") : nullptr;
            if (n && n->is_str() && n->str == name) return true;
        }
    }
    return false;
}

// F10 und die kopflosen Tests prüfen den optionalen Bodenversatz wie MapCheck.
template <class Report>
inline void issue_scan_placement_heights(const JsonValue& root, const std::string& map_name,
                                         Report report) {
    for (const char* key : {"objects", "events", "npcs"}) {
        const JsonValue* list = root.find(key);
        if (!list || !list->is_arr()) continue;
        for (size_t i = 0; i < list->arr.size(); ++i) {
            const JsonValue& item = list->arr[i];
            const JsonValue* y = item.is_obj() ? item.find("y") : nullptr;
            if (y && !y->is_null() && (!y->is_num() || !std::isfinite(y->number()))) {
                report(1, map_name + " " + key + " #" + std::to_string(i + 1),
                       "'y' muss eine endliche Zahl sein (Versatz zum Gelände)");
            }
        }
    }
}

inline std::string placement_new_name(const JsonValue& root, const char* prefix) {
    for (int i = 1; i < std::numeric_limits<int>::max(); ++i) {
        const std::string name = std::string(prefix) + " " + std::to_string(i);
        if (!placement_name_used(root, name, PlacementKind::Object, -1)) return name;
    }
    return std::string(prefix);
}

inline JsonValue placement_new(const JsonValue& root, PlacementKind kind, const std::string& prefab) {
    JsonValue v = JsonValue::make_obj();
    if (kind == PlacementKind::Object) v.set("type", JsonValue::make_str(prefab));
    else {
        v.set("name", JsonValue::make_str(placement_new_name(root, kind == PlacementKind::Npc ? "NPC" : "Schild")));
        if (kind == PlacementKind::Sign)
            v.set("id", JsonValue::make_num(placement_next_event_id(root), true));
    }
    v.set("x", JsonValue::make_num(0.0, false));
    v.set("z", JsonValue::make_num(0.0, false));
    if (kind == PlacementKind::Npc) {
        v.set("speed", JsonValue::make_num(1.5, false));
        v.set("path", JsonValue::make_arr());
        v.set("commands", JsonValue::make_arr());
    } else if (kind == PlacementKind::Sign) {
        v.set("visual", JsonValue::make_str("schild"));
        v.set("trigger", JsonValue::make_str("action"));
        v.set("radius", JsonValue::make_num(2.0, false));
        JsonValue lines = JsonValue::make_arr();
        lines.arr.push_back(JsonValue::make_str("Text eingeben"));
        JsonValue cmd = JsonValue::make_obj();
        cmd.set("cmd", JsonValue::make_str("text"));
        cmd.set("lines", std::move(lines));
        JsonValue commands = JsonValue::make_arr();
        commands.arr.push_back(std::move(cmd));
        v.set("commands", std::move(commands));
    } else {
        v.set("rot", JsonValue::make_num(0.0, false));
    }
    return v;
}

} // namespace rune::editor
