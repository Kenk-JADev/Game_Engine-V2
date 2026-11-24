#encoding: utf-8
# ============================================================================
#  Integrationstest für das Spielmenü (Etappe 20b).
#  ruby -I game/engine tools/menu_test.rb
#
#  Lädt game/main.rb mit einer Engine-Attrappe, aber echten Party-, Battle-,
#  RGSS- und Interpreter-Modulen und der echten Demo-Datenbank. Geprüft wird
#  die XP-Befehlsliste (Gegenstand/Fertigkeit/Status/...), die Unterfenster
#  inklusive Gegenstands-Zielwahl, das Ansehen von Fertigkeiten und Status
#  sowie das Esc-Verhalten Ebene für Ebene.
# ============================================================================

require 'json'

module Rune
  ROOT = File.expand_path('..', __dir__)

  module Log
    @warnings = []
    def self.warnings = @warnings
    def self.info(_message); end
    def self.warn(message) = @warnings << message
    def self.error(_message); end
  end

  module World
    @result = true
    class << self
      attr_accessor :result, :last_args, :ready
    end
    def self.ready? = !!ready
    def self.height_at(x, z) = 1.0 + x * 0.1 + z * 0.01
    def self.generate(**kwargs)
      self.last_args = kwargs
      result
    end
  end

  module Gfx
    @calls = []
    class << self
      attr_reader :calls
    end
    def self.reset_calls = @calls.clear
    def self.sky(*args) = (@calls << [:sky, *args])
    def self.sun(*args) = (@calls << [:sun, *args])
    def self.fog(*args) = (@calls << [:fog, *args])
    def self.box(*args) = (@calls << [:box, *args])
    def self.rect(*args) = (@calls << [:rect, *args])
    def self.text(*args) = (@calls << [:text, *args])
    def self.clear_color=(_color); end
    def self.terrain_texture(*args) = (@calls << [:texture, *args])
    def self.terrain_splats(*args)
      @calls << [:splats, *args]
      Rune::World.ready = false if args[1] == false # das native Binding verwirft den Boden
    end
  end

  module Physics
    def self.solid_count = 0
  end

  module Player
    @pos = [0.0, 0.0, 0.0]
    @ground = true
    class << self
      attr_accessor :pos, :ground
    end
    def self.position = @pos
    def self.teleport(x, y, z) = (self.pos = [x.to_f, y.to_f, z.to_f])
    def self.on_ground? = @ground
    def self.push_out(*_args); end
  end

  module Camera
    class << self
      attr_accessor :yaw, :mode, :distance
    end
    def self.info = 'Testkamera'
    def self.ray(_x, _y) = []
  end

  module Window
    class << self
      attr_accessor :title
    end
    def self.width = 1280
    def self.height = 720
  end

  module Time
    def self.fps = 60.0
  end

  module Input
    @keys = []
    @previous_keys = []
    class << self
      attr_accessor :keys, :capture
    end
    # Wie InputState::pressed: nur die Flanke ist gedrückt, während down?
    # auch beim Halten wahr bleibt. Mehrere Abfragen desselben Frames sind OK.
    def self.pressed?(key) = @keys.include?(key) && !@previous_keys.include?(key)
    def self.down?(key) = @keys.include?(key)
    def self.end_frame = (@previous_keys = @keys.dup)
    def self.captured? = !!capture
    def self.mouse_position = [640, 360]
    def self.mouse_pressed?(_button) = false
  end

  module Sound
    def self.play(*_args); end
    def self.loop(*_args); end
  end

  module Simulation
    class << self
      attr_accessor :paused
    end
  end
end

module Sounds
  def self.register_all; end
end

module Textures
  def self.register_all; end
  def self.register_project; end
  def self.begin_project; end
  def self.end_project; end
  def self.tile_of(_name) = 3.5
end

module Project
  class << self
    attr_accessor :save_data
  end
  def self.load_save(_name, _slot) = @save_data
  def self.save(_name, _slot, data) = (@save_data = data; true)
  def self.save_info(_name, _slot) = nil
  def self.list = []
end

module Weather
  def self.load_map(_map); end
  def self.update(*_args); end
  def self.draw; end
end

module Issues
  @entries = []
  class << self
    attr_reader :entries
  end
  def self.clear! = @entries.clear
  def self.report(*args, **_kwargs) = @entries << args
