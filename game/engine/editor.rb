#encoding: utf-8
# ============================================================================
#  Editor (Etappe 16a/16b/18m): klickbare Oberfläche mit Tab-Leiste.
#
#  ARCHITEKTUR — zwei getrennte Engines:
#
#    Scene-Editor (diese Datei)        Game-Engine (main.rb-Spielzweig)
#    ---------------------------------------------------------------------
#    arbeitet NUR auf den Kartendaten  interpretiert die Kartendaten
#    (@objects = JSON-Modell)          (@events, @npcs, @interpreter,
#    Preview-Nodes (ed_*) sind nur     GameState, Physik, Sound)
#    Anzeige, kein Spielzustand
#    Simulation eingefroren            Simulation läuft (Playtest)
#
#  Der Editor kennt keine Spiellogik: Er verändert Daten und baut die
#  Preview neu. Der Playtest (▶ Testen) sichert erst die Daten, lässt
#  dann die Game-Engine laufen und beim Zurückschalten (F4) wird die
#  Karte frisch geladen — der Editierstand bleibt heilig.
#
#  Tabs (Etappe 18m erweitert): OBJEKTE (setzen/verschieben/löschen/
#  drehen), BODEN (Flächen malen), HÖHE (Berge/Senken), EVENTS + NPCs
#  (Listenansicht), KARTE (Wechseln/Neu/Speichern).
#
#  Bedienung:
#    F4          Editor an/aus (im Playtest: zurück zum Editor)
#    Linksklick  Aktion des gewählten Werkzeugs
#    Rechtsklick Objekt unter dem Zeiger löschen
#    Shift+Klick Radierer (Boden) / Senke (Höhe) / negativ drehen
#    1..9        Prefab-Typ wählen (Tastatur-Shortcut)
#    R / G       Drehung +30 Grad / Raster umschalten
#    +/-         Pinselgrösse (im BODEN/HÖHE-Tab)
#    Strg+S      Karte speichern
#    N / V / B   neue Karte / zurück / vor
#
#  Gespeichert werden objects plus die Mal-Ebenen terrain.splats und
#  terrain.heights; die Geometrie kommt aus den Prefabs. Beim Verlassen
#  des Editors (F4) wird automatisch gespeichert (Etappe 18m).
# ============================================================================

require 'ui'

