# Setzt den Test-Nebel in der Projekt-Karte dorf.json zurück (aus für sauberen Zustand).
require 'json'
pfad = 'projekte/dorf/data/maps/dorf.json'
j = JSON.parse(File.read(pfad, encoding: 'UTF-8'))
raise 'keine fog-Sektion' unless j['fog']
j['fog']['on'] = false
File.write(pfad, JSON.pretty_generate(j) + "\n", encoding: 'UTF-8')
puts 'Nebel in dorf.json ausgeschaltet: ' + j['fog'].inspect
