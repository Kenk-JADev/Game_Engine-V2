// ============================================================================
//  event_command_prepare48_test.cpp — Etappe 48: die 30 neu an die Engine
//  angebundenen XP-Befehle im Editor-Formular (event_command_prepare).
//
//  Prüft kopflos (ohne Win32), dass ein frischer Befehl jedes neuen Typs
//  gültig serialisiert und genau die JSON-Schlüssel schreibt, die EventRuntime/
//  MapEffects/Party/Battle lesen. Quelle der Wahrheit: game/engine/event_runtime.rb.
//     g++ -std=c++20 -O1 -Wall -Wextra -Isrc src/editor/json.cpp
//         tools/event_command_prepare48_test.cpp -o build/ec_prepare48_test
// ============================================================================

#include "editor/event_form.h"
#include "editor/json.h"

#include <cstdio>
#include <string>

using namespace rune::editor;

namespace {
int g_fail = 0, g_count = 0;
void check(const char* name, bool ok) {
    ++g_count;
    if (!ok) ++g_fail;
    std::printf("  %s %s\n", ok ? "ok  " : "FAIL", name);
}

EventCommandForm form(int type, const char* name, const char* v1, const char* v2,
                      const char* v3, const char* text, int op, int flag) {
    EventCommandForm f;
    f.type = type;
    f.name = name ? name : "";
    f.val1 = v1 ? v1 : "";
    f.val2 = v2 ? v2 : "";
    f.val3 = v3 ? v3 : "";
    f.text = text ? text : "";
    f.op = op; f.flag = flag; f.source = 0;
    return f;
}

// Frischer Befehl (existing=nullptr) -> event_command_prepare erzeugt ein Objekt.
EventEdit insert(const EventCommandForm& f) {
    EventCommandForm before = f;   // bei fresh egal
    return event_command_prepare(nullptr, before, f);
}

bool has_str(const JsonValue& v, const char* key, const char* expect) {
    const JsonValue* f = v.find(key);
    return f && f->is_str() && f->str == expect;
}
bool has_num(const JsonValue& v, const char* key, double expect) {
    const JsonValue* f = v.find(key);
    return f && f->is_num() && f->number() == expect;
}
bool has_bool(const JsonValue& v, const char* key, bool expect) {
    const JsonValue* f = v.find(key);
    return f && f->type == JsonValue::Type::Bool && f->b == expect;
}
} // namespace

