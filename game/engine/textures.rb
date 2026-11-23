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

  MAX_DIMENSION = 4096
  MAX_BYTES = 64 * 1024 * 1024
  MAX_MANIFEST_BYTES = 2 * 1024 * 1024
  MAX_MATERIALS = 1024

  # Wie der Editor lehnt die Laufzeit doppelte Schlüssel ab. JSON.parse
  # benutzt []= auch für verschachtelte Objekte.
  class UniqueObject < Hash
    def []=(key, value)
      raise JSON::ParserError, "doppelter Schlüssel: #{key}" if key?(key)

      super
    end
  end

  # Beim Projektwechsel/F5 keine alten Projekt-Slots oder Metadaten behalten.
  # Eingebaute Namen liegen im Renderer in einer separaten Ebene.
  def self.begin_project
    @project_meta = @project_meta_root = nil
    Rune::Gfx.begin_project_textures
  end

  def self.end_project
    @project_meta = @project_meta_root = nil
    Rune::Gfx.end_project_textures
  end

  def self.valid_name?(value)
    value.is_a?(String) && value.encoding == Encoding::UTF_8 && value.valid_encoding? &&
      value.bytesize.between?(1, 1024) && value.bytes.none? { |b| b < 32 || b == 127 }
  end

  # Ein einziges Windows-Dateinamen-Segment, dieselben Regeln wie im Editor.
  def self.flat_file?(value)
    return false unless value.is_a?(String) && value.encoding == Encoding::UTF_8 && value.valid_encoding?
    return false unless value.bytesize.between?(1, 255)
    return false if value.end_with?('.', ' ') || value.bytes.any? { |b| b < 32 || b == 127 }
    return false if value.match?(/[\\\/:*?"<>|]/)

    stem = value.split('.', 2).first.sub(/ +\z/, '').tr('a-z', 'A-Z')
    return false if %w[CON PRN AUX NUL CONIN$ CONOUT$].include?(stem)
    return false if stem.match?(/\A(?:COM|LPT)(?:[1-9]|¹|²|³)\z/)

    true
  end

  def self.dimension(value, missing: false)
    return 64 if missing
    return nil unless value.is_a?(Integer) || value.is_a?(Float)
    return nil if value.is_a?(Float) && !value.finite?
    return nil unless value >= 1 && value <= MAX_DIMENSION && value.to_i == value

    value.to_i
  end

  def self.valid_entry?(entry)
    return false unless entry.is_a?(Hash) && valid_name?(entry['name']) && flat_file?(entry['file'])

    w = dimension(entry['w'], missing: !entry.key?('w'))
    h = dimension(entry['h'], missing: !entry.key?('h'))
    w && h && w * h * 4 <= MAX_BYTES
  end

  def self.finite_json?(value)
    case value
    when Float then value.finite?
    when Array then value.all? { |v| finite_json?(v) }
    when Hash then value.all? { |_k, v| finite_json?(v) }
    else true
    end
  end

  def self.report_texture(message)
    Rune::Log.warn("Textures: #{message}")
    Issues.report('Texturen', message) if defined?(Issues)
  end

  # Die native Brücke prüft Wurzel, alle Projektpfad-Komponenten, Reparse-
  # punkte, Dateityp und Länge per Handle. Ohne sie wird NICHT unsicher auf
  # File.binread zurückgefallen (z. B. in externen Ruby-Werkzeugen).
  def self.read_asset(file = nil, expected = nil)
    return [:unavailable, nil] unless defined?(Rune::Assets) && Rune::Assets.respond_to?(:read_texture)

    project = defined?($project_root) && $project_root ? $project_name : nil
    Rune::Assets.read_texture(Database.root.to_s, project, file, expected)
  rescue StandardError => e
    Rune::Log.warn("Textures: Dateizugriff fehlgeschlagen: #{e.message}")
    [:io, nil]
  end

  # Der Cache ist an die Datenwurzel gebunden und wird bei F5 explizit geleert.
  # Beschädigte Dokumente werden nie durch eine leere Datei ersetzt.
  def self.project_meta
    key = [Database.root.to_s, defined?($project_root) && $project_root ? $project_name : nil]
    return @project_meta if @project_meta && @project_meta_root == key

    @project_meta_root = key
    @project_meta = begin
      status, raw = read_asset
      if status == :missing
        []
      elsif status != :ok || !raw.is_a?(String) || raw.bytesize > MAX_MANIFEST_BYTES
        report_texture("textures.json nicht sicher lesbar (#{status})")
        []
      else
        raw = raw.dup.force_encoding(Encoding::UTF_8)
        raise JSON::ParserError, 'ungültiges UTF-8' unless raw.valid_encoding?

        data = JSON.parse(raw, object_class: UniqueObject, max_nesting: 100)
        raise JSON::ParserError, 'nicht endliche Zahl' unless finite_json?(data)
        list = if data.is_a?(Array)
                 data
               elsif data.is_a?(Hash) && !data.key?('textures')
                 []
               elsif data.is_a?(Hash)
                 data['textures']
               end
        raise JSON::ParserError, 'textures ist keine Liste' unless list.is_a?(Array)
        raise JSON::ParserError, 'zu viele Materialien' if list.size > MAX_MATERIALS

        names = {}
        list.each do |item|
          next unless item.is_a?(Hash) && valid_name?(item['name'])
          raise JSON::ParserError, "doppelter Materialname: #{item['name']}" if names.key?(item['name'])

          names[item['name']] = true
        end
        valid = list.select { |item| valid_entry?(item) }
        report_texture("#{list.size - valid.size} ungültige Materialeinträge übersprungen") if valid.size != list.size
        valid
      end
    rescue StandardError => e
      report_texture("textures.json nicht lesbar: #{e.message}")
      []
    end
  end

  # Kachelgrösse eines Materials in Metern (Standard 2.0).
  def self.tile_of(name)
    entry = project_meta.find { |item| item['name'] == name.to_s }
    tile = entry && entry['tile']
    return 2.0 unless (tile.is_a?(Integer) || tile.is_a?(Float)) &&
                      (!tile.is_a?(Float) || tile.finite?) && tile >= 0.01 && tile <= 10_000

    tile.to_f
  end

  # Importierte Texturen anmelden. Alle Reads und Dimensionen sind begrenzt;
  # fehlerhafte Dateien betreffen nicht die restlichen Einträge.
  def self.register_project
    imported = 0
    project_meta.each do |entry|
      w = dimension(entry['w'], missing: !entry.key?('w'))
      h = dimension(entry['h'], missing: !entry.key?('h'))
      expected = w * h * 4
      status, bytes = read_asset(entry['file'], expected)
      unless status == :ok && bytes.is_a?(String) && bytes.bytesize == expected
        report_texture("Textur '#{entry['name']}' nicht sicher lesbar (#{status}, erwartet #{expected} Bytes)")
        next
      end

      result = Rune::Gfx.texture(entry['name'], w, h, bytes)
      if result.is_a?(Integer) && result.negative?
        report_texture("Textur '#{entry['name']}' konnte nicht im Renderer angemeldet werden")
        next
      end
      imported += 1
    end
    Rune::Log.info("Projekt-Texturen: #{imported} geladen") if imported.positive?
    imported
  end
end
