#pragma once
// Verlustfreie Vorbereitung für den RGSS-/Ruby-Projektskripteditor.
#include "editor/json.h"
#include "editor/json_validation.h"
#include "editor/map_interaction.h" // eindeutige JSON-Felder

#include <algorithm>
#include <exception>
#include <string>
#include <utility>

namespace rune::editor {

enum class ScriptDocumentStatus { Ok, InvalidJson, InvalidRoot, InvalidScripts, InvalidEncoding };

inline ScriptDocumentStatus script_document_parse(const std::string& bytes, JsonValue& out) {
    if (!editor_json_raw_strings_valid(bytes)) return ScriptDocumentStatus::InvalidJson;
    JsonValue root;
    try { root = json_parse(bytes); }
    catch (const std::exception&) { return ScriptDocumentStatus::InvalidJson; }
    if (!root.is_arr() && !root.is_obj()) return ScriptDocumentStatus::InvalidRoot;
    if (!editor_json_utf8(root) || !editor_json_serializable(root)) return ScriptDocumentStatus::InvalidEncoding;
    if (root.is_obj()) {
        if (!map_field_unique(root, "scripts")) return ScriptDocumentStatus::InvalidScripts;
        const JsonValue* list = root.find("scripts");
        if (list && !list->is_arr()) return ScriptDocumentStatus::InvalidScripts;
    }
    out = std::move(root);
    return ScriptDocumentStatus::Ok;
}

inline const JsonValue* script_document_list(const JsonValue& root) {
    if (root.is_arr()) return &root;
    if (!root.is_obj() || !map_field_unique(root, "scripts")) return nullptr;
    const JsonValue* list = root.find("scripts");
    return list && list->is_arr() ? list : nullptr;
}

inline JsonValue* script_document_list(JsonValue& root, bool create = false) {
    if (root.is_arr()) return &root;
    if (!root.is_obj() || !map_field_unique(root, "scripts")) return nullptr;
    JsonValue* list = root.find("scripts");
    if (!list && create) list = &root.set("scripts", JsonValue::make_arr());
    return list && list->is_arr() ? list : nullptr;
}

// Etappe 36 (RPG Maker XP „Einfügen“): neuer Eintrag ÜBER dem markierten;
// ohne gültige Auswahl über „089 - Main“ (xp_default "089"), sonst am Ende.
inline std::size_t script_insert_position(const JsonValue& list, int selected) {
    if (!list.is_arr()) return 0;
    if (selected >= 0 && static_cast<std::size_t>(selected) < list.arr.size())
        return static_cast<std::size_t>(selected);
    for (std::size_t i = 0; i < list.arr.size(); ++i) {
        const JsonValue& item = list.arr[i];
        const JsonValue* xp = item.is_obj() ? item.find("xp_default") : nullptr;
        if (xp && xp->is_str() && xp->str == "089") return i;
    }
    return list.arr.size();
}

// Etappe 36: XP-artige fortlaufende Nummer in der Scriptliste (001, 002, ...).
inline std::string script_index_label(std::size_t index) {
    std::string n = std::to_string(index + 1);
    if (n.size() < 3) n.insert(n.begin(), 3 - n.size(), '0');
    return n;
}

// Etappe 36: Suchen im Code wie XP (Strg+F, F3 = weiter). Ohne Beachtung der
// Groß-/Kleinschreibung (ASCII), ab `from`, mit Umlauf an den Anfang.
// Rückgabe: Fundstelle oder std::wstring::npos.
inline std::size_t script_find_text(const std::wstring& text, const std::wstring& needle, std::size_t from) {
    if (needle.empty() || needle.size() > text.size()) return std::wstring::npos;
    auto lower = [](wchar_t c) { return (c >= L'A' && c <= L'Z') ? static_cast<wchar_t>(c - L'A' + L'a') : c; };
    auto match_at = [&](std::size_t i) {
        for (std::size_t k = 0; k < needle.size(); ++k)
            if (lower(text[i + k]) != lower(needle[k])) return false;
        return true;
    };
    const std::size_t last = text.size() - needle.size();
    if (from > last + 1) from = 0;
    for (std::size_t i = from; i <= last; ++i) if (match_at(i)) return i;
    for (std::size_t i = 0; i < from && i <= last; ++i) if (match_at(i)) return i;
    return std::wstring::npos;
}

// Etappe 36: Ersetzen ohne Umlauf (Strg+H). Die Suche bleibt case-insensitive;
// ein Ersetzungstext, der den Suchtext erneut enthält, wird nicht endlos ersetzt.
inline std::size_t script_replace_all(std::wstring& text, const std::wstring& needle,
                                      const std::wstring& replacement) {
    if (needle.empty()) return 0;
    std::size_t count = 0, from = 0;
    for (;;) {
        const std::size_t pos = script_find_text(text, needle, from);
        if (pos == std::wstring::npos || (from > 0 && pos < from)) break;
        text.replace(pos, needle.size(), replacement);
        from = pos + replacement.size();
        ++count;
    }
    return count;
}

// Win32-Edit verwendet CRLF, Ruby-Code kann LF/CRLF enthalten. Bei einem
// unveränderten Feld bleiben die Originalbytes erhalten; nur neue Eingaben
// erhalten LF als einheitliche Zeilentrennung.
inline std::string script_normalize_lines(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\r' && i + 1 < text.size() && text[i + 1] == '\n') continue;
        out.push_back(text[i]);
    }
    return out;
}

