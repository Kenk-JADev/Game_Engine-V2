#encoding: utf-8
# Kopfloser Ruby-Integrationstest: Projektbindung, Validierung und Übergabe an
# native Brücken. Das Dateisystem-/GPU-Verhalten selbst prüfen Windows-C++-Tests.
require 'json'
require 'fileutils'
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

module Rune
  ROOT = File.expand_path('..', __dir__)
  module Gfx
    @base = {}
    @project = {}
    @active = false
    class << self
      attr_reader :base, :project
      def texture(name, w, h, pixels)
        (@active ? @project : @base)[name] = { w: w, h: h, pixels: pixels.b.dup }
        0
      end
      def begin_project_textures
        @project.clear
        @active = true
      end
      def end_project_textures
        @project.clear
        @active = false
      end
      def get(name) = @project[name] || @base[name]
    end
  end
  module Log
    def self.info(_msg); end
    def self.warn(_msg); end
    def self.error(_msg); end
  end
  # Bewusst nur eine Attrappe: kein Beweis für Windows-Handle-Sicherheit.
  module Assets
    @calls = []
    class << self
      attr_reader :calls
      def read_texture(root, _project, filename, expected)
        @calls << [root, filename, expected]
        path = filename ? File.join(root, 'textures', filename) : File.join(root, 'textures.json')
        return [:reparse, nil] if filename && File.symlink?(File.join(root, 'textures'))
        return [:reparse, nil] if File.symlink?(path)
        return [:missing, nil] unless File.file?(path)

        max = filename ? expected : 2 * 1024 * 1024
        return [:too_large, nil] if File.size(path) > max
        data = File.binread(path)
        return [:invalid, nil] if filename && data.bytesize != expected

        [:ok, data]
      rescue StandardError
        [:io, nil]
      end
    end
  end
end

module Issues
  @reports = []
  class << self
    attr_reader :reports
    def report(*args, **_opts) = @reports << args
  end
end
module Database
  def self.root = $tex_root
end
require File.join(ROOT, 'game', 'engine', 'textures.rb')

def put_json(dir, value)
  File.write(File.join(dir, 'textures.json'), JSON.generate(value), encoding: 'UTF-8')
end

def entry(name, file, w = 1, h = 1, tile = 2.0)
  { 'name' => name, 'file' => file, 'w' => w, 'h' => h, 'tile' => tile }
end

def setup_project
  Textures.end_project
  Textures.register_all
  Textures.begin_project
  Textures.register_project
end

