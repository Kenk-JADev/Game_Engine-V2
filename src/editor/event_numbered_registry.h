#pragma once
// ============================================================================
//  Etappe 45: Nummerierte Namens-Register für Schalter und Variablen.
//
//  RPG Maker XP wählt Schalter/Variablen über eine NUMMERIERTE Liste (1..999),
//  nicht über Freitext. Dieses Register hält dazu je Nummer einen Namen. Die
//  Engine arbeitet weiter mit Namen (GameState.switch/set_switch), deshalb
//  liefert die Auswahl einen Namen; die Nummer ist die XP-Adresse und wird
//  zusätzlich als `id` im Befehl abgelegt (verlustfrei, von der Engine ignoriert).
//
//  Persistenz: database.json["switches"] / ["variables"] als Objekt
//  { "101": "Tor_Offen", ... } oder als Liste von Namen (Index = Nummer-1).
//  Fehlende Schlüssel => leeres Register; der Editor legt sie beim ersten
//  Benennen an. Unberührte database-Felder bleiben erhalten.
//
//  Ohne Win32, damit Auswahl, Format und Roundtrip kopflos prüfbar sind
//  (tools/event_command_settings_test.cpp).
// ============================================================================

#include "editor/json.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace rune::editor {

constexpr int EC_NUMBERED_MAX = 999;   // XP: 1..999

struct NumberedRegistry {
    // names[i] = Name der Nummer i+1; leer = unbenannt.
    std::vector<std::string> names;

    NumberedRegistry() : names(EC_NUMBERED_MAX) {}

    bool valid(int number) const { return number >= 1 && number <= EC_NUMBERED_MAX; }

    std::string name_of(int number) const {
        if (!valid(number)) return {};
        return names[static_cast<size_t>(number - 1)];
    }

    void set_name(int number, const std::string& name) {
        if (!valid(number)) return;
        names[static_cast<size_t>(number - 1)] = name;
    }

    // Erste Nummer mit diesem Namen, sonst 0.
    int number_of(const std::string& name) const {
        if (name.empty()) return 0;
        for (int i = 0; i < EC_NUMBERED_MAX; ++i)
            if (names[static_cast<size_t>(i)] == name) return i + 1;
        return 0;
    }

    // Beschriftung wie XP: "0101: Tor_Offen" bzw. "0101: (unbenannt)".
    std::string label(int number) const {
        if (!valid(number)) return {};
        std::string n = name_of(number);
        char buf[8];
        std::snprintf(buf, sizeof buf, "%04d", number);
        return std::string(buf) + ": " + (n.empty() ? std::string("(unbenannt)") : n);
    }

    // Aus database.json lesen (Objekt id->Name oder Liste von Namen).
    static NumberedRegistry from_json(const JsonValue* v) {
        NumberedRegistry r;
        if (!v) return r;
        if (v->is_obj()) {
            for (const auto& kv : v->obj) {
                int id = std::atoi(kv.first.c_str());
                if (r.valid(id) && kv.second.is_str()) r.set_name(id, kv.second.str);
            }
        } else if (v->is_arr()) {
            for (size_t i = 0; i < v->arr.size() && i < EC_NUMBERED_MAX; ++i)
                if (v->arr[i].is_str()) r.set_name(static_cast<int>(i) + 1, v->arr[i].str);
        }
        return r;
    }

    // Als Objekt id->Name schreiben (nur benannte Einträge, sortiert).
    JsonValue to_json() const {
        JsonValue o = JsonValue::make_obj();
        for (int i = 1; i <= EC_NUMBERED_MAX; ++i) {
            const std::string& n = names[static_cast<size_t>(i - 1)];
            if (!n.empty()) o.set(std::to_string(i), JsonValue::make_str(n));
        }
        return o;
    }
};

} // namespace rune::editor
