#encoding: utf-8

# Ein Knoten im Szenengraph. Alles was gezeichnet wird ist ein Node mit einer Box.
# Position/Rotation (Radiant)/Skalierung als [x, y, z]-Arrays.
#
# solid: true  -> der Knoten wird zum Hindernis für den Spieler (Haus, Zaun, Pfosten).
class Node
  attr_accessor :name, :position, :rotation, :scale, :color, :visible, :solid
  attr_accessor :texture, :tile   # Texturname ('gras', 'holz', ...) + Kachel in Metern
  attr_reader   :children

  def initialize(name: 'node', position: [0.0, 0.0, 0.0], rotation: [0.0, 0.0, 0.0],
                 scale: [1.0, 1.0, 1.0], color: 0xFFFFFF, solid: false,
                 texture: nil, tile: 0.0)
    @name     = name
    @position = position
    @rotation = rotation
    @scale    = scale
    @color    = color
    @solid    = solid
    @texture  = texture
    @tile     = tile
    @visible  = true
    @children = []
    @on_update = nil
    @on_draw   = nil
  end

  # Überträgt die Box dieses Knotens als Hindernis an die Engine.
  def register_solid
    return unless @solid
    Rune::Physics.add_solid(@position[0], @position[1], @position[2],
                            @scale[0],    @scale[1],    @scale[2])
  end

  # Block wird jeden Frame mit (node, dt) aufgerufen.
  def on_update(&block)
    @on_update = block
    self
  end

  # Block wird vor dem Zeichnen mit (node) aufgerufen - für Sonderfälle.
  def on_draw(&block)
    @on_draw = block
    self
  end

  def add(child)
    @children << child
    child.register_solid if child.respond_to?(:register_solid)
    child
  end
  alias << add

  def move_to(x, y, z)
    @position[0] = x
    @position[1] = y
    @position[2] = z
    self
  end

  def update(dt)
    @on_update&.call(self, dt)
    @children.each { |c| c.update(dt) }
  end

  def draw
    @on_draw&.call(self)

    if @visible
      Rune::Gfx.box(@position[0], @position[1], @position[2],
                    @scale[0],    @scale[1],    @scale[2],
                    @color,
                    @rotation[0], @rotation[1], @rotation[2],
                    @texture,     @tile)
    end

    @children.each(&:draw)
  end
end
