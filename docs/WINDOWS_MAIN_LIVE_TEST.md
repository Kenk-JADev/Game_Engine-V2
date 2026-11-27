# Windows-Live-Test: RuneEditor-Hauptfenster und Karten

**Status: offen.** Dies ist eine Anleitung, kein ausgeführter Test. Die
portablen Prüfungen und unsichtbaren Win32-Controls prüfen weder die
sichtbare Gesamtoberfläche noch eine Hardware-GPU oder einen echten
Monitorwechsel. Nicht ausgeführte Schritte ausdrücklich offen lassen.

## Vorbereitung: isolierte Kopie

1. Auf Windows 10/11 mit interaktivem Desktop, Direct3D-11-Hardware und den
   Werkzeugen aus dem README bauen und die automatisierten Tests ausführen:

   ```powershell
   .\build.ps1 -Clean
   .\build.ps1 -Test
   .\build.ps1 -WarpTest
   ```

   Datum, Commit, Ergebnisse, Windows-Version, GPU/Treiber, Auflösung und
   Monitor-DPI notieren. WARP ist Software und kein Hardware-Live-Test.
2. Im Repository eine **neue** Kopie außerhalb des Repos erstellen:

   ```powershell
   ruby tools/prepare_material_live.rb "$env:TEMP\RuneMainLive"
   if ($LASTEXITCODE -ne 0) { throw 'Vorbereitung fehlgeschlagen; Teilkopie prüfen' }
   cd "$env:TEMP\RuneMainLive"
   $map = Join-Path (Get-Location) 'projekte\A_material\data\maps\dorf.json'
   ruby -rjson -e "p=ARGV.fetch(0); d=JSON.parse(File.read(p, encoding:'UTF-8')); d['zukunft']={'gross'=>9007199254740993}; File.write(p, JSON.pretty_generate(d)+10.chr, encoding:'UTF-8')" $map
   if ($LASTEXITCODE -ne 0) { throw 'Testkarte nicht vorbereitet' }
   ```

   Das Werkzeug verweigert ein vorhandenes Ziel. Bei Bedarf einen anderen
   neuen Namen wählen; niemals Originalprojekte für diesen Test verwenden.
   Es kopiert Build und `game/` und erstellt `A_material` und `B_material`.
   Das fremde `zukunft`-Feld prüft die Erhaltung unbekannter Daten.
3. `build\RuneEditor.exe --project A_material` in der Kopie starten. Die
   erzeugte Startkarte heißt im Baum zunächst `A_material`; sie enthält zwei
   Häuser. Nur diese Kopie verändern. F4 ist keine Sandbox und verwendet die
   normalen Dateien des gewählten Testprojekts.

## Fenster, Werkzeuge und 3D

