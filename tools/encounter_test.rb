#encoding: utf-8
# ============================================================================
#  Integrationstest für Zufallskämpfe (Etappe 19l).
#  ruby -I game/engine tools/encounter_test.rb
#
#  Lädt game/main.rb mit einer Engine-Attrappe, aber echten RGSS-, Event-,
#  Interpreter-, Party- und Battle-Modulen und der echten Demo-Datenbank.
#  So werden Spielstart, Kartenwechsel und tatsächliche Schritt-Frames geprüft,
#  nicht nur die einzelnen Zählermethoden.
# ============================================================================

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
    def self.fog(*args) = (@calls << [:fog, *args])
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
  end

  module Window
    class << self
      attr_accessor :title
    end
  end

  module Input
    @keys = []
    class << self
      attr_accessor :keys, :capture
    end
    def self.pressed?(key) = @keys.include?(key)
    def self.down?(key) = @keys.include?(key)
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
end

module Project
  class << self
    attr_accessor :save_data
  end
  def self.load_save(_name, _slot) = @save_data
  def self.save(_name, _slot, data) = (@save_data = data; true)
end

module Weather
  def self.load_map(_map); end
  def self.update(*_args); end
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
end

class Editor
  def initialize(*_args) = (@active = false)
  def self.rebuild(*_args); end
  def active? = @active
  def toggle(_scene) = (@active = !@active)
  def update(*_args); end
end

$LOAD_PATH.unshift(File.join(Rune::ROOT, 'game', 'engine'))
require 'data'
require 'state'
require 'party'
require 'battle'
require 'node'
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
  def self.system = { 'start' => { 'map' => 'start' } }
end

# Die Render-/Picking-Schicht wird nicht für die Spiellogik benötigt.
module Graphics
  class << self
    attr_accessor :callback
  end
  def self.scene_update = @callback&.call
  def self.scene_reset!; end
end

# main.rb ruft beim Laden Game.setup auf: Genau diesen Kaltstart mit nil-
# Globalen wollen wir prüfen, nicht ein nachträgliches RGSS.setup im Test.
$project_root = File.join(Rune::ROOT, 'game', 'data')
load File.join(Rune::ROOT, 'game', 'main.rb')
module Game
  def self.update_picking; end
  def self.update_hover_only; end
end

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

def prepare
  Battle.abort!
  Game.setup
  Game.instance_variable_set(:@intro, nil)
  Rune::Input.keys = []
  Rune::Player.ground = true
  # Erster Frame nimmt die Startposition als Referenz auf, ohne Schritt.
  Game.update(0.016)
end

def step(distance = 0.7)
  x, y, z = Rune::Player.position
  Rune::Player.pos = [x + distance, y, z]
  Game.update(0.016)
end

puts '== Karten-Terrain und Atmosphäre: Ruby-Brücke =='
Rune::World.ready = true
check('Karten-Pinsel und Splats gehen unverändert an die native API') do
  Rune::World.result = true
  Rune::Gfx.reset_calls
  pinsel = [{ 'x' => 1, 'z' => 2, 'r' => 3, 'h' => 4 }]
  splats = [{ 'tex' => 'weg', 'x' => 0, 'z' => 0, 'w' => 4, 'h' => 4 }]
  map = { 'terrain' => { 'size' => 100, 'width' => 120, 'heights' => pinsel, 'splats' => splats } }
  original = Marshal.dump(map)
  Game.build_terrain(map) && Rune::World.last_args[:height_brushes].equal?(pinsel) &&
    Rune::World.last_args[:width] == 120 && Rune::World.last_args[:height] == 100 &&
    Rune::Gfx.calls.map(&:first) == %i[texture splats] && Rune::Gfx.calls[1][2].equal?(splats) &&
    Marshal.dump(map) == original
end
check('extreme und falsche Grundwerte werden nicht heimlich normalisiert') do
  Rune::Gfx.reset_calls
  riesig = 10**400
  map = { 'terrain' => { 'width' => false, 'height' => riesig, 'resolution' => riesig,
                         'seed' => false, 'amplitude' => 1e300, 'heights' => false,
                         'texture' => false, 'tile' => false } }
  Game.build_terrain(map)
  args = Rune::World.last_args
  args[:width] == false && args[:height] == riesig && args[:resolution] == riesig &&
    args[:seed] == false && args[:height_brushes] == false &&
    Rune::Gfx.calls[0][1] == false && Rune::Gfx.calls[0][2] == false
