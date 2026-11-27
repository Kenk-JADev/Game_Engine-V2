#encoding: utf-8
# ============================================================================
#  EventRuntime — Etappe 48: die 30 zuvor unverdrahteten XP-Befehle.
#
#  Ein kleiner Handler je Befehlstyp (Registry, kein Riesen-Switch). Der
#  bestehende Interpreter ruft diese Brücke für die neuen cmd-Strings auf;
#  die typisierte Rpg-Schicht nutzt dieselben Engine-Module über EnginePorts.
#  Alle Seiteneffekte laufen über Party/Battle/MapEffects/MessageWindow/Audio.
# ============================================================================

module EventRuntime
  TYPES = %w[
    conditional_branch change_weapons change_armors change_windowskin
    change_battle_bgm change_battle_end_me scroll_map change_map_settings
    change_fog_color_tone change_fog_opacity show_animation change_transparent_flag
    wait_for_move_completion prepare_transition stop_se change_equipment
    change_actor_class change_actor_graphic change_enemy_hp change_enemy_sp
    change_enemy_state enemy_recover_all enemy_appearance enemy_transform
    show_battle_animation deal_damage force_action abort_battle game_over
    return_to_title_screen
  ].freeze

  module_function

  # Rückgabe :wait = Interpreter soll diesen Frame anhalten; sonst nil.
  def execute(cmd, interp)
    handler = HANDLERS[cmd['cmd'].to_s]
    return nil unless handler
    handler.call(cmd, interp)
  rescue StandardError => e
    Rune::Log.warn("EventRuntime: #{cmd['cmd']} fehlgeschlagen (#{e.message})") if defined?(Rune::Log)
    nil
  end

  def compare(a, op, b)
    case op.to_s
    when '==', 'equal' then a == b
    when '!=', 'not_equal' then a != b
    when '>', 'greater' then a > b
    when '>=', 'greater_equal' then a >= b
    when '<', 'less' then a < b
    when '<=', 'less_equal' then a <= b
    else false
    end
  end

  def condition_met?(cmd, interp)
    case cmd['kind'].to_s
    when 'switch' then GameState.switch(cmd['switch_id'] || cmd['name']) == !!cmd['value']
    when 'variable'
      links = GameState.variable(cmd['variable_id'] || cmd['name'])
      rechts = cmd.key?('value_variable_id') ? GameState.variable(cmd['value_variable_id']) : cmd['value'].to_f
      compare(links, cmd['comparator'] || cmd['op'] || '==', rechts)
    when 'self_switch'
      key = interp.owner.respond_to?(:self_switch_key) ? interp.owner.self_switch_key : nil
      key ? GameState.self_switch(key, cmd['letter']) == !!cmd['value'] : false
    when 'timer'
      rest = $game_system ? $game_system.timer_seconds.to_f : 0.0
      compare(rest, cmd['comparator'] || '>=', cmd['seconds'].to_f)
    when 'actor' then Party.member?(cmd['actor_id'] || cmd['name'])
    when 'enemy' then Battle.active? && !Battle.enemy_slot(cmd['enemy_slot'] || cmd['slot']).nil?
    when 'gold' then compare(Party.gold, cmd['comparator'] || cmd['op'] || '>=', cmd['value'].to_f)
    when 'item' then compare(Party.item_count(cmd['item_id'] || cmd['name']), cmd['comparator'] || cmd['op'] || '>=', cmd['value'].to_f)
    when 'weapon' then compare(Party.equipment_count('weapons', cmd['weapon_id']), cmd['comparator'] || '>=', cmd['value'].to_f)
    when 'armor' then compare(Party.equipment_count('armors', cmd['armor_id']), cmd['comparator'] || '>=', cmd['value'].to_f)
    when 'button'
      taste = cmd['button'].to_s
      defined?(Rune::Input) && Rune::Input.pressed?(taste.to_sym)
    when 'script'
      code = cmd['expression'] || cmd['code']
      code.to_s.empty? ? false : !!Scripts.eval_code(cmd['script_key'] || 'bedingung', code, interp, interp.owner)
    else false
    end
  end

  HANDLERS = {
    'conditional_branch' => lambda do |cmd, interp|
      interp.push_branch(condition_met?(cmd, interp) ? cmd['then'] : cmd['else'])
      nil
    end,
    'change_weapons' => lambda do |cmd, _interp|
      Party.change_equipment_stock('weapons', cmd['weapon_id'] || cmd['name'], cmd['amount'],
                                   include_equipment: cmd['include_equipment'], operation: cmd['operation'] || 'add')
      nil
    end,
    'change_armors' => lambda do |cmd, _interp|
      Party.change_equipment_stock('armors', cmd['armor_id'] || cmd['name'], cmd['amount'],
                                   include_equipment: cmd['include_equipment'], operation: cmd['operation'] || 'add')
      nil
    end,
    'change_windowskin' => lambda do |cmd, _interp|
      MapEffects.windowskin(cmd['skin_key'] || cmd['name'])
      $game_system.windowskin_name = cmd['skin_key'] || cmd['name'] if $game_system
      nil
    end,
    'change_battle_bgm' => lambda do |cmd, _interp|
      $game_system.battle_bgm = cmd['bgm_key'] || cmd['name'] if $game_system
      nil
    end,
    'change_battle_end_me' => lambda do |cmd, _interp|
      $game_system.battle_end_me = cmd['me_key'] || cmd['name'] if $game_system
      nil
    end,
    'scroll_map' => lambda do |cmd, _interp|
      MapEffects.scroll(cmd['direction'], cmd['distance'], cmd['speed'])
      nil
    end,
    'change_map_settings' => lambda do |cmd, _interp|
      MapEffects.map_settings(cmd['target'], cmd['asset_key'] || cmd['name'], color: cmd['color'])
      nil
    end,
    'change_fog_color_tone' => lambda do |cmd, _interp|
      MapEffects.fog_tone(cmd['color'] || { 'r' => cmd['r'], 'g' => cmd['g'], 'b' => cmd['b'], 'gray' => cmd['gray'] })
      nil
    end,
    'change_fog_opacity' => lambda do |cmd, _interp|
      MapEffects.fog_opacity(cmd['opacity'])
      nil
    end,
    'show_animation' => lambda do |cmd, interp|
      MapEffects.show_animation(cmd['target'], cmd['animation_key'] || cmd['name'], event: interp.owner)
      nil
    end,
    'change_transparent_flag' => lambda do |cmd, _interp|
      MapEffects.transparent(cmd['transparent'])
      nil
    end,
    'wait_for_move_completion' => lambda do |_cmd, interp|
      interp.wait_for_move_completion ? :wait : nil
    end,
    'prepare_transition' => lambda do |cmd, _interp|
      MapEffects.prepare_transition(cmd['transition_key'] || cmd['name'])
      nil
    end,
    'stop_se' => lambda do |_cmd, _interp|
      Audio.se_stop if defined?(Audio) && Audio.respond_to?(:se_stop)
      nil
    end,
    'change_equipment' => lambda do |cmd, _interp|
      Party.change_equipment(cmd['actor_id'] || cmd['actor'], cmd['slot'], cmd['item_id'])
      nil
    end,
    'change_actor_class' => lambda do |cmd, _interp|
      Party.change_class(cmd['actor_id'] || cmd['actor'], cmd['class_id'])
      nil
    end,
    'change_actor_graphic' => lambda do |cmd, _interp|
      Party.change_graphic(cmd['actor_id'] || cmd['actor'],
                           character_key: cmd['character_key'], battler_key: cmd['battler_key'], face_key: cmd['face_key'])
      nil
    end,
    'change_enemy_hp' => lambda do |cmd, _interp|
      Battle.enemy_command('change_enemy_hp', cmd)
      nil
    end,
    'change_enemy_sp' => lambda do |cmd, _interp|
      Battle.enemy_command('change_enemy_sp', cmd)
      nil
    end,
    'change_enemy_state' => lambda do |cmd, _interp|
      Battle.enemy_command('change_enemy_state', cmd)
      nil
    end,
    'enemy_recover_all' => lambda do |cmd, _interp|
      Battle.enemy_command('enemy_recover_all', cmd)
      nil
    end,
    'enemy_appearance' => lambda do |cmd, _interp|
      Battle.enemy_command('enemy_appearance', cmd)
      nil
    end,
    'enemy_transform' => lambda do |cmd, _interp|
      Battle.enemy_command('enemy_transform', cmd)
      nil
    end,
    'show_battle_animation' => lambda do |cmd, _interp|
      Battle.enemy_command('show_battle_animation', cmd)
      nil
    end,
    'deal_damage' => lambda do |cmd, _interp|
      Battle.enemy_command('deal_damage', cmd)
      nil
    end,
    'force_action' => lambda do |cmd, _interp|
      Battle.enemy_command('force_action', cmd)
      nil
    end,
    'abort_battle' => lambda do |_cmd, _interp|
      Battle.abort! if defined?(Battle)
      nil
    end,
    'game_over' => lambda do |_cmd, _interp|
      $game_temp.gameover = true if $game_temp
      nil
    end,
    'return_to_title_screen' => lambda do |_cmd, _interp|
      $game_temp.to_title = true if $game_temp
      nil
    end
  }.freeze
end
