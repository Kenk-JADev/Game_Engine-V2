#encoding: utf-8
# ============================================================================
#  Kopfloser Test für die Variablen-Operationen (Etappe 20d, XP 122).
#  Lässt sich ohne Engine starten:
#     ruby -I game/engine tools/variable_ops_test.rb
#  Exit-Code 0 = alles grün.
#
#  Geprüft wird:
#    - Interpreter: set/add/sub/mul/div/mod auf dem alten Variablenwert
#    - Altform ohne `op` (value setzt, add addiert) bleibt gültig
#    - Fehlerfälle: unbekannte Operation, Division/Rest durch 0
#    - MapCheck: Vokabular, Pflichtwert, Division durch 0, Altform-Hinweise
#    - Register: Ruby-Liste == C++-Liste == Combo in editor_main.cpp
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

def lauf(interp, frames = 3, dt = 0.016)
  frames.times { interp.update(dt) }
  interp
end

# Wert setzen und einen variable-Befehl ausführen, Rückgabe = neuer Wert.
def rechne(startwert, cmd)
  GameState.reset!({})
  GameState.set_variable('x', startwert)
  i = Interpreter.new
  i.start([cmd])
  lauf(i)
  GameState.variable('x')
end

puts '== Interpreter: Operationen =='
check('set ersetzt den Wert') { rechne(7, { 'cmd' => 'variable', 'name' => 'x', 'op' => 'set', 'value' => 3 }) == 3.0 }
check('add addiert') { rechne(7, { 'cmd' => 'variable', 'name' => 'x', 'op' => 'add', 'value' => 2.5 }) == 9.5 }
check('sub subtrahiert') { rechne(7, { 'cmd' => 'variable', 'name' => 'x', 'op' => 'sub', 'value' => 10 }) == -3.0 }
check('mul multipliziert') { rechne(7, { 'cmd' => 'variable', 'name' => 'x', 'op' => 'mul', 'value' => 6 }) == 42.0 }
check('div teilt') { rechne(7, { 'cmd' => 'variable', 'name' => 'x', 'op' => 'div', 'value' => 2 }) == 3.5 }
check('mod liefert den Rest') { rechne(7, { 'cmd' => 'variable', 'name' => 'x', 'op' => 'mod', 'value' => 4 }) == 3.0 }
check('value fehlt = Operand 0') { rechne(7, { 'cmd' => 'variable', 'name' => 'x', 'op' => 'add' }) == 7.0 }

puts '== Interpreter: Altform und Fehlerfälle =='
check('Altform: value setzt, add addiert') do
  rechne(0, { 'cmd' => 'variable', 'name' => 'x', 'value' => 5, 'add' => 2 }) == 7.0
end
check('Altform nur add') { rechne(10, { 'cmd' => 'variable', 'name' => 'x', 'add' => 1 }) == 11.0 }
check('unbekannte Operation lässt den Wert stehen') do
  rechne(4, { 'cmd' => 'variable', 'name' => 'x', 'op' => 'pow', 'value' => 2 }) == 4.0
end
check('Division durch 0 lässt den Wert stehen') do
  rechne(4, { 'cmd' => 'variable', 'name' => 'x', 'op' => 'div', 'value' => 0 }) == 4.0
end
check('Rest durch 0 lässt den Wert stehen') do
  rechne(4, { 'cmd' => 'variable', 'name' => 'x', 'op' => 'mod', 'value' => 0 }) == 4.0
end
check('Fehler stoppt das Event nicht') do
  GameState.reset!({})
  i = Interpreter.new
  i.start([{ 'cmd' => 'variable', 'name' => 'x', 'op' => 'weg', 'value' => 1 },
           { 'cmd' => 'variable', 'name' => 'danach', 'value' => 1 }])
  lauf(i)
  GameState.variable('danach') == 1.0
end

puts '== MapCheck =='
OPTS = { prefabs: %w[haus], materials: %w[weg], scripts: [] }.freeze

def karte_mit(cmds)
  { 'name' => 'Testkarte',
    'terrain' => { 'width' => 120.0, 'height' => 90.0, 'resolution' => 64, 'amplitude' => 0.0 },
    'objects' => [],
    'events' => [{ 'id' => 1, 'name' => 'Variablen', 'x' => 0.0, 'z' => 0.0, 'radius' => 2.5,
                   'pages' => [{ 'conditions' => {}, 'trigger' => 'action', 'commands' => cmds }] }] }
end

def hat?(funde, fragment, severity = nil)
  funde.any? do |f|
    f.to_s.include?(fragment) && (severity.nil? || f.severity == severity)
  end
end

check('alle Operationen: keine Funde') do
  MapCheck.scan(karte_mit(MapCheck::VARIABLE_OPS.map do |op|
                            { 'cmd' => 'variable', 'name' => 'x', 'op' => op, 'value' => 1 }
                          end), **OPTS).empty?
end
check('unbekannte Operation -> Fehler') do
  hat?(MapCheck.scan(karte_mit([{ 'cmd' => 'variable', 'name' => 'x', 'op' => 'pow', 'value' => 1 }]), **OPTS),
       "unbekannte Variable-Operation 'pow'", MapCheck::FEHLER)
end
check('Operation ohne Zahl -> Warnung') do
  hat?(MapCheck.scan(karte_mit([{ 'cmd' => 'variable', 'name' => 'x', 'op' => 'add' }]), **OPTS),
       "'value' fehlt oder ist keine Zahl", MapCheck::WARNUNG)
end
check('Division durch 0 -> Fehler') do
  hat?(MapCheck.scan(karte_mit([{ 'cmd' => 'variable', 'name' => 'x', 'op' => 'div', 'value' => 0 }]), **OPTS),
       'div durch 0', MapCheck::FEHLER)
end
check('add neben op -> Warnung') do
  hat?(MapCheck.scan(karte_mit([{ 'cmd' => 'variable', 'name' => 'x', 'op' => 'add', 'value' => 1, 'add' => 2 }]), **OPTS),
       "'add' (Altform) neben 'op'", MapCheck::WARNUNG)
end
check('gemischte Altform -> Warnung') do
  hat?(MapCheck.scan(karte_mit([{ 'cmd' => 'variable', 'name' => 'x', 'value' => 1, 'add' => 2 }]), **OPTS),
       "Altform: 'value' setzt", MapCheck::WARNUNG)
end
check('Befehl ohne Wirkung -> Warnung') do
  hat?(MapCheck.scan(karte_mit([{ 'cmd' => 'variable', 'name' => 'x' }]), **OPTS),
       'ohne Wirkung', MapCheck::WARNUNG)
end

puts '== Register =='
check('MapCheck::VARIABLE_OPS sind die sechs XP-Operationen') do
  MapCheck::VARIABLE_OPS == %w[set add sub mul div mod]
end
check('C++ EVENT_VAR_OPS nennen dieselben Operationen') do
  datei = File.read(File.join(Rune::ROOT, 'src', 'editor', 'event_form.h'), encoding: 'utf-8')
  liste = datei[/EVENT_VAR_OPS\[\] = \{([^}]*)\}/, 1].to_s
  liste.scan(/"([^"]+)"/).flatten == MapCheck::VARIABLE_OPS
end

puts($fail.zero? ? 'ALLE CHECKS GRUEN' : "#{$fail} CHECK(S) FEHLGESCHLAGEN")
exit($fail.zero? ? 0 : 1)
