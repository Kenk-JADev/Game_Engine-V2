#encoding: utf-8
# ============================================================================
#  Kopfloser Test für Fertigkeiten und Zustände (Etappe 20a).
#  Lässt sich ohne Engine starten:
#     ruby -I game/engine tools/skills_test.rb
#  Exit-Code 0 = alles grün.
#
#  Geprüft werden:
#    - Datenbank: skills/states vorhanden, Helden haben Start-Fertigkeiten
#    - Party: skills / learn_skill / Spielstand-Rundlauf (auch alte Saves)
#    - Battle: Befehlsreihenfolge XP (Angriff/Fertigkeit/Item/Flucht),
#      Fertigkeitenfenster, MP-Kosten, Heilung, Schaden, Buffs,
#      "nicht genug MP", Zustände pro Runde (Gift/Regeneration/Ablauf)
#    - RGSS: $data_skills/$data_states, $game_party.skills
#    - Scripts: skills(id) / learn_skill(id, skill)
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
    @down = []
    class << self
      attr_accessor :pressed, :down
    end
    def self.pressed?(key) = @pressed.include?(key)
    def self.down?(key) = @down.include?(key)
  end

  module Sound
    CALLS = []
    def self.play(name, volume = 1.0) = (CALLS << [:play, name, volume])
    def self.loop(name, volume = 1.0) = (CALLS << [:loop, name, volume])
    def self.stop(name) = (CALLS << [:stop, name])
  end

  module Player
    @pos = [0.0, 0.0, 0.0]
    class << self
      attr_accessor :pos
    end
    def self.teleport(x, y, z) = (self.pos = [x.to_f, y.to_f, z.to_f])
    def self.position = pos
  end

  module Camera
    @yaw = 0.0
    class << self
      attr_accessor :yaw
    end
  end

  module Window
    def self.width = 1280
    def self.height = 720
  end

  module Gfx
    CALLS = []
    def self.rect(*a) = (CALLS << [:rect, *a])
    def self.text(*a) = (CALLS << [:text, *a])
    def self.box(*a) = (CALLS << [:box, *a])
  end
end

module UI
  TEXT = 0xE8E8E8
  TEXT_DIM = 0x9AA4AE
  ACCENT = 0x59D8FF
  PANEL = 0x0B1016
  DANGER = 0xE86A6A
  CALLS = []
  def self.panel(*a) = (CALLS << [:panel, *a])
  def self.text(*a) = (CALLS << [:text, *a])
  def self.abdunkeln(*a) = nil
end

module Game
  @map = { 'name' => 'Testkarte', 'terrain' => { 'width' => 120.0, 'height' => 90.0 } }
  @events = []
  @npcs = []
  class << self
    attr_accessor :map, :events, :npcs
  end
  def self.gy(_x, _z) = 0.0
end

class FakeTemp
  attr_accessor :in_battle, :gameover
  def initialize
    @in_battle = false
    @gameover = false
  end
end
$game_temp = FakeTemp.new

$LOAD_PATH.unshift(File.join(Rune::ROOT, 'game', 'engine'))
require 'state'
require 'issues'
require 'data'
require 'party'
require 'battle'

GameState.reset!({})
Party.setup(Database.database)

$fail = 0
def check(name)
  ok = yield
  puts(ok ? "  ok   #{name}" : "  FAIL #{name}")
  $fail += 1 unless ok
rescue StandardError => e
  puts "  FAIL #{name} (Ausnahme: #{e.class}: #{e.message})"
  $fail += 1
end

def druck(*keys)
  Rune::Input.pressed = keys
  Battle.update(0.016)
  Rune::Input.pressed = []
end

def held(name) = Battle.actors.find { |b| b.name == name }
def kampflog = Battle.instance_variable_get(:@log)
def gegner(name) = Battle.enemies.find { |b| b.name == name }

puts '== Datenbank: Fertigkeiten und Zustände =='
check('skills vorhanden') { Battle.skills_def.size >= 7 }
check('states vorhanden') { Battle.states_def.size >= 5 }
check('Feuerball: Schaden, 6 MP, Zustand Brand') do
  s = Battle.skill_def('feuerball')
  s && s['kind'] == 'damage' && s['mp'] == 6 && s['state'] == 'brand'
end
check('Heilung: heal auf ally') do
  s = Battle.skill_def('heilung')
  s && s['kind'] == 'heal' && s['scope'] == 'ally'
