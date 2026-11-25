#encoding: utf-8
# ============================================================================
#  Kopfloser Test für den Ton-Dateilader (Etappe 21, T1/T5).
#     ruby -I game/engine tools/audio_files_test.rb
#
#  Engine-Attrappen: Rune::Sound merkt sich geladene Klänge und Stimmen,
#  Rune::Assets liefert eine eingebettete WAV-Datei für genau einen Pfad.
#  Geprüft wird: Kandidatenreihenfolge, Laden/Anmelden, play/loop/stop,
#  Pitch-Klemmung, OGG-Warnung, fehlende Datei, Cache, reset und der
#  Fade über Zeit (XP 242/246, nicht blockierend).
# ============================================================================

require 'base64'

module Rune
  ROOT = File.expand_path('..', __dir__)
  module Log
    def self.info(m) = nil
    def self.warn(m) = puts("WARN  #{m}")
    def self.error(m) = puts("ERROR #{m}")
  end
  module Sound
    @geladen = {}
    @stimmen = []
    class << self
      attr_reader :geladen, :stimmen
    end
    def self.load(name, bytes, pitch = 100)
      @geladen[name] = { bytes: bytes.bytesize, pitch: pitch }
      true
    end
    def self.play(name, vol) = (@stimmen << [:play, name, vol])
    def self.loop(name, vol) = (@stimmen << [:loop, name, vol])
    def self.stop(name) = (@stimmen << [:stop, name])
    # Etappe 21: Fade setzt die Lautstärke laufender Stimmen.
    def self.volume(name, vol = nil)
      @lautstaerke ||= {}
      return @lautstaerke[name] || 0.0 if vol.nil?

      @lautstaerke[name] = vol
      @stimmen << [:volume, name, vol]
      vol
    end
    def self.reset! = (@geladen = {}; @stimmen = []; @lautstaerke = {})
  end
  # Assets-Attrappe: nur diese Pfade existieren.
  module Assets
    @dateien = {}
    class << self
      attr_accessor :dateien
    end
    def self.read_texture(_root, _project, pfad, _expected)
      wert = @dateien[pfad]
      wert ? [:ok, wert] : [:missing, nil]
    end
  end
end

$LOAD_PATH.unshift(File.join(__dir__, '..', 'game', 'engine'))
require 'data'
require 'state'
require 'issues'
require 'audio_files'

WAV = Base64.decode64(
  'UklGRkQAAABXQVZFZm10IBAAAAABAAIAIlYAAIhYAQAEABAAZGF0YSAAAABI9Kz0uAscDEj0rPS4CxwM' \
  'SPSs9LgLHAxI9Kz0uAscDA=='
)
Rune::Assets.dateien = {
  'audio/bgm/dorf.wav' => WAV,
  'audio/se/tuer.wav' => WAV,
  'audio/bgm/wald.ogg' => 'OggS...'
}

$fail = 0
def check(name)
  ok = yield
  puts(ok ? "  ok   #{name}" : "  FAIL #{name}")
  $fail += 1 unless ok
rescue StandardError => e
  puts "  FAIL #{name} (Ausnahme: #{e.class}: #{e.message})"
  $fail += 1
end

puts '== Kandidaten =='
check('Name ohne Pfad -> audio/bgm und audio/se werden gesucht') do
  AudioFiles.kandidaten('dorf').include?('audio/bgm/dorf.wav') &&
    AudioFiles.kandidaten('dorf').include?('audio/se/dorf.wav')
end
check('Endung wird ersetzt, nicht verdoppelt') do
  AudioFiles.kandidaten('dorf.wav').none? { |p| p.end_with?('.wav.wav') }
end
check('leerer Name -> keine Kandidaten') { AudioFiles.kandidaten('').empty? }

puts '== Laden und Abspielen =='
Rune::Sound.reset!
AudioFiles.reset
check('BGM lädt über den XP-Ordner und spielt als Loop') do
  AudioFiles.loop('dorf', volume: 0.6) &&
    Rune::Sound.geladen.key?('datei:audio/bgm/dorf.wav') &&
    Rune::Sound.stimmen.last == [:loop, 'datei:audio/bgm/dorf.wav', 0.6]
