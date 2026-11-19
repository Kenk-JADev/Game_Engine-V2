#pragma once
// ============================================================================
//  F2-Block-Zonen: stabile Zeilenmarken erhalten Zusatzdaten und die exakten
//  unberührten Koordinaten auch bei Bearbeitung, Umordnung und Einfügung.
//  Vorbereitung ist rein portabel und verändert die Karte noch nicht.
// ============================================================================

#include "editor/json.h"
#include "editor/placement_form.h" // Zeilen und endliche Eingabezahlen

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

namespace rune::editor {

struct BlockedEdit {
    bool ok = true;
    bool changed = false;
    bool replaces_invalid_list = false;
    size_t removed = 0;
    size_t replaces_invalid_entries = 0;
    std::string error;
    JsonValue value = JsonValue::make_arr();
};

inline std::string blocked_trim(const std::string& text) {
    const size_t first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    return text.substr(first, text.find_last_not_of(" \t\r\n") - first + 1);
}

// Fragezeichen repräsentiert fehlende oder nichtnumerische alte Felder: Sie
// bleiben erhalten, bis der Benutzer dieses Feld explizit ersetzt.
inline std::string blocked_field_text(const JsonValue* v) {
    if (!v || !v->is_num()) return "?";
    if ((!v->is_int || v->integer_text().empty()) && !std::isfinite(v->number())) return "?";
    std::string text = json_write(*v);
    if (!text.empty() && text.back() == '\n') text.pop_back();
    return text;
}

// #1, #2, ... sind Sitzungsmarken (keine neue JSON-Eigenschaft). Vorhandene
// Marken beim Bearbeiten/Umordnen beibehalten; neue Zeilen ohne Marke eingeben.
inline std::string blocked_form_read(const JsonValue* old) {
    if (!old || old->is_null()) return {};
    if (!old->is_arr()) return "[ungültige Block-Zonen-Liste]";
    std::string text;
    for (size_t i = 0; i < old->arr.size(); ++i) {
        if (i) text += "\r\n";
        text += "#" + std::to_string(i + 1) + ": ";
        const JsonValue& zone = old->arr[i];
        if (!zone.is_obj()) {
            text += "[ungültiger Eintrag]";
            continue;
        }
        for (const char* key : {"x", "z", "w", "h"}) {
            if (key[0] != 'x') text += "; ";
            text += blocked_field_text(zone.find(key));
        }
    }
    return text;
}

// Bei Semikolon-Trennung dürfen einzelne Werte Dezimalkommas enthalten;
// die ursprüngliche Komma-Trennung bleibt für Punktzahlen möglich.
inline std::vector<std::string> blocked_tokens(const std::string& line) {
    const char sep = line.find(';') != std::string::npos ? ';' : ',';
    std::vector<std::string> tokens;
    size_t start = 0;
    while (true) {
        const size_t end = line.find(sep, start);
        tokens.push_back(blocked_trim(line.substr(start, end == std::string::npos ? end : end - start)));
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return tokens;
}

inline BlockedEdit blocked_form_prepare(const JsonValue* old, const std::string& before,
                                        const std::string& after) {
    BlockedEdit edit;
    if (before == after) return edit;
    const bool old_array = old && old->is_arr();
    const size_t count = old_array ? old->arr.size() : 0;
    std::vector<bool> used(count, false);
    auto fail = [&edit](size_t line, const std::string& message) {
        edit.ok = false;
        edit.error = "Block-Zonen, Zeile " + std::to_string(line) + ": " + message;
    };
    const auto lines = placement_lines(after);
    for (size_t line = 0; line < lines.size(); ++line) {
        std::string body = blocked_trim(lines[line]);
        if (body.empty()) continue;
        size_t index = count; // count bedeutet: neue, nicht markierte Zeile
        if (body[0] == '#') {
            const size_t colon = body.find(':');
            if (colon == std::string::npos) {
                fail(line + 1, "Nach der Nummer fehlt ':'."); return edit;
            }
            const std::string number = blocked_trim(body.substr(1, colon - 1));
            size_t id = 0;
            const auto parsed = std::from_chars(number.data(), number.data() + number.size(), id);
            if (number.empty() || parsed.ec != std::errc{} || parsed.ptr != number.data() + number.size() ||
                id == 0 || id > count) {
                fail(line + 1, "Unbekannte Nummer. Vorhandene Nummern beibehalten; neue Zeilen ohne #.");
                return edit;
            }
            index = id - 1;
            if (used[index]) {
                fail(line + 1, "Diese Nummer wird mehrfach verwendet."); return edit;
            }
            used[index] = true;
            body = blocked_trim(body.substr(colon + 1));
        }

        const JsonValue* previous = index < count ? &old->arr[index] : nullptr;
        if (previous && !previous->is_obj() && body == "[ungültiger Eintrag]") {
            edit.value.arr.push_back(*previous);
            continue;
        }
        const auto tokens = blocked_tokens(body);
        if (tokens.size() != 4) {
            fail(line + 1, "Vier Werte x; z; w; h eingeben (Breite und Höhe > 0)."); return edit;
        }
        JsonValue zone = previous && previous->is_obj() ? *previous : JsonValue::make_obj();
        if (previous && !previous->is_obj()) ++edit.replaces_invalid_entries;
        static const char* const keys[] = {"x", "z", "w", "h"};
        for (size_t i = 0; i < 4; ++i) {
            const JsonValue* old_field = previous && previous->is_obj() ? previous->find(keys[i]) : nullptr;
            if (old_field && tokens[i] == blocked_field_text(old_field)) continue;
            // Auch ein fehlendes oder kaputtes Feld wird durch '?' ausdrücklich
            // beibehalten; neue Zonen müssen dagegen alle vier Werte besitzen.
            if (previous && previous->is_obj() && tokens[i] == "?" &&
                blocked_field_text(old_field) == "?") continue;
            double number = 0.0;
            if (!placement_number(tokens[i], number) || (i >= 2 && number <= 0.0)) {
                fail(line + 1, i >= 2 ? "Breite und Höhe müssen endliche Zahlen > 0 sein."
                                      : "X und Z müssen endliche Zahlen sein.");
                return edit;
            }
            // Umformatierte, aber numerisch gleiche Felder bleiben samt Typ und
            // ursprünglicher Präzision stehen. Vorzeichen von 0 unterscheiden.
            if (old_field && old_field->is_num() && std::isfinite(old_field->number()) &&
                old_field->number() == number &&
                (number != 0.0 || std::signbit(old_field->number()) == std::signbit(number))) continue;
            zone.set(keys[i], JsonValue::make_num(number, false));
        }
        edit.value.arr.push_back(std::move(zone));
    }
    edit.removed = static_cast<size_t>(std::count(used.begin(), used.end(), false));
    edit.replaces_invalid_list = old && !old->is_null() && !old->is_arr();
    if (old_array) edit.changed = json_write(*old) != json_write(edit.value);
    else edit.changed = edit.replaces_invalid_list || !edit.value.arr.empty();
    return edit;
}

} // namespace rune::editor
