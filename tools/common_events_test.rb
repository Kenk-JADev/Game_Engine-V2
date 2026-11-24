#encoding: utf-8
# ============================================================================
#  Kopfloser Test für Gemeinsame Events (Etappe 20c, XP "Common Events").
#  Lässt sich ohne Engine starten:
#     ruby -I game/engine tools/common_events_test.rb
#  Exit-Code 0 = alles grün.
#
#  Geprüft wird:
#    - Database: common_events/common_event lesen den Abschnitt aus
#      database.json (Beispiel im Datenbaum vorhanden)
#    - Interpreter: Aufruf-Befehl common_event, unbekannter Name,
#      Aufrufketten bis zur Tiefenbegrenzung
#    - CommonEvents: Trigger none/autorun/parallel, Schalter-Freigabe,
#      tick startet genau ein Event pro Frame und nur bei freiem Interpreter
#    - MapCheck: scan_database (Namen, Trigger, Schalter, Befehle) und die
#      Namensprüfung im Aufruf-Befehl
# ============================================================================

# --- Minimal-Attrappe der Engine-Schnittstelle -----------------------------
module Rune
  ROOT = File.expand_path('..', __dir__)

  module Log
    def self.info(m) = nil
    def self.warn(m) = puts("WARN  #{m}")
    def self.error(m) = puts("ERROR #{m}")
  end
end

$LOAD_PATH.unshift(File.join(Rune::ROOT, 'game', 'engine'))
require 'data'
require 'state'
require 'issues'
require 'common_events'
require 'interpreter'
require 'mapcheck'

$fail = 0
def check(name)
  ok = yield
  puts(ok ? "  ok   #{name}" : "  FAIL #{name}")
  $fail += 1 unless ok
rescue StandardError => e
  puts "  FAIL #{name} (Ausnahme: #{e.class}: #{e.message})"
  $fail += 1
end

def lauf(interp, frames = 20, dt = 0.016)
  frames.times { interp.update(dt) }
  interp
end

GameState.reset!({})

puts '== Database: Abschnitt common_events =='
check('Beispiel-Event im Datenbaum vorhanden') do
  namen = Database.common_events.map { |e| e['name'] }
  namen.include?('hinweis_speichern')
end
check('common_event findet per Name') do
  e = Database.common_event('hinweis_speichern')
  e.is_a?(Hash) && e['commands'].is_a?(Array)
end
check('unbekannter Name -> nil') { Database.common_event('gibtsnicht').nil? }
check('leerer Name -> nil') { Database.common_event('').nil? }

# --- Testdatenbank: eigene Gemeinsame Events für die Laufzeitprüfungen -----
TESTDB = {
  'common_events' => [
    { 'id' => 1, 'name' => 'zuwenig', 'trigger' => 'none',
      'commands' => [{ 'cmd' => 'variable', 'name' => 'treffer', 'value' => 1 }] },
    { 'id' => 2, 'name' => 'ruft_zuwenig', 'trigger' => 'none',
      'commands' => [{ 'cmd' => 'common_event', 'name' => 'zuwenig' }] },
    { 'id' => 3, 'name' => 'endlos_a', 'trigger' => 'none',
      'commands' => [{ 'cmd' => 'common_event', 'name' => 'endlos_a' }] },
    { 'id' => 4, 'name' => 'auto_an', 'trigger' => 'autorun', 'switch' => 'auto',
      'commands' => [{ 'cmd' => 'variable', 'name' => 'auto_laeufe', 'add' => 1 }] },
    { 'id' => 5, 'name' => 'parallel_an', 'trigger' => 'parallel', 'switch' => 'auto',
      'commands' => [{ 'cmd' => 'variable', 'name' => 'parallel_laeufe', 'add' => 1 }] }
  ]
}.freeze

module Database
  def self.common_events
    TESTDB['common_events']
  end

  def self.common_event(name)
    ziel = name.to_s
    return nil if ziel.empty?

    common_events.find { |e| e['name'].to_s == ziel }
  end
end

puts '== Interpreter: Aufruf (XP 117) =='
interp = Interpreter.new
interp.start([{ 'cmd' => 'common_event', 'name' => 'zuwenig' },
              { 'cmd' => 'variable', 'name' => 'danach', 'value' => 1 }])
lauf(interp, 5)
check('Aufruf führt die Befehle des Gemeinsamen Events aus') do
  GameState.variable('treffer') == 1.0 && GameState.variable('danach') == 1.0 && !interp.running?
end

GameState.reset!({})
interp = Interpreter.new
interp.start([{ 'cmd' => 'common_event', 'name' => 'ruft_zuwenig' }])
lauf(interp, 5)
check('verschachtelter Aufruf läuft durch') do
  GameState.variable('treffer') == 1.0 && !interp.running?
end

interp = Interpreter.new
interp.start([{ 'cmd' => 'common_event', 'name' => 'fehlt' },
              { 'cmd' => 'variable', 'name' => 'nach_fehler', 'value' => 3 }])
lauf(interp, 5)
check('unbekanntes Gemeinsames Event: Warnung statt Absturz') do
  GameState.variable('nach_fehler') == 3.0 && !interp.running?
end

