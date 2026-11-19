#encoding: utf-8
# ============================================================================
#  Prozedurale Sounds (wie die Texturen komplett im Code erzeugt): 16 Bit
#  mono bei 44100 Hz, als Byte-String an die Engine gemeldet.
#
#  Rune::Sound.register(name, rate, bytes) - play(name, volume) startet sie.
#  Kopflos testbar: ruby -I game/engine tools/sound_test.rb
# ============================================================================

module Sounds
  RATE = 44_100
  MUSIK_RATE = 11_025   # Musik wird in Viertel-Aufloesung erzeugt und von
                        # der Engine linear auf 44100 Hz hochgesampelt
  module_function

  # Deterministischer Rausch-Generator (gleicher LCG wie in textures.rb).
  def lcg(seed)
    s = seed
    -> { s = (s * 1103515245 + 12345) & 0x7FFFFFFF; s / 2147483647.0 - 0.5 }
  end

  # Sinus mit Huellkurve (attack/decay in Sekunden).
  def ton(freq, seconds, vol: 0.8, attack: 0.005, decay: nil, slide: 0.0)
    n = (seconds * RATE).to_i
    decay ||= seconds
    out = [0.0] * n
    phase = 0.0
    n.times do |i|
      t = i.to_f / RATE
      f = freq + slide * (t / seconds)
      phase += 2 * Math::PI * f / RATE
      env = t < attack ? t / attack : [1.0 - (t - attack) / decay, 0.0].max
      out[i] = Math.sin(phase) * vol * env
    end
    out
  end

  # Rauschen mit Tiefpass (gleitender Mittelwert) und Abkling-Huellkurve.
  def rauschen(seconds, vol: 0.5, decay: nil, glatt: 6, seed: 1234)
    n = (seconds * RATE).to_i
    decay ||= seconds
    rnd = lcg(seed)
    out = Array.new(n) { rnd.call }
    # Einfacher Tiefpass: ueber glatt Nachbarn mitteln.
    geg = Array.new(n, 0.0)
    sum = 0.0
    n.times do |i|
      sum += out[i]
      sum -= out[i - glatt] if i >= glatt
      geg[i] = sum / [i + 1, glatt].min
    end
    n.times do |i|
      t = i.to_f / RATE
      env = [1.0 - t / decay, 0.0].max
      geg[i] = geg[i] * env * 8.0 * vol
    end
    geg
  end

  def mix(*parts)
    n = parts.map { |p| p.size }.max
    out = Array.new(n, 0.0)
    parts.each do |p|
      p.each_with_index { |v, i| out[i] += v }
    end
    out
  end

  def versatz(part, seconds)
    [0.0] * (seconds * RATE).to_i + part
  end

  def to_bytes(samples)
    samples.map do |v|
      v = 32767.0 if v > 32767.0
      v = -32768.0 if v < -32768.0
      (v * 32767).round
    end.pack('v*')
  end

  # Hintergrundmusik: 8-s-Loop, vier Akkord-Pads (Am - F - C - G), je 2 s
  # mit weichem Anstieg/Abfall. Leicht verstimte zweite Stimme fuer Waerme.
  # Bei MUSIK_RATE erzeugt, damit der Start schnell bleibt.
  def musik
    rate = MUSIK_RATE
    n = (8.0 * rate).to_i
    akkorde = [
      [220.00, 261.63, 329.63],   # Am
      [174.61, 220.00, 261.63],   # F
      [261.63, 329.63, 392.00],   # C
      [196.00, 246.94, 293.66],   # G
    ]
    out = Array.new(n, 0.0)
    phasen = akkorde.map { |a| Array.new(a.size, 0.0) }
    phasen2 = akkorde.map { |a| Array.new(a.size, 0.0) }
    n.times do |i|
      t = i.to_f / rate
      k = (t / 2.0).to_i % 4          # aktueller Akkord
      tl = t - k * 2.0                # Zeit im Akkord
      env = if tl < 0.5 then tl / 0.5
            elsif tl > 1.5 then (2.0 - tl) / 0.5
            else 1.0 end
      akkorde[k].each_with_index do |f, j|
        phasen[k][j] += 2 * Math::PI * f / rate
        phasen2[k][j] += 2 * Math::PI * f * 1.004 / rate
        out[i] += (Math.sin(phasen[k][j]) + Math.sin(phasen2[k][j])) * env
      end
    end
    peak = out.map { |v| v.abs }.max
    sk = 0.42 / peak
    out.map { |v| v * sk }
  end

  # Alle Sounds anlegen und bei der Engine anmelden.
  def register_all
    # Schritt: kurzes, dumpfes Tritt-Rauschen
    schritt = mix(
      rauschen(0.09, vol: 0.30, decay: 0.07, glatt: 14, seed: 101),
      ton(95, 0.07, vol: 0.35, decay: 0.06),
    )
    Rune::Sound.register('schritt', RATE, to_bytes(schritt))

    # Klick (Menue): kurzer heller Blip
    klick = ton(1250, 0.05, vol: 0.45, decay: 0.04)
    Rune::Sound.register('klick', RATE, to_bytes(klick))

    # Jingle (Event): kleine Dur-Arpeggio C5-E5-G5-C6
    jingle = mix(
      versatz(ton(523.25, 0.16, vol: 0.40, decay: 0.14), 0.00),
      versatz(ton(659.25, 0.16, vol: 0.40, decay: 0.14), 0.11),
      versatz(ton(783.99, 0.16, vol: 0.40, decay: 0.14), 0.22),
      versatz(ton(1046.50, 0.30, vol: 0.42, decay: 0.28), 0.33),
    )
    Rune::Sound.register('jingle', RATE, to_bytes(jingle))

    # Ablehnen: zwei tiefe Buzzer
    ablehnen = mix(
      versatz(ton(196, 0.13, vol: 0.42, decay: 0.11), 0.00),
      versatz(ton(155, 0.20, vol: 0.42, decay: 0.18), 0.16),
    )
    Rune::Sound.register('ablehnen', RATE, to_bytes(ablehnen))

    # Sprung: aufsteigender Sweep
    sprung = ton(300, 0.16, vol: 0.40, decay: 0.15, slide: 320)
    Rune::Sound.register('sprung', RATE, to_bytes(sprung))

    # Landung: dumpfer Thud mit Rauschen
    landung = mix(
      ton(70, 0.13, vol: 0.55, decay: 0.11),
      rauschen(0.08, vol: 0.25, decay: 0.06, glatt: 20, seed: 202),
    )
    Rune::Sound.register('landung', RATE, to_bytes(landung))

    # Musik: Endlos-Loop, wird per Rune::Sound.loop gestartet.
    Rune::Sound.register('musik', MUSIK_RATE, to_bytes(musik))

    Rune::Log.info('7 Sounds registriert (schritt, klick, jingle, ablehnen, sprung, landung, musik)')
  end
end
