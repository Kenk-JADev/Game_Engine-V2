# Windows-Live-Test: gezielte Items und Kampfzustand

**Status: offen.** Dies ist eine Anleitung, kein bereits ausgeführter GUI- oder
Hardware-GPU-Test. Die automatisierten Ruby-Tests benutzen native Attrappen;
WARP in der Windows-CI ist Software und ersetzt diese Prüfung nicht.

## Isolierte Testkopie vorbereiten

1. Auf einem Windows-Rechner mit interaktivem Desktop und D3D11-Hardware gemäß
   README bauen und automatisiert testen (`.\build.ps1 -Clean`, dann
   `.\build.ps1 -Test`). Version, Commit, Windows, GPU/Treiber, Auflösung und
   DPI notieren. Spielstände und Projektdateien vorher sichern.
2. Nur eine **neue** Testkopie außerhalb des Repositorys verwenden:

   ```powershell
   ruby tools/prepare_material_live.rb "$env:TEMP\RuneBattleLive"
   if ($LASTEXITCODE -ne 0) { throw 'Vorbereitung fehlgeschlagen; Teilkopie prüfen' }
   cd "$env:TEMP\RuneBattleLive"
   ```

   Das Werkzeug verweigert ein vorhandenes Ziel; dann einen anderen Namen
   wählen. Es kopiert Build und Vorlage und erstellt die Testprojekte
   `A_material` und `B_material`. F4 ist keine Sandbox und benutzt die
   regulären Dateien des gewählten Projekts: nur in der Kopie arbeiten.
3. In der Kopie `.\build\RuneEditor.exe --project A_material` starten und
   die Karte `dorf` auswählen. F9: In der aus der Vorlage kopierten Datenbank
   sind `arik`, `mira`, `heiltrank`, `manatrank` und die Truppe `waldweg`
   vorhanden. Für reproduzierbare Schäden im Reiter **Gegner** bei `goblin`
   und `wolf` jeweils `ATK` auf 0 setzen und F9 **Speichern** wählen. Ein
   Gegner verursacht wegen Mindestschaden weiterhin 1 HP. Im Karten-F2
   zufällige Begegnungen für die Testkarte leer lassen, damit sie den Test
   nicht vorzeitig auslösen. Die Karte mit Strg+S sichern.

## Definierten Kampf im Editor anlegen

1. Im Script-Editor F11 **Neu** wählen, den Namen `kampf_probe` eintragen,
   folgenden Code einsetzen und **Speichern** wählen:

   ```ruby
   party.find('arik').hp = 60
   party.find('mira').hp = 0
   party.find('mira').mp = 2
   ```

   Das Script ist nur zur Testvorbereitung; es gehört nicht in ein echtes
   Spielprojekt. Die Vorlage startet mit zwei Heiltränken und einem Manatrank.
2. In F6 **Neu** wählen und ein Action-Event erstellen, z. B. bei X = 0,
   Z = 8 und Radius = 2 direkt am Startpunkt der Testkopie. **Event
   übernehmen** wählen; auf der Seite muss der Trigger `action` stehen.
   Nacheinander **[Steuerung] Script** mit Script-Name `kampf_probe` und
   **[Kampf] Kampf starten** mit Truppen-ID `waldweg`, `an` für Flucht und
   Verlust `1` (`can_lose`) jeweils über **Einfügen** ergänzen. Die Reihenfolge
   in der Befehlsliste prüfen. F6 schließen, Karte mit Strg+S speichern und
   F4 drücken.
3. Beide Intro-Textseiten mit **E** oder **Enter** bestätigen (kein Mausklick),
   die Taste zwischen den Bestätigungen loslassen. Das Event in Reichweite
   anschließend mit einem erneuten **E** auslösen. Ein Action-Event ohne
   Schildgrafik ist unsichtbar; der Hinweis mit dem Eventnamen zeigt die
   Reichweite an.

Tasten sind flankengesteuert: Für jeden erneuten Druck dieselbe Taste zunächst
loslassen. Esc gibt unabhängig von der Kampf-UI auch den nativen Mausfang frei;
mit der mittleren Maustaste lässt sich der Mausblick wieder einschalten.

## Ablauf und Erwartungen