# Ein Rahmen wird pro Update höchstens einmal geschlossen (bestehendes
# Verhalten im Interpreter), daher hier großzügig Frames geben.
interp = Interpreter.new
interp.start([{ 'cmd' => 'common_event', 'name' => 'endlos_a' }])
lauf(interp, 60)
check('Selbstaufruf endet an der Tiefenbegrenzung (kein Hänger)') do
  !interp.running? && CommonEvents::MAX_TIEFE.positive?
end

puts '== CommonEvents: Trigger und Schalter =='
GameState.reset!({})
auto = TESTDB['common_events'].find { |e| e['name'] == 'auto_an' }
none = TESTDB['common_events'].find { |e| e['name'] == 'zuwenig' }
check('Trigger none läuft nicht von selbst') { !CommonEvents.aktiv?(none) }
check('autorun mit ausgeschaltetem Schalter: inaktiv') { !CommonEvents.aktiv?(auto) }
GameState.set_switch('auto', true)
check('autorun mit eingeschaltetem Schalter: aktiv') { CommonEvents.aktiv?(auto) }
check('unbekannter Trigger wird nicht gestartet') do
  !CommonEvents.aktiv?(auto.merge('trigger' => 'irgendwas'))
end

interp = Interpreter.new
check('tick startet das aktive Event') do
  CommonEvents.tick(interp) && interp.running?
end
interp2 = Interpreter.new
interp2.start([{ 'cmd' => 'wait', 'seconds' => 1.0 }])
check('tick startet nichts, solange der Interpreter läuft') { !CommonEvents.tick(interp2) }
GameState.set_switch('auto', false)
check('ausgeschalteter Schalter: tick startet nichts') { !CommonEvents.tick(Interpreter.new) }

puts '== MapCheck: scan_database =='
def funde(db)
  MapCheck.scan_database(db, name: 'database.json')
end

def hat?(liste, fragment, severity = nil)
  liste.any? do |f|
    f.to_s.include?(fragment) && (severity.nil? || f.severity == severity)
  end
end

check('saubere Datenbank: keine Funde') { funde(TESTDB).empty? }
check('doppelter Name -> Fehler') do
  db = { 'common_events' => [TESTDB['common_events'][0], TESTDB['common_events'][0]] }
  hat?(funde(db), 'ist doppelt', MapCheck::FEHLER)
end
check('unbekannter Trigger -> Fehler') do
  hat?(funde('common_events' => [{ 'name' => 'x', 'trigger' => 'manchmal' }]),
       "unbekannter Trigger 'manchmal'", MapCheck::FEHLER)
end
check('autorun ohne Schalter -> Warnung') do
  hat?(funde('common_events' => [{ 'name' => 'x', 'trigger' => 'autorun',
                                   'commands' => [{ 'cmd' => 'log', 'text' => 'a' }] }]),
       'ohne \'switch\'', MapCheck::WARNUNG)
end
check('ohne Befehle -> Warnung') do
  hat?(funde('common_events' => [{ 'name' => 'x', 'commands' => [] }]),
       'keine Befehle', MapCheck::WARNUNG)
end
check('fehlende Befehlsliste -> Fehler') do
  hat?(funde('common_events' => [{ 'name' => 'x' }]), "'commands' fehlt/keine Liste", MapCheck::FEHLER)
end
check('unbekannter Befehl im Gemeinsamen Event -> Fehler') do
  hat?(funde('common_events' => [{ 'name' => 'x', 'commands' => [{ 'cmd' => 'zauber' }] }]),
       "unbekannter Befehlstyp 'zauber'", MapCheck::FEHLER)
end
check('kein common_events-Abschnitt: keine Funde (alte Projekte)') do
  funde('gold' => 100).empty?
end

puts '== MapCheck: Aufruf-Befehl in der Karte =='
OPTS = { prefabs: %w[haus], materials: %w[weg], scripts: [],
         common_events: %w[zuwenig] }.freeze

def karte_mit(cmds)
  { 'name' => 'Testkarte',
    'terrain' => { 'width' => 120.0, 'height' => 90.0, 'resolution' => 64, 'amplitude' => 0.0 },
    'objects' => [],
    'events' => [{ 'id' => 1, 'name' => 'Aufruf', 'x' => 0.0, 'z' => 0.0, 'radius' => 2.5,
                   'pages' => [{ 'conditions' => {}, 'trigger' => 'action', 'commands' => cmds }] }] }
end

check('bekannter Name: keine Funde') do
  MapCheck.scan(karte_mit([{ 'cmd' => 'common_event', 'name' => 'zuwenig' }]), **OPTS).empty?
end
check('unbekannter Name -> Warnung') do
  hat?(MapCheck.scan(karte_mit([{ 'cmd' => 'common_event', 'name' => 'tippfehler' }]), **OPTS),
       'ist nicht definiert', MapCheck::WARNUNG)
end
check('ohne Namen -> Fehler') do
  hat?(MapCheck.scan(karte_mit([{ 'cmd' => 'common_event' }]), **OPTS),
       "'name' fehlt oder ist leer", MapCheck::FEHLER)
end
check('ohne Namensliste (alter Aufruf) wird nicht gemeckert') do
  MapCheck.scan(karte_mit([{ 'cmd' => 'common_event', 'name' => 'irgendwas' }]),
                prefabs: %w[haus]).empty?
end

puts($fail.zero? ? 'ALLE CHECKS GRUEN' : "#{$fail} CHECK(S) FEHLGESCHLAGEN")
exit($fail.zero? ? 0 : 1)
