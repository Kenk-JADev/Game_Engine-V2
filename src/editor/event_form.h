#pragma once
// ============================================================================
// F6: plattformunabhängiges, transaktionales Event-/Seiten-/Befehlsmodell.
// Lesen verändert das JSON nie. Änderungen werden auf einer Kopie vorbereitet;
// erst nach Validierung und ggf. Bestätigung darf der Aufrufer sie übernehmen.
// ============================================================================

#include "editor/database_form.h" // strikte Zahlenparser
#include "editor/json.h"
#include "editor/json_validation.h"
#include "editor/placement_form.h" // Zeilentrennung
#include "editor/text_validation.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <limits>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace rune::editor {

// Befehlstypen des F6-Formulars. Register (Z8): docs/befehle.json;
// Etappe 20c hängt die Ablauf-Steuerung (XP 112/113/115/116/118/119/105/104)
// hinten an, damit Diff und Register-Reihenfolge Zeile für Zeile lesbar
// bleiben. Kommentare gehören NICHT in den Enum-Block: tools/befehle_test.rb
// liest die Namen dort als Liste.
// tools/befehle_test.rb vergleicht EvType/EV_TYPES mit CMD_TYPES
// (mapcheck.rb), ISSUE_CMD_TYPES (editor_main.cpp) und interpreter.rb.
enum EvType {
    ET_TEXT = 0, ET_CHOICES, ET_LOG,
    ET_WAIT, ET_SWITCH, ET_TOGGLE_SWITCH, ET_VARIABLE,
    ET_IF_SWITCH, ET_IF_VARIABLE, ET_SCRIPT,
    ET_TELEPORT, ET_MOVE_PLAYER, ET_HEAL,
    ET_WEATHER, ET_EFFECT, ET_SOUND,
    ET_ITEM, ET_GOLD,
    ET_SAVE_GAME, ET_LOAD_GAME,
    ET_SELF_SWITCH, ET_IF_SELF_SWITCH, ET_FADE,
    ET_MOVE_ROUTE, ET_INPUT_NUMBER, ET_BATTLE,
    ET_LOOP, ET_BREAK, ET_LABEL, ET_JUMP, ET_EXIT_EVENT, ET_ERASE_EVENT,
    ET_BUTTON_INPUT, ET_TEXT_OPTIONS, ET_COMMON_EVENT,
    ET_IF_GOLD, ET_IF_ITEM, ET_IF_SCRIPT,
    ET_SAVE_ACCESS, ET_MENU_ACCESS, ET_ENCOUNTER_ACCESS, ET_COUNT
};
inline constexpr const char* const EV_TYPES[ET_COUNT] = {
    "text", "choices", "log", "wait", "switch", "toggle_switch", "variable",
    "if_switch", "if_variable", "script", "teleport", "move_player", "heal",
    "weather", "effect", "sound", "item", "gold", "save_game", "load_game",
    "self_switch", "if_self_switch", "fade", "move_route", "input_number", "battle",
    "loop", "break", "label", "jump", "exit_event", "erase_event",
    "button_input", "text_options", "common_event",
    "if_gold", "if_item", "if_script",
    "save_access", "menu_access", "encounter_access"
};
inline constexpr const char* const EVENT_TRIGGERS[] = {"action", "touch", "auto", "parallel"};
inline constexpr const char* const EVENT_OPS[] = {"==", "!=", "<", "<=", ">", ">="};

// Felder des Befehlsformulars: Name, Wert 1/2/3 und Mehrzeiler. Dasselbe
// Schema steuert die Anzeige und verhindert, dass ein geschütztes Alt-Feld
// unbemerkt als Eingabe für einen neu eingefügten/umgewandelten Typ dient.
inline bool event_command_uses_field(int type, size_t field) {
    switch (field) {
        case 0: return type == ET_SWITCH || type == ET_TOGGLE_SWITCH || type == ET_VARIABLE ||
            type == ET_IF_SWITCH || type == ET_IF_VARIABLE || type == ET_SCRIPT || type == ET_SOUND ||
            type == ET_EFFECT || type == ET_ITEM || type == ET_WEATHER || type == ET_SELF_SWITCH ||
            type == ET_IF_SELF_SWITCH || type == ET_FADE || type == ET_MOVE_ROUTE ||
            type == ET_INPUT_NUMBER || type == ET_BATTLE || type == ET_LABEL ||
            type == ET_JUMP || type == ET_BUTTON_INPUT || type == ET_TEXT_OPTIONS ||
            type == ET_COMMON_EVENT || type == ET_IF_ITEM;
        case 1: return type == ET_WAIT || type == ET_VARIABLE || type == ET_IF_VARIABLE ||
            type == ET_TELEPORT || type == ET_MOVE_PLAYER || type == ET_SOUND || type == ET_EFFECT ||
            type == ET_GOLD || type == ET_HEAL || type == ET_WEATHER || type == ET_SAVE_GAME ||
            type == ET_LOAD_GAME || type == ET_FADE || type == ET_MOVE_ROUTE || type == ET_INPUT_NUMBER ||
            type == ET_CHOICES || type == ET_IF_GOLD || type == ET_IF_ITEM;
        case 2: return type == ET_VARIABLE || type == ET_TELEPORT || type == ET_MOVE_PLAYER ||
            type == ET_EFFECT || type == ET_ITEM || type == ET_MOVE_ROUTE || type == ET_INPUT_NUMBER ||
            type == ET_BATTLE || type == ET_TEXT_OPTIONS;
        case 3: return type == ET_TELEPORT || type == ET_MOVE_ROUTE;
        case 4: return type == ET_TEXT || type == ET_LOG || type == ET_CHOICES || type == ET_MOVE_ROUTE ||
            type == ET_BUTTON_INPUT || type == ET_IF_SCRIPT;
        default: return false;
    }
}

inline bool event_command_fields_available(int type, const bool (&editable)[5]) {
    for (size_t i = 0; i < 5; ++i)
        if (event_command_uses_field(type, i) && !editable[i]) return false;
    return true;
}

inline bool event_unique(const JsonValue& obj, const std::string& key) {
    if (!obj.is_obj()) return false;
    unsigned count = 0;
    for (const auto& kv : obj.obj) if (kv.first == key && ++count > 1) return false;
    return true;
}

inline void event_erase(JsonValue& obj, const std::string& key) {
    for (auto it = obj.obj.begin(); it != obj.obj.end(); ++it) {
        if (it->first == key) { obj.obj.erase(it); return; }
    }
}

inline bool event_multiline(const std::string& s) {
    if (!editor_valid_utf8(s)) return false;
    for (unsigned char c : s)
        if ((c < 0x20 && c != '\r' && c != '\n' && c != '\t') || c == 0x7f) return false;
    return true;
}

// Win32-Mehrzeiler verwenden CRLF. Die Normalisierung betrifft nur die
// Anzeige; solange das Feld unverändert bleibt, bleibt der Originalwert exakt.
inline std::string event_multiline_view(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\r' || s[i] == '\n') {
            if (s[i] == '\r' && i + 1 < s.size() && s[i + 1] == '\n') ++i;
            out += "\r\n";
        } else out += s[i];
    }
    return out;
}

inline std::string event_string_view(const JsonValue* value, const std::string& fallback = {},
                                     bool multi = false) {
    if (!value || value->is_null()) return fallback;
    if (!value->is_str() || !(multi ? event_multiline(value->str) : editor_single_line(value->str)))
        return "[nicht darstellbar]";
    return multi ? event_multiline_view(value->str) : value->str;
}

inline std::string event_number_view(const JsonValue* value, const std::string& fallback = {}) {
    if (!value || value->is_null()) return fallback;
    return database_number_text(value);
}

inline int event_bool_view(const JsonValue* value, int fallback) {
    if (!value) return fallback;
    if (value->type != JsonValue::Type::Bool) return -1;
    return value->b ? 0 : 1;
}

inline int event_choice_view(const JsonValue* value, const char* const* options, int count, int fallback) {
    if (!value || value->is_null()) return fallback;
    if (!value->is_str()) return -1;
    for (int i = 0; i < count; ++i) if (value->str == options[i]) return i;
    return -1;
}

inline bool event_list_representable(const JsonValue* value) {
    if (!value || value->is_null()) return true;
    if (!value->is_arr()) return false;
    for (const JsonValue& item : value->arr)
        if (!item.is_str() || !editor_single_line(item.str)) return false;
    return true;
}

inline std::string event_list_view(const JsonValue* value) {
    if (!value || value->is_null()) return {};
    if (!event_list_representable(value)) return "[nicht darstellbare Liste]";
    std::string text;
    for (size_t i = 0; i < value->arr.size(); ++i) {
        if (i) text += "\r\n";
        text += value->arr[i].str;
    }
    return text;
}

