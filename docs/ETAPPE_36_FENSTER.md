# Etappe 36: Fenster, Reiter, Script-Editor, DPI und Nebel

Stand: Code gebaut. Die Sichtprüfung am Windows-Desktop (F2, F5/F6, F7/F9/F11,
DPI-Wechsel) steht noch aus; kopflose Tests ersetzen sie nicht.

## Recherchegrundlage

Die XP-Vergleiche aus `docs/XP_EDITOR_RECHERCHE.md` und
`docs/XP_VERGLEICH.md` gelten weiter. Für diese Etappe wurden zusätzlich die
typischen Bedienwege des XP-Script-Editors und der Eigenschaftsfenster als
Zielbild genommen:

- Der Script-Editor zeigt eine **nummerierte, geordnete Liste**. Einfügen
  geschieht über der markierten Zeile; ohne Auswahl vor dem abschließenden
  `Main`-Script. Suche (`Strg+F`, `F3`) und Ersetzen (`Strg+H`) arbeiten im
  gerade geöffneten Script; Rechtsklick auf die Liste bietet Einfügen/Löschen.
- XP-Eigenschaftsdialoge sind **kompakt und in Reiter gegliedert**, statt eine
  einzige sehr hohe Seite mit vielen Gruppen. Die Reihenfolge der Felder ist
  dabei weniger wichtig als die erreichbare Information ohne Abschneiden.
- Die Windows-Dialoge werden bei 96 DPI entworfen. Bei höherer DPI müssen
  Clientfläche, Steuerelemente **und** Schrift mit skalieren; feste Pixel plus
  feste 16-px-Schrift führten bisher zu abgeschnittenen Beschriftungen.

Das ist eine Annäherung an die XP-Bedienlogik, keine vollständige
XP-Kompatibilität (vgl. Stufenplan S2–S8).

## Umgesetzt

### F2 „Karten-Eigenschaften“

Neue Reiter in `src/editor/editor_main.cpp`:

1. **Allgemein** – Name, Größe, Seed, Berge, Kommentar
2. **Himmel & Licht** – Panoramafarbe, Wolken, Wolkenschatten, Tageszeit,
   XP-Bildtönung, Nebel, Sichtweite, Nebelfarbe, Wetter
3. **Ton** – BGM/BGS mit Lautstärke und Pitch
4. **Zufallskämpfe** – Schritte, Truppenliste, Datenbank
5. **Block-Zonen** – eine Zone pro Zeile, `#n`-Marken bleiben erhalten

Alle Felder bleiben über `MapPropsForm` / `apply_props_from_dlg()` transaktional:
nur geänderte Felder werden geprüft und geschrieben; ein Fehler springt auf den
Reiter des betroffenen Feldes. Ungültige Altwerte bleiben unangetastet, wenn
sie nicht bearbeitet wurden. Die neuen Himmelsfelder liegen im vorhandenen
Karten-JSON-Objekt `"sky"` und werden in `map_props_form.h` nur schlüsselweise
verändert (`clouds`, `cloud_shadow`, `time`, `tone`).

### Script-Editor

- Nummerierte Liste (`001`, `002`, …) mit (`▶`)-Markierung für Startscripts.
- „Neu“ fügt vor der Auswahl ein; ohne Auswahl vor `089 - Main`, damit eigene
  Scripts vor der XP-Hauptschleife liegen. `089 - Main` bleibt gesperrt.
- Rechtsklick in der Liste: Einfügen/Löschen.
- `Strg+F` fokussiert das Suchfeld, `F3` die nächste Fundstelle (mit Umlauf,
  ASCII-Groß-/Kleinschreibung ignoriert).
- `Strg+H` fokussiert das Ersetzen-Feld. `Enter` ersetzt den nächsten Treffer,
  `Strg+Enter` oder „Alle“ ersetzt alle Treffer ohne Umlauf. Auch eine
  Ersetzung, die den Suchtext erneut enthält, terminiert.
- Der Autoload-Haken sagt ausdrücklich, dass XP-Defaultscripts und
  `089 - Main` geschützt bleiben.

Die reine Logik liegt in `src/editor/script_document.h` und ist in
`tools/json_test.cpp` getestet (`script_editor_tests`).

### DPI

`src/editor/editor_main.cpp` enthält jetzt einen gemeinsamen Umrechner
(`ui_dpi`, `ui_px`, `ui_dialog_font`). Damit werden Clientfläche, Positionen,
Steuerelemente und Schrift von F2, „Neue Karte“, Probleme, Tileset, Projekt,
Materialbase, Datenbank, Sound-Test und Platzier-Editor aus 96-DPI-Logikpixel
berechnet. Hauptfenster, Event-Editor und Script-Editor nutzten bereits
DPI-skalierte Layouts. F2 und „Neue Karte“ verwenden zusätzlich
`AdjustWindowRectExForDpi` und werden auf dem aktuellen Monitor zentriert.

Bekannte Grenze: laufend geöffnete Dialoge werden bei einem DPI-Wechsel des
Monitors nicht neu layoutet; sie werden beim nächsten Öffnen korrekt gebaut.
Ein vollständiges `WM_DPICHANGED` je Dialog ist weiterhin offen.

### Nebel am Horizont

Der Screenshot zeigte weißes Gelände in der Ferne vor kräftig blauem Himmel.
Der Szenen-Shader nebelt weit entferntes Gelände bis zur Nebelfarbe ein; der
Himmel tat das nicht. In `src/gfx/renderer.cpp` erhält der Sky-Shader jetzt
`gSkyFog` (rgb = Nebelfarbe, w = Nebel an). Er mischt den Himmel **nur
horizontnah** in die Nebelfarbe (`smoothstep(-0.02, 0.32, up)`). Zenit, Sonne
und Wolken bleiben Himmel; bei ausgeschaltetem Nebel bleibt alles unverändert.

## Prüfungen

- `powershell -ExecutionPolicy Bypass -File .\build.ps1` – beide EXEs gebaut.
- Offen: `build.ps1 -Test`, `ruby tools/test_all.rb --strict`,
  `ruby tools/lint_repo.rb`.
- Offen: Live-Prüfung von F2-Reitern, Script-Suche/Ersetzen, Rechtsklick,
  150 %/175 % DPI und dem Nebelverlauf im 3D-Fenster.
