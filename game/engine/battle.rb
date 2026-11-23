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
#                    "enemies":["goblin","wolf"] }]
#
#  Ablauf (wie XP: Befehlsphase -> Aktionsphase -> Gegnerzug):
#    1. start(troop_id) stellt Gegner auf, merkt can_escape/can_lose.
#    2. Befehlsphase: jeder lebende Held bekommt Angriff/Item/Flucht;
#       Aktionen sammeln sich in @actions.
#    3. execute: Heldenaktionen wirken (nach Agilität), dann ziehen alle
#       Gegner (zufälliger lebender Held), dann Sieg-/Niederlage-Prüfung.
#    4. Sieg: Exp/Gold/Drop an die Party, Ende. Niederlage: gameover-Flag
#       ($game_temp.gameover -> Game.to_title!), außer can_lose.
#
#  Event-Befehl: {"cmd":"battle","name":"waldweg","can_escape":true,
#                 "can_lose":false} - der Interpreter blockiert bis zum Ende.
#  Ruby: Battle.start('waldweg') bzw. Context-Helfer battle('waldweg').
#
#  Schaden (XP-Grundform, vereinfacht): atk - def/2 des Ziels, ±15 %
#  Streuung, mindestens 1. Kritik: agi/64-Chance (XP-Formel), x3-Schaden
#  beim Angriff (Helden), +25 % beim Gegner-Aggressionswert.
#
#  Grenzen (Backlog): keine Skills/Magie (MP bleibt ungenutzt), keine
#  Zustände (Vergiftung etc.), keine Kampfformationen/Positionen, Gegner
#  ziehen immer "Angriff auf zufälligen Held", kein Flucht-Textfenster,
#  Kampfhintergrund = abgedunkelte Karte.
#
#  Kopflos testbar: ruby -I game/engine tools/battle_test.rb
# ============================================================================

