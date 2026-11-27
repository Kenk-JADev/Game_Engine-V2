#encoding: utf-8
# ==============================================================================
# Rpg::EventCommands::CommandValidator — Etappe 47.
#
# Prüft beim Laden von Maps und Common Events, ob ein Befehlsprogramm gültig ist:
#   * alle Switch-/Variable-/Map-/Item-/Skill-/Actor-/Enemy-/Troop-/Animation-/
#     CommonEvent-/Waffen-/Rüstungs-/Zustands-/Klassen-IDs existieren,
#   * alle Asset-Keys (bgms, bgss, ses, mes, pictures, faces, characters,
#     windowskins, transitions, ...) existieren,
#   * Block-Strukturen (ConditionalBranch then/else, ShowChoices choice_n/cancel,
#     Loop body) konsistente indent-Level besitzen und Sprungmarken auflösbar sind.
#
# Bewusst KEINE eigene Prüflogik: der Validator ist ein dünner, injizierbarer
# Adapter über den bestehenden Loader (eine Quelle der Wahrheit für Feldtypen,
# Bereiche, Referenzen, Asset-Keys, Blöcke und Indent). Er liefert strukturierte
# Befunde statt zu werfen, damit Editor/MapCheck sie anzeigen können.
#
#   befunde = CommandValidator.validate(commands, resources: index)
#   CommandValidator.valid?(commands, resources: index)   # => true/false
#   index   = CommandValidator.resources_from_database(db, assets: asset_index)
#
# Ohne UI. Dependency Injection über resources/assets bzw. file_system.
# ==============================================================================