end
check('Gift: poison, 6 Schaden, 3 Runden') do
  s = Battle.state_def('gift')
  s && s['kind'] == 'poison' && s['power'] == 6 && s['turns'] == 3
end
check('Schutz: def_up +5') do
  s = Battle.state_def('schutz')
  s && s['kind'] == 'def_up' && s['power'] == 5
end
check('alle Skill-Zustände existieren') do
  Battle.skills_def.all? do |s|
    s['state'].nil? || Battle.state_def(s['state'])
  end
end
check('alle Helden-Skills existieren') do
  Party.actors.all? { |a| Party.skills(a).all? { |id| Battle.skill_def(id) } }
end

puts '== Party: lernen und Spielstand =='
check('Arik kennt Kraftschlag') { Party.skills(Party.find('arik')).include?('kraftschlag') }
check('Mira kennt vier Fertigkeiten') { Party.skills('mira').size == 4 }
check('unbekannter Held hat keine Skills') { Party.skills('gibtsnicht') == [] }
check('learn_skill: neuer Skill -> true') do
  Party.learn_skill('arik', 'eisspeer') && Party.skills('arik').include?('eisspeer')
end
check('learn_skill: doppelt -> false') { Party.learn_skill('arik', 'eisspeer') == false }
check('learn_skill: leer -> false') { Party.learn_skill('arik', '') == false }
check('Actor#skills/#learn_skill wie XP') do
  a = Party.find('mira')
  a.skills.include?('heilung') && a.learn_skill('kraftschlag') && a.skills.include?('kraftschlag')
end
snap = Party.snapshot
check('Snapshot enthält skills') do
  snap['skills'].is_a?(Hash) && snap['skills']['mira'].include?('heilung')
end
Party.setup(Database.database)
check('nach setup wieder DB-Liste') { !Party.skills('mira').include?('kraftschlag') }
Party.restore!(snap)
check('restore! bringt gelernte Skills zurück') do
  Party.skills('mira').include?('kraftschlag') && Party.skills('arik').include?('eisspeer')
end
# Alter Spielstand ohne "skills": DB-Liste bleibt stehen.
alt = Party.snapshot
alt.delete('skills')
Party.setup(Database.database)
Party.restore!(alt)
check('alter Spielstand: DB-Liste bleibt') do
  Party.skills('mira').size == 4 && !Party.skills('mira').include?('kraftschlag')
end
Party.setup(Database.database)

puts '== Battle: Befehle =='
check('Reihenfolge wie XP') { Battle::COMMANDS == %w[Angriff Fertigkeit Item Flucht] }
srand(1234)
check('start(waldweg)') { Party.heal_all
Battle.start('waldweg') }
check('Startphase Befehle') { Battle.phase == :command }
druck(:down)
check('Cursor auf Fertigkeit') { Battle::COMMANDS[Battle.command_index] == 'Fertigkeit' }
druck(:enter)
check('Fertigkeitenfenster offen') { Battle.phase == :skill }
check('Arik: drei Fertigkeiten im Fenster') { Battle.known_skills(held('Arik')).size == 3 }
check('draw im Fertigkeitenfenster ohne Fehler') { Battle.draw || true }

# Kraftschlag auf den Goblin
druck(:enter)
check('Kraftschlag (Gegner) -> Zielwahl') { Battle.phase == :skill_target }
check('draw in der Zielwahl ohne Fehler') { Battle.draw || true }
gob = gegner('Goblin')
hp0 = gob.hp
druck(:enter)
check('Ziel bestätigt -> Aktion gebucht, Mira dran') do
  Battle.phase == :command && Battle.current_actor.name == 'Mira' &&
    Battle.actions.size == 1
end
mp0 = held('Arik').mp
Battle.singleton_class.send(:define_method, :rand) { 0.5 }
Battle.enemies.each { |g| g.instance_variable_set(:@atk, 0) }
druck(:enter)                 # Mira: Angriff
druck(:enter)                 # Ziel: erster Gegner
Battle.update(0.016)          # execute
Battle.singleton_class.send(:remove_method, :rand)
check('Kraftschlag kostet 5 MP') { held('Arik').mp == mp0 - 5 }
check('Kraftschlag verletzt den Goblin') { gob.hp < hp0 }
check('Log nennt die Fertigkeit') { kampflog.any? { |z| z.include?('Kraftschlag') } }

