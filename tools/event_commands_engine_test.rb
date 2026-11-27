#encoding: utf-8
# ==============================================================================
#  Kopfloser Test: die 90 XP-Befehle an der ECHTEN Engine (Etappe 43).
#  Lädt GameState, Party, Battle, ScreenFx, Weather, Fade, Pictures, Mover,
#  MessageWindow, Shop, NameInput, RGSS und Interpreter mit Native-Attrappen
#  und lässt die Rpg-Programme darüber laufen. Kein GUI-/Sound-Hardwaretest.
#     ruby -I game/engine tools/event_commands_engine_test.rb
# ==============================================================================

module Rune
  ROOT = File.expand_path('..', __dir__)

  module Log
    @meldungen = []
    class << self
      attr_reader :meldungen
    end
    def self.info(_m) = nil
    def self.warn(m) = (@meldungen << [:warn, m])
    def self.error(m) = (@meldungen << [:error, m])
  end

  module Input
    @pressed = []
    class << self
      attr_accessor :pressed
    end
    def self.pressed?(key) = @pressed.include?(key)
    def self.down?(key) = @pressed.include?(key)
    def self.keys = @pressed
    def self.mouse_position = [0, 0]
    def self.mouse_pressed?(_button) = false
  end

  module Sound
    CALLS = []
    def self.play(name, volume = 1.0) = (CALLS << [:play, name, volume])
    def self.loop(name, volume = 1.0) = (CALLS << [:loop, name, volume])
    def self.stop(name) = (CALLS << [:stop, name])
    def self.stop_all = nil
  end

  module Player
    @pos = [0.0, 0.0, 0.0]
    class << self
      attr_accessor :pos
    end
    def self.teleport(x, y, z) = (self.pos = [x.to_f, y.to_f, z.to_f])
    def self.position = pos
    def self.on_ground? = true
    def self.yaw = 0.0
    def self.yaw=(wert); end
  end

  module Camera
    class << self
      attr_accessor :yaw, :shake
    end
    def self.shake=(wert); @shake = wert; end
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
    def self.texture(*a) = nil
    def self.image(*a) = nil
  end

  module Simulation
    def self.paused=(wert); end
    def self.paused = false
  end

  module ImageLoader
    def self.size(_name) = [64, 64]
  end
end

# Ein Event der Karte, wie Game.events ihn liefert.
class TestEvent
  attr_accessor :name, :x, :z, :id, :erased
  def initialize(name)
    @name = name
    @id = name
    @x = 0.0
    @z = 0.0
    @erased = false
  end
  def erase! = (@erased = true)
end

module Game
  @map = { 'name' => 'Testkarte', 'terrain' => { 'width' => 120.0, 'height' => 90.0 } }
  @events = [TestEvent.new('waechter')]
  @npcs = []
  class << self
    attr_accessor :map, :events, :npcs
  end
  def self.gy(_x, _z) = 0.0
  def self.map_id = 'dorf'
end

$LOAD_PATH.unshift(File.join(Rune::ROOT, 'game', 'engine'))
require 'json'
require 'state'
require 'issues'
require 'data'
require 'party'
require 'battle'
require 'screen_fx'
require 'weather'
require 'fade'
require 'image_loader'
require 'pictures'
require 'mover'
require 'message_window'
require 'ui'
require 'shop'
require 'name_input'
require 'rgss'
require 'interpreter'
require 'rpg'

GameState.reset!({})
Party.setup(Database.database)
RGSS.setup
Rpg::EventCommands::EnginePorts.reset_warnungen!

KATALOG = Rpg.load_event_commands
BINDUNGEN = {
  'actors' => { 1 => 'arik' },
  'items' => { 3 => 'heiltrank' },
  'troops' => { 9 => 'waldweg' },
  'states' => { 11 => 'gift' },
  'skills' => { 50 => 'kraftschlag' },
  'maps' => { 7 => 'dorf' },
  'events' => { 7 => 'waechter' }
}.freeze

