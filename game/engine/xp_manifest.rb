# encoding: utf-8
# ============================================================================
#  XP-Defaultscript-Manifest (Stufe S1, docs/STUFENPLAN_EDITOR_UND_XP_SCRIPTS.md)
#
#  Nummerierte, geprüfte Liste aller 90 Dateien unter XP-Defaultscript/.
#  Dieses Modul lädt NICHTS in den Engine-Prozess. Es beschreibt nur:
#    - Reihenfolge (Dateinummer = Ladereihenfolge, wie im XP-Scripteditor)
#    - Stufe: :definition  Klassenrumpf lädt in reinem Ruby (ohne RGSS);
#                          über das Laufzeitverhalten ist damit nichts gesagt
#             :needs_rgss  braucht Sprite/Window/RPG::Sprite (Stufen S4/S7)
#             :blocked     nie laden (089 Main: eigene Hauptschleife, S6)
#    - Konflikte mit gleichnamigen Rune3D-Klassen. Solche Dateien dürfen erst
#      nach einem Adapter in den Engine-Prozess (sonst Überschreiben oder
#      "superclass mismatch").
#  Der Test tools/xp_manifest_test.rb weist die Stufen in einem eigenen
#  Ruby-Prozess nach.
# ============================================================================
module Rune
  module XPManifest
    DIR_NAME = 'XP-Defaultscript'

    # Rune3D-Klassen, die gleichnamig in den XP-Scripts vorkommen.
    ENGINE_CLASSES = %w[Interpreter Window_Selectable Scene_Title Scene_File Scene_Gameover].freeze

    # Ab hier brauchen die Dateien RGSS-Grafikklassen (per Probe ermittelt).
    NEEDS_RGSS = ((23..26).to_a + (29..62).to_a).freeze
    BLOCKED = { 89 => 'XP-Hauptschleife (begin/loop/$scene.main) blockiert den Engine-Takt' }.freeze

    # Stufe S2: Diese XP-Dateien werden NICHT geladen, sondern durch die
    # live an GameState gekoppelten Wrapper aus rgss.rb ersetzt. XPs reine
    # Speicher-Arrays würden die Kopplung an Engine-Schalter/-Variablen
    # zerstören. Der Vertragstest prüft, welche XP-API die Wrapper bieten.
    REPLACED_BY = {
      0 => 'RGSS::GameTemp',
      1 => 'RGSS::GameSystem',
      2 => 'RGSS::GameSwitches',
      3 => 'RGSS::GameVariables',
      4 => 'RGSS::GameSelfSwitches'
    }.freeze

    Entry = Struct.new(:number, :name, :file, :stage, :reason, :conflicts, :replaced_by,
                       keyword_init: true)

    module_function

    def dir(root = Rune::ROOT)
      File.join(root, DIR_NAME)
    end

    def number_of(file)
      m = File.basename(file).match(/\A(\d{3}) - (.+)\.rb\z/)
      m && [m[1].to_i, m[2]]
    end

    # Klassen, die eine Datei auf oberster Ebene (neu) definiert.
    def defined_classes(path)
      File.read(path, encoding: 'UTF-8').scan(/^class\s+([A-Z]\w*)/).flatten.uniq
    end

    def entries(root = Rune::ROOT)
      Dir[File.join(dir(root), '*.rb')].filter_map do |path|
        num, name = number_of(path)
        next unless num
        stage = if BLOCKED.key?(num) then :blocked
                elsif NEEDS_RGSS.include?(num) then :needs_rgss
                else :definition
                end
        reason = BLOCKED[num] || (stage == :needs_rgss ? 'braucht Sprite/Window/RPG::Sprite' : 'Klassenrumpf ohne RGSS ladbar')
        Entry.new(number: num, name: name, file: path, stage: stage, reason: reason,
                  conflicts: defined_classes(path) & ENGINE_CLASSES,
                  replaced_by: REPLACED_BY[num])
      end.sort_by(&:number)
    end

    # Öffentliche XP-API einer Datei, statisch gelesen (ohne Laden):
    # def-Methoden außerhalb von private/protected-Abschnitten und
    # attr_reader/attr_writer/attr_accessor. initialize zählt nicht.
    def public_api(path)
      names = []
      hidden = false
      File.read(path, encoding: 'UTF-8').each_line do |line|
        code = line.sub(/#.*/, '')
        hidden = false if code =~ /^class\s/
        hidden = true if code =~ /^\s*(private|protected)\s*$/
        next if hidden
        if (m = code.match(/^\s*def\s+(?!self\.)([\w\[\]]+[?!=]?)/))
          names << m[1] unless m[1] == 'initialize'
        elsif (m = code.match(/^\s*attr_(reader|writer|accessor)\s+(.+)/))
          m[2].scan(/:(\w+)/).flatten.each do |n|
            names << n if m[1] != 'writer'
            names << "#{n}=" if m[1] != 'reader'
          end
        end
      end
      names.uniq
    end

    # Vertragsvergleich für ersetzte Dateien: XP-API, die der Engine-Wrapper
    # (noch) nicht anbietet. klass ist die geladene Engine-Klasse.
    def missing_api(entry, klass)
      have = klass.public_instance_methods.map(&:to_s)
      public_api(entry.file) - have
    end

    # Dateien, die ein isolierter Lader (kein Engine-Prozess) in dieser Stufe
    # laden darf - in Nummernreihenfolge.
    def loadable(root = Rune::ROOT)
      entries(root).select { |e| e.stage == :definition }
    end

    # Lädt die Klassenrümpfe in den AKTUELLEN Prozess. Nur für einen eigenen,
    # wegwerfbaren Ruby-Prozess gedacht (Test/Analyse), nie in Rune3D selbst.
    # Liefert {nummer => nil | "Fehlertext"}.
    def load_isolated!(root = Rune::ROOT)
      # Rune3D definiert Graphics (graphics.rb); reines Ruby nicht.
      raise 'XPManifest.load_isolated! nicht im Engine-Prozess verwenden' if defined?(::Graphics)
      loadable(root).to_h do |e|
        TOPLEVEL_BINDING.eval(File.read(e.file, encoding: 'UTF-8'), e.file)
        [e.number, nil]
      rescue Exception => ex # rubocop:disable Lint/RescueException - Bericht statt Abbruch
        [e.number, "#{ex.class}: #{ex.message}"]
      end
    end
  end
end
