#encoding: utf-8
# ============================================================================
#  Kopfloser Test für Bildschirmton und Blitz (Etappe 20f, XP 223/224).
#  Lässt sich ohne Engine starten:
#     ruby -I game/engine tools/screen_fx_test.rb
#  Exit-Code 0 = alles grün.
#
#  Geprüft wird:
#    - ScreenFx.tone: sofortiger Sprung (frames 0), Übergang über XP-Frames
#      (40/s), Klemmen auf -255..255, Zielerreichung und Stillstand danach
#    - ScreenFx.flash: Farben, Stärke 0..255, Abklingen über die Frames,
#      Ende bei 0, Dauer 0 = ein Frame
#    - draw: Overlay-Rechtecke (Farbe/Alpha) für Ton und Blitz, nichts ohne
#      Effekt; reset räumt alles ab
#    - Interpreter: Befehle screen_tone/flash erreichen das Modul
#    - MapCheck: Kanalbereich, frames >= 0, Blitzfarbe, Stärke
# ============================================================================

# --- Minimal-Attrappe der Engine-Schnittstelle -----------------------------
module Rune
  ROOT = File.expand_path('..', __dir__)

  module Log
    def self.info(m) = nil
    def self.warn(m) = puts("WARN  #{m}")
    def self.error(m) = puts("ERROR #{m}")
  end

  module Window
    def self.width = 1280
    def self.height = 720
  end

  # Graphics.frame_rate = 40 (XP) statt der Engine-40 aus graphics.rb
  module Gfx
    @rects = []
    def self.rect(x, y, w, h, color, alpha) = (@rects << { x: x, y: y, w: w, h: h,
                                                          color: color, alpha: alpha })
    def self.text(*args) = nil
    def self.reset! = (@rects = [])
    def self.rects = @rects
  end
end

$LOAD_PATH.unshift(File.join(Rune::ROOT, 'game', 'engine'))
require 'data'
require 'state'
require 'issues'
require 'party'
require 'screen_fx'
require 'interpreter'
require 'mapcheck'

$fail = 0
def check(name)
  ok = yield
  puts(ok ? "  ok   #{name}" : "  FAIL #{name}")
  $fail += 1 unless ok
rescue StandardError => e
  puts "  FAIL #{name} (Ausnahme: #{e.class}: #{e.message})"
  $fail += 1
end

FRAME = 1.0 / 40.0     # ein XP-Frame

def frames(n)
  n.times { ScreenFx.update(FRAME) }
end

def overlay(farbe)
  Rune::Gfx.rects.find { |r| r[:color] == farbe }
end

puts '== Ton: sofort und im Übergang =='
ScreenFx.reset
check('frames 0 setzt den Ton sofort') do
  ScreenFx.tone(r: 30, b: 60) && ScreenFx.tone_aktuell == { r: 30, g: 0, b: 60, gray: 0 }
end
check('Übergang startet beim alten Wert') do
  ScreenFx.reset
  ScreenFx.tone(r: 100, g: 50)
  ScreenFx.tone(r: 0, g: 0, b: 100, frames: 40)
  ScreenFx.tone_aktuell[:r] == 100 && ScreenFx.tone_aktuell[:b].zero?
end
check('nach der Hälfte der Frames ist der Ton ungefähr halb') do
  frames(20)
  ScreenFx.tone_aktuell[:r].between?(40, 60) && ScreenFx.tone_aktuell[:b].between?(40, 60)
end
check('nach allen Frames steht der Zielwert') do
  frames(25)
  ScreenFx.tone_aktuell == { r: 0, g: 0, b: 100, gray: 0 } && ScreenFx.tone_aktuell[:b] == 100
end
check('Werte außerhalb ±255 werden abgelehnt') do
  vorher = ScreenFx.tone_aktuell.dup
  ScreenFx.tone(r: 300) == false && ScreenFx.tone_aktuell == vorher
end
check('gray wird mitgeführt') do
  ScreenFx.reset
  ScreenFx.tone(gray: 128) && ScreenFx.tone_aktuell[:gray] == 128
end

puts '== Blitz =='
ScreenFx.reset
check('flash merkt Farbe und Stärke') do
  ScreenFx.flash('rot', strength: 200, frames: 20) &&
    ScreenFx.flash_aktuell[:farbe] == 'rot' && ScreenFx.flash_aktuell[:strength] == 200
end
check('Alpha startet bei Stärke/255') do
  (ScreenFx.flash_alpha - 200 / 255.0).abs < 0.01
end
check('Alpha klingt über die Frames ab') do
  frames(10)
  (ScreenFx.flash_alpha - 100 / 255.0).abs < 0.02
end
check('nach allen Frames ist der Blitz weg') do
  frames(10)
  ScreenFx.flash_aktuell.nil?
end
check('unbekannte Farbe wird abgelehnt') { ScreenFx.flash('lila') == false }
check('Stärke wird auf 0..255 geklemmt') do
  ScreenFx.flash('weiss', strength: 900, frames: 4) && ScreenFx.flash_aktuell[:strength] == 255
end
check('frames 0 = genau ein Frame') do
  ScreenFx.reset
  ScreenFx.flash('weiss', strength: 255, frames: 0)
  ScreenFx.flash_aktuell.nil? == false && ScreenFx.flash_alpha > 0.9
  ScreenFx.update(FRAME)
  ScreenFx.flash_aktuell.nil?
end

puts '== Zeichnen =='
ScreenFx.reset
Rune::Gfx.reset!
check('ohne Effekt wird nichts gezeichnet') do
  ScreenFx.draw
  Rune::Gfx.rects.empty?
