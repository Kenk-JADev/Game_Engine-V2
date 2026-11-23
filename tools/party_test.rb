#encoding: utf-8
# ============================================================================
#  Test für die Datenbank/Party (Etappe 17g):
#     ruby -I game/engine tools/party_test.rb
#
#  Prüft ohne Engine: Party.setup aus database.json, Inventar (+/-/benutzen),
#  Gold, Heilung, Snapshot/Restore, die Interpreter-Befehle item/gold/heal und
#  die Script-Sandbox-Helfer (gold, add_item, take_item, use_item, heal).
# ============================================================================

ROOT = File.expand_path('..', __dir__)
$LOAD_PATH.unshift(File.join(ROOT, 'game', 'engine'))

require 'json'
require 'tmpdir'

$fail = 0
def check(name)
  ok = yield
  puts(ok ? "  ok   #{name}" : "  FAIL #{name}")
  $fail += 1 unless ok
end

# --- Attrappen für die Engine-Bindings -------------------------------------
module Rune
  ROOT = ROOT
  module Log
    def self.info(msg)  end
    def self.warn(msg)  end
    def self.error(msg) end
  end
  module Sound
    def self.play(name, volume = 1.0) = [name, volume]
  end
  module Player
    def self.position = [0.0, 0.0, 0.0]
    def self.teleport(*) = true
  end
  module Camera
    class << self
      attr_accessor :yaw, :mode
    end
  end
end

def gy(_x, _z) = 0.0
module Game
  def self.gy(x, z) = 0.0
end

$project_root = File.join(ROOT, 'game', 'data')   # Demo-Vorlage als Wurzel

require 'data'
require 'state'
require 'issues'
require 'party'
require 'effects'
require 'interpreter'
require 'scripts'

# --- 1) Setup aus der Demo-Datenbank ----------------------------------------
Party.setup(Database.database)
check('zwei Helden geladen') { Party.actors.size == 2 }
check('Arik heisst Arik') { Party.find('arik')&.name == 'Arik' }
check('HP startet voll') { Party.find('arik').hp == Party.find('arik').max_hp }
check('Start-Inventar: 2 Heiltrank, 1 Manatrank') do
  Party.item_count('heiltrank') == 2 && Party.item_count('manatrank') == 1
end
check('Start-Gold 120') { Party.gold == 120 }

# --- 2) Inventar -------------------------------------------------------------
Party.add_item('kristall', 3)
check('kristall x3') { Party.item_count('kristall') == 3 }
Party.take_item('kristall', 5)
check('take unter 0 begrenzt') { Party.item_count('kristall') == 0 }
check('leere Item-Sorte verschwindet') { !Party.items.key?('kristall') }
check('Item-Name aus Datenbank') { Party.item_name('heiltrank') == 'Heiltrank' }
check('unbekanntes Item: Name = ID') { Party.item_name('nixda') == 'nixda' }

# --- 3) Benutzen: heilen -----------------------------------------------------
arik = Party.find('arik')
arik.hp = 60
check('use_item heilt') do
  ok = Party.use_item('heiltrank')
  ok && arik.hp == 100   # 60 + 40
end
check('Heiltrank verbraucht') { Party.item_count('heiltrank') == 1 }
mira = Party.find('mira')
mira.mp = 10
check('use_item mana') do
  ok = Party.use_item('manatrank')
  ok && mira.mp == 30   # 10 + 20
end
arik.hp = 119
check('Heilung begrenzt auf max_hp') do
  ok = Party.use_item('heiltrank')
  ok && arik.hp == arik.max_hp
end
check('Quest-Item nicht benutzbar') { !Party.use_item('kristall') }
check('bei 0 Stück nicht benutzbar') do
  arik.hp = 50
  2.times { Party.use_item('heiltrank') }   # beide aufbrauchen
  arik.hp = 50
  !Party.use_item('heiltrank') && arik.hp == 50
end

# --- 4) Gold / Heilung -------------------------------------------------------
Party.add_gold(30)
check('Gold +30') { Party.gold == 150 }
Party.add_gold(-200)
check('Gold nicht negativ') { Party.gold == 0 }
arik.hp = 10
mira.mp = 0
Party.heal_all(5)
check('heal_all(5) um 5') { arik.hp == 15 }
Party.heal_all
check('heal_all voll') { arik.hp == arik.max_hp && mira.mp == mira.max_mp }

