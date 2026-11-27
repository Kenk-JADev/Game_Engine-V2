#encoding: utf-8
# ============================================================================
#  Scene_Gameover (XP 080) — „Game Over"-Bildschirm (Runde 14 / Z4-K).
#
#  $game_temp.gameover blendet hierher um, statt wie bisher direkt zum
#  Titel zu springen (dort sah der Spieler nur das Log). Eine Taste führt
#  weiter zum Titelbildschirm, wie in XP. main läuft pro Frame
#  (Scene_Base), kein blockierender Loop; die ersten WARTEZEIT Sekunden
#  ignorieren Eingaben, damit die Kampf-Bestätigung nicht durchschlägt.
# ============================================================================
class Scene_Gameover < Scene_Base
  WARTEZEIT = 0.4

  def initialize
    super()
    @start = Time.now.to_f
  end

  def update
    return if Time.now.to_f - @start < WARTEZEIT
    return unless Rune::Input.pressed?(:enter) ||
                  Rune::Input.pressed?(:space) ||
                  Rune::Input.pressed?(:escape)

    Rune::Sound.play('klick', 0.7)
    Game.to_title!
  end

  def draw
    w = Rune::Window.width
    h = Rune::Window.height
    # Vollflächig schwarz - unabhängig davon, wann das Clear-Color greift.
    UI.panel(0, 0, w, h, 0x000000, 1.0)
    titel = 'Game Over'
    tw = (titel.size * 0.62 * 48).to_i
    UI.text((w - tw) / 2, (h * 0.34).to_i, titel, UI::DANGER, 48)
    hinweis = 'Die Party ist besiegt.'
    hw = (hinweis.size * 0.62 * 18).to_i
    UI.text((w - hw) / 2, (h * 0.34).to_i + 76, hinweis, UI::TEXT, 18)
    weiter = '[Enter] Weiter zum Titel'
    ww = (weiter.size * 0.62 * 16).to_i
    UI.text((w - ww) / 2, h - 80, weiter, UI::TEXT_DIM, 16)
  end
end
