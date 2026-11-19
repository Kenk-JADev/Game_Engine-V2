#encoding: utf-8
# ============================================================================
#  Kopfloser Test für das Kampf-Grundgerüst + Zahleneingabe (Etappe 19i).
#  Lässt sich ohne Engine starten:
#     ruby -I game/engine tools/battle_test.rb
#  Exit-Code 0 = alles grün.
#
#  Geprüft werden game/engine/battle.rb und die 19i-Befehle des Interpreters:
#    - Datenbank: enemies/troops aus der echten database.json
#    - Battle.start (Truppe/Helden/Absagen), active?, abort!
#    - Befehlsphase: Angriff -> Zielwahl -> Schaden, Item -> Heilung,
#      Flucht ohne can_escape
#    - Sieg (Belohnung/Party-HP/gameover bleibt aus), Niederlage
#      (gameover-Flag bzw. can_lose), Schadensformel
#    - Interpreter: battle blockiert bis Kampfende, input_number mit
#      Zifferntasten/Bestätigen/Abbruch
# ============================================================================

# --- Minimal-Attrappe der Engine-Schnittstelle -----------------------------
module Rune
  ROOT = File.expand_path('..', __dir__)

  module Log
    def self.info(m) = nil
    def self.warn(m) = puts("WARN  #{m}")
    def self.error(m) = puts("ERROR #{m}")
  end

  module Input
    @pressed = []
    @down = []
    class << self
      attr_accessor :pressed, :down
    end
    def self.pressed?(key) = @pressed.include?(key)
    def self.down?(key) = @down.include?(key)
  end

  module Sound
    CALLS = []
    def self.play(name, volume = 1.0) = (CALLS << [:play, name, volume])
    def self.loop(name, volume = 1.0) = (CALLS << [:loop, name, volume])
    def self.stop(name) = (CALLS << [:stop, name])
  end

  module Player
    @pos = [0.0, 0.0, 0.0]
    class << self
      attr_accessor :pos
    end
    def self.teleport(x, y, z) = (self.pos = [x.to_f, y.to_f, z.to_f])
    def self.position = pos
  end

  module Camera
    @yaw = 0.0
    class << self
      attr_accessor :yaw
    end
  end

  module Window
    def self.width = 1280
    def self.height = 720
  end

  module Gfx
    CALLS = []
    def self.rect(*a) = (CALLS << [:rect, *a])
    def self.text(*a) = (CALLS << [:text, *a])
    def self.box(*a) = (CALLS << [:box, *a])
  end
end

# UI-Attrappe für UI.nummer_eingabe (echtes Verhalten, Zählen der Zeichnung).
module UI
  TEXT = 0xE8E8E8
  TEXT_DIM = 0x9AA4AE
  ACCENT = 0x59D8FF
  PANEL = 0x0B1016
  DANGER = 0xE86A6A
  CALLS = []
  def self.panel(*a) = (CALLS << [:panel, *a])
  def self.text(*a) = (CALLS << [:text, *a])
end

module Game
  @map = { 'name' => 'Testkarte', 'terrain' => { 'width' => 120.0, 'height' => 90.0 } }
  @map_id = 'welt01'
  @events = []
  @npcs = []
  class << self
    attr_accessor :map, :map_id, :events, :npcs
  end

  def self.gy(_x, _z) = 0.0
end

# $game_temp-Attrappe (rgss.rb wird hier nicht geladen - Battle nutzt es
# nur für in_battle/gameover).
class FakeTemp
  attr_accessor :in_battle, :gameover
  def initialize
    @in_battle = false
    @gameover = false
  end
end
$game_temp = FakeTemp.new

$LOAD_PATH.unshift(File.join(Rune::ROOT, 'game', 'engine'))
require 'state'
require 'issues'
require 'data'         # Database: echte database.json (enemies/troops seit 19i)
require 'party'
require 'battle'
require 'ui'
require 'interpreter'

GameState.reset!({})
Party.setup(Database.database)

$fail = 0
def check(name)
  ok = yield
  puts(ok ? "  ok   #{name}" : "  FAIL #{name}")
  $fail += 1 unless ok
rescue StandardError => e
  puts "  FAIL #{name} (Ausnahme: #{e.class}: #{e.message})"
  $fail += 1
end

def druck(*keys)
  Rune::Input.pressed = keys
  Battle.update(0.016)
  Rune::Input.pressed = []
end

