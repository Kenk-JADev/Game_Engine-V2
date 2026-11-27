#encoding: utf-8
# ============================================================================
#  Tests für die modulare RPG-Datenbank (Etappe 40).
#
#  Kopf los: ruby -I game/engine tools/rpg_database_test.rb
#  Prüft Loader/Validierung, Unveränderlichkeit, Runtime-Instanzen, Services
#  und die Entkopplung über Audio-/Event-Ports.
# ============================================================================

require 'json'
require 'rpg'

ROOT = File.expand_path('..', __dir__)
PFAD = File.join(ROOT, 'game', 'config', 'rpg_database.json')
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
  unless erwartet == tatsaechlich
    raise "FAIL: #{text} (erwartet #{erwartet.inspect}, erhalten #{tatsaechlich.inspect})"
  end

  ok(text)
end

def kopie
  Marshal.load(Marshal.dump(GRUNDDOKUMENT))
end

GRUNDDOKUMENT = JSON.parse(File.read(PFAD, encoding: 'UTF-8'))

datenbank = Rpg.load_database(path: PFAD)
gleich(13, Rpg::Loader::AREA_KEYS.size + 1, '12 Listenbereiche plus System ergeben 13 Bereiche')
Rpg::Loader::AREA_KEYS.each do |bereich|
  assert(datenbank[bereich].is_a?(Rpg::Repositories::Repository), "#{bereich} hat ein Repository")
  assert(datenbank[bereich].all.any?, "#{bereich} enthält Beispieldaten")
end
gleich([101, 104, 109], datenbank.system.initial_party, 'Startparty aus der Systemdatei')
gleich(6, datenbank.elements.all.size, 'Elementkatalog hat sechs eigene Einträge')
gleich('Pyro', datenbank.elements.find(1).name, 'Element 1 heisst Pyro')

# --- Unveränderlichkeit + Resize -------------------------------------------
kael_vorlage = datenbank.actors.find(101)
assert(kael_vorlage.frozen?, 'Actor-Vorlage ist eingefroren')
assert(kael_vorlage.growth.frozen?, 'Wachstumskurven sind eingefroren')
assert(kael_vorlage.start_equipment.frozen?, 'Startausrüstung ist eingefroren')
assert(datenbank.actors.all.frozen?, 'Repository-Liste ist eingefroren')

begin
  datenbank.actors.all << :fremd
  assert(false, 'Repository-Liste darf nicht veränderbar sein')
rescue FrozenError
  ok('Repository-Liste verweigert Mutation')
end

vergroessert = datenbank.resize(:actors, 5)
gleich(3, datenbank.actors.size, 'Original-Repository bleibt bei 3 Slots')
gleich(5, vergroessert.actors.size, 'Resize erzeugt 5 Slots')
gleich(3, vergroessert.actors.all.size, 'leere Slots erscheinen nicht in all')
gleich(101, vergroessert.actors.find(101).id, 'Resize behält vorhandene Einträge')
begin
  datenbank.resize(:actors, 0)
  assert(false, 'Resize ausserhalb 1..999 muss scheitern')
rescue ArgumentError
  ok('Resize-Grenzen werden geprüft')
end

# --- Runtime-Instanzen ------------------------------------------------------
audio_port = Rpg::Ports::RecordingAudio.new
event_sink = Rpg::Ports::MemoryEventSink.new
services = Rpg.build_services(database: datenbank,
                              audio_port: audio_port,
                              dispatcher: Rpg::Ports::EventDispatcher.new(sink: event_sink),
                              random: ->(minimum, maximum) {
                                minimum == 0.0 && maximum == 100.0 ? 50.0 : maximum
                              })
