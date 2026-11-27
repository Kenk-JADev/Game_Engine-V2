#encoding: utf-8
# ============================================================================
#  Kopfloser Test für XP 318: Event-Befehl zum Lernen/Vergessen von Skills.
#  ruby -I game/engine tools/actor_skill_test.rb
# ============================================================================
module Rune
  ROOT = File.expand_path('..', __dir__)
  module Log
    def self.info(_message) = nil
    def self.warn(message) = puts("WARN  #{message}")
    def self.error(message) = puts("ERROR #{message}")
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

def reset!
  Party.setup(Database.database)
  GameState.reset!({})
  Issues.clear!
end

def lauf(interpreter, frames = 3)
  frames.times { interpreter.update(0.016) }
end

def karte_mit(cmds)
  { 'name' => 'Testkarte',
    'terrain' => { 'width' => 120.0, 'height' => 90.0, 'resolution' => 64, 'amplitude' => 0.0 },
    'objects' => [],
    'events' => [{ 'id' => 1, 'name' => 'Skills', 'x' => 0.0, 'z' => 0.0, 'radius' => 2.5,
                   'pages' => [{ 'conditions' => {}, 'trigger' => 'action', 'commands' => cmds }] }] }
end

def hat?(findings, fragment, severity = nil)
  findings.any? do |finding|
    finding.to_s.include?(fragment) && (severity.nil? || finding.severity == severity)
  end
end

skill = Database.database.fetch('skills').find { |row| row['id'] == 'eisspeer' }
raise 'Test-Skill eisspeer fehlt in game/data/database.json' unless skill
name = skill.fetch('name')
reset!
puts '== Party: Skills ändern =='
check('skill_def akzeptiert ID und Name') do
  Party.skill_def('eisspeer')['id'] == 'eisspeer' && Party.skill_def(name)['id'] == 'eisspeer'
end
check('add lernt den Skill für einen Helden') do
  Party.change_skill('arik', name, mode: 'add') == 1 && Party.skills('arik').include?('eisspeer')
end
check('doppeltes add ist wirkungslos') { Party.change_skill('arik', 'eisspeer', mode: 'add') == 0 }
check('remove vergisst den Skill') do
  Party.change_skill('arik', 'eisspeer', mode: 'remove') == 1 && !Party.skills('arik').include?('eisspeer')
end
check('add auf alle ändert jeden passenden Helden') do
  Party.setup(Database.database)
  Party.actors.each { |actor| Party.forget_skill(actor, 'eisspeer') }
  Party.change_skill('alle', 'eisspeer', mode: 'add') == Party.actors.size &&
    Party.actors.all? { |actor| Party.skills(actor).include?('eisspeer') }
end
check('unbekannter Skill und unbekanntes Ziel werden getrennt gemeldet') do
  Party.change_skill('arik', 'gibtsnicht', mode: 'add') == :unbekannt &&
    Party.change_skill('niemand', 'eisspeer', mode: 'add') == :ziel_unbekannt
end

puts '== Interpreter: XP 318 =='
reset!
interpreter = Interpreter.new
interpreter.start([
  { 'cmd' => 'actor_skill', 'actor' => 'arik', 'name' => name, 'mode' => 'add' },
  { 'cmd' => 'actor_skill', 'actor' => 'arik', 'name' => 'eisspeer', 'mode' => 'remove' },
  { 'cmd' => 'actor_skill', 'actor' => 'alle', 'name' => 'eisspeer' }
])
lauf(interpreter)
check('Event-Befehl add/remove und Default-add laufen der Reihe nach') do
  Party.skills('arik').include?('eisspeer') && Party.actors.all? { |actor| Party.skills(actor).include?('eisspeer') }
end
interpreter = Interpreter.new
interpreter.start([{ 'cmd' => 'actor_skill', 'actor' => 'arik', 'name' => 'unbekannt' }])
lauf(interpreter)
check('unbekannter Skill meldet ein Issue') do
  Issues.all.any? { |issue| issue.to_s.include?("actor_skill 'unbekannt'") }
end

puts '== MapCheck =='
OPTS = { prefabs: %w[haus], materials: %w[weg], scripts: [] }.freeze
check('gültiger actor_skill mit Name, Ziel und remove') do
  MapCheck.scan(karte_mit([{ 'cmd' => 'actor_skill', 'actor' => 'arik', 'name' => name, 'mode' => 'remove' }]), **OPTS).empty?
end
check('actor_skill ohne name -> Fehler') do
  hat?(MapCheck.scan(karte_mit([{ 'cmd' => 'actor_skill' }]), **OPTS),
       "'name' (Fertigkeit) fehlt oder ist leer", MapCheck::FEHLER)
end
check('actor_skill mit ungültigem Modus -> Fehler') do
  hat?(MapCheck.scan(karte_mit([{ 'cmd' => 'actor_skill', 'name' => 'eisspeer', 'mode' => 'forget' }]), **OPTS),
       'unbekannter actor_skill-Modus', MapCheck::FEHLER)
end
check('actor_skill mit unbekanntem Skill -> Warnung') do
  hat?(MapCheck.scan(karte_mit([{ 'cmd' => 'actor_skill', 'name' => 'gibtsnicht' }]), **OPTS),
       "Fertigkeit 'gibtsnicht' nicht in database.json", MapCheck::WARNUNG)
end
check('alle 73 Befehlstypen sind registriert') { MapCheck::CMD_TYPES.size == 73 }

puts($fail.zero? ? 'ALLE CHECKS GRUEN' : "#{$fail} CHECK(S) FEHLGESCHLAGEN")
exit($fail.zero? ? 0 : 1)
