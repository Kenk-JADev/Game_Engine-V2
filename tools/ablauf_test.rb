#encoding: utf-8
# ============================================================================
#  Kopfloser Test für die Ablauf-Befehle (Etappe 20c, RPG-Maker-XP-Feature).
#  Lässt sich ohne Engine starten:
#     ruby -I game/engine tools/ablauf_test.rb
#  Exit-Code 0 = alles grün.
#
#  Geprüft wird:
#    - Interpreter: loop/break wiederholen und verlassen, verschachtelte
#      Schleifen, break ohne Schleife, label/jump (auch unbekannte Marke),
#      exit_event, erase_event (nur mit Besitzer), button_input (blockiert,
#      schreibt die 1-basierte Position), text_options (leitet an
#      MessageWindow weiter)
#    - Event: erase! blendet Auslöser/Picking aus und bleibt gelöscht
#    - MessageWindow: Defaults, set/reset!, unbekannte Position
#    - MapCheck: Schleife ohne Ausgang, break außerhalb, fehlende Marke,
#      Tastenliste, Fenster-Position
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
    class << self
      attr_accessor :pressed
    end
    def self.pressed?(key) = @pressed.include?(key)
    def self.down?(key) = @pressed.include?(key)
  end
end

module Game
  def self.map_id = 'testkarte'
  def self.gy(_x, _z) = 0.0
  def self.reset_step_tracking = nil
end

$LOAD_PATH.unshift(File.join(Rune::ROOT, 'game', 'engine'))
require 'data'
require 'state'
require 'issues'
require 'message_window'
require 'event'
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

# Interpreter bis zum Stillstand treiben (höchstens n Frames).
def lauf(interp, frames = 40, dt = 0.016)
  frames.times { interp.update(dt) }
  interp
end

GameState.reset!({})
Rune::Input.pressed = []

puts '== Interpreter: Schleife =='
# Drei Durchläufe: die Variable wächst dreimal, dann bricht break ab.
interp = Interpreter.new
interp.start([
               { 'cmd' => 'variable', 'name' => 'n', 'value' => 0 },
               { 'cmd' => 'loop', 'commands' => [
                 { 'cmd' => 'variable', 'name' => 'n', 'add' => 1 },
                 { 'cmd' => 'if_variable', 'name' => 'n', 'op' => '>=', 'value' => 4,
                   'then' => [{ 'cmd' => 'break' }], 'else' => [] }
               ] },
               { 'cmd' => 'log', 'text' => 'hinter der Schleife' }
             ])
lauf(interp, 10)
z = GameState.variable('n')
check("loop/break zählt bis vier (n=#{z})") { z == 4.0 }
check('Befehle hinter der Schleife laufen weiter') { !interp.running? }

# Verschachtelt: inneres break verlässt nur die innere Schleife.
interp = Interpreter.new
interp.start([
               { 'cmd' => 'variable', 'name' => 'aussen', 'value' => 0 },
               { 'cmd' => 'variable', 'name' => 'innen', 'value' => 0 },
               { 'cmd' => 'loop', 'commands' => [
                 { 'cmd' => 'variable', 'name' => 'aussen', 'add' => 1 },
                 { 'cmd' => 'loop', 'commands' => [
                   { 'cmd' => 'variable', 'name' => 'innen', 'add' => 1 },
                   { 'cmd' => 'break' }
                 ] },
                 { 'cmd' => 'if_variable', 'name' => 'aussen', 'op' => '>=', 'value' => 2,
                   'then' => [{ 'cmd' => 'break' }], 'else' => [] }
               ] }
             ])
lauf(interp, 20)
check('inneres break beendet nur die innere Schleife') do
  GameState.variable('aussen') == 2.0 && GameState.variable('innen') == 2.0
end

interp = Interpreter.new
interp.start([{ 'cmd' => 'break' }, { 'cmd' => 'variable', 'name' => 'danach', 'value' => 7 }])
lauf(interp, 5)
check('break ohne Schleife stört den Ablauf nicht') do
  !interp.running? && GameState.variable('danach') == 7.0
end

puts '== Interpreter: Sprünge =='
interp = Interpreter.new
interp.start([
               { 'cmd' => 'variable', 'name' => 'spur', 'value' => 0 },
               { 'cmd' => 'jump', 'name' => 'unbekannt' },
               { 'cmd' => 'variable', 'name' => 'spur', 'value' => 5 }
             ])
lauf(interp, 5)
check('Sprung auf unbekannte Marke: Ausführung läuft weiter') do
  !interp.running? && GameState.variable('spur') == 5.0
end