struct EventForm { std::string name, x, z, radius; };
struct EventPageForm {
    int trigger = 0;
    std::string switch_name, variable_name, variable_value;
    int switch_value = 0, variable_op = 0, self_letter = 0, self_value = 0;
};
struct EventCommandForm {
    int type = ET_TEXT;
    std::string name, val1, val2, val3, text;
    int op = 0, flag = 0;
};

inline bool event_form_equal(const EventForm& a, const EventForm& b) {
    return a.name == b.name && a.x == b.x && a.z == b.z && a.radius == b.radius;
}
inline bool event_page_form_equal(const EventPageForm& a, const EventPageForm& b) {
    return a.trigger == b.trigger && a.switch_name == b.switch_name && a.switch_value == b.switch_value &&
           a.variable_name == b.variable_name && a.variable_op == b.variable_op &&
           a.variable_value == b.variable_value && a.self_letter == b.self_letter && a.self_value == b.self_value;
}
inline bool event_command_form_equal(const EventCommandForm& a, const EventCommandForm& b) {
    return a.type == b.type && a.name == b.name && a.val1 == b.val1 && a.val2 == b.val2 &&
           a.val3 == b.val3 && a.text == b.text && a.op == b.op && a.flag == b.flag;
}

struct EventEdit {
    bool ok = true, changed = false;
    bool replaces_invalid = false, removes_condition = false;
    bool converts_command = false, reassigns_choices = false;
    bool renames_event = false, unshifted_coordinate = false;
    std::string field, error;
    JsonValue value;
};

inline EventEdit event_failure(const JsonValue& original, const std::string& field, const std::string& error) {
    EventEdit r;
    r.ok = false; r.field = field; r.error = error; r.value = original;
    return r;
}

inline void event_merge_warnings(EventEdit& target, const EventEdit& part) {
    target.replaces_invalid |= part.replaces_invalid;
    target.removes_condition |= part.removes_condition;
    target.converts_command |= part.converts_command;
    target.reassigns_choices |= part.reassigns_choices;
    target.renames_event |= part.renames_event;
    target.unshifted_coordinate |= part.unshifted_coordinate;
}

// Bei pages fehlt/nicht Array/leer verwendet die Ruby-Laufzeit die alten
// Top-Level-Felder. Nicht-Objekt-Seiten bleiben sichtbar, aber uneditierbar.
inline bool event_legacy_page(const JsonValue& e) {
    const JsonValue* pages = e.is_obj() ? e.find("pages") : nullptr;
    return !pages || !pages->is_arr() || pages->arr.empty();
}
inline size_t event_page_count(const JsonValue& e) {
    if (!e.is_obj() || !event_unique(e, "pages")) return 0;
    const JsonValue* pages = e.find("pages");
    return pages && pages->is_arr() && !pages->arr.empty() ? pages->arr.size() : 1;
}
inline const JsonValue* event_page_view(const JsonValue& e, size_t index) {
    if (index >= event_page_count(e)) return nullptr;
    if (event_legacy_page(e)) return index == 0 ? &e : nullptr;
    const JsonValue& page = e.find("pages")->arr[index];
    return page.is_obj() ? &page : nullptr;
}
inline JsonValue* event_page_mutable(JsonValue& e, size_t index) {
    return const_cast<JsonValue*>(event_page_view(static_cast<const JsonValue&>(e), index));
}
inline const JsonValue* event_commands_view(const JsonValue& e, size_t page) {
    const JsonValue* pg = event_page_view(e, page);
    if (!pg || !event_unique(*pg, "commands")) return nullptr;
    const JsonValue* cmds = pg->find("commands");
    return cmds && cmds->is_arr() ? cmds : nullptr;
}

inline EventForm event_form_read(const JsonValue& e) {
    if (!e.is_obj()) return {};
    auto text = [&](const char* key, const char* fallback) {
        return event_unique(e, key) ? event_string_view(e.find(key), fallback) : "[mehrfach vorhanden]";
    };
    auto number = [&](const char* key, const char* fallback) {
        return event_unique(e, key) ? event_number_view(e.find(key), fallback) : "[mehrfach vorhanden]";
    };
    return {text("name", "Event"), number("x", "0"), number("z", "0"), number("radius", "2")};
}

inline EventPageForm event_page_form_read(const JsonValue& e, size_t page) {
    EventPageForm f;
    const JsonValue* pg = event_page_view(e, page);
    if (!pg) { f.trigger = -1; return f; }
    f.trigger = event_unique(*pg, "trigger") ? event_choice_view(pg->find("trigger"), EVENT_TRIGGERS, 4, 0) : -1;
    // Top-Level conditions gehören NICHT zur impliziten alten Seite.
    if (!event_legacy_page(e) && !event_unique(*pg, "conditions")) {
        f.switch_name = f.variable_name = "[mehrfach vorhanden]";
        f.switch_value = f.variable_op = f.self_letter = f.self_value = -1;
        return f;
    }
    const JsonValue* c = event_legacy_page(e) ? nullptr : pg->find("conditions");
    if (!c || !c->is_obj()) return f;
    const JsonValue* sw = c->find("switch");
    if (!event_unique(*c, "switch")) { f.switch_name = "[mehrfach vorhanden]"; f.switch_value = -1; }
    else if (sw && sw->is_obj()) {
        f.switch_name = event_unique(*sw, "name") ? event_string_view(sw->find("name")) : "[mehrfach vorhanden]";
        f.switch_value = event_unique(*sw, "value") ? event_bool_view(sw->find("value"), 1) : -1;
    }
    const JsonValue* va = c->find("variable");
    if (!event_unique(*c, "variable")) { f.variable_name = "[mehrfach vorhanden]"; f.variable_op = -1; }
    else if (va && va->is_obj()) {
        f.variable_name = event_unique(*va, "name") ? event_string_view(va->find("name")) : "[mehrfach vorhanden]";
        f.variable_op = event_unique(*va, "op") ? event_choice_view(va->find("op"), EVENT_OPS, 6, 0) : -1;
        f.variable_value = event_unique(*va, "value") ? event_number_view(va->find("value"), "0") : "[mehrfach vorhanden]";
    }
    const JsonValue* ss = c->find("self_switch");
    if (!event_unique(*c, "self_switch")) f.self_letter = f.self_value = -1;
    else if (ss && ss->is_obj()) {
        const JsonValue* l = ss->find("letter");
        f.self_letter = event_unique(*ss, "letter") && l && l->is_str() && l->str.size() == 1 && l->str[0] >= 'A' && l->str[0] <= 'D'
                            ? l->str[0] - 'A' + 1 : -1;
        f.self_value = event_unique(*ss, "value") ? event_bool_view(ss->find("value"), 1) : -1;
    }
    return f;
}

inline bool event_number_parse(const std::string& s, bool integer, bool finite, JsonValue& out) {
    if (!editor_single_line(s)) return false;
    if (database_integer(s, out)) return !finite || std::isfinite(out.number());
    if (integer || !database_decimal(s, out)) return false;
    return !finite || std::isfinite(out.number());
}

inline EventEdit event_form_prepare(const JsonValue& old, const EventForm& before, const EventForm& after) {
    if (!old.is_obj()) return event_failure(old, "event", "Dieser Event-Eintrag ist kein Objekt.");
    EventEdit r; r.value = old;
    auto number = [&](const char* key, const std::string& was, const std::string& now, bool positive) {
        if (was == now) return true;
        if (!event_unique(old, key)) { r = event_failure(old, key, "Dieser JSON-Schlüssel ist mehrfach vorhanden."); return false; }
        JsonValue n;
        if (!event_number_parse(now, false, true, n) || std::fabs(n.number()) > 1000000.0 ||
            (positive && n.number() <= 0.0)) {
            r = event_failure(old, key, positive ? "Radius: endliche Zahl größer als 0 und höchstens 1000000 eingeben."
                                                  : "Position: endliche Zahl zwischen -1000000 und 1000000 eingeben.");
            return false;
        }
        const JsonValue* v = old.find(key);
        if (v && !v->is_null() && (!v->is_num() || !std::isfinite(v->number()))) r.replaces_invalid = true;
        r.value.set(key, std::move(n));
        return true;
    };
    if (before.name != after.name) {
        if (!event_unique(old, "name")) return event_failure(old, "name", "Der Name ist mehrfach vorhanden.");
        if (!editor_single_line(after.name)) return event_failure(old, "name", "Der Name muss einzeiliges UTF-8 ohne Steuerzeichen sein.");
        const JsonValue* v = old.find("name");
        if (v && !v->is_null() && (!v->is_str() || !editor_single_line(v->str))) r.replaces_invalid = true;
        // Spielstände und Self-Switches referenzieren den Eventnamen. Ein
        // neuer Name ändert diese Identität, auch ohne Namenskollision.
        r.renames_event = true;
        r.value.set("name", JsonValue::make_str(after.name));
    }
    if (!number("x", before.x, after.x, false) || !number("z", before.z, after.z, false) ||
        !number("radius", before.radius, after.radius, true)) return r;
    r.changed = json_write(r.value) != json_write(old);
    return r;
}

