#encoding: utf-8
# ============================================================================
#  Kopfloser Test für das Partikelsystem (Etappe 14c): Spawn, Bewegung,
#  Schwerkraft, Lebenszeit, Clear.
#     ruby -I game/engine tools/effects_test.rb
# ============================================================================

$LOAD_PATH.unshift(File.join(File.expand_path('..', __dir__), 'game', 'engine'))

module Rune
  module Gfx
    def self.box(*_a) = nil
  end

  module Log
    def self.info(_m) = nil
    def self.warn(m) = puts("WARN  #{m}")
  end
end

require 'effects'

$fail = 0
def check(name)
  ok = yield
  puts(ok ? "  ok   #{name}" : "  FAIL #{name}")
  $fail += 1 unless ok
end

Effects.clear
check('Start: keine Partikel') { Effects.count.zero? }

Effects.burst(0.0, 1.0, 0.0, count: 10)
check('burst erzeugt Partikel') { Effects.count == 10 }

Effects.update(0.016)
check('Partikel bewegen sich') { Effects.count == 10 }

# Schwerkraft: vy sinkt
Effects.clear
Effects.burst(0.0, 0.0, 0.0, count: 1, speed: 0.0, life: 10.0, gravity: 10.0)
p = Effects::PARTS.first
vy0 = p[:vy]
Effects.update(0.5)
check('Schwerkraft wirkt') { p[:vy] < vy0 }
check('Position verändert sich') { p[:y] < 0.0 }

# Lebenszeit: Partikel verschwinden
Effects.clear
Effects.burst(0.0, 0.0, 0.0, count: 5, life: 0.2, gravity: 0.0)
Effects.update(1.0)
check('Partikel sterben ab') { Effects.count.zero? }

# draw ohne Engine darf nicht crashen
Effects.burst(0.0, 0.0, 0.0, count: 3)
Effects.draw
check('draw ohne Engine unschädlich') { true }

Effects.clear
check('clear räumt auf') { Effects.count.zero? }

# --- Integration: Interpreter-Befehl 'effect' -------------------------------
module Game
  def self.gy(x, z) = 0.0
end
require 'interpreter'
require 'issues'
require 'event'

Effects.clear
ev = Event.new('id' => 99, 'name' => 'Test', 'x' => 2.0, 'z' => 3.0,
               'commands' => [{ 'cmd' => 'effect', 'name' => 'sparkle' }])
interp = Interpreter.new
interp.start(ev.commands, ev)
interp.update(0.05)
check("Interpreter 'effect' erzeugt Partikel am Event") { Effects.count == 18 }
interp.update(0.05)
check('Interpreter danach wieder frei') { !interp.running? }
Effects.clear

puts($fail.zero? ? 'EFFECTS-TEST OK (0 Fehler)' : "EFFECTS-TEST FEHLGESCHLAGEN (#{$fail} Fehler)")
exit($fail.zero? ? 0 : 1)
