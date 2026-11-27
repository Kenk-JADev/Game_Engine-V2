import json, sys
# Aufruf: ruby -I game/engine tools/gen_event_command_catalog.rb | python3 tools/gen_event_command_catalog.py
txt = sys.stdin.read()
lay = json.loads(txt.split('===LAYOUT===')[1].split('===CATALOG===')[0].strip())
cat = json.loads(txt.split('===CATALOG===')[1].strip())

def cstr(s):
    if s is None: return 'nullptr'
    return '"' + s.replace('\\','\\\\').replace('"','\\"') + '"'

kind_map={'string':'String','string_list':'StringList','key':'Key','enum':'Enum',
 'integer':'Integer','ref':'Ref','bool':'Bool','number':'Number','actor_ref':'ActorRef',
 'enemy_slot':'EnemySlot','event_ref':'EventRef','color':'Color','move_steps':'MoveSteps',
 'goods':'Goods','equipment_ref':'EquipmentRef'}
kinds=sorted({kind_map[f['kind']] for c in cat for f in c['fields']})

# dedupe enum value lists
enum_lists={}
for c in cat:
  for f in c['fields']:
    if f['kind']=='enum' and f['values']:
      enum_lists.setdefault(tuple(f['values']), None)
enum_names={}
for i,vals in enumerate(enum_lists):
  enum_names[vals]=f'EC_ENUM_{i}'

# dedupe block lists
block_lists={}
for c in cat:
  if c['blocks']: block_lists.setdefault(tuple(c['blocks']), None)
block_names={b:f'EC_BLOCKS_{i}' for i,b in enumerate(block_lists)}

L=[]
L.append('#pragma once')
L.append('// ============================================================================')
L.append('//  Etappe 44: Befehls-Katalog des Event-Dialogs (F6) wie RPG Maker XP.')
L.append('//')
L.append('//  Quelle: Rpg::EventCommands::Layout + Katalog (game/engine/rpg/')
L.append('//  event_commands/layout.rb, catalog.rb). Diese Datei ist daraus erzeugt:')
L.append('//  90 Befehle, drei Registerkarten, je zwei Spalten und zwei Abschnitte.')
L.append('//  Bewusst ohne Win32, damit Reihenfolge, Codes, Felder und Zuordnung')
L.append('//  kopflos geprueft werden koennen (tools/event_command_catalog_test.cpp).')
L.append('//')
L.append('//  NICHT von Hand pflegen: tools/gen_event_command_catalog.rb + .py erzeugen')
L.append('//  die Datei neu; tools/event_command_catalog_test.cpp prueft Reihenfolge,')
L.append('//  Codes und Felder gegen den Bauplan. `cmd` ordnet jeden XP-Typ dem bestehenden Event-String zu')
L.append('//  (docs/befehle.json) oder nullptr, wenn die Engine ihn noch nicht fuehrt.')
L.append('// ============================================================================')
L.append('')
L.append('#include <cstddef>')
L.append('#include <cstring>')
L.append('')
L.append('namespace rune::editor {')
L.append('')
L.append(f'constexpr int EC_COMMAND_COUNT = {len(cat)};')
L.append('constexpr int EC_TAB_COUNT = 3;')
L.append('constexpr int EC_COLUMN_COUNT = 2;   // links, rechts')
L.append('constexpr int EC_SECTION_COUNT = 12; // 3 Tabs x 2 Spalten x 2 Abschnitte')
L.append('')
L.append('enum class EcFieldKind { ' + ', '.join(kinds) + ' };')
L.append('')
L.append('struct EcField {')
L.append('    const char* name;')
L.append('    EcFieldKind kind;')
L.append('    bool required;')
L.append('    const char* default_text;   // Vorgabe als Text, nullptr = keine')
L.append('    const char* const* values;  // Enum-Auswahl, nullptr = keine')
L.append('    int value_count;')
L.append('    double min_value, max_value;')
L.append('    bool has_min, has_max;')
L.append('    const char* ref;            // Referenz-Sammlung (switches, items, ...)')
L.append('    const char* key;            // Asset-Sammlung (bgms, pictures, ...)')
L.append('};')
L.append('')
L.append('struct EcCommand {')
L.append('    const char* type;           // commandType der Rpg-Schicht')
L.append('    int code;                   // XP-Code')
L.append('    int tab;                    // 1..3')
L.append('    int column;                 // 0 = links, 1 = rechts')
L.append('    const char* section;        // Abschnitts-Key')
L.append('    const char* label;          // deutsche Beschriftung')
L.append('    const char* xp_name;        // XP-Originalname')
L.append('    int position;               // Reihenfolge im Abschnitt')
L.append('    const char* summary;')
L.append('    const char* cmd;            // bestehender Event-String oder nullptr')
L.append('    const char* const* blocks;')
L.append('    int block_count;')
L.append('    const EcField* fields;')
L.append('    int field_count;')
L.append('};')
L.append('')
L.append('struct EcTabInfo { int number; const char* title; };')
L.append('struct EcSectionInfo { const char* key; const char* title; int tab; int column; };')
L.append('')

# enum arrays
for vals,name in enum_names.items():
  L.append(f'static const char* const {name}[] = {{ ' + ', '.join(cstr(v) for v in vals) + ' };')
for b,name in block_names.items():
  L.append(f'static const char* const {name}[] = {{ ' + ', '.join(cstr(v) for v in b) + ' };')
L.append('')

