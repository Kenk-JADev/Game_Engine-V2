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
    def self.info(_message); end
    def self.warn(_message); end
    def self.error(_message); end
  end

  module World
    def self.ready? = false
    def self.generate(**_kwargs); end
  end

  module Gfx
    def self.sky(*_args); end
    def self.fog(*_args); end
    def self.terrain_texture(*_args); end
    def self.terrain_splats(*_args); end
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
end

module Project
  class << self
    attr_accessor :save_data
  end
  def self.load_save(_name, _slot) = @save_data
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
  attr_reader :updates
  def initialize(_name) = (@updates = 0)
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
require 'event'
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

puts($fail.zero? ? 'ENCOUNTER-TEST OK (0 Fehler)' : "ENCOUNTER-TEST FEHLGESCHLAGEN (#{$fail} Fehler)")
exit($fail.zero? ? 0 : 1)
