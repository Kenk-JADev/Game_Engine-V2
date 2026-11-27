#encoding: utf-8
# ============================================================================
#  Windows-Integrationstest für den kopflosen WIC-Import.
#  Schreibt NUR in ein neu angelegtes, eindeutig benanntes Testprojekt und ein
#  eigenes temporäres Quellverzeichnis. Kein Backup/Restore echter Projekte:
#  dadurch gehen zeitgleiche Benutzeränderungen niemals durch den Test verloren.
# ============================================================================
require 'json'
require 'fileutils'
require 'tmpdir'
require 'open3'

ROOT = File.expand_path('..', __dir__)
EXE = File.join(ROOT, 'build', 'RuneEditor.exe')
raise 'build/RuneEditor.exe fehlt - erst .\\build.ps1 ausführen' unless File.exist?(EXE)

$fail = 0
def check(name)
  ok = yield
  puts(ok ? "  ok   #{name}" : "  FAIL #{name}")
  $fail += 1 unless ok
rescue StandardError => e
  puts "  FAIL #{name}: #{e.class}: #{e.message}"
  $fail += 1
end

def run_import(source, name, project, exe: EXE, cwd: nil)
  options = cwd ? { chdir: cwd } : {}
  output, status = Open3.capture2e(exe, '--import-tex', source, name, '--project', project, **options)
  puts "  ... Exit #{status.exitstatus}: #{output.strip}"
  status.exitstatus
end

