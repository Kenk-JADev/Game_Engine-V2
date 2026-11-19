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

module Rune
  module Camera
    @yaw = 0.0
    class << self
      attr_accessor :yaw
    end
  end
end

module Game
  @npcs = []
  class << self
    attr_accessor :npcs
  end

  def self.gy(_x, _z) = 0.0
end

# Scripts-Attrappe: script-Schritte landen hier (mover.rb prüft defined?).
module Scripts
  CALLS = []
  def self.run_code(name, code, *args)
    CALLS << [name, code]
    true
  end
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

  # Blickrichtung (19h.2): yaw in Radiant, für die Tests gemerkt.
  attr_reader :yaw
  def turn_to(y)
    @yaw = y.to_f
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
check('Sync: MapCheck-Aliase == Mover-Aliase') do
  MapCheck::MOVER_ALIASE.sort == Mover::ALIASE.keys.sort
end
check('Sync: MapCheck-XP-Codes == Mover-XP-Codes') do
  MapCheck::MOVER_XP_CODES.sort == Mover::XP_CODES.keys.sort
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

puts '== Mover: XP-Vokabular (19h.2, Game_Character 2) =='
check('normalize: XP-Code 14 = move_up') { Mover.normalize('14') == 'move_up' }
check('normalize: deutsche Aliase (19g) bleiben gültig') do
  Mover.normalize('up') == 'move_up' && Mover.normalize('warte') == 'wait' &&
    Mover.normalize('spieler_nah') == 'move_toward_player'
end
check('normalize: XP-Name bleibt') { Mover.normalize('move_down') == 'move_down' }

Mover.reset!
xp = FakeNpc.new('Xaver', 0.0, 0.0, 1.0)
Game.npcs = [xp]
Mover.start('Xaver', ['14'])   # XP-Code 14 = move_up
Mover.update(1.0)
check('XP-Code als Schritt läuft (move_up: z = -1)') { nah?(xp.position, [0.0, -1.0]) }

Mover.reset!
Mover.start('Xaver', %w[move_lower_left])
Mover.update(1.0)
d = Math.sqrt(0.5)
check('Diagonale: move_lower_left (-0.71/+0.71)') { nah?(xp.position, [-d, -1.0 + d]) }
check('Blickrichtung bei Diagonale bleibt (hier 2/unten)') { xp.yaw.zero? }

Mover.reset!
xp2 = FakeNpc.new('Xena', 0.0, 0.0, 1.0)
Game.npcs = [xp2]
Mover.start('Xena', %w[turn_left move_forward])
# Pro Update-Frame führt die Route genau EINEN neuen Schritt aus.
Mover.update(0.1)
check('turn_left: yaw = pi/2 (XP-Richtung 4)') { (xp2.yaw - Math::PI / 2).abs < 0.001 }
Mover.update(0.5)   # move_forward gestartet: halber Meter in 0.5 s
check('move_forward läuft in Blickrichtung (x = -0.5)') do
  nah?(xp2.position, [-0.5, 0.0])
end
Mover.update(0.5)
Mover.update(0.1)   # Restmeter + Abschluss-Frame
check('Route fertig nach einem Meter') { Mover.routes.empty? && nah?(xp2.position, [-1.0, 0.0]) }

Mover.reset!
dreher = FakeNpc.new('Dreh', 0.0, 0.0, 1.0)
Game.npcs = [dreher]
r = Mover.start('Dreh', %w[turn_down turn_90d_right turn_180d])
Mover.update(0.1)   # Schritt 1: turn_down
Mover.update(0.1)   # Schritt 2: turn_90d_right
check('turn_90d_right von unten(2) -> links(4): yaw pi/2') do
  (dreher.yaw - Math::PI / 2).abs < 0.001
end
Mover.update(0.1)   # Schritt 3: turn_180d
check('turn_180d von links(4) -> rechts(6): yaw -pi/2') do
  (dreher.yaw + Math::PI / 2).abs < 0.001
end
Mover.update(0.1)   # Abschluss-Frame
check('Dreh-Route ist danach fertig') { !Mover.running?(r) }

Mover.reset!
gucker = FakeNpc.new('Guck', 3.0, 4.0, 1.0)
Game.npcs = [gucker]
Rune::Player.pos = [0.0, 0.0, 0.0]
Mover.start('Guck', %w[turn_toward_player])
Mover.update(0.1)
check('turn_toward_player: größerer Achsenanteil (dz=4 -> Richtung 8, yaw pi)') do
  (gucker.yaw - Math::PI).abs < 0.001
