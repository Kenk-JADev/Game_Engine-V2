# Etappe 46: Zweige und leere Slots anklickbar zum Einfügen (F6)

**Ziel:** Die Befehlsliste verhält sich wie RPG Maker XP: Zweig-Kopfzeilen
(„Dann:“, „Sonst:“, „Wenn [x]:“, „Schleife:“, „Wenn Abbruch:“) und **leere Zweige**
sind anklickbar, um genau dort einen Befehl einzufügen — statt das leere
Inline-Feld unter einem Befehl bzw. die Befehlstyp-Combo zu benutzen.

**Vorgänger:** Etappe 45 (`docs/ETAPPE_45_COMMAND_SETTINGS_UI.md`) hat Einstellfenster,
nummerierte Listen und Asset-Ordnerwahl geliefert. Beim Einfügen musste man aber
weiter die Combo + „Einfügen“ nutzen; leere Zweige hatten keine sichtbare Zeile.

| Vorher (45) | Jetzt (46) |
|---|---|
| Leere Zweige ohne eigene Zeile | sichtbare Leer-Zeile `(leer - hier einfuegen)` je leerem Zweig |
| Doppelklick nur auf Befehle (Bearbeiten) | Doppelklick auf Zweig/Leer-Slot = **Einfügen in diesen Zweig** |
| Kopfzeile wählen + Combo + „Einfügen“ | Kopfzeile/Leer-Slot doppelklicken → Picker → Einstellfenster → einfügen |
| Ziel-Label: „Ziel: ausgewählter Zweig“ | „Ziel: Zweig (Doppelklick = einfuegen)“ |

## Änderung

`src/editor/editor_main.cpp`:
- `ev_flatten_empty(path, depth)` hängt an jeden **leeren oder fehlenden** Zweig
  (then/else, choices-Zweig, cancel-Zweig, Schleifenkörper) eine auswählbare
  Leer-Zeile (`header=true, insertable=true, index=-1`). Das Array entsteht erst
  bei gültigem Einfügen (wie bisher, verlustfrei).
- Doppelklick (`LBN_DBLCLK`) auf einen **Befehl** öffnet weiter das Einstellfenster
  (Bearbeiten); auf eine **insertable Kopfzeile/Leer-Zeile** öffnet er den
  XP-Picker, dessen „Übernehmen“ das Einstellfenster öffnet und beim Bestätigen
  in genau diesen Zweig einfügt (`ev_insert_command` nutzt den gewählten Pfad).
- Nicht insertable (beschädigte/mehrfache) Zweige bleiben geschützt: Doppelklick
  tut nichts, „Einfügen“ bleibt gesperrt.

Serialisierung unverändert der getestete Pfad `event_command_prepare` /
`event_batch_prepare` (Insert mit `create=true` legt das Zweig-Array erst bei
gültigem Befehl an). Keine Änderung an Datenformat oder Engine.

## Prüfung

- Cross-Kompilat des Editors (zig, Windows-Ziel) sauber, nur die vorbestehende
  `-Wdangling-assignment-gsl`-Warnung an alter Stelle.
- Bestehende Suiten grün: `json_test` **1031/0**, `event_command_catalog_test`
  **32/0**, `event_command_settings_test` **29/0**, `befehle_test` 0,
  `event_commands_arch_test` **1542/0**, `event_commands_engine_test` **110/0**.
- Repo-Hygiene: UTF-8/LF unverändert unauffällig.

## CI

**CI grün:** Lauf [46002910770](https://github.com/Kenk-JADev/Game_Engine-V2/actions/runs/46002910770)
(Commit `0b26d2e`), alle vier Jobs erfolgreich (C++-Logik, Repo-Hygiene,
Windows-Build mit `RuneEditor.exe`, Ruby-Engine 60 OK/0/1). Der Windows-Job
kompiliert die geänderte Befehlsliste mit der echten Toolchain; er ersetzt keinen
interaktiven Sicht-/DPI-Test.

## Sicht-/DPI-Test auf dem Windows-Rechner

CI-Grün ersetzt keinen interaktiven GUI-Test. Bitte prüfen:
- Leerer Dann-/Sonst-Zweig zeigt `(leer - hier einfuegen)`; Doppelklick darauf
  öffnet den Picker, Auswahl + Übernehmen fügt **in diesen Zweig** ein.
- Doppelklick auf eine gefüllte Kopfzeile fügt am Ende des Zweigs ein.
- Doppelklick auf einen Befehl bearbeitet ihn weiter (Etappe 45).
- Beschädigte/mehrfache Zweige bleiben nicht einfügbar (Schutz unverändert).
