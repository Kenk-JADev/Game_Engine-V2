# Aktualisiert docs/architecture.md um Etappe 18b
# (Projekt-Auswahl im Editor: Dialog, CLI --project, Merker, Anlegen).
# Aufruf: ruby tools/update_docs18b.rb
path = File.join(__dir__, '..', 'docs', 'architecture.md')
text = File.read(path, encoding: 'UTF-8')

anchor = '## Fenster-Bedienkomfort (Etappe 18a)'
raise 'Anker fuer 18b fehlt' unless text.include?(anchor)

section = <<~MD
  ## Projekt-Auswahl im Editor (Etappe 18b)

  Der Editor war bisher fest auf `projekte/dorf` eingestellt. Jetzt:

  **Projekt-Dialog:** Datei -> Projekt wechseln... (ID_MENU_PROJECT 114)
  oeffnet den moduslosen Dialog `RuneEditorProjectDlg` (IDs 4201-4205):
  Liste aller `projekte/*/data/system.json`-Projekte, Doppelklick oder
  "Oeffnen" wechselt, Textfeld + "Anlegen" legt ein neues Projekt an.

  **Neues Projekt:** create_project() baut das komplette Geruest:
  `projekte/<name>/{data/{maps,},saves}/`, `system.json` (Titel,
  Startpunkt auf die Startkarte), leere `scripts.json` und
  `database.json` (gold 0, leere actors/items), flache Startkarte
  `<name>.json` (240x240, ohne Berge). Projektname: Buchstaben, Zahlen,
  Unterstrich.

  **switch_project(name):** schliesst offene Dialoge (Zeiger werden
  ungueltig), setzt project_dir/name zurueck, leert Undo/Redo und
  Karte, laedt textures.json (mat_load) und scannt/maps/Menue neu;
  erste Karte wird geladen. Log: "Projekt gewechselt: <name> (N Karten)".

  **Start:** CLI `--project <name>` gewinnt; sonst Merker
  `log/last_project.txt` (schreibt switch_project); sonst "dorf".
  Ungueltiges Projekt faellt auf "dorf" zurueck (Log-Warnung).
  `--import-tex` kennt jetzt ebenfalls `--project <name>`.

  **Geprueft:** Live-Test log/live18b.ps1 (CLI-Start, Merker-Neustart,
  Rueckfall, Dialog-Doppelklick-Wechsel, Anlegen mit Datei-Checks,
  Aufraeumen; EnumWindows statt FindWindowW!), alle 13 Tests gruen.

MD

text = text.sub(anchor, section + anchor)
File.write(path, text, mode: 'wb')
puts 'architecture.md: 18b ergaenzt'
