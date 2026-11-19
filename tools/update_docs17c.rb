# Aktualisiert docs/architecture.md um Etappe 17c (Maus-Werkzeuge + Undo/Redo).
# Aufruf: ruby tools/update_docs17c.rb
path = File.join(__dir__, '..', 'docs', 'architecture.md')
text = File.read(path, encoding: 'UTF-8')

anchor = "## Ruby-API (Stand Etappe 2)"
raise 'Anker fuer 17c fehlt' unless text.include?(anchor)

section = <<~MD
  ## Maus-Werkzeuge und Undo/Redo (Etappe 17c)

  **Verschieben-Werkzeug:** Der Button *Verschieben* (Checkbox, linke Spalte)
  schaltet den Drag-Modus. Ein Linksklick auf ein Objekt waehlt es aus und
  startet das Ziehen: `push_undo()` genau einmal beim Drag-Start, dann
  `SetCapture`. `WM_MOUSEMOVE` rechnet aus der Mausposition einen Strahl
  (fov 60 Grad, Aspect aus dem Viewport) und schneidet ihn mit der
  horizontalen Ebene `y = drag_y` (Terrainhoehe des Objekts beim Start);
  die neue X/Z-Position wird auf das Raster (`g.grid`, 1 m) gerundet und
  sofort in Eigenschaften-Feldern, Objektliste und Titel angezeigt.
  `WM_LBUTTONUP` beendet den Drag und gibt die Maus frei.

  **Undo/Redo:** Jede Mutation (`Setzen`, `Loeschen`, `Duplizieren`,
  `Eigenschaften uebernehmen`, Drag-Start) legt vorher einen
  JSON-Schnappschuss des kompletten `objects`-Arrays auf den Undo-Stapel
  (max. 50 Eintraege, Redo wird bei neuer Aktion geleert). `Strg+Z` /
  `Strg+Y` sowie die Buttons *Undo*/*Redo* tauschen Schnappschuesse
  zwischen den beiden Stapeln; nach dem Wiederherstellen werden Liste und
  Felder neu synchronisiert und die Karte als geaendert markiert.

  **Geprueft:** Objekt setzen -> Strg+Z entfernt es -> Strg+Y stellt es
  wieder her; Drag im Verschieben-Modus aendert X/Z rastergenau (Log:
  `Drag-Ende: idx=… x=-9.000000 z=-12.000000`); Strg+S schreibt die neue
  Position in die Karten-JSON.

MD

text = text.sub(anchor, section + anchor)
File.write(path, text, mode: 'wb')
puts 'architecture.md: 17c ergaenzt'