struct ScriptForm {
    std::string name;
    std::string code;
    bool autoload = false;
    bool name_editable = false;
    bool code_editable = false;
    bool autoload_editable = false;
};

inline ScriptForm script_form_read(const JsonValue& item) {
    ScriptForm form;
    if (!item.is_obj()) return form;
    const JsonValue* name = item.find("name");
    const JsonValue* code = item.find("code");
    const JsonValue* autoload = item.find("autoload");
    form.name_editable = map_field_unique(item, "name") && (!name || name->is_str());
    form.code_editable = map_field_unique(item, "code") && (!code || code->is_str());
    form.autoload_editable = map_field_unique(item, "autoload") &&
        (!autoload || autoload->type == JsonValue::Type::Bool);
    if (name && name->is_str()) form.name = name->str;
    if (code && code->is_str()) form.code = code->str;
    if (autoload && autoload->type == JsonValue::Type::Bool) form.autoload = autoload->b;
    return form;
}

struct ScriptEdit {
    bool ok = false;
    bool changed = false;
    JsonValue value;
    std::string error;
};

inline ScriptEdit script_form_prepare(const JsonValue& item, const ScriptForm& before, const ScriptForm& input) {
    ScriptEdit result;
    if (!item.is_obj()) { result.error = "Der Eintrag ist kein Script-Objekt."; return result; }
    const bool name_changed = input.name != before.name;
    const bool code_changed = script_normalize_lines(input.code) != script_normalize_lines(before.code);
    const bool load_changed = input.autoload != before.autoload;
    if ((name_changed && !before.name_editable) || (code_changed && !before.code_editable) ||
        (load_changed && !before.autoload_editable)) {
        result.error = "Ein mehrdeutiges oder geschütztes Feld darf nicht ersetzt werden.";
        return result;
    }
    if ((name_changed || (load_changed && input.autoload)) &&
        (input.name.empty() || !editor_single_line(input.name) || !before.name_editable)) {
        result.error = "Ein Startscript benötigt einen eindeutigen, nicht leeren und lesbaren Namen.";
        return result;
    }
    if (code_changed && (input.code.find('\0') != std::string::npos || !editor_valid_utf8(input.code))) {
        result.error = "Der Ruby-Code enthält ungültiges Unicode oder ein NUL-Zeichen.";
        return result;
    }
    result.value = item;
    if (name_changed) result.value.set("name", JsonValue::make_str(input.name));
    if (code_changed) result.value.set("code", JsonValue::make_str(script_normalize_lines(input.code)));
    if (load_changed) result.value.set("autoload", JsonValue::make_bool(input.autoload));
    result.changed = name_changed || code_changed || load_changed;
    result.ok = true;
    return result;
}

struct ScriptIndent {
    bool changed = false;
    std::size_t begin = 0, end = 0, selection_begin = 0, selection_end = 0;
    std::wstring replacement;
};

// Tab rückt eine Auswahl zeilenweise ein; Shift+Tab rückt maximal zwei
// Leerzeichen oder einen Tab aus. Ein einzelner Vorgang bleibt Win32-Undo.
inline ScriptIndent script_indent(const std::wstring& text, std::size_t start, std::size_t end, bool outdent) {
    ScriptIndent edit;
    start = std::min(start, text.size());
    end = std::min(end, text.size());
    if (start > end) std::swap(start, end);
    if (start == end && !outdent) {
        edit.changed = true; edit.begin = start; edit.end = end; edit.replacement = L"  ";
        edit.selection_begin = edit.selection_end = start + 2;
        return edit;
    }
    const std::size_t previous = start == 0 ? std::wstring::npos : text.rfind(L'\n', start - 1);
    const std::size_t first = previous == std::wstring::npos ? 0 : previous + 1;
    std::size_t last = end;
    if (end > start && end > 0 && text[end - 1] == L'\n') --last;
    const std::size_t next = text.find(L'\n', last);
    const std::size_t stop = next == std::wstring::npos ? text.size() : next + 1;
    edit.begin = first; edit.end = stop;
    std::size_t line = first;
    std::size_t removed_from_first = 0;
    while (line < stop) {
        const std::size_t newline = text.find(L'\n', line);
        const std::size_t line_end = newline == std::wstring::npos || newline >= stop ? stop : newline + 1;
        std::size_t remove = 0;
        if (outdent) {
            if (text[line] == L'\t') remove = 1;
            else while (remove < 2 && line + remove < line_end && text[line + remove] == L' ') ++remove;
            if (line == first) removed_from_first = remove;
        } else edit.replacement += L"  ";
        edit.replacement.append(text, line + remove, line_end - line - remove);
        edit.changed = edit.changed || !outdent || remove != 0;
        line = line_end;
    }
    if (start == end) {
        const std::size_t adjusted = start >= first + removed_from_first ? start - removed_from_first : first;
        edit.selection_begin = edit.selection_end = adjusted;
    } else {
        edit.selection_begin = first;
        edit.selection_end = first + edit.replacement.size();
    }
    return edit;
}

inline bool script_name_used(const JsonValue& root, const std::string& name, std::size_t except) {
    const JsonValue* list = script_document_list(root);
    if (!list) return false;
    for (std::size_t i = 0; i < list->arr.size(); ++i) {
        if (i == except || !list->arr[i].is_obj()) continue;
        // Ein doppeltes name-Feld bleibt nicht editierbar, zählt aber als belegt.
        for (const auto& pair : list->arr[i].obj)
            if (pair.first == "name" && pair.second.is_str() && pair.second.str == name) return true;
    }
    return false;
}

} // namespace rune::editor
