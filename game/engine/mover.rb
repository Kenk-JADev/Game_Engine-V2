#encoding: utf-8
# ============================================================================
#  Bewegungsrouten (Etappe 19g, XP-Vokabular seit 19h.2).
#
#  Vorbild ist "Bewegungsroute festlegen" aus RPG Maker XP - die Schritt-
#  Namen und die Codes 1..45 entsprechen den RGSS-Konstanten ROUTE_*
#  (GAME_Character; Quellen: RPG-Maker-Hilfe, MV-Hilfe, tonbi.jp
#  CommandTable, rpg-maker.fr Easy-Scripts - siehe F1 in der
#  LOGIK_PRUEFUNG):
#
#     1 move_down           2 move_left           3 move_right
#     4 move_up             5 move_lower_left     6 move_lower_right
#     7 move_upper_left     8 move_upper_right    9 move_random
#    10 move_toward_player 11 move_away_from_player
#    12 move_forward      13 move_backward       14 jump
#    15 wait              16 turn_down           17 turn_left
#    18 turn_right        19 turn_up             20 turn_90d_right
#    21 turn_90d_left     22 turn_180d           23 turn_90d_right_or_left
#    24 turn_random       25 turn_toward_player  26 turn_away_from_player
#    27 switch_on         28 switch_off          29 change_speed
#    30 change_freq       31 move_animation_on   32 move_animation_off
#    33 stop_animation_on 34 stop_animation_off  35 direction_fix_on
#    36 direction_fix_off 37 through_on          38 through_off
#    39 transparent_on    40 transparent_off     41 change_graphic
#    42 change_opacity    43 change_blending     44 play_se
#    45 script
#
#  (XP-Codes werden als Schritt-Strings verstanden - "1" = move_down usw.,
#  dazu die deutschen Kurznamen aus 19g: up/down/left/right/zufall/
#  spieler_nah/spieler_weg/warte. always_on_top_* gibt es in XP nicht -
#  Rune3D-Eigenbau ohne Code.)
#
#  Nicht umsetzbar in der 3D-Engine (dokumentierte No-Ops mit Warnung beim
#  ersten Auftreten): change_graphic/opacity/blending (NPCs sind Boxen),
#  transparent_*/stop_animation_*/direction_fix_*/through_*/always_on_top_*
#  (2D-Schalter), change_freq (Bewegungsfrequenz gibt es hier nicht - Tempo
#  ist speed). jump = Hüpfer-Partikel statt Sprungphysik; play_se spielt den
#  SE-Namen aus dem Route-Feld code; script führt Ruby-Code im
#  Scripts-Context aus.
#
#  Event-Befehl:
#    {"cmd":"move_route","target":"Wanderer","steps":["move_up","wait"],
#     "repeat":false,"wait":false,"speed":2.0}
#  target = NPC-Name oder "spieler"; wait = Interpreter wartet, bis die
#  Route fertig ist (mit repeat sinnlos - dann Warnung, kein Warten).
#
#  Aus Ruby-Scripts: Mover.start('Wanderer', %w[move_up move_up]) oder der
#  Context-Helfer move_route('Wanderer', %w[turn_toward_player move_forward]).
#
#  Kopflos testbar: ruby -I game/engine tools/move_route_test.rb
# ============================================================================

require 'passage'   # Etappe 19k.3: Routen stoppen an Wänden/Hindernissen

