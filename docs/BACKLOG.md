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
### [Engine] Level-Up ohne Klassentabellen — bewusste Grenze (Runde 22)
- Symptom: Stufenaufstieg wächst prozentual (8 % HP/MP, 5 % je
  Angriffswert), XP rechnet dagegen aus `RPG::Class`-Kurven je Stufe.
  HP/MP werden dabei wie in XP nicht geheilt (nur das Maximum steigt).
- Vermutung/Analyse: Unsere `database.json` kennt nur Startwerte
  (`hp/mp/atk/def/spi/agi`), keine Klassen-/Parameterkurven; eine
  Kurventabelle wäre ein neues Datenformat (Editor-F9-Reiter).
- Betroffene Stellen: `game/engine/party.rb` (`level_up!`, `exp_for_level`),
  `game/data/database.json`, Editor-Datenbankdialog.
- Gefunden in: Runde 22 (D4 umgesetzt, Wachstum bewusst vereinfacht).
- Zweite bewusste Abweichung: Rune3D speichert **Gesamt-Exp** (alte
  Spielstände bleiben gültig), XP den Rest-Exp je Stufe; Schwelle ist
  `exp_for_level(level+1)`.

### [Editor] Issue-Scan prüft `if_self_switch`-Zweige nicht — erledigt (Runde 20)
- Symptom: Fehler in `then`/`else` von `if_self_switch` erscheinen im
  Editor-Issues-Dialog (F10) nicht, obwohl MapCheck sie findet.
- Vermutung/Analyse: `issues_scan_commands` in `editor_main.cpp`
  rekurtiert nur bei `if_switch`/`if_variable` (MapCheck tut es bei
  allen drei Typen); `ev_flatten` zeigt die Zweige dagegen korrekt an.
- Betroffene Stellen: `src/editor/editor_main.cpp`
  (`issues_scan_commands`, ~Zeile 1215).
- Gefunden in: Runde 16 (Z5, Recherche Issue-Scan).
- Fix: gemeinsame Regeln in `src/editor/event_rules.h`
  (`event_cmd_has_branches`, `event_cmd_uses_self_switch_letter`,
  `event_self_switch_letter_ok`); der Scan prüft jetzt beide Zweige von
  `if_self_switch` rekursiv und meldet ungültige Buchstaben (nicht A–D)
  bei `self_switch`/`if_self_switch`. Geprüft von
  `tools/editor_workflow_test.cpp` (4 Checks).

### [Tests/Editor] RGSS-Editor, Kartenmarker und Windows-Anzeige interaktiv prüfen — offen (Live)
- Die neuen F11-Zeilen-/Scroll-/DPI-Funktionen, NPC-/Event-Doppelklicks,
  das Rechtsklick-Startmenü und die EXE-Beschreibung sind implementiert.
  Automatisierung nutzt Modelle und synthetische Nachrichten mit WARP;
  Hardware-GPU, physische Maus, Fokus und Monitorwechsel sind nicht belegt.
- Eine isolierte Kopie mit `docs/WINDOWS_MAIN_LIVE_TEST.md` testen;
  tatsächliche Windows-Dateieigenschaften und Task-Manager-Anzeige notieren.
  Nicht durch ein CI-Grün als auf dem User-Rechner bestätigt ausgeben.
- Betroffene Stellen: `script_dialog_win.h`, `map_interaction.h`,
  `editor_main.cpp`, `build.ps1`, `tools/editor_ui_smoke.cpp`.

### [Engine] Zeitstempel in system.json nicht absturzsicher — offen (Dateischutz)
- `Project.touch` liest und schreibt mit demselben Handle, überspringt
  Konflikte mit dem Editor bzw. anderen Spielen und verändert fehlerhafte
  oder mehrdeutige Daten nicht. Die eigentliche Ruby-In-place-Schreiboperation
  ist aber nicht atomar: Abbruch, Stromausfall oder Schreibfehler können
  weiterhin zu einer unvollständigen Datei führen. Backups verwenden.
- Eine später abgestimmte atomare Ablage/Transaktion muss die bestehende
  `last_played`-Kompatibilität und den Editor-DocumentStore berücksichtigen.
- Betroffene Stellen: `game/engine/project.rb`, `src/editor/document_store_win.h`.

### [Tests/Ruby/Engine] Ruby-Start auf dem Zielrechner bestätigen — offen (Live)
- Symptom: Der User sah beim Editorstart `LoadError: cannot load such file -- json`.
  19x paketiert die zugehörige Stdlib und prüft den Start automatisiert mit
  WARP. Die Installation und Hardware des Users sind damit nicht geprüft.
- Eine neue vollständige Paketkopie bzw. einen aktuellen Build verwenden,
  eigene Projekte erhalten, Editortexturen und einen Spielstart interaktiv
  prüfen und Logs/Ergebnis gemäß `docs/WINDOWS_RUBY_LIVE_TEST.md` festhalten.
  Zusätzliche Gems/optionale native DLL-Abhängigkeiten sind nicht abgedeckt.
- Betroffene Stellen: `build.ps1`, `src/ruby/vm.cpp`,
  `tools/windows_runtime_smoke.ps1`, `tools/embedded_texture_test.cpp`.
- Gefunden in: User-Fehlermeldung nach 19w (2026-11-19).

### [Tests/Editor] Hauptfenster und Karten auf Windows interaktiv prüfen — offen (Live)
- Symptom/Status: Die 19w-Oberfläche mit Palette über Kartenbaum, Splitter,
  scrollbarem Inspektor, Toolbar und gezielten Kartenaktionen ist implementiert,
  aber noch nicht auf einem sichtbaren Windows-Desktop bedient worden.
- Vermutung/Analyse: Portable Geometrietests, Win32-Crosskompilierung und
  unsichtbare Controls können Fokus, Rendering, Monitordrehung/DPI, Tooltips
  und tatsächliche Rückfrage-/Mausabläufe auf Hardware nicht abschließend belegen.
- Betroffene Stellen: `src/editor/editor_main.cpp`, `main_layout.h`,
  `main_panel_win.h`, `main_tree_win.h`, `tools/json_test.cpp`,
  `docs/WINDOWS_MAIN_LIVE_TEST.md`. Schritte und Ergebnisse auf Zielrechner
  dokumentieren; eine Anleitung oder WARP ist kein bestandener Live-Test.
