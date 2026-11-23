#encoding: utf-8
# ============================================================================
#  Kopfloser Test für Self-Switches (Etappe 19d, RPG-Maker-XP-Feature).
#  Lässt sich ohne Engine starten:
#     ruby -I game/engine tools/self_switch_test.rb
#  Exit-Code 0 = alles grün.
#
#  Geprüft wird:
#    - GameState: setzen/lesen, Buchstaben-Normalisierung, ungültige
#      Buchstaben, Schlüssel-Isolation pro Event, Savegame-Rundlauf
#    - Event: Seitenbedingung 'self_switch' (höchste erfüllte Seite gewinnt)
#    - Interpreter: self_switch setzt den Schalter des Besitzer-Events,
#      if_self_switch wählt Dann-/Sonst-Zweig, ohne Besitzer keine Abstürze
#    - MapCheck: letter-Pflicht (A-D), value-Warnung, Bedingungs-Prüfung
# ============================================================================

# --- Minimal-Attrappe der Engine-Schnittstelle -----------------------------
module Rune
  ROOT = File.expand_path('..', __dir__)

  module Log
    def self.info(m) = nil
    def self.warn(m) = puts("WARN  #{m}")
    def self.error(m) = puts("ERROR #{m}")
  end

  module Gfx
    def self.box(*_a) = nil
  end
end

# Game-Attrappe: Event#self_switch_key braucht die Karten-ID.
module Game
  def self.map_id = 'testkarte'
  def self.gy(_x, _z) = 0.0
end

$LOAD_PATH.unshift(File.join(Rune::ROOT, 'game', 'engine'))
require 'data'
require 'state'
require 'issues'
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

GameState.reset!({})

puts '== GameState: Self-Switch-Grundlagen =='
check('frischer Zustand: Self-Switch aus') { !GameState.self_switch('k|e', 'A') }
GameState.set_self_switch('testkarte|Wache', 'A', true)
check('gesetzt -> an') { GameState.self_switch('testkarte|Wache', 'A') }
check('anderer Buchstabe bleibt aus') { !GameState.self_switch('testkarte|Wache', 'B') }
check('anderes Event bleibt aus (Schlüssel-Isolation)') do
  !GameState.self_switch('testkarte|Schmied', 'A')
end
GameState.set_self_switch('testkarte|Wache', 'a', false)
check('Kleinbuchstabe wird normalisiert (a == A)') do
  !GameState.self_switch('testkarte|Wache', 'A')
end
GameState.set_self_switch('testkarte|Wache', 'X', true)
check('ungültiger Buchstabe X wird nicht gespeichert') do
  !GameState.self_switch('testkarte|Wache', 'X')
end

puts '== GameState: Savegame-Rundlauf =='
GameState.set_self_switch('testkarte|Wache', 'B', true)
snap = GameState.snapshot
check('Snapshot enthält self_switches') { snap.key?('self_switches') }
GameState.reset!({})
check('reset! löscht Self-Switches') { !GameState.self_switch('testkarte|Wache', 'B') }
GameState.restore!(snap)
check('restore! holt Self-Switch zurück') { GameState.self_switch('testkarte|Wache', 'B') }
GameState.restore!({ 'switches' => {}, 'variables' => {} })
check('alter Savegame-Stand ohne self_switches: leer, kein Absturz') do
  !GameState.self_switch('testkarte|Wache', 'B')
end
GameState.reset!({})

puts '== Event: Seitenbedingung self_switch =='
wache = Event.new(
  'id' => 1, 'name' => 'Wache', 'x' => 0, 'z' => 0,
  'pages' => [
    { 'conditions' => {},
      'trigger' => 'action',
      'commands' => [{ 'cmd' => 'text', 'lines' => ['Seite 0: Halt!'] }] },
    { 'conditions' => { 'self_switch' => { 'letter' => 'A', 'value' => true } },
      'trigger' => 'action',
      'commands' => [{ 'cmd' => 'text', 'lines' => ['Seite 1: Willkommen zurück.'] }] }
  ]
)
check('Self-Switch-Schlüssel = Karte|Name') { wache.self_switch_key == 'testkarte|Wache' }
wache.refresh_page!
check('ohne Self-Switch: Seite 0 aktiv') { wache.page_index == 0 }
GameState.set_self_switch(wache.self_switch_key, 'A', true)
wache.refresh_page!
check('Self-Switch A an: Seite 1 gewinnt') { wache.page_index == 1 }
GameState.set_self_switch(wache.self_switch_key, 'A', false)
wache.refresh_page!
check('Self-Switch A aus: zurück auf Seite 0') { wache.page_index == 0 }
check('Bedingung ohne Event-Kontext nie erfüllt') do
  !Event.conditions_met?({ 'self_switch' => { 'letter' => 'A', 'value' => true } })
end

puts '== Interpreter: self_switch / if_self_switch =='
GameState.reset!({})
redner = Event.new('id' => 2, 'name' => 'Hebel', 'x' => 1, 'z' => 1,
                   'trigger' => 'action',
                   'commands' => [{ 'cmd' => 'self_switch', 'letter' => 'C',
                                    'value' => true }])
interp = Interpreter.new
redner.manual_fire(interp)
interp.update(0.016)
check('self_switch setzt Schalter C des Besitzer-Events') do
  GameState.self_switch('testkarte|Hebel', 'C')
end
check('Interpreter danach fertig') { !interp.running? }