$pruefungen = 0
$fehler = 0

def check(name)
  $pruefungen += 1
  ok = yield
  if ok
    puts "  ok   #{name}" if ENV['VERBOSE']
  else
    $fehler += 1
    puts "  FAIL #{name}"
  end
rescue StandardError => e
  $pruefungen += 1
  $fehler += 1
  puts "  FAIL #{name} (#{e.class}: #{e.message})"
end

def gleich(erwartet, tatsaechlich, name)
  check("#{name} (#{erwartet.inspect})") { erwartet == tatsaechlich }
end

def ports_fuer(interpreter: nil)
  Rpg::EventCommands::EnginePorts.build(katalog: KATALOG, bindungen: BINDUNGEN,
                                        interpreter: interpreter)
end

# Ein Frame wie in game/main.rb: die Engine-Module ticken mit, damit
# Bewegungsroute, Blende, Bilder und Timer wirklich fertig werden.
def frame(dt, interpreter = nil)
  interpreter&.update(dt)
  Mover.update(dt)
  Fade.update(dt)
  Pictures.update(dt)
  ScreenFx.update(dt)
  $game_system&.update(dt)
  AudioFiles.update(dt) if defined?(AudioFiles)
end

def laufen(programm_id, interpreter: nil, updates: 600, dt: 0.05)
  ports = ports_fuer(interpreter: interpreter)
  engine = Rpg.build_event_interpreter(catalog: KATALOG, ports: ports)
  engine.start(KATALOG.program(programm_id))
  updates.times do
    break unless engine.running?

    engine.update(dt)
    frame(dt, interpreter)
  end
  [engine, ports]
end

def befehl(typ, parameter, bloecke: {})
  definition = KATALOG.definition(typ)
  Rpg::EventCommands::Models::Command.new(
    commandType: typ, code: definition.code, indent: 0,
    params: Rpg::EventCommands::Typed.params_class(definition).from_params(parameter).to_h,
    children: bloecke
  )
end

# ticks: false lässt die Engine-Module stehen - so prüft man einen Effekt
# direkt nach dem Befehl, bevor Blende/Blitz/Wackeln wieder abgelaufen sind.
def einzeln(typ, parameter, interpreter: nil, bloecke: {}, updates: 30, ticks: true)
  programm = Rpg::EventCommands::Models::Program.new(id: 900, name: 'einzeln',
                                                     commands: [befehl(typ, parameter, bloecke: bloecke)])
  ports = ports_fuer(interpreter: interpreter)
  engine = Rpg.build_event_interpreter(catalog: KATALOG, ports: ports)
  engine.start(programm)
  updates.times do
    break unless engine.running?

    engine.update(0.05)
    frame(0.05, interpreter) if ticks
  end
  [engine, ports]
end

puts '== Tab 1: Zustand, Inventar, Party, System =='
GameState.reset!({})
Party.setup(Database.database)
RGSS.setup
Rune::Input.pressed = [:e]   # bestätigt Text, Wahl und Tasteneingabe
laufen(1)
Rune::Input.pressed = []

check('Schalter 101 steht in GameState') { GameState.switch('101') == true }
check('Variable 205 wurde gerechnet') { GameState.variable('205') != 0.0 }
check('Gold kam dazu') { Party.gold >= 25 }
gleich(4, Party.item_count('heiltrank'), 'zwei Heiltränke dazugekommen (Start 2)')
check('Held arik ist in der Party') { Party.member?('arik') }
check('Timer läuft') { $game_system.timer_seconds.positive? }
check('Windowskin gesetzt') { $game_system.windowskin_name == 'skin/obsidian' }
check('Kampf-BGM gesetzt') { $game_system.battle_bgm['name'].to_s != '' }
check('Kampf-End-ME gesetzt') { $game_system.battle_end_me['name'].to_s != '' }

