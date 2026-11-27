#encoding: utf-8
# ==============================================================================
# Kopfloser Test der typisierten Event-Command-Schicht.
#   ruby -I game/engine tools/event_commands_test.rb
# ==============================================================================

require 'json'
require 'rpg'

$pruefungen = 0

def ok(text)
  $pruefungen += 1
  puts "ok - #{text}" if ENV['VERBOSE']
end

def assert(bedingung, text)
  raise "FAIL: #{text}" unless bedingung

  ok(text)
end

def gleich(erwartet, tatsaechlich, text)
  return ok(text) if erwartet == tatsaechlich

  raise "FAIL: #{text} (erwartet #{erwartet.inspect}, erhalten #{tatsaechlich.inspect})"
end

def kopie
  Marshal.load(Marshal.dump(GRUNDDOKUMENT))
end

def befehl_im_programm(dokument, programm_id, typ)
  programm = dokument['programs'].find { |eintrag| eintrag['id'] == programm_id }
  programm['commands'].find { |eintrag| eintrag['commandType'] == typ }
end

def expect_error(dokument, text)
  lader = Rpg::EventCommands::Loader.new(
    file_system: Rpg::EventCommands::Support::MemoryFileSystem.new({ 'test.json' => dokument })
  )
  begin
    lader.load('test.json')
    raise "FAIL: erwartete Validierung '#{text}'"
  rescue Rpg::EventCommands::Support::ValidationError => e
    assert(e.message.include?(text), "Validierung meldet '#{text}'")
  end
end

def test_ports(choice: 0, button: 0, number: 0, battle_result: 'win', busy: false)
  senke = Rpg::Ports::MemoryEventSink.new
  dispatcher = Rpg::Ports::EventDispatcher.new(sink: senke)
  ports = Rpg::EventCommands::Ports::PortSet.new(
    switches: Rpg::EventCommands::Ports::MemorySwitchStore.new,
    variables: Rpg::EventCommands::Ports::MemoryVariableStore.new,
    self_switches: Rpg::EventCommands::Ports::MemorySelfSwitchStore.new,
    timer: Rpg::EventCommands::Ports::MemoryTimerPort.new,
    inventory: Rpg::EventCommands::Ports::MemoryInventoryPort.new,
    party: Rpg::EventCommands::Ports::MemoryPartyPort.new,
    actors: Rpg::EventCommands::Ports::RecordingActorPort.new,
    map: Rpg::EventCommands::Ports::RecordingMapController.new,
    animation: Rpg::EventCommands::Ports::RecordingAnimationPort.new,
    pictures: Rpg::EventCommands::Ports::RecordingPicturePort.new,
    weather: Rpg::EventCommands::Ports::RecordingWeatherPort.new,
    transition: Rpg::EventCommands::Ports::RecordingTransitionPort.new,
    screen: Rpg::EventCommands::Ports::RecordingScreenEffectPort.new,
    audio: Rpg::EventCommands::Ports::RecordingAudioService.new,
    battle: Rpg::EventCommands::Ports::RecordingBattleController.new(result: battle_result),
    ui: Rpg::EventCommands::Ports::RecordingUINavigationService.new(
      choice: choice, button: button, number: number, busy: busy
    ),
    script: Rpg::EventCommands::Ports::RecordingScriptPort.new,
    dispatcher: dispatcher
  )
  [ports, dispatcher, senke]
end

ROOT = File.expand_path('..', __dir__)
PFAD = File.join(ROOT, 'game', 'config', 'event_commands.json')
GRUNDDOKUMENT = JSON.parse(File.read(PFAD, encoding: 'UTF-8'))
KATALOG = Rpg.load_event_commands(path: PFAD)

# --- Katalog und Beispieldaten ------------------------------------------------

gleich(90, KATALOG.definitions.size, 'Katalog enthält alle 90 XP-Befehlstypen')
gleich(30, KATALOG.definitions.count { |definition| definition.tab == 1 }, 'Tab 1 enthält 30 Befehle')
gleich(30, KATALOG.definitions.count { |definition| definition.tab == 2 }, 'Tab 2 enthält 30 Befehle')
gleich(30, KATALOG.definitions.count { |definition| definition.tab == 3 }, 'Tab 3 enthält 30 Befehle')
assert(KATALOG.definitions.all? { |definition| %i[left right].include?(definition.column) },
       'jeder Befehl kennt seine XP-Spalte')
