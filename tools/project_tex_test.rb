#encoding: utf-8
# ============================================================================
#  Test fuer Textures.register_project (Etappe 17f, Spielseite):
#     ruby tools/project_tex_test.rb
#
#  Prueft ohne Engine: textures.json wird gelesen, rohe RGBA-Dateien werden
#  als Rune::Gfx.texture angemeldet, falsche Groessen werden uebersprungen und
#  die Kachelgroesse (tile_of) kommt aus den Metadaten.
# ============================================================================

require 'json'
require 'fileutils'
require 'tmpdir'

ROOT = File.expand_path('..', __dir__)

$fail = 0
def check(name)
  ok = yield
  puts(ok ? "  ok   #{name}" : "  FAIL #{name}")
  $fail += 1 unless ok
end

# --- Minimal-Attrappen fuer die Engine-Bindings -----------------------------
TEX_CALLS = {}

module Rune
  ROOT = File.expand_path('..', __dir__)
  module Gfx
    class << self
      def texture(name, w, h, pixels)
        TEX_CALLS[name] = { w: w, h: h, bytes: pixels.bytesize, pixels: pixels.b }
      end
    end
  end
  module Log
    def self.info(msg)
      puts "  ... #{msg}" if ENV['TEX_TEST_VERBOSE']
    end

    def self.warn(msg)
      puts "  !   #{msg}" if ENV['TEX_TEST_VERBOSE']
    end

    def self.error(msg)
      puts "  !   #{msg}" if ENV['TEX_TEST_VERBOSE']
    end
  end
end

module Database
  def self.root = $tex_root
end

require File.join(ROOT, 'game', 'engine', 'textures.rb')
require File.join(ROOT, 'game', 'engine', 'issues.rb')   # Etappe 18h: Backlog

tmp = Dir.mktmpdir('rune_projtex')
begin
  $tex_root = tmp
  FileUtils.mkdir_p(File.join(tmp, 'textures'))

  # Gutes Material: 4x2 Pixel, oben rot, unten blau.
  w = 4
  h = 2
  pixels = ([255, 0, 0, 255] * w) + ([0, 0, 255, 255] * w)
  File.binwrite(File.join(tmp, 'textures', 'wand_bunt.rgba'), pixels.pack('C*'))

  # Kaputtes Material: Datei zu kurz.
  File.binwrite(File.join(tmp, 'textures', 'kaputt.rgba'), "\0" * 5)

  meta = {
    'textures' => [
      { 'name' => 'wand_bunt', 'file' => 'wand_bunt.rgba', 'w' => w, 'h' => h,
        'tile' => 1.5, 'source' => 'wand.png' },
      { 'name' => 'kaputt', 'file' => 'kaputt.rgba', 'w' => 8, 'h' => 8 },
      { 'name' => 'fehlt', 'file' => 'gibtsnicht.rgba', 'w' => 8, 'h' => 8 },
      { 'name' => '', 'file' => 'leer.rgba', 'w' => 8, 'h' => 8 }
    ]
  }
  File.write(File.join(tmp, 'textures.json'), JSON.generate(meta), encoding: 'UTF-8')

  check('project_meta filtert Eintraege ohne Namen') do
    Textures.project_meta.size == 3   # wand_bunt + kaputt + fehlt (leer raus)
  end

  loaded = Textures.register_project
  check('nur gueltige Datei geladen') { loaded == 1 }
  check('wand_bunt registriert') { TEX_CALLS.key?('wand_bunt') }
  check('Groesse 4 x 2') do
    t = TEX_CALLS['wand_bunt']
    t && t[:w] == 4 && t[:h] == 2
  end
  check('Pixel unveraendert (rot oben, blau unten)') do
    t = TEX_CALLS['wand_bunt']
    t && t[:pixels].byteslice(0, 4).bytes == [255, 0, 0, 255] &&
      t[:pixels].byteslice(16, 4).bytes == [0, 0, 255, 255]
  end
  check('kaputte Datei nicht registriert') { !TEX_CALLS.key?('kaputt') }
  check('Kachel aus Metadaten') { Textures.tile_of('wand_bunt') == 1.5 }
  check('Standard-Kachel 2.0') { Textures.tile_of('gibtsnicht') == 2.0 }
  check('ohne textures.json: 0 Materialien') do
    FileUtils.rm_f(File.join(tmp, 'textures.json'))
    Textures.instance_variable_set(:@project_meta, nil)
    Textures.register_project.zero?
  end
ensure
  FileUtils.remove_entry(tmp) if Dir.exist?(tmp)
end

puts($fail.zero? ? 'PROJEKT-TEXTUR-TEST OK (0 Fehler)' : "PROJEKT-TEXTUR-TEST FEHLGESCHLAGEN (#{$fail} Fehler)")
exit($fail.zero? ? 0 : 1)
