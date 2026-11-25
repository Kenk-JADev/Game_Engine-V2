#encoding: utf-8
# ============================================================================
#  Ton aus Dateien (Etappe 21, T1/T5) — WAV über die Engine, OGG später.
#
#  Bis Etappe 20 waren alle Klänge prozedural: Ruby rechnete PCM und meldete es
#  über Rune::Sound.register an. XP spielt dagegen Dateien aus Audio/BGM und
#  Audio/SE. Dieser Baustein lädt eine Datei über die sichere Assets-Brücke,
#  reicht sie an Rune::Sound.load (WAV-Parser in `src/core/wav.h`) und spielt
#  sie ab:
#
#    AudioFiles.play('audio/se/tuer.wav', volume: 0.8)
#    AudioFiles.loop('audio/bgm/dorf.wav', volume: 0.6, pitch: 100)
#    AudioFiles.stop('audio/bgm/dorf.wav')
#
#  Gesucht wird in dieser Reihenfolge (erste vorhandene Datei gewinnt):
#    <pfad>                     wie angegeben
#    audio/<pfad>               Projektordner „audio“
#    audio/bgm/<pfad>           XP-Ordner für Musik
#    audio/se/<pfad>            XP-Ordner für Geräusche
#  Endungen: .wav wird geladen; .ogg/.mp3 melden eine Warnung (noch kein
#  Decoder) und fallen auf den prozeduralen Klang zurück, wenn es einen gibt.
#
#  Bewusst schlank: kein Streaming, keine Mehrfach-Loops pro Datei — die
#  Engine mischt bis zu 12 Stimmen und ersetzt Loops gleichen Namens.
# ============================================================================

module AudioFiles
  ENDUNGEN = %w[.wav .ogg .mp3].freeze
  UNTERORDNER = ['', 'audio/', 'audio/bgm/', 'audio/se/'].freeze
  MAX_BYTES = 8 * 1024 * 1024      # 8 MB je Datei (60 s WAV sind ~2,6 MB)

  @geladen = {}        # Pfad -> Klangname (nur erfolgreiche Ladungen)
  @zuletzt = {}        # Aufrufname -> aufgelöster Pfad (für stop)
  @gemeldet = {}       # Pfad -> true (Warnung nur einmal)

  class << self
    # Kandidatenpfade für einen XP-Namen ('dorf' oder 'bgm/dorf.wav').
    def kandidaten(name)
      name = name.to_s.strip
      return [] if name.empty?

      basis = name.sub(/\.(wav|ogg|mp3)\z/i, '')
      UNTERORDNER.flat_map do |ordner|
        ENDUNGEN.map { |endung| "#{ordner}#{basis}#{endung}" }
      end.uniq
    end

    # Datei zu einem Namen laden und beim Renderer anmelden.
    # Rückgabe: [pfad, klangname] oder nil.
    def aufloesen(name, pitch: 100)
      name = name.to_s.strip
      if name.empty?
        pfad = @zuletzt[name]
        return pfad ? [pfad, @geladen[pfad]] : nil
      end

      kandidaten(name).each do |pfad|
        return [pfad, @geladen[pfad]] if @geladen.key?(pfad)

        bytes = lese(pfad)
        next if bytes.nil?

        if pfad.downcase.end_with?('.ogg', '.mp3')
          warn(pfad, 'OGG/MP3 kann noch nicht dekodiert werden (WAV verwenden)')
          next
        end

        klang = "datei:#{pfad}"
        unless defined?(Rune::Sound) && Rune::Sound.respond_to?(:load)
          warn(pfad, 'Engine ohne Sound-Lader (kopfloser Lauf?)')
          return nil
        end
        unless Rune::Sound.load(klang, bytes, pitch.to_i.clamp(50, 150))
          warn(pfad, 'Format nicht unterstützt (WAV erwartet)')
          next
        end
        @geladen[pfad] = klang
        @zuletzt[name] = pfad
        return [pfad, klang]
      end
      nil
    end

    # Einmal abspielen (SE/ME). Rückgabe true, wenn eine Datei lief.
    def play(name, volume: 1.0, pitch: 100)
      treffer = aufloesen(name, pitch: pitch)
      return false if treffer.nil?

      Rune::Sound.play(treffer[1], volume.to_f.clamp(0.0, 1.0))
      true
    end

    # Endlosschleife (BGM/BGS). Rückgabe true, wenn eine Datei läuft.
    def loop(name, volume: 1.0, pitch: 100)
      treffer = aufloesen(name, pitch: pitch)
      return false if treffer.nil?

      Rune::Sound.loop(treffer[1], volume.to_f.clamp(0.0, 1.0))
      true
    end

    def stop(name)
      pfad = @zuletzt[name.to_s]
      klang = pfad ? @geladen[pfad] : nil
      return false if klang.nil?

      Rune::Sound.stop(klang) if defined?(Rune::Sound) && Rune::Sound.respond_to?(:stop)
      true
    end

    def geladen?(name)
      name = name.to_s
      return @geladen.key?(name) if @geladen.key?(name)

      pfad = @zuletzt[name]
      pfad ? @geladen.key?(pfad) : false
    end

    def reset
      @geladen.clear
      @zuletzt.clear
      @gemeldet.clear
    end

    private

    def lese(pfad)
      if defined?(Rune::Assets) && Rune::Assets.respond_to?(:read_texture)
        project = defined?($project_root) && $project_root ? $project_name : nil
        status, roh = Rune::Assets.read_texture(Database.root.to_s, project, pfad, nil)
        unless status == :ok && roh.is_a?(String)
          warn(pfad, "Datei nicht lesbar (#{status})")
          return nil
        end
        if roh.bytesize > MAX_BYTES
          warn(pfad, "Datei größer als #{MAX_BYTES / 1024 / 1024} MB")
          return nil
        end
        roh
      elsif defined?(Rune::ROOT) && Rune::ROOT
        voll = File.join(Rune::ROOT, pfad)
        File.exist?(voll) ? File.binread(voll) : nil
      end
    end

    def warn(pfad, text)
      key = [pfad, text]
      return if @gemeldet[key]

      @gemeldet[key] = true
      Rune::Log.warn("Ton '#{pfad}': #{text}") if defined?(Rune::Log)
      Issues.report('Ton', "Ton '#{pfad}': #{text}") if defined?(Issues)
    end
  end
end
