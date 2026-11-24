# Rune3D — Architektur

Eine 3D-Engine im RPG-Maker-Stil für Windows, modbar mit echtem Ruby 3.4.
Kein mruby: eingebettetes CRuby mit vollem Sprachumfang, Gems und Prism-Parser.

## Prinzip

C++ macht nur das, was Performance braucht (Fenster, D3D11, Kamera, Logging).
Alles Spielerische lebt in Ruby unter `game/` und ist jederzeit änderbar.

```
Rune3D.exe  ──┬── src/core    Win32-Fenster, Input, Log, Zeit
              ├── src/gfx     Direct3D 11: Device, Shader, Kamera, Draw-List
              └── src/ruby    CRuby-VM + Bindings (Modul "Rune")
                     │
                     └── game/   <-- das Spiel (modbar, hot-reload)
                            boot.rb        Bootstrap + Rune.tick
                            engine/        Scene, Node, Sprite-Klassen
                            main.rb        dein Spiel (F5 = neu laden)
```

## Mitgelieferte Ruby-Laufzeit (19x)

Der gemeldete Editor-Startfehler `LoadError: cannot load such file -- json`
entstand, weil DLLs allein keine Ruby-Standardbibliothek enthalten und der
bisherige Fallback fest auf `C:/Ruby34-x64` zeigte. Dass die CI bisher grün
war, belegte das Paket nicht: der Brückentest setzte dort `RUBYLIB` auf die
Build-Installation. Auch das Spiel benutzt `json` und `fileutils`.

`build.ps1` ermittelt Konfiguration und Architektur von genau dem zum Linken
gewählten Ruby und kopiert dessen `rubylibdir` nach `build/lib/ruby/<API-Version>/`.
Native `.so`-Erweiterungen liegen darin unter `<RUBY_PLATFORM>`; der Build kontrolliert JSON,
Parser, Generator, `fileutils` und `encdb`. Die Konfigurationsabfrage gibt
Pfade Base64-kodiert aus, damit auch PowerShell 5.1 Unicode erhält. Ein
markierter Build-Ordner wird gestuft ersetzt; nicht markierte Bibliotheken
werden verweigert. Wie beim bisherigen Build löscht `-Clean` ganz `build/`.
Der Austausch ist nicht als atomare Transaktion bei Absturz garantiert.

`vm.cpp` stellt die passende, EXE-relative Bibliothek an den Anfang des
`$LOAD_PATH`; vorhandenes `RUBYLIB` bleibt als Zusatz erhalten. Ein teilweise
vorhandenes Paket mit fehlenden Kernmodulen wird abgewiesen, damit keine
fremde Ruby-Version hineingemischt wird. Ohne Paket bleibt ein explizit
passender externer Load-Pfad als Kompatibilitätsweg möglich. Editor und
Spiel benutzen dieselbe VM; der Editor zeigt bei Bootfehlern eine Warnung.
Default-Gem-Spezifikationen, eigene Gems und zusätzliche DLLs für andere
native Erweiterungen werden nicht vollständig paketiert oder garantiert.

`-WarpTest` startet die echte eingebettete VM samt Editor-Boot in einer
Unicode-Kopie, ohne Ruby im PATH und ohne `RUBYLIB`, anschließend mit fremdem
`RUBYLIB` und mit absichtlich fehlenden JSON-Dateien. Der Test prüft Ursprung
und Roundtrip der nativen JSON-Erweiterungen sowie `fileutils`. Zusätzlich
startet `tools/windows_runtime_smoke.ps1` die echten Editor- und Spiel-EXEs
aus einer isolierten Paketkopie bis zur Hauptschleife; dabei wird nur für
die Grafik `RUNE_D3D_WARP=1` gesetzt. Die CI lädt das hochgeladene Artefakt
erneut herunter und testet diese entpackten Dateien. Den aktuellen Lauf prüfen;
Software-WARP ersetzt keinen interaktiven Zielrechner-/Hardware-GPU-Test.

## Kartenmarker, Spielerstart und EXE-Metadaten (19y)

`map_interaction.h` liefert datengeprüfte Marker für NPCs und Events. Der
Viewport zeichnet die Marker und trifft nur unverdeckte Geometrie. Ein
einfacher Klick selektiert; `CS_DBLCLKS` und `WM_LBUTTONDBLCLK` öffnen nur
beim selben Typ, Index, Karten-Epoch und JSON-Snapshot. F6-Entwürfe werden
vor dem gezielten Wechsel geklärt; F5 arbeitet weiterhin auf einer Kopie.
Rechtsklickziehen bleibt Kamera, ein kurzer Rechtsklick öffnet das
Kontextmenü am Geländetreffer. Raster und Kartengrenze gelten auch für den
Start; gesperrte Zonen lösen eine Rückfrage aus.

Der Projektstart wird gezielt und sofort über `DocumentStore` in
`system.json` gespeichert (`start.map/x/z/use_project_position`), ohne
Richtung und fremde Felder zu ersetzen. Er ist kein Karten-Undo. Nur beim
Spielstart/F5 und nur für die angegebene Karte überschreibt er den lokalen
Kartenstart. `Database.reload_system` liest dafür `system.json` bei F5 neu;
andere Caches, Kartenwechsel und Savegames behalten ihre Regeln.
`Project.touch` hält unter Windows beim Lesen/Schreiben von `last_played`
denselben beschreibbaren Handle ohne Delete-Sharing; konkurrierende Spiele
nutzen eine nichtblockierende Dateisperre. Ein Editor-Konflikt überspringt
den optionalen Zeitstempel. Fehlerhafte, mehrdeutige oder nicht objektförmige
Systemdaten werden nicht durch einen leeren Ersatz überschrieben. Der Ruby-
Schreibvorgang selbst bleibt ein nicht absturzsicherer In-place-Write.

`build.ps1` linkt eigene `VERSIONINFO`-Ressourcen für beide EXEs und eine
lokale Kopie von `libwinpthread.a` ohne `version.o`: `--whole-archive`
könnte sonst die fremde Beschreibung „POSIX WinThreads for Windows“ als
EXE-Ressource übernehmen. Das Toolchain-Original bleibt unverändert und
`objdump` prüft, dass keine dynamische winpthread-DLL importiert wird.
Der isolierte Smoke-Test prüft zusätzlich die EXE-Versionseigenschaften;
die konkrete Windows-Anzeige und interaktive Nutzung bleiben Live-Tests.

## Hauptfenster und Karten (19w)

Das separate `RuneEditor.exe` hat eine gruppierte Werkzeugleiste mit
vorhandenen Funktionen, links die scrollbare Palette **über** dem Kartenbaum,
in der Mitte den bestehenden D3D11-Viewport und rechts den scrollbaren
Objektinspektor. Eine waagerechte Trennleiste verändert die Höhe der beiden
linken Bereiche; die gleichen Schritte sind über **Ansicht** ohne Maus
erreichbar. Die Wunschhöhe ist Sitzungszustand, keine gespeicherte Einstellung.
Bei schmaler Fläche wird der Eigenschaftenknopf mit **F2** beschriftet.

`main_layout.h` berechnet die Spalten und Kontrollrechtecke in DPI-skalierten
Einheiten und klemmt kleine Fenster ohne Spaltenüberschneidung. Palette und
Inspektor sind native Kinder (`main_panel_win.h`); `WM_COMMAND`, `WM_NOTIFY`
und `WM_CONTEXTMENU` werden an das Hauptfenster durchgereicht. Scrollen,
Tab-Fokus und DPI-Wechsel verändern die Position der Controls, nicht ihren
Inhalt. Toolbar-Icons und Schrift werden beim DPI-Wechsel neu erzeugt. Der
Renderer erhält die Viewport-**Clientgröße**; frühe Größenmeldungen vor der
Rendererinitialisierung dürfen noch kein Bild rendern.

`main_tree_win.h` erkennt die Wurzel auch ohne Karten und selektiert das
Rechtsklickziel. Für Eigenschaften, Neu und Kontextmenü wartet eine
vorgemerkte Aktion auf den bestehenden Auswahl-/Speicherabgleich. Auswahl,
Projektepoch und Dateiname werden geprüft; nach dem Kontextmenü zusätzlich
die Kartenepoch. Ein Abbruch oder verändertes Ziel verwirft die Aktion.
Kartenformulare, Konfliktprüfung, Datentreue und Datei-Transaktionen bleiben
die bestehenden Pfade; Objekt-Undo erfasst weiterhin keine anderen Datentypen.

Automatisiert: `tools/json_test.cpp` prüft portable Geometrie und Zielschutz;
unter Windows kommen unsichtbare Win32-Panels, Nachrichten und TreeView
hinzu. Lokal wurden die Windows-Quellen nur crosskompiliert, nicht ausgeführt.
Weder diese Tests noch WARP ersetzen den noch offenen interaktiven
[Windows-/DPI-/Hardwaretest](WINDOWS_MAIN_LIVE_TEST.md).

## Frame-Ablauf

1. `Window::pump()` — Nachrichten, Tasten, Mausdelta
2. `Physics::update(dt, input, yaw, terrain)` — Spieler bewegen (WASD, Sprung,
   Schwerkraft, Kollision gegen Kisten + Terrain)
3. `Camera::update()` — Kamera folgt dem Spieler (First/Third-Person, Maus)
4. `Renderer::begin_frame()` — Clear + Tiefe
5. `Rune.tick(dt)` — Ruby: `Game.update(dt)` + `Game.draw` (Aufrufe von `Rune::Gfx.box`
   und `Rune::Gfx.rect/text` für das 2D-UI)
6. `Renderer::end_frame()` — Gelände-Mesh, dann Draw-List, dann 2D-Overlay, Present
   (ein `F12`-Screenshot wird direkt vor `Present` aus dem Backbuffer kopiert)

## Welt & Physik (Etappe 2)

- `src/world/terrain.*` — Höhenfeld (Wertrauschen + 4 Oktaven FBM), deterministisch
  pro Seed. Gibt Vertex-/Indexdaten als `gfx::MeshVertex` (Pos+Normale+Farbe) aus.
  Farben nach Höhe/Neigung (Sand/Gras/Fels), Normals aus zentralen Differenzen,
  Dorfkern wird automatisch eingeebnet (Mischradius 16→42).
- `src/world/physics.*` — Spieler als Zylinder-AABB (Radius 0.34, Höhe 1.75).
  Bewegung achsenweise (man rutscht an Wänden entlang), Gravitation 22, Sprung 6.4,
  Schritte bis 0.35 m werden automatisch erklommen, Laufen mit Shift (+90%).
  `add_solid(center, size)` registriert Hindernisse; Terrain liefert den Boden.
- `Scene#refresh_solids` bzw. `Node#solid = true` meldet Kisten an die Physik.
- Das Gelände wird einmal beim Start (und bei F5) neu aufgebaut:
  `terrain.generate()` → `renderer.set_terrain(verts, idx)` (eigene VB/IB).

## 2D-Oberflaeche (Text & Fenster)

- `src/gfx/ui.h/.cpp` — `Ui` zeichnet Rechtecke und Text als Bildschirm-Quads
  (eigener Shader, Tiefe aus, Alpha-Blending). Kein Font-Renderer, sondern GDI:
  Segoe UI wird mit `TextOutW` in eine DIB-Section gerastert (weisse Glyphen auf
  Schwarz), die Luminanz dient als Alpha. Damit ist echter Unicode-Text ohne
  zusätzliche Bibliothek möglich.
- Die Textur ist bildschirmgross; alle Texte einer Frame landen in **einer**
  Textur (ein Draw-Call pro Frame statt einem pro Zeile).
- `game/engine/ui.rb` — Ruby-Hülle: `UI.panel`, `UI.frame`, `UI.text`,
  `UI.message(lines)` (RPG-Maker-Nachrichtenfenster unten) und `UI.hud`
  (Position + FPS oben links).
- `Renderer::save_screenshot()` schreibt den Backbuffer als BMP (`F12`).

## Schatten (Etappe 9a)

Die Sonne wirft jetzt Schatten: Vor jedem Frame rendert ein eigener
**Depth-Pass** die Szene (Terrain + alle Boxen) aus Sicht der Sonne in eine
2048er-Tiefenkarte (`DXGI_FORMAT_R32_TYPELESS`, DSV `D32_FLOAT`, SRV
`R32_FLOAT`). Die Ortho-Matrix deckt das ganze Gelände ab (360 m um den
Ursprung, Licht-Kamera 180 m über der Mitte). Der Pixel-Shader vergleicht im
Licht-Raum per `SampleCmp` (bilinearer 2x2-PCF), mit Normalen-Versatz
(0.06 m) und Tiefen-Bias gegen Shadow-Acne; ausserhalb der Abdeckung gilt
"beleuchtet". Der Schattenfaktor multipliziert nur den direkten
Sonnenanteil, Ambient und Himmelsstreuung bleiben. Schatten lassen sich per
`Rune::Gfx.shadows = false` abschalten.

Die Default-Sonne steht bei (-0.55, 0.75, 0.25) — west-südwestlich, damit
die Schatten aus der Standard-Kameraperspektive (Blick nach Norden)
**seitlich** ins Bild fallen. Eine Sonne hinter der Kamera würde die
Schatten hinter die Objekte werfen, wo sie unsichtbar sind.

## Sound (Etappe 9b)

Sound läuft über die klassische **waveOut-API** (winmm) als eigener
Mixer-Thread: 44100 Hz, mono, 16 Bit, drei Ringpuffer zu je 1024 Samples
(~23 ms), maximal 12 Stimmen. Der Callback markiert fertige Puffer,
das Spiel ruft pro Frame `sound::update()` auf, das fertige Puffer neu mit
den aktiven Stimmen füllt (lineares Mischen, Clipping-Schutz).

Sounds werden **aus Ruby heraus** erzeugt und angemeldet: `Rune::Sound.register(name,
rate, bytes)` nimmt einen Byte-String (16 Bit mono, little-endian,
`pack('v*')`), wird beim Anmelden linear auf 44100 Hz resampled.
`game/engine/sounds.rb` baut alle Klänge prozedural (Sinus-Töne mit
Hüllkurve, Rauschen für Schritte): `schritt`, `klick`, `jingle`,
`ablehnen`, `sprung`, `landung`. Abspielen: `Rune::Sound.play(name, volume)`.

Verkabelt sind: Menü-Klick (Esc-Menü), Schrittgeräusche nach Laufstrecke
(0.6 m Gehen / 0.85 m Rennen, über `Input.down?(:shift)`), Sprung und
Landung sowie der neue Event-Befehl `{'cmd' => 'sound', 'name' => ..., 'volume' => ...}`
im Interpreter (z. B. Jingle beim Kristall-Bergen). Fehlt winmm oder
schlägt die Device-Öffnung fehl, startet die Engine trotzdem (nur Warnung
im Log, `Rune::Sound.ready?` => false).

**Musik:** `Rune::Sound.loop(name, volume)` startet eine Endlosschleife und
ersetzt bereits laufende Loops desselben Namens (F5- und Wechsel-sicher);
`Rune::Sound.stop(name)` stoppt alle Stimmen eines Namens. Die Titel- und
Spiel-Musik ist ein prozedural erzeugtes 8-s-Akkordpad (Am - F - C - G,
je 2 s mit weichem Anstieg/Abfall, leicht verstimte zweite Stimme), in
`sounds.rb` bei 11025 Hz gerechnet und von der Engine auf 44100 Hz
hochgesampelt — Titel 0.15, im Spiel 0.22.

## Speichern/Laden (Etappe 10)

Spielstände liegen als JSON unter `projekte/<name>/saves/slot1..3.json` und
enthalten: aktuelle Karte, Spielerposition + Blick + Kameramodus, alle
Schalter und Variablen (`GameState.snapshot`), die Namen der bereits
ausgelösten Touch-/Auto-Events sowie die Spielzeit.

Im Spiel öffnet **Esc** ein Menü (Spiellogik friert ein): Zurück,
Speichern/Laden auf 3 Slots (belegte Slots zeigen den Zeitpunkt, leere
Lade-Slots sind abgedunkelt) und Rückkehr zum Titelbildschirm. Im
Titelbildschirm zeigt jeder Projekteintrag die Anzahl der Spielstände;
**L** lädt den zuletzt gesicherten Slot des gewählten Projekts
("Fortsetzen"). Beim Laden wird die Karte neu aufgebaut, der Zustand
wiederhergestellt und das Intro unterdrückt.

## NPCs (Etappe 13)

Karten können einen `"npcs"`-Block tragen: Name, Startposition, Geschwin-
digkeit, Wegpunkt-Liste (`path`) und eigene Event-Kommandos. Jeder NPC
patrouilliert seine Wegpunkte im Loop (Exakt-Positionieren am Punkt,
Umschalten zum nächsten), rendert als braune Figur (Node) und besitzt ein
**Event mit Live-Position** — Reden/Benutzen (E) funktioniert also wie bei
statischen Events, nur dass die Reichweite mitwandert. NPCs erscheinen
automatisch im Picking (`box_center`/`pick_radius` der Event-Schnittstelle).
Ohne Wegpunkte steht der NPC still; `speed` skaliert die Schrittdauer.
## Kamera-Kollision, Pixel-Look, Effekte (Etappe 14)

**Kamera-Kollision (14a):** Die Third-Person-Kamera clippt nicht mehr durch
Gelände und Hindernisse. Pro Frame märscht `camera_clearance()` in
`main.cpp` die Sichtlinie vom Spielerkopf zur Wunschposition (32 Schritte)
und prüft jeden Punkt gegen Geländehöhe (+0.3 m Rand) und alle festen
Hindernisse (um 0.3 m aufgeblasen). Blockt die Linie, wird die Kamera
sofort auf die freie Distanz gezogen (kein Rückweg durch Geometrie), das
Wieder-Herausziehen läuft geglättet (5/s). Ist das Hindernis näher als
1.2 m, hebt die Kamera sich lieber über es hinweg (`collision_lift`, in
0.5-m-Schritten bis 2.5 m) — Zaun und Brunnen werden überflogen statt
dass das Bild in der Wand landet. Der Ist-Zustand steht im HUD
(`third d=7.0 a=3.9 k=3.9 l=0.5`: Soll-, Ist-Distanz, Kollisionsdistanz,
Anhebung) via `Rune::Camera.info`. Der Titel-Überblicksabstand (28 m)
gilt nur auf dem `Scene_Title`-Titelbild — beim Spielstart stellt
`Game.setup` auf 7 m.

**Pixel-Schärfe (14b):** Der Textur-Sampler nutzt
`D3D11_FILTER_MIN_MAG_MIP_POINT` statt linear — Texel bleiben auch aus der
Ferne krisselig sichtbar (Retro-Look), entfernte Flächen verschwimmen
nicht mehr. Die prozeduralen Texturen haben nur ein Mip-Level, also
entsteht der typische Pixel-Funkeln bei Distanz ganz ohne Mip-Verwaltung.

**Effekte (14c):** `game/engine/effects.rb` ist ein leichtgewichtiges
Partikelsystem in Ruby: kleine Boxen mit Lebenszeit, Geschwindigkeit und
Schwerkraft; das Verblassen passiert über die Grösse (`Gfx.box` hat kein
Alpha). Vorgefertigt: `Effects.sparkle` (Funken, Kristall/Belohnung),
`Effects.dust` (Landung/Staub) und `Effects.burst` (frei färbbar). Im
Spiel: Landestaub beim Aufkommen, Funken beim Kristall-Bergen (neuer
Interpreter-Befehl `{"cmd":"effect","name":"sparkle"}` — Ursprung ist
die Event-Position, sonst der Spieler). Update/Draw hängt in `main.rb`
vor bzw. nach dem Szenen-Zeichnen; Kopflos-Test: `tools/effects_test.rb`.
## NPCs im Savegame und Kollision (Etappe 15)

**Speichern/Laden:** `Npc#snapshot` liefert Name, Position und Wegpunkt-
Index; `Game.save_game` hängt die Liste unter `"npcs"` ins Savegame.
Beim Laden werden die frisch gebauten NPCs über den Namen gesucht und
`restore!` setzt Position, Node und Wegpunkt zurück — die Patrouille
läuft ab dem gesicherten Punkt weiter. Im Log steht je NPC eine
"Wiederhergestellt"-Zeile.

**Kollision:** Der Spieler schiebt sich nicht mehr durch NPCs: die Physik
bekam `Physics::push_out(cx, cz, radius)` (XZ-Kreis, Ruby-Binding
`Rune::Player.push_out`); der Schub wird verworfen, wenn er den Spieler in
ein Hindernis drücken würde. Umgekehrt wartet der NPC, wenn die nächste
Teilposition innerhalb `block_radius` (0.8 m) des Spielers läge — er
stoppt also vor dem Spieler statt durch ihn zu laufen.



## Früherer Ingame-Szeneneditor (Etappe 16; entfernt in 19t)

Der historische Ruby-Szeneneditor mit F4-Editorzweig und internem
Playtest-/Snapshot-Wechsel wurde in 19t entfernt. Karten werden im separaten
`RuneEditor.exe` bearbeitet; dessen F4 startet einen eigenen Rune3D-Prozess.
Der Laufzeit-Szenenbau für Spiel und Titelbildschirm lebt nun in
`MapObjects`.
Das bisherige interne `Editor` und `Game.playtest_start/stop` sind keine
verfügbaren Ruby-APIs mehr. Der getrennte Prozess ist keine Sandbox und
verwendet reguläre Spielstände und Script-Dateizugriffe.

## Scene-Editor als eigenes Programm (Etappe 17)

Der Editor ist **kein Spielmodus mehr**, sondern ein eigenes Programm:
`build/RuneEditor.exe` (`src/editor/editor_main.cpp`). Er teilt sich mit
dem Spiel nur Renderer, Terrain und Log — keine Game-Engine, keine
Physik, keine Events, kein Interpreter. Er arbeitet ausschliesslich auf
den Kartendaten (JSON) und rendert eine Vorschau; nichts kann therefore
beim Editieren kaputtgehen.

**Aufbau (native Win32, ComCtl32 v6 via Manifest):**
Links Werkzeug-Palette (ListBox mit den Prefabs, Buttons Setzen/
Auswählen/Drehen/Löschen/Duplizieren), Mitte 3D-Viewport als
Child-Fenster mit eigenem D3D-Swapchain, rechts Objektliste und
Eigenschaften-Felder (X/Z/Drehung + Übernehmen), oben Menü (Datei/
Karte/Ansicht/Hilfe — Karten dynamisch), unten Statusleiste.

**Datenfluss:** Editor liest `projekte/<name>/data/maps/*.json` mit
einem eigenen Minimal-JSON-Parser (`src/editor/json.h/.cpp`). Die
geparste Karte bleibt als Baum im Speicher; Änderungen betreffen nur
das `objects`-Array — `events` und `npcs` überleben das Speichern
unangetastet. Gespeichert wird im selben Pretty-Format wie Ruby
(2 Leerzeichen, Arrays mehrzeilig), damit Spiel und Editor sich nicht
gegenseitig das Format umwerfen.

**Prefabs:** eine Quelle für Spiel und Editor — `game/data/
prefabs.json` (Export: `ruby tools/export_prefabs.rb`). `prefabs.rb`
liest dieselbe Datei. Textur-Änderungen am Prefab-Bestand: Datei
ändern oder Prefabs in prefabs.rb anpassen und neu exportieren.

**Texturen:** Der Editor startet einmalig die eingebettete CRuby-VM
und lädt `game/editor/boot.rb`, das nur `Textures.register_all`
aufruft. Danach läuft alles in C++ (kein Tick, keine Szene).

**Bedienung:** Rechts-Maus halten = umsehen, WASD = bewegen,
Q/E runter/hoch, Mausrad = Höhe, Shift = schnell. Linksklick setzt
(Raster 1 m) oder wählt aus, R dreht +30°, Entf löscht, Strg+S
speichert. F12 macht einen Viewport-Screenshot nach log/.

**Geprüft:** Setzen → Auswählen → Drehen (rot=0.5236 = 30°) →
Löschen → Strg+S; die gespeicherte Karte ist bis auf die Änderung
byte-identisch zu Ruby-Format und JSON-valid; events/npcs bleiben
erhalten.

## Karten & Karten-Eigenschaften (Etappe 17b)

**Rechteckige Karten:** `Terrain::generate(width, height, resolution,
seed, amplitude)` baut jetzt Rechtecke (Zellen bleiben quadratisch,
`res_z` wird proportional abgeleitet). Alte Karten mit `size` laufen
weiter (Fallback in boot.rb, main.rb, Editor).

**Karten-JSON (neue Felder, alle optional):**

```json
"terrain":  { "width": 240, "height": 240, "resolution": 192,
              "seed": 7, "amplitude": 18.0 },
"fog":      { "on": true, "far": 120, "color": 15263976 },
"panorama": { "color": 4026293 },
"note":     "Kommentar zur Karte"
```

`panorama.color` ersetzt den Himmelverlauf durch eine Vollfarbe,
`fog` blendet die Ferne ab `0.45 * far` sanft zur Nebelfarbe
(smoothstep, im Pixel-Shader über Weltposition minus Kameraposition).
`Project.create` legt neue Projekte **flach** an (amplitude 0) —
Berge pro Karte im Eigenschaften-Dialog zuschaltbar.

**Editor:** Menü Karte hat jetzt *Neue Karte...*,
*Eigenschaften...* und *Karte löschen* plus die dynamische Karten-
Liste (Radio-Markierung der aktiven Karte). Der Eigenschaften-Dialog
(natives Win32-Fenster, moduslos) editiert Name, Breite/Höhe,
Berge an/aus + Seed + Höhe, Nebel an/aus + Sichtweite + Farbe,
Panorama-Farbe und einen freien Kommentar; OK regeneriert das
Gelände sofort. Neue Karten heissen welt01, welt02, ...

**Geprüft:** Eigenschaften-Dialog (Nebel an, 120 m, 0xE8E8E8) →
Nebel im Editor-Viewport sichtbar; Strg+S → fog/panorama/note in
der JSON, events/npcs/objects unangetastet; Neue Karte 320x240
ohne Berge → flache Ebene + welt01.json korrekt; Rune3D startet in
dorf und zeigt denselben Nebel (Karten-Atmosphäre gilt im Spiel).

