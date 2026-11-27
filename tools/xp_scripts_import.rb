#encoding: utf-8
# ============================================================================
#  XP-Defaultscripts in die scripts.json eines bestehenden Projekts eintragen
#  (Etappe 28). Neue Projekte bekommen sie bereits vom Editor.
#
#    ruby tools/xp_scripts_import.rb projekte/dorf          # Vorschau
#    ruby tools/xp_scripts_import.rb projekte/dorf --write  # schreiben
#
#  - Reihenfolge wie RPG Maker XP: 000-088 oben, Projekt-Scripts darunter
#    ("über Main"), 089 Main als letzter Eintrag.
#  - Vorhandene Einträge bleiben unverändert und in ihrer Reihenfolge.
#  - Ist schon ein xp_default-Eintrag vorhanden, ändert das Werkzeug nichts.
#  - Vor dem Schreiben wird scripts.json.bak angelegt.
#  - autoload folgt game/engine/xp_manifest.rb: nur :definition-Dateien aktiv.
# ============================================================================
require 'json'
require 'fileutils'

module Rune
  ROOT = File.expand_path('..', __dir__)
end
$LOAD_PATH.unshift(File.join(Rune::ROOT, 'game', 'engine'))
require 'xp_manifest'

module XPScriptsImport
  module_function

  def entries
    Rune::XPManifest.entries.map do |e|
      code = File.read(e.file, encoding: 'UTF-8').delete_prefix("\uFEFF")
      { 'name' => format('%03d - %s', e.number, e.name), 'xp_default' => format('%03d', e.number),
        'autoload' => e.stage == :definition, 'code' => code }
    end
  end

  # Liefert [neues_dokument, Meldung]; neues_dokument nil = nichts zu tun.
  def merge(doc)
    list = doc.is_a?(Array) ? doc : doc['scripts']
    return [nil, 'scripts.json hat keine Scriptliste'] unless list.is_a?(Array)
    return [nil, 'XP-Defaultscripts sind bereits eingetragen'] if list.any? { |h| h.is_a?(Hash) && h.key?('xp_default') }
    # XP-Konvention: Defaults oben, eigene Scripts darunter, Main ganz unten.
    defaults = entries
    main = defaults.pop
    neu = defaults + list + [main]
    out = doc.is_a?(Array) ? neu : doc.merge('scripts' => neu)
    [out, "#{entries.size} XP-Defaultscripts vor #{list.size} Projekt-Scripts eingetragen"]
  end
end

if $PROGRAM_NAME == __FILE__
  projekt = ARGV.find { |a| !a.start_with?('--') }
  abort 'Aufruf: ruby tools/xp_scripts_import.rb <projektordner> [--write]' unless projekt
  path = File.join(projekt, 'data', 'scripts.json')
  doc = File.exist?(path) ? JSON.parse(File.read(path, encoding: 'UTF-8')) : { 'scripts' => [] }
  neu, meldung = XPScriptsImport.merge(doc)
  puts meldung
  exit 0 unless neu
  if ARGV.include?('--write')
    FileUtils.cp(path, "#{path}.bak") if File.exist?(path)
    File.write(path, JSON.pretty_generate(neu) + "\n", encoding: 'UTF-8')
    puts "geschrieben: #{path}"
  else
    puts 'Vorschau - mit --write schreiben.'
  end
end
