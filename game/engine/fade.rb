#encoding: utf-8
# ============================================================================
#  Bildschirmblende (Etappe 19d, RPG Maker XP: "Bildschirm aus-/einblenden").
#
#  Fade.start('aus', 0.5) dunkelt das Bild über eine halbe Sekunde ab,
#  Fade.start('ein', 0.5) hellt es wieder auf. Die Blende läuft nebenher -
#  das Spiel (Interpreter, Physik, Events) läuft währenddessen weiter, genau
#  wie bei XP. Gezeichnet wird über die UI-Schicht (Vollbild-Rechteck mit
#  Alpha), der Renderer braucht dafür keine Änderung.
#
#  Event-Befehl: {"cmd":"fade","direction":"aus","seconds":0.5}
#  Kartenwechsel/Playtest-Ende setzen die Blende zurück (Fade.reset!), damit
#  man nie mit schwarzem Bildschirm in einer Karte landet.
#
#  Kopflos testbar: ruby -I game/engine tools/fade_test.rb
# ============================================================================

module Fade
  RICHTUNGEN = %w[ein aus].freeze

  @alpha = 0.0   # aktuelle Abdunklung (0 = Bild sichtbar, 1 = schwarz)
  @ziel  = 0.0   # Ziel-Abdunklung
  @rate  = 0.0   # Änderung pro Sekunde

  class << self
    attr_reader :alpha
  end

  # Blende starten. richtung: 'aus' = Bild wird dunkel, 'ein' = Bild kommt
  # zurück. Sekunden unter 0.05 werden auf 0.05 begrenzt (Rate-Teiler).
  def self.start(richtung = 'aus', sekunden = 0.5)
    sekunden = sekunden.to_f
    sekunden = 0.05 if sekunden < 0.05
    @ziel = richtung.to_s == 'ein' ? 0.0 : 1.0
    @rate = (@ziel - @alpha).abs / sekunden
    Rune::Log.info(format('Fade: %s über %.2fs', richtung, sekunden))
    true
  end

  # Kurzformen wie bei XP (Fadeout/Fadein Screen).
  def self.out(sekunden = 0.5)
    start('aus', sekunden)
  end

  def self.in(sekunden = 0.5)
    start('ein', sekunden)
  end

  # Läuft gerade eine Blende (Alpha hat das Ziel noch nicht erreicht)?
  def self.laeuft?
    (@alpha - @ziel).abs > 0.0001
  end

  # Ist das Bild gerade (mindestens teilweise) abgedunkelt?
  def self.sichtbar?
    @alpha > 0.001
  end

  # Ein Frame: Alpha mit der Rate Richtung Ziel bewegen.
  def self.update(dt)
    return unless laeuft?

    schritt = @rate * dt.to_f
    if (@ziel - @alpha).abs <= schritt
      @alpha = @ziel
    else
      @alpha += @ziel > @alpha ? schritt : -schritt
    end
  end

  # Sofort auf "hell" zurücksetzen (Kartenwechsel, Playtest-Ende).
  def self.reset!
    @alpha = 0.0
    @ziel  = 0.0
    @rate  = 0.0
  end

  # Vollbild-Rechteck mit der aktuellen Abdunklung; ganz zum Schluss zeichnen.
  def self.draw(breite, hoehe)
    return unless sichtbar?

    alpha = @alpha > 1.0 ? 1.0 : @alpha
    UI.panel(0, 0, breite, hoehe, 0x000000, alpha)
  end
end
