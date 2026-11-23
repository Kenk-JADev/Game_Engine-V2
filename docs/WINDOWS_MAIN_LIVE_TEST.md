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
   die Palette über dem Kartenbaum liegen, rechts Objektliste und Formular,
   dazwischen die echte 3D-Ansicht und unten die Statusleiste.
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

Erwartung und tatsächliches Ergebnis pro Schritt, nicht ausgeführte Fälle,
`log/editor.log` und bei Bedarf Bildschirmfotos festhalten. F12 speichert
nur den **Viewport**, nicht das gesamte Hauptfenster. Bei einem Fehler die
Testkopie und Meldungen sichern, den Fall im Backlog mit Symptom, Vermutung
und betroffenen Stellen dokumentieren; nicht blind wiederholt testen.