[1, 2, 3].each do |tab|
  gleich(15, KATALOG.definitions.count { |definition| definition.tab == tab && definition.column == :left },
         "Tab #{tab} hat 15 Befehle in der linken Spalte")
  gleich(15, KATALOG.definitions.count { |definition| definition.tab == tab && definition.column == :right },
         "Tab #{tab} hat 15 Befehle in der rechten Spalte")
end
gleich(90, KATALOG.types.uniq.size, 'Befehlstypen sind eindeutig')
gleich(90, KATALOG.definitions.map(&:code).uniq.size, 'Befehlscodes sind eindeutig')
assert(KATALOG.definitions.all? { |definition| definition.handler.to_s.size.positive? },
       'jeder Befehl hat eine Handler-Kennung')

%w[show_text conditional_branch loop label call_common_event transfer_player
   set_move_route screen_flash play_bgm battle_processing change_equipment
   force_action script].each do |typ|
  assert(KATALOG.definition(typ), "Befehlstyp #{typ} ist im Katalog")
end

gleich('Tor_Smaragdwald_Offen', KATALOG.resources['switches'].first['name'], 'eigener Schalter 101')
gleich(101, KATALOG.resources['switches'].first['id'], 'Schalter-ID 101')
gleich('Alchemie_Punkte', KATALOG.resources['variables'].first['name'], 'eigene Variable 205')
gleich('bgm_ancient_citadel', KATALOG.resources['bgms'].first['key'], 'eigener BGM-Key')
gleich('se_crystal_chime', KATALOG.resources['ses'].first['key'], 'eigener SE-Key')
gleich('pic_crest_seal', KATALOG.resources['pictures'].first['key'], 'eigener Bild-Key')
gleich(7, KATALOG.resources['maps'].first['id'], 'eigene Map-ID 7')
gleich(:enemy_slot, KATALOG.definition('change_enemy_hp').fields.fetch('slot').kind,
       'Gegner-Slots sind typisiert')

gleich(4, KATALOG.programs.size, 'vier Beispielprogramme inklusive Common Event')
programm1 = KATALOG.program(1)
assert(programm1, 'Programm 1 geladen')
gleich('show_text', programm1.commands.first.commandType, 'Kommando trägt commandType')
assert(KATALOG.definition('control_switches').fields.key?('from_id') &&
       KATALOG.definition('control_switches').fields.key?('to_id'),
       'Control Switches hat von/bis')
assert(KATALOG.definition('control_variables').fields.key?('from_id') &&
       KATALOG.definition('control_variables').fields.key?('to_id'),
       'Control Variables hat von/bis')
assert(KATALOG.definition('set_move_route').fields.key?('ignore'), 'Set Move Route hat Ignore')
assert(KATALOG.definition('show_picture').fields.key?('picture_slot_id'), 'Bilder haben Slot-ID')
assert(KATALOG.definition('shop_processing').fields.key?('goods'), 'Shop hat Warenliste')
assert(KATALOG.definition('set_weather_effects').fields.key?('duration'), 'Wetter hat Dauer')
assert(KATALOG.definition('change_party_member').fields.key?('initialize'), 'Party hat Initialisieren')
assert(KATALOG.definition('prepare_transition').fields.key?('transition_key'), 'Transition hat Key')
assert(programm1.commands.frozen?, 'Programmbefehle sind eingefroren')
begin
  programm1.commands.first.params['lines'] << :fremd
  assert(false, 'Kommando-Parameter dürfen nicht veränderbar sein')
rescue FrozenError
  ok('Kommando-Parameter verweigern Mutation')
end

# --- Negative Validierung ----------------------------------------------------

dokument = kopie
befehl_im_programm(dokument, 1, 'show_text')['commandType'] = 'unknown_command'
expect_error(dokument, 'unbekannter Befehlstyp')

dokument = kopie
befehl_im_programm(dokument, 1, 'show_text')['params'].delete('lines')
expect_error(dokument, 'Pflichtfeld fehlt')

dokument = kopie
befehl_im_programm(dokument, 1, 'input_number')['params']['variable_id'] = 999
expect_error(dokument, 'Referenz')

dokument = kopie
befehl_im_programm(dokument, 2, 'play_bgm')['params']['bgm_key'] = 'bgm/unbekannt'
expect_error(dokument, 'unbekannter Asset-Key')

dokument = kopie
befehl_im_programm(dokument, 1, 'show_choices')['indent'] = 3
expect_error(dokument, 'erwartet 0')

dokument = kopie
befehl_im_programm(dokument, 1, 'show_choices').delete('indent')
expect_error(dokument, 'Pflichtfeld indent fehlt')

