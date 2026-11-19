#encoding: utf-8
# ============================================================================
#  RGSS-Kompatibilitätsschicht (Etappe 19e).
#
#  Vorbild ist das Ruby Game Scripting System von RPG Maker XP (RGSS1):
#  Spieler sollen die Engine später wie dort mit eigenen Ruby-Scripts
#  anpassen können (Script-Editor, F11). Diese Schicht legt die bekannten
#  Namen über die Rune3D-Engine:
#
#    $game_system, $game_switches, $game_variables, $game_self_switches,
#    $game_party, $game_player, $game_map, $game_screen
#    Graphics (fadeout/fadein/brightness/freeze/transition)
#    Input    (trigger?/press?/repeat?/dir4, Konstanten A..F9 wie RGSS1)
#    Audio    (bgm_play/bgm_stop/se_play/me_play/bgs_*)
#    load_data / save_data
#
#  Bewusste Abweichungen (3D-Engine, JSON statt rxdata):
#    - Daten sind JSON: load_data/save_data schreiben/lesen JSON, nicht
#      Marshal; Dateinamen bleiben frei wählbar.
#    - Schalter/Variablen/Items/Events haben NAMEN (Strings) statt fester
#      Integer-IDs. Integer-Argumente werden per to_s übernommen - Scripts
#      können beide Formen nutzen, $game_self_switches versteht zusätzlich
#      das XP-Schlüsselformat [karte, event_id, buchstabe].
#    - Graphics.freeze/transition sind Zustandsmerker ohne Bild-Effekt:
#      Den Frame-Takt gibt die Engine vor (Rune.tick), nicht das Script.
#    - Audio-Pitch wird ignoriert (prozedurale Sounds kennen keine Tonhöhe),
#      Lautstärke ist wie bei RGSS 0..100.
#
#  Etappe 19h (RGSS-Vertiefung) ergänzt:
#    - $game_temp (message_text, common_event_id, player_passable? ...)
#    - $data_actors/$data_items/$data_weapons/$data_armors/$data_system aus
#      database.json/system.json (Hashes mit String-Schlüsseln statt XP-
#      Integer-Indizes; $data_weapons/$data_armors sind leer - Datenbank
#      kennt noch keine Waffen/Rüstungen, Backlog).
#    - $scene + Graphics.scene_update (Scripts dürfen eigene Szenen stellen;
#      die Engine-Schleife ersetzt das NICHT - Scene#main läuft pro Frame).
#    - Sprite/Window/Viewport/Bitmap in game/engine/graphics.rb.
#    - $game_player.passable? echt: Kartengrenzen + Geländestufe (Objekt-
#      Kollision bleibt Backlog, es gibt keine blocked?-Abfrage).
#
#  Initialisierung: RGSS.setup wird aus Game.setup aufgerufen (und bei F5).
#  Vor dem ersten setup sind die $game_*-Globalen nil; Engine-Code prüft das.
#
#  Kopflos testbar: ruby -I game/engine tools/rgss_test.rb
# ============================================================================

require 'json'

