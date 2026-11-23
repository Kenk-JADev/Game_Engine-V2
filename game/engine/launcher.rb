#encoding: utf-8
# ============================================================================
#  Titelbildschirm (Etappe 8): das Demodorf dreht sich als 3D-Hintergrund,
#  davor das Menü - Projektauswahl mit Tastatur UND Maus, Infos pro Projekt,
#  neues Projekt anlegen, Projekt löschen (mit Rückfrage).
#
#  Tasten:  Hoch/Runter oder Maus = wählen · Enter/Klick = spielen ·
#           1..9 Projekt direkt laden · N neues Projekt (Namen tippen) ·
#           D/Entf Projekt löschen (J = ja) · Esc abbrechen/beenden
#
#  Etappe 19e (User-Vorgabe, RPG-Maker-Stil): Existiert KEIN Projekt, führt
#  der Willkommensbildschirm (:first) durch die Ersteinrichtung - Enter/N
#  legt ein eigenes Projekt an, D erstellt auf Wunsch das Demo-Dorf, Esc
#  beendet. Früher wurde das Demo-Projekt still kopiert (ensure_demo);
#  jetzt entscheidet der Spieler.
# ============================================================================

module Launcher
  @state = :list      # :list, :name oder :del
  @projects = []
  @infos = {}
  @sel = 0
  @name = ''
  @t = 0.0
  @scene = nil

  ENTRY_H = 48        # Höhe eines Listeneintrags
  MAX_SHOWN = 8       # sichtbare Einträge (Rest läuft über Hoch/Runter)

  module_function

  def activate
    # Rückkehr aus dem Spiel: Projektbindung lösen, damit die Kulisse
    # wieder die Demo-Welt aus game/data zeigt.
    $project_root = nil
    Database.reset!
    @projects = Project.list
    @infos = @projects.to_h { |n| [n, Project.info(n)] }
    @sel = 0
    # Etappe 19e: ohne Projekte direkt in die Ersteinrichtung.
    @state = @projects.empty? ? :first : :list
    @name = ''
    @t = 0.0
    build_backdrop
    Rune::Input.capture = false   # Menü: freier Zeiger (Engine startet gefangen)
    Rune::Window.title = 'Rune3D'
    Rune::Sound.loop('musik', 0.15)   # ruhige Titel-Melodie (ersetzt alte Loops)
  end

  # Demodorf als Kulisse: Terrain + Objekte aus game/data (das ist ohne
  # Projekt genau die Demo-Welt), Kamera kreist langsam darum.
  def build_backdrop
    map = Database.map(Database.map_ids.first || 'dorf') || {}
    # Derselbe geprüfte Aufbau wie beim Kartenwechsel, auch für kaputte
    # Höhenpinsel/Bodenflächen und extreme Raster des Demodorfes.
    Textures.register_all
    Game.build_terrain(map)
    Rune::Gfx.sky(0x3D6FB5, 0xBFD8EC)
    Rune::Gfx.sun(-0.55, 0.75, 0.25, 0.35)
    @scene = Scene.new(map['name'] || 'Dorf')
    Editor.rebuild(map['objects'] || [], @scene)
    Rune::Camera.mode = :third
    Rune::Camera.distance = 28
  end

  def start(name)
    Rune::Sound.play('klick', 0.8)
    Project.touch(name)
    $project_name = name
    $project_root = Project.data_path(name)
    Database.reset!
    sys = Database.system
    Rune::Log.info("Projekt '#{sys['name'] || name}' geladen (#{Database.map_ids.size} Karten)")
    Rune::Window.title = "#{sys['name'] || name} - Rune3D"
    Game.setup
    Rune::Input.capture = true   # im Spiel wieder Mausblick (F5 Reload nicht betroffen)
  end

  # --------------------------------------------------------------- Update

  def update(dt)
    @t += dt
    # Kamera-Orbit: Spieler ist unsichtbarer Drehpunkt, die Kamera schaut
    # immer auf ihn - so wandert der Blick langsam übers Dorf.
    a = @t * 0.10
    r = 10.0
    px = Math.sin(a) * r
    pz = Math.cos(a) * r
    Rune::Player.teleport(px, Game.gy(px, pz) + 1.6, pz)
    Rune::Camera.yaw = a + Math::PI
    @scene.update(dt) if @scene

    case @state
    when :first then update_first
    when :list  then update_list
    when :name  then update_name
    when :del   then update_del
    end
  end

  # Etappe 19e: Ersteinrichtung, wenn kein Projekt existiert.
  def update_first
    if Rune::Input.pressed?(:enter) || Rune::Input.pressed?(:n)
      Rune::Sound.play('klick', 0.8)
      @state = :name
      @name = ''
    elsif Rune::Input.pressed?(:d)
      # Demo-Dorf aus game/data anlegen und direkt spielen (ensure_demo ist
      # idempotent - gäbe es das Projekt schon, wären wir nicht in :first).
      Project.ensure_demo
      @projects = Project.list
      @infos = @projects.to_h { |n| [n, Project.info(n)] }
      @sel = @projects.index('dorf') || 0
      start('dorf')
    elsif Rune::Input.pressed?(:escape)
      Rune::App.quit
    end
  end

  def update_list
    n = @projects.size
    shown = [n, MAX_SHOWN].min

    # Maus: Eintrag unter dem Zeiger wählen, Klick startet.
    mx, my = Rune::Input.mouse_position
    y0 = 168
    if mx >= 48 && mx <= 448 && my >= y0 && my < y0 + shown * ENTRY_H
      i = ((my - y0) / ENTRY_H).to_i
      @sel = i if i >= 0 && i < shown
      if Rune::Input.mouse_pressed?(0)   # 0 = linke Maustaste
        start(@projects[@sel])
        return
      end
    end

    # Tastatur
    @sel = (@sel - 1) % n if Rune::Input.pressed?(:up) && n > 0
    @sel = (@sel + 1) % n if Rune::Input.pressed?(:down) && n > 0

    @projects.each_with_index do |name, i|
      next if i >= 9
      next unless Rune::Input.pressed?((i + 1).to_s)
      start(name)
      return
    end

    start(@projects[@sel]) if Rune::Input.pressed?(:enter) && n > 0
    # L = Fortsetzen: zuletzt gesicherten Spielstand des gewählten Projekts.
    if Rune::Input.pressed?(:l) && n > 0
      name = @projects[@sel]
      slot = Project.latest_save(name)
      if slot
        start(name)
        Game.load_game(slot)
      else
        Rune::Sound.play('ablehnen', 0.7)
      end
    end
    if Rune::Input.pressed?(:n)
      @state = :name
      @name = ''
    elsif Rune::Input.pressed?(:d) && n > 0
      @state = :del
    end
    Rune::App.quit if Rune::Input.pressed?(:escape)
  end

  def update_name
    # Frame-basierte Eingabe: jede Taste ist genau einen Frame "gedrückt",
    # WM_CHAR ist bei PostMessage unzuverlässig (kommt Frames später).
    if Rune::Input.pressed?(:backspace)
      @name = @name[0...-1].to_s
    end
    zeichen = ('a'..'z').to_a + ('0'..'9').to_a + ['-', '_', '.']
    zeichen.each do |c|
      next unless Rune::Input.pressed?(c)
      @name << c if @name.length < 24
    end
    @name << ' ' if Rune::Input.pressed?(:space) && @name.length < 24
    if Rune::Input.pressed?(:enter) && !@name.strip.empty?
      name = Project.create(@name.strip)
      Rune::Log.info("Neues Projekt angelegt: #{name}")
      start(name)
    elsif Rune::Input.pressed?(:escape)
      @state = @projects.empty? ? :first : :list
    end
  end

  def update_del
    if Rune::Input.pressed?(:j) || Rune::Input.pressed?(:enter)
      victim = @projects[@sel]
      Rune::Log.info("Projekt gelöscht: #{victim}")
      Project.delete(victim)
      @projects = Project.list
      @infos = @projects.to_h { |x| [x, Project.info(x)] }
      @sel = @projects.any? ? @sel.clamp(0, @projects.size - 1) : 0
      @state = @projects.any? ? :list : :first
    elsif Rune::Input.pressed?(:n) || Rune::Input.pressed?(:escape)
      @state = :list
    end
  end

  # ---------------------------------------------------------------- Zeichnen

  def draw
    @scene.draw if @scene   # 3D-Kulisse zürst, dann das Menü darüber
    w = Rune::Window.width
    h = Rune::Window.height

    case @state
    when :first
      draw_list(w, h)
      abdunkeln(w)
      draw_first(w, h)
    when :list then draw_list(w, h)
    when :name
      draw_list(w, h)
      abdunkeln(w)
      draw_name(w)
    when :del
      draw_list(w, h)
      abdunkeln(w)
      draw_del(w, h)
    end
  end

  def draw_list(w, h)
    # Titel
    UI.text(48, 26, 'R U N E 3 D', UI::ACCENT, 40)
    UI.text(50, 78, 'Rollenspiel-Engine mit Ruby-Modding', UI::TEXT_DIM, 15)
    Rune::Gfx.rect(48, 106, 400, 3, UI::ACCENT, 0.8)

    # Menü-Panel links
    n = @projects.size
    shown = [n, MAX_SHOWN].min
    panel_h = 52 + shown * ENTRY_H + 10
    UI.panel(40, 118, 416, panel_h, UI::PANEL, 0.82)
    rahmen(40, 118, 416, panel_h, UI::ACCENT, 0.5)
    UI.text(56, 132, 'Projekt wahlen', UI::TEXT, 19)

    y0 = 168
    if n.zero?
      UI.text(56, y0 + 8, 'Keine Projekte gefunden.', UI::TEXT_DIM, 16)
      UI.text(56, y0 + 34, 'N drücken für ein neues.', UI::TEXT_DIM, 16)
    end
    shown.times do |i|
      name = @projects[i]
      info = @infos[name] || {}
      y = y0 + i * ENTRY_H
      sel = (i == @sel)
      UI.panel(50, y - 4, 396, ENTRY_H - 4, sel ? 0x2A4A66 : 0x101826, sel ? 0.95 : 0.55)
      Rune::Gfx.rect(50, y - 4, 4, ENTRY_H - 4, UI::ACCENT, sel ? 0.9 : 0.0)
      UI.text(64, y, (sel ? '> ' : '   ') + info['title'].to_s, sel ? UI::TEXT : UI::TEXT, 17)
      letzte = info['last'] ? "zuletzt #{info['last']}" : 'nie gespielt'
      karten = info['maps'] == 1 ? 'Karte' : 'Karten'
      st = info['saves'].to_i
      st = st.zero? ? nil : (st == 1 ? '1 Spielstand' : "#{st} Spielstände")
      zeile = ["#{info['maps']} #{karten}", st, letzte].compact.join(' · ')
      UI.text(82, y + 22, zeile, UI::TEXT_DIM, 13)
    end
    if n > shown
      UI.text(56, y0 + shown * ENTRY_H + 2, "  ... und #{n - shown} weitere (Hoch/Runter)",
              UI::TEXT_DIM, 13)
    end

    # Fussleiste
    UI.panel(0, h - 44, w, 44, 0x0D1420, 0.85)
    UI.text(w / 2 - 300, h - 32,
            '[Hoch/Runter] Wählen   [Enter] Spielen   [L] Fortsetzen   [N] Neu   [D] Löschen   [Esc] Beenden',
            UI::TEXT_DIM, 15)
    UI.text(w - 150, 12, 'Rune3D v0.8', UI::TEXT_DIM, 13)
  end

  # Willkommensdialog der Ersteinrichtung (Etappe 19e).
  def draw_first(w, h)
    bw = 600
    bh = 252
    bx = w / 2 - bw / 2
    by = h / 2 - bh / 2
    UI.panel(bx, by, bw, bh, 0x101826, 0.96)
    rahmen(bx, by, bw, bh, UI::ACCENT, 0.9)
    UI.text(bx + 24, by + 18, 'Willkommen bei Rune3D', UI::ACCENT, 24)
    UI.text(bx + 24, by + 62,
            'Es wurde noch kein Projekt gefunden - jedes Spiel braucht eins.',
            UI::TEXT, 15)
    UI.text(bx + 24, by + 102, '[Enter / N]   Neues Projekt erstellen (Name tippen)',
            UI::TEXT, 16)
    UI.text(bx + 24, by + 132, "[D]           Demo-Projekt 'Dorf' anlegen und ausprobieren",
            UI::TEXT, 16)
    UI.text(bx + 24, by + 162, '[Esc]         Rune3D beenden', UI::TEXT_DIM, 15)
    UI.text(bx + 24, by + 206,
            'Projekte leben unter projekte/ - mit Karten, Ruby-Scripts und Spielständen.',
            UI::TEXT_DIM, 13)
  end

  def draw_name(w)
    UI.text(w / 2 - 170, 160, 'Name des neuen Projekts:', UI::TEXT, 18)
    UI.panel(w / 2 - 170, 196, 340, 34, 0x101826, 1.0)
    rahmen(w / 2 - 170, 196, 340, 34, UI::ACCENT, 0.7)
    cursor = (Rune::Time.fps.to_i % 2 == 0) ? '_' : ' '
    UI.text(w / 2 - 158, 204, "#{@name}#{cursor}", UI::TEXT, 17)
    UI.text(w / 2 - 220, 250, 'Enter = anlegen und spielen   Esc = zurück', UI::TEXT_DIM, 15)
    UI.text(w / 2 - 220, 274, 'Erlaubt: Buchstaben, Ziffern, Leerzeichen, - _ .',
            UI::TEXT_DIM, 13)
  end

  def draw_del(w, h)
    name = @infos[@projects[@sel]]&.dig('title') || @projects[@sel].to_s
    bw = 460
    bx = w / 2 - bw / 2
    by = h / 2 - 70
    UI.panel(bx, by, bw, 130, 0x101826, 0.96)
    rahmen(bx, by, bw, 130, 0xE06C5A, 0.9)
    UI.text(bx + 20, by + 18, 'Projekt löschen?', 0xE06C5A, 20)
    UI.text(bx + 20, by + 52, "'#{name}' mit allen Karten endgültig entfernen?",
            UI::TEXT, 15)
    UI.text(bx + 20, by + 92, '[J] Ja, löschen      [N / Esc] Abbrechen',
            UI::TEXT_DIM, 15)
  end

  # Dünner Rahmen (UI.frame füllt die Fläche!)
  def rahmen(x, y, w, h, color, alpha)
    b = 2
    Rune::Gfx.rect(x, y, w, b, color, alpha)
    Rune::Gfx.rect(x, y + h - b, w, b, color, alpha)
    Rune::Gfx.rect(x, y, b, h, color, alpha)
    Rune::Gfx.rect(x + w - b, y, b, h, color, alpha)
  end

  # Halbtransparente Abdunklung hinter Dialogen.
  def abdunkeln(w)
    Rune::Gfx.rect(0, 0, w, Rune::Window.height, 0x060A12, 0.55)
  end
end