1. Menü, gruppierte Icons und Tooltips prüfen: keine leeren/falschen Icons;
   jeder sichtbare Knopf führt zur benannten bestehenden Funktion. Neue-Karte-
   und Eigenschaften-Dialog zunächst mit **Abbrechen** schließen. Links muss
   wie im RPG Maker XP die Palette oben und der Kartenbaum unten liegen
   (Ziehen der Trennleiste nach oben vergrößert den Baum), rechts Objektliste und Formular,
   dazwischen die echte 3D-Ansicht und unten die Statusleiste.
   Etappe 34: Boden malen/Höhe malen mit gedrückter Maus über die Karte
   ziehen = durchgehender Strich, ein Strg+Z nimmt den ganzen Strich zurück;
   Ring (Höhe) bzw. Rahmen (Boden) und Raster liegen auf dem Gelände, Shift
   färbt Radierer rot / Senke blau. Event/NPC im Modus Events ziehen; danach
   Doppelklick öffnet weiterhin. Objekt wählen, in die 3D-Ansicht klicken,
   Pfeiltasten schieben es. Mauszeiger: Fadenkreuz beim Malen, Kreuzpfeil
   beim Verschieben. Flüssig auch auf großen Karten?
   Etappe 33: Modus Events (blauer Würfel): Klick neben ein Event setzt
   kein Objekt; NPC/Event schnell doppelklicken öffnet F5 bzw. Event-Editor;
   Doppelklick auf freien Boden legt „Event N“ an genau dieser Stelle an
   (Strg+Z nimmt es zurück). Kartenbaum: Einfachklick wechselt die Karte.
   Etappe 32: Knöpfe, Radios, Listen und Scrollleisten im aktuellen
   Windows-Stil (nicht Windows 2000). Keine Geistertexte/schwarzen Streifen
   in Palette und Inspektor nach Größenänderung, Scrollen und Auswahl.
   Über dem Kartenbaum nur die Überschrift; Rechtsklick öffnet das
   Kartenmenü. Undo/Redo als Pfeile in der Toolbar.
   Etappe 31: Toolbar-Icons ohne weiße Kästchen (transparent, auch bei
   125/150/200 %); Modus-Gruppe (Hügel/Kiste/blauer Würfel) rastet ein und
   folgt Menü „Modus“ und Palettenwerkzeug.
   Etappe 30: Menü „Modus“ wechselt Gelände (Boden malen) / Objekte
   (Setzen) / Events (Auswahl + F5/F6-Hinweis); der Haken folgt auch beim
   Klick auf ein Palettenwerkzeug. „Maßstab“ 1/1/1/2/1/4 hebt bzw. senkt die
   Kamera an derselben Stelle. Ansicht-Menü ohne graue Hinweiszeilen,
   Hinweise unter F1.
   Etappe 27a: Ohne Auswahl heißt die rechte Gruppe „Karte: <Name>“ und
   zeigt Objekte/Events/NPCs-Zahlen samt F2/F5-Hinweis; nach Klick auf ein
   Objekt „Objekt <n>: <Name>“ mit den bisherigen Feldern und Hinweisen.
2. Trennleiste mit der Maus in beide Richtungen bis zur Grenze ziehen. Baum
   und Palette dürfen nicht verschwinden oder die Nachbarn überdecken.
   Dasselbe über **Ansicht** ohne Maus ausführen und zurücksetzen. Die
   Position muss Größenänderungen überstehen; nach Neustart ist derzeit
   wieder der Standard zu erwarten, nicht die letzte Position.
3. Fenster bis zum angebotenen Minimum verkleinern, maximieren, wiederherstellen
   und ggf. auf einen kleineren Monitor ziehen. Palette und Inspektor mit
   Scrollleiste und Mausrad über freier Panelfläche erreichen. Die Objekt- und
   Prefablisten dürfen ihr eigenes Mausradverhalten behalten. Per Tab/Shift+Tab
   und Pfeiltasten auch zu vorher verdeckten Controls navigieren; Fokus muss
   sichtbar werden. Auf sehr schmaler Fläche kann **Eigenschaften** als **F2**
   erscheinen. Keine verdeckten oder überlappenden Bedienelemente erwarten.
4. Bei 100 %, 125 %, 150 % und 200 % Windows-Skalierung sowie, falls vorhanden,
   beim Wechsel zwischen zwei unterschiedlich skalierten Monitoren prüfen:
   Schrift, Icons, Splitter, Baum, Listen und 3D-Fläche passen ins Fenster;
   Kamerabild bleibt nach Resize/Maximieren sichtbar und unverzerrt. Nicht
   verfügbare DPI-/Monitorfälle separat als offen vermerken.
5. Ein Haus über die Liste oder den Viewport auswählen, im rechten Formular X
   ändern und **Übernehmen**. Durch Scrollen/Verkleinern darf ein noch nicht
   übernommener Text oder die Textmarkierung nicht verloren gehen. Setzen,
   Verschieben, Drehen, Duplizieren, Löschen sowie Objekt-Undo/Redo und
   Strg+S in der Kopie probieren. Das Undo umfasst nur Objekte, nicht Böden
   oder Events. Kamera, Mausrad und 3D-Darstellung bleiben benutzbar.

## Karten und Schutz bei Wechseln

