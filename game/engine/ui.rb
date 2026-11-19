#encoding: utf-8
# Einfache 2D-Oberflaeche: Panels, Texte, Nachrichtenfenster — plus ein
# kleines Button-Framework fuer den Editor (Etappe 16a).
#
# Klick-Erkennung: Jede gezeichnete Flaeche registriert sich fuer den
# NAECHSTEN Frame (draw laeuft nach update). UI.clicked prueft den Klick
# gegen die Flaeche des Vorframes — bei 60 FPS kein spuerbarer Versatz.
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

  def self.text(x, y, s, color = TEXT, size = 18)
    Rune::Gfx.text(x, y, s.to_s, color, size)
  end

  # --- Button-Framework (Etappe 16a) ----------------------------------------

  # Frame starten: registrierte Flaechen des letzten Frames einfrieren.
  def self.begin_frame
    @prev = @rects
    @rects = []
  end

  # Zeichnet einen Button und registriert seine Flaeche. Rueckgabe: Hover?
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

  # Maus ueber einer registrierten Flaeche (Vorframe)? Dann keine Welt-Aktion.
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

  # Mehrzeiliges Nachrichtenfenster am unteren Rand (RPG-Maker-Stil).
  def self.message(lines, x: 40, y: 496, w: 1200)
    line_h = 26
    h = 30 + lines.size * line_h
    panel(x, y, w, h, PANEL, 0.80)
    panel(x, y, w, 2, ACCENT, 0.85)          # Zierlinie oben
    lines.each_with_index do |l, i|
      text(x + 22, y + 18 + i * line_h, l, TEXT, 18)
    end
  end

  # Wahlmöglichkeiten (Etappe 18f, RPG-Maker-Show-Choices): Fenster mit
  # Optionen, der gewaehlte Eintrag ist markiert (Pfeil + Hinterlegung).
  def self.choices(options, sel, x: 40, y: 496, w: 420)
    line_h = 28
    h = 34 + options.size * line_h
    panel(x, y, w, h, PANEL, 0.86)
    panel(x, y, w, 2, ACCENT, 0.85)
    text(x + 22, y + 8, 'Bitte waehlen:', TEXT_DIM, 14)
    options.each_with_index do |opt, i|
      oy = y + 30 + i * line_h
      if i == sel
        panel(x + 12, oy, w - 24, line_h - 2, ACCENT, 0.22)
        text(x + 22, oy + 3, "> #{opt}", ACCENT, 18)
      else
        text(x + 22, oy + 3, "   #{opt}", TEXT, 18)
      end
    end
    text(x + w - 150, y + h - 18, '↑↓ + E', TEXT_DIM, 12)
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

  # "-0.0" ist haesslich, im HUD soll dort "0.0" stehen.
  def self.num(s)
    s.gsub('-0.0', ' 0.0')
  end
end