fabrik = services[:runtime]
kael = fabrik.actor(101)
assert(kael.is_a?(Rpg::RuntimeActor), 'Actor-Factory liefert RuntimeActor')
gleich(46, kael.maxhp, 'Start-MaxHP aus der Wachstumskurve')
gleich(46, kael.hp, 'RuntimeActor startet mit vollem HP')
kael.hp = 10
gleich(46, kael_vorlage.growth[:maxhp].base, 'Template bleibt trotz HP-Änderung unverändert')
assert(kael.equip('shield', 502) == false, 'Fixed-Slot verweigert Ausrüstungswechsel')
assert(kael.equip('weapon', 404), 'freier Slot erlaubt Ausrüstungswechsel')
gleich(401, kael_vorlage.start_equipment.first.item_id, 'Template-Ausrüstung bleibt unverändert')

gegner = fabrik.enemy(701)
assert(gegner.is_a?(Rpg::RuntimeEnemy), 'Enemy-Factory liefert RuntimeEnemy')
gleich(420, gegner.maxhp, 'Gegnerwerte stammen aus der Vorlage')
gegner.hp = 1
gleich(420, gegner.template.maxhp, 'Enemy-Vorlage bleibt unverändert')
gleich(3, fabrik.party.size, 'Startparty erzeugt drei RuntimeActoren')

zustand = Rpg::ActiveState.new(state_id: 3, remaining_turns: 2, remaining_steps: 0)
assert(zustand.frozen?, 'ActiveState ist unveränderlich')
gleich(1, zustand.advance(turns: 1).remaining_turns, 'Tick erzeugt neuen ActiveState')

kachel = fabrik.tile(1001, 2)
assert(kachel.is_a?(Rpg::MapTileState), 'Tile-Factory liefert MapTileState')
assert(kachel.counter?, 'Kachel 2 hat ursprünglich Counter-Flag')
kachel.passable = false
assert(!kachel.passable?, 'Laufzeit-Kachel erlaubt Override')
kachel.reset!
assert(kachel.passable?, 'Reset stellt Template-Wert wieder her')
assert(datenbank.tilesets.find(1001).tile_properties.find { |p| p.index == 2 }.passable,
       'Tileset-Vorlage bleibt unverändert')

# --- Services ---------------------------------------------------------------
ausruestung = services[:equipment_aggregator].stats(actor: kael)
# Kael trägt jetzt Waffe 404; daher ATK 15 statt 24.
gleich(15, ausruestung[:atk], 'Waffen-ATK wird addiert')
gleich(34, ausruestung[:pdef], 'Rüstungs-PDEF wird addiert')
gleich(5, ausruestung[:str], 'Ausrüstungs-STR-Boni werden addiert')

zustände = services[:state_resolver]
zustände.apply(kael, 3, turns: 5)
gleich(1.35, zustände.multipliers(kael)[:pdef].round(2), 'Zustand erhöht PDEF um 35 %')
gleich(1.15, zustände.element_efficiency(kael, 1).round(2), 'Klassen-Elementeffizienz bleibt aktiv')
zustände.tick(kael)
gleich(4, kael.states.first.remaining_turns, 'Zustand tickt um eine Runde')
5.times { zustände.tick(kael) }
assert(kael.states.empty?, 'Zustand verfällt nach Ablauf der Runden')

formeln = services[:combat_formulas]
skill = datenbank.skills.find(201)
schaden = formeln.damage(skill: skill, attacker: kael, target: gegner)
assert(schaden[:hit], 'Trefferquote wird ausgewertet')
assert(schaden[:damage].positive?, 'Schaden ist grösser als 0')
gleich(0, formeln.exp_for_level(datenbank.actors.find(101), 1), 'Stufe 1 beginnt bei 0 EXP')
exp10 = formeln.exp_for_level(datenbank.actors.find(101), 10)
assert(exp10.positive?, 'EXP-Kurve wächst mit der Stufe')
gleich(10, formeln.level_for_exp(datenbank.actors.find(101), exp10), 'Stufe aus EXP wird zurückgerechnet')

