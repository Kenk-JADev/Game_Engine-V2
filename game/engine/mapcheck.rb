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

  Finding = Struct.new(:severity, :where, :message) do
    def fehler? = severity == FEHLER
    def warnung? = severity == WARNUNG

    def to_s
      "#{fehler? ? '[Fehler]  ' : '[Warnung] '}#{where}: #{message}"
    end
  end

  # Befehlstypen des Interpreters. Muss zu game/engine/interpreter.rb und zu
  # EV_TYPES / ISSUE_CMD_TYPES in src/editor/editor_main.cpp passen.
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
  ].freeze

  TRIGGER   = %w[action touch auto parallel].freeze
  WETTER    = %w[kein regen schnee].freeze
  EFFEKTE   = %w[sparkle dust burst].freeze
  VERGLEICH = %w[== != < <= > >=].freeze
  BEDINGUNGEN = %w[switch variable self_switch].freeze
  SELF_SWITCH_BUCHSTABEN = %w[A B C D].freeze     # Etappe 19d (XP: A-D)
  FADE_RICHTUNGEN = %w[ein aus].freeze            # Etappe 19d
  # Etappe 19g, XP-Vokabular seit 19h.2: muss zu Mover::SCHRITTE passen
  # (move_route_test prüft das - mapcheck.rb wird früher geladen als
  # mover.rb, deshalb keine Referenz). Deutsche Kurznamen (up/down/...) und
  # XP-Codes ('11'..'57') versteht der Mover zusätzlich als Aliase.
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
    always_on_top_on always_on_top_off
    change_graphic change_opacity change_blending script
  ].freeze

  # Deutsche Kurznamen aus Etappe 19g (Mover::ALIASE) bleiben gültig; die
  # XP-Befehls-Codes ('11'..'57') versteht der Mover ebenfalls - beide
  # Alias-Formen sind hier bewusst nicht doppelt gepflegt: Der Sync der
  # Hauptliste wird in move_route_test.rb gegen Mover::SCHRITTE assertet,
  # die Aliase gegen Mover::ALIASES.keys.
  MOVER_ALIASE = %w[
    down left right up zufall spieler_nah spieler_weg warte
    vor zurueck runter rauf
  ].freeze

  # XP-Befehls-Codes (Mover::XP_CODES) - eigene Liste aus demselben Grund.
  MOVER_XP_CODES = %w[
    11 12 13 14 15 16 17 18 19 20 21 22 23 24 25
    26 27 28 29 30 31 32 33 34 35 36 37 38 39 40
    41 42 43 44 45 46 47 48 49 50 51 52 53 57
  ].freeze

  # Schritte, die den Route-Parameter 'code' brauchen (19h.2).
  MOVER_CODE_PFLICHT = %w[switch_on switch_off change_speed script].freeze
  SPEICHER_SLOTS = (1..3).freeze

  module_function

  # ------------------------------------------------------------------ Einstieg
  # Karte (Hash aus JSON.parse) prüfen.
  #   name:      Anzeigename für die Fundstelle ("dorf.json")
  #   prefabs:   bekannte Objekttypen (Standard: Prefabs.ids, falls geladen)
  #   materials: bekannte Materialnamen (optional, sonst keine Tex-Prüfung)
  #   scripts:   bekannte Script-Namen (optional)
  #   maps:      bekannte Karten-Ids für Teleport-Ziele (optional)
  def scan(map, name: 'karte', prefabs: default_prefabs, materials: nil,
           scripts: nil, maps: nil)
    f = Sammler.new(name, prefabs: prefabs, materials: materials,
                          scripts: scripts, maps: maps)
    f.karte(map)
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
    map_files(*roots).map { |p| File.basename(p, '.json') }.sort
  end

  # Nur JSON-Dateien aus einem "maps"-Ordner zählen als Karten (database.json,
  # system.json, scripts.json und Spielstände gehören nicht dazu). Einzelne
  # Dateien, die man direkt übergibt, werden immer geprüft.
  def map_files(*roots)
    roots.flatten.flat_map do |root|
      next [root] if File.file?(root)
      next [] unless File.directory?(root)

      Dir.glob(File.join(root, '**', '*.json')).select do |pfad|
        ordner = File.dirname(pfad).split(%r{[/\\]})
        (ordner & %w[maps map]).any? && !ordner.include?('saves')
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

    def initialize(name, prefabs: nil, materials: nil, scripts: nil, maps: nil)
      @karte_name = name
      @prefabs    = prefabs
      @materials  = materials
      @scripts    = scripts
      @maps       = maps
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

    private

    def melde(severity, where, message)
      @liste << Finding.new(severity, where, message)
    end

    def zahl?(v) = v.is_a?(Numeric)
    def text?(v) = v.is_a?(String) && !v.strip.empty?

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
      if t.key?('resolution') && !(zahl?(t['resolution']) && t['resolution'].to_i > 1)
        melde(WARNUNG, wo, "'resolution' muss > 1 sein")
      end
      if t.key?('amplitude') && !zahl?(t['amplitude'])
        melde(WARNUNG, wo, "'amplitude' ist keine Zahl")
      end
      if t.key?('tile') && !(zahl?(t['tile']) && t['tile'] > 0)
        melde(WARNUNG, wo, "'tile' muss > 0 sein")
      end

      pruefe_heights(t['heights'])
      pruefe_splats(t['splats'])
    end

    # Gemalte Berge/Senken (Etappe 18l): [{x,z,r,h}]
    def pruefe_heights(liste)
      return if liste.nil?

      wo = "#{@karte_name} terrain.heights"
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
        %w[x z r h].each do |k|
          melde(FEHLER, w, "'#{k}' fehlt oder ist keine Zahl") unless zahl?(b[k])
        end
        melde(WARNUNG, w, "'r' (Radius) muss > 0 sein") if zahl?(b['r']) && b['r'] <= 0
        melde(WARNUNG, w, "'h' ist 0 (Bürste ohne Wirkung)") if zahl?(b['h']) && b['h'].zero?
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

      liste.each_with_index do |s, i|
        w = "#{wo} ##{i + 1}"
        unless s.is_a?(Hash)
          melde(FEHLER, w, 'ist kein Objekt')
          next
        end
        unless text?(s['tex'])
          melde(FEHLER, w, "ohne Material ('tex')")
        end
        %w[x z w h].each do |k|
          melde(FEHLER, w, "'#{k}' fehlt oder ist keine Zahl") unless zahl?(s[k])
        end
        %w[w h].each do |k|
          melde(WARNUNG, w, "'#{k}' muss > 0 sein") if zahl?(s[k]) && s[k] <= 0
        end
        next unless @materials && text?(s['tex']) && !@materials.include?(s['tex'])

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
    def pruefe_befehle(liste, wo)
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
        pruefe_befehl(typ.to_s, c, w)
      end
    end

    def pruefe_befehl(typ, c, w)
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
        if typ == 'variable' && !c.key?('value') && !c.key?('add')
          melde(WARNUNG, w, "weder 'value' noch 'add' (Befehl ohne Wirkung)")
        end
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
        # Tempo, Ruby-Code) - sonst läuft der Schritt ins Leere.
        if steps.is_a?(Array) &&
           steps.any? { |s| MOVER_CODE_PFLICHT.include?(s.to_s) } &&
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
      if (fog = map['fog'])
        wo = "#{@karte_name} fog"
        unless fog.is_a?(Hash)
          melde(FEHLER, wo, 'ist kein Objekt')
        else
          melde(WARNUNG, wo, "'on' ist kein Bool") if fog.key?('on') && ![true, false].include?(fog['on'])
          melde(WARNUNG, wo, "'far' muss > 0 sein") if fog.key?('far') && !(zahl?(fog['far']) && fog['far'] > 0)
          pruefe_farbe(fog['color'], wo)
        end
      end

      if (pano = map['panorama'])
        wo = "#{@karte_name} panorama"
        if pano.is_a?(Hash)
          pruefe_farbe(pano['color'], wo)
        else
          melde(FEHLER, wo, 'ist kein Objekt')
        end
      end

      if (wet = map['weather'])
        wo = "#{@karte_name} weather"
        unless wet.is_a?(Hash)
          melde(FEHLER, wo, 'ist kein Objekt')
        else
          unless WETTER.include?(wet['type'].to_s)
            melde(WARNUNG, wo, "unbekannter Typ '#{wet['type']}' (kein/regen/schnee)")
          end
          if wet.key?('intensity') && !(zahl?(wet['intensity']) && wet['intensity'] > 0)
            melde(WARNUNG, wo, "'intensity' muss > 0 sein")
          end
        end
      end
    end

    def pruefe_farbe(wert, wo)
      return unless wert
      return if zahl?(wert) && wert.to_i >= 0 && wert.to_i <= 0xFFFFFF

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
  alle = MapCheck.scan_tree(*ziele, prefabs: Prefabs.ids)

  fehler = 0
  alle.sort.each do |pfad, funde|
    next if funde.empty?

    puts File.basename(pfad)
    funde.each { |f| puts "  #{f}" }
    fehler += funde.count(&:fehler?)
  end
  puts(alle.values.flatten.empty? ? 'Alle Karten sauber.' : "Insgesamt: #{alle.values.flatten.size} Funde")
  exit(fehler.zero? ? 0 : 1)
end
