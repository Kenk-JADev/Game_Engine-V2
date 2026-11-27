#encoding: utf-8
# ============================================================================
#  Rpg::Support - gemeinsame Hilfen für die Datenbankschicht.
#
#  Alles hier ist absichtlich frei von Engine-, UI- und Renderwissen: Der
#  Loader bekommt Strings/Dateien injiziert und liefert entweder Modelle oder
#  gesammelte Fehler. Der Code bleibt deutsch und ohne Engine-Abhängigkeiten.
# ============================================================================

module Rpg
  module Support
    # Ein einzelner Validierungsbefund mit stabilem Pfad (für Editor/Log).
    class Issue
      attr_reader :path, :message

      def initialize(path, message)
        @path = path.to_s
        @message = message.to_s
        freeze
      end

      def to_s = "#{@path}: #{@message}"
    end

    # Wirft der Loader nach dem Sammeln aller Befunde. Eine halb geladene
    # Datenbank darf niemals benutzt werden.
    class ValidationError < StandardError
      attr_reader :issues

      def initialize(issues)
        @issues = issues.map(&:to_s).freeze
        super("RPG-Datenbank ungültig (#{@issues.size} Problem(e)):\n- #{@issues.join("\n- ")}")
      end
    end

    # Meldet Befunde während des Ladens. Die Default-Implementierung verwirft
    # sie; die Exception trägt trotzdem alle Fehler.
    class NullReporter
      def report(_issue) = nil
    end

    # Friert Arrays/Hashes und ihre Inhalte rekursiv ein. Data-Instanzen sind
    # von sich aus frozen; diese Funktion schützt zusätzlich verschachtelte
    # Konten vor nachträglichen Änderungen durch den Aufrufer.
    def self.deep_freeze(value)
      case value
      when Hash
        value.each do |key, item|
          deep_freeze(key)
          deep_freeze(item)
        end
        value.freeze
      when Array
        value.each { |item| deep_freeze(item) }
        value.freeze
      else
        value.freeze if value.respond_to?(:freeze)
      end
      value
    end

    # Friert Keyword-Argumente vor dem Data.define-Initialize ein. Wird von
    # allen Modellen über `prepend Immutable` benutzt.
    module Immutable
      def initialize(**attribute)
        super(**Support.deep_freeze(attribute))
      end
    end

    def self.stringify_keys(value)
      case value
      when Hash
        value.to_h { |key, item| [key.to_s, stringify_keys(item)] }
      when Array
        value.map { |item| stringify_keys(item) }
      else
        value
      end
    end

    def self.integer(value)
      return value if value.is_a?(Integer)
      return value.to_i if value.is_a?(Float) && value.finite? && value == value.to_i
      return value.to_i if value.is_a?(String) && value.match?(/\A-?\d+\z/)

      nil
    end

    def self.number(value)
      return nil unless value.is_a?(Numeric)
      return nil if value.respond_to?(:finite?) && !value.finite?

      value.to_f
    end
  end
end
