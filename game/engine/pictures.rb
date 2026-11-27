#encoding: utf-8
# ============================================================================
#  Bildschirm-Bilder (Etappe 20f, RPG Maker XP 231–235).
#
#  XP führt die Bilder in $game_screen (Game_Screen#show_picture /
#  move_picture / rotate_picture / tint_picture / erase_picture) und zeichnet
#  sie in Scene_Map über der Karte, unter Fenstern und Meldungen. Rune3D nutzt
#  dafür die 2D-Ebene des Renderers (Rune::Gfx.image/picture):
#
#    Pictures.show(1, 'pictures/logo.png', x: 0, y: 0, zoom_x: 100, zoom_y: 100,
#                  opacity: 255, blend: 0, origin: 0)
#    Pictures.move(1, x: 320, y: 240, frames: 40)
#    Pictures.rotate(1, speed: 5)        # Grad pro Frame
#    Pictures.tint(1, tone: { r: -40, g: 0, b: 0, gray: 0 }, frames: 20)
#    Pictures.erase(1)
#    Pictures.update(dt)                 # im Engine-Takt, 40 Frames/s
#    Pictures.draw                       # einmal pro Frame, über der Welt
#
#  Bilder werden nach Nummer gezeichnet (1 unten, 50 oben) - wie XP.
#  Koordinaten sind Bildschirmpixel (XP: 640x480, Rune3D: Fenstergröße);
#  `origin` 0 = oben links, 1 = Mitte, `zoom` in Prozent.
#
#  Bewusste Näherung: XP "tönt" die Bildpixel selbst (Kanal-Mapping). Der
#  Renderer kann nur multiplizieren, darum dämpft ein Tint die Gegenkanäle
#  (r > 0 nimmt Grün und Blau zurück, r < 0 dunkelt Rot, gray dunkelt alle).
#  Blend 0 (normal) und 3 (multiplizieren) sind umgesetzt; 1/2 (addieren/
#  abziehen) zeichnen wie 0 - MapCheck warnt.
# ============================================================================

