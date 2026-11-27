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

**Editor** (Nebenfenster meist moduslos; F5 ist modal):

- Hauptfenster mit Menü-/Werkzeugleiste, scrollbarer Palette links über dem
  Kartenbaum, zentraler 3D-Arbeitsfläche, Objektinspektor und Statusleiste;
  die Aufteilung links ist verstellbar
- Kartenbaum wie die XP-Kartenliste: anlegen, umbenennen (inplace), löschen,
  Doppelklick = Eigenschaften; Rechtsklick wählt zuerst die getroffene Karte
- rechteckige Karten mit eigener Größe, Nebel, Panorama, Wetter
- Objekte setzen / auswählen / verschieben / drehen / duplizieren / löschen,
  Undo + Redo, Raster
- **Boden malen**: mehrere Boden-Texturen pro Karte (Splat-Atlas), Pinsel mit
  einstellbarer Größe, Shift = Radierer
- **Höhe malen**: Berge und Senken direkt im Viewport, mehrfach klicken stapelt
- Tileset-Editor: Boden-Textur, Kachelgröße, eigene Kacheln erzeugen
- Event-Editor mit **Seiten** (Bedingungen über Schalter/Variablen/Self-
  Switches), Triggern (`action`, `touch`, `auto`, `parallel`) und 73 Befehlen
  in Kategorien (inkl. Bewegungsroute, Zahleneingabe, Kampf starten)
- NPCs und Events in der Kartenansicht auswählen und per Doppelklick bearbeiten;
  Spieler-Startposition per Rechtsklick auf das Gelände setzen
- RGSS-/Ruby-Script-Editor mit Zeilennummern, Materialbase (PNG/BMP-Import per WIC, auch per Drag & Drop),
  Datenbank mit **13 Registerkarten wie RPG Maker XP** (Helden, Klassen,
  Fertigkeiten, Items, Waffen, Rüstungen, Gegner, Truppen, Zustände,
  Animationen, Tilesets, Gemeinsame Events, System) samt Startgold, Sound-Test,
  Probleme-Dialog (F10)
- Playtest: speichert die ausgewählte Karte und startet
  `Rune3D.exe --project <name> --map <karten-id>` als eigenen Prozess.
  Eine ausdrücklich gesetzte Projekt-Startposition gilt auf ihrer Karte;
  sonst gilt der Kartenstart (oder ersatzweise der Projektstart). Ohne
  Kartenauswahl gilt die Projekt-Startkarte; eine fehlende Karte meldet einen Fehler.

**Engine**: Third-Person-Spieler mit Physik (Schwerkraft, Sprung, `solid`-Teile
als Hindernisse, Kartengrenze), Event-Interpreter mit Verzweigungen und
Wahlmöglichkeiten, Party/Helden mit Items und Gold, **rundenbasiertes
Kampf-Grundgerüst** (Angriff/Fertigkeit/Item/Flucht, Fertigkeiten mit
MP-Kosten, Zustände, Exp/Level-Up/Gold/Beute, Game-Over-Flag),
**Zahleneingabe** wie XP „Input Number", NPC-Patrouillen mit Wegpunkten und
**Bewegungsrouten** (XP-Schrittnamen und -Codes), Bildschirmblende,
Partikel-Effekte, Wetter (Regen/Schnee), prozedurale Texturen und Sounds,
Spielstände in drei Slots, Projektverwaltung. Dazu eine **RGSS-Schicht**:
`$game_system/$game_switches/$game_variables/$game_party/$game_player/
$game_map/$game_screen/$game_temp`, `Graphics`, `Input`, `Audio`,
`Sprite/Window/Viewport`, `$scene` — eigene Scripte laufen wie beim Vorbild
(Script-Editor, F5 = neu laden). Der frühere Ingame-Editor wurde entfernt:
Karten werden in `RuneEditor.exe` bearbeitet; F4 im Spiel öffnet keinen Editor.

### Kampf bedienen

Im Kampf wählt jeder lebende Held mit ↑/↓ einen Befehl und bestätigt mit
**E**, **Enter** oder **Leertaste**. Angriffe wählen mit den Pfeiltasten
einen lebenden Gegner. Bei **Item** zuerst den Heil- oder Manatrank und
anschließend mit den Pfeiltasten den gewünschten Party-Helden auswählen;
die Auswahl ist im Status markiert. Heiltränke können auch Helden mit 0 HP
wiederbeleben. Ein volles bzw. ungültiges Ziel wird nicht bestätigt.

**Esc** geht von der Zielwahl zur Itemliste bzw. von dort zum Befehl zurück;
in der Gegnerzielwahl verwirft es den Angriff. Bei der Befehlswahl versucht
Esc eine Flucht, sofern sie erlaubt ist, und öffnet nicht das Spielmenü.
Jeder erneute Tastendruck erfordert Loslassen. Esc gibt zusätzlich den
nativen Mausfang frei; die mittlere Maustaste schaltet den Mausblick wieder an.
Ein fehlgeschlagener Fluchtversuch verwirft die vorgemerkten Aktionen und
lässt die Gegner ziehen. Items werden für die Runde reserviert und erst bei
wirksamer Ausführung abgezogen. Nach einem Sieg verbrauchen spätere Aktionen
kein Item mehr. Besiegte Angriffsziel-Gegner werden durch einen lebenden
Gegner ersetzt. Aktuelle und maximale HP/MP bleiben mit der Party verbunden.

Ruby-Scripts können ebenfalls gezielt anwenden:

```ruby
use_item('heiltrank', 'mira')                  # Script-Context
$game_party.use_item('manatrank', target: 'mira')
Party.use_item('heiltrank', target: Party.find('mira'))
```

Ein Ziel ist eine Helden-ID oder eine aktuelle `Party::Actor`-Referenz.
Gezielt wird nur bei Wirkung verbraucht. Für alte Aufrufe **ohne** Ziel
bleibt das frühere Verhalten: der erste verletzte Held bzw. Held mit
fehlenden MP wird gewählt, andernfalls der Party-Anführer; dabei kann auch
bei vollen Werten ein Item verbraucht werden. Nur positive Heil-/Manastärke
ist benutzbar. Der grafische Kampf-Live-Test ist noch offen:
[`docs/WINDOWS_BATTLE_LIVE_TEST.md`](docs/WINDOWS_BATTLE_LIVE_TEST.md).

## Vergleich mit RPG Maker XP

