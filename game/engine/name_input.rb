#encoding: utf-8
# ============================================================================
#  Namenseingabe (Etappe 20e, RPG Maker XP 303 "Input Name Processing").
#
#  XP schreibt den eingegebenen Namen direkt in $game_actors[id].name; das
#  Fenster (Scene_Name) tippt nur. Hier macht das Modul die Rechenarbeit, das
#  Fenster zeichnet:
#
#    NameInput.start('arik', max: 8)
#    NameInput.active?          # Fenster offen? (Interpreter blockiert solange)
#    NameInput.buchstabe('K')
#    NameInput.backspace
#    NameInput.bestaetigen     # schreibt Party.rename
#    NameInput.abbruch         # Esc: alter Name bleibt
#
#  Zeichen: A-Z, a-z, 0-9 und Leerzeichen (XP erlaubt zusaetzlich Umlaute; die
#  Engine liest Tasten ueber Ein-Zeichen-Namen, deshalb bleiben Umlaute offen -
#  siehe Checkliste 20e). `max` ist auf 1..16 geklemmt (XP-Default 8).
# ============================================================================

module NameInput
  MAX_ZEICHEN = 16
  STANDARD_MAX = 8

  @aktiv = false
  @actor_id = nil
  @text = ''
  @max = STANDARD_MAX
  @cancel = true
  @meldung = ''

  class << self
    attr_reader :actor_id, :text, :max, :meldung

    def active? = @aktiv
    def cancel? = @cancel

    # Rückgabe false = kein Fenster (unbekannter Held).
    def start(actor_id, max: STANDARD_MAX, cancel: true)
      held = Party.held(actor_id)
      if held.nil?
        Rune::Log.warn("NameInput: Held '#{actor_id}' nicht in database.json")
        Issues.report('NameInput', "Held '#{actor_id}' nicht in database.json") if defined?(Issues)
        @aktiv = false
        return false
      end
      @actor_id = held.id
      @text = held.name.to_s
      @max = max.to_i.clamp(1, MAX_ZEICHEN)
      @cancel = cancel != false
      @meldung = ''
      @aktiv = true
      Rune::Log.info("NameInput: '#{@text}' (#{@max} Zeichen) fuer #{@actor_id}")
      true
    end

    def close
      @aktiv = false
      @actor_id = nil
      @text = ''
      @meldung = ''
    end

    # Ein Zeichen tippen (nur A-Z, a-z, 0-9, Leerzeichen).
    def buchstabe(zeichen)
      c = zeichen.to_s
      return false unless c.match?(/\A[A-Za-z0-9 ]\z/)
      return false if @text.size >= @max

      @text += c
      @meldung = ''
      true
    end

    # Feld leeren (XP erlaubt „alles löschen" vor dem Tippen).
    def leeren
      @text = ''
      @meldung = ''
      true
    end

    def backspace
      return false if @text.empty?

      @text = @text[0...-1]
      true
    end

    # E/Enter: leere Namen sind nicht erlaubt (XP piept hier nur).
    # Rückgabe true = Fenster schließen.
    def bestaetigen
      name = @text.strip
      if name.empty?
        @meldung = 'Bitte einen Namen eingeben.'
        return false
      end
      Party.rename(@actor_id, name)
      Rune::Log.info("NameInput: #{@actor_id} heisst jetzt #{name}")
      close
      true
    end

    # Esc: Der alte Name bleibt. Nur wenn der Befehl Abbruch erlaubt (XP
    # cancel), schließt Esc; sonst bleibt das Fenster offen.
    def abbruch
      unless @cancel
        @meldung = 'Abbruch ist gesperrt - Namen bestaetigen.'
        return false
      end
      close
      true
    end
  end
end
