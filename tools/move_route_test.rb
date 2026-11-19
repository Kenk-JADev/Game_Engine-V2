#encoding: utf-8
# ============================================================================
#  Kopfloser Test für Bewegungsrouten (Etappe 19g, RPG-Maker-XP-Feature).
#  Lässt sich ohne Engine starten:
#     ruby -I game/engine tools/move_route_test.rb
#  Exit-Code 0 = alles grün.
#
#  Geprüft wird:
#    - Mover: Schritte (up/down/left/right/zufall/spieler_nah/spieler_weg/
#      warte), Tempo/Zeit, repeat, stop, moving?, Routen-Ersetzung,
#      Spieler-Route (teleport-Gleiten), unbekannte Ziele/Schritte
#    - Interpreter: move_route startet, wait blockiert bis zum Ende,
#      repeat+wait blockiert NICHT (Warnung), ohne wait läuft er weiter
#    - MapCheck: target/steps-Pflicht, unbekannter Schritt, speed,
#      SCHRITTE-Sync mit Mover und die 24 Befehlstypen
# ============================================================================

# --- Minimal-Attrappe der Engine-Schnittstelle -----------------------------
module Rune
  ROOT = File.expand_path('..', __dir__)

  module Log
    def self.info(m) = nil
    def self.warn(m) = puts("WARN  #{m}")
    def self.error(m) = puts("ERROR #{m}")
  end

  module Player
    @pos = [0.0, 0.0, 0.0]
    class << self
      attr_accessor :pos
    end
    def self.teleport(x, y, z) = (self.pos = [x.to_f, y.to_f, z.to_f])
    def self.position = pos
  end
end

module Game
  @npcs = []
  class << self
    attr_accessor :npcs
  end

  def self.gy(_x, _z) = 0.0
end

$LOAD_PATH.unshift(File.join(Rune::ROOT, 'game', 'engine'))
require 'state'
require 'issues'
require 'mover'
require 'interpreter'
require 'mapcheck'

$fail = 0
def check(name)
  ok = yield
  puts(ok ? "  ok   #{name}" : "  FAIL #{name}")
  $fail += 1 unless ok
rescue StandardError => e
  puts "  FAIL #{name} (Ausnahme: #{e.class}: #{e.message})"
  $fail += 1
end

# NPC-Attrappe mit der Mover-Schnittstelle (position/move_to/speed/name).
class FakeNpc
  attr_reader :name, :speed, :spuren

  def initialize(name, x, z, speed = 1.0)
    @name = name
    @x = x.to_f
    @z = z.to_f
    @speed = speed
    @spuren = []
  end

  def position = [@x, @z]

  def move_to(x, z)
    @x = x.to_f
    @z = z.to_f
    @spuren << [@x.round(3), @z.round(3)]
  end
end

def nah?(a, b, eps = 0.001)
  (a[0] - b[0]).abs < eps && (a[1] - b[1]).abs < eps
end

Mover.reset!
Rune::Player.pos = [0.0, 0.0, 0.0]

puts '== Mover: Grundlagen =='
check('Sync: MapCheck-Schritte == Mover-Schritte') do
  MapCheck::MOVER_SCHRITTE == Mover::SCHRITTE
end
check('leere Schrittliste -> nil') { Mover.start('Wanda', []).nil? }
check('unbekanntes Ziel -> nil') { Mover.start('gibt_es_nicht', %w[up]).nil? }

wanda = FakeNpc.new('Wanda', 0.0, 0.0, 1.0)
Game.npcs = [wanda]
r = Mover.start('Wanda', %w[up])
check('Route gestartet') { r && Mover.running?(r) }
check('moving? für den NPC') { Mover.moving?(wanda) }
Mover.update(0.5)
check('halber Schritt nach 0.5 s (1 m/s)') { nah?(wanda.position, [0.0, -0.5]) }
Mover.update(0.5)
check('ganzer Schritt: z = -1 (up = -z)') { nah?(wanda.position, [0.0, -1.0]) }
Mover.update(0.1)
check('Route fertig: nicht mehr running') { !Mover.running?(r) }
check('fertig: Position bleibt') { nah?(wanda.position, [0.0, -1.0]) }
check('fertig: moving? false') { !Mover.moving?(wanda) }

