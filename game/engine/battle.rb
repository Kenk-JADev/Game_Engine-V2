#encoding: utf-8
# ============================================================================
#  Kampf-Grundgerüst (Etappe 19i, RPG Maker XP: Scene_Battle 1-4).
#
#  Rundenkampf Party gegen eine Gegnergruppe aus der Datenbank:
#
#    database.json:
#      "enemies": [{ "id":"goblin", "name":"Goblin", "hp":35, "atk":9,
#                    "def":4, "agi":7, "exp":12, "gold":8,
#                    "drop":"heiltrank", "drop_rate":0.3,
#                    "color":13209632 }],
#      "troops":  [{ "id":"waldweg", "name":"Waldweg",
#                    "enemies":["goblin","wolf"] }],
#      "skills":  [{ "id":"feuerball", "name":"Feuerball",
#                    "kind":"damage|heal|buff", "scope":"enemy|ally|self",
#                    "mp":6, "power":16, "state":"brand",
#                    "state_rate":0.4, "turns":2 }],
#      "states":  [{ "id":"gift", "name":"Gift",
#                    "kind":"poison|regen|atk_up|def_up|...",
#                    "power":6, "turns":3 }]
#    Helden lernen Fertigkeiten über "skills": ["feuerball", ...] im
#    Helden-Eintrag (Etappe 20a).
#
#  Ablauf (wie XP: Befehlsphase -> Aktionsphase -> Gegnerzug):
#    1. start(troop_id) stellt Gegner auf, merkt can_escape/can_lose.
#    2. Befehlsphase: jeder lebende Held bekommt Angriff/Fertigkeit/Item/
#       Flucht (XP-Reihenfolge); Aktionen sammeln sich in @actions.
#    3. execute: Heldenaktionen wirken (nach Agilität), dann ziehen alle
#       Gegner (zufälliger lebender Held), dann Zustände ticken (Gift/
#       Regeneration, Dauer -1), dann Sieg-/Niederlage-Prüfung.
#    4. Sieg: Exp/Gold/Drop an die Party, Ende. Niederlage: gameover-Flag
#       ($game_temp.gameover -> Scene_Gameover, Taste -> Titel), außer can_lose.
#
#  Event-Befehl: {"cmd":"battle","name":"waldweg","can_escape":true,
#                 "can_lose":false} - der Interpreter blockiert bis zum Ende.
#  Ruby: Battle.start('waldweg') bzw. Context-Helfer battle('waldweg').
#
#  Schaden (XP-Grundform, vereinfacht): atk - def/2 des Ziels, ±15 %
#  Streuung, mindestens 1. Kritik: agi/64-Chance (XP-Formel), x3-Schaden
#  beim Angriff (Helden), +25 % beim Gegner-Aggressionswert.
#
#  Fertigkeiten (20a): kind damage (Schaden + optionale Zustandschance),
#  heal (Heilung über Kraft + halbe Weisheit) und buff (Zustand auf das
#  Ziel). Kosten werden beim Ausführen abgebucht; zu wenig MP verweigert
#  schon die Auswahl. Zustände geben atk/def/spi/agi-Boni (Wut/Schutz) und
#  wirken am Rundenende (Gift/Regeneration).
#
#  Grenzen (Backlog): keine Gegner-Fertigkeiten, keine Skill-Bedingungen
#  (XP: Skill-Guards), keine Heilung von KO-Helden per Skill, keine
#  Kampfformationen/Positionen, Gegner ziehen immer "Angriff auf zufälligen
#  Held", kein Flucht-Textfenster, Kampfhintergrund = abgedunkelte Karte.
#
#  Kopflos testbar: ruby -I game/engine tools/battle_test.rb
#                  ruby -I game/engine tools/skills_test.rb
# ============================================================================

