#encoding: utf-8
# ============================================================================
#  Kopfloser Test für die Namenseingabe (Etappe 20e, XP 303).
#  Lässt sich ohne Engine starten:
#     ruby -I game/engine tools/name_input_test.rb
#  Exit-Code 0 = alles grün.
#
#  Geprüft wird:
#    - NameInput.start: Held aus dem Katalog (auch Nicht-Mitglieder), max
#      geklemmt (1..16), cancel-Flag, unbekannter Held
#    - buchstabe/backspace: erlaubte Zeichen, Längengrenze, Löschen
#    - bestaetigen: leerer Name wird abgelehnt, sonst Party.rename
#    - abbruch: Esc nur bei cancel, sonst Meldung und Fenster bleibt
#    - Spielstand: umbenannter Held behält den Namen nach restore!
#    - Interpreter: Befehl name_input blockiert, Liste läuft danach weiter
#    - MapCheck: actor Pflicht/bekannt, max 1..16, cancel bool
#    - UI.name_input: zeichnet Name/Zähler und nimmt Tasten an
# ============================================================================

# --- Minimal-Attrappe der Engine-Schnittstelle -----------------------------
module Rune
  ROOT = File.expand_path('..', __dir__)

  module Log
    def self.info(m) = nil
    def self.warn(m) = puts("WARN  #{m}")
    def self.error(m) = puts("ERROR #{m}")
  end

  module Window
    def self.width = 1280
    def self.height = 720
  end

  module Input
    @gedrueckt = []
    @gehalten = []
    def self.setze!(keys, gehalten: [])
      @gedrueckt = Array(keys)
      @gehalten = Array(gehalten)
    end
    def self.pressed?(key) = @gedrueckt.include?(key)
    def self.down?(key) = @gehalten.include?(key) || @gedrueckt.include?(key)
  end

  module Gfx
    @texte = []
    def self.text(*args) = (@texte << args)
    def self.rect(*args) = nil
    def self.reset! = (@texte = [])
    def self.gesammelt = @texte.map { |t| t[2].to_s }
  end
end

$LOAD_PATH.unshift(File.join(Rune::ROOT, 'game', 'engine'))
require 'data'
require 'state'
require 'issues'
require 'party'
require 'name_input'
require 'ui'
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
  'gold' => 0,
  'actors' => [
    { 'id' => 'arik', 'name' => 'Arik', 'level' => 1, 'hp' => 100, 'mp' => 20 },
    { 'id' => 'mira', 'name' => 'Mira', 'level' => 1, 'hp' => 80, 'mp' => 40 }
  ],
  'items' => []
}.freeze

module Database
  def self.database = DB
end

def neu!
  Party.setup(DB)
  NameInput.close
  Issues.all.clear
end

def tippe(text)
  text.each_char { |c| NameInput.buchstabe(c) }
end

def lauf(interp, frames = 3, dt = 0.016)
  frames.times { interp.update(dt) }
  interp
end

puts '== Start =='
neu!
check('start übernimmt den bisherigen Namen und den Helden') do
  NameInput.start('arik') && NameInput.active? && NameInput.text == 'Arik' &&
    NameInput.max == 8 && NameInput.cancel?
end
check('max wird auf 1..16 geklemmt') do
  NameInput.start('arik', max: 99) && NameInput.max == 16 &&
    NameInput.start('arik', max: 0) && NameInput.max == 1
end
check('cancel: false wird gemerkt') do
  NameInput.start('arik', cancel: false) && !NameInput.cancel?
end
check('unbekannter Held öffnet nichts und landet im Backlog') do
  NameInput.close
  NameInput.start('gibtsnicht') == false && !NameInput.active? &&
    Issues.all.any? { |e| e['message'].include?('gibtsnicht') }
end
check('Nicht-Mitglieder dürfen umbenannt werden (XP $game_actors)') do
  neu!
  Party.member_remove('mira')
  NameInput.start('mira') && NameInput.actor_id == 'mira'
