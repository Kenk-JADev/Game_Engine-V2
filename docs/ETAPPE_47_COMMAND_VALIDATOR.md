# Etappe 47: CommandValidator + neue Beispielwerte (Architektur nach XP-Spezifikation)

**Einordnung:** Die modulare Event-Command-Architektur nach den drei XP-Tabs ist
bereits umgesetzt und entspricht den Abschnitten 1–4 der Spezifikation:

- **Datenmodell:** `Models::Command` mit `commandType`, `indent >= 0`, typisiertem
  `params`-Hash und `blocks`; statische Definitionen (`Layout`, `Catalog`,
  `Models::Definition`) strikt getrennt von der Laufzeit (`Interpreter`,
  `RuntimeEventInstance` in `runtime.rb`).
- **Interpreter:** `programCounter`, indent-Blöcke (ConditionalBranch/ShowChoices/
  Loop), Sprungmarken-Index, Sub-Interpreter-Call-Stack (CallCommonEvent) und
  asynchrone Wartezustände (Wait/Move/Transition/UI/Shop/NameInput/Battle).
- **Handler-Registry:** ein kleiner `ICommandHandler` pro `commandType`
  (`handlers.rb`), keine monolithische switch-Anweisung.
- **Entkoppelte Services (DI):** `ISwitchStore`, `IVariableStore`,
  `ISelfSwitchStore`, `ITimerPort`, `IInventoryPort`, `IPartyPort`,
  `IActorRuntimeStore`, `IMapController`, `IMoveRouteService`, `IPictureService`,
  `IScreenEffectService`, `IWeatherService`, `IAudioService`, `IBattleController`,
  `IShopService`, `INameInputService`, `IUINavigationService`, `IScriptHookService`
  (`ports.rb`), injizierbar statt Singletons; echte Engine-Adapter in
  `engine_ports.rb` (Etappe 43).
- **Typsichere Felder:** `catalog.rb` definiert je Befehl Felder mit Kind, Bereich,
  Enum-Werten, Referenz-Sammlung und Asset-Sammlung; `typed.rb` erzeugt daraus
  90 unveränderliche Command- + 90 Parameter-Klassen (eine Quelle der Wahrheit).
  Der Loader (`loader.rb`) erzwingt Typen, Bereiche, Enums, Farben, MoveSteps,
  Goods, Block-Struktur und indent-Level beim Laden.

**Neu in Etappe 47 (Abschnitt 5 der Spezifikation):**

1. **`CommandValidator`** (`game/engine/rpg/event_commands/command_validator.rb`):
   prüft beim Laden von Maps und Common Events, ob alle Switch-/Variable-/Map-/
   Item-/Skill-/Actor-/Enemy-/Troop-/Animation-/CommonEvent-/Waffen-/Rüstungs-/
   Zustands-/Klassen-IDs sowie Asset-Keys gültig sind und ob Block-Strukturen
   konsistente indent-Level besitzen und Sprungmarken auflösbar sind.
2. **Neue eigene Beispielwerte** in `game/config/event_commands.json` und Tests.

## CommandValidator

Bewusst **keine eigene Prüflogik**: ein dünner, injizierbarer Adapter über den
bestehenden Loader (eine Quelle der Wahrheit). Er liefert strukturierte Befunde
statt zu werfen, damit Editor/MapCheck sie anzeigen können.

```ruby
index  = Rpg::EventCommands::CommandValidator.resources_from_database(db, assets: asset_index)
befunde = Rpg::EventCommands::CommandValidator.validate(commands, resources: index)
ok      = Rpg::EventCommands::CommandValidator.valid?(commands, resources: index)
# Komfort-API:
befunde = Rpg.validate_event_commands(commands, resources: index)
```

- `resources_from_database(db, assets:)` baut den Index aus einer
  `database.json`-artigen Struktur (Sammlung => Liste von `{id,name,key?}`) plus
  Asset-Key-Listen; fehlende Sammlungen werden leer geführt, damit der Loader
  alle Pflicht-Resourcenlisten sieht.
- `validate` speist ein virtuelles Dokument in den Loader (`MemoryFileSystem`)
  und fängt `ValidationError` als Befund-Liste ab. Geprüft werden damit
  automatisch: Referenz-IDs (`:ref`, `:actor_ref`, `:event_ref`), Asset-Keys
  (`:key`), Bereiche/Enums/Farben/MoveSteps/Goods, Block-Namen und indent-Level
  (`kinder`) sowie Sprungmarken (`validiere_querverweise`).

## Neue Beispielwerte

| Ressource | Alt | Neu |
|---|---|---|
| Schalter 101 | `Tor_Offen` | `Tor_Smaragdwald_Offen` |
| Variable | 202 `Quest_Phase` | **205** `Alchemie_Punkte` |
| Map | 12 an (8, 14) | **7** an **(14, 22)** |
| BGM | `bgm_tavern_night` | `bgm_ancient_citadel` |
| SE | `se_door_latch` | `se_crystal_chime` |

Die Beispielprogramme in `game/config/event_commands.json` und die Erwartungen
in `event_commands_test`, `event_commands_arch_test`, `event_commands_engine_test`
wurden konsistent umgestellt (IDs, Namen, Keys, Transfer-Koordinaten).

## Tests

- Neu: `tools/event_command_validator_test.rb` (**13 Prüfungen, 0 Fehler**):
  gültiges Programm ohne Befunde; unbekannte Switch-ID, unbekannter Asset-Key,
  falscher indent-Level, fehlende Sprungmarke werden gemeldet;
  `resources_from_database` mappt IDs/Asset-Keys und führt alle Pflicht-
  Sammlungen; database-Index validiert ein Kleinprogramm und meldet Fehler;
  Komfort-API `Rpg.validate_event_commands`.
- Bestehend grün: `event_commands_test` **107/0**, `event_commands_arch_test`
  **1542/0**, `event_commands_engine_test` **110/0**, `befehle_test` 0,
  `json_test` **1031/0**, `event_command_catalog_test` **32/0**,
  `event_command_settings_test` **29/0**.
- `tools/test_all.rb` globbt `tools/*_test.rb`; der neue Test läuft automatisch
  im CI-Engine-Job mit.

## CI

**CI grün:** Lauf [46006515442](https://github.com/Kenk-JADev/Game_Engine-V2/actions/runs/46006515442)
(Commit `14d19bc`), alle vier Jobs erfolgreich. Ruby-Engine jetzt **61 OK,
0 fehlgeschlagen, 1 übersprungen** (der neue `event_command_validator_test` läuft
via `test_all` mit); Windows-Build 62 OK/0/0; C++-Logik und Repo-Hygiene grün.

## Hinweis

Kein UI-Design in dieser Etappe (reine Code-/Validierungs-Schicht). Die
typsichere Feld-Definition je Befehl bleibt `catalog.rb` + `typed.rb`; der
CommandValidator macht sie beim Laden von Maps/Common Events durchsetzbar.
