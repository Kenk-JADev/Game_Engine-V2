# Backlog — Rune3D (Engine + Editor)

Regel: **Kein wiederholtes Test-Rumprobieren.** Tritt ein Fehler auf, der nicht
sofort in der aktuellen Etappe behoben wird (oder dessen Ursache unklar ist),
wird er hier eingetragen — mit Symptom, Vermutung und Datei/Zeile. Der Eintrag
wird erst abgearbeitet, wenn er zur aktuellen Etappe passt oder der User es
verlangt. Behobene Einträge wandern nach unten in "Erledigt" (mit Etappe).

Format pro Eintrag:

```
### [Bereich] Kurzitel — STATUS (offen/erledigt)
- Symptom:
- Vermutung/Analyse:
- Betroffene Stellen:
- Gefunden in: (Etappe/Datum)
```

Bereiche: `Engine`, `Editor`, `Ruby`, `Tests`, `Werkzeuge`, `Doku`.

---

## Offen
### [Editor] Große JSON-Ganzzahlen können beim Laden gerundet werden — offen (Daten-Treue)
- Symptom: Der C++-JSON-Parser speichert Zahlen als `double`; z. B. kann
  `9007199254740993` schon beim Laden zu `9007199254740992` werden und beim
  Speichern geändert zurückkommen, auch ohne absichtliche Feldänderung.
- Analyse: Dies betrifft grundsätzlich Karten/JSON, nicht nur Encounters.
  19m begrenzt neu eingegebene Encounter-Schrittzahlen auf 2^53−1 und
  überschreibt unberührte Werte nicht zusätzlich; der Parser selbst kann
  bestehende übergroße Ganzzahlen aber noch nicht verlustfrei halten.
- Betroffene Stellen: `src/editor/json.{h,cpp}`, `src/editor/encounter_form.h`,
  `tools/json_test.cpp`, `tools/json_bridge_check.rb`.
- Gefunden in: Etappe 19m.

### [Editor] Bearbeitete Block-Zonen verlieren Zusatzfelder — offen (Daten-Treue)
- Symptom: Beim tatsächlichen Editieren der F2-Blockzonenliste werden
  die Einträge aus `x,z,w,h` neu erzeugt; weitere Eigenschaften und sehr
  genaue Koordinaten können dabei verloren gehen.
- Analyse: 19m erhält die ursprüngliche Liste vollständig, solange das Feld
  nicht geändert wird; bei expliziter Bearbeitung fehlt noch eine Zuordnung
  der Textzeilen zu bestehenden JSON-Einträgen.
- Betroffene Stellen: `src/editor/map_props_form.h`, `src/editor/editor_main.cpp`.
- Gefunden in: Etappe 19m (ursprüngliches Feld aus 19k.2).

### [Engine] Kampf-Grundgerüst deckt noch nicht alles ab — offen (Feature-Fehlend)
- Stand 19i: Rundenkampf mit Angriff/Item/Flucht, Sieg/Niederlage/Exp/
  Gold/Drops, Gegner als 3D-Kästen mit HP-Balken.
- Es fehlen u. a.: Skills/Magie (MP bleibt ungenutzt), Zustände (Vergiftung,
  Schlaf ...), Gegner-KI jenseits „Angriff auf zufälligen Helden",
  Formationen/Positionen, Kampf-Hintergrundmusik/-Hintergrundbild,
  Kampftagebuch-Fenster zum Wegklicken. Zufallskämpfe beim Laufen sind
  seit 19l erledigt (siehe Erledigt-Eintrag 19l).
- Betroffene Stellen: `game/engine/battle.rb`, `docs/architecture.md` (19i).
- Gefunden in: Etappe 19i (Roadmap aus 19e, XP-Defaultscript als Referenz).
### [Engine] RGSS-Schicht deckt noch nicht alles ab — offen (Feature-Fehlend)
- Stand 19h: `$game_*` komplett inkl. `$game_temp`, `$data_actors/items/system`
  (Hashes mit String-IDs), `Sprite/Window/Viewport/Bitmap` (über die UI-
  Primitive), `$scene` mit `Graphics.scene_update`, `$game_player.passable?`
  echt (Kartengrenzen + Geländestufe) - siehe Erledigt-Eintrag 19h.
- Es fehlen weiterhin: `Tilemap`/`Plane` (3D-Terrain ersetzt Kachel-Ebenen),
  Bitmap aus Dateien laden (nur Farbe/Text-Merker), Viewport-Clipping,
  Sprite-Tonwerte (tone), Marshal-Format (bleibt JSON), Audio-Pitch,
  visuelle `Graphics.freeze/transition`;
  seit 19h.2 zusätzlich: Screen-Tonwerte/Flash/Shake ohne Renderer, Timer
  ohne HUD-Anzeige, change_graphic/opacity/blending-Schritte wirkungslos.
  `passable?` ist seit 19k.3 vollständig (Block-Zonen + Objekt-Kollision
  über `Passage`) - erledigt, siehe Erledigt-Eintrag 19k.3.
- Betroffene Stellen: `game/engine/rgss.rb`, `game/engine/graphics.rb`,
  `docs/architecture.md` (19e/19h).
