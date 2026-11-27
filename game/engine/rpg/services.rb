#encoding: utf-8
# ============================================================================
#  Rpg-Services -(testbare) Fachlogik ohne UI und ohne Rendering.
#
#  Jeder Service bekommt seine Abhängigkeiten im Konstruktor. Es gibt keine
#  globalen $data_*/$game_*-Zugriffe und keine direkten Audio-/Grafikaufrufe.
# ============================================================================

module Rpg
  # Summiert Ausrüstungsboni. Die Vorlagen werden nur gelesen; der Helden-
  # Zustand bleibt in RuntimeActor.
  class EquipmentStatAggregator
    KEYS = %i[atk pdef mdef eva str dex agi int].freeze

    def initialize(database:)
      @database = database
    end

    def stats(actor:)
      ergebnis = KEYS.to_h { |key| [key, 0] }
      return ergebnis unless actor.respond_to?(:equipment)

      actor.equipment.each_value do |item_id|
        next if item_id.nil? || item_id.to_i.zero?

        gegenstand = @database.weapons.find(item_id) || @database.armors.find(item_id)
        next unless gegenstand

        KEYS.each do |key|
          ergebnis[key] += gegenstand.public_send(key) if gegenstand.respond_to?(key)
          bonus_name = :"#{key}_plus"
          ergebnis[key] += gegenstand.public_send(bonus_name) if gegenstand.respond_to?(bonus_name)
        end
      end
      ergebnis
    end
  end

  # Löst aktive Zustände auf: Stat-Modifikatoren, Restriktionen, Element- und
  # Statuseffizienz sowie Runden-Ticks. Zustände sind unveränderlich; ein Tick
  # erzeugt neue ActiveState-Instanzen.
  class StateResolver
    RATE_KEYS = %i[maxhp maxsp str dex agi int atk pdef mdef eva].freeze

    def initialize(database:, dispatcher: nil)
      @database = database
      @dispatcher = dispatcher
    end

    def multipliers(battler)
      ergebnis = RATE_KEYS.to_h { |key| [key, key == :eva ? 0.0 : 1.0] }
      each_state(battler) do |zustand|
        zustand.rates.each do |key, rate|
          if key == :eva
            ergebnis[:eva] += rate.to_f
          else
            ergebnis[key] *= rate.to_f / 100.0
          end
        end
      end
      ergebnis
    end

    # Klassen-/Gegner-Effizienz mal Zustands-Mitigierung (z. B. Schutzschild).
    def element_efficiency(target, element_id)
      id = element_id.to_i
      basis = if target.respond_to?(:class_id)
                klasse = @database.classes.find(target.class_id)
                klasse ? klasse.element_multipliers.fetch(id, 1.0) : 1.0
              elsif target.respond_to?(:template) && target.template.respond_to?(:element_multipliers)
                target.template.element_multipliers.fetch(id, 1.0)
              else
                1.0
              end
      each_state(target) do |zustand|
        basis *= 0.5 if zustand.guard_element_ids.include?(id)
      end
      basis
    end

    def state_efficiency(target, state_id)
      id = state_id.to_i
      basis = if target.respond_to?(:class_id)
                klasse = @database.classes.find(target.class_id)
                klasse ? klasse.state_multipliers.fetch(id, 1.0) : 1.0
              elsif target.respond_to?(:template) && target.template.respond_to?(:state_multipliers)
                target.template.state_multipliers.fetch(id, 1.0)
              else
                1.0
              end
      each_state(target) do |zustand|
        basis *= 0.5 if zustand.minus_state_ids.include?(id)
      end
      basis
    end

    def restrictions(battler)
      each_state(battler).map(&:restriction)
    end

    def can_move?(battler) = !restrictions(battler).include?(4)
    def can_act?(battler) = restrictions(battler).none? { |r| [2, 3, 4].include?(r) }
    def can_use_skill?(battler) = !restrictions(battler).include?(1) && can_act?(battler)

    def apply(battler, state_id, turns: 3, steps: 0)
      erfolg = battler.add_state(state_id, turns: turns, steps: steps)
      @dispatcher&.publish('state.applied', target: battler, state_id: state_id.to_i,
                           success: erfolg)
      erfolg
    end

    def tick(battler)
      vorher = battler.state_ids.dup
      neue = battler.states.map { |zustand| zustand.advance(turns: 1) }
      battler.states.replace(neue.reject(&:expired?))
      @dispatcher&.publish('state.ticked', target: battler, before: vorher,
                           after: battler.state_ids.dup)
      battler.state_ids
    end

    private

    def each_state(battler)
      return enum_for(:each_state, battler) unless block_given?
      return unless battler.respond_to?(:states)

      battler.states.each do |aktiv|
        zustand = @database.states.find(aktiv.state_id)
        yield zustand if zustand
      end
    end
  end

  # Reine Schadens-/Heilformel. Keine Kampfscene, kein Zufall ohne Injektion.
  class DamageCalculator
    def initialize(state_resolver:, equipment_aggregator:, random: nil)
      @state_resolver = state_resolver
      @equipment = equipment_aggregator
      @random = random || ->(minimum, maximum) { rand(minimum..maximum) }
    end

    def compute(skill:, attacker:, target:)
      treffer = @random.call(0.0, 100.0) <= skill.hit.to_f
      return { hit: false, damage: 0, variance: [0.0, 0.0] } unless treffer

      angreifer_bonus = @equipment.stats(actor: attacker)
      ziel_bonus = @equipment.stats(actor: target)
      angriff = stat(attacker, :atk) + angreifer_bonus[:atk]
      attribute = %i[str dex agi int].to_h do |key|
        [key, stat(attacker, key) + angreifer_bonus[key]]
      end
      verteidigung = stat(target, :pdef) + ziel_bonus[:pdef]
      magie = stat(target, :mdef) + ziel_bonus[:mdef]

      basis = skill.power.to_f
      basis += angriff * skill.atk_f.to_f / 100.0
      attribute.each { |key, wert| basis += wert * skill.public_send(:"#{key}_f").to_f / 100.0 }
      basis *= [0.1, 1.0 - verteidigung * skill.pdef_f.to_f / 10_000.0].max
      basis *= [0.1, 1.0 - magie * skill.mdef_f.to_f / 10_000.0].max

      element = skill.element_ids.empty? ? 1.0 :
                skill.element_ids.map { |id| @state_resolver.element_efficiency(target, id) }.max
      basis *= element.to_f
      basis *= @state_resolver.multipliers(attacker)[:atk]
      basis /= [0.1, @state_resolver.multipliers(target)[:pdef]].max

      streuung = skill.variance.to_f
      minimum = [basis * (100.0 - streuung) / 100.0, 0.0].max
      maximum = [minimum, basis * (100.0 + streuung) / 100.0].max
      schaden = @random.call(minimum, maximum).round
      { hit: true, damage: [schaden, 1].max, element: element, variance: [minimum, maximum] }
    end

    def heal(skill:, user:, target:)
      kraft = stat(user, :int) + @equipment.stats(actor: user)[:int]
      basis = skill.power.to_f + kraft * skill.int_f.to_f / 100.0
      streuung = skill.variance.to_f
      minimum = basis * (100.0 - streuung) / 100.0
      maximum = [minimum, basis * (100.0 + streuung) / 100.0].max
      betrag = @random.call(minimum, maximum).round
      [betrag, 0].max
    end

    private

    def stat(battler, key)
      return 0.0 unless battler.respond_to?(key)

      battler.public_send(key).to_f
    end
  end

  # XP-nahe, aber eigene EXP-Kurve: Basis und Inflation steuern zwei Terme.
  class CombatFormulaService
    def initialize(database:, state_resolver:, equipment_aggregator:,
                   calculator: nil, dispatcher: nil, random: nil)
      @database = database
      @state_resolver = state_resolver
      @equipment = equipment_aggregator
      @dispatcher = dispatcher
      @calculator = calculator || DamageCalculator.new(
        state_resolver: state_resolver, equipment_aggregator: equipment_aggregator,
        random: random
      )
    end

    def damage(skill:, attacker:, target:)
      ergebnis = @calculator.compute(skill: skill, attacker: attacker, target: target)
      @dispatcher&.publish('damage.calculated', skill_id: skill.id,
                           attacker: attacker, target: target, result: ergebnis)
      ergebnis
    end

    def heal(skill:, user:, target:)
      betrag = @calculator.heal(skill: skill, user: user, target: target)
      @dispatcher&.publish('heal.calculated', skill_id: skill.id,
                           target: target, amount: betrag)
      betrag
    end

    def exp_for_level(actor, level)
      vorlage = template_for(actor)
      return 0 unless vorlage

      ziel = [[level.to_i, 1].max, vorlage.final_level].min
      gesamt = 0.0
      2.upto(ziel) do |stufe|
        n = stufe - 1
        gesamt += vorlage.exp_basis.to_f * (n**2.2)
        gesamt += vorlage.exp_inflation.to_f * (n**1.55)
      end
      gesamt.round
    end

    def level_for_exp(actor, exp)
      vorlage = template_for(actor)
      return 1 unless vorlage

      gesucht = exp.to_i
      stufe = 1
      2.upto(vorlage.final_level) do |kandidat|
        stufe = kandidat if exp_for_level(vorlage, kandidat) <= gesucht
      end
      stufe
    end

    private

    def template_for(actor)
      return actor if actor.is_a?(Models::Actor)
      return actor.template if actor.is_a?(RuntimeActor)

      @database.actors.find(actor)
    end
  end

  # Tile-Abfragen: Passierbarkeit (gesamt und 4 Richtungen), Priorität,
  # Busch/Tresen und Terrain-Tag – alles aus unveränderlichen Tileset-Daten.
  class TileQueryService
    RICHTUNGEN = %w[down left right up].freeze

    def initialize(database:, dispatcher: nil)
      @database = database
      @dispatcher = dispatcher
    end

    def property(tileset_id, index)
      tileset = @database.tilesets.find(tileset_id)
      return nil unless tileset

      tileset.tile_properties.find { |eigenschaft| eigenschaft.index == index.to_i }
    end

    def query(tileset_id, index, state: nil)
      eigenschaft = property(tileset_id, index)
      return nil unless eigenschaft

      ergebnis = if state
                   {
                     passable: state.passable?, passable_4dirs: state.passable_4dirs,
                     priority: state.priority, bush: state.bush?,
                     counter: state.counter?, terrain_tag: state.terrain_tag
                   }
                 else
                   {
                     passable: eigenschaft.passable,
                     passable_4dirs: eigenschaft.passable_4dirs,
                     priority: eigenschaft.priority, bush: eigenschaft.bush,
                     counter: eigenschaft.counter, terrain_tag: eigenschaft.terrain_tag
                   }
                 end
      @dispatcher&.publish('tile.queried', tileset_id: tileset_id.to_i,
                           index: index.to_i, result: ergebnis)
      ergebnis
    end

    def passable?(tileset_id, index, direction: nil, state: nil)
      daten = query(tileset_id, index, state: state)
      return false unless daten
      return daten[:passable] if direction.nil?

      richtung = direction.is_a?(String) ? RICHTUNGEN.index(direction) : direction.to_i
      return false unless richtung&.between?(0, 3)

      daten[:passable_4dirs][richtung]
    end

    def priority(tileset_id, index, state: nil)
      query(tileset_id, index, state: state)&.fetch(:priority, 0)
    end

    def bush?(tileset_id, index, state: nil)
      !!query(tileset_id, index, state: state)&.fetch(:bush, false)
    end

    def counter?(tileset_id, index, state: nil)
      !!query(tileset_id, index, state: state)&.fetch(:counter, false)
    end

    def terrain_tag(tileset_id, index, state: nil)
      query(tileset_id, index, state: state)&.fetch(:terrain_tag, 0)
    end
  end

  # Übersetzt Cue-Namen (z. B. :jump) in Asset-Keys und ruft das injizierte
  # AudioPort auf. Die Datenbank nennt nur Keys, nie Dateien oder Formate.
  class AudioService
    def initialize(database:, audio_port:, dispatcher: nil)
      @database = database
      @port = audio_port
      @dispatcher = dispatcher
    end

    def key_for(cue)
      @database.system.sound_cues[cue.to_s]
    end

    def play(cue)
      key = key_for(cue)
      unless key
        @dispatcher&.publish('audio.cue_missing', cue: cue.to_s)
        return false
      end
      @port.play(key)
      @dispatcher&.publish('audio.played', cue: cue.to_s, key: key, kind: kind_for(key))
      true
    end

    def kind_for(key)
      case key.to_s.split('/').first
      when 'bgm', 'bgs' then :bgm
      when 'me' then :me
      when 'se' then :se
      else :unknown
      end
    end
  end

  # Führt Event-Befehle über registrierte Handler aus. Handler sind normale
  # Callables; der Service weiß nichts über Fenster, Karten oder Grafiken.
  class EventExecutionService
    MAX_DEPTH = 10

    def initialize(database:, dispatcher:, audio_service: nil)
      @database = database
      @dispatcher = dispatcher
      @audio = audio_service
      @handlers = {}
      register_defaults
    end

    def register(code, &handler)
      raise ArgumentError, 'Event-Handler braucht einen Block' unless handler

      @handlers[code.to_i] = handler
      self
    end

    def execute(commands, context: {}, depth: 0)
      return [] if depth > MAX_DEPTH

      Array(commands).filter_map do |befehl|
        handler = @handlers[befehl.code.to_i]
        if handler
          handler.call(befehl.params, context, self, depth)
        else
          @dispatcher.publish('command.unhandled', code: befehl.code,
                              params: befehl.params, context: context)
          nil
        end
      end
    end

    def run_common_event(id, context: {}, depth: 0)
      ereignis = @database.common_events.find(id)
      unless ereignis
        @dispatcher.publish('common_event.missing', id: id.to_i)
        return []
      end
      if depth > MAX_DEPTH
        @dispatcher.publish('common_event.recursion_limit', id: ereignis.id)
        return []
      end

      @dispatcher.publish('common_event.started', id: ereignis.id, name: ereignis.name)
      ergebnis = execute(ereignis.commands,
                         context: context.merge(common_event_id: ereignis.id),
                         depth: depth + 1)
      @dispatcher.publish('common_event.finished', id: ereignis.id, result: ergebnis)
      ergebnis
    end

    private

    def register_defaults
      register(101) do |params, context, _service, _depth|
        @dispatcher.publish('message.shown', text: params['text'].to_s, context: context)
      end

      register(117) do |params, context, service, depth|
        service.run_common_event(params['common_event_id'], context: context, depth: depth)
      end

      register(121) do |params, context, _service, _depth|
        @dispatcher.publish('switch.changed', id: params['switch_id'].to_i,
                            value: !!params['value'], context: context)
      end

      register(122) do |params, context, _service, _depth|
        @dispatcher.publish('variable.changed', id: params['variable_id'].to_i,
                            operation: params['operation'].to_s,
                            value: params['value'], context: context)
      end

      register(250) do |params, _context, _service, _depth|
        @audio&.play(params['cue'])
      end
    end
  end
end