## Maus-Werkzeuge und Undo/Redo (Etappe 17c)

**Verschieben-Werkzeug:** Der Button *Verschieben* (Checkbox, linke Spalte)
schaltet den Drag-Modus. Ein Linksklick auf ein Objekt wählt es aus und
startet das Ziehen: `push_undo()` genau einmal beim Drag-Start, dann
`SetCapture`. `WM_MOUSEMOVE` rechnet aus der Mausposition einen Strahl
(fov 60 Grad, Aspect aus dem Viewport) und schneidet ihn mit der
horizontalen Ebene `y = drag_y` (Terrainhöhe des Objekts beim Start);
die neue X/Z-Position wird auf das Raster (`g.grid`, 1 m) gerundet und
sofort in Eigenschaften-Feldern, Objektliste und Titel angezeigt.
`WM_LBUTTONUP` beendet den Drag und gibt die Maus frei.

**Undo/Redo:** Jede Mutation (`Setzen`, `Loeschen`, `Duplizieren`,
`Eigenschaften uebernehmen`, Drag-Start) legt vorher einen
JSON-Schnappschuss des kompletten `objects`-Arrays auf den Undo-Stapel
(max. 50 Einträge, Redo wird bei neuer Aktion geleert). `Strg+Z` /
`Strg+Y` sowie die Buttons *Undo*/*Redo* tauschen Schnappschüsse
zwischen den beiden Stapeln; nach dem Wiederherstellen werden Liste und
Felder neu synchronisiert und die Karte als geändert markiert.

**Geprüft:** Objekt setzen -> Strg+Z entfernt es -> Strg+Y stellt es
wieder her; Drag im Verschieben-Modus ändert X/Z rastergenau (Log:
`Drag-Ende: idx=… x=-9.000000 z=-12.000000`); Strg+S schreibt die neue
Position in die Karten-JSON.

## Event-Editor (Etappe 17d)

**Eigenes Fenster:** `Events -> Event-Editor...` (`ID_MENU_EVENTS = 107`)
öffnet ein modusloses Top-Level-Fenster der Klasse
`RuneEditorEventDlg` (kein modaler Loop, damit der D3D11-Flip-Model-
Viewport weiterrendert). Beim Kartenwechsel werden seit 19o.5 zuerst offene
Entwürfe geklärt und das Fenster anschließend geschlossen (`load_map` ruft
`close_event_editor()`). Befehlsreferenzen verwenden inzwischen Pfade.

**Daten:** Der Dialog arbeitet direkt auf `g.map_root["events"]` und
schreibt dieselben Schlüssel wie die Engine (`id`, `name`, `x`, `z`,
`radius`, `trigger`, `commands`). Gespeichert wird unverändert mit
`Strg+S` im Hauptfenster; Undo/Redo deckt weiterhin nur `objects` ab.

**Aufbau:** links Event-Liste mit *Neu* (setzt das Event auf die
aktuelle Mausposition im Viewport), *Löschen* und *Duplizieren*
(neue ID, eindeutiger Kopiename und, soweit möglich, X/Z + 1). Rechts die Event-Felder
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
*Einfügen* baut ein neues Kommando; seit 19o.5 patcht *Befehl übernehmen*
beim gleichen Typ nur geänderte Felder. Ein Typwechsel ersetzt den Befehl
nach Warnung, einschließlich seiner Zweige; fremde Daten unberührter
Befehle und Events bleiben erhalten.

**Tastatur:** Solange das Event-Fenster den Fokus hat, sind die
globalen Editor-Shortcuts (R, Entf, Strg+Z/Y, ...) abgeschaltet
(`event_editor_focused()`), damit Tippen in Namens- und Textfeldern
keine Objekte dreht oder löscht.

**Geprüft:** Build und alle acht Ruby-Tests grün. Ein interaktiver
Fenstertest war in der abgeschotteten Build-Umgebung nicht möglich
(der Editor landet auf einem nicht sichtbaren Desktop; `EnumWindows`
sieht nur das Konsolenfenster).

## Projekt-Scripts und Script-Editor (17e / 19y)

Jedes Projekt darf `data/scripts.json` als Array oder Objekt mit `scripts`-
Array enthalten. Benannte Event-Scripts werden über `Scripts.run` im
`Scripts::Context` mit Helfern wie `say`, `switch`, `variable`, `teleport`
und `party` ausgeführt; Inline-Code bleibt möglich. Ein `autoload: true`
markiert ein Startscript: `Scripts.run_startup` führt diese nach
`enter_map` und `RGSS.setup` in Dateireihenfolge über `TOPLEVEL_BINDING`
aus, auch bei F5. Nicht markierte Altscripts starten nicht automatisch.
Fehler werden protokolliert und folgende Einträge laufen weiter. Der
Kontext ist **keine Sicherheits-Sandbox**; Ruby ist vertrauenswürdiger Code.
Die vorhandene RGSS-Schicht ist teilweise XP-kompatibel, nicht vollständig.

F11 (`script_dialog_win.h`) ist moduslos und nutzt ein Edit-Control mit
separater Zeilenspalte, Scroll-/Cursoranzeige, Tab/Shift+Tab, Strg+S und
nicht ausführender CRuby-Syntaxprüfung (`RubyVM::InstructionSequence.compile`
geschützt über `rb_protect`). Das portable `script_document.h` erhält
unbekannte Felder und unveränderte Codezeilenenden; mehrdeutige, ungültige
oder nicht vollständig darstellbare Felder werden geschützt. Auswahlwechsel
übernimmt den alten Eintrag; bei Reload/Schließen/Projektwechsel werden
Entwürfe geklärt. `DocumentStore` prüft den Dateistand und meldet Konflikte.
Wie bei Karten ist die Sicherung/Promotion kein atomarer Namensaustausch.

`tools/editor_workflow_test.cpp` prüft Modelle portabel, Ruby-Tests prüfen
opt-in Autoload und Fehlerisolierung; `tools/editor_ui_smoke.cpp` steuert
die echte Windows-EXE mit synthetischen Nachrichten in einer isolierten
Paketkopie. Das ersetzt keinen interaktiven DPI-/Hardware-Test.

## Grafik-Import und Materialbase (Etappe 17f)

**Projektdateien:** Importierte Grafiken liegen unter
`projekte/<p>/data/textures/<name>.rgba` (rohe 8-Bit-RGBA-Bytes, Zeilen von
oben nach unten, 4 Bytes pro Pixel). Die Metadaten stehen in
`projekte/<p>/data/textures.json`:

```json
{ "textures": [
    { "name": "wand_bunt", "file": "wand_bunt.rgba", "w": 64, "h": 64,
      "tile": 2.0, "source": "C:/bilder/wand.png" } ] }
```

**Editor:** `Grafik -> Grafik importieren...`
(`ID_MENU_IMPORT_TEX = 109`) öffnet den Windows-Dateidialog (PNG/BMP),
dekodiert per WIC (`decode_image_wic`, `-lwindowscodecs`) und schreibt
`.rgba` + `textures.json` (`mat_import_file`). Der Name wird aus dem
Dateinamen abgeleitet (`mat_slug`: klein, nur `a-z0-9_`, eindeutig).

`Grafik -> Materialbase...` (`ID_MENU_MATERIALS = 110`) öffnet das
moduslose Fenster `RuneEditorMatDlg` (IDs 7001-7013): Liste aller Prefab-
und importierten Texturen, Kachelgrösse ändern, importieren, löschen
(nur importierte), auf das ausgewählte Objekt anwenden (`obj["tex"]`)
oder den Objekt-Override entfernen.

Das Eigenschaften-Panel hat dafür ein Feld **Material**
(`IDC_ED_TEX = 3023`); leer = Prefab-Texturen, sonst Materialname.
`push_object` zeichnet dann alle Teile des Objekts mit dieser Textur.

**Spiel:** `Textures.register_project` liest `textures.json`, prüft
`w * h * 4` Bytes und meldet jede Datei über `Rune::Gfx.texture` an. Der
Aufruf steht in `Game.setup` direkt nach `Textures.register_all` (also
auch nach F5). `Textures.tile_of(name)` liefert die Kachelgrösse aus den
Metadaten. `Prefabs.instantiate` wertet `obj["tex"]` aus und überschreibt
Textur und Kachel aller Prefab-Teile.

**Kopflos testen:** `build/RuneEditor.exe --import-tex <bild> [name]`
importiert ohne Fenster (Projekt `dorf`) und gibt `OK:`/`FEHLER:` aus.

**Wichtig beim Linken:** Die Ruby-DLL exportiert eigene CRT-Symbole
(`fclose`, `fwrite`, ...). Ohne `-lucrtbase` **vor** der
Ruby-Importbibliothek werden unsere stdio-Aufrufe auf die Ruby-Exporte
umgebogen; `fclose` stürzt dann mit Access Violation ab. `build.ps1`
linkt deshalb `-lucrtbase` zürst.

**Geprüft:** `ruby tools/tex_import_test.rb` (BMP -> RGBA, Zeilenordnung,
Farben, Metadaten, Aufräumen) und `ruby tools/project_tex_test.rb`
(Spielseite: Metadaten, Grössenprüfung, Registrierung, `tile_of`) sowie
alle bisherigen Tests grün.

## Problem-Backlog in den Programmen (Etappe 18h)

Engine und Editor erkennen Fehler jetzt **selbst** und sammeln sie:

**Editor — Probleme-Dialog (F10, Extras-Menü, ID 116):** Nach jedem
Kartenladen läuft `issues_scan_map()` und füllt den Backlog
(`g_issues`): Objekte ohne/mit unbekanntem Prefab-Typ, Material-Overrides
ohne geladene Textur, Events mit unbekanntem Trigger, kaputte Seiten
(kein `commands`-Array, unbekannter Trigger, unbekannte Bedingungs-
Schlüssel) und Befehlslisten rekursiv (unbekannter Befehlstyp, fehlende
`then`/`else`/`options`/`branches`/`lines`). Jeder Fund geht zusätzlich
als `[Backlog]`-Zeile ins editor.log. Der moduslose Dialog
`RuneEditorProblemsDlg` (IDs 5301–5304) zeigt Bereich + Schwere
(`[Fehler]`/`[Warnung]`), hat „Neu scannen“ und zählt die Funde im
Titel. Auch Karten-Lese-/Schreibfehler landen im Backlog.

**Spiel — `Issues`-Modul (`game/engine/issues.rb`):** Laufzeitfehler
werden gemeldet und landen **zusätzlich** zum Log in
`log/issues.json` (wird nach jeder Meldung neu geschrieben, löscht sich
bei `Issues.clear!` in `Game.setup` — also pro Session/F5 neu):

- Interpreter: unbekanntes Kommando
- Scripts: fehlendes Script, Defekte (Syntax/Laufzeit) als `error`
- Party: unbekanntes Item beim Benutzen
- Weather: unbekannter Wetter-Typ (fällt auf 'kein' zurück)
- Texturen: fehlende/defekte Projekt-Texturdatei

**Geprüft:** `tools/issues_test.rb` (21 Checks: Modul, Interpreter,
Scripts, Party, Wetter, Datei-Inhalt), `log/live18h.ps1` (kaputtes
Testevent per Ruby injiziert → Probleme-Dialog zeigt beide Funde mit
Event-Namen, Rescan, Schließen, `[Backlog]` im editor.log), 16 Tests
grün, 8-s-Spielrun ohne Fehler und ohne issues.json (dorf ist sauber).

## Event-Seiten (Etappe 18g)

Events können jetzt **mehrere Seiten** mit Bedingungen haben — das grosse
RPG-Maker-XP-Feature. Eine Seite ist ein Block aus Bedingungen, Trigger und
Befehlsliste; es gewinnt immer die Seite mit dem **höchsten Index**, deren
Bedingungen erfüllt sind (Seite 1/Index 0 = Standard, leere Bedingungen =
immer erfüllt).

**Karten-JSON:**

```json
"pages": [
  { "conditions": { "switch":  { "name": "auftrag", "value": true },
                    "variable": { "name": "kristalle", "op": ">=", "value": 3 } },
    "trigger": "action", "commands": [ ... ] },
  { "conditions": {}, "trigger": "action", "commands": [ ... ] }
]
```

Bedingungen: `switch` (Name + an/aus) und `variable` (Name, Operator
`== != < <= > >=`, Wert); beide kombiniert sind ein UND. Alte Karten ohne
`pages` laufen weiter: `Event.build_pages` baut daraus eine implizite Seite 0
aus den Top-Level-Feldern. Ungültige Seiteneinträge (kein Hash, trigger
ausserhalb action/touch/auto/parallel, commands kein Array) werden beim
Laden normalisiert.

**Spiel (`game/engine/event.rb`):** `EventPage` ist ein Struct (conditions,
trigger, commands). `refresh_page!` bestimmt die aktive Seite neu und setzt
beim **Seitenwechsel** `fired` zurück — touch/auto-Events feuern auf der
neuen Seite erneut (klassisches RPG-Maker-Verhalten, z. B. eine Tür, die
nach dem Schalter-UMlegen beim zweiten Berühren offen ist). `trigger` und
`commands` delegieren an die aktive Seite; `manual_fire` startet deren
Kommandos. `main.rb` ruft `refresh_page!` für alle Events auf, bevor die
Trigger-Schleifen laufen.

**Editor (Event-Dialog, F6):** Linke Spalte hat unter der Event-Liste jetzt
eine **Seiten-Liste** (mit Bedingungs-Hinweis wie `[Schalter 'auftrag' = an]`)
plus *Neue Seite* / *Seite löschen* (die letzte Seite bleibt). Rechts ist
der Trigger in eine eigene Gruppenbox **"Seite"** gezogen, zusammen mit den
Bedingungsfeldern (Schalter-Name + an/aus, Variable + Operator + Wert);
leere Felder entfernen die Bedingung. Die Befehlsliste und das Befehls-
formular arbeiten auf der gewählten Seite. Seit 19o.5 zeigt F6 alte Events
ohne `pages` zunächst als implizite Seite und materialisiert sie nur bei
Bedarf; die Top-Level-Felder bleiben als Altdaten erhalten (siehe unten).

**Geprüft:** `tools/event_pages_test.rb` (44 Checks: altes Format,
Normalisierung, Seiten-Auswahl, alle Operatoren, fired-Reset,
manual_fire je Seite), `log/live18g.ps1` (Seitenliste, neue Seite,
Schalter-Bedingung, Befehl landet nur auf seiner Seite, Migration +
Speichern in dorf.json), 15 Tests grün, 8-s-Spielrun ohne Fehler.

## Event-Editor wie RPG Maker XP (Etappe 18f)

Der Event-Editor (F6) ist deutlich näher an RPG Maker XP gerückt:

**Befehlskategorien:** Die Befehls-ComboBox gruppiert jetzt 20 Typen
mit Präfixen: [Nachricht] (Text, Wahlmöglichkeiten, Log),
[Steuerung] (Warte, Schalter setzen/umschalten, Variable, Wenn
Schalter/Variable, Script), [Spieler] (Teleport, Spieler bewegen,
Heilen), [Welt] (Wetter, Effekt, Sound), [Party] (Item, Gold),
[Spiel] (Speichern, Laden). Das Enum EvType ersetzt die früheren
magischen Indexzahlen überall (Sicherheit bei 20 Typen).

**Neue Befehle (Interpreter + Editor):**
- choices: Wahlmöglichkeiten (RPG-Maker-Show-Choices). JSON:
  {"cmd":"choices","options":[...],"branches":[[...],[...]]}.
  Der Interpreter blockiert (@choices), der Spieler wählt mit
  Pfeiltasten (wrap-around) + E/Enter; der gewählte Zweig kommt
  auf den Stack. UI.choices zeichnet das Wahlfenster (Marker,
  Hinterlegung, "↑↓ + E"-Hinweis). Editor: Optionen als Zeilen im
  Textfeld, Zweige erscheinen als "Wenn [x]:"-Kopfzeilen im Baum
  (ev_flatten wie Dann/Sonst); Zweige bleiben beim Bearbeiten
  erhalten (ev_build_command übernimmt alte branches).
- toggle_switch: Schalter umschalten (GameState.toggle_switch).
- move_player: Spieler bewegen, absolut oder relativ (Rune::Player
  .teleport + Game.gy).
- weather: jetzt auch im Editor (war seit 17h nur im Interpreter).
- save_game/load_game: Spielstand per Event (Game.save_game/
  load_game, Slot 1-3). Achtung: load_game baut die Karte neu auf
  und ersetzt damit den laufenden Interpreter (dokumentiert).

**Trigger "parallel":** RPG-Maker-Parallel-Process. Das Event startet
sich neu, sobald es fertig ist — aber nur, wenn der Interpreter frei
ist (Single-Interpreter-Engine). Ein 'wait' im Event steuert das
Tempo. Einschränkung: blockiert wie andere Events die NPC-Logik,
solange es läuft (kein echter Hintergrund-Thread).

**Layout:** Fenster 1000x730 (vorher 960x700), Gruppenboxen
"Event-Eigenschaften" und "Befehle", grössere Befehlsliste
(260 statt 248 px), breitere Typ-ComboBox (250 px für Kategorie-
Präfixe), "Ziel:"-Anzeige versteht jetzt auch "Wenn [x]:"-Zweige.

**Geprüft:** tools/event18f_test.rb (23 Checks: toggle, move,
weather, save/load, choices mit Wrap/Verschachtelung, parallel im
Datenmodell, unbekannter Befehl), log/live18f.ps1 (Editor: 20 Typen,
Kategorie-Präfixe, choices einfügen mit Liste+JSON, parallel-
Trigger, Speichern — Achtung: Event-Dialog blockiert Strg+S, deshalb
erst schliessen). 13 Bestandstests + live18e/18d/18c grün, 8-s-
Spielrun ohne Fehler.

## Mal-Werkzeug für den Boden (Etappe 18e)

Splat-Flächen lassen sich jetzt direkt **mit der Maus in den Viewport
malen** (RPG-Maker-Feeling statt Zahlenfelder):

**Werkzeug:** Radio-Button "Boden malen" im linken Panel
(IDC_BTN_PAINT 3026, viertes Werkzeug neben Setzen/Auswählen/
Verschieben). Linksklick im Viewport malt eine Fläche in
Pinselgröße um den Terrain-Treffer, **Shift+Klick radiert** die
oberste (späteste) Fläche an dieser Stelle. +/- (auch Zehnertastatur)
ändert die Pinselgröße (1..64 m, Raster-gerundet), die Statuszeile
zeigt Textur + Größe + Radierer-Hinweis. Pinseltextur = aktuelle
Auswahl im Tileset-Dialog (F7); Wechseln dort übernimmt sie live in
den Pinsel. Brush-Vorschau: flaches hellblaues Quadrat in Pinselgröße
am Cursor (push_hover_marker).

**Kernfunktion splat_edit():** zentriert x/z, snappt aufs Raster
(g.grid), legt terrain.splats an bzw. hängt an; beim Radieren
entfernt sie die letzte den Punkt überdeckende Fläche und räumt eine
leere Liste komplett aus dem JSON (json_remove_key). Aufruf auch
aus WM_LBUTTONDOWN des Viewports (Mal-Modus vor place_mode geprüft);
danach regen_terrain() + ts_fill_splats() + dirty.

**Fallstricke (für Live-Tests):** WM_LBUTTONDOWN auswerten: MK_SHIFT = 0x4
(0x8 ist Strg!) - der erste Live-Test radierte nicht, weil der Test
9 statt 5 als wParam schickte. Viewport ist ein KIND-Fenster:
FindWindowExW unterhalb des Hauptfensters, nicht EnumWindows.

**Geprüft:** Live-Test log/live18e.ps1 (Modus aktivieren, Texturwahl
lauscht mit, gemalter Fleck + radiert in Log/JSON, +/-, Prozess
lebt), live18d/live18c-Regression grün, 13 Tests grün, 8-s-Spielrun
ohne Fehler.

## Splat-Böden (Etappe 18d)

Eine Karte zeigt jetzt mehrere Boden-Texturen gleichzeitig: rechteckige
**Flächen** im Karten-JSON (`terrain.splats`: Liste von
`{"tex","x","z","w","h"}` in Weltmetern, Karte auf 0 zentriert; der
spätere Eintrag gewinnt).

**Technik:** Der Renderer baut aus allen registrierten 64x64-Texturen
einen Atlas (`Renderer::update_atlas()`, Raster aus 64-px-Zellen, neuer
Slot "atlas" pro Aufruf - F5-sicher). `Terrain::generate_splat()` legt
pro Quad vier eigene Vertices an (nicht geteilt) mit UVs innerhalb
genau einer Atlas-Zelle (halb-Pixel-Inset gegen bilineares Bluten);
die Quad-Mitte entscheidet die Zelle (letzter Treffer gewinnt). Ohne
splats bleibt der alte Ein-Textur-Pfad byte-identisch
(Welt-UV, Kachelgröße via gParams.z) - alte Karten und live18c
unverändert.

**API:** `Rune::Gfx.terrain_splats(base_tex, list)` (String- ODER
Symbol-Schlüssel; Terrain wird mit den letzten generate-Parametern neu
gebaut, Atlas-Slot wird als Terrain-Textur gesetzt, tile-Skalierung 1).
`game/main.rb` nutzt den Splat-Pfad automatisch, wenn die Startkarte
`terrain.splats` hat.

**Editor:** `regen_terrain()` ist die eine Stelle, die beide Pfade
entscheidet (und von `load_map` sowie dem Tileset-Dialog aufgerufen
wird). Der Tileset-Dialog hat eine dritte Spalte "Boden-Flächen":
Liste, X/Z/Breite/Tiefe-Felder, "Hinzufügen" (mit gewählter Textur),
"Entfernen" (markierter Eintrag; leere Liste wird ganz aus dem JSON
entfernt). Nach jeder Aktion regeneriert der Boden sofort.

**Bekannte Grenze:** Jedes Terrain-Quad zeigt genau EINE Textur (kein
Weich-Übergang); die Kacheldichte hängt an der Terrain-Auflösung
(Quadgröße), `terrain.tile` wirkt im Splat-Pfad nicht.

**Geprüft:** Live-Test log/live18d.ps1 (Fläche hinzufügen/entfernen,
Splat-Log, JSON-Checks, Editor lebt), Spiel-Smoke mit 2 Splats
(Atlas + Splat-Boden im engine.log, 0 Fehler), alle 13 Tests grün,
live18c-Regression grün (Log-Format angepasst: "Kachel N m, N Fläche(n)").

## Tileset-Editor (Etappe 18c)

Neuer modusloser Dialog `RuneEditorTileDlg` (Grafik -> Tileset-Editor...,
ID_MENU_TILESET 115, Taste F7; IDs 4301-4312):

**Boden pro Karte:** Das Karten-JSON bekommt zwei neue Felder in
`terrain`: `texture` (Name der Boden-Textur, Standard "gras") und
`tile` (Kachelgrösse in Metern, Standard 2.0; erst seit dieser Etappe
einstellbar, vorher auf 2 m in die Terrain-UVs gebacken). Der Shader
skaliert die Terrain-UV mit 2.0/tile (gParams.z; Boxen nutzen weiter
gParams.y, Shadow-Pass beide 0 - sauber getrennt).

**Dialog:** Liste aller Texturen (8 prozedurale + Projekt-Importe,
mat_all_names), links Vorschau (RGBA -> 32bpp-DIB -> StretchBlt) der
angewählten Kachel, rechts Kachelgrösse + "Übernehmen (Karte)":
schreibt terrain.texture/tile, wendet sie live an (set_terrain_tex/
set_terrain_tile), markiert die Karte dirty. Status "Boden übernommen".

**Eigene Kacheln:** "Kachel erzeugen" macht aus Basisfarbe + Zweitfarbe
+ Muster (kein/punkte/streifen/rauschen) eine 64x64-RGBA-Kachel
(deterministischer LCG gleicher Eingabe = gleiche Kachel), meldet sie
im Renderer an (sofort in Vorschau und Viewport testbar). "Als
Projekt-Textur speichern" schreibt sie als .rgba + textures.json-Eintrag
(Materialimport-Syntax; das Spiel lädt sie beim Start über
Textures.register_project).

**Renderer-Zugriff:** add_texture führt jetzt Pixel + Grösse mit
(m_tex_pixels/m_tex_sizes); texture_rgba(name, out, w, h) liefert sie
für Vorschauen zurück. Rune::Gfx.terrain_texture kennt ein zweites,
optionales Argument (Kachelgrösse); game/main.rb liest terrain.texture/
tile der Startkarte statt fix 'gras' zu setzen. Der Editor wendet beide
Werte beim Laden einer Karte an.

**Umlaut-Fix:** Die sichtbaren Fenstertexte (L"-Literale) sind jetzt
einheitlich echte Umlaute ("Löschen", "Schließen", "Ausgewählt",
"Übernehmen (Karte)" zeigt korrekt "Übernehmen"); nur Log-Strings
bleiben transliteriert, weil die Live-Tests darauf matchen. Die Quelle
ist durchgängig UTF-8, GDI/GDI-Text kommt korrekt als UTF-16 in die
EXE (Tools: scan_encoding.rb, check_binary_umlaute.rb, fix_umlaute.rb).

**Geprüft:** Live-Test log/live18c.ps1 (Dialog offen, Liste,
Boden-Wechsel auf "weg" + 4-m-Kachel landet nach Strg+S im Karten-JSON,
Kachel erzeugen + als Projekt-Textur speichern, Aufräumen), alle 13
Tests grün, 8-s-Spielrun ohne engine.log-Fehler.

## Projekt-Auswahl im Editor (Etappe 18b)

Der Editor war bisher fest auf `projekte/dorf` eingestellt. Jetzt:

**Projekt-Dialog:** Datei -> Projekt wechseln... (ID_MENU_PROJECT 114)
öffnet den moduslosen Dialog `RuneEditorProjectDlg` (IDs 4201-4205):
Liste aller `projekte/*/data/system.json`-Projekte, Doppelklick oder
"Öffnen" wechselt, Textfeld + "Anlegen" legt ein neues Projekt an.

**Neues Projekt:** create_project() baut das komplette Gerüst:
`projekte/<name>/{data/{maps,},saves}/`, `system.json` (Titel,
Startpunkt auf die Startkarte), leere `scripts.json` und
`database.json` (gold 0, leere actors/items), flache Startkarte
`<name>.json` (240x240, ohne Berge). Projektname: Buchstaben, Zahlen,
Unterstrich.

**switch_project(name):** schliesst offene Dialoge (Zeiger werden
ungültig), setzt project_dir/name zurück, leert Undo/Redo und
Karte, lädt textures.json (mat_load) und scannt/maps/Menü neu;
erste Karte wird geladen. Log: "Projekt gewechselt: <name> (N Karten)".

**Start:** CLI `--project <name>` gewinnt; sonst Merker
`log/last_project.txt` (schreibt switch_project); sonst "dorf".
Ungültiges Projekt fällt auf "dorf" zurück (Log-Warnung).
`--import-tex` kennt jetzt ebenfalls `--project <name>`.

**Geprüft:** Live-Test log/live18b.ps1 (CLI-Start, Merker-Neustart,
Rückfall, Dialog-Doppelklick-Wechsel, Anlegen mit Datei-Checks,
Aufräumen; EnumWindows statt FindWindowW!), alle 13 Tests grün.

## Fenster-Bedienkomfort (Etappe 18a)

**DPI-Schärfe:** Beide Programme (Spiel und Editor) setzen beim Start
`SetProcessDpiAwarenessContext(PER_MONITOR_AWARE_V2)` - die Darstellung
bleibt auf skalierten Displays (125 %/150 %) scharf, statt von Windows
unscharf hochskaliert zu werden.

**Eigene Fenster-Icons:** Editor (gelber Runenstein) und Spiel (blauer
Kristall) zeichnen ihr Icon zur Laufzeit prozedural in GDI (16x16 +
32x32, kein Ressourcen-File - wie Toolbar-Icons und Texturen).
Wichtig: `CreateIconIndirect` verlangt eine ECHT monochrome Maske
(`CreateBitmap(1bpp)`) und ein 32bpp-DIB mit Alpha=255 - ein
Farb-Bitmap als Maske ist undefiniert und brachte die Titelzeichnung
zum Absturz (0xc0000005 im Live-Test gefunden und behoben).

**Mindestfenster-Grösse (historisch 18a, in 19w ersetzt):** WM_GETMINMAXINFO im Editor begrenzte das
Fenster auf mindestens 1024x640 - das Layout bricht beim Verkleinern
nicht mehr.

**Drag&Drop-Texturimport:** PNG/BMP-Dateien können direkt ins
Editor-Fenster gezogen werden (WM_DROPFILES + DragAcceptFiles); jede
Datei läuft durch den bestehenden mat_import_file (WIC-Dekodierung,
.rgba + textures.json, Duplikatnamen '_2', '_3' ...). Import wird im
editor.log protokolliert.

**Tastatur:** G schaltet das Fang-Raster (1.0 / 0.5 / 0.25 / 0.01 m),
Esc hebt die Auswahl auf, Enter übernimmt die Eigenschaften-Felder.
Alles wird ins editor.log geschrieben (Live-Test verifiziert darüber).

**Doppelklick in der Objektliste** richtet die Kamera auf das Objekt
(Position + Blickwinkel), Log: "Kamera auf Objekt #N gerichtet".

**Statusleiste:** Feld 3 zeigt jetzt "Raster x.xx m | y FPS".

**Grenze (für Live-Tests):** Statusleisten-/Textinhalte lassen sich
per SendMessage von aussen NICHT lesen - WM_USER+-Meldungen werden
über Prozessgrenzen nicht gemarshallt, der Zeiger zeigt in den
fremden Adressraum (SB_GETTEXTW von aussen -> Absturz des Editors!).
Live-Tests gehen deshalb nur über Log-Einträge und Prozessleben.

**Geprüft:** Live-Test log/live18a.ps1 (Fenster-Icon via WM_GETICON,
Mindestgrösse via WM_GETMINMAXINFO, G/Doppelklick/Esc via Log +
Prozessleben, stderr leer, editor.log fehlerfrei), alle 13 Tests
grün, Spiel-Smoke-Run 8 s ohne Fehler (engine.log).

## Wetter, Werkzeugleiste, Sound-Test, Playtest (Etappe 17h)

**Wetter (Spiel):** Karten-Feld `"weather": {"type":"kein|regen|schnee",
"intensity":1.0}`. Das Modul `Weather` (game/engine/weather.rb) hält ~40
Partikel je Intensitätsstufe um den Spieler im Leben (Regen = schnelle
blaue Straflen 22 m/s, Schnee = langsame weisse Flocken 2.6 m/s mit
Drift). `Weather.load_map(map)` aktiviert es beim Kartenstart,
`Weather.update(dt, px, py, pz)`/`Weather.draw` laufen in Game.update/draw.

**Wetter zur Laufzeit umstellen:** Interpreter-Befehl
`{"cmd":"weather","name":"schnee","intensity":1.5}` bzw. Script-Helfer
`weather('regen', 2.0)` (ohne Argument: Typ lesen).

**Wetter im Editor:** Karten-Eigenschaften (F2) füttern Combobox
`kein/regen/schnee` (ID 4113) + Intensität (4114); OK schreibt das
`weather`-Objekt ins Karten-JSON (live per WM_COMMAND getestet, siehe
log/live17h.ps1).

**Editor-Werkzeugleiste (historisch 17h; aktueller Stand siehe 19w):** Toolbar unter dem Menü mit 8 Buttons
(Speichern, Karten-Eigenschaften, Events, Scripts, Materialbase,
Datenbank, Sound-Test, Playtest). Die Icons zeichnet der Editor zur
Laufzeit in GDI (16x16, keine Ressourcen-Datei - wie Texturen/Sounds
komplett prozedural). Buttons senden dieselben IDs wie das Menü.
F-Tasten: F1 Hilfe, F2 Eigenschaften, F3 Materialbase, F4 Playtest,
F6 Events, F8 Sound-Test, F9 Datenbank, F11 Scripts.

**Zoom:** Mausrad im Viewport = Dolly entlang der Blickrichtung
(Shift+Rad = Höhe ändern, Altverhalten).

**Sound-Test (Editor):** Extras -> Sound-Test... (F8) öffnet den
moduslosen Dialog `RuneEditorSoundDlg` (IDs 8101-8107): Liste der 7
Spiel-Sounds, Abspielen mit Lautstärke 0.0-1.0, Musik als Loop
an/aus. Der Editor initialisiert dafür die waveOut-Engine
(core::sound) und meldet die Sounds per Ruby-Boot an
(game/editor/boot.rb requires zusätzlich sounds.rb).

**Playtest (aktualisiert in 19u):** Extras -> Playtest starten... (F4)
speichert die ausgewählte Karte und startet Rune3D als eigenen Prozess
mit `--project <name> --map <karten-id>`. Ohne Kartenauswahl gilt die
Projekt-Startkarte. Projekt und Karte werden unverändert über die Wide-CLI
übergeben; eine ungültige ausdrückliche Auswahl zeigt einen Fehler. Ein
unbekanntes Projekt mit `--project`-Karte bricht den Playtest ab (F4);
ohne F4 wählt die Engine das Projekt intern (last_played → erstes) und
startet auf dem `Scene_Title`.

**Geprüft:** `ruby -I game/engine tools/weather_test.rb` (22 Checks),
alle 13 Tests grün, Live-Test log/live17h.ps1 (Toolbar, Zoom,
Wetter-Dialog schreibt JSON, Sound-Dialog, Playtest-Prozess +
engine.log-Vermerk, Aufräumen). Dieser historische 17h-Test belegt nicht
den späteren 19u-Ablauf mit zwei unterschiedlichen Karten.

## Datenbank: Helden, Items, Gold (Etappe 17g)

**Projektdatei:** `projekte/<p>/data/database.json` (optional - fehlt sie,
gilt ein leerer Katalog):

```json
{ "gold": 120,
  "actors": [ { "id": "arik", "name": "Arik", "class": "Krieger",
                "level": 1, "hp": 120, "mp": 15, "atk": 14, "def": 10,
                "spi": 4, "agi": 8, "exp": 0 } ],
  "items":  [ { "id": "heiltrank", "name": "Heiltrank", "kind": "heal",
                "power": 40, "price": 25, "start": 2, "desc": "..." } ] }
