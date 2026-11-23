#encoding: utf-8
# ============================================================================
#  Prozedurale Texturen: werden hier in Ruby gerechnet und als RGBA-Bytestring
#  an die Engine gereicht (Rune::Gfx.texture). Kein Bilddatei-Loader nötig,
#  und Mods können eigene Texturen genauso anmelden.
#
#  Alle Texturen 64x64, deterministisch (fester Seed pro Textur), Kachelgrsse
#  in Metern steht beim Prefab-Teil (tex:/tile:).
#
#  Etappe 17f: Zusätzlich dürfen Projekte eigene Grafiken mitbringen. Der
#  Editor importiert PNG/BMP und legt sie als rohe RGBA-Datei plus
#  data/textures.json ab; register_project reicht sie beim Start an die
#  Engine weiter (Ruby hat keinen PNG-Decoder, die Engine braucht nur Bytes).
# ============================================================================

require 'json'

module Textures
  SIZE = 64

  # Kleiner deterministischer Zufall (LCG) -> gleiche Textur bei jedem Start.
  def self.rng(seed)
    state = seed & 0x7FFFFFFF
    lambda do
      state = (state * 11_035_15245 + 12_345) & 0x7FFFFFFF
      state.to_f / 0x7FFFFFFF
    end
  end

  def self.clamp8(v)
    v = v.round
    v < 0 ? 0 : (v > 255 ? 255 : v)
  end

  # Buffer als Binärstring, Pixel für Pixel RGBA.
  def self.make(seed)
    buf = String.new(capacity: SIZE * SIZE * 4, encoding: 'ASCII-8BIT')
    r = rng(seed)
    [buf, r]
  end

  def self.put(buf, x, y, cr, cg, cb)
    i = (y * SIZE + x) * 4
    buf[i]     = clamp8(cr).chr
    buf[i + 1] = clamp8(cg).chr
    buf[i + 2] = clamp8(cb).chr
    buf[i + 3] = 255.chr
  end

  # ------------------------------------------------------- einzelne Texturen

  # Gras: grüne Grundfläche mit Rauschen und vereinzelt helleren Halmen.
  def self.gras
    buf, r = make(11)
    SIZE.times do |y|
      SIZE.times do |x|
        n = r.call * 2.0 - 1.0
        blade = r.call < 0.04 ? 26 : 0
        put(buf, x, y, 88 + n * 14 + blade, 124 + n * 16 + blade, 62 + n * 10)
      end
    end
    buf
  end

  # Weg: Erdboden mit dunklen Steinchen.
  def self.weg
    buf, r = make(22)
    SIZE.times do |y|
      SIZE.times do |x|
        n = r.call * 2.0 - 1.0
        stone = r.call < 0.05 ? -28 : 0
        put(buf, x, y, 142 + n * 16 + stone, 120 + n * 14 + stone, 90 + n * 12 + stone)
      end
    end
    buf
  end

  # Wand: heller Verputz mit feinem Rauschen.
  def self.wand
    buf, r = make(33)
    SIZE.times do |y|
      SIZE.times do |x|
        n = r.call * 2.0 - 1.0
        put(buf, x, y, 186 + n * 10, 176 + n * 10, 152 + n * 9)
      end
    end
    buf
  end

  # Dach: Schindelreihen (8 Pixel hoch), jede Reihe leicht versetzt geschattet.
  def self.dach
    buf, r = make(44)
    SIZE.times do |y|
      row = y / 8
      shade = [row * 7 % 23, 14].min
      SIZE.times do |x|
        n = r.call * 2.0 - 1.0
        edge = (y % 8 == 0) ? -34 : 0        # dunkle Fuge zwischen den Reihen
        put(buf, x, y, 148 + n * 12 - shade + edge, 66 + n * 10 - shade / 2 + edge,
            50 + n * 8 - shade / 2 + edge)
      end
    end
    buf
  end

  # Holz: vier vertikale Bretter mit Maserungsstreifen.
  def self.holz
    buf, r = make(55)
    SIZE.times do |y|
      SIZE.times do |x|
        plank = x / 16
        base = [0, 8, -10, 4][plank]
        grain = ((x * 7 + y * 3) % 23 == 0) ? -22 : 0
        streak = r.call < 0.06 ? -14 : 0
        gap = (x % 16 == 0) ? -36 : 0          # Fuge zwischen den Brettern
        n = r.call * 2.0 - 1.0
        put(buf, x, y, 112 + base + n * 8 + grain + streak + gap,
            76 + base * 0.7 + n * 7 + grain / 2 + streak + gap,
            48 + base * 0.5 + n * 6 + grain / 2 + streak + gap)
      end
    end
    buf
  end

  # Laub: dunkles Blattgrün mit fleckigen hellen Stellen.
  def self.laub
    buf, r = make(66)
    SIZE.times do |y|
      SIZE.times do |x|
        n = r.call * 2.0 - 1.0
        blob = r.call < 0.08 ? 20 : 0
        put(buf, x, y, 47 + n * 10 + blob, 108 + n * 14 + blob, 54 + n * 10 + blob / 2)
      end
    end
    buf
  end

  # Rinde: braune vertikale Streifen (Stamm).
  def self.rinde
    buf, r = make(77)
    SIZE.times do |y|
      SIZE.times do |x|
        streak = ((x * 5 + (y / 6) * 3) % 17 == 0) ? -24 : 0
        n = r.call * 2.0 - 1.0
        put(buf, x, y, 92 + n * 10 + streak, 66 + n * 8 + streak / 2, 48 + n * 7 + streak / 2)
      end
    end
    buf
  end

  # Fels: grau mit hellen/dunklen Flecken.
  def self.fels
    buf, r = make(88)
    SIZE.times do |y|
      SIZE.times do |x|
        n = r.call * 2.0 - 1.0
        blob = r.call < 0.07 ? (r.call < 0.035 ? -26 : 22) : 0
        put(buf, x, y, 124 + n * 12 + blob, 122 + n * 12 + blob, 116 + n * 11 + blob)
      end
    end
    buf
  end

  # ------------------------------------------------------------ Anmeldung

  LIST = {
    'gras'  => method(:gras),
    'weg'   => method(:weg),
    'wand'  => method(:wand),
    'dach'  => method(:dach),
    'holz'  => method(:holz),
    'laub'  => method(:laub),
    'rinde' => method(:rinde),
    'fels'  => method(:fels),
  }.freeze

  # Alle Texturen an die Engine reichen. Bei F5 erneut aufgerufen; der
  # Renderer verwendet vorhandene Namens-Slots und unveränderte Pixel wieder.
  def self.register_all
    LIST.each do |name, maker|
      Rune::Gfx.texture(name, SIZE, SIZE, maker.call)
    end
    true
  end

  # -------------------------------------------------- Importierte Grafiken

  # Einträge aus data/textures.json (nur vollständige, benannte Materialien).
  def self.project_meta
    @project_meta ||= begin
      path = File.join(Database.root, 'textures.json')
      if File.exist?(path)
        data = JSON.parse(File.read(path, encoding: 'UTF-8'))
        list = data.is_a?(Array) ? data : (data['textures'] || [])
        list.select do |h|
          h.is_a?(Hash) && !h['name'].to_s.empty? && !h['file'].to_s.empty?
        end
      else
        []
      end
    rescue StandardError => e
      Rune::Log.error("Textures: textures.json nicht lesbar: #{e.message}")
      []
    end
  end

  # Kachelgrösse eines Materials in Metern (Standard 2.0).
  def self.tile_of(name)
    h = project_meta.find { |x| x['name'].to_s == name.to_s }
    h && h['tile'] ? h['tile'].to_f : 2.0
  end

  # Importierte Texturen anmelden. Liefert die Anzahl geladener Materialien.
  def self.register_project
    imported = 0
    project_meta.each do |h|
      w  = (h['w'] || SIZE).to_i
      hh = (h['h'] || SIZE).to_i
      next unless w > 0 && hh > 0

      file = File.join(Database.root, 'textures', File.basename(h['file'].to_s))
      unless File.exist?(file)
        Issues.report('Texturen', "Datei fehlt: #{File.basename(h['file'].to_s)} (Textur '#{h['name']}')")
        next
      end

      bytes = File.binread(file)
      unless bytes.bytesize == w * hh * 4
        Rune::Log.warn("Textur '#{h['name']}': #{bytes.bytesize} Bytes, " \
                       "erwartet #{w * hh * 4}")
        Issues.report('Texturen', "Textur '#{h['name']}': #{bytes.bytesize} Bytes, erwartet #{w * hh * 4}")
        next
      end

      Rune::Gfx.texture(h['name'].to_s, w, hh, bytes)
      imported += 1
    end
    Rune::Log.info("Projekt-Texturen: #{imported} geladen") if imported.positive?
    imported
  end
end