end

class Scene
  attr_reader :updates, :root
  def initialize(_name)
    @updates = 0
    @root = Node.new(name: 'root')
  end
  def add(node) = (@root.children << node; node)
  def walk(node, &block) = (block.call(node); node.children.each { |c| walk(c, &block) })
  def refresh_solids; end
  def update(_dt) = (@updates += 1)
  def draw; end
end

$LOAD_PATH.unshift(File.join(Rune::ROOT, 'game', 'engine'))
require 'data'
require 'state'
require 'party'
require 'battle'
require 'node'
require 'prefabs'
require 'map_objects'
require 'ui'
require 'event'
require 'npc'
require 'interpreter'
require 'mover'
require 'effects'
require 'fade'
require 'scripts'
require 'rgss'

# Fixtures für die Karten, aber database.json und Battle bleiben echt.
module Database
  @test_maps = {
    'start' => { 'name' => 'Start', 'terrain' => {}, 'objects' => [], 'events' => [],
                 'start' => { 'x' => 0, 'z' => 0 },
                 'encounters' => { 'troops' => ['waldweg'], 'steps' => 1 } },
    'weit' => { 'name' => 'Weit', 'terrain' => {}, 'objects' => [], 'events' => [],
                'start' => { 'x' => 100, 'z' => 100 },
                'encounters' => { 'troops' => ['banditen'], 'steps' => 1 } }
  }
  def self.map(id) = @test_maps.fetch(id)
  def self.map_ids = @test_maps.keys
  # F4: load_game prüft die Zielkarte über playtest_map (nil = fehlt) -
  # die Attrappe muss dieselbe Schnittstelle anbieten wie die echte
  # Database, sonst bricht der Spielstand-Lauftest ab.
  def self.playtest_map(id) = @test_maps[id]
  def self.system = { 'start' => { 'map' => 'start' } }
end

# Die Render-/Picking-Schicht wird nicht für die Spiellogik benötigt.
module Graphics
  class << self
    attr_accessor :callback
  end
  def self.scene_update = @callback&.call
  def self.scene_reset!; end
  def self.draw; end
end

# main.rb ruft beim Laden Game.setup auf: Genau diesen Kaltstart mit nil-
# Globalen wollen wir prüfen, nicht ein nachträgliches RGSS.setup im Test.
$project_root = File.join(Rune::ROOT, 'game', 'data')
load File.join(Rune::ROOT, 'game', 'main.rb')
# Titel-Szenen wie im echten boot.rb (AB): Launcher existiert nicht mehr.
require 'scene_base'
require 'scene_title'
require 'scene_file'
require 'scene_gameover' # Z4-K: process_temp_scene_change stellt die Szene

$fail = 0
def check(name)
  ok = yield
  puts(ok ? "  ok   #{name}" : "  FAIL #{name}")
  $fail += 1 unless ok
rescue StandardError => e
  puts "  FAIL #{name} (#{e.class}: #{e.message})"
  $fail += 1
end

def counter(value)
  $game_player.instance_variable_set(:@encounter_count, value)
end

# Logik-Frame: Wie im nativen Loop gibt Esc vor Game.update den Mausfang frei;
# der vorige Tastenzustand wird erst nach dem Frame aktualisiert. Das Zeichnen
# wird hier nur in eigenen Checks und ohne Tasteneingabe geprüft.
def game_frame(dt = 0.016)
  Rune::Input.capture = false if Rune::Input.pressed?(:escape)
  Game.update(dt)
ensure
  Rune::Input.end_frame
end

def prepare
  Battle.abort!
  # Nach to_title! steht der Modus auf :title; Setup soll wieder ins Spiel
  # führen (F5-Neustart), nicht den Titel neu aufbauen.
  Game.instance_variable_set(:@mode, nil)
  Game.setup
  Game.instance_variable_set(:@intro, nil)
  Rune::Input.keys = []
  Rune::Input.capture = true
  Rune::Player.ground = true
  # Erster Frame nimmt die Startposition als Referenz auf, ohne Schritt.
  game_frame(0.016)
end

def step(distance = 0.7)
  x, y, z = Rune::Player.position
  Rune::Player.pos = [x + distance, y, z]
  game_frame(0.016)
