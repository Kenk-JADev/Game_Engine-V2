# Aktualisiert docs/architecture.md um Etappe 18f
# (Event-Editor-Überarbeitung: Kategorien, neue Befehle, Parallel-Trigger).
# Aufruf: ruby tools/update_docs18f.rb
path = File.join(__dir__, '..', 'docs', 'architecture.md')
text = File.read(path, encoding: 'UTF-8')

anchor = '## Mal-Werkzeug für den Boden (Etappe 18e)'
raise 'Anker für 18f fehlt' unless text.include?(anchor)

section = <<~MD
  ## Event-Editor wie RPG Maker XP (Etappe 18f)

  Der Event-Editor (F6) ist deutlich näher an RPG Maker XP gerückt:

  **Befehlskategorien:** Die Befehls-ComboBox gruppiert jetzt 20 Typen
  mit Präfixen: [Nachricht] (Text, Wahlmöglichkeiten, Log),
  [Steuerung] (Warte, Schalter setzen/umschalten, Variable, Wenn
  Schalter/Variable, Script), [Spieler] (Teleport, Spieler bewegen,
  Heilen), [Welt] (Wetter, Effekt, Sound), [Party] (Item, Gold),
  [Spiel] (Speichern, Laden). Das Enum EvType ersetzt die früheren
  magischen Indexzahlen überall (Sicherheit bei 20 Typen).

  **Neue Befehle (Interpreter + Editor):**
  - choices: Wahlmöglichkeiten (RPG-Maker-Show-Choices). JSON:
    {"cmd":"choices","options":[...],"branches":[[...],[...]],
     "cancel":"aus"|"verzweigung"|n,"cancel_branch":[...]}.
    Der Interpreter blockiert (@choices), der Spieler wählt mit
    Pfeiltasten (wrap-around) + E/Enter; der gewählte Zweig kommt
    auf den Stack. cancel (Z5, XP "When Cancel"): abwesend/aus =
    Esc ignoriert (Default), "verzweigung" = eigener Abbruch-Zweig
    (XP 403, Sound ablehnen), Zahl N = Esc wie Option N;
    UI.choices zeigt bei erlaubtem Abbruch "Esc = Abbruch".
    Editor: Optionen als Zeilen im Textfeld, Zweige erscheinen als
    "Wenn [x]:"-Kopfzeilen im Baum (ev_flatten wie Dann/Sonst,
    "Wenn Abbruch:" wenn cancel=verzweigung); Zweige bleiben beim
    Bearbeiten erhalten (ev_build_command übernimmt alte branches).
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
  Tempo. Einschränkung: blockiert wie andere Events die NPC-Logik,
  solange es läuft (kein echter Hintergrund-Thread).

  **Layout:** Fenster 1000x730 (vorher 960x700), Gruppenboxen
  "Event-Eigenschaften" und "Befehle", grössere Befehlsliste
  (260 statt 248 px), breitere Typ-ComboBox (250 px für Kategorie-
  Präfixe), "Ziel:"-Anzeige versteht jetzt auch "Wenn [x]:"-Zweige.

  **Geprüft:** tools/event18f_test.rb (38 Checks: toggle, move,
  weather, save/load, choices mit Wrap/Verschachtelung und cancel/Z5,
    parallel im
  Datenmodell, unbekannter Befehl), log/live18f.ps1 (Editor: 20 Typen,
  Kategorie-Präfixe, choices einfügen mit Liste+JSON, parallel-
  Trigger, Speichern — Achtung: Event-Dialog blockiert Strg+S, deshalb
  erst schliessen). 13 Bestandstests + live18e/18d/18c grün, 8-s-
  Spielrun ohne Fehler.

MD

text = text.sub(anchor, section + anchor)
File.write(path, text, mode: 'wb')
puts 'architecture.md: 18f ergänzt'
