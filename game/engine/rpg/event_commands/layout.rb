#encoding: utf-8
# ==============================================================================
# Rpg::EventCommands::Layout — der Bauplan der drei XP-Event-Command-Tabs.
#
# Diese Datei ist die einzige Quelle für Reihenfolge, Spalte, Abschnitt und
# Beschriftung. Sie enthält bewusst keine Logik und keine UI: sie beschreibt,
# wie eine Oberfläche aussehen muss, damit sie direkt gecodet werden kann.
#
#   Tab -> Spalte (:left/:right) -> Abschnitt -> Einträge in XP-Reihenfolge
#
# Der Katalog (catalog.rb) zieht Beschriftung und Position aus dieser Tabelle;
# Layout.pruefen! stellt sicher, dass beide nicht auseinanderlaufen.
# ==============================================================================

module Rpg
  module EventCommands
    module Layout
      Entry = Data.define(:type, :code, :xp_name, :label, :tab, :column, :section, :position) do
        prepend Rpg::Support::Immutable

        def initialize(type:, code:, xp_name:, label:, tab:, column:, section:, position:)
          super(type: type.to_s, code: Integer(code), xp_name: xp_name.to_s,
                label: label.to_s, tab: Integer(tab), column: column.to_sym,
                section: section.to_s, position: Integer(position))
        end
      end

      Section = Data.define(:key, :title, :tab, :column, :entries) do
        prepend Rpg::Support::Immutable

        def types = entries.map(&:type).freeze
      end

      Column = Data.define(:side, :tab, :sections) do
        prepend Rpg::Support::Immutable

        def entries = sections.flat_map(&:entries).freeze
        def types = entries.map(&:type).freeze
      end

      Tab = Data.define(:number, :title, :columns) do
        prepend Rpg::Support::Immutable

        def column(side) = columns.find { |spalte| spalte.side == side.to_sym }
        def left = column(:left)
        def right = column(:right)
        def entries = columns.flat_map(&:entries).freeze
        def types = entries.map(&:type).freeze
      end

      # Reihenfolge und Wortlaut folgen exakt den drei Event-Command-Tabs des
      # RPG Maker XP. Jeder Eintrag: [type, code, XP-Name, deutsche Beschriftung].
      PLAN = [
        {
          number: 1,
          title: 'Dialog, Flusskontrolle, Variablen & System',
          columns: [
            { side: :left, sections: [
              { key: :dialog, title: 'Dialog & Eingabe', entries: [
                ['show_text', 101, 'Show Text', 'Text anzeigen'],
                ['show_choices', 102, 'Show Choices', 'Auswahl anzeigen'],
                ['input_number', 103, 'Input Number', 'Zahl eingeben'],
                ['change_text_options', 104, 'Change Text Options', 'Textoptionen ändern'],
                ['button_input_processing', 105, 'Button Input Processing', 'Tastendruck abfragen'],
                ['wait', 106, 'Wait', 'Warten'],
                ['comment', 108, 'Comment', 'Kommentar']
              ] },
              { key: :flow, title: 'Flusskontrolle', entries: [
                ['conditional_branch', 111, 'Conditional Branch', 'Bedingte Verzweigung'],
                ['loop', 112, 'Loop', 'Schleife'],
                ['break_loop', 113, 'Break Loop', 'Schleife verlassen'],
                ['exit_event_processing', 115, 'Exit Event Processing', 'Eventverarbeitung beenden'],
                ['erase_event', 116, 'Erase Event', 'Event löschen'],
                ['call_common_event', 117, 'Call Common Event', 'Common Event aufrufen'],
                ['label', 118, 'Label', 'Sprungmarke setzen'],
                ['jump_to_label', 119, 'Jump to Label', 'Zu Sprungmarke springen']
              ] }
            ] },
            { side: :right, sections: [
              { key: :state, title: 'Zustand & Inventar', entries: [
                ['control_switches', 121, 'Control Switches', 'Schalter steuern'],
                ['control_variables', 122, 'Control Variables', 'Variablen steuern'],
                ['control_self_switch', 123, 'Control Self Switch', 'Lokalen Schalter steuern'],
                ['control_timer', 124, 'Control Timer', 'Timer steuern'],
                ['change_gold', 125, 'Change Gold', 'Gold ändern'],
                ['change_items', 126, 'Change Items', 'Items ändern'],
                ['change_weapons', 127, 'Change Weapons', 'Waffen ändern'],
                ['change_armors', 128, 'Change Armor', 'Rüstungen ändern'],
                ['change_party_member', 129, 'Change Party Member', 'Partymitglied ändern']
              ] },
              { key: :system, title: 'Laufzeit-System', entries: [
                ['change_windowskin', 131, 'Change Windowskin', 'Fenstergrafik ändern'],
                ['change_battle_bgm', 132, 'Change Battle BGM', 'Kampf-BGM ändern'],
                ['change_battle_end_me', 133, 'Change Battle End ME', 'Kampf-End-ME ändern'],
                ['change_save_access', 134, 'Change Save Access', 'Speichern erlauben/sperren'],
                ['change_menu_access', 135, 'Change Menu Access', 'Menü erlauben/sperren'],
                ['change_encounter', 136, 'Change Encounter', 'Begegnungen erlauben/sperren']
              ] }
            ] }
          ]
        },
        {
          number: 2,
          title: 'Karte, Bewegung, Bilder, Screen & Audio',
          columns: [
            { side: :left, sections: [
              { key: :map, title: 'Karte & Bewegung', entries: [
                ['transfer_player', 201, 'Transfer Player', 'Spieler versetzen'],
                ['set_event_location', 202, 'Set Event Location', 'Event-Position setzen'],
                ['scroll_map', 203, 'Scroll Map', 'Karte scrollen'],
                ['change_map_settings', 204, 'Change Map Settings', 'Karteneinstellungen ändern'],
                ['change_fog_color_tone', 205, 'Change Fog Color Tone', 'Nebel-Farbton ändern'],
                ['change_fog_opacity', 206, 'Change Fog Opacity', 'Nebel-Deckkraft ändern'],
                ['show_animation', 207, 'Show Animation', 'Animation zeigen'],
                ['change_transparent_flag', 208, 'Change Transparent Flag', 'Transparenz setzen'],
                ['set_move_route', 209, 'Set Move Route', 'Bewegungsroute setzen'],
                ['wait_for_move_completion', 210, "Wait for Move's Completion", 'Auf Bewegung warten']
              ] },
              { key: :screen, title: 'Übergang & Bildschirm', entries: [
                ['prepare_transition', 221, 'Prepare for Transition', 'Übergang vorbereiten'],
                ['execute_transition', 222, 'Execute Transition', 'Übergang ausführen'],
                ['change_screen_tone', 223, 'Change Screen Color Tone', 'Bildschirm-Farbton ändern'],
                ['screen_flash', 224, 'Screen Flash', 'Bildschirm-Blitz'],
                ['screen_shake', 225, 'Screen Shake', 'Bildschirm wackeln']
              ] }
            ] },
            { side: :right, sections: [
              { key: :picture, title: 'Bilder & Wetter', entries: [
                ['show_picture', 231, 'Show Picture', 'Bild zeigen'],
                ['move_picture', 232, 'Move Picture', 'Bild bewegen'],
                ['rotate_picture', 233, 'Rotate Picture', 'Bild drehen'],
                ['change_picture_tone', 234, 'Change Picture Color Tone', 'Bild-Farbton ändern'],
                ['erase_picture', 235, 'Erase Picture', 'Bild löschen'],
                ['set_weather_effects', 236, 'Set Weather Effects', 'Wetter setzen']
              ] },
              { key: :audio, title: 'Audio', entries: [
                ['play_bgm', 241, 'Play BGM', 'BGM abspielen'],
                ['fade_out_bgm', 242, 'Fade Out BGM', 'BGM ausblenden'],
                ['play_bgs', 245, 'Play BGS', 'BGS abspielen'],
                ['fade_out_bgs', 246, 'Fade Out BGS', 'BGS ausblenden'],
                ['memorize_bgm_bgs', 247, 'Memorize BGM/BGS', 'BGM/BGS merken'],
                ['restore_bgm_bgs', 248, 'Restore BGM/BGS', 'BGM/BGS wiederherstellen'],
                ['play_me', 249, 'Play ME', 'ME abspielen'],
                ['play_se', 250, 'Play SE', 'SE abspielen'],
                ['stop_se', 251, 'Stop SE', 'SE stoppen']
              ] }
            ] }
          ]
        },
        {
          number: 3,
          title: 'Kampf, Charakterwerte, Gegner, Screens & Script',
          columns: [
            { side: :left, sections: [
              { key: :battle, title: 'Kampf & Interaktion', entries: [
                ['battle_processing', 301, 'Battle Processing', 'Kampf verarbeiten'],
                ['shop_processing', 302, 'Shop Processing', 'Laden verarbeiten'],
                ['name_input_processing', 303, 'Name Input Processing', 'Namenseingabe']
              ] },
              { key: :actor, title: 'Party & Charaktere', entries: [
                ['change_hp', 311, 'Change HP', 'HP ändern'],
                ['change_sp', 312, 'Change SP', 'SP ändern'],
                ['change_state', 313, 'Change State', 'Zustand ändern'],
                ['recover_all', 314, 'Recover All', 'Vollständig heilen'],
                ['change_exp', 315, 'Change EXP', 'Erfahrung ändern'],
                ['change_level', 316, 'Change Level', 'Stufe ändern'],
                ['change_parameters', 317, 'Change Parameters', 'Parameter ändern'],
                ['change_skills', 318, 'Change Skills', 'Fertigkeiten ändern'],
                ['change_equipment', 319, 'Change Equipment', 'Ausrüstung ändern'],
                ['change_actor_name', 320, 'Change Actor Name', 'Heldennamen ändern'],
                ['change_actor_class', 321, 'Change Actor Class', 'Klasse ändern'],
                ['change_actor_graphic', 322, 'Change Actor Graphic', 'Grafik ändern']
              ] }
            ] },
            { side: :right, sections: [
              { key: :enemy, title: 'Gegner & Kampfsteuerung', entries: [
                ['change_enemy_hp', 331, 'Change Enemy HP', 'Gegner-HP ändern'],
                ['change_enemy_sp', 332, 'Change Enemy SP', 'Gegner-SP ändern'],
                ['change_enemy_state', 333, 'Change Enemy State', 'Gegner-Zustand ändern'],
                ['enemy_recover_all', 334, 'Enemy Recover All', 'Gegner vollständig heilen'],
                ['enemy_appearance', 335, 'Enemy Appearance', 'Gegner erscheinen lassen'],
                ['enemy_transform', 336, 'Enemy Transform', 'Gegner verwandeln'],
                ['show_battle_animation', 337, 'Show Battle Animation', 'Kampfanimation zeigen'],
                ['deal_damage', 338, 'Deal Damage', 'Schaden austeilen'],
                ['force_action', 339, 'Force Action', 'Aktion erzwingen'],
                ['abort_battle', 340, 'Abort Battle', 'Kampf abbrechen']
              ] },
              { key: :system, title: 'System-Screens & Hook', entries: [
                ['call_menu_screen', 351, 'Call Menu Screen', 'Menü aufrufen'],
                ['call_save_screen', 352, 'Call Save Screen', 'Speichern-Bildschirm aufrufen'],
                ['game_over', 353, 'Game Over', 'Game Over'],
                ['return_to_title_screen', 354, 'Return to Title Screen', 'Zum Titel zurück'],
                ['script', 355, 'Script', 'Script-Hook']
              ] }
            ] }
          ]
        }
      ].freeze

      TABS = PLAN.map do |tab|
        Tab.new(
          number: tab[:number],
          title: tab[:title],
          columns: tab[:columns].map do |spalte|
            Column.new(
              side: spalte[:side],
              tab: tab[:number],
              sections: spalte[:sections].map do |abschnitt|
                Section.new(
                  key: abschnitt[:key].to_s,
                  title: abschnitt[:title],
                  tab: tab[:number],
                  column: spalte[:side],
                  entries: abschnitt[:entries].each_with_index.map do |(type, code, xp_name, label), index|
                    Entry.new(type: type, code: code, xp_name: xp_name, label: label,
                              tab: tab[:number], column: spalte[:side],
                              section: abschnitt[:key], position: index)
                  end.freeze
                )
              end.freeze
            )
          end.freeze
        )
      end.freeze

      ENTRIES = TABS.flat_map(&:entries).freeze
      BY_TYPE = ENTRIES.to_h { |eintrag| [eintrag.type, eintrag] }.freeze

      module_function

      def tabs = TABS
      def tab(number) = TABS.find { |eintrag| eintrag.number == number.to_i }
      def entries = ENTRIES
      def types = ENTRIES.map(&:type).freeze
      def entry(type) = BY_TYPE[type.to_s]
      def label(type) = BY_TYPE[type.to_s]&.label
      def xp_name(type) = BY_TYPE[type.to_s]&.xp_name
      def sections = TABS.flat_map { |tab_| tab_.columns.flat_map(&:sections) }.freeze

      # Der Katalog muss genau diese Befehle in genau dieser Aufteilung führen.
      # Abweichungen sind ein Programmfehler, kein Datenfehler: deshalb raise.
      def pruefen!(katalog)
        katalog_typen = katalog.definitions.map(&:type)
        fehlend = types - katalog_typen
        zu_viel = katalog_typen - types
        raise ArgumentError, "Layout fehlen im Katalog: #{fehlend.join(', ')}" if fehlend.any?
        raise ArgumentError, "Katalog ohne Layout-Eintrag: #{zu_viel.join(', ')}" if zu_viel.any?

        katalog.definitions.each do |definition|
          eintrag = BY_TYPE.fetch(definition.type)
          next if definition.tab == eintrag.tab && definition.column == eintrag.column &&
                  definition.group == eintrag.section && definition.code == eintrag.code

          raise ArgumentError,
                "#{definition.type}: Katalog (Tab #{definition.tab}/#{definition.column}/" \
                "#{definition.group}/#{definition.code}) widerspricht dem XP-Layout " \
                "(Tab #{eintrag.tab}/#{eintrag.column}/#{eintrag.section}/#{eintrag.code})"
        end
        true
      end

      # Textbaum für Dokumentation und Kopflos-Prüfung — kein UI-Code, aber
      # die Struktur, die eine Oberfläche 1:1 übernehmen kann.
      def als_text
        zeilen = []
        TABS.each do |tab_|
          zeilen << "Tab #{tab_.number} — #{tab_.title}"
          tab_.columns.each do |spalte|
            zeilen << "  #{spalte.side == :left ? 'Linke' : 'Rechte'} Spalte"
            spalte.sections.each do |abschnitt|
              zeilen << "    #{abschnitt.title}"
              abschnitt.entries.each do |eintrag|
                zeilen << format('      %3d  %-26s %s (%s)', eintrag.code, eintrag.type,
                                 eintrag.label, eintrag.xp_name)
              end
            end
          end
        end
        zeilen.join("\n")
      end
    end
  end
end