end

# Tastendruck mit Flanke: nach jedem Druck ein Frame ohne Taste, damit
# dieselbe Taste im nächsten Schritt wieder als Flanke gilt.
def druecke(*keys)
  keys.each do |key|
    Rune::Input.keys = key ? [key] : []
    game_frame
    next unless key

    Rune::Input.keys = []
    game_frame
  end
end

def screen = Game.instance_variable_get(:@menu_screen)
def menu_sel = Game.instance_variable_get(:@menu_sel)
def menu_sub = Game.instance_variable_get(:@menu_sub)
def menu_hero = Game.instance_variable_get(:@menu_hero)
def menu_msg = Game.instance_variable_get(:@menu_msg)
def menu_offen? = !!Game.instance_variable_get(:@menu)

puts '== Befehlsliste wie XP =='
prepare
druecke(:escape)
check('Esc öffnet das Menü') { menu_offen? }
check('Menü pausiert die Simulation und gibt den Zeiger frei') do
  Rune::Simulation.paused && !Rune::Input.captured?
end
labels = Game.instance_variable_get(:@menu_items).map { |it| it['label'] }
check('erste drei Punkte: Gegenstand/Fertigkeit/Status') do
  labels.first(3) == %w[Gegenstand Fertigkeit Status]
end
check('Speichern/Laden und Rückwege bleiben vorhanden') do
  labels.count { |l| l.start_with?('Speichern') } == 3 &&
    labels.count { |l| l.start_with?('Laden') } == 3 &&
    labels.include?('Zurück zum Spiel') && labels.include?('Zum Titelbildschirm')
end

puts '== Gegenstände im Menü verwenden =='
Party.find('arik').hp = 50
druecke(:enter)
check('Gegenstand öffnet die Itemliste') { screen == :item }
check('Heiltrank steht in der Liste') do
  Game.menu_item_list.include?('heiltrank')
end
check('Liste zeigt den Bestand') do
  Game.menu_screen_content[1].any? { |z| z.include?('Heiltrank') && z.include?("x#{Party.item_count('heiltrank')}") }
end
druecke(:enter)
check('Enter öffnet die Zielwahl') { screen == :item_target && menu_hero.zero? }
druecke(:right)
check('Rechts wechselt auf Mira') do
  Party.actors[menu_hero].name == 'Mira'
end
druecke(:left)
vorher = Party.item_count('heiltrank')
druecke(:enter)
check('Bestätigen heilt den Helden und verbraucht den Trank') do
  Party.find('arik').hp > 50 && Party.item_count('heiltrank') == vorher - 1
end
check('Rückmeldung nennt Held und Werte') do
  menu_msg && menu_msg.include?('Arik') && menu_msg.include?('HP')
end
check('zurück in der Itemliste') { screen == :item }

check('wirkungsloses Item verbraucht nichts') do
  Party.heal_all
  druecke(:enter)                     # Zielwahl
  bestand = Party.item_count('heiltrank')
  druecke(:enter)                     # Arik ist voll -> keine Wirkung
  Party.item_count('heiltrank') == bestand && menu_msg && menu_msg.include?('keine Wirkung')
end

puts '== Esc geht Ebene für Ebene zurück =='
check('Zielwahl -> Liste') { druecke(:escape) ? screen == :item : screen == :item }
check('Liste -> Hauptmenü') { druecke(:escape) && screen.nil? }
check('Menü bleibt offen') { menu_offen? }
druecke(:escape)
check('Esc im Hauptmenü schließt es') do
  !menu_offen? && !Rune::Simulation.paused && Rune::Input.captured?
end

puts '== Fertigkeiten ansehen =='
prepare
druecke(:escape)
druecke(:down)
check('Cursor auf Fertigkeit') { menu_sel == 1 }
druecke(:enter)
check('Fertigkeit öffnet die Heldenliste') { screen == :skill }
druecke(:enter)
check('Enter zeigt die Fertigkeiten des Helden') do
  screen == :skill_list && Game.menu_skill_lines(menu_hero).any? { |z| z.include?('Kraftschlag') }
end
check('Zeile nennt die MP-Kosten') do
  Game.menu_skill_lines(menu_hero).any? { |z| z.include?('5 MP') }
