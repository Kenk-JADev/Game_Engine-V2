# Etappe 24 — Checkliste (Zustände und Werte von Helden im Event)

Diese Checkliste ist die Abhakliste für Etappe 24. Stand: **Runde 42 — Plan
steht, Umsetzung beginnt**.

Legende: `[x]` erledigt und committet · `[~]` erledigt, CI offen · `[ ]` offen

**Vorgänger:** Etappe 23 (Musik-Befehle 243/244/247/248) ist **abgeschlossen** —
Register **69 Typen**, CI grün, letzter Commit `e0710b7`.

**Ziel:** Der Helden-Block der Event-Befehle ist bis auf drei Lücken komplett:
**313** (Zustand ändern), **317** (Parameter ändern) und **320** (Name ändern)
fehlen, obwohl Datenbank (`states`), Party-Modell (Werte, `Party.rename`) und
Kampf (Zustände mit Dauer) längst da sind. Etappe 24 schließt die drei und
räumt dabei drei veraltete ❌-Zeilen in `XP_VERGLEICH.md` auf (States, Common
Events, `$data_*`).

## 1. Aufgaben

- [x] **T1 — Zustände am Helden:** `Party::Actor` bekommt `states`
  (`[{ id:, turns: }]` wie der Kampf-Battler, max. 8), `Party.state_def`,
  `Party.change_state(ziel, zustand, mode:, turns:)` (add/remove, Ziel „alle"
  oder ein Held), Anzeige `state_names`. Spielstand führt die Zustände je Held;
  `heal` (XP 314) entfernt sie wie XP `recover_all`. `Battle.start` übernimmt
  die Zustände in die Battler, `sync_actors!` schreibt sie zurück.
- [x] **T2 — Befehl `state` (XP 313):** `{"cmd":"state","actor":"alle",
  "name":"gift","mode":"add"}`; Ziel, Zustand (ID oder Name) und Modus;
  optional `turns`. Unbekannter Zustand = Warnung im Log + Issue.
- [x] **T3 — Befehle `actor_param` (XP 317) und `actor_name` (XP 320):**
  `{"cmd":"actor_param","actor":"arik","param":"max_hp","amount":10}` (max_hp/
  max_mp/atk/def/spi/agi, vorzeichenbehaftet, Grenzen 1 bzw. 0 … 9 999 999,
  HP wird bei sinkendem Maximum gekappt) und `{"cmd":"actor_name","actor":
  "arik","name":"Arik der Kühne"}` über `Party.rename`.
- [x] **T4 — Register und Editor:** 69 → **72** Typen in `docs/befehle.json`,
  `CMD_TYPES`, `ISSUE_CMD_TYPES`, `EvType`/`EV_TYPES`, `EV_TYPE_LABELS`,
  Feldtabellen/Formular (Ziel, Zustand/Parameter/Name, Modus/Menge),
  MapCheck-Regeln.
- [~] **T5 — Tests:** neues `tools/zustand_test.rb` (Modell, Befehle,
  Spielstand, Kampf-Übergabe), `json_test` (drei Beispiele), `CMD_TYPES.size`
  in elf Tests auf 72, `event_test` falls sinnvoll.
- [~] **T6 — Doku:** `XP_VERGLEICH.md` (313 ⚠️ → ✅, 317/320 ❌ → ✅, Bilanz,
  dazu die veralteten Zeilen States/Common Events/`$data_*`),
  `architecture.md`, `LOGIK_PRUEFUNG.md`, `HANDOFF`, dieser Abschnitt.

## 2. Definition of Done

Ein Punkt ist fertig, wenn er im Kopf kommentiert ist, einen kopflosen Test
hat, im Register nichts kaputt macht und die CI auf allen vier Jobs grün ist
(Ruby, C++-Logik, Windows-Build inkl. `-Werror`, Hygiene). Der Editor wird vor
dem Push mit `zig c++ -target x86_64-windows-gnu -fsyntax-only` geprüft.

## 3. Bekannte Eigenheiten

- Zustände außerhalb des Kampfes haben **keine Rundenzählung**: `turns` bleibt
  gespeichert und läuft erst im Kampf ab (XP: `Game_Battler#update_state_turn_effect`).
- `heal` (XP 314) heißt in Rune3D weiter `heal` und wirkt auf die ganze Party;
  Zustände werden dabei entfernt (XP `recover_all`).
- Die drei neuen Befehle nennen das Ziel im Schlüssel `actor` (nicht `target`
  wie 311/312); neu angehängte Typen stehen wie immer am **Ende** des
  Registers, damit Diffs lesbar bleiben.

## 4. Reihenfolge

T1 (Modell) → T2 (ein Befehl als Muster) → T3 (die zwei einfachen) →
T4 (Register/Editor) → T5 (Tests) → T6 (Doku).
