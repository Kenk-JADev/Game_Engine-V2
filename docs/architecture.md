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

Verkabelt sind: Menü-Klick (Launcher), Schrittgeräusche nach Laufstrecke
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
Anhebung) via `Rune::Camera.info`. Der Launcher-Überblicksabstand (28 m)
gilt nur im Launcher — beim Spielstart stellt `Game.setup` auf 7 m.

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



## Scene-Editor: Editor-Engine und Playtest (Etappe 16)

Der Editor ist ein **Scene-Editor**: Er arbeitet ausschliesslich auf den
Kartendaten (JSON-Modell in `@objects`). Die 3D-Vorschau (Nodes mit
Präfix `ed_*`) ist reine Anzeige, kein Spielzustand. Events, NPCs,
Interpreter und GameState existieren im Editor-Modus nicht als
Spielobjekte — sie werden erst beim Playtest aus den Daten gebaut.

**Zwei Engines, eine Trennung:**

| Scene-Editor                        | Game-Engine (Playtest)            |
| ----------------------------------- | --------------------------------- |
| editiert Kartendaten                | interpretiert Kartendaten         |
| Simulation eingefroren              | Simulation läuft                 |
| Maus frei, UI klickbar              | Maus gefangen, WASD/Esc           |
| Änderungen landen im JSON          | Änderungen werden verworfen      |

**Einfrieren:** `Rune::Simulation.paused = true` stoppt in `main.cpp`
komplett `physics.update` (Bewegung, Schwerkraft). Ruby-seitig ruft der
Editor-Zweig `scene.update` nicht mehr auf — Animationen (z. B. der
Kristall) stehen, NPCs und Events werden nicht aktualisiert. Wichtig:
Ist der Spieler beim Umschalten noch in der Luft, läuft die Physik
erst weiter, bis `Player.on_ground?` erfüllt ist, und friert **dann**
ein — sonst schwebt der Spieler eingefroren in der Luft (Fallstrick 38).

**Playtest-Zyklus:** Der Button `▶ Testen` in der Tab-Leiste (oder F4
aus dem Editor) startet den Test: ungespeicherte Änderungen werden
zürst gesichert, `GameState` wird gesnapshottet, die Simulation
startet. F4 im Playtest springt zurück: `GameState.restore!`, Karte
frisch per `enter_map` laden, Editor an. Alles, was der Test am Spiel
geändert hat (gelaufene NPCs, geschaltete Events, Variablen), wird
verworfen — der Editierstand bleibt heilig.

**UI-Bausteine:** `UI.button` kennt neben `active:` und `danger:` jetzt
`ok:` (grün, für Positive-Aktionen wie den Test-Button). Buttons mit
gleicher Beschriftung brauchen eigene `id:`-Kennungen.

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
weiter (Fallback in boot.rb, launcher.rb, main.rb, Editor).

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

## Projekt-Scripts und Script-Editor (Etappe 17e)

**Datei:** Jedes Projekt darf `data/scripts.json` enthalten:

```json
{ "scripts": [ { "name": "gruss", "code": "say ['Hallo!']" } ] }
```

`Scripts.load_project` liest sie in `Game.setup` ein (also auch bei F5).
Ein Event ruft ein Script mit `{"cmd":"script","name":"gruss"}` auf;
`{"cmd":"script","code":"..."}` führt Code direkt aus. Der Interpreter
kennt dafür das neue Kommando `script` und `Interpreter#say(lines)`.

**Sandkasten:** Der Code läuft in `Scripts::Context#instance_eval`, nicht
im Interpreter. Helfer: `say` (öffnet das Textfenster und blockiert wie
`text`), `switch`, `set_switch`, `variable`, `set_variable`,
`add_variable`, `teleport(x, z, yaw:)`, `sound`, `effect`, `log`,
`player_position`, `game`. Fehler (auch Syntaxfehler) werden geloggt und
brechen die restliche Kommandoliste nicht ab.

**Editor:** `Scripts -> Script-Editor...` (`ID_MENU_SCRIPTS = 108`)
öffnet ein modusloses Fenster (`RuneEditorScriptDlg`) mit Script-Liste,
Namensfeld und grossem Ruby-Codefeld (Consolas). *Neu*, *Löschen*,
*Übernehmen*, *Speichern* (schreibt `projekte/<name>/data/scripts.json`)
und *Neu laden*. Beim Wechsel wird der alte Eintrag automatisch
übernommen; die Statuszeile zeigt Zielpfad und Änderungszustand.

**Event-Editor:** Der Befehlstyp *Script* trägt den Script-Namen in
`name`; die Befehlsliste zeigt `Script "gruss"`.

