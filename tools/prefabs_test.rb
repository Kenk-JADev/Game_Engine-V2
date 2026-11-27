#encoding: utf-8
# ============================================================================
#  Kopfloser Test fuer Prefabs, Instantiierung und prozedurale Texturen.
#     ruby -I game/engine tools/prefabs_test.rb
#  Exit-Code 0 = alles gruen.
#
#  Vorgaenger war tools/editor_test.rb. Dessen Editor-Teil pruefte den alten
#  Ingame-Editor (game/engine/editor.rb), den es seit Etappe 19t nicht mehr
#  gibt: Der Editor ist das eigenstaendige C++-Programm RuneEditor.exe.
#  Die Prefab-/Textur-/Kartendaten-Pruefungen bleiben hier erhalten.
# ============================================================================

module Rune
  ROOT = File.expand_path('..', __dir__)

  module Log
    def self.info(m) = nil
    def self.warn(m) = puts("WARN  #{m}")
    def self.error(m) = puts("ERROR #{m}")
  end

  # Prefab-Definitionen brauchen keine Engine, nur diese Stubs:
  def self.rad(deg) = deg * Math::PI / 180.0
end

$LOAD_PATH.unshift(File.join(Rune::ROOT, 'game', 'engine'))
require 'json'
require 'data'
require 'pick'
require 'prefabs'

$fail = 0
def check(name)
  ok = yield
  puts(ok ? "  ok   #{name}" : "  FAIL #{name}")
  $fail += 1 unless ok
end

def near(a, b, eps = 1e-3) = (a - b).abs <= eps

# --- Prefabs -------------------------------------------------------------
check('9 Prefab-Typen in der Palette') { Prefabs::LIST.size == 9 }
check('jeder Typ hat einen Namen') { Prefabs::LIST.all? { |p| !p[:name].to_s.empty? } }
check('Haus-Prefab hat 3 Teile') { Prefabs.by_id('haus')[:parts].size == 3 }
check('Baum ist solid') { Prefabs.by_id('baum')[:parts].any? { |p| p[:solid] } }

# --- Kartendaten -> Prefab-Instantiierung --------------------------------
# Mini-Szene mit Node- und Scene-Klassen
class Node
  attr_accessor :name, :position, :rotation, :scale, :color, :solid
  attr_accessor :texture, :tile
  attr_reader :children, :update_block
  def initialize(name: '', position: [0.0, 0.0, 0.0], rotation: [0.0, 0.0, 0.0],
                 scale: [1.0, 1.0, 1.0], color: 0xFFFFFF, solid: false)
    @name = name; @position = position; @rotation = rotation
    @scale = scale; @color = color; @solid = solid; @children = []
  end
  def on_update(&b) = (@update_block = b)
end

class Scene
  attr_reader :root
  def initialize = (@root = Node.new(name: 'root'))
  def add(n) = @root.children << n
  def walk(n, &b) = (b.call(n); n.children.each { |c| walk(c, &b) })
  def refresh_solids = @root.children.delete_if { |c| c.name == 'warte_loeschen' }
end

module Game
  def self.gy(x, z) = ((x + z) % 2).abs * 0.1  # stub
end

scene = Scene.new
nodes = Prefabs.instantiate(scene, { 'type' => 'haus', 'x' => 3.0, 'z' => -2.0, 'rot' => 0.0 }, 1.5, 'ed_0')

check('Haus erzeugt 3 Knoten') { nodes.size == 3 }
check('Knoten tragen Praefix ed_0') { nodes.all? { |n| n.name.start_with?('ed_0_') } }
check('Wand ist solid') { nodes[0].solid == true }
check('Basis-Hoehe angewendet') { near(nodes[0].position[1], 1.5 + 2.0) }

