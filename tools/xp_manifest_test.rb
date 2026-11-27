#encoding: utf-8
# ============================================================================
#  Kopfloser Test für Stufe S1: XP-Defaultscript-Manifest.
#  ruby tools/xp_manifest_test.rb
#  Das isolierte Laden läuft in einem eigenen Ruby-Kindprozess, damit keine
#  XP-Klassen in diesen Prozess (oder gar die Engine) gelangen.
# ============================================================================
require 'open3'
require 'rbconfig'

module Rune
  ROOT = File.expand_path('..', __dir__)
end
$LOAD_PATH.unshift(File.join(Rune::ROOT, 'game', 'engine'))
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

puts '== XP-Manifest (S1) =='
entries = Rune::XPManifest.entries
check('alle 90 Dateien erfasst') { entries.size == 90 }
check('Nummern lückenlos 0..89 und sortiert') { entries.map(&:number) == (0..89).to_a }
check('jede Datei auf der Platte hat einen Eintrag') do
  Dir[File.join(Rune::XPManifest.dir, '*.rb')].size == entries.size
end
main = entries.find { |e| e.number == 89 }
check('089 Main ist gesperrt und nicht ladbar') do
  main.stage == :blocked && Rune::XPManifest.loadable.none? { |e| e.number == 89 }
end
check('Window-/Sprite-Dateien brauchen RGSS') do
  %w[Window_Base Sprite_Character Arrow_Base].all? { |n| entries.find { |e| e.name == n }.stage == :needs_rgss }
end
check('Konflikte mit Engine-Klassen erkannt') do
  by = ->(n) { entries.find { |e| e.name == n }.conflicts }
  by.('Scene_Title') == ['Scene_Title'] && by.('Interpreter 1') == ['Interpreter'] &&
    by.('Window_Selectable') == ['Window_Selectable'] && by.('Game_Temp').empty?
end
check('Ladereihenfolge folgt der Nummerierung') do
  nums = Rune::XPManifest.loadable.map(&:number)
  nums == nums.sort && nums.first.zero?
end

# Isoliertes Laden im Kindprozess.
script = <<~RUBY
  module Rune; ROOT = #{Rune::ROOT.dump}; end
  $LOAD_PATH.unshift(File.join(Rune::ROOT, 'game', 'engine'))
  require 'xp_manifest'
  res = Rune::XPManifest.load_isolated!
  res.each { |n, err| puts "\#{n}\\t\#{err || 'ok'}" }
  puts "KLASSEN\\t\#{%w[Game_Temp Game_Party Game_Map Interpreter Scene_Map].all? { |c| Object.const_defined?(c) }}"
RUBY
out, err, status = Open3.capture3(RbConfig.ruby, '-e', script)
lines = out.lines.map { |l| l.chomp.split("\t", 2) }
results = lines.reject { |k, _| k == 'KLASSEN' }.to_h { |k, v| [k.to_i, v] }
check("Kindprozess läuft (#{status.exitstatus})") { status.success? && err.strip.empty? }
failed = results.reject { |_, v| v == 'ok' }
failed.each { |n, v| puts "       #{n}: #{v}" }
check("alle :definition-Dateien laden isoliert (#{results.size})") do
  failed.empty? && results.size == Rune::XPManifest.loadable.size
end
check('Kernklassen nach dem Laden vorhanden') { lines.assoc('KLASSEN')&.last == 'true' }
check('Guard verhindert Laden im Engine-Prozess') do
  Object.const_set(:Graphics, Module.new)
  begin
    Rune::XPManifest.load_isolated!
    false
  rescue RuntimeError
    true
  ensure
    Object.send(:remove_const, :Graphics)
  end
end

puts "XP-MANIFEST: #{$fail} Fehler"
exit($fail.zero? ? 0 : 1)
