# Etappe 20d — Checkliste (Event-Befehle Welle 2: Daten)

Diese Checkliste ist die Abhakliste für Etappe 20d. Sie steht im Repo, damit
jede Sitzung (Mensch oder KI) den Stand ohne Rätselraten sieht: Was ist fertig,
was fehlt, und woran erkennt man „fertig". Stand: **Runde 29 — Etappe 20d
abgeschlossen**.

Legende: `[x]` erledigt und committet · `[~]` erledigt, CI offen · `[ ]` offen

**Kurzfassung Stand (Runde 29, alle Commits CI-grün):** Alle sechs Pakete
sind **fertig** — Bedingungen/Sperren (111 Gold/Item/Script/Actor/Taste +
134/135/136), Timer (124) mit Anzeige, `$game_temp`-Calling-Flags,
Wert-Befehle (311/312/315/316), Variable-Operationen/Quellen (122) und
Mitglieder ändern (129) samt Mitglieder-Modell. Registerstand **49 Typen**.

**20d ist damit abgeschlossen.** Was bewusst offen bleibt, steht in
Abschnitt 3 unter „Nicht in 20d" (kein offener Haken mehr).

## 0. Definition of Done für JEDEN neuen Befehlstyp

Ein Befehl gilt erst als fertig, wenn **alle** Punkte stimmen:

- [x] `EvType`/`EV_TYPES`/`EV_TYPE_NAMES` in `src/editor/event_form.h`
- [x] `CMD_TYPES` in `game/engine/mapcheck.rb`
- [x] `when`-Zweig in `game/engine/interpreter.rb` (eigene Zeile je Typ —
      `sync_test` liest die erste Zeichenkette pro `when`)
- [x] `ISSUE_CMD_TYPES` in `src/editor/editor_main.cpp`
- [x] Eintrag in `docs/befehle.json` (Pflicht-/Optionalfelder wie im Formular)
- [x] Formular: Keys, Lesen/Schreiben, Sichtbarkeiten, Labels, Hinweise,
      `ev_cmd_label`, Zweig-Listen (`event_command_has_branches`,
      `event_command_list_mutable`, `ev_flatten`)
- [x] MapCheck-Prüfung (Pflichtfelder, Vokabular, rekursive Zweige)
- [x] Beispiel in `tools/json_test.cpp` (`samples` == `ET_COUNT`,
      Teiländerungs-Fall in der Patch-Prüfung)
- [x] Zählungen angepasst: `mapcheck_test`, `move_route_test`, `ablauf_test`,
      `bedingungen_test`, `timer_test`, `befehle.json`-Stand
- [x] Test/Check, der das neue Verhalten kopflos beweist
- [x] Doku: `architecture.md`, `XP_VERGLEICH.md`, ggf. `LOGIK_PRUEFUNG.md`,
      `HANDOFF_Naechste_KI.md`
- [x] CI grün (alle vier Jobs)

## 1. Registerstand

- [x] **49 Befehlstypen** statt 35 (35 + acht Bedingungen/Sperren + Timer
      + vier Wert-Befehle + Variable-Quellen + Mitglieder)
- [x] Ziel am Ende von 20d: alle geplanten Typen eingetragen (siehe 3.)

## 2. Fertig in Runde 28

- [x] **`if_gold`** (XP 111, Quelle Gold) — `op`/`value`/`then`/`else`,
      `compare_zahl`, Default 0
- [x] **`if_item`** (XP 111, Quelle Item) — `name`/`op`/`value`,
      `Party.item_count`, Default 1
- [x] **`if_script`** (XP 111, Quelle Script) — `Scripts.eval_code`,
      `false`/`nil` = falsch, Fehler = falsch + Issue-Backlog
- [x] **`save_access` / `menu_access` / `encounter_access`** (XP 134/135/136) —
      `allowed` (Default erlaubt) → `$game_system.*_disabled`, im Spielstand
- [x] **`timer`** (XP 124) — `mode` start/stop + `seconds`, Restzeit als
      `mm:ss` oben rechts (`UI.timer`), `GameSystem#update(dt)` zählt in
      XP-Frames, Timer im `system`-Snapshot
- [x] **`actor_hp` / `actor_sp` / `actor_exp` / `actor_level`**
      (XP 311/312/315/316) — `target` (`alle` oder Actor), `amount`
      vorzeichenbehaftet; EXP/Stufe über denselben Weg wie der Kampf;
      MapCheck prüft Menge, Actor-Namen, Stufenbereich
- [x] **`$game_temp`-Calling-Flags** (LOGIK E.3) — `TempActions.tick` für
      `battle_calling`, `menu_calling`, `save_calling`, `player_transferring`,
      `to_title`; ein Auftrag pro Frame, Flag vorher gelöscht