nodes = Prefabs.instantiate(scene, { 'type' => 'haus', 'x' => 0.0, 'z' => 0.0, 'rot' => Math::PI / 2.0 }, 0.0, 'ed_1')
check('Gedreht (90 Grad): Tuer geht nach +X statt +Z') { nodes[2].position[2].abs < 0.01 && nodes[2].position[0].abs > 2.5 }

# Kristall-Animation (frueher in main.rb fest verdrahtet, jetzt im Prefab)
cr = Prefabs.instantiate(scene, { 'type' => 'kristall', 'x' => 0.0, 'z' => 0.0, 'rot' => 0.6 }, 2.0, 'ed_9')
check('Kristall bekommt Animation') { !cr[0].update_block.nil? }
cr[0].update_block.call(cr[0], 0.5)
check('Kristall dreht sich (0.6 + 0.7)') { near(cr[0].rotation[1], 0.6 + 0.7) }
check('Kristall schwebt um die Ruhehoehe') { cr[0].position[1] > 2.0 + 1.6 }
check('radius_of: Haus ~3.4 m, Weg ~1.5 m') {
  near(Prefabs.radius_of('haus'), 3.4, 0.05) && near(Prefabs.radius_of('weg'), 1.5, 0.05)
}
check('Palette-Tasten 1..9 reichen') { Prefabs::LIST.size <= 9 }

# Das Dorf ist Datenmaterial: alle Objekte liegen in der Karten-JSON.
dorf_path = File.join(Database.root, 'maps', 'dorf.json')
dorf = JSON.parse(File.read(dorf_path, encoding: 'UTF-8'))
check('Dorf-Karte enthaelt Objekte (>= 30)') { dorf['objects'].size >= 30 }
check('Dorf-Objekttypen sind alle Prefabs') { dorf['objects'].all? { |o| Prefabs.by_id(o['type']) } }
check('Dorf hat weiterhin 6 Events') { dorf['events'].size == 6 }

# Texturen: prozedural erzeugte RGBA-Bytestrings muessen die richtige Groesse
# haben, nicht leer und bei zweitem Aufruf identisch sein.
module RuneGfxStub
  REG = {}
  def self.texture(name, w, h, bytes)
    REG[name] = [w, h, bytes]
  end
  def self.terrain_texture(name) = (@ter = name)
  def self.ter = @ter
  def self.sky(t, b) = (@sky = [t, b])
  def self.sun(*a) = (@sun = a)
  def self.box(*a) = nil
end
Rune::Gfx = RuneGfxStub

require 'textures'
require 'issues'
Textures.register_all
check('8 Texturen registriert') { RuneGfxStub::REG.size == 8 }
check('alle Texturen 64x64 mit RGBA-Daten') {
  RuneGfxStub::REG.all? { |_, (w, h, b)| w == 64 && h == 64 && b.bytesize == 64 * 64 * 4 }
}
check('Texturen sind nicht leer (Helligkeit > 0)') {
  RuneGfxStub::REG.all? { |_, (_, _, b)| b.unpack('C*').sum > 0 }
}
check('Texturen sind deterministisch') { Textures.gras == RuneGfxStub::REG['gras'][2] }
RuneGfxStub.terrain_texture('gras')
check('Gelaende-Textur gesetzt') { RuneGfxStub.ter == 'gras' }
check('Prefabs verweisen auf existierende Texturen') {
  Prefabs::LIST.flat_map { |p| p[:parts] }.all? { |part| !part[:tex] || RuneGfxStub::REG.key?(part[:tex]) }
}

# Die Laufzeit darf keinen Ingame-Editor mehr mitbringen (Etappe 19t).
check('kein editor.rb im Laufzeitbaum') { !File.exist?(File.join(Rune::ROOT, 'game', 'engine', 'editor.rb')) }

puts($fail.zero? ? 'PREFABS-TEST OK (0 Fehler)' : "PREFABS-TEST FEHLGESCHLAGEN (#{$fail} Fehler)")
exit($fail.zero? ? 0 : 1)