```

**Spiel:** `Database.database` lädt die Datei, `Party.setup` baut daraus
die Party (HP/MP starten voll, `start` = Startmenge). `Party` hält den
veränderlichen Zustand: `add_item/take_item/use_item` (heal/mana wirken auf
den ersten verletzten Helden, quest/money sind nicht benutzbar),
`add_gold`, `heal_all`, `snapshot/restore!`. Spielstände nehmen den
Party-Snapshot mit (`'party'` in `save_game`/`load_game`). Im Esc-Menü
zeigt ein Panel daneben Helden, Gold und Inventar (`Party.status_lines`).

**Interpreter-Befehle** (zusätzlich):
`{"cmd":"item","name":"heiltrank","count":2}` bzw. `"take":1`,
`{"cmd":"gold","amount":50}` (negativ = abziehen),
`{"cmd":"heal"}` (voll) bzw. `{"cmd":"heal","amount":30}`.

**Script-Kontext** (`Scripts::Context`) kennt dafür: `party`,
`gold(amount)`/`gold()`, `items`, `item_count`, `add_item`, `take_item`,
`use_item`, `heal`.

**Editor:** Menü `Datenbank -> Datenbank (Helden/Items)...`
(`ID_MENU_DATABASE = 111`) öffnet das moduslose Fenster
`RuneEditorDbDlg` (IDs 8001-8055): Reiter Helden/Items, Listen links,
Formular rechts (ID/Name/Klasse/HP/MP/ATK/DEF/SPI/AGI bzw.
ID/Name/Art/Stärke/Preis/Startmenge/Beschreibung), Gold-Feld,
Neu/Löschen/Speichern/Neu laden - schreibt `database.json`.

**Event-Editor:** drei neue Befehlstypen *Item geben/nehmen* (negative
Menge = nehmen), *Gold +/-*, *Heilen* (leer = voll).

**Demo:** `game/data/database.json` (Vorlage, wandert per
`Project.ensure_demo` in neue Projekte) und `projekte/dorf/data/database.json`
mit Arik/Mira und Heiltrank/Manatrank/Wasserkristall/Goldbeutel, angelegt
per `ruby tools/add_database17g.rb`.

**Geprüft:** `ruby -I game/engine tools/party_test.rb` (Setup, Inventar,
Benutzen, Gold, Heilung, Snapshot/Restore, Interpreter-Befehle,
Kontext-Helfer - 39 Checks) sowie alle bisherigen Tests grün.


## Höhen-Malwerkzeug & Kartengrenze (Etappe 18l)

**Berge direkt anmalen statt Seed-Fummelei:** Das Karten-JSON bekommt unter
`terrain` eine optionale Liste `heights` mit weichen Höhen-Pinseln:

```json
"terrain": {
  "width": 240.0, "height": 240.0, "resolution": 192,
  "seed": 7, "amplitude": 0.0,
  "heights": [ { "x": 45.0, "z": -79.0, "r": 4.0, "h": 2.0 } ]
}
```

- `x`/`z` = Weltposition, `r` = Pinselradius in Metern, `h` = Höhen-Änderung
  (negativ = Senke). Der `Terrain::compute_heights` addiert die Pinsel als
  weiche Kegel (cos-Falloff, Mitte volle Höhe, Rand 0) **nach** dem FBM-Rauschen
  — sie wirken auch im sonst abgeflachten Dorfkern und sind vollständig
  deterministisch reproduzierbar (F5, Saveload, Playtest).
- Default: Neue Karten sind flach (`amplitude` 0). `game/main.rb` und der
  Editor erzeugen ohne Angabe Amplitude 0 — Berge entstehen durch Malen.

**Editor:** Fünftes Werkzeug „Höhe malen" (ID 3029). Linksklick hebt um
+2 m pro Klick, Shift+Klick senkt um −2 m, Mehrfachklicken stapelt (klickt man
auf den frisch gehobenen Hang, wächst der Berg breiter; Klicks in dieselbe
Rasterzelle werden zu einem Eintrag zusammengeführt). `+/-` ändert die
Pinselgröße wie beim Boden-Malen.

**Kartengrenze:** `Physics::update` klemmt die Spielerposition auf
`[-width/2 + radius, width/2 - radius] × [-height/2 + radius, height/2 - radius]`
— die Karte kann nicht verlassen werden. Da das Terrain seit 18l beim
Kartenwechsel (`main.rb: enter_map → build_terrain`) neu aufgebaut wird, gelten
die Grenzen der jeweils geladenen Karte.

**Ruby:** `Rune::World.generate(width:, height:, resolution:, seed:,
amplitude:, height_brushes:)` — `height_brushes` = Liste `[x, z, r, h]`
(setzt `Rune::World.height_brushes=` vor dem Generieren). Die Engine baut die
Pinsel in `compute_heights` ein; `height_at`/Kollision/sichtbare Geometrie
stimmen automatisch.


## Verifikation: CI, portabler Build und Daten-Validierung (Etappe 19a)

Bis 18l stand die Prüfprozedur als Fließtext im Handoff: ein einzeiliger Befehl
mit 16 Testnamen, `build.ps1` mit hart kodiertem `C:\Ruby34-x64`, und kein
einziger automatischer Lauf. Etappe 19a macht daraus Werkzeug, das von selbst
läuft — und findet dabei einen echten Datenfehler.

### Vier Ebenen, vier Jobs

`.github/workflows/ci.yml` läuft bei jedem Push und Pull Request:

| Job | Runner | Was |
|---|---|---|
| `hygiene` | ubuntu | `tools/lint_repo.rb` |
| `engine` | ubuntu | `tools/test_all.rb` + `game/engine/mapcheck.rb` |
| `cpp-logik` | ubuntu | `tools/json_test.cpp` (`-Werror`) + `tools/json_bridge_check.rb` |
| `windows` | windows-2022 | `build.ps1 -Clean` und `-Test`, EXEs als Artefakt |

Getrennt statt in einem Job, damit ein Fehlschlag sofort sagt, WELCHE Ebene
kaputt ist. Seit 19x startet der Windows-Job die echten EXEs mit WARP aus
einer isolierten Paketkopie. Interaktive Bedienung und Hardware-GPU bleiben
trotzdem am echten Rechner zu prüfen.

### Portabler Build

`build.ps1` suchte Ruby bisher nur unter `C:\Ruby34-x64` und rechnete fest mit
`include/ruby-3.4.0` und `-lx64-ucrt-ruby340`. Jetzt:

- **Ruby**: `-RubyRoot` > `$env:RUNE_RUBY_ROOT` > `ruby.exe` im PATH (via
  `RbConfig::CONFIG['prefix']`) > `C:\Ruby34-x64`/`33`/`32`.
- **Toolchain**: `$env:RUNE_MSYS_ROOT\ucrt64` > `<Ruby>\msys64\ucrt64` >
  `C:\msys64\ucrt64` > ein `ucrt64`-g++ aus dem PATH.
- **Header, libdir, Importbibliothek, DLL-Name und Stdlib** kommen aus
  `RbConfig`; seit 19x bricht eine unvollständige Konfiguration ab, statt
  fremde 3.4-Pfade zu raten. Die Ruby-DLL und die Kernmodule sind Pflicht;
  bei anderen fehlenden Laufzeit-DLLs gibt der Build eine Warnung aus.
- `-Test` prüft C++-Logik, Windows-Dateipfade und Ruby-Suite mit JSON-/
  Event-Brücke; `-WarpTest` testet Renderer und eingebettetes Ruby isoliert.

Die Reihenfolge `-lucrtbase` VOR der Ruby-Importbibliothek bleibt unantastbar
(siehe `fclose`-Falle im Handoff).

### Test-Runner mit Voraussetzungen

`tools/test_all.rb` findet `tools/*_test.rb` per Glob, startet jeden Test als
eigenen Prozess (`RbConfig.ruby -I game/engine <datei>`) und wertet den
Exit-Code aus. `tex_import_test.rb` braucht `build/RuneEditor.exe` und wird ohne
sie als ÜBERSPRUNGEN gemeldet statt mit `raise` umzukippen — deshalb läuft die
Suite auf einem frischen Checkout und in CI auf Linux. `--strict` macht
Übersprungene zu Fehlern (Windows-Job), `-v` zeigt Ausgaben auch bei Grün,
`--only NAME` filtert.

### Karten-Validierung in Ruby: MapCheck

Der Editor prüft Karten seit 18h in C++ (`issues_scan_map`). Dieselben Regeln
gab es nirgends sonst — das Spiel hat kaputte Daten still geschluckt, und CI
konnte sie gar nicht sehen. `game/engine/mapcheck.rb` ist die
plattformunabhängige Fassung:

- **Befehle**: alle 20 Typen, rekursiv durch `then`/`else`/`branches`;
  Pflichtfelder pro Typ (`lines`, `options`+`branches`, `name`, `amount`,
  `slot` 1..3, `seconds` ≥ 0, `op` aus `== != < <= > >=`).
- **Events**: Seiten, Trigger, Bedingungen (`switch`/`variable`), doppelte Ids,
  fehlende Position.
- **Terrain**: `width`/`height`/`resolution`/`amplitude`, Höhen-Pinsel
  (`x,z,r,h`), Splat-Flächen (`tex,x,z,w,h`), Material gegen die geladene
  Materialbase.
- **NPCs**: Wegpunkte als `[x, z]`-Paare, `speed`, `radius`, eigene Befehle.
- **Atmosphäre**: Nebel/Panorama-Farbe in `0x000000..0xFFFFFF`, Wettertyp,
  Zielkarten von `teleport`.

Schweregrade wie im Editor: 1 = Warnung, 2 = Fehler. Aufrufe: `MapCheck.scan`,
`scan_file`, `scan_tree`, `report!` (→ `Issues.report`), `summary`; als CLI
`ruby -I game/engine game/engine/mapcheck.rb` (Exit 1 bei Fehlern).
`Game.enter_map` meldet seit 19a harte Fehler ins Laufzeit-Backlog
(`log/issues.json`) — Warnungen bleiben Editor und CI vorbehalten, damit das
Log nicht volläuft.

**Zwei Implementierungen, eine Regel:** `ISSUE_CMD_TYPES`/`issues_scan_map`
(C++) und `MapCheck::CMD_TYPES` (Ruby) müssen zusammenpassen. Beim Ergänzen
eines Befehls also IMMER beide ändern (und `EV_TYPES` + `EvType` im
Event-Editor). `tools/mapcheck_test.rb` prüft zusätzlich die mitgelieferten
Karten, wodurch eine Regeländerung, die vorhandene Daten bricht, sofort rot wird.

### JSON-Brücke und der %.12g-Fehler

`tools/json_test.cpp` testet Parser und Schreiber der Editor-JSON-Schicht
(88 Checks): Reihenfolge, doppelte Schlüssel, Escapes, Surrogatpaare,
Fehlermeldungen mit Position, echte Karten, Idempotenz. Der Modus
`--roundtrip ein.json aus.json` schreibt eine Datei genau so, wie der Editor sie
speichern würde. `tools/json_bridge_check.rb` vergleicht das Ergebnis mit Rubys
`JSON.pretty_generate`: erst semantisch (hart), dann bytegleich
(`--strict-format`).

**Dabei gefunden:** `write_number` nutzte `%.12g` und beschnitt still jede
Kommazahl auf 12 signifikante Stellen. Aus dem Startblickwinkel
`3.141592653589793` wurde beim Speichern im Editor `3.14159265359`, aus
Spielstandskoordinaten `2.0040341041749343e-05` wurden `2.00403410417e-05`.
Zusätzlich kippte `%g` schon kleine Werte in die Exponentenschreibweise — die
Kartenbreite `240.0` wäre als `2.4e+02` gespeichert worden, obwohl der
Kommentar im Code „wie Ruby JSON.pretty_generate" versprach.

Neu: kürzeste Präzision 1..17 mit `strtod`-Gegenprobe, Festkomma bei Exponent
`[-4, 16)` (Ruby/Python-Verhalten), `NaN`/`Unendlich` → `null` (bleibt lesbar
und wird von MapCheck gemeldet), Ganzzahlen außerhalb `long long` stürzen nicht
mehr ab. Alle 13 JSON-Dateien des Repos roundtrips jetzt bytegleich — CI prüft
das mit `--strict-format`.

### Repo-Hygiene

`tools/lint_repo.rb` prüft JSON-Parsebarkeit, UTF-8 ohne BOM, Doppelkodierung
(Mojibake-Muster, mit Whitelist für die Reparatur-/Scan-Werkzeuge),
Zeilenenden-Konvention, `boot.rb`-Requires in beide Richtungen, ignorierte
Dateien im Index, Binär-Artefakte, Grundausstattung und meldet übergroße
Quellen als Info. Ohne Git lauffähig.

**Zeilenenden:** `.gitattributes` stand auf `* -text` (keine Umwandlung),
wodurch 12 Dateien CRLF und der Rest LF hatten. Der Editor schreibt LF, Ruby
ebenso — jede CRLF-Karte kippte beim Speichern komplett um. Jetzt einheitlich
LF, CRLF nur für `*.ps1`/`*.psm1`/`*.manifest`; 20 Dateien wurden umgestellt
und die Regel wird von `lint_repo.rb` bewacht.

### Repo-Hygiene: README

Das Repo hatte keine Startseite. `README.md` nennt jetzt Zweck, den
Vergleich mit RPG Maker XP (was da ist, was fehlt), Voraussetzungen, Build-,
Test- und CI-Befehle, Projektstruktur, Bedienung und das Karten-Format.

### Ergebnis: was CI am ersten Tag gefunden hat

Der erste Lauf (`44498396607`) war rot in genau zwei Jobs — und beide Male war
der Fund echt:

| Job | Stand 1. Lauf | Ursache |
|---|---|---|
| `hygiene` | ✅ | — |
| `cpp-logik` | ✅ | JSON-Fix bestätigt: 88 Checks, 13 Dateien bytegleich |
| `engine` | ❌ | `mapcheck_test.rb`: 2 Assertionen suchten den Fundort im `message`-Feld statt in `Finding#to_s` (Test-Bug, Prüfregeln korrekt) |
| `windows` | ❌ | `build.ps1 -Test` übergab g++ Argumente mit angehängtem Komma (PowerShell-Argument-Modus) |

Der Windows-Job hat dabei schon im ersten Lauf den Hauptbeweis erbracht:
`Rune3D.exe` (1106,7 KB) und `RuneEditor.exe` (1584,7 KB) warnungsfrei mit
GCC 16.2 gebaut, Ruby 3.4.12 und Toolchain rein über `RbConfig` gefunden,
die `msys2/setup-msys2`-Rücksprungschritte korrekt übersprungen.

**Endstand (Lauf `44499944309`): alle vier Jobs grün.** Windows meldet
`17 OK, 0 fehlgeschlagen, 0 uebersprungen` — dort läuft `tex_import_test.rb`
dank `-Test` mit `--strict` gegen die wirklich gebaute EXE mit, und die
JSON-Brücke prüft 13 Dateien ohne Format-Abweichung. Artefakt `Rune3D-Windows`
(3,9 MB: beide EXEs, Ruby- und System-DLLs, Manifest).

### Fallstricke dieser Etappe

- **PowerShell-Argument-Modus:** `& $Gcc '-std=c++20', '-O1', ...` übergibt die
  Kommas MIT — g++ bekommt `-std=c++20,` und der Linker sucht eine Datei dieses
  Namens. Argumentlisten immer als Array bauen und mit `@` splatten
  (`$flags = @('-std=c++20', '-O1'); & $Gcc @flags`), wie es `Build-Target` mit
  `@CxxFlags`/`@Libs` schon immer tat.
- **setup-ruby bringt das DevKit mit:** Auf `windows-2022` liegt Ruby unter
  `C:\hostedtoolcache\windows\Ruby\3.4.x\x64` und MSYS2 darunter in
  `msys64\ucrt64` (GCC 16.2). `RI_DEVKIT` zeigt darauf. Der
  `msys2/setup-msys2`-Rücksprung im Workflow ist deshalb nur eine Versicherung
  und bleibt normalerweise übersprungen.
- **`%.12g` ist kein Format für Daten:** Wer Zahlen round-trippen will, braucht
  die kürzeste exakt runde Darstellung mit Gegenprobe (`strtod`) und eine
  Festkomma-Regel — sonst stimmt das Ergebnis nicht mit Ruby/Python überein.
- **CI-Annotationen:** `tools/test_all.rb` gibt bei Fehlschlag
  `::error title=…::…` aus, damit der Grund direkt in der Lauf-Übersicht steht —
  und zwar mit GETRENNTem Escaping: Titel/Eigenschaften brauchen `%` → `%25`,
  `:` → `%3A`, `,` → `%2C`, Zeilenumbruch → `%0A`; in der Nachricht selbst nur
  `%`, CR und LF. Wer die Nachricht mit `%2C` escaped, sieht genau das später
  lesbar in der Oberfläche stehen (`16 OK%2C 0 fehlgeschlagen`).
- **Zeilenenden sind Daten:** Solange `.gitattributes` auf `* -text` steht,
  entscheidet der Zufall (bzw. der letzte Editor) über CRLF/LF. `lint_repo.rb`
  bewacht die Konvention jetzt bei jedem Lauf.


## Echte Umlaute überall (Etappe 19b)

Bis 19b stand Deutsch im Code und in den Daten als Ersatzschreibung: `Hoehe`,
`fuer`, `loeschen` — und sichtbar für den Nutzer in Prefab-Liste („Huette",
„Wegstueck"), Objektliste, Launcher-Zeile und Dialogtexten („Drei Kristalle
traegt der Weg …"). Die UI-Strings des Editors hatten dagegen längst echte
Umlaute; deswegen wirkte das Programm uneinheitlich.

**Regel seit 19b:** Prosa (Kommentare, UI- und Spieltexte, Doku, Daten-Namen)
schreibt echte Umlaute; Identifier (Funktionen, Variablen, JSON-Keys, Prefab-
und Script-IDs, Pfade, API-Namen wie `JsonValue`/`issues_scan_map`) bleiben
ASCII. Die Umstellung lief maskiert: pro Datei wurden nur Kommentar- und
String-Bereiche (bzw. bei Doku/YAML der Fließtext) tokenweise ersetzt, Code
außerhalb davon blieb unangetastet — 113 Dateien, ~1550 Zeilen.

**Zweite Hälfte des Fundes:** Der Editor schickte datenhaltige Texte durch
ANSI-Win32-Aufrufe (`SendMessageA`, `SetWindowTextA`, `GetWindowTextA`). Mit
UTF-8-Namen wie „Hütte" wäre daraus in der Liste Doppelkodierung in Ã-Form geworden (CP1252
liest die UTF-8-Bytes als Einzelzeichen). Alle 24 Textaufrufe in
`editor_main.cpp`, `main.cpp` und `ruby/vm.cpp` nutzen jetzt die W-Varianten
mit `utf8_to_wide`/`wide_to_utf8`; `ev_set_text` war bereits W-basiert. Damit
ist der Textpfad durchgängig Unicode: JSON (UTF-8) → Wide → Win32/GDI.

**PowerShell:** `build.ps1` trägt jetzt eine UTF-8-BOM, damit Windows
PowerShell 5.1 die Umlaute in Meldungen korrekt liest (PS 7/CI stört die BOM
nicht); `tools/edclick.psm1` hatte sie bereits. `ß` bleibt, wo es war — die
Umstellung führt kein ß neu ein (schweizerische Schreibung wie „grösser"
neben bestehendem „Löschen" ist bewusst akzeptiert).

## Editor-Hauptfenster professionell (Etappe 19c)

User-Vorgabe nach Screenshot: Das Hauptfenster war an mehreren Stellen
unrund — Checkboxes für sich ausschließende Modi, ein Bedienblock ohne
Gruppierung, tote Leerräume unter der Prefab-Liste, und die Objektliste als
einanzeilige ListBox. Die anderen Dialoge des Editors (Events, Datenbank,
Materialbase) sind dagegen sauber gruppiert; das Hauptfenster zieht jetzt
nach, plus drei Dinge, die RPG Maker XP seinem Editor mitgibt: Kontextmenüs,
sortierbare Listen und eine Platzier-Vorschau.

**Linke Spalte, drei benannte Gruppen:** „Werkzeug (Modus)" mit fünf
Radios (Setzen, Auswählen, Verschieben, Boden malen, Höhe malen — genau einer
aktiv, `set_mode()` setzt Haken zentral, weil auch Prefab-Liste und
Kontextmenü den Modus wechseln), „Objekt-Typ (Prefab)" mit der Liste, die den
verbleibenden Raum füllt (kein toter Leerraum mehr), „Objekt-Aktionen"
(Drehen/Löschen/Duplizieren) und Undo/Redo am Spaltenende.

**Objektliste als ListView (Bericht-Modus):** Spalten `#`, `Objekt`, `X`, `Z`,
`Drehung`, `Material`; Klick auf den Spaltenkopf sortiert (auf-/absteigend),
ganze Zeile markierbar, Gitterlinien. Die Zeile trägt den Objektindex im
`lParam`, Sortierung und Datenreihenfolge bleiben also getrennt
(`g.obj_order`). Doppelklick richtet weiter die Kamera aus, Rechtsklick öffnet
das Kontextmenü.

**Platzier-Vorschau („Geist"):** Im Setzen-Modus folgt eine eingefärbte
Fassung des gewählten Prefabs (Mischung Richtung Mintgrün, ohne Textur — der
Renderer kennt kein Alpha) dem Cursor, eingerastet auf dasselbe Raster und
mit derselben Drehung, die der Klick setzt. `R` dreht die Vorschau in
15°-Schritten, das Statusfeld zeigt Rasterpunkt und Drehung. Aus der
„Sofort-Einfügen"-Falle wird so ein bestätigtes Platzieren.

**Kontextmenü** (Rechtsklick im Viewport und auf der Objektliste): Objekt
hier setzen, Kamera darauf, duplizieren, löschen — dieselben Befehle wie
Tastatur/Buttons, über `WM_COMMAND` verteilt.

**Introspektion für Live-Tests:** Texte sind prozessfremd nicht per
`SendMessage` lesbar (BACKLOG-Wissen). Der Editor schreibt deshalb bei jeder
Änderung der Objektliste `log/editor_objects.json` (Index, Typ, Name, X, Z,
Drehung, Material); `tools/edclick.psm1` liest die Liste daraus und liefert
weiter das alte Zeilenformat, bestehende Live-Skripte laufen also unverändert.

## Self-Switches und Bildschirmblende (Etappe 19d)

Erste Nachschub-Welle der RPG-Maker-XP-Features (User-Wunsch nach 19c):
Self-Switches und die Bildschirmblende — in XP die Kernwerkzeuge für
Event-Regie („Schrank öffnet sich nur einmal", „Szene dunkelt ab").

**Self-Switches (A-D pro Event):** `GameState` führt neben globalen Schaltern
eine zweite Tabelle mit Schlüsseln `"karte|event|buchstabe"`. Der Schlüssel
kommt vom Event (`Event#self_switch_key`, Karte = `Game.map_id`, Name statt
ID — wie die Savegames es für Events schon tun). Die Self-Switches wandern
in `snapshot`/`restore!` mit, Savegames bleiben also vollständig; alte
Savegames ohne das Feld laden mit leeren Self-Switches.

**Interpreter:** `self_switch` setzt den Buchstaben des Besitzer-Events
(`@owner`), `if_self_switch` verzweigt Dann/Sonst darauf. Ohne Besitzer
(Intro-Aufruf) wird gewarnt bzw. der Sonst-Zweig genommen — nie abgestürzt.
Ungültige Buchstaben (nicht A-D) lehnt `GameState` mit Warnung ab.

**Seiten-Bedingung:** Event-Seiten (18g) können jetzt zusätzlich zu
`switch`/`variable` ein `"self_switch": {"letter":"A","value":true}`
tragen; `Event.conditions_met?(c, ev)` bekommt dafür das Event als Kontext.
`refresh_page!` läuft weiter jeden Frame — Seitenwechsel durch Self-Switches
passieren damit automatisch (touch/auto feuern auf der neuen Seite erneut).

**Bildschirmblende:** `game/engine/fade.rb` (Modul `Fade`) interpoliert eine
Vollbild-Abdunklung (Alpha 0..1) mit Ziel und Rate; `Game.update` dreht sie
weiter, `Game.draw` legt sie über Spiel und UI, aber unter das Menü-Overlay
(damit ein abgedunkeltes Bild nie die Menü-Bedienung frisst). Kartenwechsel
rufen `Fade.reset!` — man landet nie mit schwarzem
Bildschirm in einer Karte. Der Interpreter-Befehl `fade`
(`direction` ein/aus, `seconds`) ist nicht blockierend, wie bei XP läuft das
Spiel während der Blende weiter.

**Editor:** Der Event-Dialog kennt die drei neuen Befehlstypen
(`EV_TYPES`/Labels/`ev_cmd_label`, Formular-Felder: Buchstabe im Namensfeld
+ an/aus-Combo bzw. Richtung + Sekunden). Der Seite-Kasten bekommt rechts
neben der Schalter-Bedingung zwei Combos: „Self-Switch" (keins/A-D) und
an/aus — Index 0 („keins") entfernt die Bedingung wieder.

**Sync-Pflicht bleibt:** `MapCheck::CMD_TYPES` (Ruby) = `ISSUE_CMD_TYPES` =
`EV_TYPES` (C++) — jetzt 23 Befehlstypen; `mapcheck_test.rb` zählt mit,
damit Auseinanderlaufen auffällt. MapCheck prüft `letter` A-D (Fehler),
fehlendes `value` (Warnung), Dann-/Sonst-Listen rekursiv und beim `fade`
Richtung/Dauer.

**Tests:** `tools/self_switch_test.rb` (GameState-Isolation, Savegame-Rundlauf,
Seitenwahl, Interpreter-Zweige, MapCheck) und `tools/fade_test.rb`
(Blendenverlauf, Minimaldauer, Reset, draw-Alpha, Interpreter, MapCheck),
beide kopflos wie die übrigen Engine-Tests.

## RGSS-Schicht und Ersteinrichtung (Etappe 19e)

User-Vorgabe: Die Engine soll später wie RPG Maker XP über RGSS (Ruby Game
Scripting System) in Ruby genutzt werden - Spieler schreiben ihr Custom-Game
im Script-Editor (F11) in Ruby. Zweite Vorgabe: Existiert kein Projekt,
muss der Spieler eines erstellen können (statt stiller Demo-Kopie).

**RGSS-Kompatibilitätsschicht (`game/engine/rgss.rb`):** legt die XP-Namen
über die Rune3D-Engine, initialisiert aus `Game.setup` (`RGSS.setup`, auch
bei F5/Playtest):

- `$game_system` — `menu_disabled`/`save_disabled` werden beachtet: Esc
  öffnet das Menü dann nicht, `Game.save_game` verweigert sich (Log-Warnung).
- `$game_switches`, `$game_variables` — delegieren auf `GameState`; RGSS
  nutzt Integer-IDs, Rune3D Namen - beides geht (`to_s`).
- `$game_self_switches` — versteht das XP-Schlüsselformat
  `[karte, event, letter]` (Event als ID über `Game.events` aufgelöst oder
  als Name) und den internen String `karte|event|letter`; delegiert auf die
  Self-Switch-Tabelle aus 19d.
- `$game_party` (gold/gain_gold/lose_gold/items/gain_item/lose_item/
  use_item), `$game_player` (x/y/z/moveto — 3D: moveto setzt auf den Boden),
  `$game_map` (map_id/display_name/events/npcs/event(id_oder_name)/width/
  height), `$game_screen` (weather mit RGSS-Zahlen 0-3 ODER Rune3D-Namen;
  Sturm=3 fällt auf dichten Schnee, weil es Sturm-Wetter noch nicht gibt).
- `Graphics` — `fadeout(n)`/`fadein(n)` rechnen Frames (frame_rate,
  RGSS-Default 40) in Sekunden der 19d-Bildschirmblende um;
  `brightness=`/`brightness` (0..255) setzen `Fade.alpha=` direkt;
  `freeze`/`transition` sind Zustandsmerker ohne Bild-Effekt, `update` zählt
  nur `frame_count` — den Frame-Takt gibt `Rune.tick` vor, nicht das Script.
- `Input` — RGSS1-Konstanten (DOWN=2 ... F9=29) und `trigger?/press?/
  repeat?/dir4` auf `Rune::Input.pressed?/down?`; C (Bestätigen) liegt auf
  Space/Enter/E, B (Abbrechen) auf Esc, A auf Shift; Symbol-Argumente
  (`Input.trigger?(:e)`) gehen auch. `repeat?` = `trigger?` (kein
  Auto-Repeat in der Engine), `update` ist No-Op.
- `Audio` — `bgm_play/bgs_play` (Schleifen), `me_play/se_play` (Einmal),
  `*_stop`; Lautstärke 0..100 wie RGSS, Pitch wird ignoriert, Pfad-/
  Endungs-Kürzung (`Audio/BGM/musik.ogg` -> `musik`).
- `load_data`/`save_data` — JSON statt Marshal (RxData-Format gibt es hier
  nicht); Rundlauf inkl. Umlauten getestet.

Bewusste Abweichungen stehen als Kopfdoku in rgss.rb und als Offener Eintrag
im BACKLOG (u. a. `$game_temp`, `$data_*`, Sprite/Window/Tilemap-Klassen,
`$scene`-Schleife). Wichtig für künftige Etappen: **neue Engine-Funktionen
sollen immer auch einen RGSS-/Script-Zugang bekommen** (Context-Helfer oder
$game_-Wrapper) - Ruby-Modding ist das Produktziel.

**Context-Helfer (scripts.rb):** `fade(direction, seconds)` und
`self_switch(letter, value = nil)` (lesen ohne, setzen mit Argument; nur mit
Besitzer-Event) ergänzen say/switch/variable/teleport/sound/effect/log.

**Titel ohne Projekt (AB, Runde 12):** `Scene_Title` erzeugt kein
Projekt mehr selbst (die Launcher-Zustände `:first`/`:name`/`:del` sind
entfallen; `Project.ensure_demo` läuft nur noch in Tests). Ohne Projekte
zeigt der Titel den Hinweis „Noch kein Projekt - im Editor Neues
Projekt anlegen" und blendet Neues Spiel/Fortsetzen aus; beenden geht
über „Spiel beenden". Die Kulisse ist weiter die Demo-Welt aus
game/data - sie braucht kein Projekt. Neuanlage ausschließlich im
Editor (`create_project`, Y3/Y4: Slug wie Ruby, Startkarte `welt01`,
Mini-Datenbank mit Start-Held).

**Game-Zugriffe für die Wrapper:** `Game.events`/`Game.npcs` (neu) liefern
die Events/NPCs der aktuellen Karte; `Game.map_id` kam mit 19d.

**Tests:** `tools/rgss_test.rb` (Globalen, alle Wrapper, Input-Mapping,
Graphics-Frame-Umrechnung, Audio-Kürzung, JSON-Rundlauf) und Abschnitt 6 in
`tools/script_test.rb` (fade/self_switch-Context-Helfer), beide kopflos.

## Bewegungsrouten (Etappe 19g)

Zweites großes XP-Feature nach den Self-Switches: Bewegungsrouten
("Bewegungsroute festlegen") für NPCs und Spieler.

**Mover (`game/engine/mover.rb`):** eine Route ist eine Schrittliste, die
ein Ziel abläuft - pro Sekunde `speed` Meter, ein Schritt ist 1 m. Schritte:
`up/down/left/right` (3D: -z/+z/-x/+x), `zufall`, `spieler_nah`,
`spieler_weg` (normiert zum Spieler), `warte` (0.5 s). Optionen: `repeat`
(Endlosschleife) und im Interpreter `wait` (Befehlsliste blockiert, bis die
Route fertig ist - mit repeat absichtlich NICHT, sonst Endlos-Falle; Warnung).

**Ziele:** `"spieler"` (gleitet per `Rune::Player.teleport` wie der
move_player-Befehl, inkl. Bodenhöhe) oder ein NPC-Name. NPCs brauchen
dafür `move_to(x,z)` (Event-Position + Node folgen - Reden/Benutzen bleibt
an der neuen Stelle möglich). `Npc#update` pausiert sein Wegpunkt-Laufen,
solange `Mover.moving?(self)` - die Route hat Vorrang, danach läuft die
Patrouille weiter. Eine neue Route ersetzt die alte desselben Ziels (wie XP
`force_move_route`).

**Unabhängigkeit:** `Game.update` dreht `Mover.update(dt)` neben
Effects/Fade - Routen laufen auch während Textfenstern/Wahlen weiter (XP-
Verhalten). Kartenwechsel (`enter_map`) rufen
`Mover.reset!`: Routen sind flüchtig, Ziel-NPCs sind nach dem Neuaufbau
andere Objekte (Savegames nehmen Routen nicht mit - wie bei XP).

**Event-Befehl:** `{"cmd":"move_route","target":"Wanderer","steps":["up",
"warte","spieler_nah"],"repeat":false,"wait":true,"speed":2.0}` - 24. Typ,
synchron in `CMD_TYPES`/`ISSUE_CMD_TYPES`/`EV_TYPES`; MapCheck prüft
target/steps-Pflicht, Schritt-Vokabular (eigene Konstante `MOVER_SCHRITTE`,
weil mapcheck.rb vor mover.rb lädt - der Sync wird in move_route_test.rb
gegen `Mover::SCHRITTE` assertet) und speed > 0.

**Editor:** Befehlstyp "[Bewegung] Bewegungsroute" im Event-Dialog, ganz
ohne neue Controls: Ziel im Namensfeld, Schritte als Zeilenliste im
Textfeld (wie bei text/choices), Tempo in Wert 1, "Warten bis fertig"
(1 = ja) in Wert 2, das an/aus-Combo = wiederholen (Default "an" wie im
XP-Routen-Dialog).

**Ruby-Zugang (RGSS-Leitlinie 19e):** `Mover.start('Wanderer', %w[up up],
repeat: true)` aus jedem Script plus Context-Helfer
`move_route(target, steps, repeat:, speed:)` für Event-Scripts.

**Grenzen (Backlog):** keine Kollisionsprüfung unterwegs (Passierbarkeit
fehlt insgesamt), keine Dreh-Schritte (turn_*) - Nodes rotieren sichtbar
nicht, sichtbare Events ohne NPC-Körper können nicht laufen.

**Tests:** `tools/move_route_test.rb` (Richtungen, Timing, warte/repeat/
stop/Ersetzung, Spieler-Route, Interpreter wait/ohne wait/repeat+wait,
MapCheck inkl. Sync-Assert), script_test.rb-Abschnitt 7 (Context-Helfer).

## RGSS-Vertiefung (Etappe 19h)

Die 19e-Schicht bekommt die fehlenden RGSS1-Bausteine: eigene Grafikobjekte,
Szenen und flüchtige Systemwerte - Scripts im Script-Editor (F11) können jetzt
eigene Fenster/Sprites bauen, wie im RPG Maker.

**Bitmap/Sprite/Window/Viewport (`game/engine/graphics.rb`, neu):** Rune3D
hat keine Pixel-Bitmaps; die Klassen legen die RGSS-Namen über die
UI-Primitive (`Rune::Gfx.rect/text` via `UI.panel/text`). `Bitmap` merkt
Größe/Farbe/Text (Dateien laden ist Backlog), `Sprite` zeichnet sein Bitmap
als Rechteck (x/y/z/opacity/visible/Zoom, Viewport-Vererbung), `Window` ist
ein Panel mit Titel, Zeilen und Cursor (`update` bewegt per `Input.dir4` und
liefert `:ok`/`:cancel` auf C/B), `Viewport` gruppiert (z/visible/opacity
gelten für Kinder). Alle Objekte registrieren sich in `RGSSGfx.objects`;
`Game.draw` ruft einmal `Graphics.draw`, das nach `z_effective` sortiert
zeichnet - über der Welt, unter dem Engine-UI und der Blende.

**$scene:** Scripts dürfen eigene Szenen setzen (`$scene = MeineScene`);
`Graphics.scene_update` (in `Game.update`) ruft pro Frame `Scene#main` -
der XP-Szenen-Name. Die Engine-Schleife wird dabei NICHT ersetzt (bewusste
Abweichung). Szenen-Fehler werden geloggt/gemeldet und werfen die Szene ab,
damit das Spiel weiterläuft.

**$game_temp:** `message_text/message_proc/common_event_id/player_passable/
transition_processing/...` - flüchtig pro `RGSS.reset!`, das jetzt
`Game.enter_map` ruft (Kartenwechsel/F5/Playtest: Sprites, Fenster und
`$scene` sind nicht speicherbar).

**$data_*:** `$data_actors/$data_items` als Hashes mit String-IDs aus
`database.json` (XP: Arrays mit Integer-Indizes), `$data_system` =
system.json; `$data_weapons/$data_armors` sind leer, bis die Datenbank
Waffen/Rüstungen kennt (Backlog, vorbereitet für 19i Kampf).

**passable? echt:** `$game_player.passable?(x, z, d = 2)` prüft Kartengrenzen
(`$game_map.width/height`) und Geländestufe (max. 1.2 m über `Game.gy`);
Richtungen wie XP (2=+z, 4=-x, 6=+x, 8=-z, 0/nil = Stelle selbst).
`$game_temp.player_passable = false` schaltet die Prüfung ab (XP-Debug).
Objekt-/Hindernis-Kollision fehlt weiter (keine blocked?-Abfrage, Backlog).

**Referenz (User-Upload `d90c4cd`):** `XP-Defaultscript/` = die 90 originalen
RGSS1-Default-Scripts (Game_Battler, Scene_Battle, Window_InputNumber, ...) -
Referenzmaterial für 19i (Zahleneingabe + Kampf) und spätere RGSS-Arbeit.
Liegt außerhalb der Quellverzeichnisse, kein Test/Lint lädt sie.

### RGSS-Ausrichtung an den Default-Scripts (Etappe 19h.2)

Seit dem User-Upload folgt die Schicht den Original-Dateien Feld für Feld:

- **$game_temp** = komplette Feldliste + Defaultwerte aus `000 - Game_Temp.rb`
  (choice_*, num_input_*, battle_*, shop_*, name_*, player_*, gameover,
  to_title, ...). Rune3D-Erweiterung: `passable` (schaltet die Geländeprüfung
  von `$game_player.passable?` ab). `gameover`/`to_title` werden von
  `Game.update` beachtet (`Game.to_title!`).
- **$game_system** nach `001 - Game_System.rb`: Timer in XP-Frames
  (`timer_start/timer_seconds/update(dt)` aus `Game.update`), Audio-Delegation
  (`bgm_play/se_play/...` mit Hash `{'name','volume','pitch'}` oder String),
  message_position/message_frame/save_count/windowskin_name/battle_bgm/
  battle_end_me/menu_bgm.
- **$game_screen** nach `005 - Game_Screen.rb`: tone/flash_color/shake/
  pictures als Zustände (Renderer fehlt noch, Backlog), `weather_type`
  liefert jetzt die XP-Zahl (0/1/2), weather_max.
- **$game_map.valid?(x, z)** wie XP; **$game_player** mit direction (2/4/6/8),
  steps/increase_steps (hängt an der Schritt-Sound-Logik in Game.update) und
  encounter_count/make_encounter_count (Grundlage für Zufallskämpfe, 19i).
- **Window_Selectable** nach `030 - Window_Selectable.rb`: index=/cursor_up/
  cursor_down/cursor_page_*/row_max/page_row_max/cursor_rect/help_window.
