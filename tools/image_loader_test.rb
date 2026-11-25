#encoding: utf-8
# ============================================================================
#  Kopfloser Test für den Bildlader (Etappe 20f, XP 231–235).
#     ruby -I game/engine tools/image_loader_test.rb
#
#  Die Testbilder sind als Base64 eingebettet - der Test braucht keine
#  Bilddatei im Baum und keine Assets-Brücke:
#    - 2x2 PNG RGBA (rot, grün / blau, halbtransparent weiss)
#    - 2x1 PNG RGB
#    - 2x2 PNG Palette (PLTE + tRNS)
#    - 2x2 BMP 24 Bit (von unten gespeichert)
#  Geprüft wird die reine decode()-Funktion plus die Fehlerfälle (kaputte
#  Bytes, JPEG, zu groß) und das Anmelden beim Renderer über eine Attrappe.
# ============================================================================

require 'base64'

module Rune
  ROOT = File.expand_path('..', __dir__)

  module Log
    def self.info(m) = nil
    def self.warn(m) = puts("WARN  #{m}")
    def self.error(m) = puts("ERROR #{m}")
  end

  # Renderer-Attrappe: merkt sich angemeldete Bilder.
  module Gfx
    @images = {}
    class << self
      attr_reader :images
    end
    def self.image(name, w, h, pixels)
      @images[name] = { w: w, h: h, bytes: pixels.bytesize }
      true
    end
    def self.reset! = (@images = {})
  end
end

$LOAD_PATH.unshift(File.join(__dir__, '..', 'game', 'engine'))
require 'image_loader'

PNG_RGBA = Base64.decode64(
  'iVBORw0KGgoAAAANSUhEUgAAAAIAAAACCAYAAABytg0kAAAAE0lEQVR4nGP4z8DwHwyBNAg0AABJSQl4KKDbdwAAAABJRU5ErkJggg=='
)
PNG_RGB = Base64.decode64(
  'iVBORw0KGgoAAAANSUhEUgAAAAIAAAABCAIAAAB7QOjdAAAAD0lEQVR4nGPgEpHTMLIBAAI3ANNbVlHYAAAAAElFTkSuQmCC'
)
PNG_PAL = Base64.decode64(
  'iVBORw0KGgoAAAANSUhEUgAAAAIAAAACCAMAAABFaP0WAAAABlBMVEX/AAAAAP9sof2OAAAAAnRSTlP/QJNrcdoAAAAMSURBVHicY2BgBEIAAAwAAytjy1AAAAAASUVORK5CYII='
)
BMP_24 = Base64.decode64(
  'Qk1GAAAAAAAAADYAAAAoAAAAAgAAAAIAAAABABgAAAAAABAAAAATCwAAEwsAAAAAAAAAAAAAAAD/AP8AAAD/AAD///8AAA=='
)

$fail = 0
def check(name)
  ok = yield
  puts(ok ? "  ok   #{name}" : "  FAIL #{name}")
  $fail += 1 unless ok
rescue StandardError => e
  puts "  FAIL #{name} (Ausnahme: #{e.class}: #{e.message})"
  $fail += 1
end

# Liest ein Pixel als [r, g, b, a].
def pixel(bild, x, y)
  b, h, rgba = bild
  basis = (y * b + x) * 4
  rgba.byteslice(basis, 4).bytes
end

puts '== PNG =='
check('RGBA-PNG: Größe') { ImageLoader.decode(PNG_RGBA)[0..1] == [2, 2] }
check('RGBA-PNG: rot oben links') { pixel(ImageLoader.decode(PNG_RGBA), 0, 0) == [255, 0, 0, 255] }
check('RGBA-PNG: grün oben rechts') { pixel(ImageLoader.decode(PNG_RGBA), 1, 0) == [0, 255, 0, 255] }
check('RGBA-PNG: blau unten links') { pixel(ImageLoader.decode(PNG_RGBA), 0, 1) == [0, 0, 255, 255] }
check('RGBA-PNG: Alpha bleibt erhalten') { pixel(ImageLoader.decode(PNG_RGBA), 1, 1) == [255, 255, 255, 128] }
check('RGBA-PNG: vier Bytes pro Pixel') { ImageLoader.decode(PNG_RGBA)[2].bytesize == 2 * 2 * 4 }
check('RGB-PNG: zwei Pixel, Alpha 255') do
  bild = ImageLoader.decode(PNG_RGB)
  bild[0..1] == [2, 1] && pixel(bild, 0, 0) == [10, 20, 30, 255] &&
    pixel(bild, 1, 0) == [40, 50, 60, 255]
end
check('Paletten-PNG: Farben aus PLTE') do
  bild = ImageLoader.decode(PNG_PAL)
  pixel(bild, 0, 0) == [255, 0, 0, 255] && pixel(bild, 0, 1) == [0, 0, 255, 64] &&
    pixel(bild, 1, 1) == [255, 0, 0, 255]
end
check('Paletten-PNG: Alpha aus tRNS') { pixel(ImageLoader.decode(PNG_PAL), 1, 0) == [0, 0, 255, 64] }

puts '== BMP =='
check('BMP 24 Bit: Größe') { ImageLoader.decode(BMP_24)[0..1] == [2, 2] }
check('BMP 24 Bit: Zeilen von unten nach oben gedreht') do
  # In der Datei steht die untere Zeile zuerst; nach dem Dekodieren liegt sie
  # wieder unten (y wächst nach unten).
  bild = ImageLoader.decode(BMP_24)
  pixel(bild, 0, 0) == [0, 0, 255, 255] &&        # oben links blau
    pixel(bild, 1, 0) == [255, 255, 255, 255] &&  # oben rechts weiss
    pixel(bild, 0, 1) == [255, 0, 0, 255] &&      # unten links rot
    pixel(bild, 1, 1) == [0, 255, 0, 255]         # unten rechts grün
end

puts '== Fehlerfälle =='
check('leere Bytes -> nil') { ImageLoader.decode('').nil? }
check('kein String -> nil') { ImageLoader.decode(nil).nil? }
check('JPEG wird nicht dekodiert') { ImageLoader.decode("\xFF\xD8\xFF\xE0JFIF".b).nil? }
check('abgeschnittenes PNG -> nil') { ImageLoader.decode(PNG_RGBA.byteslice(0, 40)).nil? }
check('kaputter BMP-Kopf -> nil') { ImageLoader.decode('BM' + "\x00" * 20).nil? }
check('falsche PNG-Signatur -> nil') { ImageLoader.decode('XNG' + PNG_RGBA.byteslice(4, 200)).nil? }

puts '== Anmelden =='
Rune::Gfx.reset!
check('register einer fehlenden Datei meldet nichts an') do
  Rune::Gfx.reset!
  ImageLoader.register('pb', 'game/data/bilder/gibt-es-nicht.png') == false &&
    Rune::Gfx.images.empty?
end
check('gecacht? kennt nur geladene Dateien') { !ImageLoader.gecacht?('gibt/es/nicht.png') }

puts($fail.zero? ? 'ALLE CHECKS GRUEN' : "#{$fail} CHECK(S) FEHLGESCHLAGEN")
exit($fail.zero? ? 0 : 1)
