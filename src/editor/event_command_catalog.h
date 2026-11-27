#pragma once
// ============================================================================
//  Etappe 44: Befehls-Katalog des Event-Dialogs (F6) wie RPG Maker XP.
//
//  Quelle: Rpg::EventCommands::Layout + Katalog (game/engine/rpg/
//  event_commands/layout.rb, catalog.rb). Diese Datei ist daraus erzeugt:
//  90 Befehle, drei Registerkarten, je zwei Spalten und zwei Abschnitte.
//  Bewusst ohne Win32, damit Reihenfolge, Codes, Felder und Zuordnung
//  kopflos geprueft werden koennen (tools/event_command_catalog_test.cpp).
//
//  NICHT von Hand pflegen: tools/gen_event_command_catalog.rb + .py erzeugen
//  die Datei neu; tools/event_command_catalog_test.cpp prueft Reihenfolge,
//  Codes und Felder gegen den Bauplan. `cmd` ordnet jeden XP-Typ dem bestehenden Event-String zu
//  (docs/befehle.json) oder nullptr, wenn die Engine ihn noch nicht fuehrt.
// ============================================================================

#include <cstddef>
#include <cstring>

namespace rune::editor {

constexpr int EC_COMMAND_COUNT = 90;
constexpr int EC_TAB_COUNT = 3;
constexpr int EC_COLUMN_COUNT = 2;   // links, rechts
constexpr int EC_SECTION_COUNT = 12; // 3 Tabs x 2 Spalten x 2 Abschnitte

enum class EcFieldKind { ActorRef, Bool, Color, EnemySlot, Enum, EquipmentRef, EventRef, Goods, Integer, Key, MoveSteps, Number, Ref, String, StringList };

struct EcField {
    const char* name;
    EcFieldKind kind;
    bool required;
    const char* default_text;   // Vorgabe als Text, nullptr = keine
    const char* const* values;  // Enum-Auswahl, nullptr = keine
    int value_count;
    double min_value, max_value;
    bool has_min, has_max;
    const char* ref;            // Referenz-Sammlung (switches, items, ...)
    const char* key;            // Asset-Sammlung (bgms, pictures, ...)
};

struct EcCommand {
    const char* type;           // commandType der Rpg-Schicht
    int code;                   // XP-Code
    int tab;                    // 1..3
    int column;                 // 0 = links, 1 = rechts
    const char* section;        // Abschnitts-Key
    const char* label;          // deutsche Beschriftung
    const char* xp_name;        // XP-Originalname
    int position;               // Reihenfolge im Abschnitt
    const char* summary;
    const char* cmd;            // bestehender Event-String oder nullptr
    const char* const* blocks;
    int block_count;
    const EcField* fields;
    int field_count;
};

struct EcTabInfo { int number; const char* title; };
struct EcSectionInfo { const char* key; const char* title; int tab; int column; };

static const char* const EC_ENUM_0[] = { "window", "dim", "transparent" };
static const char* const EC_ENUM_1[] = { "top", "middle", "bottom" };
static const char* const EC_ENUM_2[] = { "cancel", "disabled", "branch" };
static const char* const EC_ENUM_3[] = { "left", "center", "right" };
static const char* const EC_ENUM_4[] = { "switch", "variable", "self_switch", "timer", "actor", "enemy", "gold", "item", "weapon", "armor", "button", "script" };
static const char* const EC_ENUM_5[] = { "equal", "greater_equal", "less_equal", "greater", "less", "not_equal" };
static const char* const EC_ENUM_6[] = { "A", "B", "C", "D" };
static const char* const EC_ENUM_7[] = { "set", "add", "subtract", "multiply", "divide", "modulo" };
static const char* const EC_ENUM_8[] = { "constant", "variable", "random", "item", "actor", "enemy", "other" };
static const char* const EC_ENUM_9[] = { "party_size", "gold", "steps", "timer" };
static const char* const EC_ENUM_10[] = { "start", "stop", "set", "add", "subtract" };
static const char* const EC_ENUM_11[] = { "add", "subtract" };
static const char* const EC_ENUM_12[] = { "add", "remove" };
static const char* const EC_ENUM_13[] = { "up", "down", "left", "right" };
static const char* const EC_ENUM_14[] = { "none", "fade" };
static const char* const EC_ENUM_15[] = { "player", "this_event", "event" };
static const char* const EC_ENUM_16[] = { "panorama", "fog", "battleback" };
static const char* const EC_ENUM_17[] = { "normal", "add", "subtract" };
static const char* const EC_ENUM_18[] = { "none", "rain", "storm", "snow" };
static const char* const EC_ENUM_19[] = { "max_hp", "max_sp", "strength", "dexterity", "agility", "intelligence" };
static const char* const EC_ENUM_20[] = { "weapon", "armor", "accessory" };
static const char* const EC_BLOCKS_0[] = { "choice_0", "choice_1", "choice_2", "choice_3", "cancel" };
static const char* const EC_BLOCKS_1[] = { "then", "else" };
static const char* const EC_BLOCKS_2[] = { "body" };
static const char* const EC_BLOCKS_3[] = { "win", "escape", "lose" };

static const EcField EC_FIELDS_0[] = {
    { "lines", EcFieldKind::StringList, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, nullptr, nullptr },
    { "speaker", EcFieldKind::String, false, nullptr, nullptr, 0, 0.0, 0.0, false, false, nullptr, nullptr },
    { "face_key", EcFieldKind::Key, false, nullptr, nullptr, 0, 0.0, 0.0, false, false, nullptr, "faces" },
    { "background", EcFieldKind::Enum, false, "window", EC_ENUM_0, 3, 0.0, 0.0, false, false, nullptr, nullptr },
    { "position", EcFieldKind::Enum, false, "bottom", EC_ENUM_1, 3, 0.0, 0.0, false, false, nullptr, nullptr },
};
static const EcField EC_FIELDS_1[] = {
    { "choices", EcFieldKind::StringList, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, nullptr, nullptr },
    { "cancel_index", EcFieldKind::Integer, false, "-1", nullptr, 0, -1.0, 4.0, true, true, nullptr, nullptr },
    { "cancel_behavior", EcFieldKind::Enum, false, "cancel", EC_ENUM_2, 3, 0.0, 0.0, false, false, nullptr, nullptr },
    { "default_index", EcFieldKind::Integer, false, "0", nullptr, 0, 0.0, 3.0, true, true, nullptr, nullptr },
    { "position", EcFieldKind::Enum, false, "center", EC_ENUM_3, 3, 0.0, 0.0, false, false, nullptr, nullptr },
    { "background", EcFieldKind::Enum, false, "window", EC_ENUM_0, 3, 0.0, 0.0, false, false, nullptr, nullptr },
};
static const EcField EC_FIELDS_2[] = {
    { "variable_id", EcFieldKind::Ref, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, "variables", nullptr },
    { "digits", EcFieldKind::Integer, false, "4", nullptr, 0, 1.0, 8.0, true, true, nullptr, nullptr },
};
static const EcField EC_FIELDS_3[] = {
    { "background", EcFieldKind::Enum, true, nullptr, EC_ENUM_0, 3, 0.0, 0.0, false, false, nullptr, nullptr },
    { "position", EcFieldKind::Enum, true, nullptr, EC_ENUM_1, 3, 0.0, 0.0, false, false, nullptr, nullptr },
    { "speed", EcFieldKind::Integer, true, nullptr, nullptr, 0, 1.0, 10.0, true, true, nullptr, nullptr },
    { "no_fast", EcFieldKind::Bool, false, "false", nullptr, 0, 0.0, 0.0, false, false, nullptr, nullptr },
};
static const EcField EC_FIELDS_4[] = {
    { "variable_id", EcFieldKind::Ref, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, "variables", nullptr },
};
static const EcField EC_FIELDS_5[] = {
    { "seconds", EcFieldKind::Number, false, nullptr, nullptr, 0, 0.0, 60.0, true, true, nullptr, nullptr },
    { "frames", EcFieldKind::Integer, false, nullptr, nullptr, 0, 0.0, 3600.0, true, true, nullptr, nullptr },
};
static const EcField EC_FIELDS_6[] = {
    { "text", EcFieldKind::String, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, nullptr, nullptr },
};
static const EcField EC_FIELDS_7[] = {
    { "kind", EcFieldKind::Enum, true, nullptr, EC_ENUM_4, 12, 0.0, 0.0, false, false, nullptr, nullptr },
    { "switch_id", EcFieldKind::Ref, false, nullptr, nullptr, 0, 0.0, 0.0, false, false, "switches", nullptr },
    { "variable_id", EcFieldKind::Ref, false, nullptr, nullptr, 0, 0.0, 0.0, false, false, "variables", nullptr },
    { "comparator", EcFieldKind::Enum, false, nullptr, EC_ENUM_5, 6, 0.0, 0.0, false, false, nullptr, nullptr },
    { "value", EcFieldKind::Number, false, nullptr, nullptr, 0, 0.0, 0.0, false, false, nullptr, nullptr },
    { "value_variable_id", EcFieldKind::Ref, false, nullptr, nullptr, 0, 0.0, 0.0, false, false, "variables", nullptr },
    { "letter", EcFieldKind::Enum, false, nullptr, EC_ENUM_6, 4, 0.0, 0.0, false, false, nullptr, nullptr },
    { "seconds", EcFieldKind::Number, false, nullptr, nullptr, 0, 0.0, 0.0, true, false, nullptr, nullptr },
    { "actor_id", EcFieldKind::ActorRef, false, nullptr, nullptr, 0, 0.0, 0.0, false, false, nullptr, nullptr },
    { "enemy_slot", EcFieldKind::EnemySlot, false, nullptr, nullptr, 0, 0.0, 0.0, false, false, nullptr, nullptr },
    { "item_id", EcFieldKind::Ref, false, nullptr, nullptr, 0, 0.0, 0.0, false, false, "items", nullptr },
    { "weapon_id", EcFieldKind::Ref, false, nullptr, nullptr, 0, 0.0, 0.0, false, false, "weapons", nullptr },
    { "armor_id", EcFieldKind::Ref, false, nullptr, nullptr, 0, 0.0, 0.0, false, false, "armors", nullptr },
    { "button", EcFieldKind::Integer, false, nullptr, nullptr, 0, 0.0, 255.0, true, true, nullptr, nullptr },
    { "script_key", EcFieldKind::Key, false, nullptr, nullptr, 0, 0.0, 0.0, false, false, nullptr, "scripts" },
    { "expression", EcFieldKind::String, false, nullptr, nullptr, 0, 0.0, 0.0, false, false, nullptr, nullptr },
};
static const EcField EC_FIELDS_12[] = {
    { "common_event_id", EcFieldKind::Ref, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, "common_events", nullptr },
};
static const EcField EC_FIELDS_13[] = {
    { "name", EcFieldKind::String, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, nullptr, nullptr },
};
static const EcField EC_FIELDS_14[] = {
    { "name", EcFieldKind::String, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, nullptr, nullptr },
};
static const EcField EC_FIELDS_15[] = {
    { "from_id", EcFieldKind::Ref, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, "switches", nullptr },
    { "to_id", EcFieldKind::Ref, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, "switches", nullptr },
    { "value", EcFieldKind::Bool, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, nullptr, nullptr },
};
static const EcField EC_FIELDS_16[] = {
    { "from_id", EcFieldKind::Ref, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, "variables", nullptr },
    { "to_id", EcFieldKind::Ref, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, "variables", nullptr },
    { "operation", EcFieldKind::Enum, true, nullptr, EC_ENUM_7, 6, 0.0, 0.0, false, false, nullptr, nullptr },
    { "source", EcFieldKind::Enum, true, nullptr, EC_ENUM_8, 7, 0.0, 0.0, false, false, nullptr, nullptr },
    { "value", EcFieldKind::Number, false, nullptr, nullptr, 0, 0.0, 0.0, false, false, nullptr, nullptr },
    { "random_min", EcFieldKind::Integer, false, nullptr, nullptr, 0, 0.0, 0.0, true, false, nullptr, nullptr },
    { "random_max", EcFieldKind::Integer, false, nullptr, nullptr, 0, 0.0, 0.0, true, false, nullptr, nullptr },
    { "source_variable_id", EcFieldKind::Ref, false, nullptr, nullptr, 0, 0.0, 0.0, false, false, "variables", nullptr },
    { "item_id", EcFieldKind::Ref, false, nullptr, nullptr, 0, 0.0, 0.0, false, false, "items", nullptr },
    { "actor_id", EcFieldKind::ActorRef, false, nullptr, nullptr, 0, 0.0, 0.0, false, false, nullptr, nullptr },
    { "enemy_slot", EcFieldKind::EnemySlot, false, nullptr, nullptr, 0, 0.0, 0.0, false, false, nullptr, nullptr },
    { "other", EcFieldKind::Enum, false, nullptr, EC_ENUM_9, 4, 0.0, 0.0, false, false, nullptr, nullptr },
};
static const EcField EC_FIELDS_17[] = {
    { "letter", EcFieldKind::Enum, true, nullptr, EC_ENUM_6, 4, 0.0, 0.0, false, false, nullptr, nullptr },
    { "value", EcFieldKind::Bool, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, nullptr, nullptr },
};
static const EcField EC_FIELDS_18[] = {
    { "operation", EcFieldKind::Enum, true, nullptr, EC_ENUM_10, 5, 0.0, 0.0, false, false, nullptr, nullptr },
    { "seconds", EcFieldKind::Number, false, nullptr, nullptr, 0, 0.0, 0.0, true, false, nullptr, nullptr },
};
static const EcField EC_FIELDS_19[] = {
    { "operation", EcFieldKind::Enum, true, nullptr, EC_ENUM_11, 2, 0.0, 0.0, false, false, nullptr, nullptr },
    { "amount", EcFieldKind::Integer, true, nullptr, nullptr, 0, 0.0, 0.0, true, false, nullptr, nullptr },
};
static const EcField EC_FIELDS_20[] = {
    { "item_id", EcFieldKind::Ref, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, "items", nullptr },
    { "operation", EcFieldKind::Enum, true, nullptr, EC_ENUM_11, 2, 0.0, 0.0, false, false, nullptr, nullptr },
    { "amount", EcFieldKind::Integer, true, nullptr, nullptr, 0, 0.0, 999.0, true, true, nullptr, nullptr },
};
static const EcField EC_FIELDS_21[] = {
    { "weapon_id", EcFieldKind::Ref, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, "weapons", nullptr },
    { "operation", EcFieldKind::Enum, true, nullptr, EC_ENUM_11, 2, 0.0, 0.0, false, false, nullptr, nullptr },
    { "amount", EcFieldKind::Integer, true, nullptr, nullptr, 0, 0.0, 999.0, true, true, nullptr, nullptr },
    { "include_equipment", EcFieldKind::Bool, false, "false", nullptr, 0, 0.0, 0.0, false, false, nullptr, nullptr },
};
static const EcField EC_FIELDS_22[] = {
    { "armor_id", EcFieldKind::Ref, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, "armors", nullptr },
    { "operation", EcFieldKind::Enum, true, nullptr, EC_ENUM_11, 2, 0.0, 0.0, false, false, nullptr, nullptr },
    { "amount", EcFieldKind::Integer, true, nullptr, nullptr, 0, 0.0, 999.0, true, true, nullptr, nullptr },
    { "include_equipment", EcFieldKind::Bool, false, "false", nullptr, 0, 0.0, 0.0, false, false, nullptr, nullptr },
};
static const EcField EC_FIELDS_23[] = {
    { "actor_id", EcFieldKind::ActorRef, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, nullptr, nullptr },
    { "operation", EcFieldKind::Enum, true, nullptr, EC_ENUM_12, 2, 0.0, 0.0, false, false, nullptr, nullptr },
    { "initialize", EcFieldKind::Bool, false, "false", nullptr, 0, 0.0, 0.0, false, false, nullptr, nullptr },
};
static const EcField EC_FIELDS_24[] = {
    { "skin_key", EcFieldKind::Key, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, nullptr, "windowskins" },
};
static const EcField EC_FIELDS_25[] = {
    { "bgm_key", EcFieldKind::Key, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, nullptr, "bgms" },
};
static const EcField EC_FIELDS_26[] = {
    { "me_key", EcFieldKind::Key, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, nullptr, "mes" },
};
static const EcField EC_FIELDS_27[] = {
    { "allowed", EcFieldKind::Bool, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, nullptr, nullptr },
};
static const EcField EC_FIELDS_28[] = {
    { "allowed", EcFieldKind::Bool, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, nullptr, nullptr },
};
static const EcField EC_FIELDS_29[] = {
    { "allowed", EcFieldKind::Bool, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, nullptr, nullptr },
};
static const EcField EC_FIELDS_30[] = {
    { "map_id", EcFieldKind::Ref, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, "maps", nullptr },
    { "x", EcFieldKind::Integer, true, nullptr, nullptr, 0, 0.0, 0.0, true, false, nullptr, nullptr },
    { "y", EcFieldKind::Integer, true, nullptr, nullptr, 0, 0.0, 0.0, true, false, nullptr, nullptr },
    { "direction", EcFieldKind::Enum, false, "down", EC_ENUM_13, 4, 0.0, 0.0, false, false, nullptr, nullptr },
    { "fade", EcFieldKind::Enum, false, "none", EC_ENUM_14, 2, 0.0, 0.0, false, false, nullptr, nullptr },
};
static const EcField EC_FIELDS_31[] = {
    { "target", EcFieldKind::Enum, true, nullptr, EC_ENUM_15, 3, 0.0, 0.0, false, false, nullptr, nullptr },
    { "event_id", EcFieldKind::EventRef, false, nullptr, nullptr, 0, 0.0, 0.0, false, false, nullptr, nullptr },
    { "x", EcFieldKind::Integer, true, nullptr, nullptr, 0, 0.0, 0.0, true, false, nullptr, nullptr },
    { "y", EcFieldKind::Integer, true, nullptr, nullptr, 0, 0.0, 0.0, true, false, nullptr, nullptr },
};
static const EcField EC_FIELDS_32[] = {
    { "direction", EcFieldKind::Enum, true, nullptr, EC_ENUM_13, 4, 0.0, 0.0, false, false, nullptr, nullptr },
    { "distance", EcFieldKind::Integer, true, nullptr, nullptr, 0, 1.0, 999.0, true, true, nullptr, nullptr },
    { "speed", EcFieldKind::Integer, true, nullptr, nullptr, 0, 1.0, 10.0, true, true, nullptr, nullptr },
};
static const EcField EC_FIELDS_33[] = {
    { "target", EcFieldKind::Enum, true, nullptr, EC_ENUM_16, 3, 0.0, 0.0, false, false, nullptr, nullptr },
    { "asset_key", EcFieldKind::String, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, nullptr, nullptr },
    { "color", EcFieldKind::Color, false, nullptr, nullptr, 0, 0.0, 0.0, false, false, nullptr, nullptr },
};
static const EcField EC_FIELDS_34[] = {
    { "color", EcFieldKind::Color, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, nullptr, nullptr },
};
static const EcField EC_FIELDS_35[] = {
    { "opacity", EcFieldKind::Integer, true, nullptr, nullptr, 0, 0.0, 255.0, true, true, nullptr, nullptr },
};
static const EcField EC_FIELDS_36[] = {
    { "target", EcFieldKind::Enum, true, nullptr, EC_ENUM_15, 3, 0.0, 0.0, false, false, nullptr, nullptr },
    { "event_id", EcFieldKind::EventRef, false, nullptr, nullptr, 0, 0.0, 0.0, false, false, nullptr, nullptr },
    { "animation_key", EcFieldKind::Key, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, nullptr, "animations" },
};
static const EcField EC_FIELDS_37[] = {
    { "transparent", EcFieldKind::Bool, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, nullptr, nullptr },
};
static const EcField EC_FIELDS_38[] = {
    { "target", EcFieldKind::Enum, true, nullptr, EC_ENUM_15, 3, 0.0, 0.0, false, false, nullptr, nullptr },
    { "event_id", EcFieldKind::EventRef, false, nullptr, nullptr, 0, 0.0, 0.0, false, false, nullptr, nullptr },
    { "steps", EcFieldKind::MoveSteps, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, nullptr, nullptr },
    { "wait", EcFieldKind::Bool, false, "false", nullptr, 0, 0.0, 0.0, false, false, nullptr, nullptr },
    { "repeat", EcFieldKind::Bool, false, "false", nullptr, 0, 0.0, 0.0, false, false, nullptr, nullptr },
    { "ignore", EcFieldKind::Bool, false, "false", nullptr, 0, 0.0, 0.0, false, false, nullptr, nullptr },
};
static const EcField EC_FIELDS_40[] = {
    { "transition_key", EcFieldKind::Key, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, nullptr, "transitions" },
};
static const EcField EC_FIELDS_41[] = {
    { "transition_key", EcFieldKind::Key, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, nullptr, "transitions" },
};
static const EcField EC_FIELDS_42[] = {
    { "color", EcFieldKind::Color, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, nullptr, nullptr },
};
static const EcField EC_FIELDS_43[] = {
    { "color", EcFieldKind::Color, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, nullptr, nullptr },
    { "duration", EcFieldKind::Number, true, nullptr, nullptr, 0, 0.0, 0.0, true, false, nullptr, nullptr },
    { "no_wait", EcFieldKind::Bool, false, "false", nullptr, 0, 0.0, 0.0, false, false, nullptr, nullptr },
};
static const EcField EC_FIELDS_44[] = {
    { "strength", EcFieldKind::Integer, true, nullptr, nullptr, 0, 1.0, 20.0, true, true, nullptr, nullptr },
    { "speed", EcFieldKind::Integer, true, nullptr, nullptr, 0, 1.0, 20.0, true, true, nullptr, nullptr },
    { "duration", EcFieldKind::Number, true, nullptr, nullptr, 0, 0.0, 0.0, true, false, nullptr, nullptr },
    { "no_wait", EcFieldKind::Bool, false, "false", nullptr, 0, 0.0, 0.0, false, false, nullptr, nullptr },
};
static const EcField EC_FIELDS_45[] = {
    { "picture_slot_id", EcFieldKind::Integer, true, nullptr, nullptr, 0, 1.0, 50.0, true, true, nullptr, nullptr },
    { "picture_key", EcFieldKind::Key, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, nullptr, "pictures" },
    { "x", EcFieldKind::Integer, true, nullptr, nullptr, 0, -9999.0, 9999.0, true, true, nullptr, nullptr },
    { "y", EcFieldKind::Integer, true, nullptr, nullptr, 0, -9999.0, 9999.0, true, true, nullptr, nullptr },
    { "scale_x", EcFieldKind::Integer, false, "100", nullptr, 0, 0.0, 1000.0, true, true, nullptr, nullptr },
    { "scale_y", EcFieldKind::Integer, false, "100", nullptr, 0, 0.0, 1000.0, true, true, nullptr, nullptr },
    { "opacity", EcFieldKind::Integer, false, "255", nullptr, 0, 0.0, 255.0, true, true, nullptr, nullptr },
    { "blend", EcFieldKind::Enum, false, "normal", EC_ENUM_17, 3, 0.0, 0.0, false, false, nullptr, nullptr },
};
static const EcField EC_FIELDS_46[] = {
    { "picture_slot_id", EcFieldKind::Integer, true, nullptr, nullptr, 0, 1.0, 50.0, true, true, nullptr, nullptr },
    { "picture_key", EcFieldKind::Key, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, nullptr, "pictures" },
    { "x", EcFieldKind::Integer, true, nullptr, nullptr, 0, -9999.0, 9999.0, true, true, nullptr, nullptr },
    { "y", EcFieldKind::Integer, true, nullptr, nullptr, 0, -9999.0, 9999.0, true, true, nullptr, nullptr },
    { "scale_x", EcFieldKind::Integer, false, "100", nullptr, 0, 0.0, 1000.0, true, true, nullptr, nullptr },
    { "scale_y", EcFieldKind::Integer, false, "100", nullptr, 0, 0.0, 1000.0, true, true, nullptr, nullptr },
    { "opacity", EcFieldKind::Integer, false, "255", nullptr, 0, 0.0, 255.0, true, true, nullptr, nullptr },
    { "blend", EcFieldKind::Enum, false, "normal", EC_ENUM_17, 3, 0.0, 0.0, false, false, nullptr, nullptr },
    { "duration", EcFieldKind::Number, false, "0.0", nullptr, 0, 0.0, 0.0, true, false, nullptr, nullptr },
    { "no_wait", EcFieldKind::Bool, false, "false", nullptr, 0, 0.0, 0.0, false, false, nullptr, nullptr },
};
static const EcField EC_FIELDS_47[] = {
    { "picture_slot_id", EcFieldKind::Integer, true, nullptr, nullptr, 0, 1.0, 50.0, true, true, nullptr, nullptr },
    { "picture_key", EcFieldKind::Key, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, nullptr, "pictures" },
    { "duration", EcFieldKind::Number, false, "0.0", nullptr, 0, 0.0, 0.0, true, false, nullptr, nullptr },
    { "no_wait", EcFieldKind::Bool, false, "false", nullptr, 0, 0.0, 0.0, false, false, nullptr, nullptr },
    { "degrees", EcFieldKind::Number, true, nullptr, nullptr, 0, -360.0, 360.0, true, true, nullptr, nullptr },
};
static const EcField EC_FIELDS_48[] = {
    { "picture_slot_id", EcFieldKind::Integer, true, nullptr, nullptr, 0, 1.0, 50.0, true, true, nullptr, nullptr },
    { "picture_key", EcFieldKind::Key, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, nullptr, "pictures" },
    { "duration", EcFieldKind::Number, false, "0.0", nullptr, 0, 0.0, 0.0, true, false, nullptr, nullptr },
    { "no_wait", EcFieldKind::Bool, false, "false", nullptr, 0, 0.0, 0.0, false, false, nullptr, nullptr },
    { "color", EcFieldKind::Color, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, nullptr, nullptr },
};
static const EcField EC_FIELDS_49[] = {
    { "picture_slot_id", EcFieldKind::Integer, true, nullptr, nullptr, 0, 1.0, 50.0, true, true, nullptr, nullptr },
    { "picture_key", EcFieldKind::Key, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, nullptr, "pictures" },
};
static const EcField EC_FIELDS_50[] = {
    { "kind", EcFieldKind::Enum, true, nullptr, EC_ENUM_18, 4, 0.0, 0.0, false, false, nullptr, nullptr },
    { "intensity", EcFieldKind::Integer, true, nullptr, nullptr, 0, 0.0, 10.0, true, true, nullptr, nullptr },
    { "duration", EcFieldKind::Number, false, "0.0", nullptr, 0, 0.0, 0.0, true, false, nullptr, nullptr },
};
static const EcField EC_FIELDS_51[] = {
    { "bgm_key", EcFieldKind::Key, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, nullptr, "bgms" },
    { "volume", EcFieldKind::Integer, false, "80", nullptr, 0, 0.0, 100.0, true, true, nullptr, nullptr },
    { "pitch", EcFieldKind::Integer, false, "100", nullptr, 0, 50.0, 150.0, true, true, nullptr, nullptr },
};
static const EcField EC_FIELDS_52[] = {
    { "bgm_key", EcFieldKind::Key, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, nullptr, "bgms" },
    { "fade_seconds", EcFieldKind::Number, false, "1.0", nullptr, 0, 0.0, 0.0, true, false, nullptr, nullptr },
};
static const EcField EC_FIELDS_53[] = {
    { "bgs_key", EcFieldKind::Key, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, nullptr, "bgss" },
    { "volume", EcFieldKind::Integer, false, "80", nullptr, 0, 0.0, 100.0, true, true, nullptr, nullptr },
    { "pitch", EcFieldKind::Integer, false, "100", nullptr, 0, 50.0, 150.0, true, true, nullptr, nullptr },
};
static const EcField EC_FIELDS_54[] = {
    { "bgs_key", EcFieldKind::Key, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, nullptr, "bgss" },
    { "fade_seconds", EcFieldKind::Number, false, "1.0", nullptr, 0, 0.0, 0.0, true, false, nullptr, nullptr },
};
static const EcField EC_FIELDS_57[] = {
    { "me_key", EcFieldKind::Key, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, nullptr, "mes" },
    { "volume", EcFieldKind::Integer, false, "80", nullptr, 0, 0.0, 100.0, true, true, nullptr, nullptr },
    { "pitch", EcFieldKind::Integer, false, "100", nullptr, 0, 50.0, 150.0, true, true, nullptr, nullptr },
};
static const EcField EC_FIELDS_58[] = {
    { "se_key", EcFieldKind::Key, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, nullptr, "ses" },
    { "volume", EcFieldKind::Integer, false, "80", nullptr, 0, 0.0, 100.0, true, true, nullptr, nullptr },
    { "pitch", EcFieldKind::Integer, false, "100", nullptr, 0, 50.0, 150.0, true, true, nullptr, nullptr },
};
static const EcField EC_FIELDS_59[] = {
    { "se_key", EcFieldKind::Key, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, nullptr, "ses" },
};
static const EcField EC_FIELDS_60[] = {
    { "troop_id", EcFieldKind::Ref, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, "troops", nullptr },
    { "can_escape", EcFieldKind::Bool, false, "true", nullptr, 0, 0.0, 0.0, false, false, nullptr, nullptr },
    { "can_lose", EcFieldKind::Bool, false, "false", nullptr, 0, 0.0, 0.0, false, false, nullptr, nullptr },
};
static const EcField EC_FIELDS_61[] = {
    { "shop_key", EcFieldKind::Key, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, nullptr, "shops" },
    { "goods", EcFieldKind::Goods, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, nullptr, nullptr },
};
static const EcField EC_FIELDS_62[] = {
    { "actor_id", EcFieldKind::ActorRef, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, nullptr, nullptr },
    { "max_length", EcFieldKind::Integer, false, "8", nullptr, 0, 1.0, 16.0, true, true, nullptr, nullptr },
};
static const EcField EC_FIELDS_63[] = {
    { "actor_id", EcFieldKind::ActorRef, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, nullptr, nullptr },
    { "operation", EcFieldKind::Enum, true, nullptr, EC_ENUM_11, 2, 0.0, 0.0, false, false, nullptr, nullptr },
    { "amount", EcFieldKind::Integer, true, nullptr, nullptr, 0, 0.0, 999999.0, true, true, nullptr, nullptr },
    { "allow_knockout", EcFieldKind::Bool, false, "false", nullptr, 0, 0.0, 0.0, false, false, nullptr, nullptr },
};
static const EcField EC_FIELDS_64[] = {
    { "actor_id", EcFieldKind::ActorRef, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, nullptr, nullptr },
    { "operation", EcFieldKind::Enum, true, nullptr, EC_ENUM_11, 2, 0.0, 0.0, false, false, nullptr, nullptr },
    { "amount", EcFieldKind::Integer, true, nullptr, nullptr, 0, 0.0, 999999.0, true, true, nullptr, nullptr },
};
static const EcField EC_FIELDS_65[] = {
    { "actor_id", EcFieldKind::ActorRef, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, nullptr, nullptr },
    { "state_id", EcFieldKind::Ref, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, "states", nullptr },
    { "operation", EcFieldKind::Enum, true, nullptr, EC_ENUM_12, 2, 0.0, 0.0, false, false, nullptr, nullptr },
};
static const EcField EC_FIELDS_66[] = {
    { "actor_id", EcFieldKind::ActorRef, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, nullptr, nullptr },
};
static const EcField EC_FIELDS_67[] = {
    { "actor_id", EcFieldKind::ActorRef, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, nullptr, nullptr },
    { "operation", EcFieldKind::Enum, true, nullptr, EC_ENUM_11, 2, 0.0, 0.0, false, false, nullptr, nullptr },
    { "amount", EcFieldKind::Integer, true, nullptr, nullptr, 0, 0.0, 999999.0, true, true, nullptr, nullptr },
};
static const EcField EC_FIELDS_68[] = {
    { "actor_id", EcFieldKind::ActorRef, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, nullptr, nullptr },
    { "operation", EcFieldKind::Enum, true, nullptr, EC_ENUM_11, 2, 0.0, 0.0, false, false, nullptr, nullptr },
    { "amount", EcFieldKind::Integer, true, nullptr, nullptr, 0, 0.0, 999999.0, true, true, nullptr, nullptr },
};
static const EcField EC_FIELDS_69[] = {
    { "actor_id", EcFieldKind::ActorRef, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, nullptr, nullptr },
    { "operation", EcFieldKind::Enum, true, nullptr, EC_ENUM_11, 2, 0.0, 0.0, false, false, nullptr, nullptr },
    { "amount", EcFieldKind::Integer, true, nullptr, nullptr, 0, 0.0, 999999.0, true, true, nullptr, nullptr },
    { "parameter", EcFieldKind::Enum, true, nullptr, EC_ENUM_19, 6, 0.0, 0.0, false, false, nullptr, nullptr },
};
static const EcField EC_FIELDS_70[] = {
    { "actor_id", EcFieldKind::ActorRef, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, nullptr, nullptr },
    { "skill_id", EcFieldKind::Ref, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, "skills", nullptr },
    { "operation", EcFieldKind::Enum, true, nullptr, EC_ENUM_12, 2, 0.0, 0.0, false, false, nullptr, nullptr },
};
static const EcField EC_FIELDS_71[] = {
    { "actor_id", EcFieldKind::ActorRef, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, nullptr, nullptr },
    { "slot", EcFieldKind::Enum, true, nullptr, EC_ENUM_20, 3, 0.0, 0.0, false, false, nullptr, nullptr },
    { "item_id", EcFieldKind::EquipmentRef, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, nullptr, nullptr },
};
static const EcField EC_FIELDS_72[] = {
    { "actor_id", EcFieldKind::ActorRef, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, nullptr, nullptr },
    { "name", EcFieldKind::String, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, nullptr, nullptr },
};
static const EcField EC_FIELDS_73[] = {
    { "actor_id", EcFieldKind::ActorRef, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, nullptr, nullptr },
    { "class_id", EcFieldKind::Ref, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, "classes", nullptr },
};
static const EcField EC_FIELDS_74[] = {
    { "actor_id", EcFieldKind::ActorRef, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, nullptr, nullptr },
    { "character_key", EcFieldKind::Key, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, nullptr, "characters" },
    { "battler_key", EcFieldKind::Key, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, nullptr, "battlers" },
    { "face_key", EcFieldKind::Key, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, nullptr, "faces" },
};
static const EcField EC_FIELDS_75[] = {
    { "slot", EcFieldKind::EnemySlot, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, nullptr, nullptr },
    { "operation", EcFieldKind::Enum, true, nullptr, EC_ENUM_11, 2, 0.0, 0.0, false, false, nullptr, nullptr },
    { "amount", EcFieldKind::Integer, true, nullptr, nullptr, 0, 0.0, 999999.0, true, true, nullptr, nullptr },
    { "allow_knockout", EcFieldKind::Bool, false, "false", nullptr, 0, 0.0, 0.0, false, false, nullptr, nullptr },
};
static const EcField EC_FIELDS_76[] = {
    { "slot", EcFieldKind::EnemySlot, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, nullptr, nullptr },
    { "operation", EcFieldKind::Enum, true, nullptr, EC_ENUM_11, 2, 0.0, 0.0, false, false, nullptr, nullptr },
    { "amount", EcFieldKind::Integer, true, nullptr, nullptr, 0, 0.0, 999999.0, true, true, nullptr, nullptr },
};
static const EcField EC_FIELDS_77[] = {
    { "slot", EcFieldKind::EnemySlot, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, nullptr, nullptr },
    { "state_id", EcFieldKind::Ref, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, "states", nullptr },
    { "operation", EcFieldKind::Enum, true, nullptr, EC_ENUM_12, 2, 0.0, 0.0, false, false, nullptr, nullptr },
};
static const EcField EC_FIELDS_78[] = {
    { "slot", EcFieldKind::EnemySlot, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, nullptr, nullptr },
};
static const EcField EC_FIELDS_79[] = {
    { "slot", EcFieldKind::EnemySlot, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, nullptr, nullptr },
};
static const EcField EC_FIELDS_80[] = {
    { "slot", EcFieldKind::EnemySlot, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, nullptr, nullptr },
    { "enemy_id", EcFieldKind::Ref, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, "enemies", nullptr },
};
static const EcField EC_FIELDS_81[] = {
    { "slot", EcFieldKind::EnemySlot, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, nullptr, nullptr },
    { "animation_key", EcFieldKind::Key, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, nullptr, "animations" },
};
static const EcField EC_FIELDS_82[] = {
    { "slot", EcFieldKind::EnemySlot, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, nullptr, nullptr },
    { "amount", EcFieldKind::Integer, true, nullptr, nullptr, 0, 0.0, 0.0, true, false, nullptr, nullptr },
    { "element_id", EcFieldKind::Ref, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, "elements", nullptr },
};
static const EcField EC_FIELDS_83[] = {
    { "slot", EcFieldKind::EnemySlot, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, nullptr, nullptr },
    { "action_key", EcFieldKind::Key, true, nullptr, nullptr, 0, 0.0, 0.0, false, false, nullptr, "skills" },
    { "target_slot", EcFieldKind::EnemySlot, true, nullptr, nullptr, 0, -1.0, 0.0, true, false, nullptr, nullptr },
};
static const EcField EC_FIELDS_89[] = {
    { "script_key", EcFieldKind::Key, false, nullptr, nullptr, 0, 0.0, 0.0, false, false, nullptr, "scripts" },
    { "expression", EcFieldKind::String, false, nullptr, nullptr, 0, 0.0, 0.0, false, false, nullptr, nullptr },
    { "code", EcFieldKind::String, false, nullptr, nullptr, 0, 0.0, 0.0, false, false, nullptr, nullptr },
};

static const EcCommand EC_COMMANDS[EC_COMMAND_COUNT] = {
    { "show_text", 101, 1, 0, "dialog", "Text anzeigen", "Show Text", 0, "Zeigt eigene Textzeilen in einem Dialogfenster an.", "text", nullptr, 0, EC_FIELDS_0, 5 },
    { "show_choices", 102, 1, 0, "dialog", "Auswahl anzeigen", "Show Choices", 1, "Zeigt bis zu vier Auswahlen und verzweigt in benannte Blöcke.", "choices", EC_BLOCKS_0, 5, EC_FIELDS_1, 6 },
    { "input_number", 103, 1, 0, "dialog", "Zahl eingeben", "Input Number", 2, "Schreibt eine Zahleneingabe in eine Variable.", "input_number", nullptr, 0, EC_FIELDS_2, 2 },
    { "change_text_options", 104, 1, 0, "dialog", "Textoptionen ändern", "Change Text Options", 3, "Stellt Fensterstil, Position und Textgeschwindigkeit um.", "text_options", nullptr, 0, EC_FIELDS_3, 4 },
    { "button_input_processing", 105, 1, 0, "dialog", "Tastendruck abfragen", "Button Input Processing", 4, "Wartet auf einen Tastendruck und speichert ihn.", "button_input", nullptr, 0, EC_FIELDS_4, 1 },
    { "wait", 106, 1, 0, "dialog", "Warten", "Wait", 5, "Blockiert den Interpreter für Sekunden oder Frames.", "wait", nullptr, 0, EC_FIELDS_5, 2 },
    { "comment", 108, 1, 0, "dialog", "Kommentar", "Comment", 6, "Dokumentiert einen Schritt ohne Laufzeitwirkung.", "log", nullptr, 0, EC_FIELDS_6, 1 },
    { "conditional_branch", 111, 1, 0, "flow", "Bedingte Verzweigung", "Conditional Branch", 0, "Wertet eine Bedingung aus und führt einen Kinderblock aus.", "conditional_branch", EC_BLOCKS_1, 2, EC_FIELDS_7, 16 },
    { "loop", 112, 1, 0, "flow", "Schleife", "Loop", 1, "Wiederholt seinen Kinderblock bis Break, Exit oder Jump.", "loop", EC_BLOCKS_2, 1, nullptr, 0 },
    { "break_loop", 113, 1, 0, "flow", "Schleife verlassen", "Break Loop", 2, "Verlässt die nächstliegende Schleife.", "break", nullptr, 0, nullptr, 0 },
    { "exit_event_processing", 115, 1, 0, "flow", "Eventverarbeitung beenden", "Exit Event Processing", 3, "Beendet den aktuellen Event-Stack sofort.", "exit_event", nullptr, 0, nullptr, 0 },
    { "erase_event", 116, 1, 0, "flow", "Event löschen", "Erase Event", 4, "Entfernt das ausführende Event und beendet es.", "erase_event", nullptr, 0, nullptr, 0 },
    { "call_common_event", 117, 1, 0, "flow", "Common Event aufrufen", "Call Common Event", 5, "Ruft ein Common Event als Sub-Interpreter auf.", "common_event", nullptr, 0, EC_FIELDS_12, 1 },
    { "label", 118, 1, 0, "flow", "Sprungmarke setzen", "Label", 6, "Definiert eine Sprungmarke im aktuellen Programm.", "label", nullptr, 0, EC_FIELDS_13, 1 },
    { "jump_to_label", 119, 1, 0, "flow", "Zu Sprungmarke springen", "Jump to Label", 7, "Springt zu einer Sprungmarke im Programm.", "jump", nullptr, 0, EC_FIELDS_14, 1 },
    { "control_switches", 121, 1, 1, "state", "Schalter steuern", "Control Switches", 0, "Setzt einen Bereich globaler Schalter auf ON/OFF.", "switch", nullptr, 0, EC_FIELDS_15, 3 },
    { "control_variables", 122, 1, 1, "state", "Variablen steuern", "Control Variables", 1, "Verändert einen Bereich von Variablen aus einer typisierten Quelle.", "variable", nullptr, 0, EC_FIELDS_16, 12 },
    { "control_self_switch", 123, 1, 1, "state", "Lokalen Schalter steuern", "Control Self Switch", 2, "Setzt einen lokalen Schalter des ausführenden Events.", "self_switch", nullptr, 0, EC_FIELDS_17, 2 },
    { "control_timer", 124, 1, 1, "state", "Timer steuern", "Control Timer", 3, "Verändert den laufenden Timer.", "timer", nullptr, 0, EC_FIELDS_18, 2 },
    { "change_gold", 125, 1, 1, "state", "Gold ändern", "Change Gold", 4, "Erhöht oder verringert das Gold der Party.", "gold", nullptr, 0, EC_FIELDS_19, 2 },
    { "change_items", 126, 1, 1, "state", "Items ändern", "Change Items", 5, "Verändert den Bestand in items.", "item", nullptr, 0, EC_FIELDS_20, 3 },
    { "change_weapons", 127, 1, 1, "state", "Waffen ändern", "Change Weapons", 6, "Verändert den Bestand in weapons.", "change_weapons", nullptr, 0, EC_FIELDS_21, 4 },
    { "change_armors", 128, 1, 1, "state", "Rüstungen ändern", "Change Armor", 7, "Verändert den Bestand in armors.", "change_armors", nullptr, 0, EC_FIELDS_22, 4 },
    { "change_party_member", 129, 1, 1, "state", "Partymitglied ändern", "Change Party Member", 8, "Nimmt einen Charakter in die Party auf oder entfernt ihn; optional initialisiert.", "party_member", nullptr, 0, EC_FIELDS_23, 3 },
    { "change_windowskin", 131, 1, 1, "system", "Fenstergrafik ändern", "Change Windowskin", 0, "Wechselt das Windowskin zur Laufzeit.", "change_windowskin", nullptr, 0, EC_FIELDS_24, 1 },
    { "change_battle_bgm", 132, 1, 1, "system", "Kampf-BGM ändern", "Change Battle BGM", 1, "Setzt die Kampf-BGM.", "change_battle_bgm", nullptr, 0, EC_FIELDS_25, 1 },
    { "change_battle_end_me", 133, 1, 1, "system", "Kampf-End-ME ändern", "Change Battle End ME", 2, "Setzt das Kampf-Ende-ME.", "change_battle_end_me", nullptr, 0, EC_FIELDS_26, 1 },
    { "change_save_access", 134, 1, 1, "system", "Speichern erlauben/sperren", "Change Save Access", 3, "Erlaubt oder sperrt das Speichern.", "save_access", nullptr, 0, EC_FIELDS_27, 1 },
    { "change_menu_access", 135, 1, 1, "system", "Menü erlauben/sperren", "Change Menu Access", 4, "Erlaubt oder sperrt das Menü.", "menu_access", nullptr, 0, EC_FIELDS_28, 1 },
    { "change_encounter", 136, 1, 1, "system", "Begegnungen erlauben/sperren", "Change Encounter", 5, "Schaltet Zufallskämpfe ein oder aus.", "encounter_access", nullptr, 0, EC_FIELDS_29, 1 },
    { "transfer_player", 201, 2, 0, "map", "Spieler versetzen", "Transfer Player", 0, "Versetzt den Spieler auf eine Karte und Position.", "teleport", nullptr, 0, EC_FIELDS_30, 5 },
    { "set_event_location", 202, 2, 0, "map", "Event-Position setzen", "Set Event Location", 1, "Setzt die Position eines Events oder des Spielers.", "move_player", nullptr, 0, EC_FIELDS_31, 4 },
    { "scroll_map", 203, 2, 0, "map", "Karte scrollen", "Scroll Map", 2, "Scrollt die Kartenansicht.", "scroll_map", nullptr, 0, EC_FIELDS_32, 3 },
    { "change_map_settings", 204, 2, 0, "map", "Karteneinstellungen ändern", "Change Map Settings", 3, "Wechselt Panorama, Nebel oder Kampf-Hintergrund.", "change_map_settings", nullptr, 0, EC_FIELDS_33, 3 },
    { "change_fog_color_tone", 205, 2, 0, "map", "Nebel-Farbton ändern", "Change Fog Color Tone", 4, "Verändert Nebelfarbton und Graustufe.", "change_fog_color_tone", nullptr, 0, EC_FIELDS_34, 1 },
    { "change_fog_opacity", 206, 2, 0, "map", "Nebel-Deckkraft ändern", "Change Fog Opacity", 5, "Verändert die Nebeldeckkraft.", "change_fog_opacity", nullptr, 0, EC_FIELDS_35, 1 },
    { "show_animation", 207, 2, 0, "map", "Animation zeigen", "Show Animation", 6, "Spielt eine Animation an einem Charakter ab.", "show_animation", nullptr, 0, EC_FIELDS_36, 3 },
    { "change_transparent_flag", 208, 2, 0, "map", "Transparenz setzen", "Change Transparent Flag", 7, "Setzt das Transparenz-Flag des Spielers.", "change_transparent_flag", nullptr, 0, EC_FIELDS_37, 1 },
    { "set_move_route", 209, 2, 0, "map", "Bewegungsroute setzen", "Set Move Route", 8, "Setzt eine typisierte Bewegungsroute mit Repeat/Ignore.", "move_route", nullptr, 0, EC_FIELDS_38, 6 },
    { "wait_for_move_completion", 210, 2, 0, "map", "Auf Bewegung warten", "Wait for Move's Completion", 9, "Wartet, bis alle Routen beendet sind.", "wait_for_move_completion", nullptr, 0, nullptr, 0 },
    { "prepare_transition", 221, 2, 0, "screen", "Übergang vorbereiten", "Prepare for Transition", 0, "Bereitet einen Übergang vor.", "prepare_transition", nullptr, 0, EC_FIELDS_40, 1 },
    { "execute_transition", 222, 2, 0, "screen", "Übergang ausführen", "Execute Transition", 1, "Führt einen vorbereiteten Übergang aus.", "fade", nullptr, 0, EC_FIELDS_41, 1 },
    { "change_screen_tone", 223, 2, 0, "screen", "Bildschirm-Farbton ändern", "Change Screen Color Tone", 2, "Verändert den Bildschirm-Farbton.", "screen_tone", nullptr, 0, EC_FIELDS_42, 1 },
    { "screen_flash", 224, 2, 0, "screen", "Bildschirm-Blitz", "Screen Flash", 3, "Blitzt den Bildschirm.", "flash", nullptr, 0, EC_FIELDS_43, 3 },
    { "screen_shake", 225, 2, 0, "screen", "Bildschirm wackeln", "Screen Shake", 4, "Erschüttert den Bildschirm.", "shake", nullptr, 0, EC_FIELDS_44, 4 },
    { "show_picture", 231, 2, 1, "picture", "Bild zeigen", "Show Picture", 0, "Zeigt ein Bild an oder ersetzt es.", "show_picture", nullptr, 0, EC_FIELDS_45, 8 },
    { "move_picture", 232, 2, 1, "picture", "Bild bewegen", "Move Picture", 1, "Bewegt, skaliert und blendet ein Bild.", "move_picture", nullptr, 0, EC_FIELDS_46, 10 },
    { "rotate_picture", 233, 2, 1, "picture", "Bild drehen", "Rotate Picture", 2, "Dreht ein Bild.", "rotate_picture", nullptr, 0, EC_FIELDS_47, 5 },
    { "change_picture_tone", 234, 2, 1, "picture", "Bild-Farbton ändern", "Change Picture Color Tone", 3, "Färbt ein Bild.", "tint_picture", nullptr, 0, EC_FIELDS_48, 5 },
    { "erase_picture", 235, 2, 1, "picture", "Bild löschen", "Erase Picture", 4, "Löscht ein Bild über Slot-ID.", "erase_picture", nullptr, 0, EC_FIELDS_49, 2 },
    { "set_weather_effects", 236, 2, 1, "picture", "Wetter setzen", "Set Weather Effects", 5, "Stellt Wetterart, Stärke und Dauer ein.", "weather", nullptr, 0, EC_FIELDS_50, 3 },
    { "play_bgm", 241, 2, 1, "audio", "BGM abspielen", "Play BGM", 0, "Startet eine Hintergrundmusik.", "play_bgm", nullptr, 0, EC_FIELDS_51, 3 },
    { "fade_out_bgm", 242, 2, 1, "audio", "BGM ausblenden", "Fade Out BGM", 1, "Blendet die Hintergrundmusik aus.", "fadeout_bgm", nullptr, 0, EC_FIELDS_52, 2 },
    { "play_bgs", 245, 2, 1, "audio", "BGS abspielen", "Play BGS", 2, "Startet ein Hintergrundgeräusch.", "play_bgs", nullptr, 0, EC_FIELDS_53, 3 },
    { "fade_out_bgs", 246, 2, 1, "audio", "BGS ausblenden", "Fade Out BGS", 3, "Blendet das Hintergrundgeräusch aus.", "fadeout_bgs", nullptr, 0, EC_FIELDS_54, 2 },
    { "memorize_bgm_bgs", 247, 2, 1, "audio", "BGM/BGS merken", "Memorize BGM/BGS", 4, "Merkt sich BGM und BGS.", "memorize_audio", nullptr, 0, nullptr, 0 },
    { "restore_bgm_bgs", 248, 2, 1, "audio", "BGM/BGS wiederherstellen", "Restore BGM/BGS", 5, "Stellt gemerkte BGM und BGS wieder her.", "restore_audio", nullptr, 0, nullptr, 0 },
    { "play_me", 249, 2, 1, "audio", "ME abspielen", "Play ME", 6, "Spielt ein Music-Effect ab.", "sound", nullptr, 0, EC_FIELDS_57, 3 },
    { "play_se", 250, 2, 1, "audio", "SE abspielen", "Play SE", 7, "Spielt einen Sound-Effect ab.", "sound", nullptr, 0, EC_FIELDS_58, 3 },
    { "stop_se", 251, 2, 1, "audio", "SE stoppen", "Stop SE", 8, "Stoppt einen Sound-Effect.", "stop_se", nullptr, 0, EC_FIELDS_59, 1 },
    { "battle_processing", 301, 3, 0, "battle", "Kampf verarbeiten", "Battle Processing", 0, "Startet einen Kampf und verzweigt nach dem Ergebnis.", "battle", EC_BLOCKS_3, 3, EC_FIELDS_60, 3 },
    { "shop_processing", 302, 3, 0, "battle", "Laden verarbeiten", "Shop Processing", 1, "Öffnet einen Laden mit eigener Warenliste.", "shop", nullptr, 0, EC_FIELDS_61, 2 },
    { "name_input_processing", 303, 3, 0, "battle", "Namenseingabe", "Name Input Processing", 2, "Fragt einen neuen Charakternamen ab.", "name_input", nullptr, 0, EC_FIELDS_62, 2 },
    { "change_hp", 311, 3, 0, "actor", "HP ändern", "Change HP", 0, "Verändert die aktuellen HP eines Charakters.", "actor_hp", nullptr, 0, EC_FIELDS_63, 4 },
    { "change_sp", 312, 3, 0, "actor", "SP ändern", "Change SP", 1, "Verändert die aktuellen SP eines Charakters.", "actor_sp", nullptr, 0, EC_FIELDS_64, 3 },
    { "change_state", 313, 3, 0, "actor", "Zustand ändern", "Change State", 2, "Setzt oder entfernt einen Zustand.", "state", nullptr, 0, EC_FIELDS_65, 3 },
    { "recover_all", 314, 3, 0, "actor", "Vollständig heilen", "Recover All", 3, "Heilt einen Charakter vollständig.", "heal", nullptr, 0, EC_FIELDS_66, 1 },
    { "change_exp", 315, 3, 0, "actor", "Erfahrung ändern", "Change EXP", 4, "Verändert die EXP eines Charakters.", "actor_exp", nullptr, 0, EC_FIELDS_67, 3 },
    { "change_level", 316, 3, 0, "actor", "Stufe ändern", "Change Level", 5, "Verändert das Level eines Charakters.", "actor_level", nullptr, 0, EC_FIELDS_68, 3 },
    { "change_parameters", 317, 3, 0, "actor", "Parameter ändern", "Change Parameters", 6, "Verändert einen Kampfparameter eines Charakters.", "actor_param", nullptr, 0, EC_FIELDS_69, 4 },
    { "change_skills", 318, 3, 0, "actor", "Fertigkeiten ändern", "Change Skills", 7, "Lehrt oder vergisst eine Fähigkeit.", "actor_skill", nullptr, 0, EC_FIELDS_70, 3 },
    { "change_equipment", 319, 3, 0, "actor", "Ausrüstung ändern", "Change Equipment", 8, "Wechselt einen Ausrüstungsslot.", "change_equipment", nullptr, 0, EC_FIELDS_71, 3 },
    { "change_actor_name", 320, 3, 0, "actor", "Heldennamen ändern", "Change Actor Name", 9, "Ändert den Namen eines Charakters.", "actor_name", nullptr, 0, EC_FIELDS_72, 2 },
    { "change_actor_class", 321, 3, 0, "actor", "Klasse ändern", "Change Actor Class", 10, "Wechselt die Klasse eines Charakters.", "change_actor_class", nullptr, 0, EC_FIELDS_73, 2 },
    { "change_actor_graphic", 322, 3, 0, "actor", "Grafik ändern", "Change Actor Graphic", 11, "Wechselt die Grafiken eines Charakters.", "change_actor_graphic", nullptr, 0, EC_FIELDS_74, 4 },
    { "change_enemy_hp", 331, 3, 1, "enemy", "Gegner-HP ändern", "Change Enemy HP", 0, "Verändert die HP eines Gegners.", "change_enemy_hp", nullptr, 0, EC_FIELDS_75, 4 },
    { "change_enemy_sp", 332, 3, 1, "enemy", "Gegner-SP ändern", "Change Enemy SP", 1, "Verändert die SP eines Gegners.", "change_enemy_sp", nullptr, 0, EC_FIELDS_76, 3 },
    { "change_enemy_state", 333, 3, 1, "enemy", "Gegner-Zustand ändern", "Change Enemy State", 2, "Setzt oder entfernt einen Gegnerzustand.", "change_enemy_state", nullptr, 0, EC_FIELDS_77, 3 },
    { "enemy_recover_all", 334, 3, 1, "enemy", "Gegner vollständig heilen", "Enemy Recover All", 3, "Heilt einen Gegner vollständig.", "enemy_recover_all", nullptr, 0, EC_FIELDS_78, 1 },
    { "enemy_appearance", 335, 3, 1, "enemy", "Gegner erscheinen lassen", "Enemy Appearance", 4, "Macht einen Gegner sichtbar.", "enemy_appearance", nullptr, 0, EC_FIELDS_79, 1 },
    { "enemy_transform", 336, 3, 1, "enemy", "Gegner verwandeln", "Enemy Transform", 5, "Verwandelt einen Gegner.", "enemy_transform", nullptr, 0, EC_FIELDS_80, 2 },
    { "show_battle_animation", 337, 3, 1, "enemy", "Kampfanimation zeigen", "Show Battle Animation", 6, "Zeigt eine Animation im Kampf.", "show_battle_animation", nullptr, 0, EC_FIELDS_81, 2 },
    { "deal_damage", 338, 3, 1, "enemy", "Schaden austeilen", "Deal Damage", 7, "Fügt einem Gegner direkten Schaden zu.", "deal_damage", nullptr, 0, EC_FIELDS_82, 3 },
    { "force_action", 339, 3, 1, "enemy", "Aktion erzwingen", "Force Action", 8, "Erzwingt eine gegnerische Aktion.", "force_action", nullptr, 0, EC_FIELDS_83, 3 },
    { "abort_battle", 340, 3, 1, "enemy", "Kampf abbrechen", "Abort Battle", 9, "Bricht den laufenden Kampf ab.", "abort_battle", nullptr, 0, nullptr, 0 },
    { "call_menu_screen", 351, 3, 1, "system", "Menü aufrufen", "Call Menu Screen", 0, "Öffnet das Menü.", "menu", nullptr, 0, nullptr, 0 },
    { "call_save_screen", 352, 3, 1, "system", "Speichern-Bildschirm aufrufen", "Call Save Screen", 1, "Öffnet den Speicher-Dialog.", "save_game", nullptr, 0, nullptr, 0 },
    { "game_over", 353, 3, 1, "system", "Game Over", "Game Over", 2, "Zeigt Game Over.", "game_over", nullptr, 0, nullptr, 0 },
    { "return_to_title_screen", 354, 3, 1, "system", "Zum Titel zurück", "Return to Title Screen", 3, "Kehrt zum Titelbild zurück.", "return_to_title_screen", nullptr, 0, nullptr, 0 },
    { "script", 355, 3, 1, "system", "Script-Hook", "Script", 4, "Ruft einen entkoppelten Script-Hook oder Ausdruck auf.", "script", nullptr, 0, EC_FIELDS_89, 3 },
};

static const EcTabInfo EC_TABS[EC_TAB_COUNT] = {
    { 1, "Dialog, Flusskontrolle, Variablen & System" },
    { 2, "Karte, Bewegung, Bilder, Screen & Audio" },
    { 3, "Kampf, Charakterwerte, Gegner, Screens & Script" },
};

static const EcSectionInfo EC_SECTIONS[EC_SECTION_COUNT] = {
    { "dialog", "Dialog & Eingabe", 1, 0 },
    { "flow", "Flusskontrolle", 1, 0 },
    { "state", "Zustand & Inventar", 1, 1 },
    { "system", "Laufzeit-System", 1, 1 },
    { "map", "Karte & Bewegung", 2, 0 },
    { "screen", "Übergang & Bildschirm", 2, 0 },
    { "picture", "Bilder & Wetter", 2, 1 },
    { "audio", "Audio", 2, 1 },
    { "battle", "Kampf & Interaktion", 3, 0 },
    { "actor", "Party & Charaktere", 3, 0 },
    { "enemy", "Gegner & Kampfsteuerung", 3, 1 },
    { "system", "System-Screens & Hook", 3, 1 },
};

inline int ec_command_count() { return EC_COMMAND_COUNT; }
inline const EcCommand& ec_command(int index) { return EC_COMMANDS[index]; }

inline const EcCommand* ec_find(const char* type) {
    for (int i = 0; i < EC_COMMAND_COUNT; ++i)
        if (std::strcmp(EC_COMMANDS[i].type, type) == 0) return &EC_COMMANDS[i];
    return nullptr;
}

inline const EcTabInfo& ec_tab(int number) { return EC_TABS[number - 1]; }

inline const EcSectionInfo* ec_section(int tab, int column, const char* key) {
    for (int i = 0; i < EC_SECTION_COUNT; ++i)
        if (EC_SECTIONS[i].tab == tab && EC_SECTIONS[i].column == column &&
            std::strcmp(EC_SECTIONS[i].key, key) == 0)
            return &EC_SECTIONS[i];
    return nullptr;
}

// Indices der Befehle eines Abschnitts, in XP-Reihenfolge.
inline int ec_section_commands(int tab, int column, const char* section, int* out, int max) {
    int n = 0;
    for (int i = 0; i < EC_COMMAND_COUNT && n < max; ++i) {
        const EcCommand& c = EC_COMMANDS[i];
        if (c.tab == tab && c.column == column && std::strcmp(c.section, section) == 0)
            out[n++] = i;
    }
    return n;
}

inline const char* ec_column_name(int column) { return column == 0 ? "left" : "right"; }

// Wie viele Befehle eine Spalte eines Tabs hat (fuer die Geometrie).
inline int ec_column_count(int tab, int column) {
    int n = 0;
    for (int i = 0; i < EC_COMMAND_COUNT; ++i)
        if (EC_COMMANDS[i].tab == tab && EC_COMMANDS[i].column == column) ++n;
    return n;
}

} // namespace rune::editor
