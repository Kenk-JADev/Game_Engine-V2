# Einmaliger Einblick: Event-Struktur des Dorfs (Argumente: keine).
require 'json'
d = JSON.parse(File.read('projekte/dorf/data/maps/dorf.json', encoding: 'UTF-8'))
ev = d['events'] || []
puts "Events: #{ev.size}"
ev.each do |e|
  puts format('%2d %-20s trigger=%-8s cmds=%d', e['id'], e['name'], e['trigger'], (e['commands'] || []).size)
end
puts '--- Beispiel Waechter Mara (id 1) ---'
puts JSON.pretty_generate(ev.find { |e| e['id'] == 1 } || ev[0])
