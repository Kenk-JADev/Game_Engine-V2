#encoding: utf-8
# ============================================================================
#  Kampfzustand und gezielte Items: echte Battle-/Party-Logik mit nativen
#  Attrappen. Kein D3D-, Fenster- oder Hardwaretest.
#     ruby -I game/engine tools/battle_state_test.rb
# ============================================================================

module Rune
  module Log
    def self.info(_message); end
    def self.warn(_message); end
  end
  # Dieser Unit-Test speist einzelne Tastendruck-Flanken direkt ein. Halten
  # und Loslassen über native Frames prüft zusätzlich encounter_test.rb.
  module Input
    @pressed_edges = []
    class << self
      attr_accessor :pressed_edges
    end
    def self.pressed?(key) = @pressed_edges.include?(key)
  end
  module Sound
    def self.play(*_args); end
  end
  module Player
    def self.position = [0.0, 0.0, 0.0]
  end
  module Window
    def self.width = 1280
    def self.height = 720
  end
  module Gfx
    def self.box(*_args); end
  end
end

module UI
  TEXT = 0xffffff
  TEXT_DIM = 0x999999
  ACCENT = 0x00ffff
  PANEL = 0x111111
  DANGER = 0xff0000
  @texts = []
  class << self
    attr_reader :texts
  end
  def self.panel(*_args); end
  def self.text(_x, _y, value, *_args) = @texts << value.to_s
  def self.abdunkeln(*_args); end   # battle.rb ruft UI.abdunkeln (ehem. Launcher)
end

module Game
  def self.gy(*_args) = 0.0
end
module Issues
  def self.report(*_args); end
end
module Database
  DB = {
    'gold' => 10,
    'actors' => [
      { 'id' => 'a', 'name' => 'Anna', 'hp' => 100, 'mp' => 10, 'atk' => 10, 'agi' => 4 },
      { 'id' => 'b', 'name' => 'Bela', 'hp' => 80, 'mp' => 20, 'atk' => 12, 'agi' => 10 }
    ],
    'items' => [
      { 'id' => 'heal', 'name' => 'Heiltrank', 'kind' => 'heal', 'power' => 40, 'start' => 1 },
      { 'id' => 'mana', 'name' => 'Manatrank', 'kind' => 'mana', 'power' => 12, 'start' => 1 },
      { 'id' => 'bad', 'name' => 'Leer', 'kind' => 'heal', 'power' => -5, 'start' => 1 }
    ],
    'enemies' => [
      { 'id' => 'one', 'name' => 'Eins', 'hp' => 25, 'atk' => 1, 'def' => 0, 'agi' => 1 },
      { 'id' => 'two', 'name' => 'Zwei', 'hp' => 100, 'atk' => 1, 'def' => 0, 'agi' => 1 }
    ],
    'troops' => [{ 'id' => 'pair', 'name' => 'Paar', 'enemies' => %w[one two] }]
  }.freeze
  def self.database = DB
end

class TestBattleTemp
  attr_accessor :in_battle, :gameover
  def initialize
    @in_battle = false
    @gameover = false
  end
end
$game_temp = TestBattleTemp.new

$LOAD_PATH.unshift(File.expand_path('../game/engine', __dir__))
require 'party'
require 'battle'

$fail = 0
def check(name)
  ok = yield
  puts(ok ? "  ok   #{name}" : "  FAIL #{name}")
  $fail += 1 unless ok
rescue StandardError, ScriptError => e
  puts "  FAIL #{name} (#{e.class}: #{e.message})"
  $fail += 1
end

def reset_party
  Battle.abort! if Battle.active?
  Party.setup(Database.database)
  $game_temp = TestBattleTemp.new
  Rune::Input.pressed_edges = []
  UI.texts.clear
end

def press(*keys)
  Rune::Input.pressed_edges = keys
  Battle.update(0.016)
ensure
  Rune::Input.pressed_edges = []
end

# Seit Etappe 20a steht "Fertigkeit" zwischen Angriff und Item - der Cursor
# läuft deshalb bis zum Eintrag "Item", statt eine feste Zahl zu drücken.
def item_befehl
  Battle::COMMANDS.size.times do
    break if Battle::COMMANDS[Battle.command_index] == 'Item'
    press(:down)
  end
end

def choose_item(index = 0)
  item_befehl
  press(:enter)
  index.times { press(:down) }
  press(:enter)
end

# Keiner der folgenden Tests hängt von einem zufälligen Krit-/Schadenswurf ab.
Battle.singleton_class.send(:define_method, :rand) { 0.9 }