verzweigt = Event.new('id' => 3, 'name' => 'Truhe', 'x' => 2, 'z' => 2,
                      'trigger' => 'action',
                      'commands' => [
                        { 'cmd' => 'if_self_switch', 'letter' => 'C', 'value' => true,
                          'then' => [{ 'cmd' => 'variable', 'name' => 'dann', 'value' => 1 }],
                          'else' => [{ 'cmd' => 'variable', 'name' => 'sonst', 'value' => 1 }] }
                      ])
GameState.set_self_switch('testkarte|Truhe', 'C', true)
interp2 = Interpreter.new
verzweigt.manual_fire(interp2)
interp2.update(0.016)
check('if_self_switch wahr -> Dann-Zweig') { GameState.variable('dann') == 1.0 }
check('if_self_switch wahr -> Sonst-Zweig läuft nicht') { GameState.variable('sonst') == 0.0 }

GameState.set_self_switch('testkarte|Truhe', 'C', false)
interp3 = Interpreter.new
verzweigt.manual_fire(interp3)
interp3.update(0.016)
check('if_self_switch falsch -> Sonst-Zweig') { GameState.variable('sonst') == 1.0 }

# Ohne Besitzer (Intro-Pfad) darf nichts abstürzen; if_self_switch -> Sonst.
interp4 = Interpreter.new
interp4.start([{ 'cmd' => 'self_switch', 'letter' => 'A', 'value' => true },
               { 'cmd' => 'if_self_switch', 'letter' => 'A', 'value' => true,
                 'then' => [{ 'cmd' => 'variable', 'name' => 'dann2', 'value' => 1 }],
                 'else' => [{ 'cmd' => 'variable', 'name' => 'sonst2', 'value' => 1 }] }])
interp4.update(0.016)
check('ohne Besitzer: if_self_switch nimmt Sonst-Zweig') do
  GameState.variable('sonst2') == 1.0 && GameState.variable('dann2') == 0.0
end
# Interpreter-Design: pro update() wird höchstens ein fertiger Rahmen vom
# Stack genommen - der verschachtelte Sonst-Zweig braucht also einen zweiten
# Frame, genau wie bei if_switch/if_variable (18f).
interp4.update(0.016)
check('ohne Besitzer: Interpreter läuft trotzdem zu Ende') { !interp4.running? }

puts '== MapCheck: Prüfung der neuen Befehle und Bedingungen =='
OPTS = { prefabs: %w[haus], materials: %w[weg], scripts: [] }.freeze

def karte_mit(cmds, conditions = {})
  {
    'name' => 'Testkarte',
    'terrain' => { 'width' => 120.0, 'height' => 90.0, 'resolution' => 64,
                   'amplitude' => 0.0 },
    'objects' => [],
    'events' => [{ 'id' => 1, 'name' => 'Wache', 'x' => 0.0, 'z' => 0.0,
                   'radius' => 2.5,
                   'pages' => [{ 'conditions' => conditions,
                                 'trigger' => 'action',
                                 'commands' => cmds }] }]
  }
end

def hat?(funde, fragment, severity = nil)
  funde.any? do |f|
    f.to_s.include?(fragment) && (severity.nil? || f.severity == severity)
  end
end

sauber = karte_mit([{ 'cmd' => 'self_switch', 'letter' => 'A', 'value' => true },
                    { 'cmd' => 'if_self_switch', 'letter' => 'B', 'value' => false,
                      'then' => [{ 'cmd' => 'log', 'text' => 'ja' }],
                      'else' => [{ 'cmd' => 'log', 'text' => 'nein' }] }],
                   { 'self_switch' => { 'letter' => 'A', 'value' => true } })
check('gültige Self-Switch-Karte: keine Funde') do
  MapCheck.scan(sauber, **OPTS).empty?
end
check('self_switch ohne letter -> Fehler') do
  hat?(MapCheck.scan(karte_mit([{ 'cmd' => 'self_switch', 'value' => true }]), **OPTS),
       "'letter' muss A, B, C oder D sein", MapCheck::FEHLER)
end
check('self_switch mit letter E -> Fehler') do
  hat?(MapCheck.scan(karte_mit([{ 'cmd' => 'self_switch', 'letter' => 'E',
                                  'value' => true }], **OPTS)),
       "'letter' muss A, B, C oder D sein", MapCheck::FEHLER)
end
check('self_switch ohne value -> Warnung') do
  hat?(MapCheck.scan(karte_mit([{ 'cmd' => 'self_switch', 'letter' => 'A' }], **OPTS)),
       "'value' fehlt", MapCheck::WARNUNG)
end
check('if_self_switch ohne Zweige -> Warnung') do
  hat?(MapCheck.scan(karte_mit([{ 'cmd' => 'if_self_switch', 'letter' => 'A',
                                  'value' => true }], **OPTS)),
       "'then' fehlt/keine Liste", MapCheck::WARNUNG)
end
check('Bedingung self_switch mit falschem Buchstaben -> Fehler') do
  hat?(MapCheck.scan(karte_mit([{ 'cmd' => 'log', 'text' => 'x' }],
                               { 'self_switch' => { 'letter' => 'Z', 'value' => true } }),
                     **OPTS),
       "'letter' muss A, B, C oder D sein", MapCheck::FEHLER)
end

puts($fail.zero? ? 'ALLE CHECKS GRUEN' : "#{$fail} CHECK(S) FEHLGESCHLAGEN")
exit($fail.zero? ? 0 : 1)
