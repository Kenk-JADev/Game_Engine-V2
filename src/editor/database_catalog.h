#pragma once
// ============================================================================
//  Etappe 41: Register-Katalog des Datenbank-Dialogs (F9) wie RPG Maker XP.
//
//  XP hat 13 Register in fester Reihenfolge (Actors, Classes, Skills, Items,
//  Weapons, Armors, Enemies, Troops, States, Animations, Tilesets,
//  Common Events, System). Rune3D bildet sie auf EINE Datei ab:
//  projekte/<name>/data/database.json. Zwölf Register sind Listen, "system"
//  ist - wie in XP - ein einzelner Datensatz.
//
//  Der Katalog ist bewusst ohne Win32: Felder, Auswahlwerte, Vorgaben für
//  neue Einträge und Listenbeschriftungen lassen sich damit kopflos prüfen
//  (tools/json_test.cpp, tools/sync_test.rb).
//
//  Wichtig: `engine` sagt ehrlich, ob das Spiel die Daten heute schon liest.
//  "prepared" heißt: Der Editor pflegt die Daten verlustfrei, die Engine
//  wertet sie noch nicht aus (siehe docs/BACKLOG.md).
// ============================================================================

#include "editor/database_form.h"

#include <string>
#include <vector>

namespace rune::editor {

constexpr int DATABASE_AREA_COUNT = 13;

enum class DatabaseAreaKind { List, Object };
enum class DatabaseEngineState { Wired, Prepared };

struct DatabaseArea {
    const char* key;            // Schlüssel in database.json
    const char* label;          // Registerkarte (UTF-8)
    DatabaseAreaKind kind;
    DatabaseEngineState engine;
    const char* id_prefix;      // neue Einträge: <prefix>_1, <prefix>_2, ...
    bool numeric_id;            // Common Events zählen wie XP numerisch
    const char* hint;           // Kurzbeschreibung unter dem Formular
};

// Reihenfolge wie im XP-Datenbankfenster.
inline const DatabaseArea& database_area(int index) {
    static const DatabaseArea areas[DATABASE_AREA_COUNT] = {
        { "actors", "Helden", DatabaseAreaKind::List, DatabaseEngineState::Wired, "held", false,
          "Startwerte eines Helden. 'class' ist ein Klassenname, 'skills' die Startfertigkeiten." },
        { "classes", "Klassen", DatabaseAreaKind::List, DatabaseEngineState::Prepared, "klasse", false,
          "Klassen mit Kampfposition und erlaubter Ausrüstung. Die Engine wertet sie noch nicht aus." },
        { "skills", "Fertigkeiten", DatabaseAreaKind::List, DatabaseEngineState::Wired, "skill", false,
          "kind: damage/heal/buff, scope: enemy/ally/self. 'state' nennt einen Zustand." },
        { "items", "Items", DatabaseAreaKind::List, DatabaseEngineState::Wired, "item", false,
          "kind: heal/mana wirken, quest/money sind Vorräte. 'start' ist die Startmenge." },
        { "weapons", "Waffen", DatabaseAreaKind::List, DatabaseEngineState::Prepared, "waffe", false,
          "Waffenwerte für eine spätere Ausrüstung. Die Engine wertet sie noch nicht aus." },
        { "armors", "Rüstungen", DatabaseAreaKind::List, DatabaseEngineState::Prepared, "ruestung", false,
          "slot: shield/head/body/accessory. Die Engine wertet sie noch nicht aus." },
        { "enemies", "Gegner", DatabaseAreaKind::List, DatabaseEngineState::Wired, "gegner", false,
          "'color' ist eine RGB-Zahl (0..16777215), 'drop' eine Item-ID." },
        { "troops", "Truppen", DatabaseAreaKind::List, DatabaseEngineState::Wired, "trupp", false,
          "Gegner-IDs komma-getrennt; F2 wählt gespeicherte Truppen für Zufallskämpfe." },
        { "states", "Zustände", DatabaseAreaKind::List, DatabaseEngineState::Wired, "zustand", false,
          "kind: poison/regen oder <wert>_up/<wert>_down (atk/def/spi/agi), 'turns' = Runden." },
        { "animations", "Animationen", DatabaseAreaKind::List, DatabaseEngineState::Prepared, "animation", false,
          "Daten für Effekt-Animationen (XP 207/337). Die Engine zeichnet sie noch nicht." },
        { "tilesets", "Tilesets", DatabaseAreaKind::List, DatabaseEngineState::Prepared, "tileset", false,
          "Benannte Boden-Vorlagen. Karten pflegen ihr Gelände weiter in F7." },
        { "common_events", "Gem. Events", DatabaseAreaKind::List, DatabaseEngineState::Wired, "ereignis", true,
          "trigger: none/autorun/parallel, 'switch' gibt autorun/parallel frei. Befehle als JSON-Liste." },
        { "system", "System", DatabaseAreaKind::Object, DatabaseEngineState::Prepared, "system", false,
          "Ein einzelner Datensatz in database.json. Projektstart und Kartenbaum stehen in system.json." },
    };
    const int safe = index < 0 || index >= DATABASE_AREA_COUNT ? 0 : index;
    return areas[safe];
}

inline int database_area_index(const std::string& key) {
    for (int i = 0; i < DATABASE_AREA_COUNT; ++i)
        if (key == database_area(i).key) return i;
    return -1;
}

// --- Feldbausteine ---------------------------------------------------------

inline DatabaseFieldSpec database_text(const char* key, const char* label) {
    return { key, DatabaseFieldKind::Text, {}, label };
}
inline DatabaseFieldSpec database_int(const char* key, const char* label) {
    return { key, DatabaseFieldKind::Integer, {}, label };
}
inline DatabaseFieldSpec database_decimal(const char* key, const char* label) {
    return { key, DatabaseFieldKind::Decimal, {}, label };
}
inline DatabaseFieldSpec database_ids(const char* key, const char* label) {
    return { key, DatabaseFieldKind::IdList, {}, label };
}
inline DatabaseFieldSpec database_json(const char* key, const char* label) {
    return { key, DatabaseFieldKind::Json, {}, label };
}
inline DatabaseFieldSpec database_choice(const char* key, const char* label,
                                         std::vector<std::string> choices) {
    return { key, DatabaseFieldKind::Choice, std::move(choices), label };
}
// Ein Feld, das in vorhandenen Projekten fehlen darf (alte Datenbestände).
inline DatabaseFieldSpec database_optional(DatabaseFieldSpec spec) {
    spec.optional = true;
    return spec;
}

// Auswahlwerte, die die Engine wirklich versteht (game/engine/battle.rb,
// party.rb, common_events.rb). tools/sync_test.rb gleicht sie damit ab.
inline const std::vector<std::string>& database_item_kinds() {
    static const std::vector<std::string> values{ "heal", "mana", "quest", "money" };
    return values;
}
inline const std::vector<std::string>& database_skill_kinds() {
    static const std::vector<std::string> values{ "damage", "heal", "buff" };
    return values;
}
inline const std::vector<std::string>& database_skill_scopes() {
    static const std::vector<std::string> values{ "enemy", "ally", "self" };
    return values;
}
inline const std::vector<std::string>& database_state_kinds() {
    static const std::vector<std::string> values{
        "poison", "regen", "atk_up", "atk_down", "def_up", "def_down",
        "spi_up", "spi_down", "agi_up", "agi_down"
    };
    return values;
}
inline const std::vector<std::string>& database_common_triggers() {
    static const std::vector<std::string> values{ "none", "autorun", "parallel" };
    return values;
}
inline const std::vector<std::string>& database_positions() {
    static const std::vector<std::string> values{ "front", "middle", "back" };
    return values;
}
inline const std::vector<std::string>& database_armor_slots() {
    static const std::vector<std::string> values{ "shield", "head", "body", "accessory" };
    return values;
}
inline const std::vector<std::string>& database_weapon_kinds() {
    static const std::vector<std::string> values{ "sword", "axe", "spear", "bow", "staff", "dagger" };
    return values;
}
inline const std::vector<std::string>& database_animation_positions() {
    static const std::vector<std::string> values{ "top", "middle", "bottom", "screen" };
    return values;
}

// --- Feldtabellen ----------------------------------------------------------

inline const std::vector<DatabaseFieldSpec>& database_area_fields(int index) {
    static const std::vector<std::vector<DatabaseFieldSpec>> tables = {
        // 0 Helden
        {
            database_text("id", "ID"),
            database_text("name", "Name"),
            database_text("class", "Klasse"),
            database_int("level", "Stufe"),
            database_int("hp", "HP (max)"),
            database_int("mp", "MP (max)"),
            database_int("atk", "ATK"),
            database_int("def", "DEF"),
            database_int("spi", "SPI"),
            database_int("agi", "AGI"),
            database_int("exp", "Exp (gesamt)"),
            database_ids("skills", "Fertigkeiten (IDs, komma-getrennt)"),
        },
        // 1 Klassen
        {
            database_text("id", "ID"),
            database_text("name", "Name"),
            database_choice("position", "Kampfposition", database_positions()),
            database_ids("weapons", "Erlaubte Waffen (IDs)"),
            database_ids("armors", "Erlaubte Rüstungen (IDs)"),
            database_text("desc", "Beschreibung"),
        },
        // 2 Fertigkeiten
        {
            database_text("id", "ID"),
            database_text("name", "Name"),
            database_choice("kind", "Art", database_skill_kinds()),
            database_choice("scope", "Ziel", database_skill_scopes()),
            database_int("mp", "MP-Kosten"),
            database_int("power", "Stärke"),
            database_text("state", "Zustand (ID, leer = keiner)"),
            database_decimal("state_rate", "Zustandschance (0..1)"),
            database_int("turns", "Zustandsdauer (Runden)"),
            database_text("desc", "Beschreibung"),
        },
        // 3 Items
        {
            database_text("id", "ID"),
            database_text("name", "Name"),
            database_choice("kind", "Art", database_item_kinds()),
            database_int("power", "Stärke"),
            database_int("price", "Preis"),
            database_int("start", "Startmenge"),
            database_text("desc", "Beschreibung"),
        },
        // 4 Waffen
        {
            database_text("id", "ID"),
            database_text("name", "Name"),
            database_choice("kind", "Art", database_weapon_kinds()),
            database_int("atk", "ATK"),
            database_int("def", "DEF"),
            database_int("spi", "SPI"),
            database_int("agi", "AGI"),
            database_int("price", "Preis"),
            database_text("desc", "Beschreibung"),
        },
        // 5 Rüstungen
        {
            database_text("id", "ID"),
            database_text("name", "Name"),
            database_choice("slot", "Platz", database_armor_slots()),
            database_int("atk", "ATK"),
            database_int("def", "DEF"),
            database_int("spi", "SPI"),
            database_int("agi", "AGI"),
            database_int("price", "Preis"),
            database_text("desc", "Beschreibung"),
        },
        // 6 Gegner
        {
            database_text("id", "ID"),
            database_text("name", "Name"),
            database_int("hp", "HP"),
            database_int("mp", "MP"),
            database_int("atk", "ATK"),
            database_int("def", "DEF"),
            database_int("spi", "SPI"),
            database_int("agi", "AGI"),
            database_int("exp", "Exp (Belohnung)"),
            database_int("gold", "Gold (Beute)"),
            database_text("drop", "Drop (Item-ID, leer = keins)"),
            database_decimal("drop_rate", "Drop-Chance (0..1)"),
            database_int("color", "Farbe (RGB 0..16777215)"),
            database_text("desc", "Beschreibung"),
        },
        // 7 Truppen
        {
            database_text("id", "ID"),
            database_text("name", "Name"),
            database_ids("enemies", "Gegner (IDs, komma-getrennt)"),
            database_text("desc", "Beschreibung"),
        },
        // 8 Zustände
        {
            database_text("id", "ID"),
            database_text("name", "Name"),
            database_choice("kind", "Art", database_state_kinds()),
            database_int("power", "Stärke pro Runde"),
            database_int("turns", "Dauer (Runden)"),
            database_text("desc", "Beschreibung"),
        },
        // 9 Animationen
        {
            database_text("id", "ID"),
            database_text("name", "Name"),
            database_text("sheet", "Bild (Materialname)"),
            database_choice("position", "Position", database_animation_positions()),
            database_int("frames", "Einzelbilder"),
            database_int("duration", "Dauer (Frames)"),
            database_text("se", "Klang (SE)"),
            database_text("desc", "Beschreibung"),
        },
        // 10 Tilesets
        {
            database_text("id", "ID"),
            database_text("name", "Name"),
            database_text("texture", "Boden-Textur"),
            database_decimal("tile", "Kachelgröße (m)"),
            database_ids("splats", "Weitere Boden-Texturen"),
            database_text("desc", "Beschreibung"),
        },
        // 11 Gemeinsame Events
        {
            database_int("id", "Nummer"),
            database_text("name", "Name"),
            database_choice("trigger", "Auslöser", database_common_triggers()),
            database_optional(database_text("switch", "Schalter (leer = immer)")),
            database_json("commands", "Befehle (JSON-Liste wie im Event-Editor)"),
        },
        // 12 System (ein Datensatz)
        {
            database_ids("party", "Startparty (Helden-IDs)"),
            database_text("title_bgm", "Titelmusik (BGM)"),
            database_text("battle_bgm", "Kampfmusik (BGM)"),
            database_text("gameover_me", "Game-Over-Klang (ME)"),
            database_text("cursor_se", "Cursor-Klang (SE)"),
            database_text("cancel_se", "Abbruch-Klang (SE)"),
        },
    };
    const int safe = index < 0 || index >= DATABASE_AREA_COUNT ? 0 : index;
    return tables[static_cast<size_t>(safe)];
}

// Platzhalter-Vorgaben: Zahlen starten bei 0, Auswahlfelder beim ersten
// erlaubten Wert. Sinnvolle Startwerte setzt database_default_entry danach.
inline JsonValue database_field_default(const DatabaseFieldSpec& spec) {
    switch (spec.kind) {
        case DatabaseFieldKind::Integer: return JsonValue::make_num(0, true);
        case DatabaseFieldKind::Decimal: return JsonValue::make_num(0.0, false);
        case DatabaseFieldKind::IdList:  return JsonValue::make_arr();
        case DatabaseFieldKind::Json:    return JsonValue::make_arr();
        case DatabaseFieldKind::Choice:
            return JsonValue::make_str(spec.choices.empty() ? std::string() : spec.choices.front());
        case DatabaseFieldKind::Text:    break;
    }
    return JsonValue::make_str("");
}

// Freie ID suchen. Zahlen-IDs (Common Events) werden wie Text verglichen,
// damit auch sehr große vorhandene Nummern exakt bleiben.
inline std::string database_free_id(int index, const JsonValue* array) {
    const DatabaseArea& area = database_area(index);
    for (int n = 1; n < 1000000; ++n) {
        const std::string number = std::to_string(n);
        const std::string candidate = area.numeric_id ? number : std::string(area.id_prefix) + "_" + number;
        bool used = false;
        if (array && array->is_arr()) {
            for (const JsonValue& entry : array->arr) {
                if (!entry.is_obj()) continue;
                const JsonValue* id = entry.find("id");
                if (!id) continue;
                if (area.numeric_id) {
                    if (id->is_num() && (id->integer_text() == number ||
                        (!id->is_int && id->number() == static_cast<double>(n)))) { used = true; break; }
                } else if (id->is_str() && id->str == candidate) { used = true; break; }
            }
        }
        if (!used) return candidate;
    }
    return area.numeric_id ? "1" : std::string(area.id_prefix) + "_1";
}

// Neuer Eintrag mit Vorgaben. Unbekannte Zusatzfelder entstehen nicht.
inline JsonValue database_default_entry(int index, const JsonValue* array) {
    const DatabaseArea& area = database_area(index);
    const std::string id = database_free_id(index, array);
    JsonValue entry = JsonValue::make_obj();
    entry.set("id", area.numeric_id ? JsonValue::make_integer(id) : JsonValue::make_str(id));
    entry.set("name", JsonValue::make_str(area.numeric_id ? std::string(area.id_prefix) + "_" + id : id));
    for (const DatabaseFieldSpec& spec : database_area_fields(index)) {
        const std::string key = spec.key;
        if (key == "id" || key == "name") continue;
        entry.set(key, database_field_default(spec));
    }
    switch (index) {
        case 0:   // Helden
            entry.set("class", JsonValue::make_str("Krieger"));
            entry.set("level", JsonValue::make_num(1, true));
            entry.set("hp", JsonValue::make_num(100, true));
            entry.set("mp", JsonValue::make_num(20, true));
            entry.set("atk", JsonValue::make_num(10, true));
            entry.set("def", JsonValue::make_num(8, true));
            entry.set("spi", JsonValue::make_num(6, true));
            entry.set("agi", JsonValue::make_num(8, true));
            break;
        case 2:   // Fertigkeiten
            entry.set("mp", JsonValue::make_num(3, true));
            entry.set("power", JsonValue::make_num(10, true));
            entry.set("state_rate", JsonValue::make_num(1.0, false));
            entry.set("turns", JsonValue::make_num(3, true));
            break;
        case 3:   // Items
            entry.set("power", JsonValue::make_num(20, true));
            entry.set("price", JsonValue::make_num(10, true));
            break;
        case 4:   // Waffen
        case 5:   // Rüstungen
            entry.set("atk", JsonValue::make_num(index == 4 ? 5 : 0, true));
            entry.set("def", JsonValue::make_num(index == 5 ? 5 : 0, true));
            entry.set("price", JsonValue::make_num(50, true));
            break;
        case 6:   // Gegner
            entry.set("hp", JsonValue::make_num(30, true));
            entry.set("atk", JsonValue::make_num(10, true));
            entry.set("def", JsonValue::make_num(8, true));
            entry.set("spi", JsonValue::make_num(5, true));
            entry.set("agi", JsonValue::make_num(8, true));
            entry.set("exp", JsonValue::make_num(10, true));
            entry.set("color", JsonValue::make_num(13209632, true));
            break;
        case 8:   // Zustände
            entry.set("power", JsonValue::make_num(5, true));
            entry.set("turns", JsonValue::make_num(3, true));
            break;
        case 9:   // Animationen
            entry.set("frames", JsonValue::make_num(4, true));
            entry.set("duration", JsonValue::make_num(20, true));
            break;
        case 10:  // Tilesets
            entry.set("tile", JsonValue::make_num(1.0, false));
            break;
        default: break;
    }
    return entry;
}

// Listenzeile "1. Name  (Zusatz)" - portabel, damit CI sie prüfen kann.
inline std::string database_list_label(int index, int row, const JsonValue& entry) {
    std::string label = std::to_string(row + 1) + ". ";
    if (!entry.is_obj()) return label + "[ungültiger Eintrag]";
    const auto text_of = [&entry](const char* key, const char* fallback) {
        const JsonValue* value = entry.find(key);
        return value && value->is_str() && !value->str.empty() ? value->str : std::string(fallback);
    };
    const auto count_of = [&entry](const char* key) {
        const JsonValue* value = entry.find(key);
        return value && value->is_arr() ? value->arr.size() : size_t{0};
    };
    const JsonValue* name = entry.find("name");
    label += name && name->is_str() ? name->str : "?";
    switch (index) {
        case 0: label += " (" + text_of("class", "-") + ")"; break;
        case 1: label += " [" + text_of("position", "-") + "]"; break;
        case 2: label += " [" + text_of("kind", "-") + ", " +
                        database_number_text(entry.find("mp")) + " MP]"; break;
        case 3: label += " [" + text_of("kind", "-") + "]"; break;
        case 4: label += " [" + text_of("kind", "-") + ", ATK " +
                        database_number_text(entry.find("atk")) + "]"; break;
        case 5: label += " [" + text_of("slot", "-") + ", DEF " +
                        database_number_text(entry.find("def")) + "]"; break;
        case 6: label += " (HP " + database_number_text(entry.find("hp")) + ")"; break;
        case 7: label += " (" + std::to_string(count_of("enemies")) + " Gegner)"; break;
        case 8: label += " [" + text_of("kind", "-") + ", " +
                        database_number_text(entry.find("turns")) + " Runden]"; break;
        case 9: label += " [" + database_number_text(entry.find("frames")) + " Bilder]"; break;
        case 10: label += " [" + text_of("texture", "-") + "]"; break;
        case 11: label += " [" + text_of("trigger", "none") + ", " +
                         std::to_string(count_of("commands")) + " Befehle]"; break;
        default: break;
    }
    return label;
}

} // namespace rune::editor
