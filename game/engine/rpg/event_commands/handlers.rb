#encoding: utf-8
# ==============================================================================
# Rpg::EventCommands::Handlers — Strategy-Handler und Registry
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

      class LambdaHandler
        attr_reader :name

        def initialize(name, &block)
          raise ArgumentError, 'Handler benötigt einen Block' unless block

          @name = name.to_s
          @block = block
        end

        def call(command, interpreter)
          @block.call(command, interpreter) || Result.next
        end
      end

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

      module Scripts
        module_function

        def handler(name, &block) = LambdaHandler.new(name, &block)

        # Command-Parameter haben bewusst String-Keys. Für Port-Aufrufe mit
        # Keyword-Argumenten werden sie hier explizit zu Symbolen gemacht.
        def keyword_argumente(parameter)
          parameter.to_h { |name, wert| [name.to_sym, wert] }
        end

        def build
          {
            dialog: handler(:dialog) do |command, interpreter|
              interpreter.ports.ui.message(lines: command.params['lines'], options: message_options(command))
              interpreter.emit('text.shown', lines: command.params['lines'].size,
                                             speaker: command.params['speaker'])
              interpreter.ports.ui.busy? ? Result.wait(:ui) : Result.next
            end,
            choices: handler(:choices) do |command, interpreter|
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
            input_number: handler(:input_number) do |command, interpreter|
              wert = interpreter.ports.ui.input_number(digits: command.params['digits'])
              if interpreter.ports.ui.busy?
                Result.wait(:ui)
              else
                interpreter.ports.variables.set(command.params['variable_id'], wert)
                interpreter.emit('input.number', variable_id: command.params['variable_id'])
                Result.next
              end
            end,
            text_options: handler(:text_options) do |command, interpreter|
              interpreter.emit('text.options', command.params.dup)
              Result.next
            end,
            button_input: handler(:button_input) do |command, interpreter|
              taste = interpreter.ports.ui.button_input
              if interpreter.ports.ui.busy?
                Result.wait(:ui)
              else
                interpreter.ports.variables.set(command.params['variable_id'], taste)
                Result.next
              end
            end,
            wait: handler(:wait) do |command, interpreter|
              sekunden = if command.params.key?('frames')
                           command.params['frames'].to_f / 60.0
                         else
                           command.params.fetch('seconds', 0.0).to_f
                         end
              Result.wait(:time, seconds: sekunden)
            end,
            comment: handler(:comment) do |_command, _interpreter|
              Result.next
            end,
            conditional_branch: handler(:conditional_branch) do |command, interpreter|
              Result.push(Conditions.erfuellt?(command, interpreter) ? 'then' : 'else')
            end,
            loop: handler(:loop) { |_command, _interpreter| Result.push('body') },
            break_loop: handler(:break_loop) { |_command, _interpreter| Result.control(:break_loop) },
            exit_event: handler(:exit_event) do |_command, interpreter|
              interpreter.emit('event.exit')
              Result.control(:exit_frame)
            end,
            erase_event: handler(:erase_event) do |command, interpreter|
              interpreter.ports.map.erase_event(event_key: interpreter.event_key)
              interpreter.emit('event.erased', event_key: interpreter.event_key)
              Result.control(:exit_all)
            end,
            common_event: handler(:common_event) do |command, interpreter|
              Result.call_sub(command.params['common_event_id'])
            end,
            label: handler(:label) { |_command, _interpreter| Result.next },
            jump: handler(:jump) do |command, interpreter|
              Result.control(:jump, command.params['name'])
            end,
            switch: handler(:switch) do |command, interpreter|
              von = command.params.fetch('from_id').to_i
              bis = command.params.fetch('to_id').to_i
              (von..bis).each { |id| interpreter.ports.switches.set(id, command.params['value']) }
              interpreter.emit('switch.changed', from_id: von, to_id: bis,
                                                 value: command.params['value'])
              Result.next
            end,
            variable: handler(:variable) do |command, interpreter|
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
            self_switch: handler(:self_switch) do |command, interpreter|
              interpreter.ports.self_switches.set(interpreter.event_key,
                                                   command.params['letter'], command.params['value'])
              interpreter.emit('self_switch.changed', event_key: interpreter.event_key,
                                                      letter: command.params['letter'])
              Result.next
            end,
            timer: handler(:timer) do |command, interpreter|
              interpreter.ports.timer.change(command.params['operation'], command.params['seconds'])
              Result.next
            end,
            gold: handler(:gold) do |command, interpreter|
              interpreter.ports.party.change_gold(command.params['operation'], command.params['amount'])
              Result.next
            end,
            inventory: handler(:inventory) do |command, interpreter|
              field, id = command.params.find { |name, _wert| name.end_with?('_id') }
              collection = { 'item_id' => 'items', 'weapon_id' => 'weapons',
                             'armor_id' => 'armors' }.fetch(field.to_s)
              interpreter.ports.inventory.change(collection, id,
                                                  command.params['amount'],
                                                  command.params.fetch('include_equipment', false),
                                                  operation: command.params['operation'])
              interpreter.emit('inventory.changed', collection: collection, id: id)
              Result.next
            end,
            party: handler(:party) do |command, interpreter|
              interpreter.ports.party.change_member(command.params['actor_id'],
                                                      command.params['operation'],
                                                      initialize: command.params.fetch('initialize', false))
              interpreter.emit('party.changed', command.params.dup)
              Result.next
            end,
            system_setting: handler(:system_setting) do |command, interpreter|
              interpreter.emit('system.setting', command.params.dup)
              Result.next
            end,
            map_transfer: handler(:map_transfer) do |command, interpreter|
              interpreter.ports.map.transfer(**keyword_argumente(command.params))
              interpreter.emit('map.transferred', command.params.dup)
              Result.next
            end,
            event_location: handler(:event_location) do |command, interpreter|
              interpreter.ports.map.set_event_location(**keyword_argumente(command.params))
              Result.next
            end,
            map_scroll: handler(:map_scroll) do |command, interpreter|
              interpreter.ports.map.scroll(**keyword_argumente(command.params))
              Result.next
            end,
            map_settings: handler(:map_settings) do |command, interpreter|
              interpreter.ports.map.settings(**keyword_argumente(command.params))
              Result.next
            end,
            fog_tone: handler(:fog_tone) do |command, interpreter|
              interpreter.ports.map.fog_tone(color: command.params['color'])
              Result.next
            end,
            fog_opacity: handler(:fog_opacity) do |command, interpreter|
              interpreter.ports.map.fog_opacity(opacity: command.params['opacity'])
              Result.next
            end,
            animation: handler(:animation) do |command, interpreter|
              interpreter.ports.animation.play(key: command.params['animation_key'],
                                                target: command.params['target'],
                                                event_id: command.params['event_id'])
              Result.next
            end,
            transparent: handler(:transparent) do |command, interpreter|
              interpreter.ports.map.transparent(flag: command.params['transparent'])
              Result.next
            end,
            move_route: handler(:move_route) do |command, interpreter|
              interpreter.ports.map.move_route(**keyword_argumente(command.params))
              command.params['wait'] ? Result.wait(:move) : Result.next
            end,
            route_wait: handler(:route_wait) do |_command, _interpreter|
              Result.wait(:move)
            end,
            transition: handler(:transition) do |command, interpreter|
              if command.commandType == 'prepare_transition'
                interpreter.ports.transition.prepare(key: command.params['transition_key'])
                Result.next
              else
                interpreter.ports.transition.execute(key: command.params['transition_key'])
                interpreter.ports.transition.busy? ? Result.wait(:transition) : Result.next
              end
            end,
            screen_tone: handler(:screen_tone) do |command, interpreter|
              interpreter.ports.screen.tone(color: command.params['color'])
              Result.next
            end,
            flash: handler(:flash) do |command, interpreter|
              interpreter.ports.screen.flash(color: command.params['color'],
                                               duration: command.params['duration'])
              command.params['no_wait'] ? Result.next : Result.wait(:time, seconds: command.params['duration'])
            end,
            shake: handler(:shake) do |command, interpreter|
              argumente = command.params.slice('strength', 'speed', 'duration')
              interpreter.ports.screen.shake(**keyword_argumente(argumente))
              command.params['no_wait'] ? Result.next : Result.wait(:time, seconds: command.params['duration'])
            end,
            picture: handler(:picture) do |command, interpreter|
              operation = {
                'show_picture' => :show, 'move_picture' => :move, 'rotate_picture' => :rotate,
                'change_picture_tone' => :tone, 'erase_picture' => :erase
              }.fetch(command.commandType)
              interpreter.ports.pictures.public_send(operation, **keyword_argumente(command.params))
              if %w[move_picture rotate_picture change_picture_tone].include?(command.commandType) &&
                 !command.params.fetch('no_wait', false)
                Result.wait(:time, seconds: command.params.fetch('duration', 0.0))
              else
                Result.next
              end
            end,
            weather: handler(:weather) do |command, interpreter|
              interpreter.ports.weather.set(kind: command.params['kind'],
                                             intensity: command.params['intensity'],
                                             duration: command.params.fetch('duration', 0.0))
              Result.next
            end,
            audio: handler(:audio) do |command, interpreter|
              audio_handler(command, interpreter)
              Result.next
            end,
            battle: handler(:battle) do |command, interpreter|
              interpreter.ports.battle.start(troop_id: command.params['troop_id'],
                                              can_escape: command.params.fetch('can_escape', true),
                                              can_lose: command.params.fetch('can_lose', false))
              ergebnis = interpreter.ports.battle.result.to_s
              block_name = { 'win' => 'win', 'escape' => 'escape', 'lose' => 'lose' }.fetch(ergebnis, ergebnis)
              command.block(block_name).empty? ? Result.next : Result.push(block_name)
            end,
            shop: handler(:shop) do |command, interpreter|
              interpreter.ports.ui.shop(key: command.params['shop_key'],
                                         goods: command.params.fetch('goods', []))
              interpreter.ports.ui.busy? ? Result.wait(:ui) : Result.next
            end,
            name_input: handler(:name_input) do |command, interpreter|
              name = interpreter.ports.ui.name_input(actor_id: command.params['actor_id'],
                                                      max_length: command.params['max_length'])
              if interpreter.ports.ui.busy?
                Result.wait(:ui)
              else
                interpreter.emit('name.changed', actor_id: command.params['actor_id'], name: name)
                Result.next
              end
            end,
            actor: handler(:actor) do |command, interpreter|
              actor_handler(command, interpreter)
              Result.next
            end,
            enemy: handler(:enemy) do |command, interpreter|
              enemy_handler(command, interpreter)
              Result.next
            end,
            ui_screen: handler(:ui_screen) do |command, interpreter|
              method = {
                'call_menu_screen' => :menu, 'call_save_screen' => :save,
                'game_over' => :game_over, 'return_to_title_screen' => :title
              }.fetch(command.commandType)
              interpreter.ports.ui.public_send(method)
              interpreter.emit('ui.screen', screen: command.commandType)
              Result.next
            end,
            script: handler(:script) do |command, interpreter|
              interpreter.ports.script.run(script_key: command.params['script_key'],
                                            expression: command.params['expression'],
                                            code: command.params['code'],
                                            context: interpreter.context)
              interpreter.emit('script.hook', script_key: command.params['script_key'])
              Result.next
            end
          }
        end

        def message_options(command)
          command.params.slice('speaker', 'face_key', 'background', 'position')
        end

        def choice_options(command)
          command.params.slice('cancel_index', 'cancel_behavior', 'default_index', 'position', 'background')
        end

        def audio_handler(command, interpreter)
          parameter = command.params
          ports = interpreter.ports
          case command.commandType
          when 'play_bgm' then ports.audio.play_bgm(key: parameter['bgm_key'], volume: parameter['volume'], pitch: parameter['pitch'])
          when 'fade_out_bgm' then ports.audio.fade_out_bgm(key: parameter['bgm_key'], seconds: parameter['fade_seconds'])
          when 'play_bgs' then ports.audio.play_bgs(key: parameter['bgs_key'], volume: parameter['volume'], pitch: parameter['pitch'])
          when 'fade_out_bgs' then ports.audio.fade_out_bgs(key: parameter['bgs_key'], seconds: parameter['fade_seconds'])
          when 'memorize_bgm_bgs' then ports.audio.memorize_bgm_bgs
          when 'restore_bgm_bgs' then ports.audio.restore_bgm_bgs
          when 'play_me' then ports.audio.play_me(key: parameter['me_key'], volume: parameter['volume'], pitch: parameter['pitch'])
          when 'play_se' then ports.audio.play_se(key: parameter['se_key'], volume: parameter['volume'], pitch: parameter['pitch'])
          when 'stop_se' then ports.audio.stop_se(key: parameter['se_key'])
          end
          key_field = command.params.keys.find { |name| name.end_with?('_key') }
          interpreter.emit('audio.command', command: command.commandType,
                                            key: key_field ? parameter[key_field] : nil)
        end

        def actor_handler(command, interpreter)
          parameter = command.params
          actor_id = parameter['actor_id']
          interpreter.ports.actors.change(command: command.commandType, params: parameter)
          interpreter.emit('actor.command', command: command.commandType, actor_id: actor_id)
        end

        def enemy_handler(command, interpreter)
          parameter = command.params
          ports = interpreter.ports
          case command.commandType
          when 'force_action'
            ports.battle.force_action(slot: parameter['slot'],
                                       action_key: parameter['action_key'],
                                       target_slot: parameter['target_slot'])
          when 'abort_battle'
            ports.battle.abort
          else
            ports.battle.enemy(command: command.commandType, params: parameter)
          end
          interpreter.emit('enemy.command', command: command.commandType, slot: parameter['slot'])
        end
      end

      class Registry
        def initialize
          @by_type = {}
          @by_handler = Scripts.build.freeze
        end

        def for(definition)
          @by_type[definition.type] || @by_handler.fetch(definition.handler.to_sym) do
            raise KeyError, "Kein Handler für #{definition.type} (#{definition.handler})"
          end
        end

        def register(type, handler)
          raise ArgumentError, 'Handler muss call unterstützen' unless handler.respond_to?(:call)

          @by_type[type.to_s] = handler
          self
        end
      end
    end
  end
end
