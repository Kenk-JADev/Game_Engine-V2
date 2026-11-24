#encoding: utf-8
# ============================================================================
#  Scene_Base — kleine Szenen-Basis (XP/VX-Schreibweise).
#
#  main läuft EINMAL PRO FRAME (Rune3D ruft Graphics.scene_update auf), ohne
#  blockierenden Loop wie in XP's Main-Schleife. Zeichnen übernimmt Game.draw
#  mit draw, solange $scene gesetzt ist. Projekt-Skripte dürfen Scene_Base
#  und die konkreten Szenen überschreiben oder $scene selbst setzen, solange
#  das vor dem ersten Frame passiert (AB-Runde 9).
# ============================================================================
class Scene_Base
  # Ein Frame: Logik (XP-Name, aber ohne while-Schleife).
  def main
    update
  end

  def update; end

  def draw; end

  # Wird beim Szenenwechsel aufgerufen, wenn eine Szene aufräumen will.
  def terminate; end
end
