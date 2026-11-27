# Schreib t scripts.json (Etappe 17e): Vorlage in game/data und im Demo-Projekt.
# Aufruf: ruby tools/add_scripts17e.rb
require 'json'
require 'fileutils'

root = File.expand_path('..', __dir__)

scripts = {
  'scripts' => [
    {
      'name' => 'dorf_begruessung',
      'code' => "# Projekt-Script: wird aus einem Event mit\n" \
                "# {\"cmd\":\"script\",\"name\":\"dorf_begruessung\"} aufgerufen.\n" \
                "log 'dorf_begruessung läuft'\n" \
                "add_variable 'script_aufrufe', 1\n" \
                "say ['Das ist ein Projekt-Script.',\n" \
                "     'Bearbeitet wird es im Script-Editor des Editors.']\n"
    },
    {
      'name' => 'kristalle_zaehlen',
      'code' => "# Liest eine Variable und schreibt sie ins Log.\n" \
                "n = variable('kristalle')\n" \
                "log \"kristalle=\#{n}\"\n"
    }
  ]
}

json = JSON.pretty_generate(scripts) + "\n"
[File.join(root, 'game', 'data', 'scripts.json'),
 File.join(root, 'projekte', 'dorf', 'data', 'scripts.json')].each do |path|
  FileUtils.mkdir_p(File.dirname(path))
  File.write(path, json, encoding: 'UTF-8')
  puts "geschrieben: #{path}"
end
