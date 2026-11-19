#encoding: utf-8
# ============================================================================
#  Test für den kopflosen Grafik-Import (Etappe 17f):
#     ruby tools/tex_import_test.rb
#
#  Erzeugt ein kleines BMP, ruft RuneEditor.exe --import-tex auf und prüft
#  textures.json + die rohe RGBA-Datei (Grösse, Zeilenreihenfolge, Farben).
#  Der Projektzustand wird am Ende wiederhergestellt.
# ============================================================================

require 'json'
require 'fileutils'
require 'tmpdir'
require 'open3'

ROOT = File.expand_path('..', __dir__)
EXE  = File.join(ROOT, 'build', 'RuneEditor.exe')
DATA = File.join(ROOT, 'projekte', 'dorf', 'data')
JSON_PATH = File.join(DATA, 'textures.json')
TEX_DIR   = File.join(DATA, 'textures')

raise 'build/RuneEditor.exe fehlt - erst .\\build.ps1 ausführen' unless File.exist?(EXE)

$fail = 0
def check(name)
  ok = yield
  puts(ok ? "  ok   #{name}" : "  FAIL #{name}")
  $fail += 1 unless ok
end

tmp = Dir.mktmpdir('rune_tex')
bmp = File.join(tmp, 'testbild.bmp')

# 8x8-BMP (24 Bit): obere Hälfte rot, untere Hälfte blau.
# BMP-Zeilen stehen von unten nach oben; WIC liefert sie danach oben->unten.
w = 8
h = 8
rows = Array.new(h) { |y| (y < 4 ? [0, 0, 255] : [255, 0, 0]).pack('C3') * w }
pix = rows.reverse.join
dib = [40, w, h, 1, 24, 0, pix.bytesize, 2835, 2835, 0, 0]
      .pack('V l< l< v v V V l< l< V V')
File.binwrite(bmp, 'BM' + [14 + 40 + pix.bytesize, 0, 54].pack('V V V') + dib + pix)

# Projektzustand sichern (textures.json + textures/).
backup_json = File.exist?(JSON_PATH) ? File.read(JSON_PATH, encoding: 'UTF-8') : nil
backup_dir = nil
if Dir.exist?(TEX_DIR)
  backup_dir = File.join(tmp, 'textures_backup')
  FileUtils.cp_r(TEX_DIR, backup_dir)
end

begin
  out, status = Open3.capture2e(EXE, '--import-tex', bmp, 'testbild')
  code = status.exitstatus
  puts "  ... #{out.strip}"
  check('CLI-Import Exit 0') { code.zero? }
  check('textures.json existiert') { File.exist?(JSON_PATH) }

  meta = File.exist?(JSON_PATH) ? JSON.parse(File.read(JSON_PATH, encoding: 'UTF-8')) : {}
  entries = meta.is_a?(Hash) ? (meta['textures'] || []) : meta
  entry = entries.find { |t| t.is_a?(Hash) && t['name'] == 'testbild' }
  check('Eintrag testbild vorhanden') { !entry.nil? }
  check('Grösse 8 x 8') { entry && entry['w'] == 8 && entry['h'] == 8 }
  check('Kachel gesetzt') { entry && entry['tile'].to_f.positive? }

  rgba_path = File.join(TEX_DIR, entry ? entry['file'].to_s : 'testbild.rgba')
  check('RGBA-Datei vorhanden') { File.exist?(rgba_path) }
  if File.exist?(rgba_path)
    bytes = File.binread(rgba_path)
    check('RGBA 256 Bytes') { bytes.bytesize == 256 }
    check('oberste Zeile rot') { bytes.byteslice(0, 4).bytes == [255, 0, 0, 255] }
    check('unterste Zeile blau') { bytes.byteslice(224, 4).bytes == [0, 0, 255, 255] }
  end
ensure
  if backup_json
    File.write(JSON_PATH, backup_json, encoding: 'UTF-8')
  else
    FileUtils.rm_f(JSON_PATH)
  end
  FileUtils.rm_rf(TEX_DIR)
  FileUtils.cp_r(backup_dir, TEX_DIR) if backup_dir
  FileUtils.remove_entry(tmp) if Dir.exist?(tmp)
end

puts($fail.zero? ? 'TEX-IMPORT-TEST OK (0 Fehler)' : "TEX-IMPORT-TEST FEHLGESCHLAGEN (#{$fail} Fehler)")
exit($fail.zero? ? 0 : 1)
