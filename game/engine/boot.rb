#encoding: utf-8
# Rune3D - Bootstrap. Wird einmal beim Start geladen, danach nur noch main.rb (F5).

module Rune
  # Winkel-Helfer: die Engine rechnet in Radiant, Skripte duerfen Grad benutzen.
  DEG2RAD = Math::PI / 180.0

  def self.rad(degrees)
    degrees * DEG2RAD
  end

  # Wird jeden Frame von der Engine aufgerufen (C++ -> Ruby).
  def self.tick(dt)
    Game.update(dt) if Game.respond_to?(:update)
    Game.draw       if Game.respond_to?(:draw)
  end

  module World
    # Gelaende erzeugen - mit benannten Argumenten, damit Skripte lesbar bleiben.
    # Rune::World.generate(width: 240, height: 180, resolution: 192, seed: 7,
    #                      amplitude: 0.0)   # amplitude 0 = ganz flach
    # size: wird als Quadrat weiter unterstuetzt (alte Karten).
    # height_brushes: gemalte Berge/Senken aus dem Editor (Etappe 18l) -
    # Liste von [x, z, radius, hoehe] (hoehe < 0 = Senke). Muss vor dem
    # generate_raw-Aufruf gesetzt werden.
    def self.generate(width: nil, height: nil, size: nil, resolution: 176,
                      seed: 7, amplitude: 5.0, height_brushes: nil)
      w = width || size || 220.0
      h = height || size || w
      self.height_brushes = height_brushes || []
      generate_raw(w.to_f, h.to_f, resolution, seed, amplitude)
    end
  end
end

$LOAD_PATH.unshift(File.join(Rune::ROOT, 'game', 'engine'))

require 'node'
require 'scene'
require 'ui'
require 'data'
require 'state'
require 'issues'
require 'party'
require 'event'
require 'npc'
require 'effects'
require 'weather'
require 'interpreter'
require 'scripts'
require 'pick'
require 'prefabs'
require 'editor'
require 'textures'
require 'sounds'
require 'project'
require 'launcher'

Rune::Log.info("Bootstrap fertig, lade game/main.rb")
load File.join(Rune::ROOT, 'game', 'main.rb')
