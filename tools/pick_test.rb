#encoding: utf-8
# ============================================================================
#  Kopfloser Test fuer das Picking (Strahl/Box, Strahl/Kugel, Auswahl).
#     ruby -I game/engine tools/pick_test.rb
#  Exit-Code 0 = alles gruen.
# ============================================================================

module Rune
  ROOT = File.expand_path('..', __dir__)

  module Log
    def self.info(m) = nil
    def self.warn(m) = puts("WARN  #{m}")
    def self.error(m) = puts("ERROR #{m}")
  end
end

$LOAD_PATH.unshift(File.join(Rune::ROOT, 'game', 'engine'))
require 'pick'

$fail = 0
def check(name)
  ok = yield
  puts(ok ? "  ok   #{name}" : "  FAIL #{name}")
  $fail += 1 unless ok
end

def near(a, b, eps = 1e-4)
  (a - b).abs <= eps
end

O = [0.0, 0.0, 0.0]
FWD = [0.0, 0.0, 1.0]

# --- Strahl gegen Box ------------------------------------------------------
t = Pick.ray_box(O, FWD, [0.0, 0.0, 10.0], [2.0, 2.0, 2.0])
check('Box frontal getroffen') { t && near(t, 9.0) }

check('Box daneben verfehlt') { Pick.ray_box(O, FWD, [5.0, 0.0, 10.0], [2.0, 2.0, 2.0]).nil? }

check('Box hinter dem Strahl verfehlt') { Pick.ray_box(O, FWD, [0.0, 0.0, -10.0], [2.0, 2.0, 2.0]).nil? }

t = Pick.ray_box([0.0, 0.0, 5.0], FWD, [0.0, 0.0, 6.0], [2.0, 2.0, 2.0])
check('Ursprung in der Box -> t = 0') { t && near(t, 0.0) }

t = Pick.ray_box([0.9, 0.0, 0.0], FWD, [0.0, 0.0, 10.0], [2.0, 2.0, 2.0])
check('Streifschuss trifft') { t && near(t, 9.0) }

check('Knapp daneben (1.01) verfehlt') { Pick.ray_box([1.01, 0.0, 0.0], FWD, [0.0, 0.0, 10.0], [2.0, 2.0, 2.0]).nil? }

# parallel zur X-Achse, Box liegt seitlich
check('Parallel daneben verfehlt') { Pick.ray_box(O, [1.0, 0.0, 0.0], [0.0, 5.0, 0.0], [2.0, 2.0, 2.0]).nil? }
t = Pick.ray_box([0.0, 1.0, 0.0], [1.0, 0.0, 0.0], [0.0, 0.0, 0.0], [2.0, 2.0, 2.0])
check('Parallel innerhalb der Slabs trifft') { t && near(t, 0.0) }

# schraeger Strahl
d = [0.0, 0.0, 1.0]
t = Pick.ray_box([3.0, 0.0, 0.0], d, [0.0, 0.0, 10.0], [2.0, 2.0, 2.0])
check('Seitlich vorbei verfehlt') { t.nil? }

# --- Strahl gegen Kugel ----------------------------------------------------
t = Pick.ray_sphere(O, FWD, [0.0, 0.0, 10.0], 1.0)
check('Kugel frontal getroffen') { t && near(t, 9.0) }
check('Kugel verfehlt') { Pick.ray_sphere(O, FWD, [3.0, 0.0, 10.0], 1.0).nil? }
t = Pick.ray_sphere([0.0, 0.0, 9.5], FWD, [0.0, 0.0, 10.0], 1.0)
check('Kugel: Start in der Kugel -> t = 0') { t && near(t, 0.0) }
check('Kugel: Start davor -> Eintritt') { near(Pick.ray_sphere([0.0, 0.0, 5.0], FWD, [0.0, 0.0, 10.0], 1.0), 4.0) }

# --- Auswahl des naechsten Objekts ----------------------------------------
class FakeNode
  attr_reader :name, :position, :scale
  def initialize(name, position, scale)
    @name = name; @position = position; @scale = scale
  end
end

class FakeEvent
  attr_reader :name
  def initialize(name, center, radius)
    @name = name; @c = center; @r = radius
  end
  def box_center = @c
  def pick_radius = @r
