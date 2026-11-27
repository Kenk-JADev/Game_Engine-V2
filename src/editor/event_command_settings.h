#pragma once
// ============================================================================
//  Etappe 45: Einstell-Schema je Event-Befehl für das XP-Einstellfenster.
//
//  Beschreibt portabel, WELCHE Felder ein Befehl hat und mit WELCHEM Bedienelement
//  sie eingestellt werden — wie in RPG Maker XP ein eigenes Fenster je Befehl.
//  Die Zuordnung folgt dem laufenden Karten-Event-System (event_command_keys in
//  event_form.h), NICHT dem typisierten Rpg-Katalog: nur so bleiben geschriebene
//  Befehle für die echte Engine lesbar. Die Serialisierung übernimmt weiterhin
//  das getestete event_command_prepare (verlustfreies Mergen, Validierung).
//
//  Slots entsprechen EventCommandForm:
//    0=name 1=val1 2=val2 3=val3 4=text 5=op 6=flag 7=source
//
//  Bedienelemente:
//    NumberedSwitch/NumberedVariable = nummerierte Liste 1..999 (kein Freitext)
//    Ref      = Auswahl aus einer Datenbank-Sammlung (items, actors, troops, ...)
//    Asset    = Auswahl + Ordner-Durchsuchen (bgms, bgss, ses, pictures, faces, ...)
//    Combo    = feste Auswahl, Bool = An/Aus, Int/Number = geprüfte Zahl,
//    Text     = einzeilig, Multiline = mehrzeilig, Hidden = nicht anzeigen.
//
//  Ohne Win32; kopflos geprüft in tools/event_command_settings_test.cpp.
// ============================================================================

#include "editor/event_form.h"

#include <cctype>
#include <cstring>
#include <string>