- Gefunden in: Etappe 19w (2026-11-19).

### [Editor] Hauptfenster-Aufteilung wird nicht gespeichert — offen (Bedienung)
- Symptom: Nach einem Neustart steht die Trennleiste wieder auf der
  Standardhöhe statt auf der zuletzt gewählten Position.
- Vermutung/Analyse: `wanted_map_height` ist nur Sitzungszustand; es gibt
  noch keine gespeicherten Benutzereinstellungen für das Hauptfenster.
- Betroffene Stellen: `src/editor/editor_main.cpp`, `src/editor/main_layout.h`.
- Gefunden in: Etappe 19w (2026-11-19).

### [Tests/Engine] Kampfsteuerung auf Windows interaktiv prüfen — offen (Live)
- 19v ergänzt zielgerichtete Items, gemeinsame HP-/MP-Werte, Esc-Routing und
  saubere Kampf-/Game-Over-Übergänge. Die Ruby-Tests nutzen native Attrappen;
  keine davon bedient das sichtbare Windows-Fenster oder eine Hardware-GPU.
- Mit isolierter Projektkopie Items, KO/Wiederbelebung, Flucht, Speichern,
  F5 und Event-Game-Over gemäß `docs/WINDOWS_BATTLE_LIVE_TEST.md` prüfen.
  Ergebnisse erst nach tatsächlicher Ausführung eintragen.
- Betroffene Stellen: `game/engine/{battle,party}.rb`, `game/main.rb`,
  `tools/{battle_state,encounter,runtime_boot}_test.rb`.

### [Engine] Mausblick nach Esc im Kampf — offen (Bedienung)
- Esc bricht im Kampf eine Auswahl ab oder versucht Flucht. Die native
  Hauptschleife gibt dabei zusätzlich immer den Mausfang frei; nach dem Kampf
  kann der Mausblick deshalb bis zur mittleren Maustaste ausgeschaltet bleiben.
- Die Absicht der globalen Esc-Mausfreigabe und eine kampfspezifische Lösung
  vor einer Änderung gemeinsam bewerten; das Verhalten ist in der Live-Test-
  Anleitung und in den Eingabeattrappen berücksichtigt, nicht interaktiv geprüft.
- Betroffene Stellen: `src/main.cpp`, `src/core/window.cpp`, `game/main.rb`,
  `tools/encounter_test.rb`, `docs/WINDOWS_BATTLE_LIVE_TEST.md`.

### [Editor] Weitere Dialoge am RPG Maker XP orientieren — offen (Design)
- Symptom/Wunsch: Das Hauptfenster wurde in 19w als erster Bereich am
  Screenshot-Vorbild ausgerichtet. Projekt-, Datenbank-, Event-, Material-,
  Tileset- und Script-Dialoge wurden in dieser Etappe nicht neugestaltet.
- Vermutung/Analyse: Jeder Bereich braucht eine eigene Funktions-/Datenprüfung;
  vorhandene 3D-Funktionen, kleine Fenster und hohe DPI bleiben zu beachten.
- Ideen aus den Screenshots: Datenbank mit Kategorien, Liste und Formular;
  F6 mit Seitenreiter und strukturierter Befehlsliste; Projekt mit Titel und
  Speicherort; Material-/Grafikauswahl sowie Scriptliste mit Zeilennummern
  und Syntaxhervorhebung. Nicht implementierte Inhalte nicht als fertig zeigen.
- Betroffene Stellen: Projekt-, F6-, F9-, Material-, Tileset- und Script-
  Dialoge in `src/editor/`.
- Gefunden in: User-Screenshots des RPG Maker XP; Priorität für die nächste
  Etappe noch mit dem User abstimmen.

### [Tests/Editor/Engine] Material-Dateischutz auf Windows breit prüfen — offen (Live)
- 19q implementiert handle-relative Materialzugriffe, Größen-/UTF-8-/JSON-
  Grenzen, bytegenauen Konfliktvergleich und einen nichtdestruktiven
  Sicherungs-/Rename-Commit; F3/F7 erhalten unbekannte Daten und alte RGBA.
  Geöffnete Projektverzeichnisse bleiben bis zum Wechsel gebunden.
- Offene Grenzen: Backup und Promotion sind kein atomarer Namensaustausch;
  nach Absturz oder blockierter Wiederherstellung kann `.rune-*.bak` bleiben.
  Seit 19s nutzen Karten/F9 ein ähnliches, separat getestetes Verfahren;
  andere Projektdateien sind nicht pauschal geschützt. Die CI-Tests ersetzen
  keine Tests auf Netzlaufwerken, anderen Dateisystemen, bei
  Stromausfall oder in der interaktiven GUI. Sicherungskopien nutzen.
- Automatisierung: Windows-2022-CI für Commit `776044d`, Lauf 45162482261:
  `tools/material_test.cpp` 57 Prüfungen/0 Fehler (Handle-/Reparse-/Race-
  Fälle), Ruby-Suite 27/0 einschließlich `tools/tex_import_test.rb` im
  isolierten WIC-Projekt. Interaktive Anleitung: `docs/WINDOWS_MATERIAL_LIVE_TEST.md`.
- Betroffene Stellen: `src/core/project_assets_win.h`,
  `src/editor/material_document.h`, `src/editor/editor_main.cpp`,
  `game/engine/textures.rb`, `src/ruby/vm.cpp`.

### [Tests/Engine] Projekttextur-Wechsel grafisch validieren — offen (Live)
- 19q trennt Basis-/Projekt-Slots, räumt F5/Projektwechsel/Atlas/Terrain ab
  und lässt Prefabs Namen dynamisch auflösen. Das vermeidet das zuvor
  beobachtete Behalten fremder Namen und Überschreiben der Basis-Slots.
- Windows-2022-CI für Commit `776044d`, Lauf 45162482261: Der D3D11-WARP-
  Test rendert Pixel und Atlas-Terrain (34/0); ein zweiter Test verwendet
  eingebettetes Ruby, echte Projektdateien, A→B→A und WARP-Pixel (19/0).
  Beide nutzten damals Software-Rendering und den Load-Pfad der gewählten
  Ruby-Installation. Seit 19x prüft `-WarpTest` eine isolierte Ruby-Kopie;
  Hardware-GPU, Bedienung und vollständige Abläufe bleiben am Zielrechner offen.
