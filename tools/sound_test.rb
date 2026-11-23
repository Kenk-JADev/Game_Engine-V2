#encoding: utf-8
# ============================================================================
#  Kopfloser Test für die prozeduralen Sounds: Längen, Determinismus,
#  Pegel. Benötigt keine Engine.
#     ruby -I game/engine tools/sound_test.rb
# ============================================================================

$LOAD_PATH.unshift(File.join(File.expand_path('..', __dir__), 'game', 'engine'))

module Rune
  ROOT = File.expand_path('..', __dir__)
  module Log
    def self.info(m) = nil
    def self.warn(m) = puts("WARN  #{m}")
    def self.error(m) = puts("ERROR #{m}")
  end
  module Sound
    REG = {}
    # Wie die Engine: register resampelt linear auf 44100 Hz.
    def self.register(name, rate, bytes)
      werte = bytes.unpack('s<*').map { |v| v / 32_768.0 }
      out = if rate == 44_100
              werte
            else
              step = rate / 44_100.0
              n = (werte.size / step).to_i
              Array.new(n) do |i|
                src = i * step
                i0 = src.to_i
                f = src - i0
                i1 = [i0 + 1, werte.size - 1].min
                werte[i0] * (1 - f) + werte[i1] * f
              end
            end
      REG[name] = [44_100, out.map { |v| (v * 32_767).round }.pack('v*')]
    end
    def self.play(name, _v = 1.0)
      REG[name]
    end
  end
end

require 'sounds'

Sounds.register_all

$fail = 0
def check(name)
  ok = yield
  puts(ok ? "  ok   #{name}" : "  FAIL #{name}")
  $fail += 1 unless ok
end

check('7 Sounds registriert') { Rune::Sound::REG.size == 7 }
%w[schritt klick jingle ablehnen sprung landung musik].each do |n|
  check("Sound '#{n}' vorhanden") { Rune::Sound::REG.key?(n) }
end

check('Abtastrate 44100') { Rune::Sound::REG.values.all? { |r, _| r == 44_100 } }
check('Bytezahl gerade') { Rune::Sound::REG.values.all? { |_, b| b.bytesize.even? } }

# Dauer: Samples / 44100
dauer = ->(n) { Rune::Sound::REG[n][1].bytesize / 2 / 44_100.0 }
check('schritt ~0.09 s')  { (dauer.call('schritt') - 0.09).abs < 0.01 }
check('klick  ~0.05 s')   { (dauer.call('klick') - 0.05).abs < 0.01 }
check('jingle ~0.63 s')   { (dauer.call('jingle') - 0.63).abs < 0.02 }
check('ablehnen ~0.36 s') { (dauer.call('ablehnen') - 0.36).abs < 0.02 }
check('sprung ~0.16 s')   { (dauer.call('sprung') - 0.16).abs < 0.01 }
check('landung ~0.13 s')  { (dauer.call('landung') - 0.13).abs < 0.01 }
check('musik ~8.0 s')     { (dauer.call('musik') - 8.0).abs < 0.05 }

# Pegel: nirgends über 16 Bit, aber auch nicht stumm.
pegel_ok = true
Rune::Sound::REG.each do |name, (_, bytes)|
  werte = bytes.unpack('s<*')
  pegel_ok = false if werte.any? { |v| v.abs > 32_767 }
  pegel_ok = false if werte.map { |v| v.abs }.max < 1000
end
check('Pegel ok (nicht clippt, nicht stumm)') { pegel_ok }

# Determinismus: zweimal registrieren liefert identische Bytes.
erste = Rune::Sound::REG.dup
Sounds.register_all
check('Deterministisch') { Rune::Sound::REG.all? { |n, v| erste[n] == v } }

puts($fail.zero? ? 'SOUND-TEST OK (0 Fehler)' : "SOUND-TEST FEHLGESCHLAGEN (#{$fail} Fehler)")
exit($fail.zero? ? 0 : 1)