tmp = nil
project_dir = nil
begin
  tmp = Dir.mktmpdir('rune_tex_source_')
  # Umlaute, CJK und ergänzende Unicode-Zeichen liegen sowohl im Projekt-
  # als auch im Quelldateinamen; sie müssen die UTF-16-CLI exakt passieren.
  project_dir = Dir.mktmpdir('rune_ä_日_🚀_', File.join(ROOT, 'projekte'))
  project = File.basename(project_dir)
  data_dir = File.join(project_dir, 'data')
  tex_dir = File.join(data_dir, 'textures')
  json_path = File.join(data_dir, 'textures.json')
  FileUtils.mkdir_p(tex_dir)
  bmp = File.join(tmp, 'bild_ä_日_🚀.bmp')

  # 8x8-BMP (24 Bit): obere Hälfte rot, untere blau. BMP-Dateizeilen sind
  # bottom-up, die von WIC gelesenen RGBA-Zeilen müssen top-down bleiben.
  w = h = 8
  rows = Array.new(h) { |y| (y < 4 ? [0, 0, 255] : [255, 0, 0]).pack('C3') * w }
  pixels = rows.reverse.join
  dib = [40, w, h, 1, 24, 0, pixels.bytesize, 2835, 2835, 0, 0]
        .pack('V l< l< v v V V l< l< V V')
  File.binwrite(bmp, 'BM' + [14 + 40 + pixels.bytesize, 0, 54].pack('V V V') + dib + pixels)

  original = {
    'future' => { 'counter' => 9_007_199_254_740_993, 'unknown' => [false, nil] },
    'textures' => [
      false,
      { 'name' => 'alt', 'file' => 'alt.rgba', 'w' => 1, 'h' => 1,
        'tile' => 1.2345678901234567, 'extra' => { 'keep' => true } },
      { 'name' => 'ungueltig', 'file' => '../nicht-lesen', 'w' => 1.0e300 }
    ]
  }
  File.write(json_path, JSON.generate(original), encoding: 'UTF-8')
  occupied = File.join(tex_dir, 'testbild.rgba')
  File.binwrite(occupied, 'FREMDE DATEN')

  code = run_import(bmp, 'testbild', project)
  check('CLI-Import in isoliertes Unicode-Projekt erfolgreich') { code.zero? }
  meta = JSON.parse(File.read(json_path, encoding: 'UTF-8'))
  entries = meta.fetch('textures')
  entry = entries.find { |e| e.is_a?(Hash) && e['name'] == 'testbild' }
  check('unbekannte Wurzel, ungültige Einträge und altes Tile erhalten') do
    meta['future'] == original['future'] && entries.first(3) == original['textures']
  end
  check('Eintrag mit 8 x 8 und Kachel vorhanden') do
    entry && entry['w'] == 8 && entry['h'] == 8 && entry['tile'] == 2.0
  end
  check('bestehende RGBA-Kollision nicht überschrieben') do
    entry && entry['file'] != 'testbild.rgba' && File.binread(occupied) == 'FREMDE DATEN'
  end
  rgba_path = File.join(tex_dir, entry ? entry['file'].to_s : 'nicht-vorhanden')
  check('RGBA-Datei hat korrekte Grösse und Zeilenfolge') do
    bytes = File.binread(rgba_path)
    bytes.bytesize == 256 && bytes.byteslice(0, 4).bytes == [255, 0, 0, 255] &&
      bytes.byteslice(224, 4).bytes == [0, 0, 255, 255]
  end

  manifest_after = File.binread(json_path)
  names_after = Dir.children(tex_dir).sort
  check('gleicher Materialname wird ohne Überschreiben abgewiesen') do
    run_import(bmp, 'testbild', project) != 0 && File.binread(json_path) == manifest_after &&
      Dir.children(tex_dir).sort == names_after && File.binread(occupied) == 'FREMDE DATEN'
  end
  check('Projekt-Pfadflucht wird abgewiesen') do
    run_import(bmp, 'pfadflucht', '..') != 0 && File.binread(json_path) == manifest_after
  end
  check('Dateiname mit Pfadanteil wird abgewiesen') do
    run_import(bmp, '../pfadflucht', project) != 0 && File.binread(json_path) == manifest_after &&
      Dir.children(tex_dir).sort == names_after
  end
  huge = File.join(tmp, 'zu_gross.bmp')
  File.open(huge, 'wb') { |file| file.truncate(64 * 1024 * 1024 + 1) }
  check('übergrosse Quelldatei wird vor WIC und Schreiben abgewiesen') do
    run_import(huge, 'zu_gross', project) != 0 && File.binread(json_path) == manifest_after &&
      Dir.children(tex_dir).sort == names_after
  end
  corrupt = "{\"textures\": [\n"
  File.binwrite(json_path, corrupt)
  check('beschädigtes Manifest bleibt bytegenau erhalten') do
    run_import(bmp, 'kein_import', project) != 0 && File.binread(json_path) == corrupt &&
      Dir.children(tex_dir).sort == names_after
  end

  # Eine Kopie der echten Editor-EXE aus einer Unicode-Installation starten,
  # absichtlich mit einem anderen Arbeitsverzeichnis. Nichts im Repository oder
  # in einem bestehenden Projekt wird dafür verschoben oder überschrieben.
  install = File.join(tmp, 'Engine_ä_日_🚀')
  copy_build = File.join(install, 'build')
  FileUtils.mkdir_p([copy_build, File.join(install, 'game')])
  copy_exe = File.join(copy_build, 'RuneEditor.exe')
  FileUtils.cp(EXE, copy_exe)
  Dir.glob(File.join(ROOT, 'build', '*.dll')).each { |dll| FileUtils.cp(dll, copy_build) }
  %w[RuneEditor.exe.manifest ruby_builtin_dlls].each do |part|
    src = File.join(ROOT, 'build', part)
    FileUtils.cp_r(src, copy_build) if File.exist?(src)
  end
  copy_project = ' Projekt_ä_日_🚀'
  copy_data = File.join(install, 'projekte', copy_project, 'data')
  FileUtils.mkdir_p(copy_data)
  copy_manifest = File.join(copy_data, 'textures.json')
  copy_initial = { 'future' => { 'keep' => true }, 'textures' => [] }
  File.write(copy_manifest, JSON.generate(copy_initial), encoding: 'UTF-8')
  copy_code = run_import(bmp, 'unicode_bild', copy_project, exe: copy_exe, cwd: tmp)
  check('echte Editor-CLI findet Unicode-Installation und Projekt trotz fremdem CWD') do
    copy_code.zero? && File.binread(json_path) == corrupt
  end
  check('Unicode-Installation: Manifest und tatsächliche RGBA-Pixel erhalten') do
    copy_meta = JSON.parse(File.read(copy_manifest, encoding: 'UTF-8'))
    item = copy_meta.fetch('textures').find { |e| e['name'] == 'unicode_bild' }
    pixels = item && File.binread(File.join(copy_data, 'textures', item['file']))
    copy_meta['future'] == copy_initial['future'] && pixels && pixels.bytesize == 256 &&
      pixels.byteslice(0, 4).bytes == [255, 0, 0, 255] &&
      pixels.byteslice(224, 4).bytes == [0, 0, 255, 255]
  end
ensure
  # Nur eigene temporäre Verzeichnisse bereinigen, niemals dorf oder andere Projekte.
  FileUtils.remove_entry(project_dir) if project_dir && Dir.exist?(project_dir)
  FileUtils.remove_entry(tmp) if tmp && Dir.exist?(tmp)
end

puts($fail.zero? ? 'TEX-IMPORT-TEST OK (0 Fehler)' : "TEX-IMPORT-TEST FEHLGESCHLAGEN (#{$fail} Fehler)")
exit($fail.zero? ? 0 : 1)