- **Mover spricht XP** (Routen-Codes wie RPG Maker XP, `ROUTE_*`):
  Schritte sind die Original-Namen (move_down .. move_backward, jump, wait,
  turn_down .. turn_away_from_player, switch_on/off, change_speed,
  move_animation_on/off, play_se, script, ...), XP-Befehls-Codes ('0' =
  Ende-Marker bis '45' = script, F1-Tabelle in `mover.rb`) und die
  deutschen 19g-Kurznamen (up/down/warte/...) werden als Aliase verstanden.
  Blickrichtung (2/4/6/8) wird mitgeführt: Bewegungsschritte drehen,
  turn_*-Schritte drehen nur - NPC per `Npc#turn_to` (Node-Yaw), Spieler per
  Kamera-Yaw. Schritte mit Parameter tragen den Wert im neuen Route-Feld
  `code` (switch_on/off: Schaltername, change_speed: m/s, play_se:
  SE-Name, script: Ruby-Code);
  Event-Befehl und Context-Helfer haben dafür `code`, MapCheck warnt bei
  switch_on/play_se/script ohne `code` (auch bei numerischen Codes).
  2D-only-Schritte (change_graphic/opacity/
  blending, stop_animation_*, direction_fix_*, through_*, always_on_top_*,
  change_freq) sind dokumentierte No-Ops mit Einmal-Warnung.
- **Editor:** Bewegungsroute bekommt Wert 3 = Code-Parameter (Laden/Bauen/
  Label), Schrittfeld bleibt freie Zeilenliste - XP-Namen, Codes und
  deutsche Kurznamen gehen gleichermaßen.

**Tests:** `tools/graphics_test.rb` (kopflos: Bitmap, Sprite-Sortierung/
Viewport/dispose, Window-Cursor/-Zeichnen, $scene, reset!, passable?) +
19h-Abschnitt in `rgss_test.rb` ($game_temp/$data_* gegen die echte
database.json).

## Zahleneingabe + Kampf-Grundgerüst (Etappe 19i)

Die dritte Feature-Gruppe aus der 19e-Roadmap, orientiert an den
Default-Scripts des User-Uploads (053 - Window_InputNumber.rb,
081..084 - Scene_Battle.rb, 007..012 - Game_Battler/Actor/Enemy).

**Zahleneingabe (XP "Input Number Processing"):** neuer Befehl
`{"cmd":"input_number","name":"ziffern","digits":3,"initial":0,
"cancel":true}` (25. Typ). Der Interpreter blockiert mit `@num_input`;
`UI.nummer_eingabe(state)` zeichnet das Ziffernfeld und liest die Tasten:
↑↓ ändert die Ziffer, ←→ wechselt die Stelle, 0-9 setzt direkt,
E/Enter bestätigt, Esc bricht ab (nur mit `cancel`, sonst blockiert es
weiter - wie XP "cancel not possible"). Bestätigen/Abbruch schreibt die
Variable (Abbruch = 0) und die Befehlsliste läuft weiter. digits clamp 1..6.

**Kampf (game/engine/battle.rb):** rundenbasiertes Grundgerüst Party gegen
Truppe. Daten: `database.json` bekommt `enemies` (id/name/hp/mp/atk/def/spi/
agi/exp/gold/drop/drop_rate/color) und `troops` (id/name/enemies[]) -
vier Gegner (Goblin/Wolf/Bandit/Kristallwache) und drei Truppen
(waldweg/banditen/brunnen) liegen bei. `Battle::Battler` ist die gemeinsame
Game_Battler-Entsprechung für Helden und Gegner. Seit 19v sind HP/MP der
Helden direkt an den jeweiligen Party::Actor gebunden (siehe oben).

Ablauf (Scene_Battle-Phasen, vereinfacht): `start(troop_id, can_escape:,
can_lose:)` -> Befehlsphase pro lebendem Helden (Angriff/Item/Flucht,
Cursor ↑↓ + E wie XP) -> Angriff geht in die Zielwahl (←→ auf lebende
Gegner) -> Aktionsphase: Heldenaktionen nach Agilität sortiert (Items über
`Party.use_item`), dann Gegnerzug (zufälliger lebender Held), dann
Sieg-/Niederlage-Prüfung. Schaden = `atk - def/2`, ±15 % Streuung, min 1,
Kritikchance `agi/64` (x3 Helden, +25 % Gegner). Sieg: Exp auf lebende
Helden, Gold an die Party, Drop-Würfe; Niederlage: `$game_temp.gameover`
(-> `Game.to_title!` aus Game.update, 19h.2), außer `can_lose`. Flucht:
50 %, ohne `can_escape` nicht möglich.

