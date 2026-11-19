# Aktualisiert docs/architecture.md um Etappe 17e (Script-Editor + Projekt-Scripts).
# Aufruf: ruby tools/update_docs17e.rb
path = File.join(__dir__, '..', 'docs', 'architecture.md')
text = File.read(path, encoding: 'UTF-8')

anchor = "## Ruby-API (Stand Etappe 2)"
raise 'Anker fuer 17e fehlt' unless text.include?(anchor)

section = <<~MD
  ## Projekt-Scripts und Script-Editor (Etappe 17e)

  **Datei:** Jedes Projekt darf `data/scripts.json` enthalten:

  ```json
  { "scripts": [ { "name": "gruss", "code": "say ['Hallo!']" } ] }
  ```

  `Scripts.load_project` liest sie in `Game.setup` ein (also auch bei F5).
  Ein Event ruft ein Script mit `{"cmd":"script","name":"gruss"}` auf;
  `{"cmd":"script","code":"..."}` fuehrt Code direkt aus. Der Interpreter
  kennt dafuer das neue Kommando `script` und `Interpreter#say(lines)`.

  **Sandkasten:** Der Code laeuft in `Scripts::Context#instance_eval`, nicht
  im Interpreter. Helfer: `say` (oeffnet das Textfenster und blockiert wie
  `text`), `switch`, `set_switch`, `variable`, `set_variable`,
  `add_variable`, `teleport(x, z, yaw:)`, `sound`, `effect`, `log`,
  `player_position`, `game`. Fehler (auch Syntaxfehler) werden geloggt und
  brechen die restliche Kommandoliste nicht ab.

  **Editor:** `Scripts -> Script-Editor...` (`ID_MENU_SCRIPTS = 108`)
  oeffnet ein modusloses Fenster (`RuneEditorScriptDlg`) mit Script-Liste,
  Namensfeld und grossem Ruby-Codefeld (Consolas). *Neu*, *Loeschen*,
  *Uebernehmen*, *Speichern* (schreibt `projekte/<name>/data/scripts.json`)
  und *Neu laden*. Beim Wechsel wird der alte Eintrag automatisch
  uebernommen; die Statuszeile zeigt Zielpfad und Aenderungszustand.

  **Event-Editor:** Der Befehlstyp *Script* traegt den Script-Namen in
  `name`; die Befehlsliste zeigt `Script "gruss"`.

  **Demo:** `game/data/scripts.json` (Vorlage) und
  `projekte/dorf/data/scripts.json` enthalten `dorf_begruessung` und
  `kristalle_zaehlen` als Beispiele.

  **Geprueft:** `ruby -I game/engine tools/script_test.rb` (Schalter,
  Variablen, say-Blockade, fehlerhaftes Script, Inline-Code, effect und
  teleport) sowie alle acht bisherigen Tests gruen.

MD

text = text.sub(anchor, section + anchor)
File.write(path, text, mode: 'wb')
puts 'architecture.md: 17e ergaenzt'
