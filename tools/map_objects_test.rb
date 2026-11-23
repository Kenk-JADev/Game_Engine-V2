#encoding: utf-8
# ============================================================================
#  Kopflose Tests für Prefabs und den gemeinsamen Laufzeit-Szenenaufbau.
#  Verwendet echte Node-/Scene-/Prefab-Klassen und eine Physik-/Grafikattrappe;
#  weder ein Ingame-Editor noch Fenster, GPU oder Projektdaten werden benötigt.
#     ruby -I game/engine tools/map_objects_test.rb
# ============================================================================

module Rune
  ROOT = File.expand_path('..', __dir__)

  module Log
    def self.info(_message); end
    def self.warn(message) = puts("WARN  #{message}")
    def self.error(message) = puts("ERROR #{message}")
  end

  module Physics
    @solids = []
    class << self
      attr_reader :solids
    end
    def self.clear_solids = @solids.clear
    def self.add_solid(*args) = @solids << args
  end

  module Gfx
    REG = {}
    def self.texture(name, w, h, bytes) = (REG[name] = [w, h, bytes])
    def self.terrain_texture(name) = (@terrain = name)
    def self.terrain = @terrain
  end

  def self.rad(degrees) = degrees * Math::PI / 180.0
end

$LOAD_PATH.unshift(File.join(Rune::ROOT, 'game', 'engine'))
require 'json'
require 'data'
require 'node'
require 'scene'
require 'prefabs'
require 'map_objects'
require 'textures'
require 'issues'

$fail = 0
def check(name)
  ok = yield
  puts(ok ? "  ok   #{name}" : "  FAIL #{name}")
  $fail += 1 unless ok
rescue StandardError => e
  puts "  FAIL #{name} (#{e.class}: #{e.message})"
  $fail += 1
end

def near(a, b, eps = 1e-3) = (a - b).abs <= eps

puts '== Prefabs und Texturen =='
check('neun benannte Prefabs und Haus mit drei Teilen') do
  Prefabs::LIST.size == 9 && Prefabs::LIST.all? { |p| !p[:name].to_s.empty? } &&
    Prefabs.by_id('haus')[:parts].size == 3
end
check('Baum ist solid') { Prefabs.by_id('baum')[:parts].any? { |p| p[:solid] } }
check('unbekannter Typ ergibt kein Prefab') { Prefabs.by_id('unbekannt').nil? }
check('Prefab-Radius bleibt für Skripte verfügbar') do
  near(Prefabs.radius_of('haus'), 3.4, 0.05) && near(Prefabs.radius_of('weg'), 1.5, 0.05)
end

dorf = JSON.parse(File.read(File.join(Database.root, 'maps', 'dorf.json'), encoding: 'UTF-8'))
check('Demo-Karte enthält bekannte Prefab-Objekte und sechs Events') do
  dorf['objects'].size >= 30 && dorf['objects'].all? { |o| Prefabs.by_id(o['type']) } &&
    dorf['events'].size == 6
end
Textures.register_all
check('acht deterministische RGBA-Texturen registriert') do
  Rune::Gfx::REG.size == 8 && Rune::Gfx::REG.all? do |_name, (w, h, bytes)|
    w == 64 && h == 64 && bytes.bytesize == 64 * 64 * 4 && bytes.unpack('C*').sum > 0
  end && Textures.gras == Rune::Gfx::REG['gras'][2]
end
check('Prefab-Texturen sind in der Basis registriert') do
  Prefabs::LIST.flat_map { |p| p[:parts] }.all? { |part| !part[:tex] || Rune::Gfx::REG.key?(part[:tex]) }
end
Rune::Gfx.terrain_texture('gras')
check('Geländetextur an Grafik-API weitergegeben') { Rune::Gfx.terrain == 'gras' }

puts '== Szenenaufbau und Kollision =='
scene = Scene.new('Welt')
height_calls = []
height_at = lambda do |x, z|
  height_calls << [x, z]
  1.5
end
objects = [{ 'type' => 'haus', 'x' => 3.0, 'y' => 2.5, 'z' => -2.0, 'rot' => Math::PI / 2 },
           { 'type' => 'kristall', 'x' => 8.0, 'z' => 4.0, 'rot' => 0.6 }]
