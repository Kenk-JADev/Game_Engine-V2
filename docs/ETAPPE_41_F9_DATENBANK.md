# Etappe 41: Datenbank-Dialog F9 wie RPG Maker XP (13 Register)

Stand: Code vollständig, portable Tests und Windows-Zielkompilierung grün.
Der interaktive Windows-/DPI-Test auf dem Rechner des Nutzers steht aus
(Abschnitt „Manuelle Prüfung").

Auftrag des Nutzers: den Editor näher an RPG Maker XP bringen — als ersten
Bereich die **Datenbank F9** von vier auf alle 13 XP-Register erweitern,
mit Liste links und Formular rechts wie im Vorbild.

## XP-Recherche (vor der Umsetzung)

| Befund | Quelle |
|---|---|
| Die XP-Datenbank hat **13 Register** in fester Reihenfolge: Actors, Classes, Skills, Items, Weapons, Armors, Enemies, Troops, States, Animations, Tilesets, Common Events, System. Je Bereich gibt es eine maximale Listengröße („Change Maximum", 1…999). | Offizielle XP-Hilfe <https://www.rpg-maker.fr/dl/monos/aide/xp/source/rpgxp/database.html>, bereits ausgewertet in `docs/ETAPPE_40_RPG_DATABASE.md` |
| Ein Held verweist auf eine **Klasse** (`class_id`); die Klasse liefert Lernliste (`learnings`), Element-/Status-Ränge und erlaubte Ausrüstung. Parameter liegen als Tabelle je Stufe, die EXP-Kurve aus `exp_basis`/`exp_inflation`. | `XP-Defaultscript/011 - Game_Actor.rb` (Zeilen 15–20, 44–49, 67, 97–102, 188–234) |
| Ausrüstung besteht aus **einer Waffe und vier Rüstungsplätzen**: Schild (`armor1_id`), Helm (`armor2_id`), Körper (`armor3_id`), Accessoire (`armor4_id`). | `XP-Defaultscript/011 - Game_Actor.rb`, Zeilen 16–20 |
| Ein gemeinsames Event hat **Trigger**, **Bedingungsschalter** (`switch_id`) und eine **Befehlsliste** (`list`) — keine Seiten. | `XP-Defaultscript/017 - Game_CommonEvent.rb` (`trigger`, `switch_id`, `list`) |
| Fertigkeiten haben Zielgruppe (Scope), Kosten, Stärke und Zustandswirkung; Zustände wirken über Runden und Parameter-Boni. Rune3D bildet das mit `kind`/`scope`/`state`/`state_rate`/`turns` bzw. `poison`/`regen`/`<wert>_up`/`<wert>_down` ab. | `game/engine/battle.rb` (`skill_scope`, `apply_skill`, `state_bonus`, `zustaende_ticken`) |
| Rune3D adressiert Daten **beim Namen**, nicht bei der Nummer — Ausnahme sind die gemeinsamen Events, die wie XP eine Nummer tragen. | `game/engine/data.rb` (`common_event(name)`), `game/engine/common_events.rb` |
| Der bestehende F9-Dialog war feldgesteuert, aber auf vier Reiter (Helden/Items/Gegner/Truppen) begrenzt; Übernahme, Dateischutz und Rückfragen waren bereits datentreu. | `src/editor/editor_main.cpp` (Abschnitt F9), `src/editor/database_form.h` |

## Architekturentscheidungen

1. **Eine Datei, 13 Register.** F9 weiter auf `projekte/<name>/data/database.json`.
   Kein zweites Dokument im Dialog: der Dateischutz (Originalbytes, Sperren,
   Sicherung, Konfliktrückfrage) gilt damit unverändert für alles.
2. **Katalog statt Schaltflächen-Reihe.** `src/editor/database_catalog.h`
   beschreibt Register, Felder, Auswahlwerte, Vorgaben für neue Einträge und
   Listenbeschriftungen — ohne Win32, also kopflos prüfbar. Der alte Code mit
   vier `DB_FIELDS_*`-Tabellen und `switch`-Kaskaden ist damit abgelöst.
3. **Echte Registerkarten.** `SysTabControl32` mit `TCS_MULTILINE`: bei wenig
   Breite oder hoher DPI bricht die Leiste in mehrere Zeilen um, statt
   Beschriftungen abzuschneiden. Ein Registerwechsel übernimmt zuerst das
   offene Formular; scheitert das, bleibt das alte Register stehen.
4. **Zwei Formularspalten.** 16 Felder in zwei Spalten à acht Zeilen
   (`database_layout.h`). Gegner brauchen 14 Felder, Helden jetzt 12
   (Stufe, Gesamt-Exp und Startfertigkeiten sind neu sichtbar).
5. **System ist ein Einzeldatensatz.** Wie in XP gibt es dort keine Liste und
   kein Neu/Löschen. Die Felder liegen als Objekt `system` in
   `database.json` — **nicht** in `system.json` (Projektstart/Kartenbaum).
   Ein fehlendes Objekt entsteht erst, wenn ein Feld wirklich geändert wird.
6. **Zwei neue Feldarten.** `Choice` (Auswahl mit erlaubten Werten, im Feld
   als Klartext mit Werteliste in der Beschriftung) und `Json` (verschachtelte
   Daten als formatierter Text im Mehrzeiler). Beide bleiben datentreu:
   unbekannte Altwerte werden angezeigt und erst nach Rückfrage ersetzt.
7. **Gemeinsame Events sind jetzt vollständig editierbar.** Nummer, Name,
   Auslöser, Schalter und die **Befehlsliste als JSON**. Damit ist der in
   Etappe 20c offene Editor-Reiter für Common Events geschlossen; der
   Event-Befehl `common_event` nennt weiterhin den Namen.
8. **Ehrliche Engine-Kennzeichnung.** Jedes Register sagt in der Hinweiszeile,
   ob das Spiel die Daten heute liest. `classes`, `weapons`, `armors`,
   `animations`, `tilesets`, `system` sind **vorbereitet**: Der Editor pflegt
   sie verlustfrei, die Engine wertet sie noch nicht aus (Backlog).
9. **Startgold bleibt ein Wurzelfeld** und ist in jedem Register sichtbar —
   es gehört keiner Sammlung und wird unabhängig von der Auswahl übernommen.

## Register und Felder

| # | Register | JSON-Schlüssel | Felder | Engine |
|---|---|---|---|---|
| 1 | Helden | `actors` | 12 (neu: Stufe, Exp, Fertigkeiten) | liest |
| 2 | Klassen | `classes` | 6 | vorbereitet |
| 3 | Fertigkeiten | `skills` | 10 | liest |
| 4 | Items | `items` | 7 | liest |
| 5 | Waffen | `weapons` | 9 | vorbereitet |
| 6 | Rüstungen | `armors` | 9 (slot: shield/head/body/accessory) | vorbereitet |
| 7 | Gegner | `enemies` | 14 (neu: Farbe, Beschreibung) | liest |
| 8 | Truppen | `troops` | 4 | liest |
| 9 | Zustände | `states` | 6 | liest |
| 10 | Animationen | `animations` | 8 | vorbereitet |
| 11 | Tilesets | `tilesets` | 6 | vorbereitet |
| 12 | Gem. Events | `common_events` | 5 (Befehle als JSON) | liest |
| 13 | System | `system` (Objekt) | 6 | vorbereitet |

Auswahlwerte kommen aus der Engine, nicht aus dem Bauch:

- `skills.kind` = `damage/heal/buff`, `skills.scope` = `enemy/ally/self`
  (`Battle.skill_scope`, `apply_skill`)
- `states.kind` = `poison/regen` plus `atk|def|spi|agi` je `_up`/`_down`
  (`Battler#state_bonus`)
- `items.kind` = `heal/mana/quest/money` (`Party.use_item`; nur heal/mana wirken)
- `common_events.trigger` = `none/autorun/parallel` (`CommonEvents::TRIGGER`)

## Aufbau

```text
src/editor/database_catalog.h    13 Register, Felder, Auswahlwerte, Vorgaben, Listenzeilen
src/editor/database_layout.h     Geometrie in 96-DPI-Einheiten (zwei Spalten, großes Feld)
src/editor/database_form.h       + Choice/Json, System-Variante von prepare/commit
src/editor/editor_main.cpp       F9: Registerkarten, Katalog-Anbindung, JSON-Mehrzeiler
tools/json_test.cpp              +56 portable Checks (Katalog, Feldarten, Geometrie)
tools/sync_test.rb               Katalog gegen database.json und Engine-Vokabular
tools/database_register_test.rb  26 Checks: 13 Register gegen die echte Engine
game/engine/mapcheck.rb          leeres Schalterfeld ist kein Fehler mehr
```

## Eine Engine-Nacharbeit (gehört zur Etappe)

F9 schreibt ein leeres Schalterfeld eines gemeinsamen Events als `""`
(„leer = immer"). `CommonEvents.aktiv?` wertet das längst wie „kein Schalter",
`MapCheck` nannte es aber einen Fehler — die Vorgaben des Editors hätten also
sofort einen F10-Fund erzeugt. `MapCheck::Sammler#gemeinsame_events` prüft
jetzt nur noch, dass `switch` ein Text ist; leer ist erlaubt, ein Nicht-Text
(Zahl, Objekt, `null`) bleibt ein Fehler. Die Warnung „autorun ohne Schalter
läuft dauerhaft" bleibt bestehen. Gefunden und abgesichert durch
`tools/database_register_test.rb`.

## CI-Ergebnis

[Lauf 45981479255](https://github.com/Kenk-JADev/Game_Engine-V2/actions/runs/45981479255)
(`945cf8f`) ist **grün in allen vier Jobs**: Ruby-Engine 58 OK / 0 Fehler /
1 übersprungen (ohne gebaute EXE), Windows-Build **59 OK / 0 Fehler /
0 übersprungen**, dazu WARP 39, Windows-Pfad 39, Dokument-Datei 58, Material 57,
Editor-Arbeitsablauf 67 und die eingebettete Texturbrücke 7/40 — jeweils
0 Fehler. Der Windows-Lauf beweist den Bau und die kopflosen Tests, **nicht**
das Aussehen von F9: die Sicht-/DPI-Prüfung unten bleibt offen.

## Zwei Altfehler der CI mitbehoben

Der erste CI-Lauf zu dieser Etappe war rot — aber nicht wegen F9: dieselben
zwei Tests scheitern schon auf `main`
([Lauf 45973867530](https://github.com/Kenk-JADev/Game_Engine-V2/actions/runs/45973867530),
Commit `7d4023a`). Beide sind jetzt behoben, damit die Etappe auf grünem Grund
steht:

1. **`event_commands_test.rb` — „Programm 1 läuft bis zum Ende".** Zwei echte
   Fehler in der Etappe-40b-Schicht:
   - `Rpg::EventCommands::Interpreter#wende_an` setzte bei `:wait` nur die
     Wartezeit und ließ den Befehlsindex stehen. Nach Ablauf startete also
     derselbe Wartebefehl erneut — eine Endlosschleife. XP erhöht den Index
     nach jedem ausgeführten Befehl (`063 - Interpreter 1.rb`, „Advance
     index"); das tut der Interpreter jetzt auch. Zusätzlich bekommt ein
     frisch gesetztes Warten das Delta desselben Updates ab, wie XP, dessen
     Schleife im nächsten Durchgang sofort `@wait_count` herunterzählt.
   - `Handlers` übergab bei `change_items`/`change_weapons`/`change_armors`
     die *Operation* (`add`) als *Sammlung* an `IInventoryPort#change`. Der
     Bestand landete unter `["add", 3]`, `count("items", 3)` fand nichts.
     Der Port nimmt die Sammlung jetzt als ersten Parameter und die Richtung
     als Schlüsselwort `operation:`; `MemoryInventoryPort` protokolliert
     beides. Ein Testaufruf nutzte außerdem `ports.actor` statt `ports.actors`.
2. **`runtime_boot_test.rb` — „echter Bootstrap startet auf Scene_Title ohne
   Ingame-Editor".** Der Test verlangt, dass `game/engine/editor.rb` nicht
   mehr im Laufzeitbaum liegt (Etappe 19t). Die Datei lag aber noch dort:
   691 Zeilen alter Ingame-Editor, von `boot.rb` nicht geladen, vom
   eigenständigen RuneEditor nicht benutzt (`game/editor/boot.rb` braucht nur
   `textures` und `sounds`). Sie ist jetzt entfernt. Die nützlichen Prüfungen
   des alten `tools/editor_test.rb` (Prefabs, Instantiierung, Kristall-
   Animation, Dorf-Daten, prozedurale Texturen) leben weiter in
   **`tools/prefabs_test.rb`** — 23 übernommene Checks plus der neue Check
   „kein editor.rb im Laufzeitbaum", zusammen 24. Die reinen Editor-Checks (setzen/löschen/
   speichern des Ruby-Ingame-Editors) entfallen mit dem Code.

## Stufenplan

| Stufe | Inhalt | Prüfung | Status |
|---|---|---|---|
| 41.1 | XP-Recherche (13 Register, Klassen, Rüstungsplätze, Common Events) | Quellentabelle oben | erledigt |
| 41.2 | Portabler Register-Katalog mit Vorgaben und Listenbeschriftung | `json_test` | erledigt |
| 41.3 | Feldarten Choice und Json, System als Einzeldatensatz | `json_test` | erledigt |
| 41.4 | F9-Oberfläche: Registerkarten, zwei Spalten, Hinweiszeile | Windows-Zielkompilierung | erledigt |
| 41.5 | Vokabelabgleich Katalog ↔ Engine ↔ Demo-Daten | `sync_test` | erledigt |
| 41.6 | Dokumentation, Backlog, Handoff | diese Datei | erledigt |
| 41.7 | Zwei Altfehler der CI behoben (Event-Wait/Inventar, Ingame-Editor-Rest) | `event_commands_test`, `runtime_boot_test`, `prefabs_test` | erledigt |
| 41.8 | Interaktiver Windows-/DPI-Test | Anleitung unten | **offen (Nutzer)** |

## Prüfungen in dieser Etappe

- `g++ -std=c++20 -O1 -Wall -Wextra -Werror -Isrc src/editor/json.cpp tools/json_test.cpp`
  → **1031 Checks, 0 Fehler** (Linux, ohne Win32-Teil; 56 davon neu).
- `ruby tools/sync_test.rb` → **0 Fehler**, darunter 12 neue Register-/Vokabelchecks.
- `ruby tools/database_register_test.rb` → **26 Checks, 0 Fehler** (neu): eine
  Datenbank mit allen 13 Registern lädt, die vorbereiteten Register bleiben
  wirkungslos, die F9-Vorgaben sind spielbar (Item/Fertigkeit/Zustand/Kampf),
  MapCheck meldet dafür keine Fehler und alte Projekte laufen unverändert.
- Gegenprobe nach der MapCheck-Änderung: `common_events_test`, `party_test`,
  `battle_test`, `zustand_test`, `event_test` → alle grün.
- `zig c++ -target x86_64-windows-gnu -c -std=c++20 -Wall -Wextra … src/editor/editor_main.cpp`
  → Objekt erfolgreich gebaut, nur die **bestehende** Warnung in Zeile 3588
  (`item.pszText`, Objektliste). Das ist eine Zielkompilierung, **kein**
  Windows-Lauf und kein GUI-Test.
- `ruby tools/lint_repo.rb` → JSON, UTF-8/BOM, Zeilenenden und Requires ohne
  Befund; der Git-Abschnitt braucht ein echtes System und läuft in der CI.
- Nach der Altfehler-Nacharbeit: `event_commands_test` → **107 Prüfungen,
  0 Fehler**, `prefabs_test` → **24 Checks, 0 Fehler**, `runtime_boot_test`
  → der zuvor rote Bootstrap-Check ist grün. Alle 59 Ruby-Testdateien wurden
  einzeln gefahren; die übrigen Ausfälle in dieser Sandbox sind ausschließlich
  WASI-Grenzen (`Dir.tmpdir`, `flock`, `Open3`, fehlende `RuneEditor.exe`).

## Manuelle Prüfung (echter Windows-Rechner, offen)

Mit einer **gesicherten Kopie** eines Projekts arbeiten, nicht am Original.

1. **Register:** F9 öffnen. Sind alle 13 Karten in XP-Reihenfolge sichtbar?
   Bei 1024×640 sowie 100/125/150/200 % Skalierung prüfen, ob die Leiste
   umbricht und jede Beschriftung lesbar bleibt. Registerwechsel mit Tab und
   Maus, auch mit ungespeicherten Eingaben.
2. **Offenes Formular:** Feld ändern, dann Register wechseln und die
   Rückfrage/Validierung beantworten. Bei ungültiger Eingabe muss das alte
   Register stehen bleiben und der Text erhalten bleiben.
3. **Daten:** In „Gegner“ Farbe und Beschreibung ändern, in „Helden“ Stufe,
   Exp und Fertigkeiten. Strg+S bzw. Speichern, danach JSON vergleichen:
   nur die geänderten Felder, unbekannte Zusatzfelder und große Ganzzahlen
   unverändert, `JSON.pretty_generate`-Format.
4. **Auswahlfelder:** `skills.scope` auf einen ungültigen Wert setzen →
   Ablehnung mit Werteliste und Fokus im Feld. Einen alten unbekannten Wert
   (per Ruby-Skript injected) unverändert lassen → keine Änderung; ändern →
   Rückfrage.
5. **Gemeinsame Events:** Neues Event anlegen (Nummer wird frei gewählt),
   Auslöser `autorun` + Schalter setzen, Befehlsliste als JSON eintragen,
   speichern und im Spiel prüfen (`CommonEvents.tick`). Kaputtes JSON muss
   abgelehnt werden, ohne die Liste zu verändern.
6. **System:** Register ohne Liste; Neu/Löschen gesperrt. Ein Feld setzen →
   `system` entsteht in `database.json`, `system.json` bleibt unangetastet.
   Vorhandenes kaputtes `system` (Text statt Objekt) nur nach Rückfrage ersetzen.
7. **Dateischutz:** Datei extern ändern, dann speichern → Konfliktrückfrage;
   Nein lässt die Datei stehen. Auch Neu laden, Schließen und Projektwechsel
   mit ungespeicherten Änderungen prüfen.
8. **F2-Zusammenspiel:** F9 aus F2 öffnen, Truppe anlegen, speichern, in F2
   „Neu laden“ — die neue Truppe muss erscheinen.

Ergebnisse erst nach tatsächlicher Ausführung hier eintragen; ein grüner
CI-Lauf ersetzt diesen Test nicht.

## Bewusst nicht verändert

- Keine Änderung an `game/data/database.json`, Karten, Spielständen oder
  `system.json`. Alte Projekte laden unverändert; neue Register entstehen
  erst beim Anlegen eines Eintrags.
- Keine neue Engine-Funktion: die sechs vorbereiteten Register haben bewusst
  noch keine Spielwirkung (Backlog-Eintrag „Sechs F9-Register haben noch keine
  Spielwirkung"). Die einzige Engine-Änderung ist die MapCheck-Regel für das
  leere Schalterfeld (oben).
- Kein „Change Maximum" wie in XP (Listengröße direkt ändern) — Neu/Löschen
  bleibt der Weg; ein Größenfeld wäre ein eigener Datenverlust-Dialog.
- Keine Dropdown-Steuerelemente: Auswahlfelder bleiben Textfelder mit
  Werteliste in der Beschriftung, damit unbekannte Altdaten sichtbar und
  verlustfrei bleiben.
- Der alte Ingame-Editor existiert nicht mehr — auch die Restdatei
  `game/engine/editor.rb` ist jetzt entfernt (oben). F9 gibt es nur im RuneEditor.
