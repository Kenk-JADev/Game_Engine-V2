#encoding: utf-8
# ==============================================================================
#  Kopfloser Architektur-Test der Event-Command-Schicht (Etappe 42).
#  Prüft den Bauplan der drei XP-Tabs, die typisierten Command-Objekte,
#  je einen Handler pro Befehlstyp, die entkoppelten Ports und den Interpreter.
#     ruby -I game/engine tools/event_commands_arch_test.rb
#  Exit-Code 0 = alles gruen.
# ==============================================================================

require 'json'
require 'rpg'

PFAD = File.join(Rpg::EventCommands::Loader::DEFAULT_PATH)
ROHDOKUMENT = JSON.parse(File.read(PFAD, encoding: 'UTF-8'))
KATALOG = Rpg.load_event_commands(path: PFAD)
LAYOUT = Rpg::EventCommands::Layout

$pruefungen = 0
$fehler = 0

def ok(text)
  $pruefungen += 1
  puts "  ok   #{text}" if ENV['VERBOSE']
end

def assert(bedingung, text)
  $pruefungen += 1
  return ok(text) if bedingung

  $fehler += 1
  puts "  FAIL #{text}"
end

def gleich(erwartet, tatsaechlich, text)
  return assert(true, text) if erwartet == tatsaechlich

  $pruefungen += 1
  $fehler += 1
  puts "  FAIL #{text} (erwartet #{erwartet.inspect}, erhalten #{tatsaechlich.inspect})"
end

def wirft(klasse, text)
  yield
  assert(false, "#{text} (kein Fehler)")
rescue klasse
  assert(true, text)
rescue StandardError => e
  assert(false, "#{text} (falscher Fehler: #{e.class})")
end

# --- 1. Bauplan: drei Tabs, zwei Spalten, zwei Abschnitte, 90 Befehle ---------

ERWARTET = {
  1 => {
    left: {
      'dialog' => %w[show_text show_choices input_number change_text_options
                     button_input_processing wait comment],
      'flow' => %w[conditional_branch loop break_loop exit_event_processing erase_event
                   call_common_event label jump_to_label]
    },
    right: {
      'state' => %w[control_switches control_variables control_self_switch control_timer
                    change_gold change_items change_weapons change_armors change_party_member],
      'system' => %w[change_windowskin change_battle_bgm change_battle_end_me
                     change_save_access change_menu_access change_encounter]
    }
  },
  2 => {
    left: {
      'map' => %w[transfer_player set_event_location scroll_map change_map_settings
                  change_fog_color_tone change_fog_opacity show_animation
                  change_transparent_flag set_move_route wait_for_move_completion],
      'screen' => %w[prepare_transition execute_transition change_screen_tone
                     screen_flash screen_shake]
    },
    right: {
      'picture' => %w[show_picture move_picture rotate_picture change_picture_tone
                      erase_picture set_weather_effects],
      'audio' => %w[play_bgm fade_out_bgm play_bgs fade_out_bgs memorize_bgm_bgs
                    restore_bgm_bgs play_me play_se stop_se]
    }
  },
  3 => {
    left: {
      'battle' => %w[battle_processing shop_processing name_input_processing],
      'actor' => %w[change_hp change_sp change_state recover_all change_exp change_level
                    change_parameters change_skills change_equipment change_actor_name
                    change_actor_class change_actor_graphic]
    },
    right: {
      'enemy' => %w[change_enemy_hp change_enemy_sp change_enemy_state enemy_recover_all
                    enemy_appearance enemy_transform show_battle_animation deal_damage
                    force_action abort_battle],
      'system' => %w[call_menu_screen call_save_screen game_over return_to_title_screen script]
    }
  }
}.freeze

gleich(3, LAYOUT.tabs.size, 'genau drei Tabs')
gleich(90, LAYOUT.entries.size, 'genau 90 Befehle im Bauplan')
gleich(6, LAYOUT.tabs.flat_map(&:columns).size, 'je Tab zwei Spalten')
gleich(12, LAYOUT.sections.size, 'je Spalte zwei Abschnitte')
gleich(90, LAYOUT.types.uniq.size, 'kein Befehlstyp doppelt')

