# Etappe 20e — Checkliste (Shop & Namenseingabe)

Diese Checkliste ist die Abhakliste für Etappe 20e. Sie steht im Repo, damit
jede Sitzung (Mensch oder KI) den Stand ohne Rätselraten sieht: Was ist fertig,
was fehlt, und woran erkennt man „fertig". Stand: **Runde 30 — geplant, noch
nichts umgesetzt**.

Legende: `[x]` erledigt und committet · `[~]` erledigt, CI offen · `[ ]` offen

**Vorgänger:** `docs/ETAPPE_20D_CHECKLISTE.md` (Event-Befehle Welle 2) ist
**abgeschlossen** (49 Befehlstypen, CI grün, Commit `47f9000`).

**Ziel:** Die XP-Befehle **302 (Shop)** und **303 (Namenseingabe)** laufen in
Engine, Editor und Datenprüfung; ein Projekt kann damit einen Laden bauen und
einen Helden umbenennen, ohne Skripte zu schreiben.

## 0. Definition of Done (wie 20d)

Für jeden neuen Befehlstyp gilt dieselbe Liste wie in
`docs/ETAPPE_20D_CHECKLISTE.md` Abschnitt 0: `EvType`/`EV_TYPES`/Labels,
`CMD_TYPES`, `when`-Zweig im Interpreter, `ISSUE_CMD_TYPES`, Eintrag in
`docs/befehle.json`, Formular (Lesen/Schreiben/Sichtbarkeit/Label/Hinweis/
`ev_cmd_label`), MapCheck-Regeln, Beispiel in `tools/json_test.cpp`
(`samples` == `ET_COUNT`), Zählungen in den Tests, ein kopfloser Beweis-Test,
Doku, **CI grün**.

## 1. Plan in Scheiben

- [ ] **E1 — Shop-Modell (`Shop`):** Warenliste `[{item, price}]` (Preis 0 =
      Datenbankpreis), `Shop.open(waren)`, `Shop.buy(item)`, `Shop.sell(item)`,
      Bestand optional (unbegrenzt wie XP, `stock` als Erweiterung), Verkauf
      zu 50 % (XP `Game_Party#sell_price`-Regel), Gold/Inventar über `Party`,
      Meldungen ins Log und ins Issue-Backlog bei fehlendem Gold/Item
- [ ] **E2 — Befehl `shop` (XP 302):** `{"cmd":"shop","goods":[{"item":"heiltrank","price":20}]}`,
      `goods` als Liste (Pflicht), Preis optional, MapCheck prüft Item gegen
      `database.json` und Preise (≥ 0), Formular: Warenliste im Mehrzeiler
      (`item:preis` pro Zeile), `ev_cmd_label` = „Shop (n Waren)“
- [ ] **E3 — Shop-Fenster:** `UI.shop` / eigenes Unterfenster im Stil des
      Menüs (Kaufen/Verkaufen/Verlassen, Cursor, Gold- und Preiszeile,
      Esc = eine Ebene zurück), Anbindung an `Interpreter` (Befehl pausiert
      bis das Fenster schließt, wie Text/Wahl)
- [ ] **E4 — Befehl `name_input` (XP 303):** `{"cmd":"name_input","actor":"arik","max":8}`,
      Tastatureingabe mit Backspace/Esc (Abbruch = alter Name), Klemmen auf
      `max` (XP: 8 Zeichen), Ergebnis in `Actor#name`, im Spielstand also im
      `party`-Snapshot; MapCheck prüft Actor und `max` (1–16)
- [ ] **E5 — Tests:** `tools/shop_test.rb` (Modell, Befehl, MapCheck,
      Verkaufspreis, fehlendes Gold), `tools/name_input_test.rb` (Eingabe,
      Abbruch, Klemmen, Spielstand), je ein Beispielpaar in `json_test.cpp`,
      `sync_test`-Vergleiche der neuen Listen
- [ ] **E6 — Doku:** `architecture.md`, `XP_VERGLEICH.md` (302/303 auf ✅/⚠️),
      `LOGIK_PRUEFUNG.md` (AD.11 Shop, AD.12 Namenseingabe), `HANDOFF`

## 2. Bekannte Eigenheiten (nicht nochmal darüber stolpern)

- Das Menü-Unterfenster-System (`@menu_screen`, Esc-Ebenen) ist der nächste
  Verwandte des Shop-Fensters — erst dort nachsehen, dann Neues bauen.
- `Party` kennt seit 20d `@katalog` (alle Helden) und `@actors` (Mitglieder);
  Namenseingabe darf auch einen Nicht-Mitglied-Helden umbenennen (XP erlaubt
  das über `$game_actors`), der Name steht aber nicht im `party`-Snapshot,
  solange er nicht im Team ist.
- Preise gehören in die Warenliste des Befehls, nicht in `database.json`:
  XP speichert den Preis im Befehl („Preis“-Feld je Ware), die Datenbank hat
  nur den Grundpreis.
- Lokale Harness-Läufe (WASM) sind kein CI-Ersatz: `befehle_test`,
  `mapcheck_test`, `lint_repo` brauchen `Dir.chdir`/`Tempfile` und laufen nur
  in der CI. Der Harness liegt in `.cache/rbc` (nicht im Repo, `.gitignore`).
