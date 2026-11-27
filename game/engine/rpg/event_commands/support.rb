#encoding: utf-8
# ==============================================================================
# Rpg::EventCommands::Support — eigene Fehler- und Testadapter
# ==============================================================================

module Rpg
  module EventCommands
    module Support
      Issue = Data.define(:path, :message) do
        def to_s = "#{path}: #{message}"
      end

      class ValidationError < StandardError
        attr_reader :issues

        def initialize(issues)
          @issues = issues.map(&:to_s).freeze
          super("Event-Commands ungültig (#{@issues.size} Problem(e)):\n- #{@issues.join("\n- ")}")
          freeze
        end
      end

      class NullReporter
        def report(_issue) = nil
      end

      class CollectingReporter
        attr_reader :issues

        def initialize
          @issues = []
        end

        def report(issue)
          @issues << issue
          issue
        end
      end

      class LocalFileSystem
        def read_json(path)
          raise "Datei fehlt: #{path}" unless File.file?(path)

          JSON.parse(File.read(path, encoding: 'UTF-8'))
        end
      end

      class MemoryFileSystem
        def initialize(documents)
          @documents = documents.to_h do |path, document|
            [path.to_s, Rpg::Support.stringify_keys(document)]
          end
        end

        def read_json(path)
          raise "Datei fehlt: #{path}" unless @documents.key?(path.to_s)

          @documents[path.to_s]
        end
      end

      def self.integer(value) = Rpg::Support.integer(value)
      def self.number(value) = Rpg::Support.number(value)
      def self.stringify_keys(value) = Rpg::Support.stringify_keys(value)
      def self.deep_freeze(value) = Rpg::Support.deep_freeze(value)
    end
  end
end