- Gefunden in: Etappe 19e (User-Vorgabe „RGSS wie RPG Maker XP Online-Hilfe").
### [Editor] Platzier-Editor: komplexe NPC-Befehle und 3D-Picking — offen (Ausbau)
- Stand 19n: F5 bearbeitet NPCs, Schild-Events und Objekte mit X/Y/Z,
  Wegpunkten und einfachem Text. Zusätzliche NPC-Befehle bleiben erhalten,
  sind aber nicht vollständig in der GUI editierbar. NPCs/Schilder können
  noch nicht direkt im 3D-Viewport angeklickt/gezogen werden; F5 bietet
  stattdessen die letzte Cursorposition und numerische Felder.
- Y ist ein Bodenversatz für die Darstellung; NPC-Kollision und Event-
  Reichweite sind weiterhin XZ-basiert (keine vertikale Trennung).
- Betroffene Stellen: `src/editor/placement_dialog.h`, `game/engine/npc.rb`,
  `game/engine/event.rb`, `game/main.rb`.
- Gefunden in: Etappe 19n.

### [Engine] Parallele Events blockieren NPC-Logik — offen
- Symptom: Ein laufendes `parallel`-Event hält die NPC-Bewegung an, solange der Interpreter damit beschäftigt ist.
- Vermutung/Analyse: Single-Interpreter-Engine (Design seit 17d); kein Hintergrund-Thread vorgesehen.
- Betroffene Stellen: `game/main.rb` (Event-Schleifen), `game/engine/interpreter.rb`.
- Gefunden in: Etappe 18f (dokumentierte Einschränkung).

### [Engine] Splat-Böden ohne Weich-Übergang — offen
- Symptom: Jedes Terrain-Quad zeigt genau eine Textur; harter Rand zwischen Splats.
- Vermutung/Analyse: Quad = eine Atlas-Zelle; Weightmap/Blend wäre Neubau im Shader.
- Betroffene Stellen: `src/gfx` (Terrain-Shader), `Terrain::generate_splat`.
- Gefunden in: Etappe 18d.

### [Engine] `terrain.tile` wirkt im Splat-Pfad nicht — offen
- Symptom: Kachelgröße aus dem Tileset-Dialog gilt nur im Ein-Textur-Pfad.
- Vermutung/Analyse: Splat-UVs nutzen Atlas-Zellen mit fester Skalierung 1.
- Betroffene Stellen: `Renderer::update_atlas`, `Terrain::generate_splat`.
- Gefunden in: Etappe 18d.

### [Editor] Undo/Redo deckt nur Objekte ab — offen
- Symptom: Strg+Z/Y betrifft nur das `objects`-Array; Splat-Flächen, NPC-/Schild-Platzierungen, Event-/Seiten- und DB-Änderungen sind nicht rückholbar.
- Vermutung/Analyse: Undo-Stapel snapshottet nur `objects`; für Events/Splats fehlt ein zweiter Stapel oder ein gemeinsamer Karten-Snapshot.
- Betroffene Stellen: `src/editor/editor_main.cpp` (`push_undo`, Event-Dialog, Tileset-Dialog).
- Gefunden in: Etappen 17c/18d/18g.

### [Editor] Statusleisten-/Textinhalte per SendMessage von außen nicht lesbar — offen (Test-Wissen)
- Symptom: `SB_GETTEXTW` (WM_USER+13 = 0x40D, nicht 0x40E!) über Prozessgrenzen stürzt den Editor ab.
- Vermutung/Analyse: WM_USER+-Meldungen werden nicht gemarshallt; der Zeiger zeigt in den fremden Adressraum.
- Betroffene Stellen: Live-Tests dürfen Statusbar nicht lesen — nur Log-Einträge + Prozessleben nutzen.
- Gefunden in: Etappe 18a.

### [Editor] Drag&Drop-Import per Skript nicht injizierbar — offen (Test-Wissen)
- Symptom: WM_DROPFILES ist eine Shell-Operation und lässt sich nicht per SendMessage simulieren.
- Vermutung/Analyse: Nur manüller Test oder editor.log-Vermerk ("Drag&Drop: N Textur(en) importiert").
- Betroffene Stellen: `log/live18a.ps1`-Ansatz.
- Gefunden in: Etappe 18a.

### [Ruby] `vk_from_name` lückenhaft — offen
- Symptom: Einige Key-Namen (z. B. 'lwin') fehlen in der Tabelle der Ruby-Bindings.
- Vermutung/Analyse: Tabelle in `src/ruby/vm.cpp` einfach erweitern, wenn gebraucht.
- Betroffene Stellen: `src/ruby/vm.cpp`.
- Gefunden in: Handoff-Notiz.

### [Tests] F12-Screenshot nur per echter Eingabe (GetAsyncKeyState) — offen (Test-Wissen)
- Symptom: `PostMessage(WM_KEYDOWN, VK_F12)` löst keinen Screenshot aus; es entstehen keine `log/editor_shot_*.bmp`.
- Vermutung/Analyse: `handle_shortcuts()` fragt `GetAsyncKeyState(VK_F12)` ab — Nachrichten-Injektion ändert den Tastaturzustand nicht. Lösung: `[EdClick]::Key(0x7B)` aus `tools/edclick.psm1` (SendInput).
- Betroffene Stellen: `src/editor/editor_main.cpp` (`handle_shortcuts`), Live-Tests.
- Gefunden in: Etappe 18i.

### [Werkzeuge] PowerShell-Rewrites zerstören Umlaute in C++-Quellen — offen (Fallstrick)
- Symptom: `Get-Content -Raw` + `Set-Content -Encoding utf8` liest CP1252 → Doppelkodierung ("Ã¼") + BOM.
- Vermutung/Analyse: NIE PowerShell-String-Bastelei an Quellen; Reparatur per `tools/repair_mojibake.rb`, Scan per `tools/scan_mojibake_bytes.rb`.
- Betroffene Stellen: alle C++-Dateien mit Umlauten.
- Gefunden in: Etappe 18c.

### [Editor] Kein Weich-Übergang/Autotiles für Böden — offen (Feature-Fehlend)
- Symptom: Boden-Kanten hart; Autotile-Logik fehlt.
- Vermutung/Analyse: hängt am Splat-System (siehe oben).
- Betroffene Stellen: Terrain-Shader, Tileset-Dialog.
- Gefunden in: Handoff-Kandidatenliste.

### [Editor] Eigenschaften-Bereich: Leerraum unter „Übernehmen" ungenutzt — offen (UX)
- Symptom: Unter dem Übernehmen-Button bleiben ~50 px leer, während Objekt-Infos (Typ, Material-Herkunft, Anzahl Splats) fehlen.
- Vermutung/Analyse: Platz für ein Info-Label (Prefab-Typ + Materialstatus) oder Buttons „Objekt löschen"/„Kopieren".
- Betroffene Stellen: `src/editor/editor_main.cpp` (Gruppenbox `IDC_GRP_PROPS`, `layout_children`).
- Gefunden in: Etappe 18i (Screenshot-Review).

### [Editor] Spaltenbreiten nicht per Maus verstellbar — offen (UX)
- Symptom: Links/rechts wachsen nur automatisch; wer die Objektliste breiter will, kann sie nicht ziehen.
- Vermutung/Analyse: Splitter zwischen Viewport und Spalten; `left_w`/`right_w` würden zu Zustandswerten (Merken in `last_project.txt`-Nachbarschaft oder eigener Ini).
- Betroffene Stellen: `src/editor/editor_main.cpp` (`layout_children`), WM_LBUTTONDOWN im Hauptfenster.
- Gefunden in: Etappe 18i (Screenshot-Review).

### [Editor] „Auswählen"/„Drehen (R)" bei 1024 px knapp — offen (UX)
- Symptom: Bei Mindestbreite sind die zwei Spaltenbuttons 95 px breit; „Auswählen" passt nur knapp.
- Vermutung/Analyse: Entweder diese Zeilen ebenfalls vollbreit (kostet 36 px Höhe) oder Icons statt Text.
- Betroffene Stellen: `src/editor/editor_main.cpp` (`tool_row`-Aufrufe 0/1).
- Gefunden in: Etappe 18i.

### [Engine/Editor] MapCheck (Ruby) und issues_scan_map (C++) sind zwei Implementationen — offen (Sync-Pflicht, seit 19j maschinell bewacht)
- Symptom: Befehlsliste, Trigger, Vergleichsoperatoren und Bedingungen stehen
  zweimal im Code: `MapCheck::CMD_TYPES`/`TRIGGER`/`VERGLEICH` (Ruby, 19a) und
  `ISSUE_CMD_TYPES`/`EV_TYPES`/`EV_TRIGGERS`/`EV_OPS` (C++, 18f/18h).
- Vermutung/Analyse: Der Editor kann kein Ruby-Modul laden (eigenes Programm ohne
  Engine), das Spiel kein C++. Eine gemeinsame Quelle wäre eine JSON-Datei
  (z. B. `game/data/befehle.json`), die beide Seiten lesen — Kandidat für eine
  eigene Etappe, bis dahin gilt die Doppel-Pflege.
- Seit Etappe 19j bewacht `tools/sync_test.rb` die Doppel-Pflege maschinell:
  EV_TYPES/ISSUE_CMD_TYPES/EV_TYPE_LABELS/Enum EvType gegen
  MapCheck::CMD_TYPES, Trigger/Operatoren/Wetter/Self-Switches/Effekte und
  die when-Listen des Interpreters. Eine vergessene Seite kippt den Test.
- Betroffene Stellen: `game/engine/mapcheck.rb`, `src/editor/editor_main.cpp`
  (`ISSUE_CMD_TYPES`, `EV_TYPES`, `issues_scan_map`), `tools/sync_test.rb`,
  `game/engine/interpreter.rb`.
- Gefunden in: Etappe 19a.

### [Tests] Live-Tests (Fenster-Injektion) laufen nicht in CI — offen (Test-Wissen)
- Symptom: `tools/edclick.psm1` und `log/live*.ps1` brauchen ein echtes Fenster,
  eine GPU und D3D11; GitHub-Runner haben beides nicht.
- Vermutung/Analyse: Kein Fehler, eine Grenze. CI deckt Logik ab (Ruby-Tests,
  C++-JSON, Daten-Scan, Windows-Linken); grafisches Verhalten muss weiter am
  Rechner geprüft werden (F12-Screenshot, `edclick`).
- Betroffene Stellen: `.github/workflows/ci.yml`, `tools/edclick.psm1`.
- Gefunden in: Etappe 19a.

### [Editor] editor_main.cpp ist eine einzige Datei mit ~7200 Zeilen — offen (Struktur)
- Symptom: Fenster, Dialoge, Kartenmodell, Undo, Scan und Rendering in einer
  Datei; `lint_repo.rb` meldet sie als „Kandidat zum Aufteilen".
- Vermutung/Analyse: Aufteilen ist ohne lokalen Compiler riskant, seit 19a aber
  abgesichert (der Windows-Job kompiliert bei jedem Push). Sinnvolle Schnitte:
  `editor/dialogs_*.cpp`, `editor/map_model.cpp` (Objekte/Undo/JSON-Zugriff),
  `editor/issues_scan.cpp`. Dabei könnte der Karten-Scan auf dieselben Regeln
  wie `MapCheck` gezogen werden (siehe Sync-Pflicht oben).
- Betroffene Stellen: `src/editor/editor_main.cpp`, `build.ps1`
  (`$EditorExcludes`/`Get-ChildItem -Recurse` nimmt neue Dateien automatisch mit).
- Gefunden in: Etappe 19a (Lint-Info).

### [Doku] Keine Lizenz und kein Beitragshinweis im Repo — offen (Frage)
- Symptom: Es gibt `README.md` (19a), aber keine `LICENSE` und keine Regeln für
  Fremdbeiträge; im README steht deshalb auch nichts zur Weitergabe.
- Vermutung/Analyse: Entscheidung des Eigentümers nötig (privat behalten, MIT,
  GPL…). Solange nichts gewählt ist, bleibt alles beim Status quo.
- Betroffene Stellen: Repo-Wurzel, `README.md`.
- Gefunden in: Etappe 19a.

## Erledigt
### [Editor] NPC-Editor / Platzier-Tool mit X/Y/Z und Text — erledigt (19n)
- F5: NPCs, Schild-Events und Prefab-Objekte anlegen, bearbeiten und
  löschen; Wegpunkte, Text, Bodenversatz, letzte Cursorposition und
  validierte Eingaben. Änderungen werden transaktional übernommen.
- Laufzeit: Y-Bodenversatz für NPCs/Objekte, Schilder als Aktions-Events,
  NPCs per E und Mausklick ansprechbar; bestehende Befehle bleiben erhalten.
- Tests: C++-Formularmodell, NPC-/Prefab-/MapCheck-Tests und echte
  Game.update-Integration. Grafischer Windows-Live-Test steht noch aus;
  3D-Direktmanipulation und komplexe NPC-Befehle siehe Ausbau-Eintrag oben.
- Betroffene Stellen: `placement_form.h`, `placement_dialog.h`, `editor_main.cpp`,
  `game/engine/{npc,event,prefabs,mapcheck,editor}.rb`, `game/main.rb`.

### [Editor] Zufallskämpfe pro Karte bearbeiten — erledigt (19m)
- Wunsch: Truppen und mittlere Schrittzahl ohne JSON-Handarbeit über F2
  einstellen, mit Zugriff auf gespeicherte F9-Truppen.
- Umsetzung: F2-Auswahl mit Hinzufügen, F9 öffnen/Neu laden und frei
  editierbarer Zeilenliste; Mehrfacheinträge gewichten, leere Liste
  deaktiviert. Schrittzahl prüfen (leer = 30, sonst 1 bis 2^53−1).
  Unbekannte Truppen und nicht darstellbare Altwerte erfordern Bestätigung;
  ungültige Eingaben ändern die Karte nicht. OK übernimmt in den Speicher,
  Strg+S speichert. Kartenbaum/Projektwechsel sind während F2 gesperrt;
  ein offenes F9 muss vor dem Projektwechsel gespeichert und geschlossen
  werden, damit seine Projekt-Datenbank nicht versehentlich überschrieben wird.
  Unbearbeitete Encounter- und allgemeine Kartenfelder bleiben erhalten;
  `g.objects` wird nach Wurzeländerungen neu gebunden.
- Tests: `tools/json_test.cpp` prüft die portable Formularlogik,
  Demo-Datenbank, JSON-Roundtrip, Fehlerfälle und unveränderte Felder.
  Der Windows-Build läuft in CI; ein grafischer F2-Live-Test steht noch aus.
- Betroffene Stellen: `src/editor/{editor_main.cpp,encounter_form.h,
  map_props_form.h}`, `tools/json_test.cpp`, `README.md`.

### [Engine] Zufallskämpfe beim Laufen — erledigt (19l)
- Wunsch: eine Lücke des Kampf-Grundgerüsts (19i) schließen - Kämpfe
  gab es nur per Event-Befehl, Karten konnten keine Kämpfe auslösen.
- Umsetzung: optionales Karten-JSON `"encounters": {"troops": [...],
  "steps": 30}`; RGSS-Kartenwerte und XP-Würfel `rand(n)+rand(n)+1`.
  Boden-Schritte zählen herunter; bei 0 und freier Spielwelt startet ein
  zufälliger Kampf. Events/F4 haben Vorrang, der Editor zählt nicht,
  fehlgeschlagene Kämpfe werden erst beim nächsten Schritt erneut versucht.
  Kaltstart, F5 und Kartenwechsel würfeln korrekt; Karten-, Event-, RGSS-
  und Script-Teleports werden nicht als Schritt gezählt. MapCheck und der
  C++-Editor prüfen dieselben
  Strukturregeln; ungültige IDs werden übersprungen, ungültige Schrittzahlen
  fallen auf 30 zurück. Die Demo-Vorlage hat passende Truppen.
- Tests: `encounter_test.rb` testet Spielstart, Kartenwechsel, tatsächliche
  Kampfstarts und Sperren; `rgss_test.rb`, `mapcheck_test.rb` und der
  C++-Logiktest `json_test.cpp` prüfen Zähler und Datenvalidierung.
- Grenzen: keine Kampf-BGM/-Bilder, keine Gebiets-Tabellen wie XP.
  Bestehende Projekte werden nicht automatisch mit Gegnern/Truppen und
  Encounters versehen. Das F2-Editorfeld kam in Etappe 19m hinzu.
- Betroffene Stellen: `game/engine/{rgss,mapcheck,interpreter,scripts}.rb`, `game/main.rb`,
  `src/editor/{editor_main.cpp,encounter_check.h}`, `game/data/maps/dorf.json`,
  `tools/{encounter_test,rgss_test,mapcheck_test}.rb`, `tools/json_test.cpp`.

### [Engine/Editor] Passierbarkeit rund gemacht — erledigt (19k.3)
- Wunsch: die 19k.2-Restlücken schließen (NPCs/Routen liefen durch Zonen,
  `passable?` kannte keine Objekt-Kollision) - eine Quelle statt drei
  eigener Prüfungen.
- Umsetzung: `game/engine/passage.rb` (`Passage.zonen/zone?/hindernis?/
  blockiert?`), Physik `solid_at(x,y,z)` + Binding `Rune::Physics.
  solid_at?`, Tastpunkte über der Steiggrenze (0.45/1.2 bei kMaxStep 0.35);
  `passable?`/`$game_map.blocked?` delegieren an Passage; NPC stösst an und
  überspringt blockierte Wegpunkte; `Mover.bewege` bricht den Schritt an
  Wänden ab (auch Spieler-teleport); Start-in-Zone-Warnung in `enter_map`;
  MapCheck-Lage-Warnungen (Start/Event/NPC/Wegpunkt) + C++-Parität
  (`blocked_zone_at` im issues_scan_map).
- Tests: neu `tools/passage_test.rb`, plus Checks in rgss_test, npc_test,
  move_route_test, mapcheck_test.
- Grenzen: NPCs weichen nicht aus (kein Pfad um die Wand), Routen rutschen
  nicht an Wänden entlang, `hindernis?` tastet zwei feste Höhen.
- Betroffene Stellen: `game/engine/{passage,rgss,npc,mover,mapcheck,boot}.rb`,
  `game/main.rb`, `src/world/physics.{h,cpp}`, `src/ruby/vm.cpp`,
  `src/editor/editor_main.cpp`, `tools/*_test.rb`.

### [Engine/Editor] Passierbarkeits-System: Block-Zonen — erledigt (19k.2)
- Symptom (alt): Spieler läuft überall, wo kein `solid`-Objekt steht;
  Zonen/Flächen waren nicht blockierbar.
- Umsetzung: Karten-Schlüssel „blocked" ([{x,z,w,h}], Mitte/Größe in Metern)
  = unsichtbare Wände über alle Höhen; Physik (add_blocked/overlaps_any),
  Ruby-Bindings, enter_map-Wiring, `$game_map.blocked?` + `passable?`,
  MapCheck-Regeln + C++-Issues-Parität, Editor: F2-Feld „Block-Zonen" +
  rote Flach-Quader im Viewport. Tests: mapcheck_test (5), rgss_test (8).
- Rest: war „NPC-Pfade/Routen ignorieren Zonen, `passable?` ohne Objekt-
  Kollision" - beides seit 19k.3 erledigt (siehe Eintrag darüber).
- Betroffene Stellen: `src/world/physics.{h,cpp}`, `src/ruby/vm.cpp`,
  `game/main.rb`, `game/engine/{rgss,mapcheck}.rb`,
  `src/editor/editor_main.cpp`, `tools/{mapcheck_test,rgss_test}.rb`.

### [Editor] Datenbank-Dialog: Gegner/Truppen-Tab — erledigt (19k.1)
- Wunsch: Editor-Lücken schließen (User-Auswahl nach 19j); Gegner/Truppen
  (19i) waren bisher nur per JSON-Handarbeit editierbar.
- Umsetzung: vier Reiter (Helden/Items/Gegner/Truppen), feldgesteuertes
  Formular über DB_FIELDS_*-Tabellen (12 Gegner-Felder inkl. drop/drop_rate,
  Truppen-Gegnerliste komma-getrennt), Neu-Defaults je Reiter, unbekannte
  Schlüssel (color/desc/level/exp) bleiben erhalten; sync_test prüft die
  Feldtabellen gegen database.json.
- Betroffene Stellen: `src/editor/editor_main.cpp` (DB-Dialog-Abschnitt),
  `tools/sync_test.rb`, `README.md`.

### [Tests/Doku] Professionalisierungs-Runde: Sync-Test + README-Auffrischung — erledigt (19j)
- Wunsch (User): „Alles anschauen ob man es professionell bessern/einbauen
  kann" — Auswahl per Frage: Professionalisierungs-Runde.
- Umsetzung: `tools/sync_test.rb` (16 Checks) gleicht die Ruby/C++-Doppel-
  definitionen maschinell ab (Befehlstypen inkl. Enum-Reihenfolge und
  Labels, Trigger, Vergleichsoperatoren, Wetter, Self-Switches, Effekte,
  when-Listen des Interpreters, Datenbank-Dialogfelder gegen database.json,
  Gegner/Truppen-Referenzen); README auf den Stand 19d-19i gebracht
  (26 Befehle, Kampf/Routen/Zahleneingabe/Fade/RGSS in der XP-Tabelle,
  Engine-Beschreibung, Test-Tabelle).
- Betroffene Stellen: `tools/sync_test.rb`, `README.md`, `docs/BACKLOG.md`
  (Sync-Eintrag: „maschinell bewacht").

### [Engine/Editor] Zahleneingabe + Kampf-Grundgerüst — erledigt (19i)
- Wunsch: „Ja alles" auf die Roadmap (Bewegungsrouten/RGSS-Vertiefung/
  Zahleneingabe+Kampf), bestätigt per „Beides nacheinander"; Orientierung
  am User-Upload XP-Defaultscript (Window_InputNumber, Scene_Battle).
- Umsetzung: Befehle input_number (25) + battle (26) mit Interpreter-
  Blockade, UI.nummer_eingabe (Zifferntasten/Cursor/Esc-Abbruch),
  battle.rb (Battler, Befehls-/Item-/Ziel-/Aktionsphase, Gegnerzug,
  Sieg mit Exp/Gold/Drop, Niederlage mit gameover bzw. can_lose,
  Flucht 50 %), database.json enemies+troops (4 Gegner, 3 Truppen),
  Editor-Typen + C++-Issues-Scan, MapCheck-Regeln, Context-Helfer battle.
- Grenzen: eigener Offener Eintrag „Kampf-Grundgerüst deckt noch nicht
  alles ab".
- Betroffene Stellen: `game/engine/{battle,interpreter,ui,scripts,mapcheck,
  boot}.rb`, `game/main.rb`, `game/data/database.json`,
  `src/editor/editor_main.cpp`, `tools/{battle_test,script_test,
  mapcheck_test}.rb`.

### [Engine] RGSS an XP-Defaultscripts ausgerichtet — erledigt (19h.2)
- Wunsch (User): „XP-Defaultscript hochgeladen, dies sind die default codes,
  damit kannst du es vielleicht umstellen, besser für das RGSS-Coding."
- Umsetzung: $game_temp komplette XP-Feldliste, $game_system Timer +
  Audio-Delegation + Message-Optionen, $game_screen tone/flash/shake-Zustände
  + weather_type als XP-Zahl, $game_map.valid?, $game_player direction/steps/
  encounter_count, Window_Selectable, Mover mit XP-Schrittnamen/-Codes
  (Aliase für die 19g-Namen), turn_*-Schritte drehen NPC/Kamera echt,
  code-Parameter (switch_on/change_speed/script) inkl. Editor-Wert-3 und
  MapCheck-Warnung, gameover/to_title-Flags aus Game.update beachtet.
- Grenzen: tone/flash/shake werden nicht gemalt (kein Renderer dafür),
  change_graphic/opacity/blending u. a. 2D-Schalter sind No-Ops, Timer hat
  kein HUD - alles im offenen RGSS-Eintrag ergänzt.
- Betroffene Stellen: `game/engine/{rgss,graphics,mover,npc,interpreter,
  scripts,mapcheck}.rb`, `game/main.rb`, `src/editor/editor_main.cpp`,
  `tools/{move_route_test,rgss_test,graphics_test}.rb`.

### [Engine] RGSS-Vertiefung: Sprite/Window/$scene/$game_temp/$data_* — erledigt (19h)
- Wunsch: „Ja alles" auf die Roadmap (19g Bewegungsrouten, 19h RGSS-
  Vertiefung, 19i Zahleneingabe+Kampf); bestätigt per Auswahl „Beides
  nacheinander".
