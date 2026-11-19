#encoding: utf-8
# ============================================================================
#  Picking: Bildschirm -> Weltstrahl -> Objekt.
#
#  Die Engine liefert nur den Strahl (Rune::Camera.ray). Alles Weitere rechnet
#  Ruby: Strahl/Achsenbox-Schnitt nach dem Slab-Verfahren. Rotierte Boxen
#  werden exakt behandelt: der Strahl wird in den lokalen Boxraum zurueck-
#  gedreht (Inverse der Engine-Konvention Rx*Ry*Rz) und dort gegen die
#  ungedrehte Box geprueft.
# ============================================================================

module Pick
  # Drehungen um die drei Achsen, exakt wie die Engine-Matrizen
  # rotation_x/y/z in src/math/math.h (D3D-Konvention).
  def self.rot_x(v, a)
    return v if a.abs < 1e-9
    c = Math.cos(a); s = Math.sin(a)
    [v[0], v[1] * c - v[2] * s, v[1] * s + v[2] * c]
  end

  def self.rot_y(v, a)
    return v if a.abs < 1e-9
    c = Math.cos(a); s = Math.sin(a)
    [v[0] * c + v[2] * s, v[1], -v[0] * s + v[2] * c]
  end

  def self.rot_z(v, a)
    return v if a.abs < 1e-9
    c = Math.cos(a); s = Math.sin(a)
    [v[0] * c - v[1] * s, v[0] * s + v[1] * c, v[2]]
  end

  # Strahl gegen rotierte Box: Weltstrahl in den lokalen Boxraum drehen
  # (Inverse von Rx(rx)*Ry(ry)*Rz(rz) = Rz(-rz)*Ry(-ry)*Rx(-rx)) und dort
  # den normalen Slab-Test fahren. t bleibt in Weltmetern, weil nur
  # gedreht (nicht skaliert) wird.
  def self.ray_rot_box(origin, dir, center, size, rot)
    rx, ry, rz = rot
    off = [origin[0] - center[0], origin[1] - center[1], origin[2] - center[2]]
    off = rot_z(off, -rz)
    off = rot_y(off, -ry)
    off = rot_x(off, -rx)
    d = rot_z(dir, -rz)
    d = rot_y(d, -ry)
    d = rot_x(d, -rx)
    ray_box(off, d, [0.0, 0.0, 0.0], size)
  end
  # Strahl gegen Achsenbox (Mittelpunkt + Kantenlaengen).
  # Rueckgabe: Eintrittspunkt-Distanz (>= 0) oder nil.
  def self.ray_box(origin, dir, center, size)
    tmin = -Float::INFINITY
    tmax =  Float::INFINITY

    3.times do |i|
      half = size[i].abs * 0.5
      lo = center[i] - half
      hi = center[i] + half

      if dir[i].abs < 1e-6
        # Strahl laeuft parallel zu dieser Achse: muss innerhalb der Slabs liegen
        return nil if origin[i] < lo || origin[i] > hi
      else
        t1 = (lo - origin[i]) / dir[i]
        t2 = (hi - origin[i]) / dir[i]
        t1, t2 = t2, t1 if t1 > t2
        tmin = t1 if t1 > tmin
        tmax = t2 if t2 < tmax
        return nil if tmin > tmax
      end
    end

    return nil if tmax < 0.0
    tmin >= 0.0 ? tmin : 0.0
  end

  # Strahl gegen Kugel (fuer Events ohne Box).
  def self.ray_sphere(origin, dir, center, radius)
    oc = [origin[0] - center[0], origin[1] - center[1], origin[2] - center[2]]
    c = oc[0] * oc[0] + oc[1] * oc[1] + oc[2] * oc[2] - radius * radius
    return 0.0 if c <= 0.0 # Ursprung liegt schon in der Kugel

    b = oc[0] * dir[0] + oc[1] * dir[1] + oc[2] * dir[2]
    disc = b * b - c
    return nil if disc < 0.0

    t = -b - Math.sqrt(disc)
    t = -b + Math.sqrt(disc) if t < 0.0
    t >= 0.0 ? t : nil
  end

  # Naechstes Objekt unter dem Strahl.
  # candidate: alles mit position/scale (Node) oder eigenem box_center/box_size.
  def self.closest(origin, dir, candidates, max_dist: 120.0)
    best = nil
    best_t = max_dist

    candidates.each do |c|
      if c.respond_to?(:pick_radius) && c.pick_radius
        center = c.box_center
        t = ray_sphere(origin, dir, center, c.pick_radius)
      elsif c.respond_to?(:position) && c.respond_to?(:scale) && c.position && c.scale
        rot = c.respond_to?(:rotation) ? c.rotation : nil
        if rot && (rot[0].abs > 1e-6 || rot[1].abs > 1e-6 || rot[2].abs > 1e-6)
          t = ray_rot_box(origin, dir, c.position, c.scale, rot)
        else
          t = ray_box(origin, dir, c.position, c.scale)
        end
      else
        next
      end
      next unless t && t < best_t

      best_t = t
      best = c
    end

    best ? [best, best_t] : nil
  end
end