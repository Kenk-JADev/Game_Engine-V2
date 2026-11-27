#encoding: utf-8
# ============================================================================
#  Kopfloser Test für Zustände und Werte von Helden (Etappe 24, XP 313/317/320).
#  Lässt sich ohne Engine starten:
#     ruby -I game/engine tools/zustand_test.rb
#  Exit-Code 0 = alles grün.
#
#  Geprüft wird:
#    - Party.state_def: Zustand per ID und per Name, Unbekanntes
#    - Party.change_state: add/remove, Zielwahl, Auffrischen, Grenze von 8
#    - Party.state_names, Actor#state?, heal_all entfernt Zustände
#    - Party.change_param: alle sechs Parameter, Klemmen, HP-Kappung
#    - Party.rename über den Interpreter (XP 320)
#    - Spielstand: Zustände und Parameter überleben snapshot/restore!
#    - Kampf: Battle.start übernimmt die Zustände, sync_actors! schreibt zurück
#    - Interpreter: state/actor_param/actor_name inkl. Fehlerfälle
#    - MapCheck: Pflichtfelder, Modus, Parameter, Actor-Ziel
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
require 'party'
require 'battle'
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

DB = {
  'gold' => 10,
  'actors' => [
    { 'id' => 'arik', 'name' => 'Arik', 'level' => 1, 'hp' => 100, 'mp' => 20 },
    { 'id' => 'mira', 'name' => 'Mira', 'level' => 1, 'hp' => 80, 'mp' => 40 }
  ],
  'items' => [],
  'states' => [
    { 'id' => 'gift', 'name' => 'Gift', 'kind' => 'poison', 'power' => 5, 'turns' => 3 },
    { 'id' => 'schutz', 'name' => 'Schutz', 'kind' => 'def_up', 'power' => 4, 'turns' => 5 },
    { 'id' => 'wut', 'name' => 'Wut', 'kind' => 'atk_up', 'power' => 6, 'turns' => 2 },
    { 'id' => 's1', 'name' => 'S1', 'kind' => 'atk_up', 'power' => 1 },
    { 'id' => 's2', 'name' => 'S2', 'kind' => 'atk_up', 'power' => 1 },
    { 'id' => 's3', 'name' => 'S3', 'kind' => 'atk_up', 'power' => 1 },
    { 'id' => 's4', 'name' => 'S4', 'kind' => 'atk_up', 'power' => 1 },
    { 'id' => 's5', 'name' => 'S5', 'kind' => 'atk_up', 'power' => 1 },
    { 'id' => 's6', 'name' => 'S6', 'kind' => 'atk_up', 'power' => 1 }
  ],
  'enemies' => [
    { 'id' => 'slime', 'name' => 'Schleim', 'hp' => 20, 'atk' => 5, 'def' => 2,
      'agi' => 3, 'spi' => 1, 'exp' => 4, 'gold' => 3 }
  ],
  'troops' => [
    { 'id' => 'truppe_test', 'name' => 'Testtruppe', 'enemies' => ['slime'] }
  ]
}.freeze

module Database
  def self.database = DB
end

def neu!
  Party.setup(DB)
  Battle.abort! if Battle.active?
rescue StandardError
  nil
end

def arik = Party.find('arik')
def mira = Party.find('mira')
def ids(actor) = Party.states(actor).map { |s| s[:id] }

def lauf(interp, frames = 3, dt = 0.016)
  frames.times { interp.update(dt) }
  interp
end

puts '== Zustands-Definitionen =='
neu!
check('Zustand per ID und per Name, Schreibweise egal') do
  Party.state_def('gift')['name'] == 'Gift' && Party.state_def('Gift')['id'] == 'gift' &&
    Party.state_def('SCHUTZ')['id'] == 'schutz'
end
check('unbekannter Zustand: nil') do
  Party.state_def('gibtsnicht').nil? && Party.state_def('').nil? && Party.state_def(nil).nil?
end

puts '== Zustände ändern (XP 313) =='
neu!
check('add auf "alle" setzt bei beiden Helden den Zustand') do
  Party.change_state('alle', 'gift', mode: 'add') == 2 && arik.state?('gift') && mira.state?('gift')