- Betroffene Stellen: `src/gfx/renderer.*`, `game/engine/textures.rb`,
  `tools/renderer_warp_test.cpp`, `tools/embedded_texture_test.cpp`,
  `docs/WINDOWS_MATERIAL_LIVE_TEST.md`.

### [Tests/Editor/Engine] Unicode-Pfade interaktiv und Langpfade prüfen — offen (Live)
- 19r stellt die gezielten Projekt-/Kartenpfade auf UTF-8 intern und
  strikte Windows-W-Grenzen um. Die Windows-CI (Commit `ae0cfe6`, Lauf
  45169345662) prüft auch Zeichen außerhalb der aktiven Codepage: Pfadtest
  28/0, eingebettetes Ruby/WARP 28/0, WARP 36/0, Material 57/0 und Ruby
  27/0 einschließlich echter kopierter Editor-CLI für den WIC-Import.
- Offen: tatsächliche GUI-Projektwahl, F9, Kartenbearbeitung, F4 mit dem
  echten Spiel und F12 in einer Unicode-Installation auf einem Zielrechner;
  außerdem DPI/Hardware-GPU. Der Kindprozess-Test ist **nicht** Rune3D.exe,
  WARP ist Software, der Ruby-Test betätigt weder F5 noch den Launcher.
  Anleitung und sichere Testkopie: `docs/WINDOWS_MATERIAL_LIVE_TEST.md`.
- Keine allgemeine Langpfad-Zusage: Der Test einer langen Kartenkomponente
  verwendet explizit `\\?\`; Dialoge, Ruby und alle Windows-Versionen/
  Dateisysteme sind damit nicht allgemein abgedeckt. Die Unicode-Umstellung
  allein war kein Dateischutz; 19s ergänzt den gezielten Karten-/F9-Schutz.
  Sicherungskopien nutzen und die Grenzen des Verfahrens beachten.
- Betroffene Stellen: `src/core/win_path.h`, `src/editor/editor_main.cpp`,
  `src/ruby/vm.cpp`, `tools/windows_path_test.cpp`, Live-Anleitung.
- Gefunden in: 19r (2026-11-19).

### [Tests/Editor] Karten-/F9-Dateischutz interaktiv und auf weiteren Dateisystemen prüfen — offen (Live)
- 19s schützt den bestätigten Stand mit Handle-Identität und erneutem
  Bytevergleich unter Windows-Schreib-/Löschsperre. Automatisiert: Windows-
  CI `6a3b8e1`, Lauf 45175342986, 58/0 einschließlich Dialog-Callbacks,
  Konkurrenz, Sicherungsnamen und Unicode. Das ist kein GUI-End-to-End-Test.
- Offen: echte Editor-/F9-Bedienung, Abbruch/Ja während externer Änderungen,
  Fremdhandles und Wiederherstellungsmeldungen am Zielrechner; Anleitung in
  `docs/WINDOWS_MATERIAL_LIVE_TEST.md`. Backup/Promotion ist kein atomarer
  Namensaustausch; `.rune-*.bak`/`.tmp` nach Absturz oder Fehlern prüfen.
- Tests decken nicht alle Netz-/anderen Dateisysteme, Schreibzugriffe über
  bereits bestehende Speicherabbildungen, Stromausfall oder Metadaten-/ACL-
  Erhalt ab. Reparsepunkte und Hardlinks werden abgelehnt. Backups nutzen.
- Betroffene Stellen: `src/editor/document_store_win.h`, `editor_main.cpp`,
  `src/core/project_assets_win.h`, `tools/document_store_test.cpp`.

### [Tests/Editor] F4 mit zwei Karten interaktiv prüfen — offen (Live)
- 19u übergibt die ausgewählte Karten-ID; automatisierte Tests prüfen einen
  echten Windows-Kindprozess mit Testtreiber und den echten Ruby-Bootstrap
  mit nativen Attrappen. Keiner dieser Tests bedient RuneEditor und Rune3D
  gemeinsam durch die sichtbare Windows-/D3D-Oberfläche.
- Auf einem Zielrechner mit zwei unterschiedlichen Karten, Unicode-Namen,
  F4, F5, Dateikonflikten, Titel-/Projektwechsel und Hardware-GPU gemäß
  `docs/WINDOWS_MATERIAL_LIVE_TEST.md` prüfen; nicht als bestanden markieren,
  bevor er tatsächlich ausgeführt wurde.
- Der Spielprozess ist keine Sandbox oder Dateischnappschuss: Spielstände,
  `last_played` und Scripts nutzen reguläre Projektdateien. Die Karte wird
  nach dem Editor-Speichern vom Spiel separat gelesen; externe Änderungen
  zwischen diesen Schritten sind nicht ausgeschlossen.
- Betroffene Stellen: `src/editor/{editor_main.cpp,playtest_win.h}`,
  `src/core/win_project_cli.h`, `src/main.cpp`, `game/{main.rb,engine/data.rb}`.

### [Engine] Kampf-Grundgerüst deckt noch nicht alles ab — offen (Feature-Fehlend)
- Stand 20a (Runde 23): Rundenkampf mit Angriff, Fertigkeiten (Schaden/
  Heilung/Buff, MP-Kosten), gezielten Heil-/Manaitems, Flucht, Zuständen
  (Gift/Brand/Regeneration/Wut/Schutz mit Dauer und Werte-Boni),
  Sieg/Niederlage/Exp-Level-Up/Gold/Drops, Gegner als 3D-Kästen mit
  HP-Balken.
- Es fehlen u. a.: Gegner-Fertigkeiten und Gegner-KI jenseits „Angriff auf
  zufälligen Helden", Schlaf/Stille/Paralyse und Sperr-Regeln, Heilung von
  KO-Helden per Fertigkeit, Elemente/Erfolgsraten, Skill-Bedingungen
  (XP: Skill-Guards), F9-Reiter für `skills`/`states` (nur JSON),
  Formationen/Positionen, Kampf-Hintergrundmusik/-Hintergrundbild,
  Kampftagebuch-Fenster zum Wegklicken. Zufallskämpfe beim Laufen sind
  seit 19l erledigt (siehe Erledigt-Eintrag 19l).
- Betroffene Stellen: `game/engine/battle.rb`, `docs/architecture.md` (19i).
- Gefunden in: Etappe 19i (Roadmap aus 19e, XP-Defaultscript als Referenz).
### [Engine] RGSS-Schicht deckt noch nicht alles ab — offen (Feature-Fehlend)
- Stand 20a: `$data_skills`/`$data_states` ergänzt; `$game_*` komplett inkl.
  `$game_temp`, `$data_actors/items/skills/states/system`
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
  sind aber nicht vollständig in der GUI editierbar. Seit 19y öffnet ein
  Doppelklick auf den Marker die gezielte Bearbeitung; direktes Ziehen von
  NPCs/Schildern im 3D-Viewport fehlt weiterhin. F5 bietet numerische Felder.
- Y ist ein Bodenversatz für die Darstellung; NPC-Kollision und Event-
  Reichweite sind weiterhin XZ-basiert (keine vertikale Trennung).
- Betroffene Stellen: `src/editor/placement_dialog.h`, `game/engine/npc.rb`,
  `game/engine/event.rb`, `game/main.rb`.
- Gefunden in: Etappe 19n.

### [Engine] Parallele Events blockieren NPC-Logik — teilweise erledigt (Runde 20)
- Symptom: Ein laufendes `parallel`-Event hält die NPC-Bewegung an, solange der Interpreter damit beschäftigt ist.
- Vermutung/Analyse: Single-Interpreter-Engine (Design seit 17d); kein Hintergrund-Thread vorgesehen.
- Betroffene Stellen: `game/main.rb` (Event-Schleifen), `game/engine/interpreter.rb`.
- Gefunden in: Etappe 18f (dokumentierte Einschränkung).
- **Erledigt (Runde 20, Z8/D3):** Seitenbedingungen, NPC-Bewegung und
  NPC-Kollision laufen jetzt auch bei laufendem Interpreter (Text/Wahl/Zahl/
  `wait`) — XP-Verhalten. Geprüft von `tools/encounter_test.rb`.
- **Offen bleibt:** parallele Events können nicht *während* eines Textes
  laufen (ein Interpreter, `Interpreter#start` verweigert). XP gäbe jedem
  `parallel`-Event einen eigenen Interpreter; das braucht pro Event einen
  eigenen Zustand samt Fenster-/Effekt-Zuordnung (Neubau von
  `interpreter.rb` + `main.rb`, nicht nur eine Schleifen-Umstellung).

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

