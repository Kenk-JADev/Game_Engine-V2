#encoding: utf-8
# ============================================================================
#  Rpg::Runtime* - veränderliche Laufzeitinstanzen.
#
#  Die Vorlagen in Rpg::Models bleiben unverändert. Kämpfe, Karten und
#  Spielstände arbeiten ausschliesslich mit den Klassen in dieser Datei.
# ============================================================================

module Rpg
  # Ein aktiver Zustand ist selbst ein unveränderlicher Wert; Verlängerungen
  # erzeugen eine neue Instanz statt das Template oder den alten Wert zu ändern.
  ActiveState = Data.define(:state_id, :remaining_turns, :remaining_steps, :step_limit) do
    prepend Support::Immutable

    # `remaining_steps == 0` bedeutet "kein Schritt-Limit", solange
    # `step_limit` false ist. Nur explizit gesetzte Schrittlimits zählen.
    def initialize(state_id:, remaining_turns:, remaining_steps: 0, step_limit: nil)
      super
    end

    def advance(turns: 0, steps: 0)
      limit = step_limit ? true : nil
      naechste_schritte = limit ? remaining_steps - steps : 0
      self.class.new(
        state_id: state_id,
        remaining_turns: remaining_turns - turns,
        remaining_steps: naechste_schritte,
        step_limit: limit
      )
    end

    def expired? = remaining_turns <= 0 || (step_limit ? remaining_steps <= 0 : false)
  end

  class RuntimeActor
    MAX_STATES = 8
    SLOTS = %w[weapon shield helmet body accessory].freeze
    PARAMETER = %w[maxhp maxsp str dex agi int atk pdef mdef eva].freeze

    attr_reader :template, :class_id, :level, :hp, :sp, :states, :equipment, :bonuses

    def initialize(template:, class_id:, level: nil, hp: nil, sp: nil,
                   equipment: nil, states: nil, bonuses: nil)
      @template = template
      @class_id = class_id.to_i
      @level = [[Support.integer(level) || template.initial_level, 1].max,
                template.final_level].min
      @states = Array(states).dup
      @bonuses = (bonuses || {}).dup
      @equipment = {}
      @fixed_slots = {}

      template.start_equipment.each do |ref|
        @equipment[ref.slot] = ref.item_id
        @fixed_slots[ref.slot] = ref.fixed
      end
      (equipment || {}).each { |slot, item_id| @equipment[slot.to_s] = item_id }

      @hp = [[Support.integer(hp) || maxhp, 0].max, maxhp].min
      @sp = [[Support.integer(sp) || maxsp, 0].max, maxsp].min
    end

    def id = @template.id
    def name = @template.name

    def base_stat(key)
      curve = @template.growth.fetch(key.to_sym)
      ((curve.base || 0) + (curve.per_level || 0) * (@level - 1)).round
    end

    %i[maxhp maxsp str dex agi int].each do |key|
      define_method(key) { [0, base_stat(key) + (@bonuses[key.to_s] || 0).to_i].max }
    end

    # Helden haben keinen eigenen ATK-Grundwert in der Vorlage; er folgt aus
    # der Stärke, während Ausrüstung separat vom EquipmentStatAggregator kommt.
    def atk = str
    def pdef = 0
    def mdef = 0
    def eva = 0

    def hp=(wert)
      @hp = [[Support.integer(wert) || 0, 0].max, maxhp].min
    end

    def sp=(wert)
      @sp = [[Support.integer(wert) || 0, 0].max, maxsp].min
    end

    def alive? = @hp.positive?

    def level_up!
      return false if @level >= @template.final_level

      altes_max = maxhp
      altes_sp = maxsp
      @level += 1
      @hp += maxhp - altes_max
      @sp += maxsp - altes_sp
      true
    end

    def fixed_slot?(slot) = !!@fixed_slots[slot.to_s]

    def equip(slot, item_id)
      return false if fixed_slot?(slot)
      return false unless SLOTS.include?(slot.to_s)

      @equipment[slot.to_s] = item_id.nil? || item_id.zero? ? nil : item_id.to_i
      true
    end

    def add_state(state_id, turns: 3, steps: 0)
      id = state_id.to_i
      vorhanden = @states.find { |zustand| zustand.state_id == id }
      if vorhanden
        @states[@states.index(vorhanden)] = ActiveState.new(
          state_id: id,
          remaining_turns: [vorhanden.remaining_turns, turns.to_i].max,
          remaining_steps: [vorhanden.remaining_steps, steps.to_i].max,
          step_limit: steps.to_i.positive? || vorhanden.step_limit
        )
        return true
      end
      return false if @states.size >= MAX_STATES

      @states << ActiveState.new(state_id: id, remaining_turns: turns.to_i,
                                 remaining_steps: steps.to_i,
                                 step_limit: steps.to_i.positive?)
      true
    end

    def remove_state(state_id)
      @states.reject! { |zustand| zustand.state_id == state_id.to_i }
    end

    def state_ids = @states.map(&:state_id).freeze

    def apply_param_inc(parameter, amount)
      schluessel = parameter.to_s
      return false unless PARAMETER.include?(schluessel)

      @bonuses[schluessel] = (@bonuses[schluessel] || 0).to_i + amount.to_i
      true
    end

    def snapshot
      {
        'actor_id' => id, 'class_id' => @class_id, 'level' => @level,
        'hp' => @hp, 'sp' => @sp, 'equipment' => @equipment.dup,
        'bonuses' => @bonuses.dup,
        'states' => @states.map { |s| [s.state_id, s.remaining_turns, s.remaining_steps] }
      }.freeze
    end
  end

  class RuntimeEnemy
    MAX_STATES = 8

    attr_reader :template, :hp, :sp, :states

    def initialize(template:, hp: nil, sp: nil, states: nil)
      @template = template
      @hp = [[Support.integer(hp) || template.maxhp, 0].max, template.maxhp].min
      @sp = [[Support.integer(sp) || template.maxsp, 0].max, template.maxsp].min
      @states = Array(states).dup
    end

    def id = @template.id
    def name = @template.name
    %i[maxhp maxsp str dex agi int atk pdef mdef eva exp gold].each do |key|
      define_method(key) { @template.public_send(key) }
    end

    def hp=(wert)
      @hp = [[Support.integer(wert) || 0, 0].max, maxhp].min
    end

    def sp=(wert)
      @sp = [[Support.integer(wert) || 0, 0].max, maxsp].min
    end

    def alive? = @hp.positive?

    def add_state(state_id, turns: 3, steps: 0)
      id = state_id.to_i
      vorhanden = @states.find { |zustand| zustand.state_id == id }
      if vorhanden
        @states[@states.index(vorhanden)] = ActiveState.new(
          state_id: id,
          remaining_turns: [vorhanden.remaining_turns, turns.to_i].max,
          remaining_steps: [vorhanden.remaining_steps, steps.to_i].max,
          step_limit: steps.to_i.positive? || vorhanden.step_limit
        )
        return true
      end
      return false if @states.size >= MAX_STATES

      @states << ActiveState.new(state_id: id, remaining_turns: turns.to_i,
                                 remaining_steps: steps.to_i,
                                 step_limit: steps.to_i.positive?)
      true
    end

    def remove_state(state_id)
      @states.reject! { |zustand| zustand.state_id == state_id.to_i }
    end

    def state_ids = @states.map(&:state_id).freeze

    def snapshot
      {
        'enemy_id' => id, 'hp' => @hp, 'sp' => @sp,
        'states' => @states.map { |s| [s.state_id, s.remaining_turns, s.remaining_steps] }
      }.freeze
    end
  end

  # Eine konkrete Kachel auf einer Karte. Die TileProperty aus dem Tileset
  # bleibt Vorlage; Overrides gehören zum laufenden Spiel/dem Editorzustand.
  class MapTileState
    attr_reader :tile_property, :tileset_id, :index

    def initialize(tile_property:, tileset_id:, overrides: nil)
      @tile_property = tile_property
      @tileset_id = tileset_id.to_i
      @index = tile_property.index
      @overrides = (overrides || {}).dup
    end

    def passable?
      @overrides.key?(:passable) ? !!@overrides[:passable] : @tile_property.passable
    end

    def passable=(wert)
      @overrides[:passable] = !!wert
    end

    def passable_4dirs
      (@overrides[:passable_4dirs] || @tile_property.passable_4dirs).dup.freeze
    end

    def passable_4dirs=(werte)
      @overrides[:passable_4dirs] = Array(werte).map { |w| !!w }
    end

    def priority = @overrides.fetch(:priority, @tile_property.priority)

    def priority=(wert)
      @overrides[:priority] = wert.to_i
    end

    def bush? = @overrides.fetch(:bush, @tile_property.bush)

    def bush=(wert)
      @overrides[:bush] = !!wert
    end

    def counter? = @overrides.fetch(:counter, @tile_property.counter)

    def counter=(wert)
      @overrides[:counter] = !!wert
    end

    def terrain_tag = @overrides.fetch(:terrain_tag, @tile_property.terrain_tag)

    def terrain_tag=(wert)
      @overrides[:terrain_tag] = wert.to_i
    end

    def reset!
      @overrides.clear
      self
    end

    def snapshot = @overrides.dup.freeze
  end

  # Erzeugt Laufzeitinstanzen aus einer explizit übergebenen Datenbank.
  class RuntimeFactory
    def initialize(database)
      @database = database
    end

    def actor(actor_id, level: nil)
      vorlage = @database.actors.find(actor_id)
      return nil unless vorlage

      RuntimeActor.new(template: vorlage, class_id: vorlage.class_id, level: level)
    end

    def enemy(enemy_id)
      vorlage = @database.enemies.find(enemy_id)
      return nil unless vorlage

      RuntimeEnemy.new(template: vorlage)
    end

    def party(ids = nil)
      Array(ids || @database.system.initial_party).filter_map { |id| actor(id) }
    end

    def tile(tileset_id, index)
      vorlage = @database.tilesets.find(tileset_id)
      return nil unless vorlage

      eigenschaft = vorlage.tile_properties.find { |p| p.index == index.to_i }
      return nil unless eigenschaft

      MapTileState.new(tile_property: eigenschaft, tileset_id: vorlage.id)
    end

    def troop_members(troop_id)
      truppe = @database.troops.find(troop_id)
      return [] unless truppe

      truppe.members.filter_map { |member| [member, enemy(member.enemy_id)] if enemy(member.enemy_id) }
    end
  end
end