def druck_ui(interp, *keys)
  Rune::Input.pressed = keys
  interp.update(0.016)
  ni = interp.num_input
  interp.instance_variable_set(:@num_input, nil) if ni && !UI.nummer_eingabe(ni)
  Rune::Input.pressed = []
end

puts '== Datenbank: enemies/troops =='
check('enemies vorhanden') { Battle.enemy_def('goblin') && Battle.enemy_def('wolf') }
check('troops vorhanden') { Battle.troop_def('waldweg') && Battle.troop_def('brunnen') }

puts '== Battle.start =='
srand(1234)
check('unbekannte Truppe -> false') { Battle.start('gibt_es_nicht') == false }
check('start(waldweg) -> true') { Battle.start('waldweg') == true }
check('active? + in_battle') { Battle.active? && $game_temp.in_battle }
check('2 Gegner (Goblin, Wolf)') do
  Battle.enemies.size == 2 && Battle.enemies[0].name == 'Goblin' &&
    Battle.enemies[1].enemy?
end
check('Helden aus der Party (Arik, Mira)') do
  Battle.actors.size == 2 && Battle.actors[0].name == 'Arik' &&
    Battle.actors[0].actor?
end
check('Phase = Befehle, can_escape Default true') do
  Battle.phase == :command && Battle.can_escape
end
check('current_actor = erster lebender Held') { Battle.current_actor.name == 'Arik' }

puts '== Befehlsphase: Angriff + Zielwahl =='
druck(:down)
check('Cursor runter (Item)') { Battle.command_index == 1 }
druck(:up)
check('Cursor wieder auf Angriff') { Battle.command_index.zero? }
druck(:enter)
check('Angriff gewählt -> Zielwahl') { Battle.phase == :target }
check('Aktion gesammelt') { Battle.actions.size == 1 }
druck(:right)
check('Ziel-Cursor weiter (Wolf)') { Battle.target_index == 1 }
druck(:enter)
check('Ziel bestätigt -> nächster Held (Mira)') do
  Battle.phase == :command && Battle.current_actor.name == 'Mira'
end
goblin = Battle.enemies[0]
hp0 = goblin.hp
2.times { Battle.angriff(Battle.actors[0], goblin) }
check('Schaden zieht HP ab') { goblin.hp < hp0 && goblin.hp >= 0 }

puts '== Flucht =='
Battle.start('waldweg', can_escape: false)
2.times { druck(:down) }
check('Cursor auf Flucht') { Battle.command_index == 2 }
druck(:enter)
check('Flucht ohne can_escape: kein Abbruch, weiter Befehle') do
  Battle.active? && Battle.phase == :command
end
Battle.abort!
# 50%-Wurf der Flucht deterministisch stubben (Kernel#rand wird von
# Battle.attempt_escape ohne Receiver aufgerufen -> Singleton-Stub gewinnt).
Battle.singleton_class.send(:define_method, :rand) { 0.9 }
Battle.start('waldweg')
2.times { druck(:down) }
druck(:enter)
check('Flucht fehlschlagen: Runde beendet (Phase execute)') do
  Battle.active? && Battle.phase == :execute
end
Battle.update(0.016)   # execute: Aktionen + Gegnerzug + Runde 2
check('Aktionsphase gelaufen -> wieder Befehle (Runde 2)') do
  Battle.active? && Battle.phase == :command && Battle.turn == 1
end
Battle.abort!
Battle.singleton_class.send(:define_method, :rand) { 0.1 }
Battle.start('waldweg')
2.times { druck(:down) }
druck(:enter)
check('Flucht gelungen -> Kampf vorbei (Ergebnis flucht)') do
  !Battle.active? && Battle.result == :flucht
end
# Stub wieder entfernen: ab hier soll wieder echt gewürfelt werden.
Battle.singleton_class.send(:remove_method, :rand)

puts '== Item =='
Battle.start('waldweg')
Party.add_item('heiltrank', 1)
Battle.actors[0].damage(20)
check('usable_items enthält heiltrank') { Battle.usable_items.include?('heiltrank') }
druck(:down)
check('Cursor auf Item') { Battle.command_index == 1 }
druck(:enter)
check('Item-Auswahl offen') { Battle.phase == :item }
druck(:enter)
check('Item gewählt -> nächster Held (Mira)') do
  Battle.phase == :command && Battle.current_actor.name == 'Mira'
