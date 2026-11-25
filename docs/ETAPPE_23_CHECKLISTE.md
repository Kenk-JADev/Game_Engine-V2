# Etappe 23 — Checkliste (Musik sichern/merken als Event-Befehl)

Diese Checkliste ist die Abhakliste für Etappe 23. Stand: **Runde 41 —
abgeschlossen** (CI grün: Lauf 45607505425, alle vier Jobs).

Legende: `[x]` erledigt und committet · `[~]` erledigt, CI offen · `[ ]` offen

**Vorgänger:** Etappe 22 (Ton-Reste) ist **abgeschlossen** — MP3, `se_stop`,
`me_play`/`me_stop`, `bgm_memorize`/`bgm_restore`/`bgs_*` und die Kampfmusik;
Register **65 Typen**, CI grün, letzter Commit `677e7cf`.

**Ziel:** Etappe 22 hat die RGSS-Methoden `bgm_memorize`/`bgm_restore` und
`bgs_memorize`/`bgs_restore` gebaut, aber es gibt noch keinen Weg, sie aus einem
Event heraus aufzurufen — die XP-Befehle **243/244** (Musik sichern /
zurückholen) und **247/248** (Musik + Hintergrundklang merken /
wiederherstellen) fehlen im Register. Genau die vier kommen jetzt dazu; damit
ist der Ton-Block in `docs/XP_VERGLEICH.md` vollständig ✅.

## 1. Aufgaben

- [x] **T1 — Engine:** `Game_Interpreter` führt die vier Typen aus und ruft
  `$game_system.bgm_memorize`/`bgm_restore`/`bgs_memorize`/`bgs_restore`
  (XP-Semantik; ohne `$game_system` still, damit kopflose Tests laufen).
  `MapCheck::CMD_TYPES` nimmt sie auf, `pruefe_befehle` kennt sie als
  felderlose Befehle.
- [x] **T2 — Register und Editor:** `docs/befehle.json` (65 → **69**, Bereich
  „Ton", Stand-Zeile), `ISSUE_CMD_TYPES` (`editor_main.cpp`),
  `EvType`/`EV_TYPES` und die Feld-/Prüf-Tabellen (`event_form.h`, jeweils
  „keine Felder"), Anzeigenamen und Hinweistexte im F6-Dialog.
- [x] **T3 — Tests:** `tools/befehle_test.rb` (Register gegen alle vier
  Stellen), `tools/json_test.cpp` (je ein Beispiel, `ET_COUNT` wächst mit),
  `tools/event_test.rb` (Ausführung über eine `$game_system`-Attrappe),
  `CMD_TYPES.size` in den elf Tests auf 69.
- [x] **T4 — Doku:** `XP_VERGLEICH.md` (243/244 neu, 247/248 von ⚠️ auf ✅,
  Bilanz), `architecture.md` (Befehlstabelle), `LOGIK_PRUEFUNG.md` (AD.19),
  `HANDOFF` (Teil-Absatz, Commit-Tabelle), dieser Abschnitt.

### Ergebnis je Aufgabe

- **T1:** Der Interpreter hat je eine eigene `when`-Zeile (`save_bgm`,
  `resume_bgm`, `memorize_audio`, `restore_audio`) und ruft den Helfer
  `ton_merker(cmd)`, der die XP-Methoden auf `$game_system` abbildet (243 →
  `bgm_memorize`, 244 → `bgm_restore`, 247 → `bgm_memorize` + `bgs_memorize`,
  248 → `bgm_restore` + `bgs_restore`); ohne `$game_system` gibt er `false`
  zurück, der Befehl bleibt still. `MapCheck::CMD_TYPES` und `pruefe_befehle`
  kennen die vier als felderlose Befehle.
- **T2:** `docs/befehle.json` steht auf **69 Typen** (Stand-Zeile „Etappe 23,
  Runde 41", Bereich Ton); `ISSUE_CMD_TYPES`, `EvType`/`EV_TYPES`,
  `EV_TYPE_LABELS` (69 Einträge) und die Feldtabellen in `event_form.h`
  (`known_fields` → `{}`, Prüf-Tabelle → `break`) sind nachgezogen; das
  F6-Formular zeigt für die vier nur einen Hinweistext ohne Eingabefelder.
- **T3:** `tools/json_test.cpp` hat je ein Beispiel (873 Checks, `ET_COUNT`
  deckt sich mit `samples`), `tools/event_test.rb` prüft die Ausführung über
  eine `$game_system`-Attrappe (6 Checks inkl. „ohne `$game_system` still"),
  `CMD_TYPES.size` steht in elf Tests auf 69; `tools/befehle_test.rb` und
  `tools/sync_test.rb` vergleichen Register, EV_TYPES, ISSUE_CMD_TYPES,
  Labels und die `when`-Zeilen des Interpreters (jede Zeile ein Literal).
- **T4:** dieser Abschnitt, `XP_VERGLEICH.md` (243/244 ✅ neu, 247/248 ⚠️ →
  ✅, Bilanz **53 ✅ · 7 ⚠️ · 33 ❌**), `architecture.md`, `LOGIK_PRUEFUNG.md`
  (N-Tabelle, AD.19), `HANDOFF` mit Commit-Tabelle.

### Stolpersteine dieser Runde

- `tools/sync_test.rb` erwartet **eine `when`-Zeile pro Befehlstyp** mit genau
  sechs Leerzeichen Einrückung und nur einem Literal je Zeile — eine
  Sammelzeile `when 'a', 'b'` fällt durch.
- Im Windows-Build fiel ein Anführungszeichen im neuen Hinweistext auf: Python
  hatte aus `\"` beim Schreiben ein nacktes `"` gemacht. Seitdem prüft
  `zig c++ -target x86_64-windows-gnu -fsyntax-only -Isrc
  src/editor/editor_main.cpp` den Editor **vor** dem Push (die Linux-CI baut
  `editor_main.cpp` nicht).

## 2. Definition of Done

Ein Punkt ist fertig, wenn er im Kopf kommentiert ist, einen kopflosen Test
hat (Ruby über `tools/test_all.rb`, portables C++ über CI), im Register nichts
kaputt macht und die CI auf allen vier Jobs grün ist — inklusive
Windows-Build, der `editor_main.cpp`/`event_form.h` mit `-Werror` baut.

## 3. Bekannte Eigenheiten

- Die vier Befehle tragen **keine Felder** (XP speichert nur den Code). Sie
  stehen deshalb im F6-Dialog als Typ mit Hinweistext und ohne Eingabefelder.
- Der Merker ist derselbe wie in Etappe 22: flüchtig, kein Spielstand. Ein
  `save_bgm` überschreibt den vorherigen Merker (XP-Verhalten).
- Die Namen folgen dem Register-Stil (englisch, snake_case):
  `save_bgm` (243), `resume_bgm` (244), `memorize_audio` (247),
  `restore_audio` (248).

## 4. Reihenfolge

T1 (Engine) → T2 (Register/Editor, weil `befehle_test` sonst rot ist) →
T3 (Tests mit jedem Schritt) → T4 (Doku zum Schluss).
