#encoding: utf-8
# Prüft die Live-Testkopie einschließlich Ruby-Laufzeit nur in temporären
# Installationen. Echte Builds und Benutzerprojekte bleiben unangetastet.
require 'fileutils'
require 'json'
require 'open3'
require 'rbconfig'
require 'tmpdir'

ROOT = File.expand_path('..', __dir__)
$fail = 0

def check(name)
  ok = yield
  puts(ok ? "  ok   #{name}" : "  FAIL #{name}")
  $fail += 1 unless ok
rescue StandardError => e
  puts "  FAIL #{name}: #{e.class}: #{e.message}"
  $fail += 1
end

Dir.mktmpdir('rune_live_ä_日_🚀_') do |temp|
  source = File.join(temp, 'quellkopie')
  build = File.join(source, 'build')
  tools = File.join(source, 'tools')
  FileUtils.mkdir_p([build, tools, File.join(source, 'game')])
  FileUtils.cp_r(File.join(ROOT, 'game', 'data'), File.join(source, 'game'))
  script = File.join(tools, 'prepare_material_live.rb')
  FileUtils.cp(File.join(ROOT, 'tools', 'prepare_material_live.rb'), script)
  %w[RuneEditor.exe Rune3D.exe].each { |name| File.binwrite(File.join(build, name), name) }
  File.binwrite(File.join(build, 'ruby.dll'), 'DLL')
  FileUtils.mkdir_p(File.join(build, 'ruby_builtin_dlls'))
  File.binwrite(File.join(build, 'ruby_builtin_dlls', 'beispiel.dll'), 'BUILTIN')
  stdlib = File.join(build, 'lib', 'ruby', '3.4.0', 'x64-mingw-ucrt', 'json', 'ext')
  FileUtils.mkdir_p(stdlib)
  File.binwrite(File.join(stdlib, 'parser.so'), 'PARSER')
  File.binwrite(File.join(build, 'lib', 'ruby', '3.4.0', 'json.rb'), 'JSON')

  target = File.join(temp, 'testkopie')
  output, status = Open3.capture2e(RbConfig.ruby, script, target)
  check('isolierte Kopie erstellt') { status.success? && Dir.exist?(target) }
  check('Ruby-Standardbibliothek und native Erweiterung mitkopiert') do
    File.binread(File.join(target, 'build', 'lib', 'ruby', '3.4.0', 'json.rb')) == 'JSON' &&
      File.binread(File.join(target, 'build', 'lib', 'ruby', '3.4.0', 'x64-mingw-ucrt',
                             'json', 'ext', 'parser.so')) == 'PARSER'
  end
  check('EXEs, Laufzeit-DLLs und SxS-Ordner mitkopiert') do
    File.binread(File.join(target, 'build', 'RuneEditor.exe')) == 'RuneEditor.exe' &&
      File.binread(File.join(target, 'build', 'ruby.dll')) == 'DLL' &&
      File.binread(File.join(target, 'build', 'ruby_builtin_dlls', 'beispiel.dll')) == 'BUILTIN'
  end
  check('beide isolierten Materialprojekte angelegt') do
    %w[A_material B_material].all? do |name|
      path = File.join(target, 'projekte', name, 'data', 'maps', 'dorf.json')
      JSON.parse(File.read(path, encoding: 'UTF-8'))['name'] == name
    end
  end
  marker = File.join(target, 'behalten.txt')
  File.binwrite(marker, 'NICHT ÜBERSCHREIBEN')
  _output, status = Open3.capture2e(RbConfig.ruby, script, target)
  check('vorhandenes Ziel wird nicht überschrieben') do
    !status.success? && File.binread(marker) == 'NICHT ÜBERSCHREIBEN'.b
  end
  FileUtils.rm_rf(File.join(build, 'lib', 'ruby'))
  missing = File.join(temp, 'ohne-ruby')
  _output, status = Open3.capture2e(RbConfig.ruby, script, missing)
  check('fehlende Ruby-Bibliothek wird vor Erstellen des Ziels gemeldet') do
    !status.success? && !File.exist?(missing)
  end
  warn output unless $fail.zero?
end

puts($fail.zero? ? 'LIVE-KOPIE-TEST OK (0 Fehler)' : "LIVE-KOPIE-TEST FEHLGESCHLAGEN (#{$fail} Fehler)")
exit($fail.zero? ? 0 : 1)
