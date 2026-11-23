#encoding: utf-8
# ============================================================================
#  RGSS-Vertiefung (Etappe 19h): Bitmap, Sprite, Window, Viewport, $scene.
#
#  Vorbild sind die gleichnamigen Klassen aus RPG Maker XP (RGSS1). Rune3D
#  hat keine Pixel-Bitmaps: gezeichnet wird über die UI-Primitive der Engine
#  (Rune::Gfx.rect/text, wie UI und Fade). Deshalb:
#
#    Bitmap  - Merker für Größe/Farbe/Füllmodus; fill_rect malt Rechtecke,
#              draw_text malt Text. Bitmap-Dateien laden ist Backlog.
#    Sprite  - zeichnet sein Bitmap als Rechteck an (x, y), mit z-Tiefe,
#              opacity, visible, Zoom und Farbe (tone bleibt ohne Wirkung).
#    Window  - Fenster mit Hintergrund, Titel, Textzeilen und Auswahl
#              (index) - Cursor bewegen Input.dir4 + C/B wie bei XP.
#    Viewport - Gruppe: z/visible/opacity gelten für alle Kinder.
#
#  Alle Objekte registrieren sich in einer Liste; Game.draw ruft einmal
#  Graphics.draw, das nach z sortiert zeichnet (UI-Overlay der Engine liegt
#  darunter, die Bildschirmblende darüber - siehe main.rb).
#
#  $scene: Scripts können eigene Szenen-Objekte setzen ($scene = MeineScene).
#  Graphics.scene_update ruft pro Frame scene.main (XP: Scene#main ist die
#  Szenen-Schleife). Die Engine läuft dabei normal weiter - eine Szene
#  ersetzt hier NICHT die Engine-Schleife (bewusste Abweichung, Doku).
#  Game.enter_map ruft RGSS.reset!: Sprites/Fenster/Szene sind flüchtig.
#
#  Kopflos testbar: ruby -I game/engine tools/graphics_test.rb
# ============================================================================

module RGSSGfx
  # Alle lebenden Zeichen-Objekte (Sprites, Fenster, Viewports).
  @objects = []

  class << self
    attr_reader :objects
  end

  def self.register(objekt)
    @objects << objekt
    objekt
  end

  # Objekt entfernen (dispose) - Idempotent.
  def self.unregister(objekt)
    @objects.delete(objekt)
    nil
  end

  # Alles verwerfen (Kartenwechsel, F5).
  def self.reset!
    @objects = []
    nil
  end

  # Zeichenreihenfolge: z aufsteigend, bei gleichem z die Erzeugung.
  def self.sorted
    @objects.each_with_index.reject { |o, _| o.disposed? || !o.visible_effective? }
            .sort_by { |o, i| [o.z_effective, i] }
            .map(&:first)
  end
end

# ----------------------------------------------------------------------------
# Bitmap: RGSS kennt Pixel; Rune3D merkt sich Größe + Farbe und malt über
# Rune::Gfx. Ein Bitmap "zeichnet" nicht selbst - Sprites/Fenster tun das.
# ----------------------------------------------------------------------------
class Bitmap
  attr_reader :width, :height
  attr_accessor :color

  def initialize(width = 32, height = 32, color = 0xFFFFFF)
    @width  = width.to_i
    @height = height.to_i
    @color  = color.to_i
    @disposed = false
  end

  def disposed? = @disposed
  def dispose
    @disposed = true
  end

  # Rechteck füllen (x, y, w, h) oder Rect-Hash; Farbe optional.
  def fill_rect(x, y, w = nil, h = nil, color = nil)
    if x.is_a?(Hash)
      color ||= y
      w = x['w'] || x['width'] || @width
      h = x['h'] || x['height'] || @height
      y = x['y'] || 0
      x = x['x'] || 0
    end
    @color = color.to_i if color
    [x.to_f, y.to_f, w.to_f, h.to_f, @color]
  end

  # Textzeile für draw-Aufrufe merken (Rückgabe: die Zeile).
  def draw_text(x, y, w, h, text, _align = 0)
    @text = text.to_s
    [@text, x.to_f, y.to_f, w.to_f, h.to_f]
  end

  attr_reader :text

  def clear
    @text = nil
  end
end

