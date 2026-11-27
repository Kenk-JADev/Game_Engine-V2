#encoding: utf-8
# ============================================================================
#  Scene_Title (XP 070) — Startbildschirm: Neues Spiel / Fortsetzen /
#  Spiel beenden. Ersetzt den Projekt-Launcher (AB-Runde 9):
#
#    - Keine Projektanlage mehr im Spiel (nur noch der Editor, Y3/Y4).
#    - Continue führt zur Scene_File (Slot-Wähler, schließt die K-Lücke).
#    - Die Kulisse ist weiter die 3D-Welt: erste Karte des intern
#      gewählten Projekts, sonst die Demo-Map aus der Grund-Datenbank.
#    - Projekt-Skripte (autoload) laufen vor dem Aufbau der Szene und
#      dürfen Scene_Title neu definieren oder $scene selbst setzen.
#
#  Rahmenbedingung: main = EIN Frame pro Tick (Scene_Base), kein Loop.
# ============================================================================
class Scene_Title < Scene_Base
  def initialize(projekt: nil)
    super()
    # XP 089 ruft Scene_Title.new ohne Argumente; das gebundene Projekt bleibt
    # trotzdem erhalten (Etappe 39).
    @projekt = projekt || $project_name
    @sel = 0
    @t = 0.0
    @last = nil
    bau_kulisse
  end

  # XP-Befehle; "off" blendet aus wie in XP (z. B. ohne Spielstände).
  def commands
    [{ label: 'Neues Spiel', methode: :command_new_game, off: @projekt.nil? },
     { label: 'Fortsetzen', methode: :command_continue, off: !fortsetzen_ok? },
     { label: 'Spiel beenden', methode: :command_shutdown, off: false }]
  end

  def fortsetzen_ok?
    !@projekt.nil? && Project.has_saves?(@projekt)
  end

  # ---------------------------------------------------------------- Logik

  def update
    # Kamera-Orbit um die Kulisse (vom Launcher übernommen): der Spieler ist
    # der unsichtbare Drehpunkt, die Kamera schaut auf ihn.
    now = Time.now.to_f
    dt = @last ? [now - @last, 0.05].min : 0.0
    @last = now
    @t += dt
    a = @t * 0.10
    px = Math.sin(a) * 10.0
    pz = Math.cos(a) * 10.0
    Rune::Player.teleport(px, Game.gy(px, pz) + 1.6, pz)
    Rune::Camera.yaw = a + Math::PI
    @scene.update(dt) if @scene

    cmds = commands
    @sel = (@sel - 1) % cmds.size if Rune::Input.pressed?(:up)
    @sel = (@sel + 1) % cmds.size if Rune::Input.pressed?(:down)
    return unless Rune::Input.pressed?(:enter)

    cmd = cmds[@sel]
    if cmd[:off]
      Rune::Sound.play('ablehnen', 0.7)
    else
      send(cmd[:methode])
    end
  end

  # XP 070: frisches Spiel im gebundenen Projekt (System-Startkarte).
  def command_new_game
    Rune::Sound.play('klick', 0.9)
    Game.start_game
  end

  # XP 070: Spielstände über die Scene_File wählen.
  def command_continue
    Rune::Sound.play('klick', 0.9)
    $scene = Scene_File.new(titel: self, projekt: @projekt)
  end

  # XP 070: Engine beenden.
  def command_shutdown
    Rune::Sound.play('ablehnen', 0.5)
    Rune::App.quit
  end

  # ---------------------------------------------------------------- Kulisse

  # 3D-Welt als Titelbild: derselbe geprüfte Aufbau wie beim Launcher
  # (Welten-Terrain + Prefab-Objekte als ed_*-Knoten, ohne Editor).
  def bau_kulisse
    map = Database.map(Database.map_ids.first || 'welt01') || {}
    Textures.end_project
    Textures.register_all
    Game.build_terrain(map)
    Rune::Gfx.sky(0x3D6FB5, 0xBFD8EC)
    Rune::Gfx.sun(-0.55, 0.75, 0.25, 0.35)
    @scene = Scene.new(map['name'] || 'Titel')
    @map_objects = MapObjects.new(@scene, height_at: Game.method(:gy))
    @map_objects.rebuild(map['objects'] || [])
    Rune::Camera.mode = :third
    Rune::Camera.distance = 28
  rescue StandardError => e
    Rune::Log.error("Titel-Kulisse: #{e.class}: #{e.message}")
    Issues.report('Titel', "Kulisse: #{e.message}", severity: 'error')
    @scene = Scene.new('titel')
    @map_objects = nil
  end

  # ---------------------------------------------------------------- Zeichnen

  def draw
    Rune::Gfx.clear_color = 0x131A24
    @scene.draw if @scene
    w = Rune::Window.width
    h = Rune::Window.height
    UI.abdunkeln(w, h, 0.45)

    titel = @projekt ? Project.title(@projekt) : 'Rune3D'
    tw = (titel.size * 0.62 * 34).to_i
    UI.text((w - tw) / 2, (h * 0.15).to_i, titel, UI::TEXT, 34)

    if @projekt
      info = Project.info(@projekt)
      UI.text(24, 20, "Projekt #{@projekt} · #{info['maps']} Karten · " \
                      "#{info['saves']} Spielstände", UI::TEXT_DIM, 14)
    end

    cmds = commands
    mw = 440
    mh = 26 + cmds.size * 36 + 44
    mx = w / 2 - mw / 2
    my = (h * 0.42).to_i
    UI.panel(mx, my, mw, mh, UI::PANEL, 0.92)
    UI.rahmen(mx, my, mw, mh, UI::ACCENT, 0.6)
    cmds.each_with_index do |c, i|
      y = my + 20 + i * 36
      sel = i == @sel
      UI.panel(mx + 14, y - 4, mw - 28, 32,
               sel ? 0x2A4A66 : 0x101826, sel ? 0.95 : 0.5)
      UI.text(mx + 30, y, (sel ? '> ' : '  ') + c[:label],
              c[:off] ? UI::TEXT_DIM : UI::TEXT, 17)
    end
    UI.text(mx + 20, my + mh - 32,
            '[Hoch/Runter] Wählen   [Enter] OK', UI::TEXT_DIM, 14)

    return if @projekt

    UI.text(w / 2 - 250, my + mh + 26,
            'Noch kein Projekt - im Editor Neues Projekt anlegen.',
            UI::DANGER, 16)
  end
end