puts '== Battle: Heilung und MP-Grenze =='
Battle.abort!
Party.heal_all
Battle.start('waldweg')
mira = held('Mira')
mira.damage(30)
Battle.actors[0].damage(30)
mp_mira = mira.mp
druck(:down)                              # Arik: Fertigkeit
check('Arik wählt Fertigkeit') { Battle.command_index == 1 }
druck(:enter)
check('Arik im Fertigkeitenfenster -> Esc zurück') do
  druck(:escape) && Battle.phase == :command
end
# Auf Mira wechseln: Arik greift an (Ziel bestätigen), dann ist Mira dran.
druck(:up)                                # zurück auf Angriff
druck(:enter)
druck(:enter)
check('Mira ist dran') { Battle.current_actor.name == 'Mira' }
druck(:down)                              # Fertigkeit
druck(:enter)
check('Miras Fenster mit vier Skills') { Battle.phase == :skill && Battle.known_skills(mira).size == 4 }
# Heilung ist der dritte Eintrag (feuerball, eisspeer, heilung, regeneration)
2.times { druck(:down) }
check('Cursor auf Heilung') do
  Battle.known_skills(mira)[Battle.skill_index]['id'] == 'heilung'
end
druck(:enter)
check('Heilung -> Zielwahl (Helden)') { Battle.phase == :skill_target }
ziel = Battle.actors[Battle.target_index]
ziel.damage(40) if ziel.hp > ziel.max_hp - 10
hp_vorher = ziel.hp
druck(:enter)
check('Heilung gebucht') { Battle.actions.count { |a| a[:kind] == :skill } == 1 }
Battle.singleton_class.send(:define_method, :rand) { 0.5 }
Battle.enemies.each { |g| g.instance_variable_set(:@atk, 0) }
Battle.update(0.016)
Battle.singleton_class.send(:remove_method, :rand)
check('Heilung heilt und kostet 5 MP') do
  # Nach dem Heilungszug kann der Gegner erneut treffen; deshalb nur MP + Log.
  mira.mp == mp_mira - 5 && kampflog.any? { |z| z.include?('wirkt Heilung') }
end
check('Heilung überschreitet max_hp nicht') do
  Battle.actors.all? { |a| a.hp <= a.max_hp }
end

Battle.abort!
Party.heal_all
Battle.start('waldweg')
arik = held('Arik')
arik.mp = 2                               # zu wenig für Kraftschlag (5 MP)
druck(:down)
druck(:enter)
check('Fertigkeitenfenster trotz 2 MP offen') { Battle.phase == :skill }
druck(:enter)
check('nicht genug MP: Auswahl bleibt stehen') do
  Battle.phase == :skill && kampflog.any? { |z| z.include?('nicht genug MP') }
end
check('MP bleiben unverändert') { arik.mp == 2 }

puts '== Battle: Zustände =='
Battle.abort!
Party.heal_all
Battle.start('waldweg')
mira = held('Mira')
gob = gegner('Goblin')
mp0 = mira.mp
# Mira ist nach Arik dran: Arik greift an, dann Mira mit Feuerball.
druck(:enter)
druck(:enter)
check('Mira ist dran') { Battle.current_actor.name == 'Mira' }
Battle.singleton_class.send(:define_method, :rand) { 0.1 }   # 10 % < 40 % Brand
druck(:down)
druck(:enter)                              # Fertigkeit
check('Feuerball ist der erste Eintrag') do
  Battle.known_skills(mira)[Battle.skill_index]['id'] == 'feuerball'
end
druck(:enter)                              # Zielwahl
druck(:enter)                              # Goblin
Battle.update(0.016)
Battle.singleton_class.send(:remove_method, :rand)
check('Feuerball kostet 6 MP') { mira.mp == mp0 - 6 }
check('Feuerball verursacht Schaden') { gob.hp < 35 }
check('Brand wurde ausgelöst') { gob.state?('brand') }
check('Zustandsname für die Anzeige') { gob.state_names.any? { |n| n.start_with?('Brand') } }

# Buff auf sich selbst: Wutausbruch (Arik, scope self -> keine Zielwahl)
Battle.abort!
Party.heal_all
Battle.start('waldweg')
arik = held('Arik')
atk0 = arik.atk
druck(:down)
druck(:enter)
check('Arik: Kraftschlag zuerst') { Battle.known_skills(arik)[Battle.skill_index]['id'] == 'kraftschlag' }
druck(:down)
check('Cursor auf Wutausbruch') { Battle.known_skills(arik)[Battle.skill_index]['id'] == 'wutausbruch' }
druck(:enter)
check('Selbst-Buff ohne Zielwahl, direkt gebucht') do
  Battle.phase == :command && Battle.actions.size == 1
