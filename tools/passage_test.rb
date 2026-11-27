#encoding: utf-8
# ============================================================================
#  Kopfloser Test für Passage (Etappe 19k.3): die zentrale Passierbarkeits-
#  Abfrage aus game/engine/passage.rb.
#     ruby -I game/engine tools/passage_test.rb
#  Exit-Code 0 = alles grün.
#
#  Geprüft wird:
#    - Block-Zonen: zonen/zone? (x/z = Mitte, Rand gehört dazu, kaputte
#      Einträge werden überlesen)
#    - Hindernisse: die solid_at?-Tastpunkte über der Steiggrenze der Physik
#      (Wand ja, flacher Kasten = Stufe nein, schwebender Kasten ja)
#    - blockiert? = Zone ODER Hindernis
# ============================================================================

$LOAD_PATH.unshift(File.join(File.expand_path('..', __dir__), 'game', 'engine'))

module Rune
  ROOT = File.expand_path('..', __dir__)

  module Log
    def self.info(m) = nil
    def self.warn(m) = puts("WARN  #{m}")
    def self.error(m) = puts("ERROR #{m}")
  end

  # Physik-Attrappe: solids = Liste von AABBs [x0, y0, z0, x1, y1, z1].
  module Physics
    @solids = []
    class << self
      attr_accessor :solids
    end

    def self.solid_at?(x, y, z)
      @solids.any? do |b|
        x >= b[0] && x <= b[3] && y >= b[1] && y <= b[4] && z >= b[2] && z <= b[5]
      end
    end
  end
end

# Game-Attrappe: Karte 120x90, Geländehöhe 0, Block-Zonen frei setzbar.
module Game
  @map = { 'name' => 'Testkarte', 'terrain' => { 'width' => 120.0, 'height' => 90.0 } }
  class << self
    attr_accessor :map
  end

  def self.gy(_x, _z) = 0.0
end

require 'passage'

$fail = 0
def check(name)
  ok = yield
  puts(ok ? "  ok   #{name}" : "  FAIL #{name}")
  $fail += 1 unless ok
rescue StandardError => e
  puts "  FAIL #{name} (Ausnahme: #{e.class}: #{e.message})"
  $fail += 1
end

puts '== Passage: Block-Zonen =='
Game.map['blocked'] = [{ 'x' => 10.0, 'z' => 10.0, 'w' => 4.0, 'h' => 2.0 }]
check('zonen liefert die Zonen der Karte') { Passage.zonen.size == 1 }
check('zonen_anzahl zählt mit') { Passage.zonen_anzahl == 1 }
check('zone?: Mitte der Zone') { Passage.zone?(10.0, 10.0) }
check('zone?: Rand gehört dazu') { Passage.zone?(12.0, 11.0) && Passage.zone?(8.0, 9.0) }
check('zone?: knapp ausserhalb') do
  !Passage.zone?(12.1, 10.0) && !Passage.zone?(10.0, 11.1)
end
check('zone?: Textzahlen (JSON-Schmutz) werden gelesen') { Passage.zone?('10', '10') }
Game.map['blocked'] = ['kaputt', { 'x' => 1.0 }]
check('kaputte Einträge blocken nichts') do
  Passage.zonen.size == 1 && !Passage.zone?(1.0, 1.0)
end
Game.map['blocked'] = nil
check('ohne blocked-Schlüssel ist nichts blockiert') { !Passage.zone?(10.0, 10.0) }

puts '== Passage: Hindernisse (Objekt-Kästen) =='
Game.map.delete('blocked')
Rune::Physics.solids = [[5.0, 0.0, 5.0, 7.0, 1.8, 7.0]]
check('hindernis?: hohe Wand') { Passage.hindernis?(6.0, 6.0) }
check('hindernis?: daneben ist frei') { !Passage.hindernis?(8.0, 6.0) }
Rune::Physics.solids = [[5.0, 0.0, 5.0, 7.0, 0.3, 7.0]]
check('hindernis?: flacher Kasten ist eine Stufe, kein Hindernis') do
  !Passage.hindernis?(6.0, 6.0)
end
Rune::Physics.solids = [[5.0, 1.0, 5.0, 7.0, 2.0, 7.0]]
check('hindernis?: schwebender Kasten in Körperhöhe') { Passage.hindernis?(6.0, 6.0) }
check('Tastpunkte liegen über der Steiggrenze') do
  Passage::TASTPUNKTE.all? { |h| h > Passage::STUFE }
end
check('hindernis? mit eigener Geländehöhe') do
  Rune::Physics.solids = [[5.0, 3.0, 5.0, 7.0, 4.0, 7.0]]
  Passage.hindernis?(6.0, 6.0, 3.0) && !Passage.hindernis?(6.0, 6.0, 0.0)
end
Rune::Physics.solids = []
check('ohne Hindernisse ist alles frei') { !Passage.hindernis?(6.0, 6.0) }

puts '== Passage: blockiert? (Zone ODER Hindernis) =='
Game.map['blocked'] = [{ 'x' => 20.0, 'z' => 20.0, 'w' => 2.0, 'h' => 2.0 }]
Rune::Physics.solids = [[30.0, 0.0, 30.0, 32.0, 2.0, 32.0]]
check('blockiert?: in der Zone') { Passage.blockiert?(20.0, 20.0) }
check('blockiert?: am Hindernis') { Passage.blockiert?(31.0, 31.0) }
check('blockiert?: freies Feld') { !Passage.blockiert?(1.0, 1.0) }
Game.map['blocked'] = []
Rune::Physics.solids = []
check('leere Liste: nichts blockiert') { !Passage.blockiert?(20.0, 20.0) }

puts($fail.zero? ? 'PASSAGE-TEST OK (0 Fehler)' : "PASSAGE-TEST FEHLGESCHLAGEN (#{$fail} Fehler)")
exit($fail.zero? ? 0 : 1)