end
Mover.reset!
Mover.start('Guck', %w[turn_away_from_player])
Mover.update(0.1)
check('turn_away_from_player: Richtung 2 (yaw 0)') { gucker.yaw.abs < 0.001 }

Mover.reset!
schalter = FakeNpc.new('Schalti', 0.0, 0.0, 1.0)
Game.npcs = [schalter]
Mover.start('Schalti', %w[switch_on], code: 'route_lief')
Mover.update(0.1)
check('switch_on setzt GameState-Schalter aus code') { GameState.switch('route_lief') }
Mover.reset!
Mover.start('Schalti', %w[switch_off], code: 'route_lief')
Mover.update(0.1)
check('switch_off löscht ihn wieder') { !GameState.switch('route_lief') }

Mover.reset!
flitzer = FakeNpc.new('Flitz', 0.0, 0.0, 1.0)
Game.npcs = [flitzer]
rr = Mover.start('Flitz', %w[change_speed move_up], code: 3.0)
Mover.update(0.1)   # Schritt 1: change_speed
check('change_speed: Tempo aus code (3 m/s)') { (rr.speed - 3.0).abs < 0.001 }
Mover.update(0.5)   # Schritt 2: move_up mit 3 m/s -> 1 m (Rest) in 0.33 s
check('change_speed wirkt: Meter mit 3 m/s gelaufen') do
  nah?(flitzer.position, [0.0, -1.0])
end
Mover.update(0.1)   # Abschluss-Frame
check('Tempo-Route fertig') { Mover.routes.empty? }

Mover.reset!
Scripte = Scripts
skriptler = FakeNpc.new('Skript', 0.0, 0.0, 1.0)
Game.npcs = [skriptler]
Scripte::CALLS.clear
Mover.start('Skript', %w[script], code: "log 'aus der Route'")
Mover.update(0.1)   # Schritt 1: script
check('script-Schritt ruft Scripts.run_code mit dem code') do
  Scripte::CALLS == [['move_route', "log 'aus der Route'"]]
end

Mover.reset!
huepfer = FakeNpc.new('Hüpf', 0.0, 0.0, 1.0)
Game.npcs = [huepfer]
Mover.start('Hüpf', %w[jump])
Mover.update(0.1)   # Schritt 1: jump
Mover.update(0.1)   # Abschluss-Frame
check('jump: Route endet ohne Bewegung (Effekt ohne Effects-Modul)') do
  Mover.routes.empty? && nah?(huepfer.position, [0.0, 0.0])
end

Mover.reset!
Rune::Player.pos = [0.0, 0.0, 0.0]
Mover.start('spieler', %w[turn_right])
Mover.update(0.1)
check('turn_right beim Spieler dreht die Kamera (yaw -pi/2)') do
  (Rune::Camera.yaw + Math::PI / 2).abs < 0.001
end
Rune::Camera.yaw = 0.0

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
check('deutsche 19g-Kurznamen bleiben gültig (Aliase)') do
  MapCheck.scan(karte_mit([{ 'cmd' => 'move_route', 'target' => 'x',
                             'steps' => %w[up warte spieler_nah] }]), **OPTS).empty?
end
check('XP-Code als Schritt bleibt gültig') do
  MapCheck.scan(karte_mit([{ 'cmd' => 'move_route', 'target' => 'x',
                             'steps' => %w[11 25] }]), **OPTS).empty?
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
funde = MapCheck.scan(karte_mit([{ 'cmd' => 'move_route', 'target' => 'x',
                                   'steps' => %w[turn_left jump script],
                                   'code' => 'log 1' }]), **OPTS)
check("XP-Vokabular gültig (turn_left/jump/script): keine Funde [#{funde.map(&:to_s).first(3).join(' | ')}]") do
  funde.empty?
end
check("switch_on ohne 'code' -> Warnung") do
  hat?(MapCheck.scan(karte_mit([{ 'cmd' => 'move_route', 'target' => 'x',
                                  'steps' => %w[switch_on] }]), **OPTS),
       "brauchen 'code'", MapCheck::WARNUNG)
end
check('CMD_TYPES zählen 24') { MapCheck::CMD_TYPES.size == 24 }

puts($fail.zero? ? 'MOVE-ROUTE-TEST OK (0 Fehler)' : "MOVE-ROUTE-TEST FEHLGESCHLAGEN (#{$fail} Fehler)")
exit($fail.zero? ? 0 : 1)
