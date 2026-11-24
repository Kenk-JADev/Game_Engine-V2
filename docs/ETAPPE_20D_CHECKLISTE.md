# Etappe 20d — Checkliste (Event-Befehle Welle 2: Daten)

Diese Checkliste ist die Abhakliste für Etappe 20d. Sie steht im Repo, damit
jede Sitzung (Mensch oder KI) den Stand ohne Rätselraten sieht: Was ist fertig,
was fehlt, und woran erkennt man „fertig". Stand: **Runde 28**.

Legende: `[x]` erledigt und committet · `[~]` erledigt, CI offen · `[ ]` offen

**Kurzfassung Stand:** 11 von 17 Punkten aus Abschnitt 3 sind erledigt
(6 Bedingungen/Sperren, Timer, `$game_temp`, 4 Wert-Befehle gezählt als
Pakete). Es fehlen noch **Variable-Operationen (122)**, **Actor/Button-
Bedingungen**, **`party_member` (129)**, **Transfer-Startposition** und der
Abschluss (Bilanz/HANDOFF/CI).

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

- [x] **46 Befehlstypen** statt 35 (35 + sechs Bedingungen/Sperren + Timer
      + vier Wert-Befehle)
- [ ] Ziel am Ende von 20d: alle geplanten Typen eingetragen (siehe 3.)

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
- [x] Tests: `bedingungen_test.rb` (26), `timer_test.rb` (20),
      `temp_flags_test.rb` (14), `actor_wert_test.rb` (24), `json_test` 753,
      `sync_test` grün, `mapcheck_test` 105 `ok`
- [x] Doku nachgezogen (42 Typen, Bilanz 34 ✅ · 9 ⚠️ · 49 ❌)

## 3. Offen in 20d

- [ ] **`variable` erweitern (XP 122):** Operationen `-` `×` `÷` `Rest` und
      Quellen Zufall / Item / Actor / Gegner / Charakter / „selbst"
      (heute nur `value`/`add`) — Datenformat rückwärtskompatibel halten
      (`value`/`add` bleiben gültig), neue Felder für Operation + Quelle
- [ ] **Bedingungsquellen Actor/Button** (XP 111): `if_actor` (Party-Mitglied
      vorhanden?) und `if_button` (Taste gedrückt) — Timer/Enemy/Character/
      Weapon/Armor bleiben bewusst offen (keine Daten in Rune3D)
- [ ] **`party_member`** (XP 129) — Mitglied hinzufügen/entfernen
- [ ] **Werte-Befehle erweitern:** Operand „Variable" (XP erlaubt statt
      Konstante `variable[x]`) und Ziel „ganze Party" bei `actor_exp` prüfen
- [ ] **`player_transferring`:** Startposition `player_new_x/y/direction`
      anwenden (Rune3D-Weltkoordinaten ≠ XP-Kacheln → Umrechnung festlegen,
      LOGIK AC.10)
- [ ] MapCheck-Regeln für alle neuen Typen, Register/Doku/Zählungen wie in 0.
- [ ] Abschluss: `docs/XP_VERGLEICH.md`-Bilanz neu, `HANDOFF` mit Runden-Nr.,
      CI grün, Etappe 20d als erledigt markiert

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