# ----------------------------------------------------------------------------
# Sprite: ein Bitmap an einer Bildschirmposition. viewport = Viewport oder
# nil (dann liegt es direkt auf dem Bildschirm).
# ----------------------------------------------------------------------------
class Sprite
  attr_accessor :x, :y, :z, :visible, :opacity, :zoom_x, :zoom_y, :color
  attr_accessor :viewport, :bitmap, :angle

  def initialize(viewport = nil)
    @bitmap   = nil
    @x = 0.0
    @y = 0.0
    @z = 0.0
    @visible  = true
    @opacity  = 255
    @zoom_x   = 1.0
    @zoom_y   = 1.0
    @angle    = 0.0
    @color    = nil
    @viewport = viewport
    @disposed = false
    RGSSGfx.register(self)
  end

  def disposed? = @disposed
  def dispose
    RGSSGfx.unregister(self)
    @disposed = true
  end

  def width
    b = @bitmap
    (b ? b.width : 32).to_f * @zoom_x.to_f
  end

  def height
    b = @bitmap
    (b ? b.height : 32).to_f * @zoom_y.to_f
  end

  # Viewport-Vererbung (RGSSGfx.sorted nutzt das).
  def visible_effective?
    return false unless @visible
    @viewport.nil? || @viewport.disposed? ? true : @viewport.visible_effective?
  end

  def z_effective
    vz = @viewport && !@viewport.disposed? ? @viewport.z : 0
    @z + vz.to_f
  end

  def opacity_effective
    o = @opacity.to_f.clamp(0, 255) / 255.0
    o *= @viewport.opacity_effective if @viewport && !@viewport.disposed?
    o
  end

  def draw
    return if disposed? || !visible_effective?
    return unless @bitmap && !@bitmap.disposed?

    alpha = opacity_effective
    return if alpha <= 0.01
    farbe = @color || @bitmap.color
    Rune::Gfx.rect(@x.to_f, @y.to_f, width, height, farbe, alpha)
  end
end

# ----------------------------------------------------------------------------
# Window: Panel mit Titel, Textzeilen und Auswahl. XP-Verhalten: index -1 =
# keine Auswahl; Tastensteuerung übernimmt update (Input.dir4/C/B), scripts
# dürfen aber auch selbst zeichnen/abfragen.
# ----------------------------------------------------------------------------
class Window
  ZEILEN_HOEHE = 26

  attr_accessor :x, :y, :width, :height, :z, :visible, :opacity
  attr_accessor :viewport, :title, :lines, :index, :help
  attr_accessor :active, :back_color

  def initialize(x = 40, y = 40, width = 420, height = 200, viewport = nil)
    @x = x.to_f
    @y = y.to_f
    @width = width.to_f
    @height = height.to_f
    @z = 0.0
    @visible = true
    @opacity = 255
    @viewport = viewport
    @title = nil
    @lines = []
    @index = -1
    @active = false
    @help = nil
    @back_color = UI::PANEL
    @disposed = false
    RGSSGfx.register(self)
  end

  def disposed? = @disposed
  def dispose
    RGSSGfx.unregister(self)
    @disposed = true
  end

  def visible_effective?
    return false unless @visible
    @viewport.nil? || @viewport.disposed? ? true : @viewport.visible_effective?
  end

  def z_effective
    vz = @viewport && !@viewport.disposed? ? @viewport.z : 0
    @z + vz.to_f
  end

  def opacity_effective
    o = @opacity.to_f.clamp(0, 255) / 255.0
    o *= @viewport.opacity_effective if @viewport && !@viewport.disposed?
    o
  end

  # XP: Window#update bewegt den Cursor und quittiert mit Sound.
  # Rückgabe: :ok (C), :cancel (B), nil (nur Cursor bewegt / inaktiv).
  def update
    return nil unless @active && @visible && !@lines.empty?

    moved = false
    if Input.dir4 == 8 || Input.trigger?(Input::UP)
      @index = @index <= 0 ? @lines.size - 1 : @index - 1
      moved = true
    elsif Input.dir4 == 2 || Input.trigger?(Input::DOWN)
      @index = (@index + 1) % @lines.size
      moved = true
    end
    Rune::Sound.play('klick', 0.4) if moved && @index >= 0

    return nil unless @index >= 0
    return :ok     if Input.trigger?(Input::C)
    return :cancel if Input.trigger?(Input::B)

    nil
  end

  def draw
    return if disposed? || !visible_effective?

    alpha = opacity_effective
    return if alpha <= 0.01
    UI.panel(@x, @y, @width, @height, @back_color, (0.86 * alpha).clamp(0.0, 1.0))
    UI.panel(@x, @y, @width, 2, UI::ACCENT, (0.85 * alpha).clamp(0.0, 1.0))

    y0 = @y + 12
    if @title
      UI.text(@x + 16, y0, @title.to_s, UI::ACCENT, 16)
      y0 += 30
    end
    @lines.each_with_index do |line, i|
      ly = y0 + i * ZEILEN_HOEHE
      break if ly + ZEILEN_HOEHE > @y + @height - 6
      if i == @index
        UI.panel(@x + 8, ly - 2, @width - 16, ZEILEN_HOEHE - 2, UI::ACCENT,
                 (0.22 * alpha).clamp(0.0, 1.0))
        UI.text(@x + 18, ly + 1, "> #{line}", UI::ACCENT, 17)
      else
        UI.text(@x + 18, ly + 1, line.to_s, UI::TEXT, 17)
      end
    end
    UI.text(@x + 16, @y + @height - 20, @help.to_s, UI::TEXT_DIM, 12) if @help
  end
