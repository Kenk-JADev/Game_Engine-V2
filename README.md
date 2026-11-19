# Rune3D — 3D-RPG-Engine und Editor im Stil von RPG Maker XP

![CI](https://github.com/Kenk-JADev/Game_Engine-V2/actions/workflows/ci.yml/badge.svg?branch=main)

Rune3D ist eine Windows-Only-3D-RPG-Engine mit eigenem Editor. Das Vorbild ist
**RPG Maker XP** — dieselbe Arbeitsweise (Kartenliste, Event-Editor mit Seiten
und Bedingungen, Datenbank, Playtest), aber in 3D: frei begehbares Gelände,
Kamera in erster und dritter Person, gemalte Böden und Berge.

Zwei Programme, ein Datenbestand:

| Programm | Quelle | Zweck |
|---|---|---|
| `Rune3D.exe` | `src/main.cpp` + `game/` | das Spiel (Titelbildschirm, Playtest, Laden) |
| `RuneEditor.exe` | `src/editor/editor_main.cpp` | der Editor (Karten, Events, Tilesets, Datenbank) |

Beide sind C++20 (Win32 + Direct3D 11, kein Fremd-Framework). Die Spiellogik
liegt vollständig in **echtem CRuby 3.4** (`game/engine/*.rb`), eingebettet über
`src/ruby/vm.cpp` — hot-reloadbar mit F5, genau wie beim Vorbild Scripte zur
Laufzeit neu geladen werden.

## Was es kann

**Editor** (alles moduslos, mehrere Fenster gleichzeitig):

- Kartenbaum wie die XP-Kartenliste: anlegen, umbenennen (inplace), löschen,
  Doppelklick = Eigenschaften
- rechteckige Karten mit eigener Größe, Nebel, Panorama, Wetter
- Objekte setzen / auswählen / verschieben / drehen / duplizieren / löschen,
  Undo + Redo, Raster
- **Boden malen**: mehrere Boden-Texturen pro Karte (Splat-Atlas), Pinsel mit
  einstellbarer Größe, Shift = Radierer
- **Höhe malen**: Berge und Senken direkt im Viewport, mehrfach klicken stapelt
- Tileset-Editor: Boden-Textur, Kachelgröße, eigene Kacheln erzeugen
- Event-Editor mit **Seiten** (Bedingungen über Schalter/Variablen/Self-
  Switches), Triggern (`action`, `touch`, `auto`, `parallel`) und 26 Befehlen
  in Kategorien (inkl. Bewegungsroute, Zahleneingabe, Kampf starten)
- Script-Editor, Materialbase (PNG/BMP-Import per WIC, auch per Drag & Drop),
  Datenbank (Helden, Items, Gold), Sound-Test, Probleme-Dialog (F10)
- Playtest: startet `Rune3D.exe --project <name>` mit der gespeicherten Karte

**Engine**: Third-Person-Spieler mit Physik (Schwerkraft, Sprung, `solid`-Teile
als Hindernisse, Kartengrenze), Event-Interpreter mit Verzweigungen und
Wahlmöglichkeiten, Party/Helden mit Items und Gold, **rundenbasiertes
Kampf-Grundgerüst** (Angriff/Item/Flucht, Exp/Gold/Beute, Game-Over-Flag),
**Zahleneingabe** wie XP „Input Number", NPC-Patrouillen mit Wegpunkten und
**Bewegungsrouten** (XP-Schrittnamen und -Codes), Bildschirmblende,
Partikel-Effekte, Wetter (Regen/Schnee), prozedurale Texturen und Sounds,
Spielstände in drei Slots, Projektverwaltung. Dazu eine **RGSS-Schicht**:
`$game_system/$game_switches/$game_variables/$game_party/$game_player/
$game_map/$game_screen/$game_temp`, `Graphics`, `Input`, `Audio`,
`Sprite/Window/Viewport`, `$scene` — eigene Scripte laufen wie beim Vorbild
(Script-Editor, F5 = neu laden).

## Vergleich mit RPG Maker XP

| XP-Funktion | Rune3D | Stand |
|---|---|---|
| Kartenliste / Karten-Eigenschaften | Kartenbaum + Dialog | ✅ |
| Event-Editor, Seiten, Bedingungen | 26 Befehle, 4 Trigger, Self-Switches A-D | ✅ |
| Schalter & Variablen | `GameState` | ✅ |
| Datenbank (Helden, Items) | Helden, Items, Gold, Gegner, Truppen | ✅ teilweise |
| Tileset / Boden | Splat-Böden, Höhen-Malwerkzeug | ✅ |
| Script-Editor (Ruby) | `Scripts`-Sandbox + Editor | ✅ |
| Playtest | F4 | ✅ |
| Nebel / Panorama / Wetter | Karten-JSON + Interpreter | ✅ |
| Kampfsystem | Rundenkampf-Grundgerüst (`battle.rb`): Angriff/Item/Flucht, Exp/Gold/Drops; keine Skills/Zustände | ✅ teilweise |
| Bewegungsfolgen (Move Routes) | `mover.rb`, XP-Schrittnamen + Codes, Routen im Event-Befehl | ✅ |
| Self-Switches | A-D, Seiten-Bedingung + Befehle | ✅ |
| Zahleneingabe (Input Number) | `input_number`-Befehl, Ziffernfeld 1-6 Stellen | ✅ |
| Fade (Bildschirmblende) | `fade`-Befehl, ein/aus mit Sekunden | ✅ |
| RGSS-Script-Schicht | `$game_*`, `Graphics`, `Sprite/Window`, `$scene` — ohne Tilemap/Bitmap-Dateien | ✅ teilweise |
| Ballons, Animationen, Tilemap | — | ❌ fehlt |
| Autotiles, weiche Boden-Übergänge | ein Quad = eine Textur | ❌ fehlt |
| Charakter-Grafiken | Figuren sind Quader | ❌ fehlt |
| Passierbarkeit (blockierte Zonen) | nur `solid`-Objekte | ❌ fehlt |

Offene Punkte mit Symptom und betroffener Datei stehen in
[`docs/BACKLOG.md`](docs/BACKLOG.md).

## Voraussetzungen

- Windows 10/11 x64 mit Direct3D 11
- **Ruby 3.x x64** (RubyInstaller mit DevKit; entwickelt auf 3.4.11 unter
  `C:\Ruby34-x64`). Header und Importbibliothek werden aus `RbConfig` gelesen,
  der Installationspfad ist also frei wählbar.
- **MSYS2-ucrt64-g++** (liegt beim RubyInstaller-DevKit unter
  `<Ruby>\msys64\ucrt64`)

## Bauen

```powershell
.\build.ps1              # build\Rune3D.exe + build\RuneEditor.exe
.\build.ps1 -Clean       # komplett neu
.\build.ps1 -Run         # bauen und das Spiel starten
.\build.ps1 -Test        # Testsuite (C++-JSON-Test, Ruby-Tests, JSON-Brücke)
.\build.ps1 -RubyRoot D:\Ruby34-x64   # andere Ruby-Installation
```

`build.ps1` kopiert die Laufzeit-DLLs (`x64-ucrt-ruby340.dll`, `libgmp-10.dll`,
`libwinpthread-1.dll`, `ruby_builtin_dlls\`) neben die EXEs — beide Programme
starten danach per Doppelklick, ohne dass Ruby im PATH stehen muss.

Vor dem Bauen laufende Instanzen beenden, sonst schlägt das Linken mit
„Permission denied" fehl:

```powershell
Get-Process RuneEditor,Rune3D -ErrorAction SilentlyContinue | Stop-Process -Force
```

## Testen

Drei Ebenen, alle headless:

```powershell
.\build.ps1 -Test                              # alles zusammen
ruby tools\test_all.rb                         # alle Ruby-Modultests
ruby -I game\engine game\engine\mapcheck.rb    # Karten-Daten prüfen
ruby tools\lint_repo.rb                        # Repo-Hygiene
```

| Ebene | Was | Wo |
|---|---|---|
| Ruby-Modultests | Engine-Logik mit `Rune::*`-Stubs (Events, Seiten, Interpreter, Party, Kampf, Wetter, Scripte, Projekte, Speichern) + Ruby/C++-Vokabular-Sync | `tools/*_test.rb`, Runner `tools/test_all.rb` |
| C++-Logiktest | JSON-Schicht des Editors: Parser, Schreiber, Zahlen-Treue, Umlaute, Roundtrip echter Karten | `tools/json_test.cpp` |
| JSON-Brücke | Schreibt C++ bytegleich zu Ruby `JSON.pretty_generate`? | `tools/json_bridge_check.rb` |
| Karten-Scan | Daten-Validierung (Befehle, Trigger, Bedingungen, Wegpunkte, Terrain) | `game/engine/mapcheck.rb` |
| Repo-Hygiene | UTF-8, BOM, Doppelkodierung, Zeilenenden, Requires, Index | `tools/lint_repo.rb` |
| Live-Tests | Fenster-Injektion am echten Editor (nur Windows, manüll) | `tools/edclick.psm1` |

`tools/tex_import_test.rb` braucht eine gebaute `RuneEditor.exe` und wird ohne
sie übersprungen (der Runner meldet das).

## Continuous Integration

[`.github/workflows/ci.yml`](.github/workflows/ci.yml) läuft bei jedem Push und
Pull Request:

| Job | Runner | Inhalt |
|---|---|---|
| `hygiene` | ubuntu | `tools/lint_repo.rb` |
| `engine` | ubuntu | `tools/test_all.rb` + Karten-Scan |
| `cpp-logik` | ubuntu | `tools/json_test.cpp` mit `-Werror`, danach JSON-Brücke |
| `windows` | windows-2022 | `build.ps1 -Clean` **und** `-Test`, EXEs als Artefakt |

Der Windows-Job ist der einzige Ort, an dem D3D11/Win32 wirklich kompiliert und
gelinkt wird — die EXEs lassen sich dort nicht starten (keine GPU im Runner),
es geht um Bau und Tests. Artefakt: **Rune3D-Windows**.

## Projektstruktur

```
build.ps1            Build + Tests (portable Werkzeug-Erkennung)
src/
  main.cpp           Spiel: Schleife, Kamera, F5 = Ruby-Reload
  editor/            RuneEditor: editor_main.cpp (Fenster, Dialoge), json.cpp
  ruby/vm.cpp        CRuby einbetten + Rune::*-Bindings
  gfx/               Renderer (D3D11), Kamera, Immediate-UI
  world/             Terrain, Physik, Atlas
  core/              Fenster, Sound, Log
game/
  main.rb            Game.setup/update/draw, Menü, Playtest
  engine/            Module: Event, Interpreter, Party, Npc, Weather, ...
  data/              Vorlage-Daten (Karten, Datenbank, Prefabs, Scripte)
projekte/<name>/     ein Projekt: data/ (Karten, Datenbank, Texturen), saves/
tools/               Tests, Prüfer, Einmal-Skripte der Etappen
docs/                architecture.md, BACKLOG.md, HANDOFF_Naechste_KI.md
```

## Bedienung

**Editor** (F1 zeigt dieselbe Liste im Programm):

| Eingabe | Wirkung |
|---|---|
| rechte Maustaste halten | umsehen |
| W/A/S/D, Q/E | bewegen, runter/hoch |
| Mausrad, Shift+Rad | Zoom, Höhe — Shift = schnell |
| Linksklick | setzen / auswählen; im Verschieben-Modus ziehen |
| Boden malen | Linksklick = Fläche, Shift+Klick = Radierer |
| Höhe malen | Linksklick = Berg, Shift+Klick = Senke, `+`/`-` = Pinselgröße |
| R, Entf | Auswahl drehen (+30°), löschen |
| Strg+Z / Strg+Y, Strg+S | Undo / Redo, Karte speichern |
| G, Esc, Enter | Raster umschalten, Auswahl aufheben, Eigenschaften übernehmen |
| Doppelklick in der Objektliste | Kamera aufs Objekt richten |
| PNG/BMP ins Fenster ziehen | Textur importieren |
| F1 Hilfe · F2 Karten-Eigenschaften · F3 Materialbase · F4 Playtest · F6 Event-Editor · F7 Tileset · F8 Sound-Test · F9 Datenbank · F10 Probleme · F11 Script-Editor · F12 Screenshot | |

**Spiel**: WASD laufen, Shift rennen, Leertaste springen, E benutzen/reden,
Maussteuerung Blick, Esc Menü, F2 Debug-Ausgabe, F4 Playtest umschalten,
F5 Scripte neu laden.

## Datenformate

Karten sind JSON unter `projekte/<name>/data/maps/<id>.json`:

```json
{
  "name": "Dorf",
  "terrain": { "width": 240.0, "height": 240.0, "resolution": 192,
               "heights": [{ "x": 22.0, "z": 13.0, "r": 4.0, "h": 2.0 }],
               "splats":  [{ "tex": "weg", "x": 0.0, "z": 0.0, "w": 10.0, "h": 4.0 }] },
  "objects": [{ "type": "haus", "x": 0.0, "z": -4.0, "rot": 0.0 }],
  "events":  [{ "id": 1, "name": "Wache", "x": 0.0, "z": 4.5, "radius": 3.2,
                "pages": [{ "conditions": {}, "trigger": "action",
                            "commands": [{ "cmd": "text", "lines": ["Hallo"] }] }] }],
  "npcs":    [{ "name": "Wanderer", "x": 2.0, "z": 6.0, "speed": 1.2,
                "path": [[4.0, 4.0], [4.0, -1.0]], "commands": [] }],
  "fog": { "on": false, "far": 120.0, "color": 15263976 },
  "weather": { "type": "kein", "intensity": 1.0 }
}
```

Regeln: Editor und Spiel schreiben beide `JSON.pretty_generate`-Format (LF,
2 Leerzeichen, kürzeste exakte Zahlendarstellung) — deshalb wirft Speichern
keine Diff-Flut. Unbekannte Schlüssel bleiben beim Speichern erhalten.
Spielstände liegen in `projekte/<name>/saves/slot1..3.json`.

## Dokumentation

- [`docs/architecture.md`](docs/architecture.md) — Aufbau, Ruby-API, Etappen
- [`docs/BACKLOG.md`](docs/BACKLOG.md) — offene Fehler und Grenzen, mit Symptom
  und betroffener Datei
- [`docs/HANDOFF_Naechste_KI.md`](docs/HANDOFF_Naechste_KI.md) — Arbeitsregeln,
  Etappen-Tabelle, bekannte Fallstricke

## Arbeitsweise

Eine Etappe = ein Commit: implementieren → bauen → Tests → Live-Test →
Dokumentation (`docs/`) → Commit. Fehler, die nicht in die laufende Etappe
gehören, werden in `docs/BACKLOG.md` eingetragen statt sie in Test-Schleifen zu
verfolgen. Antworten, Kommentare und Dokumentation sind auf Deutsch; Log-Strings
im Code bleiben transliteriert (`ae`/`oe`/`ue`), weil die Live-Tests darauf
matchen.
