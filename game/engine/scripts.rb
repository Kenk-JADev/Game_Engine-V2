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
#  sound, effect, log, fade, self_switch) - kein Zugriff auf den Interpreter-
#  Stack, damit Scripts nicht mitten in der Abarbeitung Listen verändern.
#
#  Seit Etappe 19e steht Project-Scripts zusätzlich die RGSS-Schicht offen
#  (game/engine/rgss.rb): $game_switches/$game_party/$game_player/...,
#  Graphics.fadeout, Input.trigger?, Audio.se_play - die RPG-Maker-XP-API
#  für eigenes Custom-Ruby im Script-Editor (F11).
#
#  Kopflos testbar: ruby -I game/engine tools/script_test.rb
# ============================================================================

require 'json'

module Scripts
  @defs = {}

  module_function

  # scripts.json des aktiven Projekts laden (fehlende Datei ist kein Fehler).
  def load_project
    @defs = {}
    path = File.join(Database.root, 'scripts.json')
    return true unless File.exist?(path)

    data = JSON.parse(File.read(path, encoding: 'UTF-8'))
    list = data.is_a?(Array) ? data : (data['scripts'] || [])
    list.each do |h|
      next unless h.is_a?(Hash)
      name = h['name'].to_s
      next if name.empty?
      @defs[name] = h['code'].to_s
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

  # Sandkasten für Script-Code: nur diese Helfer, kein Interpreter-Interna.
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

    def player_position = Rune::Player.position

    def teleport(x, z, yaw: Math::PI)
      Rune::Player.teleport(x.to_f, Game.gy(x.to_f, z.to_f) + 0.2, z.to_f)
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
    def use_item(name) = Party.use_item(name)
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