dokument = kopie
befehl_im_programm(dokument, 1, 'jump_to_label')['params']['name'] = 'gibts_nicht'
expect_error(dokument, 'Sprungmarke')

dokument = kopie
dokument['resources']['switches'] << dokument['resources']['switches'].first.dup
expect_error(dokument, 'doppelte Ressource')

dokument = kopie
befehl_im_programm(dokument, 1, 'show_text')['code'] = 999
expect_error(dokument, 'passt nicht zu')

dokument = kopie
befehl_im_programm(dokument, 1, 'show_text')['params']['background'] = 'bunt'
expect_error(dokument, 'muss einer von')

ok('Negativfälle abgedeckt')

reporter = Rpg::EventCommands::Support::CollectingReporter.new
dokument = kopie
befehl_im_programm(dokument, 2, 'play_se')['params']['se_key'] = 'se/unbekannt'
Rpg::EventCommands::Loader.new(
  file_system: Rpg::EventCommands::Support::MemoryFileSystem.new({ 'test.json' => dokument }),
  reporter: reporter
).load('test.json') rescue nil
assert(reporter.issues.any?, 'Reporter erhält gesammelte Befunde')

# --- Interpreter: Tab 1 ------------------------------------------------------

ports, _dispatcher, senke = test_ports(button: 0)
interpreter = Rpg.build_event_interpreter(catalog: KATALOG, ports: ports)
interpreter.run(KATALOG.program(1), max_updates: 5000)
assert(!interpreter.running?, 'Programm 1 läuft bis zum Ende')
gleich(true, ports.switches.get(101), 'Schalter 101 gesetzt')
gleich(true, ports.self_switches.get(interpreter.event_key, 'A'), 'Self-Switch A gesetzt')
gleich(4.0, ports.variables.get(205), 'Variable 205 nach Schleife und Sprung')
gleich(25, ports.party.gold, 'Gold geändert')
gleich(2, ports.inventory.count('items', 3), 'Vital Tonic hinzugefügt')
gleich(1, ports.inventory.count('weapons', 4), 'Rune Blade hinzugefügt')
gleich(1, ports.inventory.count('armors', 5), 'Warded Cloak hinzugefügt')
assert(ports.party.members.include?(1), 'Kael in der Party')
gleich(45.0, ports.timer.remaining, 'Timer gesetzt')
typen = senke.types
%w[text.shown common_event.started common_event.finished loop.broken
   jump.executed event.erased event.exit].each do |typ|
  assert(typen.include?(typ), "Dispatcher meldet #{typ}")
end
assert(ports.map.calls.any? { |eintrag| eintrag[:name] == 'erase_event' }, 'Erase Event erreicht das Map-Port')
assert(ports.ui.calls.any? { |eintrag| eintrag[:name] == 'choices' }, 'Auswahlmenü aufgerufen')

# --- Interpreter: Tab 2 ------------------------------------------------------

ports, _dispatcher, senke = test_ports
interpreter = Rpg.build_event_interpreter(catalog: KATALOG, ports: ports)
interpreter.run(KATALOG.program(2), max_updates: 5000)
assert(!interpreter.running?, 'Programm 2 läuft bis zum Ende')
transfer = ports.map.calls.find { |eintrag| eintrag[:name] == 'transfer' }
gleich(7, transfer[:map_id], 'Transfer nach Map 7')
gleich(14, transfer[:x], 'Transfer-X 14')
gleich(22, transfer[:y], 'Transfer-Y 22')
route = ports.map.calls.find { |eintrag| eintrag[:name] == 'move_route' }
gleich(3, route[:steps].size, 'drei typisierte Bewegungsschritte')
assert(ports.audio.calls.any? { |eintrag| eintrag[:name] == 'play_bgm' && eintrag[:key] == 'bgm_ancient_citadel' },
       'BGM über Audio-Port gestartet')
assert(ports.pictures.calls.any? { |eintrag| eintrag[:name] == 'show' }, 'Bild angezeigt')
assert(ports.pictures.calls.any? { |eintrag| eintrag[:name] == 'erase' }, 'Bild gelöscht')
assert(ports.weather.calls.any? { |eintrag| eintrag[:name] == 'set' }, 'Wetter gesetzt')
assert(ports.transition.calls.any? { |eintrag| eintrag[:name] == 'execute' }, 'Übergang ausgeführt')
assert(ports.screen.calls.any? { |eintrag| eintrag[:name] == 'flash' }, 'Flash ausgeführt')
assert(senke.types.include?('audio.command'), 'Audio-Kommando publiziert')

