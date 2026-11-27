#encoding: utf-8
# ============================================================================
#  Kopfloser Test für Etappe 28: XP-Defaultscripts im Script-Editor, Laden
#  vor dem Spielstart im Namensraum XP und Backtrace-Fehlerfenster.
#  ruby tools/xp_scripts_test.rb
# ============================================================================
require 'tmpdir'
require 'fileutils'
require 'json'

module Rune
  ROOT = File.expand_path('..', __dir__)
  module Log
    def self.info(_m) = nil
    def self.warn(_m) = nil
    def self.error(_m) = nil
  end
  # Attrappe der C++-Bindung Rune::App.error_box (src/ruby/vm.cpp).
  module App
    BOXES = []
    def self.error_box(title, text) = (BOXES << [title, text])
  end
end

module Game
  def self.gy(_x, _z) = 0.0
end

$LOAD_PATH.unshift(File.join(Rune::ROOT, 'game', 'engine'))
require 'data'
require 'state'
require 'issues'
require 'interpreter'
require 'scripts'
require 'xp_manifest'
load File.join(Rune::ROOT, 'tools', 'xp_scripts_import.rb')

$fail = 0
def check(name)
  ok = yield
  puts(ok ? "  ok   #{name}" : "  FAIL #{name}")
  $fail += 1 unless ok
rescue StandardError => e
  puts "  FAIL #{name} (Ausnahme: #{e.class}: #{e.message})"
  $fail += 1
end

puts '== XP-Defaultscripts im Script-Editor (Etappe 28) =='
entries = XPScriptsImport.entries
check('Import liefert 90 Einträge in Nummernreihenfolge') do
  entries.size == 90 && entries.map { |e| e['xp_default'] } == (0..89).map { |n| format('%03d', n) } &&
    entries.first['name'] == '000 - Game_Temp' && entries.last['name'] == '089 - Main'
end
check('autoload nur für ohne RGSS ladbare Dateien, Main aus') do
  aktiv = entries.select { |e| e['autoload'] }.map { |e| e['xp_default'].to_i }
  aktiv == Rune::XPManifest.loadable.map(&:number) && !aktiv.include?(89)
end
check('Import stellt Defaults vor Projekt-Scripts und ist idempotent') do
  doc = { 'scripts' => [{ 'name' => 'eigenes', 'code' => '1' }], 'extra' => 7 }
  neu, = XPScriptsImport.merge(doc)
  zweit, meldung = XPScriptsImport.merge(neu)
  # XP-Konvention: eigene Scripts über Main, Main bleibt letzter Eintrag.
  neu['scripts'].size == 91 && neu['scripts'][-2]['name'] == 'eigenes' &&
    neu['scripts'].last['name'] == '089 - Main' && neu['extra'] == 7 &&
    zweit.nil? && meldung.include?('bereits')
end
check('C++-Vorgabe (xp_default_scripts.h) entspricht dem Manifest') do
  h = File.read(File.join(Rune::ROOT, 'src', 'editor', 'xp_default_scripts.h'), encoding: 'UTF-8')
  ranges = h.scan(/n >= (\d+) && n <= (\d+)\) return false/).map { |a, b| (a.to_i..b.to_i).to_a }.flatten
  ranges == Rune::XPManifest::NEEDS_RGSS && h.include?('if (n == 89) return false')
end

tmp = Dir.mktmpdir('rune_xp_scripts')
begin
  FileUtils.mkdir_p(File.join(tmp, 'maps'))
  File.write(File.join(tmp, 'system.json'), '{"name":"Test","switches":{},"variables":{}}', encoding: 'UTF-8')
  liste = entries.select { |e| e['autoload'] }.map(&:dup)
  liste << entries.last.merge('autoload' => true)      # trotz autoload nie im Eval-Durchlauf
  liste << { 'name' => 'kaputt', 'autoload' => true, 'code' => "x = 1\nnicht_definiert_xyz\n" }
  liste << { 'name' => 'danach', 'autoload' => true, 'code' => '$xp_test_danach = true' }
  File.write(File.join(tmp, 'scripts.json'), JSON.generate('scripts' => liste), encoding: 'UTF-8')
  $project_root = tmp
  Database.reset!
  Rune::App::BOXES.clear

  check('scripts.json mit Defaults geladen') { Scripts.load_project }
  ok = Scripts.run_startup
  check('Defaults landen im Namensraum XP, nicht global') do
    XP.const_defined?(:Game_Temp, false) && XP.const_defined?(:Scene_Map, false) &&
      !Object.const_defined?(:Game_Temp) && !Object.const_defined?(:Scene_Map)
  end
  check('Engine-Klassen bleiben unberührt') do
    Object.const_get(:Interpreter) == ::Interpreter && !XP.const_defined?(:Window_Selectable, false)
  end
  check('089 Main läuft nicht im Eval-Durchlauf, ist aber als Fiber-Einstieg erkannt') do
    !defined?($xp_main_ran) && $scene.nil? &&
      Scripts.instance_variable_get(:@main_code) == entries.last['code'] &&
      Scripts.instance_variable_get(:@main_name) == '089 - Main'
  end
  check('erster Fehler bricht wie XP ab, folgendes Script läuft nicht') { ok == false && $xp_test_danach.nil? }
  check('genau ein Fehlerfenster im XP-Format mit Backtrace') do
    title, text = Rune::App::BOXES.last
    Rune::App::BOXES.size == 1 && title.include?('Script-Fehler') &&
      text.include?("Script 'kaputt' Zeile 2: NameError ist aufgetreten.") &&
      text.include?('Backtrace:') && text.include?("Script 'kaputt':2") && !text.include?('Section')
  end
  check('nur der kaputte Eintrag ist als Fehler gesammelt') { Scripts.errors.map { |e| e[:script] } == ['kaputt'] }
  Scripts.load_project
  Scripts.run_startup
  check('gleicher Fehler (Titel -> Spielstart) öffnet kein zweites Fenster') { Rune::App::BOXES.size == 1 }
ensure
  $project_root = nil
  FileUtils.rm_rf(tmp)
end

puts "XP-SCRIPTS: #{$fail} Fehler"
exit($fail.zero? ? 0 : 1)
