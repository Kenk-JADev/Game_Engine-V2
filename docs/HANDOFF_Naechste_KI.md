# Handoff für die nächste KI - Rune3D, Stand nach Implementierung 19y

**Stand:** 19y RGSS-Editor, Kartenmarker, Projektstart und eigene EXE-Metadaten implementiert; **Runde 12 (AB): Scene_Title/Scene_File statt Launcher** (launcher.rb entfernt, interne Projekt-Auflösung, Y3/Y4 im Editor); **Runde 13 (Y1/Y2): Datenpflege Demo** (`tools/sync_demo.rb`, Truppen+encounters im Demo nachgezogen, Script-Befehle in beiden Bäumen); **Runde 14 (Z4): Sichtbarkeit** (echte `Scene_Gameover`, Karte+Spielzeit im Menü/Scene_File, ▾-Blinkzeiger, Menü-Item-Cache, Playtime/Steps im Panel, `'system'`-Snapshot ins Savegame); **Runde 16 (Z5): Wahl-Abbruch** (`cancel`/`cancel_branch` XP-403 in Engine+Editor+MapCheck/Issue-Scan, Esc-Routing Menü↔Eingabe gesperrt, Kombi-Fenster Text+Wahl (M) bewusst offen; Runde 15 = Z4-CI-Fix); **Runde 17 (Z6): Trigger-Design D1/D2** (auto = XP-Autorun-Neustart bis Seitenwechsel, touch = Wiederbetreten mit Rearm beim Verlassen der Nähe; Tests D1/D2 in `encounter_test`, Auto-Save/Load-Check angepasst). **Runde 18 (Z7/Z8): Roadmap + Befehls-Register + atomares Schreiben** (XP_VERGLEICH §8 um 20i–20l/D4/`$game_temp` erweitert; `docs/befehle.json` als Quellen-Register mit `tools/befehle_test.rb` gegen CMD_TYPES/EV_TYPES/ISSUE_CMD_TYPES/interpreter.rb; `Project.write_json` und Editor-`write_file` schreiben Temp+Rename). **Runde 22 (D4): XP-Stufenaufstieg** (`Party.gain_exp`/`set_exp`/`level_up!`, Kurve 2.7, Gesamt-Exp, Höchststufe 99, RGSS-Route `actor.exp = n`, Kampf nutzt `gain_exp`, Heldenwerte live; `tools/level_up_test.rb` 19 Checks). **Runde 23 (20a): Fertigkeiten und Zustände** (Datenbank `skills`/`states` + Startlisten je Held über `tools/add_skills23.rb`, `Party.skills`/`learn_skill` inkl. Spielstand, Kampfbefehl `Fertigkeit` mit MP-Kosten und Zielwahl, Schaden/Heilung/Buff, Zustände mit Dauer/Werte-Boni und Runden-Tick, `$data_skills`/`$data_states`, Script-Helfer `skills`/`learn_skill`; `tools/skills_test.rb` 70 Checks). **Runde 24 (20b): Menü wie XP** (Befehlsliste Gegenstand/Fertigkeit/Status/Speichern/Laden/Zurück/Titel, Unterfenster `@menu_screen` mit Item-Zielwahl, Fertigkeiten- und Statusseite, Esc Ebene für Ebene; `tools/menu_test.rb` 40 Checks). **Runde 26 (W): Encounter-Kadenz wie XP** (Begegnungen zählen pro Kachel = 1 m statt pro Schritt-Sound, `Game::KACHEL_METER`, Teleports/Sprünge ausgenommen; Schritt-Sound und `$game_party.steps` unverändert). **Runde 27 (20c Welle 1): Ablauf-Befehle** (`loop`/`break`, `label`/`jump`, `exit_event`, `erase_event`, `button_input`, `text_options`; 34 Befehlstypen in Register/CMD_TYPES/ISSUE_CMD_TYPES/EV_TYPES/interpreter.rb, Baumkopf „Schleife:“ und eigener Einfügepunkt im F6-Dialog, `Event#erase!` samt Darstellung/Picking, Fenster-Optionen im Modul `MessageWindow`, MapCheck-Prüfungen für Schleife ohne Ausgang/Marke/Tasten/Position; `tools/ablauf_test.rb` 30 Checks, `json_test` 717 Checks). Zweiter Teil derselben Runde: **Gemeinsame Events** (`database.json` → `common_events`, Befehl `common_event` mit Aufruftiefe 8, `CommonEvents.tick` für autorun/parallel mit Schalter, `$data_common_events`, `MapCheck.scan_database` inkl. Datenbank-Prüfung in der CLI; `tools/common_events_test.rb` 27 Checks, `json_test` 720 Checks). Offen: eigener Editor-Reiter für Gemeinsame Events. **Runde 28 (20d Teil 1+2): erweiterte Bedingungen, Sperren und Timer** (`if_gold`/`if_item`/`if_script`, `save_access`/`menu_access`/`encounter_access`, `timer` mit Anzeige oben rechts; **42 Befehlstypen**, Operator-Combo für alle Wenn-Typen, `Scripts.eval_code`, Zweige in Baum/Issue-Scan/MapCheck; `tools/bedingungen_test.rb` 26 Checks, `tools/timer_test.rb` 20 Checks, `json_test` 741 Checks). **20d Teil 3: `$game_temp`-Calling-Flags** (`TempActions`, Kampf/Menü/Speichern/Kartenwechsel/Titel aus Scripts; `tools/temp_flags_test.rb` 14 Checks). Offen in 20d: Variable-Operationen/Quellen (122), Actor/Button-Bedingungen, `party_member`, HP/SP/EXP/Level (311/312/315/316), Startposition bei `player_transferring`. **Runde 25 (20b-Nacharbeit): Parteigesichter** (`UI.gesicht` mit Datenbankfarbe `"face"` oder Namensfarbe, Initialen und HP-Streifen; Party-Panel rechts neben der Befehlsliste, 64-px-Gesicht auf der Statusseite). **Runde 21 (Kombi-Fenster): Text, Wahl und Zahleneingabe in EINEM Fenster** (`Interpreter#visible_lines`/`text_waiting?`, `UI.message(lines, choices)`, ein Fenster in `Game.draw`; 7 Checks in `event18f_test`). **Runde 20 (Z8/D3): Karte lebt während eines Textfensters weiter** (Seitenbedingungen, NPC-Bewegung, NPC-Kollision vor der Interpreter-Abfrage; Auslöser erst danach; Spieler bleibt eingefroren; `tools/encounter_test.rb` prüft es); **Issue-Scan prüft jetzt auch `if_self_switch`-Zweige und A–D** (gemeinsame Regeln in `src/editor/event_rules.h`). **Runde 19 (Z8/T + Kartenbaum): Undo über die ganze Karte** (Schnappschuss `g.map_root`, Push-Orte in F6/F5/Eigenschaften/Pinsel/F7/Kartenbaum, `restore_map_snapshot`) **und Kartenbaum-Hierarchie** (`system.json` → `map_tree`, Tab/Shift+Tab, portable Regeln in `editor/map_tree.h`). Aktuellen Git-/CI-Status prüfen; interaktiver Windows-/DPI-/Hardwaretest und Bestätigung auf dem Rechner des Users sind offen.
**Projekt:** `C:/Users/jusagu/Desktop/Engine` — Windows-Only 3D-RPG-Engine (C++20/Win32/D3D11 + echtes CRuby 3.4.11), Editor = eigenes Programm `RuneEditor.exe`.

## 0. Auftrag und Fortschritt (RPG-Maker-XP-Überholung)

Der User beauftragte eine Überarbeitung von Engine und Editor **im Stil von RPG Maker XP**. Die freigegebene Etappen-Roadmap (17b–17h) ist **komplett abgeschlossen**:

