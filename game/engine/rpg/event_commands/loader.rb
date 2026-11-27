#encoding: utf-8
# ==============================================================================
# Rpg::EventCommands::Loader — JSON -> typisierte Kommandos + Validierung
# ==============================================================================

require 'json'

module Rpg
  module EventCommands
    class Loader
      DEFAULT_PATH = if defined?(Rune::ROOT)
                       File.join(Rune::ROOT, 'game', 'config', 'event_commands.json')
                     else
                       File.expand_path('../../../config/event_commands.json', __dir__)
                     end

      RESOURCE_KEYS = %w[
        switches variables self_switches common_events maps events actors classes
        skills items weapons armors enemies troops states animations tilesets
        elements pictures shops scripts windowskins bgms bgss mes ses characters
        battlers faces fogs panoramas battlebacks transitions terrain
      ].freeze

      MOVE_ACTIONS = %w[walk jump wait turn speed frequency repeat skip].freeze
      DIRECTIONS = %w[up down left right].freeze
      COLORS = %w[red green blue gray].freeze

      def initialize(file_system: Support::LocalFileSystem.new,
                     reporter: Support::NullReporter.new)
        @file_system = file_system
        @reporter = reporter
        @katalog = Catalog.build_catalog
        @resources = {}
      end

      def load(path = DEFAULT_PATH)
        roh = lesen(path)
        return nil unless roh.is_a?(Hash)

        roh = Support.stringify_keys(roh)
        fehler = []
        version = versionsfeld(roh, fehler)
        @resources = resourcen(roh, fehler)
        programme = programme(roh, fehler)
        validiere_querverweise(programme, fehler)
        raise Support::ValidationError.new(fehler) if fehler.any?

        Models::CatalogData.new(
          version: version,
          definitions: @katalog.definitions,
          resources: @resources,
          programs: programme
        )
      end

      private

      def lesen(path)
        @file_system.read_json(path)
      rescue StandardError => e
        fehler = [Support::Issue.new(path.to_s, "Datei konnte nicht gelesen werden: #{e.message}")]
        @reporter.report(fehler.first)
        raise Support::ValidationError.new(fehler)
      end

      def problem(fehler, pfad, text)
        befund = Support::Issue.new(pfad, text)
        fehler << befund
        @reporter.report(befund)
        nil
      end

      def versionsfeld(roh, fehler)
        version = Support.integer(roh['version'])
        return version if version && version >= 1

        problem(fehler, 'version', 'muss eine ganze Zahl >= 1 sein')
        1
      end

      def resourcen(roh, fehler)
        roh.fetch('resources', {}).to_h do |schluessel, eintraege|
          pfad = "resources/#{schluessel}"
          unless eintraege.is_a?(Array)
            problem(fehler, pfad, 'muss eine Liste sein')
            next [schluessel, []]
          end
          gesehen = {}
          gueltig = eintraege.each_with_index.filter_map do |eintrag, index|
            eintrag_pfad = "#{pfad}[#{index}]"
            unless eintrag.is_a?(Hash) && eintrag.key?('id') && eintrag.key?('key')
              problem(fehler, eintrag_pfad, 'braucht id und key')
              next nil
            end
            id = Support.integer(eintrag['id'])
            key = eintrag['key'].to_s
            if id.nil? || key.empty?
              problem(fehler, eintrag_pfad, 'id und key müssen gefüllt sein')
              next nil
            end
            if gesehen.key?(id) || gesehen.value?(key)
              problem(fehler, eintrag_pfad, "doppelte Ressource #{id}/#{key}")
              next nil
            end
            gesehen[id] = key
            { 'id' => id, 'key' => key, 'name' => eintrag.fetch('name', key).to_s }
          end
          [schluessel, gueltig]
        end.tap do |werte|
          RESOURCE_KEYS.each do |schluessel|
            next if werte.key?(schluessel)

            problem(fehler, "resources/#{schluessel}", 'Pflicht-Ressourcenliste fehlt')
            werte[schluessel] = []
          end
        end
      end

      def programme(roh, fehler)
        liste = roh['programs']
        unless liste.is_a?(Array)
          problem(fehler, 'programs', 'muss eine Liste sein')
          return []
        end

        liste.each_with_index.filter_map do |eintrag, index|
          pfad = "programs[#{index}]"
          unless eintrag.is_a?(Hash)
            problem(fehler, pfad, 'muss ein Objekt sein')
            next nil
          end
          id = Support.integer(eintrag['id'])
          if id.nil? || id <= 0
            problem(fehler, "#{pfad}/id", 'muss eine positive ganze Zahl sein')
            next nil
          end
          commands = eintrag['commands']
          unless commands.is_a?(Array)
            problem(fehler, "#{pfad}/commands", 'muss eine Liste sein')
            next nil
          end
          Models::Program.new(
            id: id,
            name: eintrag.fetch('name', "programm_#{id}"),
            commands: befehle(commands, "#{pfad}/commands", 0, fehler)
          )
        end
      end

      def befehle(liste, pfad, indent, fehler)
        liste.each_with_index.filter_map do |roh, index|
          eintrag_pfad = "#{pfad}[#{index}]"
          unless roh.is_a?(Hash)
            problem(fehler, eintrag_pfad, 'muss ein Befehlsobjekt sein')
            next nil
          end
          typ_wert = roh.key?('commandType') ? roh['commandType'] : roh['type']
          typ = typ_wert.to_s
          definition = @katalog.definition(typ)
          unless definition
            problem(fehler, "#{eintrag_pfad}/commandType", "unbekannter Befehlstyp '#{typ}'")
            next nil
          end
          if roh.key?('code') && Support.integer(roh['code']) != definition.code
            problem(fehler, "#{eintrag_pfad}/code", "passt nicht zu #{typ} (#{definition.code})")
          end
          unless roh.key?('indent') && !roh['indent'].nil?
            problem(fehler, "#{eintrag_pfad}/indent", 'Pflichtfeld indent fehlt')
          end
          befund_indent = Support.integer(roh['indent'])
          if befund_indent.nil? || befund_indent != indent
            problem(fehler, "#{eintrag_pfad}/indent", "erwartet #{indent}")
          end
          parameter = parameter(roh.fetch('params', {}), definition, eintrag_pfad, fehler)
          kinder = kinder(roh.fetch('blocks', {}), definition, eintrag_pfad, indent, fehler)
          validiere_spezialfall(typ, parameter, definition, eintrag_pfad, fehler)
          Models::Command.new(
            commandType: definition.type,
            code: definition.code,
            indent: indent,
            params: parameter,
            children: kinder
          )
        end
      end

      def parameter(roh, definition, pfad, fehler)
        unless roh.is_a?(Hash)
          problem(fehler, "#{pfad}/params", 'muss ein Objekt sein')
          roh = {}
        end
        roh = Support.stringify_keys(roh)
        definition.fields.each_with_object({}) do |(name, field), ergebnis|
          wert = roh.key?(name) ? roh[name] : field.default
          if wert.nil? && field.required
            problem(fehler, "#{pfad}/params/#{name}", 'Pflichtfeld fehlt')
            next
          end
          next if wert.nil?

          geprueft = feld_wert(wert, field, "#{pfad}/params/#{name}", fehler)
          ergebnis[name] = geprueft unless geprueft.nil?
        end
      end

      def feld_wert(wert, field, pfad, fehler)
        case field.kind
        when :string
          unless wert.is_a?(String) && (!wert.empty? || field.allow_empty)
            problem(fehler, pfad, 'muss ein nicht leerer String sein')
            return nil
          end
          wert
        when :string_list
          unless wert.is_a?(Array) && wert.all? { |eintrag| eintrag.is_a?(String) }
            problem(fehler, pfad, 'muss eine Liste aus Strings sein')
            return nil
          end
          problem(fehler, pfad, 'darf nicht leer sein') if wert.empty? && !field.allow_empty
          wert.dup
        when :integer
          zahl = Support.integer(wert)
          if zahl.nil? || (field.min && zahl < field.min) || (field.max && zahl > field.max)
            problem(fehler, pfad, "muss eine ganze Zahl#{bereich(field)} sein")
            return nil
          end
          zahl
        when :number
          zahl = Support.number(wert)
          if zahl.nil? || (field.min && zahl < field.min) || (field.max && zahl > field.max)
            problem(fehler, pfad, "muss eine Zahl#{bereich(field)} sein")
            return nil
          end
          zahl
        when :bool
          unless [true, false].include?(wert)
            problem(fehler, pfad, 'muss true oder false sein')
            return nil
          end
          wert
        when :enum
          text = wert.to_s
          unless field.values.include?(text)
            problem(fehler, pfad, "muss einer von #{field.values.join(', ')} sein")
            return nil
          end
          text
        when :key
          text = wert.to_s
          unless ressource_existiert?(field.key, text, key: true)
            problem(fehler, pfad, "unbekannter Asset-Key '#{text}'")
            return nil
          end
          text
        when :ref
          unless ressource_existiert?(field.ref, wert)
            problem(fehler, pfad, "Referenz #{wert.inspect} existiert nicht in #{field.ref}")
            return nil
          end
          wert
        when :actor_ref
          if Support.integer(wert).nil? || Support.integer(wert).negative?
            problem(fehler, pfad, 'muss eine nicht negative Actor-ID sein')
            return nil
          end
          if Support.integer(wert).positive? && !ressource_existiert?('actors', wert)
            problem(fehler, pfad, "Actor #{wert} existiert nicht")
            return nil
          end
          Support.integer(wert)
        when :event_ref
          if Support.integer(wert).nil? || Support.integer(wert).negative?
            problem(fehler, pfad, 'muss eine nicht negative Event-ID sein')
            return nil
          end
          if Support.integer(wert).positive? && !ressource_existiert?('events', wert)
            problem(fehler, pfad, "Event #{wert} existiert nicht")
            return nil
          end
          Support.integer(wert)
        when :enemy_slot
          slot = Support.integer(wert)
          minimum = field.min || 0
          if slot.nil? || slot < minimum || slot > (field.max || 7)
            problem(fehler, pfad, "muss ein Slot zwischen #{minimum} und #{field.max || 7} sein")
            return nil
          end
          slot
        when :equipment_ref
          wert
        when :color
          farbe(wert, pfad, fehler)
        when :move_steps
          move_steps(wert, pfad, fehler)
        when :goods
          goods(wert, pfad, fehler)
        else
          problem(fehler, pfad, "unbekannter Feldtyp #{field.kind}")
          nil
        end
      end

      def bereich(field)
        teile = []
        teile << ">= #{field.min}" if field.min
        teile << "<= #{field.max}" if field.max
        teile.empty? ? '' : " (#{teile.join(', ')})"
      end

      def farbe(wert, pfad, fehler)
        unless wert.is_a?(Hash)
          problem(fehler, pfad, 'muss ein Farbobjekt sein')
          return nil
        end
        farbe = Support.stringify_keys(wert)
        COLORS.each_with_object({}) do |kanal, ergebnis|
          kanalwert = Support.integer(farbe.fetch(kanal, 0))
          if kanalwert.nil? || kanalwert < -255 || kanalwert > 255
            problem(fehler, "#{pfad}/#{kanal}", 'muss zwischen -255 und 255 liegen')
            next
          end
          ergebnis[kanal] = kanalwert
        end
      end

      def move_steps(wert, pfad, fehler)
        unless wert.is_a?(Array)
          problem(fehler, pfad, 'muss eine Liste von Schritten sein')
          return nil
        end
        wert.each_with_index.filter_map do |schritt, index|
          schritt_pfad = "#{pfad}[#{index}]"
          unless schritt.is_a?(Hash) && MOVE_ACTIONS.include?(schritt['action'].to_s)
            problem(fehler, "#{schritt_pfad}/action", "muss einer von #{MOVE_ACTIONS.join(', ')} sein")
            next nil
          end
          typisierter_schritt = Support.stringify_keys(schritt)
          if typisierter_schritt['action'] == 'walk' && !DIRECTIONS.include?(typisierter_schritt['direction'].to_s)
            problem(fehler, "#{schritt_pfad}/direction", 'muss eine gültige Richtung sein')
          end
          if typisierter_schritt['action'] == 'jump' &&
             (Support.integer(typisierter_schritt['dx']).nil? || Support.integer(typisierter_schritt['dz']).nil?)
            problem(fehler, schritt_pfad, 'jump braucht dx und dz')
          end
          typisierter_schritt
        end
      end

      def goods(wert, pfad, fehler)
        unless wert.is_a?(Array) && !wert.empty?
          problem(fehler, pfad, 'muss eine nicht leere Warenliste sein')
          return nil
        end
        collection = { 'item' => 'items', 'weapon' => 'weapons', 'armor' => 'armors' }
        wert.each_with_index.filter_map do |eintrag, index|
          eintrag_pfad = "#{pfad}[#{index}]"
          unless eintrag.is_a?(Hash)
            problem(fehler, eintrag_pfad, 'muss ein Warenobjekt sein')
            next nil
          end
          ware = Support.stringify_keys(eintrag)
          art = ware['kind'].to_s
          unless collection.key?(art)
            problem(fehler, "#{eintrag_pfad}/kind", "muss einer von #{collection.keys.join(', ')} sein")
            next nil
          end
          unless ressource_existiert?(collection[art], ware['id'])
            problem(fehler, "#{eintrag_pfad}/id", "Ware existiert nicht in #{collection[art]}")
            next nil
          end
          preis = Support.integer(ware.fetch('price', 0))
          menge = Support.integer(ware.fetch('quantity', 1))
          if preis.nil? || preis.negative?
            problem(fehler, "#{eintrag_pfad}/price", 'muss eine nicht negative ganze Zahl sein')
            next nil
          end
          if menge.nil? || menge < 1
            problem(fehler, "#{eintrag_pfad}/quantity", 'muss mindestens 1 sein')
            next nil
          end
          { 'kind' => art, 'id' => ware['id'], 'price' => preis, 'quantity' => menge }
        end
      end

      def kinder(roh, definition, pfad, indent, fehler)
        unless roh.is_a?(Hash)
          problem(fehler, "#{pfad}/blocks", 'muss ein Objekt sein')
          return {}
        end
        roh = Support.stringify_keys(roh)
        roh.keys.each do |name|
          next if definition.blocks.include?(name)

          problem(fehler, "#{pfad}/blocks/#{name}", "Block #{name} ist für #{definition.type} nicht erlaubt")
        end
        definition.blocks.each_with_object({}) do |name, ergebnis|
          next unless roh.key?(name)

          unless roh[name].is_a?(Array)
            problem(fehler, "#{pfad}/blocks/#{name}", 'muss eine Liste sein')
            next
          end
          ergebnis[name] = befehle(roh[name], "#{pfad}/blocks/#{name}", indent + 1, fehler)
        end
      end

      def validiere_spezialfall(typ, parameter, definition, pfad, fehler)
        case typ
        when 'conditional_branch' then validiere_bedingung(parameter, pfad, fehler)
        when 'control_switches' then validiere_bereich(parameter, 'switch', pfad, fehler)
        when 'control_variables'
          validiere_bereich(parameter, 'variable', pfad, fehler)
          validiere_variable(parameter, pfad, fehler)
        when 'wait'
          if parameter['seconds'].nil? && parameter['frames'].nil?
            problem(fehler, "#{pfad}/params", 'wait braucht seconds oder frames')
          end
        when 'change_map_settings' then validiere_karteneinstellung(parameter, pfad, fehler)
        when 'change_equipment' then validiere_ausruestung(parameter, pfad, fehler)
        when 'show_choices' then validiere_auswahlen(parameter, pfad, fehler)
        when 'script'
          if parameter['script_key'].nil? && parameter['expression'].to_s.empty? && parameter['code'].to_s.empty?
            problem(fehler, "#{pfad}/params", 'script braucht script_key, expression oder code')
          end
        end
      end

      def validiere_bereich(parameter, art, pfad, fehler)
        von = Support.integer(parameter['from_id'])
        bis = Support.integer(parameter['to_id'])
        return if von.nil? || bis.nil?
        return if von <= bis

        problem(fehler, "#{pfad}/params/to_id", "#{art}-Bereich darf nicht rückwärts laufen")
      end

      def validiere_bedingung(parameter, pfad, fehler)
        base = "#{pfad}/params"
        required = case parameter['kind']
                   when 'switch' then %w[switch_id value]
                   when 'variable' then %w[variable_id comparator]
                   when 'self_switch' then %w[letter value]
                   when 'timer' then %w[seconds]
                   when 'actor' then %w[actor_id]
                   when 'enemy' then %w[slot]
                   when 'gold' then %w[value comparator]
                   when 'item' then %w[item_id value comparator]
                   when 'weapon' then %w[weapon_id value comparator]
                   when 'armor' then %w[armor_id value comparator]
                   when 'button' then %w[button]
                   when 'script' then []
                   else []
                   end
        required.each do |name|
          problem(fehler, "#{base}/#{name}", "Pflichtfeld für #{parameter['kind']} fehlt") if parameter[name].nil?
        end
        if parameter['kind'] == 'variable' && parameter['value'].nil? && parameter['value_variable_id'].nil?
          problem(fehler, "#{base}/value", 'Variable braucht value oder value_variable_id')
        end
        if parameter['kind'] == 'script' && parameter['script_key'].nil? && parameter['expression'].to_s.empty?
          problem(fehler, "#{base}/script_key", 'Script-Bedingung braucht script_key oder expression')
        end
      end

      def validiere_variable(parameter, pfad, fehler)
        base = "#{pfad}/params"
        required = case parameter['source']
                   when 'variable' then %w[source_variable_id]
                   when 'random' then %w[random_min random_max]
                   when 'item' then %w[item_id]
                   when 'actor' then %w[actor_id]
                   when 'enemy' then %w[enemy_slot]
                   when 'other' then %w[other]
                   else []
                   end
        required.each do |name|
          problem(fehler, "#{base}/#{name}", "Pflichtfeld für #{parameter['source']} fehlt") if parameter[name].nil?
        end
        if parameter['source'] == 'constant' && parameter['value'].nil?
          problem(fehler, "#{base}/value", 'Konstante braucht value')
        end
        if parameter['source'] == 'random' && parameter['random_min'].to_i > parameter['random_max'].to_i
          problem(fehler, "#{base}/random_max", 'darf nicht kleiner als random_min sein')
        end
      end

      def validiere_karteneinstellung(parameter, pfad, fehler)
        collection = { 'panorama' => 'panoramas', 'fog' => 'fogs',
                       'battleback' => 'battlebacks' }[parameter['target']]
        return unless collection
        return if ressource_existiert?(collection, parameter['asset_key'], key: true)

        problem(fehler, "#{pfad}/params/asset_key", "unbekannter Key in #{collection}")
      end

      def validiere_ausruestung(parameter, pfad, fehler)
        collection = parameter['slot'] == 'weapon' ? 'weapons' : 'armors'
        return if parameter['item_id'].nil?
        return if ressource_existiert?(collection, parameter['item_id'])

        problem(fehler, "#{pfad}/params/item_id", "Ausrüstung existiert nicht in #{collection}")
      end

      def validiere_auswahlen(parameter, pfad, fehler)
        anzahl = parameter['choices'].to_a.size
        if anzahl < 1 || anzahl > 4
          problem(fehler, "#{pfad}/params/choices", 'erlaubt 1 bis 4 Auswahlen')
        end
        cancel = parameter['cancel_index'].to_i
        return if cancel == -1 || (cancel >= 0 && cancel < anzahl)

        problem(fehler, "#{pfad}/params/cancel_index", 'liegt außerhalb der Auswahlen')
      end

      def ressource_existiert?(collection, wert, key: false)
        eintraege = @resources.fetch(collection.to_s, [])
        eintraege.any? do |eintrag|
          key ? eintrag['key'] == wert.to_s : eintrag['id'] == wert || eintrag['id'].to_s == wert.to_s
        end
      end

      def validiere_querverweise(programme, fehler)
        programme.each do |programm|
          marken = label_namen(programm.commands)
          each_command(programm.commands) do |command, pfad|
            next unless command.commandType == 'jump_to_label'
            next if marken.include?(command.params['name'])

            problem(fehler, "#{pfad}/params/name", "Sprungmarke '#{command.params['name']}' fehlt in Programm #{programm.id}")
          end
        end
      end

      def each_command(commands, pfad = 'commands', &block)
        commands.each_with_index do |command, index|
          command_pfad = "#{pfad}[#{index}]"
          yield command, command_pfad
          command.children.each do |name, unterbefehle|
            each_command(unterbefehle, "#{command_pfad}/blocks/#{name}", &block)
          end
        end
      end

      def label_namen(commands)
        gefundene = []
        each_command(commands) do |command, _pfad|
          gefundene << command.params['name'] if command.commandType == 'label'
        end
        gefundene
      end
    end
  end
end
