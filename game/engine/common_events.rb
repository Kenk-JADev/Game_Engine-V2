#encoding: utf-8
# ============================================================================
#  Gemeinsame Events (Etappe 20c, RPG Maker XP "Common Events", $data_common_events).
#
#  Datenhaltung: game/data/database.json
#
#    "common_events": [
#      { "id": 1, "name": "zeit_ansage", "trigger": "none",
#        "switch": "zeit_aktiv", "commands": [ { "cmd": "text", "lines": ["..."] } ] }
#    ]
#
#    name     - eindeutiger Name; der Aufruf-Befehl nennt keinen Index, sondern
#               diesen Namen (Rune3D-Adresse wie bei $data_skills).
#    trigger  - "none" (nur per Befehl, XP 117), "autorun" (läuft bei freiem
#               Interpreter neu, solange der Schalter an ist) oder "parallel"
#               (dito, aber gedacht als Dauerläufer).
#    switch   - optionaler Schalter, der autorun/parallel freigibt (XP
#               switch_id); fehlt er, läuft das Event immer.
#    commands - Befehlsliste wie bei einem Ereignis.
#
#  Grenze (bewusst, LOGIK AC.6): Die Engine hat EINEN Interpreter. "parallel"
#  startet deshalb nur, wenn der Interpreter frei ist - es läuft also zwischen
#  anderen Events, nicht wirklich gleichzeitig (dieselbe Grenze wie bei
#  parallelen Karten-Events, D3). autorun verhält sich wie XP-Autorun: solange
#  der Schalter an ist, startet das Event nach jedem Ende neu - ohne Schalter
#  dreht es also endlos (MapCheck warnt).
# ============================================================================

module CommonEvents
  TRIGGER = %w[none autorun parallel].freeze
  # Verschachtelungstiefe von Aufrufen (A ruft B ruft ... ). Tiefe statt
  # Zähler, damit ein Fehler in den Daten den Interpreter nicht aufhängt.
  MAX_TIEFE = 8

  class << self
    # Läuft dieses gemeinsame Event gerade (Trigger + Schalter)?
    def aktiv?(eintrag)
      trig = (eintrag['trigger'] || 'none').to_s
      return false unless TRIGGER.include?(trig) && trig != 'none'

      name = eintrag['switch']
      return true if name.nil? || name.to_s.empty?

      GameState.switch(name)
    end

    # Jeden Frame aufrufen, wenn der Interpreter frei ist. Startet höchstens
    # EIN gemeinsames Event pro Frame (wie XP: ein Interpreter).
    def tick(interp)
      return false if interp.nil? || interp.running?

      Database.common_events.each do |eintrag|
        next unless aktiv?(eintrag)

        cmds = eintrag['commands']
        next unless cmds.is_a?(Array) && !cmds.empty?

        Rune::Log.info("Gemeinsames Event '#{eintrag['name']}' startet " \
                       "(#{eintrag['trigger'] || 'none'})")
        interp.start(cmds, nil, common: eintrag['name'])
        return true
      end
      false
    end

    # Namen für Editor/MapCheck (auch bei fehlenden Daten leer).
    def namen
      Database.common_events.map { |e| e['name'].to_s }.reject(&:empty?)
    end
  end
end
