#encoding: utf-8
# ============================================================================
#  Kopfloser Test für die RGSS-Vertiefung (Etappe 19h).
#  Lässt sich ohne Engine starten:
#     ruby -I game/engine tools/graphics_test.rb
#  Exit-Code 0 = alles grün.
#
#  Geprüft werden game/engine/graphics.rb und die 19h-Ergänzungen in rgss.rb:
#    - $game_temp und die $data_*-Globalen aus database.json/system.json
#    - $game_player.passable? (Kartengrenzen, Geländestufe, Richtungen)
#    - Bitmap (fill_rect/draw_text/clear/dispose)
#    - Sprite (z-Sortierung, visible/opacity, Viewport-Vererbung, dispose)
#    - Window (Cursor mit dir4/C/B, Zeichnen über die UI-Schicht)
#    - $scene + Graphics.scene_update (inkl. Fehler -> Szene fällt weg)
#    - RGSS.reset! / Graphics.scene_reset! (Kartenwechsel)
# ============================================================================

# --- Minimal-Attrappe der Engine-Schnittstelle -----------------------------
module Rune
  ROOT = File.expand_path('..', __dir__)

  module Log
    def self.info(m) = nil
    def self.warn(m) = puts("WARN  #{m}")
    def self.error(m) = puts("ERROR #{m}")
  end

  module Gfx
    CALLS = []
    def self.rect(x, y, w, h, color, alpha = 1.0) = (CALLS << [:rect, x, y, w, h, color, alpha])
    def self.text(x, y, s, color = 0, size = 16) = (CALLS << [:text, x, y, s, color, size])
  end

  module Sound
    CALLS = []
    def self.play(name, volume = 1.0) = (CALLS << [:play, name, volume])
    def self.loop(name, volume = 1.0) = (CALLS << [:loop, name, volume])
    def self.stop(name) = (CALLS << [:stop, name])
  end

  module Input
    @pressed = []
    @down = []
    class << self
      attr_accessor :pressed, :down
    end
    def self.pressed?(key) = @pressed.include?(key)
    def self.down?(key) = @down.include?(key)
  end

  module Window
    def self.width = 1280
    def self.height = 720
  end

  module Player
    @pos = [0.0, 0.0, 0.0]
    class << self
      attr_accessor :pos
    end
    def self.teleport(x, y, z) = (self.pos = [x.to_f, y.to_f, z.to_f])
    def self.position = pos
  end

  module Camera
    @yaw = 0.0
    class << self
      attr_accessor :yaw
    end
  end
end

# UI-Attrappe: Window/Sprite zeichnen über UI.panel/UI.text (ui.rb selbst
# würde Rune::Gfx direkt nutzen - hier zählen wir die Aufrufe).
module UI
  TEXT = 0xE8E8E8
  TEXT_DIM = 0x9AA4AE
  ACCENT = 0x59D8FF
  PANEL = 0x0B1016
  CALLS = []
  def self.panel(x, y, w, h, color = 0, alpha = 1.0) = (CALLS << [:panel, x, y, w, h, color, alpha])
  def self.text(x, y, s, color = 0, size = 16) = (CALLS << [:text, x, y, s, color, size])
end

# Game-Attrappe: Karte 120x90, „Wand" bei x > 10 (Geländestufe 5 m).
module Game
  @map = { 'name' => 'Testkarte', 'terrain' => { 'width' => 120.0, 'height' => 90.0 } }
  @map_id = 'welt01'
  @events = []
  @npcs = []
  class << self
    attr_accessor :map, :map_id, :events, :npcs
  end

  def self.gy(x, _z) = x > 10 ? 5.0 : 0.0
end

$LOAD_PATH.unshift(File.join(Rune::ROOT, 'game', 'engine'))
require 'state'
require 'issues'
require 'party'
require 'weather'
require 'fade'
require 'data'      # Database: graphics.rb/rgss.rb lesen daraus (echte JSONs)
require 'rgss'
require 'graphics'

$fail = 0
def check(name)
  ok = yield
  puts(ok ? "  ok   #{name}" : "  FAIL #{name}")
  $fail += 1 unless ok
rescue StandardError => e
  puts "  FAIL #{name} (Ausnahme: #{e.class}: #{e.message})"
  $fail += 1
end

