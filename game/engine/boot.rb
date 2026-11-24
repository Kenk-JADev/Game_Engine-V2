#encoding: utf-8
# Rune3D - Bootstrap. Wird einmal beim Start geladen, danach nur noch main.rb (F5).

module Rune
  # Winkel-Helfer: die Engine rechnet in Radiant, Skripte dürfen Grad benutzen.
  DEG2RAD = Math::PI / 180.0

  def self.rad(degrees)
    degrees * DEG2RAD
  end

  # Wird jeden Frame von der Engine aufgerufen (C++ -> Ruby).
  def self.tick(dt)
    Game.update(dt) if Game.respond_to?(:update)
    Game.draw       if Game.respond_to?(:draw)
  end

end

$LOAD_PATH.unshift(File.join(Rune::ROOT, 'game', 'engine'))

require 'terrain_api'
require 'node'
require 'scene'
require 'ui'
require 'data'
require 'state'
require 'issues'
require 'mapcheck'   # Etappe 19a: Karten-Daten prüfen (Regeln wie im Editor)
require 'passage'    # Etappe 19k.3: Passierbarkeit (Block-Zonen + Hindernisse)
require 'party'
require 'battle'     # Etappe 19i: Kampf-Grundgerüst (XP Scene_Battle)
require 'event'
require 'npc'
require 'mover'      # Etappe 19g: Bewegungsrouten (XP Move Routes)
require 'effects'
require 'weather'
require 'fade'        # Etappe 19d: Bildschirm ein-/ausblenden (XP-Feature)
require 'message_window' # Etappe 20c: Fenster-Optionen (XP 104)
require 'common_events'  # Etappe 20c: Gemeinsame Events (XP 117)
require 'temp_actions'   # Etappe 20d: $game_temp-Aufträge (LOGIK E.3)
require 'interpreter'
require 'scripts'
require 'rgss'       # Etappe 19e: RPG-Maker-XP-Schicht ($game_*, Graphics, ...)
require 'graphics'   # Etappe 19h: Sprite/Window/Viewport/$scene (RGSS-Vertiefung)
require 'pick'
require 'prefabs'
require 'map_objects'
require 'textures'
require 'sounds'
require 'project'
require 'scene_base'    # Szenen-Basis (Scene_Base) für Titel & Ladeschirm
require 'scene_title'   # AB: Startbildschirm Scene_Title statt Launcher
require 'scene_file'    # AB: Scene_File (Continue -> Slot-Wähler + Vorschau)
require 'scene_gameover' # Z4-K: Game-Over-Bildschirm (Taste -> Titel)

Rune::Log.info("Bootstrap fertig, lade game/main.rb")
load File.join(Rune::ROOT, 'game', 'main.rb')