- Umsetzung: `game/engine/graphics.rb` (Bitmap/Sprite/Window/Viewport auf
  den UI-Primitiven, z-sortiertes `Graphics.draw` aus `Game.draw`),
  `$scene` + `Graphics.scene_update` (Scene#main pro Frame, Fehler werfen
  die Szene ab), `$game_temp`, `$data_*` aus database.json/system.json,
  `passable?` echt (Grenzen + 1.2-m-Geländestufe, XP-Richtungen),
  `RGSS.reset!` aus `enter_map` (Sprites/Szene flüchtig).
- Grenzen: bleiben im offenen Eintrag „RGSS-Schicht deckt noch nicht alles
  ab" (Tilemap/Plane, Bitmap-Dateien, Audio-Pitch, freeze/transition,
  Hindernis-Kollision).
- Betroffene Stellen: `game/engine/{graphics,rgss,boot}.rb`, `game/main.rb`,
  `tools/{graphics_test,rgss_test}.rb`.

### [Engine/Editor] Bewegungsrouten (XP Move Routes) — erledigt (19g)
- Wunsch: XP-Features einbauen (User, "Ja alles" auf die Roadmap
  Bewegungsrouten/RGSS-Vertiefung/Zahleneingabe+Kampf).
- Umsetzung: `game/engine/mover.rb` (Schritte up/down/left/right/zufall/
  spieler_nah/spieler_weg/warte, repeat, Routen-Ersetzung, Spieler gleitet
  per teleport, NPC `move_to` + Pfad-Pause), Interpreter-Befehl `move_route`
  (wait blockiert bis zum Ende, repeat+wait warnt statt zu blockieren),
  Editor-Befehlstyp ohne neue Controls, MapCheck-Prüfung, Context-Helfer
  `move_route` (RGSS-Leitlinie), `Mover.reset!` bei Kartenwechsel.