interp = Interpreter.new
interp.start([
               { 'cmd' => 'jump', 'name' => 'ziel' },
               { 'cmd' => 'variable', 'name' => 'uebersprungen', 'value' => 1 },
               { 'cmd' => 'label', 'name' => 'ziel' },
               { 'cmd' => 'variable', 'name' => 'angekommen', 'value' => 1 }
             ])
lauf(interp, 5)
check('Sprung überspringt die Befehle bis zur Marke') do
  GameState.variable('uebersprungen').zero? && GameState.variable('angekommen') == 1.0
end

# XP sucht die Marke nur im AKTUELLEN Block: in einem Wenn-Zweig gefundene
# Marken gelten dort, nicht im äußeren Block (dokumentierte Grenze).
interp = Interpreter.new
interp.start([
               { 'cmd' => 'if_variable', 'name' => 'egal', 'op' => '==', 'value' => 0,
                 'else' => [],
                 'then' => [
                   { 'cmd' => 'jump', 'name' => 'innen' },
                   { 'cmd' => 'variable', 'name' => 'zweig_uebersprungen', 'value' => 1 },
                   { 'cmd' => 'label', 'name' => 'innen' },
                   { 'cmd' => 'variable', 'name' => 'zweig_ziel', 'value' => 1 }
                 ] }
             ])
lauf(interp, 5)
check('Marke im Wenn-Zweig wirkt für Sprünge im Zweig') do
  GameState.variable('zweig_uebersprungen').zero? && GameState.variable('zweig_ziel') == 1.0
end

puts '== Interpreter: Event beenden/löschen =='
interp = Interpreter.new
interp.start([{ 'cmd' => 'exit_event' }, { 'cmd' => 'variable', 'name' => 'rest', 'value' => 1 }])
lauf(interp, 5)
check('exit_event beendet die Liste') do
  !interp.running? && GameState.variable('rest').zero?
end

ev = Event.new('name' => 'Schild', 'x' => 0.0, 'z' => 0.0, 'radius' => 2.0,
               'pages' => [{ 'conditions' => {}, 'trigger' => 'action', 'commands' => [] }])
interp = Interpreter.new
interp.start([{ 'cmd' => 'erase_event' }, { 'cmd' => 'variable', 'name' => 'rest2', 'value' => 1 }], ev)
lauf(interp, 5)
check('erase_event löscht das Besitzer-Event und beendet die Liste') do
  ev.erased? && !interp.running? && GameState.variable('rest2').zero?
end
check('gelöschtes Event bleibt inaktiv') do
  ev.refresh_page!
  !ev.active? && ev.commands.empty? && !ev.manual_fire(Interpreter.new)
end
check('erase_event ohne Besitzer stürzt nicht ab') do
  i2 = Interpreter.new
  i2.start([{ 'cmd' => 'erase_event' }])
  lauf(i2, 3)
  !i2.running?
end

puts '== Interpreter: Tasteneingabe =='
Rune::Input.pressed = []
interp = Interpreter.new
interp.start([{ 'cmd' => 'button_input', 'name' => 'taste', 'keys' => %w[e escape] },
              { 'cmd' => 'variable', 'name' => 'weiter', 'value' => 1 }])
interp.update(0.016)
check('ohne Tastendruck blockiert der Interpreter') do
  interp.running? && GameState.variable('taste').zero? && GameState.variable('weiter').zero?
end
Rune::Input.pressed = [:escape]
interp.update(0.016)
lauf(interp, 3)
check('Tastendruck schreibt die 1-basierte Position (escape = 2)') do
  GameState.variable('taste') == 2.0 && GameState.variable('weiter') == 1.0 && !interp.running?
end
Rune::Input.pressed = []
check('button_input ohne Tasten wird übersprungen (kein Hänger)') do
  i3 = Interpreter.new
  i3.start([{ 'cmd' => 'button_input', 'name' => 'leer', 'keys' => [] },
            { 'cmd' => 'variable', 'name' => 'nachleer', 'value' => 2 }])
  lauf(i3, 3)
  !i3.running? && GameState.variable('nachleer') == 2.0
end

puts '== Interpreter: Fenster-Optionen =='
MessageWindow.reset!
interp = Interpreter.new
interp.start([{ 'cmd' => 'text_options', 'position' => 'oben', 'frame' => false,
                'transparent' => true }])
lauf(interp, 2)
check('text_options setzt Position, Rahmen und Transparenz') do
  MessageWindow.position == 'oben' && !MessageWindow.frame? && MessageWindow.transparent?