1. Im Kampf muss Arik bei 60/120 HP stehen, Mira bei 0/80 HP und 2/40 MP.
   Nur Arik erhält zunächst einen Befehl. Während des Kampfes dürfen Spieler
   und Weltphysik nicht weiterlaufen. Das Fenster bei Bedarf auch mit anderer
   Größe und DPI prüfen; Zielhinweise und Status müssen lesbar bleiben.
2. **Item → Manatrank** wählen; Arik ist beim MP-Wert voll. Bestätigen darf
   keinen Befehl verbuchen und nichts verbrauchen; eine Meldung nennt die
   fehlende Wirkung. Esc geht zurück zur Itemliste, ein weiteres Esc zum
   Befehl. Das Spielmenü darf dabei nicht erscheinen.
3. **Item → Heiltrank** wählen. In der Zielwahl mit rechts zu Mira wechseln;
   Markierung und Zielname müssen übereinstimmen. Einmal Esc drücken und
   prüfen, dass der Heiltrank weiterhin vorhanden ist. Erneut wählen und
   Mira bestätigen. Der Trank wird erst in der Aktionsphase verbraucht;
   Mira erwacht mit 40 HP. Anschließend schlagen beide Gegner je 1 HP auf
   einen zufälligen lebenden Helden; Mira kann danach 38–40 HP haben.
4. In der nächsten Runde Arik einen Heiltrank auf sich selbst wählen lassen.
   Bei Miras Itemwahl darf der letzte, bereits reservierte Heiltrank nicht
   nochmals auswählbar sein. Mira wählt stattdessen den Manatrank auf sich
   selbst. Nach Ausführung muss ihr MP-Wert 22 sein; die Wirkungen im
   Kampflog prüfen. Die Gegner können danach erneut je 1 HP abziehen. Den
   vollständigen Inventarstand erst nach dem Kampf im Weltmenü vergleichen.
5. Gegnerwahl öffnen, mit Esc abbrechen und prüfen, dass keine unerwünschte
   Aktion ausgeführt wird. Esc in der **Befehlswahl** versucht Flucht statt
   das Weltmenü zu öffnen. Bei Fehlschlag verfallen vorgemerkte Aktionen,
   die Gegner ziehen; bis zum Erfolg erneut versuchen. Nach dem Kampf müssen
   Steuerung und Physik wieder laufen. Erst außerhalb des Kampfes öffnet Esc
   das normale Menü. Dort HP/MP und Inventar notieren und Slot 1 speichern.
   Danach F5 drücken, das Intro bestätigen und prüfen, dass die Party auf
   Startwerte zurückgesetzt wurde. Im erneut geöffneten Menü Slot 1 laden
   und die wiederhergestellten Werte mit den Notizen vergleichen.
   Falls das Party-Panel nicht erscheint, das Spielfenster verbreitern oder
   maximieren; bei zu wenig Platz wird es ausgeblendet. Wenn es nicht sichtbar
   gemacht werden kann, diese Sichtprüfung als offen festhalten.
6. Das Event erneut starten und **F5 im Spiel** während des Kampfes drücken.
   Der Kampf ist abgebrochen und der Spieler danach wieder beweglich; das
   erneut erscheinende Intro mit E oder Enter bestätigen. F5 startet den
   Spielzustand neu; es ist kein Speichern und soll die gerade nicht
   gespeicherte Party nicht erhalten.
7. Optional im F11-Script `kampf_alle_ko` mit
   `party.actors.each { |held| held.hp = 0 }` anlegen. Zwei getrennt erreichbare
   Action-Events anlegen: jeweils zuerst das Script, dann den Kampf und einen
   Textbefehl einfügen. Beim ersten Kampf `Verlust 0` wählen: Der Titel muss
   erscheinen, ohne den folgenden Text auszuführen. Projekt neu starten und
   das zweite Event mit `Verlust 1` auslösen: Der nachfolgende Text muss
   erscheinen und die Helden bleiben bei 0 HP.

Pro Schritt Erwartung und tatsächliches Ergebnis, Log (`log/engine.log`),
Screenshots und nicht ausgeführte Fälle festhalten. Bei unerwartetem Ergebnis
die Testkopie zur Diagnose sichern, nicht das Originalprojekt überschreiben.
