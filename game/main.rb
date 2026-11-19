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

  # Karte laden: ID, Events, Objekte, Editor - und Spieler an den Startpunkt.
  # Wird vom ersten Setup und vom Kartenwechsel im Editor benutzt.
  # Himmel/Nebel/Panorama aus der Karte übernehmen (Etappe 17b).
  # Panorama-Farbe ersetzt den Himmelverlauf (Vollfarbe), Nebel blendet
  # die Ferne zur Nebelfarbe. Ohne Angaben: Standard-Himmel, kein Nebel.
  def self.apply_map_atmosphere(map)
    pan = map['panorama'] || {}
    if pan['color']
      c = pan['color']
      Rune::Gfx.sky(c, c)
    else
      Rune::Gfx.sky(0x3D6FB5, 0xBFD8EC)
    end
    fog = map['fog'] || {}
    Rune::Gfx.fog(fog['on'] ? true : false, fog['color'] || 0xBFD8EC,
                  fog['far'] || 180.0)
  end

  # Etappe 18l: Terrain einer Karte aufbauen (Grösse/Seed/Amplitude plus
  # gemalte Berge/Senken aus terrain.heights) und den Boden texteiren.
  # Wird vom Setup und von jedem Kartenwechsel (enter_map) gerufen - damit
  # Kartengrenze und gemalte Hügel auch nach dem Wechsel stimmen.
  def self.build_terrain(map)
    t = map['terrain'] || {}
    hb = t['heights']
    hb = nil unless hb.is_a?(Array) && !hb.empty?
    Rune::World.generate(width: t['width'] || t['size'] || 240,
                         height: t['height'] || t['size'] || 240,
                         resolution: t['resolution'] || 192,
                         seed: t['seed'] || 7, amplitude: t['amplitude'] || 0.0,
                         height_brushes: hb)
    # Etappe 18c/18d: Boden. Mit Boden-Flächen (splats) Atlas pro Quad,
    # sonst Einzeltextur + Kachelgrösse.
    t_splats = t['splats']
    if t_splats.is_a?(Array) && !t_splats.empty?
      Rune::Gfx.terrain_splats(t['texture'] || 'gras', t_splats)
    else
      Rune::Gfx.terrain_texture(t['texture'] || 'gras', t['tile'] || 2.0)
    end
  end

  def self.enter_map(id)
    @map_id = id
    @map = Database.map(id)
    Fade.reset!   # Etappe 19d: nie mit dunklem Bildschirm in eine Karte kommen
    @events = (@map['events'] || []).map { |h| Event.new(h) }
    @map['objects'] ||= []
    # Etappe 18l: Terrain gehört zur Karte - beim Wechsel neu aufbauen,
    # damit Grösse (Kartengrenze!) und gemalte Berge zur Karte passen.
    build_terrain(@map)
    apply_map_atmosphere(@map)

    @interpreter = Interpreter.new
    @scene = build
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

    @editor = Editor.new(@map_id, @map['objects'])
    Editor.rebuild(@map['objects'], @scene)
    @hover = nil
    @hover_prev = nil
    @hover_dist = 0.0
    @prompt = nil

    # Startpunkt: erst die Karte, dann system.json.
    st = @map['start'] || Database.system['start'] || { 'x' => 0.0, 'z' => 0.0 }
    Rune::Player.teleport(st['x'].to_f, gy(st['x'].to_f, st['z'].to_f) + 0.2, st['z'].to_f)
    Rune::Camera.yaw = (st['yaw'] || Math::PI).to_f
    Rune::Log.info("Karte '#{@map['name']}' aktiv: #{@events.size} Events, " \
                   "#{@map['objects'].size} Objekte, #{Rune::Physics.solid_count} Hindernisse")
  end

  # Karte im Editor wechseln (z. B. nach N = neue Karte oder V/B).
  def self.change_map(id)
    @editor.save(@editor.objects, @map) if @editor.dirty?
    enter_map(id)
  end

  # ---------------------------------------------------- Speichern / Laden

  # Spielstand in Slot 1..3 ablegen: Karte, Position, Blick, Kamera,
  # Schalter/Variablen, ausgelöste Events, Spielzeit.
  def self.save_game(slot)
    px, py, pz = Rune::Player.position
    data = {
      'map'       => @map_id,
      'player'    => { 'x' => px, 'y' => py, 'z' => pz,
                       'yaw' => Rune::Camera.yaw, 'mode' => Rune::Camera.mode.to_s },
      'state'     => GameState.snapshot,
      'party'     => Party.snapshot,
      'fired'     => @events.select(&:fired).map(&:name),
      'npcs'      => @npcs.map(&:snapshot),
      'time'      => @time,
      'saved'     => Time.now.strftime('%Y-%m-%d %H:%M'),
    }
    Project.save($project_name, slot, data)
    Rune::Log.info("Gespeichert: Slot #{slot} (Karte '#{@map['name']}', " \
                   "#{data['fired'].size} Events ausgelöst)")
    true
  rescue StandardError => e
    Rune::Log.error("Speichern fehlgeschlagen: #{e.message}")
    false
  end

  # Spielstand laden: Karte neu aufbauen, Zustand und Position übernehmen.
  def self.load_game(slot)
    data = Project.load_save($project_name, slot)
    unless data
      Rune::Log.warn("Laden fehlgeschlagen: Slot #{slot} ist leer")
      return false
    end
    GameState.restore!(data['state'] || {})
    Party.setup(Database.database)
    Party.restore!(data['party'] || {})
    @intro = nil   # Intro nicht nochmal zeigen

    map_id = data['map']
    map_id = Database.map_ids.first unless Database.map(map_id)
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
      n.restore!(h['x'], h['z'], h['wp'])
      Rune::Log.info(format('NPC %s wiederhergestellt: x=%.2f z=%.2f Wegpunkt %d',
                            n.name, n.position[0], n.position[1], h['wp'].to_i))
    end

    pl = data['player'] || {}
    Rune::Player.teleport(pl['x'].to_f, pl['y'].to_f, pl['z'].to_f)
    Rune::Camera.yaw = pl['yaw'].to_f
    Rune::Camera.mode = (pl['mode'] || 'third').to_sym
    @time = data['time'].to_f
    @last_pos = nil
    @was_air = nil
    @step_dist = 0.0
    Rune::Log.info("Geladen: Slot #{slot} (Karte '#{@map['name']}', Spielzeit #{@time.to_i}s)")
    true
  rescue StandardError => e
    Rune::Log.error("Laden fehlgeschlagen: #{e.message}")
    false
  end

  # ------------------------------------------------------------- Menü (Esc)

  def self.menu_items
    items = [{ 'label' => 'Zurück zum Spiel', 'act' => [:resume] }]
    1.upto(3) do |s|
      info = Project.save_info($project_name, s)
      label = "Speichern Slot #{s}" + (info ? "   (belegt: #{info['saved']})" : '   (leer)')
      items << { 'label' => label, 'act' => [:save, s] }
    end
    1.upto(3) do |s|
      info = Project.save_info($project_name, s)
      label = "Laden     Slot #{s}" + (info ? "   (#{info['saved']})" : '   (leer)')
      items << { 'label' => label, 'act' => [:load, s], 'off' => info.nil? }
    end
    items << { 'label' => 'Zum Titelbildschirm', 'act' => [:title] }
    items
  end

  def self.open_menu
    @menu_sel = 0
    @menu = true
    Rune::Input.capture = false   # Menü mit freiem Zeiger
    Rune::Sound.play('klick', 0.6)
    Rune::Log.info('Menü geöffnet')
  end

  def self.close_menu
    @menu = nil
    Rune::Input.capture = true
    Rune::Sound.play('klick', 0.5)
    Rune::Log.info('Menü geschlossen')
  end

  def self.update_menu
    items = menu_items
    n = items.size
    @menu_sel = (@menu_sel - 1) % n if Rune::Input.pressed?(:up)
    @menu_sel = (@menu_sel + 1) % n if Rune::Input.pressed?(:down)
    items.each_with_index do |it, i|
      menu_action(it) if i < 9 && Rune::Input.pressed?((i + 1).to_s)
    end
    menu_action(items[@menu_sel]) if Rune::Input.pressed?(:enter)
  end

  def self.menu_action(it)
    Rune::Log.info("Menü: #{it['label']}")
    case it['act'][0]
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
      @menu = nil
      @mode = :launcher
      Launcher.activate   # aktiviert dort auch die Titel-Musik neu
    end
  end

  def self.draw_menu
    w = Rune::Window.width
    h = Rune::Window.height
    items = menu_items
    mw = 480
    mh = 66 + items.size * 34 + 12
    mx = w / 2 - mw / 2
    my = h / 2 - mh / 2
    UI.panel(mx, my, mw, mh, UI::PANEL, 0.95)
    Launcher.rahmen(mx, my, mw, mh, UI::ACCENT, 0.6)
    UI.text(mx + 20, my + 16, 'Menü', UI::TEXT, 20)
    items.each_with_index do |it, i|
      y = my + 52 + i * 34
      sel = (i == @menu_sel)
      UI.panel(mx + 12, y - 3, mw - 24, 30, sel ? 0x2A4A66 : 0x101826, sel ? 0.95 : 0.5)
      UI.text(mx + 24, y, (sel ? '> ' : '  ') + it['label'],
              it['off'] ? UI::TEXT_DIM : UI::TEXT, 15)
    end
    UI.text(mx + 20, my + mh - 26,
            '[Hoch/Runter] Wählen   [Enter] OK   [1..8] direkt   [Esc] Zurück',
            UI::TEXT_DIM, 13)

    # Etappe 17g: Party-Status (Helden, Gold, Inventar) neben dem Menü.
    lines = Party.status_lines
    pw = 380
    ph = 60 + lines.size * 28
    px = mx - pw - 24
    py = my
    if px < 12   # bei schmalen Fenstern unter das Menü
      px = mx
      py = my + mh + 16
      return if py + ph > h   # kein Platz: ganz weglassen
    end
    UI.panel(px, py, pw, ph, UI::PANEL, 0.95)
    Launcher.rahmen(px, py, pw, ph, UI::ACCENT, 0.6)
    UI.text(px + 20, py + 14, 'Party', UI::TEXT, 18)
    lines.each_with_index do |line, i|
      UI.text(px + 20, py + 44 + i * 28, line, UI::TEXT, 14)
    end
  end

  # ---------------------------------------------------------------- Ablauf

  def self.setup
    @time = 0.0
    @debug = false
    @debug_timer = 0.0
    @prompt = nil
    Issues.clear!   # Etappe 18h: Backlog startet pro Session/F5 neu

    # Ohne Projekt: Titelbildschirm (RPG-Maker-Stil). Erst die Projektwahl
    # legt $project_root fest und ruft dieses Setup erneut auf.
    # Ausnahme (Etappe 17h): RUNE_PROJECT aus --project startet direkt -
    # Playtests aus dem Editor springen ohne Titelbildschirm ins Projekt.
    if $project_root.nil?
      env_project = ENV['RUNE_PROJECT'].to_s.strip
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
      else
        @mode = :launcher
        Launcher.activate
        return
      end
    end
    @mode = :game

    # Spielzustand aus system.json zurücksetzen (bei F5 ebenfalls).
    GameState.reset!(Database.system)
    Party.setup(Database.database)
    Scripts.load_project

    map_id = Database.system.dig('start', 'map') || Database.map_ids.first || 'welt01'

    # Optik: prozedurale Texturen, Himmel/Nebel/Panorama aus der Startkarte.
    Textures.register_all
    Textures.register_project
    # Etappe 18l: Terrain-Bau ist nach enter_map gewandert (Kartenwechsel
    # baut es ebenfalls auf, damit Kartengrenze und gemalte Berge stimmen).
    enter_map(map_id)
    Rune::Camera.mode = :third
    Rune::Camera.distance = 7   # Launcher-Überblick (28) -> normaler Follow-Cam
    Rune::Sound.loop('musik', 0.22)   # im Spiel etwas lauter als im Titel

    # Intro-Text als Event am Kartenstart (auto-Trigger desselben Effekts).
    @intro = ['Willkommen in Rune3D.  WASD = laufen, Shift = rennen, Leertaste = springen.',
              'E = mit Personen/Kristallen reden, F1/F3 = Kamera, F5 = Ruby neu laden.']
  end

  def self.update(dt)
    # Titelbildschirm: hier endet der Frame, bis ein Projekt gewählt ist.
    if @mode == :launcher
      Launcher.update(dt)
      return
    end

    @time += dt
    Rune::Log.info("Spielzeit #{@time.to_i}s") if (@time - dt).to_i != @time.to_i && @time.to_i % 30 == 0

    if Rune::Input.pressed?(:f2)
      @debug = !@debug
      Rune::Log.info("Debug-Anzeige: #{@debug ? 'an' : 'aus'}")
    end

    # Menü (Esc): friert Spiellogik, Interpreter und Events ein.
    if Rune::Input.pressed?(:escape)
      @menu ? close_menu : open_menu
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

    # Sound: Schritte, Sprung, Landung (nur im Spiel, nicht im Menü).
    unless @mode == :launcher
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
    px, py, pz = Rune::Player.position
    Weather.update(dt, px, py, pz)   # Etappe 17h: Regen/Schnee um den Spieler

    unless @interpreter.running?
      px, py, pz = Rune::Player.position

      # Aktive Event-Seite bestimmen (Etappe 18g): höchste Seite mit
      # erfüllten Bedingungen gewinnt; Wechsel setzt 'fired' zurück.
      @events.each(&:refresh_page!)

      # NPCs bewegen (Position des zugehörigen Events folgt).
      @npcs.each { |n| n.update(dt) }

      # NPC-Kollision (Etappe 15): Spieler aus NPCs herausdrücken.
      @npcs.each do |n|
        nx, nz = n.position
        Rune::Player.push_out(nx, nz, n.block_radius)
      end

      # Auto-/Touch-Events (einmalig je Kartenstart)
      @events.each do |e|
        next unless %w[touch auto].include?(e.trigger)
        next if e.fired
        if (e.trigger == 'auto') || e.near?(px, pz)
          e.fired = true
          Rune::Log.info("Event '#{e.name}' ausgelöst (#{e.trigger})")
          e.manual_fire(@interpreter)
          break
        end
      end

      # Parallel-Events (Etappe 18f, RPG-Maker-Parallel-Process): laufen im
      # Hintergrund und starten sich neu, sobald sie fertig sind. Damit sie
      # nicht die Engine fluten, nur wenn der Interpreter gerade frei ist —
      # ein 'wait' im Event steuert das Tempo (wie bei RPG Maker üblich).
      @events.each do |e|
        next unless e.trigger == 'parallel'
        next if @interpreter.running?
        e.manual_fire(@interpreter)
      end

      # Action-Events: nächstes in Reichweite anbieten (E drucken)
      @prompt = nil
      @events.each do |e|
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

    # Editor-/Playtest-Modus (F4): Editor = alles eingefroren, Playtest =
    # Spiel läuft; F4 im Playtest springt zurück in den Editor (Etappe 16b).
    if Rune::Input.pressed?(:f4)
      if @playtest
        playtest_stop
      else
        @editor.toggle(@scene)
      end
    end

    if @editor.active?
      # Scene-Editor-Prinzip (Etappe 16b): Der Editor arbeitet auf den
      # Kartendaten, die Game-Engine-Simulation steht. Ist der Spieler noch
      # in der Luft (z.B. nach dem Playtest), darf er erst landen, dann
      # friert die Physik endgültig ein — nie schwebt er eingefroren.
      if Rune::Player.on_ground?
        Rune::Simulation.paused = true
      else
        Rune::Simulation.paused = false
      end
      @editor.update(dt, @scene, @map)
      # Szene NICHT updaten: Animationen (Kristall ...) stehen im Editor still.
      update_hover_only
      return
    end
    Rune::Simulation.paused = false

    @scene.update(dt)
    update_picking
  end

  # ---------------------------------------------------- Playtest (Etappe 16b)

  # Aus dem Editor heraus testen: ungespeicherte Änderungen zürst sichern
  # (der Editierstand bleibt heilig), dann normal spielen.
  def self.playtest_start
    @editor.save(@editor.objects, @map) if @editor.dirty?
    @editor.toggle(@scene) if @editor.active?
    @playtest_state = GameState.snapshot   # Testzustand später verwerfen
    @playtest = true
    Rune::Simulation.paused = false
    Rune::Log.info('Playtest gestartet (F4 zurück zum Editor)')
  end

  # Zurück in den Editor: Karte frisch laden = unveränderter Editierstand;
  # alles, was der Test am Spielzustand verändert hat, wird verworfen.
  def self.playtest_stop
    @playtest = nil
    GameState.restore!(@playtest_state) if @playtest_state
    @playtest_state = nil
    enter_map(@map_id)
    @editor.toggle(@scene)
    Rune::Log.info('Zurück im Editor (Karte neu geladen, Testzustand verworfen)')
  end

  def self.playtest?
    @playtest ? true : false
  end

  # ------------------------------------------------------------- Maus-Picking

  # Alle anklickbaren Dinge: Szenenknoten + Events + NPCs.
  def self.pickables
    list = []
    @scene.walk(@scene.root) { |n| list << n if n != @scene.root && n.visible }
    list.concat(@events)
    list.concat(@npcs.map(&:event))
    list
  end

  # Zuletzt unter dem Zeiger getroffener Knoten (für den Editor, 1 Frame alt).
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

  # Im Editor: Rechtsklick löscht. Zürst das genau getroffene Objekt; wenn
  # der Strahl nur Boden getroffen hat (dünne Teile!), das nächstliegende
  # Editor-Objekt zum Bodenpunkt.
  def self.update_hover_only
    compute_hover
    return unless Rune::Input.mouse_pressed?(1)
    if @hover && Editor.object_index(@hover.name)
      @editor.delete_node(@hover, @scene)
    elsif @hover.nil?
      h = @editor.hit
      @editor.delete_near(h[0], h[2], @scene) if h
    end
    @hover = nil
    @hover_prev = nil
  end

  def self.update_picking
    compute_hover
    return unless Rune::Input.mouse_pressed?(0) && @hover

    if @hover.is_a?(Event)
      Rune::Log.info(format("Klick: Event '%s' (%.1f m)", @hover.name, @hover_dist))
      @hover.manual_fire(@interpreter) unless @interpreter.running?
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
      if label.start_with?('ed_')
        idx = Editor.object_index(label)
        obj = idx && @map['objects'][idx]
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
    if @mode == :launcher
      Launcher.draw
      return
    end

    Rune::Gfx.clear_color = 0x131A24

    # Der Spieler existiert nur als Punkt; im Third-Person-Modus bekommt er einen Körper.
    if Rune::Camera.mode == :third
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

    # 2D-Overlay zuletzt
    UI.hud
    if @playtest
      UI.panel(1000, 12, 264, 30, UI::PANEL, 0.6)
      UI.text(1012, 18, 'PLAYTEST — F4 = zurück zum Editor', UI::WARN, 15)
    end
    UI.message(@interpreter.message) if @interpreter.busy_text?
    if @interpreter.busy_choices?
      c = @interpreter.choices
      UI.choices(c[:options], c[:sel])
    end
    UI.text(16, 54, "> #{@prompt}  (E)") if @prompt && !@interpreter.running?
    draw_hover_label(@editor.active? ? 248 : 16, @editor.active? ? 88 : 78) if zeige
    draw_crosshair

    if @editor.active?
      @editor.draw_hit_marker
      @editor.draw_selection
      @editor.draw_ui
    end

    # Etappe 19d: Bildschirmblende über Spiel und UI (das Menü-Overlay bleibt
    # sichtbar, sonst könnte man im abgedunkelten Zustand nichts mehr wählen).
    Fade.draw(Rune::Window.width, Rune::Window.height)

    # Menü-Overlay ganz zum Schluss.
    if @menu
      Launcher.abdunkeln(Rune::Window.width)
      draw_menu
    end
  end
end

Game.setup