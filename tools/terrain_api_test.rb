#encoding: utf-8
# Kopfloser Brückentest der echten Ruby-Argumentweitergabe an das C++-Binding.
ROOT = File.expand_path('..', __dir__)
$LOAD_PATH.unshift(File.join(ROOT, 'game', 'engine'))

module Rune
  module World
    class << self
      attr_accessor :brushes, :args, :raw_result
    end
    def self.height_brushes=(value)
      self.brushes = value
    end
    def self.generate_raw(*values)
      self.args = values
      raw_result
    end
  end
end
require 'terrain_api'

$fail = 0
def check(name)
  ok = yield
  puts(ok ? "  ok   #{name}" : "  FAIL #{name}")
  $fail += 1 unless ok
rescue StandardError => e
  puts "  FAIL #{name} (#{e.class}: #{e.message})"
  $fail += 1
end

puts '== Ruby-Gelände-API =='
Rune::World.raw_result = true
check('Standardwerte und leere Pinsel') do
  Rune::World.generate && Rune::World.args == [220.0, 220.0, 176, 7, 5.0] && Rune::World.brushes == []
end
check('Legacy-size und explizite Höhe') do
  Rune::World.generate(size: 150, height: 80, resolution: 64) && Rune::World.args == [150, 80, 64, 7, 5.0]
end
pinsel = [{ 'x' => 3, 'z' => 2, 'r' => 1, 'h' => 4 }]
check('JSON-Pinsel-Hashes unverändert an Binding übergeben') do
  Rune::World.generate(width: 240, height: 120, height_brushes: pinsel) && Rune::World.brushes.equal?(pinsel)
end
check('direkte Array-Pinsel ebenso möglich') do
  liste = [[0, 0, 4, 2]]
  Rune::World.generate(height_brushes: liste) && Rune::World.brushes.equal?(liste)
end
check('nächster Aufruf ohne Pinsel leert den alten Satz') do
  Rune::World.generate && Rune::World.brushes.empty?
end
check('ungültige Werte nicht still per to_f oder Truthiness normalisiert') do
  riesig = 10**400
  Rune::World.generate(width: false, height: riesig, resolution: '192', seed: riesig,
                       amplitude: false, height_brushes: false)
  Rune::World.args == [false, riesig, '192', riesig, false] && Rune::World.brushes == false
end
check('Fehlschlag des nativen Generators wird zurückgegeben') do
  Rune::World.raw_result = false
  Rune::World.generate(width: 0) == false
end
puts($fail.zero? ? 'TERRAIN-API-TEST OK (0 Fehler)' : "TERRAIN-API-TEST FEHLGESCHLAGEN (#{$fail} Fehler)")
exit($fail.zero? ? 0 : 1)