end

nahe = FakeNode.new('nah', [0.0, 0.0, 5.0], [2.0, 2.0, 2.0])
fern = FakeNode.new('fern', [0.0, 0.0, 20.0], [2.0, 2.0, 2.0])
event = FakeEvent.new('kristall', [0.0, 0.0, 12.0], 1.0)
daneben = FakeNode.new('daneben', [10.0, 0.0, 5.0], [1.0, 1.0, 1.0])

hit = Pick.closest(O, FWD, [fern, nahe, event, daneben])
check('naechstes Objekt gewinnt') { hit && hit[0].name == 'nah' }
check('Distanz stimmt') { hit && near(hit[1], 4.0) }

hit = Pick.closest(O, FWD, [fern, event, daneben])
check('Event hinter Box wird gefunden') { hit && hit[0].name == 'kristall' }
check('Event-Distanz stimmt') { hit && near(hit[1], 11.0) }

check('nichts in Reichweite -> nil') { Pick.closest(O, FWD, [daneben]).nil? }
check('max_dist begrenzt') { Pick.closest(O, FWD, [fern], max_dist: 10.0).nil? }

# --- Rotierte Boxen (Etappe 12) ---------------------------------------------
# Wuerfel 2x2x2 im Ursprung, 45 Grad um Y gedreht. Die rotierte Box ist eine
# Raute in der xz-Ebene mit Eckdistanz sqrt(2) ~ 1.414.
S2 = Math.sqrt(2.0)
dreh = Struct.new(:name, :position, :rotation, :scale).new(
  'gedreht', [0.0, 0.0, 0.0], [0.0, Math::PI / 4, 0.0], [2.0, 2.0, 2.0])

# Strahl von +x: trifft die Rautenseite bei x = sqrt(2), nicht bei x = 1
# (wie der alte Huellbox-Test es geliefert haette).
t = Pick.ray_rot_box([3.0, 0.0, 0.0], [-1.0, 0.0, 0.0],
                     dreh.position, dreh.scale, dreh.rotation)
check('45-Grad-Box: Eintritt an der Rautenseite') { t && near(t, 3.0 - S2) }

# Diagonalstrahl an der Ecke vorbei: trifft die alte Huellbox, aber nicht
# mehr die gedrehte Box.
nil45 = Pick.ray_rot_box([1.35, 0.0, 1.35], [-1.0 / S2, 0.0, 1.0 / S2],
                         dreh.position, dreh.scale, dreh.rotation)
check('45-Grad-Box: Huellbox-Fehltreffer eliminiert') { nil45.nil? }

# closest() beachtet rotation automatisch.
hit = Pick.closest([3.0, 0.0, 0.0], [-1.0, 0.0, 0.0], [dreh])
check('closest() trifft gedrehte Box') { hit && hit[0].name == 'gedreht' }
check('closest() Distanz korrekt') { hit && near(hit[1], 3.0 - S2) }

# Rotation um X: Strahl von oben trifft bei y = sqrt(2).
drehx = Struct.new(:name, :position, :rotation, :scale).new(
  'gekippt', [0.0, 0.0, 0.0], [Math::PI / 4, 0.0, 0.0], [2.0, 2.0, 2.0])
tx = Pick.ray_rot_box([0.0, 3.0, 0.0], [0.0, -1.0, 0.0],
                      drehx.position, drehx.scale, drehx.rotation)
check('X-Rotation: Eintritt von oben') { tx && near(tx, 3.0 - S2) }

# Ungedrehte Box verhaelt sich unveraendert.
gerade = Struct.new(:name, :position, :rotation, :scale).new(
  'gerade', [0.0, 0.0, 0.0], [0.0, 0.0, 0.0], [2.0, 2.0, 2.0])
tg = Pick.ray_rot_box([3.0, 0.0, 0.0], [-1.0, 0.0, 0.0],
                      gerade.position, gerade.scale, gerade.rotation)
check('Ungedreht: Eintritt bei 2.0') { tg && near(tg, 2.0) }

puts($fail.zero? ? 'PICK-TEST OK (0 Fehler)' : "PICK-TEST FEHLGESCHLAGEN (#{$fail} Fehler)")
exit($fail.zero? ? 0 : 1)