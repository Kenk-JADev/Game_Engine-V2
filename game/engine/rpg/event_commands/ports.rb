#encoding: utf-8
# ==============================================================================
# Rpg::EventCommands::Ports — Interfaces und testbare Adapter
# ==============================================================================

module Rpg
  module EventCommands
    module Ports
      module Recording
        def calls
          @calls ||= []
        end

        def record(name, payload = {})
          calls << { name: name.to_s }.merge(payload)
          payload
        end
      end

      class ISwitchStore
        def get(_id) = raise(NotImplementedError, 'ISwitchStore#get fehlt')
        def set(_id, _value) = raise(NotImplementedError, 'ISwitchStore#set fehlt')
      end

      class NullSwitchStore < ISwitchStore
        def get(_id) = false
        def set(_id, _value) = nil
      end

      class MemorySwitchStore < ISwitchStore
        include Recording

        def initialize(initial = {})
          @werte = initial.to_h { |id, wert| [id.to_i, wert ? true : false] }
        end

        def get(id) = @werte.fetch(id.to_i, false)
        def set(id, value) = @werte[id.to_i] = value ? true : false
      end

      class IVariableStore
        def get(_id) = raise(NotImplementedError, 'IVariableStore#get fehlt')
        def set(_id, _value) = raise(NotImplementedError, 'IVariableStore#set fehlt')
      end

      class NullVariableStore < IVariableStore
        def get(_id) = 0
        def set(_id, _value) = nil
      end

      class MemoryVariableStore < IVariableStore
        include Recording

        def initialize(initial = {})
          @werte = initial.to_h { |id, wert| [id.to_i, wert.to_f] }
        end

        def get(id) = @werte.fetch(id.to_i, 0.0)
        def set(id, value) = @werte[id.to_i] = value.to_f
      end

      class ISelfSwitchStore
        def get(_event_key, _letter) = raise(NotImplementedError, 'ISelfSwitchStore#get fehlt')
        def set(_event_key, _letter, _value) = raise(NotImplementedError, 'ISelfSwitchStore#set fehlt')
        def clear(_event_key) = raise(NotImplementedError, 'ISelfSwitchStore#clear fehlt')
      end

      class NullSelfSwitchStore < ISelfSwitchStore
        def get(_event_key, _letter) = false
        def set(_event_key, _letter, _value) = nil
        def clear(_event_key) = nil
      end

      class MemorySelfSwitchStore < ISelfSwitchStore
        include Recording

        def initialize
          @werte = {}
        end

        def get(event_key, letter)
          @werte.dig(event_key.to_s, letter.to_s) == true
        end

        def set(event_key, letter, value)
          @werte[event_key.to_s] ||= {}
          @werte[event_key.to_s][letter.to_s] = value ? true : false
        end

        def clear(event_key) = @werte.delete(event_key.to_s)
      end

      class ITimerPort
        def remaining = raise(NotImplementedError, 'ITimerPort#remaining fehlt')
        def change(_operation, _seconds) = raise(NotImplementedError, 'ITimerPort#change fehlt')
      end

      class NullTimerPort < ITimerPort
        def remaining = 0.0
        def change(_operation, _seconds) = nil
      end

      class MemoryTimerPort < ITimerPort
        include Recording

        def initialize(seconds = 0.0)
          @seconds = seconds.to_f
        end

        attr_reader :seconds

        def remaining = @seconds

        def change(operation, seconds)
          @seconds = case operation.to_s
                     when 'start' then seconds.to_f
                     when 'stop' then 0.0
                     when 'set' then seconds.to_f
                     when 'add' then @seconds + seconds.to_f
                     when 'subtract' then [@seconds - seconds.to_f, 0.0].max
                     else @seconds
                     end
          record(:timer_change, operation: operation.to_s, seconds: seconds)
        end
      end

      # kind ist die Sammlung (items/weapons/armors), operation die Richtung
      # (add/subtract). Beides getrennt: sonst landet der Bestand unter dem
      # Schlüssel "add" und count("items", ...) findet nichts.
      class IInventoryPort
        def change(_kind, _id, _amount, _include_equipment, operation: 'add') = raise(NotImplementedError, 'IInventoryPort#change fehlt')
        def count(_kind, _id) = raise(NotImplementedError, 'IInventoryPort#count fehlt')
      end

      class NullInventoryPort < IInventoryPort
        def change(_kind, _id, _amount, _include_equipment, operation: 'add') = nil
        def count(_kind, _id) = 0
      end

      class MemoryInventoryPort < IInventoryPort
        include Recording

        def initialize
          @bestand = Hash.new(0)
        end

        def change(kind, id, amount, include_equipment, operation: 'add')
          schluessel = [kind.to_s, id.to_i]
          delta = amount.to_i
          delta = -delta if operation.to_s == 'subtract'
          @bestand[schluessel] = [@bestand[schluessel] + delta, 0].max
          record(:inventory_change, kind: kind.to_s, id: id.to_i, operation: operation.to_s,
                                    amount: delta, include_equipment: include_equipment)
        end

        def count(kind, id) = @bestand[[kind.to_s, id.to_i]]
      end

      class IPartyPort
        def change_member(_actor_id, _operation, initialize: false) = raise(NotImplementedError, 'IPartyPort#change_member fehlt')
        def change_gold(_operation, _amount) = raise(NotImplementedError, 'IPartyPort#change_gold fehlt')
        def members = raise(NotImplementedError, 'IPartyPort#members fehlt')
      end

      class NullPartyPort < IPartyPort
        def change_member(_actor_id, _operation, initialize: false) = nil
        def change_gold(_operation, _amount) = nil
        def members = []
      end

      class MemoryPartyPort < IPartyPort
        include Recording

        def initialize(members: [])
          @members = members.map(&:to_i)
          @gold = 0
        end

        attr_reader :gold

        def members = @members.dup

        def change_member(actor_id, operation, initialize: false)
          case operation.to_s
          when 'add' then @members << actor_id.to_i unless @members.include?(actor_id.to_i)
          when 'remove' then @members.delete(actor_id.to_i)
          end
          record(:party_change, actor_id: actor_id.to_i, operation: operation.to_s,
                                initialize: initialize ? true : false)
        end

        def change_gold(operation, amount)
          @gold += operation.to_s == 'subtract' ? -amount.to_i : amount.to_i
          @gold = 0 if @gold.negative?
          record(:gold_change, operation: operation.to_s, amount: amount.to_i)
        end
      end

      class IMapController
        def transfer(**_argumente) = raise(NotImplementedError, 'IMapController#transfer fehlt')
        def set_event_location(**_argumente) = raise(NotImplementedError, 'IMapController#set_event_location fehlt')
        def scroll(**_argumente) = raise(NotImplementedError, 'IMapController#scroll fehlt')
        def settings(**_argumente) = raise(NotImplementedError, 'IMapController#settings fehlt')
        def fog_tone(**_argumente) = raise(NotImplementedError, 'IMapController#fog_tone fehlt')
        def fog_opacity(**_argumente) = raise(NotImplementedError, 'IMapController#fog_opacity fehlt')
        def transparent(**_argumente) = raise(NotImplementedError, 'IMapController#transparent fehlt')
        def move_route(**_argumente) = raise(NotImplementedError, 'IMapController#move_route fehlt')
        def erase_event(**_argumente) = raise(NotImplementedError, 'IMapController#erase_event fehlt')
        def route_active? = raise(NotImplementedError, 'IMapController#route_active? fehlt')
      end

      class NullMapController < IMapController
        def transfer(**_argumente) = nil
        def set_event_location(**_argumente) = nil
        def scroll(**_argumente) = nil
        def settings(**_argumente) = nil
        def fog_tone(**_argumente) = nil
        def fog_opacity(**_argumente) = nil
        def transparent(**_argumente) = nil
        def move_route(**_argumente) = nil
        def erase_event(**_argumente) = nil
        def route_active? = false
      end

      class RecordingMapController < NullMapController
        include Recording

        %i[transfer set_event_location scroll settings fog_tone fog_opacity
           transparent move_route erase_event].each do |name|
          define_method(name) do |**argumente|
            record(name, argumente)
          end
        end

        def route_active? = @route_active == true
        attr_accessor :route_active
      end

      class IAnimationPort
        def play(**_argumente) = raise(NotImplementedError, 'IAnimationPort#play fehlt')
      end

      class NullAnimationPort < IAnimationPort
        def play(**_argumente) = nil
      end

      class RecordingAnimationPort < NullAnimationPort
        include Recording

        def play(**argumente) = record(:play, argumente)
      end

      class IPicturePort
        def show(**_argumente) = raise(NotImplementedError, 'IPicturePort#show fehlt')
        def move(**_argumente) = raise(NotImplementedError, 'IPicturePort#move fehlt')
        def rotate(**_argumente) = raise(NotImplementedError, 'IPicturePort#rotate fehlt')
        def tone(**_argumente) = raise(NotImplementedError, 'IPicturePort#tone fehlt')
        def erase(**_argumente) = raise(NotImplementedError, 'IPicturePort#erase fehlt')
      end

      class NullPicturePort < IPicturePort
        def show(**_argumente) = nil
        def move(**_argumente) = nil
        def rotate(**_argumente) = nil
        def tone(**_argumente) = nil
        def erase(**_argumente) = nil
      end

      class RecordingPicturePort < NullPicturePort
        include Recording

        %i[show move rotate tone erase].each do |name|
          define_method(name) do |**argumente|
            record(name, argumente)
          end
        end
      end

      class IWeatherPort
        def set(**_argumente) = raise(NotImplementedError, 'IWeatherPort#set fehlt')
      end

      class NullWeatherPort < IWeatherPort
        def set(**_argumente) = nil
      end

      class RecordingWeatherPort < NullWeatherPort
        include Recording

        def set(**argumente) = record(:set, argumente)
      end

      class ITransitionPort
        def prepare(**_argumente) = raise(NotImplementedError, 'ITransitionPort#prepare fehlt')
        def execute(**_argumente) = raise(NotImplementedError, 'ITransitionPort#execute fehlt')
        def busy? = raise(NotImplementedError, 'ITransitionPort#busy? fehlt')
      end

      class NullTransitionPort < ITransitionPort
        def prepare(**_argumente) = nil
        def execute(**_argumente) = nil
        def busy? = false
      end

      class RecordingTransitionPort < NullTransitionPort
        include Recording

        def prepare(**argumente) = record(:prepare, argumente)
        def execute(**argumente) = record(:execute, argumente)

        def busy? = @busy == true
        attr_accessor :busy
      end

      class IScreenEffectPort
        def tone(**_argumente) = raise(NotImplementedError, 'IScreenEffectPort#tone fehlt')
        def flash(**_argumente) = raise(NotImplementedError, 'IScreenEffectPort#flash fehlt')
        def shake(**_argumente) = raise(NotImplementedError, 'IScreenEffectPort#shake fehlt')
      end

      class NullScreenEffectPort < IScreenEffectPort
        def tone(**_argumente) = nil
        def flash(**_argumente) = nil
        def shake(**_argumente) = nil
      end

      class RecordingScreenEffectPort < NullScreenEffectPort
        include Recording

        %i[tone flash shake].each do |name|
          define_method(name) do |**argumente|
            record(name, argumente)
          end
        end
      end

      class IAudioService
        def play_bgm(**_argumente) = raise(NotImplementedError, 'IAudioService#play_bgm fehlt')
        def fade_out_bgm(**_argumente) = raise(NotImplementedError, 'IAudioService#fade_out_bgm fehlt')
        def play_bgs(**_argumente) = raise(NotImplementedError, 'IAudioService#play_bgs fehlt')
        def fade_out_bgs(**_argumente) = raise(NotImplementedError, 'IAudioService#fade_out_bgs fehlt')
        def memorize_bgm_bgs = raise(NotImplementedError, 'IAudioService#memorize_bgm_bgs fehlt')
        def restore_bgm_bgs = raise(NotImplementedError, 'IAudioService#restore_bgm_bgs fehlt')
        def play_me(**_argumente) = raise(NotImplementedError, 'IAudioService#play_me fehlt')
        def play_se(**_argumente) = raise(NotImplementedError, 'IAudioService#play_se fehlt')
        def stop_se(**_argumente) = raise(NotImplementedError, 'IAudioService#stop_se fehlt')
        def set_battle_bgm(**_argumente) = raise(NotImplementedError, 'IAudioService#set_battle_bgm fehlt')
        def set_battle_end_me(**_argumente) = raise(NotImplementedError, 'IAudioService#set_battle_end_me fehlt')
      end

      class NullAudioService < IAudioService
        def play_bgm(**_argumente) = nil
        def fade_out_bgm(**_argumente) = nil
        def play_bgs(**_argumente) = nil
        def fade_out_bgs(**_argumente) = nil
        def memorize_bgm_bgs = nil
        def restore_bgm_bgs = nil
        def play_me(**_argumente) = nil
        def play_se(**_argumente) = nil
        def stop_se(**_argumente) = nil
        def set_battle_bgm(**_argumente) = nil
        def set_battle_end_me(**_argumente) = nil
      end

      class RecordingAudioService < NullAudioService
        include Recording

        %i[play_bgm fade_out_bgm play_bgs fade_out_bgs play_me play_se stop_se
           set_battle_bgm set_battle_end_me].each do |name|
          define_method(name) do |**argumente|
            record(name, argumente)
          end
        end

        def memorize_bgm_bgs = record(:memorize_bgm_bgs)
        def restore_bgm_bgs = record(:restore_bgm_bgs)
      end

      class IBattleController
        def start(**_argumente) = raise(NotImplementedError, 'IBattleController#start fehlt')
        def result = raise(NotImplementedError, 'IBattleController#result fehlt')
        def enemy(**_argumente) = raise(NotImplementedError, 'IBattleController#enemy fehlt')
        def enemy_present?(_slot) = raise(NotImplementedError, 'IBattleController#enemy_present? fehlt')
        def force_action(**_argumente) = raise(NotImplementedError, 'IBattleController#force_action fehlt')
        def abort = raise(NotImplementedError, 'IBattleController#abort fehlt')
      end

      class NullBattleController < IBattleController
        def start(**_argumente) = nil
        def result = 'win'
        def enemy(**_argumente) = nil
        def enemy_present?(slot) = slot.to_i >= 0
        def force_action(**_argumente) = nil
        def abort = nil
      end

      class RecordingBattleController < NullBattleController
        include Recording

        def initialize(result: 'win', alive_slots: [0])
          @result = result
          @alive_slots = alive_slots.map(&:to_i)
        end

        def start(**argumente) = record(:start, argumente)
        def result = @result
        def enemy(**argumente) = record(:enemy, argumente)
        def enemy_present?(slot) = @alive_slots.include?(slot.to_i)
        def force_action(**argumente) = record(:force_action, argumente)
        def abort = record(:abort)
      end

      class IUINavigationService
        def message(**_argumente) = raise(NotImplementedError, 'IUINavigationService#message fehlt')
        def text_options(**_argumente) = raise(NotImplementedError, 'IUINavigationService#text_options fehlt')
        def choices(**_argumente) = raise(NotImplementedError, 'IUINavigationService#choices fehlt')
        def input_number(**_argumente) = raise(NotImplementedError, 'IUINavigationService#input_number fehlt')
        def button_input = raise(NotImplementedError, 'IUINavigationService#button_input fehlt')
        def name_input(**_argumente) = raise(NotImplementedError, 'IUINavigationService#name_input fehlt')
        def shop(**_argumente) = raise(NotImplementedError, 'IUINavigationService#shop fehlt')
        def menu = raise(NotImplementedError, 'IUINavigationService#menu fehlt')
        def save = raise(NotImplementedError, 'IUINavigationService#save fehlt')
        def game_over = raise(NotImplementedError, 'IUINavigationService#game_over fehlt')
        def title = raise(NotImplementedError, 'IUINavigationService#title fehlt')
        def last_button = raise(NotImplementedError, 'IUINavigationService#last_button fehlt')
        def busy? = raise(NotImplementedError, 'IUINavigationService#busy? fehlt')
      end

      class NullUINavigationService < IUINavigationService
        def message(**_argumente) = nil
        def text_options(**_argumente) = nil
        def choices(**_argumente) = 0
        def input_number(**_argumente) = 0
        def button_input = 0
        def name_input(**_argumente) = ''
        def shop(**_argumente) = nil
        def menu = nil
        def save = nil
        def game_over = nil
        def title = nil
        def last_button = 0
        def busy? = false
      end

      class RecordingUINavigationService < NullUINavigationService
        include Recording

        def initialize(choice: 0, number: 0, name: 'Kael', button: 11, busy: false)
          @choice = choice
          @number = number
          @name = name
          @button = button
          @busy = busy
        end

        def message(**argumente) = record(:message, argumente)
        def text_options(**argumente) = record(:text_options, argumente)
        def choices(**argumente) = record(:choices, argumente) && @choice
        def input_number(**argumente) = record(:input_number, argumente) && @number
        def button_input = record(:button_input) && @button
        def name_input(**argumente) = record(:name_input, argumente) && @name
        def shop(**argumente) = record(:shop, argumente)
        def menu = record(:menu)
        def save = record(:save)
        def game_over = record(:game_over)
        def title = record(:title)
        def last_button = @button
        def busy? = @busy == true

        # Wie bei Karte und Übergang darf ein Test das Fenster schließen.
        attr_accessor :busy
      end

      class IScriptPort
        def run(**_argumente) = raise(NotImplementedError, 'IScriptPort#run fehlt')
      end

      class NullScriptPort < IScriptPort
        def run(**_argumente) = nil
      end

      class RecordingScriptPort < NullScriptPort
        include Recording

        def run(**argumente) = record(:run, argumente)
      end

      # Ein Port für alle zwölf Heldenbefehle. Die benannten Methoden sind die
      # typisierte Sicht; wer nur change() implementiert, bleibt gültig.
      class IActorPort
        COMMANDS = %w[change_hp change_sp change_state recover_all change_exp change_level
                      change_parameters change_skills change_equipment change_actor_name
                      change_actor_class change_actor_graphic].freeze

        def change(**_argumente) = raise(NotImplementedError, 'IActorPort#change fehlt')

        COMMANDS.each do |name|
          define_method(name) do |**argumente|
            change(command: name, params: argumente)
          end
        end
      end

      class NullActorPort < IActorPort
        def change(**_argumente) = nil
      end

      class RecordingActorPort < NullActorPort
        include Recording

        def change(**argumente) = record(:change, argumente)
      end

      # Laufzeit-System aus Tab 1: Fenstergrafik und die drei Zugriffs sperren.
      class ISystemSettingsService
        def windowskin(**_argumente) = raise(NotImplementedError, 'ISystemSettingsService#windowskin fehlt')
        def save_access(**_argumente) = raise(NotImplementedError, 'ISystemSettingsService#save_access fehlt')
        def menu_access(**_argumente) = raise(NotImplementedError, 'ISystemSettingsService#menu_access fehlt')
        def encounter(**_argumente) = raise(NotImplementedError, 'ISystemSettingsService#encounter fehlt')
      end

      class NullSystemSettingsService < ISystemSettingsService
        def windowskin(**_argumente) = nil
        def save_access(**_argumente) = nil
        def menu_access(**_argumente) = nil
        def encounter(**_argumente) = nil
      end

      class RecordingSystemSettingsService < NullSystemSettingsService
        include Recording

        %i[windowskin save_access menu_access encounter].each do |name|
          define_method(name) { |**argumente| record(name, argumente) }
        end
      end

      # Die in der Architektur genannten Service-Namen sind Aliase der Ports:
      # eine Schnittstelle, zwei lesbare Namen, keine doppelte Implementierung.
      IInventoryService = IInventoryPort
      IPartyService = IPartyPort
      IPictureService = IPicturePort
      IScreenEffectService = IScreenEffectPort
      IActorService = IActorPort
      ITimerService = ITimerPort
      IAnimationService = IAnimationPort
      IWeatherService = IWeatherPort
      ITransitionService = ITransitionPort
      IScriptService = IScriptPort

      PortSet = Data.define(:switches, :variables, :self_switches, :timer,
                            :inventory, :party, :actors, :map, :animation,
                            :pictures, :weather, :transition, :screen, :audio,
                            :battle, :ui, :script, :system_settings, :dispatcher) do
        def initialize(switches: NullSwitchStore.new,
                       variables: NullVariableStore.new,
                       self_switches: NullSelfSwitchStore.new,
                       timer: NullTimerPort.new,
                       inventory: NullInventoryPort.new,
                       party: NullPartyPort.new,
                       actors: NullActorPort.new,
                       map: NullMapController.new,
                       animation: NullAnimationPort.new,
                       pictures: NullPicturePort.new,
                       weather: NullWeatherPort.new,
                       transition: NullTransitionPort.new,
                       screen: NullScreenEffectPort.new,
                       audio: NullAudioService.new,
                       battle: NullBattleController.new,
                       ui: NullUINavigationService.new,
                       script: NullScriptPort.new,
                       system_settings: NullSystemSettingsService.new,
                       dispatcher: Rpg::Ports::EventDispatcher.new)
          super(switches: switches, variables: variables, self_switches: self_switches,
                timer: timer, inventory: inventory, party: party, actors: actors, map: map,
                animation: animation, pictures: pictures, weather: weather,
                transition: transition, screen: screen, audio: audio, battle: battle,
                ui: ui, script: script, system_settings: system_settings,
                dispatcher: dispatcher)
        end
      end
    end
  end
end
