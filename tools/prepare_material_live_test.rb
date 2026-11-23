#encoding: utf-8
# Erzeugt eine NEUE, isolierte Installation für den manuellen Windows-Test.
# Keine vorhandenen Projekt-/Build-Ordner werden gelöscht oder verändert.
# Aufruf nach .\build.ps1 -Clean:
#   ruby tools/prepare_material_live_test.rb C:/Temp/rune-material-live
require 'fileutils'
require 'json'

SOURCE = File.expand_path('..', __dir__)
abort 'Aufruf: ruby tools/prepare_material_live_test.rb <neuer-ordner-ausserhalb-des-repos>' unless ARGV.size == 1
requested = File.expand_path(ARGV.first)
parent = File.dirname(requested)
abort "Elternordner existiert nicht: #{parent}" unless Dir.exist?(parent)
target = File.join(File.realpath(parent), File.basename(requested))
source_real = File.realpath(SOURCE)
norm = ->(s) { s.tr('\\', '/').downcase.sub(%r{/+\z}, '') }
abort 'Ziel muss ausserhalb des Repositorys liegen' if norm.call(target) == norm.call(source_real) ||
                                                     norm.call(target).start_with?(norm.call(source_real) + '/')
abort "Ziel existiert bereits; nichts überschrieben: #{target}" if File.exist?(target) || File.symlink?(target)
%w[RuneEditor.exe Rune3D.exe].each do |name|
  abort "build/#{name} fehlt; erst .\\build.ps1 -Clean ausführen" unless File.file?(File.join(SOURCE, 'build', name))
end

# Dir.mkdir ist exklusiv: auch ein konkurrierend angelegtes Ziel wird nie benutzt.
Dir.mkdir(target)
puts "Neue Testkopie: #{target}"
begin
  FileUtils.cp_r(File.join(SOURCE, 'game'), target)
  build = File.join(target, 'build')
  Dir.mkdir(build)
  Dir.glob(File.join(SOURCE, 'build', '*')).each do |path|
    name = File.basename(path)
    if File.file?(path) && (name.end_with?('.exe', '.dll', '.manifest')) &&
       (name == 'RuneEditor.exe' || name == 'Rune3D.exe' || name.end_with?('.dll', '.manifest'))
      FileUtils.cp(path, build)
    elsif File.directory?(path) && name == 'ruby_builtin_dlls'
      FileUtils.cp_r(path, build)
    end
  end
  projects = File.join(target, 'projekte')
  Dir.mkdir(projects)

  configs = {
    'A_material' => {
      'gras' => [255, 24, 24, 255], 'wand' => [255, 45, 25, 255],
      'gemeinsam' => [255, 210, 0, 255], 'nur_a' => [240, 0, 220, 255]
    },
    'B_material' => {
      'gemeinsam' => [0, 80, 255, 255], 'nur_b' => [0, 230, 240, 255]
    }
  }
  configs.each do |project, colors|
    base = File.join(projects, project)
    Dir.mkdir(base)
    FileUtils.cp_r(File.join(SOURCE, 'game', 'data'), base)
    data = File.join(base, 'data')
    textures = File.join(data, 'textures')
    Dir.mkdir(textures)
    entries = colors.map do |name, color|
      file = "#{name}.rgba"
      File.binwrite(File.join(textures, file), color.pack('C4') * (64 * 64))
      { 'name' => name, 'file' => file, 'w' => 64, 'h' => 64,
        'tile' => 1.2345678901234567, 'source' => 'isolierter Live-Test',
        'unbekannt' => { 'behalten' => true } }
    end
    # Der fehlerhafte Eintrag muss beim Bearbeiten bestehen bleiben, wird aber
    # weder als gültiges Material angezeigt noch als Pfad geöffnet.
    entries.insert(1, { 'name' => 'ungueltig', 'file' => '../nicht_lesen.rgba', 'w' => 1 })
    manifest = { 'zukunft' => { 'zahl' => 9_007_199_254_740_993 }, 'textures' => entries }
    File.write(File.join(data, 'textures.json'), JSON.pretty_generate(manifest) + "\n", encoding: 'UTF-8')

    system_path = File.join(data, 'system.json')
    system = JSON.parse(File.read(system_path, encoding: 'UTF-8'))
    system['name'] = project
    system['start'] = { 'map' => 'dorf', 'x' => 0.0, 'z' => 8.0, 'yaw' => 3.14159 }
    File.write(system_path, JSON.pretty_generate(system) + "\n", encoding: 'UTF-8')
    unique = project.start_with?('A_') ? 'nur_a' : 'nur_b'
    map = {
      'name' => project, 'terrain' => {
        'width' => 40.0, 'height' => 40.0, 'resolution' => 32, 'seed' => 7,
        'amplitude' => 0.0, 'texture' => 'gras', 'tile' => 2.0,
        'splats' => [
          { 'x' => -8.0, 'z' => -8.0, 'w' => 16.0, 'h' => 16.0, 'tex' => 'gemeinsam' },
          { 'x' => 8.0, 'z' => -8.0, 'w' => 8.0, 'h' => 16.0, 'tex' => unique }
        ]
      },
      'objects' => [
        { 'type' => 'haus', 'x' => -6.0, 'z' => -2.0, 'rot' => 0.0 },
        { 'type' => 'haus', 'x' => 6.0, 'z' => -2.0, 'rot' => 0.0, 'tex' => 'gemeinsam' }
      ],
      'events' => [], 'npcs' => []
    }
    File.write(File.join(data, 'maps', 'dorf.json'), JSON.pretty_generate(map) + "\n", encoding: 'UTF-8')
  end

  # Kontrastbild für den manuellen F3-Import: oben rot, unten blau.
  width = height = 8
  rows = Array.new(height) { |y| (y < 4 ? [0, 0, 255] : [255, 0, 0]).pack('C3') * width }
  pixels = rows.reverse.join
  dib = [40, width, height, 1, 24, 0, pixels.bytesize, 2835, 2835, 0, 0]
        .pack('V l< l< v v V V l< l< V V')
  File.binwrite(File.join(target, 'rot_blau.bmp'), 'BM' + [54 + pixels.bytesize, 0, 54].pack('V V V') + dib + pixels)
  puts 'Projekte A_material und B_material sowie rot_blau.bmp sind vorbereitet.'
  puts 'Nur diese Kopie verwenden; Anleitung: docs/WINDOWS_MATERIAL_LIVE_TEST.md'
rescue StandardError => e
  warn "Vorbereitung abgebrochen: #{e.class}: #{e.message}"
  warn "Teilkopie bleibt zur Prüfung erhalten: #{target}"
  exit 1
end
