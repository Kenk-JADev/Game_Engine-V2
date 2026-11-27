#encoding: utf-8
# ============================================================================
#  Scene_Map (Etappe 39) — XP-Seitenname für die laufende Spielszene.
#
#  Die 3D-Welt, Events und Interpreter bleiben in Game.update; diese Szene
#  existiert nur damit 089 - Main aus scripts.json wie in XP
#  `$scene.main` aufrufen kann. Ein Frame pro Tick, kein blockierender Loop.
# ============================================================================

require 'scene_base'

class Scene_Map < Scene_Base
  def update; end
end
