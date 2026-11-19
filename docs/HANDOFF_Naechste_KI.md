# Handoff für die nächste KI - Rune3D, Stand nach Etappe 19k

**Datum des Stands:** HEAD (nach Commit) — Etappe 19a: CI (GitHub Actions), portabler Build, Daten-Validierung
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
| 19k.1 | HEAD | Datenbank-Dialog (F9): vier Reiter Helden/Items/Gegner/Truppen, feldgesteuertes Formular (DB_FIELDS_*), Truppen-Gegnerliste komma-getrennt, sync_test prüft Feldtabellen gegen database.json |
| 19j | `74dc98f` (CI-Grün Run 44546218102) | Professionalisierung: `tools/sync_test.rb` (Ruby/C++-Vokabular maschinell abgeglichen: Befehlstypen+Enum+Labels, Trigger, Operatoren, Wetter, Self-Switches, Effekte, Interpreter-when-Listen, DB-Dialog vs. database.json) + README auf Stand 19d-19i |
| 19i | `05d3218` + Nacharbeit bis `50f9fec` (CI-Grün) | Zahleneingabe (input_number, UI.nummer_eingabe) + Kampf-Grundgerüst (battle.rb: Battler/Phasen/Sieg-Niederlage-Exp-Gold-Drop, database.json enemies+troops, Editor-Typen, CMD_TYPES 26); Tests battle_test.rb + script_test-Abschnitt 8 |
| 19h.2 | `4654729` + Nacharbeit bis `8031449` (CI-Grün) | RGSS an den User-Upload `XP-Defaultscript/` ausgerichtet: $game_temp/$game_system/$game_screen/$game_map/$game_player wie die Default-Scripts, Window_Selectable, Mover mit XP-Schrittnamen+Codes+turn_*+code-Parameter (Editor Wert 3), gameover/to_title-Flags; Tests erweitert |
| 19h | `1695951` + Nacharbeit bis `070da7e` (CI-Grün) | RGSS-Vertiefung: `graphics.rb` (Bitmap/Sprite/Window/Viewport, z-sortiert über `Graphics.draw`), `$scene` + `Graphics.scene_update`, `$game_temp`, `$data_actors/items/system`, `passable?` echt (Grenzen + Geländestufe), `RGSS.reset!` bei enter_map; Tests `graphics_test.rb` + rgss_test-Abschnitt |
| 19g | `e8ac3e5` + Nacharbeit `89eb6d6` (CI-Grün) | Bewegungsrouten (XP Move Routes): `mover.rb` (8 Schritte, repeat, wait, Ersetzung, Spieler/NPC-Ziele), Befehl `move_route` (24 Typen synchron), Editor-Formular ohne neue Controls, Context-Helfer, `Mover.reset!` bei enter_map; Tests `move_route_test.rb` + script_test-Abschnitt 7 |

Alle Etappen sind gebaut, live getestet (headless Ruby-Tests + Fenster-Injektion) und committed. Es gibt **keine uncommitteten Quelländerungen**; `opencode.json` ist lokaler Harness-Kram (steht jetzt im .gitignore).

**Nacharbeit 19a (notiert für die Historie):** Der erste CI-Lauf hat genau das getan,
wofür er da ist: `build.ps1 -Test` übergab g++ Argumente mit angehängtem Komma
(PowerShell-Argument-Modus — Array + `@`-Splatting nötig) und `tools/mapcheck_test.rb`
suchte Fundorte im `message`-Feld statt in `Finding#to_s`. Beides im Folge-Commit
repariert; der Windows-Job baute `Rune3D.exe` (1106 KB) und `RuneEditor.exe` (1584 KB)
mit GCC 16.2 warnungsfrei, `tex_import_test.rb` läuft dort dank `--strict` wirklich mit.

**Nacharbeit 18f (notiert für die Historie):** Der 18f-Stand lag beim Übergabezeitpunkt uncommittet im Baum (Handoff behauptete das Gegenteil). Er wurde verifiziert (Build + 14 Tests grün) und als `82ceab9` committet, bevor 18g begann.

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
  JSON-Brücke). Stand 19a: 17 Ruby-Tests + C++-JSON-Test (88 Checks).
  Achtung: Tests, die Engine-Module direkt laden, brauchen `require 'issues'` (Muster siehe tools/add_issues_requires.rb).