einzeln('change_save_access', { 'allowed' => false })
check('Speichern gesperrt') { $game_system.save_disabled == true }
einzeln('change_menu_access', { 'allowed' => false })
check('Menü gesperrt') { $game_system.menu_disabled == true }
einzeln('change_encounter', { 'allowed' => false })
check('Begegnungen gesperrt') { $game_system.encounter_disabled == true }
einzeln('change_save_access', { 'allowed' => true })
check('Speichern wieder erlaubt') { $game_system.save_disabled == false }

einzeln('control_self_switch', { 'letter' => 'A', 'value' => true })
check('Self-Switch A gesetzt') { GameState.self_switch('program/900', 'A') == true }

einzeln('control_timer', { 'operation' => 'start', 'seconds' => 12.5 })
check('Timer auf 12,5 s') { (12.0..12.5).cover?($game_system.timer_seconds) }
einzeln('control_timer', { 'operation' => 'stop', 'seconds' => 0 })
check('Timer gestoppt') { $game_system.timer_working == false }

puts '== Tab 3: Heldenwerte =='
Party.setup(Database.database)
held = Party.held('arik')
start_hp = held.hp
einzeln('change_hp', { 'actor_id' => 1, 'operation' => 'subtract', 'amount' => 5, 'allow_knockout' => true })
gleich(start_hp - 5, Party.held('arik').hp, 'HP um 5 gesenkt')
einzeln('change_hp', { 'actor_id' => 1, 'operation' => 'add', 'amount' => 3, 'allow_knockout' => true })
gleich(start_hp - 2, Party.held('arik').hp, 'HP um 3 erhöht')

Party.held('arik').hp = 4
einzeln('change_hp', { 'actor_id' => 1, 'operation' => 'subtract', 'amount' => 99, 'allow_knockout' => false })
gleich(1, Party.held('arik').hp, 'ohne Knockout bleibt 1 HP')

start_mp = Party.held('arik').mp
einzeln('change_sp', { 'actor_id' => 1, 'operation' => 'subtract', 'amount' => 2 })
gleich(start_mp - 2, Party.held('arik').mp, 'SP/MP um 2 gesenkt')

einzeln('change_state', { 'actor_id' => 1, 'state_id' => 11, 'operation' => 'add' })
check('Zustand gift hinzugefügt') { Party.state_names(Party.held('arik')).include?('Gift') }
einzeln('change_state', { 'actor_id' => 1, 'state_id' => 11, 'operation' => 'remove' })
check('Zustand gift entfernt') { !Party.state_names(Party.held('arik')).include?('Gift') }

einzeln('change_skills', { 'actor_id' => 1, 'skill_id' => 50, 'operation' => 'add' })
check('Fertigkeit gelernt') { Party.skills('arik').include?('kraftschlag') }
einzeln('change_skills', { 'actor_id' => 1, 'skill_id' => 50, 'operation' => 'remove' })
check('Fertigkeit vergessen') { !Party.skills('arik').include?('kraftschlag') }

start_atk = Party.held('arik').atk
einzeln('change_parameters', { 'actor_id' => 1, 'parameter' => 'strength', 'operation' => 'add', 'amount' => 4 })
gleich(start_atk + 4, Party.held('arik').atk, 'XP strength wird zu atk')

einzeln('change_actor_name', { 'actor_id' => 1, 'name' => 'Arik der Alte' })
gleich('Arik der Alte', Party.held('arik').name, 'Held umbenannt')

Party.held('arik').hp = 1
einzeln('recover_all', { 'actor_id' => 1 })
check('vollständig geheilt') { Party.held('arik').hp == Party.held('arik').max_hp }

exp_vorher = Party.held('arik').exp
einzeln('change_exp', { 'actor_id' => 1, 'operation' => 'add', 'amount' => 40 })
check('Erfahrung gestiegen') { Party.held('arik').exp > exp_vorher }

puts '== Tab 2: Karte, Bilder, Screen, Audio, Wetter =='
RGSS.setup
Game.events = [TestEvent.new('waechter')]
laufen(2)