module Mover
  # Kanonische Schritte (XP-Namen). MapCheck::MOVER_SCHRITTE muss dazu passen
  # (move_route_test assertet den Sync; mapcheck.rb lädt vor mover.rb).
  SCHRITTE = %w[
    move_down move_left move_right move_up
    move_lower_left move_lower_right move_upper_left move_upper_right
    move_random move_toward_player move_away_from_player
    move_forward move_backward jump wait
    turn_down turn_left turn_right turn_up
    turn_90d_right turn_90d_left turn_180d turn_90d_right_or_left
    turn_random turn_toward_player turn_away_from_player
    switch_on switch_off change_speed change_freq
    move_animation_on move_animation_off
    stop_animation_on stop_animation_off
    direction_fix_on direction_fix_off through_on through_off
    transparent_on transparent_off
    always_on_top_on always_on_top_off
    change_graphic change_opacity change_blending play_se script
  ].freeze

  # XP-Befehls-Codes als Schritt-String (F1): die ROUTE_*-Konstanten der
  # RPG-Maker-Serie, exakt 0..45 - Quellen: RPG-Maker-XP-Hilfe, MV-Hilfe,
  # tonbi.jp CommandTable (ROUTE_MOVE_DOWN=1 ... ROUTE_SCRIPT=45) und
  # rpg-maker.fr Easy-Scripts-Tabelle. (Die alte Liste '11'..'57' lag um 10
  # daneben, mischte 39/40 mit always_on_top und kannte kein play_se.)
  # '0' = ROUTE_END (XP hängt den Ende-Marker immer an) -> leer, kein Schritt.
  # XP hat KEINE always_on_top-Schritte - die bleiben Rune3D-Eigenbau ohne Code.
  XP_CODES = {
    '0' => '',
    '1' => 'move_down', '2' => 'move_left', '3' => 'move_right',
    '4' => 'move_up', '5' => 'move_lower_left', '6' => 'move_lower_right',
    '7' => 'move_upper_left', '8' => 'move_upper_right',
    '9' => 'move_random', '10' => 'move_toward_player',
    '11' => 'move_away_from_player', '12' => 'move_forward',
    '13' => 'move_backward', '14' => 'jump', '15' => 'wait',
    '16' => 'turn_down', '17' => 'turn_left', '18' => 'turn_right',
    '19' => 'turn_up', '20' => 'turn_90d_right', '21' => 'turn_90d_left',
    '22' => 'turn_180d', '23' => 'turn_90d_right_or_left',
    '24' => 'turn_random', '25' => 'turn_toward_player',
    '26' => 'turn_away_from_player', '27' => 'switch_on',
    '28' => 'switch_off', '29' => 'change_speed', '30' => 'change_freq',
    '31' => 'move_animation_on', '32' => 'move_animation_off',
    '33' => 'stop_animation_on', '34' => 'stop_animation_off',
    '35' => 'direction_fix_on', '36' => 'direction_fix_off',
    '37' => 'through_on', '38' => 'through_off',
    '39' => 'transparent_on', '40' => 'transparent_off',
    '41' => 'change_graphic', '42' => 'change_opacity',
    '43' => 'change_blending', '44' => 'play_se',
    '45' => 'script',
  }.freeze

  # Deutsche Kurznamen aus Etappe 19g (bestehende Karten bleiben gültig).
  ALIASE = {
    'down' => 'move_down', 'left' => 'move_left', 'right' => 'move_right',
    'up' => 'move_up', 'zufall' => 'move_random',
    'spieler_nah' => 'move_toward_player', 'spieler_weg' => 'move_away_from_player',
    'warte' => 'wait',
    'vor' => 'move_forward', 'zurueck' => 'move_backward',
    'runter' => 'turn_down', 'rauf' => 'turn_up',
  }.freeze

  # Etappe 39: XP-Code 14 (jump) hat zwei Offset-Parameter (-100..100).
  # Die Route speichert sie im Schritt selbst: "jump:dx:dz" bzw. "14:dx:dz".
  # Ein nacktes "jump" bleibt aus Kompatibilitätsgründen ein Effekt-Schritt.
  SPRUNG_MAX = 100

  # Schritt-Normalisierung: XP-Code, deutscher Alias oder XP-Name.
  def self.normalize(schritt)
    s = schritt.to_s.strip
    basis, *parameter = s.split(/[\s:]+/)
    basis = XP_CODES[basis] || ALIASE[basis] || basis
    if basis == 'jump' && parameter.size == 2 &&
       parameter.all? { |p| p.match?(/\A[+-]?\d+\z/) }
        return format('jump:%d:%d', parameter[0].to_i, parameter[1].to_i)
    end
    XP_CODES[s] || ALIASE[s] || s
  end

  # Gültige Sprungparameter aus einem normalisierten Schritt lesen.
  def self.jump_parameter(befehl)
    teile = befehl.to_s.split(':')
    return nil unless teile.size == 3 && teile[0] == 'jump'

    dx = Integer(teile[1], 10, exception: false)
    dz = Integer(teile[2], 10, exception: false)
    return nil unless dx && dz && dx.abs <= SPRUNG_MAX && dz.abs <= SPRUNG_MAX

    [dx, dz]
  end

  SCHRITT_WEITE = 1.0   # Meter pro Schritt
  WARTE_DAUER   = 0.5   # Sekunden für 'wait'
  SPIELER_TEMPO = 2.0   # Standard m/s, falls die Route kein speed angibt

  # XP-Richtungen: 2=unten(+z), 4=links(-x), 6=rechts(+x), 8=oben(-z).
  RICHTUNGEN = [2, 4, 6, 8].freeze
  # Blickwinkel (Yaw) je Richtung - Spielerkamera und NPC-Node drehen gleich.
  YAW = { 2 => 0.0, 4 => Math::PI / 2, 6 => -Math::PI / 2, 8 => Math::PI }.freeze

  # Eine laufende Route. distanz = Restmeter des aktuellen Schritts,
  # richtung = aktuelle Blickrichtung (XP 2/4/6/8).
  Route = Struct.new(:objekt, :spieler, :schritte, :index, :rest, :distanz,
                     :dx, :dz, :wiederholen, :speed, :richtung, :code,
                     :frequency, :ignore,
                     keyword_init: true)

  @routes = []
  @gewarnt = {}   # No-Op-Schritte nur einmal melden

  class << self
    attr_reader :routes
  end

  # Route starten. target: 'spieler'/'player' oder NPC-Name. Eine alte Route
  # desselben Ziels wird ersetzt (wie XP force_move_route). code = Zusatzwert
  # für Schritte mit Parameter (switch_on/off: Schaltername, change_speed:
  # m/s, script: Ruby-Code). Rückgabe: die Route oder nil bei Unsinn.
  def self.start(target, steps, repeat: false, speed: nil, code: nil, frequency: nil, ignore: false)
    schritte = Array(steps).map { |s| normalize(s) }.reject(&:empty?)
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
                      wiederholen: !!repeat, speed: tempo, richtung: 2,
                      code: code, frequency: frequency, ignore: !!ignore)

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
    @gewarnt = {}
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

    # 4) Nächsten Schritt ausführen.
    befehl = r.schritte[r.index].to_s
    r.index += 1
    ausfuehren(r, befehl, dt)
    true
  end

  # Einen normalisierten Schritt ausführen (Bewegung setzt r.distanz und
  # läuft über die folgenden Frames; der Rest ist sofort fertig).
  def self.ausfuehren(r, befehl, dt)
    case befehl
    when 'wait'
      # Etappe 48: XP change_freq (1..8) steuert die Pause zwischen Schritten.
      r.rest = r.frequency ? [(9 - r.frequency.to_i) * 0.1, 0.05].max : WARTE_DAUER

    when 'move_down', 'move_left', 'move_right', 'move_up',
         'move_lower_left', 'move_lower_right', 'move_upper_left',
         'move_upper_right', 'move_random', 'move_toward_player',
         'move_away_from_player', 'move_forward', 'move_backward'
      vec = richtung(befehl, r)
      if vec.nil?
        warn_once("Mover: Schritt '#{befehl}' nicht ausführbar (übersprungen)")
      else
        r.dx, r.dz = vec
        r.richtung = vektor_richtung(r.dx, r.dz) || r.richtung
        drehen(r)
        r.distanz = SCHRITT_WEITE
        bewege(r, dt)
      end

    when 'turn_down'  then r.richtung = 2; drehen(r)
    when 'turn_left'  then r.richtung = 4; drehen(r)
    when 'turn_right' then r.richtung = 6; drehen(r)
    when 'turn_up'    then r.richtung = 8; drehen(r)
    when 'turn_90d_right'
      r.richtung = { 2 => 4, 4 => 8, 8 => 6, 6 => 2 }[r.richtung] || 2
      drehen(r)
    when 'turn_90d_left'
      r.richtung = { 2 => 6, 6 => 8, 8 => 4, 4 => 2 }[r.richtung] || 2
      drehen(r)
    when 'turn_180d'
      r.richtung = { 2 => 8, 8 => 2, 4 => 6, 6 => 4 }[r.richtung] || 2
      drehen(r)
    when 'turn_90d_right_or_left'
      r.richtung = { 2 => [4, 6], 4 => [2, 8], 6 => [2, 8], 8 => [4, 6] }[r.richtung].sample
      drehen(r)
    when 'turn_random'
      r.richtung = RICHTUNGEN.sample
      drehen(r)
    when 'turn_toward_player'
      r.richtung = blick_zum_spieler(r, weg: false) || r.richtung
      drehen(r)
    when 'turn_away_from_player'
      r.richtung = blick_zum_spieler(r, weg: true) || r.richtung
      drehen(r)

    when /\Ajump(?::.*)?\z/
      # Etappe 39: echter Sprung wie XP-Code 14 - Regieaktion, darum ohne
      # Physik-Gate und ohne Block-Zonen-Prüfung (XP springt durch Hindernisse).
      # Ohne Parameter bleibt der alte Partikeleffekt (Altformate/Doku).
      parameter = jump_parameter(befehl)
      if parameter
        dx, dz = parameter
        r.richtung = vektor_richtung(dx, dz) || r.richtung
        drehen(r)
        x, z = ziel_position(r)
        nx, nz = x + dx.to_f, z + dz.to_f
        if r.spieler
          Rune::Player.teleport(nx, Game.gy(nx, nz) + 0.2, nz)
        elsif r.objekt.respond_to?(:move_to)
          r.objekt.move_to(nx, nz)
        end
        if defined?(Effects)
          Effects.burst(nx, Game.gy(nx, nz) + 1.0, nz, count: 8)
        end
      elsif defined?(Effects)
        x, z = ziel_position(r)
        Effects.burst(x, Game.gy(x, z) + 1.0, z, count: 8)
      end

    when 'switch_on', 'switch_off'
      name = r.code.to_s
      if name.empty?
        warn_once('Mover: switch_on/off ohne Schaltername im Route-Feld code')
      else
        GameState.set_switch(name, befehl == 'switch_on')
      end

    when 'change_speed'
      r.speed = r.code.to_f if r.code && r.code.to_f > 0

    when 'change_freq'
      # Etappe 48: Frequenz 1..8 (XP); höher = kürzere Pause zwischen Schritten.
      r.frequency = r.code.to_i.clamp(1, 8) if r.code

    when 'play_se'
      # XP-Code 44: 'code' enthält den SE-Namen; XP-Volumen 100 = 1.0.
      # Pitch hat die Engine noch nicht (offen unter "20x Ton").
      name = r.code.to_s
      if name.empty?
        warn_once('Mover: play_se ohne SE-Namen im Route-Feld code')
      else
        Rune::Sound.play(name, 1.0)
      end

    when 'script'
      code = r.code.to_s
      if code.empty?
        warn_once('Mover: script ohne Code im Route-Feld code')
      elsif defined?(Scripts)
        Scripts.run_code('move_route', code)
      end

    when 'move_animation_on'
      # Sichtbares Feedback: einmalig Partikel am Ziel (Laufstaub).
      if defined?(Effects)
        x, z = ziel_position(r)
        Effects.dust(x, Game.gy(x, z) + 0.1, z)
      end

    when 'move_animation_off',
         'stop_animation_on', 'stop_animation_off',
         'direction_fix_on', 'direction_fix_off',
         'through_on', 'through_off',
         'transparent_on', 'transparent_off',
         'always_on_top_on', 'always_on_top_off',
         'change_graphic', 'change_opacity', 'change_blending'
      warn_once("Mover: Schritt '#{befehl}' hat in Rune3D keine Wirkung (übersprungen)")

    else
      warn_once("Mover: unbekannter Schritt '#{befehl}' (übersprungen)")
    end
  end

  # Einheitsvektor für einen Bewegungsschritt; nil = unmöglich.
  def self.richtung(befehl, r)
    d = Math.sqrt(0.5)   # Diagonalen normiert
    case befehl
    when 'move_down'  then [0.0, 1.0]
    when 'move_up'    then [0.0, -1.0]
    when 'move_left'  then [-1.0, 0.0]
    when 'move_right' then [1.0, 0.0]
    when 'move_lower_left'  then [-d, d]
    when 'move_lower_right' then [d, d]
    when 'move_upper_left'  then [-d, -d]
    when 'move_upper_right' then [d, -d]
    when 'move_random'
      s = RICHTUNGEN.sample
      vektor_fuer(s)
    when 'move_forward'
      vektor_fuer(r.richtung)
    when 'move_backward'
      v = vektor_fuer(r.richtung)
      v && [-v[0], -v[1]]
    when 'move_toward_player', 'move_away_from_player'
      px, _py, pz = Rune::Player.position
      ox, oz = r.spieler ? [px, pz] : r.objekt.position
      dx = px - ox
      dz = pz - oz
      len = Math.sqrt(dx * dx + dz * dz)
      return nil if len < 0.001   # steht schon auf dem Spieler
      dx /= len
      dz /= len
      befehl == 'move_away_from_player' ? [-dx, -dz] : [dx, dz]
    end
  end

  def self.vektor_fuer(richtung)
    case richtung.to_i
    when 2 then [0.0, 1.0]
    when 4 then [-1.0, 0.0]
    when 6 then [1.0, 0.0]
    when 8 then [0.0, -1.0]
    end
  end

  # Nächste XP-Richtung (2/4/6/8) für einen Bewegungsvektor; Diagonalen
  # behalten die alte Blickrichtung (nil).
  def self.vektor_richtung(dx, dz)
    return 2 if dz > 0 && dx.zero?
    return 8 if dz < 0 && dx.zero?
    return 4 if dx < 0 && dz.zero?
    return 6 if dx > 0 && dz.zero?

    nil
  end

  # XP-Richtung zum/​vom Spieler (größerer Achsenanteil gewinnt, wie
  # move_type_toward_player in Game_Character 2).
  def self.blick_zum_spieler(r, weg: false)
    px, _py, pz = Rune::Player.position
    ox, oz = r.spieler ? [px, pz] : r.objekt.position
    dx = px - ox
    dz = pz - oz
    return nil if dx.abs < 0.001 && dz.abs < 0.001

    richtung = if dx.abs > dz.abs
                 dx > 0 ? 6 : 4
               else
                 dz > 0 ? 2 : 8
               end
    weg ? { 2 => 8, 8 => 2, 4 => 6, 6 => 4 }[richtung] : richtung
  end

  # Blickrichtung sichtbar machen: Spieler = Kamera-Yaw, NPC = Node-Yaw.
  def self.drehen(r)
    yaw = YAW[r.richtung] || 0.0
    if r.spieler
      Rune::Camera.yaw = yaw if defined?(Rune::Camera)
    elsif r.objekt.respond_to?(:turn_to)
      r.objekt.turn_to(yaw)
    elsif r.objekt.respond_to?(:node) && r.objekt.node
      rot = r.objekt.node.rotation
      rot[1] = yaw
      r.objekt.node.rotation = rot
    end
  end

  # Position des Routenziels (Effekte/Sound).
  def self.ziel_position(r)
    r.spieler ? [Rune::Player.position[0], Rune::Player.position[2]] : r.objekt.position
  end

  # Zielpunkt eines Teilschritts in XZ (für Kollisionsprüfung und Bewegung).
  def self.teil_ziel(r, weg)
    if r.spieler
      px, _py, pz = Rune::Player.position
      [px + r.dx * weg, pz + r.dz * weg]
    else
      x, z = r.objekt.position
      [x + r.dx * weg, z + r.dz * weg]
    end
  end

  # Restmeter des aktuellen Schritts ablaufen (Spieler: gleiten per
  # teleport wie move_player; NPC: move_to setzt Event + Node).
  def self.bewege(r, dt)
    weg = r.speed * dt
    weg = r.distanz if weg > r.distanz

    # Etappe 19k.3: Block-Zonen und Hindernisse stoppen auch Routen. Ohne
    # die Prüfung liefen move_*-Schritte durch die unsichtbaren Wände (der
    # Spieler-teleport umgeht die Physik, NPC-move_to sowieso). Der Schritt
    # gilt dann als abgelaufen, die Route macht mit dem nächsten Befehl
    # weiter - genau wie ein Charakter, der gegen eine Wand läuft.
    nx, nz = teil_ziel(r, weg)
    # Etappe 48: XP "ignore impossible moves" (through) lässt die Route
    # auch durch Block-Zonen laufen; sonst stoppt der Schritt wie bisher.
    if !r.ignore && Passage.blockiert?(nx, nz)
      r.distanz = 0.0
      warn_once('Mover: Schritt an Block-Zone/Hindernis gestoppt')
      return
    end

    r.distanz -= weg

    if r.spieler
      Rune::Player.teleport(nx, Game.gy(nx, nz) + 0.2, nz)
    else
      r.objekt.move_to(nx, nz)
    end
  end

  def self.warn_once(text)
    return if @gewarnt[text]

    @gewarnt[text] = true
    Rune::Log.warn(text)
  end
end