namespace rune::editor {

enum class EcSetControl {
    Hidden, Text, Multiline, Int, Number, Bool, Combo,
    NumberedSwitch, NumberedVariable, Ref, Asset
};

struct EcSetField {
    int slot = 0;
    const char* label = "";
    EcSetControl control = EcSetControl::Hidden;
    const char* collection = nullptr;   // Ref/Asset-Sammlung
    const char* const* options = nullptr;
    int option_count = 0;
};

constexpr int EC_SET_MAX_FIELDS = 8;

// Feste Auswahlen (Combos) für häufige Befehle.
static const char* const EC_SET_WEATHER[] = { "none", "rain", "storm", "snow" };
static const char* const EC_SET_EFFECT[] = { "flash", "shake" };
static const char* const EC_SET_MODE[] = { "add", "remove" };
static const char* const EC_SET_DIRECTION[] = { "out", "in" };
static const char* const EC_SET_POSITION[] = { "top", "middle", "bottom" };
static const char* const EC_SET_FRAME[] = { "window", "dim", "transparent" };
static const char* const EC_SET_TARGET[] = { "player", "this_event", "event" };
static const char* const EC_SET_PARAM[] = { "max_hp", "max_sp", "strength", "dexterity", "agility", "intelligence" };
static const char* const EC_SET_LETTER[] = { "A", "B", "C", "D" };
static const char* const EC_SET_TIMER[] = { "start", "stop" };

inline EcSetField ec_set(int slot, const char* label, EcSetControl c,
                         const char* collection = nullptr,
                         const char* const* options = nullptr, int n = 0) {
    EcSetField f;
    f.slot = slot; f.label = label; f.control = c;
    f.collection = collection; f.options = options; f.option_count = n;
    return f;
}

// Liefert die Felder eines Typs in Slot-Reihenfolge. Rückgabe = Anzahl.
inline int ec_settings_fields(int type, EcSetField* out, int max) {
    int n = 0;
    auto add = [&](EcSetField f) { if (n < max) out[n++] = f; };
    using C = EcSetControl;
    switch (type) {
        case ET_TEXT:
            add(ec_set(4, "Text (je Zeile eine)", C::Multiline));
            add(ec_set(0, "Sprecher (optional)", C::Text));
            break;
        case ET_LOG:
            add(ec_set(4, "Log-Text", C::Multiline));
            break;
        case ET_CHOICES:
            add(ec_set(4, "Optionen (je Zeile eine)", C::Multiline));
            add(ec_set(1, "Abbruch-Option (-1 = aus)", C::Int));
            break;
        case ET_WAIT:
            add(ec_set(1, "Sekunden", C::Number));
            break;
        case ET_SWITCH:
            add(ec_set(0, "Schalter (Nummer wählen)", C::NumberedSwitch, "switches"));
            add(ec_set(6, "Wert", C::Bool));
            break;
        case ET_TOGGLE_SWITCH:
            add(ec_set(0, "Schalter (Nummer wählen)", C::NumberedSwitch, "switches"));
            break;
        case ET_IF_SWITCH:
            add(ec_set(0, "Schalter (Nummer wählen)", C::NumberedSwitch, "switches"));
            add(ec_set(6, "Wert", C::Bool));
            break;
        case ET_VARIABLE:
            add(ec_set(0, "Variable (Nummer wählen)", C::NumberedVariable, "variables"));
            add(ec_set(5, "Operation", C::Combo));
            add(ec_set(7, "Quelle", C::Combo));
            add(ec_set(1, "Wert", C::Number));
            break;
        case ET_IF_VARIABLE:
            add(ec_set(0, "Variable (Nummer wählen)", C::NumberedVariable, "variables"));
            add(ec_set(5, "Vergleich", C::Combo));
            add(ec_set(1, "Wert", C::Number));
            break;
        case ET_INPUT_NUMBER:
            add(ec_set(0, "Variable (Nummer wählen)", C::NumberedVariable, "variables"));
            add(ec_set(1, "Ziffern", C::Int));
            add(ec_set(2, "Startwert", C::Int));
            break;
        case ET_BUTTON_INPUT:
            add(ec_set(0, "Variable (Nummer wählen)", C::NumberedVariable, "variables"));
            add(ec_set(4, "Tasten (Komma-getrennt)", C::Text));
            break;
        case ET_SELF_SWITCH:
            add(ec_set(0, "Buchstabe", C::Combo, nullptr, EC_SET_LETTER, 4));
            add(ec_set(6, "Wert", C::Bool));
            break;
        case ET_IF_SELF_SWITCH:
            add(ec_set(0, "Buchstabe", C::Combo, nullptr, EC_SET_LETTER, 4));
            add(ec_set(6, "Wert", C::Bool));
            break;
        case ET_ITEM:
            add(ec_set(0, "Item (aus Datenbank)", C::Ref, "items"));
            add(ec_set(1, "Anzahl", C::Int));
            add(ec_set(6, "Entnehmen statt geben", C::Bool));
            break;
        case ET_IF_ITEM:
            add(ec_set(0, "Item (aus Datenbank)", C::Ref, "items"));
            add(ec_set(5, "Vergleich", C::Combo));
            add(ec_set(1, "Wert", C::Int));
            break;
        case ET_GOLD:
            add(ec_set(1, "Betrag (negativ = abziehen)", C::Int));
            break;
        case ET_IF_GOLD:
            add(ec_set(5, "Vergleich", C::Combo));
            add(ec_set(1, "Wert", C::Int));
            break;
        case ET_HEAL:
            add(ec_set(1, "Betrag", C::Int));
            break;
        case ET_PARTY_MEMBER:
            add(ec_set(0, "Held (aus Datenbank)", C::Ref, "actors"));
            add(ec_set(6, "Entfernen statt hinzufügen", C::Bool));
            break;
        case ET_IF_ACTOR:
            add(ec_set(0, "Held (aus Datenbank)", C::Ref, "actors"));
            break;
        case ET_STATE:
            add(ec_set(0, "Zustand (aus Datenbank)", C::Ref, "states"));
            add(ec_set(1, "Held (aus Datenbank)", C::Ref, "actors"));
            add(ec_set(2, "Modus", C::Combo, nullptr, EC_SET_MODE, 2));
            break;
        case ET_ACTOR_SKILL:
            add(ec_set(0, "Fertigkeit (aus Datenbank)", C::Ref, "skills"));
            add(ec_set(1, "Held (aus Datenbank)", C::Ref, "actors"));
            add(ec_set(2, "Modus", C::Combo, nullptr, EC_SET_MODE, 2));
            break;
        case ET_ACTOR_NAME:
            add(ec_set(1, "Held (aus Datenbank)", C::Ref, "actors"));
            add(ec_set(0, "Neuer Name", C::Text));
            break;
        case ET_ACTOR_HP: case ET_ACTOR_SP: case ET_ACTOR_EXP: case ET_ACTOR_LEVEL:
            add(ec_set(1, "Held (aus Datenbank)", C::Ref, "actors"));
            add(ec_set(2, "Betrag", C::Int));
            add(ec_set(7, "Quelle", C::Combo));
            break;
        case ET_ACTOR_PARAM:
            add(ec_set(0, "Parameter", C::Combo, nullptr, EC_SET_PARAM, 6));
            add(ec_set(1, "Held (aus Datenbank)", C::Ref, "actors"));
            add(ec_set(2, "Betrag", C::Int));
            add(ec_set(7, "Quelle", C::Combo));
            break;
        case ET_BATTLE:
            add(ec_set(0, "Truppe (aus Datenbank)", C::Ref, "troops"));
            add(ec_set(6, "Flucht erlaubt", C::Bool));
            break;
        case ET_COMMON_EVENT:
            add(ec_set(0, "Gem. Event (aus Datenbank)", C::Ref, "common_events"));
            break;
        case ET_SOUND:
            add(ec_set(0, "SE (Ordner wählen)", C::Asset, "ses"));
            add(ec_set(1, "Lautstärke (0-100)", C::Int));
            break;
        case ET_PLAY_BGM:
            add(ec_set(0, "BGM (Ordner wählen)", C::Asset, "bgms"));
            add(ec_set(1, "Lautstärke (0-100)", C::Int));
            add(ec_set(2, "Pitch (50-150)", C::Int));
            break;
        case ET_PLAY_BGS:
            add(ec_set(0, "BGS (Ordner wählen)", C::Asset, "bgss"));
            add(ec_set(1, "Lautstärke (0-100)", C::Int));
            add(ec_set(2, "Pitch (50-150)", C::Int));
            break;
        case ET_FADEOUT_BGM: case ET_FADEOUT_BGS:
            add(ec_set(1, "Sekunden", C::Number));
            break;
        case ET_WEATHER:
            add(ec_set(0, "Wetter", C::Combo, nullptr, EC_SET_WEATHER, 4));
            add(ec_set(1, "Stärke (0-10)", C::Int));
            break;
        case ET_EFFECT:
            add(ec_set(0, "Effekt", C::Combo, nullptr, EC_SET_EFFECT, 2));
            add(ec_set(1, "Stärke/Höhe", C::Number));
            break;
        case ET_FADE:
            add(ec_set(0, "Richtung", C::Combo, nullptr, EC_SET_DIRECTION, 2));
            add(ec_set(1, "Sekunden", C::Number));
            break;
        case ET_TIMER:
            add(ec_set(0, "Modus", C::Combo, nullptr, EC_SET_TIMER, 2));
            add(ec_set(1, "Sekunden", C::Number));
            break;
        case ET_TEXT_OPTIONS:
            add(ec_set(0, "Position", C::Combo, nullptr, EC_SET_POSITION, 3));
            add(ec_set(1, "Rahmen", C::Combo, nullptr, EC_SET_FRAME, 3));
            add(ec_set(6, "Transparent", C::Bool));
            break;
        case ET_MOVE_ROUTE:
            add(ec_set(0, "Ziel", C::Combo, nullptr, EC_SET_TARGET, 3));
            add(ec_set(4, "Schritte (je Zeile eine)", C::Multiline));
            add(ec_set(1, "Tempo", C::Int));
            add(ec_set(6, "Auf Ende warten", C::Bool));
            break;
        case ET_MOVE_PLAYER:
            add(ec_set(1, "X", C::Int));
            add(ec_set(2, "Z", C::Int));
            add(ec_set(6, "Relativ", C::Bool));
            break;
        case ET_TELEPORT:
            add(ec_set(1, "X", C::Int));
            add(ec_set(2, "Z", C::Int));
            add(ec_set(3, "Yaw (Radiant)", C::Number));
            break;
        case ET_SHOP:
            add(ec_set(4, "Waren (je Zeile: Item,Preis)", C::Multiline));
            break;
        case ET_NAME_INPUT:
            add(ec_set(1, "Held (aus Datenbank)", C::Ref, "actors"));
            add(ec_set(2, "Max. Zeichen", C::Int));
            break;
        case ET_SCREEN_TONE:
            add(ec_set(1, "Rot (-255..255)", C::Int));
            add(ec_set(2, "Grün (-255..255)", C::Int));
            add(ec_set(3, "Blau (-255..255)", C::Int));
            break;
        case ET_FLASH:
            add(ec_set(0, "Farbe (Name)", C::Text));
            add(ec_set(1, "Stärke", C::Int));
            add(ec_set(2, "Frames", C::Int));
            break;
        case ET_SAVE_GAME: case ET_LOAD_GAME:
            add(ec_set(1, "Slot", C::Int));
            break;
        case ET_SAVE_ACCESS: case ET_MENU_ACCESS: case ET_ENCOUNTER_ACCESS:
            add(ec_set(6, "Erlaubt", C::Bool));
            break;
        case ET_LABEL: case ET_JUMP:
            add(ec_set(0, "Marke", C::Text));
            break;
        case ET_SCRIPT: case ET_IF_SCRIPT:
            add(ec_set(4, "Script-Code", C::Multiline));
            break;
        default:
            // GenerischerFallback: alle nicht-leeren Slots sinnvoll bedienen.
            add(ec_set(0, "Name / Schlüssel", C::Text));
            add(ec_set(1, "Wert 1", C::Number));
            add(ec_set(2, "Wert 2", C::Number));
            add(ec_set(3, "Wert 3", C::Number));
            add(ec_set(4, "Text", C::Multiline));
            break;
    }
    return n;
}

// ---- Asset-Ordner ----------------------------------------------------------
// Sammlung -> Unterordner im Projekt und erlaubte Endungen. Der Editor listet
// diese Dateien und schreibt den relativen Schlüssel (z. B. "bgm/dorf.ogg").
inline const char* ec_asset_subdir(const char* collection) {
    if (!collection) return "";
    if (!std::strcmp(collection, "bgms")) return "audio\\bgm";
    if (!std::strcmp(collection, "bgss")) return "audio\\bgs";
    if (!std::strcmp(collection, "ses")) return "audio\\se";
    if (!std::strcmp(collection, "pictures")) return "pictures";
    if (!std::strcmp(collection, "faces")) return "pictures\\faces";
    if (!std::strcmp(collection, "characters")) return "pictures\\characters";
    if (!std::strcmp(collection, "animations")) return "pictures\\animations";
    if (!std::strcmp(collection, "battlers")) return "pictures\\battlers";
    if (!std::strcmp(collection, "windowskins")) return "pictures\\windowskins";
    if (!std::strcmp(collection, "transitions")) return "pictures\\transitions";
    if (!std::strcmp(collection, "panoramas")) return "pictures\\panoramas";
    if (!std::strcmp(collection, "fogs")) return "pictures\\fogs";
    if (!std::strcmp(collection, "battlebacks")) return "pictures\\battlebacks";
    if (!std::strcmp(collection, "tilesets")) return "pictures\\tilesets";
    return "";
}

inline bool ec_asset_is_audio(const char* collection) {
    return collection && (!std::strcmp(collection, "bgms") ||
                          !std::strcmp(collection, "bgss") || !std::strcmp(collection, "ses"));
}

// Endungsfilter (case-insensitiv). Audio: ogg/mp3/wav/mid; Bild: png/jpg/bmp.
inline bool ec_asset_matches(const char* collection, const std::string& filename) {
    const size_t dot = filename.find_last_of('.');
    if (dot == std::string::npos) return false;
    std::string ext = filename.substr(dot + 1);
    for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (ec_asset_is_audio(collection))
        return ext == "ogg" || ext == "mp3" || ext == "wav" || ext == "mid" || ext == "midi";
    return ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "bmp";
}

// Relativer Schlüssel aus Unterordner + Dateiname (Slash, wie XP/Engine).
inline std::string ec_asset_key(const char* collection, const std::string& filename) {
    std::string sub = ec_asset_subdir(collection);
    for (char& c : sub) if (c == '\\') c = '/';
    if (sub.empty()) return filename;
    return sub + "/" + filename;
}

} // namespace rune::editor