end

puts '== Tippen =='
neu!
NameInput.start('arik', max: 5)
check('buchstabe hängt an, backspace löscht') do
  NameInput.buchstabe('K') && NameInput.buchstabe('ai') == false && NameInput.text == 'ArikK' &&
    NameInput.backspace && NameInput.text == 'Arik'
end
NameInput.close
NameInput.start('arik', max: 6)
check('Längengrenze zählt Zeichen') do
  tippe('12')
  NameInput.text == 'Arik12' && NameInput.buchstabe('3') == false && NameInput.text.size == 6
end
check('erlaubte Zeichen: A-Z, a-z, 0-9, Leerzeichen') do
  NameInput.close
  NameInput.start('arik')
  NameInput.leeren
  NameInput.buchstabe('A') && NameInput.buchstabe('z') && NameInput.buchstabe('7') &&
    NameInput.buchstabe(' ') && NameInput.buchstabe('-') == false &&
    NameInput.buchstabe('ü') == false && NameInput.text == 'Az7 '
end

puts '== Übernehmen / Abbrechen =='
neu!
NameInput.start('arik')
check('leerer Name wird abgelehnt') do
  NameInput.leeren
  NameInput.bestaetigen == false && NameInput.meldung.include?('Namen') && NameInput.active?
end
NameInput.close
NameInput.start('arik', max: 12)
check('bestaetigen schreibt den Namen und schließt') do
  NameInput.leeren
  tippe('Kaito')
  NameInput.bestaetigen == true && !NameInput.active? &&
    Party.find('arik').name == 'Kaito' && Party.held('arik').name == 'Kaito'
end
check('Abbruch lässt den alten Namen stehen') do
  NameInput.start('arik')
  tippe('X')
  NameInput.abbruch == true && !NameInput.active? && Party.find('arik').name == 'Kaito'
end
check('Abbruch gesperrt: Fenster bleibt offen und meldet') do
  NameInput.start('arik', cancel: false)
  NameInput.abbruch == false && NameInput.active? && NameInput.meldung.include?('gesperrt')
  NameInput.close
end

puts '== Spielstand =='
neu!
Party.rename('arik', 'Kaito')
snap = Party.snapshot
neu!
Party.restore!(snap)
check('um-benannter Held behält den Namen nach dem Laden') do
  Party.find('arik').name == 'Kaito'
end
check('alter Spielstand ohne name behält den Datenbanknamen') do
  neu!
  Party.restore!({ 'gold' => 0, 'items' => {}, 'actors' => [{ 'id' => 'arik', 'hp' => 50 }] })
  Party.find('arik').name == 'Arik'
end

puts '== Interpreter: name_input =='
neu!
i = Interpreter.new
i.start([{ 'cmd' => 'name_input', 'actor' => 'arik', 'max' => 5 },
         { 'cmd' => 'variable', 'name' => 'danach', 'value' => 1 }])
lauf(i)
check('name_input öffnet die Eingabe und blockiert') do
  NameInput.active? && i.running? && GameState.variable('danach').zero?
end
NameInput.leeren
tippe('Rune')
NameInput.bestaetigen
lauf(i)
check('nach dem Übernehmen läuft die Liste weiter') do
  GameState.variable('danach') == 1.0 && !i.running? && Party.find('arik').name == 'Rune'
end
i = Interpreter.new
i.start([{ 'cmd' => 'name_input', 'actor' => 'gibtsnicht' },
         { 'cmd' => 'variable', 'name' => 'unbekannt', 'value' => 2 }])
lauf(i)
check('unbekannter Held blockiert nicht') do
  GameState.variable('unbekannt') == 2.0 && !NameInput.active?
end

puts '== MapCheck =='
OPTS = { prefabs: [], materials: [], scripts: [] }.freeze

