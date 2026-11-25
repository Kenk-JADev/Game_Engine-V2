#encoding: utf-8
# ============================================================================
#  Kopfloser Test für die Wert-Befehle (Etappe 20d, XP 311/312/315/316).
#  Lässt sich ohne Engine starten:
#     ruby -I game/engine tools/actor_wert_test.rb
#  Exit-Code 0 = alles grün.
#
#  Geprüft wird:
#    - Party.ziel_actors: "alle"/leer = Party, Actor-ID und Name, Unbekanntes
#    - Party.change_hp/change_mp: Klemmen auf 0..Max, Zielwahl
#    - Party.change_exp: Stufenaufstieg über set_exp
#    - Party.change_level: Aufstieg mit Wachstum, Abstieg ohne Werte-Rückbau
#    - Interpreter: die vier Befehle erreichen die richtige Party-Methode
#    - MapCheck: Pflichtfeld amount, Menge 0, unbekannter Actor, Stufenbereich
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

DB = {
  'gold' => 10,
  'actors' => [
    { 'id' => 'arik', 'name' => 'Arik', 'level' => 1, 'hp' => 100, 'mp' => 20 },
    { 'id' => 'mira', 'name' => 'Mira', 'level' => 1, 'hp' => 80, 'mp' => 40 }
  ],
  'items' => [{ 'id' => 'heiltrank', 'name' => 'Heiltrank', 'kind' => 'heal', 'power' => 40 }]
}.freeze

def neu!
  Party.setup(DB)
end

def arik = Party.find('arik')
def mira = Party.find('mira')

def lauf(interp, frames = 3, dt = 0.016)
  frames.times { interp.update(dt) }
  interp
end

puts '== Zielwahl =='
neu!
check('leeres Ziel und "alle" meinen die ganze Party') do
  Party.ziel_actors(nil) == Party.actors && Party.ziel_actors('') == Party.actors &&
    Party.ziel_actors('alle') == Party.actors && Party.ziel_actors('ALLE') == Party.actors
end
check('Actor per ID und per Name') do
  Party.ziel_actors('arik') == [arik] && Party.ziel_actors('Arik') == [arik] &&
    Party.ziel_actors('mira') == [mira]
end
check('unbekannter Actor: leere Liste') { Party.ziel_actors('gibtsnicht').empty? }

puts '== HP/SP =='
neu!
check('-20 HP auf alle') { Party.change_hp('alle', -20) == 2 && arik.hp == 80 && mira.hp == 60 }
check('Heilung klemmt am Maximum') do
  Party.change_hp('arik', 1000) && arik.hp == arik.max_hp
end
check('Schaden klemmt bei 0') do
  Party.change_hp('mira', -1000) && mira.hp.zero?
end
check('-20 SP/MP nur bei Arik') do
  Party.change_mp('arik', -20) && arik.mp.zero? && mira.mp == 40
end
check('MP-Heilung klemmt am Maximum') do
  Party.change_mp('alle', 1000) && arik.mp == arik.max_mp && mira.mp == mira.max_mp
end
check('unbekanntes Ziel ändert nichts') do
  Party.change_hp('niemand', 50).zero? && arik.hp == arik.max_hp
end

puts '== EXP und Stufe =='
neu!
check('EXP +1000 löst Aufstiege aus') do
  level_vorher = arik.level
  Party.change_exp('arik', 1000) && arik.exp == 1000 && arik.level > level_vorher
end
check('EXP sinkt ohne Stufenabbau') do
  stufe = arik.level
  Party.change_exp('arik', -500) && arik.exp == 500 && arik.level == stufe
end
neu!
check('Stufe +2 wächst wie im Kampf') do
  hp_vorher = arik.max_hp
  Party.change_level('arik', 2) && arik.level == 3 && arik.max_hp > hp_vorher
end
check('Stufe -1 senkt nur die Stufe') do
  hp_hoch = arik.max_hp
  Party.change_level('arik', -1) && arik.level == 2 && arik.max_hp == hp_hoch