int main() {
    std::printf("== XP-Befehle Etappe 48: Editor-Serialisierung ==\n");

    {
        EventEdit e = insert(form(ET_CONDITIONAL_BRANCH, "101", "", "", "", "", 0, 0));
        check("conditional_branch ok", e.ok);
        check("conditional_branch cmd", has_str(e.value, "cmd", "conditional_branch"));
        check("conditional_branch kind=switch", has_str(e.value, "kind", "switch"));
        check("conditional_branch switch_id", has_str(e.value, "switch_id", "101"));
    }
    {
        EventEdit e = insert(form(ET_CHANGE_WEAPONS, "rune_blade", "2", "", "", "", 0, 1));
        check("change_weapons ok", e.ok);
        check("change_weapons weapon_id", has_str(e.value, "weapon_id", "rune_blade"));
        check("change_weapons amount", has_num(e.value, "amount", 2));
        check("change_weapons operation=add", has_str(e.value, "operation", "add"));
    }
    {
        EventEdit e = insert(form(ET_CHANGE_ARMORS, "warded_cloak", "1", "", "", "", 1, 1));
        check("change_armors ok", e.ok);
        check("change_armors operation=subtract", has_str(e.value, "operation", "subtract"));
    }
    {
        EventEdit e = insert(form(ET_CHANGE_WINDOWSKIN, "skin/obsidian", "", "", "", "", 0, 0));
        check("change_windowskin ok", e.ok && has_str(e.value, "skin_key", "skin/obsidian"));
    }
    {
        EventEdit e = insert(form(ET_CHANGE_BATTLE_BGM, "bgm_ancient_citadel", "", "", "", "", 0, 0));
        check("change_battle_bgm ok", e.ok && has_str(e.value, "bgm_key", "bgm_ancient_citadel"));
    }
    {
        EventEdit e = insert(form(ET_CHANGE_BATTLE_END_ME, "se_crystal_chime", "", "", "", "", 0, 0));
        check("change_battle_end_me ok", e.ok && has_str(e.value, "me_key", "se_crystal_chime"));
    }
    {
        EventEdit e = insert(form(ET_SCROLL_MAP, "", "3", "4", "", "", 3, 0));
        check("scroll_map ok", e.ok);
        check("scroll_map direction=right", has_str(e.value, "direction", "right"));
        check("scroll_map distance", has_num(e.value, "distance", 3));
        check("scroll_map speed", has_num(e.value, "speed", 4));
    }
    {
        EventEdit e = insert(form(ET_CHANGE_MAP_SETTINGS, "bb/forest", "", "", "", "", 2, 0));
        check("change_map_settings ok", e.ok);
        check("change_map_settings target=battleback", has_str(e.value, "target", "battleback"));
        check("change_map_settings asset_key", has_str(e.value, "asset_key", "bb/forest"));
    }
    {
        EventEdit e = insert(form(ET_CHANGE_FOG_COLOR_TONE, "", "10", "20", "30", "", 0, 0));
        check("change_fog_color_tone ok", e.ok);
        check("fog r", has_num(e.value, "r", 10));
        check("fog g", has_num(e.value, "g", 20));
        check("fog b", has_num(e.value, "b", 30));
    }
    {
        EventEdit e = insert(form(ET_CHANGE_FOG_OPACITY, "", "120", "", "", "", 0, 0));
        check("change_fog_opacity ok", e.ok && has_num(e.value, "opacity", 120));
    }
    {
        EventEdit e = insert(form(ET_SHOW_ANIMATION, "fx/rune_burst", "", "", "", "", 0, 0));
        check("show_animation ok", e.ok);
        check("show_animation target=player", has_str(e.value, "target", "player"));
        check("show_animation animation_key", has_str(e.value, "animation_key", "fx/rune_burst"));
    }
    {
        EventEdit e = insert(form(ET_CHANGE_TRANSPARENT_FLAG, "", "", "", "", "", 0, 0));
        check("change_transparent_flag ok", e.ok && has_bool(e.value, "transparent", true));
    }
    {
        EventEdit e = insert(form(ET_WAIT_FOR_MOVE_COMPLETION, "", "", "", "", "", 0, 0));
        check("wait_for_move_completion ok", e.ok && has_str(e.value, "cmd", "wait_for_move_completion"));
    }
    {
        EventEdit e = insert(form(ET_PREPARE_TRANSITION, "transition/rune_fade", "", "", "", "", 0, 0));
        check("prepare_transition ok", e.ok && has_str(e.value, "transition_key", "transition/rune_fade"));
    }
    {
        EventEdit e = insert(form(ET_STOP_SE, "se_door", "", "", "", "", 0, 0));
        check("stop_se ok", e.ok && has_str(e.value, "se_key", "se_door"));
    }
    {
        EventEdit e = insert(form(ET_CHANGE_EQUIPMENT, "arik", "rune_blade", "", "", "", 0, 0));
        check("change_equipment ok", e.ok);
        check("change_equipment actor_id", has_str(e.value, "actor_id", "arik"));
        check("change_equipment slot=weapon", has_str(e.value, "slot", "weapon"));
        check("change_equipment item_id", has_str(e.value, "item_id", "rune_blade"));
    }
    {
        EventEdit e = insert(form(ET_CHANGE_ACTOR_CLASS, "arik", "vanguard", "", "", "", 0, 0));
        check("change_actor_class ok", e.ok);
        check("change_actor_class class_id", has_str(e.value, "class_id", "vanguard"));
    }
    {
        EventEdit e = insert(form(ET_CHANGE_ACTOR_GRAPHIC, "arik", "char/kael", "battler/kael", "face/kael", "", 0, 0));
        check("change_actor_graphic ok", e.ok);
        check("graphic character_key", has_str(e.value, "character_key", "char/kael"));
        check("graphic battler_key", has_str(e.value, "battler_key", "battler/kael"));
        check("graphic face_key", has_str(e.value, "face_key", "face/kael"));
    }
    {
        EventEdit e = insert(form(ET_CHANGE_ENEMY_HP, "", "0", "3", "", "", 1, 0));
        check("change_enemy_hp ok", e.ok);
        check("enemy_hp slot", has_num(e.value, "slot", 0));
        check("enemy_hp amount", has_num(e.value, "amount", 3));
        check("enemy_hp operation=subtract", has_str(e.value, "operation", "subtract"));
    }
    {
        EventEdit e = insert(form(ET_CHANGE_ENEMY_SP, "", "1", "5", "", "", 0, 0));
        check("change_enemy_sp ok", e.ok && has_num(e.value, "slot", 1) && has_num(e.value, "amount", 5));
    }
    {
        EventEdit e = insert(form(ET_CHANGE_ENEMY_STATE, "gift", "0", "", "", "", 1, 0));
        check("change_enemy_state ok", e.ok);
        check("enemy_state state_id", has_str(e.value, "state_id", "gift"));
        check("enemy_state operation=remove", has_str(e.value, "operation", "remove"));
    }
    {
        EventEdit e = insert(form(ET_ENEMY_RECOVER_ALL, "", "0", "", "", "", 0, 0));
        check("enemy_recover_all ok", e.ok && has_num(e.value, "slot", 0));
    }
    {
        EventEdit e = insert(form(ET_ENEMY_APPEARANCE, "", "1", "", "", "", 0, 0));
        check("enemy_appearance ok", e.ok && has_num(e.value, "slot", 1));
    }
    {
        EventEdit e = insert(form(ET_ENEMY_TRANSFORM, "wolf", "0", "", "", "", 0, 0));
        check("enemy_transform ok", e.ok);
        check("enemy_transform enemy_id", has_str(e.value, "enemy_id", "wolf"));
    }
    {
        EventEdit e = insert(form(ET_SHOW_BATTLE_ANIMATION, "fx/rune_burst", "-1", "", "", "", 0, 0));
        check("show_battle_animation ok", e.ok && has_str(e.value, "animation_key", "fx/rune_burst"));
        check("show_battle_animation target_slot", has_num(e.value, "target_slot", -1));
    }
    {
        EventEdit e = insert(form(ET_DEAL_DAMAGE, "", "0", "5", "", "", 0, 0));
        check("deal_damage ok", e.ok);
        check("deal_damage target_slot", has_num(e.value, "target_slot", 0));
        check("deal_damage amount", has_num(e.value, "amount", 5));
    }
    {
        EventEdit e = insert(form(ET_FORCE_ACTION, "skill/rune_burst", "0", "-1", "", "", 0, 0));
        check("force_action ok", e.ok);
        check("force_action action_key", has_str(e.value, "action_key", "skill/rune_burst"));
        check("force_action target_slot", has_num(e.value, "target_slot", -1));
    }
    {
        EventEdit e = insert(form(ET_ABORT_BATTLE, "", "", "", "", "", 0, 0));
        check("abort_battle ok", e.ok && has_str(e.value, "cmd", "abort_battle"));
    }
    {
        EventEdit e = insert(form(ET_GAME_OVER, "", "", "", "", "", 0, 0));
        check("game_over ok", e.ok && has_str(e.value, "cmd", "game_over"));
    }
    {
        EventEdit e = insert(form(ET_RETURN_TO_TITLE_SCREEN, "", "", "", "", "", 0, 0));
        check("return_to_title_screen ok", e.ok && has_str(e.value, "cmd", "return_to_title_screen"));
    }

    std::printf("\n%d Checks, %d Fehler\n", g_count, g_fail);
    std::printf(g_fail ? "PREPARE-48 FEHLGESCHLAGEN\n" : "PREPARE-48 OK\n");
    return g_fail ? 1 : 0;
}
