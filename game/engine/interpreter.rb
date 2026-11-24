#encoding: utf-8
# ============================================================================
#  Event-Interpreter (angelehnt an RPG-Maker, aber schlank).
#
#  Kombi-Fenster (Z8, RPG-Maker-XP): Text, Wahl und Zahleneingabe teilen sich
#  EIN Fenster. Ein text-Kommando setzt die sichtbaren Zeilen; die Wahl
#  erscheint darunter im selben Fenster (früher: zweites Fenster). Das Fenster
#  bleibt offen, solange der Nachrichtenblock läuft (text/choices/
#  input_number) und schließt beim ersten anderen Kommando - wie in XP.
#
#  Führt eine Kommandoliste aus. Einige Kommandos sind "blockierend":
#  text (wartet auf Bestätigung) und warte (wartet auf Zeit). Solange der
#  Interpreter läuft, zeigt die Engine das Textfenster und sperrt das
#  Aktivieren weiterer Events (nur ein Interpreter zur Zeit).
#
#  Unterstützte Kommandos (game/data/maps/*.json):
#    {"cmd":"text","lines":["...", "..."]}
#    {"cmd":"choices","options":["Ja","Nein"],"branches":[[...],[...]],
#     "cancel":"aus"|"verzweigung"|n,"cancel_branch":[...]}
#                               (18f; cancel/Abbruch-Zweig Z5, n = Optionsnummer 0-basiert)
#    {"cmd":"switch","name":"a","value":true|false}
#    {"cmd":"toggle_switch","name":"a"}                                    (18f)
#    {"cmd":"variable","name":"a","value":n}        | {"add":n}
#    {"cmd":"if_switch","name":"a","value":b,"then":[...],"else":[...]}
#    {"cmd":"if_variable","name":"a","op":">=","value":n,"then":[...],"else":[...]}
#    {"cmd":"wait","seconds":n}
#    {"cmd":"teleport","x":n,"z":n,"yaw":n}
#    {"cmd":"move_player","x":n,"z":n,"relative":true|false}               (18f)
#    {"cmd":"log","text":"..."}
#    {"cmd":"item","name":"heiltrank","count":n}     | {"take":n}
#    {"cmd":"gold","amount":n}                       (negativ = abziehen)
#    {"cmd":"heal","amount":n}                       (leer/0 = voll heilen)
#    {"cmd":"weather","name":"regen","intensity":n}  (kein/regen/schnee, Etappe 17h)
#    {"cmd":"self_switch","letter":"A","value":true}     (Self-Switch, Etappe 19d)
#    {"cmd":"if_self_switch","letter":"A","value":b,"then":[...],"else":[...]}  (19d)
#    {"cmd":"fade","direction":"aus","seconds":0.5}      (Bildschirmblende, 19d)
#    {"cmd":"move_route","target":"Wanderer","steps":["up","warte"],
#     "repeat":false,"wait":false,"speed":2.0}        (Bewegungsroute, 19g)
#    {"cmd":"input_number","name":"ziffern","digits":3,"initial":0,
#     "cancel":true}                            (Zahleneingabe, 19i)
#    {"cmd":"battle","name":"waldweg","can_escape":true,"can_lose":false}
#                                                       (Kampf, 19i)
#    {"cmd":"save_game","slot":1} / {"cmd":"load_game","slot":1}           (18f)
#    {"cmd":"script","name":"projekt_script"}  |  {"cmd":"script","code":"..."}
# ============================================================================