end
check('Stufe wird auf 1..99 geklemmt') do
  Party.change_level('mira', -50) && mira.level == 1 &&
    Party.change_level('mira', 500) && mira.level == 99
end

puts '== Interpreter =='
neu!
GameState.reset!({})
i = Interpreter.new
i.start([{ 'cmd' => 'actor_hp', 'target' => 'arik', 'amount' => -30 },
         { 'cmd' => 'actor_sp', 'target' => 'arik', 'amount' => -5 },
         { 'cmd' => 'actor_exp', 'target' => 'mira', 'amount' => 250 },
         { 'cmd' => 'variable', 'name' => 'fertig', 'value' => 1 }])
lauf(i)
check('actor_hp/actor_sp erreichen den Helden') { arik.hp == 70 && arik.mp == 15 }
check('actor_exp erreicht Mira') { mira.exp == 250 && mira.level > 1 }
check('danach läuft das Event weiter') { GameState.variable('fertig') == 1.0 }
stufe = mira.level
i = Interpreter.new
i.start([{ 'cmd' => 'actor_level', 'target' => 'mira', 'amount' => 1 }])
lauf(i)
check('actor_level hebt Mira genau eine Stufe') { mira.level == stufe + 1 }

puts '== MapCheck =='
OPTS = { prefabs: %w[haus], materials: %w[weg], scripts: [] }.freeze

def karte_mit(cmds)
  { 'name' => 'Testkarte',
    'terrain' => { 'width' => 120.0, 'height' => 90.0, 'resolution' => 64, 'amplitude' => 0.0 },
    'objects' => [],
    'events' => [{ 'id' => 1, 'name' => 'Werte', 'x' => 0.0, 'z' => 0.0, 'radius' => 2.5,
                   'pages' => [{ 'conditions' => {}, 'trigger' => 'action', 'commands' => cmds }] }] }
end

def hat?(funde, fragment, severity = nil)
  funde.any? do |f|
    f.to_s.include?(fragment) && (severity.nil? || f.severity == severity)
  end
end

check('gültige Wert-Befehle: keine Funde') do
  MapCheck.scan(karte_mit([
                            { 'cmd' => 'actor_hp', 'target' => 'alle', 'amount' => -20 },
                            { 'cmd' => 'actor_sp', 'target' => 'arik', 'amount' => 5 },
                            { 'cmd' => 'actor_exp', 'amount' => 50 },
                            { 'cmd' => 'actor_level', 'target' => 'Mira', 'amount' => 1 }
                          ]), **OPTS).empty?
end
check('fehlende Menge -> Fehler') do
  hat?(MapCheck.scan(karte_mit([{ 'cmd' => 'actor_hp', 'target' => 'alle' }]), **OPTS),
       "'amount' fehlt oder ist keine Zahl", MapCheck::FEHLER)
end
check('Menge 0 -> Warnung') do
  hat?(MapCheck.scan(karte_mit([{ 'cmd' => 'actor_sp', 'amount' => 0 }]), **OPTS),
       "'amount' ist 0", MapCheck::WARNUNG)
end
check('unbekannter Actor -> Warnung') do
  hat?(MapCheck.scan(karte_mit([{ 'cmd' => 'actor_exp', 'target' => 'niemand', 'amount' => 5 }]), **OPTS),
       "Actor 'niemand' nicht in database.json", MapCheck::WARNUNG)
end
check('Stufe über 99 Schritte -> Warnung') do
  hat?(MapCheck.scan(karte_mit([{ 'cmd' => 'actor_level', 'amount' => 200 }]), **OPTS),
       'übersteigt den Stufenbereich', MapCheck::WARNUNG)
end
check('alle 55 Befehlstypen im Register der Prüfung') { MapCheck::CMD_TYPES.size == 55 }

puts($fail.zero? ? 'ALLE CHECKS GRUEN' : "#{$fail} CHECK(S) FEHLGESCHLAGEN")
exit($fail.zero? ? 0 : 1)