**Darstellung:** Gegner stehen als 3D-Kästen (Farbe aus `color`/Palette)
vor dem Spieler, HP-Balken + Namen als 2D-Overlay; Befehls-/Itemfenster,
Party- und Gegnerstatus, Kampflog (letzte 6 Zeilen) zeichnet `Battle.draw`
über dem abgedunkelten Bild. Während `Battle.active?` friert `Game.update`
die Welt einschließlich der nativen Spielerphysik ein (`Rune::Simulation.paused`);
nur Battle/Effects/Fade laufen, `Game.draw` zeigt das Kampf-Overlay. Nach
Kampfende läuft die Physik wieder. Kartenwechsel bricht Kämpfe ab (`Battle.abort!` aus
`enter_map`, neben `RGSS.reset!`).

**Event-Befehl:** `{"cmd":"battle","name":"waldweg","can_escape":true,
"can_lose":false}` (26. Typ) - der Interpreter blockiert bis zum Kampfende
(`@battle_wait`); schlägt `start` fehl (Truppe unbekannt), läuft die Liste
einfach weiter. Sync: CMD_TYPES/ISSUE_CMD_TYPES/EV_TYPES = 26,
mapcheck_test-Zählung nachgezogen.

**Editor:** "[Steuerung] Zahleneingabe" (Variable/Ziffern/Startwert,
an/aus = Abbruch erlaubt) und "[Kampf] Kampf starten" (Truppe im
Namensfeld, an/aus = Flucht möglich, Wert 2 = Niederlage ohne Game Over);
C++-Issues-Scan prüft `name`-Pflicht beider Typen.

**Ruby-Zugang (RGSS-Leitlinie):** `Battle.start('waldweg')` + Context-Helfer
`battle('waldweg', can_escape: false)`; `$game_temp.in_battle` wird
geführt, Zahleneingabe zusätzlich als RGSS-Zustand (`num_input_*` in
`$game_temp` vorhanden, der Interpreter nutzt seinen eigenen State).

**Grenzen (Backlog):** keine Skills/Magie (MP nicht für Skills verbraucht), keine Zustände
(Vergiftung etc.), keine Gegner-KI außer "Angriff auf zufälligen Helden",
keine Formationen/Positionen, Kampfhintergrund = abgedunkelte Karte.
Zufallskämpfe kamen später in Etappe 19l hinzu (siehe unten).

**Tests:** `tools/battle_test.rb` (kopflos, ~45 Checks: Datenbank, Start/
Absagen, Befehls-/Item-/Zielwahl-Cursor, Schaden, Sieg+Belohnung,
Niederlage+gameover+can_lose, Formel, Interpreter-Blockade battle/
input_number inkl. Abbruch und clamp) + script_test-Abschnitt 8
(battle-Helfer) + mapcheck_test (26 Typen, input_number/battle-Regeln).

## Professionalisierung: Sync-Test + README (Etappe 19j)

Die Sync-Pflicht für neue Befehlstypen (MapCheck::CMD_TYPES in Ruby =
ISSUE_CMD_TYPES + EV_TYPES in C++) war bisher Handarbeit mit einer
Zähl-Assertion. `tools/sync_test.rb` macht daraus eine maschinelle Prüfung:
Der Test parst `src/editor/editor_main.cpp` und seit 19o.5 auch
`src/editor/event_form.h` als Text (kein Compiler nötig) und vergleicht

- `EV_TYPES` (Reihenfolge) und `ISSUE_CMD_TYPES` (Menge) mit
  `MapCheck::CMD_TYPES`, `EV_TYPE_LABELS`-Anzahl und die `enum EvType`-
  Einträge (Name + Reihenfolge) mit denselben Listen,
- die `when`-Zeilen des Interpreters (6er-Einzug = Befehls-case) mit
  `CMD_TYPES` — ein Befehl ohne Interpreter-Implementierung fällt auf,
- `EV_TRIGGERS`/`EV_OPS` mit `MapCheck::TRIGGER`/`VERGLEICH` und den
  entsprechenden Modell-Arrays, die Wetter-
  und Self-Switch-Combos (Inline-Listen) mit `WETTER`/
  `SELF_SWITCH_BUCHSTABEN`, die Effekt-Beschriftung mit `EFFEKTE`,
- die Dialogfelder `KEYS`/`ACTOR_LABELS`/`ITEM_LABELS` des Datenbank-
  Dialogs mit den echten Schlüsseln in `game/data/database.json` sowie
  die Gegner-/Truppen-Referenzen (19i).

Damit ist der offene Backlog-Eintrag „zwei Implementationen" nicht
aufgelöst (eine gemeinsame JSON-Quelle bleibt das Ziel), aber jede
vergessene Seite kippt jetzt einen Test statt erst den Editor-Alltag.
Zweite Maßnahme: README-Vergleichstabelle und Feature-Beschreibung auf
den Stand 19d-19i gebracht (Kampf-Grundgerüst, Bewegungsrouten, Self-
Switches, Zahleneingabe, Fade, RGSS-Schicht, 26 Befehle).

## Datenbank-Dialog: Gegner und Truppen (Etappe 19k.1)

Erster Teil der Editor-Lücken-Runde (User-Auswahl nach 19j). Der
Datenbank-Dialog (F9) konnte seit 17g nur Helden und Items bearbeiten -
die 19i-Daten (`enemies`, `troops`) waren reine JSON-Handarbeit. Jetzt:

- **Vier Reiter**: Helden / Items / Gegner / Truppen; ein Listenfeld links
  (je Reiter eigene Anzeige: Klasse, Art, HP, Gegner-Anzahl), Formular
  rechts.
- **Feldgesteuertes Formular**: statt fester 9 Zeilen mit Tab-Spezifika im
  Code gibt es `DbField`-Tabellen (`DB_FIELDS_ACTOR/ITEM/ENEMY/TROOP`) aus
  JSON-Schlüssel, Beschriftung, Zahlen-Flag und Präzision; `db_fields(tab)`
  wählt sie, Layout/Füllen/Zurückschreiben sind generisch
  (`DB_MAX_ROWS` = 13 Zeilen, Gegner brauchen 12: id/name/hp/mp/atk/def/
  spi/agi/exp/gold/drop/drop_rate). Unbekannte JSON-Schlüssel (color, desc,
  level, exp bei Helden) bleiben beim Speichern erhalten - der Dialog
  schreibt nur seine Tabellenfelder.
- **Truppen-Gegnerliste**: das Array `enemies` wird komma-getrennt im Edit
  angezeigt und beim Ändern getrimmt zurückverwandelt. Seit 19o.3 erledigen
  dies `database_ids_text`/`database_ids_parse`; nicht eindeutig darstellbare
  Altdaten bleiben erhalten bzw. werden nur nach Bestätigung ersetzt.
- **Neu-Anlegen** mit sinnvollen Defaults je Reiter (gegner_N/trupp_N mit
  Basiswerten), Fenster auf 716x768 gewachsen, Menü-/Hilfetexte nachgezogen.

`tools/sync_test.rb` prüft die vier Feldtabellen gegen die echte
`game/data/database.json`: jeder editierbare Schlüssel muss in den
Einträgen vorkommen (erste Spalte immer `id`) - editiert der Dialog ins
Leere, kippt der Test.

## Block-Zonen / Passierbarkeit (Etappe 19k.2)

Zweiter Teil der Editor-Lücken-Runde: der offene Backlog-Punkt „Kein
Passierbarkeits-System". Karten bekommen einen optionalen Schlüssel
`"blocked": [{"x","z","w","h"}]` - Rechtecke in Weltmetern (x/z = Mitte,
w/h = Größe), die als **unsichtbare Wände über alle Höhen** wirken (kein
Überspringen, kein Durchlaufen).

- **Physik (C++)**: `Physics::add_blocked/is_blocked/blocked_overlaps`
  (`src/world/physics.*`); `overlaps_any` prüft die Zonen zusätzlich zu den
  `solid`-AABBs, damit die achsenweise Kollisionsauflösung und `push_out`
  (NPC-Schub) sie automatisch respektieren. Bindings: `Rune::Physics.
  clear_blocked/add_blocked(x,z,w,h)/blocked_count/blocked?(x,z)`.
- **Spiel**: `Game.enter_map` lädt die Zonen der Karte in die Physik
  (respond_to?-geschützt) und loggt die Anzahl.
- **RGSS**: `$game_map.blocked_zones`/`blocked?(x,z)` lesen dieselben
  Rechtecke aus `Game.map`; `$game_player.passable?` liefert `false` für
  Ziele in einer Zone (Grenzen + Geländestufe + Zonen).
- **Editor**: F2-Karten-Eigenschaften hat ein eigenes Feld „Block-Zonen
  (x,z,w,h pro Zeile)" (wirktungslose Zeilen werden überlesen, leere Liste
  entfernt den Schlüssel); im Viewport zeichnet `push_blocked_zones()` jede
  Zone als roten Flach-Quader auf Bodenhöhe. Der C++-Issues-Scan prüft
  dieselben Regeln wie MapCheck (x/z/w/h Zahlen = Fehler, w/h <= 0 =
  Warnung, keine Liste/Objekt = Fehler).
- **MapCheck**: `pruefe_blocked` mit denselben Regeln; Tests in
  `mapcheck_test.rb` (5 Checks) und `rgss_test.rb` (blocked?/passable?,
  8 Checks).

Grenzen: Zonen sind Rechtecke ohne Rotation/Höhenbereich. Die beiden
anderen offenen Punkte (NPCs/Routen liefen durch Zonen, `passable?` kannte
keine Objekt-Kollision) sind seit Etappe 19k.3 erledigt - siehe unten.

## Passierbarkeit rund gemacht (Etappe 19k.3)

19k.2 blockte nur den Spieler physikalisch. Seit 19k.3 gibt es **eine**
Abfrage für alles, was läuft: `game/engine/passage.rb` (Modul `Passage`).

- **`Passage.zonen` / `zone?(x, z)`**: die Block-Zonen der aktuellen Karte
  (`Game.map['blocked']`), Punkttest inklusive Rand - dieselbe Rechnung wie
  `Physics::is_blocked`. Kaputte Einträge (keine Hashes) werden überlesen.
- **`Passage.hindernis?(x, z, gy = nil)`**: Objekt-Kollision über die neue
  Physik-Abfrage `Physics::solid_at(x,y,z)` (Binding
  `Rune::Physics.solid_at?(x,y,z)`). Getastet wird über der Steiggrenze der
  Physik (`STUFE = 0.35` = `kMaxStep`): `TASTPUNKTE = [0.45, 1.2]` - ein
  flacher Kasten bleibt damit eine begehbare Stufe, eine Wand oder ein
  schwebender Kasten blockt. Ohne Engine (kopflose Tests) liefert die
  Abfrage „frei", dann zählen nur die Zonen.
- **`Passage.blockiert?(x, z, gy = nil)`** = `zone? || hindernis?`.

Anschlüsse:
- **RGSS**: `$game_player.passable?` prüft `Passage.blockiert?` (Grenzen +
  Geländestufe + Zonen + Hindernisse); `$game_map.blocked_zones`/`blocked?`
  delegieren an Passage (API von 19k.2 bleibt), neu `$game_map.hindernis?`.
- **NPCs** (`npc.rb`): ein Schritt in Zone/Hindernis lässt den NPC anstossen
  (warten) statt durchlaufen; liegt schon der **Wegpunkt** in einer Zone,
  wird er übersprungen, damit die Patrouille nicht dauerhaft an der Wand
  steht.
- **Bewegungsrouten** (`mover.rb`): `bewege` rechnet den Teilschritt über
  `teil_ziel` vor und bricht den Schritt ab (`distanz = 0`) plus
  `warn_once`, wenn das Ziel blockiert ist - gilt für NPC-`move_to` **und**
  den Spieler-`teleport` (der umging die Physik komplett).
- **Spiel** (`main.rb`): Warnung, wenn der Startpunkt in einer Block-Zone
  liegt (Spieler würde feststecken).
- **MapCheck**: Lage-Prüfungen der geprüften Karte (`@block_zonen` aus
  `map['blocked']`, nicht aus der geladenen Karte!) - Warnungen für
  Startpunkt, Events, NPC-Position und NPC-Wegpunkte in einer Zone; nur
  brauchbare Zonen (Zahlen, w/h > 0) zählen.
- **Editor (C++)**: `blocked_zone_at(x,z)` + dieselben vier Lage-Meldungen
  im `issues_scan_map` (Parität zu MapCheck).

Tests: neu `tools/passage_test.rb` (Zonen, Tastpunkte, `blockiert?`),
dazu Checks in `rgss_test.rb` (Hindernisse in `passable?`), `npc_test.rb`
(Anstossen + Wegpunkt-Skip), `move_route_test.rb` (NPC- und Spieler-Route
stoppen, Route hängt nicht) und `mapcheck_test.rb` (vier Lage-Warnungen).

Grenzen (Backlog): NPCs weichen nicht aus (anstossen/überspringen statt
Pfadfindung um die Wand herum), Bewegungs-Routen kennen kein „Wand
entlang rutschen", `hindernis?` tastet zwei feste Höhen (keine exakte
Körperbox).

## Zufallskämpfe beim Laufen (Etappe 19l)

Eine Lücke des Kampf-Grundgerüsts (19i): Kämpfe gab es nur per
Event-Befehl. Seit 19l lösen Karten selbst Kämpfe aus - wie XP
(`016 - Game_Map.rb`, `022 - Game_Player.rb`, `071 - Scene_Map.rb`).

- **Karten-JSON**: optionaler Schlüssel `"encounters"`:
  `{ "troops": ["waldweg", "banditen"], "steps": 30 }` - `troops` sind
  Truppen-IDs aus der `database.json` desselben Projekts (Zufallswahl pro
  Kampf); `steps` ist die positive ganze mittlere Schrittzahl (Default 30).
  Ohne Truppenliste gibt es keine Zufallskämpfe.
- **RGSS**: `$game_map.encounter_list` überspringt ungültige oder leere
  IDs; `encounter_step` fällt bei ungültigem Wert auf 30 zurück.
  `$game_player.make_encounter_count` würfelt `rand(n) + rand(n) + 1`
  (im Mittel n Schritte), `decrease_encounter_count` zählt bis 0 herunter.
- **Spiel** (`main.rb`): Boden-Schritt-Events (wie der Schritt-Sound:
  0,60 m beim Gehen, 0,85 m beim Rennen) zählen herunter. Bei 0, nicht
  leerer Liste, keinem laufenden Event und nicht gesperrten Begegnungen
  startet `battle_start` (Flucht erlaubt, Niederlage = Game Over) und
  würfelt neu. Ein gleichzeitiges Touch-/Auto-Event hat Vorrang, und im
  Editor laufen weder Countdown noch Zufallskampf. Ab dem Start-Frame
  steht die Welt. Ein fehlgeschlagener Start lässt den Zähler auf 0; der
  nächste freie Schritt versucht es erneut. Beim Kaltstart/F5 wird nach
  `RGSS.setup` gewürfelt, beim Kartenwechsel in `enter_map`. Teleports über
  Karte/Laden, Event (`teleport`, `move_player`), RGSS (`moveto`) und den
  Script-Helfer `teleport` zählen nicht als Schritte. Eigenes direktes
  `Rune::Player.teleport` sollte anschließend `Game.reset_step_tracking`
  aufrufen; schrittweise Bewegungsrouten bleiben dagegen zählbar. Beim
  Laden wird neu gewürfelt; der Zähler ist nicht Teil des Spielstands.
- **MapCheck + Editor**: `pruefe_encounters` bzw. `issue_scan_encounters`
  prüfen Objekt, Liste aus nicht-leeren Text-IDs und positive ganze
  `steps` (null/fehlend ist optional). F2 warnt beim Bearbeiten der Liste
  vor unbekannten Truppen; den tatsächlichen Kampfstart samt Gegnern und
  Party prüft `Battle.start` gegen die Projekt-Datenbank.
- **Demo**: Die Vorlage `game/data/maps/dorf.json` verweist auf Truppen
  aus `game/data/database.json`. Bestehende Projekte, darunter das
  mitgelieferte `projekte/dorf`, werden nicht automatisch geändert und
  brauchen eigene Truppen/Gegner, bevor dort Kämpfe konfiguriert werden.

Tests: `encounter_test.rb` führt den echten Spielstart und Schritt-Frames
mit `Battle`, Interpreter und Demo-Datenbank aus; `rgss_test.rb` und
`mapcheck_test.rb` prüfen die Datenregeln. `json_test.cpp` testet dieselbe
C++-Editorprüfung direkt und kopflos.

Grenzen (Backlog): keine Kampf-Hintergrundmusik/-bilder und keine
Gebiets-/Schritt-abhängigen Encounter-Tabellen wie XP.

## Zufallskämpfe im Karten-Editor (Etappe 19m)

**F2 → Zufallskämpfe** bearbeitet Truppen und mittlere Schrittzahl für die
aktuelle Karte. Die Auswahl liest gespeicherte Truppen aus der eigenen
`database.json`; F9 ist aus dem Dialog erreichbar und „Neu laden“ aktualisiert
die Auswahl nach dem Speichern. „Hinzufügen“ fügt eine ID an die editierbare
Zeilenliste an. Identische Zeilen bleiben erhalten und gewichten die zufällige
Auswahl; eine leere Liste deaktiviert Begegnungen. Unbekannte IDs oder eine
nicht lesbare Datenbank führen bei Listenänderung zu einer Rückfrage. Diese
Prüfung bestätigt keine vollständige Kampffähigkeit (Gegner/Party).

Schritte müssen im Formular leer (= 30) oder ganzzahlig zwischen 1 und
9007199254740991 sein. Dieser Grenzwert verhindert Rundungsverlust bei neu
eingegebenen Ganzzahlen in der double-basierten C++-JSON-Schicht; zur Grenze
vorhandener großer JSON-Zahlen siehe Backlog. Fehler oder abgelehnte
Rückfragen lassen den Dialog offen und die gesamte Karte unverändert.
„OK“ übernimmt in den Speicher, Strg+S schreibt anschließend die Karte.

`encounter_form.h` liest/bereitet Begegnungen ohne Win32 vor: Unbearbeitete
fehlende/null-Felder, ganzzahlige Floats, Duplikate und unbekannte Schlüssel
bleiben erhalten. Nicht darstellbare oder fehlerhafte Altwerte werden erst
nach ausdrücklicher Bestätigung ersetzt. `map_props_form.h` übernimmt auch
allgemeine F2-Felder nur bei tatsächlicher Änderung, erhält unberührte
gerundete Anzeigen, Block-Zonen und unbekannte Eigenschaften. Nach
Änderungen an der Kartenwurzel wird `g.objects` neu gebunden; der Kartenbaum
und der Projektwechsel bleiben während des moduslosen Dialogs gesperrt.
F9 muss vor einem Projektwechsel geschlossen werden, damit seine geladene
Datenbank nicht unter dem Pfad eines anderen Projekts gespeichert wird.

Tests: `tools/json_test.cpp` prüft das Dialog-Datenmodell, optionale und
fehlerhafte Altwerte, lange Listen, Duplikate, Werte-Grenzen, die echte
Demo-Datenbank, JSON-Roundtrips und unberührte allgemeine Eigenschaften.
CI baut/linkt den Win32-Editor und kann WARP in einem unsichtbaren Fenster
prüfen, ersetzt aber keinen interaktiven GUI-/Hardware-GPU-Test (siehe Backlog
und Handoff).

## Kampfzustand und gezielte Items (Etappe 19v)

`Battle::Battler` greift bei Party-Helden direkt auf denselben `Party::Actor`
zu: aktuelle HP/MP und echte Maximalwerte sind keine voneinander abweichenden
Kopien. 0 HP bleibt 0; ohne lebenden Helden endet der Kampf als Niederlage.
Ein bereits laufender Kampf wird nicht durch einen zweiten Start ersetzt.
Die Befehlsreihenfolge verwendet feste Actor-Positionen statt Indizes in
einer veränderlichen Liste lebender Helden. Wird ein auswählender Held
kampfunfähig, wird seine offene Auswahl verworfen; KO streicht auch seine
vorgemerkten Aktionen und Itemreservierungen.

Die Itemliste reserviert eine verfügbare Menge pro bestätigter Aktion, ohne
das Inventar sofort zu ändern. In `:item_target` wird ein bestimmter Held
einschließlich KO gewählt; ein volles/ungültiges Ziel wird abgewiesen.
Bei der Ausführung prüft `Party.use_item(..., target:)` Bestand, positive
Stärke und Wirkung erneut und verbraucht nur bei Erfolg. Führt eine
schnellere Aktion zum Sieg, werden spätere Aktionen nicht mehr ausgeführt.
Das gewählte Angriffsziel fällt bei Bedarf auf den ersten lebenden Gegner
zurück. Ausführungsreihenfolge bleibt Agilität, Gegner ziehen anschließend.

`Party.use_item(id, target: actor_oder_id)` sowie `use_item(id, ziel)` bzw.
`use_item(id, target: ziel)` im Script-Context und `$game_party` unterstützen
gezielte Anwendung. Ohne Ziel bleibt der historische Auto-Fallback auf den
ersten verletzten Helden bzw. den Leader erhalten; bei einer vollen Party
kann ein alter Aufruf also weiterhin ohne Wirkung verbrauchen. Mana nimmt
entsprechend den ersten Helden mit fehlenden MP oder den Leader. Ein
Heiltrank kann wie früher bei 0 HP wiederbeleben; Skills, Zustände und eine
XP-vollständige Item-/Gegnerlogik sind weiterhin nicht implementiert.

`Game.update` leitet Esc im Kampf an `Battle` statt an das Weltmenü. Ein
nicht erlaubter Fluchtversuch öffnet kein Menü; ein fehlgeschlagener löscht
Aktionen und Reservierungen. Der native Hauptloop gibt bei Esc zusätzlich
immer den Mausfang frei, auch während eines Kampfes; die mittlere Maustaste
schaltet den Mausblick wieder ein. Nach Game Over wird der Titelwechsel vor den
Folgekommandos des wartenden Event-Interpreters verarbeitet; `can_lose`
setzt die Event-Liste dagegen fort. Kampfende und `abort!` geben auch bei
direktem Ruby-Aufruf die Simulation frei. Kartenwechsel, Titelrücksprung,
F5 und Playtest-Fehler brechen aktive Kämpfe ab; der Fehlerbildschirm
pausiert die Simulation anschließend bewusst wieder. F5 setzt die Party
wie bisher neu auf und ist kein Speichern.

`battle_state_test.rb` prüft echte Battle-/Party-Logik mit nativen Attrappen,
`encounter_test.rb` die echte Spielschleife einschließlich Esc, Event-Folgen,
Abbruch und JSON-Rundlauf über eine `Project`-Speicherattrappe. Seine
Inputattrappe bildet Tastendruckflanken, Halten/Loslassen und die native
Esc-Mausfreigabe nach; der Battle-Unit-Test speist Flanken direkt ein.
`runtime_boot_test.rb` startet den echten Bootstrap in einer temporären
Projektkopie und prüft dort unter anderem Action-Event, Projekt-Script,
Kampf-Item und Speichern/Laden über eine echte `Project`-Slot-Datei sowie
den F5-Fehler. Fenster, Eingabe und Rendering bleiben auch dabei Attrappen.
`party_test.rb` und `rgss_test.rb` prüfen die Ruby-Zugänge. Der zentrale
Glob-Runner nimmt die neue Suite automatisch in CI und `build.ps1 -Test` auf.
Kein interaktiver Windows-/GPU-Nachweis; siehe `docs/WINDOWS_BATTLE_LIVE_TEST.md`.

## F4 mit ausgewählter Karte (Etappe 19u)

RuneEditor leitet die Karten-ID aus dem gespeicherten Dateinamen ab und
startet `Rune3D.exe --project <name> --map <id>` mit `CreateProcessW`,
zitierten Einzelkomponenten und der Engine-Wurzel als Arbeitsverzeichnis.
Das Projekt-/Karten-Epoch-Paar und der Dateiname werden nach etwaigen
Speicherrückfragen erneut geprüft. Ohne ausgewählte Karte entfällt `--map`.
IDs sind nicht leer, passen samt `.json` in eine gültige Windows-Komponente
und enthalten keine Pfade oder Gerätenamen. Optionen dürfen auch als echte
Namen vorkommen; doppelte, fehlende und unbekannte Optionen werden abgewiesen.

`main.cpp` setzt die Wide-Umgebung `RUNE_PROJECT` und `RUNE_PLAYTEST_MAP`;
ohne `--map` wird ein geerbter alter Kartenwert gelöscht. Ruby verbraucht
die Kartenoption beim direkten Projektstart und hält sie nur für dieses
Projekt. `Database` und `MapCheck` erkennen `.json`/`.JSON` samt gemischter
Schreibung und ignorieren Verzeichnisse. Für den Playtest verlangt `Database`
eine eindeutige ID und ein lesbares JSON-Objekt.
Fehlende/mehrdeutige Karten und unbekannte Projekte mit Karte zeigen einen
Fehlerbildschirm statt einer anderen Karte. F5 liest die Karte erneut und
setzt den Spielzustand zurück; auch bei einem zunächst fehlenden Projekt
bleibt die Auswahl für den erneuten Versuch erhalten. Titel/Projektwahl
löscht die Vorgabe.
Terrain, Startpunkt, Events, Begegnungen und `Game.change_map` laufen über
die normalen Enginepfade. Ohne explizite Karte bleibt der Start wie bisher.