check('Kartenwechsel angefordert') { $game_temp.player_transferring == true }
gleich('dorf', $game_temp.player_new_map_id, 'Zielkarte über Bindung aufgelöst')
gleich(14, $game_temp.player_new_x, 'X = 14')
gleich(22, $game_temp.player_new_y, 'Y = 22')
gleich(8, $game_temp.player_new_direction, 'Blickrichtung up = 8')
check('Event versetzt') { Game.events.first.x == 4.0 && Game.events.first.z == 9.0 }
check('Bewegungsroute ist durchgelaufen') { !Mover.active? }
check('Spieler wurde versetzt') { Rune::Player.pos != [0.0, 0.0, 0.0] }

einzeln('play_bgm', { 'bgm_key' => 'bgm_ancient_citadel', 'volume' => 80, 'pitch' => 100 })
gleich('bgm_ancient_citadel', $game_system.playing_bgm['name'], 'BGM läuft')
gleich(80, $game_system.playing_bgm['volume'], 'BGM-Lautstärke 80')
einzeln('play_bgs', { 'bgs_key' => 'bgs_rain_on_stone', 'volume' => 60, 'pitch' => 100 })
gleich('bgs_rain_on_stone', $game_system.playing_bgs['name'], 'BGS läuft')
einzeln('show_picture', { 'picture_slot_id' => 20, 'picture_key' => 'pic_crest_seal',
                          'x' => 100, 'y' => 80, 'scale_x' => 100, 'scale_y' => 100,
                          'opacity' => 255, 'blend' => 'normal' })
check('Bild 20 sichtbar') { !Pictures.get(20).nil? }
gleich(100.0, Pictures.get(20)[:x], 'Bild bei x=100')
einzeln('change_screen_tone', { 'color' => { 'red' => 15, 'green' => -5, 'blue' => 25, 'gray' => 0 } })
gleich(15, ScreenFx.tone_aktuell[:r], 'Bildschirmton rot 15')
einzeln('set_weather_effects', { 'kind' => 'rain', 'intensity' => 6 })
gleich('regen', Weather.type, 'XP rain wird zu regen')
gleich(6.0, Weather.intensity, 'Intensität 6')

einzeln('set_move_route', { 'target' => 'player', 'wait' => false, 'repeat' => false,
                            'steps' => [{ 'action' => 'walk', 'direction' => 'up' },
                                        { 'action' => 'jump', 'dx' => 1, 'dz' => 0 },
                                        { 'action' => 'wait' }] })
route = Mover.routes.last
check('Route hat drei Schritte') { route && route.schritte.size == 3 }
check('Sprungschritt übersetzt') { route && route.schritte.include?('jump:1:0') }
check('Gehschritt übersetzt') { route && route.schritte.include?('move_up') }

einzeln('screen_flash', { 'color' => { 'red' => 255, 'green' => 74, 'blue' => 74 }, 'duration' => 0.5 }, ticks: false)
check('Blitz aktiv') { !ScreenFx.flash_aktuell.nil? }
check('Blitzfarbe rot erkannt') { ScreenFx.flash_aktuell[:farbe] == 'rot' }

einzeln('screen_shake', { 'strength' => 6, 'speed' => 4, 'duration' => 0.5 }, ticks: false)
check('Wackeln aktiv') { !ScreenFx.shake_aktuell.nil? }

einzeln('set_weather_effects', { 'kind' => 'snow', 'intensity' => 5 })
gleich('schnee', Weather.type, 'XP snow wird zu schnee')
gleich(5.0, Weather.intensity, 'Intensität 1:1 übernommen')

einzeln('change_transparent_flag', { 'transparent' => true })
check('Spieler transparent') { $game_player.transparent == true }

Fade.reset!   # Programm 2 hat bereits ausgeblendet; Blende neu starten
einzeln('execute_transition', { 'transition_key' => 'transition/rune_fade' }, ticks: false)
check('Übergang läuft') { Fade.laeuft? }
check('Übergang blendet aus') { Fade.alpha < 0.5 }

