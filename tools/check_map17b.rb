# Prüft die gespeicherte Projekt-Karte auf die neuen 17b-Felder.
# Aufruf: ruby tools\check_map17b.rb [pfad]
require 'json'
pfad = ARGV[0] || 'projekte/dorf/data/maps/dorf.json'
j = JSON.parse(File.read(pfad, encoding: 'UTF-8'))
puts "Keys: #{j.keys.inspect}"
raise 'fog fehlt' unless j['fog']
raise 'fog.on != true' unless j['fog']['on'] == true
raise "fog.far != 120 (#{j['fog']['far'].inspect})" unless j['fog']['far'] == 120
raise "fog.color != 0xE8E8E8 (#{j['fog']['color'].inspect})" unless j['fog']['color'] == 15_263_976
raise 'panorama fehlt/falsch' unless j['panorama'] && j['panorama']['color'] == 4_026_293
raise 'note fehlt' unless j.key?('note')
raise 'events weg' unless j['events'].is_a?(Array) && j['events'].size == 6
raise 'npcs weg' unless j['npcs'].is_a?(Array) && j['npcs'].size == 1
raise "objects != 39 (#{j['objects'].size})" unless j['objects'].size == 39
puts "JSON ok: fog=#{j['fog'].inspect} pan=#{j['panorama'].inspect} " \
     "events=#{j['events'].size} npcs=#{j['npcs'].size} obj=#{j['objects'].size}"