- **Sprache/Zeichen:** Prosa (Kommentare, UI- und Spieltexte, Doku, Namen in
  JSON) schreibt echte Umlaute; Identifier (Funktionen, JSON-Keys, IDs, Pfade,
  API-Namen) bleiben ASCII. Win32-Textaufrufe immer als W-Variante mit
  `utf8_to_wide`/`wide_to_utf8` — ANSI-Aufrufe verstümmeln UTF-8 zu „Ã¼".
  `.ps1`-Dateien mit Umlauten brauchen eine UTF-8-BOM (PowerShell 5.1).
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
  `ISSUE_CMD_TYPES` + `EV_TYPES` editor_main.cpp) und die Zähl-Assertion in
  `mapcheck_test.rb` anpassen.
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
- **Editor startet nur mit Ruby im PATH** (x64-ucrt-ruby340.dll); sonst kein Fenster und kein `log/editor.log` — ~~das ist kein Bug~~ **seit Etappe 18e+ behoben:** build.ps1 kopiert `x64-ucrt-ruby340.dll`, `libgmp-10.dll`, `libwinpthread-1.dll` + `ruby_builtin_dlls\` nach `build/` (SxS-Assembly, sonst 0xc0150002), winpthread wird statisch gelinkt (`-Wl,-Bstatic,--whole-archive -lwinpthread`), und `vm.cpp` setzt `RUBYLIB` auf die Ruby-Stdlib-Pfade (sonst `LoadError: json` bei Doppelklick-Start). Beide EXEn starten jetzt ohne Ruby-PATH per Doppelklick.
- Keine Wiederhol-Schleifen: gleiches Problem zweimal → Quelle lesen, Ansatz wechseln. Jede Runde endet mit Status (fertig / blockiert + Grund).
- **Backlog-Disziplin (seit 18g):** Fehler/Grenzen, die NICHT sofort in der laufenden Etappe behoben werden, gehören in `docs/BACKLOG.md` (Symptom, Vermutung, betroffene Stellen) — nicht in wiederholte Test-Läufe. Tests laufen pro Etappe genau einmal (Testsuite + ein Live-Test); nur bei rotem Test wird nachgelesen, nicht blind neu gestartet.

## 2. Architektur-Kurzreferenz

- **Spiel:** `src/main.cpp` (Schleife, F5=Ruby-Reload) → Ruby `game/engine/boot.rb` (Requires) → `game/main.rb` (`Game.setup/update/draw`, Launcher, Menü, Playtest in-game via F4 seit 16b).
  **Module:** `Database` (JSON laden, `database.json` seit 17g), `GameState`, `Party`+Actor (17g), `Event`/`Npc`/`Interpreter`, `Scripts` (Sandbox `Scripts::Context` mit Helfern say/switch/variable/teleport/sound/effect/item/gold/heal/weather…), `Pick`, `Effects`+`Weather` (Partikel via `Rune::Gfx.box`), `Textures`/`Sounds` (prozedural), `Project`, `Launcher`, `MapCheck` (19a: Karten-Daten
  prüfen, dieselben Regeln wie `issues_scan_map` im Editor; `Game.enter_map` meldet harte
  Fehler an `Issues`).
- **Editor:** `src/editor/editor_main.cpp` (eine große Datei, ~4700 Zeilen). Eigene Fensterklassen: `RuneEditorMain`, Viewport `RuneEditorView`, Dialoge `RuneEditorDlg`/`DlgNew` (Karten), `RuneEditorEventDlg`, `ScriptDlg`, `MatDlg`, `DbDlg`, `SoundDlg` — **alle modeless**.
- Editor-IDs: Menü 100–115 (115=TILESET; 112=SOUNDTEST, 113=PLAYTEST, 114=PROJECT), Haupt-Controls 3001–3029 (Toolbar 3025, Boden-malen-Radio 3026, Kartenbaum 3027, **Höhe-malen-Radio 3029**), Karten-Dialog 4100–4114 (Wetter 4113/4114), Projekt-Dialog 4201–4205, Tileset-Dialog 4301–4319 (Liste 4301, Kachelgröße 4303, Übernehmen 4304, Speichern 4305, Erzeugen 4306, Farben/Muster/Name 4307–4310, Schließen 4311, Vorschau 4312, Splat-Flächen X/Z/B/T 4313–4316, Hinzufügen 4317, Entfernen 4318, Flächen-Liste 4319), Event 5001–5045, Script 6001–6016, Material 7001–7013, DB 8001–8055, Sound 8101–8107. F-Tasten: F1 Hilfe, F2 Eigenschaften, F3 Materialbase, F4 Playtest, F6 Events, **F7 Tileset**, F8 Sound-Test, F9 Datenbank, F11 Scripts.
- **Toolbar:** 8 Buttons, Icons prozedural per GDI gezeichnet (`make_toolbar_icon` + Namensraum `ico`), senden Menü-IDs via `WM_COMMAND`. F-Tasten: F1 Hilfe, F2 Eigenschaften, F3 Materialbase, F4 Playtest, F6 Events, F8 Sound-Test, F9 Datenbank, F11 Scripts.
- **Playtest:** Menü/Toolbar F4 → `save_map()` + `CreateProcessW("build\Rune3D.exe", "--project dorf")`. Spiel: `main.cpp` setzt `RUNE_PROJECT`, `game/main.rb` (`Game.setup`) lädt das Projekt direkt (ohne Titelbildschirm); unbekannter Name → Launcher + Warnung.
- **Wetter:** Karten-JSON `"weather":{"type":"kein|regen|schnee","intensity":1.0}`; `Weather.load_map/update/draw`; Interpreter `{"cmd":"weather",...}`; Editor-Dialog F2 schreibt das Feld.
- **Link-Falle (`fclose`-Crash):** In `build.ps1` muss `-lucrtbase` **vor** `-lx64-ucrt-ruby340` stehen, sonst leitet der Linker Editor-stdio auf Ruby-CRT-Exports um → Zugriffsverletzung. Kommentar dort steht, nicht „aufräumen".
- 17f-Import: WIC → rohe `<name>.rgba` + `textures.json` unter `projekte/<p>/data/textures/`; Spiel lädt via `Textures.register_project`. CLI: `RuneEditor.exe --import-tex <bild> [name]`.

## 3. Was gut funktioniert hat (beibehalten)

- Kopflose Ruby-Tests mit `Rune::*`-Stub-Modulen (Muster: `tools/party_test.rb`, `tools/weather_test.rb`) — jede Etappe einen, alle in der Kommandozeile kürzbar prüfbar.
- Etappen-Disziplin: implementieren → bauen → 13 Tests → Live-Skript → `tools/update_docs17*.rb` → Commit. Genau so weitermachen.
- Demo-Datenboth: `game/data/` ist Vorlage; `Project.ensure_demo` kopiert sie komplett in neue Projekte (database.json, scripts.json kommen so automatisch mit).
- `docs/architecture.md` hat pro Etappe einen Abschnitt (Anker `## Ruby-API (Stand Etappe 2)` + je Etappe davor eingefügt) — neue Abschnitte nach demselben Muster davor einfügen.