| Etappe | Commit | Inhalt |
|---|---|---|
| 17b | `41f1e0d` | Rechteckige Karten, Karten-Eigenschaften, Nebel, Kartenverwaltung |
| 17c | `5799a07` | Maus-Werkzeuge (Setzen/Picken/Verschieben) + Undo/Redo |
| 17d | `7b33e16` | Event-Editor (moduslos, Trigger + Befehlsliste) |
| 17e | `780e8db` | Script-Editor + Projekt-Scripts (`scripts.json`) |
| 17f | `3bd1dbc` | PNG/BMP-Import via WIC, Materialbase, Projekt-Texturen im Spiel |
| 17g | `d9e05e2` | Datenbank (Helden/Items/Gold), Party, Befehle item/gold/heal |
| 17h | `5702bd3` | Wetter, Werkzeugleiste mit Icons, Mausrad-Zoom, Sound-Test, Playtest (`--project`) |
| 18a | `ae9f9bf` | DPI-Schärfe, Fenster-Icons (prozedural), Mindestgrösse, Drag&Drop-Texturimport, G/Esc/Enter, Doppelklick-Fokus |
| 18b | `4cc0cd7` | Projekt-Auswahl im Editor (Dialog, `--project`, Merker `last_project.txt`, Neuanlegen mit Gerüst) |
| 18c | `3ec254e` | Tileset-Editor (Boden-Textur + Kachelgröße pro Karte, eigene Kacheln), Umlaut-Fix Fenstertexte |
| 18d | `7b85c5d` | Splat-Böden: mehrere Boden-Texturen pro Karte (Atlas + Flächen im Tileset-Dialog) |
| 18e | `b19f95f` | Mal-Werkzeug: Boden-Flächen per Maus im Viewport (Pinsel, Shift-Radierer, +/- Größe) |
| 18e+ | `978e8be` | Doppelklick-Start ohne Ruby-PATH (statisches winpthread, XCopy-DLLs, RUBYLIB) |
| 18f | `82ceab9` | Event-Editor wie RPG Maker XP: Kategorien, 6 neue Befehle, Parallel-Trigger, Choices-UI |
| 18g | `e8cfd1f` | Event-Seiten: mehrere Seiten pro Event mit Bedingungen (Schalter/Variable), Seitenwechsel resettet 'fired' |
| 18h | `a93ac1e` | Problem-Backlog: Editor-Scan + Probleme-Dialog (F10), Spiel-Issues nach log/issues.json |
| 18i | `614ab38` | Responsives Editor-Layout: Spalten wachsen mit, Werkzeugblock verteilt sich, Undo/Redo volle Breite, Viewport-Log |
| 18j | cfe2860 | Resize-Rendering (16-ms-Timer in WM_ENTERSIZEMOVE), Clear+Present nach ResizeBuffers, editor.log parallel lesbar (_SH_DENYNO) |
| 18k | `03d23f7` | Kartenbaum (XP-Kartenliste): TreeView links oben, Doppelklick = Eigenschaften, Rechtsklick-Kontextmenü mit Inplace-Umbenennen |
| — | `8abfdd2` | Fix Vollbild-Resize: hbrBackground im Hauptfenster (schwarze Ränder beim Maximieren weg) |
| 18l | `a0107fa` | Höhen-Malwerkzeug („Höhe malen“, ID 3029): Berge/Senken per Maus, terrain.heights im Karten-JSON; Kartengrenze (Physics-Clamp); Terrain-Bau pro Kartenwechsel (enter_map→build_terrain) |
| 19a | `0cd940d` + Nacharbeit | GitHub Actions (4 Jobs), `build.ps1` portabel + `-Test`, `tools/test_all.rb`, `tools/json_test.cpp` (88 Checks), `tools/json_bridge_check.rb`, `tools/lint_repo.rb`, `game/engine/mapcheck.rb` + Test, README.md, einheitliche Zeilenenden, **Fix: `json_write` beschnitt Kommazahlen (`%.12g`)** |
| 19b | `2f589ba` | Echte Umlaute überall: Prosa (Kommentare, UI-/Spieltexte, Doku, Daten-Namen) schreibt ä/ö/ü, Identifier bleiben ASCII; 24 Win32-Textaufrufe von A auf W (`utf8_to_wide`/`wide_to_utf8`); `build.ps1` mit UTF-8-BOM |
| 19c | `20494e1` + Nacharbeit `ad9ac18`, `3687d3a` (CI-Grün) | Editor-Hauptfenster professionell: linke Spalte als drei Gruppen mit Radio-Modi (`set_mode`), Objektliste als ListView (6 Spalten, sortierbar per Kopflick), Platzier-Vorschau („Geist" folgt dem Cursor, `R` dreht in 15°), Kontextmenü für Viewport und Liste, `log/editor_objects.json` als Leseweg für Live-Tests (edclick) |
| 19d | `e56b692` + Nacharbeit `0660de3` (CI-Grün) | XP-Features Welle 1: Self-Switches A-D pro Event (GameState, Seiten-Bedingung, `self_switch`/`if_self_switch`, Savegame, Editor-Combos im Seite-Kasten) + Bildschirmblende (`fade.rb`, Befehl `fade`, Reset bei Kartenwechsel); 23 Befehlstypen synchron in CMD_TYPES/ISSUE_CMD_TYPES/EV_TYPES; Tests `self_switch_test.rb`, `fade_test.rb` |
| 19e | `59845c1` (CI-Grün) | RGSS-Schicht (`game/engine/rgss.rb`): `$game_*`-Globale, `Graphics`/`Input`/`Audio`/`load_data`+`save_data` wie RPG Maker XP (Abweichungen: JSON statt Marshal, Namen statt IDs, kein `$scene`-Loop) + Launcher-Ersteinrichtung `:first` (ohne Projekt: Enter/N = erstellen, D = Demo, Esc = beenden; `ensure_demo` nicht mehr automatisch) + Context-Helfer `fade`/`self_switch` + `$game_system.menu_disabled/save_disabled` beachtet; Tests `rgss_test.rb`, script_test-Abschnitt 6 |
| 19f | `96be63d` (CI-Grün) | Artefakt-Bugfix (User-Konsole): `Rune3D-Windows` enthält jetzt `game/` + `projekte/` neben den EXEs (CI kopiert sie nach `build/`); beide EXEs loggen einen klaren Hinweis, wenn `game/` unter der Wurzel fehlt |
| 19p | `84ad7b5` (CI grün) | Terrain-/Atmosphärengrenzen, sichtbare F10-/Atlas-Fallbacks, F7-Datentreue, Ruby-Brücke und portable Terrain-Tests |
| 20d-1 | `1eaa611` | Erweiterte Bedingungen (`if_gold`/`if_item`/`if_script`) und Sperr-Befehle 134/135/136 in Engine, F6-Modell, Issue-Scan und MapCheck; `tools/bedingungen_test.rb` |
| 20d-2 | `33ac986` (CI grün: 45506017149) | Timer-Befehl 124 + Anzeige (`UI.timer`), Formular/MapCheck/Register (42 Typen); `tools/timer_test.rb` |
| 20d-3 | dieser Commit (CI-Status siehe Actions) | `$game_temp`-Calling-Flags über `TempActions` (LOGIK E.3); `tools/temp_flags_test.rb` |
| 20c | `e59b342` + `70e6fc8` (CI-Grün: 45504055987, 45504648443) | Ablauf-Befehle Welle 1: `loop`/`break`, `label`/`jump`, `exit_event`, `erase_event`, `button_input`, `text_options` (34 Typen) in Engine, F6-Modell, Issue-Scan und MapCheck; Darstellung/Picking gelöschter Events, Fenster-Optionen als Modul; `tools/ablauf_test.rb` |
| 19q | `d2e6192` bis `776044d` (CI-Grün Run 45162482261) | Projektgebundene Texturen, Materialstore, Windows-Datei-/WARP-/Ruby-Bindingtests und vorbereiteter Windows-Live-Test |
| 19r | `08b064e` + `ae0cfe6` (CI-Grün Run 45169345662) | UTF-8 intern, strikte Windows-UTF-16-Grenzen, Unicode-Projekte/Karten, Ruby-Dateisystemkodierung und F4-Prozessübergabe |
| 19s | `6a3b8e1` (CI-Grün Run 45175342986) | Handle-relative Karten-/F9-Dateitransaktionen, erneut geprüfte Rückfragen, Wiederherstellung und 58 echte Windows-Dateitests |
| 19t | `c7b2f5d` (CI-Grün Run 45178315607) | Ingame-Editor entfernt, Szenenbau für Spiel/Launcher getrennt und echte Ruby-Laufzeit kopflos geprüft |
| 19u | `bc237b3` (CI-Grün Run 45183821663) | Externer F4-Start mit gewählter Karte, Unicode-/Fehler-/F5-/Projektwechseltests |
| 19v | `9441733` + `b2f34df` (CI-Grün Run 45192650255) | Party-/Kampfzustand, gezielte Items, Esc, Game Over und Abbruch/F5; Ruby-Regressionen und Live-Anleitung |
| 19w | `d9a236c` (CI-Grün Run 45224317716) | Hauptfenster: Palette über Kartenbaum, Splitter/Scrollen/DPI und geschützte Kartenaktionen; Windows-Live-Test offen |
| 19y | aktuellen Git-/CI-Status prüfen | F11 RGSS, Marker-Doppelklick, Rechtsklick-Spielerstart, eigene EXE-Version und isolierter UI-Smoke |
| 19x | `39eab54`; CI-Lauf 45231512626 grün | Ruby-Stdlib paketieren, fehlendes JSON diagnostizieren und isolierten Start von Editor/Spiel testen |
| AB/12 | dieser Commit (CI-Status siehe Actions) | Scene_Title/Scene_File statt Launcher: `Scene_Base` (main = 1 Frame), interne Projektauflösung (last_played → erstes), Continue → Slot-Wähler, Menü „Zum Titel", UI-rahmen/abdunkeln, Y3/Y4 (Slug+welt01+Start-Held in `create_project`) |
| Y/13 | dieser Commit (CI-Status siehe Actions) | Datenpflege Demo (Y1/Y2): `tools/sync_demo.rb` (Vorlage = Quelle für database/scripts, Karten/system nur additiv, `dry:`-Prüflauf), Truppen+encounters in `projekte/dorf` nachgezogen, `add_demo_script_events.rb` (Mara → dorf_begruessung, Kristall Mitte → kristalle_zaehlen, beide Bäume), `event_test` mit Script-Fenster, neu `sync_demo_test` |
| 19o.6 | `326925b` (CI-Grün Run 44640705908) | F6: Monitor-Arbeitsfläche, DPI-skalierter Scroll-Ausschnitt, Fokus-Nachführung und portable/unsichtbare Windows-Tests |
| 19o.5 | `4635c80` (CI-Grün Run 44628573472) | F6: datentreue Event-/Seiten-/Befehlsformulare, Pending-Schutz, bedarfsweise Altseitenmigration und C++/Ruby-Event-Brücke |
| 19o.4 | `38ed554` (CI-Grün Run 44614856669) | Objektformular datentreu, Pending-/Drag-Schutz und sicheres Kartenladen; portable Tests |
| 19o.3 | `19067d4` (CI-Grün Run 44603441976) | F9: unveränderte Felder/Gold datentreu, Validierung, Warnungen und Dateischutz; portable Tests |
| 19o.2 | `f6539a4` (CI-Grün Run 44600036172) | F2-Block-Zonen: stabile Sitzungsnummern, Zusatzfelder/präzise Werte erhalten, Validierung und Rückfrage bei Verlust, F6-Zeigerschutz |
| 19o.1 | `a35cbdf` (CI-Grün Run 44598814272) | JSON-Ganzzahlen beliebiger Größe behalten ihren exakten Token; striktes Parsen, Inkrement ohne Rundung, Zahlenanzeige und Ruby-Brücke geprüft |
| 19n | `6b24b29` (CI-Grün Run 44597155419) | F5-Platzier-Editor für NPCs, Schilder und Objekte; X/Y/Z, Wegpunkte und Text, transaktional, NPC-Gespräche per E, portable Tests |
| 19m | `9bc392e` (CI-Grün Run 44589056862) | F2-Zufallskämpfe: gespeicherte F9-Truppen wählen/neu laden, Zeilenliste samt Gewichtung, validierte Schrittzahl, Altwerte/andere Kartendaten erhalten, Kartenbaum gesperrt; portable Tests in json_test |
| 19l | `5720d41`, `557a852`, `a7c4f5d` (zuletzt CI-Grün Run 44581746188) | Zufallskämpfe per Karten-JSON und XP-Formel; Start/Kartenwechsel, Event-/Editor-Vorrang, Physikpause und Teleports abgesichert, MapCheck/C++-Parität und Beispiel in game/data; Integrationstests encounter_test sowie C++-Validierung im json_test |
| 19k.3 | `bfdadac` (CI-Grün Run 44563139127) | Passierbarkeit rund gemacht: `game/engine/passage.rb` (Passage.zonen/zone?/hindernis?/blockiert?), Physik `solid_at` + `Rune::Physics.solid_at?`, passable? mit Objekt-Kollision, NPC stösst an/überspringt blockierte Wegpunkte, Mover stoppt an Wänden (auch Spieler-teleport), MapCheck-Lage-Warnungen + C++-Parität; neu tools/passage_test.rb |
| 19k.2 | `86b1bd8` (CI-Grün Run 44556771894) | Passierbarkeit: „blocked"-Zonen im Karten-JSON (Physik-Wände + Ruby-Bindings + enter_map + $game_map.blocked?/passable? + MapCheck/C++-Issues + F2-Editorfeld + rote Viewport-Quader); Tests mapcheck_test/rgss_test |
| 19k.1 | `d3f2bc8` (CI-Grün Run 44553438470) | Datenbank-Dialog (F9): vier Reiter Helden/Items/Gegner/Truppen, feldgesteuertes Formular (DB_FIELDS_*), Truppen-Gegnerliste komma-getrennt, sync_test prüft Feldtabellen gegen database.json |
| 19j | `74dc98f` (CI-Grün Run 44546218102) | Professionalisierung: `tools/sync_test.rb` (Ruby/C++-Vokabular maschinell abgeglichen: Befehlstypen+Enum+Labels, Trigger, Operatoren, Wetter, Self-Switches, Effekte, Interpreter-when-Listen, DB-Dialog vs. database.json) + README auf Stand 19d-19i |
| 19i | `05d3218` + Nacharbeit bis `50f9fec` (CI-Grün) | Zahleneingabe (input_number, UI.nummer_eingabe) + Kampf-Grundgerüst (battle.rb: Battler/Phasen/Sieg-Niederlage-Exp-Gold-Drop, database.json enemies+troops, Editor-Typen, CMD_TYPES 26); Tests battle_test.rb + script_test-Abschnitt 8 |
| 19h.2 | `4654729` + Nacharbeit bis `8031449` (CI-Grün) | RGSS an den User-Upload `XP-Defaultscript/` ausgerichtet: $game_temp/$game_system/$game_screen/$game_map/$game_player wie die Default-Scripts, Window_Selectable, Mover mit XP-Schrittnamen+Codes+turn_*+code-Parameter (Editor Wert 3), gameover/to_title-Flags; Tests erweitert |
| 19h | `1695951` + Nacharbeit bis `070da7e` (CI-Grün) | RGSS-Vertiefung: `graphics.rb` (Bitmap/Sprite/Window/Viewport, z-sortiert über `Graphics.draw`), `$scene` + `Graphics.scene_update`, `$game_temp`, `$data_actors/items/system`, `passable?` echt (Grenzen + Geländestufe), `RGSS.reset!` bei enter_map; Tests `graphics_test.rb` + rgss_test-Abschnitt |
| 19g | `e8ac3e5` + Nacharbeit `89eb6d6` (CI-Grün) | Bewegungsrouten (XP Move Routes): `mover.rb` (8 Schritte, repeat, wait, Ersetzung, Spieler/NPC-Ziele), Befehl `move_route` (24 Typen synchron), Editor-Formular ohne neue Controls, Context-Helfer, `Mover.reset!` bei enter_map; Tests `move_route_test.rb` + script_test-Abschnitt 7 |

Stand 19p: 671 JSON-/Editor-Checks und 51 echte Terrain-Checks jeweils in Release und ASan/UBSan/float-cast-overflow; Win64-Editor, Renderer, Terrain und Ruby-VM cross-kompiliert; zusätzliche unsichtbare Windows-Listboxprüfungen lokal nicht ausgeführt. Natives Ruby 3.0.6: 26 Suiten OK, 1 Windows-EXE-Skip; dieselben 26 Suiten einzeln mit Ruby 3.4-WASM im isolierten WASI-Shim OK (ohne EXE-Test). Native 3.0-JSON-Brücke: 14 Dateien, 0 Semantikfehler, 4 versionsbedingte Formatabweichungen; mit Ruby-3.4-WASM und echten C++-Fixtures: 14 Dateien ohne Semantik-/Formatabweichung und 17 Event-Checks/0 Fehler. WASM nutzt Fixtures statt Open3, die Ruby-Modultests nutzen Stubs.

Stand 19q: Windows-2022-CI für Commit `776044d`, [Lauf 45162482261](https://github.com/Kenk-JADev/Game_Engine-V2/actions/runs/45162482261), alle vier Jobs grün. Material-/Dateitest 57/0, Ruby-Suite einschließlich WIC-Import 27/0, WARP 34/0 und echte eingebettete Texturbrücke mit WARP 19/0.

Stand 19r: Windows-2022-CI für Commit `ae0cfe6`, [Lauf 45169345662](https://github.com/Kenk-JADev/Game_Engine-V2/actions/runs/45169345662), alle vier Jobs grün: Windows-Pfadtest 28/0, Material 57/0, WARP 36/0, echte eingebettete Ruby-/WARP-Brücke 28/0 und Ruby 3.4.13 27/0/0. Die Tests nutzen reale W-APIs, einen eigenen Kindprozess, echte kopierte Editor-CLI und CRuby; sie sind kein vollständiger F4-Spielstart, GUI-/DPI- oder Hardware-GPU-Test. Der Brückentest nutzt den Ruby-Load-Pfad der ausgewählten Installation. Interaktive Prüfung und Langpfadgrenzen siehe Backlog und Live-Anleitung. CI-Status und Arbeitsbaum vor Weiterarbeit prüfen; `opencode.json` ist lokaler Harness-Kram (ignoriert).

Stand 19s: [CI 45175342986](https://github.com/Kenk-JADev/Game_Engine-V2/actions/runs/45175342986) auf `6a3b8e1`: vier Jobs grün, 58 Windows-Karten-/F9-Dateitests, 28 Pfad-, 57 Material-, 36 WARP- und 28 Ruby-/WARP-Checks sowie 27 Ruby-Suiten ohne Fehler. Lokal 671 JSON-, 51 Terrain-, 25 Material-Sanitizerprüfungen/0 Fehler; Ruby 3.0.6 26 OK/1 EXE-Skip, Lint 0 Fehler/3 Warnungen. Interaktive GUI-/DPI-/Hardwaretests und weitere Dateisysteme bleiben offen.

Stand 19t: Der Ruby-Ingame-Editor und sein interner Playtest-Zyklus sind entfernt; der Szenenbau lebt in `MapObjects`, der separate RuneEditor und sein F4-Prozess bleiben. `runtime_boot_test.rb` lädt den echten Bootstrap/Launcher in einer temporären Kopie mit nativen Attrappen, `map_objects_test.rb` prüft Szenenbau und Kollisionen, `encounter_test.rb` den Spielablauf. Lokal: Ruby 3.0.6 27 Suiten OK/0 Fehler/1 Windows-EXE-Skip, MapCheck sauber, JSON 671/0, Terrain 51/0, Material 25/0, Lint 0 Fehler/3 Warnungen; `src/main.cpp` Win64 mit `-Werror` cross-kompiliert. Interaktive Windows-/GPU-Tests bleiben offen.

Stand 19u: F4 übergibt Projekt und ausgewählte Karten-ID; mit `--project` allein bleibt der normale Start erhalten. F5 liest im Playtest diese Karte erneut, auch nach einem zunächst fehlenden Projekt; manuelle Projektwahl löscht die Vorgabe. Nicht vorhandene, mehrdeutige oder nicht als JSON-Objekt lesbare Karten zeigen einen Fehler statt einer anderen Karte. `Database` und `MapCheck` erkennen auch `.JSON` und ignorieren Verzeichnisse. [CI 45183821663](https://github.com/Kenk-JADev/Game_Engine-V2/actions/runs/45183821663) auf `bc237b3`: alle vier Jobs grün; Windows-Dokumentdateien 58/0, Pfade 39/0, Material 57/0, WARP 36/0, eingebettete Ruby-/WARP-Brücke 31/0 und Ruby-Suite 28/0/0. Lokal: Ruby 3.0.6 27 OK/0 Fehler/1 Windows-EXE-Skip, MapCheck sauber, JSON 671/0, Terrain 51/0, Material 25/0. Interaktiver GUI-/Hardwaretest für 19u bleibt offen.

Stand 19v: Das Kampf-Grundgerüst wurde nutzbar stabilisiert. Party-Actors sind
die einzige HP-/MP-Quelle, KO bleibt KO, Items wählen ein Ziel und werden nur
bei Wirkung ausgeführt/abgezogen; vorgemerkte Mengen sind reserviert. Esc,
KO, erneuter Kampfstart, Game Over vor Event-Folgen sowie direkter Abbruch,
Kartenwechsel, Titel/F5 und Playtest-Fehler sind abgesichert. Alte zielose
`use_item`-Aufrufe behalten den Leader-Fallback mit möglichem Verbrauch auch
bei voller Party. [CI 45192650255](https://github.com/Kenk-JADev/Game_Engine-V2/actions/runs/45192650255)
auf `b2f34df` war grün, darunter Windows-Ruby 29/0/0. Die Nacharbeit bildet
Tastendruckflanken und die native Esc-Mausfreigabe in der Inputattrappe nach;
`runtime_boot_test.rb` prüft Action-Event, Projekt-Script und einen echten
Datei-Save-/Load nach Kampf-Item in einer temporären Kopie. Lokal Ruby 3.0.6:
28 Suiten OK/0 Fehler/1 Windows-EXE-Skip; MapCheck sauber und Lint 0 Fehler/3
bestehende Warnungen. Die Laufzeit benutzt native Attrappen: kein interaktiver
Kampf-/Hardware-GPU-Nachweis. Anleitung: `docs/WINDOWS_BATTLE_LIVE_TEST.md`.

Stand 19w: Der User hat das XP-inspirierte Design freigegeben und ausdrücklich
**Hauptfenster & Karten** als ersten Bereich gewählt. Palette über Kartenbaum,
Splitter, scrollbare Palette/Inspektor, vorhandene 3D-Arbeitsfläche und
Karten-Schnellaktionen sind implementiert. Der Splitter wird nicht persistent
gespeichert. Kartenaktionen warten auf den Auswahlabgleich und prüfen
Projekt/Datei; die bestehenden Datenrückfragen und Speicherwege bleiben.
Lokal: Ruby 3.4.13 28 OK/0 Fehler/1 Windows-EXE-Skip, MapCheck sauber;
portable JSON-Suite 686/0, Terrain 51/0, Material 25/0, JSON-Brücke 14/0 und
Event-Brücke 17/0; Repo-Hygiene 0 Fehler/2 bestehende Warnungen. Win64-
Hauptfenster und Windows-Test wurden crosskompiliert und der Test verlinkt;
das ist keine Windows-Ausführung. CI 45224317716 war in allen vier Jobs grün. Interaktive
Windows-/DPI-/Hardwareprüfung ist offen: `docs/WINDOWS_MAIN_LIVE_TEST.md`.

Stand 19x: Der User meldete beim tatsächlichen Editor-Start `LoadError: cannot
load such file -- json`. DLLs ohne die passende Stdlib und ein fester
`C:/Ruby34-x64`-Fallback waren die Deployment-Lücke; die 19w-CI verdeckte sie
durch ein gesetztes `RUBYLIB`. `build.ps1` kopiert nun die Ruby-Stdlib samt
nativen JSON-Dateien nach `build/lib/ruby`; `vm.cpp` priorisiert sie, verwirft
unvollständige Pakete und verändert `RUBYLIB` nicht. Fehlgeschlagenen Editor-
Boot meldet ein Dialog. `-WarpTest` prüft die verlegte Unicode-Laufzeit,
fremdes RUBYLIB und fehlende Dateien; `tools/windows_runtime_smoke.ps1` startet
echte Editor-/Spiel-EXEs mit Software-WARP und ohne Ruby-PATH. [CI 45230664729](https://github.com/Kenk-JADev/Game_Engine-V2/actions/runs/45230664729) auf
`f4b39e1` war in vier Jobs grün: Windows 30 Ruby-Suiten/0, WARP 36/0,
Ruby-Brücke zweimal 35/0 und zwei Negativtests je 7/0; beide echten EXEs
starteten aus der Paketkopie. Danach wurde die CI um das erneute Herunterladen
des veröffentlichten Artefakts vor dem EXE-Test ergänzt; aktuellen Lauf
prüfen. Hardware-/DPI-/User-Rechner-Test bleibt offen:
`docs/WINDOWS_RUBY_LIVE_TEST.md`. Zusätzliche Gems sind nicht garantiert.

**Userwunsch:** Das eigenständige RuneEditor-Programm funktional und optisch
an den gezeigten XP-Screenshots orientieren, ohne den 3D-Viewport oder
Datentreue aufzugeben. Weitere Dialogbereiche (u. a. F6/F9/F11) sind nicht
Teil von 19w; nächste Priorität mit dem User abstimmen.

**Nacharbeit 19a (notiert für die Historie):** Der erste CI-Lauf hat genau das getan,
wofür er da ist: `build.ps1 -Test` übergab g++ Argumente mit angehängtem Komma
(PowerShell-Argument-Modus — Array + `@`-Splatting nötig) und `tools/mapcheck_test.rb`
suchte Fundorte im `message`-Feld statt in `Finding#to_s`. Beides im Folge-Commit
repariert; der Windows-Job baute `Rune3D.exe` (1106 KB) und `RuneEditor.exe` (1584 KB)
mit GCC 16.2 warnungsfrei, `tex_import_test.rb` läuft dort dank `--strict` wirklich mit.

**Nacharbeit 18f (notiert für die Historie):** Der 18f-Stand lag beim Übergabezeitpunkt uncommittet im Baum (Handoff behauptete das Gegenteil). Er wurde verifiziert (Build + 14 Tests grün) und als `82ceab9` committet, bevor 18g begann.

**19y — aktuelle Aufgabe:** Windows-EXEs tragen eigene Versionstexte;
die Ursache „POSIX WinThreads for Windows“ ist ein möglicher fremder
`version.o` bei statischem Whole-Archive-Linking, nicht anderweitig bewiesen.
F11 bearbeitet echtes Ruby/RGSS mit Zeilennummern und opt-in Startscripts;
keine vollständige XP-Kompatibilität oder Sandbox versprechen. Auf der
Kartenfläche öffnet erst ein schneller Doppelklick auf denselben NPC-/Event-
Marker F5/F6. Ein kurzer Rechtsklick setzt den Projektstart über das Menü,
Ziehen bleibt Kamera. Projektstart wird sofort separat in `system.json`
gespeichert; F4/F5 verwenden ihn nur auf seiner Karte. `Project.touch` hält
beim Lesen/Schreiben denselben Handle und überspringt Konflikte; der Ruby-
In-place-Write ist nicht absturzsicher (Backlog). Portable und Ruby-Tests
sowie Windows-WARP-/UI-Smoke prüfen den aktuellen Stand; den aktuellen
CI-Lauf und die offenen interaktiven Zielrechnerfälle prüfen.

## 1. Arbeitsregeln (unbedingt einhalten)

- Antworten und Code-Kommentare **auf Deutsch**. User spricht Deutsch.
- **JSON und Doku per Ruby-Skript ändern** (`tools/update_docs*.rb`, `tools/add_*.rb`), nie per PowerShell-String-Bastelei.
- **Ein Commit pro Etappe.** Git: `C:\Users\jusagu\Tools\PortableGit\cmd\git.exe`.
- Build: `.\build.ps1` → `build\Rune3D.exe` + `build\RuneEditor.exe`.
  Vor dem Bauen laufende Exes stoppen: `Get-Process RuneEditor,Rune3D | Stop-Process -Force` (sonst „Permission denied" beim Linken).
  Ruby-PATH vor Tests: `$env:PATH = "C:\Ruby34-x64\msys64\ucrt64\bin;C:\Ruby34-x64\bin;$env:PATH"`.
- Tests — **zentral seit 19a, keine handgepflegte Liste mehr**:
  `ruby tools/test_all.rb` (findet `tools/*_test.rb` selbst, startet jeden Test als
  eigenen Prozess, wertet Exit-Codes aus, überspringt `tex_import_test.rb` ohne
  gebaute `RuneEditor.exe`; `--strict` = Übersprungene zählen als Fehler,
  `-v` = Ausgaben auch bei Grün, `--only NAME` = filtern).
  Auf Windows dasselbe in einem Rutsch: `.\build.ps1 -Test` (baut zusätzlich
  `build/json_test.exe`, läuft die Ruby-Suite mit `--strict` und prüft die
  JSON- und Event-Brücke). Stand 19a: 17 Ruby-Tests + C++-JSON-Test (88 Checks).
  Achtung: Tests, die Engine-Module direkt laden, brauchen `require 'issues'` (Muster siehe tools/add_issues_requires.rb).
- **Sprache/Zeichen:** Prosa (Kommentare, UI- und Spieltexte, Doku, Namen in
  JSON) schreibt echte Umlaute; Quellcode-Identifier und API-Namen bleiben
  ASCII. Projekt-/Kartennamen und Installationspfade können gültiges Unicode
  enthalten. Intern UTF-8, an Win32-Datei-/Textgrenzen die W-Varianten und
  strikte Konvertierung nutzen; ANSI kann UTF-8 verstümmeln. `.ps1`-Dateien
  mit Umlauten brauchen eine UTF-8-BOM (PowerShell 5.1).
- **Editor-Bedienung seit 19c:** Modi sind Radios in der Gruppe „Werkzeug";
  im Setzen-Modus zeigt der Geist am Cursor exakt, was der Klick setzt
  (`R` = Vorschau drehen); Objektliste sortiert per Klick auf den Spaltenkopf;
  Rechtsklick im Viewport/auf der Liste = Kontextmenü. Live-Tests lesen die
  Objektliste aus `log/editor_objects.json` (nicht per SendMessage).
- **RGSS ist Produktziel (User-Vorgabe 19e):** Spieler sollen die Engine
  später wie RPG Maker XP per Ruby (RGSS) im Script-Editor (F11) customizen.
  Deshalb: NEUE Engine-Funktionen immer auch aus Ruby erreichbar machen
  (Context-Helfer in `scripts.rb` und/oder `$game_*`-Wrapper in `rgss.rb`);
  die bewussten Abweichungen (JSON statt Marshal/RxData, Namen statt Integer-
  IDs, Engine-Takt statt `$scene`-Loop) sind in der rgss.rb-Kopfdoku und im
  BACKLOG-Offeneintrag dokumentiert - dort nachlesen, bevor man RGSS-Features
  erweitert.
- **Launcher-Ersteinrichtung (19e):** Ohne Projekte führt `:first` durch die
  Erstellung (Enter/N = Name, D = Demo-Dorf, Esc = beenden); kein
  automatisches `ensure_demo` mehr.
- **Event-Regie seit 19d:** Self-Switches (A-D) gehören zum auslösenden Event
  (Schlüssel `karte|name`), Seitenbedingungen können sie prüfen; der Befehl
  `fade` blendet den Bildschirm nicht-blockierend ab/auf. Neue Befehlstypen
  immer in ALLEN drei Listen eintragen (`CMD_TYPES` mapcheck.rb,
  `ISSUE_CMD_TYPES` editor_main.cpp und `EV_TYPES`/`EvType` event_form.h)
  und die Zähl-Assertion in `mapcheck_test.rb` anpassen. `sync_test.rb`
  liest beide C++-Dateien und vergleicht auch Trigger/Operatoren.
- **CI läuft automatisch** (`.github/workflows/ci.yml`, bei push und pull_request):
  `hygiene` (lint_repo), `engine` (test_all + mapcheck), `cpp-logik` (json_test mit
  `-Werror` + Brücke) auf ubuntu, `windows` (build.ps1 -Clean und -Test, EXEs als
  Artefakt) auf windows-2022. **Ein Commit gilt erst als fertig, wenn der Lauf grün
  ist.** Artifacts: `Rune3D-Windows`.
- **Build ist portabel (19a):** `build.ps1` sucht Ruby (`-RubyRoot` >
  `RUNE_RUBY_ROOT` > `ruby.exe` im PATH via RbConfig > `C:\Ruby34-x64`) und die
  ucrt64-Toolchain (`RUNE_MSYS_ROOT` > `<Ruby>\msys64\ucrt64` > `C:\msys64\ucrt64`);
  Header, libdir, `-l…ruby340` und DLL-Name kommen aus `RbConfig`. Der feste Pfad
  `C:\Ruby34-x64` ist nur noch Rückfall. An der Link-Reihenfolge (`-lucrtbase` VOR
  der Ruby-Bibliothek) ändert das nichts.
- Live-Verifikation über Fenster-Injektion: `tools/edclick.psm1` + Vorlage `log/live17h.ps1` (Wichtig: Fenster per `EnumWindows` nach Klassenname suchen, **nicht** `FindWindowW` über Prozessgrenzen — das ist unzuverlässig; Befehle per `SendMessage` statt `PostMessage`).
- Editor-Dialoge sind **modeless**; Screenshots via `CopyFromScreen` gehen auf dem isolierten Desktop **nicht** — stattdessen F12 im Editor (`request_screenshot`).
- **Ruby-Deployment (19x):** DLLs, `ruby_builtin_dlls` und die zum Build-Ruby passende `lib/ruby/<API>/<Plattform>` zusammen ausliefern. `vm.cpp` setzt `RUBYLIB` nicht mehr auf `C:/Ruby34-x64`; unvollständige mitgelieferte Laufzeiten werden abgewiesen. Den isolierten WARP-/EXE-Starttest und den aktuellen CI-Lauf prüfen. Andere Gems/optionale DLLs sowie Hardware-/Benutzertest sind nicht pauschal abgedeckt.
- Keine Wiederhol-Schleifen: gleiches Problem zweimal → Quelle lesen, Ansatz wechseln. Jede Runde endet mit Status (fertig / blockiert + Grund).
- **Backlog-Disziplin (seit 18g):** Fehler/Grenzen, die NICHT sofort in der laufenden Etappe behoben werden, gehören in `docs/BACKLOG.md` (Symptom, Vermutung, betroffene Stellen) — nicht in wiederholte Test-Läufe. Tests laufen pro Etappe genau einmal (Testsuite + ein Live-Test); nur bei rotem Test wird nachgelesen, nicht blind neu gestartet.

## 2. Architektur-Kurzreferenz

- **Spiel:** `src/main.cpp` (Schleife, F5=Ruby-Reload) → Ruby `game/engine/boot.rb` (Requires) → `game/main.rb` (`Game.setup/update/draw`, Launcher, Menü). Seit 19t kein Ingame-Editor; F4 im RuneEditor startet Rune3D separat.
  **Module:** `Database` (JSON laden, `database.json` seit 17g), `GameState`, `Party`+Actor (17g), `Event`/`Npc`/`Interpreter`, `Scripts` (Kontext `Scripts::Context`, keine Sandbox, mit Helfern say/switch/variable/teleport/sound/effect/item/gold/heal/weather…), `Pick`, `Effects`+`Weather` (Partikel via `Rune::Gfx.box`), `Textures`/`Sounds` (prozedural), `Project`, `Launcher`, `MapCheck` (19a: Karten-Daten
  prüfen, dieselben Regeln wie `issues_scan_map` im Editor; `Game.enter_map` meldet harte
  Fehler an `Issues`).
- **Editor:** `src/editor/editor_main.cpp` (eine große Datei, knapp 10000 Zeilen). Eigene Fensterklassen: `RuneEditorMain`, Viewport `RuneEditorView`, Dialoge `RuneEditorDlg`/`DlgNew` (Karten), `RuneEditorEventDlg`, `ScriptDlg`, `MatDlg`, `DbDlg`, `SoundDlg` — überwiegend modeless; F5-Platzierung ist modal.
- Editor-IDs: Menü 100–120 (115=TILESET, 117=PLACEMENT, 118–120=Aufteilung; 112=SOUNDTEST, 113=PLAYTEST, 114=PROJECT), Haupt-Controls 3001–3037 (Toolbar 3025, Boden-malen-Radio 3026, Kartenbaum 3027, Höhe-malen-Radio 3029, Objektformular-Hinweis 3033, Palette/Inspektor 3034/3035, Karten-Neu/Eigenschaften 3036/3037), Karten-Dialog 4100–4115 (Wetter 4113/4114, **Block-Zonen 4115**), Projekt-Dialog 4201–4205, Tileset-Dialog 4301–4319 (Liste 4301, Kachelgröße 4303, Übernehmen 4304, Speichern 4305, Erzeugen 4306, Farben/Muster/Name 4307–4310, Schließen 4311, Vorschau 4312, Splat-Flächen X/Z/B/T 4313–4316, Hinzufügen 4317, Entfernen 4318, Flächen-Liste 4319), Event 5001–5056, Script 6001–6023, Material 7001–7013, DB 8001–8055, Sound 8101–8107. F-Tasten: F1 Hilfe, F2 Eigenschaften, F3 Materialbase, F4 Playtest, F6 Events, **F7 Tileset**, F8 Sound-Test, F9 Datenbank, F11 Scripts.
- **Toolbar:** 12 Buttons in Gruppen, Icons prozedural und DPI-skaliert per GDI gezeichnet (`make_toolbar_icon` + Namensraum `ico`), senden Menü-IDs via `WM_COMMAND`. F-Tasten: F1 Hilfe, F2 Eigenschaften, F3 Materialbase, F4 Playtest, F6 Events, F8 Sound-Test, F9 Datenbank, F11 Scripts.
- **Playtest:** Menü/Toolbar F4 → `save_map()` + `CreateProcessW` mit exakt zitiertem Projekt und ausgewählter Karten-ID sowie expliziter Engine-Wurzel als CWD; EXE in `build/` oder neben `game/`. `--map` verlangt `--project`; ohne Karte bleibt die Projekt-Startkarte. Ungültige/nicht eindeutig lesbare explizite Karten zeigen einen Fehler. F5 liest die Karte erneut, Titel/Projektwahl löscht die Vorgabe. Der Prozess ist keine Sandbox und kein Dateischnappschuss (Backlog).
- **Wetter:** Karten-JSON `"weather":{"type":"kein|regen|schnee","intensity":1.0}`; `Weather.load_map/update/draw`; Interpreter `{"cmd":"weather",...}`; Editor-Dialog F2 schreibt das Feld.
- **Link-Falle (`fclose`-Crash):** In `build.ps1` muss `-lucrtbase` **vor** `-lx64-ucrt-ruby340` stehen, sonst leitet der Linker Editor-stdio auf Ruby-CRT-Exports um → Zugriffsverletzung. Kommentar dort steht, nicht „aufräumen".
- 17f-Import: WIC → rohe `<name>.rgba` unter `projekte/<p>/data/textures/` und Manifest `data/textures.json`; Spiel lädt via `Textures.register_project`. CLI: `RuneEditor.exe --import-tex <bild> [name] [--project <projekt>]`.

## 3. Was gut funktioniert hat (beibehalten)

- Kopflose Ruby-Tests mit `Rune::*`-Stub-Modulen (Muster: `tools/party_test.rb`, `tools/weather_test.rb`) — jede Etappe einen, alle in der Kommandozeile kürzbar prüfbar.
- Etappen-Disziplin: implementieren → bauen → 13 Tests → Live-Skript → `tools/update_docs17*.rb` → Commit. Genau so weitermachen.
- Demo-Datenboth: `game/data/` ist Vorlage; `Project.ensure_demo` kopiert sie komplett in neue Projekte (database.json, scripts.json kommen so automatisch mit). Bestehende Demo-Projekte zieht `ruby tools/sync_demo.rb` nach (Vorlage = Quelle für database/scripts, Karten/system nur additiv, ohne Demo-Werte zu überschreiben).
- `docs/architecture.md` hat pro Etappe einen Abschnitt (Anker `## Ruby-API (Stand Etappe 2)` + je Etappe davor eingefügt) — neue Abschnitte nach demselben Muster davor einfügen.

## 4. Bekannte Grenzen / nächste Kandidaten

**Backlog:** Offene Fehler, Grenzen und Test-Wissen stehen zentral in `docs/BACKLOG.md` — dort nachsehen und neue Erkenntnisse dorthin eintragen (nicht mehr hier listen).

- **Laufzeit-Szenen (19t):** `MapObjects` wird im Spiel und im Launcher
  instanziiert. Es erhält die Terrain-Höhenfunktion und ordnet erzeugte Nodes
  anhand ihrer Identität den Kartenobjekten zu. Bekannte `ed_*`-Namen bleiben
  für Scripts erhalten, fremde gleichnamige Nodes werden nicht entfernt.
  Das ehemalige `Editor` und `Game.playtest_start/stop` gibt es nicht mehr;
  `Game.change_map`, Begegnungen, Kamera-Picking und F5 bleiben erhalten.
- **Karten-/F9-Dateischutz (19s):** `document_store_win.h` bindet die
  Projektverzeichnisse je Aktion, beobachtet Datei-Bytes und ID, hält ein
  Attribut-Handle während einer Rückfrage und prüft danach beide unter
  Schreib-/Löschsperre erneut. Temp/Backup/Promotion ersetzen kein fremdes
  Ziel; Löschen erfolgt am geprüften Handle. Das ist kein atomarer Namens-
  austausch: `.rune-*.bak`/`.tmp` nach Crash oder blockierter Rücksetzung
  sichern und nach Prüfung manuell wiederherstellen. Reparsepunkte und
  Hardlinks werden abgelehnt. 58 echte Windows-Datei-/Race-Checks grün;
  keine GUI, kein allgemeiner Netzlaufwerk-/Memory-Mapping-/Crash-Nachweis.
- **Windows-Projektpfade (19r):** `win_path.h` definiert gültiges UTF-8
  intern und strikte UTF-16-Konvertierung an Windows-Grenzen. `win_project_cli.h`
  liest die Wide-Kommandozeile und setzt `RUNE_PROJECT` ohne ACP; `vm.cpp`
  setzt bei CRuby-Initialisierung externe und Dateisystemkodierung auf
  UTF-8 und prüft den Alias. Der erste 19r-Windowslauf scheiterte, weil
  `ruby_init()` die normale Optionsinitialisierung nicht ausführt und
  `Dir.children` sonst Namen per Codepage verlieren kann. Die Nacharbeit
  `ae0cfe6` besteht Windows-CI. Ruby bekommt `Rune::ROOT`/Skriptpfade als
  UTF-8; `Game.setup` trimmt Projektnamen nicht mehr. `project_scan_win.h`,
  `database_file_win.h`, `listbox_win.h` und `playtest_win.h` teilen die
  tatsächlichen Editorhilfen mit den Windows-Tests. Ungültige Zeichen
  werden abgewiesen oder beim Scan übersprungen; Altmerker ohne gültiges
  UTF-8 nicht automatisch recodieren. Editor-Neuanlage bleibt ASCII-only.
  Kein Pauschalversprechen für Langpfade oder Kartendatei-Sicherheit.
  F4-Test startet eine Kopie des Testtreibers, nicht das echte Spiel; der
  interaktive Ablauf steht in `docs/WINDOWS_MATERIAL_LIVE_TEST.md`.
- **Terrain/Atmosphäre (19p):** `world/terrain_limits.h` und
  `terrain_preview.h` prüfen sichere Raster-, Pinsel-, Splat-, Tile- und
  Atmosphärenwerte vor Cast/Schleifen. Karten werden dabei nicht verändert;
  Status/F10 zeigen fehlendes Gelände oder Atlas-Ersatz. F7 nutzt Original-
  indizes und eine nicht löschbare Auslassungszeile, erhält unberührte
  Felder und warnt vor Ersetzen ungültiger Werte. F2/F6 sperren das Öffnen
  in beide Richtungen; F2 prüft seinen Entwurf nach Datenrückfragen erneut.
  Bei Rückfragen/F5 werden konkurrierende Aktionen und Listenwechsel
  geschützt. `texture_path.h` prüft lexikalische Namen; 19q ergänzt
  gebundene Windows-Handles und Konfliktprüfung für Materialdateien.
  `AtlasPlan` ist auf 1024 Zellen begrenzt; Cache und
  private Atlas-Slot-Wiederverwendung sowie stabile Namens-Slots vermeiden
  wiederholte Kopien. `atlas` bleibt als Projekttextur benutzbar; fehlende
  Basiszellen fallen auf Einzeltextur zurück. Die doppelte UV-Skalierung
  ist korrigiert; `terrain.tile` ist im Splatpfad weiterhin wirkungslos.
  Das XP-Design ist inzwischen freigegeben; 19w behandelt das Hauptfenster.
  Grenzen für andere Projektdateien und grafische Live-Tests: Backlog.
- **Material-/Projekttexturen (19q):** Editor und Ruby halten geprüfte
  Projektverzeichnis-Handles, begrenzen JSON/RGBA/WIC, lehnen Reparsepunkte
  und Hardlinks ab und vergleichen beim Speichern die ursprünglichen Bytes.
  Neue RGBA-Dateien werden exklusiv angelegt, Altdaten erhalten. Der
  Backup-/Rename-Commit ist kein atomarer Austausch; Wiederherstellungsdateien
  nach Absturz/Konflikt beachten. Basis-/Projekt-Slots sind getrennt; F5 und
  A→B→A löschen alte Slots, Draws und Atlas-Terrainreferenzen. Tests:
  `material_test.cpp`, `project_tex_test.rb`, `tex_import_test.rb`,
  `renderer_warp_test.cpp` und `embedded_texture_test.cpp`. WARP ist Software,
  kein Hardware-/GUI-Nachweis. Reproduzierbare Kopie und Ablauf:
  `docs/WINDOWS_MATERIAL_LIVE_TEST.md`.
- **F6-Datentreue (19o.5):** `event_form.h` liest Event, Seite und Befehl
  nichtmutierend und bereitet gemeinsame Änderungen auf Kopien vor. Alte
  Seiten werden nur für neue Bedingungen/Seiten migriert; Top-Level-
  `trigger`/`commands` bleiben als Altdaten. Unberührte Felder, Zahlen,
  Zusatzdaten und Zweige bleiben erhalten; fehlerhafte/mehrfache Daten sind
  geschützt, destruktive Änderungen und Choice-Indexwechsel werden bestätigt.
  Pfade ersetzen langlebige Befehlszeiger; Pending-Entwürfe werden vor
  Auswahl, Schließen, Kartenwechsel und Speichern geklärt. Win32-Auswahl-
  Benachrichtigungen werden gepostet und vor Button- und Hauptfensteraktionen
  abgeglichen; MessageBox-Reentranz wird mit `data_prompt` geschützt. Neue Events/Kopien
  erhalten nicht kollidierende darstellbare Namen; jede Umbenennung warnt vor
  alten Spielstand-/Self-Switch-Schlüsseln. Große Integerpositionen werden exakt
  inkrementiert; fehlende/nicht verschiebbare Achsen warnen.
  `input_number` prüft Startwert gegen Ziffernzahl. `json_test.cpp` und die
  echte Ruby-Laufzeitbrücke prüfen das Modell; GUI-/DPI-Live-Test steht aus.
- **F6-Layout (19o.6):** `event_layout.h` begrenzt das Fenster auf die
  Arbeitsfläche des aktuellen Monitors und berechnet Scroll-/Fokusgeometrie.
  `event_view_win.h` skaliert Controls/Schrift bei DPI-Wechsel, erhält ihre
  Elternbeziehung und positioniert auch ganz verborgene Controls. Das Fenster
  ist größenänderbar; zwei Fensterleisten und Fokus-Nachführung machen den
  gesamten Ausschnitt erreichbar. Die portablen Tests lesen echte Control-
  Koordinaten; unter Windows werden zusätzlich unsichtbare Controls geprüft.
  Ein tatsächlicher GUI-/DPI-/Mehrmonitor-Test steht weiterhin aus.
- **Objektformular-Datentreue (19o.4):** `object_props_form.h` liest
  X/Z, Grad-Drehung und Material und bereitet nur Änderungen transaktional
  vor. Integer-Tokens, unberührte Floats, Y und unbekannte Felder bleiben
  erhalten; doppelte/nicht darstellbare Felder sind geschützt. Validierung,
  Rückfrage beim Ersetzen kaputter Daten, kein Undo bei unverändertem
  Formular. Offene Eingaben vor Auswahl-/Karten-/Projektwechsel und
  Objektaktionen klären; Strg+S übernimmt sie. F3/F5 beachten den Entwurf.
  ListView/TreeView-Auswahl wird nach den Win32-Callbacks abgeglichen;
  `data_prompt` sperrt für Datenfragen auch andere Editorfenster. Drag
  snappt bezogen auf den Griffpunkt, schreibt nur bewegte Achsen, bewahrt
  zurückgezogene Ausgangswerte und legt Undo erst bei einer Änderung an.
  `map_document.h` schützt JSON/Unicode, serialisierbare Zahlen und die
  eindeutige Objektliste; übrige doppelte Schlüssel sind nicht generell
  verboten. Objekte mit ungültigen/extremen Positionen bleiben in der Liste,
  aber nicht in der Vorschau. Fehlende Wurzel-`terrain` wird bei Bedarf
  zeigersicher angelegt (F6 schließen); `objects` danach neu holen.
  Karten-Dateibytes werden vor Speichern/Löschen auf externe Änderungen
  geprüft; 19s ersetzt die frühere Dateihilfe durch die oben beschriebene
  Handle-Transaktion. Sie ist kein atomarer Namensaustausch. Echte GUI-/
  End-to-End-Dateitests stehen weiterhin aus.
  Der Kartenbaum hält Einfüge-Texte bis zum Win32-Aufruf am Leben;
  `GetAsyncKeyState`-Aktionen laufen nur im Vordergrund-Hauptfenster.
  Gezielt abgesicherte Terrain-/Atmosphärenpfade siehe 19p; andere
  Projektdateien und grafische Tests bleiben im Backlog offen.
- **F9-Datentreue (19o.3):** `database_form.h` bereitet geänderte Felder
  und Gold auf Kopien vor; unberührte Werte, unbekannte Felder und sehr
  große Integer bleiben exakt. Fehlende Auswahl blockiert Gold nicht.
  Fehlerhafte Eingaben verhindern die ganze Übernahme; unlesbare/kaputte
  Datenbanken werden nicht überschrieben, Ersetzen problematischer Daten
  und Löschung verlangen Rückfrage. Nicht darstellbare Einzeltexte sind
  gesperrt. Externe Änderungen lösen vor dem Speichern eine Rückfrage aus;
  19s prüft den bestätigten Stand danach erneut unter Handle-Sperre. Schließen
  und Neu laden schützen ungespeicherte Daten. Grafischer Live-Test steht aus.
- **F2-Block-Zonen (19o.2):** `blocked_form.h` zeigt stabile temporäre
  `#n:`-Zeilenmarken; beim Editieren/Umordnen die Marke behalten, neue Zeile
  ohne Marke. Unbekannte Felder und unberührte Zahlen bleiben erhalten;
  kaputte alte Felder/Einträge sind geschützt, Ersetzen und Löschen nach
  Rückfrage, ungültige Eingabe blockiert die gesamte Übernahme. F6 muss vor
  F2 geschlossen sein (Formularzustand/Kartenkopie). Grafischer Live-Test steht aus.
- **JSON-Ganzzahlen (19o.1):** `JsonValue` bewahrt den originalen Integer-
  Token beliebiger Länge; `number()` bleibt eine double-Näherung und ist
  schreibgeschützt. `make_num`/`make_integer` ersetzen Werte, `increment()`
  inkrementiert Integer auch über 2^53 hinaus exakt. `-0` wird `0`; ungültige
  Zahlentokens werden abgelehnt. `editor_number_text` entfernt keine
  Integer-Endnullen mehr. F6 und das Objektformular schreiben unberührte
  Anzeigen seit 19o.5 bzw. 19o.4 nicht zurück.
- **Platzier-Editor (19n):** F5, `placement_dialog.h` + portables
  `placement_form.h`: NPCs, Schild-Events und Prefab-Objekte, X/Z und Y als
  Geländeversatz, Wegpunkte, erster Text, letzte Cursorposition. Dialog ist
  modal und arbeitet auf Kartenkopie; F6 muss für konsistente Entwürfe vor
  F5 geschlossen sein. OK übernimmt, Strg+S speichert. Alte komplexe
  Befehle/Zusatzfelder bleiben erhalten. `Game.update` verarbeitet NPC-Events,
  `Game.pickables` lässt NPC- und Schild-Grafik die Events nicht verdecken;
  Kollision/Interaktion weiterhin XZ-basiert; NPC-Auto-/Touch-Zustände im
  Spielstand. F5-Objektänderungen nutzen Objekt-Undo; beim Kartenladen wird
  dieser geleert, NPCs/Schilder sind noch nicht im Undo. F5/F7/F10-Hotkeys sind nun
  tatsächlich in der F-Tasten-Liste. Grafischer Windows-Live-Test steht aus.
- **F2-Zufallskämpfe (19m):** `encounter_form.h` ist das portable Formularmodell: optionale/ungültige Altdaten bleiben unbearbeitet stehen, Duplikate gewichten, geänderte Schritte sind leer (=30) oder 1 bis 2^53−1. UI `IDLG_ENC_*` in `editor_main.cpp`: Truppen aus gespeicherter Projekt-Datenbank, F9 öffnen/Neu laden, unbekannte Einträge nur nach Rückfrage, OK erst nach Validierung; danach Strg+S. `map_props_form.h` bewahrt unveränderte allgemeine F2-Felder und unbekannte Schlüssel; nach Wurzeländerungen `g.objects` neu holen. Kartenbaum und Projektwechsel sind während F2 gesperrt; F9 vor Projektwechsel speichern und schließen (sonst droht Schreiben der alten Datenbank ins neue Projekt). `json_test.cpp` prüft die Modelle und die Demo. Grafischer Windows-Live-Test steht aus.
- **Zufallskämpfe (19l):** Karten-JSON `"encounters": {"troops": [Truppen-IDs], "steps": n}`; n ist eine positive ganze Zahl (Default 30). `$game_player.make_encounter_count` würfelt `rand(n)+rand(n)+1`, `decrease_encounter_count` zählt bis 0. Boden-Schritt in `Game.update` (0,60 m Gehen / 0,85 m Rennen): Events haben Vorrang; F4 im Spiel hat seit 19t keine Editorfunktion. Bei freiem Interpreter, gültiger Liste und nicht gesperrtem `encounter_disabled` startet ein Kampf; nur bei Erfolg wird neu gewürfelt. Fehlstart versucht es beim nächsten Schritt erneut. Nach `RGSS.setup` und in `enter_map` wird gewürfelt; Karten-, Event-, RGSS- und Script-Teleports setzen die Schrittposition zurück (`Game.reset_step_tracking`); Bewegungsrouten zählen weiter. Der Zähler wird seit Runde 14 im `'system'`-Snapshot des Spielstands mitgespeichert (dort auch Timer, `timer_working`, `save_count` und die `*_disabled`-Flags; alte Spielstände ohne `'system'` behalten die Defaults). `encounter_test.rb` testet echten Spielablauf, `json_test.cpp` die C++-Validierung (`encounter_check.h`). Beispieltruppen lagen nur in der Vorlage `game/data`; `projekte/dorf` wurde in Runde 13 mit `tools/sync_demo.rb` nachgezogen (andere bestehende Projekte weiterhin nicht — bei Bedarf sync_demo aufrufen oder eigene Gegner/Truppen in F9 anlegen).
- **Passierbarkeit (19k.3):** Eine Quelle für alles, was läuft: `game/engine/passage.rb`. `Passage.blockiert?(x, z)` = Block-Zone ODER Objekt-Hindernis; Letzteres fragt `Rune::Physics.solid_at?(x, y, z)` an zwei Tastpunkten über der Steiggrenze (`STUFE = 0.35` = `kMaxStep`, Tastpunkte 0.45/1.2) — ein flacher Kasten bleibt also eine Stufe, eine Wand blockt. Kopflos (ohne `Rune::Physics`) zählen nur die Zonen, deshalb sind npc_test/move_route_test nicht auf die Engine angewiesen. Verbraucher: `passable?` (RGSS), `Npc#update` (anstossen + blockierten Wegpunkt überspringen), `Mover.bewege` (Schritt abbrechen, `warn_once`), Startpunkt-Warnung in `enter_map`. MapCheck prüft die Lage von Start/Events/NPCs/Wegpunkten in Zonen der **geprüften** Karte (`@block_zonen`), der Editor dasselbe per `blocked_zone_at`.
- **Block-Zonen (19k.2):** Karten-JSON `"blocked"` = Liste `{x,z,w,h}` in Weltmetern (**x/z = Mitte**, anders als `splats`, dort ist x/z die Ecke!). Wirkt als unsichtbare Wand über ALLE Höhen: `Physics::overlaps_any` prüft `blocked_overlaps` höhunabhängig, `Game.enter_map` lädt sie (`Rune::Physics.clear_blocked/add_blocked`), `$game_map.blocked?` + `$game_player.passable?` lesen dieselben Rechtecke aus `Game.map`. Editor: F2-Feld „Block-Zonen (x,z,w,h pro Zeile)" (IDLG_BLOCKED 4115), rote Flach-Quader im Viewport (`push_blocked_zones`, 0xFF5A5A), Issues-Scan mit MapCheck-Parität. **Achtung:** `dlg_text` hat ein 512-Zeichen-Limit — für Mehrzeiler gibt es seit 19k.2 `dlg_text_long` (Kommentar + Block-Zonen nutzen es, sonst gingen beim Speichern still Zeilen verloren). NPC-Pfade/Routen beachten Zonen seit 19k.3.
- Der Tileset-Editor ist seit 18c gebaut; seit 18d unterstützt er **Splat-Böden**: `terrain.splats` (Liste `{tex,x,z,w,h}` in Weltmetern) im Karten-JSON, Atlas aus allen 64×64-Texturen (`Renderer::update_atlas`), pro-Quad-UV ohne geteilte Vertices. Ohne Splats bleibt der alte Ein-Textur-Pfad byte-identisch.
- **Splat-Grenzen:** kein Weich-Übergang (Quad = eine Textur), Kacheldichte folgt der Terrain-Auflösung, `terrain.tile` wirkt im Splat-Pfad nicht. `Rune::Gfx.terrain_splats(base, liste)` nimmt String- UND Symbol-Schlüssel (JSON.parse liefert Strings — lange gesucht, siehe 18d).
- **Mal-Werkzeug (18e):** Radio „Boden malen" (3026); Linksklick malt, Shift+Klick radiert (MK_SHIFT = 0x4, **nicht 0x8**), +/- ändert die Pinselgröße (1–64 m), Pinseltextur folgt der Tileset-Auswahl. Viewport ist ein KIND-Fenster: im Live-Test per `FindWindowExW(main, 0, 'RuneEditorView', $null)` finden, WM_LBUTTONDOWN direkt dorthin senden. `splat_edit()` ist der gemeinsame Kern (auch der Tileset-Dialog nutzt ihn).
- **Problem-Backlog (18h):** Der Editor scannt die Karte nach jedem Laden (`issues_scan_map()`, Abschnitt vor `load_map` — steht VOR der Event-Editor-Sektion und nutzt deshalb die eigene Liste `ISSUE_CMD_TYPES`, nicht `EV_TYPES`!). Funde: unbekannte Prefab-Typen/Material-Overrides/Trigger/Befehle, kaputte Seiten + Bedingungen, fehlende then/else/options/branches/lines. Probleme-Dialog F10 (ID_MENU_PROBLEMS 116, IDs 5301–5304, Klasse `RuneEditorProblemsDlg`). Spiel: `game/engine/issues.rb` (`Issues.report(area, msg, severity:)` → Log + `log/issues.json`, `Issues.clear!` in `Game.setup`). Verdrahtet: Interpreter, Scripts, Party, Weather, Textures. Tests laden ohne boot.rb → dort `require 'issues'` ergänzen.
- **Hauptfenster (19w):** `main_layout.h` berechnet DPI-skalierte Spalten, Palette über Kartenbaum und Splitter; `main_panel_win.h` führt Kinder-Nachrichten und Fokus in scrollbaren Panels. Der Viewport bleibt 3D und nutzt seine Clientgröße für die Swapchain. Rechtsklick wählt den getroffenen Baumknoten; Aktionen warten auf den abgesicherten Auswahlabgleich. Maus-/DPI-/GPU-Live-Test steht aus (`docs/WINDOWS_MAIN_LIVE_TEST.md`). Der frühere 18i-Livetest belegt nur das damalige Layout.
- **Resize-Fakten (belegt):** Swapchain/Viewport folgen korrekt (`ResizeBuffers` fehlerfrei, Screenshot-Größe == Viewport-Größe), auch das Spiel behandelt `WM_SIZE` (`window.cpp` → `consume_resize()` → `renderer.resize()`). Bei Resize-Verdacht zürst `log/editor.log` auf `Viewport: …` prüfen. F12-Screenshot geht **nur** per echtem Tastendruck (`[EdClick]::Key(0x7B)`) — `PostMessage(WM_KEYDOWN)` wirkt nicht, weil `handle_shortcuts` `GetAsyncKeyState` fragt.
- **Event-Seiten (18g):** Events haben `pages` (Liste aus `{conditions, trigger, commands}`); die Seite mit dem **höchsten Index** mit erfüllten Bedingungen gewinnt (Seite 0 = Standard, leere Bedingungen = immer). Bedingungen: `switch` {name,value} und `variable` {name,op,value} (UND-verknüpft). `Event.refresh_page!` läuft in main.rb vor den Trigger-Schleifen; **Seitenwechsel setzt `fired` zurück** (touch/auto feuern erneut). Alte Karten ohne `pages` → implizite Seite 0; seit 19o.5 wird erst bei Bedarf migriert und Top-Level-trigger/commands bleiben als Altdaten erhalten. Editor-IDs: Seitenliste 5046, Neue Seite 5047, Seite löschen 5048, Seite übernehmen 5049, Bedingungsfelder 5050–5054. Der Trigger-ComboBox (5015) sitzt jetzt in der Gruppenbox „Seite". Tests: `tools/event_pages_test.rb` (44 Checks), `log/live18g.ps1`.
- **Event-Editor (18f):** 20 Befehlstypen mit Kategorie-Präfixen (Enum EvType ersetzt magische Indizes!), `choices` (Wahlmöglichkeiten mit Zweigen als „Wenn [x]:"-Kopfzeilen), `toggle_switch`, `move_player`, `weather`, `save_game`/`load_game`, Trigger `parallel`. Neue Tests: `tools/event18f_test.rb` (23 Checks, Mock-Muster wie event_test.rb). **Live-Test-Falle:** Strg+S wird bei Fokus im F6-Fenster nicht vom Hauptfenster verarbeitet. Zum Speichern das Hauptfenster fokussieren oder F6 schließen; seit 19o.5 werden offene F6-Eingaben vorher geklärt. `load_game` im Interpreter baut die Karte neu auf und ersetzt den laufenden Interpreter (dokumentiert).
- **Nächste Kandidaten:** GUI-Live-Tests, Undo/Redo über Objekte hinaus sowie Kampf-/RGSS-Ausbau (siehe Backlog).
- **WICHTIG bei Skript-Rewrites von Quellen:** NIEMALS `Get-Content -Raw` + `Set-Content -Encoding utf8` für C++-Dateien mit Umlauten — PowerShell liest dann CP1252 und produziert Doppelkodierung („Ã¼") + BOM. Reparatur per `tools/repair_mojibake.rb` (UTF-8 → CP1252 → UTF-8), Scan per `tools/scan_mojibake_bytes.rb`.
- **Nächste Kandidaten:** Passierbarkeits-Regeln (blockierte Zonen), Autotiles, Weich-Übergänge per Weightmap, Undo/Redo für Splat-Operationen (derzeit nur Objekte).
- Boden/Tileset (18c): `terrain.texture` + `terrain.tile` im Karten-JSON; Shader skaliert Terrain-UV mit 2.0/tile über `gParams.z` (Boxen nutzen `gParams.y`, Shadow-Pass beide 0 — nicht vermischen!). `Renderer::texture_rgba` liefert Pixel für Vorschauen; `Rune::Gfx.terrain_texture(name, tile_meters)` hat jetzt 2 Argumente (2. optional).
- **Umlaut-Richtlinie (18c):** Sichtbare Fenstertexte (`L"..."`) mit echten Umlauten; Log-Strings (`core::log_*`) bleiben transliteriert (ae/oe/ue), weil die Live-Tests (log/live*.ps1) darauf matchen. Quellen sind UTF-8, die EXE bekommt korrektes UTF-16 (nachgeprüft mit tools/check_binary_umlaute.rb).
- Projekt-Auswahl ist seit 18b drin (Dialog + CLI + Merker); der Editor startet mit dem zuletzt geöffneten Projekt. **Wichtig für Live-Tests (18a-Erkenntnis):** Statusbar-/Textinhalte sind per SendMessage von außen NICHT lesbar — WM_USER+-Meldungen werden über Prozessgrenzen nicht gemarshallt (`SB_GETTEXTW` von außen → Absturz im Editor!). Verifikation nur über Log-Einträge und Prozessleben. `SB_GETTEXTW = WM_USER+13 (0x40D)`, nicht 0x40E!
- FindWindowW über Prozessgrenzen ist unzuverlässig (17h UND 18b bestätigt) — Fenster IMMER per EnumWindows + Klassenname suchen (Vorlage: log/live18b.ps1, Find-EdWin).
- Fenster-Icons: `CreateIconIndirect` braucht eine ECHT monochrome Maske (`CreateBitmap` 1bpp) + 32bpp-DIB mit Alpha=255 — Farb-Bitmap als Maske crash die Titelzeichnung (war der 18a-Live-Test-Crash).
- Drag&Drop (PNG/BMP → Texturimport) ist eine Shell-Operation, per Skript nicht injizierbar — manüller Test oder editor.log-Vermerk ("Drag&Drop: N Textur(en) importiert").
- Events im Spiel haben nur EIN gleichzeitig laufendes Message-Fenster; parallele Events sind nicht vorgesehen (Design, kein Bug).
- Undo/Redo nur für Objekt-Mutationen, nicht für Event-/DB-Änderungen (DB-Dialog sichert pro „Speichern").
- `JsonValue` hat kein `erase`; `json_remove_key`-Helfer existiert. Ruby 3.4: keine endless **Setter** (`def foo=(v) = …` verboten), endless Null-Arg-Methoden sind ok.
- Key-Name-Tabelle `vk_from_name` in `vm.cpp` ist lückenhaft (z. B. kein 'lwin') — bei Bedarf erweitern.
- Hardware: i5-10400T, 7,7 GB RAM, Intel UHD 630, keine Admin-Rechte.

## 5. Live-Test-Vorlage

**Noch offen für 19p (echter Windows-Rechner mit D3D11):** Ein gesichertes
Testprojekt verwenden. Normalen Boden mit Höhenpinseln und mehreren Splats
im Editor und F4 prüfen: Höhen bleiben beim Neubau, Atlas-Zellen stimmen
visuell (keine doppelte UV-Skalierung), auch eine Textur namens `atlas`.
Mehrfach F5/Kartenwechsel und Textur-Neuanmeldung testen; GPU-/CPU-Speicher
darf für unveränderte bzw. gleichnamig ersetzte Texturen nicht wachsen.
Fehlende Basis-/Flächentextur und mehr als 1024 Atlas-Eingaben auf Kopien
prüfen: F10/Status nennen die Einschränkung, kein löchriges altes Mesh.

Separate Kartenkopien mit riesigen Integern, extremen Seitenverhältnissen,
ungültigen Höhenpinseln und überlangen Splatlisten öffnen: F10 nennt die
Ursache, das alte Terrain verschwindet, unveränderte JSON-Werte bleiben
beim Speichern erhalten. In F7 eine gültige Zeile nach ungültigen Zeilen
entfernen und den Originalindex prüfen; ungültige Zeile nur nach Ja
löschen, Auslassungszeile nicht. Bei ungültigem Tile nur die Textur ändern
und umgekehrt; vor Ersetzen kaputter Altdaten Abbrechen/Ja testen. Bei
offenem F7 A→B→A wechseln: Übernehmen/Entfernen müssen gesperrt sein.
F2→F6 und F6→F2 sowie Wetter/Nebel-Ersatz und F10-Neuscan prüfen. F7- und
F3-Auswahl während Rückfragen und F5, Formularentwürfe sowie normale und
ungültige RGBA-Dateinamen mit gesicherten Kopien prüfen. 19q ergänzt
Material-Reparsepunkt-/Konfliktschutz und automatisierte Tests einschließlich
der eingebetteten Ruby-Texturbrücke; den vollständigen Ablauf mit Hardware-GPU
und GUI trotzdem interaktiv prüfen. Siehe `docs/WINDOWS_MATERIAL_LIVE_TEST.md`.

**Noch offen für 19o.6 (echter Windows-Rechner):** Ein gesichertes
Testprojekt mit mehreren Events/Seiten und verschiedenen Befehlstypen öffnen.
Bei 1024×640 abzüglich Taskleiste und bei 100 %, 125 %, 150 % und 200 %
(soweit verfügbar) F6 öffnen: Fenster samt Titelleiste innerhalb der
Arbeitsfläche; beide Scrollachsen, Rand-Resize und alle Controls prüfen.
Bei viel Platz sind die Leisten deaktiviert. Auch kleinen Fensterausschnitt,
hohe Skalierung, negative Monitorkoordinaten, Taskleiste an anderer Kante,
Bildschirm-/Arbeitsflächenwechsel und Verschieben zwischen Monitoren mit
verschiedenen DPI ausprobieren. Beschriftungen, Gruppenrahmen, Combos und
deren Dropdowns müssen sichtbar und bedienbar bleiben.

Tab/Umschalt+Tab bis zu allen Feldern, Schaltflächen und Listen prüfen;
ungültige Eingaben müssen das betreffende Feld sichtbar fokussieren.
Scrollleisten, Mausrad auf freier Fläche und Umschalt+Rad testen; beim
Scrollen in Listen und Combos muss deren eigene Bedienung erhalten bleiben.
Mit offenen Event-/Seiten-/Befehlsentwürfen scrollen und Auswahlen schnell
wechseln, Ja/Nein/Abbrechen und Speichern prüfen: weder Entwurf noch alte
Auswahl dürfen ungewollt verloren gehen. Anschließend JSON-Datentreue wie
im 19o.5-Test kontrollieren. Verdeckte CI-Controltests ersetzen dies nicht.

**Noch offen für 19o.5 (echter Windows-Rechner):** Gesichertes Testprojekt
mit mindestens zwei Events und einem NPC erstellen: altes Top-Level-Event,
mehrseitiges Event mit Schalter-/Variablen-/Self-Switch-Bedingungen,
verschachtelte Wenn-/Choice-Zweige, große Integer, präzise Floatwerte,
Zusatzfelder und einen langen Text. F6 öffnen, ohne Änderungen navigieren,
schließen und speichern: kein automatisches `pages` und keine Änderung an
Originalwerten. Namen ändern und die Warnung zu alten Spielstands-/Self-Switch-
Schlüsseln bestätigen oder abbrechen; einen verschachtelten Befehl ändern und
JSON auf unberührte Werte/Typen und Fremdfelder prüfen. Erst eine Bedingung oder
neue Seite anlegen und die aufbewahrten Top-Level-Daten sowie Ruby-Verhalten
kontrollieren. Fehlende, falsche und doppelte Schlüssel separat prüfen.

Event-, Seiten- und Befehlsauswahl mit unübernommenen Eingaben schnell
wechseln; Ja/Nein/Abbrechen, ungültige Werte, Rückfragen, Fokus und alte
Auswahl prüfen. Dasselbe bei Speichern, Karten-/Projektwechsel, Schließen,
Löschen, Duplizieren und Einfügen; kein Teil-Commit bei Fehler. Wahloptionen
umordnen, Typwechsel/kaputte Daten nur nach Bestätigung ersetzen, Zweige und
Zusatzdaten kontrollieren. Dasselbe Event mehrfach duplizieren, NPC-Namen
und Self-Switch-Isolation prüfen; fehlende/nicht verschiebbare Koordinaten
müssen warnen. Zahleneingabe mit zwei Stellen: 99 gültig, 100 abgelehnt.
Lange/geschützte Felder, Tastatur/Tab/Enter sowie Layout bei 1024×640 und
verschiedenen DPI-Skalierungen prüfen. F2/F5-Guard und F6 ohne Undo beachten.

**Noch offen für 19o.4 (echter Windows-Rechner):** In einem gesicherten
Testprojekt eine normale Karte mit mindestens zwei Objekten, präzisen
X/Z-/Radiant-Werten, Ganzzahlen, Y-Versatz und Zusatzdaten anlegen (JSON
per Ruby-Skript). Ohne Änderung Übernehmen, Auswahl und Sortierung wechseln,
dann nur Material bzw. nur X ändern, Strg+S und gespeichertes JSON auf Typen
und unveränderte Felder prüfen. Undo/Redo und einen Drag ohne Bewegung,
nur entlang einer Achse sowie zurück an den Ausgangspunkt prüfen.
Ungültige Zahlen, extrem große Koordinaten, doppelte X/tex-Schlüssel und
nicht darstellbare Materialtexte testen: keine Teiländerung, gesperrte
Felder bzw. Rückfrage und fehlende Vorschau bei ungültiger Position.
Mit offenem Entwurf Auswahl-, Karten- und Projektwechsel, F3, F5, Undo,
Speichern und Schließen auslösen; Ja/Nein/Abbrechen und die alte Auswahl
bei Fehler kontrollieren. Auf reentrante Auswahlmeldungen, Fokus und
MessageBox-Owner achten; auch F2/F3/F5 bei bestehendem Drag/Capture testen.
Mehrfaches oder nicht listenförmiges `objects`, ungültiges JSON/UTF-8 und
nicht endliche Kommazahlen dürfen die aktuelle Karte nicht ersetzen; ein
fehlendes `objects` wird als leere Liste angelegt. Kartenbaum-Auswahl bei
Abbruch zurücksetzen. Während eine Karte geöffnet ist, ihre Datei extern
ändern/löschen und das Verhalten bei Speichern/Löschen (Ja/Nein) prüfen;
beim Schreibfehler muss die alte Datei erhalten bleiben. Neue Karte darf
keine inzwischen angelegte ID-Datei ersetzen. Mit F6 offener und fehlender
`terrain`-Wurzel Zeigerschutz testen. In Projekt-/Tileset-Fenster tippen und
sicherstellen, dass keine Objektkürzel/Kameratasten ausgelöst werden.
Layout/Tab/Enter bei 1024×640 und hoher DPI prüfen. CI prüft die Datei-Hilfe
kopflos, ersetzt aber nicht diese GUI-/End-to-End-Dateitests.


**Noch offen für 19o.3 (echter Windows-Rechner):** Ein Testprojekt mit
großen Integern, präziser Drop-Chance, Zusatzfeldern und alten fehlerhaften
Gegnerlisten vorbereiten (JSON per Ruby). F9 öffnen, ohne Änderung Reiter
und Auswahl wechseln, einen Namen ändern und speichern: restliche Felder
und Zahlentypen vergleichen. Ohne Auswahl Gold ändern und speichern;
ungültige Zahlen beim Auswahlwechsel und Speichern prüfen. Ersetzen
fehlerhafter Gegnerlisten, Löschen, Neu/fehlende Sammlung, Schließen und
Neu laden jeweils mit Ja/Nein/Abbrechen prüfen. Unlesbare/ungültige Datei,
doppelte Schlüssel, extern veränderte/gelöschte Datei und Schreibfehler
testen, ohne produktive Daten zu überschreiben. F9 aus F2 öffnen und
gespeicherte Truppen neu laden. Verhalten bei 1024×640/DPI und Navigation
prüfen. CI baut/linkt; sie testet diese Fenster- und Dateiabläufe nicht.

**Noch offen für 19o.2 (echter Windows-Rechner):** F6 schließen, Karte mit
zwei Zonen und unterschiedlichen Zusatzfeldern laden; F2 öffnen, eine
markierte Zeile ändern, die Zeilen umordnen, eine neue ohne Nummer ergänzen,
OK und Strg+S. JSON auf erhaltene Zusatzdaten/Präzision und Zuordnung
kontrollieren. Zeilenlöschen und Ersetzen kaputter Daten mit Nein/Ja prüfen,
unzulässige Breite und doppelte Nummer testen (keine Teilübernahme), F2-
Layout/Tab/Enter bei 1024×640/DPI testen. F6-Guard und Terrain-Kollision
im Playtest prüfen.


**Noch offen für 19n (echter Windows-Rechner):** F6 schließen, F5 öffnen,
NPC mit eindeutiger Bezeichnung, Y-Versatz, Text und zwei Wegpunkten anlegen;
Schild und Prefab-Objekt hinzufügen, Kategorie wechseln, OK, Strg+S. F5
erneut öffnen und Daten prüfen; Abbrechen, ungültige Zahlen/Wegpunkte,
lange Texte, Umbenennen auf bestehenden Namen und den Button „Letzte
Cursorposition“ prüfen. Viewport, 1024×640/DPI und Tab/Enter prüfen; F4:
NPC patrouilliert und spricht mit E/Maus, Schild zeigt Text, Y wirkt.
Während F5 dürfen andere Dialoge/Projekt-/Kartenwechsel nicht eingreifen.


**Noch offen für 19m (echter Windows-Rechner):** In einem Testprojekt in F9
Gegner und Truppe anlegen und speichern; F2 öffnen, per „Neu laden“ und
„Hinzufügen“ dieselbe Truppe zweimal eintragen, Schritte setzen, OK und
Strg+S; F2 erneut öffnen und Einträge prüfen. 0/Bruch als Schrittzahl und
unbekannte ID prüfen (Dialog bleibt bei Ablehnung offen, andere Werte
unverändert); Abbrechen und leere Liste prüfen; Kartenbaum-Wechsel bei
offenem F2 versuchen; Projektwechsel mit offenem F9 darf nicht stattfinden.
Layout bei 1024×640, Tab/Enter im Mehrzeiler und einen
tatsächlichen Kampf im Playtest prüfen. CI baut/linkt nur und simuliert dies
nicht grafisch.

`log/live18k.ps1` (Kartenbaum: TVM_GETCOUNT/TVM_GETNEXTITEM prüfen, IntPtr-Rückgabewerte von SendMessage mit .ToInt64() vergleichen!) und `log/live18j.ps1` sind die aktuellsten funktionierenden Vorlagen (Resize-Timer: MAUSFREI, nur PostMessage von WM_ENTERSIZEMOVE/WM_EXITSIZEMOVE - kein SetCursorPos/mouse_event! Drossel-Falle: DWM drosselt Present für Hintergrund-Fenster auf ~8 fps, deshalb 2 s zwischen ENTER und EXIT warten sonst zählt der Timer 0 Frames). Danach log/live18i.ps1` (responsives Layout: Fenster per `SetWindowPos` umschalten, Control-Größen per `GetWindowRect`, Beschriftung per `WM_GETTEXT` — nur bei Standard-Controls erlaubt; `Viewport: W x H` im Log als Anker). Danach `log/live18h.ps1` (Probleme-Dialog: Testevent per Ruby-JSON-Injektion, Menü 116, Liste 5301 auslesen, Rescan 5302). `log/live18g.ps1` (Event-Dialog: Seitenliste 5046 per LB_SETCURSEL + WM_COMMAND-LBN_SELCHANGE schalten, Bedingungsfelder 5050/5049, Befehle pro Seite prüfen), `log/live18f.ps1` (Kategorien + choices), `log/live18e.ps1` (Kind-Fenster per FindWindowExW, WM_LBUTTONDOWN an den Viewport mit MK_SHIFT=0x4, Tasten per `[EdClick]::Key(vk)`), `log/live18d.ps1` (Flächen-Felder + Splat-JSON), `log/live18c.ps1` (Tileset-Basis) bleiben Referenz. Für neue Tests daran orientieren. **JSON-Vorbereitung für Live-Tests immer per Ruby-Skript** (live18h-Injektion), nie per PowerShell-Regex-Ersetzen.