class Interpreter
  # @message = Zeilen, die auf Bestätigung warten (Kompatibilitäts-Zugang für
  # Tests/Skripte); das Fenster zeichnet `visible_lines`.
  attr_reader :message, :choices

  # Kommandos, die das Nachrichtenfenster offen halten (XP-Nachrichtenblock).
  NACHRICHTEN_BEFEHLE = %w[text choices input_number].freeze

  def initialize
    @stack = []      # Rahmen {list:, index:}
    @message = nil   # Zeilen, die auf die Bestätigung warten (nil = frei)
    @lines = []      # sichtbare Zeilen im Nachrichtenfenster (Kombi-Fenster)
    @choices = nil   # Wahlmöglichkeiten {options:, branches:, sel:}
    @wait_left = 0.0
    @route_wait = nil  # Etappe 19g: move_route mit wait blockiert hier
    @num_input = nil   # Etappe 19i: Zahleneingabe blockiert hier
    @battle_wait = false   # Etappe 19i: battle blockiert bis Kampfende
  end

  def running?
    !@stack.empty? || !@message.nil? || !@choices.nil? || @wait_left > 0.0 ||
      !@route_wait.nil? || !@num_input.nil? || @battle_wait
  end

  # Zahleneingabe (19i): UI-Status für Game.draw / UI.nummer_eingabe.
  attr_reader :num_input

  def busy_number?
    !@num_input.nil?
  end

  def busy_battle?
    @battle_wait
  end

  # Sichtbare Zeilen im Nachrichtenfenster - bleibt während einer Wahl
  # darunter stehen (Kombi-Fenster, Z8).
  def visible_lines
    @lines
  end

  # Fenster offen? (Zeilen sichtbar, auch wenn gerade die Wahl läuft)
  def busy_text?
    !@lines.empty?
  end

  # Wartet der Text auf Bestätigung? (nur dann blinkt der ▾-Zeiger)
  def text_waiting?
    !@message.nil?
  end

  def busy_choices?
    !@choices.nil?
  end

  # Aus einem Projekt-Script: Textfenster öffnen (blockiert wie 'text').
  def say(lines)
    @lines = Array(lines)
    @message = @lines
  end

  # Startet eine Kommandoliste. Wird bereits gesprochen, wird ignoriert.
  # owner: auslösendes Objekt (z. B. Event/NPC) für Effekt-Ursprung etc.
  def start(commands, owner = nil)
    return false if running?
    @owner = owner
    @stack.push(list: commands || [], index: 0)
    true
  end

  # Einen Frame weiterdrehen. Rendert bei Bedarf das Nachrichtenfenster.
  def update(dt)
    if @message
      advance_text
      return
    end

    if @choices
      advance_choices
      return
    end

    if @wait_left > 0.0
      @wait_left -= dt
      return
    end

    # Etappe 19g: move_route mit "wait" - blockieren, bis der Mover fertig ist.
    if @route_wait
      return if Mover.running?(@route_wait)
      @route_wait = nil
    end

    # Etappe 19i: Zahleneingabe blockiert; Eingabe läuft über UI.nummer_eingabe.
    return if @num_input

    # Etappe 19i: battle blockiert, bis Battle.active? endet.
    if @battle_wait
      return if defined?(Battle) && Battle.active?
      @battle_wait = false
    end

    # Kommandos abarbeiten, bis eines blockiert oder die Liste leer ist.
    64.times do
      frame = @stack.last
      break unless frame
      if frame[:index] >= frame[:list].size
        @stack.pop
        break
      end

      cmd = frame[:list][frame[:index]]
      frame[:index] += 1

      # Kombi-Fenster (Z8): Text/Wahl/Zahl halten das Fenster offen; jeder
      # andere Befehl schließt es wie in XP (der Block ist zu Ende).
      typ = cmd.is_a?(Hash) ? cmd['cmd'].to_s : ''
      @lines = [] unless NACHRICHTEN_BEFEHLE.include?(typ)

      case cmd['cmd']
      when 'text'
        @lines = Array(cmd['lines'])
        @message = @lines
        return
      when 'choices'
        # Wahlmöglichkeiten (RPG-Maker-Show-Choices): blockiert, bis der
        # Spieler wählt; dann läuft der passende Zweig weiter. cancel
        # (XP "When Cancel", Z5): aus (Default) = Esc ignoriert,
        # "verzweigung" = eigener Abbruch-Zweig, Zahl N = Esc wie Option N.
        opts = Array(cmd['options']).map(&:to_s)
        next if opts.empty?
        @choices = { options: opts, branches: Array(cmd['branches']), sel: 0,
                     cancel: cancel_modus(cmd['cancel'], opts.size),
                     cancel_branch: Array(cmd['cancel_branch']) }
        return
      when 'wait'      then @wait_left = (cmd['seconds'] || 0.0).to_f; return
      when 'switch'    then GameState.set_switch(cmd['name'], !!cmd['value'])
      when 'toggle_switch' then GameState.toggle_switch(cmd['name'])
      when 'self_switch'   then apply_self_switch(cmd)
      when 'if_self_switch'
        branch = self_switch_zutrifft?(cmd)
        @stack.push(list: branch ? (cmd['then'] || []) : (cmd['else'] || []), index: 0)
      when 'variable'  then apply_variable(cmd)
      when 'if_switch'
        branch = GameState.switch(cmd['name']) == !!cmd['value']
        @stack.push(list: branch ? (cmd['then'] || []) : (cmd['else'] || []), index: 0)
      when 'if_variable'
        branch = compare_variable(cmd)
        @stack.push(list: branch ? (cmd['then'] || []) : (cmd['else'] || []), index: 0)
      when 'teleport'
        Rune::Player.teleport(cmd['x'].to_f, Game.gy(cmd['x'].to_f, cmd['z'].to_f) + 0.2,
                              cmd['z'].to_f)
        Game.reset_step_tracking if Game.respond_to?(:reset_step_tracking)
        Rune::Camera.yaw = (cmd['yaw'] || Math::PI).to_f
      when 'move_player'
        # Spieler bewegen (18f): absolut oder relativ zur aktuellen Position.
        px, _py, pz = Rune::Player.position
        tx = cmd['relative'] ? px + cmd['x'].to_f : cmd['x'].to_f
        tz = cmd['relative'] ? pz + cmd['z'].to_f : cmd['z'].to_f
        Rune::Player.teleport(tx, Game.gy(tx, tz) + 0.2, tz)
        Game.reset_step_tracking if Game.respond_to?(:reset_step_tracking)
      when 'save_game'
        Game.save_game((cmd['slot'] || 1).to_i)
      when 'load_game'
        # Achtung: Lädt die Karte neu und ersetzt damit auch diesen
        # Interpreter (Game.enter_map baut @interpreter neu). Befehle
        # nach load_game in derselben Liste laufen nicht mehr weiter.
        Game.load_game((cmd['slot'] || 1).to_i)
        return
      when 'log'
        Rune::Log.info("[Event] #{cmd['text']}")
      when 'script'
        code = cmd['code'].to_s
        if code.empty?
          Scripts.run(cmd['name'], self, @owner)
        else
          Scripts.run_code(cmd['name'] || 'inline', code, self, @owner)
        end
        return if @message
      when 'sound'
        Rune::Sound.play(cmd['name'].to_s, (cmd['volume'] || 1.0).to_f)
      when 'effect'
        # Ursprung: Event-Position (wenn vorhanden), sonst der Spieler.
        if @owner.respond_to?(:x) && @owner.respond_to?(:z)
          ox = @owner.x.to_f
          oz = @owner.z.to_f
          oy = Game.gy(ox, oz) + (cmd['height'] || 1.0).to_f
        else
          px, py, pz = Rune::Player.position
          ox, oy, oz = px, py + 1.0, pz
        end
        case cmd['name'].to_s
        when 'sparkle' then Effects.sparkle(ox, oy, oz)
        when 'dust'    then Effects.dust(ox, oy, oz)
        else Effects.burst(ox, oy, oz, color: (cmd['color'] || 0xFFD866))
        end
        Rune::Log.info("[Event] Effekt: #{cmd['name']} (#{Effects.count} Partikel)")
        Rune::Log.info("[Event] Sound: #{cmd['name']}")
      when 'item'   # {"cmd":"item","name":"heiltrank","count":2} / {"take":1}
        key = cmd['name'].to_s
        if key.empty?
          Rune::Log.warn("Interpreter: item ohne name")
        elsif cmd.key?('take')
          Party.take_item(key, cmd['take'].to_i)
        else
          Party.add_item(key, (cmd['count'] || 1).to_i)
        end
      when 'gold'   # {"cmd":"gold","amount":50}  (negativ = abziehen)
        Party.add_gold(cmd['amount'].to_i)
      when 'heal'   # {"cmd":"heal"} = voll, {"cmd":"heal","amount":30}
        Party.heal_all(cmd.key?('amount') ? cmd['amount'].to_i : 0)
      when 'weather'   # {"cmd":"weather","name":"schnee","intensity":1.5} (Etappe 17h)
        Weather.set(cmd['name'] || 'kein', (cmd['intensity'] || 1.0).to_f)
      when 'fade'   # {"cmd":"fade","direction":"aus","seconds":0.5} (Etappe 19d)
        Fade.start(cmd['direction'] || 'aus', cmd['seconds'] || 0.5)
      when 'input_number'   # Zahleneingabe (Etappe 19i, RPG Maker XP)
        digits = (cmd['digits'] || 1).to_i.clamp(1, 6)
        start = (cmd['initial'] || 0).to_i.clamp(0, 10**digits - 1)
        @num_input = { variable: cmd['name'].to_s, digits: digits,
                       ziffern: start.digits(10).then { |d| Array.new(digits) { |i| d[i] || 0 } }.reverse,
                       sel: 0, cancel: cmd['cancel'] != false }
        return
      when 'battle'   # Kampf-Grundgerüst (Etappe 19i, RPG Maker XP)
        if defined?(Battle)
          ok = Battle.start(cmd['name'], can_escape: cmd['can_escape'] != false,
                            can_lose: cmd['can_lose'] == true)
          if ok
            @battle_wait = true
            return
          end
        else
          Rune::Log.warn('Interpreter: battle ohne Battle-Modul übersprungen')
        end
      when 'move_route'   # Bewegungsroute (Etappe 19g, RPG Maker XP)
        route = Mover.start(cmd['target'], cmd['steps'], repeat: cmd['repeat'],
                            speed: cmd['speed'], code: cmd['code'])
        if route && cmd['wait']
          if cmd['repeat']
            Rune::Log.warn('Interpreter: move_route wait+repeat - Warten ' \
                           'würde nie enden (Route läuft endlos weiter)')
          else
            @route_wait = route
            return
          end
        end
      else
        Rune::Log.warn("Interpreter: unbekanntes Kommando '#{cmd['cmd']}' übersprungen")
        Issues.report('Interpreter', "unbekanntes Kommando '#{cmd['cmd']}'")
      end
    end

    # Nichts mehr auf dem Stapel und keine Eingabe offen: Fenster zu.
    @lines = [] if @stack.empty? && @message.nil?
  end

  private

  def apply_variable(cmd)
    name = cmd['name']
    return Rune::Log.warn("Interpreter: variable ohne name") unless name
    v = GameState.variable(name)
    v = cmd['value'].to_f if cmd.key?('value')
    v = v + cmd['add'].to_f if cmd.key?('add')
    GameState.set_variable(name, v)
  end

  # Self-Switch (Etappe 19d, RPG Maker XP): Schalter A-D, die nur für das
  # auslösende Event gelten. Ohne Besitzer (z. B. Intro-Aufruf) sind sie
  # sinnlos - dann warnen und überspringen bzw. den Sonst-Zweig nehmen.
  def apply_self_switch(cmd)
    key = @owner.respond_to?(:self_switch_key) ? @owner.self_switch_key : nil
    unless key
      Rune::Log.warn('Interpreter: self_switch ohne Event-Besitzer ignoriert')
      Issues.report('Interpreter', 'self_switch ohne Besitzer')
      return
    end
    GameState.set_self_switch(key, cmd['letter'], !!cmd['value'])
  end

  def self_switch_zutrifft?(cmd)
    key = @owner.respond_to?(:self_switch_key) ? @owner.self_switch_key : nil
    unless key
      Rune::Log.warn('Interpreter: if_self_switch ohne Besitzer -> Sonst-Zweig')
      return false
    end
    GameState.self_switch(key, cmd['letter']) == !!cmd['value']
  end

  def compare_variable(cmd)
    a = GameState.variable(cmd['name'])
    b = (cmd['value'] || 0).to_f
    case cmd['op']
    when '==' then a == b
    when '!=' then a != b
    when '>'  then a > b
    when '>=' then a >= b
    when '<'  then a < b
    when '<=' then a <= b
    else Rune::Log.warn("Interpreter: unbekannter Vergleich '#{cmd['op']}'"); false
    end
  end

  # Nächste Zeile zeigen. Nach der letzten Zeile bleibt der Text sichtbar
  # (Kombi-Fenster): erst der nächste Befehl entscheidet, ob das Fenster
  # zugeht, weil eine Wahl darunter folgt oder der Block endet.
  def advance_text
    if Rune::Input.pressed?(:e) || Rune::Input.pressed?(:space) ||
       Rune::Input.pressed?(:enter)
      if @message.size > 1
        @message = @message[1..]
        @lines = @message
      else
        @message = nil
      end
    end
  end

  # Wahl treffen: Pfeiltasten (oder Mausklick), E/Enter bestätigt.
  # Der gewählte Zweig wird auf den Stack gelegt und läuft weiter.
  # Abbruch (Z5, XP "When Cancel"): Esc nur bei erlaubtem cancel -
  # "verzweigung" führt den eigenen Zweig, eine Zahl bestätigt wie
  # diese Option; "aus" (XP-Default) ignoriert Esc komplett.
  def advance_choices
    c = @choices
    n = c[:options].size
    abbruch = false
    if c[:cancel] != :aus && Rune::Input.pressed?(:escape)
      if c[:cancel] == :verzweigung
        zweige = c[:cancel_branch]
        @choices = nil
        Rune::Sound.play('ablehnen', 0.7)
        @stack.push(list: zweige, index: 0)
        return
      end
      # cancel ist eine Optionsnummer (0-basiert, in cancel_modus geprüft).
      c[:sel] = c[:cancel]
      abbruch = true
    end
    unless abbruch
      if Rune::Input.pressed?(:up)
        c[:sel] = (c[:sel] - 1 + n) % n
        Rune::Sound.play('klick', 0.5)
      elsif Rune::Input.pressed?(:down)
        c[:sel] = (c[:sel] + 1) % n
        Rune::Sound.play('klick', 0.5)
      end
      return unless Rune::Input.pressed?(:e) || Rune::Input.pressed?(:space) ||
                    Rune::Input.pressed?(:enter)
    end
    branch = c[:branches][c[:sel]] || []
    @choices = nil
    Rune::Sound.play('klick', 0.7)
    @stack.push(list: branch, index: 0)
  end

  # XP-konformer Abbruch-Modus eines Wahlbefehls (Z5): abwesend/false/
  # "aus" = nicht erlaubt (XP-Default), "verzweigung" = eigener Abbruch-
  # Zweig (XP 403), Zahl N (0-basiert, auch als Ziffernstring) = Esc
  # verhält sich wie Option N. Unbekannte Werte und Zahlen ausserhalb
  # der Optionen fallen auf "nicht erlaubt" zurück und werden geloggt
  # (MapCheck meldet beides zusätzlich statisch).
  def cancel_modus(roh, anzahl)
    # Bewusst if/elsif statt case: sync_test liest when-Zeilen mit
    # 4er-Einzug als Operator-Vergleiche (MapCheck::VERGLEICH) aus.
    if roh.nil? || roh == false || roh == 'aus'
      :aus
    elsif roh == true || roh == 'verzweigung'
      # true ist das Synonym von "verzweigung" (Editor zeigt es so an,
      # MapCheck und Issue-Scan lassen es zu).
      :verzweigung
    elsif roh.is_a?(Integer)
      if roh >= 0 && roh < anzahl
        roh
      else
        Rune::Log.warn("Interpreter: choices cancel=#{roh} ausserhalb 0...#{anzahl - 1}, behandelt wie aus")
        :aus
      end
    elsif roh.is_a?(String)
      n = Integer(roh, 10, exception: false)
      if n && n >= 0 && n < anzahl
        n
      else
        Rune::Log.warn("Interpreter: choices cancel='#{roh}' unbekannt, behandelt wie aus")
        :aus
      end
    else
      Rune::Log.warn("Interpreter: choices cancel=#{roh.inspect} unbekannt, behandelt wie aus")
      :aus
    end
  end
end