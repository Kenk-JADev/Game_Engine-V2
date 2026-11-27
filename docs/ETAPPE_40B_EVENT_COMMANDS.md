# Etappe 40b: Modulare Event-Command-Architektur (XP-Tabs 1–3)

Stand: Code vollständig; Build und Prüfungen werden manuell durch den Nutzer
gestartet. Diese Etappe ergänzt eine **eigene**, typisierte und
testbare Event-Command-Schicht. RPG Maker XP dient nur als Strukturvorbild für
Befehlsgruppen, Reihenfolge und Laufzeitverhalten; Namen, IDs, Asset-Keys und
Implementierung sind eigen.

## XP-Recherche (vor der Umsetzung)

| Befund | Quelle |
|---|---|
| Es gibt drei Event-Arten (Map, Battle, Common). Karten-Events haben Seiten mit zwei Switches, einer Variable und einem Self-Switch A–D; Trigger sind Action Button, Player Touch, Event Touch, Autorun, Parallel Process. Common Events haben keine Seiten, sondern None/Autorun/Parallel. | Offizielle XP-Hilfe <https://www.rpg-maker.fr/dl/monos/aide/xp/source/rpgxp/event.html> |
| Die offizielle Hilfe nennt **90 Event-Befehlstypen** und beschreibt die Bearbeitung als Liste von Befehlen. Show Text unterstützt Steuerzeichen `\V[n]`, `\N[n]`, `\C[n]`, `\G`, `\\`. | <https://www.rpg-maker.fr/dl/monos/aide/xp/source/rpgxp/event.html> |
| Der XP-Interpreter dispatcht numerische Codes: 101–136 (Tab 1), 201–251 (Tab 2), 301–355 (Tab 3) sowie Fortsetzungs-/Zweig-Codes 402, 403, 411, 413 und Kampf-Ergebnis-Codes 601–603. | `XP-Defaultscript/064 - Interpreter 2.rb` |
| `setup(list, event_id)` hält eine flache Liste mit `indent` und `index`; `update` verarbeitet bis 100 Befehle pro Frame, blockiert bei Text, Bewegungsroute, Tasten-/Zahleneingabe, Wait und laufenden Screen-Calls. Common Events laufen als Child-Interpreter, die Aufruftiefe ist auf 100 begrenzt. | `XP-Defaultscript/063 - Interpreter 1.rb` |
| Verzweigungen merken sich den Zweig pro Einrückungsebene (`@branch`); `Else` (411) und `command_skip` überspringen Blöcke gleicher Tiefe. `Loop`/`Repeat Above` (112/413) arbeiten über `indent`; `Break Loop` (113) sucht das nächste Repeat. Label (118) und Jump (119) suchen innerhalb der Liste. | `XP-Defaultscript/065 - Interpreter 3.rb` |
| Die Befehle verändern ausschließlich `$game_*`-Zustände (Switches, Variables, Party, Inventar, Map, Screen, Battle) und melden Effekte über `$game_temp`; es gibt keine eigene Befehls-Datenbank im heutigen Sinn. | `XP-Defaultscript/066 - Interpreter 4.rb` bis `069 - Interpreter 7.rb` |
| Bestehende Rune3D-Eventseiten nutzen bereits JSON-Befehle mit `cmd`-String und den Registern `docs/befehle.json` / `MapCheck::CMD_TYPES` / `EV_TYPES`. Diese Wege müssen unverändert bleiben. | `game/engine/interpreter.rb`, `tools/befehle_test.rb`, `src/editor/event_form.h` |

## Architekturentscheidungen

1. **Additive Schicht:** Die neue Architektur liegt unter `Rpg::EventCommands`
   und wird über `game/engine/rpg.rb` geladen. Sie ersetzt weder den
   bestehenden `Interpreter` noch die Editor-Register `befehle.json`,
   `EV_TYPES` oder F5/F6/Undo/Dateischutz.
2. **90 typisierte Befehle:** Alle drei XP-Tabs werden mit eigenen
   `commandType`-Keys, numerischen `code`-Werten und der XP-Spalte
   (`column: :left` / `:right`) abgebildet. Je Tab stehen 15 Befehle links
   und 15 rechts; die Codes dienen nur der Zuordnung/Doku, nicht als
   String-Parser.
3. **Serialisierbare Kommandos:** `Command` ist ein unveränderliches
   `Data.define` mit `commandType`, `code`, `indent`, `params` und `children`.
   `type` bleibt als lesbarer Alias erhalten. Verschachtelungen
   (Conditional Branch, Loop, Choices, Battle-Ergebnis) sind typisierte
   Kinderblöcke; `indent` bleibt als prüfbare Pflichttiefe erhalten.
   Strukturierte Parameter decken Bereiche (`from_id`/`to_id`), Bild-Slots
   (`picture_slot_id`), Shop-Waren (`goods`), Wetterdauer, Party-Initialisierung
   und Move-Route-Ignore ab.
4. **Katalog statt Array-Semantik:** `Catalog` beschreibt pro Typ Tab, Gruppe,
   Felder, erlaubte Werte/Referenzen und Kinderblöcke. Der Loader validiert
   daraus alle Kommandos gesammelt und verhindert halb geladene Programme.
5. **Handler-Registry:** Der Interpreter kennt keine `case`-Kaskade. Jeder
   Typ wird über seine `handler`-Kennung auf einen Strategy-Handler abgebildet;
   `register(type, handler)` erlaubt Erweiterungen ohne Interpreter-Änderung.