// Nur bei echtem Bedarf migrieren. Top-Level-Felder bleiben als Altdaten
// erhalten. Beschädigtes pages oder eine semantisch nicht äquivalente alte
// Seite wird nicht überschrieben; diese Fälle müssen extern korrigiert werden.
inline bool event_materialize_pages(JsonValue& e, std::string& error) {
    if (!e.is_obj() || !event_unique(e, "pages")) { error = "pages ist mehrfach vorhanden oder das Event ist kein Objekt."; return false; }
    JsonValue* pages = e.find("pages");
    if (pages && pages->is_arr() && !pages->arr.empty()) return true;
    if (pages && !pages->is_arr()) { error = "pages ist keine Liste. Die Altdaten bleiben erhalten; bitte extern korrigieren."; return false; }
    if (!event_unique(e, "trigger") || !event_unique(e, "commands")) {
        error = "Alte trigger-/commands-Schlüssel sind mehrfach vorhanden; Migration nicht eindeutig."; return false;
    }
    const JsonValue* trigger = e.find("trigger");
    if (trigger && !trigger->is_null() && !(trigger->type == JsonValue::Type::Bool && !trigger->b) &&
        event_choice_view(trigger, EVENT_TRIGGERS, 4, -1) < 0) {
        error = "Der alte Trigger würde bei der Seitenmigration anders wirken; bitte zuerst korrigieren."; return false;
    }
    const JsonValue* commands = e.find("commands");
    if (commands && !commands->is_null() && !(commands->type == JsonValue::Type::Bool && !commands->b) && !commands->is_arr()) {
        error = "Die alte Befehlsliste würde bei der Seitenmigration anders wirken; bitte zuerst korrigieren."; return false;
    }
    JsonValue pg = JsonValue::make_obj();
    if (trigger) pg.set("trigger", *trigger);
    if (commands) pg.set("commands", *commands);
    JsonValue list = JsonValue::make_arr();
    list.arr.push_back(std::move(pg));
    e.set("pages", std::move(list));
    return true;
}

inline EventEdit event_page_prepare(const JsonValue& old, size_t index,
                                    const EventPageForm& before, const EventPageForm& after) {
    if (!event_page_view(old, index)) return event_failure(old, "page", "Diese Seite ist nicht eindeutig oder kein Objekt.");
    EventEdit r; r.value = old;
    const bool sw_change = before.switch_name != after.switch_name || before.switch_value != after.switch_value;
    const bool va_change = before.variable_name != after.variable_name || before.variable_op != after.variable_op ||
                           before.variable_value != after.variable_value;
    const bool ss_change = before.self_letter != after.self_letter || before.self_value != after.self_value;
    if ((sw_change || va_change || ss_change) && event_legacy_page(r.value)) {
        std::string error;
        if (!event_materialize_pages(r.value, error)) return event_failure(old, "page", error);
    }
    JsonValue* pg = event_page_mutable(r.value, index);
    if (!pg) return event_failure(old, "page", "Die Seite ist nicht mehr verfügbar.");
    if (before.trigger != after.trigger) {
        if (!event_unique(*pg, "trigger")) return event_failure(old, "trigger", "Trigger ist mehrfach vorhanden.");
        if (after.trigger < 0 || after.trigger >= 4) return event_failure(old, "trigger", "Einen gültigen Trigger auswählen.");
        const JsonValue* prev = pg->find("trigger");
        if (prev && !prev->is_null() && event_choice_view(prev, EVENT_TRIGGERS, 4, -1) < 0) r.replaces_invalid = true;
        pg->set("trigger", JsonValue::make_str(EVENT_TRIGGERS[after.trigger]));
    }
    if (sw_change || va_change || ss_change) {
        if (!event_unique(*pg, "conditions")) return event_failure(old, "page", "conditions ist mehrfach vorhanden.");
        const JsonValue* existing = pg->find("conditions");
        JsonValue cond = existing && existing->is_obj() ? *existing : JsonValue::make_obj();
        if (existing && !existing->is_obj() && !existing->is_null()) r.replaces_invalid = true;
        auto get_group = [&](const char* key, JsonValue& group) {
            if (!event_unique(cond, key)) { r = event_failure(old, "page", "Eine Bedingung ist mehrfach vorhanden."); return false; }
            const JsonValue* v = cond.find(key);
            if (v && v->is_obj() &&
                (!event_unique(*v, "value") ||
                 (std::string(key) == "self_switch" ? !event_unique(*v, "letter") : !event_unique(*v, "name")) ||
                 (std::string(key) == "variable" && !event_unique(*v, "op")))) {
                r = event_failure(old, "page", "Ein Feld dieser Bedingung ist mehrfach vorhanden."); return false;
            }
            if (v && !v->is_obj() && !v->is_null()) r.replaces_invalid = true;
            group = v && v->is_obj() ? *v : JsonValue::make_obj();
            return true;
        };
        auto group_text = [&](JsonValue& group, const char* key, const std::string& text, const char* field) {
            if (!event_unique(group, key)) { r = event_failure(old, field, "Der JSON-Schlüssel ist mehrfach vorhanden."); return false; }
            if (!editor_single_line(text)) { r = event_failure(old, field, "Bitte einzeiligen Text ohne Steuerzeichen eingeben."); return false; }
            const JsonValue* prev = group.find(key);
            if (prev && !prev->is_null() && (!prev->is_str() || !editor_single_line(prev->str))) r.replaces_invalid = true;
            group.set(key, JsonValue::make_str(text)); return true;
        };
        auto group_bool = [&](JsonValue& group, int selection, const char* field) {
            if (!event_unique(group, "value")) { r = event_failure(old, field, "value ist mehrfach vorhanden."); return false; }
            if (selection < 0 || selection > 1) { r = event_failure(old, field, "Bitte an oder aus auswählen."); return false; }
            const JsonValue* prev = group.find("value");
            if (prev && !prev->is_null() && prev->type != JsonValue::Type::Bool) r.replaces_invalid = true;
            group.set("value", JsonValue::make_bool(selection == 0)); return true;
        };
        if (sw_change) {
            JsonValue sw;
            if (!get_group("switch", sw)) return r;
            const bool existed = cond.find("switch") && cond.find("switch")->is_obj();
            if (after.switch_name.empty()) {
                if (before.switch_name == after.switch_name) return event_failure(old, "switch_name", "Für eine Schalterbedingung einen Namen eingeben.");
                if (cond.find("switch")) { event_erase(cond, "switch"); r.removes_condition = true; }
            } else {
                if ((!existed || before.switch_name != after.switch_name) && !group_text(sw, "name", after.switch_name, "switch_name")) return r;
                if ((!existed || before.switch_value != after.switch_value) && !group_bool(sw, after.switch_value, "switch_value")) return r;
                cond.set("switch", std::move(sw));
            }
        }
        if (va_change) {
            JsonValue va;
            if (!get_group("variable", va)) return r;
            const bool existed = cond.find("variable") && cond.find("variable")->is_obj();
            if (after.variable_name.empty()) {
                if (before.variable_name == after.variable_name) return event_failure(old, "variable_name", "Für eine Variablenbedingung einen Namen eingeben.");
                if (cond.find("variable")) { event_erase(cond, "variable"); r.removes_condition = true; }
            } else {
                if ((!existed || before.variable_name != after.variable_name) && !group_text(va, "name", after.variable_name, "variable_name")) return r;
                if (!existed || before.variable_op != after.variable_op) {
                    if (!event_unique(va, "op")) return event_failure(old, "variable_op", "op ist mehrfach vorhanden.");
                    if (after.variable_op < 0 || after.variable_op >= 6) return event_failure(old, "variable_op", "Einen gültigen Operator auswählen.");
                    const JsonValue* prev = va.find("op");
                    if (prev && !prev->is_null() && event_choice_view(prev, EVENT_OPS, 6, -1) < 0) r.replaces_invalid = true;
                    va.set("op", JsonValue::make_str(EVENT_OPS[after.variable_op]));
                }
                if (!existed || before.variable_value != after.variable_value) {
                    if (!event_unique(va, "value")) return event_failure(old, "variable_value", "value ist mehrfach vorhanden.");
                    JsonValue value;
                    const std::string text = !existed && after.variable_value.empty() ? "0" : after.variable_value;
                    if (!event_number_parse(text, false, true, value)) return event_failure(old, "variable_value", "Eine gültige endliche Zahl eingeben.");
                    const JsonValue* prev = va.find("value");
                    if (prev && !prev->is_null() && (!prev->is_num() || !std::isfinite(prev->number()))) r.replaces_invalid = true;
                    va.set("value", std::move(value));
                }
                cond.set("variable", std::move(va));
            }
        }
        if (ss_change) {
            JsonValue ss;
            if (!get_group("self_switch", ss)) return r;
            const bool existed = cond.find("self_switch") && cond.find("self_switch")->is_obj();
            if (after.self_letter == 0) {
                if (before.self_letter == after.self_letter) return event_failure(old, "self_letter", "Für die Bedingung A bis D auswählen.");
                if (cond.find("self_switch")) { event_erase(cond, "self_switch"); r.removes_condition = true; }
            } else {
                if (after.self_letter < 1 || after.self_letter > 4) return event_failure(old, "self_letter", "Für die Bedingung A bis D auswählen.");
                if ((!existed || before.self_letter != after.self_letter) &&
                    !group_text(ss, "letter", std::string(1, static_cast<char>('A' + after.self_letter - 1)), "self_letter")) return r;
                if ((!existed || before.self_value != after.self_value) && !group_bool(ss, after.self_value, "self_value")) return r;
                cond.set("self_switch", std::move(ss));
            }
        }
        pg->set("conditions", std::move(cond));
    }
    r.changed = json_write(r.value) != json_write(old);
    return r;
}