puts '== Startwerte und Kampfzustand =='
check('Kampf übernimmt aktuelle HP/MP und die echten Maximalwerte') do
  reset_party
  a, b = Party.actors
  a.hp, a.mp, b.hp, b.mp = 37, 2, 0, 9
  started = Battle.start('pair')
  first, second = Battle.actors
  started && first.hp == 37 && first.max_hp == 100 && first.mp == 2 && first.max_mp == 10 &&
    second.hp.zero? && second.max_hp == 80 && second.mp == 9 && second.max_mp == 20 &&
    Battle.living_actors.map(&:id) == ['a'] && Battle.current_actor.id == 'a'
end
check('laufender Kampf wird durch einen zweiten Start nicht ersetzt') do
  before_enemies, before_actors = Battle.enemies, Battle.actors
  !Battle.start('pair') && Battle.active? && Battle.enemies.equal?(before_enemies) &&
    Battle.actors.equal?(before_actors) && $game_temp.in_battle
end
check('externe Party-Änderungen und Kampfwerte sind nicht zwei widersprüchliche Kopien') do
  Party.find('a').hp = 45
  Battle.actors.first.hp == 45 && (Battle.actors.first.hp = 33) == 33 && Party.find('a').hp == 33
end
check('Abbruch erhält die bereits wirksamen HP und MP') do
  Battle.actors.first.mp = 1
  Battle.abort!
  !Battle.active? && !$game_temp.in_battle && Party.find('a').hp == 33 && Party.find('a').mp == 1
end
check('alle kampfunfähigen Helden bleiben bei 0 und verlieren ohne falsche Wiederbelebung') do
  reset_party
  Party.actors.each { |actor| actor.hp = 0 }
  started = Battle.start('pair', can_lose: true)
  Battle.update(0.016)
  started && !Battle.active? && Battle.result == :niederlage && !$game_temp.gameover &&
    Party.actors.all? { |actor| actor.hp.zero? }
end

puts '== Items per Party-API =='
check('gezielte Heilung wirkt nur auf die angegebene ID') do
  reset_party
  Party.find('a').hp = 20
  Party.find('b').hp = 10
  Party.use_item('heal', target: 'b') && Party.find('a').hp == 20 &&
    Party.find('b').hp == 50 && Party.item_count('heal').zero?
end
check('voller, unbekannter oder fremder Zielheld verbraucht kein gezieltes Item') do
  reset_party
  fremd = Party.actors.first.dup
  vor = Party.item_count('heal')
  !Party.use_item('heal', target: 'a') && !Party.use_item('heal', target: 'fehlt') &&
    !Party.use_item('heal', target: fremd) && Party.item_count('heal') == vor
end
check('ungültige negative Heilstärke ist kein Schaden und verbraucht kein Item') do
  reset_party
  Party.find('a').hp = 10
  !Party.use_item('bad', target: 'a') && Party.find('a').hp == 10 && Party.item_count('bad') == 1
end
check('alte Aufrufe ohne Ziel heilen weiterhin den ersten verletzten Helden') do
  reset_party
  Party.find('b').hp = 5
  Party.use_item('heal') && Party.find('a').hp == 100 && Party.find('b').hp == 45
end
check('alter Aufruf ohne Ziel behält den Leader-Fallback bei voller Party') do
  reset_party
  Party.use_item('heal') && Party.find('a').hp == 100 && Party.find('b').hp == 80 &&
    Party.item_count('heal').zero?
end
check('aktuelle Actor-Referenz ist ein gültiges Mana-Ziel') do
  reset_party
  hero = Party.find('b')
  hero.mp = 19
  Party.use_item('mana', target: hero) && hero.mp == hero.max_mp && Party.item_count('mana').zero?
end

puts '== Item-Auswahl, Abbruch und Ziel =='
check('Itemziel kann abgebrochen werden ohne Aktion oder Verbrauch') do
  reset_party
  Party.find('a').hp = 60
  Battle.start('pair')
  choose_item
  entered = Battle.phase == :item_target && Battle.actions.empty?
  press(:escape)
  back_to_items = Battle.phase == :item && Battle.actions.empty? && Party.item_count('heal') == 1
  press(:escape)
  entered && back_to_items && Battle.phase == :command && Party.item_count('heal') == 1
end
check('ausgewähltes vollständiges Ziel bleibt ohne Wirkung und Auswahl') do
  reset_party
  Battle.start('pair')
  choose_item
  press(:enter)
  Battle.phase == :item_target && Battle.actions.empty? && Party.item_count('heal') == 1
end
check('Kampf-Overlay zeigt ausgewählten Helden als Itemziel') do
  reset_party
  Party.find('b').hp = 1
  Battle.start('pair')
  choose_item
  press(:right)
  UI.texts.clear
  Battle.draw
  Battle.phase == :item_target && UI.texts.any? { |text| text.include?('Bela') && text.include?('Ziel') }