end
check('gescheiterte native Erzeugung wendet keine alte Textur an') do
  Rune::Gfx.reset_calls
  Rune::World.result = false
  result = Game.build_terrain('terrain' => { 'width' => 1e300 })
  Rune::World.result = true
  result == false && Rune::Gfx.calls.empty?
end
check('nichtobjektartiges Terrain verwirft statt benutzt vorheriges Gelände') do
  Rune::Gfx.reset_calls
  result = Game.build_terrain('terrain' => false)
  result == false && Rune::World.last_args[:width] == 0 && Rune::Gfx.calls.empty?
end
check('falsche Splat-Liste erreicht native Schutzprüfung') do
  Rune::Gfx.reset_calls
  result = Game.build_terrain('terrain' => { 'splats' => false })
  ok = result == false && Rune::Gfx.calls.map(&:first) == %i[texture splats] && Rune::Gfx.calls[1][2] == false
  Rune::World.ready = true
  ok
end
check('Atmosphärenwerte werden zur nativen Prüfung weitergereicht') do
  Rune::Gfx.reset_calls
  Rune::Log.warnings.clear
  Game.apply_map_atmosphere('panorama' => { 'color' => false },
                            'fog' => { 'on' => 'ja', 'color' => 10**400, 'far' => 1e300 })
  Rune::Gfx.calls[0] == [:sky, false, false] && Rune::Gfx.calls[1] == [:fog, false, 10**400, 1e300] &&
    Rune::Log.warnings.any? { |message| message.include?('fog.on') }
end
Rune::World.ready = false
puts '== Zufallskampf: Start und Kartenwechsel =='
check('Kaltstart würfelt den Zähler der Startkarte') do
  $game_player.encounter_count == 1 && $game_map.encounter_step == 1
end
check('Neustart ersetzt RGSS-Objekte und würfelt erneut') do
  vorher = $game_player
  Game.setup
  !$game_player.equal?(vorher) && $game_player.encounter_count == 1
end
check('Kartenwechsel würfelt neu und Teleport zählt nicht als Schritt') do
  prepare
  vorher = $game_player.steps
  Game.enter_map('weit')
  Game.instance_variable_set(:@intro, nil)
  Game.update(0.016)
  $game_player.encounter_count == 1 && $game_player.steps == vorher && !Battle.active?
end
check('Spielstand lädt mit neuem Zähler ohne Teleport-Schritt') do
  prepare
  vorher = $game_player.steps
  Project.save_data = { 'map' => 'weit', 'state' => GameState.snapshot,
                        'party' => Party.snapshot, 'time' => 5,
                        'player' => { 'x' => -40, 'y' => 0, 'z' => 50 } }
  geladen = Game.load_game(1)
  Game.update(0.016)
  geladen && Game.map_id == 'weit' && $game_player.encounter_count == 1 &&
    $game_player.steps == vorher && !Battle.active?
end

check('Teleport-Event zählt nicht als gelaufener Schritt') do
  prepare
  vorher = $game_player.steps
  counter(1)
  Game.interpreter.start([{ 'cmd' => 'teleport', 'x' => 50, 'z' => 50 }])
  Game.update(0.016)
  Rune::Player.position[0] == 50.0 && $game_player.steps == vorher &&
    $game_player.encounter_count == 1 && !Battle.active?
end
check('move_player-Event zählt nicht als gelaufener Schritt') do
  prepare
  vorher = $game_player.steps
  counter(1)
  Game.interpreter.start([{ 'cmd' => 'move_player', 'relative' => true, 'x' => 50, 'z' => 0 }])
  Game.update(0.016)
  Rune::Player.position[0] == 50.0 && $game_player.steps == vorher &&
    $game_player.encounter_count == 1 && !Battle.active?