| XP-Funktion | Rune3D | Stand |
|---|---|---|
| Kartenliste / Karten-Eigenschaften | Kartenbaum + Dialog | ✅ |
| Event-Editor, Seiten, Bedingungen | 73 Befehle, 4 Trigger, Self-Switches A-D | ✅ |
| NPCs / Schilder / Objektpositionen | F5-Platzier-Editor, Text, Wegpunkte und Y-Bodenversatz | ✅ teilweise |
| Schalter & Variablen | `GameState` | ✅ |
| Datenbank: 13 Register, Liste + Formular | F9 mit allen 13 XP-Registern (Etappe 41): 7 davon liest die Engine, 6 sind vorbereitet (Klassen/Waffen/Rüstungen/Animationen/Tilesets/System); Gemeinsame Events inkl. Befehlsliste editierbar; kein „Change Maximum" | ✅ teilweise |
| Tileset / Boden | Splat-Böden, Höhen-Malwerkzeug | ✅ |
| Script-Editor (Ruby/RGSS) | Zeilennummern, Syntaxprüfung, opt-in Startscripts; keine Sandbox | ✅ teilweise |
| Playtest | F4 startet die gewählte Karte im eigenen Spielprozess; keine Sandbox | ✅ teilweise |
| Nebel / Panorama / Wetter | Karten-JSON + Interpreter | ✅ |
| Kampfsystem | Rundenkampf (`battle.rb`): Angriff/Fertigkeit/Item/Flucht, Fertigkeiten mit MP-Kosten und Zielwahl, Zustände mit Dauer (Gift/Regeneration/Wut/Schutz), Exp mit XP-Stufenaufstieg, Gold/Drops; Zufallskämpfe pro Karte im F2-Dialog; offen: Gegner-Fertigkeiten/-KI (`skills`/`states` sind seit Etappe 41 in F9 editierbar) | ✅ teilweise |
| Bewegungsfolgen (Move Routes) | `mover.rb`, XP-Schrittnamen + Codes, Routen im Event-Befehl | ✅ |
| Self-Switches | A-D, Seiten-Bedingung + Befehle | ✅ |
| Zahleneingabe (Input Number) | `input_number`-Befehl, Ziffernfeld 1-6 Stellen | ✅ |
| Fade (Bildschirmblende) | `fade`-Befehl, ein/aus mit Sekunden | ✅ |
| RGSS-Script-Schicht | `$game_*`, `Graphics`, `Sprite/Window`, `$scene` — ohne Tilemap/Bitmap-Dateien | ✅ teilweise |
| Ballons, Animationen, Tilemap | — | ❌ fehlt |
| Autotiles, weiche Boden-Übergänge | ein Quad = eine Textur | ❌ fehlt |
| Charakter-Grafiken | Figuren sind Quader | ❌ fehlt |
| Passierbarkeit (blockierte Zonen) | „blocked"-Rechtecke im Karten-JSON: Physik-Wände, F2-Editorfeld, rote Vorschau; `passable?` prüft Zonen **und** Objekt-Hindernisse, NPCs/Routen stoppen an Wänden (`Passage`) | ✅ |

Offene Punkte mit Symptom und betroffener Datei stehen in
[`docs/BACKLOG.md`](docs/BACKLOG.md).

## Voraussetzungen

- Windows 10/11 x64 mit Direct3D 11
- **Ruby 3.x x64** (RubyInstaller mit DevKit; entwickelt auf 3.4.11 unter
  `C:\Ruby34-x64`). Header und Importbibliothek werden aus `RbConfig` gelesen,
  der Installationspfad ist für den Build also frei wählbar.
- **MSYS2-ucrt64-g++** (liegt beim RubyInstaller-DevKit unter
  `<Ruby>\msys64\ucrt64`)

## Bauen

```powershell
.\build.ps1              # build\Rune3D.exe + build\RuneEditor.exe
.\build.ps1 -Clean       # komplett neu
.\build.ps1 -Run         # bauen und das Spiel starten
.\build.ps1 -Test        # C++-/Windows-Datei-/Ruby-Tests und JSON-/Event-Brücke
.\build.ps1 -WarpTest    # WARP + isolierte eingebettete Ruby-Laufzeit
.\tools\windows_runtime_smoke.ps1   # echte EXEs in temporärer Paketkopie starten
.\build.ps1 -RubyRoot D:\Ruby34-x64   # andere Ruby-Installation
```

`build.ps1` kopiert die DLLs **und** die Standardbibliothek der ausgewählten
Ruby-Installation nach `build/lib/ruby/<API-Version>/`, einschließlich der
nativen JSON-Erweiterungen. Editor und Spiel bevorzugen diese Bibliothek
unabhängig von `PATH` und einem fremden `RUBYLIB`. Ein unvollständig kopiertes
Paket wird als Fehler gemeldet, statt Ruby-Versionen unbemerkt zu mischen.
Ein vorhandener, nicht vom Build markierter Bibliotheksordner wird beim
normalen Build nicht überschrieben; `-Clean` löscht dagegen ganz `build/`.
Die EXEs tragen eigene Windows-Versionsinformationen: „Rune3D Editor“ bzw.
„Rune3D Spiel“. winpthreads bleibt statisch gelinkt; das Toolchain-Archiv
wird nicht verändert. Ein vollständiger Neubau ist für diese Anzeige nötig.

Bei einer bestehenden Fehlermeldung `LoadError: cannot load such file -- json`
Editor und Spiel schließen, aus dem **aktualisierten Quellstand** neu bauen
(`.\build.ps1 -Clean`) und erneut starten. Bei einem Download das gesamte neue
`Rune3D-Windows`-Artefakt entpacken: EXEs, DLLs, `lib/ruby/`, `game/` und
`projekte/` gehören zusammen. Eigene Projekte vorher sichern; nicht einen
alten Projektordner durch die Demo aus einem Archiv überschreiben.

Für die von Engine/Editor benötigten Module `json` und `fileutils` wird zur
Laufzeit keine separate Ruby-Installation benötigt. Zusätzliche Gems und
native Erweiterungen mit weiteren DLL-Abhängigkeiten werden dadurch nicht
pauschal mitgeliefert oder garantiert. Ruby und DevKit bleiben zum **Bauen**
erforderlich. Für Diagnose/automatisierte Tests erzwingt die Prozessvariable
`RUNE_D3D_WARP=1` langsames D3D11-Software-Rendering statt Hardware.

Vor dem Bauen laufende Instanzen beenden, sonst schlägt das Linken mit
„Permission denied" fehl:

```powershell
Get-Process RuneEditor,Rune3D -ErrorAction SilentlyContinue | Stop-Process -Force
```

## Windows-Projektpfade

