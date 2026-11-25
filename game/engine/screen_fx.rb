#encoding: utf-8
# ============================================================================
#  Bildschirm-Effekte (Etappe 20f, RPG Maker XP 223 "Tone" und 224 "Flash").
#
#  XP führt beides in $game_screen (Game_Screen#start_tone_change /
#  start_flash) und zeichnet es in Scene_Map über der Welt. Rune3D hat keinen
#  Post-Processing-Pass; die Effekte laufen deshalb als Vollbild-Overlays in der
#  UI-Schicht (Rune::Gfx.rect) - wie UI.abdunkeln, nur mit Verlauf:
#
#    ScreenFx.tone(r: -34, g: 0, b: 68, gray: 0, frames: 60)
#    ScreenFx.flash('weiss', strength: 255, frames: 20)
#    ScreenFx.update(dt)   # im Engine-Takt, XP-Frames zu 40/s
#    ScreenFx.draw         # einmal pro Frame, über Welt und UI
#
#  Dauer in XP-Frames (Graphics.frame_rate, 40/s). `frames: 0` schaltet sofort.
#
#  Bewusst genähert (Doku: architecture.md, LOGIK AD.13):
#    - Positive Farbanteile (r/g/b > 0) legen einen Farb-Schleier auf, negative
#      einen dunklen Schleier im Gegenanteil; XP verschiebt dagegen die
#      Farbkanäle selbst (echtes Ton-Mapping bräuchte einen Shader).
#    - gray > 0 legt einen grauen Schleier darüber; echtes Entsättigen kann die
#      Rect-Schicht nicht.
#  Der Verlauf (Ein-/Ausblenden über frames) ist dagegen XP-gleich.
# ============================================================================