end
check('RGSS-moveto und Script-teleport zählen nicht als Schritte') do
  prepare
  vorher = $game_player.steps
  counter(1)
  $game_player.moveto(50, 0)
  Game.update(0.016)
  rgss_ok = !Battle.active? && $game_player.steps == vorher && $game_player.encounter_count == 1
  Scripts::Context.new.teleport(100, 0)
  Game.update(0.016)
  rgss_ok && !Battle.active? && $game_player.steps == vorher && $game_player.encounter_count == 1
end
check('Script-Teleport verwirft den im selben Frame vorgemerkten Encounter') do
  prepare
  counter(1)
  Graphics.callback = -> { Scripts::Context.new.teleport(100, 0) }
  step
  !Battle.active? && $game_player.encounter_count == 1
ensure
  Graphics.callback = nil
end

puts '== Platzierungen: echte Spielschleife =='
check('NPC ist per E ansprechbar und zeigt seinen Text') do
  prepare
  scene = Game.instance_variable_get(:@scene)
  npc = Npc.new({ 'name' => 'Neu', 'x' => 1, 'y' => 2, 'z' => 0, 'path' => [],
                  'commands' => [{ 'cmd' => 'text', 'lines' => ['Hallo vom NPC'] }] }, scene)
  Game.npcs << npc
  Rune::Input.keys = [:e]
  Game.update(0.016)
  gestartet = Game.interpreter.running?
  Rune::Input.keys = []
  Game.update(0.016)
  gestartet && Game.interpreter.message == ['Hallo vom NPC']
ensure
  Rune::Input.keys = []
end
check('Schild aus Kartendaten wird dargestellt und per E gelesen') do
  prepare
  karte = Database.map('start')
  karte['events'] << { 'id' => 8, 'name' => 'Schild', 'x' => 0.5, 'y' => 2, 'z' => 0,
                       'visual' => 'schild', 'commands' => [{ 'cmd' => 'text', 'lines' => ['Weg'] }] }
  Game.enter_map('start')
  Game.instance_variable_set(:@intro, nil)
  scene = Game.instance_variable_get(:@scene)
  sichtbar = scene.root.children.count { |n| n.name.start_with?('event_visual_') } == 2
  Rune::Input.keys = [:e]
  Game.update(0.016)
  Rune::Input.keys = []
  Game.update(0.016)
  sichtbar && Game.interpreter.message == ['Weg']
ensure
  Rune::Input.keys = []
  karte['events'].clear if karte
end
check('Schild und NPC sind als Event statt als verdeckender Körper pickbar') do
  prepare
  scene = Game.instance_variable_get(:@scene)
  npc = Npc.new({ 'name' => 'Pick-NPC', 'x' => 1, 'z' => 0 }, scene)
  Game.npcs << npc
  schild = Event.new('name' => 'Schild', 'x' => 4, 'z' => 0, 'visual' => 'schild',
                     'commands' => [{ 'cmd' => 'text', 'lines' => ['Weg'] }])
  Game.events << schild
  # Ein Script darf denselben Namenspräfix verwenden; dessen Node gehört ihm.
  fremd = Node.new(name: 'event_visual_eigenes_script')
  scene.add(fremd)
  Game.refresh_placement_visuals(scene)
  Game.refresh_placement_visuals(scene)
  kandidaten = Game.pickables
  schilder = scene.root.children.select { |n| n != fremd && n.name.start_with?('event_visual_') }
  schilder.size == 2 && scene.root.children.include?(fremd) && kandidaten.include?(fremd) &&
    kandidaten.include?(npc.event) && kandidaten.include?(schild) &&
    !kandidaten.include?(npc.node) && schilder.none? { |node| kandidaten.include?(node) }
end
check('einmaliges NPC-Auto-Event bleibt nach einem echten Save/Load ausgelöst') do
  prepare
  karte = Database.map('start')
  karte['npcs'] = [{ 'name' => 'Auto-NPC', 'x' => 2, 'z' => 0, 'trigger' => 'auto',
                     'commands' => [{ 'cmd' => 'wait', 'seconds' => 10 }] }]
  Game.enter_map('start')
  Game.instance_variable_set(:@intro, nil)
  Game.update(0.016)
  ausgelöst = Game.npcs.first.event.fired && Game.interpreter.running?
  gespeichert = Game.save_game(1) && Project.save_data['npcs'].first['fired'] == true
  geladen = Game.load_game(1)
  wiederhergestellt = Game.npcs.first.event.fired
  Game.update(0.016)
  ausgelöst && gespeichert && geladen && wiederhergestellt && !Game.interpreter.running?