- Grenzen: keine Dreh-Schritte. (Kollisions-/Passierbarkeitsprüfung fehlt
  seit 19k.3 nicht mehr: `Mover.bewege` stoppt an Block-Zonen/Hindernissen.)
- Betroffene Stellen: `game/engine/{mover,npc,interpreter,scripts,boot}.rb`,
  `game/main.rb`, `game/engine/mapcheck.rb`, `src/editor/editor_main.cpp`,
  `tools/{move_route_test,script_test,mapcheck_test}.rb`.

### [CI/Build] Artefakt enthielt keine Laufzeit-Daten — erledigt (19f)
- Symptom (User-Konsole unter Downloads/Build): „Ruby-Fehler in
  game\editor\boot.rb: LoadError", „prefabs.json nicht gefunden",
  „Projekt nicht gefunden: dorf" - alle drei beim Start des Editors.
- Ursache: `Rune3D-Windows`-Artefakt lud nur EXEs/DLLs hoch; `find_root()`
  findet `game/` aber nur neben der EXE, eine Ebene höher oder im CWD.
- Fix: CI kopiert `game/` + `projekte/` nach `build/` und lädt beides mit
  hoch (Artefakt ist selbstlaufend, EXEs liegen neben den Daten). Zusätzlich
  melden Rune3D.exe und RuneEditor.exe beim Start einen klaren Hinweis,
  wenn unter der Wurzel kein `game/` liegt.
