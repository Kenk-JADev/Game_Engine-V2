#encoding: utf-8
# ============================================================================
#  Kopfloser Test für das Karten-BGM/BGS (Etappe 21, T3).
#     ruby -I game/engine tools/map_audio_test.rb
#
#  Engine-Attrappen: `Audio` merkt sich die Aufrufe, `Rune::Sound` den
#  Rückfall ohne RGSS-Schicht. Geprüft wird: Start beim Kartenwechsel,
#  Weiterlaufen ohne Eintrag, kein Neustart derselben Datei, Klemmung von
#  Lautstärke/Pitch, leerer Name und der Rückfall ohne `Audio`.
# ============================================================================

module Rune
  module Log
    def self.info(_m) = nil
    def self.warn(_m) = nil
    def self.error(_m) = nil
  end
end

$LOAD_PATH.unshift(File.join(__dir__, '..', 'game', 'engine'))
require 'map_audio'

$fail = 0
def check(name)
  ok = yield
  puts(ok ? "  ok   #{name}" : "  FAIL #{name}")
  $fail += 1 unless ok
  ok
rescue StandardError => e
  puts "  FAIL #{name} (Ausnahme: #{e.class}: #{e.message})"
  $fail += 1
  false
end

# Rückfall ohne RGSS-Schicht: reine Rune::Sound-Welt.
module Rune
  module Sound
    LOOPED = []
    def self.loop(name, volume = 1.0) = (LOOPED << [name, volume])
  end
end

puts '== Ohne RGSS-Schicht (Rückfall auf Rune::Sound) =='
r = MapAudio.enter({ 'bgm' => { 'name' => 'bgm/dorf', 'volume' => 50 } })
check('ohne Audio wird Rune::Sound.loop mit 0..1 aufgerufen') do
  r == [true, false] && Rune::Sound::LOOPED.last == ['bgm/dorf', 0.5]
end
check('MapAudio merkt sich die laufende Musik') { MapAudio.bgm == ['bgm/dorf', 50.0, 100] }

# Ab hier mit RGSS-Attrappe (der Interpreter/Audio-Weg).
Object.send(:remove_const, :Rune) if Rune.const_defined?(:Sound)
module Rune
  module Log
    def self.info(_m) = nil
    def self.warn(_m) = nil
  end
end
module Audio
  CALLS = []
  def self.bgm_play(name, volume = 100, pitch = 100) = (CALLS << [:bgm_play, name, volume, pitch])
  def self.bgs_play(name, volume = 100, pitch = 100) = (CALLS << [:bgs_play, name, volume, pitch])
  def self.bgm_stop = (CALLS << [:bgm_stop])
end
MapAudio.reset

puts '== Kartenwechsel =='
check('Karte ohne bgm/bgs startet nichts') do
  Audio::CALLS.clear
  MapAudio.enter({ 'name' => 'leer' }) == [false, false] && Audio::CALLS.empty?
end
check('bgm der Karte startet mit Lautstärke und Pitch') do
  Audio::CALLS.clear
  MapAudio.enter({ 'bgm' => { 'name' => 'bgm/dorf.wav', 'volume' => 80, 'pitch' => 120 } })
  Audio::CALLS == [[:bgm_play, 'bgm/dorf.wav', 80.0, 120]]
end
check('derselbe Eintrag startet nicht neu') do
  Audio::CALLS.clear
  MapAudio.enter({ 'bgm' => { 'name' => 'bgm/dorf.wav', 'volume' => 80, 'pitch' => 120 } })
  Audio::CALLS.empty?
end
check('neue Datei startet neu') do
  Audio::CALLS.clear
  MapAudio.enter({ 'bgm' => { 'name' => 'bgm/wald.wav' } })
  Audio::CALLS == [[:bgm_play, 'bgm/wald.wav', 100.0, 100]]
end
check('Karte ohne bgm lässt die Musik weiterlaufen') do
  Audio::CALLS.clear
  ergebnis = MapAudio.enter({ 'name' => 'ohne-musik' })
  ergebnis == [false, false] && Audio::CALLS.empty? && MapAudio.bgm == ['bgm/wald.wav', 100.0, 100]
end
check('leerer Name zählt als "keine Musik"') do
  Audio::CALLS.clear
  MapAudio.enter({ 'bgm' => { 'name' => '   ' } }) == [false, false] && Audio::CALLS.empty?
end
check('bgs läuft unabhängig vom bgm') do
  Audio::CALLS.clear
  MapAudio.enter({ 'bgs' => { 'name' => 'bgs/regen.wav', 'volume' => 60 } })
  Audio::CALLS == [[:bgs_play, 'bgs/regen.wav', 60.0, 100]] && MapAudio.bgs == ['bgs/regen.wav', 60.0, 100]
end
check('Lautstärke und Pitch werden geklemmt') do
  Audio::CALLS.clear
  MapAudio.enter({ 'bgm' => { 'name' => 'bgm/laut.wav', 'volume' => 250, 'pitch' => 20 } })
  Audio::CALLS == [[:bgm_play, 'bgm/laut.wav', 100.0, 50]]
end
check('kaputte Einträge stürzen nicht ab') do
  Audio::CALLS.clear
  [nil, 5, 'text', { 'bgm' => 'kein Objekt' }, { 'bgm' => { 'name' => nil } }].all? do |karte|
    MapAudio.enter(karte) == [false, false]
  end && Audio::CALLS.empty?
end
check('reset vergisst die laufende Musik ohne zu stoppen') do
  Audio::CALLS.clear
  MapAudio.reset
  MapAudio.bgm.nil? && MapAudio.bgs.nil? && Audio::CALLS.empty?
end

puts($fail.zero? ? 'ALLE CHECKS GRUEN' : "#{$fail} CHECK(S) FEHLGESCHLAGEN")
exit($fail.zero? ? 0 : 1)
