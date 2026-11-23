#pragma once
// ============================================================================
//  Minimaler JSON-Parser/-Schreiber für den RuneEditor (Etappe 17).
//  Bewusst eigenständig statt Fremd-Bibliothek: Der Editor liest und
//  schreibt nur die wohlgeformten Dateien, die das Spiel (Ruby) erzeugt.
//  Werte bleiben typgetreu (Ganzzahl vs. Kommazahl); auch beliebig grosse
//  Ganzzahlen werden unverändert geschrieben. Objekte behalten Reihenfolge
//  und unbekannte Schlüssel — events/npcs überleben das Speichern.
// ============================================================================

#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace rune::editor {

class JsonValue {
    // Für Berechnungen dient double nur als Näherung. Ursprüngliche JSON-
    // Ganzzahlen behalten zusätzlich ihre exakten Dezimalstellen. Der Wert
    // ist privat, damit keine direkte Mutation den Originaltext veralten lässt.
    double num_ = 0.0;
    std::string integer_text_;

public:
    enum class Type { Null, Bool, Num, Str, Arr, Obj };

    Type type = Type::Null;
    bool     b = false;
    bool     is_int = false;          // Ganzzahl ohne '.'/'e' erkannt?
    std::string str;                 // UTF-8
    std::vector<JsonValue> arr;
    // Objekt als Paarliste: Reihenfolge und Doppelte Schlüssel bleiben
    // erhalten (wichtig, damit gespeicherte Karten bis auf die geänderten
    // Stellen byteähnlich gleich bleiben).
    std::vector<std::pair<std::string, JsonValue>> obj;

    Type  type_of() const { return type; }
    bool  is_null() const { return type == Type::Null; }
    bool  is_num()  const { return type == Type::Num; }
    bool  is_str()  const { return type == Type::Str; }
    bool  is_arr()  const { return type == Type::Arr; }
    bool  is_obj()  const { return type == Type::Obj; }

    // Numerischer Zugriff ist eine double-Näherung; für unverändertes
    // Speichern ist bei Ganzzahlen integer_text() massgeblich.
    double number(double fallback = 0.0) const { return is_num() ? num_ : fallback; }
    const std::string& integer_text() const { return integer_text_; }
    const std::string& text() const { return str; }

    // Beim Duplizieren werden Koordinaten um 1 verschoben. Ganze JSON-Zahlen
    // lassen sich dabei auch jenseits von double exakt inkrementieren.
    void increment();

    // Erster Treffer (Doppelte Schlüssel sind in unseren Dateien verboten).
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

    // Schlüssel setzen: vorhandenen ersetzen, sonst anhängen.
    JsonValue& set(const std::string& key, JsonValue v) {
        for (auto& kv : obj) {
            if (kv.first == key) { kv.second = std::move(v); return kv.second; }
        }
        obj.emplace_back(key, std::move(v));
        return obj.back().second;
    }

    static JsonValue make_num(double v, bool as_int = false) {
        JsonValue j; j.type = Type::Num; j.num_ = v; j.is_int = as_int; return j;
    }
    // Akzeptiert nur einen gültigen ganzzahligen JSON-Token, beliebig groß.
    static JsonValue make_integer(std::string decimal);
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

// Schreibt pretty-printed JSON: 2 Leerzeichen Einrückung, ein Element pro
// Zeile und Zahlen in der kürzesten exakt runden Form — dasselbe Layout wie
// Ruby JSON.pretty_generate, damit Editor und Spiel beim Speichern nicht
// gegenseitig das Format umwerfen (siehe write_number in json.cpp).
std::string json_write(const JsonValue& v);

} // namespace rune::editor
