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
### [Editor] Objektliste verbessen + Platzier-Vorschau statt Sofort-Einfügen — offen (User-Wunsch)
- Symptom: Objektliste zeigt nur einzeilige Kurztexte; Platzieren im Setzen-Modus
  fügt beim ersten Klick sofort ein — keine Vorschau des ausgewählten Objekts.
- Wunsch: Objekt in der Liste auswählen, Vorschau (Geist des Prefabs) folgt dem
  Cursor im Viewport, Klick bestätigt das Platzieren.
- Betroffene Stellen: `src/editor/editor_main.cpp` (Objektliste, `push_hover_marker`,
  `place_object`).
- Gefunden in: User-Rückmeldung nach Etappe 18k/18l.

### [Editor] NPC-Editor / Platzier-Tool für viele Dinge (x/y/z, Text) — offen (User-Wunsch)
- Symptom: NPCs sind im Editor gar nicht editierbar (nur im Karten-JSON);
  Text/Schild-Objekte fehlen als platzierbarer Typ.
- Wunsch: Editor-Werkzeug (Dialog) zum Platzieren vieler Dinge mit X/Y/Z-Feldern:
  NPCs (Name, Position, Wegpunkte), Text-Objekte, Objekte.
- Betroffene Stellen: `src/editor/editor_main.cpp` (neuer Dialog nötig),
  `projekte/*/data/maps/*.json` ("npcs"-Schlüssel existiert, Engine seit 17d).
- Gefunden in: User-Rückmeldung nach Etappe 18k/18l.


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
- Symptom: Strg+Z/Y betrifft nur das `objects`-Array; Splat-Flächen, Event-/Seiten- und DB-Änderungen sind nicht rückholbar.
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
- Vermutung/Analyse: Nur manueller Test oder editor.log-Vermerk ("Drag&Drop: N Textur(en) importiert").
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

### [Editor] Kein Passierbarkeits-System — offen (Feature-Fehlend, kein Bug)
- Symptom: Spieler läuft überall, wo kein `solid`-Objekt steht; Zonen/Flächen sind nicht blockierbar.
- Vermutung/Analyse: Kandidat für eigene Etappe (blockierte Zonen als Karten-Daten + Physics-Check).
- Betroffene Stellen: `src/world/physics.*`, Karten-JSON.
- Gefunden in: Handoff-Kandidatenliste.

### [Editor] Kein Weich-Übergang/Autotiles für Böden — offen (Feature-Fehlend)
- Symptom: Boden-Kanten hart; Autotile-Logik fehlt.
- Vermutung/Analyse: hängt am Splat-System (siehe oben).
- Betroffene Stellen: Terrain-Shader, Tileset-Dialog.
- Gefunden in: Handoff-Kandidatenliste.

### [Editor] Objektliste zeigt nur einzeilige Kurztexte — offen (UX)
- Symptom: Lange Objektnamen werden in der schmalen Liste abgeschnitten; kein Tooltip, keine Mehrzeiligkeit.
- Vermutung/Analyse: `LBS_HASSTRINGS`-ListBox ohne `LBS_OWNERDRAWFIXED`/Tooltip; Alternative: eigene Zeichnung (`LBS_OWNERDRAWVARIABLE`) oder Tooltip-Control auf Hover.
- Betroffene Stellen: `src/editor/editor_main.cpp` (`g.list_objs`, `refresh_object_list`).
- Gefunden in: Etappe 18i (Screenshot-Review).

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

## Erledigt
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
