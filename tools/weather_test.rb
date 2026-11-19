#encoding: utf-8
# ============================================================================
#  Test für das Wetter (Etappe 17h):
#     ruby -I game/engine tools/weather_test.rb
#
#  Prüft ohne Engine: Weather.set/load_map, Partikel-Menge je Intensität,
#  Fallen/Respawn, draw (Rune::Gfx.box-Zähler) und den Interpreter-Befehl
#  {"cmd":"weather",...} samt Script-Sandbox-Helfer weather(...).
# ============================================================================

ROOT = File.expand_path('..', __dir__)
$LOAD_PATH.unshift(File.join(ROOT, 'game', 'engine'))

$fail = 0
def check(name)
  ok = yield
  puts(ok ? "  ok   #{name}" : "  FAIL #{name}")
  $fail += 1 unless ok
end

# --- Attrappen für die Engine-Bindings -------------------------------------
module Rune
  ROOT = ROOT
  module Log
    def self.info(msg)  end
    def self.warn(msg)  end
    def self.error(msg) end
  end
  module Sound
    @played = []
    class << self
      attr_reader :played
      def play(name, volume = 1.0) = (@played << [name, volume])
        def reset! = @played.clear
    end
  end
  module Player
    @pos = [0.0, 5.0, 0.0]
    class << self
      attr_accessor :pos
      def position = @pos
      def teleport(*) = true
    end
  end
  module Gfx
    @boxes = 0
    class << self
      attr_reader :boxes
      def box(*_args) = (@boxes += 1)
      def reset! = @boxes = 0
    end
  end
  module Camera
    class << self
      attr_accessor :yaw, :mode
    end
  end
end

def gy(_x, _z) = 0.0
module Game
  def self.gy(x, z) = 0.0
end

$project_root = File.join(ROOT, 'game', 'data')

require 'data'
require 'state'
require 'issues'
require 'weather'
require 'party'
require 'effects'
require 'interpreter'
require 'scripts'

# --- 1) Grundzustand / set ----------------------------------------------------
check('Start: kein Wetter, inaktiv') { !Weather.active? && Weather.type == 'kein' }
Weather.set('regen')
check('set regen') { Weather.active? && Weather.type == 'regen' }
check('Standard-Intensität 1.0') { Weather.intensity == 1.0 }
Weather.set('schnee', 2.0)
check('set schnee Intensität 2.0') { Weather.type == 'schnee' && Weather.intensity == 2.0 }
Weather.set('blubb')
check('unbekannter Typ = aus') { !Weather.active? && Weather.type == 'kein' }
Weather.set('regen', -5)
check('negative Intensität fiel auf 1.0 zurück') { Weather.intensity == 1.0 }

# --- 2) load_map aus Karten-JSON ----------------------------------------------
Weather.load_map('weather' => { 'type' => 'schnee', 'intensity' => 1.5 })
check('load_map übernimmt schnee 1.5') do
  Weather.type == 'schnee' && Weather.intensity == 1.5
end
Weather.load_map({})
check('load_map ohne weather-Feld = aus') { !Weather.active? }
Weather.load_map(nil)
check('load_map(nil) = aus') { !Weather.active? }

# --- 3) Partikel-Pflege -------------------------------------------------------
Weather.set('regen', 1.0)
Rune::Player.pos = [0.0, 5.0, 0.0]
Weather.update(0.016, 0.0, 5.0, 0.0)
check('Intensität 1.0 => 40 Partikel') { Weather.count == 40 }
Weather.set('regen', 2.0)
Weather.update(0.016, 0.0, 5.0, 0.0)
check('Intensität 2.0 => 80 Partikel') { Weather.count == 80 }
count_before = Weather.count
Weather.update(0.016, 0.0, 5.0, 0.0)
check('Menge bleibt konstant') { Weather.count == count_before }

# Partikel fallen (Regen 22 m/s * dt): hoch starten, damit sie nach
# dem Fall noch über dem Boden liegen und nicht sofort respawnen.
Weather.instance_variable_get(:@drops)[0..4].each_with_index do |p, i|
  p[:y] = 20.0 + i
end
ys_before = Weather.instance_variable_get(:@drops)[0..4].map { |p| p[:y] }
Weather.update(0.5, 0.0, 5.0, 0.0)
ys_after = Weather.instance_variable_get(:@drops)[0..4].map { |p| p[:y] }
check('Regen fällt 22 m/s') do
  (ys_before[0] - ys_after[0] - 11.0).abs < 2.0
end
check('Respawns landen wieder über dem Boden') do
  Weather.instance_variable_get(:@drops).all? { |p| p[:y] > 3.0 }
end

# Weit weg vom Spieler => Respawn im Umkreis.
Weather.set('schnee', 1.0)
Weather.update(0.016, 0.0, 5.0, 0.0)
drops = Weather.instance_variable_get(:@drops)
drops.each { |p| p[:x] = 999.0 }
Weather.update(0.016, 0.0, 5.0, 0.0)
drops = Weather.instance_variable_get(:@drops)
check('weite Partikel werden im Umkreis neu verteilt') do
  drops.all? { |p| (p[:x] - 0.0).abs < 35.0 }
end

# --- 4) draw -------------------------------------------------------------------
Rune::Gfx.reset!
Weather.draw
check('draw: eine Box je Partikel') { Rune::Gfx.boxes == Weather.count }
Weather.set('kein')
Rune::Gfx.reset!
Weather.draw
check('draw ohne Wetter zeichnet nichts') { Rune::Gfx.boxes.zero? }

# --- 5) Interpreter-Befehl ----------------------------------------------------
GameState.reset!({})
Weather.set('kein')
interp = Interpreter.new
interp.start([
  { 'cmd' => 'weather', 'name' => 'regen', 'intensity' => 1.5 },
  { 'cmd' => 'weather' },                    # ohne Name = aus
  { 'cmd' => 'weather', 'name' => 'schnee' }
])
3.times { interp.update(0.016) }
check('Interpreter: letzter Befehl gewinnt (schnee)') do
  Weather.type == 'schnee' && Weather.intensity == 1.0
end
check('Interpreter fertig') { !interp.running? }

# --- 6) Script-Sandbox ---------------------------------------------------------
ctx = Scripts::Context.new(interp, nil)
ctx.instance_eval('weather("regen", 2.0)', 'test', 1)
check('Script weather setzt regen 2.0') do
  Weather.type == 'regen' && Weather.intensity == 2.0
end
check('Script weather() liest') do
  ctx.instance_eval('weather()', 'test', 1) == 'regen'
end
ctx.instance_eval('weather("kein")', 'test', 1)
check('Script weather("kein") stellt ab') { !Weather.active? }

puts($fail.zero? ? 'WETTER-TEST OK (0 Fehler)' : "WETTER-TEST FEHLGESCHLAGEN (#{$fail} Fehler)")
exit($fail.zero? ? 0 : 1)