### [Editor] Undo/Redo deckte nur Objekte ab — erledigt (Runde 19, Z8/T)
- Symptom: Strg+Z/Y betraf nur das `objects`-Array; Splat-Flächen, NPC-/Schild-Platzierungen, Event-/Seiten- und DB-Änderungen waren nicht rückholbar.
- Vermutung/Analyse: Undo-Stapel snapshottete nur `objects`; für Events/Splats fehlte ein zweiter Stapel oder ein gemeinsamer Karten-Snapshot.
- Fix: gemeinsamer Karten-Schnappschuss (`g.map_root` als JSON, Cap 50) mit `restore_map_snapshot()`; Push-Orte in F6, F5, Karten-Eigenschaften, Berg-/Boden-Pinsel, F7 und Kartenbaum-Umbenennen (`push_undo_text` nur bei Erfolg).
- Betroffene Stellen: `src/editor/editor_main.cpp` (`push_undo`, `restore_map_snapshot`, Event-/Platzier-/Tileset-Dialog), `src/editor/placement_dialog.h`.
- Gefunden in: Etappen 17c/18d/18g; behoben in Runde 19.
- **Bewusste Grenzen:** Karten anlegen/löschen und F2/F5-Dateitransaktionen sind Dateioperationen; der Projektstart (`system.json`) liegt außerhalb der Karte.

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
  (`ISSUE_CMD_TYPES`, `issues_scan_map`), `src/editor/event_form.h` (`EV_TYPES`), `tools/sync_test.rb`,
  `game/engine/interpreter.rb`.
- Gefunden in: Etappe 19a.

### [Tests] Live-Tests (Fenster-Injektion) laufen nicht in CI — offen (Test-Wissen)
- Symptom: `tools/edclick.psm1` und `log/live*.ps1` brauchen einen
  interaktiven Desktop und die Ziel-GPU; das leistet die CI nicht.
- Vermutung/Analyse: CI prüft zusätzlich zu Logik und Windows-Build auch
  WARP mit unsichtbarem Fenster, Atlas-Pixeln und einem eingebetteten
  Ruby-Texturtest. Das ersetzt keine interaktive Hardwareprüfung der
  vollständigen Anwendung (F12-Screenshot, `edclick`). Insbesondere bleiben
  Dialog-/DPI-Tests für F6, Objektformular/F2/F5/F7/F9 und die tatsächliche
  Atlas-/Splat-Darstellung im Editor und Spiel offen (siehe Handoff).
- Betroffene Stellen: `.github/workflows/ci.yml`, `tools/edclick.psm1`.
- Gefunden in: Etappe 19a.

### [Editor] editor_main.cpp ist eine einzige Datei mit ~9400 Zeilen — offen (Struktur)
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

### [Engine/Editor] Titel-BGM/-Grafik aus system.json nur angedeutet — offen (AB-Rest)
- Symptom: `enter_title!` (`game/main.rb`) liest ein optionales
  `system.json`-Feld `title_bgm` (Fallback: `musik`); ein Feld für eine
  2D-Titelgrafik existiert nicht (Titelbild ist die 3D-Kulisse). Ob der
  Editor-Systemdialog beim Speichern fremde Zusatzschlüssel erhält, ist
  nicht geprüft.
- Vermutung/Analyse: Der Editor schreibt system.json aus bekannten
  Feldern neu → `title_bgm` geht verloren, sobald man den Systemdialog
  öffnet und speichert.
- Betroffene Stellen: `game/main.rb` (`enter_title!`),
  `src/editor/editor_main.cpp` (Systemdialog), `game/data/system.json`.
- Gefunden in: Runde 12 (Scene_Title, AB-Punkt 2).

