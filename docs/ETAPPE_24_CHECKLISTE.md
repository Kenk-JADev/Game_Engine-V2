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
- [x] **T5 — Tests:** neues `tools/zustand_test.rb` (Modell, Befehle,
  Spielstand, Kampf-Übergabe), `json_test` (drei Beispiele), `CMD_TYPES.size`
  in elf Tests auf 72, `event_test` falls sinnvoll.
- [x] **T6 — Doku:** `XP_VERGLEICH.md` (313 ⚠️ → ✅, 317/320 ❌ → ✅, Bilanz,
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

## 5. Ergebnis je Aufgabe

- **T1 — Zustände am Helden:** `Party::Actor` hat `states`
  (`[{ id:, turns: }]`), `Party::MAX_STATES = 8`, `Party.state_def` (ID oder
  Name), `Party.state_names`, `Party.states`, `Party.change_state`,
  `Party.clear_states`. `Party.snapshot`/`restore!` führen die Zustände im
  Spielstand (alte Saves ohne `states` starten ohne Zustände), `heal_all` ohne
  Betrag leert sie (XP 314 `recover_all`), `Battle.start` übernimmt sie in die
  Battler (ohne `turns` gilt die Datenbank-Dauer) und `Battle.sync_actors!`
  schreibt sie zurück.
- **T2 — `state` (XP 313):** `{"cmd":"state","actor":"alle","name":"Gift",
  "mode":"add"}`; `mode` Default `add`, `turns` optional, unbekannter Zustand
  → `:unbekannt` + Issue, voller Block → `:voll` + Warnung.
- **T3 — `actor_param` (XP 317) / `actor_name` (XP 320):**
  `Party.change_param` (sechs Parameter, Maxima ≥ 1, Rest ≥ 0, HP/MP-Kappung)
  und `Party.rename` über den Interpreter; `actor_name` verlangt genau einen
  Helden.
- **T4 — Register/Editor:** Register **72**, `CMD_TYPES`, `ISSUE_CMD_TYPES`,
  `EvType`/`EV_TYPES`, `EV_TYPE_LABELS`, Feldtabellen (Zustand/Parameter/Name
  im Namensfeld, Ziel in Wert 1, Modus/Menge in Wert 2), Anzeigezeilen und
  Hinweise im F6-Dialog, MapCheck `state`/`actor_param`/`actor_name` +
  `pruefe_actor_ziel`.
- **T5 — Tests:** neues `tools/zustand_test.rb` (42 Checks, kopflos grün),
  `json_test` **882** Checks (drei Beispiele, je Typ „unberührt“, „neu
  erzeugen“ und Feldprüfung), `CMD_TYPES.size` in elf Tests auf 72;
  `befehle_test`/`sync_test` unverändert grün.
- **T6 — Doku:** `XP_VERGLEICH` (313 ✅, 317 ✅, 320 ✅; Bilanz
  **56 ✅ · 6 ⚠️ · 31 ❌**; Datenbank-Tabelle und `$data_*`-Zeilen
  geradegezogen), `architecture.md` (neuer Abschnitt + Zeilenzahl),
  `LOGIK_PRUEFUNG` (AD.20), `HANDOFF` (Absatz, Kandidaten, Commit-Tabelle).

## 6. Stolpersteine dieser Runde

- **Kommentare im `enum EvType`-Block sind verboten:** `tools/befehle_test.rb`
  liest die Namen zwischen `{` und `}` als Liste; ein `// …`-Kommentar dort
  kippt „EvType hat für jeden Typ genau einen Eintrag“ (erster Push rot). Der
  Kommentar gehört über das Enum.
- **Zwei Jobs hingen am selben Fehler:** der Windows-Job fährt dieselbe
  Ruby-Suite wie `build.ps1 -Test`, deshalb war auch er rot — die Zuordnung
  „Windows = C++“ gilt nur für Kompilierfehler.
- **Ohne installiertes Ruby testen:** `node .cache/rbc/rbtest.cjs
  tools/<test>.rb` (ruby.wasm 3.4 in `.cache/rbc`, Repo-Dateien im Speicher).
  Stolperfalle beim Bau des Runners: der Datei-Baum darf **nicht** als
  Ruby-Hash-Literal injiziert werden (`#{…}` im Quelltext wird sonst
  interpoliert) — Base64 + `JSON.parse` löst das; `puts` landet gesammelt am
  Ende der Ausgabe, `Dir.chdir` fehlt (deshalb scheitern `befehle_test` und
  `mapcheck_test` dort weiterhin).
- **zig-Syntaxcheck:** endet weiterhin mit `failed to rename … .obj`; das ist
  kein Kompilierfehler, echte Diagnosen stehen als `error: expected …` davor.
