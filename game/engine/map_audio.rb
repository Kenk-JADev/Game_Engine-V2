#encoding: utf-8
# ============================================================================
#  Karten-Musik (Etappe 21, T3). Die Karten-JSON darf seitdem nennen:
#
#    "bgm": { "name": "bgm/dorf.wav", "volume": 80, "pitch": 100 }
#    "bgs": { "name": "bgs/regen.wav", "volume": 60 }
#
#  Beim Kartenwechsel startet die Musik der neuen Karte - das entspricht
#  Game_Map#autoplay in XP. Bewusste Abweichungen:
#
#    - Fehlt der Eintrag (oder ist der Name leer), laeuft die bisherige Musik
#      weiter. XP startet in diesem Fall ebenfalls nichts, stoppt aber auch
#      nichts; ohne Eintrag bleibt es also bei "weiterlaufen".
#    - Dieselbe Datei mit gleicher Lautstaerke und gleichem Pitch wird NICHT
#      neu gestartet. XP beginnt bei jedem Kartentransfer von vorn; hier
#      laeuft die Musik ohne Unterbrechung weiter. Wer den Neustart will,
#      nutzt den Event-Befehl `play_bgm` (XP 241).
#    - `bgs` wird wie ein Klangteppich behandelt: genannt = neu starten,
#      nicht genannt = weiterlaufen (XP startet nur, was die Karte nennt).
#
#  Ohne Datei greift `Audio.*` auf die prozeduralen Klaenge zurueck; der
#  Karten-Eintrag ist damit auch ohne WAV-Datei wirksam (und MapCheck warnt
#  bei MP3).
# ============================================================================

module MapAudio
  @bgm = nil   # [name, lautstaerke, pitch] der laufenden Kartenmusik
  @bgs = nil

  class << self
    # Karte betreten: BGM/BGS der Karte starten (nur was genannt ist).
    # Rueckgabe [bgm_gestartet, bgs_gestartet].
    def enter(karte)
      [starte(eintrag(karte, 'bgm'), :bgm), starte(eintrag(karte, 'bgs'), :bgs)]
    end

    # Was laeuft laut Kartenmusik? (Tests/Skripte) -> [name, volume, pitch].
    def bgm = @bgm && @bgm.dup
    def bgs = @bgs && @bgs.dup

    # Vergessen, ohne zu stoppen (z. B. wenn ein Spielstand die Musik setzt).
    def reset
      @bgm = nil
      @bgs = nil
    end

    # Etappe 22: nur eine Spur vergessen - ein ME verstummt die BGM, der
    # Klangteppich (BGS) laeuft weiter.
    def vergiss(spur)
      spur == :bgs ? (@bgs = nil) : (@bgm = nil)
    end

    private

    # Eintrag einer Karte lesen: nil, wenn nichts Sinnvolles dasteht.
    def eintrag(karte, feld)
      return nil unless karte.is_a?(Hash)

      wert = karte[feld]
      return nil unless wert.is_a?(Hash)

      name = wert['name'].to_s.strip
      return nil if name.empty?

      laut = wert['volume'].nil? ? 100.0 : wert['volume'].to_f
      pitch = wert['pitch'].nil? ? 100.0 : wert['pitch'].to_f
      [name, laut.clamp(0.0, 100.0), pitch.clamp(50.0, 150.0).round]
    end

    # Startet einen Eintrag, wenn er sich von dem laufenden unterscheidet.
    def starte(datei, spur)
      return false if datei.nil?

      laeuft = spur == :bgm ? @bgm : @bgs
      return false if laeuft && laeuft == datei

      name, laut, pitch = datei
      if defined?(Audio) && Audio.respond_to?(spur == :bgm ? :bgm_play : :bgs_play)
        if spur == :bgm
          Audio.bgm_play(name, laut, pitch)
        else
          Audio.bgs_play(name, laut, pitch)
        end
      elsif defined?(Rune::Sound) && Rune::Sound.respond_to?(:loop)
        # Ohne RGSS-Schicht bleibt der prozedurale Klang (0..1 wie Sound.loop).
        Rune::Sound.loop(name, laut / 100.0)
      else
        Rune::Log.warn('MapAudio: keine Ton-Schicht, Kartenmusik ignoriert') if defined?(Rune::Log)
        return false
      end

      if spur == :bgm
        @bgm = datei
      else
        @bgs = datei
      end
      Rune::Log.info("Kartenmusik #{spur}: #{name} (#{laut.round} %, Pitch #{pitch} %)") if defined?(Rune::Log)
      true
    end
  end
end
