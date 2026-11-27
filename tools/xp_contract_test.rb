#encoding: utf-8
# ============================================================================
#  Kopfloser Test für Stufe S2: XP-Dateien 000-004 werden durch die Engine-
#  Wrapper aus rgss.rb ersetzt. Geprüft wird, welche öffentliche XP-API
#  (statisch aus den XP-Dateien gelesen) die Wrapper anbieten.
#  ruby tools/xp_contract_test.rb
#
#  Harte Verträge: Game_Switches/Game_Variables/Game_SelfSwitches ([]/[]=)
#  vollständig. Für Game_Temp/Game_System werden Lücken nur BERICHTET und
#  gegen die dokumentierte Obergrenze geprüft - keine Vollständigkeit
#  behauptet (siehe docs/STUFENPLAN_EDITOR_UND_XP_SCRIPTS.md, S2).
# ============================================================================
module Rune
  ROOT = File.expand_path('..', __dir__)
  module Log
    def self.info(_m) = nil
    def self.warn(m) = puts("WARN  #{m}")
    def self.error(m) = puts("ERROR #{m}")
  end
end

module Game
  @map = { 'name' => 'Test' }
  @map_id = 'test'
  @events = []
  @npcs = []
  class << self
    attr_accessor :map, :map_id, :events, :npcs
  end
  def self.gy(_x, _z) = 0.0
end

module AudioFiles
  def self.play(*) = false
  def self.loop(*) = false
  def self.stop(*) = false
  def self.fade_ms(*) = false
end

$LOAD_PATH.unshift(File.join(Rune::ROOT, 'game', 'engine'))
require 'state'
require 'issues'
require 'party'
require 'weather'
require 'fade'
require 'data'
require 'rgss'
require 'xp_manifest'

$fail = 0
def check(name)
  ok = yield
  puts(ok ? "  ok   #{name}" : "  FAIL #{name}")
  $fail += 1 unless ok
rescue StandardError => e
  puts "  FAIL #{name} (Ausnahme: #{e.class}: #{e.message})"
  $fail += 1
end

puts '== XP-Verträge (S2): 000-004 durch Engine-Wrapper ersetzt =='
entries = Rune::XPManifest.entries.select(&:replaced_by)
check('genau 000-004 sind als ersetzt markiert') { entries.map(&:number) == (0..4).to_a }

check('API-Extraktion erkennt Indexer und attr_accessor') do
  sw = entries.find { |e| e.number == 2 }
  tmp = entries.find { |e| e.number == 0 }
  Rune::XPManifest.public_api(sw.file).sort == %w[[] []=] &&
    Rune::XPManifest.public_api(tmp.file).include?('message_text=')
end

missing = entries.to_h do |e|
  klass = Object.const_get(e.replaced_by)
  [e.number, Rune::XPManifest.missing_api(e, klass)]
end
entries.each do |e|
  gap = missing[e.number]
  api = Rune::XPManifest.public_api(e.file).size
  puts "       #{format('%03d', e.number)} #{e.name} -> #{e.replaced_by}: " \
       "#{api - gap.size}/#{api} XP-Methoden#{gap.empty? ? '' : " (fehlt: #{gap.join(', ')})"}"
end

check('Schalter, Variablen, Self-Switches: XP-API vollständig') do
  [2, 3, 4].all? { |n| missing[n].empty? }
end
check('Game_Temp/Game_System: Lücken berichtet, Engine bietet Teil-API') do
  [0, 1].all? do |n|
    e = entries.find { |x| x.number == n }
    missing[n].size < Rune::XPManifest.public_api(e.file).size
  end
end

# Semantik der harten Verträge (XP: unbekannter Schalter = false).
RGSS.setup
check('unbekannter Schalter liefert false wie XP') { $game_switches[4711] == false }
check('Schalter-Rundlauf über GameState') do
  $game_switches[7] = true
  $game_switches[7] == true && GameState.switch('7') == true
end
check('Variablen-Rundlauf über GameState') do
  $game_variables[3] = 12
  $game_variables[3].to_f == 12.0
end

puts "XP-VERTRÄGE: #{$fail} Fehler"
exit($fail.zero? ? 0 : 1)