ERWARTET.each do |nummer, spalten|
  tab = LAYOUT.tab(nummer)
  assert(!tab.nil?, "Tab #{nummer} existiert")
  assert(!tab.title.to_s.empty?, "Tab #{nummer} hat einen Titel")
  spalten.each do |seite, abschnitte|
    spalte = tab.column(seite)
    gleich(abschnitte.keys, spalte.sections.map(&:key), "Tab #{nummer}/#{seite}: Abschnitte in Reihenfolge")
    gleich(15, spalte.entries.size, "Tab #{nummer}/#{seite}: 15 Befehle")
    abschnitte.each do |schluessel, typen|
      abschnitt = spalte.sections.find { |eintrag| eintrag.key == schluessel }
      gleich(typen, abschnitt.types, "Tab #{nummer}/#{seite}/#{schluessel}: XP-Reihenfolge")
      assert(abschnitt.title.to_s.size > 3, "Abschnitt #{schluessel} hat eine Beschriftung")
    end
  end
end

assert(LAYOUT.entries.all? { |eintrag| eintrag.label.size > 2 }, 'jeder Befehl hat eine deutsche Beschriftung')
assert(LAYOUT.entries.all? { |eintrag| eintrag.xp_name.match?(/\A[A-Z]/) }, 'jeder Befehl nennt seinen XP-Namen')
gleich('Show Text', LAYOUT.xp_name('show_text'), 'XP-Name von show_text')
gleich('Auf Bewegung warten', LAYOUT.label('wait_for_move_completion'), 'Beschriftung von wait_for_move_completion')
gleich(101, LAYOUT.entry('show_text').code, 'XP-Code 101 für Show Text')
gleich(355, LAYOUT.entry('script').code, 'XP-Code 355 für Script')
assert(LAYOUT.als_text.lines.size >= 108, 'Textbaum zeigt Tabs, Spalten, Abschnitte und Befehle')

# --- 2. Katalog folgt dem Bauplan --------------------------------------------

assert(LAYOUT.pruefen!(KATALOG), 'Katalog und Bauplan stimmen überein')
gleich(90, KATALOG.definitions.size, 'Katalog führt 90 Definitionen')
[1, 2, 3].each do |nummer|
  %i[left right].each do |seite|
    gleich(15, KATALOG.definitions_for(tab: nummer, column: seite).size,
           "Katalog Tab #{nummer}/#{seite}: 15 Definitionen")
  end
end
assert(KATALOG.definitions.all? { |d| d.label == LAYOUT.label(d.type) }, 'Beschriftung kommt aus dem Bauplan')
assert(KATALOG.definitions.all? { |d| d.section == LAYOUT.entry(d.type).section }, 'Abschnitt kommt aus dem Bauplan')
gleich(0, KATALOG.definition('show_text').position, 'Show Text steht an erster Stelle')

kaputt = Rpg::EventCommands::Models::CatalogData.new(
  version: 1, definitions: KATALOG.definitions.reject { |d| d.type == 'script' },
  resources: {}, programs: []
)
wirft(ArgumentError, 'fehlender Befehl im Katalog wird abgewiesen') { LAYOUT.pruefen!(kaputt) }

# --- 3. Typisierte Command-Objekte -------------------------------------------

def alle_befehle(liste)
  liste.flat_map { |befehl| [befehl] + befehl.children.values.flat_map { |kinder| alle_befehle(kinder) } }
end

def vergleiche_roh(typisiert, roh)
  gleich(roh.params, typisiert.to_h['params'], "#{roh.commandType}: Parameter verlustfrei")
  gleich(roh.commandType, typisiert.commandType, "#{roh.commandType}: Typ bleibt")
  gleich(roh.code, typisiert.code, "#{roh.commandType}: XP-Code bleibt")
  gleich(roh.indent, typisiert.indent, "#{roh.commandType}: Einrückung bleibt")
  roh.children.each do |name, liste|
    gleich(liste.size, typisiert.block(name).size, "#{roh.commandType}/#{name}: Blocklänge bleibt")
    liste.each_with_index { |kind, index| vergleiche_roh(typisiert.block(name)[index], kind) }
  end
end

genutzt = []
KATALOG.programs.each do |programm|
  typisiert = KATALOG.typed_program(programm.id)
  gleich(programm.commands.size, typisiert.size, "Programm #{programm.id}: Länge bleibt")
  programm.commands.each_with_index do |roh, index|
    vergleiche_roh(typisiert[index], roh)
    genutzt << roh.commandType
    genutzt.concat(alle_befehle(programm.commands).map(&:commandType))
  end
end
gleich(90, genutzt.uniq.size, 'alle 90 Befehlstypen kommen in den Beispielen vor')