end
check('positiver Ton zeichnet ein Farb-Overlay über den ganzen Bildschirm') do
  ScreenFx.tone(r: 255, g: 0, b: 0)
  ScreenFx.draw
  r = overlay(0xFF0000)
  r && r[:w] == 1280 && r[:h] == 720 && r[:alpha] > 0.5
end
check('negativer Ton zeichnet einen dunklen Schleier im Gegenanteil') do
  ScreenFx.reset
  Rune::Gfx.reset!
  ScreenFx.tone(b: -255)
  ScreenFx.draw
  # b = -255 -> Gegenanteil (255,255,0) als dunkler Schleier.
  r = overlay(0xFFFF00)
  r && r[:alpha].positive?
end
check('gray legt einen grauen Schleier auf') do
  ScreenFx.reset
  Rune::Gfx.reset!
  ScreenFx.tone(gray: 128)
  ScreenFx.draw
  overlay(0x808080) != nil
end
check('Blitz zeichnet in seiner Farbe') do
  ScreenFx.reset
  Rune::Gfx.reset!
  ScreenFx.flash('blau', strength: 255, frames: 20)
  ScreenFx.draw
  r = overlay(ScreenFx::FARBEN['blau'])
  r && (r[:alpha] - 1.0).abs < 0.01
end
check('reset räumt Ton und Blitz ab') do
  ScreenFx.reset
  ScreenFx.tone_aktuell.nil? && ScreenFx.flash_aktuell.nil? && !ScreenFx.aktiv?
end

puts '== Interpreter =='
ScreenFx.reset
i = Interpreter.new
i.start([{ 'cmd' => 'screen_tone', 'r' => -34, 'b' => 68, 'frames' => 0 },
         { 'cmd' => 'flash', 'color' => 'gelb', 'strength' => 128, 'frames' => 8 }])
3.times { i.update(0.016) }
check('screen_tone erreicht das Modul') do
  ScreenFx.tone_aktuell == { r: -34, g: 0, b: 68, gray: 0 }
end
check('flash erreicht das Modul') do
  ScreenFx.flash_aktuell && ScreenFx.flash_aktuell[:farbe] == 'gelb' &&
    ScreenFx.flash_aktuell[:strength] == 128
end
ScreenFx.reset

puts '== MapCheck =='
OPTS = { prefabs: [], materials: [], scripts: [] }.freeze

def karte_mit(befehle)
  {
    'name' => 'Testkarte',
    'terrain' => { 'width' => 60.0, 'height' => 60.0, 'resolution' => 64,
                   'heights' => [], 'splats' => [] },
    'objects' => [], 'npcs' => [],
    'fog' => { 'on' => false, 'far' => 100.0, 'color' => 0xE8E8E8 },
    'weather' => { 'type' => 'kein', 'intensity' => 1.0 },
    'events' => [{ 'id' => 1, 'name' => 'Test', 'x' => 0.0, 'z' => 0.0, 'radius' => 2.5,
                   'pages' => [{ 'conditions' => {}, 'trigger' => 'action',
                                 'commands' => Array(befehle) }] }]
  }
end

def hat?(funde, text, schwere)
  funde.any? { |f| f.severity == schwere && f.message.include?(text) }
end

check('gültiger Ton: keine Funde') do
  MapCheck.scan(karte_mit([{ 'cmd' => 'screen_tone', 'r' => -34, 'b' => 68,
                             'frames' => 60 }]), **OPTS).empty?
end
check('Kanal außerhalb ±255 -> Fehler') do
  hat?(MapCheck.scan(karte_mit([{ 'cmd' => 'screen_tone', 'r' => 400 }]), **OPTS),
       "'r' muss zwischen -255 und 255 liegen", MapCheck::FEHLER)
end
check('Kanal keine Zahl -> Fehler') do
  hat?(MapCheck.scan(karte_mit([{ 'cmd' => 'screen_tone', 'g' => 'rot' }]), **OPTS),
       "'g' ist keine Zahl", MapCheck::FEHLER)
end
check('negative Frames -> Fehler') do
  hat?(MapCheck.scan(karte_mit([{ 'cmd' => 'screen_tone', 'frames' => -1 }]), **OPTS),
       "'frames' muss eine Zahl >= 0 sein", MapCheck::FEHLER)
end
check('Ton ohne Werte -> Warnung') do
  hat?(MapCheck.scan(karte_mit([{ 'cmd' => 'screen_tone' }]), **OPTS),
       'screen_tone ohne Werte', MapCheck::WARNUNG)
end
check('gültiger Blitz: keine Funde') do
  MapCheck.scan(karte_mit([{ 'cmd' => 'flash', 'color' => 'weiss', 'strength' => 200,
                             'frames' => 20 }]), **OPTS).empty?
end
check('unbekannte Blitzfarbe -> Fehler') do
  hat?(MapCheck.scan(karte_mit([{ 'cmd' => 'flash', 'color' => 'lila' }]), **OPTS),
       "unbekannte Blitzfarbe 'lila'", MapCheck::FEHLER)
end
check('Stärke über 255 -> Fehler') do
  hat?(MapCheck.scan(karte_mit([{ 'cmd' => 'flash', 'strength' => 300 }]), **OPTS),
       "'strength' muss eine Zahl von 0 bis 255 sein", MapCheck::FEHLER)
end
check('alle 54 Befehlstypen im Register der Prüfung') { MapCheck::CMD_TYPES.size == 54 }

puts($fail.zero? ? 'ALLE CHECKS GRUEN' : "#{$fail} CHECK(S) FEHLGESCHLAGEN")
exit($fail.zero? ? 0 : 1)