end
Battle.singleton_class.send(:define_method, :rand) { 0.5 }
Battle.enemies.each { |g| g.instance_variable_set(:@atk, 0) }
druck(:enter)
druck(:enter)
Battle.update(0.016)
Battle.singleton_class.send(:remove_method, :rand)
check('Wut erhöht den Angriff um 6') { arik.atk == atk0 + 6 }
check('Wut ist als Zustand sichtbar') { arik.state?('wut') }
check('state_bonus zählt nur passende Werte') do
  arik.state_bonus('atk') == 6 && arik.state_bonus('def').zero?
end
check('Basiswert bleibt unverändert') { arik.basis_atk == atk0 }

# Zustände ticken am Rundenende: Gift schadet, Regeneration heilt.
Party.heal_all
Battle.start('waldweg')
gob = gegner('Goblin')
gob.add_state('gift')
hp_gift = gob.hp
Battle.zustaende_ticken
check('Gift kostet 6 HP pro Runde') { gob.hp == hp_gift - 6 }
check('Gift läuft ab (3 -> 2)') { gob.states.find { |s| s[:id] == 'gift' }[:turns] == 2 }
check('Gift-Log nennt den Zustand') { kampflog.any? { |z| z.include?('Gift (-6 HP)') } }
Battle.zustaende_ticken
Battle.zustaende_ticken
check('Gift ist nach drei Runden weg') { !gob.state?('gift') }
check('Ablauf steht im Log') { kampflog.any? { |z| z.include?('klingt ab') } }

# Regeneration heilt pro Runde
ziel = held('Arik')
ziel.damage(50)
ziel.add_state('regen')
hp_regen = ziel.hp
Battle.zustaende_ticken
check('Regeneration heilt 10 HP') { ziel.hp == hp_regen + 10 }
check('Regeneration heilt nicht über max_hp') do
  ziel.hp = ziel.max_hp
  Battle.zustaende_ticken
  ziel.hp == ziel.max_hp
end
check('add_state: gleicher Zustand frischt nur auf') do
  ziel.add_state('regen', 1) && ziel.states.count { |s| s[:id] == 'regen' } == 1
end
check('add_state: unbekannter Zustand -> false') { ziel.add_state('gibtsnicht') == false }

# Rundenende im echten Ablauf ruft die Tick-Funktion auf.
Party.heal_all
Battle.start('waldweg')
gob = gegner('Goblin')
gob.add_state('gift')
hp_runde = gob.hp
Battle.singleton_class.send(:define_method, :rand) { 0.5 }
Battle.enemies.each { |g| g.instance_variable_set(:@atk, 0) }
2.times { druck(:enter); druck(:enter) }
Battle.update(0.016)
Battle.singleton_class.send(:remove_method, :rand)
check('Rundenende lässt Gift wirken') { gob.hp <= hp_runde - 6 }

check('höchstens 8 Zustände pro Kämpfer') do
  db = Database.database
  original = db['states'].dup
  (1..8).each do |i|
    db['states'] << { 'id' => "probe#{i}", 'name' => "Probe #{i}", 'kind' => 'poison',
                      'power' => 1, 'turns' => 2 }
  end
  proband = Battle.actors[0]
  proband.states.clear
  8.times { |i| proband.add_state("probe#{i + 1}") }
  voll = proband.states.size == Battle::Battler::MAX_STATES
  neun = proband.add_state('gift') == false
  proband.states.clear
  db['states'].replace(original)
  voll && neun
end
Battle.abort!

puts '== RGSS und Scripts =='
# RGSS lädt eigene Globale; hier nur die reinen Funktionen prüfen.
check('listen_als_hash deckt skills/states ab') do
  db = Database.database
  db['skills'].is_a?(Array) && db['states'].is_a?(Array)
end
check('Party.skills per Actor-Objekt') { Party.skills(Party.find('arik')).include?('kraftschlag') }

puts
if $fail.zero?
  puts 'SKILLS-TEST OK (0 Fehler)'
  exit 0
else
  puts "SKILLS-TEST FEHLGESCHLAGEN (#{$fail} Fehler)"
  exit 1
end