end
check('state_names zeigt den Datenbanknamen') do
  Party.state_names(arik) == ['Gift'] && arik.state_names == ['Gift']
end
check('Zustand nur für einen Helden') do
  Party.change_state('arik', 'schutz', mode: 'add') == 1 && arik.state?('schutz') &&
    !mira.state?('schutz')
end
check('add frischt die Dauer auf, kürzt sie aber nicht') do
  Party.change_state('arik', 'wut', mode: 'add', turns: 5) && arik.states.find { |s| s[:id] == 'wut' }[:turns] == 5 &&
    Party.change_state('arik', 'wut', mode: 'add', turns: 1) && arik.states.find { |s| s[:id] == 'wut' }[:turns] == 5
end
check('remove trifft nur den genannten Zustand und Helden') do
  Party.change_state('mira', 'gift', mode: 'remove') == 1 && !mira.state?('gift') && arik.state?('gift')
end
check('remove ohne Treffer zählt 0') { Party.change_state('mira', 'gift', mode: 'remove').zero? }
check('unbekannter Zustand: :unbekannt, nichts geändert') do
  vorher = ids(arik)
  Party.change_state('arik', 'gibtsnicht', mode: 'add') == :unbekannt && ids(arik) == vorher
end
check('unbekanntes Ziel: 0') { Party.change_state('niemand', 'gift', mode: 'add').zero? }
neu!
check('mehr als 8 Zustände werden abgelehnt') do
  (1..6).each { |i| Party.change_state('arik', "s#{i}", mode: 'add') }
  %w[gift schutz wut].each { |id| Party.change_state('arik', id, mode: 'add') }
  ids(arik).size == 8 && Party.change_state('arik', 'wut', mode: 'add') == :voll
end
neu!
check('clear_states leert die Liste') do
  Party.change_state('alle', 'gift', mode: 'add')
  Party.clear_states == 2 && ids(arik).empty? && ids(mira).empty?
end
neu!
check('vollständiges Heilen entfernt Zustände (XP 314 recover_all)') do
  Party.change_state('alle', 'gift', mode: 'add')
  Party.heal_all && ids(arik).empty?
end
neu!
check('Teilheilung lässt Zustände stehen') do
  arik.hp = 50
  Party.change_state('arik', 'gift', mode: 'add')
  Party.heal_all(10) && arik.hp == 60 && arik.state?('gift')
end

puts '== Parameter ändern (XP 317) =='
neu!
check('max_hp +10 hebt nur das Maximum') do
  vorher = arik.max_hp
  Party.change_param('arik', 'max_hp', 10) == 1 && arik.max_hp == vorher + 10 && arik.hp == vorher
end
check('sinkendes Maximum kappt die HP') do
  arik.hp = arik.max_hp
  Party.change_param('arik', 'max_hp', -20) && arik.hp == arik.max_hp
end
check('max_hp fällt nicht unter 1') do
  Party.change_param('arik', 'max_hp', -9999) && arik.max_hp == 1 && arik.hp == 1
end
check('atk/def/spi/agi auf alle') do
  Party.change_param('alle', 'atk', 5) == 2 && arik.atk == 15 && mira.atk == 15 &&
    Party.change_param('alle', 'agi', -1000) && arik.agi.zero? && mira.agi.zero?
end
check('unbekannter Parameter: :unbekannt') do
  Party.change_param('arik', 'level', 5) == :unbekannt && arik.level == 1
end
check('MapCheck und Party kennen dieselben Parameter') { MapCheck::PARAMETER == Party::PARAMETER }

puts '== Name ändern (XP 320) und Spielstand =='
neu!
GameState.reset!({})
i = Interpreter.new
i.start([{ 'cmd' => 'state', 'actor' => 'arik', 'name' => 'Gift' },
         { 'cmd' => 'actor_param', 'actor' => 'arik', 'param' => 'max_hp', 'amount' => 20 },
         { 'cmd' => 'actor_name', 'actor' => 'arik', 'name' => 'Arik der Kuehne' }])