module RGSS
  VERSION = 'RGSS1-kompatibel (Rune3D 0.8)'.freeze

  # ------------------------------------------------------------ $game_system

  # System-Schalter, die die Engine beachtet: Menü (Esc) und Speichern lassen
  # sich aus Scripts heraus sperren, wie bei XP.
  class GameSystem
    attr_accessor :menu_disabled, :save_disabled, :encounter_disabled

    def initialize
      @menu_disabled     = false
      @save_disabled     = false
      @encounter_disabled = false   # Platzhalter: Kampfsystem kommt später
    end

    def rgss_version = VERSION
  end

  # ------------------------------------------------------------ $game_temp

  # Flüchtige Systemwerte (Etappe 19h) - in XP pro Szene neu, hier pro
  # RGSS.reset! (Kartenwechsel/F5). message_text/message_proc sind Ablage
  # für eigene Dialog-Scripts; player_passable? steuert, ob passable?
  # überhaupt prüft (XP: im Debug-Modus false).
  class GameTemp
    attr_accessor :message_text, :message_proc, :common_event_id
    attr_accessor :player_passable, :transition_processing
    attr_accessor :map_transition_name, :battle_transition_name

    def initialize
      @message_text = nil
      @message_proc = nil
      @common_event_id = 0
      @player_passable = true
      @transition_processing = false
      @map_transition_name = ''
      @battle_transition_name = ''
    end

    def player_passable? = @player_passable
  end

  # ---------------------------------------------------------- $game_switches

  # Globale Schalter. RGSS nutzt Integer-IDs, Rune3D Namen - beides geht
  # (Integer werden zu "5", Namen bleiben "auftrag").
  class GameSwitches
    def [](name) = GameState.switch(name.to_s)
    def []=(name, value)
      GameState.set_switch(name.to_s, value)
      value
    end
  end

  # --------------------------------------------------------- $game_variables

  class GameVariables
    def [](name) = GameState.variable(name.to_s)
    def []=(name, value)
      GameState.set_variable(name.to_s, value.to_f)
      value
    end
  end

  # ---------------------------------------------------- $game_self_switches

  # Self-Switches (19d). Schlüssel wie XP: [karte, event, buchstabe] - das
  # Event darf die ID (Integer, wird über Game.events aufgelöst) oder der
  # Name sein. Alternativ der interne String-Schlüssel "karte|event|buchstabe".
  class GameSelfSwitches
    def [](key)
      k, letter = resolve(key)
      return false if k.nil?
      GameState.self_switch(k, letter)
    end

    def []=(key, value)
      k, letter = resolve(key)
      if k.nil?
        Rune::Log.warn("RGSS: Self-Switch-Schlüssel #{key.inspect} nicht aufgelöst")
        return value
      end
      GameState.set_self_switch(k, letter, value)
      value
    end

    private

    # Hinweis: Der Karten-Eintrag des XP-Schlüssels wird ignoriert -
    # Self-Switches lassen sich nur für die AKTUELLE Karte setzen (Game.map_id).
    def resolve(key)
      case key
      when Array
        return nil unless key.size == 3
        _karte, event, letter = key
        name = event.is_a?(String) ? event : name_fuer_id(event)
        return nil if name.nil? || name.to_s.empty?
        ["#{Game.map_id}|#{name}", letter.to_s]
      when String
        teile = key.split('|')
        teile.size == 3 ? ["#{teile[0]}|#{teile[1]}", teile[2]] : nil
      end
    end

    def name_fuer_id(id)
      liste = Game.respond_to?(:events) ? Game.events : []
      ev = liste.find { |e| e.respond_to?(:id) && e.id.to_i == id.to_i }
      ev&.name
    end
  end

  # ----------------------------------------------------------- $game_party

  class GameParty
    def gold = Party.gold
    def gain_gold(amount) = Party.add_gold(amount.to_i)
    def lose_gold(amount) = Party.add_gold(-amount.to_i)

    def items = Party.items
    def item_count(name) = Party.item_count(name.to_s)
    def gain_item(name, count = 1) = Party.add_item(name.to_s, count.to_i)
    def lose_item(name, count = 1) = Party.take_item(name.to_s, count.to_i)
    def use_item(name) = Party.use_item(name.to_s)

    # Rune3D-Zusatz (kein RGSS): ganze Party heilen, wie der heal-Befehl.
    def heal_all(amount = 0) = Party.heal_all(amount.to_i)
  end

  # ---------------------------------------------------------- $game_player

  # Der Spieler in 3D: x/z sind Weltkoordinaten, y die Höhe. moveto setzt
  # ihn auf den Boden (Game.gy) wie der teleport-Befehl.
  class GamePlayer
    def position = Rune::Player.position
    def x = Rune::Player.position[0]
    def y = Rune::Player.position[1]
    def z = Rune::Player.position[2]

    def moveto(x, z, yaw: Math::PI)
      Rune::Player.teleport(x.to_f, Game.gy(x.to_f, z.to_f) + 0.2, z.to_f)
      Rune::Camera.yaw = yaw.to_f
    end

    # Passierbar (Etappe 19h, echt soweit möglich): innerhalb der Karten-
    # grenzen und keine zu hohe Geländestufe. Richtungen wie XP: 2=unten
    # (+z), 4=links (-x), 6=rechts (+x), 8=oben (-z) - ohne Richtung wird
    # die Zielstelle selbst geprüft. Objekt-Kollision fehlt (Backlog: es
    # gibt keine blocked?-Abfrage für Hindernisse).
    MAX_STUFE = 1.2   # Meter Höhenunterschied, die ein Schritt überwindet

    def passable?(x, z, direction = nil)
      return true unless $game_temp && $game_temp.player_passable?

      dx, dz = case direction.to_i
               when 2 then [0.0, 1.0]
               when 4 then [-1.0, 0.0]
               when 6 then [1.0, 0.0]
               when 8 then [0.0, -1.0]
               else [0.0, 0.0]
               end
      tx = x.to_f + dx
      tz = z.to_f + dz

      # Ohne Karte (RGSS.setup lief noch nicht) entfällt die Grenzprüfung -
      # die Geländestufe wird trotzdem geprüft.
      if $game_map
        w = $game_map.width
        h = $game_map.height
        return false if (w > 0 && (tx < 0 || tx > w)) || (h > 0 && (tz < 0 || tz > h))
      end

      (Game.gy(tx, tz) - Game.gy(x.to_f, z.to_f)).abs <= MAX_STUFE
    end

    def refresh; end
  end

  # ------------------------------------------------------------- $game_map

  class GameMap
    def map_id = Game.map_id
    def display_name = (Game.map || {})['name']

    def events = Game.respond_to?(:events) ? Game.events : []
    def npcs   = Game.respond_to?(:npcs) ? Game.npcs : []

    # Event per ID oder Name (RGSS nutzt IDs; Rune3D erlaubt beides).
    # Achtung: Event-IDs sind optional (Default 0) - deshalb NICHT per
    # to_i über einen Strang vergleichen, sonst trifft die Namenssuche
    # ('Wache'.to_i == 0) jedes Event mit Default-ID.
    def event(id_or_name)
      if id_or_name.is_a?(Integer)
        events.find { |e| e.id.to_i == id_or_name }
      else
        name = id_or_name.to_s
        events.find { |e| e.name == name } ||
          events.find { |e| e.id.to_i == name.to_i && !name.empty? }
      end
    end

    def width
      terrain = (Game.map || {})['terrain'] || {}
      (terrain['width'] || terrain['size'] || 0).to_f
    end

    def height
      terrain = (Game.map || {})['terrain'] || {}
      (terrain['height'] || terrain['size'] || 0).to_f
    end
  end

  # ---------------------------------------------------------- $game_screen

  # Wetter wie bei XP über $game_screen. RGSS-Zahlen (0=kein, 1=Regen,
  # 2=Schnee, 3=Sturm) und Rune3D-Namen (kein/regen/schnee, auch englisch)
  # werden beide verstanden; Sturm gibt es noch nicht -> dichter Schnee.
  class GameScreen
    def weather(type = 'kein', power = 1.0)
      name = case type
             when 0, nil, 'kein', 'none'    then 'kein'
             when 1, 'regen', 'rain'        then 'regen'
             when 2, 'schnee', 'snow'       then 'schnee'
             when 3, 'sturm', 'storm'       then 'schnee'
             else type.to_s
             end
      Weather.set(name, power.to_f)
    end

    def weather_type = Weather.type
    def weather_intensity = Weather.intensity
  end

  # ------------------------------------------------------------- $data_*

  # Datenbank als RGSS-Globale (Etappe 19h). XP nutzt Integer-Indizes in
  # Arrays; Rune3D hat String-IDs - deshalb Hashes mit 'id'/'name'-Schlüsseln.
  def self.load_data_globals
    # ::Database - sonst sucht Ruby die Konstante im RGSS-Modul (NameError).
    db = ::Database.database
    $data_actors = liste_als_hash(db['actors'])
    $data_items  = liste_als_hash(db['items'])
    $data_weapons = {}   # Datenbank kennt noch keine Waffen (Backlog)
    $data_armors = {}    # ... und keine Rüstungen (Backlog)
    $data_system = ::Database.system
  end

  def self.liste_als_hash(liste)
    (liste || []).each_with_object({}) do |h, ziel|
      next unless h.is_a?(Hash)
      id = h['id'].to_s
      ziel[id] = h unless id.empty?
    end
  end
  private_class_method :liste_als_hash

  # Alle $game_*-Globalen (neu) besetzen. Läuft aus Game.setup (Start, F5,
  # Playtest) - die Wrapper delegieren live, ein Neuaufbau der Karte
  # (enter_map) braucht also kein erneutes setup.
  def self.setup
    $game_temp          = GameTemp.new
    $game_system        = GameSystem.new
    $game_switches      = GameSwitches.new
    $game_variables     = GameVariables.new
    $game_self_switches = GameSelfSwitches.new
    $game_party         = GameParty.new
    $game_player        = GamePlayer.new
    $game_map           = GameMap.new
    $game_screen        = GameScreen.new
    $scene              = nil   # eigene Szenen setzen Scripts selbst
    load_data_globals
    Rune::Log.info("RGSS-Schicht bereit (#{VERSION})")
  end

  # Flüchtige RGSS-Dinge verwerfen (Kartenwechsel, F5, Playtest-Ende):
  # Sprites/Fenster/$scene und $game_temp sind nicht speicherbar.
  def self.reset!
    Graphics.scene_reset! if defined?(Graphics) && Graphics.respond_to?(:scene_reset!)
    $game_temp = GameTemp.new
  end