tmp = Dir.mktmpdir('rune_project_tex')
begin
  a = File.join(tmp, 'A')
  b = File.join(tmp, 'B')
  [a, b].each { |dir| FileUtils.mkdir_p(File.join(dir, 'textures')) }
  $tex_root = a
  $project_root = a
  $project_name = 'A'

  pixels = ([255, 0, 0, 255] * 4) + ([0, 0, 255, 255] * 4)
  File.binwrite(File.join(a, 'textures', 'wand_bunt.rgba'), pixels.pack('C*'))
  File.binwrite(File.join(a, 'textures', 'kaputt.rgba'), "\0" * 5)
  put_json(a, 'textures' => [entry('wand_bunt', 'wand_bunt.rgba', 4, 2, 1.5),
                              entry('kaputt', 'kaputt.rgba', 8, 8),
                              entry('fehlt', 'gibtsnicht.rgba', 8, 8),
                              entry('', 'leer.rgba', 8, 8)])
  check('Metadaten filtern ungültige Namen') { Textures.project_meta.size == 3 }
  Textures.begin_project
  check('nur Datei mit korrekter Bytezahl registriert') { Textures.register_project == 1 }
  check('Dimension und obere/untere Zeile bleiben erhalten') do
    t = Rune::Gfx.get('wand_bunt')
    t && t[:w] == 4 && t[:h] == 2 && t[:pixels].byteslice(0, 4).bytes == [255, 0, 0, 255] &&
      t[:pixels].byteslice(16, 4).bytes == [0, 0, 255, 255]
  end
  check('kaputte Datei nicht registriert') { Rune::Gfx.get('kaputt').nil? }
  check('Kachel aus Metadaten / Standard') { Textures.tile_of('wand_bunt') == 1.5 && Textures.tile_of('fehltx') == 2.0 }

  File.binwrite(File.join(a, 'textures', 'rot.rgba'), [255, 0, 0, 255].pack('C*'))
  File.binwrite(File.join(b, 'textures', 'blau.rgba'), [0, 0, 255, 255].pack('C*'))
  put_json(a, 'textures' => [entry('gras', 'rot.rgba'), entry('nur_a', 'rot.rgba')])
  put_json(b, 'textures' => [entry('nur_b', 'blau.rgba', 1, 1, 3.5)])
  check('F5 lädt geänderte Metadaten und Projekt-Override A') do
    setup_project == 2 && Rune::Gfx.get('gras')[:pixels].bytes == [255, 0, 0, 255] &&
      Rune::Gfx.get('nur_a') && !Rune::Gfx.get('wand_bunt')
  end
  $tex_root = $project_root = b
  $project_name = 'B'
  check('Cache ist an die Projektwurzel gebunden, auch vor Reset') do
    Textures.project_meta.map { |x| x['name'] } == ['nur_b'] && Textures.tile_of('nur_b') == 3.5
  end
  check('A → B räumt A auf und stellt eingebautes Gras wieder her') do
    setup_project == 1 && !Rune::Gfx.get('nur_a') && Rune::Gfx.get('nur_b') &&
      Rune::Gfx.get('gras')[:pixels] == Textures.gras
  end
  $tex_root = $project_root = a
  $project_name = 'A'
  check('B → A lädt A erneut und entfernt B') do
    setup_project == 2 && !Rune::Gfx.get('nur_b') && Rune::Gfx.get('nur_a') &&
      Rune::Gfx.get('gras')[:pixels].bytes == [255, 0, 0, 255]
  end
  Textures.end_project
  check('Rückkehr zum Titel (Textures.end_project) stellt die Basis wieder her') do
    !Rune::Gfx.get('nur_a') && Rune::Gfx.get('gras')[:pixels] == Textures.gras
  end

  invalid = [
    entry('pfad', '../rot.rgba'), entry('geraet', 'CON.rgba'), entry('nul', "a\u0000b.rgba"),
    entry('gross', 'rot.rgba', 10**100, 1), entry('negativ', 'rot.rgba', -1, 1),
    entry('bruch', 'rot.rgba', 1.5, 1), entry('string', 'rot.rgba', '1', 1),
    entry("zweizeilig\nname", 'rot.rgba'), entry('ok', 'rot.rgba')
  ]
  put_json(a, 'extra' => { 'erhalten' => true }, 'textures' => invalid)
  Textures.begin_project
  before = Rune::Assets.calls.size
  check('extreme Dimensionen, Pfade und ungültige Namen werden übersprungen') do
    Textures.project_meta.map { |x| x['name'] } == ['ok'] && Textures.register_project == 1 &&
      Rune::Assets.calls[before..].none? { |x| x[1] && x[1] != 'rot.rgba' }
  end
  put_json(a, 'textures' => [entry('ok', 'rot.rgba', 1, 1, 'defekt')])
  Textures.begin_project
  check('ungültiges Tile bleibt Standard statt to_f-Konvertierung') { Textures.tile_of('ok') == 2.0 }
  File.binwrite(File.join(a, 'textures.json'), '{"textures":[],"textures":[]}')
  Textures.begin_project
  check('doppelte JSON-Schlüssel sperren das Dokument') { Textures.project_meta.empty? }
  put_json(a, 'textures' => [entry('gleich', 'rot.rgba'), entry('gleich', 'rot.rgba')])
  Textures.begin_project
  check('doppelte Materialnamen sperren das Dokument') { Textures.project_meta.empty? }
  File.binwrite(File.join(a, 'textures.json'), '{"textures":[],"x":1e400}')
  Textures.begin_project
  check('nicht endliche unbekannte Zahlen sperren das Dokument') { Textures.project_meta.empty? }
  File.binwrite(File.join(a, 'textures.json'), '[' * 101 + '0' + ']' * 101)
  Textures.begin_project
  check('zu tiefe JSON-Verschachtelung wird abgewiesen') { Textures.project_meta.empty? }
  File.open(File.join(a, 'textures.json'), 'wb') { |f| f.truncate(Textures::MAX_MANIFEST_BYTES + 1) }
  Textures.begin_project
  check('übergrosses Manifest wird vor dem Lesen abgewiesen') { Textures.project_meta.empty? }
  FileUtils.rm_f(File.join(a, 'textures.json'))
  Textures.begin_project
  check('fehlendes Manifest ist ein leerer Katalog') { Textures.register_project.zero? }
ensure
  FileUtils.remove_entry(tmp) if Dir.exist?(tmp)
end
puts($fail.zero? ? 'PROJEKT-TEXTUR-TEST OK (0 Fehler)' : "PROJEKT-TEXTUR-TEST FEHLGESCHLAGEN (#{$fail} Fehler)")
exit($fail.zero? ? 0 : 1)