text = KATALOG.typed(KATALOG.program(1).commands.first)
gleich(Rpg::EventCommands::Typed::Commands::ShowText, text.class, 'ShowText hat eine eigene Klasse')
gleich(['Kael: Das Siegel antwortet.', 'Mira: Der Riegel wartet.'], text.lines, 'ShowText#lines ist typisiert')
gleich('window', text.background, 'ShowText füllt Vorgaben auf')
assert(text.frozen?, 'typisiertes Command-Objekt ist unveränderlich')
assert(text.params.frozen?, 'Parameterstruktur ist unveränderlich')
gleich(Rpg::EventCommands::Typed::Params::ShowText, text.params.class, 'eigene Parameterklasse')

wahl = KATALOG.typed(KATALOG.program(1).commands.find { |c| c.commandType == 'show_choices' })
assert(wahl.block('choice_0').first.is_a?(Rpg::EventCommands::Typed::Commands::ShowText),
       'Blöcke enthalten typisierte Kinder')
assert(wahl.children.values.flatten.all?(&:frozen?), 'Kinder sind unveränderlich')

zahl = KATALOG.typed(KATALOG.program(1).commands.find { |c| c.commandType == 'input_number' })
gleich(202, zahl.variable_id, 'InputNumber zeigt auf Variable 202')
assert(zahl.digits.is_a?(Integer), 'Stellenanzahl ist ganzzahlig')

warten = KATALOG.typed(KATALOG.program(1).commands.find { |c| c.commandType == 'wait' })
assert(warten.seconds.is_a?(Float) || warten.frames.is_a?(Integer), 'Wait kennt Sekunden und Frames')

party = KATALOG.typed(KATALOG.program(1).commands.find { |c| c.commandType == 'change_party_member' })
gleich(true, party.initialize_actor, 'ChangePartyMember#initialize_actor (Ruby-sicherer Name)')
gleich(true, party.to_h['params']['initialize'], 'JSON-Name initialize bleibt beim Rückschreiben')

skript = KATALOG.typed(KATALOG.program(3).commands.find { |c| c.commandType == 'script' })
gleich(355, skript.code, 'Script#code bleibt der XP-Code')
assert(skript.param_code.is_a?(String), 'Script-Quelltext liegt in param_code')

transfer = KATALOG.typed(KATALOG.program(2).commands.find { |c| c.commandType == 'transfer_player' })
gleich([12, 8, 14], [transfer.map_id, transfer.x, transfer.y], 'TransferPlayer auf Karte 12 an (8, 14)')

KATALOG.definitions.each do |definition|
  klasse = Rpg::EventCommands::Typed.command_class(definition)
  params = Rpg::EventCommands::Typed.params_class(definition)
  gleich(definition.type, klasse.command_type, "#{definition.type}: Command-Klasse benannt")
  gleich(definition.fields.size, params.field_names.size, "#{definition.type}: alle Felder typisiert")
end

wirft(ArgumentError, 'Pflichtfeld fehlt wird abgewiesen') do
  Rpg::EventCommands::Typed.params_class(KATALOG.definition('show_text')).from_params({})
end
wirft(ArgumentError, 'falscher Typ wird abgewiesen') do
  Rpg::EventCommands::Typed.params_class(KATALOG.definition('input_number'))
    .from_params('variable_id' => 202, 'digits' => 'vier')
end
gleich(text.class, KATALOG.typed(KATALOG.program(1).commands.first).class,
       'wiederholter Cast nutzt dieselbe Klasse')

wirft(Rpg::EventCommands::Typed::UnknownCommand, 'unbekannter Befehlstyp wird abgewiesen') do
  KATALOG.typed(Rpg::EventCommands::Models::Command.new(commandType: 'gibts_nicht', code: 999, indent: 0))
end

# --- 4. Je Befehlstyp ein eigener Handler ------------------------------------

REGISTER = Rpg::EventCommands::Handlers::Registry.new
gleich(90, REGISTER.size, 'Registry hält 90 Handler')
gleich(90, REGISTER.types.uniq.size, 'kein Handler-Typ doppelt')
assert(KATALOG.definitions.all? { |d| REGISTER.registered?(d.type) }, 'jeder Befehlstyp ist registriert')
assert(KATALOG.definitions.all? { |d| REGISTER.for(d).command_type == d.type },
       'Handler-Identität passt zum Befehlstyp')
gleich(90, KATALOG.definitions.map { |d| REGISTER.for(d).object_id }.uniq.size,
       'keine geteilte Handler-Instanz zwischen Typen')