end
check('Lautstärke wird auf 0..1 geklemmt') do
  AudioFiles.play('tuer', volume: 5) &&
    (Rune::Sound.stimmen.last[2] - 1.0).abs < 1e-9
end
check('Pitch wird auf 50..150 geklemmt') do
  AudioFiles.reset
  Rune::Sound.reset!
  AudioFiles.play('tuer', volume: 0.5, pitch: 300) &&
    Rune::Sound.geladen['datei:audio/se/tuer.wav'][:pitch] == 150
end
check('zweites Laden nutzt den Cache (kein neues load)') do
  vorher = Rune::Sound.geladen.size
  AudioFiles.play('tuer')
  Rune::Sound.geladen.size == vorher
end
check('stop findet den Klang über den Aufrufnamen') do
  AudioFiles.stop('tuer') && Rune::Sound.stimmen.last == [:stop, 'datei:audio/se/tuer.wav']
end
check('geladen? kennt Name und Pfad') do
  AudioFiles.geladen?('tuer') && AudioFiles.geladen?('audio/se/tuer.wav')
end

puts '== Fehlerfälle =='
AudioFiles.reset
Rune::Sound.reset!
check('unbekannte Datei -> false') { AudioFiles.play('gibtesnicht') == false }
check('OGG wird abgelehnt und gewarnt') do
  AudioFiles.play('wald') == false && !Rune::Sound.geladen.key?('datei:audio/bgm/wald.ogg')
end
check('stop ohne geladenen Klang -> false') { AudioFiles.stop('nichts') == false }
check('reset leert den Cache') do
  AudioFiles.reset
  !AudioFiles.geladen?('tuer')
end

puts '== Fade über Zeit (XP 242/246) =='
AudioFiles.reset
Rune::Sound.reset!
check('fade ohne laufenden Klang -> false') { AudioFiles.fade('tuer', seconds: 1) == false }
check('fade mit seconds: 0 stoppt sofort') do
  AudioFiles.loop('tuer', volume: 0.8) &&
    AudioFiles.fade('tuer', seconds: 0) &&
    Rune::Sound.stimmen.last == [:stop, 'datei:audio/se/tuer.wav']
end
check('fade_ms rechnet Millisekunden in Sekunden') do
  AudioFiles.loop('dorf', volume: 0.8) &&
    AudioFiles.fade_ms('dorf', 1000) &&
    AudioFiles.fade_aktiv?('dorf')
end
check('nach der Hälfte ist die Lautstärke etwa halbiert') do
  AudioFiles.update(0.5)
  laut = Rune::Sound.volume('datei:audio/bgm/dorf.wav')
  (laut - 0.4).abs < 0.05
end
check('Fade läuft weiter, solange Zeit übrig ist') do
  # letzter Eintrag ist das Lautstärke-Update, kein Stopp
  AudioFiles.fade_aktiv?('dorf') && Rune::Sound.stimmen.last.first == :volume
end
check('am Ende wird gestoppt und der Fade geräumt') do
  AudioFiles.update(0.6)
  Rune::Sound.stimmen.last == [:stop, 'datei:audio/bgm/dorf.wav'] &&
    !AudioFiles.fade_aktiv?('dorf')
end
check('neues loop bricht einen laufenden Fade ab') do
  AudioFiles.loop('dorf', volume: 0.5) && AudioFiles.fade('dorf', seconds: 2) &&
    AudioFiles.loop('dorf', volume: 0.5) && !AudioFiles.fade_aktiv?('dorf')
end
check('stop räumt einen laufenden Fade') do
  AudioFiles.fade('dorf', seconds: 2) && AudioFiles.stop('dorf') &&
    !AudioFiles.fade_aktiv?('dorf')
end
check('reset räumt laufende Fades') do
  AudioFiles.loop('dorf', volume: 0.7)
  AudioFiles.fade('dorf', seconds: 2)
  AudioFiles.reset
  !AudioFiles.fade_aktiv?('dorf')
end

puts($fail.zero? ? 'ALLE CHECKS GRUEN' : "#{$fail} CHECK(S) FEHLGESCHLAGEN")
exit($fail.zero? ? 0 : 1)
