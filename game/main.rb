#encoding: utf-8
# ============================================================================
#  Rune3D - Beispielszene ("Dorf") mit Event-Interpreter.
#  F5 lädt diese Datei neu, ohne die Engine zu beenden.
# ============================================================================

Sounds.register_all   # prozedurale Sounds an die Engine melden (idempotent)

module Game
  # ---------------------------------------------------------------- Aufbau

  # Hilfsfunktion: Geländehöhe an einer Stelle (0.0, solange kein Terrain da ist).
  def self.gy(x, z)
    Rune::World.ready? ? Rune::World.height_at(x, z) : 0.0
  end

# Die ganze Szene kommt aus der Karte: Prefab-Objekte (game/data/maps/*.json,
# Feld "objects") plus Events. Nichts ist mehr fest im Code verdrahtet.
def self.build
  Scene.new(map['name'] || 'Dorf')
end

  # ------------------------------------------------------------ Karten-Daten

  # Aktuelle Karte aus dem Projekt (mit Cache).
  def self.map
    @map
  end

  # Aktuelle Karten-ID (Etappe 19d: Teil des Self-Switch-Schlüssels).
  def self.map_id
    @map_id
  end

  # Events/NPCs der aktuellen Karte (Etappe 19e: RGSS-Wrapper $game_map liest
  # sie, z.B. um Self-Switch-Schlüssel per Event-ID aufzulösen).
  def self.events
    @events || []
  end

  # Etappe 20c: Darstellungs-Nodes je Event (Index wie @events) - erase_event
  # blendet damit genau das gelöschte Event aus. Leer vor dem ersten enter_map.
  def self.event_visuals
    @event_visuals || []
  end

  def self.npcs
    @npcs || []
  end

  # Nach einem Terrain-Neubau durch ein Spielscript können statische
  # Schilder und NPCs der neuen Bodenhöhe folgen.
  def self.refresh_placement_visuals(scene = @scene)
    return unless scene && scene.respond_to?(:root)

    # Nur selbst erzeugte Nodes ersetzen; ein Script darf einen beliebigen
    # Namen verwenden und soll dabei nicht versehentlich Objekte verlieren.
    old_nodes = (@placement_visuals || []).each_with_object({}) { |node, set| set[node] = true }
    scene.root.children.reject! { |node| old_nodes.key?(node) }
    # Etappe 20c: Nodes je Event merken, damit erase_event genau die
    # Darstellung DIESES Events ausblenden kann.
    @event_visuals = (@events || []).each_with_index.map { |event, i| event.add_visual(scene, i) }
    @placement_visuals = @event_visuals.flatten(1)
    (@npcs || []).each(&:refresh_terrain_height)
  end

  # Laufzeit-Zugriff für die RGSS-Schicht (19h.2: $game_system.map_interpreter)
  # und für Tests.
  def self.interpreter
    @interpreter
  end

  def self.time
    @time || 0.0
  end

  # Himmel/Nebel/Panorama aus der Karte übernehmen (Etappe 17b).
  # Panorama-Farbe ersetzt den Himmelverlauf (Vollfarbe), Nebel blendet
  # die Ferne zur Nebelfarbe. Ohne Angaben: Standard-Himmel, kein Nebel.
  def self.apply_map_atmosphere(map)
    pan = map['panorama']
    Rune::Log.warn('panorama ist kein Objekt; Standardhimmel') if !pan.nil? && !pan.is_a?(Hash)
    pan = {} unless pan.is_a?(Hash)
    unless pan['color'].nil?
      c = pan['color']
      Rune::Gfx.sky(c, c)
    else
      Rune::Gfx.sky(0x3D6FB5, 0xBFD8EC)
    end
    fog = map['fog']
    Rune::Log.warn('fog ist kein Objekt; Nebel aus') if !fog.nil? && !fog.is_a?(Hash)
    fog = {} unless fog.is_a?(Hash)
    unless fog['on'].nil? || fog['on'] == true || fog['on'] == false
      Rune::Log.warn('fog.on ist kein Bool; Nebel aus')
    end
    Rune::Gfx.fog(fog['on'] == true, fog['color'].nil? ? 0xBFD8EC : fog['color'],
                  fog['far'].nil? ? 180.0 : fog['far'])
  end

  # Etappe 18l: Terrain einer Karte aufbauen (Grösse/Seed/Amplitude plus
  # gemalte Berge/Senken aus terrain.heights) und den Boden texteiren.
  # Wird vom Setup und von jedem Kartenwechsel (enter_map) gerufen - damit
  # Kartengrenze und gemalte Hügel auch nach dem Wechsel stimmen.
  def self.build_terrain(map)
    t = map['terrain']
    if !t.nil? && !t.is_a?(Hash)
      Rune::Log.warn('terrain ist kein Objekt; kein Gelände')
      Rune::World.generate(width: 0) # vorhandenes Terrain ausdrücklich verwerfen
      return false
    end
    t ||= {}
    value = ->(key, fallback) { t[key].nil? ? fallback : t[key] }
    size = value.call('size', 240)
    ok = Rune::World.generate(width: value.call('width', size),
                              height: value.call('height', size),
                              resolution: value.call('resolution', 192),
                              seed: value.call('seed', 7), amplitude: value.call('amplitude', 0.0),
                              height_brushes: t['heights'])
    return false unless ok

    # Zuerst die Basistextur setzen: Wenn kein Atlas möglich ist, bleibt ein
    # erkennbarer Einzeltexturboden statt der Textur der vorherigen Karte.
    base_texture = value.call('texture', 'gras')
    Rune::Gfx.terrain_texture(base_texture, value.call('tile', 2.0))
    t_splats = t['splats']
    if !t_splats.nil? && (!t_splats.is_a?(Array) || !t_splats.empty?)
      Rune::Gfx.terrain_splats(base_texture, t_splats)
      # Bei ungültigen Flächen löscht das Binding das Terrain; ohne Atlas
      # bleibt hingegen der bereits aufgebaute Einzeltexturboden erhalten.
      return false unless Rune::World.ready?
    end
    true
  end

  # Teleports sind keine gelaufenen Schritte. Die nächste Position wird neu
  # als Referenz aufgenommen; auch ein vorher angesammelter Teil-Schritt
  # darf weder einen Sound noch einen Zufallskampf auslösen.
  # W (Runde 26): XP zählt einen Schritt pro Kachel. Rune3D zählte bisher
  # jeden Schritt-Sound (0,60 m, rennend 0,85 m) als Begegnungsschritt -
  # dadurch kamen Zufallskämpfe rund 40 % zu häufig. Eine Kachel entspricht
  # hier KACHEL_METER Metern; der Schritt-Sound bleibt bei 0,60/0,85 m.
  KACHEL_METER = 1.0

  def self.reset_step_tracking
    @last_pos = nil
    @step_dist = 0.0
    @step_event = nil
    @kachel_dist = 0.0
    @kachel_schritt = nil
    @was_air = nil
  end

  # Eine explizit im Editor gesetzte Projekt-Startposition gilt beim Neustart
  # und F4 nur auf dieser Karte. Alte Projekte behalten die frühere Priorität
  # des Karten-Starts; Transfers und Spielstände bleiben davon unberührt.
  def self.project_start_for(map_id)
    start = Database.system['start']
    return nil unless start.is_a?(Hash) && start['use_project_position'] == true && start['map'] == map_id
    return nil unless %w[x z].all? { |key| start[key].is_a?(Numeric) && start[key].to_f.finite? }
    return nil unless start['yaw'].nil? || (start['yaw'].is_a?(Numeric) && start['yaw'].to_f.finite?)

    start
  end

  # Karte mit Terrain, Objekten, Events und NPCs aufbauen und den Spieler
  # an ihren Startpunkt setzen; auch beim Laden eines Spielstands verwendet.
  def self.enter_map(id, loaded_map: nil, start_override: nil)
    @map_id = id
    @map = loaded_map || Database.map(id)
    reset_step_tracking
    Fade.reset!   # Etappe 19d: nie mit dunklem Bildschirm in eine Karte kommen
    Mover.reset!  # Etappe 19g: Routen sind flüchtig (Ziel-NPCs sind neu)
    RGSS.reset!   # Etappe 19h: Sprites/Fenster/$scene sind flüchtig
    Battle.abort! # Etappe 19i: Kämpfe sind flüchtig (Kartenwechsel)
    @events = (@map['events'] || []).map { |h| Event.new(h) }
    @map['objects'] ||= []
    # Etappe 18l: Terrain gehört zur Karte - beim Wechsel neu aufbauen,
    # damit Grösse (Kartengrenze!) und gemalte Berge zur Karte passen.
    build_terrain(@map)
    apply_map_atmosphere(@map)

    # Etappe 19k.2: Block-Zonen aus dem Karten-JSON in die Physik
    # (unsichtbare Wände über alle Höhen; RGSS passable? liest dieselben
    # Rechtecke über Game.map['blocked']).
    if Rune::Physics.respond_to?(:clear_blocked)
      Rune::Physics.clear_blocked
      zonen = 0
      Array(@map['blocked']).each do |b|
        next unless b.is_a?(Hash)

        Rune::Physics.add_blocked(b['x'].to_f, b['z'].to_f, b['w'].to_f, b['h'].to_f)
        zonen += 1
      end
      Rune::Log.info("Karte: #{zonen} Block-Zone(n)") if zonen.positive?
    end

    @interpreter = Interpreter.new
    # Etappe 19l: Zufallskampf-Zähler der neuen Karte würfeln (XP
    # Game_Player#refresh beim Transfer).
    $game_player.make_encounter_count if $game_player
    @scene = build
    # Schilder sind normale Events mit optionaler visueller Darstellung.
    @event_visuals = @events.each_with_index.map { |event, i| event.add_visual(@scene, i) }
    @placement_visuals = @event_visuals.flatten(1)
    @scene.refresh_solids
    @npcs = (@map['npcs'] || []).map { |h| Npc.new(h, @scene) }
    Effects.clear
    Weather.load_map(@map)   # Etappe 17h: Wetter der Karte aktivieren

    # Etappe 19a: Kartendaten prüfen. Der Editor scannt dieselben Regeln in
    # C++ (issues_scan_map in editor_main.cpp), hier landen nur harte Fehler im
    # Laufzeit-Backlog (log/issues.json) - Warnungen bleiben Editor und CI.
    # Der defined?-Schutz hält main.rb lauffähig, falls das Modul fehlt.
    if defined?(MapCheck) && defined?(Issues)
      funde = MapCheck.scan(@map, name: id.to_s).select(&:fehler?)
      MapCheck.report!(funde) unless funde.empty?
    end

    @map_objects = MapObjects.new(@scene, height_at: method(:gy))
    @map_objects.rebuild(@map['objects'])
    @hover = nil
    @hover_prev = nil
    @hover_dist = 0.0
    @prompt = nil

    # Projektvorgabe nur bei explizitem Spielstart; sonst wie bisher Karte,
    # dann system.json. Laden eines Spielstands setzt seine Position danach.
    st = start_override || @map['start'] || Database.system['start'] || { 'x' => 0.0, 'z' => 0.0 }
    Rune::Player.teleport(st['x'].to_f, gy(st['x'].to_f, st['z'].to_f) + 0.2, st['z'].to_f)
    Rune::Camera.yaw = (st['yaw'] || Math::PI).to_f
    # Etappe 19k.3: Ein Startpunkt in der Block-Zone wäre ein feststeckender
    # Spieler - MapCheck und der Editor warnen, hier zusätzlich zur Laufzeit.
    if defined?(Passage) && Passage.zone?(st['x'].to_f, st['z'].to_f)
      Rune::Log.warn('Startpunkt liegt in einer Block-Zone (Spieler steckt fest)')
    end
    Rune::Log.info("Karte '#{@map['name']}' aktiv: #{@events.size} Events, " \
                   "#{@map['objects'].size} Objekte, #{Rune::Physics.solid_count} Hindernisse")
  end

  # Öffentlicher Kartenwechsel für Spielscripts; kein Editorzustand wird
  # automatisch gespeichert.
  def self.change_map(id)
    enter_map(id)
  end

  # ---------------------------------------------------- Speichern / Laden

  # Spielstand in Slot 1..3 ablegen: Karte, Position, Blick, Kamera,
  # Schalter/Variablen, ausgelöste Events, Spielzeit.
  def self.save_game(slot)
    if $game_system && $game_system.save_disabled
      Rune::Log.warn('Speichern gesperrt ($game_system.save_disabled)')
      return false
    end
    # XP Scene_Save: der Save-Zähler zählt den anstehenden Schreibvorgang
    # und wird mitgeschrieben (J/$game_system im Savegame).
    $game_system.save_count = $game_system.save_count.to_i + 1 if $game_system
    px, py, pz = Rune::Player.position
    data = {
      'map'       => @map_id,
      'player'    => { 'x' => px, 'y' => py, 'z' => pz,
                       'yaw' => Rune::Camera.yaw, 'mode' => Rune::Camera.mode.to_s },
      'state'     => GameState.snapshot,
      'party'     => Party.snapshot,
      'system'    => system_snapshot,
      'fired'     => @events.select(&:fired).map(&:name),
      'npcs'      => @npcs.map(&:snapshot),
      'time'      => @time,
      'saved'     => Time.now.strftime('%Y-%m-%d %H:%M'),
    }
    # Z8: Project.save schreibt atomar und meldet Fehler, statt einen halben
    # Spielstand zu hinterlassen - ein fehlgeschlagener Schreibvorgang ist
    # KEIN gespeicherter Spielstand.
    unless Project.save($project_name, slot, data)
      Rune::Log.error("Speichern fehlgeschlagen: Slot #{slot} nicht schreibbar")
      return false
    end
    Rune::Log.info("Gespeichert: Slot #{slot} (Karte '#{@map['name']}', " \
                   "#{data['fired'].size} Events ausgelöst)")
    true
  rescue StandardError => e
    Rune::Log.error("Speichern fehlgeschlagen: #{e.message}")
    false
  end

  # $game_system-Zustand im Spielstand (J/XP $game_system): Sperr-Flags,
  # Timer und Save-Zähler. Alte Spielstände ohne 'system' behalten die
  # Default-Werte - system_restore! ist dafür bewusst ein Noop.
  def self.system_snapshot
    return nil unless $game_system
    { 'menu_disabled'      => !!$game_system.menu_disabled,
      'save_disabled'      => !!$game_system.save_disabled,
      'encounter_disabled' => !!$game_system.encounter_disabled,
      'timer'              => $game_system.timer.to_f,
      'timer_working'      => !!$game_system.timer_working,
      'save_count'         => $game_system.save_count.to_i }
  end

  def self.system_restore!(data)
    return unless $game_system && data.is_a?(Hash)
    $game_system.menu_disabled      = !!data['menu_disabled']
    $game_system.save_disabled      = !!data['save_disabled']
    $game_system.encounter_disabled = !!data['encounter_disabled']
    $game_system.timer              = data['timer'].to_f
    $game_system.timer_working      = !!data['timer_working']
    $game_system.save_count         = data['save_count'].to_i
  end

  # Spielstand laden: Karte neu aufbauen, Zustand und Position übernehmen.
  def self.load_game(slot)
    data = Project.load_save($project_name, slot)
    unless data
      Rune::Log.warn("Laden fehlgeschlagen: Slot #{slot} ist leer")
      return false
    end

    # F4: fehlende/unlesbare Zielkarte = Fehler, NICHT stille Ersetzung -
    # sonst lädt ein defekter Spielstand das Projekt hart, während der
    # Zustand (Position/NPCs/Schalter) aus dem Spielstand kommt. Erst
    # prüfen, dann Zustand anfassen. ACHTUNG: Database.map() liefert bei
    # Fehlern einen leeren Ersatz-Hash (nie nil) - playtest_map ist die
    # einzige präzise Prüfung (nil bei fehlend/mehrdeutig/unlesbar).
    map_id = data['map']
    unless map_id.is_a?(String) && Database.playtest_map(map_id)
      Issues.report('Laden',
                    "Karte '#{map_id}' im Spielstand Slot #{slot} fehlt oder " \
                    'ist unlesbar - Laden abgebrochen', severity: 'error')
      return false
    end

    GameState.restore!(data['state'] || {})
    system_restore!(data['system'])   # J: Sperr-Flags/Timer/Save-Zähler
    Party.setup(Database.database)
    Party.restore!(data['party'] || {})
    @intro = nil   # Intro nicht nochmal zeigen

    enter_map(map_id)

    # Ausgelöste Touch-/Auto-Events wiederherstellen.
    (data['fired'] || []).each do |n|
      ev = @events.find { |e| e.name == n }
      ev.fired = true if ev
    end

    # NPC-Positionen und Wegpunkte wiederherstellen (Etappe 15).
    (data['npcs'] || []).each do |h|
      n = @npcs.find { |o| o.name == h['name'] }
      next unless n
      n.restore!(h['x'], h['z'], h['wp'], h['fired'])
      Rune::Log.info(format('NPC %s wiederhergestellt: x=%.2f z=%.2f Wegpunkt %d',
                            n.name, n.position[0], n.position[1], h['wp'].to_i))
    end

    pl = data['player'] || {}
    Rune::Player.teleport(pl['x'].to_f, pl['y'].to_f, pl['z'].to_f)
    Rune::Camera.yaw = pl['yaw'].to_f
    Rune::Camera.mode = (pl['mode'] || 'third').to_sym
    @time = data['time'].to_f
    reset_step_tracking
    Rune::Log.info("Geladen: Slot #{slot} (Karte '#{@map['name']}', Spielzeit #{@time.to_i}s)")
    # Vom Titel/Ladeschirm geladen (Scene_File): Szene verlassen und
    # wieder in den Spielmodus - auch wenn gerade pausiert war.
    $scene = nil
    @mode = :game
    Rune::Input.capture = true
    Rune::Simulation.paused = false
    true
  rescue StandardError => e
    Rune::Log.error("Laden fehlgeschlagen: #{e.message}")
    false
  end

  # Kampf starten (19i): für Scripts und Events - Battle.start mit Absicherung.
  def self.battle_start(troop_id, can_escape: true, can_lose: false)
    return false unless defined?(Battle)

    Battle.start(troop_id, can_escape: can_escape, can_lose: can_lose)
  end

  # Zurück zum Titelbildschirm (19h.2: $game_temp.to_title/gameover wie XP).
  def self.to_title!
    Battle.abort! if defined?(Battle)
    enter_title!
    Rune::Log.info('Zurück zum Titelbildschirm ($game_temp)')
  end

  # Scene_Title betreten (AB): Das Projekt bleibt gebunden (Continue liest
  # die Spielstände desselben Projekts), F4-Vorgaben fallen weg. Projekt-
  # Skripte (autoload) laufen VOR der Szene - XP: Default-Skript zuerst,
  # danach die Projekt-Skripte, die Scene_Title überschreiben oder $scene
  # selbst setzen dürfen.
  def self.enter_title!
    menu_reset!
    @mode = :title
    @menu_sel = 0
    @intro = nil
    clear_playtest_start
    Rune::Input.capture = false
    $scene = nil   # Reste einer Vorgänger-Szene; Scripts dürfen neu setzen
    Database.reset!
    Scripts.load_project
    Scripts.run_startup
    $scene ||= Scene_Title.new(projekt: $project_name)
    # Titel-BGM optional aus system.json (title_bgm), sonst der Spielton.
    bgm = Database.system['title_bgm'].to_s
    Rune::Sound.loop(bgm.empty? ? 'musik' : bgm, 0.15)
    Rune::Log.info("Titelbildschirm (Scene_Title, Projekt: " \
                   "#{$project_name || 'keines'})")
  end

  # Internes Projekt für den normalen Start (AB): zuletzt gespielt, sonst
  # das erste alphabetisch. Neue Projekte entstehen nur im Editor (Y3/Y4).
  def self.resolve_internal_project!
    name = Project.preferred
    return false if name.nil?

    Project.touch(name)
    $project_name = name
    $project_root = Project.data_path(name)
    Database.reset!
    sys = Database.system
    Rune::Window.title = "#{sys['name'] || name} - Rune3D"
    Rune::Log.info("Projekt '#{name}' intern gewählt " \
                   "(#{Database.map_ids.size} Karten)")
    true
  end

  # Ein gebundenes Projekt explizit neu starten (Tests, Werkzeuge) - der
  # Ersatz für den alten Launcher.start. Der Titelmodus wird vor dem Setup
  # aufgehoben, damit start_game greift.
  def self.start_project(name)
    clear_playtest_start
    Project.touch(name)
    $project_name = name
    $project_root = Project.data_path(name)
    Database.reset!
    sys = Database.system
    Rune::Window.title = "#{sys['name'] || name} - Rune3D"
    @mode = nil
    setup
    Rune::Input.capture = true
    $scene = nil
    self
  end

  # Bereits gesetztes Game Over muss vor weiteren Event-Befehlen greifen;
  # eine weitere Prüfung später im Frame nimmt neu gesetzte Flags auf.
  def self.process_temp_scene_change
    return false unless $game_temp

    if $game_temp.gameover
      $game_temp.gameover = false
      show_gameover!   # K/XP Scene_Gameover: erst der Screen, dann der Titel
    elsif $game_temp.to_title
      $game_temp.to_title = false
      to_title!
    elsif TempActions.tick(temp_handler)
      # Etappe 20d: Kampf/Menü/Speichern/Kartenwechsel aus $game_temp -
      # derselbe Frame endet hier, wie in XP Scene_Map#update.
      return true
    else
      return false
    end
    true
  end

  # Etappe 20d: Verdrahtung der $game_temp-Aufträge (Logik in TempActions).
  def self.temp_handler
    {
      battle: lambda do |troop_id, can_escape, can_lose|
        battle_start(troop_id, can_escape: can_escape, can_lose: can_lose)
      end,
      menu: lambda do |speichern|
        # menu_disabled gilt wie bei Esc auch für Script-Aufträge; ein
        # Speicherwunsch landet auf dem ersten Speichern-Eintrag.
        if $game_system && $game_system.menu_disabled
          Rune::Log.warn('$game_temp: Menü gesperrt ($game_system.menu_disabled)')
          next
        end
        open_menu
        if speichern && @menu_items
          i = @menu_items.index { |it| it['act'].is_a?(Array) && it['act'][0] == :save }
          @menu_sel = i if i
        end
      end,
      transfer: ->(karte) { change_map(karte) },
      title: -> { to_title! }
    }
  end

  # Game Over (K/XP 080): schwarzer Bildschirm mit „Game Over", eine Taste
  # führt über Scene_Gameover#update zum Titel. Bewusst KEIN enter_title!:
  # Projekt und Playtest-Vorgabe bleiben gebunden, bis der Spieler die
  # Taste drückt (XP zeigt zuerst den Screen, danach den Titel).
  def self.show_gameover!
    menu_reset!
    @mode = :title
    Rune::Input.capture = false
    $scene = Scene_Gameover.new
    Rune::Log.info('Game Over (Scene_Gameover) - Taste führt zum Titel')
  end

  # ------------------------------------------------------------- Menü (Esc)

  # Spielzeit als m:ss bzw. h:mm:ss (Menü-Panel und Slot-Vorschau, V/J).
  def self.zeit_text(sekunden)
    s = sekunden.to_f
    h = (s / 3600).to_i
    m = ((s - h * 3600) / 60).to_i
    sec = (s - h * 3600 - m * 60).to_i
    h.positive? ? format('%d:%02d:%02d', h, m, sec) : format('%d:%02d', m, sec)
  end

  # 20b: Befehlsliste wie XP (Scene_Menu): Item/Skill/Status zuerst, danach
  # Speichern/Laden und die Rückwege. Esc im Unterfenster geht eine Ebene
  # zurück, erst im Hauptmenü schließt es.
  def self.menu_items
    items = [{ 'label' => 'Gegenstand', 'act' => [:item] },
             { 'label' => 'Fertigkeit', 'act' => [:skill] },
             { 'label' => 'Status', 'act' => [:status] }]
    1.upto(3) do |s|
      info = Project.save_info($project_name, s)
      label = if info
                zeit = info['time'] ? " · #{zeit_text(info['time'])}" : ''
                "Speichern Slot #{s}   (belegt: #{info['map']}#{zeit})"
              else
                "Speichern Slot #{s}   (leer)"
              end
      items << { 'label' => label, 'act' => [:save, s] }
    end
    1.upto(3) do |s|
      info = Project.save_info($project_name, s)
      label = if info
                zeit = info['time'] ? " · #{zeit_text(info['time'])}" : ''
                "Laden     Slot #{s}   (#{info['map']}#{zeit})"
              else
                "Laden     Slot #{s}   (leer)"
              end
      items << { 'label' => label, 'act' => [:load, s], 'off' => info.nil? }
    end
    items << { 'label' => 'Zurück zum Spiel', 'act' => [:resume] }
    items << { 'label' => 'Zum Titelbildschirm', 'act' => [:title] }
    items
  end

  # Z5: Eine offene Eingabe (Text/Wahl/Zahl) hat Vorrang - Esc gehört
  # dann der Eingabe (Abbruch), nicht dem Menü. Gleiche Bedingung wie
  # die F5-Pause in Game.update und wie close_menu.
  def self.eingabe_offen?
    !!@interpreter && (@interpreter.busy_text? || @interpreter.busy_choices? ||
                       @interpreter.busy_number?)
  end

  # Menüzustand komplett zurück (Titel, Game Over, Playtest-Fehler, Schließen).
  def self.menu_reset!
    @menu = nil
    @menu_items = nil
    @menu_screen = nil
    @menu_sub = 0
    @menu_hero = 0
    @menu_item = nil
    @menu_msg = nil
  end

  def self.open_menu
    menu_reset!
    @menu_sel = 0
    # V: beim Öffnen cachen - vorher las update+draw pro Frame bis zu
    # 12 Save-JSONs (save_info je Slot, doppelt). Menü-Labels ändern sich
    # nur durch Speichern/Laden; beides schliesst das Menü (neuer Cache).
    @menu_items = menu_items
    @menu = true
    Rune::Simulation.paused = true   # F5: Menü friert die Simulation ein
    Rune::Input.capture = false   # Menü mit freiem Zeiger
    Rune::Sound.play('klick', 0.6)
    Rune::Log.info('Menü geöffnet')
  end

  def self.close_menu
    menu_reset!
    # Steht noch ein Text/Wahl/Zahl offen, hält die Pause (F5) - der
    # nächste Frame rechnet ohnehin neu.
    Rune::Simulation.paused = !!(@interpreter &&
      (@interpreter.busy_text? || @interpreter.busy_choices? ||
       @interpreter.busy_number?))
    Rune::Input.capture = true
    Rune::Sound.play('klick', 0.5)
    Rune::Log.info('Menü geschlossen')
  end

  def self.update_menu
    return update_menu_screen if @menu_screen

    items = @menu_items || (@menu_items = menu_items)
    n = items.size
    @menu_sel = (@menu_sel - 1) % n if Rune::Input.pressed?(:up)
    @menu_sel = (@menu_sel + 1) % n if Rune::Input.pressed?(:down)
    items.each_with_index do |it, i|
      menu_action(it) if i < 9 && Rune::Input.pressed?((i + 1).to_s)
    end
    menu_action(items[@menu_sel]) if Rune::Input.pressed?(:enter)
  end

  # ------------------------------------------------- Menü-Unterfenster (20b)

  # Esc im Menü: erst eine Unterfenster-Ebene verlassen, sonst schließen.
  def self.menu_back
    if @menu_screen
      @menu_screen = case @menu_screen
                     when :item_target then :item
                     when :skill_list  then :skill
                     when :status_detail then :status
                     else nil
                     end
      @menu_msg = nil
      @menu_sub = 0
      Rune::Sound.play('klick', 0.5)
    else
      close_menu
    end
  end

  # Verwendbare Gegenstände in Datenbank-Reihenfolge (XP: Scene_Item zeigt
  # nur benutzbare Items; Quest-/Goldbeutel bleiben außen vor).
  def self.menu_item_list
    ids = (Database.database['items'] || []).map { |h| h['id'].to_s }
    ids.select { |id| Party.item_count(id).positive? && Party.usable_item?(id) }
  end

  # Fertigkeiten eines Helden als Anzeigezeilen (Fertigkeiten wirken im
  # Kampf - XP erlaubt im Menü ebenfalls kein Anwenden).
  def self.menu_skill_lines(index)
    actor = Party.actors[index]
    return [] unless actor

    Party.skills(actor).filter_map do |id|
      defi = Battle.skill_def(id)
      next unless defi

      "#{defi['name'] || id}   #{Battle.skill_cost(defi)} MP"
    end
  end

  def self.menu_skill_desc(index, sub)
    actor = Party.actors[index]
    return nil unless actor

    id = Party.skills(actor)[sub]
    defi = id ? Battle.skill_def(id) : nil
    defi ? defi['desc'].to_s : nil
  end

  # Statusseite eines Helden (XP: Scene_Status mit allen Werten).
  def self.menu_status_lines(index)
    a = Party.actors[index]
    return [] unless a

    ["#{a.name}   #{a.klass}   Lv #{a.level}",
     "HP  #{a.hp} / #{a.max_hp}",
     "MP  #{a.mp} / #{a.max_mp}",
     "Angriff       #{a.atk}",
     "Verteidigung  #{a.def}",
     "Weisheit      #{a.spi}",
     "Beweglichkeit #{a.agi}",
     "EXP  #{a.exp} / #{a.next_exp}",
     "Fertigkeiten  #{Party.skills(a).size}",
     "Gold  #{Party.gold}"]
  end

  def self.update_menu_screen
    case @menu_screen
    when :item then update_menu_item
    when :item_target then update_menu_item_target
    when :skill then update_menu_actor_list(:skill_list)
    when :status then update_menu_actor_list(:status_detail)
    when :skill_list, :status_detail then update_menu_info
    else menu_back
    end
  end

  def self.update_menu_item
    liste = menu_item_list
    if liste.empty?
      @menu_msg ||= 'Keine verwendbaren Gegenstände'
      menu_back if Rune::Input.pressed?(:escape) || Rune::Input.pressed?(:enter)
      return
    end
    @menu_sub %= liste.size
    @menu_sub = (@menu_sub - 1) % liste.size if Rune::Input.pressed?(:up)
    @menu_sub = (@menu_sub + 1) % liste.size if Rune::Input.pressed?(:down)
    if Rune::Input.pressed?(:enter)
      @menu_item = liste[@menu_sub]
      @menu_hero = 0
      @menu_msg = nil
      @menu_screen = :item_target
      Rune::Sound.play('klick', 0.5)
    elsif Rune::Input.pressed?(:escape)
      menu_back
    end
  end

  def self.update_menu_item_target
    helden = Party.actors
    if helden.empty?
      menu_back
      return
    end
    @menu_hero %= helden.size
    if Rune::Input.pressed?(:left) || Rune::Input.pressed?(:up)
      @menu_hero = (@menu_hero - 1) % helden.size
    elsif Rune::Input.pressed?(:right) || Rune::Input.pressed?(:down)
      @menu_hero = (@menu_hero + 1) % helden.size
    elsif Rune::Input.pressed?(:enter)
      held = helden[@menu_hero]
      item = @menu_item
      if item && Party.use_item(item, target: held)
        @menu_msg = "#{Party.item_name(item)}: #{held.name}  HP #{held.hp}  MP #{held.mp}"
        Rune::Sound.play('jingle', 0.5)
        @menu_screen = :item
        @menu_sub = 0
      else
        name = item ? Party.item_name(item) : 'Gegenstand'
        @menu_msg = "#{name}: keine Wirkung bei #{held.name}"
        Rune::Sound.play('ablehnen', 0.6)
      end
    elsif Rune::Input.pressed?(:escape)
      @menu_item = nil
      menu_back
    end
  end

  def self.update_menu_actor_list(ziel)
    helden = Party.actors
    if helden.empty?
      menu_back if Rune::Input.pressed?(:escape) || Rune::Input.pressed?(:enter)
      return
    end
    @menu_hero %= helden.size
    @menu_hero = (@menu_hero - 1) % helden.size if Rune::Input.pressed?(:up)
    @menu_hero = (@menu_hero + 1) % helden.size if Rune::Input.pressed?(:down)
    if Rune::Input.pressed?(:enter)
      @menu_screen = ziel
      @menu_sub = 0
      Rune::Sound.play('klick', 0.5)
    elsif Rune::Input.pressed?(:escape)
      menu_back
    end
  end

  # Fertigkeiten-/Statusseite: nur ansehen, Enter und Esc gehen zurück.
  def self.update_menu_info
    zeilen = @menu_screen == :skill_list ? menu_skill_lines(@menu_hero) : menu_status_lines(@menu_hero)
    if Rune::Input.pressed?(:up) && !zeilen.empty?
      @menu_sub = (@menu_sub - 1) % zeilen.size
    elsif Rune::Input.pressed?(:down) && !zeilen.empty?
      @menu_sub = (@menu_sub + 1) % zeilen.size
    elsif Rune::Input.pressed?(:enter) || Rune::Input.pressed?(:escape)
      menu_back
    end
  end

  def self.menu_action(it)
    Rune::Log.info("Menü: #{it['label']}")
    case it['act'][0]
    when :item
      @menu_screen = :item
      @menu_sub = 0
      @menu_msg = menu_item_list.empty? ? 'Keine verwendbaren Gegenstände' : nil
      Rune::Sound.play('klick', 0.5)
    when :skill
      @menu_screen = :skill
      @menu_hero = 0
      @menu_msg = nil
      Rune::Sound.play('klick', 0.5)
    when :status
      @menu_screen = :status
      @menu_hero = 0
      @menu_msg = nil
      Rune::Sound.play('klick', 0.5)
    when :resume then close_menu
    when :save
      if save_game(it['act'][1])
        Rune::Sound.play('jingle', 0.5)
        close_menu
      else
        Rune::Sound.play('ablehnen', 0.7)
      end
    when :load
      if load_game(it['act'][1])
        Rune::Sound.play('klick', 0.9)
        close_menu
      else
        Rune::Sound.play('ablehnen', 0.7)
      end
    when :title
      to_title!   # enter_title! baut Scene_Title auf und startet die Titelmusik
    end
  end

  def self.draw_menu
    w = Rune::Window.width
    h = Rune::Window.height
    items = @menu_items || (@menu_items = menu_items)
    mw = 480
    mh = 66 + items.size * 34 + 12
    mx = w / 2 - mw / 2
    my = h / 2 - mh / 2
    UI.panel(mx, my, mw, mh, UI::PANEL, 0.95)
    UI.rahmen(mx, my, mw, mh, UI::ACCENT, 0.6)
    UI.text(mx + 20, my + 16, 'Menü', UI::TEXT, 20)
    unter = @menu_screen ? UI::TEXT_DIM : UI::TEXT
    items.each_with_index do |it, i|
      y = my + 52 + i * 34
      sel = (i == @menu_sel) && @menu_screen.nil?
      UI.panel(mx + 12, y - 3, mw - 24, 30, sel ? 0x2A4A66 : 0x101826, sel ? 0.95 : 0.5)
      farbe = it['off'] ? UI::TEXT_DIM : unter
      UI.text(mx + 24, y, (sel ? '> ' : '  ') + it['label'], farbe, 15)
    end
    UI.text(mx + 20, my + mh - 26,
            @menu_screen ? '[Hoch/Runter] Wählen   [Enter] OK   [Esc] Zurück' :
                           '[Hoch/Runter] Wählen   [Enter] OK   [1..8] direkt   [Esc] Zurück',
            UI::TEXT_DIM, 13)

    # 20b: Rechte Spalte - offenes Unterfenster (Gegenstände/Fertigkeit/
    # Status) hat Vorrang, sonst das Party-Panel wie in XP (Scene_Menu).
    spalte_w = 380
    spalte_x = mx + mw + 24
    spalte_x = [w - spalte_w - 12, 12].max if spalte_x + spalte_w > w - 12
    if @menu_screen
      draw_menu_screen(spalte_x, my, spalte_w)
    else
      draw_party_panel(spalte_x, my, spalte_w)
    end
  end

  # Party-Panel im Menü: Gesichter, HP-Zeilen, Gold, Spielzeit und Schritte.
  def self.draw_party_panel(x, y, breite)
    # Etappe 17g: Party-Status (Helden, Gold, Inventar) neben dem Menü.
    # V: Playtime/Steps wie XP Scene_Menu ins Panel (Spielzeit = @time,
    # Schritte = $game_party.steps aus F3).
    lines = Party.status_lines
    stufen = (defined?($game_party) && $game_party) ? $game_party.steps.to_i : 0
    extra = ["Spielzeit  #{zeit_text(@time)}", "Schritte  #{stufen}"]
    alle = lines + extra
    pw = breite
    ph = 60 + alle.size * 28
    px = x
    py = y
    return if py + ph > Rune::Window.height - 8   # kein Platz: weglassen
    UI.panel(px, py, pw, ph, UI::PANEL, 0.95)
    UI.rahmen(px, py, pw, ph, UI::ACCENT, 0.6)
    UI.text(px + 20, py + 14, 'Party', UI::TEXT, 18)
    alle.each_with_index do |line, i|
      if i < Party.actors.size
        held = Party.actors[i]
        UI.gesicht(px + 16, py + 40 + i * 28, 24, held.name,
                   farbe: Party.face_color(held), hp: held.hp, max_hp: held.max_hp)
        UI.text(px + 48, py + 44 + i * 28, line, UI::TEXT, 14)
      else
        UI.text(px + 20, py + 44 + i * 28, line, UI::TEXT_DIM, 14)
      end
    end
  end


  # Unterfenster zeichnen: links die Liste, oben eine Überschrift, unten die
  # Rückmeldung bzw. die Bedienhinweise (XP zeigt dieselben Fenster getrennt).
  def self.draw_menu_screen(x, y, breite)
    titel, zeilen, cursor = menu_screen_content
    # Gesichter (20b-Nacharbeit): Statusseite mit Parteigesicht (XP: Face-Grafik),
    # die Werte rutschen entsprechend nach rechts.
    held = @menu_screen == :status_detail ? Party.actors[@menu_hero] : nil
    gesicht = held ? 64 : 0
    text_x = x + 20 + (gesicht.positive? ? gesicht + 16 : 0)
    hoehe = [60 + [zeilen.size, 1].max * 26 + 34, gesicht.zero? ? 0 : 60 + gesicht + 10].max
    UI.panel(x, y, breite, hoehe, UI::PANEL, 0.95)
    UI.rahmen(x, y, breite, hoehe, UI::ACCENT, 0.6)
    UI.text(x + 20, y + 14, titel, UI::TEXT, 18)
    if gesicht.positive?
      UI.gesicht(x + 20, y + 46, gesicht, held.name, farbe: Party.face_color(held),
                 hp: held.hp, max_hp: held.max_hp, auswahl: true)
    end
    if zeilen.empty?
      UI.text(text_x, y + 46, '—', UI::TEXT_DIM, 14)
    else
      zeilen.each_with_index do |zeile, i|
        zy = y + 46 + i * 26
        if cursor == i
          UI.panel(x + 12, zy - 3, breite - 24, 24, 0x2A4A66, 0.9)
          UI.text(text_x, zy, "> #{zeile}", UI::ACCENT, 14)
        else
          UI.text(text_x, zy, "  #{zeile}", UI::TEXT, 14)
        end
      end
    end
    fuss = @menu_msg || menu_screen_hint
    UI.text(x + 20, y + hoehe - 26, fuss, @menu_msg ? UI::ACCENT : UI::TEXT_DIM, 13)
  end

  def self.menu_screen_hint
    case @menu_screen
    when :item then '[Enter] Verwenden   [Esc] Zurück'
    when :item_target then '[Links/Rechts] Held wählen   [Enter] Anwenden   [Esc] Zurück'
    when :skill, :status then '[Hoch/Runter] Held   [Enter] Ansehen   [Esc] Zurück'
    else 'Enter / Esc = zurück'
    end
  end

  # Inhalt des Unterfensters: [Titel, Zeilen, Cursor]
  def self.menu_screen_content
    helden = Party.actors
    case @menu_screen
    when :item
      ids = menu_item_list
      return ['Gegenstände', [], nil] if ids.empty?

      [ 'Gegenstände',
        ids.map { |id| "#{Party.item_name(id)}  x#{Party.item_count(id)}  " \
                       "(#{(Party.item_def(id) || {})['kind'] == 'mana' ? 'MP' : 'HP'})" },
        @menu_sub ]
    when :item_target
      name = @menu_item ? Party.item_name(@menu_item) : 'Gegenstand'
      ["#{name} — Ziel wählen", helden.map(&:hp_line), @menu_hero]
    when :skill
      ['Fertigkeit — Held wählen', helden.map { |a| "#{a.name}  Lv #{a.level}  #{a.mp}/#{a.max_mp} MP" },
       @menu_hero]
    when :status
      ['Status — Held wählen', helden.map(&:hp_line), @menu_hero]
    when :skill_list
      held = helden[@menu_hero]
      ["Fertigkeiten: #{held ? held.name : '—'}", menu_skill_lines(@menu_hero), @menu_sub]
    when :status_detail
      held = helden[@menu_hero]
      ["Status: #{held ? held.name : '—'}", menu_status_lines(@menu_hero), nil]
    else
      ['', [], nil]
    end
  end

  # ---------------------------------------------------------------- Ablauf

  # Die F4-Vorgabe gilt nur im gestarteten Projekt. Beim Titelbildschirm oder
  # einer manuellen Projektwahl wird sie gelöscht; F5 im Playtest behält sie.
  def self.clear_playtest_start
    @playtest_project = nil
    @playtest_map = nil
  end

  def self.playtest_failed(message)
    Battle.abort! if defined?(Battle)
    @mode = :playtest_error
    @playtest_error = message
    menu_reset!
    Rune::Input.capture = false
    Rune::Simulation.paused = true
    Rune::Log.warn("Playtest abgebrochen: #{message}")
  end

  def self.setup
    recovering = @mode == :playtest_error
    @time = 0.0
    @debug = false
    @debug_timer = 0.0
    @prompt = nil
    Issues.clear!   # Etappe 18h: Backlog startet pro Session/F5 neu

    # F5 auf dem Titelbildschirm: Szene neu aufbauen, nicht ins Spiel
    # springen (der Modus überlebt das Neuladen von main.rb).
    if @mode == :title && !recovering
      enter_title!
      return
    end

    # Ohne Projekt: interne Auswahl (F4 > last_played > erstes Projekt),
    # dann Scene_Title. --project startet direkt; eine einmal gelesene
    # --map-Vorgabe gehört nur zu diesem Projekt. Anlage nur im Editor.
    if $project_root.nil?
      requested_map = ENV.delete('RUNE_PLAYTEST_MAP')
      # Projektverzeichnisnamen sind exakt; führende Leerzeichen sind auf
      # Windows zulässig und dürfen bei --project/F4 nicht entfernt werden.
      env_project = ENV['RUNE_PROJECT'].to_s
      if !requested_map.nil?
        @playtest_project = env_project
        @playtest_map = requested_map
      elsif recovering && @playtest_project == env_project
        # Auch ein zunächst fehlendes Projekt darf beim F5-Versuch nicht die
        # ausgewählte Karte verlieren und still dessen Startkarte verwenden.
        requested_map = @playtest_map
      else
        clear_playtest_start
      end
      known = Project.list.include?(env_project)
      Rune::Log.warn("RUNE_PROJECT '#{env_project}' ist unbekannt") if
        !env_project.empty? && !known
      if !env_project.empty? && known
        Project.touch(env_project)
        $project_name = env_project
        $project_root = Project.data_path(env_project)
        Database.reset!
        sys = Database.system
        Rune::Log.info("Playtest: Projekt '#{sys['name'] || env_project}' " \
                       "direkt geladen (#{Database.map_ids.size} Karten)")
        Rune::Window.title = "#{sys['name'] || env_project} - Rune3D"
        Rune::Input.capture = true
      elsif !requested_map.nil?
        playtest_failed("Projekt '#{env_project}' wurde nicht gefunden.")
        return
      elsif !env_project.empty?
        # F4 ohne Kartenwahl: ein angefordertes, aber unbekanntes Projekt
        # ist ein Fehler - kein stiller Wechsel auf einen anderen Start.
        playtest_failed("Projekt '#{env_project}' wurde nicht gefunden.")
        return
      else
        # Normaler Start ohne F4: intern auflösen (zuletzt gespielt, sonst
        # das erste Projekt) und auf den Scene_Title. Gibt es kein Projekt,
        # zeigt der Titel den Hinweis, im Editor Neues Projekt anzulegen.
        resolve_internal_project!
        enter_title!
        return
      end
    end
    start_game
  end

  # Spielstart im gebundenen Projekt: System-Startkarte (oder die
  # ausdrücklich gewählte F4-Karte), RGSS-Globale, Projekt-Skripte.
  # Genutzt von F4 (direkt), F5, dem Titel (Neues Spiel) und den Tests.
  def self.start_game
    clear_playtest_start if @playtest_project && @playtest_project != $project_name

    # Eine ausdrücklich gewünschte Karte muss eindeutig existieren und ein
    # lesbares JSON-Objekt sein. Niemals unbemerkt eine andere Karte starten.
    selected_map = nil
    unless @playtest_map.nil?
      selected_map = Database.playtest_map(@playtest_map)
      unless selected_map
        playtest_failed("Karte '#{@playtest_map}' fehlt, ist mehrdeutig oder nicht lesbar.")
        return
      end
    end
    @mode = :game
    $scene = nil          # Titel/Ladeschirm gehört nicht ins Spiel
    Rune::Simulation.paused = false   # F5: Spiel startet laufend
    Rune::Input.capture = true        # auch für den Start vom Titel

    # Der Editor kann während des Playtests den Projektstart verschieben.
    # F5/Neustart liest system.json deshalb erneut, ohne andere Caches zu leeren.
    Database.reload_system
    # Spielzustand aus system.json zurücksetzen (bei F5 ebenfalls).
    GameState.reset!(Database.system)
    # Etappe 20c: Fenster-Optionen (XP 104) starten mit den XP-Defaults.
    MessageWindow.reset! if defined?(MessageWindow)
    Party.setup(Database.database)
    Scripts.load_project

    map_id = selected_map ? @playtest_map :
      (Database.system.dig('start', 'map') || Database.map_ids.first || 'welt01')

    # Optik: prozedurale Texturen, Himmel/Nebel/Panorama aus der Startkarte.
    Textures.end_project
    Textures.register_all
    Textures.begin_project
    Textures.register_project
    # Etappe 18l: Terrain-Bau ist nach enter_map gewandert (Kartenwechsel
    # baut es ebenfalls auf, damit Kartengrenze und gemalte Berge stimmen).
    enter_map(map_id, loaded_map: selected_map, start_override: project_start_for(map_id))
    RGSS.setup   # Etappe 19e: $game_*-Globale (RPG-Maker-XP-Schicht)
    # enter_map läuft beim Kaltstart vor RGSS.setup; dieses ersetzt den
    # Spieler auch bei F5. Den neuen Spieler daher auf der Startkarte würfeln.
    $game_player.make_encounter_count
    Rune::Camera.mode = :third
    Rune::Camera.distance = 7   # Titel-Überblick (28) -> normaler Follow-Cam
    Rune::Sound.loop('musik', 0.22)   # im Spiel etwas lauter als im Titel
    # Opt-in-RGSS-Projektscripte: nach Aufbau von Karte und $game_*-Globalen,
    # vor dem ersten Spielframe, in der Reihenfolge des Script-Editors.
    Scripts.run_startup

    # Intro-Text als Event am Kartenstart (auto-Trigger desselben Effekts).
    @intro = ['Willkommen in Rune3D.  WASD = laufen, Shift = rennen, Leertaste = springen.',
              'E = mit Personen/Kristallen reden, F1/F3 = Kamera, F5 = Ruby neu laden.']
  end

  def self.update(dt)
    # Titel (Scene_Title/Scene_File) und fehlgeschlagener Playtest starten
    # keine Spiel-Frames.
    if @mode == :title
      Rune::Simulation.paused = true   # F5: keine Physik auf dem Titelbild
      if $scene.nil?
        # Szene durch einen Fehler verloren: einmal neu aufbauen, sonst
        # beenden (Flucht über Esc), damit kein schwarzer Leerlauf entsteht.
        begin
          enter_title!
        rescue StandardError => e
          Rune::Log.error("Titelbildschirm: #{e.class}: #{e.message}")
          Rune::App.quit if Rune::Input.pressed?(:escape)
          return
        end
      end
      Graphics.scene_update   # ruft $scene.main (ein Frame, kein Loop)
      return
    elsif @mode == :playtest_error
      Rune::App.quit if Rune::Input.pressed?(:escape)
      return
    end

    @time += dt
    Rune::Log.info("Spielzeit #{@time.to_i}s") if (@time - dt).to_i != @time.to_i && @time.to_i % 30 == 0

    if Rune::Input.pressed?(:f2)
      @debug = !@debug
      Rune::Log.info("Debug-Anzeige: #{@debug ? 'an' : 'aus'}")
    end

    # Kampfeingaben haben Vorrang vor dem Spielmenü. Esc muss Ziel-/Itemwahl
    # abbrechen oder im Kampf eine Flucht versuchen, nicht das Menü öffnen.
    if defined?(Battle) && Battle.active?
      close_menu if @menu
      Rune::Simulation.paused = true
      Battle.update(dt)
      Effects.update(dt)
      Fade.update(dt)
      Rune::Simulation.paused = false unless Battle.active?
      return
    end

    return if process_temp_scene_change

    # Menü (Esc): friert Spiellogik, Interpreter und Events ein.
    # Z5: Während einer offenen Eingabe (Text/Wahl/Zahl) gehört Esc der
    # Eingabe, nicht dem Menü (XP öffnet kein Scene_Menu über einer
    # aktiven Nachricht) - sonst frisst das Menü den Wahl-/Zahlen-Abbruch.
    if Rune::Input.pressed?(:escape)
      # Etappe 19e: $game_system.menu_disabled (RGSS) unterdrückt das Öffnen;
      # ein bereits offenes Menü schliesst Esc immer.
      if @menu
        # 20b: Ein Unterfenster behandelt Esc selbst (eine Ebene zurück) -
        # hier nur das Hauptmenü schließen, sonst ginge Esc zwei Ebenen weit.
        close_menu if @menu_screen.nil?
      elsif !eingabe_offen? && (!$game_system || !$game_system.menu_disabled)
        open_menu
      end
    end
    if @menu
      update_menu
      return
    end

    if @debug
      @debug_timer = (@debug_timer || 0.0) + dt
      if @debug_timer >= 0.75
        @debug_timer = 0.0
        px, py, pz = Rune::Player.position
        Rune::Log.info(format('Spieler x=%.2f y=%.2f z=%.2f Boden=%s FPS=%.0f',
                              px, py, pz, Rune::Player.on_ground?, Rune::Time.fps))
      end
    end

    # Interpreter zürst: blockiert er (Textfenster offen), wird nichts Neues
    # ausgelöst und auch kein Prompt gesetzt.
    @interpreter.update(dt)
    # Ein Event kann selbst einen Kampf gestartet haben. Vor dem nächsten
    # Physik-Tick anhalten, statt noch einen Welt-Frame abzuarbeiten.
    if defined?(Battle) && Battle.active?
      Rune::Simulation.paused = true
      return
    end
    # Zahleneingabe (19i): Eingabe am Eingabefeld weiterdrehen.
    if @interpreter.busy_number?
      ni = @interpreter.num_input
      @interpreter.instance_variable_set(:@num_input, nil) unless UI.nummer_eingabe(ni)
    end

    # Sound: Schritte, Sprung, Landung (nur im Spiel, nicht im Menü).
    if @mode == :game
      px, py, pz = Rune::Player.position
      on_ground = Rune::Player.on_ground?
      if @last_pos
        dist = Math.sqrt((px - @last_pos[0])**2 + (pz - @last_pos[2])**2)
        if on_ground && dist > 0.01
          @step_dist = (@step_dist || 0.0) + dist
          rennt = Rune::Input.down?(:shift)
          if @step_dist > (rennt ? 0.85 : 0.60)
            @step_dist = 0.0
            @step_count = (@step_count || 0) + 1
            Rune::Log.info('Erster Schritt-Sound abgespielt') if @step_count == 1
            Rune::Sound.play('schritt', rennt ? 0.55 : 0.40)
            @step_event = true   # 19h.2: $game_player.increase_steps
          end
          # Begegnungen zählen pro Kachel (W): Sprünge und Teleports
          # (grosse Distanzen) zählen nicht als gelaufener Weg.
          if dist < 3.0
            @kachel_dist = (@kachel_dist || 0.0) + dist
            # Epsilon: zehn Schritte à 0,1 m ergeben in Fließkomma
            # 0,9999999 m - die Kachel muss trotzdem zählen.
            if @kachel_dist >= KACHEL_METER - 1e-6
              @kachel_dist -= KACHEL_METER
              @kachel_schritt = true
            end
          end
        end
      end
      Rune::Sound.play('sprung', 0.5) if on_ground && Rune::Input.pressed?(:space)
      Rune::Sound.play('landung', 0.7) if on_ground && @was_air == true
      Effects.dust(px, py, pz) if on_ground && @was_air == true   # Staub bei Landung
      @was_air = !on_ground
      @last_pos = [px, py, pz]
    end

    Effects.update(dt)
    Fade.update(dt)   # Etappe 19d: Bildschirmblende (XP-Fade) weiterdrehen
    Mover.update(dt)  # Etappe 19g: Bewegungsrouten laufen IMMER (auch bei Text)
    Graphics.update         # Etappe 19h.2: RGSS-Frame-Zähler (frame_count)
    $game_system.update(dt) if $game_system   # 19h.2: XP-Timer tickt
    # Eine Kachel für Begegnungen vormerken (W: XP zählt pro Kachel, nicht
    # pro Schritt-Sound). Ob sie zählt, entscheiden wir nach Events und
    # laufenden Kämpfen; F4 hat im Spiel keine Editorfunktion.
    encounter_schritt = @kachel_schritt && $game_player &&
                        !(defined?(Battle) && Battle.active?)
    $game_player.increase_steps if $game_player && @step_event
    @step_event = nil
    @kachel_schritt = nil
    Graphics.scene_update   # Etappe 19h: $scene.main (RGSS-Szenen-Schleife)
    if defined?(Battle) && Battle.active?
      Rune::Simulation.paused = true
      return
    end

    # 19h.2: Auch in diesem Frame neu gesetzte XP-Flags beachten.
    return if process_temp_scene_change
    px, py, pz = Rune::Player.position
    Weather.update(dt, px, py, pz)   # Etappe 17h: Regen/Schnee um den Spieler

    # D3 (XP-Gleichheit): Die Karte lebt weiter, während ein Textfenster
    # offen ist. Seitenbedingungen, NPC-Bewegung und NPC-Kollision laufen
    # deshalb VOR der Interpreter-Abfrage; nur die Auslöser (auto/touch/E)
    # brauchen einen freien Interpreter. Menü und Kampf kehren oben schon
    # zurück (XP: eigene Szenen) - dort steht die Karte weiterhin still.
    #
    # Auch NPCs besitzen ein Event. Dieselbe Trigger-Verarbeitung erlaubt
    # insbesondere Gespräche per E, nicht nur per Maus-Picking.
    # Etappe 20c: ein per erase_event gelöschtes Event löst nichts mehr aus
    # und verschwindet aus der Darstellung (bis zum nächsten Kartenwechsel).
    @events.each_with_index do |event, i|
      next unless event.erased? && @event_visuals && !event.visual_hidden
      @event_visuals[i].to_a.each { |node| node.visible = false unless node.nil? }
      event.visual_hidden = true
    end
    event_liste = (@events + @npcs.map(&:event)).reject(&:erased?)
    event_liste.each(&:refresh_page!)

    # NPCs bewegen (Position des zugehörigen Events folgt).
    @npcs.each { |n| n.update(dt) }

    # NPC-Kollision (Etappe 15): Spieler aus NPCs herausdrücken.
    @npcs.each do |n|
      nx, nz = n.position
      Rune::Player.push_out(nx, nz, n.block_radius)
    end

    unless @interpreter.running?
      # Etappe 20c: Gemeinsame Events mit Trigger (XP-Autorun/parallel)
      # starten vor den Karten-Events, wie in XP Game_Map#update.
      CommonEvents.tick(@interpreter) if defined?(CommonEvents)

      px, py, pz = Rune::Player.position

      # Auto-/Touch-Events (Etappe Z6, Designentscheidungen D1/D2):
      # Der ganze Block läuft nur, wenn der Interpreter frei ist.
      #  - auto = XP-Autorun (D1): startet jedes Frame neu und läuft bis
      #    zum Seitenwechsel (klassisch: Self-Switch am Ende, sonst
      #    Endlosschleife wie in XP). 'fired' sperrt auto nicht mehr - es
      #    merkt nur das erste Log je Seite.
      #  - touch = XP-Wiederbetreten (D2): feuert bei jeder Ankunft in
      #    der Nähe erneut; Verlassen der Nähe stellt 'fired' wieder her
      #    (ein Self-Switch macht es einmalig - wie XP-Autoren es
      #    gewöhnt sind).
      event_liste.each do |e|
        trig = e.trigger
        next unless %w[touch auto].include?(trig)
        if trig == 'auto'
          Rune::Log.info("Event '#{e.name}' ausgelöst (auto)") unless e.fired
          e.fired = true
          e.manual_fire(@interpreter)
          break
        end
        if e.near?(px, pz)
          next if e.fired
          e.fired = true
          Rune::Log.info("Event '#{e.name}' ausgelöst (touch)")
          e.manual_fire(@interpreter)
          break
        elsif e.fired
          e.fired = false # D2: Nähe verlassen - nächste Ankunft feuert neu
        end
      end

      # Parallel-Events (Etappe 18f, RPG-Maker-Parallel-Process): laufen im
      # Hintergrund und starten sich neu, sobald sie fertig sind. Damit sie
      # nicht die Engine fluten, nur wenn der Interpreter gerade frei ist —
      # ein 'wait' im Event steuert das Tempo (wie bei RPG Maker üblich).
      event_liste.each do |e|
        next unless e.trigger == 'parallel'
        next if @interpreter.running?
        e.manual_fire(@interpreter)
      end

      # Action-Events: nächstes in Reichweite anbieten (E drucken)
      @prompt = nil
      event_liste.each do |e|
        break if @interpreter.running? # kein bereits gestartetes Event verdrängen
        next unless e.trigger == 'action'
        if e.near?(px, pz)
          @prompt = e.name
          if Rune::Input.pressed?(:e)
            Rune::Log.info("Event '#{e.name}' ausgelöst (E)")
            e.manual_fire(@interpreter)
            @prompt = nil
          end
          break
        end
      end

      # Intro einmalig anzeigen (wie ein auto-Event, aber ohne Daten-Datei)
      if @intro
        @interpreter.start([{ 'cmd' => 'text', 'lines' => @intro[0..0] },
                            { 'cmd' => 'text', 'lines' => @intro[1..1] }])
        @intro = nil
      end
    end

    # F5 (XP): offene Text/Wahl/Zahleneingabe friert den SPIELER ein - er
    # kann sich nicht bewegen, bis die Eingabe abgeschlossen ist. D3: NPCs
    # und Seitenbedingungen laufen dabei weiter (wie in XP). 'wait' in einer
    # Bewegungsroute pausiert dagegen NICHT.
    Rune::Simulation.paused = @menu ||
                              @interpreter.busy_text? ||
                              @interpreter.busy_choices? ||
                              @interpreter.busy_number?

    # Events haben Vorrang: ein im selben Schritt ausgelöstes Event
    # darf nicht zusammen mit einem Zufallskampf starten. Ein gesperrter oder
    # fehlgeschlagener Kampf lässt den Zähler bei 0; der nächste Schritt
    # versucht es erneut. Nach Erfolg steht die Welt sofort still.
    # Ein Script kann nach dem Vormerken teleportieren und damit die
    # Schritt-Referenz zurücksetzen; dann darf der alte Schritt nicht zählen.
    if encounter_schritt && @last_pos && $game_player && $game_map
      $game_player.decrease_encounter_count
      liste = $game_map.encounter_list
      if $game_player.encounter_count == 0 && !liste.empty? &&
         !($game_system && $game_system.encounter_disabled) &&
         !@interpreter.running? && !(defined?(Battle) && Battle.active?)
        if battle_start(liste.sample)
          $game_player.make_encounter_count
          @prompt = nil
          Rune::Simulation.paused = true
          return
        end
      end
    end

    @scene.update(dt)
    update_picking
  end

  # ------------------------------------------------------------- Maus-Picking

  # Alle anklickbaren Dinge: Szenenknoten + Events + NPCs.
  def self.pickables
    list = []
    hidden_nodes = (@npcs.map(&:node) + (@placement_visuals || []))
                   .each_with_object({}) { |node, set| set[node] = true }
    @scene.walk(@scene.root) do |n|
      # Nur die zugehörigen Körper/Schilder auslassen, nicht beliebige
      # Script-Nodes mit ähnlich lautendem Namen.
      next if n == @scene.root || !n.visible || hidden_nodes.key?(n)

      list << n
    end
    list.concat(@events.reject(&:erased?))
    list.concat(@npcs.map(&:event).reject(&:erased?))
    list
  end

  # Zuletzt unter dem Zeiger getroffener Knoten bzw. getroffenes Event.
  def self.hover
    @hover
  end

  # Strahl aus der Mausposition (im Fang-Modus: Fadenkreuz in der Mitte).
  def self.compute_hover
    mx, my = Rune::Input.mouse_position
    ray = Rune::Camera.ray(mx, my)
    return if ray.empty?

    hit = Pick.closest(ray[0, 3], ray[3, 3], pickables)
    @hover, @hover_dist = hit ? [hit[0], hit[1]] : [nil, 0.0]

    # Nur bei Wechsel ins Log, sonst rauscht es jede Sekunde.
    if @hover != @hover_prev
      @hover_prev = @hover
      Rune::Log.info(format("Hover: %s (%.1f m)", @hover.name, @hover_dist)) if @hover
    end
  end

  def self.update_picking
    compute_hover
    return unless Rune::Input.mouse_pressed?(0) && @hover

    if @hover.is_a?(Event)
      # D9 (XP-Gleichheit): nur aktive 'action'-Events starten per Klick -
      # touch/auto/parallel laufen über die Trigger-Schleife, inaktive
      # Events (keine erfüllte Seitenbedingung) gar nicht.
      if @hover.active? && @hover.trigger == 'action'
        unless @interpreter.running?
          Rune::Log.info(format("Klick: Event '%s' (%.1f m)", @hover.name, @hover_dist))
          @hover.manual_fire(@interpreter)
        end
      else
        Rune::Log.info(format("Klick ohne Wirkung: Event '%s (%s)' (%.1f m)",
                              @hover.name, @hover.trigger || 'inaktiv', @hover_dist))
      end
    else
      sz = @hover.scale
      Rune::Log.info(format("Klick: Objekt '%s' (%.1f m) Grösse %.1f x %.1f x %.1f",
                            @hover.name, @hover_dist, sz[0], sz[1], sz[2]))
    end
  end

  # Kanten einer Box als 12 dünne Kästen (es gibt noch keine Linien-Pipeline).
  def self.draw_wire_box(cx, cy, cz, sx, sy, sz, color, t = 0.045)
    hx = sx * 0.5; hy = sy * 0.5; hz = sz * 0.5
    xs = [-hx, hx]; ys = [-hy, hy]; zs = [-hz, hz]

    # 4 Kanten entlang X
    ys.each do |dy|
      zs.each do |dz|
        Rune::Gfx.box(cx, cy + dy, cz + dz, sx + t, t, t, color)
      end
    end
    # 4 Kanten entlang Y
    xs.each do |dx|
      zs.each do |dz|
        Rune::Gfx.box(cx + dx, cy, cz + dz, t, sy + t, t, color)
      end
    end
    # 4 Kanten entlang Z
    xs.each do |dx|
      ys.each do |dy|
        Rune::Gfx.box(cx + dx, cy + dy, cz, t, t, sz + t, color)
      end
    end
  end

  # Beschriftung des Objekts unter der Maus / dem Fadenkreuz.
  def self.draw_hover_label(x = 16, y = 78)
    return unless @hover
    if @hover.is_a?(Event)
      UI.text(x, y, format('%s (Event, %.1f m)  Klick = reden', @hover.name, @hover_dist), UI::ACCENT, 16)
    else
      s = @hover.scale
      label = @hover.name
      idx = @map_objects.object_index(@hover)
      if idx
        obj = @map['objects'][idx]
        label = obj ? Prefabs.name_of(obj['type']) : 'Objekt'
      end
      UI.text(x, y, format('%s  %.1f x %.1f x %.1f  (%.1f m)',
                           label, s[0], s[1], s[2], @hover_dist), UI::ACCENT, 16)
    end
  end

  # Kleines Fadenkreuz, solange die Maus gefangen ist.
  def self.draw_crosshair
    return unless Rune::Input.captured?
    cx = Rune::Window.width / 2
    cy = Rune::Window.height / 2
    Rune::Gfx.rect(cx - 1, cy - 6, 2, 12, 0xFFFFFF, 0.65)
    Rune::Gfx.rect(cx - 6, cy - 1, 12, 2, 0xFFFFFF, 0.65)
  end

  def self.draw
    if @mode == :title
      Rune::Gfx.clear_color = 0x131A24
      if $scene && $scene.respond_to?(:draw)
        $scene.draw
      else
        UI.text(40, 40, 'Titelbildschirm nicht verfügbar', UI::DANGER, 18)
        UI.text(40, 72, '[Esc] Beenden', UI::TEXT_DIM, 14)
      end
      return
    elsif @mode == :playtest_error
      Rune::Gfx.clear_color = 0x131A24
      width = Rune::Window.width
      y = Rune::Window.height / 2 - 70
      UI.panel(20, y, width - 40, 140, UI::PANEL, 0.97)
      UI.text(40, y + 18, 'Playtest abgebrochen', UI::DANGER, 20)
      UI.text(40, y + 54, @playtest_error.to_s[0, 100], UI::TEXT, 15)
      UI.text(40, y + 92, 'Karte/Projekt prüfen. [F5] Erneut   [Esc] Beenden', UI::TEXT_DIM, 15)
      return
    end

    Rune::Gfx.clear_color = 0x131A24

    # Der Spieler existiert nur als Punkt; im Third-Person-Modus bekommt er einen Körper.
    # F6: $game_player.transparent (XP) verbirgt den Körper.
    if Rune::Camera.mode == :third && !($game_player && $game_player.transparent)
      px, py, pz = Rune::Player.position
      Rune::Gfx.box(px, py + 0.9, pz, 0.7, 1.8, 0.7, 0xE8E2D0,
                    0.0, Rune::Camera.yaw, 0.0)
    end

    @scene.draw
    Effects.draw   # Partikel über der Szene, unter dem UI
    Weather.draw   # Etappe 17h: Wetter über den Partikeln
    # Hervorhebung des Objekts unter der Maus / dem Fadenkreuz.
    # Im Fang-Modus (Egoperspektive) nur Events, sonst wäre alles markiert.
    zeige = @hover && (@hover.is_a?(Event) || !Rune::Input.captured?)
    if zeige
      if @hover.is_a?(Event)
        c = @hover.box_center
        r = @hover.pick_radius * 1.15
        draw_wire_box(c[0], c[1], c[2], r * 2, r * 2, r * 2, 0x59D8FF)
      else
        p = @hover.position
        s = @hover.scale
        draw_wire_box(p[0], p[1], p[2], s[0] * 1.02, s[1] * 1.02, s[2] * 1.02, 0x59D8FF)
      end
    end

    Graphics.draw   # Etappe 19h: RGSS-Sprites/Fenster über der Welt

    # 2D-Overlay zuletzt
    UI.hud
    # Etappe 20d: Timer-Fenster oben rechts (XP 124/Window_Timer) - sichtbar,
    # solange der Timer arbeitet.
    UI.timer($game_system.timer_seconds) if $game_system && $game_system.timer_working
    # Z8: Text und Wahl in EINEM Fenster (XP-Kombi-Fenster). Die Optionen
    # stehen unter dem Text; fehlt einer der beiden Teile, bleibt er leer.
    wahl = @interpreter.busy_choices? ? @interpreter.choices : nil
    if @interpreter.busy_text? || wahl
      # Etappe 20c: Position/Rahmen/Transparenz aus dem text_options-Befehl
      # (XP 104). MessageWindow liegt außerhalb des Interpreters, damit die
      # Einstellung Kartenwechsel überlebt wie in XP.
      UI.message(@interpreter.visible_lines, wahl, wartet: @interpreter.text_waiting?,
                 position: (defined?(MessageWindow) ? MessageWindow.position : 'unten'),
                 frame: (defined?(MessageWindow) ? MessageWindow.frame? : true),
                 transparent: (defined?(MessageWindow) ? MessageWindow.transparent? : false))
    end
    UI.nummer_eingabe(@interpreter.num_input) if @interpreter.busy_number?
    Battle.draw if defined?(Battle) && (Battle.active? || @interpreter.busy_battle?)
    UI.text(16, 54, "> #{@prompt}  (E)") if @prompt && !@interpreter.running?
    draw_hover_label if zeige
    draw_crosshair

    # Etappe 19d: Bildschirmblende über Spiel und UI (das Menü-Overlay bleibt
    # sichtbar, sonst könnte man im abgedunkelten Zustand nichts mehr wählen).
    Fade.draw(Rune::Window.width, Rune::Window.height)

    # Menü-Overlay ganz zum Schluss.
    if @menu
      UI.abdunkeln(Rune::Window.width)
      draw_menu
    end
  end
end

Game.setup