**Demo:** `game/data/scripts.json` (Vorlage) und
`projekte/dorf/data/scripts.json` enthalten `dorf_begruessung` und
`kristalle_zaehlen` als Beispiele.

**Geprüft:** `ruby -I game/engine tools/script_test.rb` (Schalter,
Variablen, say-Blockade, fehlerhaftes Script, Inline-Code, effect und
teleport) sowie alle acht bisherigen Tests grün.

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
formular arbeiten auf der gewählten Seite. Öffnet der Dialog ein altes
Event ohne `pages`, migriert `ev_ensure_pages` es sofort (Top-Level-Felder
werden zu Seite 0, Top-Level-Schlüssel fallen weg); gespeichert wird nur
das geöffnete Event migriert.

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

**Mindestfenster-Grösse:** WM_GETMINMAXINFO im Editor begrenzt das
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

**Editor-Werkzeugleiste:** Toolbar unter dem Menü mit 8 Buttons
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

**Playtest:** Extras -> Playtest starten... (F4) speichert die Karte
und startet `build\Rune3D.exe --project dorf` als eigenen Prozess.
Das Spiel wertet `--project <name>` aus (main.cpp setzt
RUNE_PROJECT), springt also ohne Titelbildschirm direkt ins Projekt
(game/main.rb: ENV['RUNE_PROJECT'] in Game.setup). Ein unbekannter
Projektname fällt auf den Titelbildschirm zurück (mit Warnung).

**Geprüft:** `ruby -I game/engine tools/weather_test.rb` (22 Checks),
alle 13 Tests grün, Live-Test log/live17h.ps1 (Toolbar, Zoom,
Wetter-Dialog schreibt JSON, Sound-Dialog, Playtest-Prozess +
engine.log-Vermerk, Aufräumen).

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

**Script-Sandbox** (`Scripts::Context`) kennt dafür: `party`,
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
Sandbox-Helfer - 39 Checks) sowie alle bisherigen Tests grün.


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
  Launcher erzeugen ohne Angabe Amplitude 0 — Berge entstehen durch Malen.

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
kaputt ist. Der Windows-Job ist der einzige Ort, an dem Win32/D3D11 wirklich
kompiliert und gelinkt wird; starten lassen sich die EXEs dort nicht (keine GPU
im Runner), grafische Prüfung bleibt bei `tools/edclick.psm1` am echten Rechner.

### Portabler Build

`build.ps1` suchte Ruby bisher nur unter `C:\Ruby34-x64` und rechnete fest mit
`include/ruby-3.4.0` und `-lx64-ucrt-ruby340`. Jetzt:

- **Ruby**: `-RubyRoot` > `$env:RUNE_RUBY_ROOT` > `ruby.exe` im PATH (via
  `RbConfig::CONFIG['prefix']`) > `C:\Ruby34-x64`/`33`/`32`.
- **Toolchain**: `$env:RUNE_MSYS_ROOT\ucrt64` > `<Ruby>\msys64\ucrt64` >
  `C:\msys64\ucrt64` > ein `ucrt64`-g++ aus dem PATH.
- **Header, libdir, Importbibliothek, DLL-Name** kommen aus `RbConfig`
  (`rubyhdrdir`, `rubyarchhdrdir`, `libdir`, `LIBRUBYARG_SHARED`,
  `RUBY_SO_NAME`), mit RubyInstaller-3.4-Fallback, falls die Abfrage leer ist.
- Neu: `-Test` = C++-JSON-Test + Ruby-Suite + JSON-Brücke. Fehlende
  Laufzeit-DLLs warnen nur noch statt den Build abzubrechen.

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
UTF-8-Namen wie „Hütte" wäre daraus in der Liste „HÃ¼tte" geworden (CP1252
liest die UTF-8-Bytes als Einzelzeichen). Alle 24 Textaufrufe in
`editor_main.cpp`, `main.cpp` und `ruby/vm.cpp` nutzen jetzt die W-Varianten
mit `utf8_to_wide`/`wide_to_utf8`; `ev_set_text` war bereits W-basiert. Damit
ist der Textpfad durchgängig Unicode: JSON (UTF-8) → Wide → Win32/GDI.

