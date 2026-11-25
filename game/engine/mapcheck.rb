#encoding: utf-8
# ============================================================================
#  MapCheck — Karten-Daten prüfen (Etappe 19a).
#
#  Der RuneEditor scannt die geladene Karte seit Etappe 18h in C++
#  (issues_scan_map). Dieselben Regeln müssen auch ohne Editor verfügbar
#  sein: für das Spiel (Datenfehler melden statt sie still zu schlucken),
#  für die kopflosen Tests und für CI. Deshalb liegt die Prüfung hier als
#  reines Ruby-Modul — ohne Rune::*, ohne Fenster, ohne Grafik.
#
#  Aufrufe:
#    MapCheck.scan(karte)                       -> [Finding]
#    MapCheck.scan_file('.../dorf.json')        -> [Finding]
#    MapCheck.scan_tree('projekte/dorf/data')   -> { pfad => [Finding] }
#    MapCheck.report!(fundliste)                # -> Issues.report, falls geladen
#    MapCheck.summary(fundliste)                # -> "2 Fehler, 1 Warnung"
#
#  Kommandozeile (Prüft alle Karten des Repos, Exit 1 bei Fehlern):
#    ruby -I game/engine game/engine/mapcheck.rb
#    ruby -I game/engine game/engine/mapcheck.rb projekte/dorf/data/maps
#
#  Kopflos testbar: ruby -I game/engine tools/mapcheck_test.rb
# ============================================================================
require 'json'

