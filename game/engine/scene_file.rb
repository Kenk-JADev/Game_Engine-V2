#encoding: utf-8
# ============================================================================
#  Scene_File / Scene_Load (XP 077) — Slot-Wähler für Spielstände.
#  Continue auf dem Titel führt hierher (AB-Runde 9, schließt die
#  K-Lücke "kein Slot-Dialog"): Hilfezeile + drei Slots mit Vorschau
#  (Karte und Spielstand-Zeitpunkt aus Project.save_info).
#  main läuft pro Frame (Scene_Base), kein blockierender Loop.
# ============================================================================
class Scene_File < Scene_Base
  SLOTS = [1, 2, 3]

  def initialize(titel:, projekt: nil)
    super()
    @titel = titel      # Scene_Title-Instanz für den Abbruch
    @projekt = projekt
    @sel = 0
  end

  def update
    @sel = (@sel - 1) % SLOTS.size if Rune::Input.pressed?(:up)
    @sel = (@sel + 1) % SLOTS.size if Rune::Input.pressed?(:down)
    if Rune::Input.pressed?(:enter)
      laden
    elsif Rune::Input.pressed?(:escape)
      Rune::Sound.play('ablehnen', 0.5)
      $scene = @titel
    end
  end

  def laden
    slot = SLOTS[@sel]
    # Leerer Slot ist kein Fehler: nur melden, nicht in die Fehler-Logik.
    if @projekt.nil? || !Project.save_info(@projekt, slot)
      Rune::Sound.play('ablehnen', 0.7)
      return
    end
    if Game.load_game(slot)
      Rune::Sound.play('klick', 0.9)
    else
      Rune::Sound.play('ablehnen', 0.7)
    end
  end

  def draw
    Rune::Gfx.clear_color = 0x131A24
    w = Rune::Window.width
    h = Rune::Window.height
    UI.abdunkeln(w, h, 0.6)

    UI.text(40, 32, 'Spiel laden', UI::TEXT, 26)
    UI.text(40, 72, 'Welchen Spielstand möchtest du laden?', UI::TEXT_DIM, 15)

    SLOTS.each_with_index do |slot, i|
      y = 124 + i * 96
      sel = i == @sel
      UI.panel(80, y, w - 160, 84, sel ? 0x16283A : UI::PANEL, 0.95)
      UI.rahmen(80, y, w - 160, 84, sel ? UI::ACCENT : 0x2A3542,
                sel ? 0.8 : 0.5)
      info = @projekt ? Project.save_info(@projekt, slot) : nil
      if info
        UI.text(104, y + 16, "Slot #{slot}", UI::TEXT, 19)
        UI.text(104, y + 48,
                "Karte: #{info['map']}   #{info['saved'].to_s[0, 16]}",
                UI::TEXT_DIM, 15)
      else
        UI.text(104, y + 16, "Slot #{slot}", UI::TEXT_DIM, 19)
        UI.text(104, y + 48, 'Leer', UI::TEXT_DIM, 15)
      end
    end

    UI.text(80, h - 52,
            '[Hoch/Runter] Slot   [Enter] Laden   [Esc] Zurück',
            UI::TEXT_DIM, 14)
  end
end
