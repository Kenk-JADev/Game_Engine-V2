# Aktualisiert docs/architecture.md um Etappe 18a
# (DPI-Schaerfe, prozedurale Fenster-Icons, Mindestgroesse,
#  Drag&Drop-Texturimport, Tasten G/Esc/Enter, Doppelklick-Fokus).
# Aufruf: ruby tools/update_docs18a.rb
path = File.join(__dir__, '..', 'docs', 'architecture.md')
text = File.read(path, encoding: 'UTF-8')

anchor = '## Wetter, Werkzeugleiste, Sound-Test, Playtest (Etappe 17h)'
raise 'Anker fuer 18a fehlt' unless text.include?(anchor)

section = <<~MD
  ## Fenster-Bedienkomfort (Etappe 18a)

  **DPI-Schaerfe:** Beide Programme (Spiel und Editor) setzen beim Start
  `SetProcessDpiAwarenessContext(PER_MONITOR_AWARE_V2)` - die Darstellung
  bleibt auf skalierten Displays (125 %/150 %) scharf, statt von Windows
  unscharf hochskaliert zu werden.

  **Eigene Fenster-Icons:** Editor (gelber Runenstein) und Spiel (blauer
  Kristall) zeichnen ihr Icon zur Laufzeit prozedural in GDI (16x16 +
  32x32, kein Ressourcen-File - wie Toolbar-Icons und Texturen).
  Wichtig: `CreateIconIndirect` verlangt eine ECHT monochrome Maske
  (`CreateBitmap(1bpp)`) und ein 32bpp-DIB mit Alpha=255 - ein
  Farb-Bitmap als Maske ist undefiniert und brachte die Titelzeichnung
  zum Absturz (0xc0000005 im Live-Test gefunden und behoben).

  **Mindestfenster-Groesse:** WM_GETMINMAXINFO im Editor begrenzt das
  Fenster auf mindestens 1024x640 - das Layout bricht beim Verkleinern
  nicht mehr.

  **Drag&Drop-Texturimport:** PNG/BMP-Dateien koennen direkt ins
  Editor-Fenster gezogen werden (WM_DROPFILES + DragAcceptFiles); jede
  Datei laeuft durch den bestehenden mat_import_file (WIC-Dekodierung,
  .rgba + textures.json, Duplikatnamen '_2', '_3' ...). Import wird im
  editor.log protokolliert.

  **Tastatur:** G schaltet das Fang-Raster (1.0 / 0.5 / 0.25 / 0.01 m),
  Esc hebt die Auswahl auf, Enter uebernimmt die Eigenschaften-Felder.
  Alles wird ins editor.log geschrieben (Live-Test verifiziert darueber).

  **Doppelklick in der Objektliste** richtet die Kamera auf das Objekt
  (Position + Blickwinkel), Log: "Kamera auf Objekt #N gerichtet".

  **Statusleiste:** Feld 3 zeigt jetzt "Raster x.xx m | y FPS".

  **Grenze (fuer Live-Tests):** Statusleisten-/Textinhalte lassen sich
  per SendMessage von aussen NICHT lesen - WM_USER+-Meldungen werden
  ueber Prozessgrenzen nicht gemarshallt, der Zeiger zeigt in den
  fremden Adressraum (SB_GETTEXTW von aussen -> Absturz des Editors!).
  Live-Tests gehen deshalb nur ueber Log-Eintraege und Prozessleben.

  **Geprueft:** Live-Test log/live18a.ps1 (Fenster-Icon via WM_GETICON,
  Mindestgroesse via WM_GETMINMAXINFO, G/Doppelklick/Esc via Log +
  Prozessleben, stderr leer, editor.log fehlerfrei), alle 13 Tests
  gruen, Spiel-Smoke-Run 8 s ohne Fehler (engine.log).

MD

text = text.sub(anchor, section + anchor)
File.write(path, text, mode: 'wb')
puts 'architecture.md: 18a ergaenzt'
