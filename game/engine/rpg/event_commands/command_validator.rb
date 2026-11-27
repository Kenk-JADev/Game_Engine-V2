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
    end
  end
end