module Battle
  # Ein Kämpfer (XP: Game_Battler - hier Helden wie Gegner in einer Klasse,
  # actor?/enemy? trennt sie).
  class Battler
    attr_accessor :name, :id
    attr_reader :actor, :defi

    def initialize(id:, name:, hp:, mp: 0, max_hp: nil, max_mp: nil,
                   atk: 10, def_: 8, spi: 5, agi: 8, actor: false, defi: nil)
      @id = id.to_s
      @name = name.to_s
      @max_hp = [(max_hp.nil? ? hp : max_hp).to_i, 1].max
      @max_mp = [(max_mp.nil? ? mp : max_mp).to_i, 0].max
      @hp = [[hp.to_i, 0].max, @max_hp].min
      @mp = [[mp.to_i, 0].max, @max_mp].min
      @atk = atk.to_i
      @def = def_.to_i
      @spi = spi.to_i
      @agi = agi.to_i
      @actor = actor
      @defi = defi          # Party::Actor bei Helden, Enemy-Hash bei Gegnern
      @states = []          # 20a: [{ id:, turns: }] (XP: Game_Battler#states)
      @hidden = false       # Etappe 48: XP 335 Enemy Appearance (versteckt)
    end

    # Etappe 48: XP 335/336 - Sichtbarkeit und Verwandlung. Eine Verwandlung
    # ersetzt die Definition, behält aber HP/MP/Zustände wie ein neuer Gegner
    # mit denselben aktuellen Werten (XP: Game_Enemy#transform).
    attr_accessor :hidden

    def visible? = !@hidden && alive?

    def transform(enemy_id)
      defi = Battle.enemy_def(enemy_id)
      return false unless defi
      @id = enemy_id.to_s
      @name = (defi['name'] || enemy_id).to_s
      @max_hp = [(defi['hp'] || @max_hp).to_i, 1].max
      @max_mp = [(defi['mp'] || @max_mp).to_i, 0].max
      @hp = [[@hp, 0].max, @max_hp].min
      @mp = [[@mp, 0].max, @max_mp].min
      @atk = (defi['atk'] || @atk).to_i
      @def = (defi['def'] || @def).to_i
      @spi = (defi['spi'] || @spi).to_i
      @agi = (defi['agi'] || @agi).to_i
      @defi = defi
      true
    end

    # ---------------------------------------------------------- Zustände (20a)
    # XP erlaubt höchstens 8 Zustände pro Kämpfer; jeder hat eine Restdauer
    # in Runden. Gleicher Zustand erneut = Dauer auffrischen.
    MAX_STATES = 8

    attr_reader :states

    def add_state(id, turns = nil)
      defi = Battle.state_def(id)
      return false unless defi

      dauer = (turns || defi['turns'] || 3).to_i
      dauer = 1 if dauer < 1
      vorhanden = @states.find { |s| s[:id] == id.to_s }
      if vorhanden
        vorhanden[:turns] = [vorhanden[:turns], dauer].max
        return true
      end
      return false if @states.size >= MAX_STATES

      @states << { id: id.to_s, turns: dauer }
      true
    end

    def state?(id) = @states.any? { |s| s[:id] == id.to_s }

    # Etappe 43: Zustand gezielt entfernen (XP 313/333) und alle löschen
    # (XP 314/334, "Recover All"). Rückgabe: wurde etwas entfernt?
    def remove_state(id)
      vorher = @states.size
      @states.reject! { |s| s[:id] == id.to_s }
      @states.size != vorher
    end

    def clear_states
      @states.clear
      self
    end

    # Anzeige fürs Kampf-UI: "Gift(2) Schutz(3)".
    def state_names
      @states.filter_map do |s|
        defi = Battle.state_def(s[:id])
        defi ? "#{defi['name'] || s[:id]}(#{s[:turns]})" : nil
      end
    end

    # Summe der Zustandsboni auf atk/def/spi/agi (XP: state_effect).
    def state_bonus(wert)
      @states.sum do |s|
        defi = Battle.state_def(s[:id])
        next 0 unless defi

        art = defi['kind'].to_s
        kraft = (defi['power'] || 0).to_i
        if art == "#{wert}_up" then kraft
        elsif art == "#{wert}_down" then -kraft
        else 0
        end
      end
    end

    def actor? = @actor
    def enemy? = !@actor

    # Heldenwerte haben genau eine Quelle: den Party-Actor. Ein Item oder
    # Script darf nicht von einer veralteten Kampfkopie überschrieben werden.
    def party_actor?
      @actor && @defi.respond_to?(:hp) && @defi.respond_to?(:hp=) &&
        @defi.respond_to?(:mp) && @defi.respond_to?(:mp=) &&
        @defi.respond_to?(:max_hp) && @defi.respond_to?(:max_mp)
    end

    def max_hp = party_actor? ? [@defi.max_hp.to_i, 1].max : @max_hp
    def max_mp = party_actor? ? [@defi.max_mp.to_i, 0].max : @max_mp
    def hp = party_actor? ? [[@defi.hp.to_i, 0].max, max_hp].min : @hp
    def mp = party_actor? ? [[@defi.mp.to_i, 0].max, max_mp].min : @mp

    # D4 (Runde 22): Auch die Angriffswerte der Helden haben eine Quelle -
    # ein Stufenaufstieg mitten im Kampf wirkt damit sofort (wie XP), statt
    # erst im nächsten Kampf. 20a: Zustände (Wut/Schutz ...) kommen obendrauf.
    def basis_atk = party_actor? ? @defi.atk.to_i : @atk
    def basis_def = party_actor? ? @defi.def.to_i : @def
    def basis_spi = party_actor? ? @defi.spi.to_i : @spi
    def basis_agi = party_actor? ? @defi.agi.to_i : @agi

    def atk = [basis_atk + state_bonus('atk'), 1].max
    def def = [basis_def + state_bonus('def'), 1].max
    def spi = [basis_spi + state_bonus('spi'), 0].max
    def agi = [basis_agi + state_bonus('agi'), 1].max

    def hp=(value)
      value = [[value.to_i, 0].max, max_hp].min
      party_actor? ? @defi.hp = value : @hp = value
    end

    def mp=(value)
      value = [[value.to_i, 0].max, max_mp].min
      party_actor? ? @defi.mp = value : @mp = value
    end

    def alive? = hp > 0
    def dead? = !alive?

    def damage(amount)
      self.hp = hp - amount.to_i
      hp
    end

    def heal(amount)
      self.hp = hp + amount.to_i
      hp
    end

    def hp_line
      "#{name}  HP #{hp}/#{max_hp}"
    end
  end

  # Gegnerdefinitionen/Truppen aus der Datenbank.
  def self.enemies_def
    ::Database.database['enemies'] || []
  end

  def self.enemy_def(id)
    enemies_def.find { |h| h.is_a?(Hash) && h['id'].to_s == id.to_s }
  end

  def self.troops_def
    ::Database.database['troops'] || []
  end

  def self.troop_def(id)
    troops_def.find { |h| h.is_a?(Hash) && h['id'].to_s == id.to_s }
  end

  # Fertigkeiten und Zustände (20a) - beide aus database.json.
  def self.skills_def
    ::Database.database['skills'] || []
  end

  def self.states_def
    ::Database.database['states'] || []
  end

  def self.skill_def(id)
    skills_def.find { |h| h.is_a?(Hash) && h['id'].to_s == id.to_s }
  end

  def self.state_def(id)
    states_def.find { |h| h.is_a?(Hash) && h['id'].to_s == id.to_s }
  end

  # Gelernte Fertigkeiten eines Helden als Definitionen. Unbekannte IDs
  # (z. B. aus einem älteren Spielstand) werden still übergangen.
  def self.known_skills(battler)
    return [] unless battler && battler.defi.respond_to?(:skills)

    Party.skills(battler.defi).filter_map { |id| skill_def(id) }
  end

  def self.skill_cost(skill) = [(skill['mp'] || 0).to_i, 0].max

  # Zielgruppe einer Fertigkeit (XP: scope): enemy (Standard), ally, self.
  def self.skill_scope(skill)
    art = (skill['scope'] || 'enemy').to_s
    %w[ally self].include?(art) ? art : 'enemy'
  end

  def self.affordable?(battler, skill) = battler.mp >= skill_cost(skill)

  # ------------------------------------------------------------- Zustand

  @active = false
  @enemies = []
  @actors = []
  @phase = nil          # :command | :execute | :item | :target | :item_target | :done
  @actor_index = 0
  @command_index = 0
  @item_index = 0
  @target_index = 0
  @selecting_actor = nil
  @selected_item = nil
  @skill_index = 0          # 20a: Cursor im Fertigkeitenfenster
  @selected_skill = nil     # 20a: gewählte Fertigkeit während der Zielwahl
  @actions = []         # gesammelte {actor:, kind:, item:, target:}
  @can_escape = true
  @can_lose = false
  @result = nil         # :sieg | :flucht | :niederlage | nil
  @log = []
  @turn = 0

  class << self
    attr_reader :enemies, :actors, :phase, :actor_index, :command_index,
                :item_index, :target_index, :actions, :result, :log, :turn,
                :skill_index, :selected_skill, :forced_actions
    attr_accessor :can_escape, :can_lose
  end

  # XP-Reihenfolge: Angriff / Fertigkeit / Item / Flucht.
  COMMANDS = %w[Angriff Fertigkeit Item Flucht].freeze

  def self.active? = @active
  def self.in_battle = @active   # RGSS-Lesart ($game_temp.in_battle)

  # ------------------------------------------------------------- Start/Ende

  # Kampf starten. troop_id: ID (oder Name) aus database.json->troops.
  # can_lose: Niederlage endet nicht mit gameover (XP battle_can_lose).
  def self.start(troop_id, can_escape: true, can_lose: false)
    if @active
      Rune::Log.warn('Battle: ein Kampf läuft bereits - neuer Start abgelehnt')
      return false
    end

    troop = troop_def(troop_id) || troops_def.find { |h| h.is_a?(Hash) && h['name'].to_s == troop_id.to_s }
    unless troop
      Rune::Log.warn("Battle: Truppe '#{troop_id}' nicht in database.json")
      Issues.report('Battle', "Kampf: Truppe '#{troop_id}' unbekannt")
      return false
    end

    liste = Array(troop['enemies']).map(&:to_s).reject(&:empty?)
    if liste.empty?
      Rune::Log.warn("Battle: Truppe '#{troop_id}' hat keine Gegner")
      return false
    end

    enemies = liste.filter_map do |eid|
      defi = enemy_def(eid)
      unless defi
        Rune::Log.warn("Battle: Gegner '#{eid}' nicht in database.json (übersprungen)")
        next nil
      end
      Battler.new(id: eid, name: defi['name'] || eid, hp: defi['hp'] || 30,
                  mp: defi['mp'] || 0, atk: defi['atk'] || 10,
                  def_: defi['def'] || 8, spi: defi['spi'] || 5,
                  agi: defi['agi'] || 8, actor: false, defi: defi)
    end
    if enemies.empty?
      Rune::Log.warn("Battle: Truppe '#{troop_id}' ergibt keine Gegner")
      return false
    end

    actors = Party.actors.map do |a|
      b = Battler.new(id: a.id, name: a.name, hp: a.hp, mp: a.mp,
                      max_hp: a.max_hp, max_mp: a.max_mp, atk: a.atk,
                      def_: a.def, spi: a.spi, agi: a.agi, actor: true, defi: a)
      # Etappe 24 (XP 313): Zustände aus der Party übernehmen - ein per Event
      # gegebener Zustand wirkt damit auch im Kampf. Ohne `turns` gilt die
      # Datenbank-Dauer (Battler.add_state).
      Array(a.states).each { |s| b.add_state(s[:id], s[:turns]) }
      b
    end
    if actors.empty?
      Rune::Log.warn('Battle: keine Helden in der Party - Kampf abgesagt')
      return false
    end

    # Vor der vollständigen Validierung keinen laufenden/vorherigen Zustand
    # teilweise ersetzen. 0 HP bleibt 0; alle KO führt zur Niederlageprüfung.
    @enemies = enemies
    @actors = actors
    @active = true
    @can_escape = can_escape ? true : false
    @can_lose = can_lose ? true : false
    @actions = []
    @forced_actions = []   # Etappe 48: XP 339 Force Action
    @log = []
    @turn = 0
    @result = nil
    @actor_index = 0
    @command_index = 0
    @item_index = 0
    @target_index = 0
    @selecting_actor = nil
    @selected_item = nil
    @skill_index = 0
    @selected_skill = nil
    @phase = @actors.any?(&:alive?) ? :command : :execute
    $game_temp.in_battle = true if $game_temp
    musik_start
    Rune::Log.info("Battle: Kampf gegen Truppe '#{troop['name'] || troop_id}' " \
                   "(#{@enemies.size} Gegner, #{@actors.size} Helden)")
    log("Kampf beginnt: #{troop['name'] || troop_id}!")
    Rune::Sound.play('landung', 0.8) if defined?(Rune::Sound)
    true
  end

  # ------------------------------------------------------------- Kampf-Musik
  # XP (Scene_Battle): vor dem Kampf die Kartenmusik merken und battle_bgm
  # spielen, danach bgm_restore und - nur nach einem Sieg - battle_end_me.
  # Reihenfolge wie im XP-Defaultscript: erst das ME, dann die Musik zurueck
  # (das ME verstummt den Kampf-Titel, die wiederhergestellte Kartenmusik
  # laeuft darunter weiter). Ohne $game_system (kopfhafte Tests) still.
  def self.musik_start
    return unless defined?($game_system) && $game_system
    return unless defined?(Audio) && $game_system.respond_to?(:bgm_play)

    $game_system.bgm_memorize
    $game_system.bgm_play($game_system.battle_bgm)
  end

  def self.musik_ende(sieg: false)
    return unless defined?($game_system) && $game_system
    return unless defined?(Audio) && $game_system.respond_to?(:bgm_restore)

    $game_system.me_play($game_system.battle_end_me) if sieg
    $game_system.bgm_restore
  end

  # Kampf beenden (ohne Wertung), etwa beim Kartenwechsel.
  def self.abort!
    return unless @active

    @active = false
    @phase = nil
    @enemies = []
    @actors = []
    @actions = []
    @forced_actions = []
    @selecting_actor = nil
    @selected_item = nil
    @selected_skill = nil
    $game_temp.in_battle = false if $game_temp
    release_simulation
    musik_ende
    Rune::Log.info('Battle: Kampf abgebrochen')
  end

  # ---------------------------------------------------- Event-Befehle (48)
  # XP 331-340: Gegnerbefehle wirken auf einen Slot (0-basiert) der laufenden
  # Truppe. Rückgabe false = kein Kampf/kein Slot; der Interpreter meldet das.
  def self.enemy_slot(index)
    return nil unless @active
    @enemies[index.to_i]
  end

  def self.enemy_command(cmd, params = {})
    return false unless @active
    p = params.transform_keys(&:to_s)
    slot = p['slot'].to_i
    gegner = enemy_slot(slot)
    case cmd.to_s
    when 'change_enemy_hp'
      return false unless gegner
      wert = p['amount'].to_i
      wert = -wert if p['operation'].to_s == 'subtract'
      if wert.negative? && !p['allow_knockout'] && gegner.alive?
        wert = [wert, -(gegner.hp - 1)].max
      end
      wert.negative? ? gegner.damage(-wert) : gegner.heal(wert)
      log("#{gegner.name} HP -> #{gegner.hp}")
      beende_pruefen
      true
    when 'change_enemy_sp'
      return false unless gegner
      wert = p['amount'].to_i
      wert = -wert if p['operation'].to_s == 'subtract'
      gegner.mp = [[gegner.mp + wert, gegner.max_mp].min, 0].max
      true
    when 'change_enemy_state'
      return false unless gegner
      if p['operation'].to_s == 'remove'
        gegner.remove_state(p['state_id'])
      else
        gegner.add_state(p['state_id'], p['turns'])
      end
      true
    when 'enemy_recover_all'
      return false unless gegner
      gegner.heal(gegner.max_hp)
      gegner.mp = gegner.max_mp
      gegner.clear_states
      true
    when 'enemy_appearance'
      return false unless gegner
      gegner.hidden = !gegner.hidden
      log("#{gegner.name} #{gegner.hidden ? 'verschwindet' : 'erscheint'}")
      true
    when 'enemy_transform'
      return false unless gegner
      ok = gegner.transform(p['enemy_id'])
      log("#{gegner.name} verwandelt sich") if ok
      ok
    when 'show_battle_animation'
      # Etappe 48: target_slot -1 = alle Gegner, sonst dieser Platz.
      if p.key?('target_slot')
        ziel = p['target_slot'].to_i
        ziele = ziel < 0 ? @enemies.compact : [@enemies[ziel]].compact
        return false if ziele.empty?
        ziele.each { |g| Battle.animation_at(g, p['animation_key']) }
        return true
      end
      return false unless gegner
      Battle.animation_at(gegner, p['animation_key'])
      true
    when 'deal_damage'
      # Etappe 48: der Katalog nennt den Gegnerplatz target_slot (nicht slot).
      ziel = p.key?('target_slot') ? enemy_slot(p['target_slot'].to_i) : gegner
      return false unless ziel
      schaden = [p['amount'].to_i, 0].max
      ziel.damage(schaden)
      log("#{ziel.name} erleidet #{schaden} Schaden")
      beende_pruefen
      true
    when 'force_action'
      return false unless gegner
      @forced_actions << { enemy: gegner, action_key: p['action_key'].to_s,
                           target_slot: p['target_slot'].to_i }
      true
    when 'abort_battle'
      abort!
      true
    else false
    end
  end

  # Einfache Kampfanimation als Partikelburst am Gegner (XP 337). Ein Projekt
  # kann später eine echte Animationsdatei hinterlegen; der Key wird geführt.
  def self.animation_at(battler, key = nil)
    return unless defined?(Effects) && defined?(Rune::Player)
    pos = enemy_positions.find { |(_x, _y, _z, g)| g.equal?(battler) }
    return unless pos
    x, y, z, = pos
    Effects.burst(x, y, z, color: 0xFFE14A, count: 16, speed: 2.4, life: 0.6)
    Rune::Log.info("Battle: Animation '#{key}' auf #{battler.name}") if key
  end

  # Erzwungene Gegneraktion (XP 339): Skill oder Angriff auf ein Ziel.
  def self.execute_forced_actions
    @forced_actions.each do |act|
      gegner = act[:enemy]
      next if gegner.dead? || gegner.hidden
      skill = skill_def(act[:action_key])
      ziel = if act[:target_slot] >= 0
               @actors[act[:target_slot]]
             else
               living_actors.sample
             end
      next unless ziel && ziel.alive?
      if skill
        wende_skill_an(gegner, skill, ziel)
      else
        angriff(gegner, ziel, gegner: true)
      end
    end
    @forced_actions = []
  end

  # Auch direkte Ruby-Aufrufe von abort!/ende dürfen die Welt nicht pausiert
  # lassen. Kopflose Tests ohne native Simulation bleiben weiterhin möglich.
  def self.release_simulation
    if defined?(Rune::Simulation) && Rune::Simulation.respond_to?(:paused=)
      Rune::Simulation.paused = false
    end
  end

  # ------------------------------------------------------------- Eingabe

  # Ein Frame Kampf-UI: Tastatur steuert Befehls-/Item-/Zielwahl.
  # Läuft nur in der Befehlsphase; execute läuft ohne Eingabe durch.
  def self.update(dt)
    return unless @active
    # Auch externe Ruby-Änderungen können eine Seite während der Auswahl
    # besiegen; dann darf die UI nicht in einer leeren Zielwahl hängen bleiben.
    return if beende_pruefen
    # KO hebt vorgemerkte Aktionen und ihre Itemreservierungen auf. Eine
    # spätere Wiederbelebung startet keinen alten, bereits verlorenen Befehl.
    @actions.reject! { |action| action[:actor].dead? }
    return if cancel_invalid_selection

    case @phase
    when :command      then update_command
    when :skill        then update_skill
    when :skill_target then update_skill_target
    when :item         then update_item
    when :target       then update_target
    when :item_target  then update_item_target
    when :execute      then update_execute(dt)
    end
  end

  # Bei äußerem KO/Wechsel eines noch auswählenden Helden die angefangene
  # Auswahl verwerfen, ohne bestätigte Aktionen anderer Helden anzutasten.
  def self.cancel_pending_selection
    if @phase == :target && @selecting_actor
      @actions.reject! do |action|
        action[:actor].equal?(@selecting_actor) && action[:kind] == :attack && !action.key?(:target)
      end
    end
    @selecting_actor = nil
    @selected_item = nil
    @selected_skill = nil
  end

  def self.cancel_invalid_selection
    return false unless %i[item target item_target skill skill_target].include?(@phase)
    return false if @selecting_actor && @selecting_actor.equal?(current_actor)

    cancel_pending_selection
    @phase = :command
    @command_index = 0
    true
  end

  # Befehlsphase: Angriff/Fertigkeit/Item/Flucht für den aktuellen Helden.
  def self.update_command
    actor = current_actor
    return finish_round unless actor   # alle durch

    if Rune::Input.pressed?(:up)
      @command_index = (@command_index - 1) % COMMANDS.size
      klick
    elsif Rune::Input.pressed?(:down)
      @command_index = (@command_index + 1) % COMMANDS.size
      klick
    elsif Rune::Input.pressed?(:enter) || Rune::Input.pressed?(:e) ||
          Rune::Input.pressed?(:space)
      case COMMANDS[@command_index]
      when 'Angriff'
        @actions << { actor: actor, kind: :attack }
        @selecting_actor = actor
        log("#{actor.name}: Angriff gewählt")
        @target_index = 0
        @phase = :target
        klick
      when 'Fertigkeit'
        if known_skills(actor).empty?
          log('Keine Fertigkeiten gelernt!')
          Rune::Sound.play('ablehnen', 0.6) if defined?(Rune::Sound)
        else
          @skill_index = 0
          @selecting_actor = actor
          @phase = :skill
          klick
        end
      when 'Item'
        if usable_items.empty?
          log('Keine benutzbaren Items!')
          Rune::Sound.play('ablehnen', 0.6) if defined?(Rune::Sound)
        else
          @item_index = 0
          @selecting_actor = actor
          @phase = :item
          klick
        end
      when 'Flucht'
        attempt_escape
      end
    elsif Rune::Input.pressed?(:escape)
      attempt_escape if @can_escape
    end
  end

  # Itemwahl: benutzbare Items (heal/mana) der Party.
  def self.update_item
    items = usable_items
    if items.empty?
      cancel_pending_selection
      @phase = :command
      return
    end
    @item_index = 0 if @item_index >= items.size

    if Rune::Input.pressed?(:up)
      @item_index = (@item_index - 1) % items.size
      klick
    elsif Rune::Input.pressed?(:down)
      @item_index = (@item_index + 1) % items.size
      klick
    elsif Rune::Input.pressed?(:enter) || Rune::Input.pressed?(:e) ||
          Rune::Input.pressed?(:space)
      @selected_item = items[@item_index]
      @target_index = 0
      @phase = :item_target
      klick
    elsif Rune::Input.pressed?(:escape)
      cancel_pending_selection
      @phase = :command
      klick
    end
  end

  # Fertigkeitenwahl (20a): gelernte Skills des aktuellen Helden mit MP-Kosten.
  # Zu wenig MP wird verweigert (XP zeigt die Kosten im Fenster).
  def self.update_skill
    actor = current_actor
    return finish_round unless actor

    liste = known_skills(actor)
    if liste.empty?
      cancel_pending_selection
      @phase = :command
      return
    end
    @skill_index = 0 if @skill_index >= liste.size

    if Rune::Input.pressed?(:up)
      @skill_index = (@skill_index - 1) % liste.size
      klick
    elsif Rune::Input.pressed?(:down)
      @skill_index = (@skill_index + 1) % liste.size
      klick
    elsif Rune::Input.pressed?(:enter) || Rune::Input.pressed?(:e) ||
          Rune::Input.pressed?(:space)
      skill = liste[@skill_index]
      art = skill_scope(skill)
      unless affordable?(actor, skill)
        log("#{actor.name} hat nicht genug MP für #{skill['name'] || skill['id']}")
        Rune::Sound.play('ablehnen', 0.6) if defined?(Rune::Sound)
        return
      end
      if art == 'self'
        @actions << { actor: actor, kind: :skill, skill: skill['id'], target: actor }
        log("#{actor.name}: #{skill['name'] || skill['id']}")
        next_actor
      else
        @selected_skill = skill['id']
        @target_index = 0
        @phase = :skill_target
        klick
      end
    elsif Rune::Input.pressed?(:escape)
      cancel_pending_selection
      @phase = :command
      klick
    end
  end

  # Zielwahl einer Fertigkeit: Gegner (Standard) oder Helden (Heilung/Buff).
  def self.update_skill_target
    actor = current_actor
    return finish_round unless actor

    skill = skill_def(@selected_skill)
    unless skill
      @selected_skill = nil
      @phase = :skill
      return
    end
    # Heil-/Buff-Ziele sind lebende Helden (XP erlaubt HP-Skills nicht auf
    # KO-Ziele; Wiederbeleben bleibt dem Item vorbehalten).
    ziele = skill_scope(skill) == 'ally' ? living_actors : living_enemies
    if ziele.empty?
      cancel_pending_selection
      @phase = :skill
      return
    end
    @target_index %= ziele.size

    if Rune::Input.pressed?(:left) || Rune::Input.pressed?(:up)
      @target_index = (@target_index - 1) % ziele.size
      klick
    elsif Rune::Input.pressed?(:right) || Rune::Input.pressed?(:down)
      @target_index = (@target_index + 1) % ziele.size
      klick
    elsif Rune::Input.pressed?(:enter) || Rune::Input.pressed?(:e) ||
          Rune::Input.pressed?(:space)
      ziel = ziele[@target_index]
      unless affordable?(actor, skill)
        log("#{actor.name} hat nicht genug MP für #{skill['name'] || skill['id']}")
        Rune::Sound.play('ablehnen', 0.6) if defined?(Rune::Sound)
        return
      end
      @actions << { actor: actor, kind: :skill, skill: skill['id'], target: ziel }
      log("#{actor.name}: #{skill['name'] || skill['id']} -> #{ziel.name}")
      @selected_skill = nil
      next_actor
    elsif Rune::Input.pressed?(:escape)
      @selected_skill = nil
      @phase = :skill
      klick
    end
  end

  # Itemziel: ein Party-Held einschließlich KO; bestätigen bucht die Aktion,
  # nicht den Verbrauch. Esc kehrt ohne Reservierung zur Itemliste zurück.
  def self.update_item_target
    return finish_round unless current_actor
    if @actors.empty? || !@selected_item || available_item_count(@selected_item) <= 0
      @selected_item = nil
      @phase = :item
      return
    end
    @target_index %= @actors.size

    if Rune::Input.pressed?(:left) || Rune::Input.pressed?(:up)
      @target_index = (@target_index - 1) % @actors.size
      klick
    elsif Rune::Input.pressed?(:right) || Rune::Input.pressed?(:down)
      @target_index = (@target_index + 1) % @actors.size
      klick
    elsif Rune::Input.pressed?(:enter) || Rune::Input.pressed?(:e) ||
          Rune::Input.pressed?(:space)
      target = @actors[@target_index]
      unless Party.item_applicable?(@selected_item, target.defi)
        log("#{Party.item_name(@selected_item)} hat bei #{target.name} keine Wirkung")
        Rune::Sound.play('ablehnen', 0.6) if defined?(Rune::Sound)
        return
      end
      @actions << { actor: current_actor, kind: :item, item: @selected_item, target: target }
      log("#{current_actor.name}: #{Party.item_name(@selected_item)} für #{target.name}")
      @selected_item = nil
      next_actor
    elsif Rune::Input.pressed?(:escape)
      @selected_item = nil
      @phase = :item
      klick
    end
  end

  # Zielwahl: lebender Gegner (links/rechts).
  def self.update_target
    lebendig = living_enemies
    if lebendig.empty?
      cancel_pending_selection
      @phase = :command
      return
    end
    @target_index = 0 if @target_index >= lebendig.size

    if Rune::Input.pressed?(:left)
      @target_index = (@target_index - 1) % lebendig.size
      klick
    elsif Rune::Input.pressed?(:right)
      @target_index = (@target_index + 1) % lebendig.size
      klick
    elsif Rune::Input.pressed?(:up) || Rune::Input.pressed?(:down)
      @target_index = (@target_index + 1) % lebendig.size
      klick
    elsif Rune::Input.pressed?(:enter) || Rune::Input.pressed?(:e) ||
          Rune::Input.pressed?(:space)
      ziel = lebendig[@target_index]
      letzte = @actions.reverse.find do |action|
        action[:actor].equal?(@selecting_actor) && action[:kind] == :attack && !action.key?(:target)
      end
      letzte[:target] = ziel if letzte
      log("Ziel: #{ziel.name}")
      next_actor
    elsif Rune::Input.pressed?(:escape)
      cancel_pending_selection
      @phase = :command
      klick
    end
  end

  # Aktionsphase: gesammelte Aktionen + Gegnerzug + Ende-Prüfung.
  def self.update_execute(dt)
    # Heldenaktionen nach Agilität (XP: make_action_orders).
    @actions.sort_by! { |a| -a[:actor].agi }
    @actions.each do |act|
      actor = act[:actor]
      next if actor.dead?

      case act[:kind]
      when :attack
        ziel = act[:target]
        ziel = living_enemies.first unless ziel && @enemies.any? { |enemy| enemy.equal?(ziel) } && ziel.alive?
        next unless ziel
        angriff(actor, ziel)
      when :skill
        skill = skill_def(act[:skill])
        ziel = act[:target]
        if skill && ziel
          wende_skill_an(actor, skill, ziel)
        else
          log("#{actor.name}: Fertigkeit konnte nicht benutzt werden")
        end
      when :item
        item, ziel = act[:item], act[:target]
        if item && ziel && @actors.any? { |candidate| candidate.equal?(ziel) } &&
           Party.use_item(item, target: ziel.defi)
          log("#{actor.name} benutzt #{Party.item_name(item)} für #{ziel.name}")
        else
          log("#{actor.name}: Item konnte nicht benutzt werden")
        end
      end
      # Nach dem letzten Gegner keine späteren Items mehr verbrauchen.
      return if beende_pruefen
    end
    @actions = []

    # Etappe 48: erzwungene Aktionen (XP 339) wirken vor dem normalen Gegnerzug.
    execute_forced_actions
    return if beende_pruefen

    # Gegnerzug: jeder lebende, sichtbare Gegner schlägt einen zufälligen Helden.
    @enemies.each do |g|
      next if g.dead? || g.hidden
      ziel = living_actors.sample
      break unless ziel
      angriff(g, ziel, gegner: true)
    end

    # 20a: Zustände wirken am Rundenende (Gift/Regeneration) und zählen
    # ihre Dauer herunter - wie XP am Ende der Runde.
    zustaende_ticken
    return if beende_pruefen

    @turn += 1
    beende_pruefen || beginn_runde
  end

  # ------------------------------------------------------------- intern

  def self.current_actor
    # Index in der festen Party-Reihenfolge, nicht in einer wechselnden Liste
    # lebender Helden: Ein späteres KO darf niemanden überspringen.
    @actors.drop(@actor_index).find(&:alive?)
  end

  def self.next_actor
    actor = current_actor
    @actor_index = actor ? @actors.index(actor) + 1 : @actors.size
    cancel_pending_selection
    @command_index = 0
    @phase = :command
    klick
    finish_round if current_actor.nil?
  end

  def self.finish_round
    @phase = :execute
  end

  def self.beginn_runde
    @actor_index = 0
    @command_index = 0
    @selecting_actor = nil
    @selected_item = nil
    @selected_skill = nil
    @phase = :command
  end

  def self.living_enemies = @enemies.select(&:alive?)
  def self.living_actors  = @actors.select(&:alive?)

  # Bereits gewählte Aktionen reservieren ihre Menge nur für diese Runde.
  # Tatsächlich abgezogen wird erst beim erfolgreichen Ausführen.
  def self.available_item_count(id)
    key = id.to_s
    reserved = @actions.count { |act| act[:kind] == :item && act[:item] == key && act[:actor].alive? }
    [Party.item_count(key) - reserved, 0].max
  end

  def self.usable_items
    Party.items.keys.select { |id| available_item_count(id).positive? && Party.usable_item?(id) }
         .sort_by { |id| Party.item_name(id) }
  end

  def self.item_kind(id)
    (Party.item_def(id) || {})['kind'].to_s
  end

  # Angriff mit Schaden, Kritik und Tod-Prüfung (XP: Game_Battler#attack).
  def self.angriff(quelle, ziel, gegner: false)
    return unless ziel.alive?

    kritisch = rand < (quelle.agi / 64.0)
    schaden = ((quelle.atk - ziel.def / 2.0) * (rand * 0.3 + 0.85)).round
    schaden = (schaden * 3).round if kritisch && !gegner
    schaden = (schaden * 1.25).round if kritisch && gegner
    schaden = 1 if schaden < 1

    ziel.damage(schaden)
    log("#{quelle.name} trifft #{ziel.name} für #{schaden}" \
        "#{kritisch ? ' (kritisch!)' : ''}")
    Rune::Sound.play(gegner ? 'landung' : 'schritt', 0.55) if defined?(Rune::Sound)
    log("#{ziel.name} fällt!") if ziel.dead?
    sync_actors!
  end

  # Fertigkeit anwenden (20a): MP abbuchen, dann Schaden/Heilung/Zustand.
  # Rückgabe false, wenn sie nicht (mehr) wirkt - die MP bleiben dann stehen.
  def self.wende_skill_an(quelle, skill, ziel)
    kosten = skill_cost(skill)
    if quelle.mp < kosten
      log("#{quelle.name} hat nicht genug MP für #{skill['name'] || skill['id']}")
      return false
    end
    quelle.mp = quelle.mp - kosten
    name = skill['name'] || skill['id']
    art = (skill['kind'] || 'damage').to_s

    case art
    when 'heal'
      heil = [(skill['power'] || 20).to_i + (quelle.spi / 2.0).round, 1].max
      vorher = ziel.hp
      ziel.heal(heil)
      log("#{quelle.name} wirkt #{name} auf #{ziel.name} (+#{ziel.hp - vorher} HP)")
    when 'buff'
      zustand = skill['state'].to_s
      defi = state_def(zustand)
      chanced = rand < (skill['state_rate'] || 1.0).to_f
      if defi && chanced && ziel.add_state(zustand, skill['turns'])
        log("#{quelle.name} wirkt #{name} auf #{ziel.name}: #{defi['name'] || zustand}")
      else
        log("#{quelle.name} wirkt #{name}, aber nichts geschieht")
      end
    else
      schaden = ((skill['power'] || 10).to_i + quelle.spi * 0.5 - ziel.def / 2.0) *
                (rand * 0.3 + 0.85)
      schaden = [schaden.round, 1].max
      ziel.damage(schaden)
      log("#{quelle.name} wirkt #{name} auf #{ziel.name}: #{schaden} Schaden")
      log("#{ziel.name} fällt!") if ziel.dead?
      # Zustandschance (XP: state_effectivness / +x %)
      if (zustand = skill['state'].to_s) != '' && ziel.alive?
        defi = state_def(zustand)
        if defi && rand < (skill['state_rate'] || 1.0).to_f && ziel.add_state(zustand, skill['turns'])
          log("#{ziel.name}: #{defi['name'] || zustand}!")
        end
      end
    end
    Rune::Sound.play('landung', 0.5) if defined?(Rune::Sound)
    sync_actors!
    true
  end

  # Rundenende: Gift schadet, Regeneration heilt, danach Dauer -1 und
  # abgelaufene Zustände entfernen (XP: Game_Battler#update_state_turn_effect).
  def self.zustaende_ticken
    (@actors + @enemies).each do |b|
      next if b.dead? && b.states.empty?

      b.states.dup.each do |s|
        defi = state_def(s[:id])
        next unless defi

        case defi['kind'].to_s
        when 'poison'
          schaden = [(defi['power'] || 1).to_i, 1].max
          b.damage(schaden)
          log("#{b.name}: #{defi['name'] || s[:id]} (-#{schaden} HP)")
          log("#{b.name} fällt!") if b.dead?
        when 'regen'
          vorher = b.hp
          b.heal([(defi['power'] || 1).to_i, 1].max)
          log("#{b.name}: #{defi['name'] || s[:id]} (+#{b.hp - vorher} HP)") if b.hp > vorher
        end
      end
      next if b.dead?

      b.states.each { |s| s[:turns] -= 1 }
      b.states.reject! do |s|
        next false if s[:turns].to_i.positive?

        defi = state_def(s[:id])
        log("#{b.name}: #{defi ? defi['name'] : s[:id]} klingt ab")
        true
      end
    end
    sync_actors!
  end

  # Fluchtversuch: 50 % (XP: phase 4 escape). Ohne can_escape nicht möglich.
  def self.attempt_escape
    unless @can_escape
      log('Flucht nicht möglich!')
      Rune::Sound.play('ablehnen', 0.7) if defined?(Rune::Sound)
      return
    end
    if rand < 0.5
      log('Flucht gelungen!')
      ende(:flucht)
    else
      log('Flucht misslungen!')
      Rune::Sound.play('ablehnen', 0.7) if defined?(Rune::Sound)
      @actions = []
      finish_round
    end
  end

  # Sieg/Niederlage prüfen. true = Kampf entschieden.
  def self.beende_pruefen
    if living_actors.empty?
      niederlage
      true
    elsif living_enemies.empty?
      sieg
      true
    else
      false
    end
  end

  def self.sieg
    exp = @enemies.sum { |g| g.defi ? (g.defi['exp'] || 0).to_i : 0 }
    gold = @enemies.sum { |g| g.defi ? (g.defi['gold'] || 0).to_i : 0 }
    @enemies.each do |g|
      next unless g.defi && g.defi['drop']
      rate = (g.defi['drop_rate'] || 0).to_f
      next unless rand < rate
      Party.add_item(g.defi['drop'].to_s, 1)
      log("Beute: #{Party.item_name(g.defi['drop'])}")
    end
    unless exp.zero?
      @actors.each do |a|
        next if a.dead?
        next unless a.defi.respond_to?(:exp)
        # D4 (Runde 22): XP-Stufenaufstieg - Überschuss zählt für die
        # nächste Stufe, Statuswerte wachsen (Party.gain_exp).
        stufen = Party.gain_exp(a.defi, exp)
        log("#{a.name} steigt auf Stufe #{a.defi.level}!") if stufen.positive?
      end
      log("Sieg! #{exp} Exp erhalten")
    end
    Party.add_gold(gold) unless gold.zero?
    log("#{gold} Gold gefunden") unless gold.zero?
    Rune::Sound.play('jingle', 0.7) if defined?(Rune::Sound)
    ende(:sieg)
  end

  def self.niederlage
    if @can_lose
      log('Niederlage - aber der Kampf war nur eine Prüfung.')
      ende(:niederlage, ohne_gameover: true)
    else
      log('Die Party ist besiegt...')
      Rune::Sound.play('ablehnen', 0.9) if defined?(Rune::Sound)
      ende(:niederlage)
    end
  end

  # Kampf beenden: Helden-HP zurück in die Party, gameover-Flag setzen.
  def self.ende(result, ohne_gameover: false)
    @result = result
    sync_actors!
    @active = false
    @phase = :done
    @actions = []
    @selecting_actor = nil
    @selected_item = nil
    $game_temp.in_battle = false if $game_temp
    release_simulation
    if result == :niederlage && !ohne_gameover && $game_temp
      $game_temp.gameover = true
    end
    musik_ende(sieg: result == :sieg)
    Rune::Log.info("Battle: Kampf beendet (#{result})")
  end

  # Kompatibilitätshelfer: reguläre Helden delegieren HP/MP bereits direkt
  # an denselben Party-Actor; es gibt keine veraltete Kampfkopie mehr.
  def self.sync_actors!
    @actors.each do |a|
      pa = a.defi
      next unless pa.respond_to?(:hp=)
      pa.hp = a.hp
      pa.mp = a.mp if pa.respond_to?(:mp=)
      # Zustände zurück in die Party (Etappe 24): im Kampf abgelaufene
      # verschwinden damit auch aus Menü und Spielstand.
      pa.states = a.states.map { |s| { id: s[:id], turns: s[:turns] } } if pa.respond_to?(:states=)
    end
  end

  def self.log(text)
    @log << text.to_s
    @log = @log.last(6)
    Rune::Log.info("Battle: #{text}")
  end

  def self.klick
    Rune::Sound.play('klick', 0.35) if defined?(Rune::Sound)
  end

  # ------------------------------------------------------------- Zeichnen

  # Gegner als 3D-Kästen vor dem Spieler (Kamera-Yaw = Blick nach -z).
  def self.enemy_positions
    return [] unless @active
    px, _py, pz = Rune::Player.position
    n = @enemies.size
    @enemies.each_with_index.map do |g, i|
      x = px + (i - (n - 1) / 2.0) * 1.6
      [x, Game.gy(x, pz + 3.0) + 0.9, pz + 3.0, g]
    end
  end

  # Farben: Gegner-Definition (color) oder Palette nach Listenplatz.
  PALETTE = [0xC85050, 0xB4783C, 0x9050C8, 0x50A0C8, 0x7CB450, 0xC8A050].freeze

  def self.enemy_color(g, index)
    return (g.defi['color'] || PALETTE[index % PALETTE.size]).to_i if g.defi
    PALETTE[index % PALETTE.size]
  end

  # 2D-Overlay: Gegner-Namen/HP, Party-Status, Befehle, Kampflog.
  def self.draw
    return unless @active || @phase == :done

    w = Rune::Window.width
    h = Rune::Window.height

    # Etappe 48: Kampfhintergrund (XP 204 battleback), falls gesetzt.
    MapEffects.draw_battleback if defined?(MapEffects)

    # Gegner-Kästen + HP-Balken (3D-Teil).
    enemy_positions.each_with_index do |(x, y, z, g), i|
      farbe = g.alive? ? enemy_color(g, i) : 0x3A3A3A
      Rune::Gfx.box(x, y, z, 0.8, 1.8, 0.8, farbe)
      next unless g.alive?
      # HP-Balken über dem Gegner (2D: feste Position pro Gegner).
      bx = w / 2 - (@enemies.size * 70) / 2 + i * 70
      by = h / 2 - 150
      anteil = [g.hp.to_f / g.max_hp, 0.0].max
      UI.panel(bx, by, 60, 8, 0x220000, 0.8)
      balken = anteil > 0.5 ? 0x86D993 : 0xE86A6A
      UI.panel(bx, by, (60 * anteil).round, 8, balken, 0.9)
      UI.text(bx, by + 10, g.name, UI::TEXT, 13)
    end

    # Abdunkeln wie im Menü.
    UI.abdunkeln(w)

    # Befehlsfenster (links).
    if %i[command item target item_target skill skill_target].include?(@phase)
      actor = current_actor
      cw = 220
      ch = 40 + COMMANDS.size * 28
      cx = 40
      cy = h - ch - 160
      UI.panel(cx, cy, cw, ch, UI::PANEL, 0.92)
      UI.panel(cx, cy, cw, 2, UI::ACCENT, 0.85)
      UI.text(cx + 14, cy + 8, actor ? actor.name : '', UI::ACCENT, 16)
      COMMANDS.each_with_index do |c, i|
        y = cy + 34 + i * 28
        grau = (c == 'Flucht' && !@can_escape)
        if i == @command_index && @phase == :command
          UI.text(cx + 14, y, "> #{c}", grau ? UI::TEXT_DIM : UI::ACCENT, 16)
        else
          UI.text(cx + 14, y, "  #{c}", grau ? UI::TEXT_DIM : UI::TEXT, 16)
        end
      end
    end

    # Fertigkeitenfenster (über den Befehlen, XP: Skill-Fenster rechts).
    if %i[skill skill_target].include?(@phase)
      actor = current_actor
      skills = actor ? known_skills(actor) : []
      sw = 300
      sh = 24 + [skills.size, 1].max * 24
      sx = 40
      sy = h - 160 - sh - 50
      UI.panel(sx, sy, sw, sh, UI::PANEL, 0.92)
      UI.panel(sx, sy, sw, 2, UI::ACCENT, 0.85)
      if skills.empty?
        UI.text(sx + 14, sy + 10, 'Keine Fertigkeiten', UI::TEXT_DIM, 14)
      else
        skills.each_with_index do |skill, i|
          y = sy + 10 + i * 24
          kosten = skill_cost(skill)
          text = "#{skill['name'] || skill['id']}  #{kosten} MP"
          genug = actor && actor.mp >= kosten
          gewaehlt = i == @skill_index && @phase == :skill
          farbe = if !genug then UI::TEXT_DIM
                  elsif gewaehlt then UI::ACCENT
                  else UI::TEXT
                  end
          UI.text(sx + 14, y, gewaehlt ? "> #{text}" : "  #{text}", farbe, 14)
        end
      end
      UI.text(sx + 14, sy - 20, actor ? "#{actor.name}: #{actor.mp}/#{actor.max_mp} MP" : '',
              UI::TEXT_DIM, 13)
    end

    # Itemfenster (über den Befehlen).
    if @phase == :item
      items = usable_items
      iw = 260
      ih = 20 + items.size * 24
      ix = 40
      iy = h - 160 - ih - 50
      UI.panel(ix, iy, iw, ih, UI::PANEL, 0.92)
      items.each_with_index do |id, i|
        y = iy + 10 + i * 24
        text = "#{Party.item_name(id)} x#{available_item_count(id)}"
        UI.text(ix + 14, y, i == @item_index ? "> #{text}" : "  #{text}",
                i == @item_index ? UI::ACCENT : UI::TEXT, 14)
      end
    end

    # Ziel-Hinweis.
    if @phase == :target
      lebendig = living_enemies
      if (ziel = lebendig[@target_index])
        UI.text(w / 2 - 100, h / 2 - 180, "Ziel: #{ziel.name}  (←/→, E = bestätigen)",
                UI::ACCENT, 15)
      end
    elsif @phase == :item_target && (ziel = @actors[@target_index])
      UI.text(w / 2 - 220, h / 2 - 180,
              "#{Party.item_name(@selected_item)} — Ziel: #{ziel.name}  (←/→, E / Esc)", UI::ACCENT, 15)
    elsif @phase == :skill_target
      skill = skill_def(@selected_skill)
      ziele = skill && skill_scope(skill) == 'ally' ? living_actors : living_enemies
      if skill && (ziel = ziele[@target_index])
        UI.text(w / 2 - 220, h / 2 - 180,
                "#{skill['name'] || skill['id']} — Ziel: #{ziel.name}  (←/→, E / Esc)", UI::ACCENT, 15)
      end
    end

    # Party-Status (unten links neben den Befehlen).
    pw = 340
    ph = 20 + Party.actors.size * 26
    px2 = 40
    py2 = h - ph - 20
    UI.panel(px2, py2, pw, ph, UI::PANEL, 0.88)
    Party.actors.each_with_index do |a, i|
      selected = (@phase == :item_target || @phase == :skill_target) &&
                 @actors[@target_index]&.defi&.equal?(a)
      zustaende = @actors[i] ? @actors[i].state_names.join(' ') : ''
      zeile = zustaende.empty? ? a.hp_line : "#{a.hp_line} #{zustaende}"
      text = selected ? "> #{zeile}" : "  #{zeile}"
      color = selected ? UI::ACCENT : (a.hp.zero? ? UI::DANGER : UI::TEXT)
      UI.text(px2 + 14, py2 + 8 + i * 26, text, color, 14)
    end

    # Gegner-Liste (rechts).
    gw = 260
    gh = 20 + @enemies.size * 24
    gx = w - gw - 40
    gy2 = h - gh - 20
    UI.panel(gx, gy2, gw, gh, UI::PANEL, 0.88)
    @enemies.each_with_index do |g, i|
      zustaende = g.state_names.join(' ')
      zeile = zustaende.empty? ? g.hp_line : "#{g.hp_line} #{zustaende}"
      UI.text(gx + 14, gy2 + 8 + i * 24, zeile,
              g.alive? ? UI::TEXT : UI::TEXT_DIM, 14)
    end

    # Kampflog (oben).
    lw = 520
    lh = 16 + @log.size * 22
    UI.panel(w / 2 - lw / 2, 20, lw, lh, UI::PANEL, 0.8)
    @log.each_with_index do |zeile, i|
      UI.text(w / 2 - lw / 2 + 14, 26 + i * 22, zeile, UI::TEXT, 14)
    end

    # Ergebnis-Anzeige.
    if @phase == :done && @result
      UI.text(w / 2 - 90, h / 2, "Kampf beendet: #{@result}", UI::ACCENT, 22)
    end
  end
end