module ScreenFx
  FARBEN = {
    'weiss' => 0xFFFFFF, 'rot' => 0xFF4A4A, 'gruen' => 0x4AFF6B,
    'blau' => 0x4A8CFF, 'gelb' => 0xFFE14A
  }.freeze
  STANDARD_FARBE = 'weiss'
  MAX_TONE = 255      # XP: -255..255 je Kanal
  MAX_STAERKE = 255   # XP: Flash-Stärke 0..255

  @tone = nil          # { r:, g:, b:, gray: } aktuell
  @tone_von = nil      # Ausgangswert des laufenden Übergangs
  @tone_ziel = nil
  @tone_rest = 0.0     # Sekunden
  @tone_dauer = 0.0
  @flash = nil         # { farbe:, strength:, dauer:, rest: }

  class << self
    # Leser mit eigenem Namen: `tone`/`flash` sind die Setter (XP-Stil).
    def tone_aktuell = @tone
    def flash_aktuell = @flash

    def frame_rate
      defined?(Graphics) && Graphics.respond_to?(:frame_rate) ? Graphics.frame_rate.to_f : 40.0
    end

    # Bildschirmton setzen bzw. über `frames` dorthin überblenden.
    # Rückgabe false, wenn ein Wert außerhalb -255..255 liegt (dann passiert
    # nichts; MapCheck meldet solche Werte schon in den Daten).
    def tone(r: nil, g: nil, b: nil, gray: nil, frames: 0)
      ziel = { r: r.to_i, g: g.to_i, b: b.to_i, gray: gray.to_i }
      return false unless ziel.values.all? { |v| v.between?(-MAX_TONE, MAX_TONE) }

      dauer = dauer_sekunden(frames)
      if dauer.zero?
        @tone = ziel
        @tone_von = nil
        @tone_ziel = nil
        @tone_rest = 0.0
        @tone_dauer = 0.0
      else
        @tone_von = @tone || { r: 0, g: 0, b: 0, gray: 0 }
        @tone_ziel = ziel
        @tone_dauer = dauer
        @tone_rest = dauer
        @tone = @tone_von.dup
      end
      Rune::Log.info("ScreenFx: Ton r=#{ziel[:r]} g=#{ziel[:g]} b=#{ziel[:b]} " \
                     "gray=#{ziel[:gray]} (#{frames} Frames)")
      true
    end

    # Blitz: Farbe (weiss/rot/gruen/blau/gelb), Stärke 0..255, Dauer.
    def flash(farbe = STANDARD_FARBE, strength: MAX_STAERKE, frames: 20)
      name = farbe.to_s.strip.downcase
      name = STANDARD_FARBE if name.empty?
      return false unless FARBEN.key?(name)

      staerke = strength.to_i.clamp(0, MAX_STAERKE)
      dauer = dauer_sekunden(frames)
      # Dauer 0 = ein einzelner Frame in voller Stärke (XP zeigt ihn trotzdem).
      dauer = 1.0 / frame_rate if dauer.zero?
      @flash = { farbe: name, strength: staerke, dauer: dauer, rest: dauer }
      Rune::Log.info("ScreenFx: Blitz #{name} #{staerke} (#{frames} Frames)")
      true
    end

    def update(dt)
      if @tone_ziel && @tone_rest.positive?
        @tone_rest = [@tone_rest - dt, 0.0].max
        anteil = @tone_dauer.positive? ? (1.0 - @tone_rest / @tone_dauer) : 1.0
        @tone = @tone_von.each_with_object({}) do |(kanal, von), hash|
          hash[kanal] = (von + (@tone_ziel[kanal] - von) * anteil).round
        end
        if @tone_rest.zero?
          @tone = @tone_ziel.dup
          @tone_von = nil
          @tone_ziel = nil
          @tone_dauer = 0.0
        end
      end
      @flash[:rest] -= dt if @flash
      @flash = nil if @flash && @flash[:rest] <= 0.0
    end

    # Alles zurücksetzen (neues Spiel, F5) - XP: $game_screen = Game_Screen.new.
    def reset
      @tone = nil
      @tone_von = nil
      @tone_ziel = nil
      @tone_rest = 0.0
      @tone_dauer = 0.0
      @flash = nil
    end

    def aktiv?
      !@tone.nil? || !@flash.nil?
    end

    # Aktuelle Deckkraft des Blitzes (0..1) - für Tests und Skripte.
    def flash_alpha
      return 0.0 if @flash.nil? || @flash[:dauer] <= 0.0

      (@flash[:strength] / MAX_STAERKE.to_f) * (@flash[:rest] / @flash[:dauer])
    end

    # Einmal pro Frame zeichnen (nach Welt und UI, vor der Bildschirmblende).
    def draw
      return unless aktiv?

      w = Rune::Window.width
      h = Rune::Window.height
      if @tone
        zeichne_tone(w, h)
      end
      if @flash
        Rune::Gfx.rect(0, 0, w, h, FARBEN[@flash[:farbe]], flash_alpha)
      end
    end

    private

    def zeichne_tone(w, h)
      r = @tone[:r].clamp(-MAX_TONE, MAX_TONE)
      g = @tone[:g].clamp(-MAX_TONE, MAX_TONE)
      b = @tone[:b].clamp(-MAX_TONE, MAX_TONE)
      gray = @tone[:gray].clamp(-MAX_TONE, MAX_TONE)
      # Positive Anteile: Farb-Schleier in der Mischfarbe.
      pos = [r, g, b].map { |v| v.positive? ? v : 0 }
      staerkster = pos.max.to_i
      if staerkster.positive?
        farbe = (pos[0] << 16) | (pos[1] << 8) | pos[2]
        Rune::Gfx.rect(0, 0, w, h, farbe, staerkster / MAX_TONE.to_f * 0.55)
      end
      # Negative Anteile: dunkler Schleier im Gegenanteil (Annäherung an XP).
      neg = [r, g, b].map { |v| v.negative? ? -v : 0 }
      dunkel = neg.max.to_i
      if dunkel.positive?
        gegen = ((255 - neg[0]) << 16) | ((255 - neg[1]) << 8) | (255 - neg[2])
        Rune::Gfx.rect(0, 0, w, h, gegen, dunkel / MAX_TONE.to_f * 0.45)
      end
      # Grau: Schleier (echtes Entsättigen kann die Rect-Schicht nicht).
      if gray.positive?
        Rune::Gfx.rect(0, 0, w, h, 0x808080, gray / MAX_TONE.to_f * 0.40)
      end
    end

    def dauer_sekunden(frames)
      [frames.to_i, 0].max / frame_rate
    end
  end
end
