#encoding: utf-8
# ==============================================================================
# Rpg::EventCommands::Typed — ein typisiertes Command-Objekt je Befehlstyp.
#
# Der Loader liefert bereits geprüfte Models::Command-Datensätze mit einem
# Parameter-Hash. Diese Schicht legt darüber 90 eigene Klassen (eine je
# commandType) mit benannten, typisierten Feldern:
#
#   text = katalog.typed(befehl)          # => Typed::Commands::ShowText
#   text.commandType                      # => "show_text"
#   text.code                             # => 101
#   text.indent                           # => 0
#   text.lines                            # => ["Der Wächter hebt den Blick."]
#   text.speaker                          # => "Kael"
#   text.to_h                             # => verlustfreier Rohdatensatz
#
# Die Klassen entstehen aus dem Feldschema des Katalogs (eine Quelle der
# Wahrheit), sind unveränderlich und erzwingen ihre Typen beim Cast. Kein
# Array-Index-Zugriff, keine stillen Typwechsel, keine UI-Abhängigkeit.
# ==============================================================================

module Rpg
  module EventCommands
    module Typed
      # Gemeinsames Verhalten aller Parameter-Strukturen.
      module ParamsApi
        def to_h
          self.class.field_names.each_with_object({}) do |name, ergebnis|
            wert = self[Typed.param_name(name)]
            ergebnis[name] = wert unless wert.nil?
          end
        end

        def field_names = self.class.field_names
        def [](schluessel) = public_send(Typed.param_name(schluessel))
        def command_type = self.class.command_type
      end

      # Gemeinsames Verhalten aller typisierten Command-Objekte.
      module CommandApi
        def commandType = definition.type
        def type = definition.type
        def command_type = definition.type
        def code = definition.code
        def tab = definition.tab
        def column = definition.column
        def label = definition.label

        def block(name) = children.fetch(name.to_s, [])
        def leaf? = children.empty?

        # Verlustfrei zurück in die Rohform des Loaders (commandType/code/
        # indent/params/blocks). Nur Blöcke, die wirklich existieren.
        def to_h
          roh = { 'commandType' => commandType, 'code' => code, 'indent' => indent,
                  'params' => params.to_h }
          roh['blocks'] = children.to_h { |name, liste| [name, liste.map(&:to_h)] } unless children.empty?
          roh
        end
      end

      module Params
      end

      module Commands
      end

      class UnknownCommand < StandardError; end

      # Ein Feldname darf nicht mit Rubys Konstruktor oder einer Data-Methode
      # kollidieren. Der JSON-Name bleibt unverändert, nur der Ruby-Leser
      # bekommt einen sicheren Namen (initialize -> initialize_actor).
      PARAM_ALIASES = { 'initialize' => 'initialize_actor' }.freeze
      PARAM_RESERVED = %w[initialize class method send hash freeze frozen? to_h to_s
                          inspect members with deconstruct deconstruct_keys].freeze

      # Auf dem Command-Objekt stehen bereits commandType/code/indent/params/
      # children/definition. Gleichnamige Felder lesen sich über param_<name>.
      COMMAND_RESERVED = %w[commandType type command_type code tab column label block
                            leaf? to_h indent params children definition].freeze

      module_function

      # Wandelt ein Models::Command in sein typisiertes Gegenstück. Unbekannte
      # Typen sind ein Programmfehler und werden nicht stillschweigend
      # durchgereicht.
      def cast(command, catalog)
        eintrag = eintrag_fuer(catalog, command.commandType)
        raise UnknownCommand, "Unbekannter Befehlstyp '#{command.commandType}'" unless eintrag

        definition = eintrag[:definition]
        parameter_klasse = (eintrag[:params] ||= params_class(definition))
        klasse = (eintrag[:command] ||= command_class(definition))
        klasse.new(
          indent: command.indent,
          params: parameter_klasse.from_params(command.params),
          children: command.children.to_h { |name, liste| [name, liste.map { |kind| cast(kind, catalog) }.freeze] },
          definition: definition
        )
      end

      # Ein Index je Katalog: Der Cast eines Befehls ist damit ein Hash-Zugriff
      # statt einer Suche über 90 Definitionen — wichtig, weil Events jeden
      # Frame laufen. Die Klassen entstehen einmal und werden wiederverwendet.
      def eintrag_fuer(catalog, type)
        katalog_index(catalog)[type.to_s]
      end

      def katalog_index(catalog)
        @indexes ||= []
        treffer = @indexes.find { |paar| paar[0].equal?(catalog) }
        unless treffer
          treffer = [catalog, catalog.definitions.to_h { |d| [d.type, { definition: d }] }]
          @indexes.shift if @indexes.size >= 8
          @indexes << treffer
        end
        treffer[1]
      end

      def cast_program(program, catalog)
        raise ArgumentError, 'Programm fehlt' unless program

        program.commands.map { |command| cast(command, catalog) }.freeze
      end

      def params_class(definition)
        cache[:params][definition] ||= baue_params(definition)
      end

      def command_class(definition)
        cache[:commands][definition] ||= baue_command(definition)
      end

      def cache
        @cache ||= { params: {}, commands: {} }
      end

      # Nur für Tests: Klassen bleiben wertgleich zum Schema, ein Reset ist
      # trotzdem nützlich, wenn ein Katalog im Test bewusst verändert wird.
      def reset_cache!
        @cache = { params: {}, commands: {} }
        @indexes = []
      end

      # Konstanten bleiben mit dem Schema synchron, ohne Rubys Warnung beim
      # Überschreiben einer bereits definierten Konstante.
      def konstante_setzen(modul, name, klasse)
        modul.send(:remove_const, name) if modul.const_defined?(name, false)
        modul.const_set(name, klasse)
      end

      def param_name(field_name)
        name = field_name.to_s
        return PARAM_ALIASES.fetch(name, "#{name}_value") if PARAM_RESERVED.include?(name)

        name
      end

      def command_reader(field_name)
        name = field_name.to_s
        return PARAM_ALIASES[name] if PARAM_ALIASES.key?(name)
        return "param_#{name}" if COMMAND_RESERVED.include?(name) || PARAM_RESERVED.include?(name)

        name
      end

      def klassenname(type)
        type.to_s.split('_').map { |teil| teil.capitalize }.join
      end

      def coerced(wert, field)
        return nil if wert.nil?

        case field.kind
        when :integer, :actor_ref, :event_ref, :enemy_slot
          zahl = Rpg::Support.integer(wert)
          raise ArgumentError, "#{field.name}: ganze Zahl erwartet, #{wert.inspect} erhalten" unless zahl

          zahl
        when :number
          zahl = Rpg::Support.number(wert)
          raise ArgumentError, "#{field.name}: Zahl erwartet, #{wert.inspect} erhalten" unless zahl

          zahl
        when :bool
          unless [true, false].include?(wert)
            raise ArgumentError, "#{field.name}: true/false erwartet, #{wert.inspect} erhalten"
          end

          wert
        when :string, :enum, :key
          wert.to_s
        when :ref
          Rpg::Support.integer(wert) || wert.to_s
        when :string_list
          unless wert.is_a?(Array) && wert.all? { |eintrag| eintrag.is_a?(String) }
            raise ArgumentError, "#{field.name}: Stringliste erwartet"
          end

          wert.map(&:freeze).freeze
        when :color, :move_steps, :goods, :equipment_ref
          Rpg::Support.deep_freeze(Rpg::Support.stringify_keys(wert))
        else
          raise ArgumentError, "#{field.name}: unbekannter Feldtyp #{field.kind}"
        end
      end

      def baue_params(definition)
        felder = definition.fields.values
        namen = felder.map { |field| Typed.param_name(field.name).to_sym }
        klasse = Data.define(*namen)
        klasse.include(ParamsApi)
        klasse.define_singleton_method(:field_names) { felder.map(&:name).freeze }
        klasse.define_singleton_method(:fields) { felder.freeze }
        klasse.define_singleton_method(:command_type) { definition.type }
        klasse.define_singleton_method(:definition) { definition }
        klasse.define_singleton_method(:from_params) do |roh|
          roh = Rpg::Support.stringify_keys(roh || {})
          werte = felder.to_h do |field|
            wert = roh.key?(field.name) ? roh[field.name] : field.default
            if wert.nil? && field.required
              raise ArgumentError, "#{definition.type}.#{field.name}: Pflichtfeld fehlt"
            end

            [Typed.param_name(field.name).to_sym, Typed.coerced(wert, field)]
          end
          new(**werte)
        end
        Typed.konstante_setzen(Params, Typed.klassenname(definition.type), klasse)
        klasse
      end

      def baue_command(definition)
        klasse = Data.define(:indent, :params, :children, :definition)
        klasse.include(CommandApi)
        klasse.prepend(Rpg::Support::Immutable)
        klasse.define_singleton_method(:command_type) { definition.type }
        klasse.define_singleton_method(:params_class) { Typed.params_class(definition) }
        definition.fields.each_key do |name|
          klasse.define_method(Typed.command_reader(name)) { params[name] }
        end
        Typed.konstante_setzen(Commands, Typed.klassenname(definition.type), klasse)
        klasse
      end
    end
  end
end
