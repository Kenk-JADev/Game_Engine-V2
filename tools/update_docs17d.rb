# Aktualisiert docs/architecture.md um Etappe 17d (Event-Editor).
# Aufruf: ruby tools/update_docs17d.rb
path = File.join(__dir__, '..', 'docs', 'architecture.md')
text = File.read(path, encoding: 'UTF-8')

anchor = "## Ruby-API (Stand Etappe 2)"
raise 'Anker für 17d fehlt' unless text.include?(anchor)

section = <<~MD
  ## Event-Editor (Etappe 17d)

  **Eigenes Fenster:** `Events -> Event-Editor...` (`ID_MENU_EVENTS = 107`)
  öffnet ein modusloses Top-Level-Fenster der Klasse
  `RuneEditorEventDlg` (kein modaler Loop, damit der D3D11-Flip-Model-
  Viewport weiterrendert). Beim Kartenwechsel wird es geschlossen, weil
  Zeiger in `map_root` ungültig werden (`load_map` ruft
  `close_event_editor()`).

  **Daten:** Der Dialog arbeitet direkt auf `g.map_root["events"]` und
  schreibt dieselben Schlüssel wie die Engine (`id`, `name`, `x`, `z`,
  `radius`, `trigger`, `commands`). Gespeichert wird unverändert mit
  `Strg+S` im Hauptfenster; Undo/Redo deckt weiterhin nur `objects` ab.

  **Aufbau:** links Event-Liste mit *Neu* (setzt das Event auf die
  aktuelle Mausposition im Viewport), *Löschen* und *Duplizieren*
  (neue ID, Name + " (Kopie)", X/Z + 1). Rechts die Event-Felder
  (Name, X, Z, Radius, Trigger `action`/`touch`/`auto`) mit
  *Event übernehmen*, darunter die Befehlsliste.

  **Befehlsliste:** Der JSON-Baum wird flach mit Einrückung angezeigt;
  `if_switch`/`if_variable` bekommen die Kopfzeilen `-> Dann:` und
  `-> Sonst:`. Ein Klick auf eine Kopfzeile bestimmt, wohin neue Befehle
  eingefügt werden (Wurzel, Dann- oder Sonst-Zweig); die Statuszeile im
  Dialog zeigt das Ziel. *Hoch*/*Runter* tauscht Nachbarn derselben
  Ebene, *Befehl löschen* entfernt inklusive Zweigen.

  **Formular:** Die Typauswahl bietet alle Interpreter-Kommandos:
  Text (mehrzeilig, eine Zeile pro Textfenster-Zeile), Warte, Schalter
  setzen, Variable (Wert und/oder Add), Wenn Schalter, Wenn Variable
  (Operator `==`, `!=`, `<`, `<=`, `>`, `>=`), Teleport (X, Z, Yaw),
  Log, Sound (Name, Lautstärke) und Effekt (Name, Höhe, Hexfarbe).
  *Einfügen* baut ein neues Kommando, *Befehl übernehmen* ersetzt das
  markierte; beim Ändern eines Wenn-Befehls bleiben Dann/Sonst erhalten,
  solange der Typ ein Wenn bleibt. Unbekannte Schlüssel anderer Events
  überleben das Speichern, weil nur die bearbeiteten Felder gesetzt
  werden.

  **Tastatur:** Solange das Event-Fenster den Fokus hat, sind die
  globalen Editor-Shortcuts (R, Entf, Strg+Z/Y, ...) abgeschaltet
  (`event_editor_focused()`), damit Tippen in Namens- und Textfeldern
  keine Objekte dreht oder löscht.

  **Geprüft:** Build und alle acht Ruby-Tests grün. Ein interaktiver
  Fenstertest war in der abgeschotteten Build-Umgebung nicht möglich
  (der Editor landet auf einem nicht sichtbaren Desktop; `EnumWindows`
  sieht nur das Konsolenfenster).

MD

text = text.sub(anchor, section + anchor)
File.write(path, text, mode: 'wb')
puts 'architecture.md: 17d ergänzt'