1. Über **Neu** eine zweite Karte `Zweitkarte` mit den vorgeschlagenen Maßen
   anlegen. Beide Einträge müssen im Baum stehen und unterschiedliche Inhalte
   zeigen. Zwischen ihnen wechseln; Doppelklick und der Eigenschaftenknopf
   müssen jeweils die gewählte Karte öffnen. Dialog schließen, bevor der
   Kartenbaum erneut bedient wird.
2. Auf der Startkarte eine erkennbare Objektänderung machen, aber zunächst
   nicht speichern. Die **andere Karte rechtsklicken** und bei der Rückfrage
   zum Speichern **Abbrechen** wählen. Erwartung: alte Karte und Änderung
   bleiben aktiv, die alte Auswahl wird wiederhergestellt; es darf kein Menü
   mit Aktionen für die falsche Karte aufgehen.
3. Den Rechtsklick auf die andere Karte wiederholen und das Speichern der
   alten Karte bestätigen. Nach erfolgreichem Wechsel soll das Kontextmenü
   für die tatsächlich getroffene Karte erscheinen. Dessen Eigenschaften
   öffnen und Kartenname prüfen. Ebenso einen unübernommenen Objektfeld-
   Entwurf beim Kartenwechsel abbrechen: Text und alte Karte bleiben erhalten.
   Optional vor dem Löschen auf beiden Karten F4 starten: Die jeweils gewählte
   Karte muss im separaten Spielprozess erscheinen. Den Spielprozess schließen;
   der Editor bleibt geöffnet.
4. In der Kopie Umbenennen und Löschen zuerst abbrechen, dann ausdrücklich
   bestätigen. Nur die gewählte Karte darf geändert bzw. entfernt werden;
   beim Löschen auf Hinweise zu ungespeicherten und externen Änderungen
   achten. Auf der Projektwurzel darf keine Karteigenschaft/Löschung aktiv
   sein. Strg+S und anschließendes Neuladen prüfen.
5. Nach dem Speichern der Startkarte im **anderen** PowerShell-Fenster den
   unbekannten Wert der Testkopie prüfen:

   ```powershell
   ruby -rjson -e "d=JSON.parse(File.read(ARGV.fetch(0), encoding:'UTF-8')); abort 'Testfeld verloren oder verändert' unless d.dig('zukunft','gross')==9007199254740993; puts 'Testfeld erhalten'" "$env:TEMP\RuneMainLive\projekte\A_material\data\maps\dorf.json"
   ```

   Bestehende F5/F6/F7/F9/F11-Funktionen nur als Regression prüfen, nicht
   als neue 19w-Implementierung ausgeben.

## 19y: Versionsname, NPC/Event-Marker und Spielerstart

Auch hier ausschließlich die oben angelegte Kopie verwenden. Die
Testprojekte enthalten anfangs keine NPCs/Events. In `A_material` per F5
einen NPC und per F6 zwei Events an gut sichtbaren, getrennten Positionen
anlegen; jeweils übernehmen und die Karte mit Strg+S speichern.

1. In den Eigenschaften von `build\RuneEditor.exe` die Dateibeschreibung
   **Rune3D Editor**, für `build\Rune3D.exe` **Rune3D Spiel** und als Produkt
   **Rune3D** prüfen. Den gestarteten Editor zusätzlich im Task-Manager
   ansehen und die tatsächliche Anzeige notieren. Das ist ein neuer Build;
   alte EXEs/Windows-Caches können weiterhin den alten Text anzeigen.
2. Jeden NPC-/Eventmarker einmal anklicken: Er soll ausgewählt sein, aber
   keinen Dialog öffnen. Danach zweimal schnell auf denselben Marker klicken:
   der passende NPC muss in F5 bzw. das passende Event in F6 ausgewählt
   sein. Langsame Klicks sowie schneller Wechsel zwischen zwei verschiedenen
   Markern dürfen keine falsche Bearbeitung öffnen. Auf verdeckte Marker,
   Pinsel-/Setzmodus, Maus-Capture und bestehendes Objektziehen achten.
