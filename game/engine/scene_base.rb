#encoding: utf-8
# ============================================================================
#  Scene_Base — kleine Szenen-Basis (XP/VX-Schreibweise).
#
#  main läuft EINMAL PRO FRAME (Root-Fiber über Graphics.scene_update oder seit
#  Etappe 39 der Main-Fiber aus 089 - Main), ohne blockierenden Loop wie in
#  XPs Main-Schleife. Zeichnen übernimmt Game.draw
#  mit draw, solange $scene gesetzt ist. Projekt-Skripte dürfen Scene_Base
#  und die konkreten Szenen überschreiben oder $scene selbst setzen, solange
#  das vor dem ersten Frame passiert (AB-Runde 9).
# ============================================================================
class Scene_Base
  # Ein Frame: Logik (XP-Name, aber ohne while-Schleife). Der guarded Yield
  # gehört seit Etappe 39 zum Main-Fiber: im Root-Fiber passiert nichts.
  def main
    scene = self
    Graphics.update if defined?(Graphics) && Graphics.respond_to?(:update)
    # Wurde $scene während des Frame-Yields ersetzt (z. B. Direktstart setzt
    # Scene_Map, während der Fiber noch in Scene_Title.main steht), darf die
    # alte Szene keine Eingaben mehr verarbeiten.
    return unless $scene.equal?(scene)

    update
    Scripts.yield_frame if defined?(Scripts) && Scripts.respond_to?(:yield_frame)
  end

  def update; end

  def draw; end

  # Wird beim Szenenwechsel aufgerufen, wenn eine Szene aufräumen will.
  def terminate; end
end