module Rpg
  module EventCommands
    module CommandValidator
      VIRTUAL_PATH = 'command_validator.json'.freeze

      module_function

      # Liefert ein Array von Befund-Strings (leer = gültig).
      def validate(commands, resources:, version: 1, name: 'validate')
        dokument = {
          'version' => version,
          'resources' => resources,
          'programs' => [{ 'id' => 1, 'name' => name, 'commands' => commands }]
        }
        loader = Loader.new(file_system: Support::MemoryFileSystem.new(VIRTUAL_PATH => dokument))
        loader.load(VIRTUAL_PATH)
        []
      rescue Support::ValidationError => e
        e.issues
      end

      def valid?(commands, resources:, version: 1, name: 'validate')
        validate(commands, resources: resources, version: version, name: name).empty?
      end

      # Baut den Ressourcen-Index aus einer database.json-artigen Struktur.
      # db: Hash Sammlung => Liste von {id,name,key?}; assets: Hash Sammlung =>
      # Liste von Asset-Keys (Strings). Fehlende Sammlungen werden leer geführt,
      # damit der Loader alle Pflicht-Resourcenlisten sieht.
      def resources_from_database(db, assets: {})
        db ||= {}
        assets ||= {}
        Loader::RESOURCE_KEYS.each_with_object({}) do |schluessel, ergebnis|
          eintraege = db[schluessel] || db[schluessel.to_sym] ||
                      assets[schluessel] || assets[schluessel.to_sym] || []
          ergebnis[schluessel] = Array(eintraege).each_with_index.map do |eintrag, index|
            if eintrag.is_a?(Hash)
              id = eintrag['id'] || eintrag[:id] || (index + 1)
              key = eintrag['key'] || eintrag[:key] || "#{schluessel}/#{id}"
              { 'id' => id, 'key' => key.to_s, 'name' => (eintrag['name'] || eintrag[:name] || key).to_s }
            else
              { 'id' => index + 1, 'key' => eintrag.to_s, 'name' => eintrag.to_s }
            end
          end
        end
      end

      # Etappe 48: Adapter für das LEGACY-Format der Karten-/Common-Event-
      # Befehle ({"cmd":"item","name":"heiltrank",...}). Maps und Common Events
      # nutzen diese Form, nicht die typisierte Rpg-Form. Geprüft werden dieselben
      # Guarantees wie validate(): Ressourcen-IDs/Asset-Keys existieren und die
      # Block-Struktur (then/else/branches/commands/cancel_branch) ist konsistent.
      # Eine Referenz wird nur gemeldet, wenn die Sammlung geführt ist (nicht
      # leer) - so erzeugen fehlende Asset-Ordner keine Massenbefunde.
      LEGACY_REFS = {
        'switch' => [['name', 'switches']], 'toggle_switch' => [['name', 'switches']],
        'if_switch' => [['name', 'switches']],
        'variable' => [['name', 'variables']], 'if_variable' => [['name', 'variables']],
        'item' => [['name', 'items']], 'if_item' => [['name', 'items']],
        'actor_hp' => [['actor', 'actors']], 'actor_sp' => [['actor', 'actors']],
        'actor_exp' => [['actor', 'actors']], 'actor_level' => [['actor', 'actors']],
        'actor_param' => [['actor', 'actors']], 'actor_name' => [['actor', 'actors']],
        'state' => [['name', 'states'], ['actor', 'actors']],
        'actor_skill' => [['name', 'skills'], ['actor', 'actors']],
        'party_member' => [['name', 'actors']],
        'battle' => [['name', 'troops']], 'common_event' => [['name', 'common_events']],
        'sound' => [['name', 'ses']], 'play_bgm' => [['name', 'bgms']], 'play_bgs' => [['name', 'bgss']],
        'change_weapons' => [['weapon_id', 'weapons']], 'change_armors' => [['armor_id', 'armors']],
        'change_equipment' => [['actor_id', 'actors']],
        'change_actor_class' => [['actor_id', 'actors'], ['class_id', 'classes']],
        'change_actor_graphic' => [['actor_id', 'actors']],
        'change_enemy_state' => [['state_id', 'states']], 'enemy_transform' => [['enemy_id', 'enemies']],
        'change_battle_bgm' => [['bgm_key', 'bgms']], 'change_battle_end_me' => [['me_key', 'mes']],
        'change_windowskin' => [['skin_key', 'windowskins']],
        'prepare_transition' => [['transition_key', 'transitions']],
        'show_animation' => [['animation_key', 'animations']],
        'show_battle_animation' => [['animation_key', 'animations']]
      }.freeze

      BLOCK_KEYS = %w[then else commands cancel_branch].freeze

      def validate_legacy(commands, resources:, name: 'legacy')
        befunde = []
        legacy_liste(Array(commands), resources || {}, name, befunde, 0)
        befunde
      end

      def legacy_valid?(commands, resources:, name: 'legacy')
        validate_legacy(commands, resources: resources, name: name).empty?
      end

      def legacy_liste(liste, resources, wo, befunde, tiefe)
        if tiefe > 16
          befunde << "#{wo}: Block-Struktur zu tief verschachtelt (> 16)"
          return
        end
        liste.each_with_index do |c, i|
          next unless c.is_a?(Hash)

          w = "#{wo} Befehl ##{i + 1}"
          typ = c['cmd'].to_s
          # conditional_branch: artabhängige Referenz.
          if typ == 'conditional_branch'
            feld, sammlung = case c['kind'].to_s
                             when 'switch' then ['switch_id', 'switches']
                             when 'variable' then ['variable_id', 'variables']
                             when 'actor' then ['actor_id', 'actors']
                             when 'item' then ['item_id', 'items']
                             when 'weapon' then ['weapon_id', 'weapons']
                             when 'armor' then ['armor_id', 'armors']
                             end
            legacy_ref(c, feld, sammlung, resources, w, befunde) if feld
          end
          Array(LEGACY_REFS[typ]).each { |feld, sammlung| legacy_ref(c, feld, sammlung, resources, w, befunde) }
          # Block-Struktur: then/else/commands/cancel_branch sind Listen,
          # branches ist eine Liste von Listen.
          BLOCK_KEYS.each do |schluessel|
            next unless c.key?(schluessel)

            wert = c[schluessel]
            if wert.nil?
              next
            elsif wert.is_a?(Array)
              legacy_liste(wert, resources, "#{w} #{schluessel}", befunde, tiefe + 1)
            else
              befunde << "#{w}: '#{schluessel}' muss eine Befehlsliste sein"
            end
          end
          if c.key?('branches')
            branches = c['branches']
            if branches.is_a?(Array)
              branches.each_with_index do |zweig, j|
                if zweig.is_a?(Array)
                  legacy_liste(zweig, resources, "#{w} Zweig #{j}", befunde, tiefe + 1)
                else
                  befunde << "#{w}: Zweig #{j} muss eine Befehlsliste sein"
                end
              end
            else
              befunde << "#{w}: 'branches' muss eine Liste von Zweigen sein"
            end
          end
        end
      end

      def legacy_ref(c, feld, sammlung, resources, wo, befunde)
        wert = c[feld]
        return if wert.nil? || wert.to_s.strip.empty?

        eintraege = resources[sammlung] || resources[sammlung.to_sym] || []
        return if eintraege.empty? # Sammlung nicht geführt -> nicht prüfbar
        return if eintraege.any? { |e| referenz_treffer?(wert, e) }

        befunde << "#{wo}: #{sammlung}-Referenz '#{wert}' (#{feld}) nicht in der Datenbank"
      end

      def referenz_treffer?(wert, eintrag)
        ziel = wert.to_s.strip.downcase
        [eintrag['id'], eintrag['name'], eintrag['key'],
         eintrag['key'].to_s.split('/').last].any? { |k| k.to_s.strip.downcase == ziel }
      end
    end
  end
end
