#encoding: utf-8
# ==============================================================================
# Rpg::EventCommands::Handlers — ein kleiner Handler je Befehlstyp (90 Stück).
#
# Kein switch/case über commandType: Jeder der 90 XP-Befehle hat hier genau
# einen eigenen ICommandHandler, der unter seinem Typ in der Registry liegt.
# Gemeinsame Rechenwege (Bedingungen, Variablenquellen) stehen als kleine
# Funktionen daneben; die Handler selbst bleiben drei bis zehn Zeilen lang.
#
# Alle Seiteneffekte laufen über die injizierten Ports (ports.rb). Ein Handler
# kennt weder Renderer noch $game_*-Globale noch eine UI.
# ==============================================================================

module Rpg
  module EventCommands
    module Handlers
      Result = Data.define(:action, :value, :data) do
        def initialize(action:, value: nil, data: {})
          super(action: action.to_sym, value: value, data: data)
        end

        def self.next = new(action: :next)
        def self.push(block_name) = new(action: :push, value: block_name)
        def self.call_sub(id) = new(action: :call, value: id)
        def self.wait(kind, seconds: 0.0) = new(action: :wait, value: kind, data: { seconds: seconds })
        def self.control(action, value = nil, data: {}) = new(action: action, value: value, data: data)
      end

      # Ein Handler-Objekt pro Befehlstyp. `command_type` ist die Identität,
      # damit Registry und Tests prüfen können, dass wirklich jeder Typ seinen
      # eigenen Handler besitzt.
      class CommandHandler
        attr_reader :command_type

        def initialize(command_type, &block)
          raise ArgumentError, 'Handler benötigt einen Block' unless block

          @command_type = command_type.to_s
          @block = block
          freeze
        end

        def name = command_type
        def call(command, interpreter) = @block.call(command, interpreter) || Result.next
      end

      # Rückwärtskompatibler Alias: frühere Aufrufer kannten nur LambdaHandler.
      LambdaHandler = CommandHandler

      module Conditions
        module_function

        def erfuellt?(command, interpreter)
          parameter = command.params
          ports = interpreter.ports
          case parameter['kind']
          when 'switch' then ports.switches.get(parameter['switch_id']) == wert_als_bool(parameter['value'])
          when 'variable' then vergleich(ports.variables.get(parameter['variable_id']),
                                          vergleichswert(parameter, interpreter), parameter['comparator'])
          when 'self_switch'
            ports.self_switches.get(interpreter.event_key, parameter['letter']) == wert_als_bool(parameter['value'])
          when 'timer' then vergleich(ports.timer.remaining, parameter['seconds'].to_f, parameter['comparator'] || 'greater_equal')
          when 'actor' then ports.party.members.include?(parameter['actor_id'].to_i)
          when 'enemy' then ports.battle.enemy_present?(parameter['slot'].to_i)
          when 'gold' then vergleich(ports.party.gold, parameter['value'].to_f, parameter['comparator'] || 'greater_equal')
          when 'item', 'weapon', 'armor'
            collection = { 'item' => 'items', 'weapon' => 'weapons', 'armor' => 'armors' }.fetch(parameter['kind'])
            id = parameter["#{parameter['kind']}_id"]
            vergleich(ports.inventory.count(collection, id), parameter['value'].to_f, parameter['comparator'] || 'greater_equal')
          when 'button' then ports.ui.last_button == parameter['button'].to_i
          when 'script'
            !!ports.script.run(script_key: parameter['script_key'],
                               expression: parameter['expression'],
                               context: interpreter.context)
          else false
          end
        end

        def vergleichswert(parameter, interpreter)
          return interpreter.ports.variables.get(parameter['value_variable_id']) if parameter['value_variable_id']

          parameter['value'].to_f
        end

        def vergleich(links, rechts, comparator)
          case comparator.to_s
          when 'equal' then links == rechts
          when 'greater_equal' then links >= rechts
          when 'less_equal' then links <= rechts
          when 'greater' then links > rechts
          when 'less' then links < rechts
          when 'not_equal' then links != rechts
          else false
          end
        end

        def wert_als_bool(wert)
          wert == true || wert.to_i == 1
        end
      end

      module Values
        module_function

        def quellenwert(command, interpreter)
          parameter = command.params
          ports = interpreter.ports
          case parameter['source']
          when 'variable' then ports.variables.get(parameter['source_variable_id'])
          when 'random'
            minimum = parameter.fetch('random_min', 0).to_i
            maximum = parameter.fetch('random_max', minimum).to_i
            interpreter.random.rand((maximum - minimum + 1)).to_f + minimum
          when 'item' then ports.inventory.count('items', parameter['item_id']).to_f
          when 'actor' then ports.party.members.include?(parameter['actor_id'].to_i) ? 1.0 : 0.0
          when 'enemy' then ports.battle.enemy_present?(parameter['enemy_slot'].to_i) ? 1.0 : 0.0
          when 'other'
            case parameter['other']
            when 'party_size' then ports.party.members.size.to_f
            when 'gold' then ports.party.gold.to_f
            when 'timer' then ports.timer.remaining
            else 0.0
            end
          else parameter['value'].to_f
          end
        end

        def anwenden(alter_wert, operation, wert)
          case operation.to_s
          when 'set' then wert
          when 'add' then alter_wert + wert
          when 'subtract' then alter_wert - wert
          when 'multiply' then alter_wert * wert
          when 'divide' then wert.zero? ? alter_wert : alter_wert / wert
          when 'modulo' then wert.zero? ? alter_wert : alter_wert % wert
          else alter_wert
          end
        end
      end

      # Kleine gemeinsame Aufrufwege. Sie ersetzen keine Handler-Entscheidung:
      # welcher Port wie angesprochen wird, steht im Handler des jeweiligen Typs.
      module Aufrufe
        module_function

        # Command-Parameter haben bewusst String-Keys. Für Port-Aufrufe mit
        # Keyword-Argumenten werden sie hier explizit zu Symbolen gemacht.
        def keyword_argumente(parameter)
          parameter.to_h { |name, wert| [name.to_sym, wert] }
        end

        def inventar(sammlung, command, interpreter)
          parameter = command.params
          interpreter.ports.inventory.change(sammlung, parameter["#{sammlung.chomp('s')}_id"],
                                             parameter['amount'],
                                             parameter.fetch('include_equipment', false),
                                             operation: parameter['operation'])
          interpreter.emit('inventory.changed', collection: sammlung,
                                                id: parameter["#{sammlung.chomp('s')}_id"])
          Result.next
        end

        def audio(command, interpreter, key_feld)
          interpreter.emit('audio.command', command: command.commandType,
                                            key: command.params[key_feld])
          Result.next
        end

        def actor(methode, command, interpreter)
          interpreter.ports.actors.public_send(methode, **keyword_argumente(command.params))
          interpreter.emit('actor.command', command: command.commandType,
                                            actor_id: command.params['actor_id'])
          Result.next
        end

        def gegner(command, interpreter)
          interpreter.ports.battle.enemy(command: command.commandType, params: command.params)
          interpreter.emit('enemy.command', command: command.commandType,
                                            slot: command.params['slot'])
          Result.next
        end

        def bild(methode, command, interpreter, wartet: false)
          interpreter.ports.pictures.public_send(methode, **keyword_argumente(command.params))
          return Result.next unless wartet
          return Result.next if command.params.fetch('no_wait', false)

          Result.wait(:time, seconds: command.params.fetch('duration', 0.0))
        end

        def bildschirm(effekt, command, interpreter)
          interpreter.emit('screen.effect', command: command.commandType)
          return Result.next if command.params.fetch('no_wait', false)

          Result.wait(:time, seconds: command.params.fetch('duration', 0.0))
        end
      end

      module Scripts
        module_function

        def handler(name, &block) = CommandHandler.new(name, &block)
        def keyword_argumente(parameter) = Aufrufe.keyword_argumente(parameter)

        # 90 Handler, einer je Befehlstyp, in der Reihenfolge des XP-Layouts.
        def build
          {}.merge(tab1_handler, tab2_handler, tab3_handler)
        end

        def message_options(command)
          command.params.slice('speaker', 'face_key', 'background', 'position')
        end

        def choice_options(command)
          command.params.slice('cancel_index', 'cancel_behavior', 'default_index', 'position', 'background')
        end

        # --- Tab 1: Dialog, Flusskontrolle, Variablen & System ---------------
        def tab1_handler
          {
            'show_text' => handler('show_text') do |command, interpreter|
              interpreter.ports.ui.message(lines: command.params['lines'], options: message_options(command))
              interpreter.emit('text.shown', lines: command.params['lines'].size,
                                             speaker: command.params['speaker'])
              interpreter.ports.ui.busy? ? Result.wait(:ui) : Result.next
            end,
            'show_choices' => handler('show_choices') do |command, interpreter|
              index = interpreter.ports.ui.choices(choices: command.params['choices'],
                                                   options: choice_options(command))
              if interpreter.ports.ui.busy?
                Result.wait(:ui)
              else
                interpreter.choice_result = index
                block_name = if command.params.fetch('cancel_behavior', 'cancel') == 'disabled' ||
                                index != command.params['cancel_index'].to_i
                               "choice_#{index}"
                             else
                               'cancel'
                             end
                Result.push(block_name)
              end
            end,
            'input_number' => handler('input_number') do |command, interpreter|
              wert = interpreter.ports.ui.input_number(digits: command.params['digits'])
              if interpreter.ports.ui.busy?
                Result.wait(:ui)
              else
                interpreter.ports.variables.set(command.params['variable_id'], wert)
                interpreter.emit('input.number', variable_id: command.params['variable_id'])
                Result.next
              end
            end,
            'change_text_options' => handler('change_text_options') do |command, interpreter|
              interpreter.ports.ui.text_options(**keyword_argumente(command.params))
              interpreter.emit('text.options', command.params.dup)
              Result.next
            end,
            'button_input_processing' => handler('button_input_processing') do |command, interpreter|
              taste = interpreter.ports.ui.button_input
              if interpreter.ports.ui.busy?
                Result.wait(:ui)
              else
                interpreter.ports.variables.set(command.params['variable_id'], taste)
                Result.next
              end
            end,
            'wait' => handler('wait') do |command, _interpreter|
              sekunden = if command.params.key?('frames')
                           command.params['frames'].to_f / 60.0
                         else
                           command.params.fetch('seconds', 0.0).to_f
                         end
              Result.wait(:time, seconds: sekunden)
            end,
            'comment' => handler('comment') { |_command, _interpreter| Result.next },
            'conditional_branch' => handler('conditional_branch') do |command, interpreter|
              Result.push(Conditions.erfuellt?(command, interpreter) ? 'then' : 'else')
            end,
            'loop' => handler('loop') { |_command, _interpreter| Result.push('body') },
            'break_loop' => handler('break_loop') { |_command, _interpreter| Result.control(:break_loop) },
            'exit_event_processing' => handler('exit_event_processing') do |_command, interpreter|
              interpreter.emit('event.exit')
              Result.control(:exit_frame)
            end,
            'erase_event' => handler('erase_event') do |_command, interpreter|
              interpreter.ports.map.erase_event(event_key: interpreter.event_key)
              interpreter.emit('event.erased', event_key: interpreter.event_key)
              Result.control(:exit_all)
            end,
            'call_common_event' => handler('call_common_event') do |command, _interpreter|
              Result.call_sub(command.params['common_event_id'])
            end,
            'label' => handler('label') { |_command, _interpreter| Result.next },
            'jump_to_label' => handler('jump_to_label') do |command, _interpreter|
              Result.control(:jump, command.params['name'])
            end,
            'control_switches' => handler('control_switches') do |command, interpreter|
              von = command.params.fetch('from_id').to_i
              bis = command.params.fetch('to_id').to_i
              (von..bis).each { |id| interpreter.ports.switches.set(id, command.params['value']) }
              interpreter.emit('switch.changed', from_id: von, to_id: bis,
                                                 value: command.params['value'])
              Result.next
            end,
            'control_variables' => handler('control_variables') do |command, interpreter|
              von = command.params.fetch('from_id').to_i
              bis = command.params.fetch('to_id').to_i
              quelle = Values.quellenwert(command, interpreter)
              (von..bis).each do |id|
                alter_wert = interpreter.ports.variables.get(id)
                wert = Values.anwenden(alter_wert, command.params['operation'], quelle)
                interpreter.ports.variables.set(id, wert)
              end
              interpreter.emit('variable.changed', from_id: von, to_id: bis,
                                                   value: interpreter.ports.variables.get(bis))
              Result.next
            end,
            'control_self_switch' => handler('control_self_switch') do |command, interpreter|
              interpreter.ports.self_switches.set(interpreter.event_key,
                                                  command.params['letter'], command.params['value'])
              interpreter.emit('self_switch.changed', event_key: interpreter.event_key,
                                                      letter: command.params['letter'])
              Result.next
            end,
            'control_timer' => handler('control_timer') do |command, interpreter|
              interpreter.ports.timer.change(command.params['operation'], command.params['seconds'])
              Result.next
            end,
            'change_gold' => handler('change_gold') do |command, interpreter|
              interpreter.ports.party.change_gold(command.params['operation'], command.params['amount'])
              Result.next
            end,
            'change_items' => handler('change_items') do |command, interpreter|
              Aufrufe.inventar('items', command, interpreter)
            end,
            'change_weapons' => handler('change_weapons') do |command, interpreter|
              Aufrufe.inventar('weapons', command, interpreter)
            end,
            'change_armors' => handler('change_armors') do |command, interpreter|
              Aufrufe.inventar('armors', command, interpreter)
            end,
            'change_party_member' => handler('change_party_member') do |command, interpreter|
              interpreter.ports.party.change_member(command.params['actor_id'],
                                                    command.params['operation'],
                                                    initialize: command.params.fetch('initialize', false))
              interpreter.emit('party.changed', command.params.dup)
              Result.next
            end,
            'change_windowskin' => handler('change_windowskin') do |command, interpreter|
              interpreter.ports.system_settings.windowskin(key: command.params['skin_key'])
              interpreter.emit('system.setting', command.params.dup)
              Result.next
            end,
            'change_battle_bgm' => handler('change_battle_bgm') do |command, interpreter|
              interpreter.ports.audio.set_battle_bgm(key: command.params['bgm_key'])
              Aufrufe.audio(command, interpreter, 'bgm_key')
            end,
            'change_battle_end_me' => handler('change_battle_end_me') do |command, interpreter|
              interpreter.ports.audio.set_battle_end_me(key: command.params['me_key'])
              Aufrufe.audio(command, interpreter, 'me_key')
            end,
            'change_save_access' => handler('change_save_access') do |command, interpreter|
              interpreter.ports.system_settings.save_access(allowed: command.params['allowed'])
              interpreter.emit('system.setting', command.params.dup)
              Result.next
            end,
            'change_menu_access' => handler('change_menu_access') do |command, interpreter|
              interpreter.ports.system_settings.menu_access(allowed: command.params['allowed'])
              interpreter.emit('system.setting', command.params.dup)
              Result.next
            end,
            'change_encounter' => handler('change_encounter') do |command, interpreter|
              interpreter.ports.system_settings.encounter(allowed: command.params['allowed'])
              interpreter.emit('system.setting', command.params.dup)
              Result.next
            end
          }
        end

        # --- Tab 2: Karte, Bewegung, Bilder, Screen & Audio ------------------
        def tab2_handler
          {
            'transfer_player' => handler('transfer_player') do |command, interpreter|
              interpreter.ports.map.transfer(**keyword_argumente(command.params))
              interpreter.emit('map.transferred', command.params.dup)
              Result.next
            end,
            'set_event_location' => handler('set_event_location') do |command, interpreter|
              interpreter.ports.map.set_event_location(**keyword_argumente(command.params))
              Result.next
            end,
            'scroll_map' => handler('scroll_map') do |command, interpreter|
              interpreter.ports.map.scroll(**keyword_argumente(command.params))
              Result.next
            end,
            'change_map_settings' => handler('change_map_settings') do |command, interpreter|
              interpreter.ports.map.settings(**keyword_argumente(command.params))
              Result.next
            end,
            'change_fog_color_tone' => handler('change_fog_color_tone') do |command, interpreter|
              interpreter.ports.map.fog_tone(color: command.params['color'])
              Result.next
            end,
            'change_fog_opacity' => handler('change_fog_opacity') do |command, interpreter|
              interpreter.ports.map.fog_opacity(opacity: command.params['opacity'])
              Result.next
            end,
            'show_animation' => handler('show_animation') do |command, interpreter|
              interpreter.ports.animation.play(key: command.params['animation_key'],
                                               target: command.params['target'],
                                               event_id: command.params['event_id'])
              Result.next
            end,
            'change_transparent_flag' => handler('change_transparent_flag') do |command, interpreter|
              interpreter.ports.map.transparent(flag: command.params['transparent'])
              Result.next
            end,
            'set_move_route' => handler('set_move_route') do |command, interpreter|
              interpreter.ports.map.move_route(**keyword_argumente(command.params))
              command.params['wait'] ? Result.wait(:move) : Result.next
            end,
            'wait_for_move_completion' => handler('wait_for_move_completion') do |_command, _interpreter|
              Result.wait(:move)
            end,
            'prepare_transition' => handler('prepare_transition') do |command, interpreter|
              interpreter.ports.transition.prepare(key: command.params['transition_key'])
              Result.next
            end,
            'execute_transition' => handler('execute_transition') do |command, interpreter|
              interpreter.ports.transition.execute(key: command.params['transition_key'])
              interpreter.ports.transition.busy? ? Result.wait(:transition) : Result.next
            end,
            'change_screen_tone' => handler('change_screen_tone') do |command, interpreter|
              interpreter.ports.screen.tone(color: command.params['color'])
              Result.next
            end,
            'screen_flash' => handler('screen_flash') do |command, interpreter|
              interpreter.ports.screen.flash(color: command.params['color'],
                                             duration: command.params['duration'])
              Aufrufe.bildschirm(:flash, command, interpreter)
            end,
            'screen_shake' => handler('screen_shake') do |command, interpreter|
              argumente = command.params.slice('strength', 'speed', 'duration')
              interpreter.ports.screen.shake(**keyword_argumente(argumente))
              Aufrufe.bildschirm(:shake, command, interpreter)
            end,
            'show_picture' => handler('show_picture') do |command, interpreter|
              Aufrufe.bild(:show, command, interpreter)
            end,
            'move_picture' => handler('move_picture') do |command, interpreter|
              Aufrufe.bild(:move, command, interpreter, wartet: true)
            end,
            'rotate_picture' => handler('rotate_picture') do |command, interpreter|
              Aufrufe.bild(:rotate, command, interpreter, wartet: true)
            end,
            'change_picture_tone' => handler('change_picture_tone') do |command, interpreter|
              Aufrufe.bild(:tone, command, interpreter, wartet: true)
            end,
            'erase_picture' => handler('erase_picture') do |command, interpreter|
              Aufrufe.bild(:erase, command, interpreter)
            end,
            'set_weather_effects' => handler('set_weather_effects') do |command, interpreter|
              interpreter.ports.weather.set(kind: command.params['kind'],
                                            intensity: command.params['intensity'],
                                            duration: command.params.fetch('duration', 0.0))
              Result.next
            end,
            'play_bgm' => handler('play_bgm') do |command, interpreter|
              parameter = command.params
              interpreter.ports.audio.play_bgm(key: parameter['bgm_key'], volume: parameter['volume'],
                                               pitch: parameter['pitch'])
              Aufrufe.audio(command, interpreter, 'bgm_key')
            end,
            'fade_out_bgm' => handler('fade_out_bgm') do |command, interpreter|
              interpreter.ports.audio.fade_out_bgm(key: command.params['bgm_key'],
                                                   seconds: command.params['fade_seconds'])
              Aufrufe.audio(command, interpreter, 'bgm_key')
            end,
            'play_bgs' => handler('play_bgs') do |command, interpreter|
              parameter = command.params
              interpreter.ports.audio.play_bgs(key: parameter['bgs_key'], volume: parameter['volume'],
                                               pitch: parameter['pitch'])
              Aufrufe.audio(command, interpreter, 'bgs_key')
            end,
            'fade_out_bgs' => handler('fade_out_bgs') do |command, interpreter|
              interpreter.ports.audio.fade_out_bgs(key: command.params['bgs_key'],
                                                   seconds: command.params['fade_seconds'])
              Aufrufe.audio(command, interpreter, 'bgs_key')
            end,
            'memorize_bgm_bgs' => handler('memorize_bgm_bgs') do |command, interpreter|
              interpreter.ports.audio.memorize_bgm_bgs
              Aufrufe.audio(command, interpreter, 'bgm_key')
            end,
            'restore_bgm_bgs' => handler('restore_bgm_bgs') do |command, interpreter|
              interpreter.ports.audio.restore_bgm_bgs
              Aufrufe.audio(command, interpreter, 'bgm_key')
            end,
            'play_me' => handler('play_me') do |command, interpreter|
              parameter = command.params
              interpreter.ports.audio.play_me(key: parameter['me_key'], volume: parameter['volume'],
                                              pitch: parameter['pitch'])
              Aufrufe.audio(command, interpreter, 'me_key')
            end,
            'play_se' => handler('play_se') do |command, interpreter|
              parameter = command.params
              interpreter.ports.audio.play_se(key: parameter['se_key'], volume: parameter['volume'],
                                              pitch: parameter['pitch'])
              Aufrufe.audio(command, interpreter, 'se_key')
            end,
            'stop_se' => handler('stop_se') do |command, interpreter|
              interpreter.ports.audio.stop_se(key: command.params['se_key'])
              Aufrufe.audio(command, interpreter, 'se_key')
            end
          }
        end

        # --- Tab 3: Kampf, Charakterwerte, Gegner, Screens & Script ----------
        def tab3_handler
          {
            'battle_processing' => handler('battle_processing') do |command, interpreter|
              interpreter.ports.battle.start(troop_id: command.params['troop_id'],
                                             can_escape: command.params.fetch('can_escape', true),
                                             can_lose: command.params.fetch('can_lose', false))
              ergebnis = interpreter.ports.battle.result.to_s
              command.block(ergebnis).empty? ? Result.next : Result.push(ergebnis)
            end,
            'shop_processing' => handler('shop_processing') do |command, interpreter|
              interpreter.ports.ui.shop(key: command.params['shop_key'],
                                        goods: command.params.fetch('goods', []))
              interpreter.ports.ui.busy? ? Result.wait(:ui) : Result.next
            end,
            'name_input_processing' => handler('name_input_processing') do |command, interpreter|
              name = interpreter.ports.ui.name_input(actor_id: command.params['actor_id'],
                                                     max_length: command.params['max_length'])
              if interpreter.ports.ui.busy?
                Result.wait(:ui)
              else
                interpreter.emit('name.changed', actor_id: command.params['actor_id'], name: name)
                Result.next
              end
            end,
            'change_hp' => handler('change_hp') { |c, i| Aufrufe.actor(:change_hp, c, i) },
            'change_sp' => handler('change_sp') { |c, i| Aufrufe.actor(:change_sp, c, i) },
            'change_state' => handler('change_state') { |c, i| Aufrufe.actor(:change_state, c, i) },
            'recover_all' => handler('recover_all') { |c, i| Aufrufe.actor(:recover_all, c, i) },
            'change_exp' => handler('change_exp') { |c, i| Aufrufe.actor(:change_exp, c, i) },
            'change_level' => handler('change_level') { |c, i| Aufrufe.actor(:change_level, c, i) },
            'change_parameters' => handler('change_parameters') { |c, i| Aufrufe.actor(:change_parameters, c, i) },
            'change_skills' => handler('change_skills') { |c, i| Aufrufe.actor(:change_skills, c, i) },
            'change_equipment' => handler('change_equipment') { |c, i| Aufrufe.actor(:change_equipment, c, i) },
            'change_actor_name' => handler('change_actor_name') { |c, i| Aufrufe.actor(:change_actor_name, c, i) },
            'change_actor_class' => handler('change_actor_class') { |c, i| Aufrufe.actor(:change_actor_class, c, i) },
            'change_actor_graphic' => handler('change_actor_graphic') { |c, i| Aufrufe.actor(:change_actor_graphic, c, i) },
            'change_enemy_hp' => handler('change_enemy_hp') { |c, i| Aufrufe.gegner(c, i) },
            'change_enemy_sp' => handler('change_enemy_sp') { |c, i| Aufrufe.gegner(c, i) },
            'change_enemy_state' => handler('change_enemy_state') { |c, i| Aufrufe.gegner(c, i) },
            'enemy_recover_all' => handler('enemy_recover_all') { |c, i| Aufrufe.gegner(c, i) },
            'enemy_appearance' => handler('enemy_appearance') { |c, i| Aufrufe.gegner(c, i) },
            'enemy_transform' => handler('enemy_transform') { |c, i| Aufrufe.gegner(c, i) },
            'show_battle_animation' => handler('show_battle_animation') { |c, i| Aufrufe.gegner(c, i) },
            'deal_damage' => handler('deal_damage') { |c, i| Aufrufe.gegner(c, i) },
            'force_action' => handler('force_action') do |command, interpreter|
              parameter = command.params
              interpreter.ports.battle.force_action(slot: parameter['slot'],
                                                    action_key: parameter['action_key'],
                                                    target_slot: parameter['target_slot'])
              interpreter.emit('enemy.command', command: command.commandType, slot: parameter['slot'])
              Result.next
            end,
            'abort_battle' => handler('abort_battle') do |command, interpreter|
              interpreter.ports.battle.abort
              interpreter.emit('enemy.command', command: command.commandType, slot: command.params['slot'])
              Result.next
            end,
            'call_menu_screen' => handler('call_menu_screen') do |_command, interpreter|
              interpreter.ports.ui.menu
              interpreter.emit('ui.screen', screen: 'call_menu_screen')
              Result.next
            end,
            'call_save_screen' => handler('call_save_screen') do |_command, interpreter|
              interpreter.ports.ui.save
              interpreter.emit('ui.screen', screen: 'call_save_screen')
              Result.next
            end,
            'game_over' => handler('game_over') do |_command, interpreter|
              interpreter.ports.ui.game_over
              interpreter.emit('ui.screen', screen: 'game_over')
              Result.next
            end,
            'return_to_title_screen' => handler('return_to_title_screen') do |_command, interpreter|
              interpreter.ports.ui.title
              interpreter.emit('ui.screen', screen: 'return_to_title_screen')
              Result.next
            end,
            'script' => handler('script') do |command, interpreter|
              interpreter.ports.script.run(script_key: command.params['script_key'],
                                           expression: command.params['expression'],
                                           code: command.params['code'],
                                           context: interpreter.context)
              interpreter.emit('script.hook', script_key: command.params['script_key'])
              Result.next
            end
          }
        end
      end

      # Die Registry hält genau einen Handler je Befehlstyp. Überschreiben ist
      # erlaubt (Dependency Injection für Tests und eigene Engine-Anbindungen),
      # ein fehlender Handler ist ein harter Fehler.
      class Registry
        def initialize(handlers: Scripts.build)
          @by_type = handlers.to_h { |typ, handler| [typ.to_s, handler] }
        end

        def for(definition)
          typ = definition.respond_to?(:type) ? definition.type : definition
          @by_type.fetch(typ.to_s) do
            raise KeyError, "Kein Handler für Befehlstyp '#{typ}'"
          end
        end

        def register(type, handler)
          raise ArgumentError, 'Handler muss call unterstützen' unless handler.respond_to?(:call)

          @by_type[type.to_s] = handler
          self
        end

        def registered?(type) = @by_type.key?(type.to_s)
        def types = @by_type.keys.freeze
        def size = @by_type.size
        def handlers = @by_type.dup.freeze
      end
    end
  end
end