puts '== Mover: Richtungen =='
Mover.reset!
hugo = FakeNpc.new('Hugo', 0.0, 0.0, 2.0)
Game.npcs = [hugo]
Mover.start('Hugo', %w[down])
Mover.update(0.5)   # 2 m/s * 0.5 s = 1 m
check('down = +z') { nah?(hugo.position, [0.0, 1.0]) }
Mover.reset!
Mover.start('Hugo', %w[left])
Mover.update(0.5)
check('left = -x') { nah?(hugo.position, [-1.0, 1.0]) }
Mover.reset!
Mover.start('Hugo', %w[right])
Mover.update(0.5)
check('right = +x') { nah?(hugo.position, [0.0, 1.0]) }

Mover.reset!
zufall = FakeNpc.new('Zofi', 0.0, 0.0, 1.0)
Game.npcs = [zufall]
Mover.start('Zofi', %w[zufall])
Mover.update(1.0)
d = Math.sqrt(zufall.position[0]**2 + zufall.position[1]**2)
check('zufall: genau 1 m weit, Richtung frei') { (d - 1.0).abs < 0.001 }

puts '== Mover: Spieler-Bezug =='
Mover.reset!
Rune::Player.pos = [0.0, 0.0, 0.0]
nah = FakeNpc.new('Nah', 3.0, 4.0, 1.0)
Game.npcs = [nah]
Mover.start('Nah', %w[spieler_nah])
Mover.update(1.0)
check('spieler_nah: 1 m auf den Spieler zu (3-4-5-Dreieck)') do
  nah?(nah.position, [2.4, 3.2])
end
Mover.reset!
weg = FakeNpc.new('Weg', 3.0, 4.0, 1.0)
Game.npcs = [weg]
Mover.start('Weg', %w[spieler_weg])
Mover.update(1.0)
check('spieler_weg: 1 m vom Spieler weg') { nah?(weg.position, [3.6, 4.8]) }

puts '== Mover: warte / repeat / stop / Ersetzung =='
Mover.reset!
warti = FakeNpc.new('Warti', 0.0, 0.0, 1.0)
Game.npcs = [warti]
Mover.start('Warti', %w[warte up])
Mover.update(0.3)
check('warte: bewegt nichts (0.3 s)') { nah?(warti.position, [0.0, 0.0]) }
Mover.update(0.6)
check('warte tickt 0.5 s - der überfällige Frame bewegt noch nichts') do
  nah?(warti.position, [0.0, 0.0])
end
Mover.update(0.5)
check('nach warte: up läuft (0.5 m)') { nah?(warti.position, [0.0, -0.5]) }
Mover.update(0.5)
check('up fertig gelaufen (1 m)') { nah?(warti.position, [0.0, -1.0]) }
Mover.update(0.1)
check('Route abgeschlossen') { Mover.routes.empty? }

Mover.reset!
kreisel = FakeNpc.new('Kreisel', 0.0, 0.0, 1.0)
Game.npcs = [kreisel]
rr = Mover.start('Kreisel', %w[up], repeat: true)
Mover.update(1.0)
Mover.update(1.0)
check('repeat: läuft endlos weiter (z = -2)') do
  nah?(kreisel.position, [0.0, -2.0]) && Mover.running?(rr)
end
Mover.stop(kreisel)
check('stop(npc) beendet die Route') { Mover.routes.empty? }

Mover.reset!
doppelt = FakeNpc.new('Doppelt', 0.0, 0.0, 1.0)
Game.npcs = [doppelt]
Mover.start('Doppelt', %w[up])
Mover.start('Doppelt', %w[down])
check('zweite Route ersetzt die erste') { Mover.routes.size == 1 }
Mover.update(1.0)
check('neue Route gilt: down = +z') { nah?(doppelt.position, [0.0, 1.0]) }

puts '== Mover: Spieler-Route =='
Mover.reset!
Rune::Player.pos = [0.0, 0.0, 0.0]
Mover.start('spieler', %w[right], speed: 2.0)
Mover.update(0.5)
check('spieler-Route gleitet per teleport (x=1, Bodenhöhe 0.2)') do
  Rune::Player.pos == [1.0, 0.2, 0.0]
end
Mover.update(0.5)
check('Route zu Ende (ein Schritt = ein Meter)') do
  Rune::Player.pos == [1.0, 0.2, 0.0] && Mover.routes.empty?
end