Editor und Spiel führen Engine-Wurzel sowie Projekt- und Kartennamen intern
als gültiges UTF-8. Für diese Pfade wird an Windows-Datei- und
Prozessgrenzen strikt in UTF-16 umgewandelt; nicht darstellbare oder ungültige Zeichen werden nicht
über die System-Codepage ersetzt. Der Editor kann bestehende gültige Unicode-
Projekt- und Kartennamen finden; **neu** angelegte Projektordner nutzen im
Editor weiterhin nur ASCII-Buchstaben, Ziffern und `_`. F4 übergibt den
Projektnamen unverändert und startet das Spiel mit der Engine-Wurzel als
Arbeitsverzeichnis, sowohl im `build/`- als auch im EXE-neben-`game/`-Layout.
Ein alter Projektmerker, der kein gültiges UTF-8 enthält, wird nicht aus der
System-Codepage erraten: Projekt einmal erneut auswählen.

Die automatisierten Windows-Tests decken auch Namen außerhalb der aktiven
Codepage ab. Karten und F9 verwenden zusätzlich die unten beschriebene
Handle-Transaktion; Materialdateien haben einen eigenen Schutz. Beides ist
keine Garantie für alle Langpfade, Dateisysteme oder übrigen Projektdateien.
Der interaktive [Live-Test](docs/WINDOWS_MATERIAL_LIVE_TEST.md) bleibt offen.

## Testen

Die wichtigsten Prüfungen, überwiegend headless:

```powershell
.\build.ps1 -Test                              # alles zusammen
ruby tools\test_all.rb                         # alle Ruby-Modultests
ruby -I game\engine game\engine\mapcheck.rb    # Karten-Daten prüfen
ruby tools\lint_repo.rb                        # Repo-Hygiene
```

| Ebene | Was | Wo |
|---|---|---|
| Ruby-Modultests | Engine-Logik mit `Rune::*`-Stubs (Events, Seiten, Interpreter, Party, Kampf, Wetter, Scripte, Projekte, Speichern) + Ruby/C++-Vokabular-Sync | `tools/*_test.rb`, Runner `tools/test_all.rb` |
| Ruby-Laufzeit | Echter Bootstrap, Launcher, Szenenbau, F4/F5, Kampf-Eingabe und Save/Load mit nativen Attrappen | `tools/runtime_boot_test.rb`, `tools/map_objects_test.rb`, `tools/encounter_test.rb` |
| C++-Logiktest | JSON, Editor-Formulare, F9-Registerkatalog und -Geometrie, Hauptfenster-Geometrie und Kartenaktionsschutz; unter Windows zusätzlich unsichtbare Panel-/Baumtests | `tools/json_test.cpp` |
| F9-Register ↔ Engine | Datenbank mit allen 13 Registern, vorbereitete Register wirkungslos, F9-Vorgaben spielbar, alte Projekte gültig | `tools/database_register_test.rb` |
| Event-Command-Architektur | Bauplan der drei XP-Tabs (90 Befehle), typisierte Command-Objekte, ein Handler je Typ, Ports und Interpreter | `tools/event_commands_arch_test.rb`, `tools/event_commands_test.rb` |
| XP-Befehlskatalog (F6) | Portable Abbildung des Bauplans: 90 Befehle, 3 Tabs, 12 Abschnitte, Codes, Felder, `cmd`-Zuordnung und überlappungsfreie Geometrie | `tools/event_command_catalog_test.cpp` |
| Einstell-Schema + Register (F6) | Nummerierte Schalter-/Variablen-Register 1..999, Einstellfelder je Befehl, Asset-Ordnerfilter | `tools/event_command_settings_test.cpp` |
| Script-/Kartenmodell | RGSS-Dokumente, Einrückung, Markeridentität und Startposition | `tools/editor_workflow_test.cpp` |
| C++-Terrain | Echte Geländegenerierung, Höhen-/Splat-Budgets, Atlasplan und Fehlerpfade ohne D3D | `tools/terrain_test.cpp` |
| Materialien/Dateien | Verlustfreier JSON-Baum, unter Windows Handle-Konflikte, Junctions und Hardlinks | `tools/material_test.cpp` |
| Windows-Projektpfade | UTF-8/UTF-16, Projekt-/Kartenscan, F9-Datei-I/O, F4-Kindprozess | `tools/windows_path_test.cpp` (`-Test`) |
| Windows-Karten/F9 | Echte Dateihandles, simulierte Bestätigungen, Konkurrenz, Unicode, Hardlinks und Junctions | `tools/document_store_test.cpp` (`-Test`) |
| D3D11-WARP | Software-Renderer, Pixel, Projektwechsel und Atlas-/Terrainreferenzen | `tools/renderer_warp_test.cpp` (`-WarpTest`) |
| Eingebettete Ruby-Brücke | Echter Editor-Boot, JSON-Erweiterungen, `fileutils`, isolierte Unicode-Laufzeit und WARP-Pixel | `tools/embedded_texture_test.cpp` (`-WarpTest`) |
| Windows-Start/UI | Echte EXEs im Unicode-Paket ohne Ruby-PATH und `RUBYLIB`, WARP; synthetische Win32-Editorabläufe | `tools/windows_runtime_smoke.ps1`, `tools/editor_ui_smoke.cpp` |
| JSON-Brücke | Schreibt C++ bytegleich zu Ruby `JSON.pretty_generate`? | `tools/json_bridge_check.rb` |
| Event-Brücke | C++-F6-Änderungen gegen echte Ruby-Event- und Interpreter-Laufzeit | `tools/event_bridge_check.rb` |
| Karten-Scan | Daten-Validierung (Befehle, Trigger, Bedingungen, Wegpunkte, Terrain) | `game/engine/mapcheck.rb` |
| Repo-Hygiene | UTF-8, BOM, Doppelkodierung, Zeilenenden, Requires, Index | `tools/lint_repo.rb` |
| Live-Tests | Interaktiver GUI-/Hardwaretest in isolierter Kopie (noch offen) | `docs/WINDOWS_MAIN_LIVE_TEST.md`, `docs/WINDOWS_MATERIAL_LIVE_TEST.md`, `docs/WINDOWS_BATTLE_LIVE_TEST.md` |

`tools/tex_import_test.rb` braucht eine gebaute `RuneEditor.exe` und wird ohne
sie übersprungen (der Runner meldet das).

## Continuous Integration

[`.github/workflows/ci.yml`](.github/workflows/ci.yml) läuft bei jedem Push und
Pull Request:

