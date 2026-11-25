#encoding: utf-8
# ============================================================================
#  Kopfloser Test für die Bildschirm-Bilder (Etappe 20f, XP 231–235).
#     ruby -I game/engine tools/pictures_test.rb
#
#  Geprüft wird:
#    - Pictures.show: Zustand, Grenzen der Nummer, Anmeldung beim Bildlader
#    - move: linearer Übergang über Frames (x/y/Zoom/Deckkraft)
#    - rotate: Drehung pro Frame, angle wächst, 0 stoppt
#    - tint: Kanäle -255..255 mit Übergang, Dämpfung der Gegenkanäle
#    - erase/reset, Zeichenreihenfolge nach Nummer, origin 0/1
#    - Interpreter-Befehle 231–235 erreichen das Modul
#    - MapCheck: Nummer, Deckkraft, Blend, Tint-Kanäle, fehlende Datei
# ============================================================================

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

  module Gfx
    @pictures = []
    class << self
      attr_accessor :pictures
    end
    def self.image(_name, _w, _h, _pixels) = true
    def self.picture(name, x, y, w, h, alpha, tint, angle)
      @pictures << { name: name, x: x, y: y, w: w, h: h,
                     alpha: alpha, tint: tint, angle: angle }
    end
    def self.reset! = (@pictures = [])
  end
end

$LOAD_PATH.unshift(File.join(__dir__, '..', 'game', 'engine'))
require 'data'
require 'state'
require 'issues'

# Bildlader-Attrappe: jedes Bild ist 4x2 Pixel groß.
module ImageLoader
  def self.load(_pfad) = [4, 2, "\x00" * 32]
  def self.register(_name, _pfad) = true
  def self.cache_leeren = nil
end

require 'pictures'
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

FRAME = 1.0 / 40.0

def frames(n)
  n.times { Pictures.update(FRAME) }
end

puts '== show / erase =='
Pictures.reset
Rune::Gfx.reset!
check('show legt das Bild an') do
  Pictures.show(1, 'bilder/logo.png', x: 100, y: 50) &&
    Pictures.get(1)[:datei] == 'bilder/logo.png' && Pictures.anzahl == 1
end
check('XP-Defaults: Zoom 100, Deckkraft 255, origin 0') do
  stand = Pictures.get(1)
  stand[:zoom_x] == 100.0 && stand[:opacity] == 255 && stand[:origin].zero?
end
check('zeichnen nutzt Position und Originalgröße') do
  Pictures.draw
  p = Rune::Gfx.pictures.last
  p && p[:x] == 100.0 && p[:y] == 50.0 && p[:w] == 4.0 && p[:h] == 2.0
end
check('Zoom 200 verdoppelt die Zeichenfläche') do
  Pictures.show(2, 'bilder/logo.png', x: 0, y: 0, zoom_x: 200, zoom_y: 200)
  Rune::Gfx.reset!
  Pictures.draw
  p = Rune::Gfx.pictures.find { |b| b[:name] == 'picture:2' }
  p && p[:w] == 8.0 && p[:h] == 4.0
end
check('origin 1 setzt den Anker in die Bildmitte') do
  Pictures.show(3, 'bilder/logo.png', x: 100, y: 100, origin: 1)
  Rune::Gfx.reset!
  Pictures.draw
  p = Rune::Gfx.pictures.find { |b| b[:name] == 'picture:3' }
  p && p[:x] == 98.0 && p[:y] == 99.0
end
check('Nummer 0 und 51 werden abgelehnt') do
  Pictures.show(0, 'x.png') == false && Pictures.show(51, 'x.png') == false
end
check('leere Datei wird abgelehnt') { Pictures.show(4, '') == false }
check('Bilder werden nach Nummer gezeichnet (1 unten)') do
  Pictures.reset
  Pictures.show(3, 'a.png')
  Pictures.show(1, 'b.png')
  Rune::Gfx.reset!
  Pictures.draw
  Rune::Gfx.pictures.map { |p| p[:name] } == %w[picture:1 picture:3]
end
check('erase entfernt das Bild') do
  Pictures.erase(3) && Pictures.get(3).nil? && Pictures.anzahl == 1
