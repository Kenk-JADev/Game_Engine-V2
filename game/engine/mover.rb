#encoding: utf-8
# ============================================================================
#  Bewegungsrouten (Etappe 19g, RPG Maker XP: "Bewegungsroute festlegen").
#
#  Eine Route ist eine Folge von Schritten, die ein Ziel (NPC oder Spieler)
#  abläuft - unabhängig vom Interpreter (wie bei XP laufen Routen auch,
#  während ein Textfenster offen ist). Schritte:
#
#    up / down / left / right  - 1 Meter in die Richtung (3D: -z/+z/-x/+x)
#    zufall                    - 1 Meter in eine Zufallsrichtung
#    spieler_nah               - 1 Meter auf den Spieler zu
#    spieler_weg               - 1 Meter vom Spieler weg
#    warte                     - 0.5 s stehenbleiben
#
#  Event-Befehl:
#    {"cmd":"move_route","target":"Wanderer","steps":["up","up","warte"],
#     "repeat":false,"wait":false,"speed":2.0}
#  target = NPC-Name oder "spieler"; wait = Interpreter wartet, bis die
#  Route fertig ist (mit repeat sinnlos - dann Warnung, kein Warten);
#  speed leer = NPC-Tempo bzw. 2.0 m/s für den Spieler.
#
#  Aus Ruby-Scripts (RGSS-Leitlinie 19e): Mover.start('Wanderer', %w[up up])
#  oder der Context-Helfer move_route('Wanderer', %w[up up]).
#
#  Grenzen (dokumentiert, Backlog): sichtbare Events ohne Node können keine
#  Route laufen (nur NPCs haben Körper), Dreh-Schritte (turn_*) fehlen, und
#  gespeicherte Spielstände nehmen Routen nicht mit (wie XP: flüchtig).
#
#  Kopflos testbar: ruby -I game/engine tools/move_route_test.rb
# ============================================================================

