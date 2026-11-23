# Windows-Live-Test: Materialien, Projektwechsel und D3D11

**Status:** Anleitung und Tests vorbereitet. Ein interaktiver Lauf mit Fenster,
DPI und Hardware-GPU ist damit **nicht** belegt; die Ergebnisse müssen auf dem
Zielrechner eingetragen werden. D3D11-WARP ist Software-Rendering und kein
Ersatz für den GPU-/Bedientest.

## 1. Isolierte Kopie erstellen

1. Windows mit interaktivem Desktop, D3D11-Hardware, Ruby 3.4 und MSYS2/UCRT64
   gemäß README verwenden. Im Repository bauen und die automatisierten Tests
   ausführen (PowerShell):

   ```powershell
   .\build.ps1 -Clean
   .\build.ps1 -Test
   .\build.ps1 -WarpTest
   ```

   `-Test` prüft unter anderem den WIC-Import in einem **eigenen temporären**
   Projekt und den Windows-Dateistore. `-WarpTest` rendert mit WARP in einem
   unsichtbaren Fenster, liest Pixel aus dem Backbuffer und prüft zusätzlich
   die echte eingebettete Ruby-Texturbrücke mit temporären A/B-Projekten.
   Der Brückentest verwendet den Load-Pfad der ausgewählten Ruby-Installation;
   das ist kein Test des installationsunabhängigen Deployments. Ein Fehlschlag
   oder nicht verfügbares WARP ist kein bestandener Test. Die EXE-Bauartefakte
   brauchen weiterhin die im README beschriebenen Ruby-Laufzeitkomponenten.
2. Nur eine **neue** Kopie außerhalb des Repositorys verwenden, z. B.:

   ```powershell
   ruby tools/prepare_material_live.rb "$env:TEMP\RuneMaterialLive"
   cd "$env:TEMP\RuneMaterialLive"
   ```

   Der Generator verweigert ein vorhandenes Ziel und verändert keine echten
   Projekte. Falls es existiert, einen anderen neuen Namen wählen; nicht blind
   löschen. Er kopiert Build und `game/` und legt nur `A_material`, `B_material`
   und `rot_blau.bmp` an. Vor dem Test ggf. die beiden `textures.json` und die
   RGBA-Dateien der Kopie separat sichern, damit jede Änderung überprüfbar ist.
3. Bei jedem Ergebnis notieren: Datum, Commit, Windows-Version, GPU/Treiber,
   Auflösung/DPI, Schritte, erwartete/tatsächliche Farben, relevante Logzeilen
   und Screenshotpfade. **Nicht** „bestanden“ eintragen, wenn ein Schritt nicht
   ausgeführt wurde.

## 2. Erwartete Testdaten

| Merkmal | A_material | B_material |
|---|---|---|
| Boden `gras` | Projekt-Override rot | eingebaute Basis grün |
| Splat/Objekt `gemeinsam` | gelb | blau |
| zweite Splat-Textur | `nur_a`, magenta | `nur_b`, cyan |
| Standard-Prefabtextur `wand` am linken Haus | Projekt-Override rot | eingebaute Wand |
| rechter Haus-Prefab | Objektmaterial `gemeinsam`, gelb | Objektmaterial `gemeinsam`, blau |

Die Testkarten sind flach und enthalten beide Splatflächen. Jede Materialdatei
enthält zudem ein unbekanntes Wurzelfeld, unbekannte Eintragsfelder, einen
präzisen Kachelwert und einen absichtlich ungültigen Pfadeintrag. Letzterer
muss auf der Platte bleiben, darf aber nicht als Textur geladen werden.

## 3. Editor: Wechsel, Atlas und Materialdatei

1. `build\RuneEditor.exe --project A_material` starten. F7 öffnen; `gras`,
   `gemeinsam` und `nur_a` ansehen. Die Farbvorschau darf Rot und Blau nicht
   vertauschen. Im Viewport Boden, beide Splatflächen und beide Häuser prüfen;
   bei Bedarf Kamera bewegen. F12 sichert den **Viewport** als BMP im Logordner.
2. Über **Datei → Projekt wechseln…** B öffnen. F3 wird beim Wechsel
   geschlossen; ein noch offenes F7 muss für das neue Projekt geschlossen und
   neu geöffnet werden. Erwartung: kein `nur_a`, kein rotes A-`gras` und keine
   rote A-`wand`; stattdessen eingebaute Basis, B-`gemeinsam` und `nur_b`.
   Beide Flächen und beide Häuser dürfen keine Zelle des alten Atlas zeigen.
   Zurück zu A wechseln und die Erwartungen aus Schritt 1 erneut prüfen.
3. Die Folge A→B→A mindestens zehnmal wiederholen. Prozessspeicher im
   Task-Manager beobachten: nach anfänglichem Aufwärmen darf er nicht bei jedem
   Wechsel unbegrenzt steigen. Das ist eine Beobachtung, kein präziser
   GPU-Leak-Nachweis. F7 nach jedem Wechsel neu öffnen; alte Entwürfe dürfen
   keine Karte des neuen Projekts verändern.