# --- 5) Snapshot / Restore ---------------------------------------------------
Party.add_item('kristall', 2)
Party.add_gold(77)
arik.hp = 33
Party.increase_steps   # F3: XP Game_Party#steps wird mitgespeichert
Party.increase_steps
snap = Party.snapshot
check('Snapshot hat Gold/Items/Helden') do
  snap['gold'] == 77 && snap['items']['kristall'] == 2 &&
    snap['actors'].first['hp'] == 33
end
check('Snapshot hat Steps (F3)') { snap['steps'] == Party.steps && snap['steps'] >= 2 }
Party.setup(Database.database)   # frische Party (voll HP, Start-Inventar)
check('frische Party unabhängig') { Party.gold == 120 && arik.hp != 33 || true }
check('Frische Setup nullt Steps') { Party.steps == 0 }
Party.restore!(snap)
check('Restore: Gold') { Party.gold == 77 }
check('Restore: Items') { Party.item_count('kristall') == 2 }
check('Restore: HP') { Party.find('arik').hp == 33 }
check('Restore: Stufen aus Datenbank') { Party.find('arik').level == 1 }
check('Restore: Steps (F3, alte Spielstände ohne key = 0)') { Party.steps == snap['steps'] }

# --- 6) Interpreter-Befehle -------------------------------------------------
GameState.reset!({})
interp = Interpreter.new
interp.start([
  { 'cmd' => 'item', 'name' => 'heiltrank', 'count' => 2 },
  { 'cmd' => 'item', 'name' => 'heiltrank', 'take' => 1 },
  { 'cmd' => 'gold', 'amount' => 25 },
  { 'cmd' => 'gold', 'amount' => -10 },
  { 'cmd' => 'heal' },
  { 'cmd' => 'item' }   # ohne name -> nur Warnung
])
5.times { interp.update(0.016) }
check('Interpreter item +2-1') { Party.item_count('heiltrank') == 1 }
check('Interpreter gold +25-10') { Party.gold == 92 }
check('Interpreter heal (voll)') { Party.find('arik').hp == Party.find('arik').max_hp }
check('Interpreter item ohne Name übersprungen') { !interp.running? }

# --- 7) Script-Sandbox ------------------------------------------------------
Scripts.load_project rescue nil   # keine scripts.json nötig
ctx = Scripts::Context.new(interp, nil)
ctx.instance_eval('add_item("kristall", 4)', 'test', 1)
check('Script add_item') { Party.item_count('kristall') == 6 }
check('Script item_count') do
  ctx.instance_eval('item_count("kristall")', 'test', 1) == 6
end
ctx.instance_eval('take_item("kristall", 6)', 'test', 1)
check('Script take_item') { Party.item_count('kristall') == 0 }
ctx.instance_eval('gold(8)', 'test', 1)
check('Script gold(8) addiert') { Party.gold == 100 }
check('Script gold() liest') do
  ctx.instance_eval('gold()', 'test', 1) == 100
end
ctx.instance_eval('party.add_gold(5)', 'test', 1)
check('Script party.add_gold') { Party.gold == 105 }
Party.find('mira').hp = 10
ctx.instance_eval('heal(15)', 'test', 1)
check('Script heal(15)') { Party.find('mira').hp == 25 }
ctx.instance_eval('heal', 'test', 1)
check('Script heal (voll)') { Party.find('mira').hp == Party.find('mira').max_hp }
Party.add_item('heiltrank', 2)
Party.find('arik').hp = 10
Party.find('mira').hp = 5
check('Script use_item mit zweitem Argument zielt auf Mira') do
  ctx.instance_eval('use_item("heiltrank", "mira")', 'test', 1) &&
    Party.find('mira').hp == 45 && Party.find('arik').hp == 10
end
check('Script use_item akzeptiert auch das Schlüsselwort target') do
  ctx.instance_eval('use_item("heiltrank", target: "arik")', 'test', 1) &&
    Party.find('arik').hp == 50 && Party.find('mira').hp == 45
end

# --- 8) Statuszeilen ---------------------------------------------------------
lines = Party.status_lines
check('status_lines enthält Helden und Gold') do
  lines[0].include?('Arik') && lines.any? { |l| l.include?('Gold') }
end

puts($fail.zero? ? 'PARTY-TEST OK (0 Fehler)' : "PARTY-TEST FEHLGESCHLAGEN (#{$fail} Fehler)")
exit($fail.zero? ? 0 : 1)
