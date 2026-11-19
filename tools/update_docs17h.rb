# Aktualisiert docs/architecture.md um Etappe 17h
# (Wetter, Editor-Werkzeugleiste mit Icons, Zoom, Sound-Test, Playtest).
# Aufruf: ruby tools/update_docs17h.rb
path = File.join(__dir__, '..', 'docs', 'architecture.md')
text = File.read(path, encoding: 'UTF-8')

anchor = '## Datenbank: Helden, Items, Gold (Etappe 17g)'
raise 'Anker für 17h fehlt' unless text.include?(anchor)

section = <<~MD
  ## Wetter, Werkzeugleiste, Sound-Test, Playtest (Etappe 17h)

  **Wetter (Spiel):** Karten-Feld `"weather": {"type":"kein|regen|schnee",
  "intensity":1.0}`. Das Modul `Weather` (game/engine/weather.rb) hält ~40
  Partikel je Intensitätsstufe um den Spieler im Leben (Regen = schnelle
  blaue Straflen 22 m/s, Schnee = langsame weisse Flocken 2.6 m/s mit
  Drift). `Weather.load_map(map)` aktiviert es beim Kartenstart,
  `Weather.update(dt, px, py, pz)`/`Weather.draw` laufen in Game.update/draw.

  **Wetter zur Laufzeit umstellen:** Interpreter-Befehl
  `{"cmd":"weather","name":"schnee","intensity":1.5}` bzw. Script-Helfer
  `weather('regen', 2.0)` (ohne Argument: Typ lesen).

  **Wetter im Editor:** Karten-Eigenschaften (F2) füttern Combobox
  `kein/regen/schnee` (ID 4113) + Intensität (4114); OK schreibt das
  `weather`-Objekt ins Karten-JSON (live per WM_COMMAND getestet, siehe
  log/live17h.ps1).

  **Editor-Werkzeugleiste:** Toolbar unter dem Menü mit 8 Buttons
  (Speichern, Karten-Eigenschaften, Events, Scripts, Materialbase,
  Datenbank, Sound-Test, Playtest). Die Icons zeichnet der Editor zur
  Laufzeit in GDI (16x16, keine Ressourcen-Datei - wie Texturen/Sounds
  komplett prozedural). Buttons senden dieselben IDs wie das Menü.
  F-Tasten: F1 Hilfe, F2 Eigenschaften, F3 Materialbase, F4 Playtest,
  F6 Events, F8 Sound-Test, F9 Datenbank, F11 Scripts.

  **Zoom:** Mausrad im Viewport = Dolly entlang der Blickrichtung
  (Shift+Rad = Höhe ändern, Altverhalten).

  **Sound-Test (Editor):** Extras -> Sound-Test... (F8) öffnet den
  moduslosen Dialog `RuneEditorSoundDlg` (IDs 8101-8107): Liste der 7
  Spiel-Sounds, Abspielen mit Lautstärke 0.0-1.0, Musik als Loop
  an/aus. Der Editor initialisiert dafür die waveOut-Engine
  (core::sound) und meldet die Sounds per Ruby-Boot an
  (game/editor/boot.rb requires zusätzlich sounds.rb).

  **Playtest:** Extras -> Playtest starten... (F4) speichert die Karte
  und startet `build\\Rune3D.exe --project dorf` als eigenen Prozess.
  Das Spiel wertet `--project <name>` aus (main.cpp setzt
  RUNE_PROJECT), springt also ohne Titelbildschirm direkt ins Projekt
  (game/main.rb: ENV['RUNE_PROJECT'] in Game.setup). Ein unbekannter
  Projektname fällt auf den Titelbildschirm zurück (mit Warnung).

  **Geprüft:** `ruby -I game/engine tools/weather_test.rb` (22 Checks),
  alle 13 Tests grün, Live-Test log/live17h.ps1 (Toolbar, Zoom,
  Wetter-Dialog schreibt JSON, Sound-Dialog, Playtest-Prozess +
  engine.log-Vermerk, Aufräumen).

MD

text = text.sub(anchor, section + anchor)
File.write(path, text, mode: 'wb')
puts 'architecture.md: 17h ergänzt'
