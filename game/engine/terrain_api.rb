#encoding: utf-8
# Geprüfte native Gelände-API mit lesbaren Schlüsselwortargumenten.
# Das Binding validiert Bereiche, Pinsel und Rechenbudget, bevor es castet.
module Rune
  module World
    # size: alte quadratische Karten; height_brushes: [{x,z,r,h}] aus JSON
    # oder [[x,z,r,h]] für Skripte. Fehlende Pinsel leeren den alten Satz.
    def self.generate(width: nil, height: nil, size: nil, resolution: 176,
                      seed: 7, amplitude: 5.0, height_brushes: nil)
      w = width.nil? ? (size.nil? ? 220.0 : size) : width
      h = height.nil? ? (size.nil? ? w : size) : height
      self.height_brushes = height_brushes.nil? ? [] : height_brushes
      # Keine to_f-Koerzierung: ungültige Kartenfelder bleiben erkennbar.
      generate_raw(w, h, resolution, seed, amplitude)
    end
  end
end