einzeln('memorize_bgm_bgs', {})
einzeln('play_bgm', { 'bgm_key' => 'bgm_ancient_citadel', 'volume' => 60, 'pitch' => 110 })
einzeln('restore_bgm_bgs', {})
check('BGM wiederhergestellt') { $game_system.playing_bgm['name'] == 'bgm_ancient_citadel' }

puts '== Tab 3: Kampf und Gegner =='
Party.setup(Database.database)
Battle.start('waldweg')
check('Kampf läuft') { Battle.active? }
gegner = Battle.enemies.first
start_hp = gegner.hp
einzeln('change_enemy_hp', { 'slot' => 0, 'operation' => 'subtract', 'amount' => 3, 'allow_knockout' => true })
gleich(start_hp - 3, Battle.enemies.first.hp, 'Gegner-HP gesenkt')
einzeln('deal_damage', { 'slot' => 0, 'amount' => 2, 'element_id' => 2 })
gleich(start_hp - 5, Battle.enemies.first.hp, 'Deal Damage wirkt')
einzeln('change_enemy_state', { 'slot' => 0, 'state_id' => 11, 'operation' => 'add' })
check('Gegner hat Zustand') { Battle.enemies.first.state?('gift') }
einzeln('change_enemy_state', { 'slot' => 0, 'state_id' => 11, 'operation' => 'remove' })
check('Gegnerzustand entfernt') { !Battle.enemies.first.state?('gift') }
einzeln('enemy_recover_all', { 'slot' => 0 })
check('Gegner geheilt') { Battle.enemies.first.hp == Battle.enemies.first.max_hp }
einzeln('abort_battle', {})
check('Kampf abgebrochen') { !Battle.active? }

puts '== UI: Text, Wahl, Zahl, Laden, Name, Screens =='
event_interpreter = Interpreter.new
einzeln('show_text', { 'lines' => ['Der Wächter hebt den Blick.'] }, interpreter: event_interpreter)
check('Textfenster offen') { event_interpreter.text_waiting? }
check('Zeilen sichtbar') { event_interpreter.visible_lines == ['Der Wächter hebt den Blick.'] }
Rune::Input.pressed = [:e]
event_interpreter.update(0.016)
Rune::Input.pressed = []
check('Text bestätigt') { !event_interpreter.text_waiting? }

event_interpreter = Interpreter.new
_engine, ports = einzeln('show_choices', { 'choices' => ['Ja', 'Nein'], 'cancel_index' => 1 },
                         interpreter: event_interpreter)
check('Wahl offen') { event_interpreter.busy_choices? }
Rune::Input.pressed = [:down]
event_interpreter.update(0.016)
Rune::Input.pressed = [:e]
event_interpreter.update(0.016)
Rune::Input.pressed = []
gleich(1, event_interpreter.choice_index, 'zweite Option gewählt')
check('Wahl geschlossen') { !event_interpreter.busy_choices? }

event_interpreter = Interpreter.new
einzeln('input_number', { 'variable_id' => 205, 'digits' => 3 }, interpreter: event_interpreter)
check('Zahleneingabe offen') { event_interpreter.busy_number? }
gleich(3, event_interpreter.num_input[:digits], 'drei Stellen')

einzeln('shop_processing', { 'shop_key' => 'shop/rune_market',
                             'goods' => [{ 'kind' => 'item', 'id' => 3, 'price' => 20, 'quantity' => 5 }] })
check('Laden offen') { Shop.active? }
Shop.close
einzeln('name_input_processing', { 'actor_id' => 1, 'max_length' => 8 })
check('Namenseingabe offen') { NameInput.active? }
NameInput.close

einzeln('call_menu_screen', {})
check('Menü angefordert') { $game_temp.menu_calling == true }
einzeln('call_save_screen', {})
check('Speichern angefordert') { $game_temp.save_calling == true }
einzeln('game_over', {})
check('Game Over angefordert') { $game_temp.gameover == true }
einzeln('return_to_title_screen', {})
check('Titel angefordert') { $game_temp.to_title == true }