- Betroffene Stellen: `.github/workflows/ci.yml`, `src/main.cpp`,
  `src/editor/editor_main.cpp`.
- Gefunden in: User-Konsolenausgabe (Download des 19e-Artefakts).

### [Engine/Launcher] RGSS-Schicht + Ersteinrichtung ohne Projekt — erledigt (19e)
- Wunsch (User): die Engine soll wie RPG Maker XP per RGSS in Ruby nutzbar
  sein, damit Spieler später im Script-Editor eigenen Custom-Code schreiben;
  ausserdem: gibt es kein Projekt, muss der Spieler eines erstellen können.
- Umsetzung: `game/engine/rgss.rb` legt die XP-API über die Engine
  (`$game_system/$game_switches/$game_variables/$game_self_switches/
  $game_party/$game_player/$game_map/$game_screen`, `Graphics`, `Input`,
  `Audio`, `load_data/save_data`); `RGSS.setup` läuft aus `Game.setup`.
  Launcher: ohne Projekte führt ein Willkommensbildschirm (:first) in die
  Projekterstellung (Enter/N = Name tippen, D = Demo-Dorf, Esc = beenden);
  `Project.ensure_demo` wird nicht mehr automatisch beim Start aufgerufen.
  `$game_system.menu_disabled/save_disabled` werden von Menü (Esc) und
  `Game.save_game` beachtet. Context-Helfer `fade`/`self_switch` ergänzt.