inline EventEdit event_add_page_prepare(const JsonValue& old) {
    EventEdit r; r.value = old;
    std::string error;
    if (!event_materialize_pages(r.value, error)) return event_failure(old, "page", error);
    JsonValue page = JsonValue::make_obj();
    page.set("conditions", JsonValue::make_obj());
    page.set("trigger", JsonValue::make_str("action"));
    page.set("commands", JsonValue::make_arr());
    r.value.find("pages")->arr.push_back(std::move(page));
    r.changed = true;
    return r;
}

inline EventEdit event_delete_page_prepare(const JsonValue& old, size_t page) {
    if (!old.is_obj() || !event_unique(old, "pages")) return event_failure(old, "page", "pages ist nicht eindeutig.");
    const JsonValue* pages = old.find("pages");
    if (!pages || !pages->is_arr() || pages->arr.size() <= 1 || page >= pages->arr.size())
        return event_failure(old, "page", "Die letzte Seite kann nicht gelöscht werden.");
    EventEdit r; r.value = old;
    auto& items = r.value.find("pages")->arr;
    items.erase(items.begin() + static_cast<std::ptrdiff_t>(page));
    r.changed = true;
    return r;
}

// Jede Stufe adressiert eine Liste, nicht einen langlebigen Zeiger in den
// JSON-Baum. Nach Kopie, Migration, Vektorwachstum und Commit neu auflösen.
struct EventBranchStep {
    size_t command = 0;
    std::string key; // then, else oder branches
    size_t choice = 0;
};
using EventCommandPath = std::vector<EventBranchStep>;

inline JsonValue* event_command_list_mutable(JsonValue& e, size_t page, const EventCommandPath& path,
                                              bool create, std::string& error) {
    JsonValue* pg = event_page_mutable(e, page);
    if (!pg) { error = "Die Seite ist nicht eindeutig oder kein Objekt."; return nullptr; }
    if (!event_unique(*pg, "commands")) { error = "commands ist mehrfach vorhanden."; return nullptr; }
    JsonValue* list = pg->find("commands");
    if (!list && create) list = &pg->set("commands", JsonValue::make_arr());
    if (!list || !list->is_arr()) { error = "commands fehlt oder ist keine Liste; bestehende Daten werden nicht ersetzt."; return nullptr; }
    for (const EventBranchStep& step : path) {
        if (step.command >= list->arr.size() || !list->arr[step.command].is_obj()) {
            error = "Der Befehlszweig ist nicht mehr verfügbar."; return nullptr;
        }
        JsonValue& command = list->arr[step.command];
        if (!event_unique(command, "cmd") || !event_unique(command, step.key)) {
            error = "Befehl oder Zweig ist mehrfach vorhanden."; return nullptr;
        }
        const JsonValue* typ = command.find("cmd");
        const std::string type = typ && typ->is_str() ? typ->str : "";
        if (step.key == "then" || step.key == "else") {
            if (type != "if_switch" && type != "if_variable" && type != "if_self_switch" &&
                type != "if_gold" && type != "if_item" && type != "if_script") {
                error = "Der Wenn-Zweig gehört nicht zu einem Wenn-Befehl."; return nullptr;
            }
            JsonValue* next = command.find(step.key);
            if (!next && create) next = &command.set(step.key, JsonValue::make_arr());
            if (!next || !next->is_arr()) { error = "Der Zweig ist keine Liste; Altdaten bleiben erhalten."; return nullptr; }
            list = next;
        } else if (step.key == "branches") {
            if (type != "choices" || !event_unique(command, "options")) {
                error = "Die Optionen sind nicht eindeutig."; return nullptr;
            }
            const JsonValue* opts = command.find("options");
            if (!opts || !opts->is_arr() || step.choice >= opts->arr.size()) {
                error = "Die Wahlmöglichkeit ist nicht mehr verfügbar."; return nullptr;
            }
            JsonValue* branches = command.find("branches");
            if (!branches && create) branches = &command.set("branches", JsonValue::make_arr());
            if (!branches || !branches->is_arr()) { error = "branches ist keine Liste; Altdaten bleiben erhalten."; return nullptr; }
            if (step.choice >= branches->arr.size() && create)
                while (branches->arr.size() <= step.choice) branches->arr.push_back(JsonValue::make_arr());
            if (step.choice >= branches->arr.size() || !branches->arr[step.choice].is_arr()) {
                error = "Dieser Wahlzweig ist keine Liste; Altdaten bleiben erhalten."; return nullptr;
            }
            list = &branches->arr[step.choice];
        } else if (step.key == "commands") {
            // Etappe 20c: Körper einer Schleife (XP 112/413).
            if (type != "loop") { error = "Der Schleifenkörper gehört nicht zu diesem Befehl."; return nullptr; }
            JsonValue* body = command.find("commands");
            if (!body && create) body = &command.set("commands", JsonValue::make_arr());
            if (!body || !body->is_arr()) { error = "commands ist keine Liste; Altdaten bleiben erhalten."; return nullptr; }
            list = body;
        } else if (step.key == "cancel_branch") {
            // Z5: eigener Abbruch-Zweig (XP 403) - Pfad wie branches, nur
            // ohne Optionsindex; create erzeugt die fehlende Liste.
            if (type != "choices" || !event_unique(command, "cancel_branch")) {
                error = "Der Abbruch-Zweig ist nicht eindeutig.";
                return nullptr;
            }
            JsonValue* cb = command.find("cancel_branch");
            if (!cb && create) cb = &command.set("cancel_branch", JsonValue::make_arr());
            if (!cb || !cb->is_arr()) {
                error = "cancel_branch ist keine Liste; Altdaten bleiben erhalten.";
                return nullptr;
            }
            list = cb;
        } else { error = "Unbekannter Befehlszweig."; return nullptr; }
    }
    return list;
}

inline std::string event_negate_number_text(const JsonValue* value) {
    std::string s = event_number_view(value);
    if (s.empty() || s[0] == '[') return s;
    if (s[0] == '-') return s.substr(1);
    return s == "0" ? s : "-" + s;
}

