#encoding: utf-8
# ============================================================================
#  NPC: ein bewegter Charakter, der einen Wegpunkt-Kurs ablaeuft (Patrouille).
#
#  Der NPC besitzt ein eigenes Event (action-Trigger) — seine Position folgt
#  der Bewegung, sodass Reden/Benutzen wie bei statischen Events funktioniert.
#  Das Rendern uebernimmt ein Node in der Szene (braune Figur, wie der
#  Spieler im Third-Person-Modus).
#
#  Kopflos testbar: ruby -I game/engine tools/npc_test.rb
# ============================================================================

class Npc
  attr_reader :name, :event, :node
  attr_accessor :speed, :path

  def initialize(h, scene)
    @name   = h['name'] || 'NPC'
    @speed  = (h['speed'] || 1.5).to_f
    @path   = (h['path'] || []).map { |p| [p[0].to_f, p[1].to_f] }
    @target = 0

    x = h['x'].to_f
    z = h['z'].to_f

    # Event mit Live-Position (x/z werden bei jeder Bewegung aktualisiert).
    ev = { 'name' => @name, 'x' => x, 'z' => z,
           'radius' => h['radius'] || 2.5,
           'trigger' => h['trigger'] || 'action',
           'commands' => h['commands'] || [] }
    @event = Event.new(ev)

    @node = Node.new(name: "npc_#{@name}",
                     position: [x, Game.gy(x, z) + 0.9, z],
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

  # Ein Frame Bewegung: Richtung naechsten Wegpunkt laufen, dort Umschalten
  # zum uebernächsten. Ohne Wegpunkte bleibt der NPC stehen. Steht der
  # Spieler im Weg (Etappe 15), wartet der NPC statt durch ihn zu laufen.
  def update(dt)
    return if @path.empty?

    tx, tz = @path[@target]
    x, z = position
    dx = tx - x
    dz = tz - z
    dist = Math.sqrt(dx * dx + dz * dz)

    if dist <= @speed * dt
      # Wegpunkt erreicht: exakt draufstellen, naechsten anvisieren.
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

      @event.x = nx
      @event.z = nz
    end

    @node.position = [@event.x, Game.gy(@event.x, @event.z) + 0.9, @event.z]
  end

  # --- Speichern/Laden (Etappe 15) ------------------------------------------

  # Zustand fuer das Savegame: Position und Wegpunkt-Index.
  def snapshot
    { 'name' => @name, 'x' => @event.x, 'z' => @event.z, 'wp' => @target }
  end

  # Position und Wegpunkt nach dem Laden wiederherstellen.
  def restore!(x, z, wp)
    @target = wp.to_i % @path.size unless @path.empty?
    @event.x = x.to_f
    @event.z = z.to_f
    @node.position = [@event.x, Game.gy(@event.x, @event.z) + 0.9, @event.z]
  end

  # --- Picking: gleiche Schnittstelle wie Event ----------------------------
  def box_center
    @node.position
  end

  def pick_radius
    1.2
  end
end