end

# ============================================================================
#  Graphics (RGSS1): Fade-Kommandos auf der Rune3D-Bildschirmblende (19d).
#  Frame-Zeiten rechnet die Schicht mit Graphics.frame_rate in Sekunden um.
# ============================================================================
module Graphics
  @frame_rate  = 40   # RGSS1-Default
  @frame_count = 0
  @frozen      = false

  class << self
    attr_accessor :frame_rate
    attr_reader :frame_count
  end

  # Frame-Takt: die Engine (Rune.tick) ruft update/draw selbst - wer den
  # RGSS-Zähler führen will (Scripts, die frame_count lesen), ruft das hier.
  def self.update
    @frame_count += 1
  end

  # Helligkeit 0..255 (RGSS). 255 = Bild sichtbar, 0 = schwarz.
  def self.brightness
    ((1.0 - Fade.alpha) * 255).round
  end

  def self.brightness=(value)
    v = value.to_f.clamp(0, 255)
    Fade.alpha = 1.0 - v / 255.0
  end

  def self.fadeout(frames)
    Fade.start('aus', frames.to_f / @frame_rate.to_f)
  end

  def self.fadein(frames)
    Fade.start('ein', frames.to_f / @frame_rate.to_f)
  end

  # freeze/transition: Zustand merken, kein Bild-Effekt (siehe Kopfdoku).
  def self.freeze
    @frozen = true
  end

  def self.frozen?
    @frozen
  end

  def self.transition(frames = 8, _varchar = '00', _vague = 20)
    @frozen = false
    frames
  end
