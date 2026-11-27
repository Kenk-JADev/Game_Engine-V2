# Etappe 40: Modulare RPG-Datenbankarchitektur nach XP-Vorbild

Stand: Engine-Schicht abgeschlossen und geprüft; die editorische F9-Oberfläche
bleibt offen. Diese Etappe baut eine **eigene**, typisierte und
testbare Datenbankschicht für Rune3D. RPG Maker XP dient nur als
Strukturvorbild: Feldgruppen, Reihenfolge und Zusammenhänge werden nachgebaut,
die konkreten Werte, Bezeichnungen, Asset-Keys und Formeln sind eigene.

## XP-Recherche (vor der Umsetzung)

| Befund | Quelle |
|---|---|
| Die XP-Datenbank hat 13 Register: Actors, Classes, Skills, Items, Weapons, Armors, Enemies, Troops, States, Animations, Tilesets, Common Events, System. Jeder Bereich hat eine maximal änderbare Listengröße (1…999); das Anpassen erhöht/verkleinert die Array-Länge. | Offizielle XP-Hilfe <https://www.rpg-maker.fr/dl/monos/aide/xp/source/rpgxp/database.html> |
| `RPG::Actor` hält `class_id`, `initial_level`, `final_level`, `exp_basis`/`exp_inflation` (10…50), `character_name`/`battler_name`, `parameters` als `Table[6,100]` (MaxHP, MaxSP, STR, DEX, AGI, INT) und die fünf Start-Equipment-Slots inklusive Fixed-Flags. | <https://www.rpg-maker.fr/dl/monos/aide/xp/source/rgss/gc_rpg_actor.html> |
| `RPG::Class` hält Kampfposition (0 Front, 1 Mitte, 2 Hinten), erlaubte Waffen-/Rüstungs-IDs, Element-/Status-Ränge und eine Lernliste (`level` → `skill_id`). | <https://www.rpg-maker.fr/dl/monos/aide/xp/source/rgss/gc_rpg_class.html> |
| `RPG::Skill` hält Scope 0…7, Occasion 0…3, zwei Animations-IDs, Menü-SE, Common Event, SP-Kosten, Power, F-Faktoren (`atk_f`, `eva_f`, `str_f`, `dex_f`, `agi_f`, `int_f`), Trefferquote, `pdef_f`/`mdef_f`, Varianz sowie Element- und Status-Sets. | <https://www.rpg-maker.fr/dl/monos/aide/xp/source/rgss/gc_rpg_skill.html> |
| `RPG::Enemy` hält Battler-Grafik, die acht Kampfwerte, zwei Animations-IDs, Element-/Status-Ränge, Prioritäts-Aktionen mit Bedingung sowie EXP/Gold/Drop inklusive Wahrscheinlichkeit. `RPG::Troop` hält platzierte Mitglieder und bedingte Kampf-Event-Seiten. | <https://www.rpg-maker.fr/dl/monos/aide/xp/source/rgss/gc_rpg_enemy.html>, <https://www.rpg-maker.fr/dl/monos/aide/xp/source/rgss/gc_rpg_troop.html> |
| `RPG::State` hält Restriktion 0…4, Flags (Nonresistance, HP-0, keine EXP, kein Evade, Slip Damage), Rating, prozentuale Parameter-Raten, Element-/Status-Sets und Release-Bedingungen (`hold_turn`, `auto_release_prob`, `shock_release_prob`). | <https://www.rpg-maker.fr/dl/monos/aide/xp/source/rgss/gc_rpg_state.html> |
| `RPG::Animation` hält Sheet-Key, Position 0…3 (Top/Mitte/Unten/Bildschirm), Frame-Anzahl, Sprite-Zellen und Timing-Tabelle (Frame, SE, Flash, Farbe, Dauer). `RPG::Tileset` hält Grafik-Keys, Autotiles sowie Tabellen für Passage (inkl. 4 Richtungen, Bush, Counter), Priorität und Terrain-Tag. | <https://www.rpg-maker.fr/dl/monos/aide/xp/source/rgss/gc_rpg_animation.html>, <https://www.rpg-maker.fr/dl/monos/aide/xp/source/rgss/gc_rpg_tileset.html> |
| `RPG::System` bündelt Startparty, Element-/Schalter-/Variablenlisten, Systemgrafiken, BGM/ME/SE und das Vokabular als zentrale Begriffe. | <https://www.rpg-maker.fr/dl/monos/aide/xp/source/rgss/gc_rpg_system.html> |
| XP lädt die Daten beim Start in `$data_*`-Tabellen; laufende Kämpfe/Karten verändern Kopien in `$game_*`, nicht die Vorlagen. | <https://www.rpg-maker.fr/dl/monos/aide/xp/source/rgss/g_rpg_data.html>, `docs/XP_RECHERCHE.md` |

Die offizielle XP-Doku nennt 13 Register, der Auftrag spricht von „11 Bereichen“;
umgesetzt werden **alle aufgezählten 13 Bereiche** – die drei zusätzlichen sind
Common Events, System und die Aufspaltung von Skills/Items/Equipment.

## Architekturentscheidungen

1. **Externe Konfiguration:** `game/config/rpg_database.json` ist die einzige
   Datenquelle. `Rpg::Loader` bekommt `file_system:` und `reporter:` injiziert
   und kennt keine globale Datenbank. Der bestehende `Database`-Pfad für
   `data/*.json` bleibt unangetastet; die neue Schicht ist additiv.