end
check('gezielter Heiltrank belebt KO-Ziel und bleibt nach Kampf und Save-Roundtrip') do
  reset_party
  Party.find('a').hp = 60
  Party.find('b').hp = 0
  Battle.start('pair')
  choose_item
  press(:right)
  press(:enter)
  queued = Battle.phase == :execute && Battle.actions.one?
  # Zwei Gegner verursachen jeweils genau einen Schaden; die Zielwahl des
  # Gegnerzugs ist zufällig, ihre Summe aber deterministisch.
  Battle.enemies.each { |enemy| enemy.instance_variable_set(:@atk, 0) }
  Battle.update(0.016)
  after_turn = Battle.turn == 1 && Party.find('b').hp.between?(38, 40) &&
               Party.find('a').hp + Party.find('b').hp == 98
  Battle.enemies.each { |enemy| enemy.hp = 0 }
  Battle.finish_round
  Battle.update(0.016)
  saved = Party.snapshot
  Party.setup(Database.database)
  Party.restore!(saved)
  queued && after_turn && Battle.result == :sieg && Party.find('b').hp.between?(38, 40) &&
    Party.find('a').hp + Party.find('b').hp == 98 && Party.item_count('heal').zero?
end
check('Manatrank erhöht die MP des gewählten Helden und synchronisiert sie') do
  reset_party
  Party.find('b').mp = 2
  Battle.start('pair')
  choose_item(1)
  press(:right)
  press(:enter)
  press(:enter) # Bela wählt Angriff
  press(:enter) # Ziel
  Battle.enemies.each { |enemy| enemy.instance_variable_set(:@atk, 0) }
  Battle.update(0.016)
  after_turn = Battle.turn == 1 && Party.find('b').mp == 14 && Party.item_count('mana').zero?
  Battle.enemies.each { |enemy| enemy.hp = 0 }
  Battle.finish_round
  Battle.update(0.016)
  after_turn && Battle.result == :sieg && Party.find('b').mp == 14 && Party.find('a').mp == 10
end
check('ein bereits verplantes Einzelitem kann nicht doppelt gewählt werden') do
  reset_party
  Party.find('a').hp = 40
  Party.find('b').hp = 40
  Party.take_item('mana', 1)
  Party.take_item('bad', 1)
  Battle.start('pair')
  choose_item
  press(:enter)
  remaining = Battle.usable_items
  press(:down)
  press(:enter)
  remaining.empty? && Battle.phase == :command && Battle.actions.one? && Party.item_count('heal') == 1
end
check('nach Auswahl und Kampf-Abbruch bleibt ein nicht ausgeführtes Item erhalten') do
  Battle.abort!
  Party.item_count('heal') == 1
end
check('Itemzahl berücksichtigt Reservierungen auch bei symbolischer ID') do
  reset_party
  Party.find('a').hp = 40
  Battle.start('pair')
  choose_item
  press(:enter)
  Battle.available_item_count('heal').zero? && Battle.available_item_count(:heal).zero? &&
    Party.item_count('heal') == 1
end
check('zwei Heilungen auf dasselbe Ziel verbrauchen nur bei Wirkung') do
  reset_party
  Party.find('a').hp = 60
  Party.add_item('heal', 1)
  Battle.start('pair')
  choose_item
  press(:enter) # Anna zielt auf Anna
  choose_item
  press(:enter) # Bela zielt auf Anna
  selected = Battle.phase == :execute && Battle.actions.size == 2 && Party.item_count('heal') == 2
  Battle.enemies.each { |enemy| enemy.instance_variable_set(:@atk, 0) }
  Battle.update(0.016)
  selected && Party.item_count('heal') == 1 && Party.find('a').hp.between?(98, 100) &&
    Battle.instance_variable_get(:@log).any? { |line| line.include?('konnte nicht benutzt') }
end
check('zwischen Auswahl und Ausführung entnommenes Item heilt nicht') do
  reset_party
  Party.find('a').hp = 50
  Battle.start('pair')
  choose_item
  press(:enter)
  press(:enter) # Bela greift an
  press(:enter)
  selected = Battle.phase == :execute && Battle.actions.size == 2
  Party.take_item('heal', 1)
  Battle.enemies.each { |enemy| enemy.instance_variable_set(:@atk, 0) }
  Battle.update(0.016)
  selected && Party.find('a').hp <= 50 && Party.item_count('heal').zero? &&
    Battle.instance_variable_get(:@log).any? { |line| line.include?('konnte nicht benutzt') }