end

# ============================================================================
#  Input (RGSS1): Konstanten und trigger?/press?/repeat?/dir4 auf Rune::Input.
#  Die Engine pollt die Tasten selbst - Input.update ist ein No-Op.
# ============================================================================
module Input
  DOWN  = 2
  LEFT  = 4
  RIGHT = 6
  UP    = 8
  A     = 11
  B     = 12
  C     = 13
  X     = 14
  Y     = 15
  Z     = 16
  L     = 17
  R     = 18
  SHIFT = 21
  CTRL  = 22
  ALT   = 23
  F5    = 25
  F6    = 26
  F7    = 27
  F8    = 28
  F9    = 29

  # RGSS-Konstante -> Rune3D-Tastensymbole. C (Bestätigen) liegt auf den
  # Tasten, die im Spiel ohnehin "benutzen/weiter" sind (E/Space/Enter),
  # B (Abbrechen) auf Esc wie im Menü.
  TASTEN = {
    DOWN => %i[down], LEFT => %i[left], RIGHT => %i[right], UP => %i[up],
    A => %i[shift], B => %i[escape], C => %i[space enter e],
    X => %i[x], Y => %i[y], Z => %i[z], L => %i[q], R => %i[r],
    SHIFT => %i[shift], CTRL => %i[ctrl], ALT => %i[alt],
    F5 => %i[f5], F6 => %i[f6], F7 => %i[f7], F8 => %i[f8], F9 => %i[f9],
  }.freeze

  def self.update; end   # Engine-Polling; hier nichts zu tun

  # Ein Frame gedrückt? (Symbol-Argumente gehen direkt, z.B. :e)
  def self.trigger?(button)
    tasten(button).any? { |k| Rune::Input.pressed?(k) }
  end

  # Gehalten?
  def self.press?(button)
    tasten(button).any? { |k| Rune::Input.down?(k) }
  end

  # Auto-Repeat gibt es in der Engine nicht - wie trigger? (dokumentiert).
  def self.repeat?(button)
    trigger?(button)
  end

  # Richtung als RGSS-Zahl: 2=unten, 4=links, 6=rechts, 8=oben, 0=keine.
  def self.dir4
    return 2 if Rune::Input.down?(:down)
    return 4 if Rune::Input.down?(:left)
    return 6 if Rune::Input.down?(:right)
    return 8 if Rune::Input.down?(:up)

    0
  end

  def self.tasten(button)
    return [button] if button.is_a?(Symbol)

    TASTEN[button] || []
  end
  private_class_method :tasten