6. **Ports statt Engine-Kopplung:** Switches, Variablen, Self-Switches, Timer,
   Inventar, Party, Karte, Audio, Screen, Bilder, Wetter, Übergang, Kampf, UI
   und Script werden über injizierte Interfaces angesprochen. Null- und
   Recording-Adapter machen den Interpreter ohne Grafik/UI/Audio testbar.
7. **Eigene Werte:** Schalter 101 `Tor_Offen`, Variable 202
   `Quest_Phase`, Self-Switch `A`, BGM `bgm_tavern_night`, SE
   `se_door_latch`, Bild `pic_crest_seal`, Karte 12 bei (8, 14), Gegner-Slot 0.
8. **Stack statt Globalzustand:** Call Common Event erzeugt einen Sub-Frame
   mit Tiefenlimit; Wait-, Dialog-, Moves- und Übergangszustände liegen im
   Interpreter, Label/Jump und Break Loop arbeiten auf Frame-Ebenen.

## Aufbau

```text
game/config/event_commands.json          Katalogwerte, Ressourcen, Beispielprogramme
game/engine/rpg/event_commands.rb        Einstieg und Modulmethoden
game/engine/rpg/event_commands/support.rb      Fehler, Reporter, Dateisystem
game/engine/rpg/event_commands/models.rb       Command/Program/Field/Definition
game/engine/rpg/event_commands/catalog.rb      90 Befehlsdefinitionen
game/engine/rpg/event_commands/ports.rb        Interfaces, Null-/Recording-Adapter
game/engine/rpg/event_commands/handlers.rb     Strategy-Handler + Registry
game/engine/rpg/event_commands/loader.rb       JSON -> Modelle + Validierung
game/engine/rpg/event_commands/interpreter.rb  Frame-Stack, Wait, Verzweigung
tools/event_commands_test.rb             Tests der Schicht
```

## Stufenplan

| Stufe | Inhalt | Prüfung | Status |
|---|---|---|---|
| 40b.1 | XP-Recherche und Doku | Quellentabelle oben | erledigt |
| 40b.2 | Katalog aller 90 Befehle, eigene Ressourcen und Beispielprogramme | Katalog-/Loader-Test | umgesetzt |
| 40b.3 | Unveränderliche Command-Modelle, Loader und Gesamtvalidierung | Negative Tests | umgesetzt |
| 40b.4 | Ports, Handler-Registry und Interpreter inkl. Wait/Stack/Labels | Interpreter-Tests | umgesetzt |
| 40b.5 | Einbindung in `rpg.rb`, Gesamtprüfung | TestSuite/Lint/Build | Code fertig; manuell zu prüfen |

## Nachtrag aus Etappe 41: zwei Fehler dieser Schicht behoben

`tools/event_commands_test.rb` war seit dem Stand `7d4023a` rot und ist jetzt
grün (107 Prüfungen). Zwei echte Fehler, keine Testkosmetik:

- **Wait blieb auf demselben Befehl stehen.** `Interpreter#wende_an` setzte bei
  `:wait` nur `@wait`, ohne `frame.index` zu erhöhen. Nach Ablauf wurde
  derselbe Wartebefehl erneut ausgeführt — das Programm endete nie. XP erhöht
  den Index nach jedem ausgeführten Befehl (`063 - Interpreter 1.rb`,
  „Advance index"). Zusätzlich zählt XP ein frisch gesetztes Warten schon im
  selben Update herunter; `update` gibt deshalb das Delta dieses Updates an
  `tick_wait` weiter, bevor es wegen Wartens abbricht.
- **Inventar landete unter falschem Schlüssel.** `Handlers#inventory` rief
  `ports.inventory.change(operation, id, amount, include_equipment)` — der
  Port behandelte den ersten Parameter aber als Sammlung. Der Bestand wurde
  unter `["add", 3]` geführt, `count("items", 3)` blieb 0. Der Port heißt jetzt
  `change(kind, id, amount, include_equipment, operation: 'add')`, der Handler
  übergibt `items`/`weapons`/`armors` und die Richtung getrennt, und
  `MemoryInventoryPort` protokolliert `operation` mit.

## Manuelle Prüfung

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\pruefe_alles.ps1
```

Das Skript erkennt neue Dateien automatisch: `tools/test_all.rb` findet alle
`tools/*_test.rb` per Glob, `pruefe_alles.ps1` findet `tools/lint_*.rb` und
`tools/*_smoke.ps1` ebenfalls per Glob, und `build.ps1` kompiliert alle
`src/**/*.cpp`. Neu eingebaute Dateien müssen daher in keiner Liste mehr
nachgetragen werden. Alle Schritte laufen auch nach einem Fehler weiter; die
Fehler werden gesammelt, gemeinsam angezeigt und in `log\pruefe_alles_*.log`
geschrieben. Bei Fehlern wartet das Skript am Ende auf ENTER. Falls die
Rechte nicht ausreichen, startet es sich einmal per UAC als Administrator neu.

## Bewusst nicht verändert

- Kein neuer `cmd`-String in `docs/befehle.json`, `MapCheck::CMD_TYPES`,
  `EV_TYPES` oder `ISSUE_CMD_TYPES`; F6 und der Issue-Scan bleiben identisch.
- Keine Änderung an `game/data/maps/*.json`, `game/data/database.json` oder
  vorhandenen Eventseiten. Die neue Schicht hat eine eigene Konfigurationsdatei.
- Keine 1:1-Kopie der XP-Skripte; nur dokumentierte Struktur und Semantik.
- Keine Singletons und kein Zugriff auf `$data_*`, `$game_*`, UI oder Renderer.
