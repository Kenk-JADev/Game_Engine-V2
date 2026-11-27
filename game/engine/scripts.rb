#encoding: utf-8
# ============================================================================
#  Projekt-Scripts (Etappe 17e).
#
#  Jedes Projekt darf unter data/scripts.json benannte Ruby-Schnipsel ablegen:
#
#    {"scripts":[{"name":"gruss","code":"say ['Hallo!']"}]}
#
#  Events rufen sie mit {"cmd":"script","name":"gruss"} auf; alternativ trägt
#  ein Befehl den Code direkt in "code". Der Code läuft in einem Context, der
#  die RPG-Maker-typischen Helfer anbietet (say, switch, variable, teleport,
#  sound, effect, log, fade, self_switch). Das ist keine Sicherheitsbox:
#  Ruby-Code kann auch auf den Interpreter und das übrige Programm zugreifen.
#
#  Seit Etappe 19e steht Project-Scripts zusätzlich die RGSS-Schicht offen
#  (game/engine/rgss.rb): $game_switches/$game_party/$game_player/...,
#  Graphics.fadeout, Input.trigger?, Audio.se_play - die RPG-Maker-XP-API
#  für eigenes Custom-Ruby im Script-Editor (F11).
#
#  Kopflos testbar: ruby -I game/engine tools/script_test.rb
# ============================================================================

require 'json'
require 'battle'   # Etappe 19i: Context-Helfer battle() braucht Battle

# Namensraum für die XP-Defaultscripts (Etappe 28, XP-Script-Lader). Die
# Klassen landen als XP::Game_Party, XP::Scene_Map usw. und überschreiben so
# weder die Rune3D-Klassen (Interpreter, Scene_Title, Window_Selectable ...)
# noch scheitern sie an "superclass mismatch". Konstanten ohne Präfix
# (Window, Sprite, Bitmap) lösen weiter zu den Rune3D-Klassen auf.
module XP
end