end
# Mira greift an -> Runde beendet; execute im nächsten Frame: Item wirkt
# (heilt den ersten verletzten Helden = Arik), Gegnerzug, Runde 2 beginnt.
druck(:enter)
druck(:enter)
Battle.update(0.016)
check('Aktionsphase gelaufen (Wieder Befehle, Runde 2)') do
  Battle.phase == :command && Battle.turn == 1
end
a0 = Battle.actors[0]
check("Heiltrank wirkt (ist hp=#{a0.hp}/#{a0.max_hp} item=#{Party.item_count('heiltrank')} log=#{Battle.instance_variable_get(:@log).last(3).join(' | ')})") do
  a0.hp >= a0.max_hp - 15 && Party.item_count('heiltrank').zero?
end
Battle.abort!
check('abort!: Kampf aus, in_battle zurück') do
  !Battle.active? && !$game_temp.in_battle
end

puts '== Sieg =='
Battle.enemies.each { |g| g.damage(9999) }
Battle.instance_variable_set(:@actions, [])
Battle.instance_variable_set(:@phase, :execute)
gold0 = Party.gold
Battle.update(0.016)   # execute: keine Aktionen, alle Gegner tot -> Sieg
check("Ergebnis Sieg (ist result=#{Battle.result.inspect} phase=#{Battle.phase.inspect} active=#{Battle.active?} hp=#{Battle.enemies.map(&:hp).inspect})") do
  Battle.result == :sieg && !Battle.active?
end
check("Belohnung: Gold gestiegen (vor=#{gold0} nach=#{Party.gold})") { Party.gold >= gold0 + 8 }
check("Exp auf Party-Helden verteilt (ist #{Party.actors.map { |a| [a.name, a.exp] }.inspect})") do
  Party.actors.sum(&:exp) >= 30
end
check('gameover bleibt aus') { $game_temp.gameover == false }
check('in_battle zurückgesetzt') { $game_temp.in_battle == false }

puts '== Niederlage + can_lose =='
srand(99)
Battle.start('waldweg')
Battle.actors.each { |a| a.hp = 0 }
Battle.instance_variable_set(:@phase, :execute)
Battle.update(0.016)
check('Niederlage setzt gameover') do
  Battle.result == :niederlage && $game_temp.gameover == true
end
check('Party-HP synchronisiert (0 bleibt 0)') { Party.actors.all? { |a| a.hp.zero? } }
Party.heal_all
$game_temp.gameover = false
Battle.start('waldweg', can_lose: true)
Battle.actors.each { |a| a.hp = 0 }
Battle.instance_variable_set(:@phase, :execute)
Battle.update(0.016)
check('can_lose: Niederlage OHNE gameover') do
  Battle.result == :niederlage && $game_temp.gameover == false
end
Party.heal_all
Battle.abort!

puts '== Schadensformel =='
quelle = Battle::Battler.new(id: 'q', name: 'Q', hp: 10, atk: 20, def_: 4, agi: 0)
ziel   = Battle::Battler.new(id: 'z', name: 'Z', hp: 1000, atk: 5, def_: 10, agi: 0)
srand(4242)
Battle.angriff(quelle, ziel)
schaden = 1000 - ziel.hp
check('Schaden ~ atk - def/2, ±15 %, min 1 (hier ~15)') do
  schaden >= 12 && schaden <= 18
end
check('Battler.damage clamped auf 0') do
  ziel.damage(99999)
  ziel.hp.zero? && ziel.dead?
end

puts '== Interpreter: battle blockiert =='
srand(7)
interp = Interpreter.new
ok = interp.start([{ 'cmd' => 'battle', 'name' => 'waldweg' },
                   { 'cmd' => 'variable', 'name' => 'nach', 'value' => 1 }])
interp.update(0.016)
check('battle gestartet + Interpreter blockiert') do
  ok && Battle.active? && interp.running? && interp.busy_battle?
end
check('Folge-Befehl läuft noch nicht') { GameState.variable('nach').zero? }
Battle.abort!
interp.update(0.016)
check('nach Kampfende läuft die Liste weiter') do
  GameState.variable('nach') == 1.0 && !interp.busy_battle?
end
interp2 = Interpreter.new
interp2.start([{ 'cmd' => 'battle', 'name' => 'unbekannt' },
               { 'cmd' => 'variable', 'name' => 'trotz', 'value' => 1 }])
interp2.update(0.016)
check('unbekannte Truppe: kein Blockieren, Liste läuft weiter') do
  !Battle.active? && GameState.variable('trotz') == 1.0
