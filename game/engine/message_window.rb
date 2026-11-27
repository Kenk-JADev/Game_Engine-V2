#encoding: utf-8
# ============================================================================
#  Nachrichtenfenster-Einstellungen (Etappe 20c, RPG Maker XP "Change Text
#  Options", Befehl 104).
#
#  In XP merkt sich $game_system Position, Rahmen und Transparenz des
#  Nachrichtenfensters; die Einstellung gilt bis zum nächsten 104 und
#  überlebt Kartenwechsel. Genau so liegt sie hier in einem Modul außerhalb
#  des Interpreters: Der Interpreter wird pro Karte neu gebaut, die
#  Fenster-Optionen sollen das aber nicht.
#
#    MessageWindow.set(position: 'oben', frame: false, transparent: true)
#    MessageWindow.position    # 'oben' | 'mitte' | 'unten'
#    MessageWindow.frame?     # true = Fensterrahmen zeichnen
#    MessageWindow.transparent?
#    MessageWindow.reset!     # Neues Spiel/Laden: XP-Defaults
#
#  Unbekannte Positionen werden verworfen (Stand bleibt), damit ein Tippfehler
#  in den Kartendaten das Fenster nicht unbrauchbar macht; MapCheck und der
#  Editor melden den Wert zusätzlich statisch.
# ============================================================================

module MessageWindow
  POSITIONEN = %w[oben mitte unten].freeze
  STANDARD_POSITION = 'unten'.freeze

  @position = STANDARD_POSITION
  @frame = true
  @transparent = false
  @skin_key = ''
  @speed = 0
  @no_fast = false

  class << self
    attr_reader :position, :skin_key, :speed, :no_fast

    def frame?
      @frame
    end

    def transparent?
      @transparent
    end

    # Etappe 48: Windowskin/Textgeschwindigkeit werden geführt; die Zeichnung
    # nutzt den Skin, sobald ein Projekt das Asset hat.
    def skin_key=(key)
      @skin_key = key.to_s
    end

    def speed=(value)
      @speed = value.to_i.clamp(0, 10)
    end

    def no_fast=(flag)
      @no_fast = !!flag
    end

    # Nur gesetzte (nicht-nil) Werte ändern; Rückgabe = true, wenn etwas
    # übernommen wurde.
    def set(position: nil, frame: nil, transparent: nil)
      changed = false
      unless position.nil?
        pos = position.to_s
        if POSITIONEN.include?(pos)
          changed = true if pos != @position
          @position = pos
        else
          Rune::Log.warn("MessageWindow: unbekannte Position '#{position}' ignoriert")
        end
      end
      unless frame.nil?
        changed = true if !!frame != @frame
        @frame = !!frame
      end
      unless transparent.nil?
        changed = true if !!transparent != @transparent
        @transparent = !!transparent
      end
      changed
    end

    # XP-Defaults (unten, Rahmen an, nicht transparent).
    def reset!
      @position = STANDARD_POSITION
      @frame = true
      @transparent = false
      @skin_key = ''
      @speed = 0
      @no_fast = false
      nil
    end
  end
end