end
check('Sieg durch schnelleren Angriff verbraucht späteres Item nicht') do
  reset_party
  Party.find('a').hp = 60
  Battle.start('pair')
  Battle.enemies.first.hp = 1
  Battle.enemies.last.hp = 0
  choose_item
  press(:enter) # Anna plant Heilung
  press(:enter) # Bela greift den letzten Gegner an
  press(:enter)
  selected = Battle.phase == :execute && Battle.actions.size == 2
  Battle.update(0.016)
  selected && Battle.result == :sieg && Party.find('a').hp == 60 &&
    Party.item_count('heal') == 1 && Battle.actions.empty?
end

puts '== Mehrere Aktionen und Ziele =='
check('zweiter Angriff wechselt auf einen lebenden Gegner, falls das gewählte Ziel bereits fiel') do
  reset_party
  Battle.start('pair')
  Battle.enemies.first.hp = 1
  press(:enter)
  press(:enter)
  press(:enter)
  press(:enter)
  target = Battle.enemies.last
  Battle.update(0.016)
  Battle.enemies.first.dead? && target.hp < target.max_hp
end
check('anfänglich kampfunfähiger erster Held gibt dem zweiten keinen Doppelzug') do
  reset_party
  Party.find('a').hp = 0
  Battle.start('pair')
  started = Battle.current_actor&.id == 'b'
  press(:enter)
  press(:enter)
  started && Battle.phase == :execute && Battle.actions.map { |action| action[:actor].id } == ['b']
end
check('KO eines bereits gewählten Helden überspringt nicht den nächsten') do
  reset_party
  Battle.start('pair')
  press(:enter)
  press(:enter) # Anna hat ihre Aktion bestätigt
  was_bela = Battle.current_actor&.id == 'b'
  Party.find('a').hp = 0
  press(:enter)
  in_target = Battle.phase == :target && Battle.current_actor&.id == 'b'
  press(:enter)
  was_bela && in_target && Battle.phase == :execute &&
    Battle.actions.map { |action| action[:actor].id } == ['b']
end
check('KO während Zielwahl verwirft nur die unbestätigte Aktion') do
  reset_party
  Battle.start('pair')
  press(:enter) # Anna wählt Angriff, Ziel noch offen
  Party.find('a').hp = 0
  press(:enter) # darf nicht den nun aktiven Bela als Annas Ziel bestätigen
  cancelled = Battle.phase == :command && Battle.current_actor&.id == 'b' && Battle.actions.empty?
  press(:enter)
  press(:enter)
  cancelled && Battle.phase == :execute && Battle.actions.map { |action| action[:actor].id } == ['b']
end
check('KO während Itemzielwahl überträgt die Auswahl nicht auf den nächsten Helden') do
  reset_party
  Party.find('a').hp = 60
  Battle.start('pair')
  choose_item
  Party.find('a').hp = 0
  press(:enter)
  Battle.phase == :command && Battle.current_actor&.id == 'b' &&
    Battle.actions.empty? && Party.item_count('heal') == 1
end
check('KO eines Helden gibt seine vorgemerkte Itemmenge für den nächsten frei') do
  reset_party
  Party.find('a').hp = 60
  Party.find('b').hp = 40
  Party.take_item('mana', 1)
  Battle.start('pair')
  choose_item
  press(:enter) # Anna reserviert den einzigen Heiltrank
  reserved = Battle.actions.one? && Battle.current_actor&.id == 'b'
  Party.find('a').hp = 0
  item_befehl
  press(:enter)
  in_items = Battle.phase == :item
  press(:enter)
  press(:right)
  press(:enter)
  reserved && in_items && Battle.phase == :execute &&
    Battle.actions.map { |action| action[:actor].id } == ['b'] && Party.item_count('heal') == 1
end
check('alle extern besiegten Gegner beenden auch eine offene Zielwahl') do
  reset_party
  Battle.start('pair')
  press(:enter)
  Battle.enemies.each { |enemy| enemy.hp = 0 }
  Battle.update(0.016)
  !Battle.active? && Battle.result == :sieg && Battle.actions.empty? && !$game_temp.in_battle
end
check('alle extern kampfunfähigen Helden beenden auch die Itemzielwahl') do
  reset_party
  Party.find('a').hp = 60
  Battle.start('pair', can_lose: true)
  choose_item
  Party.actors.each { |actor| actor.hp = 0 }
  Battle.update(0.016)
  !Battle.active? && Battle.result == :niederlage && Battle.actions.empty? &&
    Party.item_count('heal') == 1 && Party.actors.all? { |actor| actor.hp.zero? }
end

Battle.abort! if Battle.active?
Battle.singleton_class.send(:remove_method, :rand)
puts($fail.zero? ? 'BATTLE-STATE-TEST OK (0 Fehler)' : "BATTLE-STATE-TEST FEHLGESCHLAGEN (#{$fail} Fehler)")
exit($fail.zero? ? 0 : 1)
