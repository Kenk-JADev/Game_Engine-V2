#encoding: utf-8
# ============================================================================
#  Kopfloser Test fuer die Etappe-18f-Befehle (choices, toggle_switch,
#  move_player, weather, save/load) und den Parallel-Trigger.
#     ruby -I game/engine tools/event18f_test.rb
#  Exit-Code 0 = alles gruen.
# ============================================================================

module Rune
  ROOT = File.expand_path('..', __dir__)

  @keys = []
  @pos = [0.0, 0.0, 0.0]
  @yaw = 0.0

  def self.keys=(v); @keys = v; end
  def self.pos=(v); @pos = v; end

  module Log
    def self.info(m) = nil
    def self.warn(m) = puts("WARN  #{m}")
    def self.error(m) = puts("ERROR #{m}")
  end

  module Input
    def self.pressed?(key) = Rune.instance_variable_get(:@keys).include?(key)
  end

  module Player
    def self.teleport(x, y, z) = (Rune.pos = [x, y, z])
    def self.position = Rune.instance_variable_get(:@pos)
  end

  module Camera
    def self.yaw=(v); Rune.instance_variable_set(:@yaw, v); end
  end

  module Sound
    PLAYED = []
    def self.play(name, _v = 1.0) = PLAYED << name
  end
end

$LOAD_PATH.unshift(File.join(Rune::ROOT, 'game', 'engine'))
require 'data'
require 'state'
require 'issues'
require 'event'
require 'interpreter'
require 'effects'

module Rune
  module Gfx
    def self.box(*_a) = nil
  end
end

# Game-Attrappe fuer save/load (Game.save_game/load_game wird im Interpreter
# aufgerufen; die echte Implementierung braucht die ganze Engine).
module Game
  SAVED = []
  LOADED = []
  def self.save_game(slot) = SAVED << slot
  def self.load_game(slot) = LOADED << slot
  def self.gy(_x, _z) = 2.0
end

$fail = 0
def check(name)
  ok = yield
  puts(ok ? "  ok   #{name}" : "  FAIL #{name}")
  $fail += 1 unless ok
end

def keys!(*ks)
  Rune.keys = ks
  $interp.update(0.016)
  Rune.keys = []
end

def drain(limit = 200)
  limit.times do
    break if $interp.message || $interp.choices
    break unless $interp.running?
    $interp.update(0.016)
  end
end

GameState.reset!({ 'switches' => {}, 'variables' => {} })

# --- toggle_switch ----------------------------------------------------------
$interp = Interpreter.new
$interp.start([{ 'cmd' => 'toggle_switch', 'name' => 'test' }])
drain
check('toggle_switch: aus -> an') { GameState.switch('test') }
$interp = Interpreter.new
$interp.start([{ 'cmd' => 'toggle_switch', 'name' => 'test' }])
drain
check('toggle_switch: an -> aus') { !GameState.switch('test') }

# --- move_player ------------------------------------------------------------
Rune.pos = [10.0, 0.0, 20.0]
$interp = Interpreter.new
$interp.start([{ 'cmd' => 'move_player', 'x' => 5.0, 'z' => -3.0, 'relative' => true }])
drain
check('move_player relativ: x=15, z=17') do
  p = Rune::Player.position
  p[0] == 15.0 && p[2] == 17.0
end
$interp = Interpreter.new
$interp.start([{ 'cmd' => 'move_player', 'x' => 0.0, 'z' => 0.0, 'relative' => false }])
drain
check('move_player absolut: x=0, z=0') do
  p = Rune::Player.position
  p[0] == 0.0 && p[2] == 0.0
end

# --- weather ----------------------------------------------------------------
require 'weather'
$interp = Interpreter.new
$interp.start([{ 'cmd' => 'weather', 'name' => 'regen', 'intensity' => 1.5 }])
drain
check('weather: regen aktiv') { Weather.type == 'regen' }
check('weather: Intensitaet 1.5') { (Weather.intensity - 1.5).abs < 0.001 }

