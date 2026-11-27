# Dokumentiert den Fullbild-Fix (hbrBackground im Hauptfenster) als
# Nacharbeit zu 18j in docs/architecture.md.
# Aufruf: ruby tools/update_docs_fullscreen_fix.rb
path = File.join(__dir__, '..', 'docs', 'architecture.md')
text = File.read(path, encoding: 'UTF-8')

anchor = '## Kartenbaum wie RPG Maker XP (Etappe 18k)'
raise 'Anker fehlt' unless text.include?(anchor)

section = <<~MD
  ## Fullbild-Fix: schwarze Ränder beim Maximieren (Nacharbeit 18j)

  **Symptom (User-Report):** Im Vollbild/Maximieren blieben neu aufgedeckte
  Stellen des Hauptfensters schwarz bzw. zeigten alten Inhalt.

  **Ursache:** Die Fensterklasse `RuneEditorMain` hatte **keinen
  Hintergrund-Pinsel** (`hbrBackground == null`). Windows löscht den
  Fensterhintergrund ohne Pinsel nie - beim Vergrößern neu freigelegte
  Bereiche (Ränder zwischen den Controls) werden von keinem Control
  bemalt und zeigten Müll. Alle Dialog-Klassen des Editors hatten schon
  `COLOR_BTNFACE`, nur das Hauptfenster nicht.

  **Fix:** `wc.hbrBackground = GetSysColorBrush(COLOR_BTNFACE)` bei der
  Registrierung von `RuneEditorMain`.

  **Geprüft:** log/live_fullscreen.ps1 (9 Checks: SC_MAXIMIZE/
  SC_RESTORE programmatisch, Viewport-Log 937x828 -> 1300x937, Prozess
  lebt, kein [ERROR]). Visülle Verifikation durch User im Vordergrund-
  Fenster (Drossel-Falle aus 18j gilt nicht für echtes Maximieren).

MD

text = text.sub(anchor, section + anchor)
File.write(path, text, mode: 'wb')
puts 'architecture.md: Fullbild-Fix ergänzt'