original = Marshal.dump(objects)
builder = MapObjects.new(scene, height_at: height_at)
builder.rebuild(objects)
haus = scene.root.children.select { |node| node.name.start_with?('ed_0_') }
kristall = scene.root.children.find { |node| node.name == 'ed_1_0' }
check('Haus und Kristall samt historischer Knotennamen aufgebaut') do
  haus.size == 3 && kristall && scene.root.children.size == 4
end
check('Geländehöhe für jedes Objekt an dessen Position abgefragt') do
  height_calls == [[3.0, -2.0], [8.0, 4.0]]
end
check('Y-Versatz des Objekts gilt für Geometrie und solide Teile') do
  near(haus[0].position[1], 1.5 + 2.5 + 2.0) && haus[0].solid &&
    Rune::Physics.solids.any? { |solid| near(solid[1], haus[0].position[1]) }
end
check('Haus-Drehung um 90 Grad verschiebt Tür nach +X') do
  near(haus[2].position[2], -2.0) && haus[2].position[0] > 5.5
end
check('alle erzeugten Teile besitzen den richtigen Objektindex') do
  haus.all? { |node| builder.object_index(node) == 0 } && builder.object_index(kristall) == 1
end
check('Kristall-Animation läuft in der echten Scene') do
  vorher = kristall.position[1]
  scene.update(0.5)
  near(kristall.rotation[1], 0.6 + 0.7) && kristall.position[1] > vorher
end
check('Kartenmodell wurde nicht geändert') { Marshal.dump(objects) == original }
check('solide Teile nach Szenenaufbau bei Physik registriert') do
  Rune::Physics.solids.size == scene.root.children.count(&:solid)
end

# Namensgleichheit darf Skriptknoten nicht in Kartenobjekte umdeuten oder
# beim Wiederaufbau löschen. Die Identität gehört jeweils zu genau einer Szene.
fremd = Node.new(name: 'ed_0_0', position: [20, 2, 20], solid: true)
scene.add(fremd)
old_haus = haus.first
builder.rebuild([{ 'type' => 'baum', 'x' => 5, 'z' => 7 }])
new_nodes = scene.root.children.reject { |node| node.equal?(fremd) }
check('Wiederaufbau entfernt nur eigene frühere Knoten, nicht namensgleiche Skriptknoten') do
  scene.root.children.include?(fremd) && !scene.root.children.include?(old_haus) &&
    new_nodes.any? && builder.object_index(fremd).nil?
end
check('alte Indizes sind vergessen, neue Teile zeigen auf den neuen Index') do
  builder.object_index(old_haus).nil? && new_nodes.all? { |node| builder.object_index(node) == 0 }
end
check('Physik enthält nach Wiederaufbau auch fremde solide Knoten') do
  Rune::Physics.solids.size == scene.root.children.count(&:solid) &&
    Rune::Physics.solids.any? { |solid| solid[0] == 20 }
end
check('unbekannte Prefabs erzeugen keine Knoten oder falsche Indizes') do
  builder.rebuild([{ 'type' => 'nicht_vorhanden', 'x' => 0, 'z' => 0 }])
  scene.root.children == [fremd] && builder.object_index(fremd).nil?
end

other = Scene.new('Kulisse')
other_builder = MapObjects.new(other, height_at: ->(_x, _z) { 0.0 })
other_builder.rebuild([{ 'type' => 'haus', 'x' => 0, 'z' => 0 }])
other_node = other.root.children.first
builder.rebuild([{ 'type' => 'kristall', 'x' => 1, 'z' => 1 }])
check('Spiel- und Kulissenszene behalten getrennte Knoten und Identitäten') do
  other.root.children.include?(other_node) && other_builder.object_index(other_node) == 0 &&
    builder.object_index(other_node).nil? && scene.root.children.include?(fremd)
end
check('kein Ingame-Editor wird vom Szenenaufbau geladen') { !defined?(Editor) }

puts($fail.zero? ? 'MAP-OBJECTS-TEST OK (0 Fehler)' : "MAP-OBJECTS-TEST FEHLGESCHLAGEN (#{$fail} Fehler)")
exit($fail.zero? ? 0 : 1)