### [Editor/Ruby] Zwei Anlagepfade für neue Projekte — offen (Redundanz)
- Symptom: `Project.create` (Ruby) und `create_project` (C++) bauen
  dasselbe Projektgerüst; seit Runde 12 inhaltlich synchron (Slug,
  `welt01`, Mini-Datenbank), aber doppelt gepflegt.
- Vermutung/Analyse: Der Ruby-Pfad dient Tests/Referenz, der C++-Pfad
  dem Editor; Konsolidierung erst, wenn eine gemeinsame Spezifikation
  (z. B. JSON-Template oder Werkzeug) existiert.
- Betroffene Stellen: `game/engine/project.rb`, `src/editor/editor_main.cpp`.
- Gefunden in: Runde 12 (Y4-Nachgang).

## Erledigt
### [Engine] Esc-Menü kann über offenem Textfenster aufgehen — erledigt (Runde 16, Z5)
- Symptom: Während ein Event-Text offen ist, öffnet Esc das Pause-Menü über
  dem Text; nach Schliessen des Menüs liegt das Textfenster wieder darunter.
  (Die Physik-Pause von F5 greift in beiden Fällen korrekt.)
- Ursache: `Game.update` prüfte Esc/`@menu` VOR `@interpreter.update(dt)`
  ohne `busy_text?/busy_choices?`-Sperre. XP lässt `Scene_Menu` bei
  `message_window_showing` nicht zu.
- Fix: `Game.eingabe_offen?` (Text/Wahl/Zahl) sperrt das Menü-Esc-Öffnen
  in `game/main.rb`; zugleich gehört Esc damit der Wahl-/Zahlen-Eingabe
  (cancel-Abbruch wirkt, Z5). Abdeckung: runtime_boot-Checks „Z5: Esc
  während Wahlfenster/Zahleneingabe".
- Betroffene Stellen: `game/main.rb` (Menü-Esc-Block, `eingabe_offen?`).

### [Ruby/Editor/Engine] Fehlende JSON-Standardbibliothek — implementiert (19x)
- Symptom: `game/editor/boot.rb` → `textures.rb` meldete `LoadError: json`.
- Ursache: DLLs wurden kopiert, die Ruby-Standardbibliothek nicht; feste
  `C:/Ruby34-x64`-Pfade und ein CI-`RUBYLIB` verdeckten die Lücke.
- `build.ps1` liefert die passende Bibliothek inkl. nativer JSON-Module aus;
  die VM bevorzugt diese und weist unvollständige Pakete zurück. Ein Dialog
  macht Fehler beim Editor-Boot sichtbar. Tests und Artefakt wurden ergänzt;
  aktuellen Windows-CI-Lauf prüfen. Zielrechner-Live-Test bleibt oben offen.
- Betroffene Stellen: `build.ps1`, `src/ruby/vm.cpp`,
  `src/editor/editor_main.cpp`, `.github/workflows/ci.yml`, `tools/`.

### [Editor] XP-orientiertes Hauptfenster und Karten — implementiert (19w)
- Palette über Kartenbaum, verstellbare Trennleiste, skalierte Toolbar,
  scrollbare Seitenleisten und zentrale 3D-Ansicht; bestehende Kartenaktionen
  bleiben an Auswahl, Projekt und Datei gebunden. Der interaktive Windows-/
  DPI-/Hardwaretest bleibt als eigener offener Eintrag oben bestehen.
- Betroffene Stellen: `src/editor/editor_main.cpp`, `main_layout.h`,
  `main_panel_win.h`, `main_tree_win.h`, `tools/json_test.cpp`.

### [Engine] Kampfzustand und gezielte Items — implementiert (19v)
- HP/MP verwenden die Party als einzige Quelle; aktuelle und maximale Werte
  bleiben erhalten, KO bleibt KO. Ein aktiver Kampf kann nicht ersetzt werden.
- Itemzielwahl für Party-Helden inklusive KO, Abbruch ohne Verbrauch,
  Reservierung pro Runde und erneute Wirkungs-/Bestandsprüfung bei Ausführung.
  Verlorene Ziele bei Angriffen fallen auf lebende Gegner zurück. Script-/
  RGSS-Helfer akzeptieren ein optionales Ziel; der alte zielose Leader-
  Fallback bleibt kompatibel und kann bei voller Party ein Item verbrauchen.
- Esc wird im Kampf nicht vom Weltmenü abgefangen. Flucht, KO, Game Over,
  Titel/Kartenwechsel, F5 und Playtest-Fehler räumen Aktionen/Pause auf.