einzeln('change_text_options', { 'background' => 'dim', 'position' => 'top', 'speed' => 4 })
gleich('oben', MessageWindow.position, 'Textposition oben')

puts '== Blockierende Fenster: Wahl, Zahl, Taste, Laden, Name =='
GameState.reset!({})
Party.setup(Database.database)
RGSS.setup

def programm_aus(id, commands)
  Rpg::EventCommands::Models::Program.new(id: id, name: "test#{id}", commands: commands)
end

def motor(interpreter: nil)
  ports = ports_fuer(interpreter: interpreter)
  [Rpg.build_event_interpreter(catalog: KATALOG, ports: ports), ports]
end

# Wahl: der gewählte Zweig muss wirklich laufen (XP 102).
wahl = befehl('show_choices', { 'choices' => ['Tor auf', 'Tor zu'], 'cancel_index' => 1 },
              bloecke: {
                'choice_0' => [befehl('control_switches', { 'from_id' => 301, 'to_id' => 301, 'value' => true })],
                'choice_1' => [befehl('control_switches', { 'from_id' => 302, 'to_id' => 302, 'value' => true })]
              })
interp = Interpreter.new
engine, = motor(interpreter: interp)
engine.start(programm_aus(901, [wahl]))
engine.update(0.05)
interp.update(0.05)
check('Wahl blockiert den Rpg-Interpreter') { engine.waiting? && engine.wait_kind == :ui }
check('Wahlfenster offen') { interp.busy_choices? }
Rune::Input.pressed = [:e]
interp.update(0.05)
Rune::Input.pressed = []
engine.update(0.05)
check('erster Wahlzweig lief') { GameState.switch('301') && !GameState.switch('302') }
check('Wahlprogramm zu Ende') { !engine.running? }
gleich(0, engine.choice_result, 'choice_result steht auf 0')

# Zahleneingabe: UI.nummer_eingabe schreibt, der Befehl liest danach (XP 103).
interp = Interpreter.new
engine, = motor(interpreter: interp)
engine.start(programm_aus(902, [befehl('input_number', { 'variable_id' => 205, 'digits' => 3 })]))
engine.update(0.05)
check('Zahleneingabe blockiert') { engine.wait_kind == :ui && interp.busy_number? }
feld = interp.num_input
Rune::Input.pressed = [:'7']
UI.nummer_eingabe(feld)
Rune::Input.pressed = [:e]
UI.nummer_eingabe(feld)
Rune::Input.pressed = []
interp.instance_variable_set(:@num_input, nil)
engine.update(0.05)
gleich(700, GameState.variable('205').to_i, 'Zahl 700 in Variable 205')
check('Zahlenprogramm zu Ende') { !engine.running? }

# Tasteneingabe wartet auf eine Taste (XP 105).
engine, = motor
engine.start(programm_aus(903, [befehl('button_input_processing', { 'variable_id' => 203 })]))
engine.update(0.05)
check('Tasteneingabe wartet') { engine.wait_kind == :ui }
check('noch kein Tastenwert') { GameState.variable('203').to_i.zero? }
Rune::Input.pressed = [:e]
engine.update(0.05)
Rune::Input.pressed = []
gleich(11, GameState.variable('203').to_i, 'Taste E wird zu 11')
check('Tastenprogramm zu Ende') { !engine.running? }

# Laden blockiert, bis das Fenster schließt (XP 302).
engine, = motor
engine.start(programm_aus(904, [befehl('shop_processing',
                                       { 'shop_key' => 'shop/rune_market',
                                         'goods' => [{ 'kind' => 'item', 'id' => 3, 'price' => 20 }] })]))
engine.update(0.05)
check('Laden blockiert') { Shop.active? && engine.wait_kind == :ui }
Shop.close
engine.update(0.05)
check('Laden zu, Programm läuft weiter') { !engine.running? }