inline EventCommandForm event_command_form_read(const JsonValue& c) {
    EventCommandForm f;
    f.type = -1;
    if (!c.is_obj() || !event_unique(c, "cmd")) return f;
    const JsonValue* kind = c.find("cmd");
    if (kind && kind->is_str()) for (int i = 0; i < ET_COUNT; ++i) if (kind->str == EV_TYPES[i]) f.type = i;
    if (f.type < 0) return f;
    auto text = [&](const char* key, const std::string& def = {}) {
        return event_unique(c, key) ? event_string_view(c.find(key), def) : "[mehrfach vorhanden]";
    };
    auto num = [&](const char* key, const std::string& def = {}) {
        return event_unique(c, key) ? event_number_view(c.find(key), def) : "[mehrfach vorhanden]";
    };
    auto flag = [&](const char* key, int def) { return event_unique(c, key) ? event_bool_view(c.find(key), def) : -1; };
    auto lines = [&](const char* key) { return event_unique(c, key) ? event_list_view(c.find(key)) : "[mehrfach vorhanden]"; };
    f.name = text("name");
    switch (f.type) {
        case ET_TEXT: f.text = lines("lines"); break;
        case ET_CHOICES: {
            f.text = lines("options");
            // Z5: cancel (XP "When Cancel"): "" = nicht erlaubt (XP-
            // Default), "aus" explizit gesperrt, "verzweigung" = eigener
            // Abbruch-Zweig, Zahl = Esc wie diese Option (0-basiert).
            if (!event_unique(c, "cancel")) f.val1 = "[mehrfach vorhanden]";
            else {
                const JsonValue* cx = c.find("cancel");
                if (!cx || cx->is_null() ||
                    (cx->type == JsonValue::Type::Bool && !cx->b)) f.val1 = "";
                else if (cx->type == JsonValue::Type::Bool) f.val1 = "verzweigung";
                else if (cx->is_str()) f.val1 = cx->str;
                else if (cx->is_num()) f.val1 = event_number_view(cx, "");
                else f.val1 = "[ungültig]";
            }
            break;
        }
        case ET_LOG: f.text = event_unique(c, "text") ? event_string_view(c.find("text"), "", true) : "[mehrfach vorhanden]"; break;
        case ET_WAIT: f.val1 = num("seconds", "0"); break;
        case ET_SWITCH: case ET_IF_SWITCH: f.flag = flag("value", 1); break;
        case ET_SELF_SWITCH: case ET_IF_SELF_SWITCH:
            f.name = text("letter"); f.flag = flag("value", 1); break;
        case ET_FADE: f.name = text("direction", "aus"); f.val1 = num("seconds", "0.5"); break;
        case ET_MOVE_ROUTE:
            f.name = text("target"); f.text = lines("steps"); f.val1 = num("speed");
            f.val2 = c.find("wait") ? (flag("wait", 1) == 0 ? "1" : flag("wait", 1) == 1 ? "0" : "[ungültig/mehrfach]") : "";
            f.val3 = text("code"); f.flag = flag("repeat", 1); break;
        case ET_INPUT_NUMBER:
            f.val1 = num("digits"); f.val2 = num("initial"); f.flag = flag("cancel", 0); break;
        case ET_BATTLE:
            f.val2 = c.find("can_lose") ? (flag("can_lose", 1) == 0 ? "1" : flag("can_lose", 1) == 1 ? "0" : "[ungültig/mehrfach]") : "";
            f.flag = flag("can_escape", 0); break;
        case ET_VARIABLE: f.val1 = num("value"); f.val2 = num("add"); break;
        case ET_IF_VARIABLE: f.op = event_unique(c, "op") ? event_choice_view(c.find("op"), EVENT_OPS, 6, 0) : -1; f.val1 = num("value", "0"); break;
        case ET_TELEPORT: f.val1 = num("x", "0"); f.val2 = num("z", "0"); f.val3 = num("yaw", "3.14159265"); break;
        case ET_MOVE_PLAYER: f.val1 = num("x", "0"); f.val2 = num("z", "0"); break;
        case ET_SOUND: f.val1 = num("volume", "1"); break;
        case ET_EFFECT: {
            f.val1 = num("height", "1");
            const JsonValue* color = c.find("color");
            if (!event_unique(c, "color")) f.val2 = "[mehrfach vorhanden]";
            else if (!color || color->is_null()) f.val2 = "FFD866";
            else if (!color->is_num() || !std::isfinite(color->number()) || color->number() < 0 ||
                     color->number() > 0xffffff || std::floor(color->number()) != color->number()) f.val2 = "[ungültig]";
            else { char hex[16]; std::snprintf(hex, sizeof(hex), "%06X", static_cast<unsigned>(color->number())); f.val2 = hex; }
            break;
        }
        case ET_ITEM:
            if (!event_unique(c, "take") || !event_unique(c, "count")) f.val2 = "[mehrfach vorhanden]";
            else if (c.find("take")) f.val2 = event_negate_number_text(c.find("take"));
            else f.val2 = num("count", "1");
            break;
        case ET_GOLD: f.val1 = num("amount", "0"); break;
        case ET_HEAL: f.val1 = num("amount"); break;
        case ET_WEATHER: f.val1 = num("intensity", "1"); break;
        case ET_SAVE_GAME: case ET_LOAD_GAME: f.val1 = num("slot", "1"); break;
        case ET_BUTTON_INPUT: f.text = lines("keys"); break;
        case ET_IF_GOLD: case ET_IF_ITEM:
            f.op = event_unique(c, "op") ? event_choice_view(c.find("op"), EVENT_OPS, 6, 0) : -1;
            f.val1 = num("value", "0"); break;
        case ET_IF_SCRIPT:
            f.text = event_unique(c, "code") ? event_string_view(c.find("code"), "", true) : "[mehrfach vorhanden]";
            break;
        case ET_SAVE_ACCESS: case ET_MENU_ACCESS: case ET_ENCOUNTER_ACCESS:
            f.flag = flag("allowed", 0); break;
        case ET_TEXT_OPTIONS:
            f.name = text("position", "unten");
            f.flag = flag("frame", 0);
            // val2 1/0/leer wie bei battle.can_lose: leer = Schlüssel fehlt
            // (XP-Default: nicht transparent).
            f.val2 = c.find("transparent")
                         ? (flag("transparent", 1) == 0 ? "1"
                            : flag("transparent", 1) == 1 ? "0" : "[ungültig/mehrfach]")
                         : "";
            break;
        default: break;
    }
    return f;
}

// Doppelte für einen Typ relevante Schlüssel machen seine Anzeige und Ruby-
// Semantik mehrdeutig (Ruby nimmt den letzten, JsonValue den ersten Treffer).
inline std::vector<const char*> event_command_keys(int type) {
    switch (type) {
        case ET_TEXT: return {"lines"};
        case ET_CHOICES: return {"options", "branches", "cancel", "cancel_branch"};
        case ET_LOG: return {"text"};
        case ET_WAIT: return {"seconds"};
        case ET_SWITCH: return {"name", "value"};
        case ET_TOGGLE_SWITCH: return {"name"};
        case ET_VARIABLE: return {"name", "value", "add"};
        case ET_IF_SWITCH: return {"name", "value", "then", "else"};
        case ET_IF_VARIABLE: return {"name", "value", "op", "then", "else"};
        case ET_SCRIPT: return {"name", "code"};
        case ET_TELEPORT: return {"x", "z", "yaw"};
        case ET_MOVE_PLAYER: return {"x", "z", "relative"};
        case ET_HEAL: case ET_GOLD: return {"amount"};
        case ET_WEATHER: return {"name", "intensity"};
        case ET_EFFECT: return {"name", "height", "color"};
        case ET_SOUND: return {"name", "volume"};
        case ET_ITEM: return {"name", "count", "take"};
        case ET_SAVE_GAME: case ET_LOAD_GAME: return {"slot"};
        case ET_SELF_SWITCH: return {"letter", "value"};
        case ET_IF_SELF_SWITCH: return {"letter", "value", "then", "else"};
        case ET_FADE: return {"direction", "seconds"};
        case ET_MOVE_ROUTE: return {"target", "steps", "speed", "wait", "code", "repeat"};
        case ET_INPUT_NUMBER: return {"name", "digits", "initial", "cancel"};
        case ET_BATTLE: return {"name", "can_escape", "can_lose"};
        case ET_LOOP: return {"commands"};
        case ET_BREAK: case ET_EXIT_EVENT: case ET_ERASE_EVENT: return {};
        case ET_LABEL: case ET_JUMP: return {"name"};
        case ET_BUTTON_INPUT: return {"name", "keys"};
        case ET_TEXT_OPTIONS: return {"position", "frame", "transparent"};
        case ET_COMMON_EVENT: return {"name"};
        case ET_IF_GOLD: return {"op", "value", "then", "else"};
        case ET_IF_ITEM: return {"name", "op", "value", "then", "else"};
        case ET_IF_SCRIPT: return {"code", "name", "then", "else"};
        case ET_SAVE_ACCESS: case ET_MENU_ACCESS: case ET_ENCOUNTER_ACCESS: return {"allowed"};
        default: return {};
    }
}