# cmd mapping (XP type -> existing cmd string)
cmd_map={
 'show_text':'text','show_choices':'choices','input_number':'input_number',
 'change_text_options':'text_options','button_input_processing':'button_input',
 'wait':'wait','comment':'log','conditional_branch':None,'loop':'loop',
 'break_loop':'break','exit_event_processing':'exit_event','erase_event':'erase_event',
 'call_common_event':'common_event','label':'label','jump_to_label':'jump',
 'control_switches':'switch','control_variables':'variable','control_self_switch':'self_switch',
 'control_timer':'timer','change_gold':'gold','change_items':'item',
 'change_weapons':None,'change_armors':None,'change_party_member':'party_member',
 'change_windowskin':None,'change_battle_bgm':None,'change_battle_end_me':None,
 'change_save_access':'save_access','change_menu_access':'menu_access','change_encounter':'encounter_access',
 'transfer_player':'teleport','set_event_location':'move_player','scroll_map':None,
 'change_map_settings':None,'change_fog_color_tone':None,'change_fog_opacity':None,
 'show_animation':None,'change_transparent_flag':None,'set_move_route':'move_route',
 'wait_for_move_completion':None,'prepare_transition':None,'execute_transition':'fade',
 'change_screen_tone':'screen_tone','screen_flash':'flash','screen_shake':'shake',
 'show_picture':'show_picture','move_picture':'move_picture','rotate_picture':'rotate_picture',
 'change_picture_tone':'tint_picture','erase_picture':'erase_picture','set_weather_effects':'weather',
 'play_bgm':'play_bgm','fade_out_bgm':'fadeout_bgm','play_bgs':'play_bgs','fade_out_bgs':'fadeout_bgs',
 'memorize_bgm_bgs':'memorize_audio','restore_bgm_bgs':'restore_audio','play_me':'sound',
 'play_se':'sound','stop_se':None,
 'battle_processing':'battle','shop_processing':'shop','name_input_processing':'name_input',
 'change_hp':'actor_hp','change_sp':'actor_sp','change_state':'state','recover_all':'heal',
 'change_exp':'actor_exp','change_level':'actor_level','change_parameters':'actor_param',
 'change_skills':'actor_skill','change_equipment':None,'change_actor_name':'actor_name',
 'change_actor_class':None,'change_actor_graphic':None,
 'change_enemy_hp':None,'change_enemy_sp':None,'change_enemy_state':None,'enemy_recover_all':None,
 'enemy_appearance':None,'enemy_transform':None,'show_battle_animation':None,'deal_damage':None,
 'force_action':None,'abort_battle':None,
 'call_menu_screen':'menu','call_save_screen':'save_game','game_over':None,
 'return_to_title_screen':None,'script':'script',
}

# field arrays per command
for i,c in enumerate(cat):
  if not c['fields']: continue
  L.append(f'static const EcField EC_FIELDS_{i}[] = {{')
  for f in c['fields']:
    k=kind_map[f['kind']]
    dv=f['default']
    if dv is None: dt='nullptr'
    elif isinstance(dv,bool): dt=cstr('true' if dv else 'false')
    elif isinstance(dv,(int,float)): dt=cstr(str(dv))
    else: dt=cstr(str(dv))
    if f['kind']=='enum' and f['values']:
      vn=enum_names[tuple(f['values'])]; vc=str(len(f['values']))
    else: vn='nullptr'; vc='0'
    mn=f['min']; mx=f['max']
    has_min='true' if mn is not None else 'false'
    has_max='true' if mx is not None else 'false'
    mnv=str(float(mn)) if mn is not None else '0.0'
    mxv=str(float(mx)) if mx is not None else '0.0'
    L.append(f'    {{ {cstr(f["name"])}, EcFieldKind::{k}, {"true" if f["required"] else "false"}, {dt}, {vn}, {vc}, {mnv}, {mxv}, {has_min}, {has_max}, {cstr(f["ref"])}, {cstr(f["key"])} }},')
  L.append('};')
L.append('')

# command table
L.append('static const EcCommand EC_COMMANDS[EC_COMMAND_COUNT] = {')
for i,c in enumerate(cat):
  col=0 if c['column']=='left' else 1
  bl=block_names.get(tuple(c['blocks']),'nullptr') if c['blocks'] else 'nullptr'
  blc=str(len(c['blocks']))
  fa=f'EC_FIELDS_{i}' if c['fields'] else 'nullptr'
  fc=str(len(c['fields']))
  cmd=cmd_map.get(c['type'])
  L.append(f'    {{ {cstr(c["type"])}, {c["code"]}, {c["tab"]}, {col}, {cstr(c["section"])}, {cstr(c["label"])}, {cstr(c["xp_name"])}, {c["position"]}, {cstr(c["summary"])}, {cstr(cmd)}, {bl}, {blc}, {fa}, {fc} }},')
L.append('};')
L.append('')

# tab info
L.append('static const EcTabInfo EC_TABS[EC_TAB_COUNT] = {')
for t in lay:
  L.append(f'    {{ {t["number"]}, {cstr(t["title"])} }},')
L.append('};')
L.append('')
# section info
L.append('static const EcSectionInfo EC_SECTIONS[EC_SECTION_COUNT] = {')
for t in lay:
  for ci,col in enumerate(t['columns']):
    for s in col['sections']:
      L.append(f'    {{ {cstr(s["key"])}, {cstr(s["title"])}, {t["number"]}, {ci} }},')
L.append('};')
L.append('')

# accessors
L.append('''inline int ec_command_count() { return EC_COMMAND_COUNT; }
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
''')
L.append('} // namespace rune::editor')
open('src/editor/event_command_catalog.h','w',encoding='utf-8').write('\n'.join(L)+'\n')
print('generated', len(cat), 'commands')