end
check('erase ohne Bild gibt false') { Pictures.erase(9) == false }

puts '== move (XP 232) =='
Pictures.reset
Pictures.show(1, 'a.png', x: 0, y: 0, opacity: 255)
check('move über 40 Frames setzt das Ziel') do
  Pictures.move(1, x: 400, y: 200, frames: 40) && Pictures.get(1)[:ziel][:x] == 400.0
end
check('nach der Hälfte ist die Position ungefähr halb') do
  frames(20)
  stand = Pictures.get(1)
  stand[:x].between?(180.0, 220.0) && stand[:y].between?(90.0, 110.0)
end
check('nach allen Frames steht das Ziel exakt') do
  frames(25)
  stand = Pictures.get(1)
  stand[:x] == 400.0 && stand[:y] == 200.0 && stand[:ziel].nil?
end
check('move ohne Frames springt sofort') do
  Pictures.move(1, x: 10, frames: 0) && Pictures.get(1)[:x] == 10.0
end
check('Deckkraft lässt sich überblenden') do
  Pictures.move(1, opacity: 0, frames: 10)
  frames(12)
  Pictures.get(1)[:opacity] == 0
end
check('move ohne Bild gibt false') { Pictures.move(7, x: 1) == false }

puts '== rotate (XP 233) =='
Pictures.reset
Pictures.show(1, 'a.png')
check('speed merkt die Drehung (Grad pro Frame)') do
  Pictures.rotate(1, speed: 1) && Pictures.get(1)[:drehung] == 1.0
end
check('nach einem Frame ist der Winkel 1 Grad') do
  Pictures.update(FRAME)
  (Pictures.get(1)[:angle] - 1.0).abs < 0.01
end
check('nach zehn Frames sind es 10 Grad') do
  frames(9)
  (Pictures.get(1)[:angle] - 10.0).abs < 0.05
end
check('speed 0 hält die Drehung an') do
  Pictures.rotate(1, speed: 0)
  vorher = Pictures.get(1)[:angle]
  frames(5)
  Pictures.get(1)[:angle] == vorher
end
check('rotate ohne Bild gibt false') { Pictures.rotate(5, speed: 3) == false }

puts '== tint (XP 234) =='
Pictures.reset
Pictures.show(1, 'a.png')
check('tint übernimmt die Kanäle') do
  Pictures.tint(1, tone: { r: -60, g: 0, b: 40, gray: 0 }, frames: 0) &&
    Pictures.get(1)[:tone][:r] == -60 && Pictures.get(1)[:tone][:b] == 40
end
check('Kanäle werden auf ±255 geklemmt') do
  Pictures.tint(1, tone: { r: 900 }, frames: 0) && Pictures.get(1)[:tone][:r] == 255
end
check('Tint blendet über die Frames') do
  Pictures.reset
  Pictures.show(1, 'a.png')
  Pictures.tint(1, tone: { r: -200 }, frames: 40)
  frames(20)
  Pictures.get(1)[:tone][:r].between?(-120, -80)
end
check('rotes Tint dämpft Grün und Blau im Zeichenaufruf') do
  Pictures.reset
  Pictures.show(1, 'a.png')
  Pictures.tint(1, tone: { r: 255 }, frames: 0)
  Rune::Gfx.reset!
  Pictures.draw
  p = Rune::Gfx.pictures.last
  rot = (p[:tint] >> 16) & 0xFF
  gruen = (p[:tint] >> 8) & 0xFF
  rot == 255 && gruen.zero?
end
check('gray dämpft alle Kanäle') do
  Pictures.reset
  Pictures.show(1, 'a.png')
  Pictures.tint(1, tone: { gray: 255 }, frames: 0)
  Rune::Gfx.reset!
  Pictures.draw
  Rune::Gfx.pictures.last[:tint].zero?
end