def karte_mit(befehle)
  {
    'name' => 'Testkarte',
    'terrain' => { 'width' => 60.0, 'height' => 60.0, 'resolution' => 64,
                   'heights' => [], 'splats' => [] },
    'objects' => [], 'npcs' => [],
    'fog' => { 'on' => false, 'far' => 100.0, 'color' => 0xE8E8E8 },
    'weather' => { 'type' => 'kein', 'intensity' => 1.0 },
    'events' => [{ 'id' => 1, 'name' => 'Test', 'x' => 0.0, 'z' => 0.0, 'radius' => 2.5,
                   'pages' => [{ 'conditions' => {}, 'trigger' => 'action',
                                 'commands' => Array(befehle) }] }]
  }
end

def hat?(funde, text, schwere)
  funde.any? { |f| f.severity == schwere && f.message.include?(text) }
end

check('gültige Namenseingabe: keine Funde') do
  MapCheck.scan(karte_mit([{ 'cmd' => 'name_input', 'actor' => 'arik',
                             'max' => 8, 'cancel' => true }]), **OPTS).empty?
end
check('actor fehlt -> Fehler') do
  hat?(MapCheck.scan(karte_mit([{ 'cmd' => 'name_input' }]), **OPTS),
       "'actor' fehlt oder ist leer", MapCheck::FEHLER)
end
check('unbekannter Actor -> Warnung') do
  hat?(MapCheck.scan(karte_mit([{ 'cmd' => 'name_input', 'actor' => 'zeta' }]), **OPTS),
       "Actor 'zeta' nicht in database.json", MapCheck::WARNUNG)
end
check('max außerhalb 1..16 -> Fehler') do
  hat?(MapCheck.scan(karte_mit([{ 'cmd' => 'name_input', 'actor' => 'arik', 'max' => 0 }]), **OPTS),
       "'max' muss eine Zahl von 1 bis 16 sein", MapCheck::FEHLER)
end
check('cancel kein Wahrheitswert -> Warnung') do
  hat?(MapCheck.scan(karte_mit([{ 'cmd' => 'name_input', 'actor' => 'arik',
                                  'cancel' => 'ja' }]), **OPTS),
       "'cancel' sollte true oder false sein", MapCheck::WARNUNG)
end

puts '== Fenster (UI.name_input) =='
neu!
NameInput.start('arik', max: 8)
check('UI.name_input zeichnet Held, Text und Zähler') do
  Rune::Gfx.reset!
  Rune::Input.setze!([])
  UI.name_input(NameInput, input: false) == false &&
    Rune::Gfx.gesammelt.any? { |t| t.include?('Arik') } &&
    Rune::Gfx.gesammelt.any? { |t| t.include?('4/8') }
end
check('UI.name_input tippt kleine und große Buchstaben') do
  Rune::Input.setze!([:k])
  UI.name_input(NameInput)
  Rune::Input.setze!([:a], gehalten: [:shift])
  UI.name_input(NameInput)
  NameInput.text == 'ArikkA'
end
check('UI.name_input löscht mit Backspace') do
  Rune::Input.setze!([:backspace])
  UI.name_input(NameInput)
  NameInput.text == 'Arikk'
end
check('UI.name_input tippt "e" als Buchstaben') do
  Rune::Input.setze!([:e])
  UI.name_input(NameInput) == false && NameInput.text == 'Arikke' && NameInput.active?
end
check('UI.name_input übernimmt mit Enter und schließt') do
  Rune::Input.setze!([:enter])
  UI.name_input(NameInput) == true && !NameInput.active? && Party.find('arik').name == 'Arikke'
end
Rune::Input.setze!([])

check('alle 69 Befehlstypen im Register der Prüfung') { MapCheck::CMD_TYPES.size == 69 }

puts($fail.zero? ? 'ALLE CHECKS GRUEN' : "#{$fail} CHECK(S) FEHLGESCHLAGEN")
exit($fail.zero? ? 0 : 1)
