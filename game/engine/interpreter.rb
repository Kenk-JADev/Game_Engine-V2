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
#    {"cmd":"loop","commands":[...]}                    (Schleife, 20c; XP 112/413)
#    {"cmd":"break"}                                    (Schleife verlassen, 20c)
#    {"cmd":"label","name":"marke"}                     (Sprungmarke, 20c)
#    {"cmd":"jump","name":"marke"}                      (Sprung, 20c; nur im Block)
#    {"cmd":"exit_event"}                               (Event beenden, 20c)
#    {"cmd":"erase_event"}                              (Event löschen, 20c; bis Kartenwechsel)
#    {"cmd":"button_input","name":"taste","keys":["e"]} (Tasteneingabe, 20c)
#    {"cmd":"text_options","position":"oben","frame":false,"transparent":true}
#                                                       (Fenster-Optionen, 20c)
#    {"cmd":"common_event","name":"zeit_ansage"}          (Gemeinsames Event, 20c; XP 117)
#    {"cmd":"if_gold","op":">=","value":100,"then":[...],"else":[...]}   (20d; XP 111)
#    {"cmd":"if_item","name":"heiltrank","op":">=","value":1,"then":[...],"else":[...]}
#    {"cmd":"if_script","code":"$game_party.gold > 100","then":[...],"else":[...]}
#    {"cmd":"save_access","allowed":false} / menu_access / encounter_access
#                                                       (20d; XP 134/135/136)
#    {"cmd":"timer","mode":"start","seconds":120}       (Timer, 20d; XP 124)
#    {"cmd":"shop","goods":[{"item":"heiltrank","price":20}]}   (Laden, 20e; XP 302)
#    {"cmd":"name_input","actor":"arik","max":8,"cancel":true}  (Name, 20e; XP 303)
#    {"cmd":"screen_tone","r":-34,"g":0,"b":68,"gray":0,"frames":60} (20f; XP 223)
#    {"cmd":"flash","color":"weiss","strength":255,"frames":20}       (20f; XP 224)
#    {"cmd":"shake","power":5,"speed":5,"frames":40}         (Wackeln, 20f; XP 225)
#    {"cmd":"show_picture","number":1,"name":"pictures/logo.png","x":320,"y":240}
#    {"cmd":"move_picture","number":1,"x":200,"frames":40}
#    {"cmd":"rotate_picture","number":1,"speed":5}
#    {"cmd":"tint_picture","number":1,"r":-60,"frames":20}
#    {"cmd":"erase_picture","number":1}                      (Bilder, 20f; XP 231-235)
#    {"cmd":"menu"}                                        (Menü öffnen, 20f; XP 351)
#    {"cmd":"if_battle_result","result":"sieg","then":[],"else":[]}   (20f; XP 601-603)
#    {"cmd":"if_actor","name":"arik","then":[],"else":[]}   (Wenn Actor, 20d)
#    {"cmd":"if_button","name":"e","then":[],"else":[]}     (Wenn Taste, 20d)
#    {"cmd":"actor_hp","target":"alle","amount":-20}    (Werte ändern, 20d)
#    {"cmd":"actor_sp"/"actor_exp"/"actor_level",...}   (XP 311/312/315/316)
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
    @button_input = nil    # Etappe 20c: Tasteneingabe blockiert hier
    @shop_wait = false     # Etappe 20e: Laden blockiert bis Shop.close
    @name_wait = false     # Etappe 20e: Namenseingabe blockiert bis NameInput.close
  end

  def running?
    !@stack.empty? || !@message.nil? || !@choices.nil? || @wait_left > 0.0 ||
      !@route_wait.nil? || !@num_input.nil? || @battle_wait || !@button_input.nil? ||
      @shop_wait || @name_wait
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
  # common: Name des gemeinsamen Events (Etappe 20c) - nur für Log/Tiefe.
  def start(commands, owner = nil, common: nil)
    return false if running?
    @owner = owner
    rahmen = { list: commands || [], index: 0 }
    rahmen[:common] = common if common
    @stack.push(rahmen)
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

    # Etappe 20c: Tasteneingabe (XP 105) blockiert, bis eine der Tasten kommt.
    # Gespeichert wird die 1-basierte Position in der Tastenliste (XP legt
    # seinen Tastencode ab; die Position ist im Event lesbarer - LOGIK D6).
    if @button_input
      taste = @button_input[:keys].index { |k| Rune::Input.pressed?(k.to_sym) }
      return if taste.nil?
      GameState.set_variable(@button_input[:variable], taste + 1)
      Rune::Log.info("Tasteneingabe: '#{@button_input[:keys][taste]}' -> " \
                     "#{@button_input[:variable]} = #{taste + 1}")
      @button_input = nil
    end

    # Etappe 19i: battle blockiert, bis Battle.active? endet.
    if @battle_wait
      return if defined?(Battle) && Battle.active?
      @battle_wait = false
    end

    # Etappe 20e: der Laden blockiert, bis das Fenster schließt (XP 302).
    if @shop_wait
      return if defined?(Shop) && Shop.active?
      @shop_wait = false
    end

    # Etappe 20e: Namenseingabe blockiert wie der Laden (XP 303).
    if @name_wait
      return if defined?(NameInput) && NameInput.active?
      @name_wait = false
    end

    # Kommandos abarbeiten, bis eines blockiert oder die Liste leer ist.
    64.times do
      frame = @stack.last
      break unless frame
      if frame[:index] >= frame[:list].size
        if frame[:loop] && !frame[:list].empty?
          frame[:index] = 0        # Schleife: von vorn (XP 413)
        else
          Rune::Log.warn('Interpreter: leere Schleife beendet') if frame[:loop]
          @stack.pop
          break
        end
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
      when 'if_gold'   # Etappe 20d: Wenn Gold (XP 111, Quelle Gold)
        branch = compare_zahl(Party.gold, cmd['op'], (cmd['value'] || 0).to_f)
        @stack.push(list: branch ? (cmd['then'] || []) : (cmd['else'] || []), index: 0)
      when 'if_item'   # Etappe 20d: Wenn Item (XP 111, Quelle Item)
        branch = compare_zahl(Party.item_count(cmd['name']), cmd['op'], (cmd['value'] || 0).to_f)
        @stack.push(list: branch ? (cmd['then'] || []) : (cmd['else'] || []), index: 0)
      when 'if_actor'   # Etappe 20d: Wenn Actor im Team (XP 111, Quelle Actor)
        branch = actor_im_team?(cmd['name'])
        @stack.push(list: branch ? (cmd['then'] || []) : (cmd['else'] || []), index: 0)
      when 'if_button'  # Etappe 20d: Wenn Taste gedrückt (XP 111, Quelle Button)
        branch = taste_gedrueckt?(cmd['name'])
        @stack.push(list: branch ? (cmd['then'] || []) : (cmd['else'] || []), index: 0)
      when 'if_battle_result'  # Etappe 20f (XP 601-603): Ergebnis des letzten
                               # Kampfs (Battle.result: :sieg/:flucht/:niederlage).
        ergebnis = defined?(Battle) && Battle ? Battle.result : nil
        branch = !cmd['result'].to_s.empty? && ergebnis.to_s == cmd['result'].to_s
        @stack.push(list: branch ? (cmd['then'] || []) : (cmd['else'] || []), index: 0)
      when 'if_script' # Etappe 20d: Wenn Script (XP 111, Quelle Script)
        @stack.push(list: script_zutreffend?(cmd) ? (cmd['then'] || []) : (cmd['else'] || []),
                    index: 0)
      when 'save_access'  # Etappe 20d: Speichern erlauben/sperren (XP 134)
        zugriff_setzen(cmd, :save_disabled)
      when 'menu_access'  # Etappe 20d: Menü erlauben/sperren (XP 135)
        zugriff_setzen(cmd, :menu_disabled)
      when 'encounter_access'  # Etappe 20d: Zufallskämpfe erlauben/sperren (XP 136)
        zugriff_setzen(cmd, :encounter_disabled)
      when 'timer'  # Etappe 20d: Timer steuern (XP 124)
        timer_steuern(cmd)
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
      when 'actor_hp'  # Etappe 20d: HP ändern (XP 311), negativ = Schaden
        Party.change_hp(cmd['target'], cmd['amount'])
      when 'actor_sp'  # Etappe 20d: SP/MP ändern (XP 312)
        Party.change_mp(cmd['target'], cmd['amount'])
      when 'actor_exp'  # Etappe 20d: Erfahrung ändern (XP 315, mit Aufstieg)
        Party.change_exp(cmd['target'], cmd['amount'])
      when 'actor_level'  # Etappe 20d: Stufe ändern (XP 316)
        Party.change_level(cmd['target'], cmd['amount'])
      when 'party_member'   # Etappe 20d (XP 129): {"cmd":"party_member","name":"arik","mode":"add"}
        mitglied(cmd)
      when 'name_input'   # Etappe 20e (XP 303): Held umbenennen
        @name_wait = NameInput.start(cmd['actor'] || cmd['name'],
                                     max: cmd['max'] || NameInput::STANDARD_MAX,
                                     cancel: cmd['cancel'] != false)
        return if @name_wait
      when 'shop'   # Etappe 20e (XP 302): Laden öffnen, blockiert bis Shop.close
        @shop_wait = Shop.start(cmd['goods'])
        return if @shop_wait
      when 'heal'   # {"cmd":"heal"} = voll, {"cmd":"heal","amount":30}
        Party.heal_all(cmd.key?('amount') ? cmd['amount'].to_i : 0)
      when 'weather'   # {"cmd":"weather","name":"schnee","intensity":1.5} (Etappe 17h)
        Weather.set(cmd['name'] || 'kein', (cmd['intensity'] || 1.0).to_f)
      when 'menu'   # Etappe 20f (XP 351): Menü öffnen - über $game_temp, wie
                    # ein Script ($game_temp.menu_calling). Der Scene-Wechsel
                    # passiert im nächsten Frame (TempActions.tick).
        if defined?($game_temp) && $game_temp
          $game_temp.menu_calling = true
          Rune::Log.info('Interpreter: Menü angefordert ($game_temp.menu_calling)')
        else
          Rune::Log.warn('Interpreter: menu ohne $game_temp (Kopfloser Lauf?)')
        end
      when 'show_picture'   # Etappe 20f (XP 231): Bild zeigen
        Pictures.show(cmd['number'], cmd['name'], x: cmd['x'] || 0, y: cmd['y'] || 0,
                      zoom_x: cmd['zoom_x'] || 100, zoom_y: cmd['zoom_y'] || 100,
                      opacity: cmd['opacity'] || 255, blend: cmd['blend'] || 0,
                      origin: cmd['origin'] || 0)
      when 'move_picture'   # Etappe 20f (XP 232): Bild bewegen/skalieren
        Pictures.move(cmd['number'], x: cmd['x'], y: cmd['y'],
                      zoom_x: cmd['zoom_x'], zoom_y: cmd['zoom_y'],
                      opacity: cmd['opacity'], blend: cmd['blend'],
                      origin: cmd['origin'], frames: cmd['frames'] || 40)
      when 'rotate_picture'  # Etappe 20f (XP 233): Drehgeschwindigkeit in Grad/Frame
        Pictures.rotate(cmd['number'], speed: cmd['speed'] || 0)
      when 'tint_picture'    # Etappe 20f (XP 234): Bild tönen
        Pictures.tint(cmd['number'],
                      tone: { r: cmd['r'], g: cmd['g'], b: cmd['b'], gray: cmd['gray'] },
                      frames: cmd['frames'] || 40)
      when 'erase_picture'   # Etappe 20f (XP 235): Bild löschen
        Pictures.erase(cmd['number'])
      when 'shake'   # Etappe 20f (XP 225): Bildschirmwackeln über die Kamera
        ScreenFx.shake(power: cmd['power'] || 5, speed: cmd['speed'] || 5,
                       frames: cmd['frames'] || 40)
      when 'screen_tone'   # Etappe 20f (XP 223): Bildschirmton, frames = Übergang
        ScreenFx.tone(r: cmd['r'], g: cmd['g'], b: cmd['b'],
                      gray: cmd['gray'], frames: cmd['frames'])
      when 'flash'   # Etappe 20f (XP 224): Blitz (weiss/rot/gruen/blau/gelb)
        ScreenFx.flash(cmd['color'] || cmd['name'],
                       strength: cmd['strength'] || ScreenFx::MAX_STAERKE,
                       frames: cmd['frames'] || 20)
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
      when 'loop'   # Schleife (Etappe 20c, XP 112/413)
        @stack.push(list: Array(cmd['commands']), index: 0, loop: true)
      when 'break'  # Schleife verlassen (Etappe 20c, XP 113)
        schleife_verlassen
      when 'label'  # Sprungmarke (Etappe 20c, XP 118) - zur Laufzeit nur Ziel
        nil
      when 'jump'   # Sprung zu Marke (Etappe 20c, XP 119)
        marken_sprung(cmd['name'])
      when 'exit_event'   # Event beenden (Etappe 20c, XP 115)
        @stack.clear
        return
      when 'erase_event'  # Event löschen (Etappe 20c, XP 116)
        erase_owner
        @stack.clear
        return
      when 'button_input' # Tasteneingabe (Etappe 20c, XP 105)
        tasten = Array(cmd['keys']).map(&:to_s).reject(&:empty?)
        if tasten.empty?
          Rune::Log.warn('Interpreter: button_input ohne Tasten übersprungen')
          Issues.report('Interpreter', 'button_input ohne Tasten')
        else
          @button_input = { variable: cmd['name'].to_s, keys: tasten }
          return
        end
      when 'common_event' # Gemeinsames Event aufrufen (Etappe 20c, XP 117)
        gemeinsames_event(cmd['name'])
      when 'text_options' # Fenster-Optionen (Etappe 20c, XP 104)
        MessageWindow.set(position: cmd['position'],
                          frame: cmd.key?('frame') ? !!cmd['frame'] : nil,
                          transparent: cmd.key?('transparent') ? !!cmd['transparent'] : nil)
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

  # Etappe 20d: Variable setzen/rechnen (XP 122). Neu ist `op` mit
  # set/add/sub/mul/div/mod; `value` ist dann der Operand. Die Altform ohne
  # `op` (value setzt, add addiert) bleibt gültig - der Editor stellt sie beim
  # nächsten Übernehmen um.
  #
  # Operatoren als Tabelle statt case: sync_test liest alle "when"-Zeilen mit
  # 4er-Einzug als Operator-Tabelle von compare_variable.
  VARIABLE_OPS = {
    'set' => ->(_v, x) { x },
    'add' => ->(v, x) { v + x },
    'sub' => ->(v, x) { v - x },
    'mul' => ->(v, x) { v * x },
    'div' => ->(v, x) { v / x },
    'mod' => ->(v, x) { v % x }
  }.freeze

  def apply_variable(cmd)
    name = cmd['name']
    return Rune::Log.warn("Interpreter: variable ohne name") unless name
    v = GameState.variable(name)
    if cmd.key?('op')
      op = cmd['op'].to_s
      rechner = VARIABLE_OPS[op]
      if rechner.nil?
        Rune::Log.warn("Interpreter: unbekannte Variable-Operation '#{op}'")
        Issues.report('Interpreter', "variable: Operation '#{op}' unbekannt")
        return false
      end
      operand = operand_fuer(cmd, v)
      if operand.nil?
        return false
      end
      if %w[div mod].include?(op) && operand.zero?
        Rune::Log.warn("Interpreter: variable #{op} durch 0 bei '#{name}' ignoriert")
        Issues.report('Interpreter', "variable '#{name}': #{op} durch 0")
        return false
      end
      GameState.set_variable(name, rechner.call(v, operand))
      return true
    end
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

  # Etappe 20c: innerste Schleife verlassen (XP 113). Ohne Schleife warnen -
  # der Befehl hat dann keine Wirkung, soll aber nichts kaputt machen.
  def schleife_verlassen
    tiefe = @stack.rindex { |rahmen| rahmen[:loop] }
    unless tiefe
      Rune::Log.warn('Interpreter: break ohne Schleife ignoriert')
      Issues.report('Interpreter', 'break ohne Schleife')
      return false
    end
    # Die Schleife selbst und alles, was in ihr noch offen ist, verlassen.
    @stack.slice!(tiefe..)
    true
  end

  # Etappe 20c: Sprung zur Marke im AKTUELLEN Block (XP 119 sucht ebenfalls
  # nur dort). Hinter der Marke geht es weiter; fehlt sie, bleibt der
  # Laufzeiger stehen und der Fund landet im Issue-Backlog.
  def marken_sprung(name)
    rahmen = @stack.last
    ziel = name.to_s
    return false unless rahmen

    treffer = rahmen[:list].index do |x|
      x.is_a?(Hash) && x['cmd'] == 'label' && x['name'].to_s == ziel
    end
    if treffer
      rahmen[:index] = treffer + 1
      true
    else
      Rune::Log.warn("Interpreter: Marke '#{ziel}' nicht gefunden")
      Issues.report('Interpreter', "Marke '#{ziel}' nicht gefunden")
      false
    end
  end

  # Etappe 20c: Event löschen (XP 116) - nur das auslösende Event, und nur
  # bis zum nächsten Kartenwechsel (wie XP: die Karte lädt die Events neu).
  def erase_owner
    unless @owner.respond_to?(:erase!)
      Rune::Log.warn('Interpreter: erase_event ohne Event-Besitzer ignoriert')
      Issues.report('Interpreter', 'erase_event ohne Besitzer')
      return false
    end
    @owner.erase!
    Rune::Log.info("Event '#{@owner.respond_to?(:name) ? @owner.name : '?'}' gelöscht")
    true
  end

  # Etappe 20c: Gemeinsames Event aufrufen (XP 117). Die Befehle laufen im
  # selben Interpreter weiter; das aufrufende Event setzt danach fort.
  # Unbekannte Namen und zu tiefe Aufrufketten werden gemeldet und
  # übersprungen (kein Absturz, kein Hänger).
  def gemeinsames_event(name)
    ziel = name.to_s
    if ziel.empty?
      Rune::Log.warn('Interpreter: common_event ohne name übersprungen')
      Issues.report('Interpreter', 'common_event ohne name')
      return false
    end
    tiefe = @stack.count { |r| r[:common] }
    if tiefe >= CommonEvents::MAX_TIEFE
      Rune::Log.warn("Interpreter: common_event '#{ziel}' zu tief verschachtelt")
      Issues.report('Interpreter', "common_event '#{ziel}' zu tief verschachtelt")
      return false
    end
    eintrag = Database.common_event(ziel)
    befehle = eintrag && eintrag['commands']
    unless befehle.is_a?(Array)
      Rune::Log.warn("Interpreter: gemeinsames Event '#{ziel}' fehlt")
      Issues.report('Interpreter', "gemeinsames Event '#{ziel}' fehlt")
      return false
    end
    @stack.push(list: befehle, index: 0, common: ziel)
    true
  end

  # Die when-Zeilen mit 4er-Einzug sind zugleich die Vergleichsoperator-Tabelle
  # für tools/sync_test.rb (MapCheck::VERGLEICH) - deshalb steht der case hier
  # und nicht in einem zweiten Helfer.
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

  # Etappe 20d: ein Vergleich für alle Quellen (Variable, Gold, Item).
  # Bewusst als Tabelle statt als zweites case: sync_test liest die when-
  # Zeilen mit 4er-Einzug als Vergleichsoperator-Tabelle (MapCheck::VERGLEICH)
  # aus, und die darf nur an EINER Stelle stehen (compare_variable).
  VERGLEICHE = {
    '==' => ->(a, b) { a == b },
    '!=' => ->(a, b) { a != b },
    '>'  => ->(a, b) { a > b },
    '>=' => ->(a, b) { a >= b },
    '<'  => ->(a, b) { a < b },
    '<=' => ->(a, b) { a <= b }
  }.freeze

  def compare_zahl(a, op, b)
    vgl = VERGLEICHE[op.to_s]
    unless vgl
      Rune::Log.warn("Interpreter: unbekannter Vergleich '#{op}'")
      return false
    end
    vgl.call(a, b)
  end

  # Etappe 20d: Timer (XP 124). "start" setzt die Restzeit (Sekunden), "stop"
  # hält an und löscht sie; die Anzeige hängt nur an $game_system.timer_working
  # (XP zeigt das Fenster automatisch, solange der Timer läuft).
  def timer_steuern(cmd)
    modus = (cmd['mode'] || 'start').to_s.downcase
    unless defined?($game_system) && $game_system.respond_to?(:timer_start)
      Rune::Log.warn('Interpreter: timer ohne $game_system ignoriert')
      Issues.report('Interpreter', 'timer ohne $game_system')
      return false
    end
    # Bewusst if/elsif statt case: sync_test liest alle "when"-Zeilen mit
    # 4er-Einzug als Operator-Tabelle von compare_variable.
    sekunden = (cmd['seconds'] || 0).to_f
    if modus != 'start' && modus != 'stop'
      Rune::Log.warn("Interpreter: timer-Modus '#{modus}' unbekannt (start/stop)")
      Issues.report('Interpreter', "timer-Modus '#{modus}' unbekannt")
      return false
    end
    if modus == 'start' && sekunden > 0
      $game_system.timer_start(sekunden)
      Rune::Log.info("Timer gestartet: #{sekunden} s")
    else
      # "stop" und "start" mit 0 s: laufenden Timer anhalten und löschen.
      Rune::Log.warn('Interpreter: timer start ohne Sekunden -> stop') if modus == 'start'
      $game_system.timer_working = false
      $game_system.timer = 0
      Rune::Log.info('Timer gestoppt') if modus == 'stop'
    end
    true
  end

  # Etappe 20d: Operand des variable-Befehls (XP 122, Quellen). Ohne `source`
  # gilt "const" (der Wert aus `value`). Rückgabe nil = Quelle unbekannt oder
  # unbrauchbar (dann bleibt die Variable stehen).
  QUELLEN = {
    'const' => ->(cmd, _v) { (cmd['value'] || 0).to_f },
    'random' => lambda { |cmd, _v|
      obergrenze = (cmd['value'] || 0).to_i
      if obergrenze <= 0
        Rune::Log.warn('Interpreter: variable random ohne Obergrenze')
        Issues.report('Interpreter', 'variable random ohne Obergrenze (> 0)')
        next nil
      end
      rand(obergrenze).to_f
    },
    'self' => ->(_cmd, v) { v },
    'variable' => lambda { |cmd, _v|
      quelle = cmd['src'].to_s.strip
      if quelle.empty?
        Rune::Log.warn('Interpreter: variable-Quelle ohne src')
        Issues.report('Interpreter', 'variable-Quelle ohne src')
        next nil
      end
      GameState.variable(quelle).to_f
    },
    'item' => lambda { |cmd, _v|
      quelle = cmd['src'].to_s.strip
      if quelle.empty? || !defined?(Party) || !Party.respond_to?(:item_count)
        Rune::Log.warn('Interpreter: variable-Itemquelle ohne src')
        Issues.report('Interpreter', 'variable-Itemquelle ohne src')
        next nil
      end
      Party.item_count(quelle).to_f
    }
  }.freeze

  def operand_fuer(cmd, aktueller_wert)
    quelle = cmd.key?('source') ? cmd['source'].to_s.strip.downcase : 'const'
    quelle = 'const' if quelle.empty?
    rechner = QUELLEN[quelle]
    if rechner.nil?
      Rune::Log.warn("Interpreter: variable-Quelle '#{quelle}' unbekannt")
      Issues.report('Interpreter', "variable-Quelle '#{quelle}' unbekannt")
      return nil
    end
    rechner.call(cmd, aktueller_wert)
  end

  # Etappe 20d (XP 129, Kommando "Mitglieder ändern"): add/remove. XP kennt
  # nur add/remove; alles andere landet im Issue-Backlog, damit ein Tippfehler
  # im Karteneditor nicht still das Team umbaut.
  def mitglied(cmd)
    modus = cmd.key?('mode') ? cmd['mode'].to_s.strip.downcase : 'add'
    modus = 'add' if modus.empty?
    name = cmd['name'].to_s.strip
    if name.empty?
      Rune::Log.warn("Interpreter: party_member ohne name")
      Issues.report('Interpreter', 'party_member ohne name')
      return
    end
    ergebnis = case modus
               when 'add'    then Party.member_add(name)
               when 'remove' then Party.member_remove(name)
               else
                 Rune::Log.warn("Interpreter: party_member-Modus '#{modus}' unbekannt")
                 Issues.report('Interpreter', "party_member-Modus '#{modus}' unbekannt")
                 return
               end
    case ergebnis
    when :unbekannt
      Rune::Log.warn("Interpreter: party_member '#{name}' nicht in der Datenbank")
      Issues.report('Interpreter', "party_member '#{name}' nicht in der Datenbank")
    when :letzter
      Rune::Log.warn("Interpreter: party_member '#{name}' ist das letzte Mitglied")
      Issues.report('Interpreter', "party_member: '#{name}' ist das letzte Mitglied und bleibt")
    end
  end

  # Etappe 20d: "Wenn Actor" - XP fragt $game_party.actors, also die
  # Mitgliederliste (Party.actors, seit party_member veraenderbar).
  def actor_im_team?(name)
    id = name.to_s.strip
    return false if id.empty?
    return false unless defined?(Party) && Party.respond_to?(:find)
    !Party.find(id).nil?
  end

  # Etappe 20d: "Wenn Taste" (XP 111, Quelle Button) - wie XP Input.trigger?
  # gilt nur der Frame, in dem die Taste gedrückt wurde. Ohne Eingabeschicht
  # (kopfloser Test) ist die Bedingung falsch.
  def taste_gedrueckt?(name)
    taste = name.to_s.strip
    return false if taste.empty?
    return false unless defined?(Rune::Input)
    eingabe = Rune::Input
    return eingabe.pressed?(taste.to_sym) if eingabe.respond_to?(:pressed?)
    return eingabe.down?(taste.to_sym) if eingabe.respond_to?(:down?)
    false
  end

  # Etappe 20d: "Wenn Script" - der Ausdruck liefert selbst den Wahrheitswert
  # (XP 111, Seite "Script"). Fehler gelten als falsch.
  def script_zutreffend?(cmd)
    code = cmd['code'].to_s
    if code.strip.empty?
      Rune::Log.warn('Interpreter: if_script ohne code -> Sonst-Zweig')
      return false
    end
    !!Scripts.eval_code(cmd['name'] || 'bedingung', code, self, @owner)
  end

  # Etappe 20d: 134/135/136 - Speichern/Menü/Zufallskämpfe erlauben oder
  # sperren. Die Sperren liegen in $game_system (RGSS) und landen im Spielstand.
  def zugriff_setzen(cmd, feld)
    erlaubt = cmd.key?('allowed') ? !!cmd['allowed'] : true
    unless defined?($game_system) && $game_system.respond_to?("#{feld}=")
      Rune::Log.warn("Interpreter: #{cmd['cmd']} ohne $game_system ignoriert")
      Issues.report('Interpreter', "#{cmd['cmd']} ohne $game_system")
      return false
    end
    $game_system.public_send("#{feld}=", !erlaubt)
    Rune::Log.info("Zugriff: #{feld} = #{!erlaubt}")
    true
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