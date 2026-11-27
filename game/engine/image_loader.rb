#encoding: utf-8
# ============================================================================
#  Bildlader für die Bildschirm-Bilder (Etappe 20f, XP 231–235).
#
#  Der Renderer zeichnet Bilder als Textur; Bilddateien dekodiert Rune3D hier
#  in Ruby und meldet die Rohpixel über Rune::Gfx.image an. Unterstützt werden
#  die Formate, die auch XP für Bilder kennt:
#
#    PNG  8 Bit, nicht verschränkt, Farbtypen 0/2/3/4/6 (Grau, RGB, Palette,
#         Grau+Alpha, RGBA) mit optionaler tRNS-Transparenz
#    BMP  24 und 32 Bit, unkomprimiert, aufrecht oder von oben gespeichert
#
#  JPEG wird bewusst nicht dekodiert (bräuchte einen eigenen Codec); die Datei
#  wird dann mit einer Warnung übersprungen.
#
#  `decode(bytes)` ist eine reine Funktion (Bytes rein, Pixel raus) und damit
#  kopflos testbar; `load(path)` holt die Datei über die sichere
#  Assets-Brücke, `register(name, path)` meldet sie beim Renderer an.
# ============================================================================

require 'zlib'

module ImageLoader
  PNG_SIGNATUR = "\x89PNG\r\n\x1a\n".b.freeze
  # XP-Bilder sind klein; 4096 wie bei den Texturen ist die harte Grenze.
  MAX_KANTE = 4096

  @cache = {}       # Pfad -> [w, h, rgba] (nur erfolgreiche Ladungen)
  @gemeldet = {}    # Pfad -> true, damit Warnungen nicht pro Frame kommen

  class << self
    # Dekodiert PNG- oder BMP-Bytes. Rückgabe [breite, höhe, rgba-bytes] oder
    # nil, wenn das Format nicht unterstützt wird.
    def decode(bytes)
      return nil unless bytes.is_a?(String)

      roh = bytes.b
      return dekodiere_png(roh) if roh.start_with?(PNG_SIGNATUR)
      return dekodiere_bmp(roh) if roh.start_with?('BM')
      nil
    end

    # Bild über die sichere Assets-Brücke laden und dekodieren.
    # Rückgabe [w, h, rgba] oder nil.
    def load(pfad)
      pfad = pfad.to_s
      return @cache[pfad] if @cache.key?(pfad)
      return nil if pfad.empty?

      bytes = lese_datei(pfad)
      return nil if bytes.nil?

      bild = decode(bytes)
      if bild.nil?
        warn(pfad, 'Format nicht unterstützt (PNG oder BMP erwartet)')
        return nil
      end
      @cache[pfad] = bild
    rescue StandardError => e
      warn(pfad, "Laden fehlgeschlagen: #{e.message}")
      nil
    end

    # Bild beim Renderer anmelden (Name = Schlüssel für Rune::Gfx.picture).
    def register(name, pfad)
      bild = load(pfad)
      return false if bild.nil?
      return false unless defined?(Rune::Gfx) && Rune::Gfx.respond_to?(:image)

      Rune::Gfx.image(name.to_s, bild[0], bild[1], bild[2]) ? true : false
    end

    def cache_leeren
      @cache.clear
      @gemeldet.clear
    end

    def gecacht?(pfad) = @cache.key?(pfad.to_s)

    # Bildgröße ohne Pixel (für MapCheck/Hinweise): [w, h] oder nil.
    def groesse(pfad)
      bild = load(pfad)
      bild ? [bild[0], bild[1]] : nil
    end

    private

    def lese_datei(pfad)
      if defined?(Rune::Assets) && Rune::Assets.respond_to?(:read_texture)
        project = defined?($project_root) && $project_root ? $project_name : nil
        status, roh = Rune::Assets.read_texture(Database.root.to_s, project, pfad, nil)
        unless status == :ok && roh.is_a?(String)
          warn(pfad, "Datei nicht lesbar (#{status})")
          return nil
        end
        roh
      elsif defined?(Rune::ROOT) && Rune::ROOT
        # Externe Werkzeuge und Tests ohne Brücke lesen relativ zur Wurzel.
        voll = File.join(Rune::ROOT, pfad.to_s)
        File.exist?(voll) ? File.binread(voll) : nil
      end
    end

    def warn(pfad, text)
      key = [pfad, text]
      return if @gemeldet[key]

      @gemeldet[key] = true
      Rune::Log.warn("Bild '#{pfad}': #{text}") if defined?(Rune::Log)
      Issues.report('Bilder', "Bild '#{pfad}': #{text}") if defined?(Issues)
    end

    # ---------------------------------------------------------------- PNG
    def dekodiere_png(roh)
      pos = PNG_SIGNATUR.bytesize
      kopf = nil
      palette = nil
      transparenz = nil
      idat = +''.b

      while pos + 8 <= roh.bytesize
        laenge = roh.byteslice(pos, 4).unpack1('N')
        typ = roh.byteslice(pos + 4, 4)
        return nil if laenge.negative? || pos + 12 + laenge > roh.bytesize

        daten = roh.byteslice(pos + 8, laenge)
        case typ
        when 'IHDR'
          breite, hoehe, tiefe, farbtyp, kompression, filter, interlace =
            daten.unpack('NNCCCCC')
          return nil unless kompression.zero? && filter.zero? && interlace.zero?
          return nil unless tiefe == 8
          return nil unless [0, 2, 3, 4, 6].include?(farbtyp)
          return nil if breite.zero? || hoehe.zero?
          return nil if breite > MAX_KANTE || hoehe > MAX_KANTE
          kopf = { breite: breite, hoehe: hoehe, farbtyp: farbtyp, tiefe: tiefe }
        when 'PLTE' then palette = daten
        when 'tRNS' then transparenz = daten
        when 'IDAT' then idat << daten
        when 'IEND' then break
        end
        pos += 12 + laenge
      end

      return nil if kopf.nil? || idat.empty?
      return nil if kopf[:farbtyp] == 3 && (palette.nil? || palette.empty?)

      kanaele = { 0 => 1, 2 => 3, 3 => 1, 4 => 2, 6 => 4 }[kopf[:farbtyp]]
      entpackt = Zlib::Inflate.inflate(idat)
      zeile = kopf[:breite] * kanaele
      return nil if entpackt.bytesize < (zeile + 1) * kopf[:hoehe]

      pixel = entfiltere(entpackt, kopf[:hoehe], zeile, kanaele)
      rgba = nach_rgba(pixel, kopf, kanaele, palette, transparenz)
      return nil if rgba.nil?

      [kopf[:breite], kopf[:hoehe], rgba]
    rescue Zlib::Error
      nil
    end

    # Entfernt die PNG-Zeilenfilter (None/Sub/Up/Average/Paeth).
    def entfiltere(daten, hoehe, zeile, kanaele)
      ausgabe = +''.b
      vorher = Array.new(zeile, 0)
      pos = 0
      hoehe.times do
        filter = daten.getbyte(pos)
        pos += 1
        aktuell = daten.byteslice(pos, zeile).bytes
        pos += zeile
        vorher = entfiltere_zeile(aktuell, vorher, filter, kanaele)
        ausgabe << vorher.pack('C*')
      end
      ausgabe
    end

    def entfiltere_zeile(aktuell, vorher, filter, kanaele)
      fall = case filter
             when 0 then nil
             when 1 then :sub
             when 2 then :up
             when 3 then :average
             when 4 then :paeth
             end
      return aktuell if fall.nil?

      aktuell.each_with_index do |wert, i|
        links = i >= kanaele ? aktuell[i - kanaele] : 0
        oben = vorher[i]
        links_oben = i >= kanaele ? vorher[i - kanaele] : 0
        aktuell[i] = case fall
                     when :sub then (wert + links) & 0xFF
                     when :up then (wert + oben) & 0xFF
                     when :average then (wert + ((links + oben) / 2)) & 0xFF
                     when :paeth then (wert + paeth(links, oben, links_oben)) & 0xFF
                     end
      end
      aktuell
    end

    def paeth(a, b, c)
      p = a + b - c
      pa = (p - a).abs
      pb = (p - b).abs
      pc = (p - c).abs
      return a if pa <= pb && pa <= pc
      return b if pb <= pc

      c
    end

    def nach_rgba(pixel, kopf, kanaele, palette, transparenz)
      anzahl = kopf[:breite] * kopf[:hoehe]
      rgba = String.new(capacity: anzahl * 4, encoding: Encoding::BINARY)
      farbtyp = kopf[:farbtyp]

      anzahl.times do |i|
        quelle = i * kanaele
        fall = case farbtyp
               when 0 then [pixel.getbyte(quelle)] * 3 + [255]
               when 2 then [pixel.getbyte(quelle), pixel.getbyte(quelle + 1),
                            pixel.getbyte(quelle + 2), 255]
               when 3 then aus_palette(pixel.getbyte(quelle), palette, transparenz)
               when 4 then [pixel.getbyte(quelle)] * 3 + [pixel.getbyte(quelle + 1)]
               when 6 then [pixel.getbyte(quelle), pixel.getbyte(quelle + 1),
                            pixel.getbyte(quelle + 2), pixel.getbyte(quelle + 3)]
               end
        return nil if fall.nil?

        rgba << fall.pack('C4')
      end
      rgba
    end

    def aus_palette(index, palette, transparenz)
      return nil if palette.nil? || (index * 3) + 2 >= palette.bytesize

      rot = palette.getbyte(index * 3)
      gruen = palette.getbyte(index * 3 + 1)
      blau = palette.getbyte(index * 3 + 2)
      alpha = transparenz && index < transparenz.bytesize ? transparenz.getbyte(index) : 255
      [rot, gruen, blau, alpha]
    end

    # ---------------------------------------------------------------- BMP
    def dekodiere_bmp(roh)
      return nil if roh.bytesize < 54

      versatz = roh.byteslice(10, 4).unpack1('V')
      kopfgroesse = roh.byteslice(14, 4).unpack1('V')
      return nil unless [40, 108, 124].include?(kopfgroesse)

      breite = roh.byteslice(18, 4).unpack1('l<')
      hoehe = roh.byteslice(22, 4).unpack1('l<')
      tiefe = roh.byteslice(28, 2).unpack1('v')
      kompression = roh.byteslice(30, 4).unpack1('V')
      return nil unless [24, 32].include?(tiefe) && kompression.zero?

      oben_nach_unten = hoehe.negative?
      hoehe = hoehe.abs
      return nil if breite <= 0 || hoehe.zero?
      return nil if breite > MAX_KANTE || hoehe > MAX_KANTE

      kanaele = tiefe / 8
      zeile = ((breite * kanaele + 3) / 4) * 4        # 4-Byte-Ausrichtung
      return nil if versatz + zeile * hoehe > roh.bytesize

      rgba = String.new(capacity: breite * hoehe * 4, encoding: Encoding::BINARY)
      hoehe.times do |y|
        quelle = oben_nach_unten ? y : (hoehe - 1 - y)
        basis = versatz + quelle * zeile
        breite.times do |x|
          pos = basis + x * kanaele
          blau = roh.getbyte(pos)
          gruen = roh.getbyte(pos + 1)
          rot = roh.getbyte(pos + 2)
          alpha = kanaele == 4 ? roh.getbyte(pos + 3) : 255
          rgba << [rot, gruen, blau, alpha].pack('C4')
        end
      end
      [breite, hoehe, rgba]
    end
  end
end
