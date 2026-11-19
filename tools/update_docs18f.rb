# Aktualisiert docs/architecture.md um Etappe 18f
# (Event-Editor-UEberarbeitung: Kategorien, neue Befehle, Parallel-Trigger).
# Aufruf: ruby tools/update_docs18f.rb
path = File.join(__dir__, '..', 'docs', 'architecture.md')
text = File.read(path, encoding: 'UTF-8')

anchor = '## Mal-Werkzeug für den Boden (Etappe 18e)'
raise 'Anker fuer 18f fehlt' unless text.include?(anchor)

section = <<~MD
  ## Event-Editor wie RPG Maker XP (Etappe 18f)

  Der Event-Editor (F6) ist deutlich naeher an RPG Maker XP gerueckt:

  **Befehlskategorien:** Die Befehls-ComboBox gruppiert jetzt 20 Typen
  mit Praefixen: [Nachricht] (Text, Wahlmöglichkeiten, Log),
  [Steuerung] (Warte, Schalter setzen/umschalten, Variable, Wenn
  Schalter/Variable, Script), [Spieler] (Teleport, Spieler bewegen,
  Heilen), [Welt] (Wetter, Effekt, Sound), [Party] (Item, Gold),
  [Spiel] (Speichern, Laden). Das Enum EvType ersetzt die frueheren
  magischen Indexzahlen ueberall (Sicherheit bei 20 Typen).

  **Neue Befehle (Interpreter + Editor):**
  - choices: Wahlmöglichkeiten (RPG-Maker-Show-Choices). JSON:
    {"cmd":"choices","options":[...],"branches":[[...],[...]]}.
    Der Interpreter blockiert (@choices), der Spieler waehlt mit
    Pfeiltasten (wrap-around) + E/Enter; der gewaehlte Zweig kommt
    auf den Stack. UI.choices zeichnet das Wahlfenster (Marker,
    Hinterlegung, "↑↓ + E"-Hinweis). Editor: Optionen als Zeilen im
    Textfeld, Zweige erscheinen als "Wenn [x]:"-Kopfzeilen im Baum
    (ev_flatten wie Dann/Sonst); Zweige bleiben beim Bearbeiten
    erhalten (ev_build_command uebernimmt alte branches).
  - toggle_switch: Schalter umschalten (GameState.toggle_switch).
  - move_player: Spieler bewegen, absolut oder relativ (Rune::Player
    .teleport + Game.gy).
  - weather: jetzt auch im Editor (war seit 17h nur im Interpreter).
  - save_game/load_game: Spielstand per Event (Game.save_game/
    load_game, Slot 1-3). Achtung: load_game baut die Karte neu auf
    und ersetzt damit den laufenden Interpreter (dokumentiert).

  **Trigger "parallel":** RPG-Maker-Parallel-Process. Das Event startet
  sich neu, sobald es fertig ist — aber nur, wenn der Interpreter frei
  ist (Single-Interpreter-Engine). Ein 'wait' im Event steuert das
  Tempo. Einschraenkung: blockiert wie andere Events die NPC-Logik,
  solange es laeuft (kein echter Hintergrund-Thread).

  **Layout:** Fenster 1000x730 (vorher 960x700), Gruppenboxen
  "Event-Eigenschaften" und "Befehle", groessere Befehlsliste
  (260 statt 248 px), breitere Typ-ComboBox (250 px fuer Kategorie-
  Praefixe), "Ziel:"-Anzeige versteht jetzt auch "Wenn [x]:"-Zweige.

  **Geprueft:** tools/event18f_test.rb (23 Checks: toggle, move,
  weather, save/load, choices mit Wrap/Verschachtelung, parallel im
  Datenmodell, unbekannter Befehl), log/live18f.ps1 (Editor: 20 Typen,
  Kategorie-Praefixe, choices einfuegen mit Liste+JSON, parallel-
  Trigger, Speichern — Achtung: Event-Dialog blockiert Strg+S, deshalb
  erst schliessen). 13 Bestandstests + live18e/18d/18c gruen, 8-s-
  Spielrun ohne Fehler.

MD

text = text.sub(anchor, section + anchor)
File.write(path, text, mode: 'wb')
puts 'architecture.md: 18f ergaenzt'