end

# ----------------------------------------------------------------------------
# Window_Selectable: XP-Auswahlfenster (030 - Window_Selectable.rb) - Index,
# Cursor-Bewegung mit Wrap, Seiten/Zeilen. Window erbt das Zeichnen; hier
# kommen die XP-Methoden dazu, die Default-Scripts überall nutzen.
# ----------------------------------------------------------------------------
class Window_Selectable < Window
  attr_reader :index
  attr_accessor :help_window

  def initialize(x = 40, y = 40, width = 420, height = 200, viewport = nil)
    super
    @index = -1
    @help_window = nil
  end

  def row_max = @lines.size
  def page_row_max = [((@height - 32) / ZEILEN_HOEHE).to_i, 1].max
  def page_size = page_row_max
  def top_row = 0   # Scrollen braucht ein Text-Clipping (Backlog)
  def cursor_width = @width - 16
  def cursor_rect = [@x + 8, @y + 12 + (@index < 0 ? 0 : @index) * ZEILEN_HOEHE,
                     cursor_width, ZEILEN_HOEHE]

  # XP: index= clamped auf -1..row_max-1 und aktualisiert das Hilfefenster.
  def index=(value)
    @index = [[value.to_i, -1].max, row_max - 1].min
    update_help
    @index
  end

  def update_help
    @help_window.help = @lines[@index].to_s if @help_window.respond_to?(:help=)
  end

  def cursor_up(wrap = true)
    self.index = @index > 0 ? @index - 1 : (wrap ? row_max - 1 : @index)
  end

  def cursor_down(wrap = true)
    self.index = @index < row_max - 1 ? @index + 1 : (wrap ? 0 : @index)
  end

  def cursor_left(wrap = true) = cursor_up(wrap)
  def cursor_right(wrap = true) = cursor_down(wrap)
  def cursor_page_up   = self.index = [@index - page_size, 0].max
  def cursor_page_down = self.index = [@index + page_size, row_max - 1].min
end

# ----------------------------------------------------------------------------
# Viewport: Gruppe mit eigener z-Ebene, sichtbar/opacity für alle Kinder.
# ----------------------------------------------------------------------------
class Viewport
  attr_accessor :x, :y, :width, :height, :z, :visible, :opacity

  def initialize(x = 0, y = 0, width = 0, height = 0)
    @x = x.to_f
    @y = y.to_f
    @width = width.to_f     # 0 = ganzer Bildschirm (Kinder clipping: Backlog)
    @height = height.to_f
    @z = 0.0
    @visible = true
    @opacity = 255
    @disposed = false
    RGSSGfx.register(self)
  end

  def disposed? = @disposed
  def dispose
    RGSSGfx.unregister(self)
    @disposed = true
  end

  def visible_effective? = @visible
  def z_effective = @z
  def opacity_effective = @opacity.to_f.clamp(0, 255) / 255.0

  # Ein Viewport zeichnet nichts selbst - nur seine Kinder.
  def draw; end
end

# ============================================================================
#  Graphics-Erweiterung (das Modul selbst steht in rgss.rb, 19e):
#  Szene-Schleife ($scene) und das gemeinsame Zeichnen aller RGSS-Objekte.
# ============================================================================
module Graphics
  class << self
    attr_accessor :scene
  end

  # Pro Frame: $scene.main aufrufen, wenn die Szene es anbietet (XP-Name).
  def self.scene_update
    return unless $scene
    $scene.main if $scene.respond_to?(:main)
  rescue StandardError => e
    Rune::Log.error("RGSS-Szene: #{e.class}: #{e.message}")
    Issues.report('RGSS', "Szenen-Fehler: #{e.message}", severity: 'error')
    $scene = nil
  end

  # Alle RGSS-Objekte zeichnen (nach z sortiert). Ruft Game.draw.
  def self.draw
    RGSSGfx.sorted.each(&:draw)
  end

  # Kartenwechsel/F5: Objekte und Szene verwerfen.
  def self.scene_reset!
    RGSSGfx.reset!
    $scene = nil
  end
end