- [x] Operator-Combo im Formular gilt für Variable **und** die neuen
      Wenn-Typen (`op_feld`-Lambda)
- [x] Zweige der neuen Wenn-Typen im F6-Baum einfügbar/bearbeitbar und im
      Editor-Issue-Scan rekursiv geprüft (`event_cmd_has_branches`)
- [x] Tests: `bedingungen_test.rb` (34), `timer_test.rb` (20),
      `temp_flags_test.rb` (17), `actor_wert_test.rb` (24),
      `variable_ops_test.rb` (36), `party_member_test.rb` (29),
      `json_test` 778, `sync_test` 46 grün,
      `mapcheck_test` 105 `ok`
- [x] Doku nachgezogen (49 Typen, Bilanz 37 ✅ · 7 ⚠️ · 48 ❌)

## 3. Offen in 20d

- [x] **`variable`-Operationen (XP 122):** `set/add/sub/mul/div/mod` über
      `op` + `value`, Altform `value`/`add` bleibt gültig und wird im Editor
      umgestellt; ÷0/`mod` 0 = Warnung + Wert bleibt
- [x] **`variable`-Quellen (XP 122):** `source` (`const`/`variable`/`random`/
      `item`/`self`) + `src`, eigenes Quellen-Combo im Formular, MapCheck-Regeln,
      `sync_test` vergleicht C++/Combo/Ruby-Liste. **Nicht umgesetzt:**
      Actor/Enemy/Character (keine Gegner-/Charakterdaten; Actor-Attribute
      bräuchten ein Feldauswahl-Combo)
- [x] **Bedingungsquellen Actor/Button** (XP 111): `if_actor` (Team-Mitglied)
      und `if_button` (Taste im Druckframe); Timer/Enemy/Character/Weapon/
      Armor bleiben bewusst offen (keine Daten bzw. kein Zeitgeber als Seite)
- [x] **`party_member`** (XP 129) — `Party` hat jetzt `@katalog` (alle
      Datenbank-Helden) + `@actors` (Mitglieder, Reihenfolge = Beitritt),
      `member_add`/`member_remove`/`member?`; `snapshot` führt die Mitglieder
      (`actors`), `restore!` baut `@actors` daraus neu auf (alte Saves ohne
      `actors` behalten das Team). Befehl `party_member` mit `name` + `mode`
      add/remove, MapCheck prüft Modus und Actor, Formular moderiert den Modus
      (wie `fade`). Das **letzte Mitglied bleibt** (XP erlaubt ein leeres Team;
      Kampf/Menü/Spielstand bräuchten dafür eigene Regeln) — MapCheck und
      Interpreter melden den Versuch. `if_actor` fragt jetzt die Mitgliederliste
      statt „alle Datenbank-Helden". Test: `tools/party_member_test.rb` (29)
- [x] **Werte-Befehle:** Ziel „ganze Party" ist über `target: "alle"` (und
      leeres Ziel) abgedeckt
- [ ] **Nicht in 20d:** Operand „Variable" bei den Wert-Befehlen (XP erlaubt
      `variable[x]` statt Konstante) — braucht einen zweiten Operanden-Typ im
      Formular; als Roadmap-Punkt weitergeführt
- [x] **`player_transferring`:** Startposition `player_new_x/y` (Weltkoordinaten,
      `0/0` = bisherige Stelle) und `player_new_direction` (2/4/6/8 → Yaw-Tabelle
      `RICHTUNG_YAW`) werden angewendet; Test in `temp_flags_test.rb`
- [x] MapCheck-Regeln für alle neuen Typen, Register/Doku/Zählungen wie in 0.
- [x] Abschlussrunde: Bilanz neu (49 Typen, 37 ✅ · 7 ⚠️ · 48 ❌), `HANDOFF`
      mit Runden-Nr., CI grün. **20d ist abgeschlossen.**

## 4. Bekannte Eigenheiten (nicht nochmal darüber stolpern)

- Ein Interpreter-Rahmen pro Update → Tiefe/Ende-Tests mit ≥ 60 Frames.
- `sync_test` liest **alle** `when`-Zeilen mit 4er-Einzug als Operator-Tabelle
  → in Helfern `if/elsif` statt `case` verwenden.
- Nach Editor-Änderungen ist `sync_test` rot, bis alle Registerstellen
  (Punkt 0) nachgezogen sind — erst Register, dann Tests.
- `RAISE`/`LoadError`: `main.rb` lädt `temp_actions` selbst, weil Tests
  `main.rb` ohne `boot.rb` laden.
- Lokale Harness-Läufe (WASM) sind kein CI-Ersatz; `encounter_test` u. a.
  laufen dort nicht (Segfault/fehlende Pfade).