## 4. Bekannte Grenzen / nächste Kandidaten

**Backlog:** Offene Fehler, Grenzen und Test-Wissen stehen zentral in `docs/BACKLOG.md` — dort nachsehen und neue Erkenntnisse dorthin eintragen (nicht mehr hier listen).

- Der Tileset-Editor ist seit 18c gebaut; seit 18d unterstützt er **Splat-Böden**: `terrain.splats` (Liste `{tex,x,z,w,h}` in Weltmetern) im Karten-JSON, Atlas aus allen 64×64-Texturen (`Renderer::update_atlas`), pro-Quad-UV ohne geteilte Vertices. Ohne Splats bleibt der alte Ein-Textur-Pfad byte-identisch.
- **Splat-Grenzen:** kein Weich-Übergang (Quad = eine Textur), Kacheldichte folgt der Terrain-Auflösung, `terrain.tile` wirkt im Splat-Pfad nicht. `Rune::Gfx.terrain_splats(base, liste)` nimmt String- UND Symbol-Schlüssel (JSON.parse liefert Strings — lange gesucht, siehe 18d).
- **Mal-Werkzeug (18e):** Radio „Boden malen" (3026); Linksklick malt, Shift+Klick radiert (MK_SHIFT = 0x4, **nicht 0x8**), +/- ändert die Pinselgröße (1–64 m), Pinseltextur folgt der Tileset-Auswahl. Viewport ist ein KIND-Fenster: im Live-Test per `FindWindowExW(main, 0, 'RuneEditorView', $null)` finden, WM_LBUTTONDOWN direkt dorthin senden. `splat_edit()` ist der gemeinsame Kern (auch der Tileset-Dialog nutzt ihn).
- **Problem-Backlog (18h):** Der Editor scannt die Karte nach jedem Laden (`issues_scan_map()`, Abschnitt vor `load_map` — steht VOR der Event-Editor-Sektion und nutzt deshalb die eigene Liste `ISSUE_CMD_TYPES`, nicht `EV_TYPES`!). Funde: unbekannte Prefab-Typen/Material-Overrides/Trigger/Befehle, kaputte Seiten + Bedingungen, fehlende then/else/options/branches/lines. Probleme-Dialog F10 (ID_MENU_PROBLEMS 116, IDs 5301–5304, Klasse `RuneEditorProblemsDlg`). Spiel: `game/engine/issues.rb` (`Issues.report(area, msg, severity:)` → Log + `log/issues.json`, `Issues.clear!` in `Game.setup`). Verdrahtet: Interpreter, Scripts, Party, Weather, Textures. Tests laden ohne boot.rb → dort `require 'issues'` ergänzen.
- **Responsives Layout (18i):** `layout_children()` rechnet die Spaltenbreiten aus der Fensterbreite: `left_w = clamp(W*16/100, 214, 280)`, `right_w = clamp(W*19/100, 258, 340)`, Viewport bekommt den Rest. Die Werkzeugliste bekommt den freien Platz, der Bedienblock (7 Zeilen à 30 px, 6 px Lücke = 246 px) wird **darunter** verteilt — vorher standen feste y-Werte im Code, dadurch schoben sich Undo/Redo bei 1024×640 in die Statusleiste und bei großen Fenstern blieb unten alles leer. Undo/Redo jetzt **volle Spaltenbreite** (alte 96 px schnitten „(Strg+Z)/(Strg+Y)" ab). Statusleisten-Felder proportional (`W/8, W/9, W/6, -1`). Neu: `renderer_resize()` schreibt bei Änderung `Viewport: W x H` ins Log. Live-Test: `log/live18i.ps1` (18 Checks). Lambda-Falle: `move_child(HWND h, …)` kollidiert mit `int h` des Aufrufers → Parameter heißen `hwnd`/`ht`.
- **Resize-Fakten (belegt):** Swapchain/Viewport folgen korrekt (`ResizeBuffers` fehlerfrei, Screenshot-Größe == Viewport-Größe), auch das Spiel behandelt `WM_SIZE` (`window.cpp` → `consume_resize()` → `renderer.resize()`). Bei Resize-Verdacht zürst `log/editor.log` auf `Viewport: …` prüfen. F12-Screenshot geht **nur** per echtem Tastendruck (`[EdClick]::Key(0x7B)`) — `PostMessage(WM_KEYDOWN)` wirkt nicht, weil `handle_shortcuts` `GetAsyncKeyState` fragt.
- **Event-Seiten (18g):** Events haben `pages` (Liste aus `{conditions, trigger, commands}`); die Seite mit dem **höchsten Index** mit erfüllten Bedingungen gewinnt (Seite 0 = Standard, leere Bedingungen = immer). Bedingungen: `switch` {name,value} und `variable` {name,op,value} (UND-verknüpft). `Event.refresh_page!` läuft in main.rb vor den Trigger-Schleifen; **Seitenwechsel setzt `fired` zurück** (touch/auto feuern erneut). Alte Karten ohne `pages` → implizite Seite 0; der Editor migriert das geöffnete Event sofort (`ev_ensure_pages`) und speichert dann ohne Top-Level-trigger/commands. Editor-IDs: Seitenliste 5046, Neue Seite 5047, Seite löschen 5048, Seite übernehmen 5049, Bedingungsfelder 5050–5054. Der Trigger-ComboBox (5015) sitzt jetzt in der Gruppenbox „Seite". Tests: `tools/event_pages_test.rb` (44 Checks), `log/live18g.ps1`.
- **Event-Editor (18f):** 20 Befehlstypen mit Kategorie-Präfixen (Enum EvType ersetzt magische Indizes!), `choices` (Wahlmöglichkeiten mit Zweigen als „Wenn [x]:"-Kopfzeilen), `toggle_switch`, `move_player`, `weather`, `save_game`/`load_game`, Trigger `parallel`. Neue Tests: `tools/event18f_test.rb` (23 Checks, Mock-Muster wie event_test.rb). **Live-Test-Falle:** Der Event-Dialog blockiert Strg+S des Hauptfensters (`event_editor_focused()`) → im Test erst Dialog schließen (IDEV_CLOSE 5005), dann speichern. `load_game` im Interpreter baut die Karte neu auf und ersetzt den laufenden Interpreter (dokumentiert).
- **Nächste Kandidaten:** die beiden User-Wünsche aus `docs/BACKLOG.md` (Platzier-Vorschau
  statt Sofort-Einfügen + bessere Objektliste; NPC-/Platzier-Dialog mit X/Y/Z und
  Text-Objekten), danach Move-Routes, Self-Switches, Kampf-Befehle, Fade, Zahleneingabe,
  Passierbarkeits-Regeln, Undo/Redo für Splats. Seit 19a ist jeder dieser Schritte durch
  CI abgesichert (Windows-Job kompiliert den Editor wirklich).
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

`log/live18k.ps1` (Kartenbaum: TVM_GETCOUNT/TVM_GETNEXTITEM prüfen, IntPtr-Rückgabewerte von SendMessage mit .ToInt64() vergleichen!) und `log/live18j.ps1` sind die aktuellsten funktionierenden Vorlagen (Resize-Timer: MAUSFREI, nur PostMessage von WM_ENTERSIZEMOVE/WM_EXITSIZEMOVE - kein SetCursorPos/mouse_event! Drossel-Falle: DWM drosselt Present für Hintergrund-Fenster auf ~8 fps, deshalb 2 s zwischen ENTER und EXIT warten sonst zählt der Timer 0 Frames). Danach log/live18i.ps1` (responsives Layout: Fenster per `SetWindowPos` umschalten, Control-Größen per `GetWindowRect`, Beschriftung per `WM_GETTEXT` — nur bei Standard-Controls erlaubt; `Viewport: W x H` im Log als Anker). Danach `log/live18h.ps1` (Probleme-Dialog: Testevent per Ruby-JSON-Injektion, Menü 116, Liste 5301 auslesen, Rescan 5302). `log/live18g.ps1` (Event-Dialog: Seitenliste 5046 per LB_SETCURSEL + WM_COMMAND-LBN_SELCHANGE schalten, Bedingungsfelder 5050/5049, Befehle pro Seite prüfen), `log/live18f.ps1` (Kategorien + choices), `log/live18e.ps1` (Kind-Fenster per FindWindowExW, WM_LBUTTONDOWN an den Viewport mit MK_SHIFT=0x4, Tasten per `[EdClick]::Key(vk)`), `log/live18d.ps1` (Flächen-Felder + Splat-JSON), `log/live18c.ps1` (Tileset-Basis) bleiben Referenz. Für neue Tests daran orientieren. **JSON-Vorbereitung für Live-Tests immer per Ruby-Skript** (live18h-Injektion), nie per PowerShell-Regex-Ersetzen.