puts '== Interpreter: move_route =='
Mover.reset!
Rune::Player.pos = [0.0, 0.0, 0.0]
wache = FakeNpc.new('Wache', 0.0, 0.0, 10.0)
Game.npcs = [wache]
interp = Interpreter.new
interp.start([{ 'cmd' => 'move_route', 'target' => 'Wache', 'steps' => %w[up],
                'wait' => true },
              { 'cmd' => 'variable', 'name' => 'fertig', 'value' => 1 }])
interp.update(0.016)
check('wait: Interpreter blockiert') { interp.running? }
check('wait: Variable noch nicht gesetzt') { GameState.variable('fertig').zero? }
check('Route läuft') { Mover.active? }
Mover.update(0.5)
Mover.update(0.5)
check('Route fertig gelaufen') { !Mover.active? }
interp.update(0.016)
interp.update(0.016)
check('nach wait: Befehle laufen weiter') { GameState.variable('fertig') == 1.0 }
check('Interpreter fertig') { !interp.running? }

Mover.reset!
interp2 = Interpreter.new
interp2.start([{ 'cmd' => 'move_route', 'target' => 'Wache', 'steps' => %w[up up],
                 'wait' => true, 'repeat' => true },
               { 'cmd' => 'variable', 'name' => 'trotzdem', 'value' => 1 }])
interp2.update(0.016)
check('repeat+wait: blockiert NICHT (Endlos-Falle)') { !interp2.running? }
check('repeat+wait: Folgebefehl lief') { GameState.variable('trotzdem') == 1.0 }
Mover.reset!

interp3 = Interpreter.new
interp3.start([{ 'cmd' => 'move_route', 'target' => 'Wache', 'steps' => %w[up] },
               { 'cmd' => 'variable', 'name' => 'sofort', 'value' => 1 }])
interp3.update(0.016)
check('ohne wait: Liste läuft sofort weiter') { GameState.variable('sofort') == 1.0 }
check('ohne wait: Route läuft trotzdem nebenher') { Mover.active? }
Mover.reset!

puts '== MapCheck: move_route =='
OPTS = { prefabs: %w[haus], materials: %w[weg], scripts: [] }.freeze

def karte_mit(cmds)
  {
    'name' => 'Testkarte',
    'terrain' => { 'width' => 120.0, 'height' => 90.0, 'resolution' => 64,
                   'amplitude' => 0.0 },
    'objects' => [],
    'events' => [{ 'id' => 1, 'name' => 'Route', 'x' => 0.0, 'z' => 0.0,
                   'radius' => 2.5,
                   'pages' => [{ 'conditions' => {}, 'trigger' => 'action',
                                 'commands' => cmds }] }]
  }
end

def hat?(funde, fragment, severity = nil)
  funde.any? do |f|
    f.to_s.include?(fragment) && (severity.nil? || f.severity == severity)
  end
end

check('gültige Route: keine Funde') do
  MapCheck.scan(karte_mit([{ 'cmd' => 'move_route', 'target' => 'Wanderer',
                             'steps' => %w[up up warte], 'speed' => 1.5 }]),
                **OPTS).empty?
end
check("Route ohne 'target' -> Fehler") do
  hat?(MapCheck.scan(karte_mit([{ 'cmd' => 'move_route', 'steps' => %w[up] }]), **OPTS),
       "'target' fehlt", MapCheck::FEHLER)
end
check('Route ohne steps -> Fehler') do
  hat?(MapCheck.scan(karte_mit([{ 'cmd' => 'move_route', 'target' => 'x' }]), **OPTS),
       "'steps' fehlt/leer", MapCheck::FEHLER)
end
check('unbekannter Schritt -> Warnung') do
  hat?(MapCheck.scan(karte_mit([{ 'cmd' => 'move_route', 'target' => 'x',
                                  'steps' => %w[up huepf] }]), **OPTS),
       "unbekannter Schritt 'huepf'", MapCheck::WARNUNG)
end
check('speed <= 0 -> Warnung') do
  hat?(MapCheck.scan(karte_mit([{ 'cmd' => 'move_route', 'target' => 'x',
                                  'steps' => %w[up], 'speed' => 0 }]), **OPTS),
       "'speed' muss eine Zahl > 0 sein", MapCheck::WARNUNG)
end
check('CMD_TYPES zählen 24') { MapCheck::CMD_TYPES.size == 24 }

puts($fail.zero? ? 'MOVE-ROUTE-TEST OK (0 Fehler)' : "MOVE-ROUTE-TEST FEHLGESCHLAGEN (#{$fail} Fehler)")
exit($fail.zero? ? 0 : 1)