ensure
  karte.delete('npcs') if karte
end
check('Touch-Event verdrängt kein NPC-Gespräch und hat im selben Frame Vorrang') do
  prepare
  scene = Game.instance_variable_get(:@scene)
  Game.npcs << Npc.new({ 'name' => 'Redner', 'x' => 1, 'z' => 0,
                         'commands' => [{ 'cmd' => 'text', 'lines' => ['NPC'] }] }, scene)
  Game.events << Event.new('name' => 'Kontakt', 'x' => 0, 'z' => 0, 'trigger' => 'touch',
                           'commands' => [{ 'cmd' => 'text', 'lines' => ['Kontakt'] }])
  Rune::Input.keys = [:e]
  Game.update(0.016)
  Rune::Input.keys = []
  Game.update(0.016)
  Game.interpreter.message == ['Kontakt']
ensure
  Rune::Input.keys = []
end

puts '== Zufallskampf: Schritt, Battle und Sperren =='
check('zwei Schritte starten die echte Truppe und würfeln danach neu') do
  prepare
  counter(2)
  step
  zwischenstand = $game_player.encounter_count == 1 && !Battle.active?
  scene = Game.instance_variable_get(:@scene)
  updates = scene.updates
  step
  zwischenstand && Battle.active? && $game_temp.in_battle && Battle.can_escape &&
    !Battle.can_lose && Battle.enemies.map(&:id) == %w[goblin wolf] &&
    $game_player.encounter_count == 1 && scene.updates == updates && Rune::Simulation.paused
end
check('die Physik pausiert im Kampf und läuft nach Sieg wieder an') do
  prepare
  counter(1)
  step
  pausiert = Battle.active? && Rune::Simulation.paused
  Battle.enemies.each { |g| g.hp = 0 }
  Battle.finish_round
  Game.update(0.016)
  pausiert && !Battle.active? && !Rune::Simulation.paused
end
check('ein Event-Kampf pausiert die Physik noch im Start-Frame') do
  prepare
  Game.interpreter.start([{ 'cmd' => 'battle', 'name' => 'waldweg' }])
  Game.update(0.016)
  Battle.active? && Rune::Simulation.paused
end
check('ohne Truppenliste startet kein Kampf') do
  prepare
  Game.map['encounters']['troops'] = []
  counter(1)
  step
  !Battle.active? && $game_player.encounter_count == 0
ensure
  Game.map['encounters']['troops'] = ['waldweg']
end
check('encounter_disabled sperrt den Kampf, Freigabe erlaubt nächsten Schritt') do
  prepare
  counter(1)
  $game_system.encounter_disabled = true
  step
  gesperrt = !Battle.active? && $game_player.encounter_count == 0
  $game_system.encounter_disabled = false
  step
  gesperrt && Battle.active?
end
check('laufender Interpreter verhindert Kampf bis zum nächsten freien Schritt') do
  prepare
  counter(1)
  Game.interpreter.start([{ 'cmd' => 'wait', 'seconds' => 10 }])
  step
  wartend = !Battle.active? && Game.interpreter.running? && $game_player.encounter_count == 0
  Game.interpreter.instance_variable_set(:@wait_left, 0.0)
  Game.update(0.016)
  step
  wartend && Battle.active?
end
check('Touch-Event desselben Schritts hat Vorrang vor Zufallskampf') do
  prepare
  Game.events << Event.new('name' => 'Auslöser', 'x' => 0.7, 'z' => 0,
                           'radius' => 0.2, 'trigger' => 'touch',
                           'commands' => [{ 'cmd' => 'wait', 'seconds' => 10 }])
  counter(1)
  step
  !Battle.active? && Game.interpreter.running? && $game_player.encounter_count == 0
end
check('im Editor keine Begegnung und kein Encounter-Countdown') do
  prepare
  counter(1)
  editor = Game.instance_variable_get(:@editor)
  editor.toggle(Game.instance_variable_get(:@scene))
  step
  im_editor = editor.active? && !Battle.active? && $game_player.encounter_count == 1
  editor.toggle(Game.instance_variable_get(:@scene))
  step
  im_editor && Battle.active?
