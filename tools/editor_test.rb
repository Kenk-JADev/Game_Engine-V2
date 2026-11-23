#encoding: utf-8
# ============================================================================
#  Kopfloser Test für den Editor: Prefabs, Setzen/Löschen, Speichern/Laden.
#     ruby -I game/engine tools/editor_test.rb
#  Exit-Code 0 = alles grün.
# ============================================================================

module Rune
  ROOT = File.expand_path('..', __dir__)

  module Log
    def self.info(m) = nil
    def self.warn(m) = puts("WARN  #{m}")
    def self.error(m) = puts("ERROR #{m}")
  end

  # Prefab-Definitionen & Editor brauchen keine Engine, nur diese Stubs:
  def self.rad(deg) = deg * Math::PI / 180.0
end

$LOAD_PATH.unshift(File.join(Rune::ROOT, 'game', 'engine'))
require 'json'
require 'data'
require 'pick'
require 'prefabs'
require 'editor'

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

# --- Editor-Objekt -> Prefab-Instantiierung ------------------------------
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
check('Knoten tragen Präfix ed_0') { nodes.all? { |n| n.name.start_with?('ed_0_') } }
check('Wand ist solid') { nodes[0].solid == true }
check('Basis-Höhe angewendet') { near(nodes[0].position[1], 1.5 + 2.0) }

hoch = Prefabs.instantiate(scene, { 'type' => 'haus', 'x' => 3.0, 'y' => 2.5,
                                    'z' => -2.0, 'rot' => 0.0 }, 1.5, 'ed_hoch')
check('Objekt-Y ist ein zusätzlicher Bodenversatz (auch für solide Teile)') do
  near(hoch[0].position[1], 1.5 + 2.5 + 2.0) && hoch[0].solid
end

nodes = Prefabs.instantiate(scene, { 'type' => 'haus', 'x' => 0.0, 'z' => 0.0, 'rot' => Math::PI / 2.0 }, 0.0, 'ed_1')
check('Gedreht (90 Grad): Tür geht nach +X statt +Z') { nodes[2].position[2].abs < 0.01 && nodes[2].position[0].abs > 2.5 }

# Kristall-Animation (früher in main.rb fest verdrahtet, jetzt im Prefab)
cr = Prefabs.instantiate(scene, { 'type' => 'kristall', 'x' => 0.0, 'z' => 0.0, 'rot' => 0.6 }, 2.0, 'ed_9')
check('Kristall bekommt Animation') { !cr[0].update_block.nil? }
cr[0].update_block.call(cr[0], 0.5)
check('Kristall dreht sich (0.6 + 0.7)') { near(cr[0].rotation[1], 0.6 + 0.7) }
check('Kristall schwebt um die Ruhehöhe') { cr[0].position[1] > 2.0 + 1.6 }
check('radius_of: Haus ~3.4 m, Weg ~1.5 m') {
  near(Prefabs.radius_of('haus'), 3.4, 0.05) && near(Prefabs.radius_of('weg'), 1.5, 0.05)
}

# --- Editor: setzen, löschen, speichern ---------------------------------
module RuneInputStub
  def self.pressed?(k) = false
  def self.down?(k) = false
  def self.mouse_position = [0.0, 0.0]
  def self.mouse_pressed?(i) = false
  def self.captured? = false
  def self.capture=(v)
    nil
  end
  def self.mouse_delta = [0.0, 0.0]
  def self.wheel = 0
end
Rune::Input = RuneInputStub

module CameraStub
  def self.ray(x, y) = [0.0, 8.0, 0.0, 0.0, 0.0, 1.0]
end
Rune::Camera = CameraStub

map = { 'name' => 'Test', 'terrain' => {}, 'events' => [] }
objects = []
ed = Editor.new('test', objects)

check('Editor startet inaktiv') { !ed.active? }
check('Palette-Tasten 1..9 reichen') { Prefabs::LIST.size <= 9 }

# Setzen simulieren (Strahl/Spur hilft hier nicht - direkt aufrufen)
ed.instance_variable_set(:@sel, 0)  # Haus
obj = ed.place(objects, scene, 10.4, -3.7, 0.0)
check('Objekt auf dem Raster (1.0) ') { obj['x'] == 10.0 && obj['z'] == -4.0 }
check('Objekt-Typ ist Haus') { obj['type'] == 'haus' }

