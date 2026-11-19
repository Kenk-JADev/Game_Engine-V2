# Aktualisiert docs/architecture.md um Etappe 18k
# (Kartenbaum: XP-Kartenliste links im Editor).
# Aufruf: ruby tools/update_docs18k.rb
path = File.join(__dir__, '..', 'docs', 'architecture.md')
text = File.read(path, encoding: 'UTF-8')

anchor = '## Resize-Rendering und lesbares Editor-Log (Etappe 18j)'
raise 'Anker fuer 18k fehlt' unless text.include?(anchor)

section = <<~MD
  ## Kartenbaum wie RPG Maker XP (Etappe 18k)

  Der Editor bekommt links oben eine **Kartenliste als Baum** (TreeView) -
  das zentrale Navigationselement des RPG Maker XP:

  **Aufbau:** Wurzel = Projektname, darunter alle Karten des Projekts
  (Item-LPARAM = Karten-Index). `maptree_fill()` pflegt den Baum parallel
  zum Karten-Menue (`rebuild_map_menu` ruft es), `load_map` synchronisiert
  ihn nach dem Laden; die aktuelle Karte ist markiert
  (`TVS_SHOWSELALWAYS`, sichtbar auch ohne Fokus).

  **Bedienung (wie XP):**
  - Einzelklick auf eine Karte laedt sie (die schmutzige Karte wird vorher
    gesichert wie beim Kartenwechsel im Menue).
  - Doppelklick oeffnet die Karten-Eigenschaften (F2-Dialog).
  - Rechtsklick: Kontextmenue Eigenschaften / Umbenennen / Neue Karte /
    Loeschen. Umbenennen startet das **Inplace-Edit** im Baum
    (TVS_EDITLABELS); der neue Name landet in `map_root["name"]` (nur die
    aktuell geladene Karte), die Karte wird als geaendert markiert -
    Strg+S speichert wie ueblich. Leerer Name verwirft das Edit.
  - Die Wurzel (Projekt) ist nicht umbenennbar (BEGINLABELEDIT blockiert).

  **Bedienblock:** Die linke Spalte ist jetzt zweigeteilt - oben der
  Kartenbaum (feste 150 px inkl. KARTEN-Label), darunter die Werkzeuge
  (18i-Logik unveraendert: Liste nimmt freien Platz, Block folgt).
  Mindestfensterhoehe bleibt 640 px; die Werkzeugliste wird dadurch etwas
  kuerzer (Viewort bleibt >= 80 px).

  **Shortcut-Schutz:** Waehrend des Label-Edits sind die globalen
  Shortcuts abgeschaltet (Entf wuerde sonst Objekte loeschen, R drehen) -
  `handle_shortcuts` prueft `TreeView_GetEditControl`, dasselbe Muster
  wie bei den moduslosen Dialogen.

  ** IDs:** IDC_MAPTREE 3027, IDC_LBL_MAPS 3028, Kontextmenue
  IDM_TREE_PROPS/NEW/RENAME/DELETE 3110-3113. TreeView-Klassen sind in
  InitCommonControlsEx ergaenzt (ICC_TREEVIEW_CLASSES).

  **Geprueft:** Live-Test log/live18k.ps1 (8 Checks: Control vorhanden,
  Wurzel + Karten vorhanden, Auswahl markiert, kein Edit offen, kein
  [ERROR], Prozess lebt), 16 headless Tests gruen. Umbenennen per
  Inplace-Edit ist im Live-Test nicht injizierbar (Edit-Control-Focus
  ueber Prozessgrenzen unzuverlaessig, bekannte Grenze wie 18a).

MD

text = text.sub(anchor, section + anchor)
File.write(path, text, mode: 'wb')
puts 'architecture.md: 18k ergaenzt'
