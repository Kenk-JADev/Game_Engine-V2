#encoding: utf-8
# Einfache 2D-Oberfläche: Panels, Texte, Nachrichtenfenster und ein kleines
# Button-Framework, das auch eigene Ruby-Skripte verwenden können.
#
# Klick-Erkennung: Jede gezeichnete Fläche registriert sich für den
# NÄCHSTEN Frame (draw läuft nach update). UI.clicked prüft den Klick
# gegen die Fläche des Vorframes — bei 60 FPS kein spürbarer Versatz.
# Alles hier landet in der UI-Schicht der Engine (Rechtecke + GDI-Text).
module UI
  TEXT      = 0xE8E8E8
  TEXT_DIM  = 0x9AA4AE
  ACCENT    = 0x59D8FF
  PANEL     = 0x0B1016
  DANGER    = 0xE86A6A
  OK        = 0x86D993
  WARN      = 0xFFD166

  BTN_BG        = 0x14202B
  BTN_HOVER     = 0x233646
  BTN_ACTIVE_BG = 0x2A4B5E
  BTN_DANGER_BG = 0x2A1215

  @rects = nil
  @prev  = nil

  def self.panel(x, y, w, h, color = PANEL, alpha = 0.72)
    Rune::Gfx.rect(x, y, w, h, color, alpha)
  end

  def self.frame(x, y, w, h, color = ACCENT, alpha = 0.55)
    panel(x, y, w, h, color, alpha)
  end

  # Dünner Rahmen (UI.frame füllt die Fläche!). Ehemals Launcher.rahmen -
  # der Launcher ist seit AB gefallen, Menü/Kampf/Titel nutzen das hier.
  def self.rahmen(x, y, w, h, color = ACCENT, alpha = 0.6)
    b = 2
    Rune::Gfx.rect(x, y, w, b, color, alpha)
    Rune::Gfx.rect(x, y + h - b, w, b, color, alpha)
    Rune::Gfx.rect(x, y, b, h, color, alpha)
    Rune::Gfx.rect(x + w - b, y, b, h, color, alpha)
  end

  # Halbtransparente Abdunklung hinter Dialogen (Menü, Kampf, Titel).
  # Ehemals Launcher.abdunkeln.
  def self.abdunkeln(w = Rune::Window.width, h = Rune::Window.height,
                     alpha = 0.55)
    Rune::Gfx.rect(0, 0, w, h, 0x060A12, alpha)
  end

  def self.text(x, y, s, color = TEXT, size = 18)
    Rune::Gfx.text(x, y, s.to_s, color, size)
  end

  # --- Parteigesichter (Nacharbeit zu 20b) -----------------------------------------
  # XP zeigt Face-Grafiken aus Graphics/Faces. Rune3D hat keine Bilddateien
  # für Gesichter, deshalb ein prozedurales Gesicht: Farbfläche mit den
  # Initialen und einem HP-Streifen. Die Farbe kommt aus der Datenbank
  # ("face": 0xRRGGBB) oder stabil aus dem Namen.
  GESICHTER = [0x5A7FB0, 0x9A6BB0, 0xB08A5A, 0x5AA88A, 0xB05A6B, 0x6B8FB0].freeze

  def self.gesicht_farbe(name)
    text = name.to_s
    return GESICHTER[0] if text.empty?

    GESICHTER[text.each_byte.sum % GESICHTER.size]
  end

  def self.initialen(name)
    teile = name.to_s.split(/\s+/).reject(&:empty?)
    return '?' if teile.empty?

    teile.map { |t| t[0] }.join[0, 2].upcase
  end

  # size = Kantenlänge; farbe überschreibt die Namensfarbe (Datenbank),
  # hp/max_hp zeichnen den Lebensstreifen am unteren Rand, auswahl den Rahmen.
  def self.gesicht(x, y, size, name, farbe: nil, hp: nil, max_hp: nil, auswahl: false)
    farbe ||= gesicht_farbe(name)
    panel(x, y, size, size, farbe, 0.85)
    rahmen(x, y, size, size, auswahl ? ACCENT : 0x0B1016, auswahl ? 0.95 : 0.5)
    groesse = [size / 3, 11].max
    schrift = initialen(name)
    text(x + (size - groesse * schrift.length) / 2, y + size / 2 - groesse / 2,
         schrift, 0x0B1016, groesse)
    return unless max_hp.to_i.positive?

    anteil = [[hp.to_f / max_hp.to_f, 0.0].max, 1.0].min
    panel(x + 3, y + size - 7, size - 6, 4, 0x220A0A, 0.9)
    panel(x + 3, y + size - 7, ((size - 6) * anteil).round, 4,
          anteil > 0.5 ? 0x86D993 : 0xE86A6A, 0.95)
  end

  # --- Button-Framework (Etappe 16a) ----------------------------------------

  # Frame starten: registrierte Flächen des letzten Frames einfrieren.
  def self.begin_frame
    @prev = @rects
    @rects = []
  end

  # Zeichnet einen Button und registriert seine Fläche. Rückgabe: Hover?
  # id: Klick-Kennung (default: das Label selbst).
  def self.button(x, y, w, h, label, active: false, danger: false, ok: false, size: 15, id: nil)
    mx, my = Rune::Input.mouse_position
    hover = mx >= x && mx < x + w && my >= y && my < y + h

    if active
      bg, fg = BTN_ACTIVE_BG, ACCENT
    elsif ok
      bg = hover ? 0x1E4A2A : 0x14291A
      fg = hover ? 0xBFFFCF : OK
    elsif danger
      bg = hover ? 0x4A1A1E : BTN_DANGER_BG
      fg = hover ? 0xFF9A9A : DANGER
    else
      bg = hover ? BTN_HOVER : BTN_BG
      fg = hover ? 0xFFFFFF : TEXT
    end

    panel(x, y, w, h, bg, 0.94)
    panel(x, y, w, 1, ACCENT, active ? 0.9 : 0.22)   # Zierlinie oben
    panel(x, y, 2, h, ACCENT, 0.6) if active          # aktiver Balken links
    text(x + 10, y + (h - size) / 2 - 1, label, fg, size)
    @rects << [x, y, w, h, id || label]
    hover
  end

  # Label des Buttons, auf den in diesem Frame geklickt wurde (nil = keiner).
  def self.clicked
    return nil unless Rune::Input.mouse_pressed?(0)
    mx, my = Rune::Input.mouse_position
    (@prev || []).reverse_each do |(x, y, w, h, label)|
      return label if mx >= x && mx < x + w && my >= y && my < y + h
    end
    nil
  end

  # Maus über einer registrierten Fläche (Vorframe)? Dann keine Welt-Aktion.
  def self.over_ui?
    mx, my = Rune::Input.mouse_position
    (@prev || []).any? { |(x, y, w, h, _)| mx >= x && mx < x + w && my >= y && my < y + h }
  end

  # Fensterrahmen mit Titelleiste.
  def self.window(x, y, w, h, title)
    panel(x, y, w, h, 0x0B1118, 0.88)
    panel(x, y, w, 24, 0x101B26, 0.95)
    panel(x, y, w, 2, ACCENT, 0.8)
    text(x + 10, y + 4, title, ACCENT, 15)
  end

  # --- Bestehende Bausteine --------------------------------------------------

  # Mehrzeiliges Nachrichtenfenster am unteren Rand (RPG-Maker-Stil) mit
  # optionaler Wahl im SELBEN Fenster (Kombi-Fenster, Z8 wie XP): Die
  # Optionen stehen unter dem Text, das Fenster wächst nach oben, die
  # Unterkante bleibt stehen. choices = {options:, sel:, cancel:} oder nil.
  # wartet: Text wartet auf Bestätigung -> blinkender ▾-Zeiger.
  def self.message(lines, choices = nil, x: 40, y: 496, w: 1200, wartet: true)
    line_h = 26
    text_h = 30 + lines.size * line_h
    optionen = choices ? choices[:options] : []
    option_h = optionen.empty? ? 0 : 14 + optionen.size * 24 + 10
    h = text_h + option_h
    top = y + text_h - h                    # Unterkante bleibt bei y + text_h
    panel(x, top, w, h, PANEL, 0.80)
    panel(x, top, w, 2, ACCENT, 0.85)       # Zierlinie oben
    lines.each_with_index do |l, i|
      text(x + 22, top + 18 + i * line_h, l, TEXT, 18)
    end
    unless optionen.empty?
      basis = top + text_h - 6
      optionen.each_with_index do |opt, i|
        oy = basis + i * 24
        if i == choices[:sel]
          panel(x + 26, oy - 3, w - 52, 22, ACCENT, 0.18)
          text(x + 36, oy, '▶', ACCENT, 15)
          text(x + 60, oy, opt, ACCENT, 17)
        else
          text(x + 60, oy, opt, TEXT, 17)
        end
      end
      hint = choices[:cancel] && choices[:cancel] != :aus ? '↑↓ + E   Esc = Abbruch' : '↑↓ + E'
      text(x + w - (choices[:cancel] && choices[:cancel] != :aus ? 190 : 90),
           top + h - 20, hint, TEXT_DIM, 12)
    end
    # Pause-Zeiger ▾ (M/XP Window_Message): blinkt unten rechts, solange
    # auf die Bestätigung gewartet wird - sonst ahnt man E/Space nicht.
    if wartet && ((Time.now.to_f * 2.0).to_i % 2).zero?
      text(x + w - 28, top + h - 24, '▾', ACCENT, 16)
    end
  end

  # Wahlmöglichkeiten in einem eigenen Fenster. Seit dem Kombi-Fenster (Z8)
  # zeichnet die Spiel-Logik die Wahl IM Textfenster (`message` mit
  # choices-Argument); dieser Baustein bleibt für Sonderfälle (z. B. Skripte)
  # und für Tests erhalten.
  # cancel (Z5): zeigt den Esc-Hinweis, wenn der Befehl Abbruch erlaubt.
  def self.choices(options, sel, x: 40, y: 496, w: 420, cancel: false)
    line_h = 28
    h = 34 + options.size * line_h
    panel(x, y, w, h, PANEL, 0.86)
    panel(x, y, w, 2, ACCENT, 0.85)
    text(x + 22, y + 8, 'Bitte wählen:', TEXT_DIM, 14)
    options.each_with_index do |opt, i|
      oy = y + 30 + i * line_h
      if i == sel
        panel(x + 12, oy, w - 24, line_h - 2, ACCENT, 0.22)
        text(x + 22, oy + 3, "> #{opt}", ACCENT, 18)
      else
        text(x + 22, oy + 3, "   #{opt}", TEXT, 18)
      end
    end
    hint = cancel ? '↑↓ + E   Esc = Abbruch' : '↑↓ + E'
    text(x + w - (cancel ? 180 : 150), y + h - 18, hint, TEXT_DIM, 12)
  end

  # Zahleneingabe (Etappe 19i, RPG Maker XP "Input Number"): Ziffernfeld,
  # hoch/runter ändert die markierte Ziffer, links/rechts markiert,
  # E/Enter bestätigt, Esc bricht ab (0), Zifferntasten setzen direkt.
  # state = {variable:, digits:, ziffern:[9,9,9], sel:0, cancel:true}.
  # Rückgabe: true wenn weiter blockiert, false wenn fertig (Wert steht in
  # GameState - Abbruch mit cancel=false setzt ebenfalls 0 wie bei XP).
  def self.nummer_eingabe(state)
    ziffern = state[:ziffern]
    n = ziffern.size
    cw = 40
    w = n * cw + 40
    x = (Rune::Window.width - w) / 2
    y = 200
    panel(x, y, w, 104, PANEL, 0.92)
    panel(x, y, w, 2, ACCENT, 0.85)
    text(x + 16, y + 10, "Zahl eingeben: #{state[:variable]}", TEXT_DIM, 14)
    ziffern.each_with_index do |z, i|
      zx = x + 20 + i * cw
      if i == state[:sel]
        panel(zx - 4, y + 34, cw - 4, 44, ACCENT, 0.25)
        text(zx + 8, y + 42, z.to_s, ACCENT, 26)
      else
        text(zx + 8, y + 42, z.to_s, TEXT, 26)
      end
    end
    text(x + 16, y + 84, '↑↓ Ziffer   ←→ Stelle   0-9 direkt   E = OK' +
         (state[:cancel] ? '   Esc = Abbruch' : ''), TEXT_DIM, 12)

    geaendert = false
    if Rune::Input.pressed?(:up)
      ziffern[state[:sel]] = (ziffern[state[:sel]] + 1) % 10
      geaendert = true
    elsif Rune::Input.pressed?(:down)
      ziffern[state[:sel]] = (ziffern[state[:sel]] - 1) % 10
      geaendert = true
    elsif Rune::Input.pressed?(:left)
      state[:sel] = (state[:sel] - 1) % n
      geaendert = true
    elsif Rune::Input.pressed?(:right)
      state[:sel] = (state[:sel] + 1) % n
      geaendert = true
    end
    ('0'..'9').each do |t|
      next unless Rune::Input.pressed?(t.to_sym)
      ziffern[state[:sel]] = t.to_i
      state[:sel] = (state[:sel] + 1) % n
      geaendert = true
    end
    Rune::Sound.play('klick', 0.35) if geaendert

    if Rune::Input.pressed?(:enter) || Rune::Input.pressed?(:e) ||
       Rune::Input.pressed?(:space)
      GameState.set_variable(state[:variable], ziffern.join.to_i)
      Rune::Sound.play('jingle', 0.5)
      return false
    end
    if state[:cancel] && Rune::Input.pressed?(:escape)
      GameState.set_variable(state[:variable], 0)
      Rune::Sound.play('ablehnen', 0.6)
      return false
    end

    true
  end

  # Kopfzeile mit Position, FPS und Kamera-Zustand (zum Einmessen der Welt).
  def self.hud(x = 16, y = 12)
    px, py, pz = Rune::Player.position
    line = format('x %.1f  y %.1f  z %.1f   %.0f FPS   %s',
                  px, py, pz, Rune::Time.fps, Rune::Camera.info)
    line = num(line)
    panel(x, y, 520, 30, PANEL, 0.55)
    text(x + 12, y + 6, line, TEXT_DIM, 16)
  end

  # "-0.0" ist hässlich, im HUD soll dort "0.0" stehen.
  def self.num(s)
    s.gsub('-0.0', ' 0.0')
  end
end