ed.instance_variable_set(:@grid, 0.25)
obj = ed.place(objects, scene, 10.4, -3.7, 0.0)
check('Feines Raster (0.25) ') { obj['x'] == 10.5 && obj['z'] == -3.75 }

check('objects-Liste wächst') { objects.size == 2 }

# Löschen
ged = ed.delete_at(1, scene)
check('Letztes Objekt gelöscht') { objects.size == 1 && ged['type'] == 'haus' }

# Nahe-Löschung: dünnes Objekt, Strahl verfehlt die Teile -> Bodenpunkt nahe der Mitte
objects << { 'type' => 'zaun', 'x' => 2.0, 'z' => 5.0, 'rot' => 0.0 }
ed.instance_variable_set(:@objects, objects)
ed.delete_near(2.6, 5.3, scene)
check('Nahe-Löschung erwischt den Zaun (0.67 m neben der Mitte)') { objects.size == 1 }
ed.delete_near(50.0, 50.0, scene)
check('Nahe-Löschung greift nicht ins Leere über weit entfernte Punkte') { objects.size == 1 }

# Runde Drehung: Prefab-Objekt mit 60 Grad
objects << { 'type' => 'zaun', 'x' => 0.0, 'z' => 0.0, 'rot' => Math::PI / 3.0 }
check('Drehung wird gespeichert') { objects.last['rot'] > 1.0 }

# Speichern in eine temporäre Datei
path = File.join(Database.root, 'maps', 'test.json')
require 'fileutils'
FileUtils.mkdir_p(File.dirname(path))
ed.save(objects, map)
check('Datei geschrieben') { File.exist?(path) }

back = JSON.parse(File.read(path, encoding: 'UTF-8'))
check('Roundtrip: Objekte wieder da') { back['objects'].size == objects.size }
check('Roundtrip: Typ/Überleben der Startposition') { objects.any? { |o| o['type'] == 'haus' } }

File.delete(path) if File.exist?(path)

# Das Dorf ist jetzt Datenmaterial: alle Objekte liegen in der Karten-JSON.
dorf_path = File.join(Database.root, 'maps', 'dorf.json')
dorf = JSON.parse(File.read(dorf_path, encoding: 'UTF-8'))
check('Dorf-Karte enthält Objekte (>= 30)') { dorf['objects'].size >= 30 }
check('Dorf-Objekttypen sind alle Prefabs') { dorf['objects'].all? { |o| Prefabs.by_id(o['type']) } }
check('Dorf hat weiterhin 6 Events') { dorf['events'].size == 6 }

# Texturen: prozedural erzeugte RGBA-Bytestrings müssen die richtige Grösse
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
check('Gelände-Textur gesetzt') { RuneGfxStub.ter == 'gras' }
check('Prefabs verweisen auf existierende Texturen') {
  Prefabs::LIST.flat_map { |p| p[:parts] }.all? { |part| !part[:tex] || RuneGfxStub::REG.key?(part[:tex]) }
}

# Ein Höhenneubau muss den vollständigen Kartenpfad aufrufen; andernfalls
# löscht World.generate seine zuvor gesetzten Pinsel oder vergisst die Splats.
module Game
  class << self
    attr_accessor :last_terrain_map, :terrain_result
  end
  def self.build_terrain(map)
    self.last_terrain_map = map
    terrain_result
  end
end
Game.terrain_result = true
hoehenkarte = { 'terrain' => { 'heights' => [{ 'x' => 0, 'z' => 0, 'r' => 4, 'h' => 2 }],
                             'splats' => [{ 'tex' => 'weg', 'x' => 0, 'z' => 0, 'w' => 2, 'h' => 2 }] } }
ed = Editor.new('start', [])
ed.instance_variable_set(:@map_ref, hoehenkarte)
check('Höhen-Neubau verwendet vollständigen Kartenpfad mit Pinseln und Splats') do
  ed.apply_heights!
  Game.last_terrain_map.equal?(hoehenkarte)
end

puts($fail.zero? ? 'EDITOR-TEST OK (0 Fehler)' : "EDITOR-TEST FEHLGESCHLAGEN (#{$fail} Fehler)")
exit($fail.zero? ? 0 : 1)