- Offene Reste: eigener Offen-Eintrag „RGSS-Schicht deckt noch nicht alles ab".
- Betroffene Stellen: `game/engine/{rgss,launcher,scripts,fade,boot}.rb`,
  `game/main.rb`, `tools/{rgss_test,script_test}.rb`.

### [Engine/Editor] Self-Switches + Bildschirmblende (XP-Features) — erledigt (19d)
- Wunsch: RPG-Maker-XP-Funktionen ergänzen (User-Vorgabe nach Etappe 19c).
- Umsetzung: Self-Switches A-D pro Event (`GameState`, Seiten-Bedingung
  `self_switch`, Interpreter-Befehle `self_switch`/`if_self_switch`,
  Savegame-Mitnahme, Editor-Formular und Self-Switch-Combos im Seite-Kasten)
  sowie Bildschirmblende (`game/engine/fade.rb`, Interpreter-Befehl `fade`,
  Vollbild-Alpha über die UI-Schicht, Reset bei Kartenwechsel/Playtest-Ende).
- Betroffene Stellen: `game/engine/{state,event,interpreter,mapcheck,fade}.rb`,
  `game/main.rb`, `game/engine/boot.rb`, `src/editor/editor_main.cpp`,
  `tools/{self_switch_test,fade_test,mapcheck_test}.rb`.
