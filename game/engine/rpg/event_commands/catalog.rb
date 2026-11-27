#encoding: utf-8
# ==============================================================================
# Rpg::EventCommands::Catalog — 90 eigene Befehlsdefinitionen (Tabs 1–3)
# ==============================================================================

module Rpg
  module EventCommands
    module Catalog
      class Builder
        def initialize
          @definitions = []
        end

        def f(name, kind, **optionen)
          Models::Field.new(name: name, kind: kind, **optionen)
        end

        def command(type, code, tab, group, handler, fields: {}, blocks: {},
                    column: nil, summary: '')
          @definitions << Models::Definition.new(
            type: type, code: code, tab: tab, column: column,
            group: group, handler: handler,
            fields: fields, blocks: blocks, summary: summary
          )
        end

        def build
          Models::CatalogData.new(
            version: 1, definitions: @definitions, resources: {}, programs: []
          )
        end

        def define_all
          define_tab1
          define_tab2
          define_tab3
        end

        private

        def define_tab1
          command('show_text', 101, 1, :dialog, :dialog,
                  fields: {
                    lines: f('lines', :string_list),
                    speaker: f('speaker', :string, required: false, allow_empty: true),
                    face_key: f('face_key', :key, required: false, key: 'faces'),
                    background: f('background', :enum, required: false, default: 'window',
                                  values: %w[window dim transparent]),
                    position: f('position', :enum, required: false, default: 'bottom',
                                values: %w[top middle bottom])
                  },
                  summary: 'Zeigt eigene Textzeilen in einem Dialogfenster an.')

          command('show_choices', 102, 1, :dialog, :choices,
                  fields: {
                    choices: f('choices', :string_list),
                    cancel_index: f('cancel_index', :integer, required: false, default: -1, min: -1, max: 4),
                    cancel_behavior: f('cancel_behavior', :enum, required: false, default: 'cancel',
                                       values: %w[cancel disabled branch]),
                    default_index: f('default_index', :integer, required: false, default: 0, min: 0, max: 3),
                    position: f('position', :enum, required: false, default: 'center',
                                values: %w[left center right]),
                    background: f('background', :enum, required: false, default: 'window',
                                  values: %w[window dim transparent])
                  },
                  blocks: %w[choice_0 choice_1 choice_2 choice_3 cancel],
                  summary: 'Zeigt bis zu vier Auswahlen und verzweigt in benannte Blöcke.')

          command('input_number', 103, 1, :dialog, :input_number,
                  fields: {
                    variable_id: f('variable_id', :ref, ref: 'variables'),
                    digits: f('digits', :integer, required: false, default: 4, min: 1, max: 8)
                  },
                  summary: 'Schreibt eine Zahleneingabe in eine Variable.')

          command('change_text_options', 104, 1, :dialog, :text_options,
                  fields: {
                    background: f('background', :enum, values: %w[window dim transparent]),
                    position: f('position', :enum, values: %w[top middle bottom]),
                    speed: f('speed', :integer, min: 1, max: 10),
                    no_fast: f('no_fast', :bool, required: false, default: false)
                  },
                  summary: 'Stellt Fensterstil, Position und Textgeschwindigkeit um.')

          command('button_input_processing', 105, 1, :dialog, :button_input,
                  fields: { variable_id: f('variable_id', :ref, ref: 'variables') },
                  summary: 'Wartet auf einen Tastendruck und speichert ihn.')

          command('wait', 106, 1, :dialog, :wait,
                  fields: {
                    seconds: f('seconds', :number, required: false, min: 0.0, max: 60.0),
                    frames: f('frames', :integer, required: false, min: 0, max: 3600)
                  },
                  summary: 'Blockiert den Interpreter für Sekunden oder Frames.')

          command('comment', 108, 1, :dialog, :comment,
                  fields: { text: f('text', :string, allow_empty: true) },
                  summary: 'Dokumentiert einen Schritt ohne Laufzeitwirkung.')

          command('conditional_branch', 111, 1, :flow, :conditional_branch,
                  fields: condition_fields,
                  blocks: %w[then else],
                  summary: 'Wertet eine Bedingung aus und führt einen Kinderblock aus.')

          command('loop', 112, 1, :flow, :loop,
                  blocks: %w[body],
                  summary: 'Wiederholt seinen Kinderblock bis Break, Exit oder Jump.')

          command('break_loop', 113, 1, :flow, :break_loop,
                  summary: 'Verlässt die nächstliegende Schleife.')

          command('exit_event_processing', 115, 1, :flow, :exit_event,
                  summary: 'Beendet den aktuellen Event-Stack sofort.')

          command('erase_event', 116, 1, :flow, :erase_event,
                  summary: 'Entfernt das ausführende Event und beendet es.')

          command('call_common_event', 117, 1, :flow, :common_event,
                  fields: { common_event_id: f('common_event_id', :ref, ref: 'common_events') },
                  summary: 'Ruft ein Common Event als Sub-Interpreter auf.')

          command('label', 118, 1, :flow, :label,
                  fields: { name: f('name', :string) },
                  summary: 'Definiert eine Sprungmarke im aktuellen Programm.')

          command('jump_to_label', 119, 1, :flow, :jump,
                  fields: { name: f('name', :string) },
                  summary: 'Springt zu einer Sprungmarke im Programm.')

          command('control_switches', 121, 1, :state, :switch,
                  fields: {
                    from_id: f('from_id', :ref, ref: 'switches'),
                    to_id: f('to_id', :ref, ref: 'switches'),
                    value: f('value', :bool)
                  },
                  summary: 'Setzt einen Bereich globaler Schalter auf ON/OFF.')

          command('control_variables', 122, 1, :state, :variable,
                  fields: {
                    from_id: f('from_id', :ref, ref: 'variables'),
                    to_id: f('to_id', :ref, ref: 'variables'),
                    operation: f('operation', :enum,
                                 values: %w[set add subtract multiply divide modulo]),
                    source: f('source', :enum,
                              values: %w[constant variable random item actor enemy other]),
                    value: f('value', :number, required: false),
                    random_min: f('random_min', :integer, required: false, min: 0),
                    random_max: f('random_max', :integer, required: false, min: 0),
                    source_variable_id: f('source_variable_id', :ref, ref: 'variables', required: false),
                    item_id: f('item_id', :ref, ref: 'items', required: false),
                    actor_id: f('actor_id', :actor_ref, required: false),
                    enemy_slot: f('enemy_slot', :enemy_slot, required: false),
                    other: f('other', :enum, required: false,
                             values: %w[party_size gold steps timer])
                  },
                  summary: 'Verändert einen Bereich von Variablen aus einer typisierten Quelle.')

          command('control_self_switch', 123, 1, :state, :self_switch,
                  fields: {
                    letter: f('letter', :enum, values: %w[A B C D]),
                    value: f('value', :bool)
                  },
                  summary: 'Setzt einen lokalen Schalter des ausführenden Events.')

          command('control_timer', 124, 1, :state, :timer,
                  fields: {
                    operation: f('operation', :enum, values: %w[start stop set add subtract]),
                    seconds: f('seconds', :number, required: false, min: 0.0)
                  },
                  summary: 'Verändert den laufenden Timer.')

          command('change_gold', 125, 1, :state, :gold,
                  fields: {
                    operation: f('operation', :enum, values: %w[add subtract]),
                    amount: f('amount', :integer, min: 0)
                  },
                  summary: 'Erhöht oder verringert das Gold der Party.')

          { 126 => ['change_items', 'items'], 127 => ['change_weapons', 'weapons'],
            128 => ['change_armors', 'armors'] }.each do |code, (type, collection)|
            fields = {
              "#{collection == 'items' ? 'item' : collection == 'weapons' ? 'weapon' : 'armor'}_id":
                f("#{collection == 'items' ? 'item' : collection == 'weapons' ? 'weapon' : 'armor'}_id",
                  :ref, ref: collection),
              operation: f('operation', :enum, values: %w[add subtract]),
              amount: f('amount', :integer, min: 0, max: 999)
            }
            fields[:include_equipment] = f('include_equipment', :bool, required: false, default: false) unless collection == 'items'
            command(type, code, 1, :state, :inventory, fields: fields,
                    summary: "Verändert den Bestand in #{collection}.")
          end

          command('change_party_member', 129, 1, :state, :party,
                  fields: {
                    actor_id: f('actor_id', :actor_ref),
                    operation: f('operation', :enum, values: %w[add remove]),
                    initialize: f('initialize', :bool, required: false, default: false)
                  },
                  summary: 'Nimmt einen Charakter in die Party auf oder entfernt ihn; optional initialisiert.')

          command('change_windowskin', 131, 1, :system, :system_setting,
                  fields: { skin_key: f('skin_key', :key, key: 'windowskins') },
                  summary: 'Wechselt das Windowskin zur Laufzeit.')

          command('change_battle_bgm', 132, 1, :system, :audio,
                  fields: { bgm_key: f('bgm_key', :key, key: 'bgms') },
                  summary: 'Setzt die Kampf-BGM.')

          command('change_battle_end_me', 133, 1, :system, :audio,
                  fields: { me_key: f('me_key', :key, key: 'mes') },
                  summary: 'Setzt das Kampf-Ende-ME.')

          command('change_save_access', 134, 1, :system, :system_setting,
                  fields: { allowed: f('allowed', :bool) },
                  summary: 'Erlaubt oder sperrt das Speichern.')

          command('change_menu_access', 135, 1, :system, :system_setting,
                  fields: { allowed: f('allowed', :bool) },
                  summary: 'Erlaubt oder sperrt das Menü.')

          command('change_encounter', 136, 1, :system, :system_setting,
                  fields: { allowed: f('allowed', :bool) },
                  summary: 'Schaltet Zufallskämpfe ein oder aus.')
        end

        def define_tab2
          command('transfer_player', 201, 2, :map, :map_transfer,
                  fields: {
                    map_id: f('map_id', :ref, ref: 'maps'),
                    x: f('x', :integer, min: 0),
                    y: f('y', :integer, min: 0),
                    direction: f('direction', :enum, required: false, default: 'down',
                                 values: %w[up down left right]),
                    fade: f('fade', :enum, required: false, default: 'none',
                            values: %w[none fade])
                  },
                  summary: 'Versetzt den Spieler auf eine Karte und Position.')

          command('set_event_location', 202, 2, :map, :event_location,
                  fields: character_target_fields.merge(
                    x: f('x', :integer, min: 0), y: f('y', :integer, min: 0)
                  ),
                  summary: 'Setzt die Position eines Events oder des Spielers.')

          command('scroll_map', 203, 2, :map, :map_scroll,
                  fields: {
                    direction: f('direction', :enum, values: %w[up down left right]),
                    distance: f('distance', :integer, min: 1, max: 999),
                    speed: f('speed', :integer, min: 1, max: 10)
                  },
                  summary: 'Scrollt die Kartenansicht.')

          command('change_map_settings', 204, 2, :map, :map_settings,
                  fields: {
                    target: f('target', :enum, values: %w[panorama fog battleback]),
                    asset_key: f('asset_key', :string),
                    color: f('color', :color, required: false)
                  },
                  summary: 'Wechselt Panorama, Nebel oder Kampf-Hintergrund.')

          command('change_fog_color_tone', 205, 2, :map, :fog_tone,
                  fields: { color: f('color', :color) },
                  summary: 'Verändert Nebelfarbton und Graustufe.')

          command('change_fog_opacity', 206, 2, :map, :fog_opacity,
                  fields: { opacity: f('opacity', :integer, min: 0, max: 255) },
                  summary: 'Verändert die Nebeldeckkraft.')

          command('show_animation', 207, 2, :map, :animation,
                  fields: character_target_fields.merge(
                    animation_key: f('animation_key', :key, key: 'animations')
                  ),
                  summary: 'Spielt eine Animation an einem Charakter ab.')

          command('change_transparent_flag', 208, 2, :map, :transparent,
                  fields: { transparent: f('transparent', :bool) },
                  summary: 'Setzt das Transparenz-Flag des Spielers.')

          command('set_move_route', 209, 2, :map, :move_route,
                  fields: character_target_fields.merge(
                    steps: f('steps', :move_steps),
                    wait: f('wait', :bool, required: false, default: false),
                    repeat: f('repeat', :bool, required: false, default: false),
                    ignore: f('ignore', :bool, required: false, default: false)
                  ),
                  summary: 'Setzt eine typisierte Bewegungsroute mit Repeat/Ignore.')

          command('wait_for_move_completion', 210, 2, :map, :route_wait,
                  summary: 'Wartet, bis alle Routen beendet sind.')

          command('prepare_transition', 221, 2, :screen, :transition,
                  fields: { transition_key: f('transition_key', :key, key: 'transitions') },
                  summary: 'Bereitet einen Übergang vor.')

          command('execute_transition', 222, 2, :screen, :transition,
                  fields: { transition_key: f('transition_key', :key, key: 'transitions') },
                  summary: 'Führt einen vorbereiteten Übergang aus.')

          command('change_screen_tone', 223, 2, :screen, :screen_tone,
                  fields: { color: f('color', :color) },
                  summary: 'Verändert den Bildschirm-Farbton.')

          command('screen_flash', 224, 2, :screen, :flash,
                  fields: {
                    color: f('color', :color),
                    duration: f('duration', :number, min: 0.0),
                    no_wait: f('no_wait', :bool, required: false, default: false)
                  },
                  summary: 'Blitzt den Bildschirm.')

          command('screen_shake', 225, 2, :screen, :shake,
                  fields: {
                    strength: f('strength', :integer, min: 1, max: 20),
                    speed: f('speed', :integer, min: 1, max: 20),
                    duration: f('duration', :number, min: 0.0),
                    no_wait: f('no_wait', :bool, required: false, default: false)
                  },
                  summary: 'Erschüttert den Bildschirm.')

          command('show_picture', 231, 2, :picture, :picture,
                  fields: picture_fields(include_motion: false),
                  summary: 'Zeigt ein Bild an oder ersetzt es.')

          command('move_picture', 232, 2, :picture, :picture,
                  fields: picture_fields(include_motion: true),
                  summary: 'Bewegt, skaliert und blendet ein Bild.')

          command('rotate_picture', 233, 2, :picture, :picture,
                  fields: picture_fields(include_motion: true, include_position: false).merge(
                    degrees: f('degrees', :number, min: -360.0, max: 360.0)
                  ),
                  summary: 'Dreht ein Bild.')

          command('change_picture_tone', 234, 2, :picture, :picture,
                  fields: picture_fields(include_motion: true, include_position: false).merge(
                    color: f('color', :color)
                  ),
                  summary: 'Färbt ein Bild.')

          command('erase_picture', 235, 2, :picture, :picture,
                  fields: {
                    picture_slot_id: f('picture_slot_id', :integer, min: 1, max: 50),
                    picture_key: f('picture_key', :key, key: 'pictures')
                  },
                  summary: 'Löscht ein Bild über Slot-ID.')

          command('set_weather_effects', 236, 2, :picture, :weather,
                  fields: {
                    kind: f('kind', :enum, values: %w[none rain storm snow]),
                    intensity: f('intensity', :integer, min: 0, max: 10),
                    duration: f('duration', :number, required: false, default: 0.0, min: 0.0)
                  },
                  summary: 'Stellt Wetterart, Stärke und Dauer ein.')

          command('play_bgm', 241, 2, :audio, :audio,
                  fields: audio_fields('bgm_key', 'bgms'),
                  summary: 'Startet eine Hintergrundmusik.')

          command('fade_out_bgm', 242, 2, :audio, :audio,
                  fields: fade_fields('bgm_key', 'bgms'),
                  summary: 'Blendet die Hintergrundmusik aus.')

          command('play_bgs', 245, 2, :audio, :audio,
                  fields: audio_fields('bgs_key', 'bgss'),
                  summary: 'Startet ein Hintergrundgeräusch.')

          command('fade_out_bgs', 246, 2, :audio, :audio,
                  fields: fade_fields('bgs_key', 'bgss'),
                  summary: 'Blendet das Hintergrundgeräusch aus.')

          command('memorize_bgm_bgs', 247, 2, :audio, :audio,
                  summary: 'Merkt sich BGM und BGS.')

          command('restore_bgm_bgs', 248, 2, :audio, :audio,
                  summary: 'Stellt gemerkte BGM und BGS wieder her.')

          command('play_me', 249, 2, :audio, :audio,
                  fields: audio_fields('me_key', 'mes'),
                  summary: 'Spielt ein Music-Effect ab.')

          command('play_se', 250, 2, :audio, :audio,
                  fields: audio_fields('se_key', 'ses'),
                  summary: 'Spielt einen Sound-Effect ab.')

          command('stop_se', 251, 2, :audio, :audio,
                  fields: { se_key: f('se_key', :key, key: 'ses') },
                  summary: 'Stoppt einen Sound-Effect.')
        end

        def define_tab3
          command('battle_processing', 301, 3, :battle, :battle,
                  fields: {
                    troop_id: f('troop_id', :ref, ref: 'troops'),
                    can_escape: f('can_escape', :bool, required: false, default: true),
                    can_lose: f('can_lose', :bool, required: false, default: false)
                  },
                  blocks: %w[win escape lose],
                  summary: 'Startet einen Kampf und verzweigt nach dem Ergebnis.')

          command('shop_processing', 302, 3, :battle, :shop,
                  fields: {
                    shop_key: f('shop_key', :key, key: 'shops'),
                    goods: f('goods', :goods)
                  },
                  summary: 'Öffnet einen Laden mit eigener Warenliste.')

          command('name_input_processing', 303, 3, :battle, :name_input,
                  fields: {
                    actor_id: f('actor_id', :actor_ref),
                    max_length: f('max_length', :integer, required: false, default: 8, min: 1, max: 16)
                  },
                  summary: 'Fragt einen neuen Charakternamen ab.')

          command('change_hp', 311, 3, :actor, :actor,
                  fields: actor_value_fields.merge(allow_knockout: f('allow_knockout', :bool, required: false, default: false)),
                  summary: 'Verändert die aktuellen HP eines Charakters.')

          command('change_sp', 312, 3, :actor, :actor,
                  fields: actor_value_fields,
                  summary: 'Verändert die aktuellen SP eines Charakters.')

          command('change_state', 313, 3, :actor, :actor,
                  fields: {
                    actor_id: f('actor_id', :actor_ref),
                    state_id: f('state_id', :ref, ref: 'states'),
                    operation: f('operation', :enum, values: %w[add remove])
                  },
                  summary: 'Setzt oder entfernt einen Zustand.')

          command('recover_all', 314, 3, :actor, :actor,
                  fields: { actor_id: f('actor_id', :actor_ref) },
                  summary: 'Heilt einen Charakter vollständig.')

          command('change_exp', 315, 3, :actor, :actor,
                  fields: actor_value_fields,
                  summary: 'Verändert die EXP eines Charakters.')

          command('change_level', 316, 3, :actor, :actor,
                  fields: actor_value_fields,
                  summary: 'Verändert das Level eines Charakters.')

          command('change_parameters', 317, 3, :actor, :actor,
                  fields: actor_value_fields.merge(
                    parameter: f('parameter', :enum,
                                 values: %w[max_hp max_sp strength dexterity agility intelligence])
                  ),
                  summary: 'Verändert einen Kampfparameter eines Charakters.')

          command('change_skills', 318, 3, :actor, :actor,
                  fields: {
                    actor_id: f('actor_id', :actor_ref),
                    skill_id: f('skill_id', :ref, ref: 'skills'),
                    operation: f('operation', :enum, values: %w[add remove])
                  },
                  summary: 'Lehrt oder vergisst eine Fähigkeit.')

          command('change_equipment', 319, 3, :actor, :actor,
                  fields: {
                    actor_id: f('actor_id', :actor_ref),
                    slot: f('slot', :enum, values: %w[weapon armor accessory]),
                    item_id: f('item_id', :equipment_ref)
                  },
                  summary: 'Wechselt einen Ausrüstungsslot.')

          command('change_actor_name', 320, 3, :actor, :actor,
                  fields: {
                    actor_id: f('actor_id', :actor_ref),
                    name: f('name', :string)
                  },
                  summary: 'Ändert den Namen eines Charakters.')

          command('change_actor_class', 321, 3, :actor, :actor,
                  fields: {
                    actor_id: f('actor_id', :actor_ref),
                    class_id: f('class_id', :ref, ref: 'classes')
                  },
                  summary: 'Wechselt die Klasse eines Charakters.')

          command('change_actor_graphic', 322, 3, :actor, :actor,
                  fields: {
                    actor_id: f('actor_id', :actor_ref),
                    character_key: f('character_key', :key, key: 'characters'),
                    battler_key: f('battler_key', :key, key: 'battlers'),
                    face_key: f('face_key', :key, key: 'faces')
                  },
                  summary: 'Wechselt die Grafiken eines Charakters.')

          command('change_enemy_hp', 331, 3, :enemy, :enemy,
                  fields: enemy_value_fields.merge(allow_knockout: f('allow_knockout', :bool, required: false, default: false)),
                  summary: 'Verändert die HP eines Gegners.')

          command('change_enemy_sp', 332, 3, :enemy, :enemy,
                  fields: enemy_value_fields,
                  summary: 'Verändert die SP eines Gegners.')

          command('change_enemy_state', 333, 3, :enemy, :enemy,
                  fields: {
                    slot: f('slot', :enemy_slot),
                    state_id: f('state_id', :ref, ref: 'states'),
                    operation: f('operation', :enum, values: %w[add remove])
                  },
                  summary: 'Setzt oder entfernt einen Gegnerzustand.')

          command('enemy_recover_all', 334, 3, :enemy, :enemy,
                  fields: { slot: f('slot', :enemy_slot) },
                  summary: 'Heilt einen Gegner vollständig.')

          command('enemy_appearance', 335, 3, :enemy, :enemy,
                  fields: { slot: f('slot', :enemy_slot) },
                  summary: 'Macht einen Gegner sichtbar.')

          command('enemy_transform', 336, 3, :enemy, :enemy,
                  fields: {
                    slot: f('slot', :enemy_slot),
                    enemy_id: f('enemy_id', :ref, ref: 'enemies')
                  },
                  summary: 'Verwandelt einen Gegner.')

          command('show_battle_animation', 337, 3, :enemy, :enemy,
                  fields: {
                    slot: f('slot', :enemy_slot),
                    animation_key: f('animation_key', :key, key: 'animations')
                  },
                  summary: 'Zeigt eine Animation im Kampf.')

          command('deal_damage', 338, 3, :enemy, :enemy,
                  fields: {
                    slot: f('slot', :enemy_slot),
                    amount: f('amount', :integer, min: 0),
                    element_id: f('element_id', :ref, ref: 'elements')
                  },
                  summary: 'Fügt einem Gegner direkten Schaden zu.')

          command('force_action', 339, 3, :enemy, :enemy,
                  fields: {
                    slot: f('slot', :enemy_slot),
                    action_key: f('action_key', :key, key: 'skills'),
                    target_slot: f('target_slot', :enemy_slot, min: -1)
                  },
                  summary: 'Erzwingt eine gegnerische Aktion.')

          command('abort_battle', 340, 3, :enemy, :enemy,
                  summary: 'Bricht den laufenden Kampf ab.')

          command('call_menu_screen', 351, 3, :system, :ui_screen,
                  summary: 'Öffnet das Menü.')

          command('call_save_screen', 352, 3, :system, :ui_screen,
                  summary: 'Öffnet den Speicher-Dialog.')

          command('game_over', 353, 3, :system, :ui_screen,
                  summary: 'Zeigt Game Over.')

          command('return_to_title_screen', 354, 3, :system, :ui_screen,
                  summary: 'Kehrt zum Titelbild zurück.')

          command('script', 355, 3, :system, :script,
                  fields: {
                    script_key: f('script_key', :key, required: false, key: 'scripts'),
                    expression: f('expression', :string, required: false, allow_empty: true),
                    code: f('code', :string, required: false, allow_empty: true)
                  },
                  summary: 'Ruft einen entkoppelten Script-Hook oder Ausdruck auf.')
        end

        def condition_fields
          {
            kind: f('kind', :enum,
                    values: %w[switch variable self_switch timer actor enemy gold item weapon armor button script]),
            switch_id: f('switch_id', :ref, ref: 'switches', required: false),
            variable_id: f('variable_id', :ref, ref: 'variables', required: false),
            comparator: f('comparator', :enum, required: false,
                          values: %w[equal greater_equal less_equal greater less not_equal]),
            value: f('value', :number, required: false),
            value_variable_id: f('value_variable_id', :ref, ref: 'variables', required: false),
            letter: f('letter', :enum, required: false, values: %w[A B C D]),
            seconds: f('seconds', :number, required: false, min: 0.0),
            actor_id: f('actor_id', :actor_ref, required: false),
            enemy_slot: f('enemy_slot', :enemy_slot, required: false),
            item_id: f('item_id', :ref, ref: 'items', required: false),
            weapon_id: f('weapon_id', :ref, ref: 'weapons', required: false),
            armor_id: f('armor_id', :ref, ref: 'armors', required: false),
            button: f('button', :integer, required: false, min: 0, max: 255),
            script_key: f('script_key', :key, required: false, key: 'scripts'),
            expression: f('expression', :string, required: false, allow_empty: true)
          }
        end

        def character_target_fields
          {
            target: f('target', :enum, values: %w[player this_event event]),
            event_id: f('event_id', :event_ref, required: false)
          }
        end

        def picture_fields(include_motion:, include_position: true)
          fields = {
            picture_slot_id: f('picture_slot_id', :integer, min: 1, max: 50),
            picture_key: f('picture_key', :key, key: 'pictures')
          }
          if include_position
            fields.merge!(
              x: f('x', :integer, min: -9999, max: 9999),
              y: f('y', :integer, min: -9999, max: 9999),
              scale_x: f('scale_x', :integer, required: false, default: 100, min: 0, max: 1000),
              scale_y: f('scale_y', :integer, required: false, default: 100, min: 0, max: 1000),
              opacity: f('opacity', :integer, required: false, default: 255, min: 0, max: 255),
              blend: f('blend', :enum, required: false, default: 'normal',
                       values: %w[normal add subtract])
            )
          end
          if include_motion
            fields[:duration] = f('duration', :number, required: false, default: 0.0, min: 0.0)
            fields[:no_wait] = f('no_wait', :bool, required: false, default: false)
          end
          fields
        end

        def audio_fields(key_name, collection)
          {
            key_name.to_sym => f(key_name, :key, key: collection),
            volume: f('volume', :integer, required: false, default: 80, min: 0, max: 100),
            pitch: f('pitch', :integer, required: false, default: 100, min: 50, max: 150)
          }
        end

        def fade_fields(key_name, collection)
          {
            key_name.to_sym => f(key_name, :key, key: collection),
            fade_seconds: f('fade_seconds', :number, required: false, default: 1.0, min: 0.0)
          }
        end

        def actor_value_fields
          {
            actor_id: f('actor_id', :actor_ref),
            operation: f('operation', :enum, values: %w[add subtract]),
            amount: f('amount', :integer, min: 0, max: 999999)
          }
        end

        def enemy_value_fields
          {
            slot: f('slot', :enemy_slot),
            operation: f('operation', :enum, values: %w[add subtract]),
            amount: f('amount', :integer, min: 0, max: 999999)
          }
        end
      end

      def self.build_catalog
        Builder.new.tap(&:define_all).build
      end
    end
  end
end