3. Im geöffneten F6-Dialog eine Eingabe ändern, ohne zu übernehmen, und
   einen NPC doppelklicken: Abbrechen muss den Evententwurf und F6 erhalten;
   Nein verwirft nach Rückfrage, Ja übernimmt und öffnet anschließend den
   richtigen NPC. Den NPC-Dialog erst mit OK oder Abbrechen beenden.
4. Auf sichtbarem Gelände **kurz rechtsklicken** und „Spieler-Startposition
   hier setzen“ wählen; Rechtsklickziehen muss weiter die Kamera drehen und
   darf kein Menü öffnen. Das grüne Symbol zeigt den Start nur auf seiner
   Karte; G ändert die Rasterweite. Eine gesperrte Zone erzeugt eine Warnung.
   Der Projektstart wird sofort separat gespeichert und ist nicht im
   Karten-Undo; andere nicht gespeicherte Kartenänderungen bleiben separat.
5. In der Testkopie `projekte\A_material\data\system.json` prüfen: `start.map`,
   `x`, `z`, `use_project_position` stimmen; die bisherige `yaw` und fremde
   Felder bleiben erhalten. F4 auf dieser Karte startet dort; F4 auf einer
   anderen Karte verwendet deren Kartenstart. Im laufenden Spiel nach einer
   erneuten Startänderung F5 drücken: system.json wird erneut gelesen, die
   F4-Kartenauswahl bleibt bestehen. Kartenwechsel und Spielstand-Position
   dürfen nicht auf den Projektstart zurückspringen.

## 19y: F11-Ruby-/RGSS-Editor und Schutz

1. F11 öffnen. Liste, Name, Code, Zeilennummern und Cursoranzeige prüfen;
   einen langen Text mit vielen Zeilen eingeben, scrollen, auswählen, Tab und
   Umschalt+Tab sowie Rückgängig testen. Zeilennummern sollen beim vertikalen
   Scrollen und bei 100/125/150/200 % DPI zu den Codezeilen passen. Fenster
   verkleinern, maximieren und nach Möglichkeit auf einen anders skalierten
   Monitor verschieben; Text und Auswahl müssen erhalten bleiben.
2. Ein neues Script als Startscript markieren, z. B.
   `$game_variables[7] = 42` und `Rune::Log.info('RGSS-Starttest')` auf zwei
   Zeilen. Syntax prüfen: gültiger Code wird nicht ausgeführt; ungültiges
   Ruby soll einen Fehler mit Zeile melden. Mit Strg+S speichern und F4/F5
   starten; im Spiel-Log muss der Starttest erscheinen. Ein nicht markiertes
   Script darf nicht automatisch starten. Die RGSS-Schicht ist nur teilweise
   XP-kompatibel; Scripts sind vertrauenswürdiger Ruby-Code, keine Sandbox.
3. In F11 einen Entwurf ändern und andere Listeinträge auswählen; beim
   Rückwechsel muss er erhalten bleiben. Schließen und Projektwechsel jeweils
   mit Abbrechen, Speichern und Verwerfen prüfen. Externe Änderungen an der
   **Testkopie** von `scripts.json` während offenem F11 erzeugen einen
   Dateikonflikt: Nein erhält beide Stände, Ja überschreibt nur nach Warnung.
   Beschädigtes JSON beim Neu laden darf den offenen Entwurf nicht löschen.
   Kontrollieren, dass unbekannte Zusatzfelder unverändert erhalten bleiben.

Erwartung und tatsächliches Ergebnis pro Schritt, nicht ausgeführte Fälle,
`log/editor.log` und bei Bedarf Bildschirmfotos festhalten. F12 speichert
nur den **Viewport**, nicht das gesamte Hauptfenster. Bei einem Fehler die
Testkopie und Meldungen sichern, den Fall im Backlog mit Symptom, Vermutung
und betroffenen Stellen dokumentieren; nicht blind wiederholt testen.