module MapCheck
  # Schweregrade wie im Editor (issue_report): 1 = Warnung, 2 = Fehler.
  WARNUNG = 1
  FEHLER  = 2

  # Muss zu src/world/terrain_limits.h passen (tools/sync_test.rb).
  TERRAIN_MIN_SIZE = 1.0
  TERRAIN_MAX_SIZE = 10_000.0
  TERRAIN_MIN_RES = 8
  TERRAIN_MAX_RES = 1024
  TERRAIN_MAX_QUADS = 262_144
  TERRAIN_MAX_AMPLITUDE = 10_000.0
  TERRAIN_MAX_COORD = 1_000_000.0
  TERRAIN_MIN_RADIUS = 0.001
  TERRAIN_MAX_BRUSHES = 2048
  TERRAIN_MAX_BRUSH_SAMPLES = 4_000_000
  TERRAIN_MAX_SPLATS = 2048
  TERRAIN_MAX_SPLAT_CHECKS = 8_000_000
  TERRAIN_MAX_TEXTURE_BYTES = 1024
  TERRAIN_MIN_TILE = 0.01
  TERRAIN_MAX_TILE = 10_000.0
  FOG_MIN_FAR = 0.1
  FOG_MAX_FAR = 100_000.0
  WEATHER_MAX_INTENSITY = 10.0

  Finding = Struct.new(:severity, :where, :message) do
    def fehler? = severity == FEHLER
    def warnung? = severity == WARNUNG

    def to_s
      "#{fehler? ? '[Fehler]  ' : '[Warnung] '}#{where}: #{message}"
    end
  end

  # Befehlstypen des Interpreters. Muss zu game/engine/interpreter.rb und zu
  # EV_TYPES / ISSUE_CMD_TYPES in src/editor/editor_main.cpp passen.
  # Register (Z8): docs/befehle.json - tools/befehle_test.rb vergleicht alle
  # fünf Stellen samt Reihenfolge.
  CMD_TYPES = %w[
    text choices log
    wait switch toggle_switch variable
    if_switch if_variable script
    teleport move_player heal
    weather effect sound
    item gold
    save_game load_game
    self_switch if_self_switch fade
    move_route input_number battle
    loop break label jump exit_event erase_event
    button_input text_options common_event
    if_gold if_item if_script if_actor if_button
    save_access menu_access encounter_access
    timer
    actor_hp actor_sp actor_exp actor_level
    party_member shop name_input
    screen_tone flash
    menu
  ].freeze

  TRIGGER   = %w[action touch auto parallel].freeze
  WETTER    = %w[kein regen schnee].freeze
  EFFEKTE   = %w[sparkle dust burst].freeze
  VERGLEICH = %w[== != < <= > >=].freeze
  BEDINGUNGEN = %w[switch variable self_switch].freeze
  SELF_SWITCH_BUCHSTABEN = %w[A B C D].freeze     # Etappe 19d (XP: A-D)
  FADE_RICHTUNGEN = %w[ein aus].freeze            # Etappe 19d
  # Etappe 20c: Fensterpositionen von "Fenster-Optionen" (XP 104) und die
  # Tastennamen der Tasteneingabe (XP 105). Die Tastennamen sind dieselben,
  # die Rune::Input kennt (src/ruby/vm.cpp: Buchstaben + Sondertasten).
  FENSTER_POSITIONEN = %w[oben mitte unten].freeze
  # Etappe 20c: Trigger Gemeinsamer Events (CommonEvents::TRIGGER; hier als
  # eigene Liste, weil mapcheck.rb vor common_events.rb geladen wird).
  COMMON_TRIGGER = %w[none autorun parallel].freeze
  TIMER_MODI = %w[start stop].freeze     # Etappe 20d (XP 124)
  # Etappe 20d: Mitglieder aendern (XP 129) - XP kennt nur add/remove.
  PARTY_MODI = %w[add remove].freeze
  # Etappe 20f: XP-Blitzfarben (Game_Screen#start_flash kennt nur diese).
  FLASH_FARBEN = %w[weiss rot gruen blau gelb].freeze
  # Etappe 20d: Operationen von `variable` (XP 122) - dieselbe Liste wie
  # EVENT_VAR_OPS im Editor (sync_test vergleicht beide).
  VARIABLE_OPS = %w[set add sub mul div mod].freeze
  # Etappe 20d: Quellen des Operanden (XP 122). "const" ist der Default, wenn
  # `source` fehlt; "self" liest den bisherigen Variablenwert.
  VARIABLE_QUELLEN = %w[const variable random item self].freeze
  TASTEN = (%w[e space enter escape up down left right shift ctrl alt tab
               backspace] + ('a'..'z').to_a + (1..12).map { |n| "f#{n}" }).freeze
  # Etappe 19g, XP-Vokabular seit 19h.2: muss zu Mover::SCHRITTE passen
  # (move_route_test prüft das - mapcheck.rb wird früher geladen als
  # mover.rb, deshalb keine Referenz). Deutsche Kurznamen (up/down/...) und
  # XP-Codes ('1'..'45') versteht der Mover zusätzlich als Aliase.
  MOVER_SCHRITTE = %w[
    move_down move_left move_right move_up
    move_lower_left move_lower_right move_upper_left move_upper_right
    move_random move_toward_player move_away_from_player
    move_forward move_backward jump wait
    turn_down turn_left turn_right turn_up
    turn_90d_right turn_90d_left turn_180d turn_90d_right_or_left
    turn_random turn_toward_player turn_away_from_player
    switch_on switch_off change_speed change_freq
    move_animation_on move_animation_off
    stop_animation_on stop_animation_off
    direction_fix_on direction_fix_off through_on through_off
    transparent_on transparent_off
    always_on_top_on always_on_top_off
    change_graphic change_opacity change_blending play_se script
  ].freeze

  # Deutsche Kurznamen aus Etappe 19g (Mover::ALIASE) bleiben gültig; die
  # XP-Befehls-Codes ('1'..'45') versteht der Mover ebenfalls - beide
  # Alias-Formen sind hier bewusst nicht doppelt gepflegt: Der Sync der
  # Hauptliste wird in move_route_test.rb gegen Mover::SCHRITTE assertet,
  # die Aliase gegen Mover::ALIASES.keys.
  MOVER_ALIASE = %w[
    down left right up zufall spieler_nah spieler_weg warte
    vor zurueck runter rauf
  ].freeze

  # XP-Befehls-Codes (Mover::XP_CODES) - eigene Liste aus demselben Grund.
  # Reihenfolge wie in RPG Maker XP/MV: 0 = Ende-Marker, 1..11 Bewegung,
  # 12..15 vor/zurück/Sprung/Warte, 16..26 Drehen, 27..38 Schalter/Tempo/
  # Animation/Through, 39..40 Transparent, 41..43 Grafik/Deckkraft/Blendung,
  # 44 SE, 45 Script.
  MOVER_XP_CODES = %w[
    0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
    16 17 18 19 20 21 22 23 24 25 26 27 28 29 30
    31 32 33 34 35 36 37 38 39 40 41 42 43 44 45
  ].freeze

  # Schritte, die den Route-Parameter 'code' brauchen (19h.2).
  MOVER_CODE_PFLICHT = %w[switch_on switch_off change_speed play_se script].freeze
  SPEICHER_SLOTS = (1..3).freeze

  module_function

  # ------------------------------------------------------------------ Einstieg
  # Karte (Hash aus JSON.parse) prüfen.
  #   name:      Anzeigename für die Fundstelle ("dorf.json")
  #   prefabs:   bekannte Objekttypen (Standard: Prefabs.ids, falls geladen)
  #   materials: bekannte Materialnamen (optional, sonst keine Tex-Prüfung)
  #   scripts:   bekannte Script-Namen (optional)
  #   maps:      bekannte Karten-Ids für Teleport-Ziele (optional)
  #   common_events: bekannte Namen Gemeinsamer Events (optional, 20c)
  def scan(map, name: 'karte', prefabs: default_prefabs, materials: nil,
           scripts: nil, maps: nil, common_events: nil)
    f = Sammler.new(name, prefabs: prefabs, materials: materials,
                          scripts: scripts, maps: maps,
                          common_events: common_events)
    f.karte(map)
    f.liste
  end

  # Etappe 20c: database.json prüfen (bisher nur Karten). Geprüft werden die
  # Gemeinsamen Events: eindeutige Namen, gültige Trigger, Schalter-Pflicht
  # bei autorun/parallel und die Befehlslisten selbst.
  def scan_database(db, name: 'database.json')
    f = Sammler.new(name, prefabs: default_prefabs)
    f.gemeinsame_events(db)
    f.liste
  end

  # Eine Karten-Datei laden und prüfen. Parse-Fehler werden als EIN Fehler
  # gemeldet (die Datei ist dann nicht weiter prüfbar).
  def scan_file(path, **opts)
    text = File.read(path, encoding: 'utf-8')
    scan(JSON.parse(text), **opts.merge(name: opts[:name] || File.basename(path)))
  rescue JSON::ParserError => e
    [Finding.new(FEHLER, File.basename(path.to_s), "JSON-Parse-Fehler: #{e.message}")]
  rescue SystemCallError => e
    [Finding.new(FEHLER, File.basename(path.to_s), "nicht lesbar: #{e.message}")]
  end

  # Alle Karten unter einem oder mehreren Verzeichnissen prüfen.
  # Rückgabe: { pfad => fundliste } (nur Pfade mit Funden, wenn nur_funde).
  def scan_tree(*roots, nur_funde: false, **opts)
    ids = map_ids(*roots)
    out = {}
    map_files(*roots).sort.each do |path|
      funde = scan_file(path, **opts.merge(maps: ids))
      out[path] = funde unless nur_funde && funde.empty?
    end
    out
  end

  # Karten-Ids (Dateinamen ohne .json) aus den Verzeichnissen — für die
  # Teleport-Zielprüfung.
  def map_ids(*roots)
    map_files(*roots).map { |p| File.basename(p).sub(/\.json\z/i, '') }.sort
  end

  # Nur JSON-Dateien aus einem "maps"-Ordner zählen als Karten (database.json,
  # system.json, scripts.json und Spielstände gehören nicht dazu). Einzelne
  # Dateien, die man direkt übergibt, werden immer geprüft.
  def map_files(*roots)
    roots.flatten.flat_map do |root|
      next [root] if File.file?(root)
      next [] unless File.directory?(root)

      # Der Editor und Database akzeptieren auch .JSON und gemischte
      # Groß-/Kleinschreibung. Verzeichnisse mit dieser Endung sind keine Karte.
      Dir.glob(File.join(root, '**', '*.[jJ][sS][oO][nN]')).select do |pfad|
        ordner = File.dirname(pfad).split(%r{[/\\]})
        File.file?(pfad) && (ordner & %w[maps map]).any? && !ordner.include?('saves')
      end
    end
  end

  # Funde in das Laufzeit-Backlog schreiben (Issues, Etappe 18h). Ohne Issues
  # passiert nichts — das Modul bleibt damit frei von Engine-Abhängigkeiten.
  def report!(funde, area: 'Karte')
    return 0 unless defined?(Issues) && Issues.respond_to?(:report)

    funde.each do |f|
      Issues.report(area, "#{f.where}: #{f.message}",
                    severity: f.fehler? ? 'error' : 'warn')
    end
    funde.size
  end

  def summary(funde)
    fehler   = funde.count(&:fehler?)
    warnung  = funde.count(&:warnung?)
    "#{fehler} Fehler, #{warnung} Warnung#{warnung == 1 ? '' : 'en'}"
  end

  def default_prefabs
    return Prefabs.ids if defined?(Prefabs) && Prefabs.respond_to?(:ids)

    pfad = File.expand_path('../data/prefabs.json', __dir__)
    return nil unless File.exist?(pfad)

    JSON.parse(File.read(pfad, encoding: 'utf-8'))['prefabs'].map { |p| p['id'] }
  rescue StandardError
    nil
  end

  # ------------------------------------------------------------- Sammler-Klasse
  # Hält die Optionen und die Fundliste und kapselt die Typ-Prüfungen, damit
  # scan() oben klein bleibt.
  class Sammler
    attr_reader :liste

    def initialize(name, prefabs: nil, materials: nil, scripts: nil, maps: nil,
                   common_events: nil)
      @karte_name = name
      @prefabs    = prefabs
      @materials  = materials
      @scripts    = scripts
      @maps       = maps
      @common_events = common_events
      @liste      = []
      # Etappe 19k.3: Block-Zonen der GEPRÜFTEN Karte (nicht der geladenen!) -
      # für die Lage-Prüfungen „steht in einer Block-Zone".
      @block_zonen = []
    end

    def karte(map)
      unless map.is_a?(Hash)
        melde(FEHLER, @karte_name, 'Wurzel ist kein Objekt')
        return
      end

      @block_zonen = zonen_aus(map)   # Etappe 19k.3: vor den Lage-Prüfungen
      pruefe_name(map)
      pruefe_terrain(map['terrain'])
      pruefe_objekte(map['objects'])
      pruefe_events(map['events'])
      pruefe_npcs(map['npcs'])
      pruefe_blocked(map['blocked'])
      pruefe_encounters(map['encounters'])   # Etappe 19l: Zufallskämpfe
      pruefe_atmosphaere(map)
    end

    # Etappe 20c: Gemeinsame Events der Datenbank (öffentlich, weil
    # scan_database sie direkt aufruft).
    def gemeinsame_events(db)
      return unless db.is_a?(Hash) && db.key?('common_events')

      liste = db['common_events']
      wo = "#{@karte_name} common_events"
      unless liste.is_a?(Array)
        melde(FEHLER, wo, 'ist keine Liste')
        return
      end

      namen = []
      liste.each_with_index do |e, i|
        w = "#{wo} ##{i + 1}"
        unless e.is_a?(Hash)
          melde(FEHLER, w, 'ist kein Objekt')
          next
        end
        w += " ('#{e['name']}')" if text?(e['name'])
        if !text?(e['name'])
          melde(FEHLER, w, "'name' fehlt oder ist leer")
        else
          melde(FEHLER, w, "Name '#{e['name']}' ist doppelt") if namen.include?(e['name'].to_s)
          namen << e['name'].to_s
        end
        trig = e.key?('trigger') ? e['trigger'].to_s : 'none'
        if !COMMON_TRIGGER.include?(trig)
          melde(FEHLER, w, "unbekannter Trigger '#{trig}' (none/autorun/parallel)")
        elsif trig != 'none' && !text?(e['switch'])
          melde(WARNUNG, w,
                "#{trig} ohne 'switch': läuft dauerhaft und blockiert andere Events")
        end
        if e.key?('switch') && !text?(e['switch'])
          melde(FEHLER, w, "'switch' muss ein Name sein")
        end
        befehle = e['commands']
        if !befehle.is_a?(Array)
          melde(FEHLER, w, "'commands' fehlt/keine Liste")
        elsif befehle.empty?
          melde(WARNUNG, w, 'keine Befehle (Aufruf bewirkt nichts)')
        else
          pruefe_befehle(befehle, w)
        end
      end
    end

    private

    def melde(severity, where, message)
      @liste << Finding.new(severity, where, message)
    end

    # Etappe 20d: `variable` prüfen (XP 122). Neu ist `op` + `value`; die
    # Altform (`value`/`add` ohne op) bleibt gültig, wird aber angemahnt, weil
    # der Editor sie beim nächsten Übernehmen umstellt.
    # Etappe 20d: Quelle des Operanden prüfen (XP 122).
    def pruefe_variable_quelle(c, w)
      # Ohne `source` gilt die Altform (value/add) - die prüft pruefe_variable.
      return unless c.key?('source')
      quelle = c['source'].to_s.strip.downcase
      quelle = 'const' if quelle.empty?
      unless VARIABLE_QUELLEN.include?(quelle)
        melde(FEHLER, w, "unbekannte Variable-Quelle '#{quelle}' (#{VARIABLE_QUELLEN.join('/')})")
        return
      end
      src = c.key?('src') ? c['src'].to_s.strip : ''
      case quelle
      when 'variable'
        melde(FEHLER, w, "'src' (Variable) fehlt oder ist leer") if src.empty?
      when 'item'
        if src.empty?
          melde(FEHLER, w, "'src' (Item) fehlt oder ist leer")
        else
          bekannt = (Database.database['items'] || []).any? do |h|
            h.is_a?(Hash) && h['id'].to_s == src
          end
          melde(WARNUNG, w, "Item '#{src}' nicht in database.json") unless bekannt
        end
      when 'random'
        if !zahl?(c['value']) || c['value'].to_f < 1
          melde(FEHLER, w, "random braucht 'value' >= 1 (Obergrenze)")
        end
      when 'self'
        # kein Zusatzfeld nötig (bisheriger Variablenwert)
      when 'const'
        # Der Wert wird unten bei der Operation geprüft (Altform ohne `op`
        # hat kein `source` und wird gar nicht erst hier geprüft).
      end
      if !src.empty? && %w[const random self].include?(quelle)
        melde(WARNUNG, w, "'src' wird bei Quelle '#{quelle}' ignoriert")
      end
    end

    def pruefe_variable(c, w)
      pruefe_variable_quelle(c, w)
      op = c['op']
      if op
        quelle = c.key?('source') ? c['source'].to_s.strip.downcase : 'const'
        if quelle.empty? || quelle == 'const'
          melde(WARNUNG, w, "'value' fehlt oder ist keine Zahl") unless zahl?(c['value'])
        end
        unless VARIABLE_OPS.include?(op.to_s)
          melde(FEHLER, w, "unbekannte Variable-Operation '#{op}' (#{VARIABLE_OPS.join('/')})")
        end
        if %w[div mod].include?(op.to_s) && zahl?(c['value']) && c['value'].to_f.zero?
          melde(FEHLER, w, "#{op} durch 0")
        end
        if c.key?('add')
          melde(WARNUNG, w, "'add' (Altform) neben 'op' - 'add' wird ignoriert")
        end
      elsif !c.key?('value') && !c.key?('add')
        melde(WARNUNG, w, "weder 'value' noch 'add' (Befehl ohne Wirkung)")
      elsif c.key?('value') && c.key?('add')
        melde(WARNUNG, w, "Altform: 'value' setzt und 'add' addiert - bitte 'op' verwenden")
      end
    end

    def zahl?(v) = v.is_a?(Numeric)
    def text?(v) = v.is_a?(String) && !v.strip.empty?

    # F1: numerischen XP-Code ('27') auf den Schritt-Namen (switch_on)
    # auflösen - über Mover::XP_CODES, die beim Karten-Scan geladen ist.
    # In reinen MapCheck-Attrappen (ohne Mover) bleibt der Rohtext.
    def xp_schritt(s)
      name = s.to_s
      if defined?(Mover) && Mover.const_defined?(:XP_CODES)
        Mover::XP_CODES[name] || name
      else
        name
      end
    end

    # Optionaler y-Versatz ist bei null/fehlendem Feld wie 0 zu behandeln.
    def pruefe_y(objekt, wo)
      y = objekt['y']
      return if y.nil? || (zahl?(y) && y.to_f.finite?)

      melde(WARNUNG, wo, "'y' muss eine endliche Zahl sein (Versatz zum Gelände)")
    end

    # --- Etappe 19k.3: Lage in Block-Zonen ----------------------------------
    # Nur brauchbare Zonen (Zahlen, w/h > 0) zählen - kaputte Einträge meldet
    # pruefe_blocked bereits als Fehler/Warnung.
    def zonen_aus(map)
      liste = map['blocked']
      return [] unless liste.is_a?(Array)

      liste.select do |b|
        b.is_a?(Hash) && %w[x z w h].all? { |k| zahl?(b[k]) } &&
          b['w'] > 0 && b['h'] > 0
      end
    end

    # Liegt (x, z) in einer Block-Zone? Rand gehört dazu (wie Passage/Physik).
    def in_zone?(x, z)
      return false unless zahl?(x) && zahl?(z)

      @block_zonen.any? do |b|
        (x.to_f - b['x'].to_f).abs * 2.0 <= b['w'].to_f &&
          (z.to_f - b['z'].to_f).abs * 2.0 <= b['h'].to_f
      end
    end

    # ------------------------------------------------------------------ Karte
    def pruefe_name(map)
      melde(WARNUNG, @karte_name, "'name' fehlt oder ist leer") unless text?(map['name'])
      return if map['start'].nil?

      st = map['start']
      wo = "#{@karte_name} Startpunkt"
      melde(FEHLER, wo, 'ist kein Objekt') unless st.is_a?(Hash)
      return unless st.is_a?(Hash)

      %w[x z].each do |k|
        melde(FEHLER, wo, "'#{k}' fehlt oder ist keine Zahl") unless zahl?(st[k])
      end
      if in_zone?(st['x'], st['z'])
        melde(WARNUNG, wo, 'liegt in einer Block-Zone (Spieler steckt fest)')
      end
    end

    # Ruby-Integer können beliebig groß sein. Vor to_f/round und jeder
    # Multiplikation prüfen, statt Inf oder einen riesigen Loop zu erzeugen.
    def terrain_bereich?(v, min, max)
      (v.is_a?(Integer) || (v.is_a?(Float) && v.finite?)) && v >= min && v <= max
    end

    def terrain_float32(v) = [v.to_f].pack('f').unpack1('f')
    def terrain_texture_name?(v)
      v.is_a?(String) && !v.empty? && v.bytesize <= TERRAIN_MAX_TEXTURE_BYTES && !v.include?("\0")
    end

    # Entspricht TerrainPlan; Rückgabe nil, wenn das Raster nicht sicher ist.
    def terrain_pruefplan(t)
      size = t['size'].nil? ? 240 : t['size']
      width = t['width'].nil? ? size : t['width']
      height = t['height'].nil? ? size : t['height']
      res = t['resolution'].nil? ? 192 : t['resolution']
      seed = t['seed'].nil? ? 7 : t['seed']
      amp = t['amplitude'].nil? ? 0 : t['amplitude']
      return nil unless terrain_bereich?(width, TERRAIN_MIN_SIZE, TERRAIN_MAX_SIZE) &&
                        terrain_bereich?(height, TERRAIN_MIN_SIZE, TERRAIN_MAX_SIZE) &&
                        terrain_bereich?(res, TERRAIN_MIN_RES, TERRAIN_MAX_RES) &&
                        terrain_bereich?(seed, 0, 4_294_967_295) &&
                        terrain_bereich?(amp, -TERRAIN_MAX_AMPLITUDE, TERRAIN_MAX_AMPLITUDE)

      w = terrain_float32(width)
      h = terrain_float32(height)
      rx = res.to_i
      z = terrain_float32(terrain_float32(rx * h) / [1.0, w].max)
      return nil if z >= TERRAIN_MAX_RES + 0.5

      rz = [TERRAIN_MIN_RES, z.round].max
      return nil if rx * rz > TERRAIN_MAX_QUADS

      [w, h, rx, rz]
    end

    def pruefe_terrain(t)
      return if t.nil?

      wo = "#{@karte_name} terrain"
      unless t.is_a?(Hash)
        melde(FEHLER, wo, 'ist kein Objekt')
        return
      end

      groesse = t['width'] || t['size']
      hoehe   = t['height'] || t['size']
      melde(WARNUNG, wo, "'width'/'size' fehlt oder <= 0") unless zahl?(groesse) && groesse > 0
      melde(WARNUNG, wo, "'height'/'size' fehlt oder <= 0") unless zahl?(hoehe) && hoehe > 0
      if t.key?('resolution') && !(terrain_bereich?(t['resolution'], 2, TERRAIN_MAX_RES))
        melde(WARNUNG, wo, "'resolution' muss > 1 sein und im sicheren Bereich liegen")
      end
      if t.key?('amplitude') && !zahl?(t['amplitude'])
        melde(WARNUNG, wo, "'amplitude' ist keine Zahl")
      end
      if t.key?('tile') && !t['tile'].nil? && !terrain_bereich?(t['tile'], TERRAIN_MIN_TILE, TERRAIN_MAX_TILE)
        melde(WARNUNG, wo, "'tile' muss zwischen 0.01 und 10000 liegen; Ersatz 2 m")
      end
      if !t['texture'].nil? && !terrain_texture_name?(t['texture'])
        melde(WARNUNG, wo, "'texture' ungültig oder zu lang; Gras als Ersatz")
      end

      plan = terrain_pruefplan(t)
      melde(WARNUNG, wo, 'Maße, Auflösung, Seed, Amplitude oder Rasterbudget ungültig; kein Gelände') unless plan
      pruefe_heights(t['heights'])
      pruefe_splats(t['splats'])
      pruefe_terrain_budget(t, plan) if plan
    end

    def pruefe_terrain_budget(t, plan)
      width, height, rx, rz = plan
      splats = t['splats']
      if splats.is_a?(Array) && !splats.empty? && rx * rz > TERRAIN_MAX_SPLAT_CHECKS / splats.size
        melde(WARNUNG, "#{@karte_name} terrain.splats", 'Rechenbudget überschritten; kein Gelände')
      end
      brushes = t['heights']
      return unless brushes.is_a?(Array) && brushes.size <= TERRAIN_MAX_BRUSHES

      samples = 0
      brushes.each do |b|
        next unless b.is_a?(Hash) &&
                    terrain_bereich?(b['x'], -TERRAIN_MAX_COORD, TERRAIN_MAX_COORD) &&
                    terrain_bereich?(b['z'], -TERRAIN_MAX_COORD, TERRAIN_MAX_COORD) &&
                    terrain_bereich?(b['r'], TERRAIN_MIN_RADIUS, TERRAIN_MAX_COORD) &&
                    terrain_bereich?(b['h'], -TERRAIN_MAX_AMPLITUDE, TERRAIN_MAX_AMPLITUDE)
        x, z, radius, h = %w[x z r h].map { |key| terrain_float32(b[key]) }
        next if h.zero?

        hw, hh = width * 0.5, height * 0.5
        next if x - radius > hw || x + radius < -hw || z - radius > hh || z + radius < -hh

        x0 = (((x - radius + hw) * rx / width).clamp(0.0, rx)).floor
        x1 = (((x + radius + hw) * rx / width).clamp(0.0, rx)).ceil
        z0 = (((z - radius + hh) * rz / height).clamp(0.0, rz)).floor
        z1 = (((z + radius + hh) * rz / height).clamp(0.0, rz)).ceil
        samples += (x1 - x0 + 1) * (z1 - z0 + 1)
        if samples > TERRAIN_MAX_BRUSH_SAMPLES
          melde(WARNUNG, "#{@karte_name} terrain.heights", 'Rechenbudget überschritten; kein Gelände')
          break
        end
      end
    end

    # Gemalte Berge/Senken (Etappe 18l): [{x,z,r,h}]
    def pruefe_heights(liste)
      return if liste.nil?

      wo = "#{@karte_name} terrain.heights"
      unless liste.is_a?(Array)
        melde(FEHLER, wo, 'ist keine Liste')
        return
      end
      if liste.size > TERRAIN_MAX_BRUSHES
        melde(WARNUNG, wo, 'mehr als 2048 Höhenpinsel; kein Gelände')
        return
      end

      liste.each_with_index do |b, i|
        w = "#{wo} ##{i + 1}"
        unless b.is_a?(Hash)
          melde(FEHLER, w, 'ist kein Objekt')
          next
        end
        %w[x z r h].each do |k|
          melde(FEHLER, w, "'#{k}' fehlt oder ist keine Zahl") unless zahl?(b[k])
        end
        melde(WARNUNG, w, "'r' (Radius) muss > 0 sein") if zahl?(b['r']) && b['r'] <= 0
        melde(WARNUNG, w, "'h' ist 0 (Bürste ohne Wirkung)") if zahl?(b['h']) && b['h'].zero?
        if %w[x z r h].all? { |k| zahl?(b[k]) } &&
           !(terrain_bereich?(b['x'], -TERRAIN_MAX_COORD, TERRAIN_MAX_COORD) &&
             terrain_bereich?(b['z'], -TERRAIN_MAX_COORD, TERRAIN_MAX_COORD) &&
             terrain_bereich?(b['r'], TERRAIN_MIN_RADIUS, TERRAIN_MAX_COORD) &&
             terrain_bereich?(b['h'], -TERRAIN_MAX_AMPLITUDE, TERRAIN_MAX_AMPLITUDE))
          melde(WARNUNG, w, 'Pinselwerte außerhalb des sicheren Bereichs; kein Gelände')
        end
      end
    end

    # Block-Zonen (Etappe 19k.2): [{x,z,w,h}] - x/z = Mitte in Weltmetern,
    # w/h = Größe; die Physik blockt sie über alle Höhen.
    def pruefe_blocked(liste)
      return if liste.nil?

      wo = "#{@karte_name} blocked"
      unless liste.is_a?(Array)
        melde(FEHLER, wo, 'ist keine Liste')
        return
      end

      liste.each_with_index do |b, i|
        w = "#{wo} ##{i + 1}"
        unless b.is_a?(Hash)
          melde(FEHLER, w, 'ist kein Objekt')
          next
        end
        %w[x z w h].each do |k|
          melde(FEHLER, w, "'#{k}' fehlt oder ist keine Zahl") unless zahl?(b[k])
        end
        %w[w h].each do |k|
          melde(WARNUNG, w, "'#{k}' muss > 0 sein") if zahl?(b[k]) && b[k] <= 0
        end
      end
    end

    # Zufallskämpfe (Etappe 19l): { "troops": ["waldweg", ...],
    # "steps": 30 } - Truppen-IDs aus database.json, mittlere Schrittzahl
    # zwischen Kämpfen. Ob die Truppen existieren, prüft erst der Kampf
    # (Battle.start warnt) - MapCheck kennt die Datenbank nicht (wie beim
    # battle-Befehl, der nur 'name' als Text verlangt).
    def pruefe_encounters(enc)
      return if enc.nil?

      wo = "#{@karte_name} encounters"
      unless enc.is_a?(Hash)
        melde(FEHLER, wo, 'ist kein Objekt')
        return
      end

      troops = enc['troops']
      unless troops.nil?
        unless troops.is_a?(Array)
          melde(FEHLER, wo, "'troops' ist keine Liste")
        else
          troops.each_with_index do |t, i|
            melde(FEHLER, "#{wo} troops ##{i + 1}", 'ist keine Truppen-ID (Text)') unless text?(t)
          end
        end
      end

      steps = enc['steps']
      gueltig = (steps.is_a?(Integer) && steps.positive?) ||
                 (steps.is_a?(Float) && steps.finite? && steps >= 1 && steps == steps.floor)
      if !steps.nil? && !gueltig
        melde(WARNUNG, wo, "'steps' muss eine ganze Zahl > 0 sein")
      end
    end

    # Splat-Flächen (Etappe 18d): [{tex,x,z,w,h}]
    def pruefe_splats(liste)
      return if liste.nil?

      wo = "#{@karte_name} terrain.splats"
      unless liste.is_a?(Array)
        melde(FEHLER, wo, 'ist keine Liste')
        return
      end
      if liste.size > TERRAIN_MAX_SPLATS
        melde(WARNUNG, wo, 'mehr als 2048 Bodenflächen; kein Gelände')
        return
      end

      liste.each_with_index do |s, i|
        w = "#{wo} ##{i + 1}"
        unless s.is_a?(Hash)
          melde(FEHLER, w, 'ist kein Objekt')
          next
        end
        if !terrain_texture_name?(s['tex']) && s['tex'].is_a?(String) && !s['tex'].empty?
          melde(WARNUNG, w, 'Texturname ungültig oder zu lang; kein Gelände')
        elsif !terrain_texture_name?(s['tex']) || !text?(s['tex'])
          melde(FEHLER, w, "ohne Material ('tex')")
        end
        %w[x z w h].each do |k|
          melde(FEHLER, w, "'#{k}' fehlt oder ist keine Zahl") unless zahl?(s[k])
        end
        %w[w h].each do |k|
          melde(WARNUNG, w, "'#{k}' muss > 0 sein") if zahl?(s[k]) && s[k] <= 0
        end
        if %w[x z w h].all? { |k| zahl?(s[k]) } &&
           !(terrain_bereich?(s['x'], -TERRAIN_MAX_COORD, TERRAIN_MAX_COORD) &&
             terrain_bereich?(s['z'], -TERRAIN_MAX_COORD, TERRAIN_MAX_COORD) &&
             terrain_bereich?(s['w'], TERRAIN_MIN_RADIUS, TERRAIN_MAX_COORD) &&
             terrain_bereich?(s['h'], TERRAIN_MIN_RADIUS, TERRAIN_MAX_COORD))
          melde(WARNUNG, w, 'Fläche außerhalb des sicheren Bereichs; kein Gelände')
        end
        next unless @materials && terrain_texture_name?(s['tex']) && text?(s['tex']) &&
                    !@materials.include?(s['tex'])

        melde(WARNUNG, w, "Material '#{s['tex']}' ist nicht geladen")
      end
    end

    def pruefe_objekte(liste)
      return if liste.nil?

      wo = "#{@karte_name} objects"
      unless liste.is_a?(Array)
        melde(FEHLER, wo, 'ist keine Liste')
        return
      end

      liste.each_with_index do |o, i|
        w = "#{wo} ##{i + 1}"
        unless o.is_a?(Hash)
          melde(FEHLER, w, 'ist kein Objekt')
          next
        end
        typ = o['type']
        unless text?(typ)
          melde(FEHLER, w, "ohne 'type'")
        end
        if @prefabs && text?(typ) && !@prefabs.include?(typ.to_s)
          melde(WARNUNG, w, "unbekannter Prefab-Typ '#{typ}'")
        end
        %w[x z].each do |k|
          melde(FEHLER, w, "'#{k}' fehlt oder ist keine Zahl") unless zahl?(o[k])
        end
        pruefe_y(o, w)
        melde(WARNUNG, w, "'rot' ist keine Zahl") if o.key?('rot') && !zahl?(o['rot'])
        if @materials && text?(o['tex']) && !@materials.include?(o['tex'])
          melde(WARNUNG, w, "Material '#{o['tex']}' ist nicht geladen")
        end
      end
    end

    def pruefe_events(liste)
      return if liste.nil?

      wo = "#{@karte_name} events"
      unless liste.is_a?(Array)
        melde(FEHLER, wo, 'ist keine Liste')
        return
      end

      ids = Hash.new(0)
      liste.each_with_index do |e, i|
        w = "#{wo} ##{i + 1}"
        unless e.is_a?(Hash)
          melde(FEHLER, w, 'ist kein Objekt')
          next
        end
        w += " ('#{e['name']}')" if text?(e['name'])
        melde(WARNUNG, w, "'name' fehlt oder ist leer") unless text?(e['name'])
        %w[x z].each do |k|
          melde(FEHLER, w, "'#{k}' fehlt oder ist keine Zahl") unless zahl?(e[k])
        end
        pruefe_y(e, w)
        melde(WARNUNG, w, 'steht in einer Block-Zone (nicht erreichbar)') if in_zone?(e['x'], e['z'])
        if e.key?('radius') && !(zahl?(e['radius']) && e['radius'] > 0)
          melde(WARNUNG, w, "'radius' muss > 0 sein")
        end
        if e.key?('id')
          ids[e['id']] += 1
          melde(WARNUNG, w, "'id' ist keine Zahl") unless zahl?(e['id'])
        end
        # Altes Format: Trigger/commands direkt am Event (weiter unterstützt).
        if e.key?('trigger') && !TRIGGER.include?(e['trigger'].to_s)
          melde(WARNUNG, w, "unbekannter Trigger '#{e['trigger']}'")
        end
        if e['pages'].is_a?(Array)
          melde(WARNUNG, w, "'pages' ist leer") if e['pages'].empty?

          e['pages'].each_with_index { |p, pi| pruefe_seite(p, "#{w} Seite #{pi + 1}") }
        elsif e.key?('commands')
          pruefe_befehle(e['commands'], w)
        else
          melde(WARNUNG, w, "weder 'pages' noch 'commands'")
        end
      end

      ids.each do |id, anzahl|
        next unless anzahl > 1

        melde(WARNUNG, wo, "Event-Id #{id} kommt #{anzahl}x vor")
      end
    end

    def pruefe_seite(seite, wo)
      unless seite.is_a?(Hash)
        melde(FEHLER, wo, 'ist kein Objekt')
        return
      end

      if seite.key?('trigger') && !TRIGGER.include?(seite['trigger'].to_s)
        melde(WARNUNG, wo, "unbekannter Trigger '#{seite['trigger']}'")
      end
      if !seite['commands'].is_a?(Array)
        melde(FEHLER, wo, "'commands' fehlt/keine Liste")
      else
        pruefe_befehle(seite['commands'], wo)
      end
      pruefe_bedingungen(seite['conditions'], wo)
    end

    def pruefe_bedingungen(cond, wo)
      return if cond.nil?

      unless cond.is_a?(Hash)
        melde(FEHLER, wo, "'conditions' ist kein Objekt")
        return
      end

      cond.each do |key, wert|
        unless BEDINGUNGEN.include?(key.to_s)
          melde(WARNUNG, wo, "unbekannte Bedingung '#{key}'")
          next
        end
        unless wert.is_a?(Hash)
          melde(FEHLER, "#{wo} Bedingung #{key}", 'ist kein Objekt')
          next
        end
        if key.to_s == 'self_switch'
          # Self-Switch-Bedingung (Etappe 19d): letter + value statt name.
          unless SELF_SWITCH_BUCHSTABEN.include?(wert['letter'].to_s)
            melde(FEHLER, "#{wo} Bedingung #{key}", "'letter' muss A, B, C oder D sein")
          end
          melde(WARNUNG, "#{wo} Bedingung #{key}", "'value' fehlt") unless wert.key?('value')
          next
        end
        unless text?(wert['name'])
          melde(FEHLER, "#{wo} Bedingung #{key}", "'name' fehlt oder ist leer")
        end
        next unless key.to_s == 'variable'

        op = wert['op']
        if op.nil? || !VERGLEICH.include?(op.to_s)
          melde(WARNUNG, "#{wo} Bedingung #{key}", "unbekannter Vergleich '#{op}'")
        end
        melde(WARNUNG, "#{wo} Bedingung #{key}", "'value' fehlt") unless wert.key?('value')
      end
    end

    # Befehlsliste rekursiv (inkl. Dann-/Sonst-/Choices-Zweige) — wie im Editor.
    def pruefe_befehle(liste, wo, schleife: 0)
      unless liste.is_a?(Array)
        melde(FEHLER, wo, "'commands' ist keine Liste")
        return
      end

      liste.each_with_index do |c, i|
        w = "#{wo} Befehl ##{i + 1}"
        unless c.is_a?(Hash)
          melde(FEHLER, w, 'Befehl ist kein Objekt')
          next
        end
        typ = c['cmd']
        unless text?(typ)
          melde(FEHLER, w, "ohne 'cmd'")
          next
        end
        unless CMD_TYPES.include?(typ.to_s)
          melde(FEHLER, w, "unbekannter Befehlstyp '#{typ}'")
          next
        end
        pruefe_befehl(typ.to_s, c, w, schleife: schleife)
        # Etappe 20c: Sprungziele müssen in DIESER Liste als Marke stehen
        # (XP 118/119 sucht ebenfalls nur im aktuellen Block).
        if typ.to_s == 'jump'
          ziel = c['name'].to_s
          marken = liste.select { |x| x.is_a?(Hash) && x['cmd'] == 'label' }
          unless marken.any? { |x| x['name'].to_s == ziel }
            melde(WARNUNG, w, "keine Marke '#{ziel}' in diesem Block")
          end
        end
      end
    end

    def pruefe_befehl(typ, c, w, schleife: 0)
      case typ
      when 'text'
        if !c['lines'].is_a?(Array) || c['lines'].empty?
          melde(WARNUNG, w, "'lines' fehlt/leer")
        end
      when 'choices'
        opts = c['options']
        if !opts.is_a?(Array) || opts.empty?
          melde(FEHLER, w, "'options' fehlt/leer")
        end
        branches = c['branches']
        if !branches.is_a?(Array)
          melde(FEHLER, w, "'branches' fehlt")
        elsif opts.is_a?(Array) && branches.size < opts.size
          melde(WARNUNG, w, 'weniger Zweige als Optionen')
        end
        # Z5: Zweige rekursiv prüfen (der Kommentar oben behauptete das
        # schon lange für choices - jetzt stimmt es auch) und dabei
        # Nicht-Listen melden (die Engine stürzt z. B. auf einem
        # Wahrheitswert-Zweig beim .size ab).
        if branches.is_a?(Array)
          branches.each_with_index do |z, zi|
            if z.is_a?(Array)
              pruefe_befehle(z, "#{w} (Wahl ##{zi + 1})")
            else
              melde(FEHLER, w, "Wahlzweig ##{zi + 1} ist keine Liste")
            end
          end
        end
        # Z5: cancel (XP "When Cancel") - dieselben Regeln wie im Editor
        # (Issue-Scan) und in Interpreter#cancel_modus.
        cancel = c['cancel']
        cancel_ok =
          if cancel.nil? || cancel == false || cancel == 'aus' ||
             cancel == 'verzweigung' || cancel == true
            true
          elsif cancel.is_a?(Integer)
            opts.is_a?(Array) && cancel >= 0 && cancel < opts.size
          elsif cancel.is_a?(String) && cancel.match?(/\A\d+\z/)
            opts.is_a?(Array) && cancel.to_i < opts.size
          else
            false
          end
        unless cancel_ok
          melde(WARNUNG, w,
                "unbekannter cancel-Wert #{cancel.inspect} (aus/verzweigung/Optionsnummer)")
        end
        cb = c['cancel_branch']
        if !cb.nil? && !cb.is_a?(Array)
          melde(FEHLER, w, "'cancel_branch' ist keine Liste")
        elsif cb.is_a?(Array)
          pruefe_befehle(cb, "#{w} (Abbruch)")
        elsif cancel == 'verzweigung' || cancel == true
          melde(WARNUNG, w, "'cancel_branch' fehlt/keine Liste")
        end
      when 'if_switch', 'if_variable'
        %w[then else].each do |k|
          melde(WARNUNG, w, "'#{k}' fehlt/keine Liste") unless c[k].is_a?(Array)
        end
        pruefe_befehle(c['then'], "#{w} (Dann)")   if c['then'].is_a?(Array)
        pruefe_befehle(c['else'], "#{w} (Sonst)")  if c['else'].is_a?(Array)
        if typ == 'if_variable' && !VERGLEICH.include?(c['op'].to_s)
          melde(WARNUNG, w, "unbekannter Vergleich '#{c['op']}'")
        end
        melde(WARNUNG, w, "'name' fehlt") unless text?(c['name'])
      when 'wait'
        if c.key?('seconds') && !(zahl?(c['seconds']) && c['seconds'] >= 0)
          melde(WARNUNG, w, "'seconds' muss eine Zahl >= 0 sein")
        end
      when 'switch', 'toggle_switch', 'variable'
        melde(FEHLER, w, "'name' fehlt oder ist leer") unless text?(c['name'])
        pruefe_variable(c, w) if typ == 'variable'   # Etappe 20d (XP 122)
      when 'script'
        if !text?(c['name']) && !text?(c['code'])
          melde(FEHLER, w, "weder 'name' noch 'code'")
        elsif @scripts && text?(c['name']) && !text?(c['code']) && !@scripts.include?(c['name'])
          melde(WARNUNG, w, "Script '#{c['name']}' ist nicht definiert")
        end
      when 'teleport', 'move_player'
        %w[x z].each do |k|
          melde(FEHLER, w, "'#{k}' fehlt oder ist keine Zahl") unless zahl?(c[k])
        end
        pruefe_kartenziel(c, w)
      when 'weather'
        if !text?(c['name']) || !WETTER.include?(c['name'].to_s)
          melde(WARNUNG, w, "unbekanntes Wetter '#{c['name']}' (kein/regen/schnee)")
        end
        if c.key?('intensity') && !(zahl?(c['intensity']) && c['intensity'] > 0)
          melde(WARNUNG, w, "'intensity' muss > 0 sein")
        end
      when 'effect'
        if text?(c['name']) && !EFFEKTE.include?(c['name'].to_s)
          melde(WARNUNG, w, "unbekannter Effekt '#{c['name']}' (sparkle/dust/burst)")
        end
      when 'sound'
        melde(FEHLER, w, "'name' fehlt oder ist leer") unless text?(c['name'])
      when 'item'
        melde(FEHLER, w, "'name' fehlt oder ist leer") unless text?(c['name'])
        if c.key?('count') && !(zahl?(c['count']) && c['count'] != 0)
          melde(WARNUNG, w, "'count' muss eine Zahl != 0 sein")
        end
      when 'gold'
        melde(WARNUNG, w, "'amount' fehlt oder ist 0") unless zahl?(c['amount']) && c['amount'] != 0
      when 'heal'
        if c.key?('amount') && !(zahl?(c['amount']) && c['amount'] > 0)
          melde(WARNUNG, w, "'amount' muss > 0 sein")
        end
      when 'save_game', 'load_game'
        if c.key?('slot') && (!zahl?(c['slot']) || !SPEICHER_SLOTS.cover?(c['slot'].to_i))
          melde(WARNUNG, w, "'slot' muss #{SPEICHER_SLOTS.first}..#{SPEICHER_SLOTS.last} sein")
        end
      when 'log'
        melde(WARNUNG, w, "'text' fehlt oder ist leer") unless text?(c['text'])
      when 'self_switch'
        # Etappe 19d: Self-Switch des auslösenden Events setzen (A-D).
        unless SELF_SWITCH_BUCHSTABEN.include?(c['letter'].to_s)
          melde(FEHLER, w, "'letter' muss A, B, C oder D sein")
        end
        melde(WARNUNG, w, "'value' fehlt") unless c.key?('value')
      when 'if_self_switch'
        %w[then else].each do |k|
          melde(WARNUNG, w, "'#{k}' fehlt/keine Liste") unless c[k].is_a?(Array)
        end
        pruefe_befehle(c['then'], "#{w} (Dann)")   if c['then'].is_a?(Array)
        pruefe_befehle(c['else'], "#{w} (Sonst)")  if c['else'].is_a?(Array)
        unless SELF_SWITCH_BUCHSTABEN.include?(c['letter'].to_s)
          melde(FEHLER, w, "'letter' muss A, B, C oder D sein")
        end
      when 'fade'
        if text?(c['direction']) && !FADE_RICHTUNGEN.include?(c['direction'].to_s)
          melde(WARNUNG, w, "unbekannte Richtung '#{c['direction']}' (ein/aus)")
        end
        if c.key?('seconds') && !(zahl?(c['seconds']) && c['seconds'] >= 0)
          melde(WARNUNG, w, "'seconds' muss eine Zahl >= 0 sein")
        end
      when 'move_route'
        # Etappe 19g: Bewegungsroute (Ziel = NPC-Name oder 'spieler').
        melde(FEHLER, w, "'target' fehlt oder ist leer") unless text?(c['target'])
        steps = c['steps']
        if !steps.is_a?(Array) || steps.empty?
          melde(FEHLER, w, "'steps' fehlt/leer")
        else
          steps.each_with_index do |s, si|
            name = s.to_s
            unless MOVER_SCHRITTE.include?(name) || MOVER_ALIASE.include?(name) ||
                   MOVER_XP_CODES.include?(name)
              melde(WARNUNG, "#{w} Schritt ##{si + 1}",
                    "unbekannter Schritt '#{s}' (XP-Namen/Codes oder " \
                    "#{MOVER_ALIASE.join('/')})")
            end
          end
        end
        if c.key?('speed') && !(zahl?(c['speed']) && c['speed'] > 0)
          melde(WARNUNG, w, "'speed' muss eine Zahl > 0 sein")
        end
        # 19h.2: Schritte mit Parameter brauchen 'code' (Schaltername,
        # Tempo, SE-Name, Ruby-Code) - sonst läuft der Schritt ins Leere.
        # F1: numerische XP-Codes vorher auflösen ('27' == switch_on),
        # sonst feuert die Warnung nur bei Namenstext.
        if steps.is_a?(Array) &&
           steps.any? { |s| MOVER_CODE_PFLICHT.include?(xp_schritt(s)) } &&
           !text?(c['code'])
          melde(WARNUNG, w, "Schritte #{MOVER_CODE_PFLICHT.join('/')} brauchen 'code'")
        end
      when 'input_number'
        # Etappe 19i: Zahleneingabe (XP Input Number Processing).
        melde(FEHLER, w, "'name' fehlt oder ist leer") unless text?(c['name'])
        if c.key?('digits') && !(zahl?(c['digits']) && (1..6).cover?(c['digits'].to_i))
          melde(WARNUNG, w, "'digits' muss 1..6 sein")
        end
        if c.key?('initial') && !(zahl?(c['initial']) && c['initial'] >= 0)
          melde(WARNUNG, w, "'initial' muss eine Zahl >= 0 sein")
        end
      when 'battle'
        # Etappe 19i: Kampf gegen eine Truppe aus database.json.
        melde(FEHLER, w, "'name' fehlt oder ist leer") unless text?(c['name'])
      when 'loop'
        # Etappe 20c: Schleife (XP 112 + 413) - der Körper ist 'commands'.
        koerper = c['commands']
        if !koerper.is_a?(Array)
          melde(FEHLER, w, "'commands' fehlt/keine Liste")
        else
          pruefe_befehle(koerper, "#{w} (Schleife)", schleife: schleife + 1)
          unless enthaelt_ausgang?(koerper)
            melde(WARNUNG, w,
                  'Schleife ohne break/jump/exit_event läuft endlos ' \
                  '(nur ein Seitenwechsel oder Event-Ende hält sie an)')
          end
        end
      when 'break'
        if schleife.zero?
          melde(WARNUNG, w, "'break' außerhalb einer Schleife hat keine Wirkung")
        end
      when 'label', 'jump'
        unless text?(c['name']) && !c['name'].to_s.strip.empty?
          melde(FEHLER, w, "'name' (Marke) fehlt oder ist leer")
        end
      when 'exit_event', 'erase_event'
        # Keine Felder: Beenden bzw. Löschen dieses Events (XP 115/116).
        nil
      when 'button_input'
        melde(FEHLER, w, "'name' fehlt oder ist leer") unless text?(c['name'])
        tasten = c['keys']
        if !tasten.is_a?(Array) || tasten.empty?
          melde(FEHLER, w, "'keys' fehlt/leer")
        else
          tasten.each_with_index do |t, ti|
            next if TASTEN.include?(t.to_s)
            melde(WARNUNG, "#{w} Taste ##{ti + 1}",
                  "unbekannte Taste '#{t}' (z. B. e, space, escape, up, f1)")
          end
        end
      when 'if_actor', 'if_button'
        # Etappe 20d: Bedingungsquellen Actor (XP 111) und Taste.
        %w[then else].each do |k|
          melde(WARNUNG, w, "'#{k}' fehlt/keine Liste") unless c[k].is_a?(Array)
        end
        pruefe_befehle(c['then'], "#{w} (Dann)")   if c['then'].is_a?(Array)
        pruefe_befehle(c['else'], "#{w} (Sonst)")  if c['else'].is_a?(Array)
        if !text?(c['name'])
          melde(FEHLER, w, "'name' fehlt oder ist leer")
        elsif typ == 'if_actor'
          namen = (Database.database['actors'] || []).filter_map do |h|
            h.is_a?(Hash) ? [h['id'].to_s, (h['name'] || h['id']).to_s] : nil
          end
          ziel = c['name'].to_s.strip
          bekannt = namen.any? { |id, name| ziel.casecmp?(id) || ziel.casecmp?(name) }
          melde(WARNUNG, w, "Actor '#{ziel}' nicht in database.json") unless bekannt
        end
      when 'if_gold', 'if_item', 'if_script'
        # Etappe 20d: weitere Bedingungsquellen (XP 111). Zweige wie bei den
        # übrigen Wenn-Befehlen prüfen (Pflicht + rekursiv).
        %w[then else].each do |k|
          melde(WARNUNG, w, "'#{k}' fehlt/keine Liste") unless c[k].is_a?(Array)
        end
        pruefe_befehle(c['then'], "#{w} (Dann)")   if c['then'].is_a?(Array)
        pruefe_befehle(c['else'], "#{w} (Sonst)")  if c['else'].is_a?(Array)
        if typ == 'if_script'
          melde(FEHLER, w, "'code' fehlt oder ist leer") unless text?(c['code'])
        else
          melde(WARNUNG, w, "unbekannter Vergleich '#{c['op']}'") unless VERGLEICH.include?(c['op'].to_s)
          melde(WARNUNG, w, "'value' fehlt oder ist keine Zahl") unless zahl?(c['value'])
          if typ == 'if_item'
            melde(FEHLER, w, "'name' fehlt oder ist leer") unless text?(c['name'])
            if c.key?('value') && zahl?(c['value']) && c['value'].negative?
              melde(WARNUNG, w, "'value' sollte nicht negativ sein (Item-Menge)")
            end
          end
        end
      when 'actor_hp', 'actor_sp', 'actor_exp', 'actor_level'
        # Etappe 20d: HP/SP/EXP/Stufe ändern (XP 311/312/315/316). Ein
        # vorzeichenbehaftetes Feld: positiv heilt/erhöht, negativ senkt.
        melde(FEHLER, w, "'amount' fehlt oder ist keine Zahl") unless zahl?(c['amount'])
        if c['amount'].is_a?(Numeric) && c['amount'].to_i.zero?
          melde(WARNUNG, w, "'amount' ist 0 - der Befehl bewirkt nichts")
        end
        ziel = c.key?('target') ? c['target'].to_s.strip : ''
        if !ziel.empty? && !ziel.casecmp?('alle')
          namen = (Database.database['actors'] || []).filter_map do |h|
            h.is_a?(Hash) ? [h['id'].to_s, (h['name'] || h['id']).to_s] : nil
          end
          bekannt = namen.any? { |id, name| ziel.casecmp?(id) || ziel.casecmp?(name) }
          melde(WARNUNG, w, "Actor '#{ziel}' nicht in database.json") unless bekannt
        end
        if typ == 'actor_level' && c['amount'].is_a?(Numeric) && c['amount'].to_i.abs > 99
          melde(WARNUNG, w, "'amount' übersteigt den Stufenbereich")
        end
      when 'screen_tone'
        # Etappe 20f (XP 223): Ton je Kanal -255..255, frames = Übergangsdauer.
        %w[r g b gray].each do |kanal|
          next unless c.key?(kanal)
          wert = c[kanal]
          if !zahl?(wert)
            melde(FEHLER, w, "'#{kanal}' ist keine Zahl")
          elsif !wert.to_i.between?(-255, 255)
            melde(FEHLER, w, "'#{kanal}' muss zwischen -255 und 255 liegen")
          end
        end
        if c.key?('frames') && !(zahl?(c['frames']) && c['frames'].to_i >= 0)
          melde(FEHLER, w, "'frames' muss eine Zahl >= 0 sein")
        end
        if %w[r g b gray].none? { |k| c.key?(k) } && !c.key?('frames')
          melde(WARNUNG, w, 'screen_tone ohne Werte ändert nichts')
        end
      when 'flash'
        # Etappe 20f (XP 224): Farbe, Stärke 0..255, Dauer in Frames.
        if c.key?('color') && !FLASH_FARBEN.include?(c['color'].to_s.strip.downcase)
          melde(FEHLER, w, "unbekannte Blitzfarbe '#{c['color']}' " \
                           "(#{FLASH_FARBEN.join('/')})")
        end
        if c.key?('strength') &&
           !(zahl?(c['strength']) && c['strength'].to_i.between?(0, 255))
          melde(FEHLER, w, "'strength' muss eine Zahl von 0 bis 255 sein")
        end
        if c.key?('frames') && !(zahl?(c['frames']) && c['frames'].to_i >= 0)
          melde(FEHLER, w, "'frames' muss eine Zahl >= 0 sein")
        end
      when 'name_input'
        # Etappe 20e (XP 303): Held umbenennen. `actor` ist Pflicht,
        # `max` 1..16 (XP-Default 8), `cancel` = Esc erlaubt (Default an).
        ziel = c['actor'].to_s.strip
        if ziel.empty?
          melde(FEHLER, w, "'actor' fehlt oder ist leer")
        else
          namen = (Database.database['actors'] || []).filter_map do |h|
            h.is_a?(Hash) ? [h['id'].to_s, (h['name'] || h['id']).to_s] : nil
          end
          bekannt = namen.any? { |id, name| ziel.casecmp?(id) || ziel.casecmp?(name) }
          melde(WARNUNG, w, "Actor '#{ziel}' nicht in database.json") unless bekannt
        end
        unless !c.key?('max') || (zahl?(c['max']) && c['max'].to_i.between?(1, 16))
          melde(FEHLER, w, "'max' muss eine Zahl von 1 bis 16 sein")
        end
        if c.key?('cancel') && ![true, false].include?(c['cancel'])
          melde(WARNUNG, w, "'cancel' sollte true oder false sein")
        end
      when 'shop'
        # Etappe 20e (XP 302): Warenliste. Jede Ware ist ein Objekt
        # {"item": "<id>", "price": <zahl>}; `price` fehlt = Datenbankpreis.
        waren = c['goods']
        if !waren.is_a?(Array) || waren.empty?
          melde(FEHLER, w, "'goods' fehlt oder ist leer")
        else
          bekannt = (Database.database['items'] || []).filter_map do |h|
            h.is_a?(Hash) ? [h['id'].to_s, (h['name'] || h['id']).to_s] : nil
          end
          waren.each_with_index do |ware, wi|
            ort = "#{w} Ware ##{wi + 1}"
            if !ware.is_a?(Hash)
              melde(FEHLER, ort, 'Ware ist kein Objekt')
              next
            end
            id = ware['item'].to_s.strip
            if id.empty?
              melde(FEHLER, ort, "'item' fehlt oder ist leer")
            else
              treffer = bekannt.find { |iid, name| id.casecmp?(iid) || id.casecmp?(name) }
              melde(WARNUNG, ort, "Item '#{id}' nicht in database.json") if treffer.nil?
            end
            next unless ware.key?('price')
            preis = ware['price']
            if !zahl?(preis)
              melde(FEHLER, ort, "'price' ist keine Zahl")
            elsif preis.negative?
              melde(FEHLER, ort, "'price' darf nicht negativ sein")
            end
          end
          ids = waren.filter_map { |ware| ware.is_a?(Hash) ? ware['item'].to_s.strip.downcase : nil }
          doppelt = ids.tally.select { |_, n| n > 1 }.keys
          melde(WARNUNG, w, "Ware doppelt im Angebot: #{doppelt.join(', ')}") unless doppelt.empty?
        end
      when 'party_member'
        # Etappe 20d: Mitglied aufnehmen/entfernen (XP 129). `name` ist der
        # Actor (ID oder Name aus database.json), `mode` add/remove.
        modus = c.key?('mode') ? c['mode'].to_s.strip.downcase : 'add'
        modus = 'add' if modus.empty?
        unless PARTY_MODI.include?(modus)
          melde(FEHLER, w, "unbekannter party_member-Modus '#{modus}' (add/remove)")
        end
        ziel = c['name'].to_s.strip
        if ziel.empty?
          melde(FEHLER, w, "'name' fehlt oder ist leer")
        else
          namen = (Database.database['actors'] || []).filter_map do |h|
            h.is_a?(Hash) ? [h['id'].to_s, (h['name'] || h['id']).to_s] : nil
          end
          bekannt = namen.any? { |id, name| ziel.casecmp?(id) || ziel.casecmp?(name) }
          melde(WARNUNG, w, "Actor '#{ziel}' nicht in database.json") unless bekannt
        end
      when 'timer'
        # Etappe 20d: Timer steuern (XP 124).
        modus = c.key?('mode') ? c['mode'].to_s : 'start'
        unless TIMER_MODI.include?(modus)
          melde(FEHLER, w, "unbekannter timer-Modus '#{modus}' (start/stop)")
        end
        if c.key?('seconds') && !(zahl?(c['seconds']) && c['seconds'] >= 0)
          melde(WARNUNG, w, "'seconds' muss eine Zahl >= 0 sein")
        end
        if modus == 'start' && !(zahl?(c['seconds']) && c['seconds'] > 0)
          melde(WARNUNG, w, "timer start ohne Sekunden hat keine Wirkung")
        end
      when 'save_access', 'menu_access', 'encounter_access'
        # Etappe 20d: XP 134/135/136 - Speichern/Menü/Zufallskämpfe sperren.
        if c.key?('allowed') && ![true, false].include?(c['allowed'])
          melde(WARNUNG, w, "'allowed' muss true oder false sein")
        end
      when 'common_event'
        # Etappe 20c: Gemeinsames Event aufrufen (XP 117).
        name = c['name']
        if !text?(name)
          melde(FEHLER, w, "'name' fehlt oder ist leer")
        elsif @common_events && !@common_events.include?(name.to_s)
          melde(WARNUNG, w, "gemeinsames Event '#{name}' ist nicht definiert")
        end
      when 'text_options'
        # Etappe 20c: Fenster-Optionen (XP 104).
        if c.key?('position') && !FENSTER_POSITIONEN.include?(c['position'].to_s)
          melde(WARNUNG, w,
                "'position' muss oben/mitte/unten sein (nicht '#{c['position']}')")
        end
        %w[frame transparent].each do |k|
          next unless c.key?(k)
          melde(WARNUNG, w, "'#{k}' muss true oder false sein") unless [true, false].include?(c[k])
        end
      end
    end

    # Etappe 20c: Verlässt irgendein Befehl die Schleife? Sprünge und
    # Event-Enden zählen mit; ein break in einer INNEREN Schleife nicht.
    def enthaelt_ausgang?(liste)
      return false unless liste.is_a?(Array)

      liste.any? do |c|
        next false unless c.is_a?(Hash)

        case c['cmd']
        when 'break', 'jump', 'exit_event' then true
        when 'loop' then false
        else
          %w[then else cancel_branch].any? { |k| enthaelt_ausgang?(c[k]) } ||
            (c['branches'].is_a?(Array) && c['branches'].any? { |z| enthaelt_ausgang?(z) })
        end
      end
    end

    # Teleport/Bewegung kann eine Zielkarte nennen (Kartenwechsel); wenn eine
    # Kartenliste bekannt ist, muss das Ziel existieren.
    def pruefe_kartenziel(c, w)
      ziel = c['map'] || c['to']
      return unless ziel
      return unless @maps

      melde(FEHLER, w, "Zielkarte '#{ziel}' existiert nicht") unless @maps.include?(ziel.to_s)
    end

    def pruefe_npcs(liste)
      return if liste.nil?

      wo = "#{@karte_name} npcs"
      unless liste.is_a?(Array)
        melde(FEHLER, wo, 'ist keine Liste')
        return
      end

      liste.each_with_index do |n, i|
        w = "#{wo} ##{i + 1}"
        unless n.is_a?(Hash)
          melde(FEHLER, w, 'ist kein Objekt')
          next
        end
        w += " ('#{n['name']}')" if text?(n['name'])
        melde(WARNUNG, w, "'name' fehlt oder ist leer") unless text?(n['name'])
        %w[x z].each do |k|
          melde(FEHLER, w, "'#{k}' fehlt oder ist keine Zahl") unless zahl?(n[k])
        end
        pruefe_y(n, w)
        melde(WARNUNG, w, 'steht in einer Block-Zone (kommt nicht heraus)') if in_zone?(n['x'], n['z'])
        if n.key?('speed') && !(zahl?(n['speed']) && n['speed'] > 0)
          melde(WARNUNG, w, "'speed' muss > 0 sein")
        end
        if n.key?('radius') && !(zahl?(n['radius']) && n['radius'] > 0)
          melde(WARNUNG, w, "'radius' muss > 0 sein")
        end
        pruefe_wegpunkte(n['path'], w)
        pruefe_befehle(n['commands'], w) if n.key?('commands')
      end
    end

    def pruefe_wegpunkte(path, wo)
      return if path.nil?

      w = "#{wo} path"
      unless path.is_a?(Array)
        melde(FEHLER, w, 'ist keine Liste')
        return
      end

      path.each_with_index do |p, i|
        pw = "#{w} ##{i + 1}"
        unless p.is_a?(Array) && p.size == 2 && p.all? { |v| zahl?(v) }
          melde(FEHLER, pw, 'muss ein Paar [x, z] aus Zahlen sein')
          next
        end
        # Etappe 19k.3: Der NPC läuft bis an die Zone heran und wartet dann -
        # ein Wegpunkt in der Wand ist fast immer ein Versehen.
        melde(WARNUNG, pw, 'liegt in einer Block-Zone') if in_zone?(p[0], p[1])
      end
      melde(WARNUNG, w, 'nur ein Wegpunkt (NPC läuft nirgends hin)') if path.size == 1
    end

    def pruefe_atmosphaere(map)
      unless (fog = map['fog']).nil?
        wo = "#{@karte_name} fog"
        unless fog.is_a?(Hash)
          melde(FEHLER, wo, 'ist kein Objekt')
        else
          melde(WARNUNG, wo, "'on' ist kein Bool") if fog.key?('on') && ![true, false].include?(fog['on'])
          if fog.key?('far') && !fog['far'].nil? && !terrain_bereich?(fog['far'], FOG_MIN_FAR, FOG_MAX_FAR)
            melde(WARNUNG, wo, "'far' muss zwischen 0.1 und 100000 liegen; Ersatz 180 m")
          end
          pruefe_farbe(fog['color'], wo)
        end
      end

      unless (pano = map['panorama']).nil?
        wo = "#{@karte_name} panorama"
        if pano.is_a?(Hash)
          pruefe_farbe(pano['color'], wo)
        else
          melde(FEHLER, wo, 'ist kein Objekt')
        end
      end

      unless (wet = map['weather']).nil?
        wo = "#{@karte_name} weather"
        unless wet.is_a?(Hash)
          melde(FEHLER, wo, 'ist kein Objekt')
        else
          unless WETTER.include?(wet['type'].to_s)
            melde(WARNUNG, wo, "unbekannter Typ '#{wet['type']}' (kein/regen/schnee)")
          end
          if wet.key?('intensity') && !wet['intensity'].nil? &&
             !(terrain_bereich?(wet['intensity'], 0, WEATHER_MAX_INTENSITY) && wet['intensity'] > 0)
            melde(WARNUNG, wo, "'intensity' muss > 0 und <= 10 sein; Ersatz 1")
          end
        end
      end
    end

    def pruefe_farbe(wert, wo)
      return if wert.nil?
      return if terrain_bereich?(wert, 0, 0xFFFFFF)

      melde(WARNUNG, wo, "'color' muss 0x000000..0xFFFFFF sein (ist #{wert.inspect})")
    end
  end