- Folge-Etappen: weitere XP-Features (Bewegungsrouten, Zahleneingabe,
  Kampf-System …).

### [Editor] Objektliste zeigt nur einzeilige Kurztexte — erledigt (19c)
- Behoben mit dem ListView-Umbau in Etappe 19c: sechs Spalten (Objekt, X, Z,
  Drehung, Material) statt einzeiliger ListBox-Kurztexte.
- Ursprünglich gefunden in: Etappe 18i (Screenshot-Review).

### [Editor] Objektliste + Platzier-Vorschau — erledigt (19c)
- Symptom/Wunsch: einzeilige ListBox, Sofort-Einfügen ohne Vorschau; User wollte
  Listen-Auswahl, Geist-Vorschau am Cursor und Klick als Bestätigung.
- Fix: Objektliste als ListView mit sechs Spalten und Sortierung per Kopflick;
  Geist-Vorschau (eingefärbt, Raster- und drehungsgenau, `R` dreht in 15°);
  linke Spalte als drei Gruppen mit Radio-Modi; Kontextmenü für Viewport und
  Liste; `log/editor_objects.json` als Leseweg für Live-Tests.
- Betroffene Stellen: `src/editor/editor_main.cpp`, `tools/edclick.psm1`.
- Gefunden in: User-Rückmeldung nach Etappe 18k/18l.

