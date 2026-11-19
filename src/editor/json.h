#pragma once
// ============================================================================
//  Minimaler JSON-Parser/-Schreiber fuer den RuneEditor (Etappe 17).
//  Bewusst eigenstaendig statt Fremd-Bibliothek: Der Editor liest und
//  schreibt nur die wohlgeformten Dateien, die das Spiel (Ruby) erzeugt.
//  Werte bleiben typgetreu (Ganzzahl vs. Kommazahl), Objekte behalten ihre
//  Reihenfolge und unbekannte Schluessel — events/npcs ueberleben das
//  Speichern also unangetastet.
// ============================================================================

#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace rune::editor {

class JsonValue {
public:
    enum class Type { Null, Bool, Num, Str, Arr, Obj };

    Type type = Type::Null;
    bool     b = false;
    double   num = 0.0;
    bool     is_int = false;          // Ganzzahl ohne '.'/'e' erkannt?
    std::string str;                 // UTF-8
    std::vector<JsonValue> arr;
    // Objekt als Paarliste: Reihenfolge und Doppelte Schluessel bleiben
    // erhalten (wichtig, damit gespeicherte Karten bis auf die geaenderten
    // Stellen byteaehnlich gleich bleiben).
    std::vector<std::pair<std::string, JsonValue>> obj;

    Type  type_of() const { return type; }
    bool  is_null() const { return type == Type::Null; }
    bool  is_num()  const { return type == Type::Num; }
    bool  is_str()  const { return type == Type::Str; }
    bool  is_arr()  const { return type == Type::Arr; }
    bool  is_obj()  const { return type == Type::Obj; }

    double number(double fallback = 0.0) const { return is_num() ? num : fallback; }
    const std::string& text() const { return str; }

    // Erster Treffer (Doppelte Schluessel sind in unseren Dateien verboten).
    JsonValue* find(const std::string& key) {
        for (auto& kv : obj) {
            if (kv.first == key) return &kv.second;
        }
        return nullptr;
    }
    const JsonValue* find(const std::string& key) const {
        for (const auto& kv : obj) {
            if (kv.first == key) return &kv.second;
        }
        return nullptr;
    }

    // Schluessel setzen: vorhandenen ersetzen, sonst anhaengen.
    JsonValue& set(const std::string& key, JsonValue v) {
        for (auto& kv : obj) {
            if (kv.first == key) { kv.second = std::move(v); return kv.second; }
        }
        obj.emplace_back(key, std::move(v));
        return obj.back().second;
    }

    static JsonValue make_num(double v, bool as_int = false) {
        JsonValue j; j.type = Type::Num; j.num = v; j.is_int = as_int; return j;
    }
    static JsonValue make_str(std::string s) {
        JsonValue j; j.type = Type::Str; j.str = std::move(s); return j;
    }
    static JsonValue make_bool(bool v) {
        JsonValue j; j.type = Type::Bool; j.b = v; return j;
    }
    static JsonValue make_arr() {
        JsonValue j; j.type = Type::Arr; return j;
    }
    static JsonValue make_obj() {
        JsonValue j; j.type = Type::Obj; return j;
    }
};

// Parst JSON-Text (UTF-8). Wirft std::runtime_error mit Position bei Fehlern.
JsonValue json_parse(const std::string& text);

// Schreibt pretty-printed JSON: 2 Leerzeichen Einrueckung, ein Element pro
// Zeile und Zahlen in der kuerzesten exakt runden Form — dasselbe Layout wie
// Ruby JSON.pretty_generate, damit Editor und Spiel beim Speichern nicht
// gegenseitig das Format umwerfen (siehe write_number in json.cpp).
std::string json_write(const JsonValue& v);

} // namespace rune::editor