module Battle
  # Ein Kämpfer (XP: Game_Battler - hier Helden wie Gegner in einer Klasse,
  # actor?/enemy? trennt sie).
  class Battler
    attr_accessor :hp, :mp, :name, :id
    attr_reader :max_hp, :max_mp, :atk, :def, :spi, :agi, :actor, :defi

    def initialize(id:, name:, hp:, mp: 0, atk: 10, def_: 8, spi: 5, agi: 8,
                   actor: false, defi: nil)
      @id = id.to_s
      @name = name.to_s
      @max_hp = [hp.to_i, 1].max
      @max_mp = mp.to_i
      @hp = @max_hp
      @mp = @max_mp
      @atk = atk.to_i
      @def = def_.to_i
      @spi = spi.to_i
      @agi = agi.to_i
      @actor = actor
      @defi = defi          # Party::Actor bei Helden, Enemy-Hash bei Gegnern
    end

    def actor? = @actor
    def enemy? = !@actor
    def alive? = @hp > 0
    def dead? = !alive?

    def damage(amount)
      @hp = [@hp - amount.to_i, 0].max
      @hp
    end

    def heal(amount)
      @hp = [@hp + amount.to_i, @max_hp].min
      @hp
    end

    def hp_line
      "#{name}  HP #{hp}/#{@max_hp}"
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

  # ------------------------------------------------------------- Zustand

  @active = false
  @enemies = []
  @actors = []
  @phase = nil          # :command | :execute | :item | :target | :done
  @actor_index = 0
  @command_index = 0
  @item_index = 0
  @target_index = 0
  @actions = []         # gesammelte {actor:, kind:, item:}
  @can_escape = true
  @can_lose = false
  @result = nil         # :sieg | :flucht | :niederlage | nil
  @log = []
  @turn = 0

  class << self
    attr_reader :enemies, :actors, :phase, :actor_index, :command_index,
                :item_index, :target_index, :actions, :result, :log, :turn
    attr_accessor :can_escape, :can_lose
  end

  COMMANDS = %w[Angriff Item Flucht].freeze

  def self.active? = @active
  def self.in_battle = @active   # RGSS-Lesart ($game_temp.in_battle)

  # ------------------------------------------------------------- Start/Ende

  # Kampf starten. troop_id: ID (oder Name) aus database.json->troops.
  # can_lose: Niederlage endet nicht mit gameover (XP battle_can_lose).
  def self.start(troop_id, can_escape: true, can_lose: false)
    troop = troop_def(troop_id) || troops_def.find { |h| h['name'].to_s == troop_id.to_s }
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

    @enemies = liste.filter_map do |eid|
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
    if @enemies.empty?
      Rune::Log.warn("Battle: Truppe '#{troop_id}' ergibt keine Gegner")
      return false
    end

    @actors = Party.actors.map do |a|
      Battler.new(id: a.id, name: a.name, hp: a.hp, mp: a.mp, atk: a.atk,
                  def_: a.def, spi: a.spi, agi: a.agi, actor: true, defi: a)
    end
    if @actors.empty?
      Rune::Log.warn('Battle: keine Helden in der Party - Kampf abgesagt')
      return false
    end

    @active = true
    @can_escape = can_escape ? true : false
    @can_lose = can_lose ? true : false
    @actions = []
    @log = []
    @turn = 0
    @result = nil
    @actor_index = 0
    @command_index = 0
    @phase = :command
    $game_temp.in_battle = true if $game_temp
    Rune::Log.info("Battle: Kampf gegen Truppe '#{troop['name'] || troop_id}' " \
                   "(#{@enemies.size} Gegner, #{@actors.size} Helden)")
    log("Kampf beginnt: #{troop['name'] || troop_id}!")
    Rune::Sound.play('landung', 0.8) if defined?(Rune::Sound)
    true
  end

  # Kampf beenden (ohne Wertung), etwa beim Kartenwechsel.
  def self.abort!
    return unless @active

    @active = false
    @phase = nil
    @enemies = []
    @actors = []
    @actions = []
    $game_temp.in_battle = false if $game_temp
    Rune::Log.info('Battle: Kampf abgebrochen')
  end

  # ------------------------------------------------------------- Eingabe

  # Ein Frame Kampf-UI: Tastatur steuert Befehls-/Item-/Zielwahl.
  # Läuft nur in der Befehlsphase; execute läuft ohne Eingabe durch.
  def self.update(dt)
    return unless @active

    case @phase
    when :command  then update_command
    when :item     then update_item
    when :target   then update_target
    when :execute  then update_execute(dt)
    end
  end

  # Befehlsphase: Angriff/Item/Flucht für den aktuellen Helden.
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
        log("#{actor.name}: Angriff gewählt")
        @target_index = 0
        @phase = :target
        klick
      when 'Item'
        if usable_items.empty?
          log('Keine benutzbaren Items!')
          Rune::Sound.play('ablehnen', 0.6) if defined?(Rune::Sound)
        else
          @item_index = 0
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
      @actions << { actor: current_actor, kind: :item, item: items[@item_index] }
      log("#{current_actor.name}: #{Party.item_name(items[@item_index])} gewählt")
      next_actor
    elsif Rune::Input.pressed?(:escape)
      @phase = :command
      klick
    end
  end

  # Zielwahl: lebender Gegner (links/rechts).
  def self.update_target
    lebendig = living_enemies
    if lebendig.empty?
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
      letzte = @actions.find { |a| a[:actor].equal?(current_actor) }
      letzte[:target] = ziel if letzte
      log("Ziel: #{ziel.name}")
      next_actor
    elsif Rune::Input.pressed?(:escape)
      @actions.reject! { |a| a[:actor].equal?(current_actor) }
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
        ziel = act[:target] || living_enemies.first
        next unless ziel
        angriff(actor, ziel)
      when :item
        item = act[:item]
        next unless item && Party.item_count(item).positive?
        if Party.use_item(item)
          log("#{actor.name} benutzt #{Party.item_name(item)}")
          sync_actors!
        end
      end
    end
    @actions = []

    return if beende_pruefen

    # Gegnerzug: jeder lebende Gegner schlägt einen zufälligen Helden.
    @enemies.each do |g|
      next if g.dead?
      ziel = living_actors.sample
      break unless ziel
      angriff(g, ziel, gegner: true)
    end

    @turn += 1
    beende_pruefen || beginn_runde
  end

  # ------------------------------------------------------------- intern

  def self.current_actor
    lebendig = living_actors
    return nil if @actor_index >= lebendig.size
    lebendig[@actor_index]
  end

  def self.next_actor
    @actor_index += 1
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
    @phase = :command
  end

  def self.living_enemies = @enemies.select(&:alive?)
  def self.living_actors  = @actors.select(&:alive?)

  # Benutzbare Items (Menge > 0, Typ heal/mana) - wie XP Window_Item.
  def self.usable_items
    Party.items.select do |id, menge|
      menge.positive? && %w[heal mana].include?(item_kind(id))
    end.keys.sort_by { |id| Party.item_name(id) }
  end

  def self.item_kind(id)
    defi = Party.item_def(id)
    defi ? defi['kind'].to_s : ''
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
    if living_enemies.empty?
      sieg
      true
    elsif living_actors.empty?
      niederlage
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
        a.defi.exp += exp if a.defi.respond_to?(:exp)
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
    $game_temp.in_battle = false if $game_temp
    if result == :niederlage && !ohne_gameover && $game_temp
      $game_temp.gameover = true
    end
    Rune::Log.info("Battle: Kampf beendet (#{result})")
  end

  # Helden-HP/MP der Battle-Kopien zurück in die Party-Actors schreiben.
  def self.sync_actors!
    @actors.each do |a|
      pa = a.defi
      next unless pa.respond_to?(:hp=)
      pa.hp = a.hp
      pa.mp = a.mp if pa.respond_to?(:mp=)
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
    Launcher.abdunkeln(w)

    # Befehlsfenster (links).
    if @phase == :command || @phase == :item || @phase == :target
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
        text = "#{Party.item_name(id)} x#{Party.item_count(id)}"
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
    end

    # Party-Status (unten links neben den Befehlen).
    pw = 340
    ph = 20 + Party.actors.size * 26
    px2 = 40
    py2 = h - ph - 20
    UI.panel(px2, py2, pw, ph, UI::PANEL, 0.88)
    Party.actors.each_with_index do |a, i|
      UI.text(px2 + 14, py2 + 8 + i * 26, a.hp_line,
              a.hp.zero? ? UI::DANGER : UI::TEXT, 14)
    end

    # Gegner-Liste (rechts).
    gw = 260
    gh = 20 + @enemies.size * 24
    gx = w - gw - 40
    gy2 = h - gh - 20
    UI.panel(gx, gy2, gw, gh, UI::PANEL, 0.88)
    @enemies.each_with_index do |g, i|
      UI.text(gx + 14, gy2 + 8 + i * 24, g.hp_line,
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