end

# --------------------------------------------------------------------- CLI
if $PROGRAM_NAME == __FILE__
  $LOAD_PATH.unshift(File.expand_path(__dir__)) unless $LOAD_PATH.include?(File.expand_path(__dir__))
  require 'prefabs'

  wurzel = File.expand_path('../..', __dir__)
  ziele = ARGV.empty? ? [File.join(wurzel, 'game', 'data', 'maps'),
                         File.join(wurzel, 'projekte')] : ARGV

  # Etappe 20c: Datenbanken neben den Karten mitprüfen (Gemeinsame Events)
  # und ihre Namen an die Karten-Prüfung geben - so fällt ein Tippfehler im
  # Befehl "Gemeinsames Event" schon hier auf.
  datenbanken = ziele.flat_map do |ziel|
    basis = File.directory?(ziel) ? ziel : File.dirname(File.expand_path(ziel))
    Dir.glob(File.join(basis, '**', 'database.json'))
  end.uniq.sort
  common_namen = []
  datenbank_funde = {}
  datenbanken.each do |pfad|
    db = begin
      JSON.parse(File.read(pfad, encoding: 'utf-8'))
    rescue StandardError => e
      datenbank_funde[pfad] = [MapCheck::Finding.new(MapCheck::FEHLER, File.basename(pfad),
                                                     "nicht lesbar: #{e.message}")]
      next
    end
    common_namen.concat(Array(db['common_events']).filter_map { |e| e['name'].to_s if e.is_a?(Hash) })
    funde = MapCheck.scan_database(db, name: File.basename(pfad))
    datenbank_funde[pfad] = funde unless funde.empty?
  end

  alle = MapCheck.scan_tree(*ziele, prefabs: Prefabs.ids, common_events: common_namen.uniq)

  fehler = 0
  alle.merge(datenbank_funde).sort.each do |pfad, funde|
    next if funde.empty?

    puts File.basename(pfad)
    funde.each { |f| puts "  #{f}" }
    fehler += funde.count(&:fehler?)
  end
  gesamt = alle.values.flatten + datenbank_funde.values.flatten
  puts(gesamt.empty? ? 'Alle Karten sauber.' : "Insgesamt: #{gesamt.size} Funde")
  exit(fehler.zero? ? 0 : 1)
end
