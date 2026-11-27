#encoding: utf-8
# ============================================================================
#  Legt die Demo-Datenbank (Helden/Items) für Etappe 17g an:
#    game/data/database.json            (Vorlage, wird zu neuen Projekten
#                                        kopiert - siehe Project.ensure_demo)
#    projekte/dorf/data/database.json   (bestehendes Demo-Projekt)
# aufruf: ruby tools/add_database17g.rb
# ============================================================================

require 'json'
require 'fileutils'

ROOT = File.expand_path('..', __dir__)

DB = {
  'gold' => 120,
  'actors' => [
    { 'id' => 'arik', 'name' => 'Arik', 'class' => 'Krieger',
      'level' => 1, 'hp' => 120, 'mp' => 15,
      'atk' => 14, 'def' => 10, 'spi' => 4, 'agi' => 8, 'exp' => 0 },
    { 'id' => 'mira', 'name' => 'Mira', 'class' => 'Magierin',
      'level' => 1, 'hp' => 80, 'mp' => 40,
      'atk' => 6, 'def' => 6, 'spi' => 15, 'agi' => 10, 'exp' => 0 }
  ],
  'items' => [
    { 'id' => 'heiltrank', 'name' => 'Heiltrank', 'kind' => 'heal',
      'power' => 40, 'price' => 25, 'start' => 2,
      'desc' => 'Heilt 40 HP eines Helden.' },
    { 'id' => 'manatrank', 'name' => 'Manatrank', 'kind' => 'mana',
      'power' => 20, 'price' => 30, 'start' => 1,
      'desc' => 'Stellt 20 MP wieder her.' },
    { 'id' => 'kristall', 'name' => 'Wasserkristall', 'kind' => 'quest',
      'power' => 0, 'price' => 0, 'start' => 0,
      'desc' => 'Seltsamer blauer Kristall aus dem Brunnen.' },
    { 'id' => 'goldbeutel', 'name' => 'Goldbeutel', 'kind' => 'money',
      'power' => 50, 'price' => 0, 'start' => 0,
      'desc' => 'Bringt 50 Gold beim Öffnen.' }
  ]
}.freeze

targets = [
  File.join(ROOT, 'game', 'data', 'database.json'),
  File.join(ROOT, 'projekte', 'dorf', 'data', 'database.json')
]

targets.each do |path|
  next if File.exist?(path)   # bereits vorhanden: nicht überschreiben
  FileUtils.mkdir_p(File.dirname(path))
  File.write(path, JSON.pretty_generate(DB) + "\n", encoding: 'UTF-8')
  puts "angelegt: #{path}"
end

puts 'fertig'
