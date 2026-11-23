# Windows-Live-Test: Ruby-Start im Editor und Spiel

**Status:** Anleitung vorbereitet; die interaktive Prüfung auf dem Rechner
des Users und mit Hardware-GPU ist noch nicht ausgeführt. WARP- und CI-Tests
belegen nicht die konkrete lokale Installation oder die Bedienung.

1. Eigene Projekte sichern. Die aktualisierten Quellen beziehen, laufenden
   Editor und laufendes Spiel beenden und in PowerShell im Repository bauen:

   ```powershell
   .\build.ps1 -Clean
   .\build.ps1 -Test
   .\build.ps1 -WarpTest
   .\tools\windows_runtime_smoke.ps1
   ```

   `-Clean` entfernt vollständig den Ordner `build/`. Die Tests starten
   eigene temporäre Kopien; ein Fehler oder ausgelassener Test ist kein
   bestandener Live-Test. Alternativ ein **vollständiges neues** CI-Artefakt
   mit EXEs, DLLs, `lib/ruby/`, `game/` und `projekte/` in einen neuen Ordner
   entpacken. Eigene Projekte nicht mit Demo-Projekten überschreiben.
2. In der neuen Installation `RuneEditor.exe` regulär ohne `RUNE_D3D_WARP`
   starten. Eine Karte öffnen und sichtbare Boden-/Objekttexturen prüfen.
   `log/editor.log` darf keinen `LoadError: json` oder anderen Ruby-Startfehler
   enthalten; die Registrierung der Editor-Texturen und Sounds muss erscheinen.
3. `Rune3D.exe` regulär starten, ein Projekt auswählen und eine Karte öffnen;
   ggf. aus dem Editor mit F4 starten. Auch `log/engine.log` auf Ruby-Fehler
   prüfen. Kein Projekt speichern oder überschreiben, das nicht zum Test dient.
4. Datum, Commit/Artefakt, Windows-Version, GPU/Treiber, Installationspfad,
   Schritte, beobachtete Darstellung und relevante Logzeilen dokumentieren.
   Tritt ein Fehler auf, die **vollständigen** betroffenen Logs sichern und
   das Ergebnis als fehlgeschlagen/offen festhalten.

Die mitgelieferten Kernmodule benötigen keine separate Ruby-Installation.
Eigene Gems oder native Erweiterungen mit weiteren DLLs sind nicht Teil
dieses Nachweises. Software-WARP testet keine Hardware-GPU oder DPI-Bedienung.
