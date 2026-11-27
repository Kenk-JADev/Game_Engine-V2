#encoding: utf-8
# ============================================================================
#  Rpg::Repositories - schreibgeschützte Zugriffe auf die Vorlagen.
#
#  Ein Repository entspricht einer XP-Tabelle ($data_actors, $data_skills, ...),
#  gibt aber keine beschreibbaren Arrays heraus. `resize` erzeugt eine neue
#  Instanz – vergleichbar mit "Change Maximum", nur ohne Mutation.
# ============================================================================

module Rpg
  module Repositories
    class Repository
      attr_reader :area

      def initialize(area, entries)
        @area = area.to_sym
        @entries = Support.deep_freeze(Array(entries).dup)
      end

      # Die kompakte, eingefrorene Liste (leere Resize-Slots sind entfernt).
      def all
        @entries.compact.freeze
      end

      def find(id)
        ziel = Support.integer(id)
        return nil if ziel.nil?

        @entries.find { |eintrag| eintrag && eintrag.id == ziel }
      end

      def ids
        all.map(&:id).freeze
      end

      def size = @entries.size
      def empty? = all.empty?

      # XP erlaubt 1..999 Einträge pro Tabelle. Neue Slots sind `nil` und
      # werden von `all`/`find` übersprungen, genau wie leere XP-Slots.
      def resize(neu)
        anzahl = Support.integer(neu)
        unless anzahl && anzahl.between?(1, 999)
          raise ArgumentError, "Listengröße für #{@area} muss zwischen 1 und 999 liegen"
        end

        eintraege = @entries.dup
        if anzahl > eintraege.size
          eintraege.concat(Array.new(anzahl - eintraege.size))
        else
          eintraege = eintraege.first(anzahl)
        end
        self.class.new(@area, eintraege)
      end

      def to_h = { area: @area, size: size, ids: ids }
    end

    # Bündel aller Repositories. Wird vom Loader erzeugt und nie global
    # abgelegt; Aufrufer reichen die Instanz explizit weiter.
    class Database
      AREAS = %i[
        actors classes skills items weapons armors enemies troops states
        animations tilesets common_events elements species switches variables
      ].freeze

      attr_reader :version, :system

      def initialize(version:, repositories:, system:)
        @version = version.to_i
        @repositories = Support.deep_freeze(repositories.dup)
        @system = system
      end

      def [](bereich)
        @repositories[bereich.to_sym]
      end

      AREAS.each do |bereich|
        define_method(bereich) { @repositories[bereich] }
      end

      def repo_names = @repositories.keys.freeze

      # Liefert eine NEUE Datenbank; die übergebene Instanz bleibt unberührt.
      def resize(bereich, anzahl)
        schluessel = bereich.to_sym
        repo = @repositories[schluessel]
        raise ArgumentError, "unbekannter Bereich: #{bereich}" unless repo

        neu = @repositories.merge(schluessel => repo.resize(anzahl))
        self.class.new(version: @version, repositories: neu, system: @system)
      end

      def to_h
        @repositories.to_h { |name, repo| [name, repo.to_h] }.freeze
      end
    end
  end
end