# --- save_game / load_game --------------------------------------------------
$interp = Interpreter.new
$interp.start([{ 'cmd' => 'save_game', 'slot' => 2 }])
drain
check('save_game Slot 2') { Game::SAVED == [2] }
$interp = Interpreter.new
$interp.start([{ 'cmd' => 'load_game', 'slot' => 3 }])
drain
check('load_game Slot 3') { Game::LOADED == [3] }

# --- choices: Anzeige, Wechsel, Zweig ---------------------------------------
cmds = [
  { 'cmd' => 'choices',
    'options' => ['Ja', 'Nein', 'Vielleicht'],
    'branches' => [
      [{ 'cmd' => 'switch', 'name' => 'wahl_ja', 'value' => true }],
      [{ 'cmd' => 'switch', 'name' => 'wahl_nein', 'value' => true }],
      [{ 'cmd' => 'switch', 'name' => 'wahl_vl', 'value' => true }],
    ] },
]
$interp = Interpreter.new
$interp.start(cmds)
drain
check('choices blockiert (Wahlfenster offen)') { $interp.busy_choices? }
check('choices: 3 Optionen') { $interp.choices[:options].size == 3 }
check('choices: Start-Auswahl 0') { $interp.choices[:sel] == 0 }
keys!(:down)
check('choices: Pfeil runter -> 1') { $interp.choices[:sel] == 1 }
keys!(:down)
check('choices: Pfeil runter -> 2') { $interp.choices[:sel] == 2 }
keys!(:down)
check('choices: Pfeil runter -> wrap zu 0') { $interp.choices[:sel] == 0 }
keys!(:up)
check('choices: Pfeil rauf -> wrap zu 2') { $interp.choices[:sel] == 2 }
keys!(:e)
check('choices: Fenster zu nach Bestaetigung') { $interp.choices.nil? }
drain
check('choices: richtiger Zweig (Vielleicht)') do
  GameState.switch('wahl_vl') && !GameState.switch('wahl_ja') && !GameState.switch('wahl_nein')
end
check('choices: Interpreter fertig') { !$interp.running? }

# --- choices: verschachtelt mit Text ----------------------------------------
cmds2 = [
  { 'cmd' => 'text', 'lines' => ['Frage?'] },
  { 'cmd' => 'choices',
    'options' => ['A', 'B'],
    'branches' => [
      [{ 'cmd' => 'text', 'lines' => ['Du hast A gewaehlt'] }],
      [{ 'cmd' => 'text', 'lines' => ['Du hast B gewaehlt'] }],
    ] },
]
$interp = Interpreter.new
$interp.start(cmds2)
drain
check('choices+text: erst Text') { $interp.message == ['Frage?'] }
keys!(:e)   # Text bestaetigen
drain
check('choices+text: dann Wahlfenster') { $interp.busy_choices? }
keys!(:down)
keys!(:e)
drain
check('choices+text: B-Text erscheint') { $interp.message == ['Du hast B gewaehlt'] }
keys!(:e)
drain
check('choices+text: fertig') { !$interp.running? }

# --- Parallel-Trigger: Event-Datenmodell ------------------------------------
ev = Event.new({ 'id' => 99, 'name' => 'ParaTest', 'trigger' => 'parallel',
                 'x' => 0, 'z' => 0, 'commands' => [] })
check('parallel-Trigger im Datenmodell') { ev.trigger == 'parallel' }

# --- Interpreter lief auch mit unbekanntem Befehl weiter --------------------
$interp = Interpreter.new
$interp.start([{ 'cmd' => 'unbekannt_xyz' }, { 'cmd' => 'switch', 'name' => 'nach', 'value' => true }])
drain
check('unbekannter Befehl wird uebersprungen') { GameState.switch('nach') }

puts($fail.zero? ? '== EVENT18F-TEST: ALLES GRUEN ==' : "== #{$fail} FEHLER ==")
exit($fail.zero? ? 0 : 1)
