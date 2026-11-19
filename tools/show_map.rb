# Zeigt die Kernfelder einer Karten-JSON. Aufruf: ruby tools\show_map.rb <pfad>
require 'json'
j = JSON.parse(File.read(ARGV[0], encoding: 'UTF-8'))
puts "name: #{j['name'].inspect}"
puts "terrain: #{j['terrain'].inspect}"
puts "fog: #{j['fog'].inspect}"
puts "panorama: #{j['panorama'].inspect}"
puts "note: #{j['note'].inspect}"
puts "objects: #{j['objects'].size}  events: #{(j['events'] || []).size}  npcs: #{(j['npcs'] || []).size}"