end
interp = Interpreter.new
interp.start([{ 'cmd' => 'text_options', 'position' => 'seitlich' }])
lauf(interp, 2)
check('unbekannte Position wird ignoriert (Stand bleibt)') { MessageWindow.position == 'oben' }
MessageWindow.reset!
check('reset! stellt die XP-Defaults her') do
  MessageWindow.position == 'unten' && MessageWindow.frame? && !MessageWindow.transparent?
end

puts '== MapCheck: Ablauf-Prüfungen =='
OPTS = { prefabs: %w[haus], materials: %w[weg], scripts: [] }.freeze

def karte_mit(cmds)
  {
    'name' => 'Testkarte',
    'terrain' => { 'width' => 120.0, 'height' => 90.0, 'resolution' => 64,
                   'amplitude' => 0.0 },
    'objects' => [],
    'events' => [{ 'id' => 1, 'name' => 'Ablauf', 'x' => 0.0, 'z' => 0.0,
                   'radius' => 2.5,
                   'pages' => [{ 'conditions' => {}, 'trigger' => 'action',
                                 'commands' => cmds }] }]
  }
end

def hat?(funde, fragment, severity = nil)
  funde.any? do |f|
    f.to_s.include?(fragment) && (severity.nil? || f.severity == severity)
  end
end

check('gültige Schleife mit break: keine Funde') do
  MapCheck.scan(karte_mit([{ 'cmd' => 'loop',
                             'commands' => [{ 'cmd' => 'break' }] }]), **OPTS).empty?
end
check('Schleife ohne Ausgang -> Warnung') do
  hat?(MapCheck.scan(karte_mit([{ 'cmd' => 'loop', 'commands' => [{ 'cmd' => 'log', 'text' => 'x' }] }]),
                     **OPTS),
       'läuft endlos', MapCheck::WARNUNG)
end
check('Schleife ohne commands -> Fehler') do
  hat?(MapCheck.scan(karte_mit([{ 'cmd' => 'loop' }]), **OPTS),
       "'commands' fehlt/keine Liste", MapCheck::FEHLER)
end
check('break außerhalb einer Schleife -> Warnung') do
  hat?(MapCheck.scan(karte_mit([{ 'cmd' => 'break' }]), **OPTS),
       'außerhalb einer Schleife', MapCheck::WARNUNG)
end
check('Sprung ohne Marke -> Warnung') do
  hat?(MapCheck.scan(karte_mit([{ 'cmd' => 'jump', 'name' => 'fehlt' }]), **OPTS),
       "keine Marke 'fehlt'", MapCheck::WARNUNG)
end
check('Sprung mit Marke: keine Funde') do
  MapCheck.scan(karte_mit([{ 'cmd' => 'jump', 'name' => 'ziel' },
                           { 'cmd' => 'label', 'name' => 'ziel' }]), **OPTS).empty?
end
check('exit_event/erase_event brauchen keine Felder') do
  MapCheck.scan(karte_mit([{ 'cmd' => 'exit_event' }, { 'cmd' => 'erase_event' }]), **OPTS).empty?
end
check('button_input ohne Tasten -> Fehler') do
  hat?(MapCheck.scan(karte_mit([{ 'cmd' => 'button_input', 'name' => 't' }]), **OPTS),
       "'keys' fehlt/leer", MapCheck::FEHLER)
end
check('button_input mit unbekannter Taste -> Warnung') do
  hat?(MapCheck.scan(karte_mit([{ 'cmd' => 'button_input', 'name' => 't', 'keys' => ['sondertaste'] }]), **OPTS),
       "unbekannte Taste 'sondertaste'", MapCheck::WARNUNG)
end
check('button_input mit gültigen Tasten: keine Funde') do
  MapCheck.scan(karte_mit([{ 'cmd' => 'button_input', 'name' => 't',
                             'keys' => %w[e space up f1] }]), **OPTS).empty?
end
check('text_options mit unbekannter Position -> Warnung') do
  hat?(MapCheck.scan(karte_mit([{ 'cmd' => 'text_options', 'position' => 'seitlich' }]), **OPTS),
       "'position' muss", MapCheck::WARNUNG)
end
check('text_options mit gültigen Werten: keine Funde') do
  MapCheck.scan(karte_mit([{ 'cmd' => 'text_options', 'position' => 'mitte',
                             'frame' => false, 'transparent' => true }]), **OPTS).empty?
end
check('alle 54 Befehlstypen im Register der Prüfung') { MapCheck::CMD_TYPES.size == 54 }

puts($fail.zero? ? 'ALLE CHECKS GRUEN' : "#{$fail} CHECK(S) FEHLGESCHLAGEN")
exit($fail.zero? ? 0 : 1)