module Pictures
  # XP erlaubt 1..50 Bilder; dieselbe Grenze hält die Zustandsliste klein.
  MAX_NUMMER = 50
  STANDARD_DAUER = 40        # XP-Default für Übergänge (Frames)
  BLEND_MODI = %w[normal addieren abziehen multiplizieren].freeze

  @bilder = {}               # Nummer -> Zustand
  @gemeldet = {}             # Datei -> true (Warnung nur einmal)

  class << self
    # Alle sichtbaren Bilder (Nummer -> Kopie des Zustands) - für Tests/Skripte.
    def alle = @bilder.transform_values(&:dup)

    def get(nummer) = @bilder[nummer]

    def anzahl = @bilder.size

    # Bild zeigen (XP 231). Unbekannte Dateien werden gemeldet und nicht gezeigt.
    def show(nummer, datei, x: 0, y: 0, zoom_x: 100, zoom_y: 100,
             opacity: 255, blend: 0, origin: 0)
      nummer = pruefe_nummer(nummer)
      return false if nummer.nil?

      datei = datei.to_s
      return false if datei.empty?

      name = "picture:#{nummer}"
      geladen = defined?(ImageLoader) && ImageLoader.register(name, datei)

      @bilder[nummer] = {
        number: nummer, datei: datei, name: name, geladen: geladen ? true : false,
        x: x.to_f, y: y.to_f,
        zoom_x: zoom_x.to_f.clamp(0.0, 2000.0), zoom_y: zoom_y.to_f.clamp(0.0, 2000.0),
        opacity: opacity.to_i.clamp(0, 255), blend: blend.to_i.clamp(0, 3),
        origin: origin.to_i.zero? ? 0 : 1,
        angle: 0.0, drehung: 0.0,
        tone: { r: 0, g: 0, b: 0, gray: 0 },
        ziel: nil, dauer: 0.0, rest: 0.0
      }
      Rune::Log.info("Pictures: Bild #{nummer} (#{datei}) bei #{x}/#{y}")
      true
    end

    # Bild verschieben/skalieren/ausblenden (XP 232): linear über `frames`.
    def move(nummer, x: nil, y: nil, zoom_x: nil, zoom_y: nil, opacity: nil,
             blend: nil, origin: nil, frames: STANDARD_DAUER)
      stand = @bilder[nummer]
      return false if stand.nil?

      ziel = {
        x: (x || stand[:x]).to_f, y: (y || stand[:y]).to_f,
        zoom_x: (zoom_x || stand[:zoom_x]).to_f.clamp(0.0, 2000.0),
        zoom_y: (zoom_y || stand[:zoom_y]).to_f.clamp(0.0, 2000.0),
        opacity: (opacity || stand[:opacity]).to_i.clamp(0, 255),
        blend: (blend || stand[:blend]).to_i.clamp(0, 3),
        origin: origin.nil? ? stand[:origin] : (origin.to_i.zero? ? 0 : 1)
      }
      dauer = [frames.to_i, 0].max / frame_rate
      if dauer.zero?
        ziel.each { |k, v| stand[k] = v }
        stand[:ziel] = nil
        stand[:von] = nil
        stand[:dauer] = stand[:rest] = 0.0
      else
        # Startwerte festhalten: XP interpoliert linear vom Stand beim Befehl
        # zum Ziel, nicht schrittweise vom jeweils letzten Frame.
        stand[:von] = { x: stand[:x], y: stand[:y], zoom_x: stand[:zoom_x],
                        zoom_y: stand[:zoom_y], opacity: stand[:opacity] }
        stand[:ziel] = ziel
        stand[:dauer] = dauer
        stand[:rest] = dauer
      end
      true
    end

    # Bild drehen (XP 233): `speed` Grad pro Frame (negativ = linksherum).
    def rotate(nummer, speed: 0)
      stand = @bilder[nummer]
      return false if stand.nil?

      stand[:drehung] = speed.to_f
      true
    end

    # Bild tönen (XP 234): Kanäle -255..255, Übergang über `frames`.
    def tint(nummer, tone: {}, frames: STANDARD_DAUER)
      stand = @bilder[nummer]
      return false if stand.nil?

      ziel = {
        r: tone[:r].to_i.clamp(-255, 255),
        g: tone[:g].to_i.clamp(-255, 255),
        b: tone[:b].to_i.clamp(-255, 255),
        gray: tone[:gray].to_i.clamp(-255, 255)
      }
      dauer = [frames.to_i, 0].max / frame_rate
      if dauer.zero?
        stand[:tone] = ziel
        stand[:tone_ziel] = nil
        stand[:tone_dauer] = stand[:tone_rest] = 0.0
      else
        stand[:tone_von] = stand[:tone].dup
        stand[:tone_ziel] = ziel
        stand[:tone_dauer] = dauer
        stand[:tone_rest] = dauer
      end
      true
    end

    # Bild löschen (XP 235). Rückgabe false, wenn es keins gab.
    def erase(nummer)
      return false unless @bilder.key?(nummer)

      @bilder.delete(nummer)
      Rune::Log.info("Pictures: Bild #{nummer} gelöscht")
      true
    end

    def update(dt)
      return if @bilder.empty?

      @bilder.each_value do |stand|
        if stand[:ziel]
          stand[:rest] = [stand[:rest] - dt, 0.0].max
          anteil = stand[:dauer].positive? ? (1.0 - stand[:rest] / stand[:dauer]) : 1.0
          ziel = stand[:ziel]
          von = stand[:von] || ziel
          # Alle Zahlen linear mischen; blend/origin springen am Ende.
          %i[x y zoom_x zoom_y opacity].each do |k|
            stand[k] = von[k] + (ziel[k] - von[k]) * anteil
          end
          if stand[:rest].zero?
            %i[x y zoom_x zoom_y opacity].each { |k| stand[k] = ziel[k] }
            stand[:blend] = ziel[:blend]
            stand[:origin] = ziel[:origin]
            stand[:ziel] = nil
            stand[:von] = nil
          end
        end

        if stand[:tone_ziel]
          stand[:tone_rest] = [stand[:tone_rest] - dt, 0.0].max
          anteil = stand[:tone_dauer].positive? ? (1.0 - stand[:tone_rest] / stand[:tone_dauer]) : 1.0
          von = stand[:tone_von]
          ziel = stand[:tone_ziel]
          stand[:tone] = von.each_with_object({}) do |(kanal, wert), hash|
            hash[kanal] = (wert + (ziel[kanal] - wert) * anteil).round
          end
          if stand[:tone_rest].zero?
            stand[:tone] = ziel.dup
            stand[:tone_von] = nil
            stand[:tone_ziel] = nil
          end
        end

        stand[:angle] = (stand[:angle] + stand[:drehung] * dt * frame_rate) % 360.0 if
          stand[:drehung] != 0.0
      end
    end

    def draw
      return if @bilder.empty?

      @bilder.keys.sort.each do |nummer|
        stand = @bilder[nummer]
        next unless stand[:geladen]

        breite, hoehe = bildgroesse(stand)
        next if breite <= 0.0 || hoehe <= 0.0

        px, py = ecke(stand, breite, hoehe)
        Rune::Gfx.picture(stand[:name], px, py, breite, hoehe,
                          stand[:opacity] / 255.0, tint_farbe(stand[:tone]),
                          stand[:angle])
      end
    end

    def reset
      @bilder.clear
      @gemeldet.clear
    end

    private

    def frame_rate
      defined?(Graphics) && Graphics.respond_to?(:frame_rate) ? Graphics.frame_rate.to_f : 40.0
    end

    def pruefe_nummer(nummer)
      nummer = nummer.to_i
      return nil unless nummer.between?(1, MAX_NUMMER)

      nummer
    end

    # Bildgröße in Pixeln: Originalgröße mal Zoom, bzw. 1x1 bei 100 %.
    def bildgroesse(stand)
      bild = defined?(ImageLoader) ? ImageLoader.load(stand[:datei]) : nil
      basis_b, basis_h = bild ? [bild[0], bild[1]] : [0, 0]
      [basis_b * stand[:zoom_x] / 100.0, basis_h * stand[:zoom_y] / 100.0]
    end

    # `origin` 1 = Mitte: XP gibt dann die Bildmitte als Ankerpunkt.
    def ecke(stand, breite, hoehe)
      return [stand[:x], stand[:y]] if stand[:origin].zero?

      [stand[:x] - breite / 2.0, stand[:y] - hoehe / 2.0]
    end

    # Tint als Multiplikationsfarbe (siehe Kopfkommentar).
    def tint_farbe(tone)
      r = tone[:r].to_i
      g = tone[:g].to_i
      b = tone[:b].to_i
      gray = tone[:gray].to_i
      fr = 255 - [g, 0].max - [b, 0].max - [gray, 0].max + [r, 0].min
      fg = 255 - [r, 0].max - [b, 0].max - [gray, 0].max + [g, 0].min
      fb = 255 - [r, 0].max - [g, 0].max - [gray, 0].max + [b, 0].min
      (fr.clamp(0, 255) << 16) | (fg.clamp(0, 255) << 8) | fb.clamp(0, 255)
    end
  end
end