lauf(i)
check('Interpreter setzt Zustand, Parameter und Namen') do
  arik.state?('gift') && arik.max_hp == 120 && arik.name == 'Arik der Kuehne'
end
snap = Party.snapshot
neu!
check('setup nach Neustart kennt keine Zustände') { ids(arik).empty? && arik.name == 'Arik' }
Party.restore!(snap)
check('restore! holt Zustand, Parameter und Namen zurück') do
  arik.state?('gift') && arik.max_hp == 120 && arik.name == 'Arik der Kuehne'
end

puts '== Kampf-Übergabe =='
neu!
Party.change_state('arik', 'gift', mode: 'add')
Party.change_state('arik', 'schutz', mode: 'add', turns: 2)
check('Battle.start übernimmt die Zustände der Party') do
  Battle.start('truppe_test') && Battle.actors.first.state?('gift') &&
    Battle.actors.first.states.find { |s| s[:id] == 'schutz' }[:turns] == 2
end
check('Zustand ohne turns bekommt die Datenbank-Dauer') do
  Battle.actors.first.states.find { |s| s[:id] == 'gift' }[:turns] == 3
end
check('sync_actors! schreibt die Zustände zurück') do
  Battle.actors.first.states.reject! { |s| s[:id] == 'gift' }
  Battle.sync_actors!
  !arik.state?('gift') && arik.state?('schutz')
end
Battle.abort!

puts '== Interpreter-Fehlerfälle =='
neu!
GameState.reset!({})
Issues.all.clear
i = Interpreter.new
i.start([{ 'cmd' => 'state', 'actor' => 'alle', 'name' => 'gibtsnicht' },
         { 'cmd' => 'state', 'actor' => 'alle', 'name' => 'gift', 'mode' => 'kippen' },
         { 'cmd' => 'state', 'actor' => 'alle' },
         { 'cmd' => 'actor_param', 'actor' => 'alle', 'param' => 'level', 'amount' => 1 },
         { 'cmd' => 'actor_param', 'actor' => 'alle', 'amount' => 1 },
         { 'cmd' => 'actor_name', 'actor' => 'alle', 'name' => 'Alle' },
         { 'cmd' => 'actor_name', 'actor' => 'arik' },
         { 'cmd' => 'variable', 'name' => 'weiter', 'value' => 1 }])
lauf(i)
check('Fehlerfälle landen im Issue-Backlog') do
  Issues.all.size >= 6 &&
    Issues.all.any? { |f| f.to_s.include?("state 'gibtsnicht'") } &&
    Issues.all.any? { |f| f.to_s.include?("state-Modus 'kippen'") } &&
    Issues.all.any? { |f| f.to_s.include?('state ohne name') } &&
    Issues.all.any? { |f| f.to_s.include?("actor_param 'level'") } &&
    Issues.all.any? { |f| f.to_s.include?('actor_param ohne param') } &&
    Issues.all.any? { |f| f.to_s.include?('actor_name braucht genau einen Helden') } &&
    Issues.all.any? { |f| f.to_s.include?('actor_name ohne name') }
end
check('nach den Fehlern läuft das Event weiter') { GameState.variable('weiter') == 1.0 }
check('"alle" hat nichts umbenannt') { arik.name == 'Arik' && mira.name == 'Mira' && ids(arik).empty? }

puts '== MapCheck =='
OPTS = { prefabs: %w[haus], materials: %w[weg], scripts: [] }.freeze

def karte_mit(cmds)
  { 'name' => 'Testkarte',
    'terrain' => { 'width' => 120.0, 'height' => 90.0, 'resolution' => 64, 'amplitude' => 0.0 },
    'objects' => [],
    'events' => [{ 'id' => 1, 'name' => 'Zustände', 'x' => 0.0, 'z' => 0.0, 'radius' => 2.5,
                   'pages' => [{ 'conditions' => {}, 'trigger' => 'action', 'commands' => cmds }] }] }
end

def hat?(funde, fragment, severity = nil)
  funde.any? { |f| f.to_s.include?(fragment) && (severity.nil? || f.severity == severity) }
end