puts '== RGSS.setup: $game_temp und $data_* =='
RGSS.setup
check('$game_temp besetzt') { $game_temp.is_a?(RGSS::GameTemp) }
check('$game_temp: Standardwerte') do
  $game_temp.message_text.nil? && $game_temp.common_event_id.zero? &&
    $game_temp.player_passable?
end
check('$data_actors aus database.json (Hash mit String-IDs)') do
  $data_actors.key?('arik') && $data_actors['arik']['name'] == 'Arik' &&
    $data_actors.size == 2
end
check('$data_items aus database.json') { $data_items.key?('heiltrank') }
check('$data_weapons/$data_armors leer (Datenbank kennt sie noch nicht)') do
  $data_weapons == {} && $data_armors == {}
end
check('$data_system = system.json') { $data_system.is_a?(Hash) && !$data_system.empty? }
check('$scene startet leer') { $scene.nil? }

puts '== $game_player.passable? (Etappe 19h: echt) =='
# Die Stufe misst von der SPIELERPOSITION zum Ziel - der Spieler wird für
# die Checks explizit gestellt (moveto setzt ihn auf Game.gy + 0.2).
# Gelände-Mock: flach (0 m) bis x = 10, dahinter „Wand" (5 m).
$game_player.moveto(5.0, 5.0)
check('in der Karte, flach -> passierbar') { $game_player.passable?(6.0, 5.0) }
check('Richtung 6 (rechts) in die Wand -> false') do
  !$game_player.passable?(10.0, 5.0, 6)
end
check('Stufe hoch vom flachen Boden (delta 5 m > 1.2 m) -> false') do
  !$game_player.passable?(12.0, 5.0)
end
$game_player.moveto(12.0, 5.0)   # auf die Wand (gy 5.0)
check('auf der Wand nach links hinunter -> false (Stufe 5 m)') do
  !$game_player.passable?(11.0, 5.0, 4)
end
check('auf der Wand flach weiter (rechts) -> true') do
  $game_player.passable?(13.0, 5.0, 6)
end
$game_player.moveto(5.0, 5.0)    # zurück ins Flache
check('hinter der Kartengrenze (x > width) -> false') do
  !$game_player.passable?(125.0, 5.0)
end
$game_temp.player_passable = false
check('player_passable? aus -> immer true (XP-Debug-Verhalten)') do
  $game_player.passable?(125.0, 5.0)
end
$game_temp.player_passable = true

puts '== Bitmap =='
bmp = Bitmap.new(100, 40, 0xFF0000)
check('Größe/Farbe') { bmp.width == 100 && bmp.height == 40 && bmp.color == 0xFF0000 }
check('fill_rect mit neuer Farbe') do
  bmp.fill_rect(0, 0, 10, 10, 0x00FF00)
  bmp.color == 0x00FF00
end
check('fill_rect mit Rect-Hash') do
  r = bmp.fill_rect({ 'x' => 2, 'y' => 3, 'w' => 5, 'h' => 6 }, 0x0000FF)
  r == [2.0, 3.0, 5.0, 6.0, 0x0000FF]
end
check('draw_text merkt die Zeile') do
  bmp.draw_text(0, 0, 100, 20, 'Hallo Welt')
  bmp.text == 'Hallo Welt'
end
bmp.clear
check('clear löscht den Text') { bmp.text.nil? }
bmp.dispose
check('dispose') { bmp.disposed? }

puts '== Sprite und Zeichensortierung =='
GFX = Rune::Gfx::CALLS
RGSSGfx.reset!
s1 = Sprite.new
s1.bitmap = Bitmap.new(10, 10, 0xFFFFFF)
s1.x = 5
s1.y = 6
s1.z = 200
check('registriert') { RGSSGfx.objects.size == 1 }
check('z_effective ohne Viewport') { s1.z_effective == 200.0 }
GFX.clear
Graphics.draw
check('zeichnet Rechteck an x/y mit Bitmap-Farbe') do
  GFX.any? { |c| c[0] == :rect && c[1] == 5.0 && c[2] == 6.0 && c[5] == 0xFFFFFF }
end
s1.visible = false
GFX.clear
Graphics.draw
check('visible=false -> nichts gezeichnet') { GFX.empty? }
s1.visible = true
s1.opacity = 0
GFX.clear
Graphics.draw
check('opacity=0 -> nichts gezeichnet') { GFX.empty? }
s1.opacity = 255

s2 = Sprite.new
s2.bitmap = Bitmap.new(4, 4, 0xFF0000)
s2.z = 100
check('sorted: kleineres z zuerst') do
  liste = RGSSGfx.sorted
  liste.first.equal?(s2) && liste.last.equal?(s1)