end
check('Beschreibung ist lesbar') { Game.menu_skill_desc(menu_hero, 0).to_s.include?('Hieb') }
druecke(:escape)
check('Esc zurück zur Heldenliste') { screen == :skill }
druecke(:escape)
check('Esc zurück ins Hauptmenü') { screen.nil? && menu_offen? }

puts '== Status ansehen =='
druecke(:down)
check('Cursor auf Status') { menu_sel == 2 }
druecke(:enter)
check('Status öffnet die Heldenliste') { screen == :status }
druecke(:enter)
zeilen = Game.menu_status_lines(menu_hero)
check('Statusseite zeigt alle Werte') do
  werte = %w[HP MP Angriff Verteidigung Weisheit Beweglichkeit EXP Gold]
  screen == :status_detail && werte.all? { |wort| zeilen.any? { |z| z.include?(wort) } }
end
check('Stufe und Name stehen oben') do
  zeilen.first.include?('Arik') && zeilen.first.include?('Lv')
end
druecke(:escape)
check('Esc zurück zur Heldenliste') { screen == :status }
druecke(:escape)

puts '== Zeichnen der Unterfenster =='
check('jedes Unterfenster zeichnet ohne Fehler') do
  ok = true
  %i[item item_target skill skill_list status status_detail].each do |s|
    Game.instance_variable_set(:@menu_screen, s)
    Game.instance_variable_set(:@menu_sub, 0)
    Game.instance_variable_set(:@menu_hero, 0)
    begin
      Game.draw
    rescue StandardError => e
      puts "    Zeichnen #{s}: #{e.class}: #{e.message}"
      ok = false
    end
  end
  Game.instance_variable_set(:@menu_screen, nil)
  ok
end

puts '== Parteigesichter =='
check('Gesichtsfarbe ist stabil und kommt aus dem Namen') do
  UI::GESICHTER.include?(UI.gesicht_farbe('Arik')) &&
    UI.gesicht_farbe('Arik') == UI.gesicht_farbe('Arik')
end
check('verschiedene Namen bekommen verschiedene Farben') do
  UI.gesicht_farbe('Arik') != UI.gesicht_farbe('Mira')
end
check('Initialen aus dem Namen') do
  UI.initialen('Arik') == 'A' && UI.initialen('Alte Wache') == 'AW' && UI.initialen('') == '?'
end
check('Datenbankfarbe gilt nur, wenn sie eine Zahl ist') do
  Party.face_color('arik').nil? || Party.face_color('arik').is_a?(Integer)
end
check('Party-Panel zeichnet ein Gesicht je Held') do
  Rune::Gfx.reset_calls
  Game.instance_variable_set(:@menu_screen, nil)
  Game.draw
  farben = Party.actors.map { |a| UI.gesicht_farbe(a.name) }
  Rune::Gfx.calls.count { |c| c[0] == :rect && farben.include?(c[5]) } >= Party.actors.size
end
check('Statusseite zeigt das Gesicht des Helden') do
  Rune::Gfx.reset_calls
  Game.instance_variable_set(:@menu_screen, :status_detail)
  Game.instance_variable_set(:@menu_hero, 0)
  Game.draw
  farbe = Party.face_color(Party.actors[0]) || UI.gesicht_farbe(Party.actors[0].name)
  Rune::Gfx.calls.any? { |c| c[0] == :rect && c[3] == 64 && c[4] == 64 && c[5] == farbe }
end
check('Gesicht zeigt die Initialen als Text') do
  Rune::Gfx.calls.any? { |c| c[0] == :text && c[3].to_s == UI.initialen(Party.actors[0].name) }
end
Game.instance_variable_set(:@menu_screen, nil)

puts '== Zahlenkürzel und Titel =='
Game.instance_variable_set(:@menu_sel, 0)
druecke('1')
check('Ziffer 1 öffnet Gegenstände') { screen == :item }
druecke(:escape, nil, :escape)
check('Menü geschlossen') { !menu_offen? }
check('Simulation läuft wieder') { !Rune::Simulation.paused }

puts
if $fail.zero?
  puts 'MENÜ-TEST OK (0 Fehler)'
  exit 0
else
  puts "MENÜ-TEST FEHLGESCHLAGEN (#{$fail} Fehler)"
  exit 1
end
