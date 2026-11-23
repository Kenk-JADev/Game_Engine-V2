#encoding: utf-8

# Eine Szene besitzt einen Wurzelknoten. Später kommen hier Kamera, Licht,
# Kollision und Event-Interpreter dazu - für Etappe 1 reicht der Baum.
class Scene
  attr_reader :name, :root

  def initialize(name = 'scene')
    @name = name
    @root = Node.new(name: name)
  end

  def add(node)
    @root.add(node)
    node
  end

  # Alle Hindernisse neu an die Engine melden (nach Änderungen an der Szene).
  def refresh_solids
    Rune::Physics.clear_solids
    walk(@root) { |n| n.register_solid }
    self
  end

  def walk(node, &block)
    block.call(node)
    node.children.each { |c| walk(c, &block) }
  end

  def update(dt)
    @root.update(dt)
  end

  def draw
    @root.draw
  end

  def find(name)
    @root.children.find { |c| c.name == name }
  end
end