wirft(KeyError, 'fehlender Handler ist ein harter Fehler') do
  REGISTER.for(Rpg::EventCommands::Models::Definition.new(type: 'gibts_nicht', code: 999, tab: 1,
                                                         group: :dialog, handler: :dialog))
end

eigene = Rpg::EventCommands::Handlers::Registry.new
eigene.register('comment', Rpg::EventCommands::Handlers::CommandHandler.new('comment') do |_c, i|
  i.emit('test.eigen')
  Rpg::EventCommands::Handlers::Result.next
end)
senke = Rpg::Ports::MemoryEventSink.new
ports = Rpg::EventCommands::Ports::PortSet.new(dispatcher: Rpg::Ports::EventDispatcher.new(sink: senke))
programm = Rpg::EventCommands::Models::Program.new(
  id: 90, name: 'eigen',
  commands: [Rpg::EventCommands::Models::Command.new(commandType: 'comment', code: 108, indent: 0,
                                                     params: { 'text' => 'x' })]
)
Rpg.build_event_interpreter(catalog: KATALOG, ports: ports, registry: eigene).run(programm, max_updates: 5)
assert(senke.types.include?('test.eigen'), 'injizierter Handler ersetzt den Standard')

# --- 5. Ports: Schnittstellen, Vorgaben und Dependency Injection -------------

%w[ISwitchStore IVariableStore ISelfSwitchStore IInventoryService IPartyService
   IMapController IPictureService IScreenEffectService IAudioService IBattleController
   IUINavigationService ISystemSettingsService].each do |name|
  assert(Rpg::EventCommands::Ports.const_defined?(name), "Schnittstelle #{name} existiert")
end
gleich(Rpg::EventCommands::Ports::IInventoryPort, Rpg::EventCommands::Ports::IInventoryService,
       'IInventoryService ist der Inventar-Port')
gleich(Rpg::EventCommands::Ports::IPicturePort, Rpg::EventCommands::Ports::IPictureService,
       'IPictureService ist der Bild-Port')

leer = Rpg::EventCommands::Ports::PortSet.new
assert(leer.switches.is_a?(Rpg::EventCommands::Ports::NullSwitchStore), 'ohne Injektion greifen Null-Adapter')
assert(leer.system_settings.is_a?(Rpg::EventCommands::Ports::NullSystemSettingsService),
       'System-Settings haben einen Null-Adapter')
Rpg.build_event_interpreter(catalog: KATALOG, ports: leer).run(KATALOG.program(1), max_updates: 500)
assert(true, 'Null-Ports lassen ein Programm ohne Engine durchlaufen')

def test_ports
  senke = Rpg::Ports::MemoryEventSink.new
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
    battle: Rpg::EventCommands::Ports::RecordingBattleController.new,
    ui: Rpg::EventCommands::Ports::RecordingUINavigationService.new,
    script: Rpg::EventCommands::Ports::RecordingScriptPort.new,
    system_settings: Rpg::EventCommands::Ports::RecordingSystemSettingsService.new,
    dispatcher: Rpg::Ports::EventDispatcher.new(sink: senke)
  )
  [ports, senke]
end

ports, senke = test_ports
Rpg.build_event_interpreter(catalog: KATALOG, ports: ports).run(KATALOG.program(1), max_updates: 500)
gleich(true, ports.switches.get(101), 'Schalter 101 (Tor_Offen) gesetzt')
assert(ports.variables.get(202) != 0, 'Variable 202 (Quest_Phase) verändert')
gleich(4, ports.system_settings.calls.size, 'Windowskin, Speichern, Menü, Begegnung erreichen den Port')
gleich(%w[windowskin save_access menu_access encounter], ports.system_settings.calls.map { |c| c[:name] },
       'System-Settings in Befehlsreihenfolge')
assert(ports.audio.calls.any? { |c| c[:name] == 'set_battle_bgm' }, 'Kampf-BGM erreicht den Audio-Port')
assert(ports.audio.calls.any? { |c| c[:name] == 'set_battle_end_me' }, 'Kampf-End-ME erreicht den Audio-Port')
assert(senke.types.include?('system.setting'), 'Systemänderung wird gemeldet')
assert(ports.ui.calls.any? { |c| c[:name] == 'text_options' }, 'Textoptionen erreichen den UI-Port')