end
check('F4 beim letzten Schritt öffnet den Editor statt eines Kampfes') do
  prepare
  counter(1)
  Rune::Input.keys = [:f4]
  step
  editor = Game.instance_variable_get(:@editor)
  editor.active? && !Battle.active? && $game_player.encounter_count == 1
ensure
  Rune::Input.keys = []
end
check('unbekannte Truppe behält Zähler 0; später gültige startet') do
  prepare
  Game.map['encounters']['troops'] = ['fehlend']
  counter(1)
  step
  fehlgeschlagen = !Battle.active? && $game_player.encounter_count == 0 &&
                   Issues.entries.any? { |entry| entry.join.include?('fehlend') }
  Game.map['encounters']['troops'] = ['waldweg']
  step
  fehlgeschlagen && Battle.active? && $game_player.encounter_count == 1
ensure
  Game.map['encounters']['troops'] = ['waldweg']
end
check('Bewegung in der Luft zählt nicht als Encounter-Schritt') do
  prepare
  counter(1)
  Rune::Player.ground = false
  step
  !Battle.active? && $game_player.encounter_count == 1
ensure
  Rune::Player.ground = true
end
check('Rennen nutzt die längere Schrittstrecke von 0,85 m') do
  prepare
  counter(1)
  Rune::Input.keys = [:shift]
  step(0.7)
  erst = !Battle.active? && $game_player.encounter_count == 1
  step(0.2)
  erst && Battle.active?
ensure
  Rune::Input.keys = []
end
check('schrittweise Spieler-Route zählt trotz interner Teleports') do
  prepare
  counter(1)
  Mover.start('spieler', ['move_right'], speed: 2.0)
  30.times do
    Game.update(0.05)
    break if Battle.active?
  end
  Battle.active? && Rune::Player.position[0] > 0.6
end
check('ungültige IDs werden übersprungen, eine gültige startet trotzdem') do
  prepare
  Game.map['encounters']['troops'] = ['', 42, nil, 'waldweg']
  counter(1)
  step
  Battle.active? && Battle.enemies.map(&:id) == %w[goblin wolf] && Issues.entries.empty?
ensure
  Game.map['encounters']['troops'] = ['waldweg']
end
check('alle Truppen der Demo-Vorlage lassen sich mit ihrer Datenbank starten') do
  prepare
  pfad = File.join(Rune::ROOT, 'game', 'data', 'maps', 'dorf.json')
  ids = JSON.parse(File.read(pfad, encoding: 'utf-8')).dig('encounters', 'troops')
  ids.is_a?(Array) && !ids.empty? && ids.all? do |id|
    gestartet = Battle.start(id)
    Battle.abort!
    gestartet
  end
end

puts '== Playtest-Projektname =='
check('Game.setup erhält Leerzeichen und Unicode im RUNE_PROJECT-Namen exakt') do
  original_env = ENV['RUNE_PROJECT']
  original_root = $project_root
  original_name = $project_name
  name = " Projekt_ä_日_🚀"
  ENV['RUNE_PROJECT'] = name
  # Unter POSIX-C-Locale kann ENV ASCII-8BIT liefern; Windows verwendet UTF-8.
  # Die echte Projektliste liefert dieselbe Dateinamenkodierung wie ENV.
  env_name = ENV['RUNE_PROJECT']
  Project.define_singleton_method(:list) { [env_name] }
  Project.define_singleton_method(:touch) { |value| @touched_name = value; true }
  Project.define_singleton_method(:data_path) { |_value| File.join(Rune::ROOT, 'game', 'data') }
  $project_root = nil
  $project_name = nil
  Game.setup
  $project_name.b == name.b && Project.instance_variable_get(:@touched_name).b == name.b &&
    $project_root == File.join(Rune::ROOT, 'game', 'data')
ensure
  ENV['RUNE_PROJECT'] = original_env
  $project_root = original_root
  $project_name = original_name
end

puts($fail.zero? ? 'ENCOUNTER-TEST OK (0 Fehler)' : "ENCOUNTER-TEST FEHLGESCHLAGEN (#{$fail} Fehler)")
exit($fail.zero? ? 0 : 1)