end

# ============================================================================
#  Audio (RGSS1): bgm/bgs = Schleifen (Rune::Sound.loop), se/me = Einmal-
#  Sounds (Rune::Sound.play). Lautstärke 0..100 wie RGSS; Pitch wird
#  ignoriert. Dateinamen dürfen Pfade/Endungen tragen ("Audio/BGM/x.ogg").
# ============================================================================
module Audio
  @bgm = nil
  @bgs = nil

  def self.bgm_play(filename, volume = 100, _pitch = 100)
    @bgm = basisname(filename)
    Rune::Sound.loop(@bgm, lautstaerke(volume))
  end

  def self.bgm_stop
    Rune::Sound.stop(@bgm) if @bgm
    @bgm = nil
  end

  # fading gibt es in der Sound-Schicht nicht -> sofort stoppen (Doku).
  def self.bgm_fade(_time)
    bgm_stop
  end

  def self.bgs_play(filename, volume = 100, _pitch = 100)
    @bgs = basisname(filename)
    Rune::Sound.loop(@bgs, lautstaerke(volume))
  end

  def self.bgs_stop
    Rune::Sound.stop(@bgs) if @bgs
    @bgs = nil
  end

  def self.bgs_fade(_time)
    bgs_stop
  end

  def self.me_play(filename, volume = 100, _pitch = 100)
    Rune::Sound.play(basisname(filename), lautstaerke(volume))
  end

  def self.se_play(filename, volume = 100, _pitch = 100)
    Rune::Sound.play(basisname(filename), lautstaerke(volume))
  end

  # Einmal-Sounds lassen sich nicht einzeln beenden; se_stop ist ein No-Op.
  def self.se_stop; end

  def self.basisname(filename)
    File.basename(filename.to_s).sub(/\.[^.]*/, '')
  end
  private_class_method :basisname

  def self.lautstaerke(volume)
    (volume.to_f / 100.0).clamp(0.0, 1.0)
  end
  private_class_method :lautstaerke
end

# ============================================================================
#  load_data / save_data (RGSS1) - hier JSON-basiert (siehe Kopfdoku).
# ============================================================================

def save_data(obj, filename)
  File.write(filename.to_s, JSON.pretty_generate(obj) + "\n", encoding: 'UTF-8')
  true
end

def load_data(filename)
  return nil unless File.exist?(filename.to_s)

  JSON.parse(File.read(filename.to_s, encoding: 'UTF-8'))
rescue StandardError => e
  Rune::Log.error("load_data('#{filename}'): #{e.message}")
  nil
end