# Namenseingabe: der neue Name wird nach dem Bestätigen gelesen (XP 303).
interp = Interpreter.new
engine, ports = motor(interpreter: interp)
gelesen = []
ports.dispatcher.subscribe('name.changed') { |nutzen| gelesen << nutzen[:name] }
engine.start(programm_aus(905, [befehl('name_input_processing', { 'actor_id' => 1, 'max_length' => 8 })]))
engine.update(0.05)
check('Namenseingabe blockiert') { NameInput.active? && engine.wait_kind == :ui }
NameInput.bestaetigen
engine.update(0.05)
check('Namenseingabe zu, Programm weiter') { !engine.running? }
gleich([Party.held('arik').name], gelesen, 'name.changed meldet den bestätigten Namen')

puts '== Ehrliche Grenzen: was die Engine nicht kann, wird gemeldet =='
Rpg::EventCommands::EnginePorts.reset_warnungen!
einzeln('change_equipment', { 'actor_id' => 1, 'slot' => 'weapon', 'item_id' => 4 })
einzeln('change_actor_class', { 'actor_id' => 1, 'class_id' => 10 })
einzeln('change_actor_graphic', { 'actor_id' => 1, 'character_key' => 'char/kael',
                                  'face_key' => 'face/kael', 'battler_key' => 'battler/kael' })
einzeln('change_weapons', { 'weapon_id' => 4, 'operation' => 'add', 'amount' => 1, 'include_equipment' => false })
einzeln('change_armors', { 'armor_id' => 5, 'operation' => 'add', 'amount' => 1, 'include_equipment' => false })
einzeln('scroll_map', { 'direction' => 'right', 'distance' => 3, 'speed' => 4 })
einzeln('change_fog_opacity', { 'opacity' => 120 })
einzeln('show_animation', { 'target' => 'player', 'animation_key' => 'fx/rune_burst' })
einzeln('enemy_transform', { 'slot' => 0, 'enemy_id' => 8 })
einzeln('force_action', { 'slot' => 0, 'action_key' => 'skill/rune_burst', 'target_slot' => 0 })

%w[change_equipment change_actor_class change_actor_graphic change_weapons change_armors
   scroll_map change_fog_opacity show_animation enemy_transform force_action].each do |typ|
  check("#{typ} wird als Grenze gemeldet") { Rpg::EventCommands::EnginePorts.fehlt?(typ) }
end
check('Warnungen stehen im Engine-Log') do
  Rune::Log.meldungen.any? { |stufe, text| stufe == :warn && text.include?('Rpg-Event:') }
end
check('keine stille Ausnahme') { Rpg::EventCommands::EnginePorts.warnungen.all? { |w| w[:grund].size > 5 } }

puts '== Dependency Injection: Attrappen statt Engine =='
attrappe = Class.new do
  attr_reader :werte
  def initialize = @werte = {}
  def switch(name) = @werte[name.to_s] || false
  def set_switch(name, value) = (@werte[name.to_s] = !!value)
end.new
ports = Rpg::EventCommands::EnginePorts.build(katalog: KATALOG, zustand: attrappe)
ports.switches.set(999, true)
check('injizierter Zustand bekommt den Schalter') { attrappe.werte['999'] == true }
check('echtes GameState bleibt unberührt') { GameState.switch('999') == false }

schrift = Class.new do
  attr_reader :aufrufe
  def initialize = @aufrufe = []
  def eval_code(code, _kontext) = (@aufrufe << code; true)
end.new
ports = Rpg::EventCommands::EnginePorts.build(katalog: KATALOG, scripts: schrift)
ports.script.run(script_key: 'script/check_gate', code: 'gate.check', context: {})
gleich(['gate.check'], schrift.aufrufe, 'Script-Hook läuft über die injizierte Schnittstelle')

puts($fehler.zero? ? "EVENT-COMMANDS AN DER ENGINE OK (#{$pruefungen} Prüfungen, 0 Fehler)"
                   : "EVENT-COMMANDS AN DER ENGINE FEHLGESCHLAGEN (#{$fehler} von #{$pruefungen})")
exit($fehler.zero? ? 0 : 1)