end

puts '== Interpreter: input_number =='
interp3 = Interpreter.new
interp3.start([{ 'cmd' => 'input_number', 'name' => 'ziffern', 'digits' => 3,
                 'initial' => 0 },
               { 'cmd' => 'variable', 'name' => 'fertig', 'value' => 1 }])
interp3.update(0.016)
check('Zahleneingabe offen + blockiert') do
  interp3.busy_number? && interp3.running?
end
def ist3(interp)
  ni = interp.num_input
  ni ? "#{ni[:ziffern].inspect} sel=#{ni[:sel]}" : 'nil'
end

ni = interp3.num_input
check('Status: 3 Ziffern, Cursor 0') do
  ni[:digits] == 3 && ni[:ziffern] == [0, 0, 0] && ni[:sel].zero?
end
druck_ui(interp3, :'1')
check("Zifferntaste 1 -> [1,0,0] sel=1 (ist #{ist3(interp3)})") do
  interp3.num_input[:ziffern] == [1, 0, 0] && interp3.num_input[:sel] == 1
end
druck_ui(interp3, :'2')
check("Zifferntaste 2 -> [1,2,0] sel=2 (ist #{ist3(interp3)})") do
  interp3.num_input[:ziffern] == [1, 2, 0] && interp3.num_input[:sel] == 2
end
druck_ui(interp3, :'7')
check("Zifferntaste 7 -> [1,2,7] sel=0 (ist #{ist3(interp3)})") do
  interp3.num_input[:ziffern] == [1, 2, 7] && interp3.num_input[:sel].zero?
end
druck_ui(interp3, :up)
check("hoch zählt erste Ziffer -> [2,2,7] (ist #{ist3(interp3)})") do
  interp3.num_input[:ziffern] == [2, 2, 7]
end
druck_ui(interp3, :left)
check("links wrap -> sel=2 (ist #{ist3(interp3)})") { interp3.num_input[:sel] == 2 }
druck_ui(interp3, :left)
check("links -> sel=1 (ist #{ist3(interp3)})") { interp3.num_input[:sel] == 1 }
druck_ui(interp3, :right)
check("rechts -> sel=2 (ist #{ist3(interp3)})") { interp3.num_input[:sel] == 2 }
druck_ui(interp3, :down)
check("runter zählt dritte Ziffer -> [2,2,6] (ist #{ist3(interp3)})") do
  interp3.num_input[:ziffern] == [2, 2, 6]
end
druck_ui(interp3, :enter)
check("Bestätigen setzt die Variable (226, ist #{GameState.variable('ziffern')})") do
  GameState.variable('ziffern') == 226.0 && !interp3.busy_number?
end
interp3.update(0.016)
check('Folge-Befehl läuft nach der Eingabe') { GameState.variable('fertig') == 1.0 }

interp4 = Interpreter.new
interp4.start([{ 'cmd' => 'input_number', 'name' => 'abbrechen', 'digits' => 2 }])
interp4.update(0.016)
druck_ui(interp4, :'5')
druck_ui(interp4, :escape)
check('Abbruch (cancel Default an) -> Variable 0') do
  GameState.variable('abbrechen').zero? && !interp4.busy_number?
end

interp5 = Interpreter.new
interp5.start([{ 'cmd' => 'input_number', 'name' => 'ohneabbruch', 'digits' => 2,
                 'cancel' => false }])
interp5.update(0.016)
druck_ui(interp5, :escape)
check('cancel=false: Esc blockiert weiter') { interp5.busy_number? }
druck_ui(interp5, :'9', :enter)
check('Bestätigen geht trotzdem (90)') do
  GameState.variable('ohneabbruch') == 90.0
end

interp6 = Interpreter.new
interp6.start([{ 'cmd' => 'input_number', 'name' => 'klemme', 'digits' => 99,
                 'initial' => 12345678 }])
interp6.update(0.016)
check('digits clamp 1..6, initial clamp') do
  interp6.num_input[:digits] == 6 && interp6.num_input[:ziffern] == [9, 9, 9, 9, 9, 9]
end
interp6.instance_variable_set(:@num_input, nil)

puts($fail.zero? ? 'BATTLE-TEST OK [V3] (0 Fehler)' : "BATTLE-TEST FEHLGESCHLAGEN [V3] (#{$fail} Fehler)")
exit($fail.zero? ? 0 : 1)
