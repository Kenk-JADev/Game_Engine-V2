#encoding: utf-8
# Einmalige Konvertierung: das fest verdrahtete Dorf (main.rb) wird zur
# Objektliste in game/data/maps/dorf.json. Danach kann es im Editor geändert
# oder gelöscht werden.  Aufruf:  ruby tools\village_to_json.rb
require 'json'

path = File.join(__dir__, '..', 'game', 'data', 'maps', 'dorf.json')
d = JSON.parse(File.read(path, encoding: 'UTF-8'))

objs = []
# Weg nach Norden: 18 Stücke (früher 3.0 x 0.06 x 2.4, Abstand 3.0)
18.times { |i| objs << { 'type' => 'weg', 'x' => 0.0, 'z' => -4.0 - i * 3.0, 'rot' => 0.0 } }
# Zaun südlich der Dorfmitte: 7 Segmente über x = -7 .. 7 bei z = -3
[-6.0, -4.0, -2.0, 0.0, 2.0, 4.0, 6.0].each do |x|
  objs << { 'type' => 'zaun', 'x' => x, 'z' => -3.0, 'rot' => 0.0 }
end
# Alter Brunnen
objs << { 'type' => 'brunnen', 'x' => -5.0, 'z' => 3.0, 'rot' => 0.0 }
# Sechs Häuser
[[-10.0, -8.0], [10.0, -8.0], [-11.0, -18.0], [11.0, -18.0], [-9.0, -28.0], [9.0, -28.0]].each do |x, z|
  objs << { 'type' => 'haus', 'x' => x, 'z' => z, 'rot' => 0.0 }
end
# Kristalle (Startdrehung wie früher i * 0.6)
[[-4.0, 0.0], [4.0, 0.6]].each { |x, r| objs << { 'type' => 'kristall', 'x' => x, 'z' => -12.0, 'rot' => r } }
objs << { 'type' => 'kristall', 'x' => 0.0, 'z' => -22.0, 'rot' => 1.8 }
# Vier Laternen
[[-3.0, -6.0], [3.0, -6.0], [-3.0, -26.0], [3.0, -26.0]].each do |x, z|
  objs << { 'type' => 'laterne', 'x' => x, 'z' => z, 'rot' => 0.0 }
end

d['objects'] = objs
File.write(path, JSON.pretty_generate(d) + "\n", encoding: 'UTF-8')
puts "Objekte: #{objs.size}  (#{objs.group_by { |o| o['type'] }.map { |k, v| "#{k}:#{v.size}" }.join(', ')})"