| Job | Runner | Inhalt |
|---|---|---|
| `hygiene` | ubuntu | `tools/lint_repo.rb` |
| `engine` | ubuntu | `tools/test_all.rb` + Karten-Scan |
| `cpp-logik` | ubuntu | JSON-/Editor-, Terrain- und Material-C++-Tests mit `-Werror`, danach JSON- und Event-Brücke |
| `windows` | windows-2022 | Build, Datei-/Importtests, WARP und Start des erneut entpackten Artefakts |

CI für die F4-Kartenübergabe auf Commit `bc237b3`, Lauf
[45183821663](https://github.com/Kenk-JADev/Game_Engine-V2/actions/runs/45183821663):
alle vier Jobs grün; unter Windows 58 Karten-/F9-Datei-, 39 Pfad-, 57
Material-/Datei-, 36 WARP- und 31 eingebettete Ruby-/WARP-Prüfungen sowie
28 Ruby-Testsuiten, jeweils ohne Fehler oder übersprungene Tests.
Der Dateitest nutzt echte Windows-Dateisystem-Handles und deterministisch
ausgelöste Konkurrenz, aber keine sichtbaren Editorfenster. Der Pfadtest nutzt
echte Windows-W-APIs und einen eigenen F4-Kindprozess, aber nicht das
komplette Spiel. WARP testet den echten D3D11-Renderer mit Software, aber
keine Hardware-GPU, Bedienung oder DPI.
Dieser ältere 19u-Lauf bezog die Standardbibliothek noch aus dem Build-Ruby.
Der aktuelle Windows-Job lädt das erzeugte Artefakt wieder herunter und startet
beide echten EXEs aus einer verlegten Paketkopie mit WARP. Interaktive Bedienung, DPI und Hardware-GPU
bleiben dennoch offen; Anleitungen: [Ruby-Start](docs/WINDOWS_RUBY_LIVE_TEST.md)
und [Materialien](docs/WINDOWS_MATERIAL_LIVE_TEST.md). Artefakt: **Rune3D-Windows**.

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
  main.rb            Game.setup/update/draw, Menü, Kartenlaufzeit
  engine/            Module: Event, Interpreter, Party, Npc, Weather, ...
  data/              Vorlage-Daten (Karten, Datenbank, Prefabs, Scripte)
projekte/<name>/     ein Projekt: data/ (Karten, Datenbank, Texturen), saves/
tools/               Tests, Prüfer, Einmal-Skripte der Etappen
docs/                architecture.md, BACKLOG.md, HANDOFF_Naechste_KI.md
```

## Bedienung

### Hauptfenster und Karten

Links liegt die Werkzeugpalette **über** dem Kartenbaum. Die waagerechte
Trennleiste dazwischen lässt sich ziehen; unter **Ansicht** kann man die
Bereiche auch ohne Maus vergrößern oder die Aufteilung zurücksetzen. Bei
wenig Höhe haben Palette und Objektinspektor eigene Scrollleisten. Die
Aufteilung gilt nur für die laufende Sitzung. Die mittlere Arbeitsfläche
bleibt der vorhandene 3D-Viewport; die Toolbar öffnet vorhandene Funktionen.

Im Kartenbereich öffnet **Neu** den bestehenden Dialog zum Anlegen; mit
**Eigenschaften** (auf schmaler Fläche als **F2** beschriftet) bzw. per
Doppelklick wird die ausgewählte Karte bearbeitet. Rechtsklick wählt den
getroffenen Knoten und öffnet nach dem Kartenwechsel das Kontextmenü für
Eigenschaften, Umbenennen, Neu und Löschen. Auf der Projektwurzel sind nur
passende Aktionen verfügbar. Beim Wechsel werden offene Objekt-/Event-
Eingaben und ungespeicherte Änderungen wie bisher abgefragt; bei Abbruch
bleibt die alte Karte aktiv. **Strg+S** speichert sie über den vorhandenen
Dateischutz. Objekt-Undo/Redo umfasst weiterhin keine Events oder Böden.

**NPCs, Events und Spielerstart:** Braune NPCs, Schilder und violette
Eventpunkte können einzeln angeklickt werden; erst ein schneller Doppelklick
auf denselben Marker öffnet dessen Bearbeitung (NPC: F5, Event: F6). Vor einem
NPC-Wechsel werden offene F6-Formulare geklärt. Verdeckte oder ungültige
Marker können über F5/F6 bearbeitet werden. Das grüne Symbol zeigt den
expliziten Projektstart auf dieser Karte.

Ein kurzer Rechtsklick auf sichtbares Gelände öffnet **Spieler-Startposition
hier setzen**; Rechtsklickziehen dreht weiterhin die Kamera. Die Position
rastet im aktuellen Raster ein (G ändert die Rasterweite). Der Start wird
**sofort** in `data/system.json` gespeichert, unabhängig von der Kartendatei
und ihrem Undo; Blickrichtung und zusätzliche Projektfelder bleiben erhalten.
Vor einer gesperrten Zone wird gewarnt. Neustart/F4 und F5 im Spiel lesen die
Position auf der zugehörigen Karte; F4 auf einer anderen Karte benutzt deren
Kartenstart. Ein normaler Kartenwechsel und Spielstände behalten ihr Verhalten.
Ist `system.json` beim Spielstart gesperrt, wird der optionale Zeitstempel
„zuletzt gespielt“ übersprungen, statt eine konkurrierende Startänderung
zu überschreiben. Projektdateien trotzdem regelmäßig sichern.

Die Windows-GUI, Monitorwechsel und Hardware-GPU müssen noch interaktiv
geprüft werden: [Anleitung](docs/WINDOWS_MAIN_LIVE_TEST.md).

**Editor** (F1 zeigt die Tastatur- und Mausbedienung im Programm):

| Eingabe | Wirkung |
|---|---|
| rechte Maustaste ziehen | umsehen; ein kurzer Rechtsklick auf Gelände öffnet das Kontextmenü |
| W/A/S/D, Q/E | bewegen, runter/hoch |
| Mausrad, Shift+Rad | Zoom, Höhe — Shift = schnell |
| Linksklick | setzen / auswählen; im Verschieben-Modus ziehen |
| Doppelklick auf NPC/Event | erster Klick wählt, schneller zweiter Klick öffnet den gezielten F5-/F6-Dialog |
| Rechtsklick auf Gelände | Spieler-Startposition hier setzen (speichert sofort) |
| Boden malen | Linksklick = Fläche, Shift+Klick = Radierer |
| Höhe malen | Linksklick = Berg, Shift+Klick = Senke, `+`/`-` = Pinselgröße |
| R, Entf | Auswahl drehen (+30°), löschen |
| Strg+Z / Strg+Y, Strg+S | Undo / Redo, Karte speichern |
| G, Esc, Enter | Raster umschalten, Auswahl aufheben, Eigenschaften übernehmen |
| Doppelklick in der Objektliste | Kamera aufs Objekt richten |
| PNG/BMP ins Fenster ziehen | Textur importieren |
| F1 Hilfe · F2 Karten-Eigenschaften · F3 Materialbase · F4 Playtest · F5 Platzier-Editor · F6 Event-Editor · F7 Tileset · F8 Sound-Test · F9 Datenbank · F10 Probleme · F11 Script-Editor · F12 Screenshot | |

**Spiel**: WASD laufen, Shift rennen, Leertaste springen, E benutzen/reden,
Maussteuerung Blick, Esc Menü (Gegenstand/Fertigkeit/Status, Speichern/
Laden, Titel; Esc geht eine Fensterebene zurück), F2 Debug-Ausgabe,
F5 Scripte neu laden.
F4 startet nur im RuneEditor einen separaten Spielprozess mit der gewählten
Karte. Dort setzt F5 den Spielzustand zurück und liest Karte und Projektstart erneut;
nach Rückkehr zum Titel und manueller Projektwahl gilt wieder die normale
Startkarte. Ohne Editor-Auswahl wird die Projekt-Startkarte verwendet.
Das Spiel schließen, um zum weiter geöffneten Editor zurückzukehren. Der
Prozess ist keine Sandbox: Spielstände, Scripts und Projektdateien bleiben
die gewöhnlichen Dateien. Alte Scripts, die `Editor` oder den internen
Playtest-Zyklus direkt aufrufen, müssen angepasst werden; `Game.change_map`
bleibt als Laufzeitaufruf erhalten.

### Ruby-/RGSS-Script-Editor (F11)

F11 öffnet die Scripts des aktuellen Projekts in `data/scripts.json`.
Der Editor zeigt eine Zeilenspalte und Cursorposition; **Tab** fügt zwei
Leerzeichen ein bzw. rückt eine Auswahl ein, **Umschalt+Tab** rückt aus.
**Syntax prüfen** kompiliert den Ruby-Text ohne ihn auszuführen. Beim
Auswahlwechsel wird der bisherige Eintrag übernommen, **Speichern** oder
**Strg+S** schreibt die Datei. Schließen/Projektwechsel fragt nach noch
ungespeicherten Änderungen; Neu laden, ungültige Daten und externe
Dateikonflikte ersetzen keinen Entwurf ohne ausdrückliche Entscheidung.
Nicht darstellbare oder doppelt vorhandene Felder bleiben geschützt;
unbekannte Daten bleiben beim Bearbeiten erhalten.

Ein benanntes Script kann wie bisher mit einem Eventbefehl aufgerufen
werden. Mit **Als RGSS-Startscript beim Spielstart ausführen** wird es
zusätzlich beim Neustart und F5 des Spiels in Dateireihenfolge im Top-Level
ausgeführt, nachdem Karte und `$game_*`-Globale angelegt wurden. Ohne diese
Markierung laufen alte Event-Scripts nicht automatisch. Ein Fehler wird
protokolliert und hält die folgenden Startscripts nicht an.

```json
{ "scripts": [
  { "name": "setup", "autoload": true, "code": "$game_variables[1] = 42" },
  { "name": "gruss", "code": "say ['Hallo!']" }
] }
```

`{"cmd":"script","name":"gruss"}` ruft das zweite Script aus einem
Event auf; dort stehen die Event-Helfer wie `say` zur Verfügung. Startscripts
laufen im Top-Level und nutzen z. B. `$game_variables`. Die RGSS-Schicht
ist **teilweise** kompatibel, nicht die vollständige RPG-Maker-XP-Laufzeit;
es läuft das mitgelieferte moderne CRuby. Ruby-Scripts sind vertrauenswürdiger
Code mit Dateizugriff, **keine Sandbox**. Nur vertrauenswürdige Scripts ausführen.

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
  "weather": { "type": "kein", "intensity": 1.0 },
  "blocked": [{ "x": 8.0, "z": 2.0, "w": 4.0, "h": 3.0 }],
  "encounters": { "troops": ["waldweg", "banditen"], "steps": 30 }
}
```

### Objekt-Eigenschaften im Hauptfenster

Objekt in der Liste oder im Viewport auswählen. X/Z sind Weltkoordinaten,
die Drehung wird in Grad eingegeben; ein leeres Material nutzt die Prefab-
Texturen. Geänderte Zahlen müssen endlich und zwischen −1.000.000 und
1.000.000 liegen (Dezimalpunkt oder -komma); Materialnamen sind einzeilig.
**Übernehmen** oder Enter übernimmt das Formular, **Strg+S** übernimmt
offene Eingaben und speichert anschließend die Karte. Ungültige Eingaben
verhindern die Übernahme. Beim Auswahl-, Karten- oder Projektwechsel sowie
anderen Aktionen werden noch offene Formulareingaben zum Übernehmen oder
Verwerfen angeboten; Abbrechen behält die Eingaben und die alte Auswahl.

Unveränderte Zahlen, Y-Versatz und unbekannte Zusatzfelder bleiben erhalten.
Mehrfach vorhandene oder nicht vollständig darstellbare Objektfelder sind
gesperrt; fehlerhafte Altdaten werden nur nach Bestätigung ersetzt. Bei
ungültiger oder extremer Position/Drehung bleibt der Eintrag in der Liste,
wird aber nicht im Viewport dargestellt. Ziehen verändert nur bewegte
Achsen; Undo entsteht erst bei einer tatsächlichen Positionsänderung. Beim
Zurückziehen an den Ausgangspunkt werden die ursprünglichen Achsenwerte
wiederhergestellt. Objekt-Undo gilt weiterhin nur für Objekte.

**NPCs, Schilder und Objekte:** Karte wählen, ggf. den Event-Editor F6
schließen, dann **F5** oder „Events → NPCs, Schilder und Objekte“ öffnen.
Kategorie und Eintrag wählen oder mit „Neu“ anlegen. X/Z sind Weltpositionen;
Y ist ein Versatz zur Geländehöhe (0 = auf dem Boden). Für NPCs Tempo,
Gesprächsradius und Wegpunkte (`X; Z`, eine Zeile je Punkt) eingeben; für
NPCs und Schilder lässt sich der erste Text bearbeiten. Zusätzliche Befehle
bleiben erhalten. Mehrseitige Schild-Events lassen sich vollständig in F6
bearbeiten; komplexe NPC-Kommandos bleiben erhalten, sind hier aber nicht
vollständig editierbar. „Letzte Cursorposition“ übernimmt den zuletzt im
Viewport getroffenen Bodenpunkt. **OK** übernimmt alle Dialogänderungen in
die Karte, **Abbrechen** verwirft sie, **Strg+S** speichert anschließend.
NPCs und Schilder erscheinen im Viewport; die Wegpunkte sind blau markiert.
NPC-Gespräche und Schilder sind im Spiel mit **E** oder Mausklick nutzbar.
Kollisions- und Interaktionsreichweite bleiben XZ-basiert, auch bei Y-Versatz.
Objektänderungen aus F5 sind mit Strg+Z rückgängig zu machen; NPCs und
Schilder sind noch nicht im Undo enthalten. Beim Kartenwechsel wird der
Objekt-Undo-Stapel geleert.

### Event-Editor (F6)

F6 öffnet ein eigenes Fenster für Events, Seiten und Befehle. „Event
übernehmen“, „Seite übernehmen“ und „Befehl übernehmen“ ändern jeweils
das betreffende Formular; „Einfügen“ erstellt einen neuen Befehl im
angezeigten Zielzweig. Noch offene Eingaben werden vor Auswahlwechsel,
Schließen, Karten-/Projektwechsel und Speichern abgefragt: Ja übernimmt,
Nein verwirft die Formulareingaben, Abbrechen behält sie samt alter Auswahl.
Ein nicht ausgewählter Befehlsentwurf wird bei Ja eingefügt. Ungültige
Eingaben verhindern die gesamte gemeinsame Übernahme. Die Karte wird
anschließend im Hauptfenster mit Strg+S gespeichert; mit Fokus im F6-Fenster
ist das Tastenkürzel für das Hauptfenster abgeschaltet.

**XP-Befehlsauswahl (Etappe 44):** Neben der Befehlstyp-ComboBox öffnet „XP…“
einen Dialog mit drei Registerkarten wie RPG Maker XP (je zwei Spalten und zwei
Abschnitten, 90 Befehle). Ein Klick wählt den Befehl, „Übernehmen“ stellt die
ComboBox auf den passenden Event-String. 60 Befehle sind ausführbar verdrahtet;
30 sind als „vorbereitet“ markiert und melden ihre Grenze, statt still in eine
Karte geschrieben zu werden. Siehe `docs/ETAPPE_44_EVENT_COMMAND_TABS_UI.md`.

**XP-Einstellfenster & Bedienung (Etappe 45):** Jeder Befehl hat ein eigenes
Einstellfenster mit genau seinen Feldern. Schalter und Variablen werden über eine
nummerierte Liste 1..999 (mit Namen) gewählt, nicht eingetippt; Items/Helden/
Truppen/… aus der Datenbank; Audio und Bilder über „Durchsuchen…“ aus einem Ordner
(extern gewählte Dateien werden ins Projekt kopiert). In der Befehlsliste:
Klick = auswählen, Doppelklick = Bearbeiten, Rechtsklick = Menü
(Bearbeiten…/Einfügen…/Löschen). Leere Zweige zeigen eine anklickbare Leer-Zeile;
Doppelklick auf eine Zweig-Kopfzeile (Dann/Sonst/…) oder Leer-Zeile fügt in genau
diesen Zweig ein (Etappe 46). Siehe `docs/ETAPPE_45_COMMAND_SETTINGS_UI.md` und
`docs/ETAPPE_46_BRANCH_INSERT_SLOTS.md`.

Nur geänderte Felder werden geschrieben. Unberührte Zahlen, fehlende Felder,
Zusatzdaten und verschachtelte Zweige bleiben erhalten. Alte Events ohne
Seitenliste erscheinen zunächst als implizite Seite; erst neue Bedingungen
oder „Neue Seite“ legen `pages` an. Die ursprünglichen Top-Level-Felder
`trigger`/`commands` bleiben als Altdaten erhalten; die Laufzeit nutzt bei
vorhandenen Seiten deren Werte. Beschädigte oder mehrdeutige Daten werden
nicht still ersetzt. Vor Löschen, Typwechsel oder Ersetzen kaputter Daten
wird gewarnt. Bei geänderten Wahloptionen bleiben Zweige nach Index erhalten;
ihre Zuordnung kann sich ändern, zusätzliche Zweige bleiben im JSON.

Neue Events und Kopien erhalten eindeutige Namen gegenüber darstellbaren
Event-/NPC-Namen; eine entsprechende Namenskollision beim Umbenennen wird
abgelehnt. Jede Umbenennung wird bestätigt, weil Spielstände und Self-Switches
weiterhin den alten Namen verwenden. Beim Duplizieren werden ganzzahlige
Positionen exakt um eins erhöht. Fehlt eine Koordinate oder lässt sie sich
nicht verschieben, bleibt sie unverändert und es erscheint eine Warnung.
Bei der Zahleneingabe muss der Startwert zur Ziffernzahl passen. Sehr lange,
vom Windows-Feld nicht verlustfrei darstellbare Eingaben sind geschützt.
Für Events gibt es weiterhin kein Undo; vor F2/F5 muss F6 geschlossen sein.
Das F6-Fenster passt sich beim Öffnen der Arbeitsfläche des aktuellen
Editor-Monitors an und kann am Rand verkleinert werden. Bei Platzmangel
erreichen horizontale und vertikale Fenster-Scrollleisten das gesamte
Formular; Tab und Umschalt+Tab führen den Fokus nach. Das Mausrad scrollt
vertikal, Umschalt+Mausrad horizontal, sofern das fokussierte Control das
Rad nicht selbst verarbeitet. Listen und Komboboxen behalten ihr eigenes
Radverhalten. Bei genügend Platz sind die Leisten deaktiviert. Ein echter
Windows-GUI-/DPI-Test steht noch aus.

### Terrain, Boden und Atmosphäre (F2/F7)

Für Vorschau und Spiel gelten sichere Grenzen: Kartenbreite/-höhe 1–10000 m,
Auflösung 8–1024 Zellen in X und höchstens 262144 Quads insgesamt;
Seitenverhältnis und Z-Auflösung werden ebenfalls geprüft. Seed liegt bei
0–4294967295, Amplitude bei ±10000. Höchstens 2048 Höhenpinsel und 2048
Bodenflächen sind erlaubt, zusätzlich gelten Rechenbudgets. Texturnamen
dürfen höchstens 1024 UTF-8-Bytes und kein NUL enthalten. Für `terrain.tile`
sind 0,01–10000 m, für Nebelweite 0,1–100000 m und für Wetterintensität
Werte größer 0 bis 10 zulässig. Farben liegen zwischen 0 und 0xFFFFFF.

Fehlerhafte oder zu aufwendige Grundparameter, Höhenpinsel und Bodenflächen
bleiben im Karten-JSON erhalten; der Editor zeigt statt eines alten/falschen
Meshs keine Gelände-Vorschau und nennt die Ursache über **F10** und die
Statusleiste. Ungültige Einzelwerte für Textur, Kachelgröße oder Atmosphäre
erhalten nur für die Darstellung sichere Ersatzwerte. F2/F7 schreiben
unberührte Felder nicht zurück; ungültige Tileset-Altdaten werden nur bei
gezielter Änderung nach Rückfrage ersetzt. Im F7-Dialog bleiben ungültige
Bodenflächen sichtbar; die Auslassungszeile bei überlangen Listen ist nicht
löschbar. Auch nach dem Löschen des letzten Eintrags bleibt eine vorhandene
leere Flächenliste erhalten. Nach Karten-/Projektwechsel F7 schließen und neu
öffnen, bevor Daten geändert werden.

Projekttexturen haben eine eigene Lebensdauer und überlagern eingebaute
Texturen nur im geöffneten Projekt. Projektwechsel/F5 entfernen alte Slots und
Atlas-Referenzen. Materialien werden begrenzt gelesen (maximal 4096×4096,
64 MiB je RGBA, 128 MiB verwaltete Texturpixel, 2 MiB Manifest); das
Materialdokument erhält unbekannte und ungültige Einträge. Defektes JSON bleibt
schreibgeschützt. Import/F7 legen neue RGBA-Dateien an, ersetzen keine fremden
Dateien und erhalten alte RGBA-Dateien. F3 entfernt nur Metadaten.

Materialpfade werden über gebundene Windows-Verzeichnishandles geprüft; einfache
Dateinamen sind Pflicht, Reparsepunkte und mehrfach verlinkte Dateien werden
abgewiesen. Speichern vergleicht die Originalbytes und legt eine Sicherung an;
bei Konflikt wird nicht überschrieben. Es ist kein atomarer Namensaustausch:
bei Absturz kann eine `.rune-*.bak` verbleiben. Andere Projektdateien sind
nicht durch diesen Mechanismus geschützt; Sicherungskopien bleiben ratsam.

Der Atlas ist auf 1024 Texturen à 64×64 begrenzt (höchstens 16 MiB pro
RGBA-Kopie). Ohne Atlas oder ohne passende Basistextur bleibt der Boden als
Einzeltextur-Vorschau; fehlende Flächentexturen zeigen die Basis. Die
Einschränkung steht in Status/F10 bzw. im Spiel-Log. `terrain.tile` wirkt
weiterhin nur ohne Splat-Atlas. Der interaktive GUI-/Hardware-D3D-Test steht
aus; der automatisierte D3D11-WARP-Test verwendet Software-Rendering.

### Karten-Eigenschaften (F2)

`blocked` (optional) sind unsichtbare Wände über alle Höhen (x/z = Mitte,
w/h = Größe in Metern). In **F2** stehen vorhandene Zonen als nummerierte
Zeilen, etwa `#1: 0; 2; 4; 3`. Beim Bearbeiten oder Verschieben der Zeile
die Nummer stehen lassen: So bleiben Zusatzdaten und unveränderte Werte
erhalten. Für eine neue Zone eine Zeile ohne Nummer eingeben (`x; z; w; h`
oder `x,z,w,h`); Dezimalkomma ist bei Semikolontrennung möglich. Entfernte
Zeilen und das Ersetzen ungültiger Altdaten werden vor OK bestätigt. Bei
fehlerhafter Eingabe bleibt die ganze Karte unverändert. Block-Zonen sind
als rote Flach-Quader sichtbar. Vor F2 den Event-Editor F6 schließen; umgekehrt lässt sich F6 erst nach dem Schließen von F2 öffnen.

`encounters` ist optional. Im Editor die gewünschte Karte wählen und **F2**
öffnen: Unter „Zufallskämpfe“ eine gespeicherte Projekt-Truppe auswählen
und „Hinzufügen“ klicken, oder IDs direkt eingeben (eine pro Zeile).
Mehrfache Einträge machen eine Truppe entsprechend wahrscheinlicher; eine
leere Liste schaltet Zufallskämpfe aus. „Schritte (Mittelwert)“ ist eine
positive ganze Zahl, ein leeres Feld bedeutet 30. Neue Eingaben sind auf
1 bis 9007199254740991 begrenzt, damit der C++-JSON-Editor sie exakt speichert.

Gegner und Truppen werden in **F9** angelegt und dort gespeichert; F2 kann
F9 öffnen und über „Neu laden“ dessen gespeicherte Truppen aktualisieren.
Vor einem Projektwechsel F9 speichern und schließen. Bei einer geänderten
Liste führen unbekannte IDs oder eine nicht lesbare Datenbank zu einer
Rückfrage vor dem Übernehmen. Ein Kampf braucht außerdem eine Truppe mit gültigen Gegnern
und eine spielbare Party. **OK** übernimmt die Änderungen zunächst in die
Karte, **Strg+S** speichert sie auf Platte; Abbrechen verwirft den Dialog.
Die Vorlage `game/data` enthält Beispieltruppen. Bestehende Projekte
werden nicht automatisch umgestellt; für die Demo zieht
`ruby tools/sync_demo.rb` Truppen, Gegner und `encounters` nach
(Vorlage = Quelle, ohne Demo-Werte zu überschreiben).

### Datenbank (F9)

Seit **Etappe 41** hat F9 alle **13 Register von RPG Maker XP** als echte
Registerkarten (bei wenig Platz oder hoher DPI mehrzeilig) in XP-Reihenfolge:
Helden, Klassen, Fertigkeiten, Items, Waffen, Rüstungen, Gegner, Truppen,
Zustände, Animationen, Tilesets, Gemeinsame Events, System. Links die Liste,
rechts das Formular in zwei Spalten; das Startgold steht unabhängig vom
Register oben rechts. Die Hinweiszeile sagt pro Register, ob das Spiel die
Daten schon liest: **Helden, Fertigkeiten, Items, Gegner, Truppen, Zustände
und Gemeinsame Events** wirken im Spiel; **Klassen, Waffen, Rüstungen,
Animationen, Tilesets und System** sind vorbereitet und werden vollständig
gespeichert, aber noch nicht ausgewertet.

**System** ist wie in XP ein einzelner Datensatz (keine Liste, kein
Neu/Löschen) und liegt als Objekt `system` in `database.json` — Projektstart
und Kartenbaum stehen weiterhin in `system.json`. **Gemeinsame Events** haben
Nummer, Name, Auslöser, Schalter und ihre **Befehlsliste als JSON** direkt im
Dialog; kaputtes JSON wird abgelehnt, ohne die Liste zu verändern.
Auswahlfelder (Art, Ziel, Auslöser, Rüstungsplatz …) nennen ihre erlaubten
Werte in der Beschriftung; ein unbekannter Altwert bleibt stehen und wird nur
nach Rückfrage ersetzt. Details und Prüfanleitung:
[`docs/ETAPPE_41_F9_DATENBANK.md`](docs/ETAPPE_41_F9_DATENBANK.md).

F9 übernimmt beim Register- oder Eintragswechsel nur geänderte Felder;
unveränderte Zahlen, optionale Felder und unbekannte Zusatzdaten bleiben
erhalten. Gold kann auch ohne ausgewählten Eintrag geändert werden. Ganze
Zahlen werden exakt gespeichert; Kommazahlen akzeptieren Punkt oder Komma.
Ungültige Eingaben blockieren die Übernahme. Fehlende Sammlungen entstehen
erst beim Anlegen eines Eintrags; fehlerhafte Sammlungen und uneindeutige
alte Gegnerlisten werden nur nach Rückfrage ersetzt. Nicht darstellbare
Einzeltexte sind geschützt. Beim Schließen/Neu-Laden wird vor dem Verwerfen
gefragt. Hat ein anderes Programm die Datei seit dem Laden geändert, fragt F9
vor dem Überschreiben; zur Dateitransaktion und ihren Grenzen siehe unten.
Ungültiges JSON, doppelte Schlüssel, ungültiges Unicode und nicht endliche
Kommazahlen verhindern das Laden, statt eine leere Datenbank zu überschreiben.

Das Kartenladen lehnt ungültiges JSON/Unicode, nicht darstellbare
Kommazahlen sowie ein mehrfaches oder nicht listenförmiges `objects` ab.
Andere doppelte JSON-Schlüssel bleiben erhalten. Die Terrain-/Atmosphären-
Vorschau prüft zusätzlich ihre Eingaben und Budgets; das ist keine
vollständige semantische Validierung aller Projektdateien (siehe Backlog).

**Karten und F9:** Vor Speichern oder Kartenlöschen vergleicht der Editor
die Originalbytes mit dem geladenen bzw. gespeicherten Stand und fragt bei
Abweichungen. Nach einem Ja werden die konkret beobachteten Bytes und die
Dateiidentität unter einer Windows-Schreib-/Löschsperre erneut geprüft. Eine
Änderung während der Rückfrage bricht den Vorgang ab. Neue Dateien werden
nur ohne Überschreiben eingehängt; gelöscht wird nur das bestätigte Handle.
Während der Transaktion sind Projektverzeichnisse gebunden, Reparsepunkte
und mehrfach verlinkte Dokumentdateien werden abgelehnt. Ein fremder
Dateihandle, der die nötige Sperre nicht zulässt, führt zum Abbruch.

Das Verfahren ist **kein atomarer Namensaustausch**: Das alte Dokument wird
kurz als `.rune-*.bak` gesichert und eine vollständige `.rune-*.tmp` ohne
Ersetzen veröffentlicht. Bei Absturz kann der Name vorübergehend fehlen
oder eine Sicherung liegen bleiben; bei blockierter Rücksetzung nennt der
Editor den Wiederherstellungspfad. Sicherung und fremdes Ziel vergleichen
und manuell sichern/wiederherstellen, nicht blind löschen. Dateisysteme ohne
passende IDs/Handle-Unterstützung können abgewiesen werden; Netzlaufwerke,
Speicherabbildungen bereits geöffneter Dateien, Stromausfälle und die GUI
sind nicht allgemein abgesichert bzw. getestet. Andere Projektdateien fallen
nicht automatisch unter diesen Schutz. Regelmäßige Backups bleiben nötig.

Regeln: Editor und Spiel schreiben beide `JSON.pretty_generate`-Format (LF,
2 Leerzeichen, kürzeste exakte Zahlendarstellung) — deshalb wirft Speichern
keine Diff-Flut. Unbekannte Schlüssel und große vorhandene JSON-Ganzzahlen
bleiben beim reinen Laden/Speichern erhalten. Die Formulare F2, F5, F6, F7, F9
und das Objektformular schreiben unberührte Felder nicht zurück. Numerische
Berechnungen und neu eingegebene Kommazahlen bleiben double-basiert.
Spielstände liegen in `projekte/<name>/saves/slot1..3.json`.

## Dokumentation

- [`docs/architecture.md`](docs/architecture.md) — Aufbau, Ruby-API, Etappen
- [`docs/BACKLOG.md`](docs/BACKLOG.md) — offene Fehler und Grenzen, mit Symptom
  und betroffener Datei
- [`docs/ETAPPE_41_F9_DATENBANK.md`](docs/ETAPPE_41_F9_DATENBANK.md) — F9 mit 13
  XP-Registern: Recherche, Aufbau, Prüfungen und Live-Test-Anleitung
- [`docs/ETAPPE_42_EVENT_COMMAND_TABS.md`](docs/ETAPPE_42_EVENT_COMMAND_TABS.md) —
  Event-Command-Architektur nach den drei XP-Tabs: Bauplan, Typen, Handler,
  Ports, Beispielwerte
- [`docs/ETAPPE_43_ENGINE_PORTS.md`](docs/ETAPPE_43_ENGINE_PORTS.md) — die 90
  Befehle an der echten Engine: 19 Adapter, blockierende Fenster, gemeldete
  Grenzen
- [`docs/HANDOFF_Naechste_KI.md`](docs/HANDOFF_Naechste_KI.md) — Arbeitsregeln,
  Etappen-Tabelle, bekannte Fallstricke

## Arbeitsweise

Eine Etappe = ein Commit: implementieren → bauen → Tests → Live-Test →
Dokumentation (`docs/`) → Commit. Fehler, die nicht in die laufende Etappe
gehören, werden in `docs/BACKLOG.md` eingetragen statt sie in Test-Schleifen zu
verfolgen. Antworten, Kommentare und Dokumentation sind auf Deutsch; Log-Strings
im Code bleiben transliteriert (`ae`/`oe`/`ue`), weil die Live-Tests darauf
matchen.
