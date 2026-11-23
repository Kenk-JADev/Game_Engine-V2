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
require 'passage'   # Etappe 19k.3: Block-Zonen + Hindernisse (eine Quelle)

module RGSS
  VERSION = 'RGSS1-kompatibel (Rune3D 0.8)'.freeze

  # ------------------------------------------------------------ $game_system

  # System-Schalter, die die Engine beachtet: Menü (Esc) und Speichern lassen
  # sich aus Scripts heraus sperren, wie bei XP.
  # Nach XP-Defaultscript/001 - Game_System.rb: Timer, Audio-Delegation
  # (Rune3D-Audio-Objekte sind Hashes {'name','volume','pitch'} statt
  # RPG::BGM), Message-Optionen und Save-Zähler. update(dt) tickt den Timer
  # und wird von Game.update gerufen.
  class GameSystem
    attr_accessor :menu_disabled, :save_disabled, :encounter_disabled
    attr_accessor :timer, :timer_working
    attr_accessor :message_position, :message_frame, :save_count
    attr_accessor :windowskin_name, :battle_bgm, :battle_end_me
    attr_accessor :menu_bgm

    def initialize
      @menu_disabled      = false
      @save_disabled      = false
      @encounter_disabled = false
      @timer              = 0
      @timer_working      = false
      @message_position   = 2     # XP-Default: unten
      @message_frame      = 1     # XP-Default: Rahmen an
      @save_count         = 0
      @windowskin_name    = 'Default'
      @battle_bgm         = { 'name' => '', 'volume' => 100, 'pitch' => 100 }
      @battle_end_me      = { 'name' => '', 'volume' => 100, 'pitch' => 100 }
      @menu_bgm           = { 'name' => '', 'volume' => 100, 'pitch' => 100 }
    end

    # Timer in Frames (XP zählt 60er-Frames); Rune3D tickt in Sekunden.
    def update(dt)
      return unless @timer_working

      @timer -= dt.to_f * Graphics.frame_rate
      if @timer <= 0
        @timer = 0
        @timer_working = false
      end
    end

    def timer_start(seconds)
      @timer = seconds.to_f * Graphics.frame_rate
      @timer_working = true
    end

    def timer_seconds
      @timer / Graphics.frame_rate.to_f
    end

    # Audio-Delegation: Argument ist XP-gewohnt Hash/RPG::AudioFile-ähnlich
    # oder ein nackter Name.
    def bgm_play(audio) = Audio.bgm_play(*audio_args(audio))
    def bgm_stop        = Audio.bgm_stop
    def bgm_fade(time)  = Audio.bgm_fade(time)
    def bgs_play(audio) = Audio.bgs_play(*audio_args(audio))
    def bgs_stop        = Audio.bgs_stop
    def bgs_fade(time)  = Audio.bgs_fade(time)
    def me_play(audio)  = Audio.me_play(*audio_args(audio))
    def se_play(audio)  = Audio.se_play(*audio_args(audio))
    def se_stop         = Audio.se_stop

    def rgss_version = VERSION

    private

    def audio_args(audio)
      return [audio] if audio.is_a?(String) || audio.is_a?(Symbol)
      h = audio.respond_to?(:to_h) ? audio.to_h : audio
      h = h.transform_keys(&:to_s) if h.is_a?(Hash)
      [h['name'].to_s, h['volume'] || 100, h['pitch'] || 100]
    end
  end

  # ------------------------------------------------------------ $game_temp

  # Flüchtige Systemwerte (Etappe 19h) - in XP pro Szene neu, hier pro
  # RGSS.reset! (Kartenwechsel/F5). message_text/message_proc sind Ablage
  # für eigene Dialog-Scripts; player_passable? steuert, ob passable?
  # überhaupt prüft (XP: im Debug-Modus false).
  # Feldliste 1:1 aus XP-Defaultscript/000 - Game_Temp.rb (Defaultwerte
  # inklusive). Rune3D-Erweiterung: passable (XP hat das nicht - dort ist
  # Passierbarkeit reine Kartensache; hier schaltet es die Geländeprüfung
  # von $game_player.passable? ab, Default true).
  class GameTemp
    attr_accessor :map_bgm, :message_text, :message_proc
    attr_accessor :choice_start, :choice_max, :choice_cancel_type, :choice_proc
    attr_accessor :num_input_start, :num_input_variable_id, :num_input_digits_max
    attr_accessor :message_window_showing, :common_event_id
    attr_accessor :in_battle, :battle_calling, :battle_troop_id
    attr_accessor :battle_can_escape, :battle_can_lose, :battle_proc
    attr_accessor :battle_turn, :battle_event_flags, :battle_abort
    attr_accessor :battle_main_phase, :battleback_name, :forcing_battler
    attr_accessor :shop_calling, :shop_id
    attr_accessor :name_calling, :name_actor_id, :name_max_char
    attr_accessor :menu_calling, :menu_beep, :save_calling, :debug_calling
    attr_accessor :player_transferring, :player_new_map_id
    attr_accessor :player_new_x, :player_new_y, :player_new_direction
    attr_accessor :transition_processing, :transition_name
    attr_accessor :gameover, :to_title, :last_file_index
    attr_accessor :debug_top_row, :debug_index
    attr_accessor :passable   # Rune3D-Erweiterung (siehe oben)

    def initialize
      @map_bgm = nil
      @message_text = nil
      @message_proc = nil
      @choice_start = 99
      @choice_max = 0
      @choice_cancel_type = 0
      @choice_proc = nil
      @num_input_start = 99
      @num_input_variable_id = 0
      @num_input_digits_max = 0
      @message_window_showing = false
      @common_event_id = 0
      @in_battle = false
      @battle_calling = false
      @battle_troop_id = 0
      @battle_can_escape = false
      @battle_can_lose = false
      @battle_proc = nil
      @battle_turn = 0
      @battle_event_flags = {}
      @battle_abort = false
      @battle_main_phase = false
      @battleback_name = ''
      @forcing_battler = nil
      @shop_calling = false
      @shop_id = 0
      @name_calling = false
      @name_actor_id = 0
      @name_max_char = 0
      @menu_calling = false
      @menu_beep = false
      @save_calling = false
      @debug_calling = false
      @player_transferring = false
      @player_new_map_id = 0
      @player_new_x = 0
      @player_new_y = 0
      @player_new_direction = 0
      @transition_processing = false
      @transition_name = ''
      @gameover = false
      @to_title = false
      @last_file_index = 0
      @debug_top_row = 0
      @debug_index = 0
      @passable = true
    end

    def player_passable? = @passable
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
      Game.reset_step_tracking if Game.respond_to?(:reset_step_tracking)
      Rune::Camera.yaw = yaw.to_f
    end

    # XP Game_Character: Blickrichtung 2/4/6/8, Schrittzähler und
    # Encounter-Zähler (022 - Game_Player.rb). increase_steps wird von der
    # Schritt-Sound-Logik in Game.update gerufen.
    def steps
      @steps ||= 0
    end

    def encounter_count
      @encounter_count ||= 0
    end

    def direction
      @direction ||= 2
    end

    def direction=(value)
      @direction = [2, 4, 6, 8].include?(value.to_i) ? value.to_i : 2
    end

    def increase_steps
      @steps = (@steps || 0) + 1
    end

    # XP Game_Player#make_encounter_count (022): rand(n) + rand(n) + 1 mit
    # n = encounter_step der Karte (Default 30) - im Mittel n Schritte bis
    # zum nächsten Zufallskampf. Wird beim Kartenbetreten und beim
    # Kampfstart neu gewürfelt (Game.enter_map / Game.update).
    def make_encounter_count
      n = ($game_map && $game_map.encounter_step) || 30
      n = 30 if n <= 0
      @encounter_count = rand(n) + rand(n) + 1
    end

    # XP: nur herunterzählen, nie unter 0 - das Neu-Würfeln übernimmt
    # make_encounter_count, wenn der Kampf wirklich startet (Game.update).
    def decrease_encounter_count
      return if @encounter_count.nil? || @encounter_count <= 0

      @encounter_count -= 1
    end

    # Passierbar (Etappe 19h, seit 19k.3 vollständig): innerhalb der Karten-
    # grenzen, keine zu hohe Geländestufe, keine Block-Zone und kein
    # Hindernis (Objekt-Kasten über der Steiggrenze). Richtungen wie XP:
    # 2=unten (+z), 4=links (-x), 6=rechts (+x), 8=oben (-z) - ohne
    # Richtung wird die Zielstelle selbst geprüft.
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
        # Etappe 19k.2/19k.3: Block-Zonen der Karte und Objekt-Hindernisse
        # (Passage ist dieselbe Quelle, die auch NPCs und Routen fragen).
        return false if Passage.blockiert?(tx, tz)
      end

      # Die Stufe misst am SPIELER: (x, z) ist der Ausgangspunkt des Schritts
      # (XP: x/y des Events), aber ob das Ziel erreichbar ist, entscheidet der
      # Höhenunterschied zur aktuellen Spielerposition - der übergebene Punkt
      # kann anywhere sein (Scripts prüfen oft voraus).
      px, _py, pz = Rune::Player.position
      (Game.gy(tx, tz) - Game.gy(px, pz)).abs <= MAX_STUFE
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

    # XP Game_Map#valid?: liegt (x, z) innerhalb der Karte?
    def valid?(x, z)
      w = width
      h = height
      return false if (w > 0 && (x.to_f < 0 || x.to_f > w)) ||
                      (h > 0 && (z.to_f < 0 || z.to_f > h))

      true
    end

    # Etappe 19k.2: Block-Zonen der Karte ("blocked": [{x,z,w,h}] mit
    # x/z = Mitte in Weltmetern, w/h = Größe). Die Physik blockt sie über
    # alle Höhen (unsichtbare Wände, kein Überspringen); passable? und
    # eigene Scripts fragen blocked? ab. Seit 19k.3 liegt die Rechnung in
    # Passage (eine Quelle für Spieler, NPCs und Bewegungs-Routen).
    def blocked_zones = Passage.zonen

    def blocked?(x, z) = Passage.zone?(x, z)

    # Etappe 19k.3: zusätzlich die Objekt-Hindernisse (nur mit Engine).
    def hindernis?(x, z) = Passage.hindernis?(x, z)

    # Etappe 19l: Zufallskämpfe der Karte (XP Game_Map#encounter_list/
    # #encounter_step, 016). Karten-JSON:
    #   "encounters": { "troops": ["waldweg", "banditen"], "steps": 30 }
    # troops: nicht-leere Truppen-IDs aus database.json (Zufallswahl);
    # steps:  positive ganze mittlere Schrittzahl (XP-Default 30).
    # Ungültige IDs werden übersprungen; ungültige steps ergeben 30.
    def encounter_list
      enc = (Game.map || {})['encounters']
      return [] unless enc.is_a?(Hash)

      troops = enc['troops']
      return [] unless troops.is_a?(Array)

      troops.select { |t| t.is_a?(String) && !t.strip.empty? }
    end

    def encounter_step
      enc = (Game.map || {})['encounters']
      return 30 unless enc.is_a?(Hash)

      n = enc['steps']
      return n if n.is_a?(Integer) && n.positive?
      return n.to_i if n.is_a?(Float) && n.finite? && n >= 1 && n == n.floor

      30
    end
  end

  # ---------------------------------------------------------- $game_screen

  # Wetter wie bei XP über $game_screen. RGSS-Zahlen (0=kein, 1=Regen,
  # 2=Schnee, 3=Sturm) und Rune3D-Namen (kein/regen/schnee, auch englisch)
  # werden beide verstanden; Sturm gibt es noch nicht -> dichter Schnee.
  class GameScreen
    # XP-Zustände (005 - Game_Screen.rb): tone/flash/shake werden als Werte
    # geführt, aber (noch) nicht gemalt - Rune3D hat keinen Tonwert-/Shake-
    # Renderer (Backlog). pictures: Platzhalter-Hash, Bild-Objekte fehlen.
    attr_reader :tone, :flash_color, :shake, :pictures, :weather_max

    def initialize
      @tone = [0.0, 0.0, 0.0, 0.0]
      @flash_color = [0.0, 0.0, 0.0, 0.0]
      @shake_power = 0
      @shake_speed = 0
      @shake_duration = 0
      @shake = 0
      @pictures = {}
      @weather_type = 0
      @weather_max = 0.0
    end

    def start_tone_change(tone, duration = 0)
      @tone = Array(tone).map(&:to_f)
      @tone_duration = duration.to_i   # Verlauf: Backlog (Wert gilt sofort)
    end

    def start_flash(color, duration)
      @flash_color = Array(color).map(&:to_f)
      @flash_duration = duration.to_i
    end

    def start_shake(power, speed, duration)
      @shake_power = power.to_i
      @shake_speed = speed.to_i
      @shake_duration = duration.to_i
    end

    def weather(type = 'kein', power = 1.0)
      name = case type
             when 0, nil, 'kein', 'none'    then 'kein'
             when 1, 'regen', 'rain'        then 'regen'
             when 2, 'schnee', 'snow'       then 'schnee'
             when 3, 'sturm', 'storm'       then 'schnee'
             else type.to_s
             end
      Weather.set(name, power.to_f)
      @weather_type = type.is_a?(Integer) ? type : 0
      @weather_max = power.to_f
    end

    # XP liefert die RGSS-Zahl (0/1/2/3) - Rune3D-Namen werden zurückgemappt.
    def weather_type
      case Weather.type
      when 'regen'  then 1
      when 'schnee' then 2
      else 0
      end
    end

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

  # Alle $game_*-Globalen bei Game.setup (Start/F5) neu besetzen. Die
  # Wrapper delegieren live; enter_map braucht kein erneutes setup.
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

  # Flüchtige RGSS-Dinge verwerfen (Kartenwechsel, F5):
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