### [Editor/Daten] Ersatzschreibung (ue/ae/oe) in Prosa + ANSI-Textaufrufe — erledigt (19b)
- Symptom: Prefab-Liste „Huette"/„Wegstueck", NPC „Waechter Mara", Dialogzeilen
  („… traegt der Weg …"), Launcher-Hilfe „Waehlen/Loeschen"; zugleich hätten
  echte Umlaute in Daten über die ANSI-Aufrufe (`SendMessageA` u. a.) als
  „HÃ¼tte" enden können.
- Fix: Prosa repo-weit auf echte Umlaute (maskiert: nur Kommentare/Strings/
  Doku-Fließtext, Identifier bleiben ASCII), JSON-Displaywerte surgical,
  24 Win32-Textaufrufe auf W-Varianten umgestellt, `build.ps1` mit UTF-8-BOM.
- Betroffene Stellen: 113 Dateien; Textpfad jetzt durchgängig UTF-8 → Wide.
- Gefunden in: User-Rückmeldung mit Editor-Screenshot (nach Etappe 19a).

### [Engine] `json_write` beschnitt Kommazahlen (`%.12g`) — erledigt (19a)
- Symptom: Beim Speichern im Editor wurden Zahlen still verkürzt:
  `yaw 3.141592653589793` → `3.14159265359`, Spielstandskoordinaten
  `2.0040341041749343e-05` → `2.00403410417e-05`. Kartenbreite `240.0` wäre als
  `2.4e+02` geschrieben worden, obwohl der Kommentar „wie Ruby
  JSON.pretty_generate" versprach.
- Fix: `write_number` sucht die kürzeste Präzision 1..17 mit `strtod`-Gegenprobe,
  schreibt Festkomma bei Exponent `[-4, 16)` (Ruby/Python-Verhalten), bildet
  `NaN`/`Unendlich` auf `null` ab (Datei bleibt lesbar, MapCheck meldet es) und
  stürzt bei Ganzzahlen außerhalb `long long` nicht mehr ab. Alle 13 JSON-Dateien
  des Repos sind damit bytegleich zu Rubys Ausgabe — geprüft von
  `tools/json_bridge_check.rb --strict-format` in CI, Belege in
  `tools/json_test.cpp` (88 Checks).
- Gefunden in: Etappe 19a (durch die neue Brückenprüfung).

### [Werkzeuge] `build.ps1 -Test` übergab Argumente mit Komma an g++ — erledigt (19a)
- Symptom: `ld.exe: cannot find '-std=c++20',` — der Linker suchte Dateien mit
  Komma im Namen.
- Ursache: Im Argument-Modus von PowerShell ist ein Komma kein Trenner, sondern
  bleibt am Token kleben. `& $Gcc '-a', '-b'` übergibt `"-a,"` und `"-b"`.
- Fix: Argumentlisten als Array bauen und mit `@` splatten (`$JsonTestFlags`,
  `$SuiteArgs`, `$BrueckeArgs`), wie `Build-Target` es mit `@CxxFlags`/`@Libs`
  schon immer tat.
- Gefunden in: Etappe 19a (erster CI-Lauf, Windows-Job).

### [Werkzeuge] Testliste stand nur als Fließtext im Handoff — erledigt (19a)
- Symptom: 16 Testnamen in einem Shell-Befehl im Handoff; jeder neue Test musste
  von Hand eingetragen werden, `tex_import_test.rb` kippte ohne gebaute EXE mit
  `raise` um.
- Fix: `tools/test_all.rb` findet Tests per Glob, kennt Voraussetzungen, meldet
  Übersprungene und liefert einen Exit-Code; in CI und über `build.ps1 -Test`.
- Gefunden in: Etappe 19a.

### [Doku] Gemischte Zeilenenden (12 Dateien CRLF, Rest LF) — erledigt (19a)
- Symptom: `.gitattributes` stand auf `* -text`, deshalb entschied der letzte
  Editor über CRLF/LF. Der Editor schreibt JSON mit LF — jede CRLF-Karte kippte
  beim Speichern komplett um (Diff-Flut, kein Review möglich).
- Fix: `.gitattributes` jetzt `* text=auto eol=lf`, CRLF nur für
  `*.ps1`/`*.psm1`/`*.manifest`; 20 Dateien umgestellt; `tools/lint_repo.rb`
  bewacht die Konvention in CI.
- Gefunden in: Etappe 19a.

### [Doku] Repo ohne README — erledigt (19a)
- Symptom: Kein Einstieg für neue Rechner/Mitwirkende; Bau- und Testbefehle
  standen nur im Handoff (mit festem Pfad `C:/Users/jusagu/Desktop/Engine`).
- Fix: `README.md` mit Zweck, RPG-Maker-XP-Vergleich (was da ist/was fehlt),
  Voraussetzungen, Bauen, Testen, CI-Jobs, Projektstruktur, Bedienung,
  Datenformat.
- Gefunden in: Etappe 19a.

### [Werkzeuge] `build.ps1` lief nur auf einem Rechner — erledigt (19a)
- Symptom: `$RubyRoot = 'C:\Ruby34-x64'`, `include\ruby-3.4.0` und
  `-lx64-ucrt-ruby340` waren hart kodiert — auf jedem anderen Rechner und in CI
  brach das Skript mit „Fehlt: …" ab.
- Fix: Suche über `-RubyRoot` / `RUNE_RUBY_ROOT` / `ruby.exe` im PATH
  (`RbConfig::CONFIG['prefix']`) / bekannte Pfade; Header, `libdir`,
  `LIBRUBYARG_SHARED` und `RUBY_SO_NAME` aus `RbConfig`, mit
  RubyInstaller-3.4-Rückfall. Neu: `-Test`. Belegt durch den Windows-Job der CI
  (Ruby unter `C:\hostedtoolcache\windows\Ruby\3.4.x\x64`, DevKit-Toolchain
  darunter in `msys64\ucrt64`, GCC 16.2, beide EXEs warnungsfrei gebaut).
- Gefunden in: Etappe 19a.

### [Engine] Spieler konnte die Karte verlassen — erledigt (Etappe 18l)
- Symptom: Physik ließ den Spieler über den Kartenrand ins Leere laufen.
- Fix: `Physics::update` klemmt die Spielerposition auf
  ±(Kartenbreite/2 − Radius) × ±(Kartenhöhe/2 − Radius);
  Terrain-Bau erfolgt seit 18l pro Kartenwechsel (`enter_map → build_terrain`),
  damit die Grenzen der geladenen Karte gelten.
- Gefunden in: User-Rückmeldung („grenze der Map kann man nicht verlassen“).


### [Editor] Abgeschnittene Undo/Redo-Buttons + Leerraum/Verschobene Spalten beim Resize — erledigt (18i)
- Symptom: Bei 1024×640 liefen Undo/Redo in die Statusleiste, Labels waren in 96-px-Buttons abgeschnitten; bei großen Fenstern blieb unten viel Leerraum, Spalten zu schmal.
- Ursache: `layout_children()` nutzte feste y-Koordinaten (`tb_h + 358 … tb_h + 538`) und feste Spaltenbreiten (214/258).
- Fix: Spaltenbreiten `clamp(W*16/100,214,280)` / `clamp(W*19/100,258,340)`, Werkzeugliste nimmt freien Platz, Bedienblock folgt darunter (246 px), Undo/Redo volle Breite, Statusleisten-Felder proportional.
- Behoben in: Etappe 18i.

### [Editor] Fenster-Icon-Crash (0xc0000005 in der Titelzeichnung) — erledigt (18a)
- Ursache: `CreateIconIndirect` braucht ECHT monochrome Maske (`CreateBitmap` 1bpp) + 32bpp-DIB mit Alpha=255; Farb-Bitmap als Maske ist undefiniert.
- Behoben in: Etappe 18a.

### [Engine] Editor startete ohne Ruby-PATH nicht (0xc0150002 / LoadError json) — erledigt (18e+)
- Ursache: fehlende DLLs + RUBYLIB; build.ps1 kopiert jetzt Ruby-DLLs + `ruby_builtin_dlls\`, winpthread statisch, `vm.cpp` setzt RUBYLIB.
- Behoben in: Etappe 18e+.

### [Engine] Splat-UVs schlug fehl, weil JSON.parse String-Schlüssel liefert — erledigt (18d)
- Ursache: `Rune::Gfx.terrain_splats` nimmt jetzt String- UND Symbol-Schlüssel.
- Behoben in: Etappe 18d.

### [Engine] Mal-Werkzeug radierte nicht im Live-Test — erledigt (18e)
- Ursache: MK_SHIFT = 0x4 (nicht 0x8 — 0x8 ist Strg).
- Behoben in: Etappe 18e.