ports, _senke = test_ports
Rpg.build_event_interpreter(catalog: KATALOG, ports: ports).run(KATALOG.program(2), max_updates: 500)
assert(ports.map.calls.any? { |c| c[:name] == 'transfer' && c[:map_id] == 12 && c[:x] == 8 && c[:y] == 14 },
       'Transfer auf Karte 12 (8, 14) erreicht den Karten-Port')
assert(ports.audio.calls.any? { |c| c[:name] == 'play_bgm' && c[:key] == 'bgm_tavern_night' },
       'BGM bgm_tavern_night erreicht den Audio-Port')
assert(ports.audio.calls.any? { |c| c[:name] == 'play_se' && c[:key] == 'se_door_latch' },
       'SE se_door_latch erreicht den Audio-Port')
assert(ports.pictures.calls.size >= 5, 'alle fünf Bildbefehle erreichen den Bild-Port')
gleich(1, ports.weather.calls.size, 'Wetter erreicht den Wetter-Port')

ports, _senke = test_ports
Rpg.build_event_interpreter(catalog: KATALOG, ports: ports).run(KATALOG.program(3), max_updates: 500)
gleich(12, ports.actors.calls.size, 'zwölf Heldenbefehle erreichen den Helden-Port')
gleich(10, ports.battle.calls.count { |c| %w[enemy force_action abort].include?(c[:name]) },
       'zehn Gegnerbefehle erreichen den Kampf-Port')
assert(!defined?($game_party) && !defined?($data_actors), 'keine RGSS-Globalen werden angefasst')

# --- 6. Interpreter: Zeiger, Blöcke, Marken, Aufrufstapel, Warten ------------

ports, senke = test_ports
interpreter = Rpg.build_event_interpreter(catalog: KATALOG, ports: ports)
interpreter.start(KATALOG.program(1))
gleich(0, interpreter.program_counter, 'Befehlszeiger startet bei 0')
gleich(1, interpreter.depth, 'nur das Programm liegt auf dem Stapel')
interpreter.update(0.016)
assert(interpreter.program_counter.positive?, 'Befehlszeiger rückt vor')
assert(interpreter.current_command.is_a?(Rpg::EventCommands::Models::Command), 'aktueller Befehl ist lesbar')
assert(interpreter.call_stack.first[:program_id] == 1, 'Aufrufstapel nennt das Programm')

interpreter.run(KATALOG.program(1), max_updates: 500)
assert(!interpreter.running?, 'Programm 1 läuft bis zum Ende')
assert(interpreter.program_counter.nil?, 'Befehlszeiger ist nach dem Ende leer')
assert(senke.types.include?('common_event.started'), 'Common Event wurde aufgerufen')
assert(senke.types.include?('common_event.finished'), 'Common Event wurde beendet')
assert(senke.types.include?('loop.broken'), 'Break Loop verlässt die Schleife')
assert(senke.types.include?('jump.executed'), 'Jump to Label springt')

ports, _senke = test_ports
interpreter = Rpg.build_event_interpreter(catalog: KATALOG, ports: ports)
interpreter.start(KATALOG.program(1))
500.times do
  interpreter.update(0.016)
  break if interpreter.common_event_depth.positive?
end
gleich(1, interpreter.common_event_depth, 'Common Event liegt als eigener Frame auf dem Stapel')
gleich(:common, interpreter.call_stack.last[:kind], 'oberster Frame ist ein Common Event')
gleich(9, interpreter.call_stack.last[:common_event_id], 'Aufrufstapel nennt Common Event 9')
interpreter.run(KATALOG.program(1), max_updates: 500)

def ein_befehl(typ, parameter, ports: nil, bloecke: {})
  definition = KATALOG.definition(typ)
  befehl = Rpg::EventCommands::Models::Command.new(
    commandType: typ, code: definition.code, indent: 0,
    params: Rpg::EventCommands::Typed.params_class(definition).from_params(parameter).to_h,
    children: bloecke
  )
  programm = Rpg::EventCommands::Models::Program.new(id: 99, name: 'einzeln', commands: [befehl])
  interpreter = Rpg.build_event_interpreter(catalog: KATALOG, ports: ports || test_ports.first)
  [interpreter, programm]
end

interpreter, programm = ein_befehl('wait', { 'frames' => 30 })
interpreter.start(programm)
interpreter.update(0.25)
gleich(:time, interpreter.wait_kind, 'Wait wartet in Zeit')
assert(interpreter.busy?, 'Wait blockiert')
interpreter.update(0.25)
assert(!interpreter.running?, '30 Frames sind nach zwei Updates vorbei')

