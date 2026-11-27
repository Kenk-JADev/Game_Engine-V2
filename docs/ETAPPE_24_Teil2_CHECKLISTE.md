# Etappe 24 Teil 2 — Variablenoperand für Heldenwerte

**Stand:** Runde 43 — Umsetzung und lokale Tests fertig; Git-/CI-Status nicht
verfügbar, weil dieser Arbeitsordner keine `.git`-Metadaten enthält.

Ziel: Die XP-Befehle 311/312/315/316/317 akzeptieren neben einem konstanten
`amount` auch eine Spielvariable als vorzeichenbehafteten Ganzzahl-Operand.

## Ergebnis

- [x] Runtime: `source: "variable"` liest `GameState.variable(src).to_i`;
  ohne `source` bleibt das bisherige konstante `amount` gültig.
- [x] MapCheck: konstante Mengen, Variablennamen und unbekannte Quellen
  geprüft; Variablenquelle erfordert `src`.
- [x] F6: Quelle const/variable plus Eingabefeld für `src` bei den fünf
  Heldenwert-Befehlen; unveränderte Altbefehle bleiben datentreu.
- [x] Tests: `actor_wert_test.rb` deckt alle fünf Befehle und Fehlerquellen ab;
  `json_test.cpp` prüft F6-Lesen, Bearbeiten, Umstellen und Pflichtfelder.
- [x] Register und XP-/Architektur-/Logik-Dokumentation synchronisiert.

## Datenformat

```json
{"cmd":"actor_hp", "target":"arik", "source":"variable", "src":"hp_bonus"}
{"cmd":"actor_param", "actor":"arik", "param":"atk", "source":"variable", "src":"atk_bonus"}
```

Die Variable wird zum Ausführungszeitpunkt gelesen und in eine Ganzzahl
umgewandelt. Wenn `source` fehlt oder `"const"` ist, kommt der Operand aus
`amount`. Der Variablenpfad ignoriert `amount`, erhält vorhandene Zusatzdaten
aber beim F6-Bearbeiten.

**Verifikation:** `ruby tools/test_all.rb` (50 OK, 1 EXE-abhängig übersprungen),
`powershell -ExecutionPolicy Bypass -File .\build.ps1 -Test` (alle verfügbaren
Prüfungen grün, `tex_import_test.rb` mangels `build/RuneEditor.exe` übersprungen).
Git-Commit und CI sind in dieser Dateikopie nicht möglich.