**PowerShell:** `build.ps1` trägt jetzt eine UTF-8-BOM, damit Windows
PowerShell 5.1 die Umlaute in Meldungen korrekt liest (PS 7/CI stört die BOM
nicht); `tools/edclick.psm1` hatte sie bereits. `ß` bleibt, wo es war — die
Umstellung führt kein ß neu ein (schweizerische Schreibung wie „grösser"
neben bestehendem „Löschen" ist bewusst akzeptiert).

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

`F4` Editor an/aus · im Editor: `1..9` Typ, Linksklick setzen, Rechtsklick
löschen, `R` drehen, `G` Raster, `N` neue Karte, `V`/`B` Karte zurück/vor,
`Strg+S` speichern ·
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
| Frei | sichtbar, echtes Koordinatenpaar | Position des echten Zeigers | Menü/Editor: hover + klicken |

Im gefangenen Modus steht `mouse_position` immer in der Bildschirmmitte
(das Fadenkreuz). `wm_mousemove` und `update_mouse_delta` pflegen die
Position nur im freien Modus.

**Klick-Aktion in `main.rb`:** Der Strahl wird pro Frame gegen alle sichtbaren
Szenenknoten **und** alle Events getestet. Getroffenes Objekt wird mit einem
dünnen Drahtkasten (12 Einzelboxen, es gibt noch keine Linien-Pipeline) und
einer Label-Zeile markiert. Linksklick: Events feuern sofort (auch ohne `E`),
Objekte schreiben Name/Größe/Abstand ins Log.

Test ohne Engine: `ruby -I game/engine tools/pick_test.rb` (19 Prüfungen).

## Editor (Etappe 5)

`F4` schaltet den Editor an: Mausfang aus, Palette links, Zielmarker (gelber
Drahtkasten) auf dem Boden unter dem Zeiger.

| Taste | Wirkung |
|---|---|
| `1` … `9` | Objekttyp aus der Palette wählen |
| Linksklick | Objekt auf den Boden setzen (Raster 1.0 m, `G` = 0.25 m) |
| Rechtsklick | Objekt unter dem Zeiger löschen |
| `R` | Drehung in 30-Grad-Schritten (für neue Objekte) |
| `Strg+S` | Karte speichern (`game/data/maps/<id>.json`) |

**Ablauf:** Der Strahl aus der Mausposition wird alle 0.4 m am Gelände
entlangmarschiert, bis er die Höhenfläche durchstößt (`Editor.terrain_hit`).
Der Treffer wird auf das Raster gerundet. Gespeichert wird nur die Objektliste;
die Geometrie steht in den Prefabs.

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
Beim Laden baut `Editor.rebuild` alle Objekte in die Szene ein und meldet die
Hindernisse neu an die Physik; sie sind also sofort begehbar/fest.

Das ganze Dorf ist Datenmaterial: Weg (18 Stücke), Zaun (7 Segmente),
Brunnen, sechs Häuser, drei Kristalle, vier Laternen liegen als 39 Objekte in
`game/data/maps/dorf.json`. `main.rb` baut nur noch die leere Szene — alles
    Andere kommt aus der Karte und ist im Editor änder- und löschbar.
Konvertiert wurde einmalig mit `tools/village_to_json.rb` (Positionen 1:1 aus
dem alten Code übernommen).

Test ohne Engine: `ruby -I game/engine tools/editor_test.rb` (26 Prüfungen).

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
`Editor.rebuild`; die Kamera kreist, indem der Spieler (unsichtbar) auf einem
Kreis um das Dorf teleportiert wird und `Camera.yaw` entgegen zeigt. Die Maus
ist im Menü frei (`Rune::Input.capture = false` — die Engine startet
gefangen!), Einträge per Hoch/Runter **oder** Mausklick wählbar.

Pro Projekt zeigt die Liste Titel, Kartenanzahl und "zuletzt gespielt"
(`Project.info`; `Project.touch` schreibt `last_played` in `system.json` beim
Start). `D` löscht das markierte Projekt nach Rückfrage (`Project.delete`).
Beim Spielstart fängt der Launcher die Maus wieder ein, damit der Mausblick
wie bisher sofort funktioniert.

Im **Editor** kamen Kartenbefehle dazu: `N` legt `welt02`, `welt03`, … an und
wechselt hin, `V`/`B` blättern zurück/vor (ungespeicherte Änderungen werden
dabei automatisch gesichert). `Game.change_map` sichert die schmutzige Karte,
bevor sie entladen wird.

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

**3. RUBYLIB in vm.cpp:** Die Ruby-DLL berechnet ihr lib-Präfix
relativ zum eigenen Speicherort. Liegt sie in build/, findet sie
stdlib und Gems ('json') nicht mehr — das Spiel startet dann ohne
Spielskripte („LoadError: json", 0 Draws). ruby::ruby::init() setzt
deshalb RUBYLIB auf die acht Installationspfade (site_ruby,
vendor_ruby, 3.4.0, x64-mingw-ucrt) — nur falls RUBYLIB nicht schon
gesetzt ist. ruby_init_loadpath() nimmt die Variable in $LOAD_PATH auf.

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
