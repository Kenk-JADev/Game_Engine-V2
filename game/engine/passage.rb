#encoding: utf-8
# ============================================================================
#  Passage: die zentrale Passierbarkeits-Abfrage (Etappe 19k.3).
#
#  Zwei Dinge machen eine Stelle unpassierbar:
#    1. Block-Zonen  - „blocked" im Karten-JSON: [{x,z,w,h}] in Weltmetern,
#       x/z = MITTE, w/h = Größe. Unsichtbare Wände über alle Höhen
#       (Etappe 19k.2, Physik: Physics::add_blocked).
#    2. Hindernisse  - Objekt-Kästen („solid"-Teile der Prefabs), die höher
#       reichen als die Steiggrenze der Physik. Niedrige Kästen sind Stufen
#       und bleiben begehbar, genau wie beim Spieler (kMaxStep = 0.35 m).
#
#  Wer fragt?  $game_player.passable? (RGSS), NPC-Patrouillen (npc.rb),
#  Bewegungs-Routen (mover.rb). Eine Quelle statt dreier eigener Tests -
#  deshalb liegt die Logik hier und nicht in den Klassen.
#
#  Kopflos testbar: ruby -I game/engine tools/passage_test.rb
#  Ohne Engine (Tests) gibt es keine Physik: dann zählen nur die Block-Zonen
#  der Karte, die Hindernis-Abfrage liefert „frei".
# ============================================================================

module Passage
  # Steighöhe der Physik (src/world/physics.cpp: kMaxStep) - was darunter
  # bleibt, ist eine Stufe und kein Hindernis.
  STUFE = 0.35

  # Tastpunkte über dem Boden, an denen die Physik nach Hindernissen gefragt
  # wird: knapp über der Steiggrenze (Wand ja / Stufe nein) und in
  # Körperhöhe (erwischt auch schwebende oder überhängende Kästen).
  TASTPUNKTE = [STUFE + 0.1, 1.2].freeze

  class << self
    # Block-Zonen der aktuellen Karte (Game.map['blocked']) - nur Hashes,
    # damit kaputte JSON-Daten nicht mit NoMethodError durchschlagen
    # (MapCheck meldet sie als Fehler).
    def zonen
      karte = defined?(Game) && Game.respond_to?(:map) ? Game.map : nil
      return [] unless karte.is_a?(Hash)

      Array(karte['blocked']).select { |b| b.is_a?(Hash) }
    end

    # Liegt (x, z) in einer Block-Zone? Der Rand gehört dazu (wie in der
    # Physik: is_blocked prüft x0 <= x <= x1).
    def zone?(x, z)
      zonen.any? do |b|
        (x.to_f - b['x'].to_f).abs * 2.0 <= b['w'].to_f &&
          (z.to_f - b['z'].to_f).abs * 2.0 <= b['h'].to_f
      end
    end

    # Steht an (x, z) ein Hindernis, das höher als die Steiggrenze reicht?
    # gy = Geländehöhe (Fußboden) der Stelle; ohne Angabe wird sie geholt.
    def hindernis?(x, z, gy = nil)
      return false unless defined?(Rune::Physics) && Rune::Physics.respond_to?(:solid_at?)

      boden = gy
      boden = (Game.gy(x, z) if Game.respond_to?(:gy)) if boden.nil? && defined?(Game)
      boden = 0.0 if boden.nil?
      TASTPUNKTE.any? { |h| Rune::Physics.solid_at?(x.to_f, boden.to_f + h, z.to_f) }
    end

    # Die eine Abfrage für alles, was läuft: Spieler (passable?), NPCs,
    # Bewegungs-Routen. true = hier kommt niemand durch.
    def blockiert?(x, z, gy = nil)
      zone?(x, z) || hindernis?(x, z, gy)
    end

    # Zahl der Block-Zonen (Log/Tests) - ohne Engine immer 0.
    def zonen_anzahl
      zonen.size
    end
  end
end
