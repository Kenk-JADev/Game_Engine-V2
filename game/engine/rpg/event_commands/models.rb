#encoding: utf-8
# ==============================================================================
# Rpg::EventCommands::Models — unveränderliche Kommando- und Katalogdaten
# ==============================================================================

module Rpg
  module EventCommands
    module Models
      Field = Data.define(:name, :kind, :required, :default, :values, :min,
                          :max, :ref, :key, :allow_empty, :summary) do
        prepend Rpg::Support::Immutable

        def initialize(name:, kind: :string, required: true, default: nil,
                       values: nil, min: nil, max: nil, ref: nil, key: nil,
                       allow_empty: false, summary: '')
          super(
            name: name.to_s,
            kind: kind.to_sym,
            required: required ? true : false,
            default: default,
            values: values&.map { |wert| wert.is_a?(Symbol) ? wert.to_s : wert },
            min: min,
            max: max,
            ref: ref&.to_s,
            key: key&.to_s,
            allow_empty: allow_empty ? true : false,
            summary: summary
          )
        end
      end

      # Eine Befehlsdefinition kennt zusätzlich ihre XP-Spalte. Die Spalte
      # wird aus dem Code abgeleitet, solange sie nicht ausdrücklich gesetzt
      # wird; damit bleibt der Katalog deklarativ und ohne Sonderfälle.
      Definition = Data.define(:type, :commandType, :code, :tab, :column,
                               :group, :handler, :fields, :blocks, :summary) do
        prepend Rpg::Support::Immutable

        def initialize(type:, code:, tab:, group:, handler:, column: nil,
                       fields: {}, blocks: {}, summary: '')
          super(
            type: type.to_s,
            commandType: type.to_s,
            code: Integer(code),
            tab: Integer(tab),
            column: (column || self.class.column_for(Integer(code), Integer(tab))).to_sym,
            group: group.to_s,
            handler: handler.to_s,
            fields: fields.to_h { |name, field| [name.to_s, field] },
            blocks: blocks.map(&:to_s),
            summary: summary
          )
        end

        def self.column_for(code, tab)
          links = case tab
                  when 1 then code.between?(101, 119)
                  when 2 then code.between?(201, 210) || code.between?(221, 225)
                  when 3 then code.between?(301, 303) || code.between?(311, 322)
                  else false
                  end
          links ? :left : :right
        end
      end

      # Kanonisches Feld ist commandType; type bleibt als lesbarer Alias für
      # bereits vorhandene Aufrufer erhalten.
      Command = Data.define(:commandType, :code, :indent, :params, :children) do
        prepend Rpg::Support::Immutable

        def initialize(commandType:, code:, indent:, params: {}, children: {})
          super(
            commandType: commandType.to_s,
            code: Integer(code),
            indent: Integer(indent),
            params: params,
            children: children.to_h { |name, commands| [name.to_s, commands] }
          )
        end

        def type = commandType
        def command_type = commandType
        def block(name) = children.fetch(name.to_s, [])
        def leaf? = children.empty?
      end

      Program = Data.define(:id, :name, :commands) do
        prepend Rpg::Support::Immutable

        def initialize(id:, name:, commands:)
          super(id: Integer(id), name: name.to_s, commands: commands)
        end

        def labels
          commands.each_with_object({}) do |command, treffer|
            treffer[command.params['name']] = command if command.commandType == 'label'
          end.freeze
        end
      end

      CatalogData = Data.define(:version, :definitions, :resources, :programs) do
        prepend Rpg::Support::Immutable

        def definition(type) = definitions.find { |eintrag| eintrag.type == type.to_s }
        def types = definitions.map(&:type).freeze
        def program(id) = programs.find { |eintrag| eintrag.id == id.to_i }

        def definitions_for(tab: nil, column: nil)
          definitions.select do |eintrag|
            (tab.nil? || eintrag.tab == tab.to_i) &&
              (column.nil? || eintrag.column == column.to_sym)
          end.freeze
        end
      end
    end
  end
end
