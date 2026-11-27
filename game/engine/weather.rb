#encoding: utf-8
# ============================================================================
#  Wetter (Etappe 17h): Regen/Schnee als Partikel um den Spieler.
#
#  Quelle ist das Karten-Feld   "weather": {"type":"regen","intensity":1.0}
#  ("kein" oder fehlend = kein Wetter). Der Interpreter-Befehl
#  {"cmd":"weather","name":"schnee","intensity":1.5} stellt es zur Laufzeit
#  um; Scripts können weather('regen', 2.0) rufen.
#
#  Wie Effects sind die Partikel kleine Boxen; Regen = schnelle lange
#  Strahlen, Schnee = langsame schwebende Flocken. Kopflos testbar:
#  ruby -I game/engine tools/weather_test.rb
# ============================================================================

module Weather
  TYPES = %w[kein regen schnee].freeze
  MAX_INTENSITY = 10.0 # höchstens 400 Partikel, passend zu terrain_limits.h

  @type = 'kein'
  @intensity = 1.0
  @drops = []          # aktive Partikel { x,y,z,vy,size,sway,phase }
  @rng = 0
  @duration = 0.0      # Etappe 48: XP 236 Dauer (0 = unbegrenzt)

  module_function

  def type = @type
  def intensity = @intensity
  def duration = @duration
  def active? = @type != 'kein' && @intensity.positive?

  # Einstellen (auch aus dem Interpreter/Scripts). Unbekannt = aus.
  # duration > 0 beendet das Wetter nach Sekunden (Etappe 48).
  def set(name, intensity = 1.0, duration = 0.0)
    @duration = duration.to_f.positive? ? duration.to_f : 0.0
    key = name.to_s
    unless TYPES.include?(key) || key == 'kein'
      Issues.report('Wetter', "unbekannter Wetter-Typ '#{key}' (nehme 'kein')")
    end
    key = 'kein' unless TYPES.include?(key)
    if key == 'kein'
      @type = 'kein'
      @drops.clear
      Rune::Log.info('Wetter: aus')
    else
      @type = key
      n = (intensity.is_a?(Integer) || intensity.is_a?(Float)) ? intensity.to_f : Float::NAN
      unless n.finite? && n.positive? && n <= MAX_INTENSITY
        Rune::Log.warn('Wetter-Intensität ungültig; 1.0 als Ersatz')
        n = 1.0
      end
      @intensity = n
      @drops.clear
      Rune::Log.info("Wetter: #{key} (Intensität #{@intensity})")
    end
    self
  end

  # Aus dem Karten-Hash übernehmen (Kartenstart/F5).
  def load_map(map)
    w = (map || {})['weather']
    unless w.nil? || w.is_a?(Hash)
      Rune::Log.warn('weather ist kein Objekt; Wetter aus')
      w = nil
    end
    w ||= {}
    set(w['type'] || 'kein', w['intensity'].nil? ? 1.0 : w['intensity'])
  end

  # Partikel um den Spieler herum neu verteilen (Spawn im Umkreis).
  def spawn(cx, cz, ground_y)
    r = 14.0 + rand * 14.0
    a = rand * Math::PI * 2.0
    { x: cx + Math.sin(a) * r,
      y: ground_y + 10.0 + rand * 12.0,
      z: cz + Math.cos(a) * r }
  end

  def update(dt, px, py, pz)
    # Etappe 48: zeitlich begrenztes Wetter herunterzählen.
    if @duration.positive?
      @duration = [@duration - dt, 0.0].max
      if @duration.zero?
        set('kein', 0)
        return
      end
    end
    return unless active?

    # Zielmenge: ~ 40 Partikel pro Intensitätsstufe.
    want = (40 * @intensity).round
    gy = py - 2.0   # grober Boden (Partikel verschwinden unter dem Spieler)

    # Nachfüllen.
    (@drops.size...want).each { @drops << spawn(px, pz, py) }

    fall = (@type == 'regen') ? 22.0 : 2.6
    sway = (@type == 'regen') ? 0.0 : 0.9

    @drops.each do |p|
      p[:y] -= fall * dt
      p[:phase] = (p[:phase] || rand * 6.28) + dt * 1.7
      p[:x] += Math.sin(p[:phase]) * sway * dt if sway.positive?
      p[:z] += Math.cos(p[:phase] * 0.7) * sway * dt if sway.positive?
    end

    # Unter dem Boden/zu weit weg: neu spawnen. Die Schwelle liegt etwas
    # über dem Spawn-Radius (14-28 m), damit nichts flackert.
    @drops.map! do |p|
      if p[:y] < gy || (p[:x] - px)**2 + (p[:z] - pz)**2 > 1024.0
        spawn(px, pz, py)
      else
        p
      end
    end
  end

  def draw
    return unless active?
    @drops.each do |p|
      if @type == 'regen'
        # Strahl: dünn, lang, blaugrau.
        Rune::Gfx.box(p[:x], p[:y], p[:z], 0.02, 0.45, 0.02, 0x9FB8D0)
      else
        # Flocke: klein, weiss, langsam.
        Rune::Gfx.box(p[:x], p[:y], p[:z], 0.09, 0.09, 0.09, 0xEFF4FA)
      end
    end
  end

  def count = @drops.size

  def clear
    @drops.clear
  end
end