Der Kindprozess ist keine Sandbox und kein Dateischnappschuss. Ruby liest
die Karte nach dem Editor-Speichern separat; externe Änderungen oder
Dateilinks sind damit nicht allgemein ausgeschlossen. Normale Spielstände,
Scripts und `last_played` bleiben aktiv. Die Windows-Pfadtests nutzen einen
echten Test-Kindprozess, die Ruby-Boot-Tests native Attrappen; sie sind kein
sichtbarer F4-GUI-/Hardwaretest. [CI 45183821663](https://github.com/Kenk-JADev/Game_Engine-V2/actions/runs/45183821663)
für `bc237b3` ist grün. Die Zwei-Karten-Live-Prüfung nach
`docs/WINDOWS_MATERIAL_LIVE_TEST.md` bleibt offen.

## Rune3D-Laufzeit ohne Ingame-Editor (Etappe 19t)

`boot.rb` lädt `map_objects.rb` statt des entfernten `editor.rb`.
`Game.enter_map` und `Scene_Title#bau_kulisse` erzeugen je eine eigene
`MapObjects`-Instanz, die mit `Prefabs.instantiate`, Geländehöhe und
`Scene.refresh_solids` die Spiel- bzw. Kulissenszene aufbaut. Die alten
`ed_*`-Knotennamen bleiben für Scripts bestehen; die Zuordnung und das
Entfernen früherer Knoten erfolgen über Identität, sodass namensgleiche
Script-Nodes erhalten bleiben. Picking zeigt weiter Prefab-Namen an.

F4 hat in Rune3D keine Editorfunktion mehr. `Game.change_map`, Terrain,
Events, NPCs, Laufzeit-Picking, Kampfpausen und F5 bleiben erhalten;
beim Encounter hat ein laufendes Event Vorrang. F4 im eigenständigen
RuneEditor speichert die Karte und startet das Spiel als eigenen Prozess.
Seit 19u übergibt er die ausgewählte Karten-ID. Der Prozess benutzt
reguläre Projekt-Spielstände und ist keine Sandbox.

`map_objects_test.rb` prüft echte Ruby-Knoten/Szenen und eine Physikattrappe,
`runtime_boot_test.rb` lädt echten Bootstrap, Launcher und Spiel in einer
temporären Kopie mit nativen Attrappen, `encounter_test.rb` prüft F4/F5,
Kartenwechsel, Picking und Kampfvorrang. Kein GUI-/Hardware-GPU-Test.

## Windows-Karten-/F9-Dateitransaktionen (Etappe 19s)

`document_store_win.h` verwendet den vertrauten Engine-Anker und öffnet
Projekt-, Daten- und Kartenordner einzeln relativ zu gehaltenen Handles;
Dateinamen sind Einzelkomponenten, Reparsepunkte und Hardlinks verboten.
Karten laden/anlegen/speichern/löschen und F9 laden/speichern verwenden
diesen Pfad. Andere Projektdateien sind dadurch nicht automatisch geschützt.

Die Beobachtung liest die vollständigen Bytes bei gesperrten Schreib-/
Löschzugriffen und hält zusätzlich ein Attribut-Handle mit Datei-ID während
einer Rückfrage. Nach einem Ja wird das Ziel erneut ohne Schreib-/Delete-
Sharing geöffnet und Identität plus exakte Bytes werden erneut verglichen;
auf Abweichung, fehlende Unterstützung oder Fremdsperre folgt Abbruch. Bei
fehlender Datei schützt No-Replace gegen spätere Neuanleger. Die vorbereitete,
geflushte Datei wird handle-relativ veröffentlicht; das gesperrte Original
wird zuvor in einen eindeutigen Backupnamen verschoben. Kartenlöschung
markiert ausschließlich das bestätigte Handle zum Löschen.

Es gibt keinen atomaren Namensaustausch: Zwischen Backup und Promotion kann
der alte Name fehlen. Eine Konkurrenzdatei wird nicht ersetzt; falls die
Rücksetzung blockiert ist, wird der `.rune-*.bak`-Pfad angezeigt. Fehlende
Temp-Bereinigung wird ebenfalls gemeldet. Ein Prozess-/Systemabsturz kann
Sicherungs- oder Tempdateien hinterlassen; manuell vergleichen und sichern.
Netzlaufwerke, andere Dateisysteme, bestehende beschreibbare Speicher-
abbildungen, Metadaten/ACL-Erhalt und Stromausfall sind nicht allgemein
geprüft; FileIdInfo und Rename müssen vom Dateisystem unterstützt werden.

`tools/document_store_test.cpp` prüft denselben Transaktionsablauf mit
simulierten Bestätigungen und realen Windows-I/O-Renn-Hooks, einschließlich
Unicode, fremdem CWD, Fremdhandles, Links, Identitätswechsel mit gleichen
Bytes, Neuanlage, Backupkollision und blockierter Rücksetzung. Windows-CI
`6a3b8e1`, Lauf 45175342986: 58/0, alle vier Jobs grün. Kein interaktiver
GUI-/DPI-/Hardwaretest; Testvorschlag im Windows-Live-Dokument.

## Windows-Projekt- und Kartenpfade (Etappe 19r)

Der ursprünglich im Backlog vorgeschlagene reine UTF-16-Durchstich wurde
bewusst nicht gewählt: C++-Datenmodelle, JSON und Ruby führen Projekt- und
Installationspfade als **gültiges UTF-8**. `core/win_path.h` konvertiert
an Windows-Grenzen strikt nach/von UTF-16 (`MB_ERR_INVALID_CHARS`,
`WC_ERR_INVALID_CHARS`), ohne ACP-Ersatzzeichen. Fehlerhafte Pfade werden
abgewiesen, ungültige UTF-16-Verzeichniseinträge beim Scan übersprungen.
Die gemeinsame Wurzelsuche verwendet Wide-APIs: der Editor bevorzugt die
EXE-Installation, das Spiel ein CWD mit `game/`. Beide speichern die
normalisierte Wurzel intern in UTF-8.

Der Editor verwendet dieselbe Kodierung für Projekt-/Kartenscan, Lesen,
Speichern/Löschen, F9, den Projektmerker und den gebundenen Materialstore.
Die Projektlistbox holt Texte mit dynamischer Länge; neue temporäre Dateien
für Karten/F9 haben kurze Namen im Zielverzeichnis. F4 validiert den
Projektnamen, sucht `build/Rune3D.exe` oder `Rune3D.exe` neben `game/` und
startet `CreateProcessW` mit zitiertem Argument und expliziter Engine-CWD.
Die echte Wide-Kommandozeile und `RUNE_PROJECT` bleiben unverändert; ein
alter Projektmerker mit ungültigem UTF-8 wird nicht per ACP geraten. Neue
Projektordner im Editor bleiben auf ASCII-Buchstaben/Ziffern/`_` begrenzt.

`Rune::ROOT` und Skriptpfade werden Ruby als UTF-8 übergeben. Weil das
eingebettete `ruby_init()` nicht die normale Ruby-Optionsinitialisierung
durchläuft, setzt `vm.cpp` externe und Dateisystemkodierung explizit auf
UTF-8; ein abweichender Dateisystem-Alias lässt den VM-Start scheitern,
statt Verzeichnisnamen still über die Codepage zu ersetzen. `Game.setup`
entfernt keine führenden Leerzeichen aus `RUNE_PROJECT`.

Windows-2022-CI für `ae0cfe6`, [Lauf 45169345662](https://github.com/Kenk-JADev/Game_Engine-V2/actions/runs/45169345662):
28 Pfadprüfungen/0 Fehler, darunter Umlaut, CJK und Emoji (teils außerhalb
der aktiven Codepage), F9-/Kartenhilfen, langer Listboxtext und F4-Kindprozess in beiden
EXE-Layouts. Der Kindprozess ist der Testtreiber, nicht das komplette Spiel.
Die echte kopierte Editor-CLI importiert ein Bild in einer Unicode-Wurzel;
die eingebettete Ruby-/WARP-Brücke prüft reale Projekt-/Karten-/Spielstand-
pfade sowie Texturen und Pixel (28/0). WARP insgesamt 36/0, Materialtest
57/0, Ruby-Suiten 27/0/0. Keine interaktive GUI, keine Hardware-GPU und kein
vollständiger Launcher-/F5-Test. Ein langer Kartenkomponententest verwendet
explizit `\\?\` und belegt keine allgemeine Langpfadunterstützung. Karten
und Datenbank bekamen dadurch noch keinen Dateischutz; 19s ergänzt die oben
beschriebene eigene Transaktion. Live-Test:
`docs/WINDOWS_MATERIAL_LIVE_TEST.md`.

## Projektgebundene Materialien und Texturen (Etappe 19q)

`texture_limits.h` begrenzt Dimensionen/Bytes, Slots, Manifest und Eingabebild.
`material_document.h` hält den vollständigen JSON-Baum: unbekannte und
ungültige Einträge bleiben erhalten, doppelte Schlüssel/Namen, ungültiges
UTF-8 und nicht speicherbare Zahlen sperren das Dokument. F3 ändert gezielt
Tile oder entfernt nur Metadaten; F7 bestätigt Ersatz und erhält altes RGBA.

`project_assets_win.h` verankert unter der ausdrücklich vertrauten Engine-
Wurzel jede Material-Pfadkomponente mit einem Windows-Verzeichnishandle ohne
Delete-Sharing. Reparsepunkte, Hardlinks und Pfad-/Gerätenamen werden
abgewiesen; Reads und WIC sind begrenzt. Editor und Ruby halten diese
Projektbindung bis zum Wechsel. Import legt RGBA exklusiv neu an; der
Manifest-Commit vergleicht die Originalbytes unter Dateisperre, verschiebt
bei Bedarf das Original in eine Sicherung und hängt die vorbereitete Datei
ohne Überschreiben ein. Das Rename nutzt `NtSetInformationFile` mit dem
gehaltenen Zielverzeichnis und einem einzelnen Dateinamen, ohne Pfad-Fallback.
Das ist kein atomarer Namensaustausch: bei Absturz oder blockierter
Rücksetzung bleibt eine Sicherung zur Wiederherstellung. Der Materialschutz
allein umfasst keine anderen Projektdateien und vertraut dem Engine-Anker;
der separate Karten-/F9-Schutz kam in 19s hinzu.

Der Renderer hält Basis- und Projekt-Namensräume separat; Projekt-Overrides
gewinnen bei der Auflösung und im Atlas. Projektwechsel/F5 verwerfen Projekt-
Slots, Draw-Kommandos und Terrain-/Atlasreferenzen, während Basisslots
unverändert bleiben; freie Slots werden wiederverwendet. Prefabs speichern
Namen statt langlebiger Slotnummern. Änderungen oder Löschungen räumen
betroffene Atlas-Terrainreferenzen ab, bevor der Aufrufer neu aufbaut.

`material_test.cpp` testet den portablen Baum und unter Windows Dateisperren,
Hardlinks, Junctions, ein abweichendes Arbeitsverzeichnis und einen
bewusst erzeugten Konflikt. Der WIC-Test nutzt nur ein temporäres Projekt.
`renderer_warp_test.cpp` liest D3D11-WARP-Pixel, einschließlich Atlas-Terrain
und A→B→A. `embedded_texture_test.cpp` verbindet echte CRuby-Module, den
Windows-Dateistore und den WARP-Renderer, einschließlich Unicode-Projektname,
Textur-Neuladepfad und A→B→A über die API. Er simuliert weder das Drücken von
F5 noch den vollständigen Launcher. Der Test setzt den Load-Pfad der gewählten
Ruby-Installation und belegt nicht deren entkoppeltes Deployment.

Windows-2022-CI, Commit `776044d`, Lauf 45162482261: Material 57/0, WARP
34/0, eingebettete Texturbrücke 19/0 und Ruby-Suite 27/0. Ein interaktiver
Hardware-/GUI-/DPI-Test bleibt separat erforderlich:
`docs/WINDOWS_MATERIAL_LIVE_TEST.md`.

## Sichere Terrain-/Atmosphärenvorschau (Etappe 19p)

`world/terrain_limits.h` definiert einen portablen Plan: Breite/Höhe
1–10000, X-Auflösung 8–1024, höchstens 1024 Z-Zellen und 262144 Quads;
Seed 0–4294967295, Amplitude ±10000, Pinsel-/Flächenkoordinaten ±1000000,
Radius bzw. Ausdehnung 0,001–1000000. Pro Karte höchstens je 2048 Pinsel
und Splats, 4 Mio. Pinsel-Samples und 8 Mio. Quad-Splat-Prüfungen. Vor
`float`-/Integer-Casts wird geprüft; Höheabfragen werden vor dem Index-Cast
auf die Kartengrenze geklemmt. `Terrain` gibt bei ungültigem Plan, Budget-
oder Speicherfehler `false` zurück und verwirft das alte/teilweise Mesh.
`gfx/mesh_vertex.h` entkoppelt das echte Terrain für Linux-Tests von Win32.

`terrain_preview.h` liest ohne JSON-Mutation, erkennt mehrdeutige Felder
und hält Grundfehler, Pinsel und Flächen vom Mesh fern. Einzelne ungültige
Texture-/Tile-/Atmosphärenwerte erzeugen Warnungen und Vorschau-Ersatz;
Texturnamen sind auf 1024 Bytes ohne NUL begrenzt. Tile gilt für 0,01–10000,
Fog-Far für 0,1–100000 und RGB für 0–0xFFFFFF. Wetterintensität ist größer
0 bis 10 (höchstens 400 Partikel). `mapcheck.rb` prüft dieselben Grenzwerte;
`tools/sync_test.rb` vergleicht die Konstanten. F10 und die Statusleiste
behalten auch tatsächliche Aufbau-/Atlasfehler sichtbar; JSON-Altdaten
werden dadurch nicht repariert oder automatisch gespeichert.

F2 validiert gezielt geänderte Felder; F7 bereitet Textur/Kachel auf einer
Kopie vor, ohne fehlende oder ungültige unberührte Felder anzulegen bzw.
zu ersetzen. Das bewusste Ersetzen ungültiger F7-Felder wird bestätigt.
Die Splatliste zeigt bis zu 2048 Originalzeilen mit ihren echten JSON-
Indizes und eine nicht löschbare Auslassungszeile. Ungültige Einträge
brauchen vor dem Löschen eine Rückfrage; eine vorhandene leere Liste bleibt
erhalten. F7 bindet sich an Karten-/Projekt-Epochen (auch A→B→A); zentrale
Datenrückfragen sperren konkurrierende Editorfenster. F2 prüft den Entwurf
nach einer Rückfrage erneut; F2 und F6 öffnen nicht gleichzeitig. Bereits
zugestellte Listenauswahlen werden anhand ihrer Originalindizes abgeglichen;
F5 sperrt konkurrierende F7-/Material-/Script-/F9-Datenaktionen.

`texture_path.h` weist beim Editor-Lesen und -Schreiben/Löschen von RGBA-
Dateien Pfadtrenner, Datenströme, Windows-Gerätenamen und zu lange Namen
lexikalisch ab. In 19p allein waren Reparsepunkte und Dateikonflikte damit
nicht geprüft; 19q ergänzt den oben beschriebenen Material-Dateistore.

`AtlasPlan` begrenzt 64×64-Tiles auf 1024 (maximal 2048×2048, 16 MiB pro
RGBA-Kopie). `Renderer::update_atlas` verwendet bei unveränderten
Eingaben den Cache, bei Änderungen einen privaten wiederverwendeten Slot;
eine öffentliche Textur namens `atlas` bleibt eine normale Eingabe.
Auch wiederholtes Registrieren desselben Textur-Namens ersetzt den Slot
statt alte CPU-/GPU-Kopien anzuhäufen. 19q ergänzt Projekt-Lebensdauer und
Speichergrenzen für Texturen; andere Projektdateien sind davon nicht erfasst.
Fehlt Atlas oder Basiszelle, zeigen Editor und Spiel Einzeltextur-Boden;
unbekannte Flächentexturen fallen auf die Basis zurück und werden gemeldet.
Der Shader-Faktor 2/tile steht für Atlas-UV neutral auf 1 (`tile` = 2),
nicht wie früher auf 2. `terrain.tile` wird im Splatpfad nicht angewendet.

`terrain_api.rb` reicht Hash- oder Array-Höhenpinsel ohne unsichere Ruby-
Koerzierung an die nativen Bindings und leert den alten Pinselsatz beim
nächsten Aufruf. `Game.build_terrain` wird auch beim Launcher und dem
Ruby-Editor-Neuaufbau genutzt; es setzt erst die Basistextur und erhält
bei fehlendem Atlas den schon erzeugten Einzeltexturboden. Fehlerhafte
Flächen verwerfen dagegen das Terrain. Die 32-Bit-`long`-Byteberechnung
für Ruby-Texturen wird vor der Multiplikation auf D3D-Maße begrenzt.

Lokal: 671 JSON-/Editor-Checks und 51 Terrain-Checks jeweils in Release
und mit ASan/UBSan/float-cast-overflow; Ruby 3.0.6 nativ 26 Suiten OK,
ein EXE-Skip; dieselben 26 Suiten einzeln mit Ruby 3.4-WASM im isolierten
WASI-Shim OK (ohne EXE-Test). Die 3.4-JSON-/Event-
Brücken nutzen echte C++-Fixtures (14 Dateien/0 Format- oder Semantik-
fehler, 17 Event-Checks/0 Fehler), da WASM kein Open3 ausführen kann.
Win64-Editor, Renderer, Terrain und Ruby-VM wurden cross-kompiliert;
zusätzliche unsichtbare Windows-Listboxprüfungen wurden lokal nicht ausgeführt.
Der spätere 19q-CI-Lauf baut unter Windows mit Ruby 3.4 und prüft WARP sowie
die eingebettete Texturbrücke (Ergebnisse im 19q-Abschnitt). Interaktive
GUI-/Hardware-GPU-Prüfung und GPU-Fehler-Injektion bleiben offen (Handoff/Backlog).

## F6-Layout für kleine Arbeitsflächen (Etappe 19o.6)

`event_layout.h` berechnet unabhängig von Win32 das Einpassen auf eine
Monitor-Arbeitsfläche, DPI-Skalierung, Scrollgrenzen und Fokus-Nachführung.
Die Formularfläche beträgt 1000×702 bei 96 DPI; Combo-Dropdowns sind eigene
Popups und zählen nicht zu ihrer geschlossenen Zeile. Das äußere Fenster
berücksichtigt Rahmen und Scrollleisten, wird auf dem Monitor des
Hauptfensters zentriert/begrenzt und ist größenänderbar. Bei DPI- oder
Arbeitsflächenwechsel werden Größe, Schrift und Koordinaten angepasst.

`event_view_win.h` hält logische Originalkoordinaten aller Controls und
positioniert beim Scrollen auch vollständig außerhalb des Ausschnitts
befindliche Kinder per `DeferWindowPos`. Die Fensterleisten bleiben bei
genügend Platz deaktiviert sichtbar; Mausrad wird nur verarbeitet, wenn es
kein Kind verarbeitet, Umschalt+Rad scrollt horizontal. Nach einem echten
Fokuswechsel und nach Validierungsfehlern wird der Fokus sichtbar gemacht;
manuelles Scrollen springt nicht zum alten Fokus zurück. Die Controls bleiben
direkte Dialogkinder, damit `WM_COMMAND`, Auswahl-/Pending-Schutz, Combos,
Mehrzeiler und `IsDialogMessageW` weiterhin denselben Weg nutzen.

`tools/json_test.cpp` liest die tatsächlichen F6-Controlpositionen aus der
Quelle und simuliert kleine Ansichten mit 100–300 % DPI, Scrollgrenzen,
Fokus und negativen Monitorkoordinaten. Unter Windows ergänzt es unsichtbare
Controltests für Nachrichten, Text, Auswahl, Combos und simulierten
DPI-Wechsel; `build.ps1 -Test` linkt dafür User32/GDI32. Lokal bestanden
613 portable Checks und Sanitizer; Win64 wurde cross-kompiliert und der
Test gelinkt. Ein echter Windows-GUI-/DPI-/Mehrmonitor-Test ist weiterhin
erforderlich (Testplan im Handoff); CI-Build oder versteckte Controls
belegen keine tatsächliche Darstellung oder Tastatur-/Mausbedienung.

## Datentreue im Event-Editor (Etappe 19o.5)

`event_form.h` kapselt das portable Modell für Event-, Seiten- und
Befehlsformulare. Lesen, Anzeigen und Öffnen verändern das JSON nicht.
Vorher-/Nachher-Snapshots unterscheiden unberührte Werte von echten
Eingaben; `event_batch_prepare` validiert eine Kombination der drei
Formulare auf einer Kopie. Die Win32-Oberfläche übernimmt sie erst nach allen
Prüfungen und gegebenenfalls Bestätigung. Ein Fehler rollt die gesamte
Kombination zurück. Unveränderte große Integer-Tokens, präzise Floats, fehlende Felder,
fremde Metadaten und verschachtelte Zweige bleiben erhalten. Nicht
vollständig in Win32 darstellbare Felder sind geschützt; sie dürfen auch
nicht still als Eingabe für einen neu eingefügten/umgewandelten Befehl
dienen. Für editierte Zahlen und Texte gelten explizite Validierungen.

Events ohne gültige nichtleere `pages`-Liste zeigen wie die Ruby-Laufzeit
eine implizite Seite. Trigger- und Befehlsänderungen bleiben zunächst im
Altformat. Neue Bedingungen oder Seiten materialisieren die alte Seite auf
einer Kopie; alte Top-Level-`trigger`/`commands` bleiben erhalten, die
Laufzeit verwendet bei vorhandenen Seiten die Seitenwerte. Nichtlistenförmige
oder doppelte `pages` und semantisch nicht äquivalente Alttrigger/-befehle
verhindern eine Migration, statt Daten zu überschreiben. Doppelte relevante
Schlüssel werden nicht mehrdeutig bearbeitet (Ruby/Parser haben verschiedene
Trefferregeln). Fehlende Zweige entstehen erst beim Einfügen; falsche Zweige
bleiben erhalten. Ein Typwechsel und das Entfernen von Daten werden
bestätigt. Wahlzweige bleiben nach Index erhalten, auch überzählige Zweige;
Änderungen an Optionen lösen eine Zuordnungswarnung aus.

F6 hält keine langlebigen JSON-Befehlszeiger mehr: `EventCommandPath`
adressiert Zweig und Index auf der jeweils aktuellen Kopie. ListBox-
Auswahlmeldungen werden nach dem Callback abgeglichen; Button-Aktionen und
Aktionen des Hauptfensters klären noch gepostete Auswahlwechsel. Bei Pending-Eingaben fragt
`resolve_event_edits()` vor Auswahl, Schließen, strukturellen Aktionen,
Speichern und Karten-/Projektwechsel. Nein verwirft nur den Entwurf,
Abbrechen erhält ihn und die alte Auswahl. `data_prompt` sperrt andere
Editorfenster und verschiebt reentrante Auswahlmeldungen. Die Karte wird
erst bei tatsächlicher Änderung `dirty`.

Neue Events und Kopien erhalten positive freie IDs und eindeutige Namen
gegenüber darstellbaren Event-/NPC-Namen (Self-Switch-/Spielstandschlüssel);
geänderte kollidierende Namen werden abgelehnt. Auch eine kollisionsfreie
Umbenennung braucht Bestätigung, weil alte Spielstände und Self-Switches nicht
auf den neuen Schlüssel migriert werden. Die ID-Suche berücksichtigt
doppelte Schlüssel und arbeitet auch bei vielen Events effizient. Kopien
inkrementieren ganzzahlige Koordinaten exakt; fehlende, ungültige oder nicht
verschiebbare Koordinaten bleiben erhalten und werden gemeldet. Bei neu
eingegebenem `input_number` muss `initial` zur Ziffernzahl passen, damit
Ruby den Wert nicht still klemmt; unveränderte historische Werte bleiben.
F2/F5 erfordern weiterhin ein geschlossenes F6-Fenster, Event-Undo fehlt.

`tools/json_test.cpp` prüft alle 26 Typen, Migration, Batch-Rollback,
Duplikate, große Zahlen, Schutz und echte Karten. `event_bridge_check.rb`
lädt C++-Ergebnisse mit Ruby `Event`/`Interpreter` (einschließlich Choices,
Self-Switch-Isolation und Zahleneingabe); `sync_test.rb` liest Modell und
Win32-Typ-/Trigger-Listen. Lokal 577 portable Checks/0 Fehler, Sanitizer,
Win64-Cross-Kompilierung und 17/0 Event-Brückenchecks via Ruby-WASM-Fixture;
native Open3-Ausführung und vollständigen Windows-Link prüft CI. Die GUI und
Auswahl-/MessageBox-Abläufe benötigen weiterhin einen echten Windows-Live-Test.
Zum überarbeiteten F6-Layout und dem ausstehenden DPI-Test siehe 19o.6 oben.

## Datentreue im Objektformular (Etappe 19o.4)

`object_props_form.h` liest X/Z, Rotation (Grad) und Material als
Formular-Snapshot. `object_props_prepare` prüft Änderungen vollständig auf
einer Kopie; nur tatsächlich veränderte Felder werden ersetzt. Unberührte
Integer-Tokens und Kommazahlen, Y-Versatz, fehlende Felder und unbekannte
Eigenschaften bleiben erhalten. Neue Zahlen sind endlich, maximal ±1 Mio.
und erlauben Dezimalkomma; Material ist einzeiliges UTF-8. Ungültige
Eingaben verhindern die gesamte Übernahme; fehlerhafte oder nicht
darstellbare Altdaten erfordern vor dem Ersetzen Zustimmung. Doppelte
Eigenschaften und Texte, die ein Win32-Editfeld nicht verlustfrei
zurückgibt, sind für die jeweilige Eingabe gesperrt. F3 nutzt dieselbe
Vorbereitung und schützt ein nicht darstellbares altes Material.

Auswahl, Undo/Redo, Duplizieren, Drehen, Löschen, F3-Materialaktionen,
F5, Karten- und Projektwechsel sowie Schließen klären offene Formulareingaben;
Strg+S übernimmt sie vor dem Speichern. Ein unverändertes Formular erzeugt
keinen Undo-Schnappschuss. Win32-Listen-/Baumauswahl wird nach Abschluss
ihrer Benachrichtigungen abgeglichen; bei Abbruch wird die alte Auswahl
wiederhergestellt. Daten-Rückfragen sperren auch moduslose Nebenfenster.
Objekt-Feldfokus unterdrückt Kamera-/Bearbeitungskürzel, nicht aber Strg+S.
Tasten anderer Fenster oder Programme steuern weder Kamera noch Objekte.
Die Kartenbaum-Einfügetexte bleiben bis zum jeweiligen Win32-Aufruf gültig.

Ein Drag speichert den Griffpunkt und den ursprünglichen Objektwert. Erst
nach Bewegung über eine halbe Rasterzelle ändert sich eine Achse; nur
bewegte Achsen werden eingerastet. Beim Zurückziehen zum Anfang werden
ursprüngliche Zahlentypen, Präzision und fehlende/null-Felder restauriert.
Undo wird beim ersten tatsächlichen Positionswechsel angelegt, und Capture
endet bei Zustandswechsel oder Verlust. Die Viewport-Höhenabfrage klemmt
Positionen vor dem Terrain-Index-Cast auf die Kartengrenzen. Objekte mit
ungültigen, mehrfachen oder extremen X/Y/Z-/Drehungswerten bleiben im
JSON und in der Liste, werden aber nicht gerendert oder gepickt.

`map_document.h` prüft transaktional vor dem Laden: gültige JSON-Wurzel,
UTF-8/Unicode, keine nicht darstellbaren Kommazahlen und höchstens ein
`objects`, das, falls vorhanden, eine Liste sein muss. Andere doppelte
Schlüssel bleiben erhalten; Objektfelder mit Duplikaten sind gesperrt.
`json_validation.h` und `text_validation.h` sind gemeinsame Prüfer für
Karte und F9. Die Prüfung deckt nicht alle semantischen Extremwerte ab; der gezielte
Terrain-/Atmosphärenschutz wurde in 19p ergänzt. Karten behalten
die Originalbytes ihres letzten Ladens/Speicherns: Vor Speichern oder
Löschen wird bei externen Änderungen gefragt. Der frühere Pfad über
`atomic_file_win.h` ist seit 19s durch die oben beschriebene Handle-
Transaktion ersetzt; sie ist kein atomarer Namensaustausch.

`tools/json_test.cpp` testet u. a. 400-stellige Integer, präzise Werte,
ungültige und doppelte Felder, Preview-/Drag-Schutz, Rückstellen und die
echten Dorfkarten sowie Dateischnappschüsse. Lokal: 390 portable Checks/0
Fehler, ASan/UBSan/float-cast-overflow, Win64-Cross-Kompilierung und 25
Ruby-WASM-Tests (ein EXE-Skip); die Ruby/C++-Brücke prüfte 14 Dateien/0
Fehler. Die Windows-CI führt zusätzliche kopflose Tests der gemeinsamen
Dateihilfe aus (Unicode-Pfad, gesperrtes Ziel, kein Überschreiben bei
Neuanlage, mehrere Blöcke). Ein echter Windows-GUI- und End-to-End-Test
der Dateikonflikt-Dialoge steht aus.

## Datentreue im F9-Datenbankdialog (Etappe 19o.3)

`database_form.h` liest Text, Integer, Float und Gegnerlisten als portable
Formularwerte. `database_form_prepare` prüft alle geänderten Felder und
Gold auf einer Kopie, `database_form_commit` übernimmt erst nach Rückfragen.
Unveränderte Werte, Typen, fehlende Felder (auch Helden-Level/Exp) und
unbekannte Daten werden nicht ergänzt oder gerundet. Ganzzahlen werden
als Dezimaltext mit beliebiger Stellenzahl eingegeben (optional Plus/Minus,
führende Nullen werden normalisiert), Dezimalwerte müssen endlich sein und
dürfen Dezimalkomma/Exponent verwenden. Semantische Bereiche wie HP > 0
oder Drop-Chance 0..1 werden weiterhin nicht geprüft.

Gold ist unabhängig von der Eintragsauswahl. Bei Fehleingaben bleiben alle
Entwürfe erhalten und Auswahl/Reiter wechseln nicht. Gegner-IDs können
doppelt vorkommen; mehrdeutige/falsche Altdaten werden bei Listenänderung
bestätigt, Nicht-Objekte bleiben bis zum bestätigten Löschen stehen. F9
sperrt Einzeltexte, die das Windows-Editfeld nicht verlustfrei darstellen
kann. Fehlende Sammlungen entstehen erst bei Neu, falsche Sammlungen nur
nach ausdrücklicher Zustimmung. Schließen und Neu laden fragen bei
ungespeicherten Änderungen; beim Schließen des Editors wird F9 ebenfalls
berücksichtigt.

Ungültiges JSON, doppelte Schlüssel, ungültiges UTF-8/Unicode, falsche
Wurzeltypen und nicht endliche Float-Altwerte werden nicht geladen. Ein
Lesefehler ersetzt weder den bisherigen Entwurf noch die Datei. Speichern
vergleicht die Dateibytes mit dem Stand beim Laden/Speichern, fragt bei
externen Änderungen. Seit 19s wird der bestätigte Stand anschließend unter
einer Handle-Sperre erneut geprüft und ohne ersetzendes Rename veröffentlicht
(Grenzen siehe oben). Fehlende database.json lässt sich explizit neu speichern.

`tools/json_test.cpp` testet die Logik einschließlich ungültiger Dokumente,
fehlender Auswahl, sehr großer Werte, kaputter Listen, Konfliktvergleich
und der mitgelieferten Datenbanken. Lokal: 314 C++-Checks und ASan/UBSan
erfolgreich, Win64 cross-kompiliert, Ruby-WASM 25 OK/1 Windows-EXE-Skip.
Die Win32-Dateioperationen und Dialog-Abläufe sind damit nicht grafisch
getestet; ein echter Windows-Live-Test steht aus.

## Datentreue Block-Zonen im F2-Dialog (Etappe 19o.2)

`blocked_form.h` zeigt für jeden vorhandenen Arrayeintrag eine temporäre
Zeilenmarke `#1:` usw. (keine neue JSON-Eigenschaft). Umordnen und Ändern
derselben markierten Zeile behalten Zusatzfelder und unberührte Zahlen samt
Typ/Präzision. Neue Zeilen ohne Nummer benutzen `x; z; w; h` oder die alte
`x,z,w,h`-Syntax; Semikolon erlaubt Dezimalkommas. Entfernte Nummern und
ungültige ersetzte Altdaten melden sich vor dem Übernehmen zur Bestätigung.
Doppelte/unbekannte Marken, nicht endliche Zahlen, Werte über 1.000.000 und
nichtpositive neue Breiten/Höhen werden abgelehnt; ungültige alte Einzel-
felder (`?`) und Nicht-Objekte lassen sich unverändert mitführen. Zu lange
bzw. im Windows-Editfeld nicht darstellbare Listen werden gesperrt.

`blocked_form_prepare` baut einen validierten Entwurf ohne Schreibzugriff.
Erst nach allen Block-/Encounter-Prüfungen und Bestätigungen übernimmt
`map_props_apply` die F2-Felder; bei Fehler bleibt die gesamte Karte
unverändert. F6 muss vor F2 für konsistente Karten-/Formularentwürfe
geschlossen sein. `json_test.cpp` prüft u. a. gleiche
Geometrien mit verschiedenen Zusatzdaten, Umordnung, Zahlenpräzision,
Lösch-/Ersetzungswarnungen, fehlerhafte Daten und atomare Übernahme.
Grafischer F2-Live-Test auf Windows steht weiterhin aus.

## Exakte JSON-Ganzzahlen (Etappe 19o.1)

Ganzzahlige JSON-Tokens werden in `JsonValue` zusätzlich zur double-Näherung
als validierte Dezimalzeichenfolge gehalten und beim Schreiben unverändert
ausgegeben (auch außerhalb des double-Bereichs). `number()` liefert für
Berechnungen weiterhin nur die Näherung; der Wert ist privat, Ersetzen
erfolgt über `make_num`/`make_integer`, `increment()` arbeitet für gelesene
Ganzzahlen exakt. Das sichere Inkrement nutzen Objekt-/Event-Duplizieren;
seit 19o.5 schreibt F6 unveränderte Formularwerte nicht mehr zurück.
Für neue `make_num(..., true)`-Werte schreibt der Generator die ganzzahlige
double-Näherung ohne int64-Cast. JSON-Zahlen werden streng nach Grammatik
eingelesen; `-0` als Ganzzahl wird wie in Ruby zu `0` normalisiert.

Tests decken 2^53, 2^63, negative und 400-stellige Werte, Kopien, Änderungen,
Inkremente und ungültige Tokens ab. Die Ruby-JSON-Brücke vergleicht außerdem
Typen und synthetische große Zahlen. Grenzen: Kommazahlen/Rechenoperationen
bleiben double-basiert; F6 und das Objektformular schützen seit 19o.5 bzw.
19o.4 unberührte Formularfelder.

## NPCs, Schilder und Objekte im Platzier-Editor (Etappe 19n)

F5 bzw. „Events → NPCs, Schilder und Objekte“ öffnet einen modalen Dialog
mit Kategorienliste, Neu/Löschen und Formular. X/Z sind Weltpositionen, Y ist
ein optionaler Versatz zur Geländehöhe; Objektdrehung/Schilddrehung wird im
Dialog in Grad, im JSON in Radiant geführt. Die letzte Cursor-Bodenposition
lässt sich übernehmen. NPCs haben Name, Tempo, Radius, Wegpunkte (`X; Z`,
Dezimalkomma erlaubt) und einen ersten Text. Schilder sind gewöhnliche
Action-Events mit eindeutiger ID, `visual: "schild"` und Textbefehl; die
zwei nicht-soliden Zeichenknoten werden beim Terrain-Neubau aktualisiert.
Bestehende Objekte behalten ihren Prefab-Typ und alle nicht geänderten Felder.

`placement_form.h` validiert auf Kopien und bewahrt unbekannte Eigenschaften,
weitere Befehle und unveränderte präzise Zahlen. Ungültige alte Wegpunkte
werden erst nach Rückfrage ersetzt; nicht darstellbare/komplexe Texte werden
nicht überschrieben. Mehrseitige Schilder werden in F6 weiterbearbeitet;
komplexe NPC-Kommandos sind im Formular nur eingeschränkt editierbar. Der
Dialog sammelt Änderungen auf einer Kartenkopie (OK übernimmt, Abbrechen
verwirft, Strg+S speichert), deaktiviert die übrigen Fenster und verlangt
einen geschlossenen Event-Editor, damit dessen Formularzustand und die
bearbeitete Kartenkopie nicht auseinanderlaufen.
NPCs und Schilder samt Wegpunkten werden im Standalone-Editor angezeigt.
Objektänderungen aus F5 werden im vorhandenen Objekt-Undo abgelegt; NPC-/
Schildänderungen sind noch nicht rückholbar. Der Undo-Stapel wird bei jedem
Kartenladen geleert, damit keine Objekte aus einer anderen Karte auftauchen.

Laufzeit: `Npc` und `Prefabs.instantiate` berücksichtigen optionales `y`;
Kollision und E-Reichweite bleiben wie bisher zweidimensional in X/Z.
`Game.update` verarbeitet auch NPC-Events und `Game.pickables` verdeckt ihre
Events nicht mit Körperknoten. Script-Nodes werden unabhängig vom Namen
nicht als Schildgrafik behandelt. Einmalig ausgelöste NPC-Auto-/Touch-Events
überleben Spielstände; ältere Spielstände ohne `fired` bleiben gültig.
MapCheck und F10 prüfen ungültiges `y`.
Tests: `json_test.cpp`, `npc_test.rb`, `map_objects_test.rb`, `mapcheck_test.rb`
und `encounter_test.rb` (echter Spielablauf). Ein grafischer Windows-Test
für F5, Layout/Tab und Playtest steht noch aus.

## Ruby-API (Stand Etappe 2)

| API | Bedeutung |
|---|---|
| `Rune::Gfx.clear_color = 0xRRGGBB` | Himmelsfarbe |
| `Rune::Gfx.box(px,py,pz, sx,sy,sz, color, rx=0, ry=0, rz=0)` | Box in die Draw-List |
| `Rune::Gfx.rect(x, y, w, h, color, alpha = 1.0)` | Bildschirm-Rechteck |
| `Rune::Gfx.text(x, y, s, color = 0xFFFFFF, size = 18)` | Bildschirm-Text |
| `Rune::Camera.mode = :first \| :third` | Kameramodus |
| `Rune::Camera.yaw` / `.yaw =` | Blickrichtung (Radiant) |
| `Rune::Camera.player_position` | Spielerposition (aus der Physik) |
| `Rune::World.generate(width: 240, height: 240, resolution: 192, seed: 7, amplitude: 18.0)` | Gelände bauen (rechteckig, height entfällt = Quadrat) |
| `Rune::Gfx.fog(an, farbe, sichtweite)` | Distanznebel an/aus, Nebelfarbe, Distanz |
| `Rune::World.height_at(x, z)` | Bodenhöhe an einer Stelle |
| `Rune::World.ready?` | Gelände vorhanden? |
| `Rune::Player.position` / `.teleport(x,y,z)` | Spieler |
| `Rune::Player.on_ground?` / `.speed=` / `.jump_speed=` | Zustand/Tuning |
| `Rune::Physics.clear_solids` / `.add_solid(x,y,z, sx,sy,sz)` / `.solid_count` | Hindernisse |
| `Rune::Physics.solid_at?(x,y,z)` | Punkt in einem Hindernis? (19k.3, für `Passage.hindernis?`) |
| `Rune::Physics.clear_blocked` / `.add_blocked(x,z,w,h)` / `.blocked_count` / `.blocked?(x,z)` | Block-Zonen der Karte (19k.2) |
| `Rune::Input.down?(:w)` / `pressed?(:f5)` | Tastenzustand |
| `Rune::Input.mouse_delta` / `.wheel` | Mausbewegung |
| `Rune::Input.mouse_position` | Zeiger in Fensterpixeln `[x, y]` (im Fang-Modus: Mitte) |
| `Rune::Input.mouse_down?(0)` / `mouse_pressed?(0)` | Maustasten (0 links, 1 rechts, 2 Mitte) |
| `Rune::Input.captured?` | Maus gefangen? |
| `Rune::Input.capture = false` | Mausfang von Ruby aus umschalten |
| `Rune::Camera.ray(x, y)` | Weltstrahl `[ox,oy,oz,dx,dy,dz]` fürs Picking |
| `Rune::Camera.distance` / `.distance=` | Third-Person-Abstand |
| `Rune::Camera.info` | Kamera-Zustand als Kurzstring (HUD/Debug) |
| `Rune::Time.fps` / `.dt` | Zeit |
| `Rune::Log.info/warn/error` | Logdatei `log/engine.log` |

`Rune::Camera.follow(x,y,z)` ist entfallen — die Kamera folgt immer dem Physik-Spieler.

## Tasten

RuneEditor: `F4` startet den Playtest als eigenen Prozess. Im Spiel gibt es
seit 19t keinen Editor-Modus mehr.
`WASD` laufen · `Shift` rennen · `Leertaste` springen · `E` reden/benutzen ·
`F1` First-Person · `F3` Third-Person · `F2` Debug-Position ins Log ·
`F5` Ruby neu laden · `F12` Screenshot nach `log/screenshot_N.bmp` ·
`Esc` Menü (Speichern/Laden/Titel) bzw. Maus freigeben · `Mitte` Fang umschalten · Klick = Event/Picking ·
Titelbildschirm: `1..9`/`Hoch`/`Runter`/Maus Projekt wählen, `Enter`/Klick
spielen, `L` Fortsetzen, `N` neues Projekt, `D` löschen (J = ja), `Esc` beenden

## RPG-Maker-Kern (Etappe 3)

Der Kern liegt **komplett in Ruby** (`game/engine/`), die Engine liefert nur
Eingabe, Welt, Textfenster und Teleport. Damit ist alles ohne C++-Neubau
änderbar (F5 genügt).

| Datei | Aufgabe |
|---|---|
| `game/engine/data.rb` | `Database.system` / `Database.map(id)`, JSON mit Cache, UTF-8 |
| `game/engine/state.rb` | `GameState` — Schalter (bool) und Variablen (Zahl) |
| `game/engine/event.rb` | `Event` — Position, Radius, Auslöser, Kommandoliste |
| `game/engine/interpreter.rb` | `Interpreter` — führt Kommandos aus, hält das Textfenster |

**Auslöser:** `action` (in Reichweite + `E`) · `touch` (Betreten, einmalig) ·
`auto` (beim Kartenstart). Es läuft immer nur **ein** Interpreter; solange das
Textfenster offen ist, wird nichts Neues ausgelöst.

**Kommandos** (in `game/data/maps/*.json`):

| Kommando | Wirkung |
|---|---|
| `{"cmd":"text","lines":[…]}` | Nachrichtenfenster, blockiert bis `E` |
| `{"cmd":"switch","name":…,"value":true}` | Schalter setzen |
| `{"cmd":"variable","name":…,"value":n}` / `,"add":n` | Variable setzen/erhöhen |
| `{"cmd":"if_switch","name":…,"value":true,"then":[…],"else":[…]}` | Verzweigung |
| `{"cmd":"if_variable","name":…,"op":">=","value":n,"then":[…],"else":[…]}` | Vergleich (`== != < <= > >=`) |
| `{"cmd":"wait","seconds":n}` | blockiert für n Sekunden |
| `{"cmd":"teleport","x":…,"z":…,"yaw":…}` | Spieler versetzen |
| `{"cmd":"log","text":…}` | Zeile ins Log |

Test ohne Engine: `ruby -I game/engine tools/event_test.rb` (23 Prüfungen,
Exit-Code 0 = grün).

## Maus & Picking (Etappe 4)

Die Engine rechnet nur den **Weltstrahl** aus der Bildschirmposition
(`Rune::Camera.ray(x, y)` → `[ox, oy, oz, dx, dy, dz]`); alles Weitere rechnet
Ruby (`game/engine/pick.rb`): Strahl/Achsenbox nach dem Slab-Verfahren,
Strahl/Kugel für Events, Auswahl des nächstgelegenen Objekts. **Rotierte
Boxen** werden seit Etappe 12 exakt getroffen: der Strahl wird in den
lokalen Boxraum zurückgedreht (Inverse der Engine-Konvention
`Rx·Ry·Rz` = `Rz(-rz)·Ry(-ry)·Rx(-rx)`) und dort gegen die ungedrehte Box
geprüft — `t` bleibt in Weltmetern, weil nur gedreht, nicht skaliert wird.

**Zwei Maus-Modi** (Fang umschalten: mittlere Maustaste, frei: `Esc`):

| Modus | Mauszeiger | Strahl kommt aus | Zweck |
|---|---|---|---|
| Gefangen | versteckt, Mitte | Fadenkreuz in der Bildschirmmitte | Egoperspektive: schauen + zielen |
| Frei | sichtbar, echtes Koordinatenpaar | Position des echten Zeigers | Menü/Spiel: hover + klicken |

Im gefangenen Modus steht `mouse_position` immer in der Bildschirmmitte
(das Fadenkreuz). `wm_mousemove` und `update_mouse_delta` pflegen die
Position nur im freien Modus.

**Klick-Aktion in `main.rb`:** Der Strahl wird pro Frame gegen alle sichtbaren
Szenenknoten **und** alle Events getestet. Getroffenes Objekt wird mit einem
dünnen Drahtkasten (12 Einzelboxen, es gibt noch keine Linien-Pipeline) und
einer Label-Zeile markiert. Linksklick: Events feuern sofort (auch ohne `E`),
Objekte schreiben Name/Größe/Abstand ins Log.

Test ohne Engine: `ruby -I game/engine tools/pick_test.rb` (19 Prüfungen).

## Prefabs und Kartenobjekte (Etappe 5, Szenenbau seit 19t)

Der damalige Ingame-Editor ist seit 19t entfernt; bearbeitet wird in
RuneEditor. Die Prefabs und der gemeinsame Laufzeit-Szenenbau bleiben.

**Prefabs** (`game/engine/prefabs.rb`) — 9 Typen mit je einer Liste von Kästen
(Versatz `d`, Größe `s`, Farbe `c`, `solid`):
Haus, Hütte, Zaun (2 m), Baum, Fels, Laterne, Kristall, Brunnen, Wegstück.
Ein neuer Typ ist ein Eintrag in `Prefabs::LIST` — kein C++-Neubau nötig.
Ein Prefab kann ein Feld `anim:` tragen (`'spin'` = Kristall: Drehen +
Schweben); die Animation hängt dann in `Prefabs.instantiate` am Knoten.

**Karten-JSON** — nur diese vier Felder pro Objekt:
```json
"objects": [ { "type": "haus", "x": -4.0, "z": -2.0, "rot": 0.0 } ]
```
Beim Laden baut `MapObjects#rebuild` alle Objekte in die Szene ein und meldet die
Hindernisse neu an die Physik; sie sind also sofort begehbar/fest.

Das ganze Dorf ist Datenmaterial: Weg (18 Stücke), Zaun (7 Segmente),
Brunnen, sechs Häuser, drei Kristalle, vier Laternen liegen als 39 Objekte in
`game/data/maps/dorf.json`. `main.rb` baut nur noch die leere Szene — alles
    Andere kommt aus der Karte und ist im Editor änder- und löschbar.
Konvertiert wurde einmalig mit `tools/village_to_json.rb` (Positionen 1:1 aus
dem alten Code übernommen).

Tests ohne native Engine: `ruby -I game/engine tools/map_objects_test.rb` und `ruby tools/runtime_boot_test.rb`.

## Versionsverwaltung

PortableGit 2.56.1 liegt ohne Adminrechte in `C:\Users\jusagu\Tools\PortableGit`
und ist im Benutzer-PATH (`...\PortableGit\cmd`) — neue Terminals kennen
`git` direkt. Im Projektordner gilt:

- `git status` / `git diff` — Was geändert?
- `git add -A && git commit -m "..."` — Stand sichern
- `git log --oneline` — Historie
- `git checkout -- <datei>` — einzelne Datei zurücksetzen

`.gitattributes` schaltet jede Zeilenende-Konvertierung ab (`* -text`), damit
Diffs nicht durch CRLF-Rauschen unlesbar werden. Ignoriert werden `build/`,
`log/`, `*.exe`, `*.o`, `*.dll`.

## Projektsystem (Etappe 8)

Ohne Projekt zeigt das Spiel den **Titelbildschirm** (`Launcher`):

| Taste | Wirkung |
|---|---|
| 1..9 | Projekt laden (Liste aus `projekte/`) |
| N | Neues Projekt — Name eintippen, `Enter` legt an und startet |
| Esc | Beenden bzw. zurück zur Liste |

Ein **Projekt** ist ein Ordner `projekte/<slug>/data/` — dieselbe Struktur wie
`game/data` (`system.json`, `maps/*.json`). `Database.root` zeigt je nach
`$project_root` auf `game/data` oder auf das Projekt; alle Lade-/Speicherpfade
(Karten, Editor, Events) laufen darüber. `Project.ensure_demo` kopiert beim
ersten Start `game/data` nach `projekte/dorf`, damit es immer ein Beispiel gibt.
Ein neues Projekt bekommt nur eine leere Karte `welt01` (nur Terrain,
Startpunkt 0/0) — keine Objekte, keine Events.

Der Titelbildschirm zeigt das **Demodorf als 3D-Kulisse**: `Launcher.activate`
generiert das Terrain aus `game/data` und baut die Szene mit
`MapObjects#rebuild`; die Kamera kreist, indem der Spieler (unsichtbar) auf einem
Kreis um das Dorf teleportiert wird und `Camera.yaw` entgegen zeigt. Die Maus
ist im Menü frei (`Rune::Input.capture = false` — die Engine startet
gefangen!), Einträge per Hoch/Runter **oder** Mausklick wählbar.

Pro Projekt zeigt die Liste Titel, Kartenanzahl und "zuletzt gespielt"
(`Project.info`; `Project.touch` schreibt `last_played` in `system.json` beim
Start). `D` löscht das markierte Projekt nach Rückfrage (`Project.delete`).
Beim Spielstart fängt der Launcher die Maus wieder ein, damit der Mausblick
wie bisher sofort funktioniert.

Kartenbearbeitung und Kartenwechsel im Editor erfolgen über RuneEditor.
`Game.change_map` bleibt als Spielscript-Aufruf für einen Laufzeitwechsel
erhalten, speichert dabei aber keine Editor-Entwürfe.

## Datenformat

Eigenes Format (JSON) unter `game/data/` — bewusst nicht RPG-Maker-kompatibel,
damit 3D-Daten (Höhen, Positionen, Rotationen) sauber abbildbar sind.
Ein Importer für MZ/MV-JSON ist als spätere Option vorgesehen.

- `game/data/system.json` — Spielname, Startkarte, Startposition (`x`,`z`,`yaw`),
  Anfangswerte für Schalter und Variablen.
- `game/data/maps/<id>.json` — `name`, `terrain` (size/resolution/seed/amplitude)
  und `events` (Liste aus `id`, `name`, `x`, `z`, `radius`, `trigger`, `commands`).

Die Geometrie (Häuser, Zäune, Kristalle) steht noch in `game/main.rb`;
Events referenzieren nur Positionen. Etappe 4 (Editor) schreibt beides in JSON.

## Fallstricke (wichtig beim Einbetten von CRuby 3.4)

1. **`ruby_init()` reicht nicht.** Seit 3.4 werden Kernmethoden in eingebauten
   Ruby-Skripten definiert (`Integer#times`, `Integer#to_i`, `Array#last`, …).
   Ohne den Aufruf von `rb_call_builtin_inits()` fehlen diese Methoden und jeder
   Skriptaufruf scheitert mit `undefined method '#<Symbol:0x…>'`.
   Der Aufruf steht in `src/ruby/vm.cpp`; `tools/ruby_smoke.cpp` prüft ihn.
2. **`rb_load` braucht absolute Pfade** mit Forward-Slashes — sonst
   `cannot load such file`. `ruby::load_script()` baut den Pfad ab der
   Projektwurzel selbst.
3. **`directxmath.h` aus MSYS2 ist nur ein Stub** (keine Vektoren/Matrizen).
   Deshalb die eigene, schlanke Mathe-Schicht in `src/math/math.h`.
4. **Ruby-Fehler pro Frame** würden das Log fluten. `ruby::tick()` loggt,
   zählt Fehler und stoppt den Tick nach drei Fehlern in Folge bis zum F5-Reload.
5. **`snprintf` ist in `ruby/subst.h` ein Makro.** In Übersetzungseinheiten, die
   Ruby-Header einbinden, wird `snprintf` zu `ruby_snprintf` umdefiniert —
   `std::snprintf` scheitert dann mit `'ruby_snprintf' is not a member of 'std'`.
   In `vm.cpp` deshalb Strings mit `std::to_string` zusammensetzen.
6. **Terrain- und Box-Draw teilen sich einen Objekt-CB.** `end_frame()` zeichnet
   das Gelände zürst mit Identitätsmatrix und Weiß, danach werden die Box-Puffer
   wieder gebunden. Wer Draw-Reihenfolgen ändert, muss beides beachten.
7. **UI-Textbreite muss jeden Frame neu gemessen werden.** Die Textbefehle werden
   pro Frame neu erzeugt (Breite 0). Nur das *Rastern* der GDI-Textur ist gecacht
   (Cache-Key = Position + Größe + Inhalt). Wer die Messung mitcacht, bekommt
   unsichtbaren Text, weil `w` im neuen Befehl wieder 0 ist.
8. **HLSL-Variablen ausserhalb eines `cbuffer` landen in `$Globals`, nicht in
   `register(b0)`.** Das Flag `use_tex` steht deshalb *im* `CB2D`-Block, sonst
   bleibt es immer 0 und Text wird als deckender Kasten gezeichnet.
9. **`Ui` und der 3D-Renderer teilen den Kontext.** `apply_pipeline()` setzt
   Tiefen-, Blend- und Rasterizer-State auf `nullptr` zurück (Standardzustand),
   damit der 3D-Pfad nach dem Overlay nicht mit UI-Zustand weiterläuft.
10. **`require 'json'` scheitert ohne Encoding-Datenbank.** In der eingebetteten
    Ruby fehlt `Encoding::UTF_8`, bis `require 'enc/encdb'` gelaufen ist.
    `vm.cpp` lädt das direkt nach `rb_call_builtin_inits()`; danach funktionieren
    `JSON.parse`/`JSON.generate` (JSON 2.9.1) und damit `game/data/*.json`.
    `require 'json'` muss trotzdem **selbst** im Skript stehen (`data.rb`).
11. **`Data` ist in Ruby 3.2+ eine eingebaute Klasse** (`Data.define`).
    `module Data` scheitert mit `TypeError: Data is not a module`. Die
    Datenbank heißt deshalb `Database`.
12. **`refresh_solids` nach dem Szenenaufbau.** `Node#add` meldet jedes `solid:`
    sofort an die Physik. Ohne `Physics.clear_solids` davor verdoppeln sich die
    Hindernisse bei jedem F5-Reload (31 → 62).
13. **Mausfang verfälscht die absolute Position.** Im Fang-Modus setzt
    `update_mouse_delta` den Zeiger jede Frame in die Bildschirmmitte zurück.
    `mouse_position` ist dann nur die Mitte (Fadenkreuz). Absolute Koordinaten
    für Picking gibt es nur im freien Modus (`Esc`).
14. **`GET_X_LPARAM` braucht `<windowsx.h>`.** Die Makros liegen nicht in
    `windows.h`. Ohne Include: `error: 'GET_X_LPARAM' was not declared`.
15. **Linksklick darf die Maus nicht mehr fangen.** Vorher fing `WM_LBUTTONDOWN`
    den Zeiger (Kameradrehung) — das würde Klicken im Menü/Editor unmöglich
    machen. Heute schaltet nur die mittlere Taste den Fang um, der Editor
    schaltet ihn selbst über `Rune::Input.capture =`.
16. **Endlose Methoden können keine Setter sein.** `def self.x=(v) = …` ist ein
    Syntaxfehler ("a setter method cannot be defined in an endless method
    definition"). Setter immer als normaler `def … end` schreiben.
17. **`boot.rb` lädt am Ende `game/main.rb`.** Ein kopfloser Test darf deshalb
    nicht `require 'boot'` benutzen, sondern nur die einzelnen Dateien
    (`data`, `pick`, `prefabs`, `editor`) plus Stubs für `Rune::*` und `Game`.
18. **Rechtsklick löscht nur Editor-Objekte.** `Editor.object_index` akzeptiert
    nur Namen mit Präfix `ed_`. Andere Knoten bleiben
    unberührt — sonst würde ein Klick auf Mara den halben Wald abräumen.
19. **Dünne Prefab-Teile verfehlt der Strahl leicht.** Zaunlatte (0.13 m) oder
    Pfahl (0.2 m) trifft ein Bodenstrahl oft daneben — der Rechtsklick würde
    ins Leere gehen. Deshalb löscht der Editor im Fallback das Objekt, dessen
    Mitte dem Bodenpunkt am nächsten liegt (`Editor.delete_near`, Radius aus
    `Prefabs.radius_of`).
20. **Konstantpuffer gilt pro Shader-Stufe.** `gSun` stand im FrameCB, aber der
    Pixel-Shader bekam b0 nie gebunden (`PSSetConstantBuffers(0, ...)` fehlte)
    — die ganze Szene war schlagig schwarz, obwohl die Geometrie stimmte.
21. **Sonne entgegen der Blickrichtung = alles im Schatten.** Die Kamera schaut
    nach Norden (-Z); mit einer Sonne bei dz < 0 lag jede Südwand (die
    kamerazugewandte!) im Schatten. Sonne jetzt bei (0.40, 0.75, 0.50).
22. **Flache Deko-Boxen versinken im Gelände.** Das Wegstück lag 0.02 m über
    der Höhe seines Mittelpunkts, aber das Dreiecksnetz interpoliert — an den
    Rändern lugte Gras durch. Versatz jetzt 0.08 m.
23. **`WM_CHAR` kommt beliebig spät.** Bei per `PostMessage` injizierten
    Tastendrücken lag das Zeichen bis zu 5 Frames *nach* dem `WM_KEYDOWN` im
    Puffer — ein textbasierter Eingabedialog frisst dann die Auslösetaste
    ("nMeine Welt"). Texteingaben deshalb frame-basiert über
    `Rune::Input.pressed?(zeichen)` sammeln, nicht über `WM_CHAR`.
24. **`$project_root` zeigt auf den `data`-Ordner**, nicht auf den
    Projektordner (`Project.data_path`, nicht `Project.path`). Sonst sucht
    `Database.map_ids` in `projekte/<name>/maps` und findet nichts — der
    Fehler zeigt sich als "create_map liefert wieder welt01".
25. **`UI.frame` füllt die Fläche, es ist kein Rahmen.** Intern ruft es nur
    `panel` mit gleicher Farbe — ein "Rahmen" um einen Dialog füllt also den
    ganzen Kasten (der Löschdialog war rot). Dünne Rahmen selbst bauen:
    vier `Rune::Gfx.rect`-Streifen (`Launcher.rahmen`).
26. **Tiefenkarten müssen TYPELESS angelegt werden.** `D32_FLOAT` mit
    `BIND_DEPTH_STENCIL | BIND_SHADER_RESOURCE` gibt `E_INVALIDARG`. Richtig:
    Textur als `R32_TYPELESS`, DSV-View als `D32_FLOAT`, SRV-View als
    `R32_FLOAT`.
27. **Matrizen-Reihenfolge wie bei der Kamera:** `mul(view, proj)` — nicht
    `mul(proj, view)`. Der Licht-VP folgt demselben Muster wie `gViewProj`.
28. **Schatten brauchen eine zur Kamera passende Sonne.** Steht die Sonne
    hinter der Kamera, fallen alle Schatten hinter die Objekte — der
    Schatten-Pass arbeitet korrekt, aber im Bild ist nichts zu sehen.
29. **Test-Attrappen müssen die echte API treffen und auch benutzt werden.**
    Der Sound-Test scheiterte doppelt an sich selbst: erst referenzierte er
    `Sounds::REG` statt `Rune::Sound::REG` (die Attrappe hängt unter
    `Rune::`), dann rief er `Sounds.register_all` nie auf und wundernte sich
    über leere Registrierung. Erst minimal reproduzieren (Stand-alone-
    Skript), dann den Test fixen.
30. **Der Launcher lädt das PROJEKT, nicht `game/data`.** Änderungen an
    `game/data/maps/dorf.json` wirken im Spiel erst nach dem Neuanlegen des
    Demo-Projekts — `projekte/dorf/data/maps/dorf.json` ist eine Kopie.
    Live-Tests mit Event-Änderungen müssen die Projektdatei editieren
    (oder das Projekt löschen, `ensure_demo` kopiert neu).
31. **Action-Events nehmen das ERSTE in Reichweite, nicht das nächste.**
    Die Schleife über `@events` bricht beim ersten `near?`-Treffer ab —
    Reihenfolge in der JSON-Datei entscheidet, nicht die Distanz.
32. **PowerShell-JSON-Roundtrip zerstört Textzeilen.** `ConvertFrom-Json`
    + `ConvertTo-Json` verwandelt `"lines": ["A", "B"]` in einen einzigen
    String "A B". JSON-Dateien deshalb immer mit Ruby
    (`JSON.parse`/`JSON.pretty_generate`) editieren.
33. **waveOut-Startverzögerung = Menge vorgequeuer Stille.** Ein neuer
    Sound wird erst in den nächsten neu gefüllten Puffer gemischt und
    hört sich also frühestens nach dem Rest der Queue. Mit 4 x 8192
    Samples (~740 ms Vorlauf) wirkte jeder Klick träg. Kleine Puffer
    (3 x 1024 = max ~70 ms) machen den Start sofort hörbar — bei Frame-
    Hengstern kann es dafür kurz knacken, das ist der Kompromiss.
34. **Der Launcher-Kamera-Abstand lebt im Spiel weiter.** Der Launcher
    stellt `Rune::Camera.distance = 28` für die Weltübersicht; beim
    Spielstart wurde das nie zurückgesetzt — die Third-Person-Kamera
    schwebte 28 m über dem Dorf und jeder Kollisions-Clamp griff viel zu
    spät. `Game.setup` setzt jetzt 7 m.
35. **Der Kamera-Strahl trifft die gegnerische Seite.** Die Kamera ist
    immer HINTER dem Spieler — ein vorn liegender Zaun blockt sie nie.
    Kamera-Kollision greift nur, wenn der Spieler rückwärts in etwas
    läuft, an einem Hindernis steht und dreht, oder bergauf schaut. Tests
    müssen diese Situationen gezielt aufbauen (Mausdrehung per
    `SetCursorPos` vom Fensterzentrum aus injizieren — der Engine-Reset
    erzeugt pro Aufruf genau ein Delta).
36. **`std::snprintf` neben Ruby-Headern bricht.** `ruby/subst.h` macht
    `snprintf` zum Makro (`ruby_snprintf`); `std::snprintf` wird dann zu
    `std::ruby_snprintf` und findet keine Funktion. In Ruby-nahen C++-
    Dateien (`vm.cpp`) deshalb `snprintf` ohne `std::` rufen.
37. **Projektkopien veralten.** `ensure_demo` legt `projekte/dorf` nur bei
    Neuanlegen aus `game/data` kopien an. Neue Kartenfelder (z. B. der
    `"npcs"`-Block aus Etappe 13) fehlen in älteren Projektkopien
    weiterhin — das Spiel läuft ohne NPCs und niemand meldet einen
    Fehler. Nach Engine-Erweiterungen am Kartenformat die Projektkopien
    prüfen oder das Projekt neu anlegen.

38. **Einfrieren in der Luft.** Wird die Simulation pausiert
    (`Simulation.paused`), während der Spieler springt oder nach einem
    `enter_map` noch nicht gelandet ist, bleibt er eingefroren schweben
    (`Boden=false`, y>0) und die Kamera hängt in der Höhe. Vor dem
    Pausieren prüfen: erst `Rune::Player.on_ground?` abwarten, dann
    `paused = true` setzen.

39. **Editor schreibt ein anderes JSON-Format als Ruby.** Schreibt der
    Editor kompakt (Arrays in einer Zeile) und Ruby pretty (alles
    mehrzeilig), flippt das Format bei jedem Speicherwechsel und die
    Versionsverwaltung zeigt jede Karte als komplett geändert. Der
    Editor-Writer mimt deshalb exakt `JSON.pretty_generate`
    (2 Leerzeichen, mehrzeilige Arrays, Ganzzahl ohne Dezimalpunkt,
    Kommazahl immer mit `.0`).
40. **GetAsyncKeyState sieht gepostete Nachrichten nicht.** Tests, die
    Tasten per `PostMessage` injizieren (wie beim Spiel), erreichen
    `GetAsyncKeyState` im Editor nicht — für Tastatur-Injection dort
    `keybd_event` verwenden (setzt den globalen Async-Status). Für
    Mausklicks funktioniert `mouse_event` nach `SetCursorPos`;
    `SendInput` mit handgebautem INPUT-Struct versagt stumm, wenn die
    Struct-Größe nicht exakt 40 Bytes (x64) ist.
41. **Modaler Dialog blockiert den Render-Loop.** Eine eigene
    GetMessage-Schleife (modales Warten) stoppt die Render-Hauptschleife
    des Editors — der Viewport zeigt schwarz, weil das DXGI flip-model
    Frames nicht zurückbehält. Lösung: Eigenschaften-/Neue-Karte-Dialog
    sind **moduslos**; die Hauptschleife rendert weiter, während
    `view_proc`, `handle_shortcuts` und WM_COMMAND Eingaben ignorieren,
    solange `g.props_dlg` offen ist. (Für Test-Injection in Edit-Felder:
    cross-process `SetFocus` ist blockiert — Felder direkt per
    `WM_SETTEXT` füllen.)

## Doppelklick-Deployment (Nacharbeit zu 18e)

Vorher starteten beide EXEn nur, wenn der MSYS2/Ruby-PATH gesetzt war —
per Explorer-Doppelklick kam „libwinpthread-1.dll nicht gefunden" bzw.
Aktivierungsfehler 0xc0150002. Drei Ursachen, drei Fixes:

**1. libwinpthread statisch:** Der g++-Driver hängt HINTER den
Nutzer-Flags `-lpthread` (dynamisch) an; ein einfaches `-Bstatic`
vor `-lwinpthread` wirkt deshalb nicht. Lösung in build.ps1:
`-Wl,-Bstatic,--whole-archive -lwinpthread -Wl,--no-whole-archive,-Bdynamic`
— whole-archive erzwingt die statische libwinpthread.a, egal was
danach kommt. Beide EXEn importieren die DLL nicht mehr.

**2. XCopy-DLLs:** Die Ruby-DLL (x64-ucrt-ruby340.dll) importiert
libgmp-10.dll und libwinpthread-1.dll und fordert per eingebettetem
Manifest die private SxS-Assembly „ruby_builtin_dlls" an. build.ps1
kopiert deshalb diese drei Dateien plus den Ordner
`ruby_builtin_dlls` (Manifest + DLLs, liegt in C:\Ruby34-x64\bin)
nach build/. Fehlt der Ordner: Fehler 0xc0150002 (Ereignisprotokoll
SideBySide, Quelle der Diagnose).

**3. RUBYLIB (historischer Stand 18e+):** Die Ruby-DLL berechnete ihr
lib-Präfix relativ zu `build/`; der damalige Fallback setzte `RUBYLIB` auf
feste Pfade unter `C:/Ruby34-x64`. Das war bei anderen Installationen oder
vorgegebenem `RUBYLIB` unzuverlässig und löste erneut `LoadError: json` aus.
Seit 19x wird die passende Bibliothek mitgeliefert und bevorzugt; Details
und Grenzen stehen im aktuellen Abschnitt „Mitgelieferte Ruby-Laufzeit“.

**Tauefalle für manülle Link-Tests:** g++/gcc ohne den Toolchain-PATH
schlagen mit Exit 1 und VOLLKOMMEN leerer Ausgabe fehl (cc1.exe kann
nicht starten — Loader-Fehler, der auf stderr nicht ankommt). Immer
vorher `$env:PATH = "C:\Ruby34-x64\msys64\ucrt64\bin;..."` setzen.
PowerShell-Array-Parameter mit `-Wl,a,b` sind ok, inline nach
`& $cmd -flag` wird an Kommas zerpflückt („Argument in der
Parameterliste fehlt").

**Verifiziert:** Doppelklick-Simulation ohne Ruby-PATH (PATH=
C:\Windows\System32;C:\Windows): Editor-Fenster steht (Titel
„RuneEditor - dorf - Dorf"), Spiel rendert (79 Draws, 0 Fehler), mit
Ruby-PATH alle 13 Tests + live18e/18d/18c grün.


## Fullbild-Fix: schwarze Ränder beim Maximieren (Nacharbeit 18j)

**Symptom (User-Report):** Im Vollbild/Maximieren blieben neu aufgedeckte
Stellen des Hauptfensters schwarz bzw. zeigten alten Inhalt.

**Ursache:** Die Fensterklasse `RuneEditorMain` hatte **keinen
Hintergrund-Pinsel** (`hbrBackground == null`). Windows löscht den
Fensterhintergrund ohne Pinsel nie - beim Vergrößern neu freigelegte
Bereiche (Ränder zwischen den Controls) werden von keinem Control
bemalt und zeigten Müll. Alle Dialog-Klassen des Editors hatten schon
`COLOR_BTNFACE`, nur das Hauptfenster nicht.

**Fix:** `wc.hbrBackground = GetSysColorBrush(COLOR_BTNFACE)` bei der
Registrierung von `RuneEditorMain`.

**Geprüft:** log/live_fullscreen.ps1 (9 Checks: SC_MAXIMIZE/
SC_RESTORE programmatisch, Viewport-Log 937x828 -> 1300x937, Prozess
lebt, kein [ERROR]). Visülle Verifikation durch User im Vordergrund-
Fenster (Drossel-Falle aus 18j gilt nicht für echtes Maximieren).

## Kartenbaum wie RPG Maker XP (Etappe 18k)

Historischer Stand; seit 19w liegt der Kartenbaum unter der Palette.

Der Editor bekommt links oben eine **Kartenliste als Baum** (TreeView) -
das zentrale Navigationselement des RPG Maker XP:

**Aufbau:** Wurzel = Projektname, darunter alle Karten des Projekts
(Item-LPARAM = Karten-Index). `maptree_fill()` pflegt den Baum parallel
zum Karten-Menü (`rebuild_map_menu` ruft es), `load_map` synchronisiert
ihn nach dem Laden; die aktuelle Karte ist markiert
(`TVS_SHOWSELALWAYS`, sichtbar auch ohne Fokus).

**Bedienung (wie XP):**
- Einzelklick auf eine Karte lädt sie (die schmutzige Karte wird vorher
  gesichert wie beim Kartenwechsel im Menü).
- Doppelklick öffnet die Karten-Eigenschaften (F2-Dialog).
- Rechtsklick: Kontextmenü Eigenschaften / Umbenennen / Neue Karte /
  Löschen. Umbenennen startet das **Inplace-Edit** im Baum
  (TVS_EDITLABELS); der neue Name landet in `map_root["name"]` (nur die
  aktuell geladene Karte), die Karte wird als geändert markiert -
  Strg+S speichert wie üblich. Leerer Name verwirft das Edit.
- Die Wurzel (Projekt) ist nicht umbenennbar (BEGINLABELEDIT blockiert).

**Bedienblock:** Die linke Spalte ist jetzt zweigeteilt - oben der
Kartenbaum (feste 150 px inkl. KARTEN-Label), darunter die Werkzeuge
(18i-Logik unverändert: Liste nimmt freien Platz, Block folgt).
Mindestfensterhöhe bleibt 640 px; die Werkzeugliste wird dadurch etwas
kürzer (Viewort bleibt >= 80 px).

**Shortcut-Schutz:** Während des Label-Edits sind die globalen
Shortcuts abgeschaltet (Entf würde sonst Objekte löschen, R drehen) -
`handle_shortcuts` prüft `TreeView_GetEditControl`, dasselbe Muster
wie bei den moduslosen Dialogen.

** IDs:** IDC_MAPTREE 3027, IDC_LBL_MAPS 3028, Kontextmenü
IDM_TREE_PROPS/NEW/RENAME/DELETE 3110-3113. TreeView-Klassen sind in
InitCommonControlsEx ergänzt (ICC_TREEVIEW_CLASSES).

**Geprüft:** Live-Test log/live18k.ps1 (8 Checks: Control vorhanden,
Wurzel + Karten vorhanden, Auswahl markiert, kein Edit offen, kein
[ERROR], Prozess lebt), 16 headless Tests grün. Umbenennen per
Inplace-Edit ist im Live-Test nicht injizierbar (Edit-Control-Focus
über Prozessgrenzen unzuverlässig, bekannte Grenze wie 18a).

## Resize-Rendering und lesbares Editor-Log (Etappe 18j)

Zwei Fixes rund ums Fenster-Resizen und die Live-Tests:

**1. Rendern während des interaktiven Resizes:** Beim Ziehen der Fenster-
kante (oder Minimieren/Restore) läuft die modale Schleife von Windows -
die Render-Hauptschleife des Editors pausiert. Neue Flächen blieben
schwarz oder zeigten den Hintergrund (im Live-Test: das Spielfenster).
- `WM_ENTERSIZEMOVE` setzt einen 16-ms-Timer (`IDT_RESIZE`), dessen
  `WM_TIMER` `render_frame()` zieht und `g.resize_ticks` zählt.
- `WM_EXITSIZEMOVE` killt den Timer, legt Layout frisch an, rendert und
  protokolliert `Resize-Timer: N Frames waehrend des Ziehens gerendert`
  (Anker für Live-Tests).
- `WM_SIZE` rendert jetzt zusätzlich sofort einen frischen Frame
  (Reentrancy-Guard `in_size`), damit DWM nicht den falsch skalierten
  Altfrahmen zeigt.
- `Renderer::resize()` cleart RTV+DSV und präsentiert einmal direkt nach
  `ResizeBuffers` - der Backbuffer-Inhalt ist danach UNDEFINIERT, ohne
  Clear stand Müll im Bild.

**2. editor.log parallel lesbar (leicht gebrochene Live-Test-Kette):**
`core::log_open` nutzt jetzt `_wfsopen(..., _SH_DENYNO)` statt
`fopen_s("w")` - fopen_s sperrt exklusiv, weshalb kein Live-Test das Log
während des Editor-Laufs lesen konnte (alle alten Skripte mussten den
Prozess erst beenden; die Diagnose-Skripte dieser Etappe scheiterten
daran zweimal, bevor die Ursache im Log-Code stand).

**Live-Test-Wissen (wichtig für künftige Skripte):**
- WM_ENTERSIZEMOVE/WM_EXITSIZEMOVE einfach per PostMessage senden - das
  ist dieselbe Nachrichtenfolge wie beim echten Zug an der Fensterkante,
  komplett ohne Maus (kein pixelgenaues Treffen der Resize-Border, kein
  Klauen des Desktop-Cursors - vorherige Versuche mit SetCursorPos/
  mouse_event-Drag scheiterten serienweise an SetForegroundWindow-
  Einschränkungen und falschen Hit-Tests: SC_SIZE|HTBOTTOMRIGHT ist
  0xF011, nicht 0xF008 = HTMINBUTTON).
- Drossel-Falle: DWM drosselt Present für verdeckte Hintergrund-Fenster
  auf ~8 fps - der Timer liefert im Test also nur ~1 Frame pro 120 ms.
  Tests müssen >= 2 s zwischen ENTER- und EXIT-Nachricht warten, sonst
  zählt der Handler 0 Frames (gilt nur für Tests; real steht der
  Editor beim Ziehen im Vordergrund).
- Log dieser Läufe zeigt: Editor-Start, Timer-Protokoll und Beenden
  landen sauber in editor.log, das Log bleibt dabei lesbar.

**Geprüft:** Live-Test log/live18j.ps1 (6 Checks: Fenster gefunden,
simulierte Resize-Schleife überlebt, Timer protokolliert 47 Frames,
programmatisches Resize + Viewport-Log, kein [ERROR]), 16 headless
Tests grün, Build ohne Warnungen.

## Responsives Editor-Layout (Etappe 18i)

Historische Maße und Live-Ergebnisse; das aktuelle Layout ist in 19w beschrieben.

Problem (aus Screenshot gemeldet): Bei kleinem Fenster schoben sich
„Undo (Strg+Z)" / „Redo (Strg+Y)" in die Statusleiste, die Labels waren
in 96-px-Buttons abgeschnitten; bei grossen Fenstern blieb unter dem
Werkzeugblock viel Leerraum und die Objektliste war zu schmal.

Lösung in `layout_children()` (`src/editor/editor_main.cpp`):

- Spaltenbreiten aus der Fensterbreite, aber begrenzt:
  `left_w = clamp(W*16/100, 214, 280)`, `right_w = clamp(W*19/100, 258, 340)`.
  Bei 1024 px bleibt es bei den alten 214/258, bei 1800 px wachsen die
  Spalten auf 280/332 — der Viewport nimmt immer den Rest.
- Der Bedienblock der linken Spalte hat eine feste Höhe
  `7*30 + 6*6 = 246 px` und sitzt **unter** der Werkzeugliste. Die Liste
  bekommt vorher allen freien Platz (`list_h >= 120`), dadurch kann der
  Block nie in die Statusleiste ragen.
- Undo/Redo nutzen die volle Spaltenbreite (früher 2x96 px).
- Statusleisten-Felder proportional (`W/8`, `W/9`, `W/6`, Rest) statt
  fest 160/130/220.
- `move_child`/`tool_row`-Lambdas bündeln die SetWindowPos-Aufrufe;
  Achtung: Lambdas mit Parameter `h` kollidieren mit dem `HWND h` der
  Aufrufer — Parameter heissen `hwnd`/`ht`.
- `renderer_resize()` loggt jede Änderung als `Viewport: W x H`; das ist
  der Anker für Live-Tests (Layout und Swapchain müssen dieselbe Grösse
  zeigen).

Verifiziert (`log/live18i.ps1`, 18 Checks): 1024x640 -> Undo 198 px breit,
Beschriftung per WM_GETTEXT vollständig, Button-Unterkante == Statusleisten-
Oberkante (keine Überlappung); 1800x1040 -> Prefabliste 198->264,
Objektliste 242->322, Viewport 536->1166; Rückkehr auf 1280x800 ohne
Fehler; `editor.log` ohne ResizeBuffers-Fehler.
