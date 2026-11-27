# Aktualisiert docs/architecture.md um Etappe 18b
# (Projekt-Auswahl im Editor: Dialog, CLI --project, Merker, Anlegen).
# Aufruf: ruby tools/update_docs18b.rb
path = File.join(__dir__, '..', 'docs', 'architecture.md')
text = File.read(path, encoding: 'UTF-8')

anchor = '## Fenster-Bedienkomfort (Etappe 18a)'
raise 'Anker für 18b fehlt' unless text.include?(anchor)

section = <<~MD
  ## Projekt-Auswahl im Editor (Etappe 18b)

  Der Editor war bisher fest auf `projekte/dorf` eingestellt. Jetzt:

  **Projekt-Dialog:** Datei -> Projekt wechseln... (ID_MENU_PROJECT 114)
  öffnet den moduslosen Dialog `RuneEditorProjectDlg` (IDs 4201-4205):
  Liste aller `projekte/*/data/system.json`-Projekte, Doppelklick oder
  "Öffnen" wechselt, Textfeld + "Anlegen" legt ein neues Projekt an.

  **Neues Projekt:** create_project() baut das komplette Gerüst:
  `projekte/<name>/{data/{maps,},saves}/`, `system.json` (Titel,
  Startpunkt auf die Startkarte), leere `scripts.json` und
  `database.json` (gold 0, leere actors/items), flache Startkarte
  `<name>.json` (240x240, ohne Berge). Projektname: Buchstaben, Zahlen,
  Unterstrich.

  **switch_project(name):** schliesst offene Dialoge (Zeiger werden
  ungültig), setzt project_dir/name zurück, leert Undo/Redo und
  Karte, lädt textures.json (mat_load) und scannt/maps/Menü neu;
  erste Karte wird geladen. Log: "Projekt gewechselt: <name> (N Karten)".

  **Start:** CLI `--project <name>` gewinnt; sonst Merker
  `log/last_project.txt` (schreibt switch_project); sonst "dorf".
  Ungültiges Projekt fällt auf "dorf" zurück (Log-Warnung).
  `--import-tex` kennt jetzt ebenfalls `--project <name>`.

  **Geprüft:** Live-Test log/live18b.ps1 (CLI-Start, Merker-Neustart,
  Rückfall, Dialog-Doppelklick-Wechsel, Anlegen mit Datei-Checks,
  Aufräumen; EnumWindows statt FindWindowW!), alle 13 Tests grün.

MD

text = text.sub(anchor, section + anchor)
File.write(path, text, mode: 'wb')
puts 'architecture.md: 18b ergänzt'
