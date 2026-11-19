#encoding: utf-8
# ============================================================================
#  Kopfloser Test für NPCs (Etappe 13): Wegpunkt-Bewegung, Loop, Event-
#  Kopplung, Picking-Schnittstelle.
#     ruby -I game/engine tools/npc_test.rb
# ============================================================================

$LOAD_PATH.unshift(File.join(File.expand_path('..', __dir__), 'game', 'engine'))

module Rune
  ROOT = File.expand_path('..', __dir__)
  module Log
    def self.info(m) = nil
    def self.warn(m) = puts("WARN  #{m}")
    def self.error(m) = puts("ERROR #{m}")
  end

  @ppos = [99.0, 0.0, 99.0]   # Spieler weit weg = NPC unblockiert
  def self.ppos=(v)
    @ppos = v
  end
  module Player
    def self.position = Rune.instance_variable_get(:@ppos)
    def self.push_out(_cx, _cz, _r) = nil
  end
end

# Game.gy-Attrappe (Höhe 0), bevor npc.rb geladen wird.
module Game
  def self.gy(_x, _z) = 0.0
end

require 'event'
require 'interpreter'
require 'issues'
require 'node'
require 'scene'
require 'npc'

$fail = 0
def check(name)
  ok = yield
  puts(ok ? "  ok   #{name}" : "  FAIL #{name}")
  $fail += 1 unless ok
end

scene = Scene.new('Test')

npc = Npc.new(
  { 'name' => 'Wächter', 'x' => 0.0, 'z' => 0.0, 'speed' => 2.0,
    'path' => [[4.0, 0.0], [4.0, 4.0]],
    'commands' => [{ 'cmd' => 'text', 'lines' => ['Hallo'] }] },
  scene
)

check('NPC startet an seiner Position') { npc.position == [0.0, 0.0] }
check('NPC-Node in der Szene') { scene.root.children.any? { |n| n.name == 'npc_Wächter' } }

# --- Bewegung ----------------------------------------------------------------
npc.update(1.0)   # 2 m in Richtung (4, 0)
check('Bewegung Richtung Wegpunkt') { npc.position[0] > 1.9 && npc.position[1].abs < 0.01 }

npc.update(1.0)
check('Erster Wegpunkt exakt erreicht') { npc.position == [4.0, 0.0] }

npc.update(0.5)
check('Zweiter Wegpunkt wird angelaufen') { npc.position[1] > 0.0 && npc.position[0] == 4.0 }

npc.update(2.0)
check('Zweiter Wegpunkt erreicht') { npc.position == [4.0, 4.0] }

npc.update(2.0)
check('Loop: läuft wieder zum ersten Wegpunkt') { npc.position[0] == 4.0 && npc.position[1] < 4.0 }

# --- Event-Kopplung ----------------------------------------------------------
check('Event heisst wie der NPC') { npc.event.name == 'Wächter' }
check('Event-Position folgt Bewegung') { npc.event.x == npc.position[0] && npc.event.z == npc.position[1] }

px, pz = npc.position
check('Event near? an NPC-Position') { npc.event.near?(px + 0.5, pz) }
npc.event.manual_fire(Interpreter.new)
check('Event-Kommandos vorhanden') { !npc.event.commands.empty? }

# --- Picking-Schnittstelle ---------------------------------------------------
check('box_center auf Node-Position') { npc.box_center == npc.node.position }
check('pick_radius positiv') { npc.pick_radius > 0.0 }

# --- Ohne Wegpunkte: steht ---------------------------------------------------
stein = Npc.new({ 'name' => 'Stein', 'x' => 1.0, 'z' => 2.0 }, scene)
stein.update(5.0)
check('NPC ohne Wegpunkte bleibt stehen') { stein.position == [1.0, 2.0] }

# --- Geschwindigkeit ---------------------------------------------------------
schnell = Npc.new({ 'name' => 'Sprint', 'x' => 0.0, 'z' => 0.0, 'speed' => 10.0,
                    'path' => [[100.0, 0.0]] }, scene)
schnell.update(1.0)
check('Geschwindigkeit skaliert') { schnell.position[0] == 10.0 }

# --- Spieler blockt Weg (Etappe 15) ------------------------------------------
blocker = Npc.new({ 'name' => 'Block', 'x' => 0.0, 'z' => 0.0, 'speed' => 2.0,
                    'path' => [[10.0, 0.0]] }, scene)
Rune.ppos = [1.5, 0.0, 0.0]   # Spieler direkt auf der Laufstrecke
blocker.update(0.5)           # nächste Position (1,0) läge 0.5 m vom Spieler
check('NPC wartet, wenn Spieler im Weg') { blocker.position[0] == 0.0 }

Rune.ppos = [1.5, 0.0, 5.0]   # Spieler zur Seite
blocker.update(0.5)
check('NPC läuft weiter, wenn Weg frei') { blocker.position[0] > 0.9 }

# --- Snapshot / Restore (Etappe 15) ------------------------------------------
blocker.update(1.0)           # weiterlaufen lassen
snap = blocker.snapshot
check('Snapshot enthält Name/Position/Wegpunkt') do
  snap['name'] == 'Block' && snap['x'] == blocker.position[0] &&
    snap['z'] == blocker.position[1] && snap['wp'].is_a?(Integer)
end

blocker.update(2.0)           # irgendwo anders hin
blocker.restore!(snap['x'], snap['z'], snap['wp'])
check('Restore stellt Position wieder her') do
  blocker.position[0] == snap['x'] && blocker.position[1] == snap['z']
end
check('Restore stellt Wegpunkt wieder her') { blocker.snapshot['wp'] == snap['wp'] }

# Event-Position nach Restore synchron (E-Interaktion am geladenen Ort)
check('Event-Position nach Restore synchron') do
  blocker.event.x == blocker.position[0] && blocker.event.z == blocker.position[1]
end

Rune.ppos = [99.0, 0.0, 99.0]

puts($fail.zero? ? 'NPC-TEST OK (0 Fehler)' : "NPC-TEST FEHLGESCHLAGEN (#{$fail} Fehler)")
exit($fail.zero? ? 0 : 1)