ports = test_ports.first
ports.map.route_active = true
interpreter, programm = ein_befehl('wait_for_move_completion', {}, ports: ports)
interpreter.start(programm)
interpreter.update(1.0)
gleich(:move, interpreter.wait_kind, 'Wait for Move wartet auf die Route')
ports.map.route_active = false
interpreter.update(1.0)
assert(!interpreter.running?, 'Route beendet das Warten')

ports = test_ports.first
ports.ui.busy = true
interpreter, programm = ein_befehl('show_text', { 'lines' => ['Hallo'] }, ports: ports)
interpreter.start(programm)
interpreter.update(0.016)
gleich(:ui, interpreter.wait_kind, 'Textfenster blockiert den Interpreter')
ports.ui.busy = false
interpreter.update(0.016)
assert(!interpreter.running?, 'geschlossenes Textfenster gibt den Interpreter frei')

ports = test_ports.first
ports.transition.busy = true
interpreter, programm = ein_befehl('execute_transition', { 'transition_key' => 'transition/rune_fade' }, ports: ports)
interpreter.start(programm)
interpreter.update(0.016)
gleich(:transition, interpreter.wait_kind, 'Übergang blockiert den Interpreter')
ports.transition.busy = false
interpreter.update(0.016)
assert(!interpreter.running?, 'fertiger Übergang gibt den Interpreter frei')

# Ein Common Event mit Wartezeit bleibt sichtbar auf dem Aufrufstapel.
unterprogramm = Rpg::EventCommands::Models::Program.new(
  id: 71, name: 'wartendes_common_event',
  commands: [Rpg::EventCommands::Models::Command.new(commandType: 'wait', code: 106, indent: 0,
                                                     params: { 'seconds' => 2.0 })]
)
katalog_mit_unterprogramm = Rpg::EventCommands::Models::CatalogData.new(
  version: 1, definitions: KATALOG.definitions, resources: KATALOG.resources,
  programs: KATALOG.programs + [unterprogramm]
)
rufer = Rpg::EventCommands::Models::Program.new(
  id: 72, name: 'rufer',
  commands: [Rpg::EventCommands::Models::Command.new(commandType: 'call_common_event', code: 117, indent: 0,
                                                     params: { 'common_event_id' => 71 })]
)
interpreter = Rpg.build_event_interpreter(catalog: katalog_mit_unterprogramm, ports: test_ports.first)
interpreter.start(rufer)
interpreter.update(0.5)
gleich(1, interpreter.common_event_depth, 'Call Common Event öffnet einen Unterinterpreter')
gleich(2, interpreter.depth, 'Aufrufer und Unterprogramm liegen auf dem Stapel')
gleich(:common, interpreter.call_stack.last[:kind], 'oberster Frame ist das Common Event')
gleich(71, interpreter.call_stack.last[:common_event_id], 'Aufrufstapel nennt die Common-Event-ID')
gleich(1, interpreter.call_stack.first[:index], 'Aufrufer steht hinter seinem Aufrufbefehl')
3.times { interpreter.update(1.0) }
assert(!interpreter.running?, 'Unterinterpreter kehrt zum Aufrufer zurück')

# Rekursionsschutz: ein Common Event, das sich selbst aufruft.
selbst = Rpg::EventCommands::Models::Program.new(
  id: 70, name: 'rekursiv',
  commands: [Rpg::EventCommands::Models::Command.new(commandType: 'call_common_event', code: 117, indent: 0,
                                                     params: { 'common_event_id' => 70 })]
)
katalog_mit_rekursion = Rpg::EventCommands::Models::CatalogData.new(
  version: 1, definitions: KATALOG.definitions, resources: KATALOG.resources,
  programs: KATALOG.programs + [selbst]
)
ports, senke = test_ports
interpreter = Rpg.build_event_interpreter(catalog: katalog_mit_rekursion, ports: ports)
interpreter.start(selbst)
40.times { interpreter.update(1.0) }
assert(interpreter.common_event_depth <= Rpg::EventCommands::Interpreter::MAX_DEPTH,
       'Aufruftiefe bleibt begrenzt')
assert(senke.types.include?('common_event.recursion_limit'), 'Rekursionsgrenze wird gemeldet')

puts($fehler.zero? ? "Event-Command-Architektur: #{$pruefungen} Prüfungen, 0 Fehler"
                   : "Event-Command-Architektur: #{$pruefungen} Prüfungen, #{$fehler} Fehler")
exit($fehler.zero? ? 0 : 1)
