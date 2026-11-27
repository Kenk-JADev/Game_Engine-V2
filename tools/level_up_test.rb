#encoding: utf-8
# ============================================================================
#  Kopfloser Test für den XP-Stufenaufstieg (D4, Runde 22).
#     ruby -I game/engine tools/level_up_test.rb
#  Prüft Kurve, Aufstieg, Überschuss, RGSS-Route (actor.exp = n), Deckel und
#  dass ein Spielstand beim Laden keine Stufen nachholt.
# ============================================================================

module Rune
  ROOT = File.expand_path('..', __dir__)

  module Log
    @lines = []
    class << self
      attr_reader :lines
    end
    def self.info(m) = @lines << m.to_s
    def self.warn(m) = @lines << m.to_s
    def self.error(m) = @lines << m.to_s
  end
end

$LOAD_PATH.unshift(File.join(Rune::ROOT, 'game', 'engine'))
require 'party'

$fail = 0
def check(name)
  ok = yield
  puts(ok ? "  ok   #{name}" : "  FAIL #{name}")
  $fail += 1 unless ok
rescue StandardError => e
  puts "  FAIL #{name} (#{e.class}: #{e.message})"
  $fail += 1
end

DB = {
  'gold' => 10,
  'actors' => [
    { 'id' => 'arik', 'name' => 'Arik', 'class' => 'Krieger', 'level' => 1,
      'hp' => 120, 'mp' => 15, 'atk' => 14, 'def' => 10, 'spi' => 4, 'agi' => 8,
      'exp' => 0 },
  ],
}

Party.setup(DB)
arik = Party.find('arik')

puts '== XP-Kurve =='
check('Stufe 1 verlangt 0 Exp') { Party.exp_for_level(1).zero? }
check('Stufe 2 = 1 Exp, Stufe 3 = 7 Exp (XP-Formel 2.7)') do
  Party.exp_for_level(2) == 1 && Party.exp_for_level(3) == 7
end
check('Kurve wächst monoton bis zur Höchststufe') do
  (2..Party::MAX_LEVEL).all? { |i| Party.exp_for_level(i) > Party.exp_for_level(i - 1) }
end
check('unter Stufe 1 bleibt die Schwelle 0') { Party.exp_for_level(0).zero? }
check('oberhalb der Höchststufe wird nicht weiter gerechnet') do
  Party.exp_for_level(500) == Party.exp_for_level(Party::MAX_LEVEL)
end

puts '== Aufstieg =='
check('keine Erfahrung, keine Stufe') do
  Party.gain_exp(arik, 0).zero? && arik.level == 1
end
check('genug Erfahrung hebt eine Stufe und erhöht das HP-Maximum') do
  hp_vorher = arik.max_hp
  stufen = Party.gain_exp(arik, 1)
  stufen == 1 && arik.level == 2 && arik.max_hp > hp_vorher
end
check('Aufstieg heilt nicht (HP/MP bleiben stehen, wie XP)') do
  vorher = [arik.hp, arik.mp]
  Party.gain_exp(arik, Party.exp_for_level(arik.level + 1) - arik.exp)
  [arik.hp, arik.mp] == vorher
end
check('Angriffswerte wachsen beim Aufstieg mit') do
  vorher = [arik.atk, arik.def, arik.spi, arik.agi]
  Party.gain_exp(arik, Party.exp_for_level(arik.level + 1) - arik.exp)
  nachher = [arik.atk, arik.def, arik.spi, arik.agi]
  nachher.zip(vorher).all? { |n, v| n > v }
end
check('Überschuss hebt mehrere Stufen auf einmal') do
  vorher = arik.level
  stufen = Party.gain_exp(arik, 500)
  arik.level > vorher + 1 && stufen == arik.level - vorher
end
check('Gesamt-Erfahrung bleibt stehen (Rune3D-Modell)') { arik.exp > 500 }

puts '== RGSS-Route (actor.exp = n) =='
check('exp= hebt Stufen wie gain_exp') do
  vorher = arik.level
  arik.exp = arik.exp + 500
  arik.level > vorher
end
check('negative Erfahrung wird auf 0 begrenzt, Stufen bleiben') do
  stufe = arik.level
  arik.exp = -50
  arik.exp.zero? && arik.level == stufe
end
check('next_exp liefert die nächste Schwelle') do
  arik.next_exp == Party.exp_for_level(arik.level + 1)
end
check('Höchststufe ist bei 99 gedeckelt') do
  arik.exp = Party.exp_for_level(Party::MAX_LEVEL)
  arik.level == Party::MAX_LEVEL && Party.gain_exp(arik, 99_999).zero?
end

puts '== Spielstand =='
Party.setup(DB)
Party.restore!({ 'actors' => [{ 'id' => 'arik', 'level' => 5, 'hp' => 50,
                                'mp' => 5, 'exp' => 1000 }] })
check('Laden holt keine Stufen nach (Werte sind schon eingerechnet)') do
  a = Party.find('arik')
  a.level == 5 && a.exp == 1000 && a.hp == 50
end
check('Snapshot enthält Stufe und Gesamt-Erfahrung') do
  eintrag = Party.snapshot['actors'].first
  eintrag['level'] == 5 && eintrag['exp'] == 1000
end

Party.setup(DB)
hoch = Party.find('arik')
Party.gain_exp(hoch, Party.exp_for_level(4))
check('Snapshot enthält die gewachsenen Werte (max_hp/Angriff)') do
  eintrag = Party.snapshot['actors'].first
  hoch.level >= 4 && eintrag['max_hp'] == hoch.max_hp && eintrag['atk'] == hoch.atk &&
    eintrag['max_hp'] > 120 && eintrag['atk'] > 14
end
check('Rundlauf: aufgestiegener Held behält seine Werte') do
  vorher = Party.snapshot
  Party.setup(DB)                # wie Game.load_game: erst Datenbank
  Party.restore!(vorher)         # dann Spielstand
  Party.snapshot == vorher
end
check('alter Spielstand ohne gewachsene Werte bleibt ladbar') do
  Party.setup(DB)
  Party.restore!({ 'actors' => [{ 'id' => 'arik', 'level' => 3, 'hp' => 40,
                                  'mp' => 2, 'exp' => 7 }] })
  a = Party.find('arik')
  a.level == 3 && a.max_hp == 120 && a.atk == 14 && a.hp == 40
end

puts($fail.zero? ? '== LEVEL-UP-TEST: ALLES GRÜN ==' : "== #{$fail} FEHLER ==")
exit($fail.zero? ? 0 : 1)
