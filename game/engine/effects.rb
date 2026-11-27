#encoding: utf-8
# ============================================================================
#  Effekte: leichtgewichtiges Partikelsystem (Etappe 14c), komplett in Ruby.
#
#  Partikel sind kleine Boxen (Gfx.box) mit Lebenszeit, Geschwindigkeit und
#  Schwerkraft; das Verblassen passiert über die Grösse (Gfx.box hat kein
#  Alpha). Update/Draw werden von main.rb pro Frame aufgerufen.
#
#  Kopflos testbar: ruby -I game/engine tools/effects_test.rb
# ============================================================================

module Effects
  PARTS = []

  module_function

  # Ausbruch von Partikeln an einem Punkt.
  def burst(x, y, z, color: 0xFFD866, count: 14, speed: 2.2, size: 0.14,
            life: 0.7, gravity: 5.0)
    count.times do
      winkel = rand * Math::PI * 2
      radial = rand
      PARTS << { x: x, y: y, z: z,
                 vx: Math.sin(winkel) * speed * radial,
                 vy: (0.5 + rand * 0.8) * speed * 0.6,
                 vz: Math.cos(winkel) * speed * radial,
                 life: life * (0.7 + rand * 0.6), max: life,
                 size: size * (0.7 + rand * 0.6), color: color, grav: gravity }
    end
  end

  # Funkelnd (Kristall bergen, Belohnung): hell, langsam schwebend.
  def sparkle(x, y, z)
    burst(x, y, z, color: 0x7FE0FF, count: 18, speed: 2.6, size: 0.13,
          life: 0.8, gravity: 3.5)
  end

  # Staubwolke (Landung, Sprung): grau, träge.
  def dust(x, y, z)
    burst(x, y, z, color: 0xB8AFA0, count: 12, speed: 1.6, size: 0.17,
          life: 0.45, gravity: 1.0)
  end

  def clear
    PARTS.clear
  end

  def count
    PARTS.size
  end

  def update(dt)
    PARTS.reject! do |p|
      p[:life] -= dt
      p[:vy] -= p[:grav] * dt
      p[:x] += p[:vx] * dt
      p[:y] += p[:vy] * dt
      p[:z] += p[:vz] * dt
      p[:life] <= 0.0
    end
  end

  def draw
    PARTS.each do |p|
      f = (p[:life] / p[:max]).clamp(0.0, 1.0)
      s = p[:size] * (0.2 + f * 0.8)   # schrumpfen = "verblassen"
      Rune::Gfx.box(p[:x], p[:y], p[:z], s, s, s, p[:color], 0.0, 0.0, 0.0)
    end
  end
end