4. In A F3 öffnen, `gemeinsam` auswählen, die Kachel absichtlich ändern und
   speichern. `textures.json` semantisch vergleichen: nur der gewählte Wert
   darf sich ändern; `zukunft`, `unbekannt`, der ungültige Eintrag und alle
   anderen Materialien müssen erhalten bleiben. Ein unveränderter Wert darf
   nicht ungewollt gerundet werden. Ein Material in F3 nach Bestätigung
   entfernen: nur Metadaten verschwinden, seine `.rgba` bleibt liegen und der
   Terrain-/Atlas-Boden wird neu aufgebaut. Die Kopie für weitere Tests ggf.
   neu anlegen.
5. F3 → Import: `rot_blau.bmp` wählen. F7-Vorschau und RGBA-Datei prüfen:
   oberste Zeile rot, unterste blau. Bereits belegten Dateinamen und einen
   vorhandenen Materialnamen testen: fremde RGBA-Dateien bleiben erhalten;
   bestehende Metadaten dürfen nicht still überschrieben werden. In F7 eine
   Kachel unter bestehendem Namen erzeugen und anschließend speichern: beide
   Ersetzungen erfordern Bestätigung, bei Abbruch bleiben Dateien unverändert;
   bei Bestätigung bleibt die alte RGBA-Datei als Sicherung erhalten.
6. Externes Konfliktszenario nur in der Testkopie: Editor mit A offen lassen,
   `A_material/data/textures.json` extern ändern und danach im Editor F3/F7
   speichern. Erwartung: Konfliktmeldung, keine überschriebenen externen Bytes,
   keine neue unreferenzierte RGBA-Datei. Editor schließen oder auf ein anderes
   Projekt und zurück wechseln, um den Katalog neu zu laden. Wiederholen mit
   ungültigem JSON: Import/Speichern gesperrt, Originalbytes unverändert.
7. Testprotokoll, `log/editor.log` und F12-Screenshots sichern. Die automatisierte
   Windows-Dateitest-Suite prüft daneben Junctions/Hardlinks, gesperrte Dateien,
   einen erzwungenen Namenskonflikt und den Wiederherstellungsnamen.

## 4. Spiel: F5 und A→B→A

1. `build\Rune3D.exe` **ohne** `--project` starten. Im Launcher A wählen.
   Boden, Splatflächen und Häuser wie in der Tabelle prüfen. Die Logdatei ist
   `log/engine.log`; `log/issues.json` zeigt gemeldete Materialprobleme.
2. Im **Spiel** F5 mehrmals drücken (hier Skript-/Projekt-Neuladen; im Editor
   ist F5 dagegen der Platzierdialog). Visuell müssen A und seine Atlaszellen
   gleich bleiben. Keine neuen Fehlermeldungen oder stetiges Speicherwachstum.
3. Esc → **Zum Titelbildschirm**, dort B wählen. Die Kulisse im Launcher muss
   wieder die eingebaute Demo ohne A-Overrides zeigen. In B die Werte aus der
   Tabelle prüfen; dann über den Titelbildschirm wieder A wählen und erneut
   vergleichen. F5 in beiden Projekten wiederholen.
4. Optional F4-Playtest aus dem Editor separat prüfen; Änderungen an Karte und
   Testprojekt nur innerhalb der Kopie machen. Window-Resize und DPI-Wechsel
   sind zusätzliche GUI-Fälle und ersetzen keinen Farbabgleich.

## 5. Grenzen und Wiederherstellung

* Dateipfade der Materialdaten werden unter dem gewählten Engine-Anker einzeln
  über Windows-Handles geöffnet; Junctions/Symlinks und mehrfach verlinkte
  Dateien werden abgelehnt. Das vom Benutzer ausgewählte Importbild darf
  außerhalb des Projekts liegen. Andere Datenpfade (Karten, Datenbank usw.)
  sind hierdurch **nicht** automatisch abgesichert.
* Der Speicher-Commit prüft alte Bytes und verwendet Sicherung plus Rename
  ohne Überschreiben. Zwischen den Renames kann der Zielname kurz fehlen;
  Stromausfall/Absturz kann eine `.rune-*.bak` hinterlassen. Bei einem
  konkurrierenden Ziel die angezeigte Sicherung erhalten und nach Prüfung
  manuell wiederherstellen; niemals automatisch fremde Dateien löschen.
* Der Editor verwendet für einige bestehende Projekt-/Installationspfade noch
  Win32-ANSI-Aufrufe. Unicode-Projekt-/Installationspfade außerhalb der aktiven
  Windows-Codepage sind **kein** allgemein zugesagter GUI-Fall; der kopflose
  Import nimmt seine Argumente dagegen per UTF-16 entgegen.
* Nach Abschluss alle Programme der Kopie schließen. Testkopie erst nach
  Sicherung/Prüfung entfernen; niemals das Original-Repository bereinigen.