// existing == nullptr erzeugt einen neuen Befehl. Ein Typwechsel ersetzt den
// ganzen Befehl nur nach ausdrücklicher Bestätigung; bei gleichem Typ werden
// ausschließlich geänderte Felder gepatcht, inklusive aller fremden Felder.
inline EventEdit event_command_prepare(const JsonValue* existing, const EventCommandForm& before,
                                       const EventCommandForm& after) {
    const JsonValue original = existing ? *existing : JsonValue::make_obj();
    if (existing && event_command_form_equal(before, after)) { EventEdit r; r.value = original; return r; }
    if (after.type < 0 || after.type >= ET_COUNT) return event_failure(original, "cmd_type", "Einen gültigen Befehlstyp auswählen.");
    if (existing && existing->is_obj() && !event_unique(*existing, "cmd"))
        return event_failure(original, "cmd_type", "cmd ist mehrfach vorhanden und nicht eindeutig bearbeitbar.");
    const bool fresh = !existing || !existing->is_obj() || before.type != after.type;
    if (!fresh) for (const char* key : event_command_keys(after.type))
        if (!event_unique(*existing, key)) return event_failure(original, "cmd_type", "Ein Feld dieses Befehls ist mehrfach vorhanden; bitte extern prüfen.");
    EventEdit r;
    r.value = fresh ? JsonValue::make_obj() : original;
    r.converts_command = existing && fresh;
    if (fresh) r.value.set("cmd", JsonValue::make_str(EV_TYPES[after.type]));
    auto fail = [&](const char* field, const char* message) {
        r = event_failure(original, field, message);
        return false;
    };
    auto unique = [&](const char* key, const char* field) {
        return event_unique(r.value, key) || fail(field, "Dieser JSON-Schlüssel ist mehrfach vorhanden.");
    };
    auto text = [&](const char* key, const char* field, const std::string& was, const std::string& now,
                    bool optional, bool multiline, bool required) {
        if (!fresh && was == now) return true;
        if (!unique(key, field)) return false;
        if (!(multiline ? event_multiline(now) : editor_single_line(now)))
            return fail(field, "Ungültiger Text oder Steuerzeichen im Formular.");
        if (required && database_trim(now).empty()) return fail(field, "Bitte einen Namen eingeben.");
        const JsonValue* v = r.value.find(key);
        if (v && !v->is_null() && (!v->is_str() || !(multiline ? event_multiline(v->str) : editor_single_line(v->str))))
            r.replaces_invalid = true;
        if (optional && now.empty()) event_erase(r.value, key);
        else r.value.set(key, JsonValue::make_str(now));
        return true;
    };
    auto number = [&](const char* key, const char* field, const std::string& was, const std::string& now,
                      bool optional, const char* fallback, bool integer, bool finite,
                      double min, double max, bool strict_min = false) {
        if (!fresh && was == now) return true;
        if (!unique(key, field)) return false;
        const JsonValue* v = r.value.find(key);
        if (v && !v->is_null() && (!v->is_num() || (finite && !std::isfinite(v->number())))) r.replaces_invalid = true;
        if (optional && now.empty()) { event_erase(r.value, key); return true; }
        const std::string s = fresh && now.empty() ? fallback : now;
        JsonValue value;
        if (!event_number_parse(s, integer, finite, value) ||
            ((finite || std::isfinite(value.number())) &&
             (value.number() < min || value.number() > max || (strict_min && value.number() <= min))))
            return fail(field, integer ? "Bitte eine gültige Ganzzahl im erlaubten Bereich eingeben."
                                       : "Bitte eine gültige endliche Zahl im erlaubten Bereich eingeben.");
        r.value.set(key, std::move(value)); return true;
    };
    auto boolean = [&](const char* key, const char* field, int was, int now) {
        if (!fresh && was == now) return true;
        if (!unique(key, field)) return false;
        if (now < 0 || now > 1) return fail(field, "Bitte an oder aus auswählen.");
        const JsonValue* v = r.value.find(key);
        if (v && !v->is_null() && v->type != JsonValue::Type::Bool) r.replaces_invalid = true;
        r.value.set(key, JsonValue::make_bool(now == 0)); return true;
    };
    auto bool_text = [&](const char* key, const char* field, const std::string& was, const std::string& now) {
        if (!fresh && was == now) return true;
        if (!unique(key, field)) return false;
        const JsonValue* v = r.value.find(key);
        if (v && !v->is_null() && v->type != JsonValue::Type::Bool) r.replaces_invalid = true;
        if (now.empty()) { event_erase(r.value, key); return true; }
        if (now != "0" && now != "1") return fail(field, "Bitte 1, 0 oder ein leeres Feld eingeben.");
        r.value.set(key, JsonValue::make_bool(now == "1")); return true;
    };
    auto list = [&](const char* key, const char* field, const std::string& was, const std::string& now,
                    bool skip_empty) {
        if (!fresh && was == now) return true;
        if (!unique(key, field)) return false;
        if (!event_multiline(now)) return fail(field, "Die Liste enthält ungültige Steuerzeichen oder ungültiges UTF-8.");
        JsonValue items = JsonValue::make_arr();
        if (!skip_empty || !now.empty()) for (const std::string& line : placement_lines(now)) {
            if (!editor_single_line(line)) return fail(field, "Jede Zeile muss einzeiliger Text ohne Steuerzeichen sein.");
            if (skip_empty && line.empty()) return fail(field, "Bitte keine leeren Optionen oder Schritte eingeben.");
            items.arr.push_back(JsonValue::make_str(line));
        }
        const JsonValue* v = r.value.find(key);
        if (v && !event_list_representable(v)) r.replaces_invalid = true;
        if (!fresh && after.type == ET_CHOICES && r.value.find("branches")) r.reassigns_choices = true;
        r.value.set(key, std::move(items));
        return true;
    };
    // Operator der Wenn-Befehle (XP 111): gemeinsam für Variable, Gold, Item.
    auto op_feld = [&]() {
        if (!fresh && before.op == after.op) return true;
        if (!unique("op", "cmd_op")) return false;
        if (after.op < 0 || after.op >= 6) return fail("cmd_op", "Einen gültigen Operator auswählen.");
        const JsonValue* old_op = r.value.find("op");
        if (old_op && !old_op->is_null() && event_choice_view(old_op, EVENT_OPS, 6, -1) < 0)
            r.replaces_invalid = true;
        r.value.set("op", JsonValue::make_str(EVENT_OPS[after.op]));
        return true;
    };
    const double unlimited = std::numeric_limits<double>::infinity();
    auto num = [&](const char* key, const char* field, const std::string& was, const std::string& now,
                   bool optional = false, const char* fallback = "0", bool integer = false,
                   bool finite = true, double min = -std::numeric_limits<double>::infinity(),
                   double max = std::numeric_limits<double>::infinity(), bool strict_min = false) {
        return number(key, field, was, now, optional, fallback, integer, finite, min, max, strict_min);
    };
    auto name = [&](const char* key = "name", bool required = true) {
        return text(key, "cmd_name", before.name, after.name, false, false, required);
    };
    // Z5: cancel von choices (XP "When Cancel"). Leer löscht den Schlüssel
    // (XP-Default: nicht erlaubt), "aus" sperrt explizit, "verzweigung"
    // aktiviert den Abbruch-Zweig, eine Zahl ist die Optionsnummer
    // (0-basiert, gegen die (ggf. gerade geänderte) Optionsliste geprüft).
    auto cancel_feld = [&]() {
        // Unverändertes Feld nie anfassen: beim Typwechsel (fresh) steht
        // dort noch der alte Typ-Wert - der wird nicht still zu cancel.
        if (before.val1 == after.val1) return true;
        if (!unique("cancel", "cmd_val1")) return false;
        const std::string v = database_trim(after.val1);
        if (v.empty()) { event_erase(r.value, "cancel"); return true; }
        if (v == "aus" || v == "verzweigung") {
            const JsonValue* alt = r.value.find("cancel");
            if (alt && !alt->is_null() && !alt->is_str() && !alt->is_num() &&
                alt->type != JsonValue::Type::Bool) r.replaces_invalid = true;
            r.value.set("cancel", JsonValue::make_str(v));
            return true;
        }
        JsonValue num;
        const JsonValue* opts = r.value.find("options");
        const long long grenze = (opts && opts->is_arr())
            ? static_cast<long long>(opts->arr.size()) : 0;
        if (!event_number_parse(v, true, true, num) || num.number() < 0 ||
            num.number() >= static_cast<double>(grenze)) {
            return fail("cmd_val1",
                        grenze > 0
                            ? "Bitte 'aus', 'verzweigung' oder eine Optionsnummer von 0 bis N-1 eingeben (N = Anzahl Optionen)."
                            : "Bitte 'aus', 'verzweigung' oder eine Optionsnummer eingeben; es gibt noch keine Optionen.");
        }
        r.value.set("cancel", std::move(num));
        return true;
    };
    auto v1 = [&](const char* key, bool optional = false, const char* fallback = "0", bool integer = false,
                  bool finite = true, double min = -std::numeric_limits<double>::infinity(),
                  double max = std::numeric_limits<double>::infinity(), bool strict_min = false) {
        return num(key, "cmd_val1", before.val1, after.val1, optional, fallback, integer, finite, min, max, strict_min);
    };
    auto v2 = [&](const char* key, bool optional = false, const char* fallback = "0", bool integer = false,
                  bool finite = true, double min = -std::numeric_limits<double>::infinity(),
                  double max = std::numeric_limits<double>::infinity(), bool strict_min = false) {
        return num(key, "cmd_val2", before.val2, after.val2, optional, fallback, integer, finite, min, max, strict_min);
    };
    bool ok = true;
    switch (after.type) {
        case ET_TEXT:
            ok = list("lines", "cmd_text", before.text, after.text, false); break;
        case ET_CHOICES:
            ok = list("options", "cmd_text", before.text, after.text, true);
            if (ok && fresh) {
                JsonValue branches = JsonValue::make_arr();
                for (size_t i = 0; i < r.value.find("options")->arr.size(); ++i)
                    branches.arr.push_back(JsonValue::make_arr());
                r.value.set("branches", std::move(branches));
            }
            if (ok) ok = cancel_feld();
            break;
        case ET_LOG: ok = text("text", "cmd_text", before.text, after.text, false, true, false); break;
        case ET_WAIT: ok = v1("seconds", false, "1", false, true, 0, unlimited); break;
        case ET_SWITCH: case ET_IF_SWITCH:
            ok = name() && boolean("value", "cmd_flag", before.flag, after.flag); break;
        case ET_TOGGLE_SWITCH: case ET_SCRIPT: ok = name(); break;
        case ET_SELF_SWITCH: case ET_IF_SELF_SWITCH: {
            if (fresh || before.name != after.name) {
                std::string letter = after.name;
                if (letter.size() == 1 && letter[0] >= 'a' && letter[0] <= 'd') letter[0] -= 'a' - 'A';
                if (letter.size() != 1 || letter[0] < 'A' || letter[0] > 'D')
                    ok = fail("cmd_name", "Bitte Self-Switch A bis D eingeben.");
                else ok = text("letter", "cmd_name", before.name, letter, false, false, true);
            }
            if (ok) ok = boolean("value", "cmd_flag", before.flag, after.flag);
            break;
        }
        case ET_VARIABLE:
            ok = name() && v1("value", true) && v2("add", true); break;
        case ET_IF_VARIABLE:
            ok = name() && op_feld() && v1("value");
            break;
        case ET_TELEPORT:
            ok = v1("x", false, "0", false, true, -1000000, 1000000) &&
                 v2("z", false, "0", false, true, -1000000, 1000000) &&
                 num("yaw", "cmd_val3", before.val3, after.val3, true, "3.14159265", false, true, -unlimited, unlimited);
            break;
        case ET_MOVE_PLAYER:
            ok = v1("x", false, "0", false, true, -1000000, 1000000) &&
                 v2("z", false, "0", false, true, -1000000, 1000000);
            if (ok && fresh) r.value.set("relative", JsonValue::make_bool(true));
            break;
        case ET_HEAL: ok = v1("amount", true, "0", true, false); break;
        case ET_WEATHER: ok = name() && v1("intensity", false, "1"); break;
        case ET_SOUND: ok = name() && v1("volume", false, "1"); break;
        case ET_EFFECT: {
            ok = name() && v1("height", false, "1");
            if (ok && (fresh || before.val2 != after.val2)) {
                if (!unique("color", "cmd_val2")) { ok = false; break; }
                std::string s = database_trim(after.val2);
                if (fresh && s.empty()) s = "FFD866";
                if (!s.empty() && s[0] == '#') s.erase(0, 1);
                else if (s.size() >= 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s.erase(0, 2);
                unsigned value = 0;
                if (s.empty() || s.size() > 6) { ok = fail("cmd_val2", "Farbe als bis zu sechs Hex-Ziffern eingeben."); break; }
                for (char c : s) {
                    unsigned d;
                    if (c >= '0' && c <= '9') d = static_cast<unsigned>(c - '0');
                    else if (c >= 'a' && c <= 'f') d = static_cast<unsigned>(c - 'a' + 10);
                    else if (c >= 'A' && c <= 'F') d = static_cast<unsigned>(c - 'A' + 10);
                    else { ok = fail("cmd_val2", "Farbe als bis zu sechs Hex-Ziffern eingeben."); break; }
                    value = value * 16 + d;
                }
                if (ok) {
                    const JsonValue* prev = r.value.find("color");
                    if (prev && !prev->is_null() && (!prev->is_num() || !std::isfinite(prev->number()) ||
                        prev->number() < 0 || prev->number() > 0xffffff || std::floor(prev->number()) != prev->number()))
                        r.replaces_invalid = true;
                    r.value.set("color", JsonValue::make_integer(std::to_string(value)));
                }
            }
            break;
        }
        case ET_ITEM: {
            ok = name();
            if (ok && (fresh || before.val2 != after.val2)) {
                if (!unique("count", "cmd_val2") || !unique("take", "cmd_val2")) { ok = false; break; }
                JsonValue amount;
                const std::string s = fresh && after.val2.empty() ? "1" : after.val2;
                if (!event_number_parse(s, true, false, amount)) { ok = fail("cmd_val2", "Die Menge muss eine Ganzzahl sein."); break; }
                const JsonValue* take = r.value.find("take");
                const JsonValue* count = r.value.find("count");
                if ((take && !take->is_null() && !take->is_num()) || (count && !count->is_null() && !count->is_num()))
                    r.replaces_invalid = true;
                std::string token = amount.integer_text();
                if (!token.empty() && token[0] == '-') {
                    token.erase(0, 1);
                    r.value.set("take", JsonValue::make_integer(token));
                } else {
                    if (take) { r.replaces_invalid = true; event_erase(r.value, "take"); }
                    r.value.set("count", std::move(amount));
                }
            }
            break;
        }
        case ET_GOLD: ok = v1("amount", false, "0", true, false); break;
        case ET_SAVE_GAME: case ET_LOAD_GAME: ok = v1("slot", false, "1", true, true, 1, 3); break;
        case ET_FADE: {
            if (fresh || before.name != after.name) {
                std::string direction = after.name.empty() && fresh ? "aus" : after.name;
                std::transform(direction.begin(), direction.end(), direction.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                if (direction != "aus" && direction != "ein") ok = fail("cmd_name", "Richtung muss 'aus' oder 'ein' sein.");
                else ok = text("direction", "cmd_name", before.name, direction, false, false, true);
            }
            if (ok) ok = v1("seconds", false, "0.5", false, true, 0, unlimited);
            break;
        }
        case ET_MOVE_ROUTE:
            ok = name("target") && list("steps", "cmd_text", before.text, after.text, true) &&
                 v1("speed", true, "0", false, true, 0, unlimited, true) &&
                 bool_text("wait", "cmd_val2", before.val2, after.val2) &&
                 text("code", "cmd_val3", before.val3, after.val3, true, false, false) &&
                 boolean("repeat", "cmd_flag", before.flag, after.flag);
            break;
        case ET_INPUT_NUMBER: {
            ok = name() && v1("digits", true, "1", true, true, 1, 6) &&
                 v2("initial", true, "0", true, true, 0, 999999) &&
                 boolean("cancel", "cmd_flag", before.flag, after.flag);
            if (ok && (fresh || before.val1 != after.val1 || before.val2 != after.val2)) {
                // Der Ruby-Interpreter klemmt initial auf 10^digits-1. Neue
                // Eingaben dürfen dadurch nicht still einen anderen Wert zeigen.
                auto valid_whole = [&](const char* key, int fallback, int lo, int hi, int& out) {
                    const JsonValue* value = r.value.find(key);
                    if (!value || value->is_null()) { out = fallback; return true; }
                    if (!value->is_num() || !std::isfinite(value->number()) ||
                        value->number() < lo || value->number() > hi || std::floor(value->number()) != value->number())
                        return false;
                    out = static_cast<int>(value->number());
                    return true;
                };
                int digits = 1, initial = 0;
                if (!valid_whole("digits", 1, 1, 6, digits))
                    ok = fail("cmd_val1", "Bitte zuerst eine gültige Ziffernzahl von 1 bis 6 eingeben.");
                else if (!valid_whole("initial", 0, 0, 999999, initial))
                    ok = fail("cmd_val2", "Bitte zuerst einen gültigen nichtnegativen Startwert eingeben.");
                else {
                    int limit = 1;
                    for (int i = 0; i < digits; ++i) limit *= 10;
                    if (initial >= limit)
                        ok = fail("cmd_val2", "Der Startwert passt nicht zur Ziffernzahl und würde im Spiel geklemmt.");
                }
            }
            break;
        }
        case ET_BATTLE:
            ok = name() && bool_text("can_lose", "cmd_val2", before.val2, after.val2) &&
                 boolean("can_escape", "cmd_flag", before.flag, after.flag);
            break;
        // Etappe 20c: Ablauf-Steuerung. loop/break/exit_event/erase_event
        // tragen keine Felder (der Schleifenkörper ist ein eigener Zweig),
        // marke/ziel der Sprünge sind Namen, Tasteneingabe eine Tastenliste.
        case ET_LOOP: case ET_BREAK: case ET_EXIT_EVENT: case ET_ERASE_EVENT:
            break;
        case ET_LABEL: case ET_JUMP: case ET_COMMON_EVENT:
            ok = name();
            break;
        // Etappe 20d: weitere Bedingungsquellen (XP 111) und die Sperr-
        // Befehle für Speichern/Menü/Zufallskämpfe (XP 134/135/136).
        case ET_IF_GOLD:
            ok = op_feld() && v1("value", true, "0");
            break;
        case ET_IF_ITEM:
            ok = name() && op_feld() && v1("value", true, "1", true, true, 0, unlimited);
            break;
        case ET_IF_SCRIPT:
            ok = text("code", "cmd_text", before.text, after.text, false, true, true);
            break;
        case ET_SAVE_ACCESS: case ET_MENU_ACCESS: case ET_ENCOUNTER_ACCESS:
            ok = boolean("allowed", "cmd_flag", before.flag, after.flag);
            break;
        case ET_BUTTON_INPUT:
            ok = name() && list("keys", "cmd_text", before.text, after.text, true);
            break;
        case ET_TEXT_OPTIONS: {
            if (fresh || before.name != after.name) {
                std::string position = after.name.empty() && fresh ? "unten" : after.name;
                std::transform(position.begin(), position.end(), position.begin(),
                               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                if (position != "oben" && position != "mitte" && position != "unten")
                    ok = fail("cmd_name", "Position muss 'oben', 'mitte' oder 'unten' sein.");
                else ok = text("position", "cmd_name", before.name, position, false, false, true);
            }
            if (ok) ok = boolean("frame", "cmd_flag", before.flag, after.flag);
            if (ok) ok = bool_text("transparent", "cmd_val2", before.val2, after.val2);
            break;
        }
        default: return event_failure(original, "cmd_type", "Unbekannter Befehlstyp.");
    }
    if (!ok) return r;
    if (fresh && (after.type == ET_IF_SWITCH || after.type == ET_IF_VARIABLE ||
                  after.type == ET_IF_SELF_SWITCH || after.type == ET_IF_GOLD ||
                  after.type == ET_IF_ITEM || after.type == ET_IF_SCRIPT)) {
        r.value.set("then", JsonValue::make_arr());
        r.value.set("else", JsonValue::make_arr());
    }
    // Etappe 20c: eine neue Schleife bekommt sofort einen (leeren) Körper,
    // damit der Baum einen Einfügepunkt hat.
    if (fresh && after.type == ET_LOOP) r.value.set("commands", JsonValue::make_arr());
    r.changed = !existing || json_write(r.value) != json_write(original);
    return r;
}

enum class EventCommandAction { None, Update, Insert };
struct EventBatch {
    bool event_fields = false, page_fields = false;
    EventForm event_before, event_after;
    EventPageForm page_before, page_after;
    size_t page = 0;
    EventCommandAction command = EventCommandAction::None;
    EventCommandForm command_before, command_after;
    EventCommandPath path;
    size_t command_index = 0;
};

inline EventEdit event_batch_prepare(const JsonValue& old, const EventBatch& batch) {
    EventEdit result; result.value = old;
    auto apply = [&](EventEdit part) {
        if (!part.ok) { result = event_failure(old, part.field, part.error); return false; }
        event_merge_warnings(result, part);
        result.value = std::move(part.value);
        return true;
    };
    if (batch.event_fields && !apply(event_form_prepare(result.value, batch.event_before, batch.event_after))) return result;
    if (batch.page_fields && !apply(event_page_prepare(result.value, batch.page, batch.page_before, batch.page_after))) return result;
    if (batch.command != EventCommandAction::None) {
        std::string error;
        if (batch.command == EventCommandAction::Insert) {
            EventEdit cmd = event_command_prepare(nullptr, batch.command_before, batch.command_after);
            if (!cmd.ok) return event_failure(old, cmd.field, cmd.error);
            JsonValue* list = event_command_list_mutable(result.value, batch.page, batch.path, true, error);
            if (!list) return event_failure(old, "commands", error);
            list->arr.push_back(std::move(cmd.value));
        } else {
            JsonValue* list = event_command_list_mutable(result.value, batch.page, batch.path, false, error);
            if (!list || batch.command_index >= list->arr.size())
                return event_failure(old, "commands", error.empty() ? "Der Befehl ist nicht mehr verfügbar." : error);
            EventEdit cmd = event_command_prepare(&list->arr[batch.command_index], batch.command_before, batch.command_after);
            if (!cmd.ok) return event_failure(old, cmd.field, cmd.error);
            event_merge_warnings(result, cmd);
            list->arr[batch.command_index] = std::move(cmd.value);
        }
    }
    result.changed = json_write(result.value) != json_write(old);
    return result;
}

inline EventEdit event_command_remove_or_move(const JsonValue& old, size_t page, const EventCommandPath& path,
                                               size_t index, int delta) {
    EventEdit r; r.value = old;
    std::string error;
    JsonValue* list = event_command_list_mutable(r.value, page, path, false, error);
    if (!list || index >= list->arr.size()) return event_failure(old, "commands", error.empty() ? "Befehl nicht gefunden." : error);
    if (delta == 0) list->arr.erase(list->arr.begin() + static_cast<std::ptrdiff_t>(index));
    else {
        if ((delta < 0 && index == 0) || (delta > 0 && index + 1 >= list->arr.size())) return r;
        const size_t to = delta < 0 ? index - 1 : index + 1;
        std::swap(list->arr[index], list->arr[to]);
    }
    r.changed = true;
    return r;
}

// Die Laufzeit verwendet Namen für Spielstände und Self-Switches; NPCs
// besitzen intern ebenfalls ein Event. Auch bei kaputten doppelten Schlüsseln
// alle möglichen Namen beachten, statt nur den ersten Treffer anzusehen.
inline std::unordered_set<std::string> event_reserved_names(const JsonValue& root, int skip_event = -1) {
    std::unordered_set<std::string> names;
    if (!root.is_obj()) return names;
    for (const auto& collection : root.obj) {
        const bool events = collection.first == "events";
        if ((!events && collection.first != "npcs") || !collection.second.is_arr()) continue;
        const char* fallback = events ? "Event" : "NPC";
        for (size_t i = 0; i < collection.second.arr.size(); ++i) {
            if (events && skip_event >= 0 && i == static_cast<size_t>(skip_event)) continue;
            const JsonValue& item = collection.second.arr[i];
            if (!item.is_obj()) continue;
            bool found = false;
            for (const auto& kv : item.obj) if (kv.first == "name") {
                found = true;
                const JsonValue& n = kv.second;
                if (n.is_str()) names.insert(n.str);
                else if (n.is_null() || (n.type == JsonValue::Type::Bool && !n.b)) names.insert(fallback);
                else if (n.type == JsonValue::Type::Bool) names.insert("true");
                else if (n.is_num() && (std::isfinite(n.number()) || (n.is_int && !n.integer_text().empty())))
                    names.insert(database_number_text(&n));
            }
            if (!found) names.insert(fallback);
        }
    }
    return names;
}

inline bool event_name_used(const JsonValue& root, const std::string& name, int skip_event = -1) {
    return event_reserved_names(root, skip_event).count(name) != 0;
}

inline std::string event_available_name(const JsonValue& root, const std::string& base) {
    const auto names = event_reserved_names(root);
    if (!names.count(base)) return base;
    for (size_t suffix = 2;; ++suffix) {
        const std::string name = base + " " + std::to_string(suffix);
        if (!names.count(name)) return name;
    }
}

// Auch doppelte ID-Schlüssel berücksichtigen: Ruby liest bei Duplikaten nicht
// unbedingt denselben Treffer wie JsonValue::find().
inline int event_next_id(const JsonValue& root) {
    if (!root.is_obj() || !event_unique(root, "events")) return 0;
    const JsonValue* events = root.find("events");
    if (events && !events->is_arr()) return 0;
    std::unordered_set<int> used;
    if (events) for (const JsonValue& e : events->arr) {
        if (!e.is_obj()) continue;
        for (const auto& kv : e.obj) {
            if (kv.first != "id" || !kv.second.is_num()) continue;
            const double n = kv.second.number();
            // Keine Konvertierung unendlicher/grosser oder gebrochener IDs.
            if (std::isfinite(n) && n >= 1 && n < std::numeric_limits<int>::max() && std::floor(n) == n)
                used.insert(static_cast<int>(n));
        }
    }
    for (int candidate = 1; candidate < std::numeric_limits<int>::max(); ++candidate)
        if (!used.count(candidate)) return candidate;
    return 0;
}

inline EventEdit event_duplicate_prepare(const JsonValue& old, int new_id, const JsonValue* root = nullptr) {
    if (!old.is_obj()) return event_failure(old, "event", "Ein ungültiger Event-Eintrag kann nicht dupliziert werden.");
    if (new_id <= 0) return event_failure(old, "id", "Für die Kopie ist keine gültige Event-ID verfügbar.");
    for (const char* key : {"id", "name", "x", "z"})
        if (!event_unique(old, key)) return event_failure(old, key, "Ein für die Kopie benötigter Schlüssel ist mehrfach vorhanden.");
    const JsonValue* n = old.find("name");
    if (n && !n->is_null() && !n->is_str()) return event_failure(old, "name", "Der Name ist kein Text; die Kopie würde Daten verlieren.");
    EventEdit r; r.value = old;
    r.value.set("id", JsonValue::make_integer(std::to_string(new_id)));
    const std::string base = (n && n->is_str() ? n->str : "Event") + " (Kopie)";
    r.value.set("name", JsonValue::make_str(root ? event_available_name(*root, base) : base));
    for (const char* key : {"x", "z"}) {
        JsonValue* value = r.value.find(key);
        if (!value || !value->is_num()) { r.unshifted_coordinate = true; continue; }
        JsonValue original = *value;
        value->increment(); // Integer-Tokens auch jenseits von double exakt
        if (!editor_json_serializable(*value) || json_write(*value) == json_write(original)) {
            *value = std::move(original);
            r.unshifted_coordinate = true;
        }
    }
    r.changed = true;
    return r;
}

} // namespace rune::editor