end

vp = Viewport.new
vp.z = 500
s3 = Sprite.new(vp)
s3.bitmap = Bitmap.new(4, 4, 0x00FF00)
s3.z = 0
check('Viewport-z addiert sich (z_effective 500)') { s3.z_effective == 500.0 }
check('sorted: Viewport-Sprite liegt über s1 (z 200)') do
  RGSSGfx.sorted.last.equal?(s3)
end
vp.visible = false
check('Viewport unsichtbar -> Kind unsichtbar') { !s3.visible_effective? }
vp.visible = true
vp.opacity = 128
check('Viewport-opacity halbiert die Kind-Deckkraft') do
  (s3.opacity_effective - 128 / 255.0).abs < 0.001
end

s1.dispose
check('dispose entfernt aus der Liste') do
  s1.disposed? && !RGSSGfx.objects.include?(s1)
end

puts '== Window =='
RGSSGfx.reset!
Rune::Input.pressed = []
Rune::Input.down = []
fenster = Window.new(40, 40, 420, 200)
fenster.title = 'Wähle'
fenster.lines = %w[Eins Zwei Drei]
fenster.index = 0
fenster.active = true
check('Standard: kein Befehl') { fenster.update.nil? }
Rune::Input.down = [:down]
check('dir4=2: Cursor runter') do
  fenster.update
  fenster.index == 1
end
Rune::Input.down = [:up]
check('dir4=8: Cursor hoch') do
  fenster.update
  fenster.index.zero?
end
Rune::Input.down = [:up]
check('Cursor wrappt nach oben') do
  fenster.update
  fenster.index == 2
end
Rune::Input.down = []
Rune::Input.pressed = [:enter]
check('C (Enter) -> :ok') { fenster.update == :ok }
Rune::Input.pressed = [:escape]
check('B (Esc) -> :cancel') { fenster.update == :cancel }
Rune::Input.pressed = []
fenster.active = false
Rune::Input.down = [:down]
check('inaktiv: update bewegt nichts') do
  fenster.update
  fenster.index == 2
end
Rune::Input.down = []
UI::CALLS.clear
fenster.active = true
fenster.index = 0
fenster.help = '↑↓ + E'
Graphics.draw
check('Window zeichnet Panel, Titel und Zeilen über UI') do
  UI::CALLS.any? { |c| c[0] == :panel } &&
    UI::CALLS.any? { |c| c[0] == :text && c[3] == 'Wähle' } &&
    UI::CALLS.any? { |c| c[0] == :text && c[3].to_s.include?('Eins') }
end
fenster.visible = false
UI::CALLS.clear
Graphics.draw
check('visible=false -> Fenster zeichnet nicht') { UI::CALLS.empty? }
fenster.visible = true

puts '== $scene und Graphics.scene_update =='
class TestSzene
  attr_reader :aufrufe
  def initialize(fehler: false)
    @aufrufe = 0
    @fehler = fehler
  end

  def main
    @aufrufe += 1
    raise 'absichtlicher Szenen-Fehler' if @fehler
  end
end

szene = TestSzene.new
$scene = szene
Graphics.scene_update
Graphics.scene_update
check('scene_update ruft Scene#main pro Frame') { szene.aufrufe == 2 }
kaputt = TestSzene.new(fehler: true)
$scene = kaputt
Graphics.scene_update
check('Szenen-Fehler: $scene fällt weg, kein Absturz') { $scene.nil? }
$scene = nil
Graphics.scene_update
check('ohne Szene passiert nichts') { true }

puts '== RGSS.reset! / Graphics.scene_reset! =='
$scene = TestSzene.new
RGSSGfx.reset!
Sprite.new
check('Objekte liegen bereit') { RGSSGfx.objects.size == 1 && !$scene.nil? }
RGSS.reset!
check('reset!: Sprites und Szene weg') { RGSSGfx.objects.empty? && $scene.nil? }
check('reset!: $game_temp ist frisch') do
  $game_temp.is_a?(RGSS::GameTemp) && $game_temp.message_text.nil?
end

puts($fail.zero? ? 'GRAPHICS-TEST OK (0 Fehler)' : "GRAPHICS-TEST FEHLGESCHLAGEN (#{$fail} Fehler)")
exit($fail.zero? ? 0 : 1)