class Editor
  TABS = [%w[objekte OBJEKTE], %w[boden BODEN], %w[hoehe HÖHE],
          %w[events EVENTS], %w[npcs NPCS], %w[karte KARTE]].freeze

  # Bodentaugliche Texturen für den Mal-Pinsel (aus Textures::LIST).
  BRUSH_TEX = %w[gras weg fels holz].freeze

  attr_reader :rotation, :tab, :tool, :selected

  def initialize(map_id, objects)
    @map_id  = map_id
    @objects = objects        # Array aus {"type","x","z","rot"}, direkt aus dem JSON
    @active  = false
    @sel     = 0              # Index in Prefabs::LIST
    @rotation = 0.0
    @grid    = 1.0
    @hit     = nil            # [x, y, z] unter dem Zeiger
    @dirty   = false
    @status  = nil
    @status_t = 0.0
    @save_lock = 0.0
    @next_id  = (@objects.size > 0 ? @objects.size : 0)

    @tab      = :objekte
    @tool     = :place        # :place, :move, :paint, :height
    @selected = nil           # Index des ausgewählten Objekts
    @grab     = nil           # Index des verschobenen Objekts
    @map_ref  = nil
    @scene_ref = nil

    # Etappe 18m: Mal-Werkzeuge (Boden/Höhe) wie im Standalone-Editor.
    @paint_tex = 'weg'        # Textur des Boden-Pinsels
    @paint_size = 8.0         # Pinselkante in Metern (Boden + Höhe)
    @height_step = 2.0        # Berg/Senke pro Klick in Metern
  end

  def active? = @active
  def objects = @objects
  def dirty? = @dirty
  def hit = @hit          # letzter Bodenpunkt unter dem Zeiger (für Lösch-Fallback)

  # Prefab-Objektindex aus einem Knotennamen ("ed_7_2" -> 7)
  def self.object_index(node_name)
    return nil unless node_name.to_s.start_with?('ed_')
    node_name.to_s.split('_')[1].to_i
  end

  def toggle(scene)
    # Etappe 18m: Beim Verlassen automatisch speichern - ungespeicherte
    # Arbeit soll nie mehr verloren gehen ("Speichern fehlt").
    if @active
      autosave_on_exit
    end
    @active = !@active
    # Im Editor braucht man den echten Zeiger, im Spiel den Mausfang.
    Rune::Input.capture = !@active
    @grab = nil if @active
    Rune::Log.info("Editor: #{@active ? 'an' : 'aus'} (#{@objects.size} Objekte)")
    @active
  end

  def current_prefab = Prefabs::LIST[@sel]

  def set_status(text)
    @status = text
    @status_t = 3.0
    Rune::Log.info("Editor: #{text}")
  end

  def autosave_on_exit
    return unless @dirty
    save(@objects, @map_ref || {})
  rescue => e
    Rune::Log.error("Editor: Auto-Save fehlgeschlagen: #{e.message}")
  end

  # ------------------------------------------------------------- Strahl -> Boden

  # Marschiert den Strahl entlang, bis er das Gelände durchstösst.
  def self.terrain_hit(origin, dir, max_t: 160.0, step: 0.4)
    return nil unless Rune::World.ready?
    t = 0.5
    while t < max_t
      x = origin[0] + dir[0] * t
      y = origin[1] + dir[1] * t
      z = origin[2] + dir[2] * t
      h = Rune::World.height_at(x, z)
      return [x, h, z] if y <= h && y > -80.0
      return nil if y < -80.0
      t += step
    end
    nil
  end

  def snap(v) = (@grid > 0.0 ? (v / @grid).round * @grid : v)

  # ------------------------------------------------------- Malen (Etappe 18m)

  # terrain-Hash der Karte anlegen/holen (Karten-JSON, im Speicher).
  def map_terrain
    map = @map_ref || {}
    map['terrain'] ||= {}
    map['terrain']
  end

  # Boden-Fläche an (x,z) malen (erase=false) oder die zuletzt
  # überdeckende Fläche radiert (erase=true). Snap aufs Raster, danach
  # Live-Anwendung über terrain_splats/terrain_texture.
  def paint_ground(x, z, erase)
    t = map_terrain
    size = @paint_size
    x = snap(x)
    z = snap(z)
    splats = t['splats'] || []
    if erase
      i = splats.rindex do |s|
        s['x'].to_f <= x && x < s['x'].to_f + s['w'].to_f &&
          s['z'].to_f <= z && z < s['z'].to_f + s['h'].to_f
      end
      return false unless i
      splats.delete_at(i)
      t['splats'] = splats
      set_status("Boden radiusert bei #{x.round}/#{z.round}")
    else
      splats << { 'tex' => @paint_tex, 'x' => (x - size * 0.5).round(3),
                  'z' => (z - size * 0.5).round(3),
                  'w' => size, 'h' => size }
      t['splats'] = splats
      set_status("Boden gemalt: #{@paint_tex} bei #{x.round}/#{z.round} (#{size.round} m)")
    end
    @dirty = true
    apply_ground!
    true
  end

  # Höhen-Pinsel {x,z,r,h} in terrain.heights: Klick = Berg (+@height_step),
  # Shift+Klick = Senke. Klicks in dieselbe Rasterzelle (gleicher Radius)
  # stapeln zu einem Eintrag, damit die JSON-Liste klein bleibt.
  def paint_height(x, z, sink)
    t = map_terrain
    x = snap(x)
    z = snap(z)
    r = [@paint_size * 0.5, @grid].max
    h = sink ? -@height_step : @height_step
    heights = t['heights'] ||= []
    cell = @grid * 0.5
    e = heights.index do |hh|
      (hh['x'].to_f - x).abs <= cell && (hh['z'].to_f - z).abs <= cell &&
        (hh['r'].to_f - r).abs < 1e-3
    end
    if e
      heights[e]['h'] = (heights[e]['h'].to_f + h).round(3)
    else
      heights << { 'x' => x.round(3), 'z' => z.round(3),
                   'r' => r.round(3), 'h' => h.round(3) }
    end
    @dirty = true
    set_status("#{sink ? 'Senke' : 'Berg'} bei #{x.round}/#{z.round} (#{"%.1f" % h.abs} m)")
    apply_heights!
    true
  end

  # Boden live neu texteiren (gleicher Pfad wie enter_map: Atlas wenn
  # Splats vorhanden sind, sonst Einzeltextur).
  def apply_ground!
    t = map_terrain
    splats = t['splats']
    if splats.is_a?(Array) && !splats.empty?
      Rune::Gfx.terrain_splats(t['texture'] || 'gras', splats)
    else
      Rune::Gfx.terrain_texture(t['texture'] || 'gras', t['tile'] || 2.0)
    end
  rescue => e
    Rune::Log.error("Editor: Boden-Neubau fehlgeschlagen: #{e.message}")
  end

  # Höhen live anwenden: Pinsel setzen und Terrain mit den Karten-
  # Parametern neu generieren (build_terraform der Karte).
  def apply_heights!
    tt = (@map_ref || {})['terrain'] || {}
    Rune::World.height_brushes = (tt['heights'] || []).map do |hh|
      [hh['x'].to_f, hh['z'].to_f, hh['r'].to_f, hh['h'].to_f]
    end
    Rune::World.generate(width: tt['width'] || tt['size'] || 240,
                        height: tt['height'] || tt['size'] || 240,
                        resolution: tt['resolution'] || 192,
                        seed: tt['seed'] || 7,
                        amplitude: tt['amplitude'] || 0.0)
    if @scene_ref
      Editor.rebuild(@objects, @scene_ref)
      Game.refresh_placement_visuals(@scene_ref) if Game.respond_to?(:refresh_placement_visuals)
    end
  rescue => e
    Rune::Log.error("Editor: H\xC3\xB6hen-Neubau fehlgeschlagen: #{e.message}")
  end

  # ---------------------------------------------------------------- Aktionen

  def place(objects, scene, x, z, rot)
    obj = { 'type' => current_prefab[:id],
            'x' => snap(x).round(3),
            'z' => snap(z).round(3),
            'rot' => rot.round(3) }
    objects << obj
    @dirty = true
    set_status("gesetzt: #{current_prefab[:name]} bei #{obj['x']}/#{obj['z']}")
    obj
  end

  def delete_at(index, scene)
    return unless index && @objects[index]
    obj = @objects.delete_at(index)
    @selected = nil if @selected == index
    @grab = nil if @grab == index
    @dirty = true
    set_status("gelöscht: #{Prefabs.name_of(obj['type'])}")
    obj
  end

  # Objekt an eine neue Position setzen (Verschieben-Werkzeug).
  def move_object(index, x, z, scene)
    obj = @objects[index]
    return unless obj
    obj['x'] = snap(x).round(3)
    obj['z'] = snap(z).round(3)
    @dirty = true
    set_status("verschoben: #{Prefabs.name_of(obj['type'])} -> #{obj['x']}/#{obj['z']}")
    Editor.rebuild(@objects, scene)
    obj
  end

  # Ausgewähltes Objekt um ±30 Grad drehen.
  def rotate_selected(delta, scene)
    obj = @objects[@selected]
    return unless obj
    obj['rot'] = ((obj['rot'].to_f + delta) % (Math::PI * 2.0)).round(3)
    @dirty = true
    set_status("#{Prefabs.name_of(obj['type'])}: #{(obj['rot'] * 180.0 / Math::PI).round} Grad")
    Editor.rebuild(@objects, scene)
  end

  # Baut alle Editor-Objekte neu in die Szene (einfachster Weg nach Änderungen).
  def self.rebuild(objects, scene)
    scene.root.children.reject! { |n| n.name.start_with?('ed_') }
    objects.each_with_index do |obj, i|
      Prefabs.instantiate(scene, obj, Game.gy(obj['x'].to_f, obj['z'].to_f), "ed_#{i}")
    end
    scene.refresh_solids
  end

  # Speichert die Karte komplett: Objekte UND die Mal-Ebenen (terrain.splats,
  # terrain.heights), weil terrain Teil des Karten-JSON ist und im Speicher
  # von uns verändert wurde (Etappe 18m - "Speichern fehlt").
  def save(objects, map)
    path = File.join(Database.root, 'maps', "#{@map_id}.json")
    map['objects'] = @objects
    File.write(path, JSON.pretty_generate(map) + "\n", encoding: 'UTF-8')
    @dirty = false
    set_status("gespeichert: #{@objects.size} Objekte + Boden + H\xC3\xB6hen -> maps/#{@map_id}.json")
    true
  rescue => e
    Rune::Log.error("Editor: Speichern fehlgeschlagen: #{e.message}")
    false
  end

  # ---------------------------------------------------------------- Klicks

  # Ein Button-Klick aus der UI (Kennung) aufschlüsseln und ausführen.
  # Rückgabe: true, wenn die Karte gewechselt wurde (Update abbrechen).
  def handle_click(label, scene, map)
    case label
    when /\Atab:(\w+)\z/
      @tab = Regexp.last_match(1).to_sym
      @grab = nil
      # Tab direkt mit Werkzeug verdrahten (Etappe 18m): BODEN = malen,
      # HÖHE = Berge, OBJEKTE = Setzen statt Mal-Werkzeug.
      @tool = :paint  if @tab == :boden
      @tool = :height if @tab == :hoehe
      if @tab == :objekte && (@tool == :paint || @tool == :height)
        @tool = :place
      end
    when /\Aprefab:(.+)\z/
      id = Regexp.last_match(1)
      idx = Prefabs::LIST.index { |p| p[:id] == id }
      if idx
        @sel = idx
        @tool = :place
        @tab = :objekte
        @grab = nil
        set_status("Typ: #{current_prefab[:name]}")
      end
    when 'tool:place'
      @tool = :place
      @tab = :objekte
      @grab = nil
    when 'tool:move'
      @tool = :move
      set_status('Verschieben: Objekt anklicken, dann Ziel anklicken')
    when 'rot:-' then bump_rotation(-Math::PI / 6.0)
    when 'rot:+' then bump_rotation(Math::PI / 6.0)
    when 'grid'
      @grid = (@grid == 1.0 ? 0.25 : 1.0)
      set_status("Raster: #{@grid} m")
    when 'save'
      return false if @save_lock.to_f > 0.0
      @save_lock = 0.4
      save(@objects, map)
    # --- Mal-Werkzeuge (Etappe 18m) ---
    when /\Abrushtex:(.+)\z/
      @paint_tex = Regexp.last_match(1)
      set_status("Pinsel-Textur: #{@paint_tex}")
    when 'brush+'
      @paint_size = [[@paint_size + 2.0, 64.0].min, 0.25].max
      set_status("Pinsel: #{@paint_size} m")
    when 'brush-'
      @paint_size = [[@paint_size - 2.0, 0.25].max, 0.25].max
      set_status("Pinsel: #{@paint_size} m")
    when 'newmap'
      id = Project.create_map
      set_status("Neue Karte: #{id}")
      Game.change_map(id)
      return true
    when /\Amap:(.+)\z/
      id = Regexp.last_match(1)
      if Database.map(id)
        Game.change_map(id)
        return true
      end
    when 'playtest'
      Game.playtest_start
    when 'sel:rot'
      rotate_selected(Math::PI / 6.0, scene)
    when 'sel:delete'
      delete_at(@selected, scene)
      Editor.rebuild(@objects, scene)
    end
    false
  end

  def bump_rotation(delta)
    @rotation = (@rotation + delta) % (Math::PI * 2.0)
    set_status(format('Drehung: %.0f Grad', @rotation * 180.0 / Math::PI))
  end

  # ---------------------------------------------------------------- Update

  def update(dt, scene, map)
    @status_t -= dt if @status_t > 0.0
    @save_lock -= dt if @save_lock.to_f > 0.0
    @map_ref = map
    @scene_ref = scene

    click = UI.clicked
    return if click && handle_click(click, scene, map)   # Kartenwechsel -> neu eintauchen

    # Tastatur-Shortcuts (neben der Maus-UI)
    9.times do |i|
      next if i >= Prefabs::LIST.size
      next unless Rune::Input.pressed?((i + 1).to_s)
      @sel = i
      @tool = :place
      set_status("Typ: #{current_prefab[:name]}")
    end
    bump_rotation(Math::PI / 6.0 * (Rune::Input.down?(:shift) ? -1.0 : 1.0)) if Rune::Input.pressed?(:r)
    if Rune::Input.pressed?(:g)
      @grid = (@grid == 1.0 ? 0.25 : 1.0)
      set_status("Raster: #{@grid} m")
    end
    # Pinselgrösse mit +/- (Etappe 18m, BODEN/HÖHE-Tab)
    if @tab == :boden || @tab == :hoehe
      if Rune::Input.pressed?(:add) || Rune::Input.pressed?(:equals) ||
         Rune::Input.pressed?(:plus)
        @paint_size = [[@paint_size + 2.0, 64.0].min, 0.25].max
        set_status("Pinsel: #{@paint_size} m")
      elsif Rune::Input.pressed?(:minus) || Rune::Input.pressed?(:subtract)
        @paint_size = [[@paint_size - 2.0, 0.25].max, 0.25].max
        set_status("Pinsel: #{@paint_size} m")
      end
    end
    if Rune::Input.pressed?(:n)
      id = Project.create_map
      set_status("Neue Karte: #{id}")
      Game.change_map(id)
      return
    end
    if Rune::Input.pressed?(:v) || Rune::Input.pressed?(:b)
      ids = Database.map_ids
      unless ids.empty?
        i = ids.index(@map_id) || 0
        dir = Rune::Input.pressed?(:b) ? 1 : -1
        Game.change_map(ids[(i + dir) % ids.size])
        return
      end
    end
    if Rune::Input.pressed?(:s) && Rune::Input.down?(:ctrl) && @save_lock <= 0.0
      @save_lock = 0.4
      save(@objects, map)
    end

    # Keine Welt-Aktion, wenn die Maus auf der UI steht oder ein Klick dort landete.
    return if click || UI.over_ui?

    # Zielpunkt unter dem Zeiger
    mx, my = Rune::Input.mouse_position
    ray = Rune::Camera.ray(mx, my)
    @hit = ray.empty? ? nil : Editor.terrain_hit(ray[0, 3], ray[3, 3])
    return if @hit.nil?

    shift = Rune::Input.down?(:shift)

    case @tool
    when :paint   # Etappe 18m: Boden malen / Shift radiert
      if Rune::Input.mouse_pressed?(0)
        paint_ground(@hit[0], @hit[2], shift)
      end
    when :height  # Etappe 18m: Berge / Shift = Senken
      if Rune::Input.mouse_pressed?(0)
        paint_height(@hit[0], @hit[2], shift)
      end
    when :move
      if @grab
        # Getragenes Objekt am Bodenpunkt ablegen.
        if Rune::Input.mouse_pressed?(0)
          move_object(@grab, @hit[0], @hit[2], scene)
          @grab = nil
        end
      elsif Rune::Input.mouse_pressed?(0)
        # Objekt unter dem Zeiger greifen (Hover kommt aus main.rb, 1 Frame alt).
        idx = Editor.object_index(Game.respond_to?(:hover) ? Game.hover&.name : nil)
        if idx && @objects[idx]
          @grab = idx
          @selected = idx
          set_status("greife: #{Prefabs.name_of(@objects[idx]['type'])} (Klick setzt ab)")
        end
      end
    else # :place
      if Rune::Input.mouse_pressed?(0)
        place(@objects, scene, @hit[0], @hit[2], @rotation)
        Editor.rebuild(@objects, scene)
      end
    end
  end

  # Rechtsklick-Löschung kommt aus main.rb (dort ist der getroffene Knoten bekannt).
  def delete_node(node, scene)
    idx = Editor.object_index(node&.name)
    if idx && @objects[idx]
      delete_at(idx, scene)
      Editor.rebuild(@objects, scene)
    end
  end

  # Notlösung für dünne Prefabs (Zaunlatten, Pfähle): Der Strahl verfehlt
  # die dünnen Einzelteile leicht. Deshalb das Objekt löschen, dessen Mitte
  # der Bodenpunkt am nächsten liegt (innerhalb seines groben Radius).
  def delete_near(x, z, scene)
    best = nil
    best_d = nil
    @objects.each_with_index do |o, i|
      dx = x - o['x'].to_f
      dz = z - o['z'].to_f
      d = Math.sqrt(dx * dx + dz * dz)
      next if d > Prefabs.radius_of(o['type'])
      if best_d.nil? || d < best_d
        best = i
        best_d = d
      end
    end
    delete_at(best, scene) if best
    Editor.rebuild(@objects, scene) if best
  end

  # ---------------------------------------------------------------- Zeichnen

  def draw_hit_marker
    return unless @hit && (@tool == :place || @grab)
    p = current_prefab
    # Grobe Hüllbox des Prefabs
    w = 0.0; h = 0.0; d = 0.0
    p[:parts].each do |part|
      w = [w, part[:s][0]].max
      h = [h, part[:d][1] + part[:s][1] * 0.5].max
      d = [d, part[:s][2]].max
    end
    Game.draw_wire_box(@hit[0], @hit[1] + h * 0.5, @hit[2], w * 1.05, h * 1.05, d * 1.05, 0xFFD166)
  end

  # Auswahl-Markierung um das ausgewählte Objekt.
  def draw_selection
    return unless @selected && @objects[@selected]
    obj = @objects[@selected]
    r = Prefabs.radius_of(obj['type'])
    versatz = obj['y'].is_a?(Numeric) && obj['y'].to_f.finite? ? obj['y'].to_f : 0.0
    Game.draw_wire_box(obj['x'].to_f, Game.gy(obj['x'].to_f, obj['z'].to_f) + versatz + 1.2,
                       obj['z'].to_f, r * 2.0, 2.4, r * 2.0, UI::ACCENT)
  end

  # Pinsel-Vorschau am Cursor (Etappe 18m): flaches Quadrat in Pinselgrösse
  # auf dem Boden - blau beim Boden-Malen, orange beim Höhen-Malen.
  def draw_brush_marker
    return unless @hit && (@tool == :paint || @tool == :height)
    col = @tool == :paint ? 0x66CCFF : 0xFF8830
    s = @paint_size * 0.98
    Game.draw_wire_box(@hit[0], @hit[1] + 0.05, @hit[2], s, 0.08, s, col)
  end

  # --- UI-Bausteine -----------------------------------------------------------

  TAB_BAR_Y = 48

  def draw_tabs
    TABS.each_with_index do |(id, label), i|
      UI.button(16 + i * 104, TAB_BAR_Y, 98, 26, label,
                active: @tab == id.to_sym, id: "tab:#{id}")
    end
    if @dirty
      UI.text(16 + TABS.size * 104 + 8, TAB_BAR_Y + 5, '[ungespeichert]', UI::WARN, 15)
    end
    # Playtest rechts in der Leiste (Etappe 16b): Editor-Engine <-> Spiel-Engine.
    UI.button(1140, TAB_BAR_Y, 124, 26, '▶ Testen', ok: true, id: 'playtest')
  end

  def draw_objekte_tab
    UI.window(16, 82, 244, 344, 'PALETTE')
    Prefabs::LIST.each_with_index do |p, i|
      col = i % 2
      row = i / 2
      UI.button(24 + col * 112, 114 + row * 30, 104, 26, p[:name],
                active: i == @sel, id: "prefab:#{p[:id]}")
    end

    y = 114 + ((Prefabs::LIST.size + 1) / 2) * 30 + 8
    UI.button(24, y, 104, 26, 'Setzen', active: @tool == :place, id: 'tool:place')
    UI.button(136, y, 104, 26, 'Verschieben', active: @tool == :move, id: 'tool:move')
    y += 32
    UI.button(24, y, 48, 26, '-30°', id: 'rot:-')
    UI.button(78, y, 48, 26, '+30°', id: 'rot:+')
    UI.text(132, y + 5, format('%d°', (@rotation * 180.0 / Math::PI).round), UI::TEXT_DIM, 15)
    y += 32
    UI.button(24, y, 104, 26, "Raster #{@grid} m", id: 'grid')
    UI.button(136, y, 104, 26, 'Speichern', active: @dirty, id: 'save')
  end

  # Etappe 18m: BODEN-Tab - Textur-Palette + Pinselgrösse + Radierer-Hinweis.
  def draw_boden_tab
    UI.window(16, 82, 244, 344, 'BODEN MALEN')
    UI.text(24, 112, 'Textur:', UI::TEXT, 15)
    BRUSH_TEX.each_with_index do |tex, i|
      col = i % 2
      row = i / 2
      UI.button(24 + col * 112, 134 + row * 30, 104, 26, tex,
                active: @paint_tex == tex, id: "brushtex:#{tex}")
    end
    y = 134 + ((BRUSH_TEX.size + 1) / 2) * 30 + 16
    UI.button(24, y, 48, 26, '-', id: 'brush-')
    UI.text(80, y + 5, "Pinsel #{@paint_size.round} m", UI::TEXT_DIM, 15)
    UI.button(196, y, 48, 26, '+', id: 'brush+')
    y += 34
    UI.text(24, y, 'Klick = malen', UI::TEXT_DIM, 14)
    UI.text(24, y + 18, 'Shift+Klick = radieren', UI::TEXT_DIM, 14)
    UI.button(24, y + 40, 104, 26, 'Speichern', active: @dirty, id: 'save')
  end

  # Etappe 18m: HÖHE-Tab - Berg/Senke malen wie im Standalone-Editor.
  def draw_hoehe_tab
    UI.window(16, 82, 244, 344, 'HÖHE MALEN')
    UI.button(24, 114, 104, 26, '-', id: 'brush-')
    UI.text(80, 119, "Pinsel #{@paint_size.round} m", UI::TEXT_DIM, 15)
    UI.button(196, 114, 48, 26, '+', id: 'brush+')
    UI.text(24, 156, "Hub #{@height_step.round} m pro Klick", UI::TEXT_DIM, 14)
    UI.text(24, 176, 'Klick = Berg anhöhen', UI::TEXT_DIM, 14)
    UI.text(24, 194, 'Shift+Klick = Senke abtragen', UI::TEXT_DIM, 14)
    UI.text(24, 212, 'Mehrfachklick stapelt', UI::TEXT_DIM, 14)
    UI.button(24, 240, 104, 26, 'Speichern', active: @dirty, id: 'save')
    t = map_terrain
    n = (t['heights'] || []).size
    UI.text(24, 276, "#{n} Höhen-Pinsel gesetzt", UI::TEXT_DIM, 14)
  end

  def draw_selection_panel
    return unless @selected && @objects[@selected]
    obj = @objects[@selected]
    UI.window(16, 434, 244, 96, 'AUSWAHL')
    UI.text(24, 466, Prefabs.name_of(obj['type']), UI::TEXT, 16)
    UI.text(24, 486, format('x %s   z %s   %d°', obj['x'], obj['z'],
                            (obj['rot'].to_f * 180.0 / Math::PI).round), UI::TEXT_DIM, 14)
    UI.button(24, 500, 104, 24, 'Drehen +30°', id: 'sel:rot')
    UI.button(136, 500, 104, 24, 'Löschen', danger: true, id: 'sel:delete')
  end

  def draw_karte_tab
    UI.window(16, 82, 244, 250, 'KARTEN')
    ids = Database.map_ids
    ids.first(8).each_with_index do |id, i|
      UI.button(24, 114 + i * 30, 228, 26, id, active: id == @map_id, id: "map:#{id}")
    end
    y = 114 + [ids.size, 8].min * 30 + 8
    UI.button(24, y, 110, 26, '+ Neue Karte', id: 'newmap')
    UI.button(142, y, 110, 26, 'Speichern', active: @dirty, id: 'save')
  end

  def draw_list_tab(kind)
    title = kind == :events ? 'EVENTS' : 'NPCS'
    entries = kind == :events ? (@map_ref && @map_ref['events']) : (@map_ref && @map_ref['npcs'])
    UI.window(16, 82, 320, 300, title)
    if !entries || entries.empty?
      UI.text(24, 116, kind == :events ? 'Keine Events auf dieser Karte.' : 'Keine NPCs auf dieser Karte.',
              UI::TEXT_DIM, 15)
    else
      entries.first(9).each_with_index do |e, i|
        y = 112 + i * 24
        name = e['name'] || '?'
        info = format('%s  (%s, %s/%s)', name, e['trigger'] || 'action', e['x'], e['z'])
        UI.text(24, y, "- #{info}", UI::TEXT, 15)
      end
      if entries.size > 9
        UI.text(24, 112 + 9 * 24, "... #{entries.size - 9} weitere", UI::TEXT_DIM, 14)
      end
    end
    UI.text(24, 356, 'Bearbeiten im Karten-JSON / Standalone-Editor.', UI::TEXT_DIM, 14)
  end

  def draw_ui
    UI.begin_frame

    draw_tabs
    case @tab
    when :objekte then draw_objekte_tab
    when :boden   then draw_boden_tab
    when :hoehe   then draw_hoehe_tab
    when :karte   then draw_karte_tab
    when :events  then draw_list_tab(:events)
    when :npcs    then draw_list_tab(:npcs)
    end
    draw_selection_panel if @tab == :objekte

    # Statusleiste unten
    UI.panel(0, 692, 1280, 28, UI::PANEL, 0.78)
    hint = case @tool
           when :move   then (@grab ? 'Klick = ablegen' : 'Klick = Objekt greifen')
           when :paint  then 'Klick = Boden malen, Shift+Klick = radieren'
           when :height then 'Klick = Berg, Shift+Klick = Senke, +/- Pinsel'
           else 'Klick = setzen, Rechtsklick = löschen'
           end
    UI.text(16, 697, @status && @status_t > 0.0 ? @status : hint, UI::ACCENT, 15)
    UI.text(620, 697, "Objekte: #{@objects.size}   Raster: #{@grid} m   " \
                      "Werkzeug: #{TOOL_NAMES[@tool]}",
            UI::TEXT_DIM, 14)
    UI.text(1080, 697, 'F4 = fertig (speichert automatisch)', UI::TEXT_DIM, 14)
  end

  TOOL_NAMES = { place: 'Setzen', move: 'Verschieben', paint: 'Boden malen',
                 height: 'Höhe malen' }.freeze
end