puts '== Interpreter =='
Pictures.reset
i = Interpreter.new
i.start([{ 'cmd' => 'show_picture', 'number' => 2, 'name' => 'bilder/x.png',
           'x' => 10, 'y' => 20 },
         { 'cmd' => 'rotate_picture', 'number' => 2, 'speed' => 5 },
         { 'cmd' => 'tint_picture', 'number' => 2, 'r' => -40, 'frames' => 0 },
         { 'cmd' => 'move_picture', 'number' => 2, 'x' => 50, 'frames' => 0 }])
i.update(0.016)
check('show/rotate/tint/move erreichen das Modul') do
  stand = Pictures.get(2)
  stand && stand[:x] == 50.0 && stand[:drehung] == 5.0 && stand[:tone][:r] == -40
end
check('erase_picture löscht das Bild') do
  i2 = Interpreter.new
  i2.start([{ 'cmd' => 'erase_picture', 'number' => 2 }])
  i2.update(0.016)
  Pictures.get(2).nil?
end

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

check('gültiges show_picture: keine Funde') do
  MapCheck.scan(karte_mit([{ 'cmd' => 'show_picture', 'number' => 1,
                             'name' => 'bilder/logo.png', 'x' => 10, 'y' => 20 }]),
                **OPTS).empty?
end
check('show_picture ohne Datei -> Fehler') do
  hat?(MapCheck.scan(karte_mit([{ 'cmd' => 'show_picture', 'number' => 1 }]), **OPTS),
       "'name' fehlt oder ist leer", MapCheck::FEHLER)
end
check('Bildnummer 0 -> Fehler') do
  hat?(MapCheck.scan(karte_mit([{ 'cmd' => 'show_picture', 'number' => 0,
                                  'name' => 'a.png' }]), **OPTS),
       "'number' muss eine ganze Zahl von 1 bis 50 sein", MapCheck::FEHLER)
end
check('Deckkraft 300 -> Fehler') do
  hat?(MapCheck.scan(karte_mit([{ 'cmd' => 'show_picture', 'name' => 'a.png',
                                  'opacity' => 300 }]), **OPTS),
       "'opacity' muss eine Zahl von 0 bis 255 sein", MapCheck::FEHLER)
end
check('Blend 2 -> Warnung (noch kein Additions-Blend)') do
  hat?(MapCheck.scan(karte_mit([{ 'cmd' => 'show_picture', 'name' => 'a.png',
                                  'blend' => 2 }]), **OPTS),
       'wird wie 0 gezeichnet', MapCheck::WARNUNG)
end
check('Zoom über 2000 -> Fehler') do
  hat?(MapCheck.scan(karte_mit([{ 'cmd' => 'show_picture', 'name' => 'a.png',
                                  'zoom_x' => 5000 }]), **OPTS),
       "'zoom_x' muss zwischen 0 und 2000 liegen", MapCheck::FEHLER)
end
check('move_picture ohne Zielwerte -> Warnung') do
  hat?(MapCheck.scan(karte_mit([{ 'cmd' => 'move_picture', 'number' => 1 }]), **OPTS),
       'move_picture ohne Zielwerte', MapCheck::WARNUNG)
end
check('rotate_picture mit speed 0 -> Warnung') do
  hat?(MapCheck.scan(karte_mit([{ 'cmd' => 'rotate_picture', 'number' => 1, 'speed' => 0 }]),
                     **OPTS), "'speed' ist 0", MapCheck::WARNUNG)
end
check('tint_picture mit Kanal 300 -> Fehler') do
  hat?(MapCheck.scan(karte_mit([{ 'cmd' => 'tint_picture', 'number' => 1, 'r' => 300 }]),
                     **OPTS), "'r' muss zwischen -255 und 255 liegen", MapCheck::FEHLER)
end
check('erase_picture ohne Nummer -> Fehler') do
  hat?(MapCheck.scan(karte_mit([{ 'cmd' => 'erase_picture' }]), **OPTS),
       "'number' fehlt oder ist keine Zahl", MapCheck::FEHLER)
end
check('alle 65 Befehlstypen im Register der Prüfung') { MapCheck::CMD_TYPES.size == 65 }

puts($fail.zero? ? 'ALLE CHECKS GRUEN' : "#{$fail} CHECK(S) FEHLGESCHLAGEN")
exit($fail.zero? ? 0 : 1)
