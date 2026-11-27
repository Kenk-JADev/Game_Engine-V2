#encoding: utf-8
# ============================================================================
#  Rpg::Loader - externes JSON in unveränderliche Datenbankmodelle überführen.
#
#  Der Loader ist der einzige Ort, der JSON-Strings kennt. Er prüft Pflicht-
#  felder, doppelte IDs, Enums, alle Querverweise und Asset-Keys und sammelt
#  Befunde, bevor er einen `ValidationError` wirft. Dateizugriff und
#  Fehlerausgabe sind injizierbar (Dependency Injection).
# ============================================================================

require 'json'
require 'set'

module Rpg
  class Loader
    AREA_KEYS = %i[
      actors classes skills items weapons armors enemies troops states
      animations tilesets common_events
    ].freeze
    SYSTEM_LIST_KEYS = %i[elements species switches variables].freeze
    EQUIPMENT_SLOTS = %w[weapon shield helmet body accessory].freeze
    PARAMETERS = %w[maxhp maxsp str dex agi int atk pdef mdef eva].freeze
    GROWTH_KEYS = %i[maxhp maxsp str dex agi int].freeze
    ARMOR_KINDS = %w[shield helmet body accessory].freeze
    TRIGGERS = %w[none autorun parallel].freeze
    EVENT_SPANS = %w[battle turn moment hp_below].freeze
    COMPARATORS = %w[== <= >= < >].freeze
    FLASH_TARGETS = %w[user target screen].freeze

    DEFAULT_PATH = if defined?(Rune::ROOT)
                     File.join(Rune::ROOT, 'game', 'config', 'rpg_database.json')
                   else
                     File.expand_path('../../config/rpg_database.json', __dir__)
                   end

    # Einzige Dateiquelle; Tests können hier ein Hash-im-Speicher injizieren.
    class LocalFileSystem
      def read_json(path)
        raise "Datei fehlt: #{path}" unless File.file?(path)

        JSON.parse(File.read(path, encoding: 'UTF-8'))
      end
    end

    class MemoryFileSystem
      def initialize(documents)
        @documents = documents.to_h { |path, document| [path.to_s, Support.stringify_keys(document)] }
      end

      def read_json(path)
        raise "Datei fehlt: #{path}" unless @documents.key?(path.to_s)

        @documents[path.to_s]
      end
    end

    class CollectingReporter
      attr_reader :issues

      def initialize
        @issues = []
      end

      def report(issue)
        @issues << issue
      end
    end

    def initialize(file_system: LocalFileSystem.new, reporter: Support::NullReporter.new)
      @file_system = file_system
      @reporter = reporter
    end

    def load(path = DEFAULT_PATH)
      fehler = []
      anfang = nil
      begin
        anfang = @file_system.read_json(path)
      rescue StandardError => e
        problem(fehler, path, "Datei konnte nicht gelesen werden: #{e.message}")
        raise Support::ValidationError.new(fehler)
      end

      unless anfang.is_a?(Hash)
        problem(fehler, path, 'Wurzel muss ein JSON-Objekt sein')
        raise Support::ValidationError.new(fehler)
      end
      roh = Support.stringify_keys(anfang)

      version = int_field(roh, 'version', path, fehler, default: 1, min: 1)
      unless %w[actors classes skills items weapons armors enemies troops states
                animations tilesets common_events system].all? { |key| roh.key?(key) }
        problem(fehler, path, 'ein Pflichtbereich (13 Bereiche) fehlt')
      end

      system_roh = roh['system'].is_a?(Hash) ? roh['system'] : {}
      problem(fehler, "#{path}/system", 'System muss ein Objekt sein') unless roh['system'].is_a?(Hash)
      katalog = build_catalog(system_roh, path, fehler)

      eingaben = {}
      AREA_KEYS.each { |area| eingaben[area] = collect_entries(roh[area.to_s], area, path, fehler) }
      listen = {}
      SYSTEM_LIST_KEYS.each do |key|
        listen[key] = collect_entries(system_roh[key.to_s], key, "#{path}/system", fehler)
      end

      modelle = build_models(eingaben, listen, system_roh, katalog, path, fehler)
      validate_references(modelle, fehler, path)
      validate_assets(roh, katalog, fehler, path)

      raise Support::ValidationError.new(fehler) if fehler.any?

      build_database(modelle, version)
    end

    private

    # ------------------------------------------------------------ Basishelfer

    def problem(fehler, pfad, text)
      befund = Support::Issue.new(pfad, text)
      fehler << befund
      @reporter.report(befund)
      nil
    end

    def string_field(hash, key, pfad, fehler, required: true, default: '', allow_empty: false)
      wert = hash[key]
      if wert.nil?
        problem(fehler, "#{pfad}/#{key}", 'Pflichtfeld fehlt') if required
        return default
      end
      unless wert.is_a?(String)
        problem(fehler, "#{pfad}/#{key}", 'erwartet einen String')
        return default
      end
      unless allow_empty || !wert.strip.empty?
        problem(fehler, "#{pfad}/#{key}", 'String darf nicht leer sein')
        return default
      end
      wert
    end

    def int_field(hash, key, pfad, fehler, required: false, default: 0,
                  min: nil, max: nil, positive: false)
      roh_wert = hash[key]
      if roh_wert.nil? && !required
        return default
      end
      wert = Support.integer(roh_wert)
      unless wert
        problem(fehler, "#{pfad}/#{key}", 'erwartet eine ganze Zahl')
        return default
      end
      if positive && wert <= 0
        problem(fehler, "#{pfad}/#{key}", 'muss grösser als 0 sein')
        return default
      end
      if min && wert < min
        problem(fehler, "#{pfad}/#{key}", "muss mindestens #{min} sein")
        return default
      end
      if max && wert > max
        problem(fehler, "#{pfad}/#{key}", "darf höchstens #{max} sein")
        return default
      end
      wert
    end

    def float_field(hash, key, pfad, fehler, required: true, default: 0.0,
                    min: nil, max: nil)
      roh_wert = hash[key]
      if roh_wert.nil? && !required
        return default
      end
      wert = Support.number(roh_wert)
      unless wert
        problem(fehler, "#{pfad}/#{key}", 'erwartet eine Zahl')
        return default
      end
      if min && wert < min
        problem(fehler, "#{pfad}/#{key}", "muss mindestens #{min} sein")
        return default
      end
      if max && wert > max
        problem(fehler, "#{pfad}/#{key}", "darf höchstens #{max} sein")
        return default
      end
      wert
    end

    def bool_field(hash, key, pfad, fehler, required: true, default: false)
      wert = hash[key]
      if wert.nil?
        problem(fehler, "#{pfad}/#{key}", 'Pflichtfeld fehlt') if required
        return default
      end
      return wert if [true, false].include?(wert)

      problem(fehler, "#{pfad}/#{key}", 'erwartet true oder false')
      default
    end

    def ref_field(hash, key, pfad, fehler)
      wert = int_field(hash, key, pfad, fehler, min: 0)
      wert.zero? ? nil : wert
    end

    def int_list(hash, key, pfad, fehler)
      wert = hash[key]
      return [] if wert.nil?

      unless wert.is_a?(Array)
        problem(fehler, "#{pfad}/#{key}", 'erwartet eine Liste')
        return []
      end
      wert.each_with_index.filter_map do |eintrag, index|
        id = Support.integer(eintrag)
        if id && id.positive?
          id
        else
          problem(fehler, "#{pfad}/#{key}[#{index}]", 'erwartet eine ID > 0')
          nil
        end
      end
    end

    def multiplier_map(hash, key, pfad, fehler)
      wert = hash[key]
      return {} if wert.nil?
      unless wert.is_a?(Hash)
        problem(fehler, "#{pfad}/#{key}", 'erwartet ein Objekt mit ID → Faktor')
        return {}
      end

      wert.each_with_object({}) do |(schluessel, faktor), ergebnis|
        id = Support.integer(schluessel)
       zahl = Support.number(faktor)
        unless id && id.positive?
          problem(fehler, "#{pfad}/#{key}/#{schluessel}", 'erwartet eine ID > 0')
          next
        end
        unless zahl && zahl.between?(0.25, 3.0)
          problem(fehler, "#{pfad}/#{key}/#{schluessel}", 'Faktor muss zwischen 0.25 und 3.0 liegen')
          next
        end
        ergebnis[id] = zahl
      end
    end

    def rates_field(hash, key, pfad, fehler)
      wert = hash[key]
      unless wert.is_a?(Hash)
        problem(fehler, "#{pfad}/#{key}", 'erwartet ein Objekt mit allen Parameter-Raten')
        wert = {}
      end
      PARAMETERS.each_with_object({}) do |name, ergebnis|
        basis = "#{pfad}/#{key}/#{name}"
        unless wert.key?(name)
          problem(fehler, basis, 'Pflichtfeld fehlt')
          ergebnis[name.to_sym] = name == 'eva' ? 0.0 : 100.0
          next
        end
        ergebnis[name.to_sym] = float_field(wert, name, "#{pfad}/#{key}", fehler,
                                            min: name == 'eva' ? -100 : 0,
                                            max: name == 'eva' ? 100 : 300)
      end
    end

    def growth_field(hash, pfad, fehler)
      wert = hash['growth']
      unless wert.is_a?(Hash)
        problem(fehler, "#{pfad}/growth", 'erwartet Wachstumskurven')
        wert = {}
      end
      GROWTH_KEYS.each_with_object({}) do |name, ergebnis|
        teil = wert[name.to_s]
        basis = "#{pfad}/growth/#{name}"
        unless teil.is_a?(Hash)
          problem(fehler, basis, 'erwartet base und per_level')
          ergebnis[name] = Models::GrowthCurve.new(base: 0.0, per_level: 0.0)
          next
        end
        ergebnis[name] = Models::GrowthCurve.new(
          base: float_field(teil, 'base', basis, fehler, min: 0, max: 100_000),
          per_level: float_field(teil, 'per_level', basis, fehler, min: -100, max: 1000)
        )
      end
    end

    def commands_field(hash, key, pfad, fehler)
      wert = hash[key]
      return [] if wert.nil?
      unless wert.is_a?(Array)
        problem(fehler, "#{pfad}/#{key}", 'erwartet eine Befehlsliste')
        return []
      end
      wert.each_with_index.map do |eintrag, index|
        basis = "#{pfad}/#{key}[#{index}]"
        unless eintrag.is_a?(Hash)
          problem(fehler, basis, 'erwartet einen Befehl')
          next Models::EventCommand.new(code: 0, params: {})
        end
        params = eintrag['params'].is_a?(Hash) ? Support.stringify_keys(eintrag['params']) : {}
        problem(fehler, "#{basis}/params", 'erwartet ein Objekt') unless eintrag['params'].is_a?(Hash)
        Models::EventCommand.new(
          code: int_field(eintrag, 'code', basis, fehler, required: true, min: 1),
          params: params
        )
      end
    end

    def asset_field(hash, key, pfad, fehler, katalog, required: true)
      wert = string_field(hash, key, pfad, fehler, required: required)
      return wert if wert.to_s.empty?

      unless katalog.key?(wert)
        problem(fehler, "#{pfad}/#{key}", "unbekannter Asset-Key '#{wert}'")
      end
      wert
    end

    def asset_wert(wert, pfad, fehler, katalog)
      return if wert.to_s.empty?

      unless katalog.key?(wert.to_s)
        problem(fehler, pfad, "unbekannter Asset-Key '#{wert}'")
      end
    end

    def build_catalog(system_roh, pfad, fehler)
      roh = system_roh['asset_catalog']
      unless roh.is_a?(Hash)
        problem(fehler, "#{pfad}/system/asset_catalog", 'erwartet ein Asset-Katalog')
        return {}
      end
      roh.each_with_object({}) do |(key, art), ergebnis|
        if key.to_s.empty? || art.to_s.empty?
          problem(fehler, "#{pfad}/system/asset_catalog/#{key}", 'Key und Typ dürfen nicht leer sein')
          next
        end
        ergebnis[key.to_s] = art.to_s
      end
    end

    # ------------------------------------------------------- ID-Einsammlung

    def collect_entries(wert, bereich, pfad, fehler)
      unless wert.is_a?(Array)
        problem(fehler, "#{pfad}/#{bereich}", 'erwartet eine Liste')
        return []
      end

      gesehen = {}
      wert.each_with_index.each_with_object([]) do |(eintrag, index), ergebnis|
        basis = "#{pfad}/#{bereich}[#{index}]"
        unless eintrag.is_a?(Hash)
          problem(fehler, basis, 'erwartet ein Objekt')
          next
        end
        id = Support.integer(eintrag['id'])
        unless id && id.positive?
          problem(fehler, "#{basis}/id", 'erwartet eine ID > 0')
          next
        end
        if gesehen[id]
          problem(fehler, "#{basis}/id", "doppelte ID #{id}")
          next
        end
        gesehen[id] = true
        ergebnis << [eintrag, basis]
      end
    end

    # ---------------------------------------------------------- Modellbau

    def build_models(eingaben, listen, system_roh, katalog, pfad, fehler)
      modelle = {}
      AREA_KEYS.each do |bereich|
        modelle[bereich] = eingaben[bereich].map do |eintrag, basis|
          send("build_#{bereich}", eintrag, basis, fehler, katalog)
        end
      end

      listen_modelle = {}
      SYSTEM_LIST_KEYS.each do |key|
        listen_modelle[key] = listen[key].map do |eintrag, basis|
          Models::CatalogEntry.new(
            id: int_field(eintrag, 'id', basis, fehler, required: true, positive: true),
            name: string_field(eintrag, 'name', basis, fehler)
          )
        end
      end
      modelle[:system_lists] = listen_modelle
      modelle[:system] = build_system(system_roh, listen_modelle, katalog, pfad, fehler)
      modelle
    end

    def build_actors(eintrag, basis, fehler, katalog)
      Models::Actor.new(
        id: int_field(eintrag, 'id', basis, fehler, required: true, positive: true),
        name: string_field(eintrag, 'name', basis, fehler),
        class_id: int_field(eintrag, 'class_id', basis, fehler, required: true, positive: true),
        initial_level: int_field(eintrag, 'initial_level', basis, fehler, required: true, min: 1, max: 999),
        final_level: int_field(eintrag, 'final_level', basis, fehler, required: true, min: 1, max: 999),
        exp_basis: int_field(eintrag, 'exp_basis', basis, fehler, required: true, min: 10, max: 50),
        exp_inflation: int_field(eintrag, 'exp_inflation', basis, fehler, required: true, min: 10, max: 50),
        character_key: asset_field(eintrag, 'character_key', basis, fehler, katalog),
        character_hue: int_field(eintrag, 'character_hue', basis, fehler, min: 0, max: 360),
        battler_key: asset_field(eintrag, 'battler_key', basis, fehler, katalog),
        battler_hue: int_field(eintrag, 'battler_hue', basis, fehler, min: 0, max: 360),
        growth: growth_field(eintrag, basis, fehler),
        start_equipment: equipment_field(eintrag, basis, fehler)
      )
    end

    def equipment_field(eintrag, basis, fehler)
      roh = eintrag['start_equipment']
      unless roh.is_a?(Array)
        problem(fehler, "#{basis}/start_equipment", 'erwartet eine Ausrüstungsliste')
        roh = []
      end
      gefunden = {}
      roh.each_with_index do |ref, index|
        teil = "#{basis}/start_equipment[#{index}]"
        unless ref.is_a?(Hash)
          problem(fehler, teil, 'erwartet einen Ausrüstungsslot')
          next
        end
        slot = string_field(ref, 'slot', teil, fehler)
        unless EQUIPMENT_SLOTS.include?(slot)
          problem(fehler, "#{teil}/slot", "unbekannter Slot '#{slot}'")
          next
        end
        if gefunden.key?(slot)
          problem(fehler, "#{teil}/slot", "Slot '#{slot}' ist doppelt")
          next
        end
        item_id = int_field(ref, 'item_id', teil, fehler, min: 0)
        gefunden[slot] = Models::EquipmentRef.new(
          slot: slot,
          item_id: item_id.zero? ? nil : item_id,
          fixed: bool_field(ref, 'fixed', teil, fehler)
        )
      end
      EQUIPMENT_SLOTS.map do |slot|
        gefunden[slot] || Models::EquipmentRef.new(slot: slot, item_id: nil, fixed: false)
      end
    end

    def build_classes(eintrag, basis, fehler, _katalog)
      roh = eintrag['learnings']
      unless roh.is_a?(Array)
        problem(fehler, "#{basis}/learnings", 'erwartet eine Lernliste')
        roh = []
      end
      learnings = roh.each_with_index.map do |teil, index|
        pfad = "#{basis}/learnings[#{index}]"
        unless teil.is_a?(Hash)
          problem(fehler, pfad, 'erwartet einen Lerneintrag')
          next Models::Learning.new(level: 0, skill_id: 0)
        end
        Models::Learning.new(
          level: int_field(teil, 'level', pfad, fehler, required: true, min: 1, max: 999),
          skill_id: int_field(teil, 'skill_id', pfad, fehler, required: true, positive: true)
        )
      end
      position = int_field(eintrag, 'position', basis, fehler, min: 0, max: 2)
      Models::ClassData.new(
        id: int_field(eintrag, 'id', basis, fehler, required: true, positive: true),
        name: string_field(eintrag, 'name', basis, fehler),
        position: position,
        weapon_ids: int_list(eintrag, 'weapon_ids', basis, fehler),
        armor_ids: int_list(eintrag, 'armor_ids', basis, fehler),
        element_multipliers: multiplier_map(eintrag, 'element_multipliers', basis, fehler),
        state_multipliers: multiplier_map(eintrag, 'state_multipliers', basis, fehler),
        learnings: learnings
      )
    end

    def build_skills(eintrag, basis, fehler, katalog)
      Models::Skill.new(
        id: int_field(eintrag, 'id', basis, fehler, required: true, positive: true),
        name: string_field(eintrag, 'name', basis, fehler),
        icon_key: asset_field(eintrag, 'icon_key', basis, fehler, katalog),
        description: string_field(eintrag, 'description', basis, fehler, allow_empty: true),
        scope: int_field(eintrag, 'scope', basis, fehler, min: 0, max: 7),
        occasion: int_field(eintrag, 'occasion', basis, fehler, min: 0, max: 3),
        animation1_id: ref_field(eintrag, 'animation1_id', basis, fehler),
        animation2_id: ref_field(eintrag, 'animation2_id', basis, fehler),
        menu_se_key: asset_field(eintrag, 'menu_se_key', basis, fehler, katalog),
        common_event_id: ref_field(eintrag, 'common_event_id', basis, fehler),
        sp_cost: int_field(eintrag, 'sp_cost', basis, fehler, min: 0),
        power: int_field(eintrag, 'power', basis, fehler, min: 0, max: 9999),
        atk_f: int_field(eintrag, 'atk_f', basis, fehler, min: 0, max: 1000),
        eva_f: int_field(eintrag, 'eva_f', basis, fehler, min: -100, max: 1000),
        str_f: int_field(eintrag, 'str_f', basis, fehler, min: 0, max: 1000),
        dex_f: int_field(eintrag, 'dex_f', basis, fehler, min: 0, max: 1000),
        agi_f: int_field(eintrag, 'agi_f', basis, fehler, min: 0, max: 1000),
        int_f: int_field(eintrag, 'int_f', basis, fehler, min: 0, max: 1000),
        hit: int_field(eintrag, 'hit', basis, fehler, min: 0, max: 100),
        pdef_f: int_field(eintrag, 'pdef_f', basis, fehler, min: 0, max: 1000),
        mdef_f: int_field(eintrag, 'mdef_f', basis, fehler, min: 0, max: 1000),
        variance: int_field(eintrag, 'variance', basis, fehler, min: 0, max: 100),
        element_ids: int_list(eintrag, 'element_ids', basis, fehler),
        plus_state_ids: int_list(eintrag, 'plus_state_ids', basis, fehler),
        minus_state_ids: int_list(eintrag, 'minus_state_ids', basis, fehler)
      )
    end

    def build_items(eintrag, basis, fehler, katalog)
      Models::Item.new(
        id: int_field(eintrag, 'id', basis, fehler, required: true, positive: true),
        name: string_field(eintrag, 'name', basis, fehler),
        icon_key: asset_field(eintrag, 'icon_key', basis, fehler, katalog),
        description: string_field(eintrag, 'description', basis, fehler, allow_empty: true),
        scope: int_field(eintrag, 'scope', basis, fehler, min: 0, max: 7),
        occasion: int_field(eintrag, 'occasion', basis, fehler, min: 0, max: 3),
        animation1_id: ref_field(eintrag, 'animation1_id', basis, fehler),
        animation2_id: ref_field(eintrag, 'animation2_id', basis, fehler),
        menu_se_key: asset_field(eintrag, 'menu_se_key', basis, fehler, katalog),
        common_event_id: ref_field(eintrag, 'common_event_id', basis, fehler),
        price: int_field(eintrag, 'price', basis, fehler, min: 0),
        consumable: bool_field(eintrag, 'consumable', basis, fehler),
        hp_recovery_rate: int_field(eintrag, 'hp_recovery_rate', basis, fehler, min: 0, max: 100),
        sp_recovery_rate: int_field(eintrag, 'sp_recovery_rate', basis, fehler, min: 0, max: 100),
        hp_recovery_value: int_field(eintrag, 'hp_recovery_value', basis, fehler, min: 0),
        sp_recovery_value: int_field(eintrag, 'sp_recovery_value', basis, fehler, min: 0),
        param_inc: param_inc_field(eintrag, basis, fehler),
        element_ids: int_list(eintrag, 'element_ids', basis, fehler),
        plus_state_ids: int_list(eintrag, 'plus_state_ids', basis, fehler),
        minus_state_ids: int_list(eintrag, 'minus_state_ids', basis, fehler)
      )
    end

    def param_inc_field(eintrag, basis, fehler)
      roh = eintrag['param_inc']
      unless roh.is_a?(Hash)
        problem(fehler, "#{basis}/param_inc", 'erwartet parameter und amount')
        roh = {}
      end
      parameter = string_field(roh, 'parameter', "#{basis}/param_inc", fehler, default: 'maxhp')
      unless PARAMETERS.include?(parameter)
        problem(fehler, "#{basis}/param_inc/parameter", "unbekannter Parameter '#{parameter}'")
        parameter = 'maxhp'
      end
      {
        parameter: parameter,
        amount: int_field(roh, 'amount', "#{basis}/param_inc", fehler, min: -1000, max: 1000)
      }
    end

    def build_weapons(eintrag, basis, fehler, katalog)
      Models::Weapon.new(
        id: int_field(eintrag, 'id', basis, fehler, required: true, positive: true),
        name: string_field(eintrag, 'name', basis, fehler),
        icon_key: asset_field(eintrag, 'icon_key', basis, fehler, katalog),
        description: string_field(eintrag, 'description', basis, fehler, allow_empty: true),
        animation1_id: ref_field(eintrag, 'animation1_id', basis, fehler),
        animation2_id: ref_field(eintrag, 'animation2_id', basis, fehler),
        price: int_field(eintrag, 'price', basis, fehler, min: 0),
        atk: int_field(eintrag, 'atk', basis, fehler, min: -100, max: 999),
        pdef: int_field(eintrag, 'pdef', basis, fehler, min: -100, max: 999),
        mdef: int_field(eintrag, 'mdef', basis, fehler, min: -100, max: 999),
        eva: int_field(eintrag, 'eva', basis, fehler, min: -100, max: 999),
        str_plus: int_field(eintrag, 'str_plus', basis, fehler, min: -100, max: 999),
        dex_plus: int_field(eintrag, 'dex_plus', basis, fehler, min: -100, max: 999),
        agi_plus: int_field(eintrag, 'agi_plus', basis, fehler, min: -100, max: 999),
        int_plus: int_field(eintrag, 'int_plus', basis, fehler, min: -100, max: 999),
        element_ids: int_list(eintrag, 'element_ids', basis, fehler),
        plus_state_ids: int_list(eintrag, 'plus_state_ids', basis, fehler)
      )
    end

    def build_armors(eintrag, basis, fehler, katalog)
      kind = string_field(eintrag, 'kind', basis, fehler)
      unless ARMOR_KINDS.include?(kind)
        problem(fehler, "#{basis}/kind", "unbekannte Rüstungsart '#{kind}'")
        kind = 'body'
      end
      Models::Armor.new(
        id: int_field(eintrag, 'id', basis, fehler, required: true, positive: true),
        name: string_field(eintrag, 'name', basis, fehler),
        icon_key: asset_field(eintrag, 'icon_key', basis, fehler, katalog),
        description: string_field(eintrag, 'description', basis, fehler, allow_empty: true),
        kind: kind,
        auto_state_id: ref_field(eintrag, 'auto_state_id', basis, fehler),
        price: int_field(eintrag, 'price', basis, fehler, min: 0),
        pdef: int_field(eintrag, 'pdef', basis, fehler, min: -100, max: 999),
        mdef: int_field(eintrag, 'mdef', basis, fehler, min: -100, max: 999),
        eva: int_field(eintrag, 'eva', basis, fehler, min: -100, max: 999),
        str_plus: int_field(eintrag, 'str_plus', basis, fehler, min: -100, max: 999),
        dex_plus: int_field(eintrag, 'dex_plus', basis, fehler, min: -100, max: 999),
        agi_plus: int_field(eintrag, 'agi_plus', basis, fehler, min: -100, max: 999),
        int_plus: int_field(eintrag, 'int_plus', basis, fehler, min: -100, max: 999),
        element_resistances: multiplier_map(eintrag, 'element_resistances', basis, fehler),
        state_resistances: multiplier_map(eintrag, 'state_resistances', basis, fehler)
      )
    end

    def build_enemies(eintrag, basis, fehler, katalog)
      roh = eintrag['actions']
      unless roh.is_a?(Array)
        problem(fehler, "#{basis}/actions", 'erwartet eine Aktionsliste')
        roh = []
      end
      aktionen = roh.each_with_index.map do |teil, index|
        pfad = "#{basis}/actions[#{index}]"
        unless teil.is_a?(Hash)
          problem(fehler, pfad, 'erwartet eine Aktion')
          next Models::EnemyAction.new(skill_id: nil, priority: 0, condition: {})
        end
        bedingung = teil['condition'].is_a?(Hash) ? Support.stringify_keys(teil['condition']) : {}
        problem(fehler, "#{pfad}/condition", 'erwartet ein Objekt') unless teil['condition'].is_a?(Hash)
        Models::EnemyAction.new(
          skill_id: ref_field(teil, 'skill_id', pfad, fehler),
          priority: int_field(teil, 'priority', pfad, fehler, min: 0, max: 99),
          condition: bedingung
        )
      end
      schatz = eintrag['treasure'].is_a?(Hash) ? Support.stringify_keys(eintrag['treasure']) : {}
      problem(fehler, "#{basis}/treasure", 'erwartet ein Objekt') unless eintrag['treasure'].is_a?(Hash)
      Models::Enemy.new(
        id: int_field(eintrag, 'id', basis, fehler, required: true, positive: true),
        name: string_field(eintrag, 'name', basis, fehler),
        battler_key: asset_field(eintrag, 'battler_key', basis, fehler, katalog),
        battler_hue: int_field(eintrag, 'battler_hue', basis, fehler, min: 0, max: 360),
        maxhp: int_field(eintrag, 'maxhp', basis, fehler, required: true, min: 1),
        maxsp: int_field(eintrag, 'maxsp', basis, fehler, min: 0),
        str: int_field(eintrag, 'str', basis, fehler, min: 0),
        dex: int_field(eintrag, 'dex', basis, fehler, min: 0),
        agi: int_field(eintrag, 'agi', basis, fehler, min: 0),
        int: int_field(eintrag, 'int', basis, fehler, min: 0),
        atk: int_field(eintrag, 'atk', basis, fehler, min: 0),
        pdef: int_field(eintrag, 'pdef', basis, fehler, min: 0),
        mdef: int_field(eintrag, 'mdef', basis, fehler, min: 0),
        eva: int_field(eintrag, 'eva', basis, fehler, min: 0),
        animation1_id: ref_field(eintrag, 'animation1_id', basis, fehler),
        animation2_id: ref_field(eintrag, 'animation2_id', basis, fehler),
        element_multipliers: multiplier_map(eintrag, 'element_multipliers', basis, fehler),
        state_multipliers: multiplier_map(eintrag, 'state_multipliers', basis, fehler),
        actions: aktionen,
        exp: int_field(eintrag, 'exp', basis, fehler, min: 0),
        gold: int_field(eintrag, 'gold', basis, fehler, min: 0),
        treasure: Models::EnemyTreasure.new(
          item_id: ref_field(schatz, 'item_id', "#{basis}/treasure", fehler),
          weapon_id: ref_field(schatz, 'weapon_id', "#{basis}/treasure", fehler),
          armor_id: ref_field(schatz, 'armor_id', "#{basis}/treasure", fehler),
          probability: int_field(schatz, 'probability', "#{basis}/treasure", fehler, min: 0, max: 100)
        )
      )
    end

    def build_troops(eintrag, basis, fehler, katalog)
      mitglieder = Array(eintrag['members']).each_with_index.map do |teil, index|
        pfad = "#{basis}/members[#{index}]"
        unless teil.is_a?(Hash)
          problem(fehler, pfad, 'erwartet ein Gruppenmitglied')
          next Models::TroopMember.new(enemy_id: 0, x: 0, z: 0)
        end
        Models::TroopMember.new(
          enemy_id: int_field(teil, 'enemy_id', pfad, fehler, required: true, positive: true),
          x: int_field(teil, 'x', pfad, fehler, min: 0, max: 99),
          z: int_field(teil, 'z', pfad, fehler, min: 0, max: 99)
        )
      end
      ereignisse = Array(eintrag['events']).each_with_index.map do |teil, index|
        pfad = "#{basis}/events[#{index}]"
        unless teil.is_a?(Hash)
          problem(fehler, pfad, 'erwartet ein Kampf-Event')
          next Models::BattleEvent.new(id: 0, name: '', condition: {}, switch_id: nil, commands: [])
        end
        bedingung = teil['condition'].is_a?(Hash) ? Support.stringify_keys(teil['condition']) : {}
        problem(fehler, "#{pfad}/condition", 'erwartet ein Objekt') unless teil['condition'].is_a?(Hash)
        span = bedingung['span'].to_s
        unless EVENT_SPANS.include?(span)
          problem(fehler, "#{pfad}/condition/span", "unbekannte Span-Art '#{span}'")
        end
        unless COMPARATORS.include?(bedingung['comparator'].to_s)
          problem(fehler, "#{pfad}/condition/comparator", 'unbekannter Vergleich')
        end
        Models::BattleEvent.new(
          id: int_field(teil, 'id', pfad, fehler, required: true, positive: true),
          name: string_field(teil, 'name', pfad, fehler),
          condition: bedingung,
          switch_id: ref_field(teil, 'switch_id', pfad, fehler),
          commands: commands_field(teil, 'commands', pfad, fehler)
        )
      end
      Models::Troop.new(
        id: int_field(eintrag, 'id', basis, fehler, required: true, positive: true),
        name: string_field(eintrag, 'name', basis, fehler),
        background_key: asset_field(eintrag, 'background_key', basis, fehler, katalog),
        members: mitglieder,
        events: ereignisse
      )
    end

    def build_states(eintrag, basis, fehler, _katalog)
      Models::State.new(
        id: int_field(eintrag, 'id', basis, fehler, required: true, positive: true),
        name: string_field(eintrag, 'name', basis, fehler),
        animation_id: ref_field(eintrag, 'animation_id', basis, fehler),
        restriction: int_field(eintrag, 'restriction', basis, fehler, min: 0, max: 4),
        nonresistance: bool_field(eintrag, 'nonresistance', basis, fehler),
        zero_hp: bool_field(eintrag, 'zero_hp', basis, fehler),
        cant_get_exp: bool_field(eintrag, 'cant_get_exp', basis, fehler),
        cant_evade: bool_field(eintrag, 'cant_evade', basis, fehler),
        slip_damage: bool_field(eintrag, 'slip_damage', basis, fehler),
        rating: int_field(eintrag, 'rating', basis, fehler, min: 0, max: 10),
        hit_rate: int_field(eintrag, 'hit_rate', basis, fehler, min: 0, max: 100),
        rates: rates_field(eintrag, 'rates', basis, fehler),
        battle_only: bool_field(eintrag, 'battle_only', basis, fehler),
        hold_turn: int_field(eintrag, 'hold_turn', basis, fehler, min: 0, max: 999),
        auto_release_prob: int_field(eintrag, 'auto_release_prob', basis, fehler, min: 0, max: 100),
        shock_release_prob: int_field(eintrag, 'shock_release_prob', basis, fehler, min: 0, max: 100),
        guard_element_ids: int_list(eintrag, 'guard_element_ids', basis, fehler),
        plus_state_ids: int_list(eintrag, 'plus_state_ids', basis, fehler),
        minus_state_ids: int_list(eintrag, 'minus_state_ids', basis, fehler)
      )
    end

    def build_animations(eintrag, basis, fehler, katalog)
      frames = Array(eintrag['frames']).each_with_index.map do |teil, index|
        pfad = "#{basis}/frames[#{index}]"
        unless teil.is_a?(Hash)
          problem(fehler, pfad, 'erwartet eine Sprite-Zelle')
          next Models::AnimationFrame.new(cell_x: 0, cell_y: 0, width: 0, height: 0)
        end
        Models::AnimationFrame.new(
          cell_x: int_field(teil, 'cell_x', pfad, fehler, min: 0),
          cell_y: int_field(teil, 'cell_y', pfad, fehler, min: 0),
          width: int_field(teil, 'width', pfad, fehler, min: 1),
          height: int_field(teil, 'height', pfad, fehler, min: 1)
        )
      end
      timings = Array(eintrag['timings']).each_with_index.map do |teil, index|
        pfad = "#{basis}/timings[#{index}]"
        unless teil.is_a?(Hash)
          problem(fehler, pfad, 'erwartet einen Timing-Eintrag')
          next Models::AnimationTiming.new(
            frame: 0, se_key: '', flash_target: 'target',
            color: Models::Color.new(r: 255, g: 255, b: 255), duration: 0, condition: 'always'
          )
        end
        farbe = teil['color'].is_a?(Hash) ? Support.stringify_keys(teil['color']) : {}
        problem(fehler, "#{pfad}/color", 'erwartet rgb') unless teil['color'].is_a?(Hash)
        ziel = string_field(teil, 'flash_target', pfad, fehler, default: 'target')
        unless FLASH_TARGETS.include?(ziel)
          problem(fehler, "#{pfad}/flash_target", "unbekanntes Ziel '#{ziel}'")
          ziel = 'target'
        end
        Models::AnimationTiming.new(
          frame: int_field(teil, 'frame', pfad, fehler, min: 0),
          se_key: asset_field(teil, 'se_key', pfad, fehler, katalog),
          flash_target: ziel,
          color: Models::Color.new(
            r: int_field(farbe, 'r', "#{pfad}/color", fehler, min: 0, max: 255),
            g: int_field(farbe, 'g', "#{pfad}/color", fehler, min: 0, max: 255),
            b: int_field(farbe, 'b', "#{pfad}/color", fehler, min: 0, max: 255)
          ),
          duration: int_field(teil, 'duration', pfad, fehler, min: 0, max: 600),
          condition: string_field(teil, 'condition', pfad, fehler, default: 'always', allow_empty: true)
        )
      end
      position = int_field(eintrag, 'position', basis, fehler, min: 0, max: 3)
      Models::Animation.new(
        id: int_field(eintrag, 'id', basis, fehler, required: true, positive: true),
        name: string_field(eintrag, 'name', basis, fehler),
        sheet_key: asset_field(eintrag, 'sheet_key', basis, fehler, katalog),
        position: position,
        frame_max: int_field(eintrag, 'frame_max', basis, fehler, required: true, min: 1, max: 256),
        frames: frames,
        timings: timings
      )
    end

    def build_tilesets(eintrag, basis, fehler, katalog)
      autotiles = Array(eintrag['autotile_keys']).each_with_index.map do |key, index|
        asset_wert(key, "#{basis}/autotile_keys[#{index}]", fehler, katalog)
        key.to_s
      end
      properties = Array(eintrag['tile_properties']).each_with_index.map do |teil, index|
        pfad = "#{basis}/tile_properties[#{index}]"
        unless teil.is_a?(Hash)
          problem(fehler, pfad, 'erwartet Tile-Eigenschaften')
          next Models::TileProperty.new(
            index: index, passable: false, passable_4dirs: [false, false, false, false],
            priority: 0, bush: false, counter: false, terrain_tag: 0
          )
        end
        richtungen = teil['passable_4dirs'].is_a?(Array) ? teil['passable_4dirs'] : []
        if richtungen.size != 4
          problem(fehler, "#{pfad}/passable_4dirs", 'erwartet genau 4 Richtungsflaggen')
          richtungen = [false, false, false, false]
        end
        Models::TileProperty.new(
          index: int_field(teil, 'index', pfad, fehler, min: 0),
          passable: bool_field(teil, 'passable', pfad, fehler),
          passable_4dirs: richtungen.map { |wert| !!wert },
          priority: int_field(teil, 'priority', pfad, fehler, min: 0, max: 5),
          bush: bool_field(teil, 'bush', pfad, fehler),
          counter: bool_field(teil, 'counter', pfad, fehler),
          terrain_tag: int_field(teil, 'terrain_tag', pfad, fehler, min: 0, max: 7)
        )
      end
      Models::Tileset.new(
        id: int_field(eintrag, 'id', basis, fehler, required: true, positive: true),
        name: string_field(eintrag, 'name', basis, fehler),
        tileset_key: asset_field(eintrag, 'tileset_key', basis, fehler, katalog),
        autotile_keys: autotiles,
        panorama_key: asset_field(eintrag, 'panorama_key', basis, fehler, katalog, required: false),
        fog_key: asset_field(eintrag, 'fog_key', basis, fehler, katalog, required: false),
        battleback_key: asset_field(eintrag, 'battleback_key', basis, fehler, katalog, required: false),
        fog_opacity: int_field(eintrag, 'fog_opacity', basis, fehler, min: 0, max: 255),
        fog_blend_type: int_field(eintrag, 'fog_blend_type', basis, fehler, min: 0, max: 3),
        fog_zoom: int_field(eintrag, 'fog_zoom', basis, fehler, min: 0, max: 1000),
        fog_sx: int_field(eintrag, 'fog_sx', basis, fehler, min: -100, max: 100),
        fog_sy: int_field(eintrag, 'fog_sy', basis, fehler, min: -100, max: 100),
        tile_properties: properties
      )
    end

    def build_common_events(eintrag, basis, fehler, _katalog)
      trigger = string_field(eintrag, 'trigger', basis, fehler)
      unless TRIGGERS.include?(trigger)
        problem(fehler, "#{basis}/trigger", "unbekannter Trigger '#{trigger}'")
        trigger = 'none'
      end
      Models::CommonEvent.new(
        id: int_field(eintrag, 'id', basis, fehler, required: true, positive: true),
        name: string_field(eintrag, 'name', basis, fehler),
        trigger: trigger,
        condition_switch_id: ref_field(eintrag, 'condition_switch_id', basis, fehler),
        commands: commands_field(eintrag, 'commands', basis, fehler)
      )
    end

    def build_system(roh, listen, katalog, pfad, fehler)
      basis = "#{pfad}/system"
      graphics_roh = roh['graphics'].is_a?(Hash) ? roh['graphics'] : {}
      problem(fehler, "#{basis}/graphics", 'erwartet ein Objekt') unless roh['graphics'].is_a?(Hash)
      graphics = Models::GraphicsConfig.new(
        windowskin_key: asset_field(graphics_roh, 'windowskin_key', basis, fehler, katalog),
        title_key: asset_field(graphics_roh, 'title_key', basis, fehler, katalog),
        gameover_key: asset_field(graphics_roh, 'gameover_key', basis, fehler, katalog),
        battle_transition_key: asset_field(graphics_roh, 'battle_transition_key', basis, fehler, katalog),
        battleback_key: asset_field(graphics_roh, 'battleback_key', basis, fehler, katalog)
      )

      audio_roh = roh['audio'].is_a?(Hash) ? roh['audio'] : {}
      problem(fehler, "#{basis}/audio", 'erwartet ein Objekt') unless roh['audio'].is_a?(Hash)
      audio_namen = Models::AudioConfig.members
      audio_werte = audio_namen.each_with_object({}) do |name, ergebnis|
        wert = string_field(audio_roh, name.to_s, "#{basis}/audio", fehler)
        asset_wert(wert, "#{basis}/audio/#{name}", fehler, katalog)
        ergebnis[name] = wert
      end
      audio = Models::AudioConfig.new(**audio_werte)

      cues_roh = roh['sound_cues'].is_a?(Hash) ? roh['sound_cues'] : {}
      problem(fehler, "#{basis}/sound_cues", 'erwartet ein Objekt') unless roh['sound_cues'].is_a?(Hash)
      sound_cues = cues_roh.each_with_object({}) do |(name, key), ergebnis|
        wert = key.is_a?(String) ? key : ''
        problem(fehler, "#{basis}/sound_cues/#{name}", 'erwartet einen Asset-Key') if wert.empty?
        asset_wert(wert, "#{basis}/sound_cues/#{name}", fehler, katalog)
        ergebnis[name.to_s] = wert
      end

      vokabular_roh = roh['vocabulary'].is_a?(Hash) ? roh['vocabulary'] : {}
      problem(fehler, "#{basis}/vocabulary", 'erwartet ein Objekt') unless roh['vocabulary'].is_a?(Hash)
      vokabular_werte = Models::Vocabulary.members.each_with_object({}) do |name, ergebnis|
        ergebnis[name] = string_field(vokabular_roh, name.to_s, "#{basis}/vocabulary", fehler)
      end
      vokabular = Models::Vocabulary.new(**vokabular_werte)

      Models::SystemData.new(
        magic_number: int_field(roh, 'magic_number', basis, fehler, min: 0),
        initial_party: int_list(roh, 'initial_party', basis, fehler),
        start_map_id: string_field(roh, 'start_map_id', basis, fehler, allow_empty: true),
        start_x: int_field(roh, 'start_x', basis, fehler, min: 0),
        start_y: int_field(roh, 'start_y', basis, fehler, min: 0),
        graphics: graphics, audio: audio, sound_cues: sound_cues,
        vocabulary: vokabular, asset_catalog: katalog.dup,
        element_ids: listen[:elements].map(&:id),
        species_ids: listen[:species].map(&:id),
        switch_ids: listen[:switches].map(&:id),
        variable_ids: listen[:variables].map(&:id)
      )
    end

    # ------------------------------------------------------- Querverweise

    def validate_references(modelle, fehler, pfad)
      menge = {}
      (AREA_KEYS + SYSTEM_LIST_KEYS).each do |bereich|
        eintraege = bereich == :system ? [] : (modelle[bereich] || modelle.dig(:system_lists, bereich) || [])
        menge[bereich] = eintraege.map(&:id).to_set
      end

      modelle[:actors].each do |obj|
        basis = "#{pfad}/actors[#{obj.id}]"
        check_ref(basis, 'Klasse', obj.class_id, menge[:classes], fehler)
        obj.start_equipment.each do |ref|
          ziel = ref.slot == 'weapon' ? menge[:weapons] : menge[:armors]
          check_ref("#{basis}/start_equipment/#{ref.slot}", 'Ausrüstung', ref.item_id, ziel, fehler)
        end
      end

      modelle[:classes].each do |obj|
        basis = "#{pfad}/classes[#{obj.id}]"
        check_list(basis, 'Waffe', obj.weapon_ids, menge[:weapons], fehler)
        check_list(basis, 'Rüstung', obj.armor_ids, menge[:armors], fehler)
        check_map_keys(basis, 'Element', obj.element_multipliers, menge[:elements], fehler)
        check_map_keys(basis, 'Zustand', obj.state_multipliers, menge[:states], fehler)
        obj.learnings.each do |lernung|
          check_ref("#{basis}/learnings/#{lernung.level}", 'Fertigkeit', lernung.skill_id,
                    menge[:skills], fehler)
        end
      end

      modelle[:skills].each do |obj|
        basis = "#{pfad}/skills[#{obj.id}]"
        check_ref(basis, 'Animation 1', obj.animation1_id, menge[:animations], fehler)
        check_ref(basis, 'Animation 2', obj.animation2_id, menge[:animations], fehler)
        check_ref(basis, 'Common Event', obj.common_event_id, menge[:common_events], fehler)
        check_list(basis, 'Element', obj.element_ids, menge[:elements], fehler)
        check_list(basis, 'plus state', obj.plus_state_ids, menge[:states], fehler)
        check_list(basis, 'minus state', obj.minus_state_ids, menge[:states], fehler)
      end

      modelle[:items].each do |obj|
        basis = "#{pfad}/items[#{obj.id}]"
        check_ref(basis, 'Animation 1', obj.animation1_id, menge[:animations], fehler)
        check_ref(basis, 'Animation 2', obj.animation2_id, menge[:animations], fehler)
        check_ref(basis, 'Common Event', obj.common_event_id, menge[:common_events], fehler)
        check_list(basis, 'Element', obj.element_ids, menge[:elements], fehler)
        check_list(basis, 'plus state', obj.plus_state_ids, menge[:states], fehler)
        check_list(basis, 'minus state', obj.minus_state_ids, menge[:states], fehler)
      end

      modelle[:weapons].each do |obj|
        basis = "#{pfad}/weapons[#{obj.id}]"
        check_ref(basis, 'Animation 1', obj.animation1_id, menge[:animations], fehler)
        check_ref(basis, 'Animation 2', obj.animation2_id, menge[:animations], fehler)
        check_list(basis, 'Element', obj.element_ids, menge[:elements], fehler)
        check_list(basis, 'plus state', obj.plus_state_ids, menge[:states], fehler)
      end

      modelle[:armors].each do |obj|
        basis = "#{pfad}/armors[#{obj.id}]"
        check_ref(basis, 'Auto-State', obj.auto_state_id, menge[:states], fehler)
        check_map_keys(basis, 'Element', obj.element_resistances, menge[:elements], fehler)
        check_map_keys(basis, 'Zustand', obj.state_resistances, menge[:states], fehler)
      end

      modelle[:enemies].each do |obj|
        basis = "#{pfad}/enemies[#{obj.id}]"
        check_ref(basis, 'Animation 1', obj.animation1_id, menge[:animations], fehler)
        check_ref(basis, 'Animation 2', obj.animation2_id, menge[:animations], fehler)
        check_map_keys(basis, 'Element', obj.element_multipliers, menge[:elements], fehler)
        check_map_keys(basis, 'Zustand', obj.state_multipliers, menge[:states], fehler)
        obj.actions.each_with_index do |aktion, index|
          check_ref("#{basis}/actions[#{index}]", 'Fertigkeit', aktion.skill_id,
                    menge[:skills], fehler)
        end
        schatz = obj.treasure
        check_ref(basis, 'Item-Drop', schatz.item_id, menge[:items], fehler)
        check_ref(basis, 'Waffen-Drop', schatz.weapon_id, menge[:weapons], fehler)
        check_ref(basis, 'Rüstungs-Drop', schatz.armor_id, menge[:armors], fehler)
      end

      modelle[:troops].each do |obj|
        basis = "#{pfad}/troops[#{obj.id}]"
        obj.members.each_with_index do |member, index|
          check_ref("#{basis}/members[#{index}]", 'Gegner', member.enemy_id,
                    menge[:enemies], fehler)
        end
        obj.events.each_with_index do |ereignis, index|
          ereignis_basis = "#{basis}/events[#{index}]"
          check_ref(ereignis_basis, 'Schalter', ereignis.switch_id, menge[:switches], fehler)
          validate_commands(ereignis.commands, ereignis_basis, fehler, menge)
        end
      end

      modelle[:states].each do |obj|
        basis = "#{pfad}/states[#{obj.id}]"
        check_ref(basis, 'Animation', obj.animation_id, menge[:animations], fehler)
        check_list(basis, 'Schutzelement', obj.guard_element_ids, menge[:elements], fehler)
        check_list(basis, 'plus state', obj.plus_state_ids, menge[:states], fehler)
        check_list(basis, 'minus state', obj.minus_state_ids, menge[:states], fehler)
      end

      modelle[:animations].each do |obj|
        basis = "#{pfad}/animations[#{obj.id}]"
        if obj.frames.size != obj.frame_max
          problem(fehler, "#{basis}/frame_max",
                  "Frame-Anzahl #{obj.frame_max} passt nicht zu #{obj.frames.size} Frame-Daten")
        end
        obj.timings.each_with_index do |timing, index|
          if timing.frame >= obj.frame_max
            problem(fehler, "#{basis}/timings[#{index}]/frame",
                    "Frame #{timing.frame} liegt ausserhalb von 0…#{obj.frame_max - 1}")
          end
        end
      end

      modelle[:common_events].each do |obj|
        basis = "#{pfad}/common_events[#{obj.id}]"
        check_ref(basis, 'Bedingungsschalter', obj.condition_switch_id,
                  menge[:switches], fehler)
        validate_commands(obj.commands, basis, fehler, menge)
      end

      system = modelle[:system]
      system.initial_party.each do |id|
        check_ref("#{pfad}/system/initial_party", 'Startheld', id, menge[:actors], fehler)
      end
    end

    def check_ref(basis, label, id, erlaubt, fehler)
      return if id.nil? || id.to_i.zero?

      problem(fehler, basis, "#{label} #{id} existiert nicht") unless erlaubt.include?(id.to_i)
    end

    def check_list(basis, label, ids, erlaubt, fehler)
      ids.each { |id| check_ref(basis, label, id, erlaubt, fehler) }
    end

    def check_map_keys(basis, label, hash, erlaubt, fehler)
      hash.each_key { |id| check_ref(basis, label, id, erlaubt, fehler) }
    end

    def validate_commands(commands, basis, fehler, menge)
      commands.each_with_index do |befehl, index|
        pfad = "#{basis}/commands[#{index}]"
        params = befehl.params
        case befehl.code
        when 117
          check_ref(pfad, 'Common Event', params['common_event_id'], menge[:common_events], fehler)
        when 121
          check_ref(pfad, 'Schalter', params['switch_id'], menge[:switches], fehler)
        when 122
          check_ref(pfad, 'Variable', params['variable_id'], menge[:variables], fehler)
        end
      end
    end

    # ---------------------------------------------------------- Asset-Keys

    def validate_assets(roh, katalog, fehler, pfad)
      benutzt = []
      collect_asset_keys(roh.reject { |key, _| key == 'system' }, benutzt, pfad)
      system = roh['system'].is_a?(Hash) ? roh['system'] : {}
      system_ohne_katalog = system.reject do |key, _|
        %w[asset_catalog sound_cues audio].include?(key)
      end
      collect_asset_keys(system_ohne_katalog, benutzt, "#{pfad}/system")

      (system['audio'] || {}).each do |name, key|
        benutzt << [key, "#{pfad}/system/audio/#{name}"] if key.is_a?(String)
      end
      (system['sound_cues'] || {}).each do |name, key|
        benutzt << [key, "#{pfad}/system/sound_cues/#{name}"] if key.is_a?(String)
      end

      benutzt.each do |key, basis|
        naechster = key.to_s
        next if naechster.empty?
        unless katalog.key?(naechster)
          problem(fehler, basis, "unbekannter Asset-Key '#{naechster}'")
        end
      end
    end

    def collect_asset_keys(wert, treffer, pfad)
      case wert
      when Hash
        wert.each do |key, inghalt|
          if key.to_s.end_with?('_key') && inghalt.is_a?(String)
            treffer << [inghalt, "#{pfad}/#{key}"]
          end
          collect_asset_keys(inghalt, treffer, "#{pfad}/#{key}")
        end
      when Array
        wert.each_with_index { |eintrag, index| collect_asset_keys(eintrag, treffer, "#{pfad}[#{index}]") }
      end
    end

    # ------------------------------------------------------------- Abschluss

    def build_database(modelle, version)
      repositories = {}
      AREA_KEYS.each do |bereich|
        repositories[bereich] = Repositories::Repository.new(bereich, modelle[bereich])
      end
      SYSTEM_LIST_KEYS.each do |bereich|
        repositories[bereich] = Repositories::Repository.new(
          bereich, modelle.dig(:system_lists, bereich)
        )
      end
      Repositories::Database.new(version: version, repositories: repositories,
                                 system: modelle[:system])
    end
  end
end
