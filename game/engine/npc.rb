#encoding: utf-8
# ============================================================================
#  NPC: ein bewegter Charakter, der einen Wegpunkt-Kurs abläuft (Patrouille).
#
#  Der NPC besitzt ein eigenes Event (action-Trigger) — seine Position folgt
#  der Bewegung, sodass Reden/Benutzen wie bei statischen Events funktioniert.
#  Das Rendern übernimmt ein Node in der Szene (braune Figur, wie der
#  Spieler im Third-Person-Modus).
#
#  Kopflos testbar: ruby -I game/engine tools/npc_test.rb
# ============================================================================

require 'passage'   # Etappe 19k.3: Block-Zonen/Hindernisse respektieren

class Npc
  attr_reader :name, :event, :node
  attr_accessor :speed, :path

  def initialize(h, scene)
    @name   = h['name'] || 'NPC'
    @speed  = (h['speed'] || 1.5).to_f
    @path   = (h['path'] || []).map { |p| [p[0].to_f, p[1].to_f] }
    @target = 0
    # Optionaler Versatz über/unter dem Gelände; alte Karten bleiben bei 0.
    @height_offset = h['y'].is_a?(Numeric) && h['y'].to_f.finite? ? h['y'].to_f : 0.0

    x = h['x'].to_f
    z = h['z'].to_f

    # Event mit Live-Position (x/z werden bei jeder Bewegung aktualisiert).
    ev = { 'name' => @name, 'x' => x, 'y' => @height_offset, 'z' => z,
           'radius' => h['radius'] || 2.5,
           'trigger' => h['trigger'] || 'action',
           'commands' => h['commands'] || [] }
    @event = Event.new(ev)

    @node = Node.new(name: "npc_#{@name}",
                     position: [x, Game.gy(x, z) + @height_offset + 0.9, z],
                     rotation: [0.0, 0.0, 0.0],
                     scale: [0.7, 1.8, 0.7],
                     color: h['color'] || 0xC49A6C)
    scene.add(@node) if scene.respond_to?(:add)
  end

  # Aktuelle Position (XZ).
  def position
    [@event.x, @event.z]
  end

  # Radius, in dem der NPC den Spieler blockt (NPC + Spieler + Luft).
  def block_radius
    0.8
  end

  # Ein Frame Bewegung: Richtung nächsten Wegpunkt laufen, dort Umschalten
  # zum übernächsten. Ohne Wegpunkte bleibt der NPC stehen. Steht der
  # Spieler im Weg (Etappe 15), wartet der NPC statt durch ihn zu laufen.
  def update(dt)
    # Bewegungsroute aktiv (Etappe 19g)? Dann bewegt der Mover diesen NPC -
    # das eigene Wegpunkt-Laufen pausiert, bis die Route fertig ist.
    return if defined?(Mover) && Mover.moving?(self)
    return if @path.empty?

    tx, tz = @path[@target]

    # Etappe 19k.3: liegt der Wegpunkt selbst in einer Block-Zone, wird er
    # übersprungen - sonst stünde die Patrouille dauerhaft vor der Wand.
    if Passage.zone?(tx, tz)
      @target = (@target + 1) % @path.size
      return
    end

    x, z = position
    dx = tx - x
    dz = tz - z
    dist = Math.sqrt(dx * dx + dz * dz)

    if dist <= @speed * dt
      # Wegpunkt erreicht: exakt draufstellen, nächsten anvisieren.
      @event.x = tx
      @event.z = tz
      @target = (@target + 1) % @path.size
    else
      step = @speed * dt
      nx = x + dx / dist * step
      nz = z + dz / dist * step

      # Spieler im Weg? Dann diesen Frame warten.
      px, _py, pz = Rune::Player.position
      ddx = nx - px
      ddz = nz - pz
      return if ddx * ddx + ddz * ddz < block_radius * block_radius

      # Etappe 19k.3: Block-Zone oder Hindernis im Weg? Dann anstossen
      # (stehen bleiben) statt hindurchzulaufen - wie der Spieler.
      return if Passage.blockiert?(nx, nz)

      @event.x = nx
      @event.z = nz
    end

    refresh_terrain_height
  end

  # Nach einer Terrainänderung den Körper neu auf seiner Bodenhöhe setzen.
  def refresh_terrain_height
    @node.position = [@event.x, Game.gy(@event.x, @event.z) + @height_offset + 0.9, @event.z]
  end

  # Position direkt setzen (Bewegungsrouten, Etappe 19g): Event folgt, damit
  # Reden/Benutzen an der neuen Stelle funktioniert, Node folgt fürs Bild.
  def move_to(x, z)
    @event.x = x.to_f
    @event.z = z.to_f
    refresh_terrain_height
  end

  # Blickrichtung drehen (Bewegungsrouten turn_*, Etappe 19h.2): yaw in
  # Radiant um die Hochachse - dieselbe Konvention wie die Spielerkamera.
  def turn_to(yaw)
    rot = @node.rotation
    rot[1] = yaw.to_f
    @node.rotation = rot
  end

  # --- Speichern/Laden (Etappe 15) ------------------------------------------

  # Einmalige NPC-Touch-/Auto-Events müssen ebenso wie Karten-Events einen
  # Spielstand überleben. Alte Spielstände ohne fired bleiben kompatibel.
  def snapshot
    { 'name' => @name, 'x' => @event.x, 'z' => @event.z, 'wp' => @target,
      'fired' => @event.fired }
  end

  # Position, Wegpunkt und Eventzustand nach dem Laden wiederherstellen.
  def restore!(x, z, wp, fired = false)
    @target = wp.to_i % @path.size unless @path.empty?
    @event.x = x.to_f
    @event.z = z.to_f
    @event.fired = !!fired
    refresh_terrain_height
  end

  # --- Picking: gleiche Schnittstelle wie Event ----------------------------
  def box_center
    @node.position
  end

  def pick_radius
    1.2
  end
end