tiles = services[:tile_query]
assert(!tiles.passable?(1001, 1), 'Tileset-Block ist nicht passierbar')
assert(!tiles.passable?(1001, 2, direction: 'left'), '4-Richtungs-Passierbarkeit wird geprüft')
gleich(1, tiles.priority(1001, 2), 'Tile-Priorität wird abgefragt')
assert(tiles.bush?(1001, 3), 'Busch-Flag wird abgefragt')
assert(tiles.counter?(1001, 2), 'Counter-Flag wird abgefragt')
gleich(3, tiles.terrain_tag(1001, 3), 'Terrain-Tag wird abgefragt')

audio = services[:audio]
assert(audio.play(:jump), 'Audio-Cue wird aufgelöst')
gleich(['se/jump_soft'], audio_port.calls, 'AudioPort erhält den Asset-Key')
assert(!audio.play(:gibts_nicht), 'fehlender Cue wird gemeldet')
gleich(:se, audio.kind_for('se/menu_confirm'), 'Audio-Kind wird aus dem Key abgeleitet')

ereignisse = services[:event_execution]
ereignisse.execute(datenbank.troops.find(801).events.first.commands)
assert(event_sink.types.include?('switch.changed'), 'Event-Befehl 121 wird ausgeführt')
assert(event_sink.types.include?('message.shown'), 'Event-Befehl 101 wird ausgeführt')
ereignisse.run_common_event(1101)
assert(event_sink.types.include?('common_event.started'), 'Common Event wird gestartet')
assert(event_sink.types.include?('variable.changed'), 'Common Event ändert Variablen')
assert(event_sink.types.include?('common_event.finished'), 'Common Event wird beendet')
ereignisse.register(999) { |params, _context, _service, _depth| params['wert'] }
gleich([7], ereignisse.execute([Rpg::Models::EventCommand.new(code: 999, params: { 'wert' => 7 })]),
       'eigene Command-Handler sind injizierbar')
ereignisse.execute([Rpg::Models::EventCommand.new(code: 998, params: {})])
assert(event_sink.types.include?('command.unhandled'), 'unbekannte Befehle werden gemeldet')

# --- Validierung (negative Fälle) ------------------------------------------
def expect_error(dokument, text)
  lader = Rpg::Loader.new(file_system: Rpg::Loader::MemoryFileSystem.new(
    { 'test.json' => dokument }
  ))
  begin
    lader.load('test.json')
    raise "FAIL: erwartete Validierung '#{text}'"
  rescue Rpg::Support::ValidationError => e
    raise "FAIL: Fehlermeldung enthält nicht '#{text}': #{e.message.lines.first}" unless e.message.include?(text)

    ok("Validierung meldet: #{text}")
  end
end

dokument = kopie
dokument.delete('skills')
expect_error(dokument, 'Pflichtbereich')

dokument = kopie
dokument['actors'][1]['id'] = 101
expect_error(dokument, 'doppelte ID')

dokument = kopie
dokument['actors'][0]['class_id'] = 9999
expect_error(dokument, 'Klasse 9999 existiert nicht')

dokument = kopie
dokument['skills'][0]['icon_key'] = 'icon/unbekannt'
expect_error(dokument, 'unbekannter Asset-Key')

dokument = kopie
dokument['skills'][0]['scope'] = 99
expect_error(dokument, 'darf höchstens 7 sein')

dokument = kopie
dokument['animations'][0]['frame_max'] = 1
expect_error(dokument, 'Frame-Anzahl')

reporter = Rpg::Loader::CollectingReporter.new
begin
  Rpg::Loader.new(file_system: Rpg::Loader::MemoryFileSystem.new(
    { 'test.json' => kopie.tap { |d| d['actors'][0]['class_id'] = 9999 } }
  ), reporter: reporter).load('test.json')
rescue Rpg::Support::ValidationError
  nil
end
assert(reporter.issues.any?, 'Reporter erhält alle Validierungsbefunde')

puts "RPG-Datenbank: #{$pruefungen} Prüfungen, 0 Fehler"