# --- Interpreter: Tab 3 ------------------------------------------------------

ports, _dispatcher, senke = test_ports
interpreter = Rpg.build_event_interpreter(catalog: KATALOG, ports: ports)
interpreter.run(KATALOG.program(3), max_updates: 5000)
assert(!interpreter.running?, 'Programm 3 läuft bis zum Ende')
assert(ports.battle.calls.any? { |eintrag| eintrag[:name] == 'start' && eintrag[:troop_id] == 9 },
       'Kampf mit Troop 9 gestartet')
gleich(12, ports.actors.calls.size, 'zwölf Actor-Operationen über Actor-Port')
gleich(8, ports.battle.calls.count { |eintrag| eintrag[:name] == 'enemy' }, 'acht Gegnerwert-Operationen')
assert(ports.battle.calls.any? { |eintrag| eintrag[:name] == 'force_action' }, 'Force Action ausgeführt')
assert(ports.battle.calls.any? { |eintrag| eintrag[:name] == 'abort' }, 'Abort Battle ausgeführt')
assert(ports.script.calls.any? { |eintrag| eintrag[:script_key] == 'script/check_gate' }, 'Script-Hook aufgerufen')
assert(ports.ui.calls.any? { |eintrag| eintrag[:name] == 'menu' }, 'Menü-Screen geöffnet')
assert(senke.types.include?('actor.command'), 'Actor-Kommando publiziert')

# --- Wait-, Move- und Erweiterbarkeitstests ----------------------------------

ports, _dispatcher, _senke = test_ports
interpreter = Rpg.build_event_interpreter(catalog: KATALOG, ports: ports)
warte_programm = Rpg::EventCommands::Models::Program.new(
  id: 90, name: 'warte_test',
  commands: [Rpg::EventCommands::Models::Command.new(
    commandType: 'wait', code: 106, indent: 0, params: { 'seconds' => 1.0 }
  )]
)
interpreter.start(warte_programm)
interpreter.update(0.25)
assert(interpreter.busy?, 'Wait blockiert den Interpreter')
interpreter.update(1.0)
assert(!interpreter.running?, 'Wait endet nach Ablauf')

ports, _dispatcher, _senke = test_ports
interpreter = Rpg.build_event_interpreter(catalog: KATALOG, ports: ports)
rahmen_programm = Rpg::EventCommands::Models::Program.new(
  id: 93, name: 'frames_test',
  commands: [Rpg::EventCommands::Models::Command.new(
    commandType: 'wait', code: 106, indent: 0, params: { 'frames' => 30 }
  )]
)
interpreter.start(rahmen_programm)
interpreter.update(0.25)
assert(interpreter.busy?, 'Frame-Wait blockiert den Interpreter')
interpreter.update(0.25)
assert(!interpreter.running?, 'Frame-Wait endet nach 30 Frames')

ports, _dispatcher, _senke = test_ports
ports.map.route_active = true
interpreter = Rpg.build_event_interpreter(catalog: KATALOG, ports: ports)
routen_programm = Rpg::EventCommands::Models::Program.new(
  id: 91, name: 'routen_test',
  commands: [Rpg::EventCommands::Models::Command.new(
    commandType: 'set_move_route', code: 209, indent: 0,
    params: { 'target' => 'player', 'steps' => [{ 'action' => 'wait' }], 'wait' => true, 'repeat' => false }
  )]
)
interpreter.start(routen_programm)
interpreter.update(1.0)
assert(interpreter.busy?, 'Wait for Move blockiert bei aktiver Route')
ports.map.route_active = false
interpreter.update(1.0)
assert(!interpreter.running?, 'Move-Wait endet mit Route')

ports, _dispatcher, senke = test_ports
interpreter = Rpg.build_event_interpreter(catalog: KATALOG, ports: ports)
interpreter.register('comment', lambda do |command, ziel|
  ziel.emit('handler.override', text: command.params['text'])
  Rpg::EventCommands::Handlers::Result.next
end)
kommentar_programm = Rpg::EventCommands::Models::Program.new(
  id: 92, name: 'handler_test',
  commands: [Rpg::EventCommands::Models::Command.new(
    commandType: 'comment', code: 108, indent: 0, params: { 'text' => 'eigener Handler' }
  )]
)
interpreter.run(kommentar_programm, max_updates: 10)
assert(senke.types.include?('handler.override'), 'Handler-Registry erlaubt Override ohne Interpreter-Änderung')

puts "Event-Commands: #{$pruefungen} Prüfungen, 0 Fehler"