check('gültige Befehle: keine Funde') do
  MapCheck.scan(karte_mit([
                            { 'cmd' => 'state', 'actor' => 'alle', 'name' => 'gift', 'mode' => 'add' },
                            { 'cmd' => 'state', 'actor' => 'Mira', 'name' => 'Schutz', 'mode' => 'remove',
                              'turns' => 2 },
                            { 'cmd' => 'actor_param', 'actor' => 'arik', 'param' => 'max_hp', 'amount' => 10 },
                            { 'cmd' => 'actor_name', 'actor' => 'arik', 'name' => 'Arik der Kuehne' }
                          ]), **OPTS).empty?
end
check('state ohne name -> Fehler') do
  hat?(MapCheck.scan(karte_mit([{ 'cmd' => 'state', 'actor' => 'alle' }]), **OPTS),
       "'name' (Zustand) fehlt oder ist leer", MapCheck::FEHLER)
end
check('state mit falschem Modus -> Fehler') do
  hat?(MapCheck.scan(karte_mit([{ 'cmd' => 'state', 'name' => 'gift', 'mode' => 'kippen' }]), **OPTS),
       "unbekannter state-Modus 'kippen'", MapCheck::FEHLER)
end
check('unbekannter Zustand -> Warnung') do
  hat?(MapCheck.scan(karte_mit([{ 'cmd' => 'state', 'name' => 'gibtsnicht' }]), **OPTS),
       "Zustand 'gibtsnicht' nicht in database.json", MapCheck::WARNUNG)
end
check('state mit turns 0 -> Fehler') do
  hat?(MapCheck.scan(karte_mit([{ 'cmd' => 'state', 'name' => 'gift', 'turns' => 0 }]), **OPTS),
       "'turns' muss eine Zahl > 0 sein", MapCheck::FEHLER)
end
check('unbekannter Actor beim Zustand -> Warnung') do
  hat?(MapCheck.scan(karte_mit([{ 'cmd' => 'state', 'actor' => 'niemand', 'name' => 'gift' }]), **OPTS),
       "Actor 'niemand' nicht in database.json", MapCheck::WARNUNG)
end
check('actor_param ohne param -> Fehler') do
  hat?(MapCheck.scan(karte_mit([{ 'cmd' => 'actor_param', 'amount' => 5 }]), **OPTS),
       "'param' fehlt", MapCheck::FEHLER)
end
check('actor_param mit unbekanntem Parameter -> Fehler') do
  hat?(MapCheck.scan(karte_mit([{ 'cmd' => 'actor_param', 'param' => 'level', 'amount' => 1 }]), **OPTS),
       "unbekannter Parameter 'level'", MapCheck::FEHLER)
end
check('actor_param ohne Menge -> Fehler') do
  hat?(MapCheck.scan(karte_mit([{ 'cmd' => 'actor_param', 'param' => 'atk' }]), **OPTS),
       "'amount' fehlt oder ist keine Zahl", MapCheck::FEHLER)
end
check('actor_param mit Menge 0 -> Warnung') do
  hat?(MapCheck.scan(karte_mit([{ 'cmd' => 'actor_param', 'param' => 'atk', 'amount' => 0 }]), **OPTS),
       "'amount' ist 0", MapCheck::WARNUNG)
end
check('actor_name ohne Namen -> Fehler') do
  hat?(MapCheck.scan(karte_mit([{ 'cmd' => 'actor_name', 'actor' => 'arik' }]), **OPTS),
       "'name' fehlt oder ist leer", MapCheck::FEHLER)
end
check('actor_name mit "alle" -> Fehler') do
  hat?(MapCheck.scan(karte_mit([{ 'cmd' => 'actor_name', 'actor' => 'alle', 'name' => 'X' }]), **OPTS),
       "'actor' muss genau einen Helden nennen", MapCheck::FEHLER)
end
check('alle 73 Befehlstypen im Register der Prüfung') { MapCheck::CMD_TYPES.size == 73 }

puts($fail.zero? ? 'ALLE CHECKS GRUEN' : "#{$fail} CHECK(S) FEHLGESCHLAGEN")
exit($fail.zero? ? 0 : 1)