2. **Unveränderliche Vorlagen:** Alle Datenmodelle sind `Data.define`-Klassen,
   deren Konten und verschachtelte Arrays/Hashes tief eingefroren werden.
   Repositories geben nur Lesekopien heraus. Es gibt keine Setter auf Templates.
3. **Eigene Werte:** Kael/Vanguard, Mira/Arcanist, Orin/Warden; Ashen Wraith,
   Rift Drake, Construct Sentinel; Pyro/Frost/Storm/Terra/Aether/Shadow;
   Vital Tonic, Ether Vial, Rune Salve. Asset-Keys liegen im eigenen Schema
   (`char/…`, `battler/…`, `icon/…`, `fx/…`, `bgm/…`, `se/…`, `me/…`).
4. **Laufzeit vs. Vorlage:** `RuntimeActor`, `RuntimeEnemy`, `ActiveState` und
   `MapTileState` halten veränderlichen Spielzustand. Sie werden aus Vorlagen
   erzeugt und können niemals ein Template überschreiben.
5. **Services statt Globalfunktionen:** `CombatFormulaService`,
   `DamageCalculator`, `StateResolver`, `EquipmentStatAggregator`,
   `TileQueryService`, `EventExecutionService` und `AudioService` werden
   explizit mit ihren Abhängigkeiten konstruiert. `EventDispatcher` und
   `AudioPort` koppeln Effekte an die Engine, ohne Grafik-/Audio-Code in
   Datenmodelle zu ziehen.
6. **Validierung als Startgate:** Der Loader meldet fehlende Pflichtfelder,
   doppelte IDs, ungültige Enums, fehlende Querverweise und unbekannte
   Asset-Keys gesammelt. Ein `ValidationError` verhindert das Benutzen eines
   halb geladenen Katalogs.
7. **Listengrößen:** `Rpg::Database#resize(area, size)` liefert eine neue
   Datenbank mit veränderter Repository-Länge; die alte bleibt unverändert.
   Damit ist „Change Maximum“ funktional nachgebaut, ohne Mutations-Seiten­effekte.

## Aufbau

```text
game/config/rpg_database.json        eigene Beispieldaten (13 Bereiche)
game/engine/rpg.rb                   Namespace + zentrale Requires
game/engine/rpg/support.rb           Deep-Freeze, Typprüfung, Fehlerklassen
game/engine/rpg/models.rb            alle unveränderlichen Datenmodelle
game/engine/rpg/ports.rb             Audio-/Event-Ports, Dispatcher, Fake-Ports
game/engine/rpg/loader.rb            JSON → Modelle + Validierung
game/engine/rpg/repositories.rb      schreibgeschützte Kataloge + resize
game/engine/rpg/runtime.rb           RuntimeActor/Enemy/ActiveState/MapTileState
game/engine/rpg/services.rb          Kampf-, Zustands-, Tile- und Eventlogik
tools/rpg_database_test.rb           Tests der Schicht
```

## Stufenplan

| Stufe | Inhalt | Prüfung | Status |
|---|---|---|---|
| 40.1 | XP-Recherche und Doku | Quellentabelle oben | erledigt |
| 40.2 | Externes JSON mit allen 13 Bereichen und eigenen Werten | Loader-Test | umgesetzt |
| 40.3 | Typisierte, unveränderliche Modelle + Repositories | Frozen-/Resize-Test | umgesetzt |
| 40.4 | Loader mit Pflichtfeld-, Duplikat-, Querverweis- und Asset-Prüfung | Negative Tests | umgesetzt |
| 40.5 | Runtime-Instanzen und Service-Dispatch | Service-Tests | umgesetzt |
| 40.6 | Einbindung in `boot.rb`, Gesamtprüfung | TestSuite/Lint/Build | ✅ erledigt |

## Abschlussprüfung

- `ruby tools/rpg_database_test.rb`: 92 Prüfungen, 0 Fehler.
- `powershell -ExecutionPolicy Bypass -File .\build.ps1 -Test`: alle Suites grün
  (Ruby 56 OK / 0 Fehler; JSON 1029 Checks; Event-/Arbeitsabläufe 67;
  Terrain 51; WAV 18; Material 57; Dokument 58; Windows-Pfade 39;
  JSON-Brücke 15 Dateien; Event-Brücke 19 Checks).
- `ruby tools/lint_repo.rb`: 0 Fehler, 2 bekannte Warnungen
  (Manifest-Zeilenenden, offene TODO/FIXME-Markierungen).
- `powershell -ExecutionPolicy Bypass -File .\build.ps1`: beide EXEs neu
  gelinkt, keine `*.old.exe`-Artefakte.
- `powershell -ExecutionPolicy Bypass -File .\tools\windows_runtime_smoke.ps1`:
  isolierter Start von Editor und Spiel inklusive Editor-UI-Smoke
  (36 Prüfungen, 0 Fehler) erfolgreich.

## Bewusst nicht verändert

- Keine Editor-UI (F9) und kein neues Rendering: Diese Etappe liefert die
  Engine-Schicht; die editorische Oberfläche bleibt im Stufenplan (Stufe 40) offen.
- Keine Überschreibung von `game/data/database.json`, `system.json`, Karten oder
  Spielständen. Bestehende Projekte laden weiter wie zuvor.
- Keine 1:1-Kopie von XP-Quelltext; nur dokumentierte Struktur und Semantik.
