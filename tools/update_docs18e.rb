# Aktualisiert docs/architecture.md um Etappe 18e
# (Mal-Werkzeug: Boden-Flächen per Maus im Viewport).
# Aufruf: ruby tools/update_docs18e.rb
path = File.join(__dir__, '..', 'docs', 'architecture.md')
text = File.read(path, encoding: 'UTF-8')

anchor = '## Splat-Böden (Etappe 18d)'
raise 'Anker für 18e fehlt' unless text.include?(anchor)

section = <<~MD
  ## Mal-Werkzeug für den Boden (Etappe 18e)

  Splat-Flächen lassen sich jetzt direkt **mit der Maus in den Viewport
  malen** (RPG-Maker-Feeling statt Zahlenfelder):

  **Werkzeug:** Radio-Button "Boden malen" im linken Panel
  (IDC_BTN_PAINT 3026, viertes Werkzeug neben Setzen/Auswählen/
  Verschieben). Linksklick im Viewport malt eine Fläche in
  Pinselgröße um den Terrain-Treffer, **Shift+Klick radiert** die
  oberste (späteste) Fläche an dieser Stelle. +/- (auch Zehnertastatur)
  ändert die Pinselgröße (1..64 m, Raster-gerundet), die Statuszeile
  zeigt Textur + Größe + Radierer-Hinweis. Pinseltextur = aktuelle
  Auswahl im Tileset-Dialog (F7); Wechseln dort übernimmt sie live in
  den Pinsel. Brush-Vorschau: flaches hellblaues Quadrat in Pinselgröße
  am Cursor (push_hover_marker).

  **Kernfunktion splat_edit():** zentriert x/z, snappt aufs Raster
  (g.grid), legt terrain.splats an bzw. hängt an; beim Radieren
  entfernt sie die letzte den Punkt überdeckende Fläche und räumt eine
  leere Liste komplett aus dem JSON (json_remove_key). Aufruf auch
  aus WM_LBUTTONDOWN des Viewports (Mal-Modus vor place_mode geprüft);
  danach regen_terrain() + ts_fill_splats() + dirty.

  **Fallstricke (für Live-Tests):** WM_LBUTTONDOWN auswerten: MK_SHIFT = 0x4
  (0x8 ist Strg!) - der erste Live-Test radierte nicht, weil der Test
  9 statt 5 als wParam schickte. Viewport ist ein KIND-Fenster:
  FindWindowExW unterhalb des Hauptfensters, nicht EnumWindows.

  **Geprüft:** Live-Test log/live18e.ps1 (Modus aktivieren, Texturwahl
  lauscht mit, gemalter Fleck + radiert in Log/JSON, +/-, Prozess
  lebt), live18d/live18c-Regression grün, 13 Tests grün, 8-s-Spielrun
  ohne Fehler.

MD

text = text.sub(anchor, section + anchor)
File.write(path, text, mode: 'wb')
puts 'architecture.md: 18e ergänzt'
