# Vorbereitung des Spiel-Smoke-Tests: dorf.json sichern, Test-Splats
# einsetzen, Spiel 8 s laufen lassen (engine.log pruefen), zuruecksetzen.
# Aufruf: ruby tools/splat_smoke.rb
require 'json'

map = 'projekte/dorf/data/maps/dorf.json'
bak = File.binread(map)
begin
  data = JSON.parse(bak)
  (data['terrain'] ||= {})['splats'] = [
    { 'tex' => 'weg', 'x' => -22.0, 'z' => -18.0, 'w' => 44.0, 'h' => 10.0 },
    { 'tex' => 'fels', 'x' => 30.0, 'z' => 8.0, 'w' => 26.0, 'h' => 20.0 }
  ]
  File.write(map, JSON.pretty_generate(data))
  puts 'dorf.json mit 2 Test-Splats versehen'
ensure
  File.binwrite(map + '.bak', bak)
end