- `battle_state_test.rb`, `battle_test.rb`, `party_test.rb`, `rgss_test.rb`,
  `encounter_test.rb` und `runtime_boot_test.rb` sichern die Logik ab. Die
  Inputattrappe berücksichtigt Halten/Loslassen; der Bootstrap prüft ein
  Kampf-Item und echtes Slot-Datei-I/O in einer temporären Projektkopie.
  Auf `b2f34df` war [CI 45192650255](https://github.com/Kenk-JADev/Game_Engine-V2/actions/runs/45192650255)
  grün, Windows-Ruby 29/0/0. Der interaktive Windows-Test bleibt oben offen.
- Betroffene Stellen: `game/engine/{battle,party,rgss,scripts}.rb`,
  `game/main.rb`, die genannten Tests und `docs/WINDOWS_BATTLE_LIVE_TEST.md`.

### [Editor/Engine] F4 startet die gewählte Karte — implementiert (19u)
- RuneEditor speichert die aktive Karte, prüft die unveränderte Auswahl
  und übergibt Projekt und Karten-ID UTF-16-sicher an Rune3D. Ohne gewählte
  Karte bleibt es beim Projektstart; mit ungültiger oder nicht eindeutig
  lesbarer Auswahl erscheint ein Fehler statt einer anderen Karte.
- F5 im Playtest liest die gewählte Karte erneut. Auch wenn das Projekt beim
  Start fehlte, behält F5 die Vorgabe für einen erneuten Versuch im selben
  Projekt. Eine manuelle Projektwahl nach dem Titelbildschirm entfernt sie.
  `Game.change_map`, Terrain, Events und Begegnungen bleiben regulär aktiv.
  `MapCheck` berücksichtigt auch `.JSON` und ignoriert Kartenverzeichnisse.
- `windows_path_test.cpp` prüft Parser, Validierung, Unicode, Prozessstart
  und geerbte Umgebung; `embedded_texture_test.cpp` die Ruby-Umgebung;
  `runtime_boot_test.rb` prüft getrennte Projekt-/Kartenszenarien mit echten
  Ruby-Scripts und nativen Attrappen. [CI 45183821663](https://github.com/Kenk-JADev/Game_Engine-V2/actions/runs/45183821663)
  auf `bc237b3` ist grün; der Live-Test oben bleibt offen. Keine Sandbox und
  kein Hardware-/GUI-Nachweis.
- Betroffene Stellen: `src/editor/{editor_main.cpp,playtest_win.h}`,
  `src/core/{win_component,win_project_cli}.h`, `src/main.cpp`,
  `game/{main.rb,engine/data.rb,engine/launcher.rb,engine/mapcheck.rb}`
  und die genannten Tests (einschließlich `mapcheck_test.rb`).

### [Engine] Eingebauten Szeneneditor aus Rune3D entfernt — implementiert (19t)
- `RuneEditor.exe` bleibt das Bearbeitungsprogramm. Der frühere Ruby-Editor,
  sein F4-Modus und der interne Playtest-Zyklus wurden aus Rune3D entfernt;
  F4 bietet im Spiel keine eingebaute Kartenbearbeitung. Start eines separaten Spielprozesses
  per F4 im RuneEditor bleibt erhalten.
- `MapObjects` baut Prefabs für Spiel und Launcher, mit Bodenhöhe, Material,
  Animationen und Physik. Die bekannten Knotennamen `ed_*` bleiben erhalten;
  Zuordnung und Wiederaufbau verwenden tatsächliche Knotenidentitäten statt
  fremde Script-Nodes wegen ihres Namens zu löschen. `Game.change_map` bleibt.
- Tests: `map_objects_test.rb` (echte Ruby-Szene/Physikattrappe),
  `runtime_boot_test.rb` (echter Bootstrap/Launcher/F5 in temporärer Kopie,
  native APIs als Attrappen), `encounter_test.rb` (Spielablauf/Kampf/F4).
  CI auf `c7b2f5d`: [Lauf 45178315607](https://github.com/Kenk-JADev/Game_Engine-V2/actions/runs/45178315607) grün; grafischer Windows-/Hardwaretest steht aus.
- Betroffene Stellen: `game/engine/{boot,launcher,map_objects,prefabs}.rb`,
  `game/main.rb`, `tools/{map_objects,runtime_boot,encounter}_test.rb`.

### [Editor] F9-/Karten-Dateikonfliktfenster — implementiert (19s)
- Zuvor konnten Änderungen zwischen Konfliktvergleich und Ersetzen/Löschen
  verloren gehen. Karten laden/anlegen/speichern/löschen und F9 laden/
  speichern nutzen nun `document_store_win.h` und den geprüften Projektanker.
- Beobachtung hält eine Datei-ID über die Rückfrage; danach werden ID und
  Bytes unter einer Schreib-/Löschsperre erneut geprüft. Neuanlage und
  Promotion ersetzen kein fremdes Ziel; Löschen betrifft nur das geprüfte
  Handle. Blockierte Rücksetzung meldet beide möglichen Wiederherstellungs-
  namen. F9-Datenrückfragen sperren andere Editorfenster.
- Windows-CI für `6a3b8e1`, Lauf 45175342986: 58 Dateiprüfungen/0 Fehler;
  alle vier Jobs grün. Grenzen und fehlender GUI-Test stehen oben.
- Betroffene Stellen: `src/editor/document_store_win.h`, `editor_main.cpp`,
  `src/core/project_assets_win.h`, `tools/document_store_test.cpp`, `build.ps1`.
- Gefunden in: Etappe 19o.3 (F9), 19o.4 (Karten).

### [Editor/Engine] Projekt-Dateipfade mischen System-Codepage und UTF-8 — implementiert (19r)
- Symptom/Ursache: Die frühere Wurzelsuche und die Projekt-/Kartenscans
  benutzten Win32-A-Dateinamen, während F9 den zusammengesetzten Pfad als
  UTF-8 interpretierte. Fremd angelegte Unicode-Namen konnten dadurch
  verstümmelt sein oder nicht geöffnet werden.
- Lösung: Anders als der ursprüngliche Vorschlag „Ende-zu-Ende UTF-16“
  halten C++/Ruby und JSON Pfade intern als gültiges UTF-8; an Windows-
  Grenzen erfolgt strikte, verlustfreie Konvertierung nach/von UTF-16.
  Wurzelsuche, Projekt-/Kartenscan, Dateizugriffe/F9, Materialstore,
  Projektmerker, CLI/Umgebung, Ruby-Skripte, F4 und Screenshots nutzen den
  Vertrag. Lange Projektlistenzeilen werden dynamisch gelesen; temporäre
  Karten-/F9-Dateien haben kurze Namen. Ungültiges UTF-8/UTF-16 wird
  abgewiesen bzw. bei Verzeichniseinträgen übersprungen.
- Das eingebettete Ruby muss seine externe und Dateisystemkodierung selbst
  initialisieren: Der erste 19r-CI-Lauf fiel bei Unicode-Verzeichnislisten
  aus, da `ruby_init()` nicht die normale Optionsinitialisierung ausführt.
  `vm.cpp` setzt UTF-8 und lehnt einen anderen Dateisystem-Alias ab.
- Belegt: Windows-2022-CI für `ae0cfe6`, Lauf 45169345662, alle Jobs grün;
  `windows_path_test.cpp` 28/0, Material 57/0, WARP 36/0, echte Ruby-/WARP-
  Brücke 28/0, Ruby-Suiten 27/0. GUI und allgemeine Langpfade bleiben
  ausdrücklich offen (siehe Eintrag oben).
- Betroffene Stellen: `src/core/{win_path,win_project_cli,project_assets_win}.h`,
  `src/editor/{project_scan_win,playtest_win,database_file_win,atomic_file_win}.h`,
  `src/editor/editor_main.cpp`, `src/main.cpp`, `src/ruby/vm.cpp`,
  `game/main.rb`, `tools/windows_path_test.cpp`.
- Gefunden in: Prüfung zu 19o.4; gezielte Behebung in 19r.

### [Editor/Engine] Terrain-/Atmosphärengrenzen und Vorschau — implementiert (19p)
- Der Terrainplan prüft Maße, Seitenverhältnis, Auflösung, Seed, Amplitude,
  Pinsel-/Flächenwerte und Arbeitsbudgets vor Cast, Schleifen und Meshbau.
  Ungültige oder zu teure Kartenwerte bleiben im JSON; die Vorschau wird
  geleert und der Grund steht dauerhaft in Status/F10. Für Kachelgröße und
  Atmosphäre gibt es gewarnte Ersatzwerte. F2/F7 bewahren unberührte und
  uneindeutige Altdaten; das Ersetzen ungültiger F7-Werte erfordert Zustimmung.
- Der Editor lehnt Pfade und Windows-Gerätenamen als RGBA-Dateinamen ab.
  19p war nur ein lexikalischer Schutz; 19q ergänzt handle-relative
  Reparsepunkt-/Konfliktprüfungen für Materialdateien (Grenzen oben).
- Der Atlas hat höchstens 1024 64×64-Zellen, nutzt einen privaten Slot und
  cacht unveränderte Inhalte; auch erneut registrierte Texturen behalten
  ihren Namens-Slot. Fehlen Atlas/Basiszelle, bleibt Einzeltextur-Terrain;
  fehlende Flächentexturen verwenden die Basis. Atlas-UV werden nicht mehr
  versehentlich doppelt skaliert. Höhenpinsel bleiben beim Ruby-Karten- und
  Editor-Neuaufbau erhalten; Wetterpartikel sind auf Intensität 10 begrenzt.
- Lokal: C++-Release und Sanitizer, portable Terrain-/Formularfälle,
  Ruby-Tests und Brücken sowie Win64-Cross-Kompilierung. Ein echter
  Windows-GUI-/D3D-/GPU-Lauf und native Binding-Fehlerpfade bleiben offen;
  Tests und konkreter Plan stehen im Handoff. Andere Projektdateien sind
  ausdrücklich nicht pauschal geschützt (siehe offene Einträge oben).
- Betroffene Stellen: `src/world/{terrain_limits,terrain,atlas}.*`,
  `src/gfx/renderer.*`, `src/editor/{terrain_preview,tile_form,map_props_form}.h`,
  `src/editor/editor_main.cpp`, `src/ruby/vm.cpp`, `game/engine/terrain_api.rb`,
  `game/engine/{weather,mapcheck}.rb`, `game/main.rb`, `tools/terrain_test.cpp`.
- Gefunden in: Prüfung zu Etappe 19o.4; gezielte Behebung in 19p.

### [Editor] F6-Fenster auf kleinen Arbeitsflächen erreichbar — implementiert (19o.6)
- Das frühere feste 1000×730-Fenster konnte unterhalb der Arbeitsfläche
  liegen. Jetzt werden Fenstergröße und Position auf den aktuellen Monitor
  begrenzt; das Fenster ist größenänderbar und der Inhalt zweiachsig
  scrollbar. Fokuswechsel führen den Ausschnitt nach, DPI-Wechsel skalieren
  Schrift und Control-Geometrie. Listen/Combos bleiben direkte Dialogkinder.
- Portable Tests prüfen die tatsächlichen Control-Koordinaten und simulieren
  kleine Ausschnitte, mehrere DPI-Stufen und negative Monitorkoordinaten.
  Windows-Tests verwenden unsichtbare Win32-Controls; das ist kein echter
  Desktop-/DPI-Livetest. Dieser bleibt im Live-Test-Eintrag ausdrücklich offen.
- Betroffene Stellen: `src/editor/event_layout.h`, `event_view_win.h`,
  `editor_main.cpp`, `tools/json_test.cpp`, `build.ps1`.
- Gefunden in: F6-Audit zu Etappe 19o.5.

### [Editor] F6 schreibt unberührte Felder um — erledigt (19o.5)
- `event_form.h` liest ohne Mutation und bereitet Event-, Seiten- und
  Befehlsänderungen gemeinsam auf Kopien vor. Nur geänderte Felder werden
  ersetzt; unberührte Zahlen, Typen, Zusatzdaten und Zweige bleiben erhalten.
- Altseiten werden erst bei Bedarf materialisiert; ursprüngliche Top-Level-
  Daten bleiben erhalten. Uneindeutige oder beschädigte Strukturen werden
  geschützt, destruktive Änderungen und Wahlzweig-Neuzuordnung bestätigt.
  Ungültige Eingaben blockieren die gesamte gemeinsame Übernahme.
- Offene Entwürfe vor Auswahlwechsel, Schließen, Karten-/Projektwechsel und
  Speichern klären; verschachtelte Befehle werden über Pfade statt dauerhafte
  JSON-Zeiger adressiert. Neue Events/Kopien erhalten eindeutige darstellbare
  Namen, integergenaue Positionsverschiebung und Warnung bei fehlgeschlagener
  Verschiebung. Umbenennen warnt vor alten Spielstand-/Self-Switch-Schlüsseln;
  eine zu kleine Ziffernzahl klemmt den Startwert nicht still.
- Tests: portable C++-Suite, Sanitizer, Win64-Cross-Kompilierung,
  Ruby-Tests und C++/Ruby-Event-Brücke. Echter Windows-GUI-/DPI-Test bleibt
  offen (siehe Live-Test-Eintrag und Handoff). Event-Undo ist seit Runde 19
  vorhanden (Karten-Schnappschuss, siehe Editor-Eintrag oben).
- Betroffene Stellen: `src/editor/event_form.h`, `editor_main.cpp`,
  `tools/json_test.cpp`, `tools/event_bridge_check.rb`, `tools/sync_test.rb`.

### [Editor] Objektformular schreibt unberührte Felder um — erledigt (19o.4)
- Das portable `object_props_form.h` bereitet X/Z, Grad-Drehung und
  Material transaktional vor. Unveränderte Integer-/Floatwerte, Y, fehlende
  Felder und Zusatzdaten bleiben erhalten; ungültige Eingaben blockieren
  alles. Doppelte/nicht darstellbare Felder sind gesperrt, problematische
  Ersetzungen verlangen Zustimmung. Kein Undo bei unverändertem Formular.
- Ausstehende Eingaben werden vor Auswahl-/Karten-/Projektwechsel und
  Objektaktionen geklärt; Strg+S bezieht sie ein. ListView/TreeView-
  Aktionen und Rückfragen werden gegen Reentranz abgesichert. F3/F5 und
  Drag berücksichtigen das Formular; Drag behält unberührte Achsen und
  stellt beim Zurückziehen den ursprünglichen Zahlentyp wieder her.
- Sicheres Kartenladen prüft JSON/Unicode, Serialisierbarkeit und das
  `objects`-Array, ohne andere doppelte Schlüssel zu entfernen. Ungültige
  oder extreme Objektkoordinaten werden nicht gerendert/gepickt.
- Kartenspeichern vergleicht Dateibytes und ersetzt das Ziel erst nach
  vollständigem Schreiben einer temporären Datei. Bei externer Änderung
  oder Löschung wird gefragt; Karten-Löschen prüft externe Änderungen,
  Neuanlage überschreibt keine zwischenzeitlich angelegte Datei. Der
  Vergleich ist keine durchgehende Sperre (separater Backlog-Eintrag).
- Tests: `tools/json_test.cpp`, ASan/UBSan und Win64-Cross-Kompilierung;
  Windows-CI prüft die gemeinsame Dateihilfe kopflos. Der grafische
  Windows-Live-Test steht aus. F6 ist seit 19o.5 geschützt; der gezielte
  Terrain-/Atmosphärenschutz ist seit 19p implementiert (siehe oben).
- Betroffene Stellen: `src/editor/{object_props_form,map_document,json_validation,text_validation,atomic_file_win}.h`,
  `src/editor/editor_main.cpp`, `src/editor/placement_dialog.h`.

### [Editor] F9 schreibt unberührte Werte und verliert Gold — erledigt (19o.3)
- Das portable `database_form.h` übernimmt nur geänderte Felder; große
  Ganzzahlen, präzise Floats, optionale und unbekannte Werte bleiben
  erhalten. Gold funktioniert auch ohne Auswahl; ungültige Eingaben
  verhindern die gesamte Übernahme. Uneindeutige Gegnerlisten und kaputte
  Sammlungen werden nur nach Rückfrage ersetzt, Löschung wird bestätigt.
- Fehlerhafte/unlesbare Datenbankdateien werden nicht durch leere ersetzt;
  Änderungen durch andere Programme vor dem Speichern lösen eine Rückfrage
  aus. Speichern nutzt eine temporäre Datei; Schließen/Neu laden fragen
  bei ungespeicherten Änderungen. Das frühere Konflikt-Zeitfenster wird seit
  19s durch die separat dokumentierte Handle-Transaktion geschützt.
- Tests: `tools/json_test.cpp`, Win64-Cross-Kompilierung und Ruby-Suite;
  grafischer Windows-Live-Test steht aus.
- Betroffene Stellen: `src/editor/database_form.h`, `editor_main.cpp`.

### [Editor] Bearbeitete Block-Zonen verlieren Zusatzfelder — erledigt (19o.2)
- F2 zeigt temporäre Zeilenmarken; beim Ändern oder Umordnen bleiben
  Zusatzdaten und unberührte Koordinaten exakt bei der richtigen Zone.
  Neue Zeilen ohne Nummer; Entfernen/Ersetzen nach Rückfrage. Fehlerhafte
  Eingaben verhindern die gesamte F2-Übernahme; alte kaputte Einträge
  können unverändert erhalten werden. F6 vor F2 schließen (Formularzustand).
- Tests: `tools/json_test.cpp`; grafischer Windows-Live-Test steht aus.
- Betroffene Stellen: `src/editor/blocked_form.h`, `map_props_form.h`,
  `editor_main.cpp`.

### [Editor] Große JSON-Ganzzahlen beim Laden und Speichern gerundet — erledigt (19o.1)
- `JsonValue` bewahrt Dezimalstellen beliebig großer Integer-Tokens,
  einschließlich negativer Zahlen und solcher außerhalb des double-Bereichs.
  Geänderte Werte ersetzen ihren Token, Duplizieren inkrementiert ganze
  Koordinaten exakt; die numerische Rechensicht bleibt eine double-Näherung.
  Seit 19o.5 schreibt F6 unberührte Formularanzeigen nicht mehr zurück.
- JSON-Zahlengrammatik wird strikt geprüft; neue numerische Ganzzahlen werden
  ohne überlaufenden long-long-Cast geschrieben. Ruby-Brücke prüft Typ und
  Inhalt mit großen synthetischen Integern. Encounter-Eingaben sind weiterhin
  auf 2^53−1 begrenzt.
- Betroffene Stellen: `src/editor/json.{h,cpp}`, `number_format.h`,
  `tools/json_test.cpp`, `tools/json_bridge_check.rb`.

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
  `game/engine/{npc,event,prefabs,mapcheck,map_objects}.rb`, `game/main.rb`.

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
  zufälliger Kampf. Events haben Vorrang; F4 hat seit 19t im Spiel keine
  Editorfunktion mehr. Fehlgeschlagene Kämpfe werden erst beim nächsten
  Schritt erneut versucht.
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
  Vollbild-Alpha über die UI-Schicht, Reset bei Kartenwechsel).
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
- Historisch 18e+: DLLs und `ruby_builtin_dlls\` wurden kopiert, winpthread statisch gelinkt und `RUBYLIB` fest gesetzt. Die verbleibende Stdlib-Lücke wurde in 19x gesondert bearbeitet.
- Behoben in: Etappe 18e+.

### [Engine] Splat-UVs schlug fehl, weil JSON.parse String-Schlüssel liefert — erledigt (18d)
- Ursache: `Rune::Gfx.terrain_splats` nimmt jetzt String- UND Symbol-Schlüssel.
- Behoben in: Etappe 18d.

### [Engine] Mal-Werkzeug radierte nicht im Live-Test — erledigt (18e)
- Ursache: MK_SHIFT = 0x4 (nicht 0x8 — 0x8 ist Strg).
- Behoben in: Etappe 18e.