module Scripts
  @defs = {}
  # Etappe 28b (XP-Logik): EINE geordnete Startliste wie Scripts.rxdata.
  # Einträge [name, code, :xp | :project] in Script-Editor-Reihenfolge.
  @startup = []
  @reported = {}        # bereits gezeigte Fehler (kein Doppelfenster bei Titel+Start)
  @errors = []          # Fehler des letzten run_startup (XP: höchstens einer)

  # Etappe 39: 089 Main steht NICHT in @startup (kein Blockieren des
  # Eval-Durchlaufs), wird aber als Fiber gestartet und pro Frame resumed.
  XP_BLOCKED = [89].freeze
  @main_code = nil
  @main_name = nil
  @main_fiber = nil
  @yielded_this_resume = false

  module_function

  def errors = @errors.dup

  # scripts.json des aktiven Projekts laden (fehlende Datei ist kein Fehler).
  def load_project
    @defs = {}
    @startup = []
    @main_code = nil
    @main_name = nil
    path = File.join(Database.root, 'scripts.json')
    return true unless File.exist?(path)

    data = JSON.parse(File.read(path, encoding: 'UTF-8'))
    list = data.is_a?(Array) ? data : (data['scripts'] || [])
    list.each do |h|
      next unless h.is_a?(Hash)
      name = h['name'].to_s
      next if name.empty?
      code = h['code'].to_s
      @defs[name] = code
      # Etappe 39: 089 ist der Systemeinstieg. Er bleibt aus @startup draussen,
      # wird aber separat als Fiber gestartet (siehe start_main).
      if main_script?(h, name)
        @main_code = code
        @main_name = name
      end
      # XP-Defaultscripts (Schlüssel xp_default = "000".."089") laufen im
      # XP-Namensraum; 089 Main nicht im Eval-Durchlauf, sondern als Fiber.
      # Nur ausdrücklich markierte Scripte automatisch ausführen; alte
      # Event-Scripte bleiben unverändert und laufen nur auf Anfrage.
      next unless h['autoload'] == true
      if h.key?('xp_default')
        next if XP_BLOCKED.include?(h['xp_default'].to_s.to_i)
        @startup << [name, code, :xp]
      else
        @startup << [name, code, :project]
      end
    end
    Rune::Log.info("Scripts: #{@defs.size} Projekt-Scripts geladen")
    true
  rescue StandardError => e
    Rune::Log.error("Scripts: scripts.json nicht lesbar: #{e.message}")
    Issues.report('Scripts', "scripts.json nicht lesbar: #{e.message}", severity: 'error')
    false
  end

  def names = @defs.keys.sort
  def defined?(name) = @defs.key?(name.to_s)

  # Wie eine RGSS-Scriptliste: explizit markierte Einträge in Dateireihenfolge
  # im Top-Level ausführen, damit auch Klassendefinitionen möglich sind. Die
  # RGSS-Globalen und die erste Karte stehen bereits bereit. F5/Neustart führt
  # sie erneut aus. Das ist vertrauenswürdiger Ruby-Code, keine Sicherheitsbox.
  # Ablauf wie RPG Maker XP (Scripts.rxdata): alle aktiven Einträge strikt in
  # Listenreihenfolge; der Dateiname im Backtrace ist "Section###" wie bei XP
  # und wird für die Meldung auf den Scriptnamen abgebildet. Der ERSTE Fehler
  # bricht das Laden ab (XP: "Script 'X' line N: ... occurred.").
  # Abweichungen: XP-Defaults laufen im Namensraum XP (sonst überschrieben sie
  # die Engine-Klassen), 089 Main läuft hier nicht im Eval-Durchlauf, sondern
  # danach als Fiber (start_main), und das Spielfenster bleibt nach der
  # Meldung offen (XP beendet sich).
  def run_startup
    @errors = []
    @startup.each_with_index do |(name, code, kind), index|
      section = section_name(index)
      if kind == :xp
        XP.module_eval(code, section, 1)
      else
        TOPLEVEL_BINDING.eval(code, section, 1)
      end
    rescue StandardError, ScriptError => e
      startup_error(name, e)
      break
    end
    show_error_window
    @errors.empty?
  end

  # 089 - Main aus scripts.json erkennen (XP-Schlüssel oder Name). Ein
  # eigener Eintrag ohne xp_default zählt ebenfalls, wenn er so heisst.
  def main_script?(entry, name)
    h = entry.is_a?(Hash) ? entry : {}
    h['xp_default'].to_s.to_i == 89 || name.to_s.match?(/\A089\s*-\s*Main\b/i)
  end

  # Ob die geladene Scriptliste einen echten 089-Einstieg besitzt. Ohne Main
  # bleibt die bisherige Frame-Konvention unverändert (alte Projekte).
  def main_code? = !@main_code.to_s.strip.empty?

  # ---------------------------------------------------------------- Main-Fiber
  #
  # XP laesst 089 - Main als blockierende while-Schleife laufen. Rune3D gibt
  # den Frame-Takt vor: Der Code laeuft in einem Fiber und yieldet ueber die
  # guarded Helper in Scene_Base#main / Graphics.update genau einmal pro
  # Resume zurueck. Kein C++-Loop, kein Einfrieren, kein FiberError im
  # Root-Fiber. 089 bleibt weiterhin nicht Teil von @startup.
  def start_main
    stop_main
    code = @main_code.to_s
    if code.strip.empty?
      Rune::Log.info('Scripts: kein 089 - Main in scripts.json (Engine-Takt)')
      return false
    end

    @yielded_this_resume = false
    @main_fiber = Fiber.new do
      TOPLEVEL_BINDING.eval(code, '089 - Main', 1)
    end
    @main_fiber.resume
    Rune::Log.info("Scripts: #{@main_name || '089 - Main'} als Frame-Fiber gestartet")
    true
  rescue StandardError, ScriptError => e
    main_error(e)
    stop_main
    false
  end

  # Ein Resume pro Frame aus Rune.tick. Ein already beendeter Fiber wird
  # verworfen; ein Fehler geht durch dieselbe Pipeline wie Start-Scriptfehler.
  def tick_main
    fiber = @main_fiber
    return false unless fiber&.alive?

    @yielded_this_resume = false
    fiber.resume
    @main_fiber = nil unless fiber.alive?
    true
  rescue FiberError => e
    Rune::Log.warn("Scripts: Main-Fiber nicht fortsetzbar: #{e.message}")
    stop_main
    false
  rescue StandardError, ScriptError => e
    main_error(e)
    stop_main
    false
  end

  # Für Game.update: Root-Fiber darf $scene nicht zusätzlich updaten, solange
  # der Main-Fiber die Szene treibt.
  def main_active? = !@main_fiber.nil? && @main_fiber.alive?

  # Nur im Fiber selbst darf Fiber.yield gerufen werden.
  def main_running? = main_active? && Fiber.current.equal?(@main_fiber)

  # Idempotenter Yield-Punkt: Scene_Base#main und Graphics.update rufen ihn
  # beide; pro Resume gibt es genau einen Frame-Wechsel.
  def yield_frame
    return unless main_running?
    return if @yielded_this_resume

    @yielded_this_resume = true
    Fiber.yield
  end

  def stop_main
    fiber = @main_fiber
    @main_fiber = nil
    @yielded_this_resume = false
    fiber = nil
    true
  end

  def main_error(e)
    message = readable_trace([e.message]).first
    script = @main_name || '089 - Main'
    Rune::Log.error("Main-Script '#{script}': #{e.class}: #{message}")
    Issues.report('Scripts', "Main-Script '#{script}': #{e.class}: #{message}", severity: 'error')
    @errors << { script: script, error: e }
    show_error_window
  end

  def section_name(index) = format('Section%03d', index)

  # "Section007:12:in ..." -> "Script '000 - Game_Temp':12:in ..."
  def readable_trace(lines)
    Array(lines).map do |line|
      line.to_s.sub(/\ASection(\d+)/) do
        entry = @startup[Regexp.last_match(1).to_i]
        entry ? "Script '#{entry[0]}'" : Regexp.last_match(0)
      end
    end
  end

  def startup_error(name, e)
    message = readable_trace([e.message]).first
    Rune::Log.error("Start-Script '#{name}': #{e.class}: #{message}")
    Issues.report('Scripts', "Start-Script '#{name}': #{e.class}: #{message}", severity: 'error')
    @errors << { script: name, error: e }
  end

  # Text für das Fehlerfenster: Script, Zeile, Klasse, Meldung und die
  # Backtrace-Zeilen, die in Scripts liegen (Engine-Interna ausgeblendet).
  # XP-Format: "Script 'Name' Zeile N: Klasse ist aufgetreten." + Meldung,
  # danach der Backtrace (nur Script-Frames, mit Scriptnamen statt Section).
  def error_report(entry)
    e = entry[:error]
    trace = readable_trace(e.backtrace)
    # SyntaxError hat oft keinen Script-Frame im Backtrace, aber "Section…:N:"
    # in der Meldung selbst.
    message = readable_trace([e.message]).first
    script_lines = trace.select { |l| l.start_with?("Script '") }
    quelle = script_lines.first || message[/\AScript '[^']*':\d+/].to_s
    zeile = quelle[/':(\d+)/, 1]
    text = +"Script '#{entry[:script]}'"
    text << " Zeile #{zeile}" if zeile
    text << ": #{e.class} ist aufgetreten.\n\n#{message}\n"
    shown = script_lines.empty? ? trace.first(8) : script_lines.first(12)
    text << "\nBacktrace:\n" << shown.map { |l| "  #{l}" }.join("\n") unless shown.empty?
    text
  end

  def show_error_window
    neu = @errors.reject { |entry| @reported.key?(error_report(entry)) }
    return if neu.empty?
    neu.each { |entry| @reported[error_report(entry)] = true }
    text = error_report(neu.first)
    text << "\n\nDie folgenden Scripts wurden nicht geladen. Das Spiel läuft weiter; " \
            "Script im Script-Editor (F11) korrigieren und mit F5 neu laden."
    if defined?(Rune::App) && Rune::App.respond_to?(:error_box)
      Rune::App.error_box('Rune3D - Script-Fehler', text)
    end
  end

  # Benanntes Script ausführen. interpreter/event dürfen nil sein (Tests).
  def run(name, interpreter = nil, event = nil)
    key = name.to_s
    code = @defs[key]
    unless code
      Rune::Log.warn("Script '#{key}' nicht gefunden")
      Issues.report('Scripts', "Script '#{key}' nicht gefunden")
      return false
    end
    Context.new(interpreter, event).instance_eval(code, "script:#{key}", 1)
    true
  rescue StandardError, ScriptError => e
    Rune::Log.error("Script '#{key}': #{e.class}: #{e.message}")
    Issues.report('Scripts', "Script '#{key}': #{e.class}: #{e.message}", severity: 'error')
    false
  end

  # Code direkt aus einem Event-Befehl ausführen.
  def run_code(name, code, interpreter = nil, event = nil)
    Context.new(interpreter, event).instance_eval(code.to_s, "event:#{name}", 1)
    true
  rescue StandardError, ScriptError => e
    Rune::Log.error("Script '#{name}': #{e.class}: #{e.message}")
    Issues.report('Scripts', "Inline-Script '#{name}': #{e.class}: #{e.message}", severity: 'error')
    false
  end

  # Etappe 20d: Ausdruck auswerten und SEINEN Wert liefern (für bedingte
  # Verzweigungen wie "Wenn Script"). Fehler werden geloggt und als nil
  # (also falsch) behandelt - eine kaputte Bedingung darf das Event nicht
  # aufhängen.
  def eval_code(name, code, interpreter = nil, event = nil)
    Context.new(interpreter, event).instance_eval(code.to_s, "event:#{name}", 1)
  rescue StandardError, ScriptError => e
    Rune::Log.error("Script '#{name}': #{e.class}: #{e.message}")
    Issues.report('Scripts', "Bedingungs-Script '#{name}': #{e.class}: #{e.message}",
                  severity: 'error')
    nil
  end

  # Kontext mit diesen Helfern. Das ist keine Sicherheits-Sandbox:
  # Ruby-Code kann auf Interpreter und Umgebung zugreifen.
  class Context
    attr_reader :event, :interpreter

    def initialize(interpreter = nil, event = nil)
      @interpreter = interpreter
      @event = event
    end

    def game = Game

    # Textfenster öffnen (blockiert wie ein text-Kommando).
    def say(*lines)
      return unless @interpreter
      @interpreter.say(lines.flatten.map(&:to_s))
    end

    def switch(name) = GameState.switch(name.to_s)
    def set_switch(name, value = true) = GameState.set_switch(name.to_s, !!value)
    def variable(name) = GameState.variable(name.to_s)
    def set_variable(name, value) = GameState.set_variable(name.to_s, value.to_f)
    def add_variable(name, delta) = set_variable(name, variable(name) + delta.to_f)

    # Erfahrung geben (D4, Runde 22): Held per id, XP-Stufenaufstieg inklusive.
    # Rückgabe: Anzahl Stufen.
    def gain_exp(id, amount)
      actor = Party.find(id.to_s)
      return 0 unless actor
      Party.gain_exp(actor, amount.to_i)
    end

    # Fertigkeiten (20a): gelernte Skills eines Helden als ID-Liste.
    def skills(id)
      actor = Party.find(id.to_s)
      actor ? Party.skills(actor) : []
    end

    # Fertigkeit lernen (z. B. nach einem Event). true = neu gelernt.
    def learn_skill(id, skill_id)
      actor = Party.find(id.to_s)
      return false unless actor
      Party.learn_skill(actor, skill_id.to_s)
    end

    def forget_skill(id, skill_id)
      actor = Party.find(id.to_s)
      return false unless actor
      Party.forget_skill(actor, skill_id.to_s)
    end

    def player_position = Rune::Player.position

    def teleport(x, z, yaw: Math::PI)
      Rune::Player.teleport(x.to_f, Game.gy(x.to_f, z.to_f) + 0.2, z.to_f)
      Game.reset_step_tracking if Game.respond_to?(:reset_step_tracking)
      Rune::Camera.yaw = yaw.to_f
    end

    def log(msg) = Rune::Log.info("[Script] #{msg}")
    def sound(name, volume = 1.0) = Rune::Sound.play(name.to_s, volume.to_f)

    # Etappe 17g: Party-/Inventar-Helfer für Projekt-Scripts.
    def party = Party
    def gold(amount = nil)
      return Party.gold if amount.nil?
      Party.add_gold(amount.to_i)
    end
    def items = Party.items.dup
    def item_count(name) = Party.item_count(name)
    def add_item(name, count = 1) = Party.add_item(name, count.to_i)
    def take_item(name, count = 1) = Party.take_item(name, count.to_i)
    # Ziel als zweites Argument oder target:; ohne Ziel wie bisher automatisch.
    def use_item(name, target_id = nil, target: target_id) = Party.use_item(name, target: target)
    def heal(amount = 0) = Party.heal_all(amount.to_i)

    # Etappe 17h: Wetter umstellen (kein/regen/schnee); ohne Argument lesen.
    def weather(name = nil, intensity = 1.0)
      return Weather.type if name.nil?
      Weather.set(name.to_s, intensity.to_f)
    end

    # Etappe 19e: Bildschirmblende und Self-Switches als Kurz-Helfer
    # (RGSS-Scripts nutzen alternativ Graphics.fadeout/$game_self_switches).
    def fade(direction = 'aus', seconds = 0.5)
      Fade.start(direction.to_s, seconds.to_f)
    end

    # Bewegungsroute starten (Etappe 19g): Ziel 'spieler' oder NPC-Name,
    # Schritte z.B. %w[up up warte zufall]. Läuft nebenher (Mover).
    def move_route(target, steps, repeat: false, speed: nil, code: nil)
      Mover.start(target, Array(steps), repeat: repeat, speed: speed, code: code)
    end

    # Kampf starten (Etappe 19i): battle('waldweg') wie der Event-Befehl.
    def battle(troop_id, can_escape: true, can_lose: false)
      return false unless defined?(Battle)

      Battle.start(troop_id, can_escape: can_escape, can_lose: can_lose)
    end

    # Self-Switch des Besitzer-Events: ohne Argument lesen, mit Argument
    # setzen. Ohne Event (nil) liefern beide nil - das Script läuft ins Leere.
    def self_switch(letter, value = nil)
      return nil unless @event.respond_to?(:self_switch_key)

      key = @event.self_switch_key
      return GameState.self_switch(key, letter) if value.nil?

      GameState.set_self_switch(key, letter, !!value)
    end

    # Partikel am Event (oder am Spieler, wenn kein Event bekannt ist).
    def effect(name = 'sparkle', height: 1.0, color: 0xFFD866)
      if @event.respond_to?(:x) && @event.respond_to?(:z)
        ox = @event.x.to_f
        oz = @event.z.to_f
      else
        px, _py, pz = Rune::Player.position
        ox = px
        oz = pz
      end
      oy = Game.gy(ox, oz) + height.to_f
      case name.to_s
      when 'sparkle' then Effects.sparkle(ox, oy, oz)
      when 'dust'    then Effects.dust(ox, oy, oz)
      else Effects.burst(ox, oy, oz, color: color)
      end
    end
  end
end