module Mover
  SCHRITTE = %w[up down left right zufall spieler_nah spieler_weg warte].freeze

  SCHRITT_WEITE = 1.0   # Meter pro Schritt
  WARTE_DAUER   = 0.5   # Sekunden für 'warte'
  SPIELER_TEMPO = 2.0   # Standard m/s, falls die Route kein speed angibt

  # Eine laufende Route. distanz = Restmeter des aktuellen Schritts.
  Route = Struct.new(:objekt, :spieler, :schritte, :index, :rest, :distanz,
                     :dx, :dz, :wiederholen, :speed, keyword_init: true)

  @routes = []

  class << self
    attr_reader :routes
  end

  # Route starten. target: 'spieler'/'player' oder NPC-Name. Eine alte Route
  # desselben Ziels wird ersetzt (wie XP force_move_route). Rückgabe: die
  # Route (für Mover.running?) oder nil bei Unsinn.
  def self.start(target, steps, repeat: false, speed: nil)
    schritte = Array(steps).map { |s| s.to_s.strip }.reject(&:empty?)
    if schritte.empty?
      Rune::Log.warn('Mover: leere Bewegungsroute ignoriert')
      return nil
    end

    spieler = %w[spieler player].include?(target.to_s.downcase)
    objekt = nil
    unless spieler
      liste = Game.respond_to?(:npcs) ? Game.npcs : []
      objekt = liste.find { |n| n.respond_to?(:name) && n.name == target.to_s }
      unless objekt
        Rune::Log.warn("Mover: Ziel '#{target}' nicht gefunden")
        Issues.report('Mover', "Bewegungsroute: Ziel '#{target}' unbekannt")
        return nil
      end
    end

    tempo = speed.nil? ? nil : speed.to_f
    tempo = nil if tempo && tempo <= 0
    tempo ||= spieler ? SPIELER_TEMPO : (objekt.respond_to?(:speed) ? objekt.speed.to_f : 1.5)

    route = Route.new(objekt: objekt, spieler: spieler, schritte: schritte,
                      index: 0, rest: 0.0, distanz: 0.0, dx: 0.0, dz: 0.0,
                      wiederholen: !!repeat, speed: tempo)

    # Alte Route desselben Ziels ersetzen.
    @routes.reject! do |r|
      spieler ? r.spieler : (!r.spieler && r.objekt.equal?(objekt))
    end
    @routes << route

    Rune::Log.info("Mover: Route für #{spieler ? 'Spieler' : "NPC '#{target}'"} " \
                   "(#{schritte.size} Schritte#{repeat ? ', wiederholend' : ''}, " \
                   "#{format('%.1f', tempo)} m/s)")
    route
  end

  # Läuft diese Route noch? (Interpreter: move_route mit wait)
  def self.running?(route)
    !route.nil? && @routes.include?(route)
  end

  # Bewegt Mover dieses Objekt gerade? (NPC-Pfad-Pause in Npc#update)
  def self.moving?(objekt)
    @routes.any? { |r| !r.spieler && r.objekt.equal?(objekt) }
  end

  def self.active?
    @routes.any?
  end

  # Route eines Ziels (oder alle) beenden.
  def self.stop(objekt = nil)
    @routes.reject! { |r| objekt.nil? || r.spieler || r.objekt.equal?(objekt) }
  end

  # Kartenwechsel/Neustart: Routen sind flüchtig (Ziel-NPCs sind neu).
  def self.reset!
    @routes = []
  end

  # Ein Frame für alle Routen. Fertig-Routen fliegen raus.
  def self.update(dt)
    return if @routes.empty?

    @routes = @routes.select { |r| schritt(r, dt.to_f) }
  end

  # ------------------------------------------------------------- intern

  # Einen Frame arbeiten: erst laufende Bewegung, dann Warten, dann
  # nächsten Schritt starten. Rückgabe false = Route fertig (nicht repeat).
  def self.schritt(r, dt)
    # 1) Aktuelle Bewegung läuft noch?
    if r.distanz > 0.0
      bewege(r, dt)
      return true
    end

    # 2) Warte-Schritt tickt.
    if r.rest > 0.0
      r.rest -= dt
      return true
    end

    # 3) Route zu Ende: wiederholen oder abschliessen.
    if r.index >= r.schritte.size
      if r.wiederholen
        r.index = 0
      else
        return false
      end
    end

    # 4) Nächsten Schritt starten.
    befehl = r.schritte[r.index].to_s
    r.index += 1
    if befehl == 'warte'
      r.rest = WARTE_DAUER
      return true
    end

    vec = richtung(befehl, r)
    if vec.nil?
      Rune::Log.warn("Mover: Schritt '#{befehl}' nicht ausführbar (übersprungen)")
      return true
    end
    r.dx, r.dz = vec
    r.distanz = SCHRITT_WEITE
    bewege(r, dt)
    true
  end

  # Einheitsvektor für einen Schritt; nil = unbekannt/unmöglich.
  def self.richtung(befehl, r)
    case befehl
    when 'up'    then [0.0, -1.0]
    when 'down'  then [0.0, 1.0]
    when 'left'  then [-1.0, 0.0]
    when 'right' then [1.0, 0.0]
    when 'zufall'
      winkel = rand * 2 * Math::PI
      [Math.cos(winkel), Math.sin(winkel)]
    when 'spieler_nah', 'spieler_weg'
      px, _py, pz = Rune::Player.position
      ox, oz = r.spieler ? [px, pz] : r.objekt.position
      dx = px - ox
      dz = pz - oz
      len = Math.sqrt(dx * dx + dz * dz)
      return nil if len < 0.001   # steht schon auf dem Spieler
      dx /= len
      dz /= len
      befehl == 'spieler_weg' ? [-dx, -dz] : [dx, dz]
    end
  end

  # Restmeter des aktuellen Schritts ablaufen (Spieler: gleiten per
  # teleport wie move_player; NPC: move_to setzt Event + Node).
  def self.bewege(r, dt)
    weg = r.speed * dt
    weg = r.distanz if weg > r.distanz
    r.distanz -= weg

    if r.spieler
      px, _py, pz = Rune::Player.position
      nx = px + r.dx * weg
      nz = pz + r.dz * weg
      Rune::Player.teleport(nx, Game.gy(nx, nz) + 0.2, nz)
    else
      x, z = r.objekt.position
      r.objekt.move_to(x + r.dx * weg, z + r.dz * weg)
    end
  end
end
