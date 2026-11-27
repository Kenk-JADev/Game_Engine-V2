# Etappe 42: Event-Command-Architektur nach den drei XP-Tabs

**Ziel:** Die gesamte Event-Command-Architektur folgt exakt dem Aufbau der drei
Event-Command-Tabs des RPG Maker XP — als sauberes, modulares Code-Design mit
eigenen Beispielwerten für diese Engine. Bewusst **ohne UI**: die Schicht
liefert den Bauplan, die Typen, die Handler und die Schnittstellen, sodass eine
Oberfläche (oder der C++-Editor) sie direkt übernehmen kann.

**Vorgänger:** Etappe 40b (`docs/ETAPPE_40B_EVENT_COMMANDS.md`) hatte die
Schicht angelegt. Etappe 42 macht sie vollständig und eindeutig:

| Vorher (40b) | Jetzt (42) |
|---|---|
| Spalte wurde aus dem Code geraten, Abschnitte nur als `group` | `Layout` ist der geschriebene Bauplan: 3 Tabs → 2 Spalten → 2 Abschnitte → 90 Befehle in XP-Reihenfolge, mit XP-Name und deutscher Beschriftung |
| Ein `Models::Command` mit Parameter-Hash für alle Typen | Zusätzlich **90 typisierte Command-Klassen** (`Typed::Commands::ShowText` …) mit benannten, geprüften Feldern |
| ~35 Gruppen-Handler mit `case command.commandType` darin | **90 einzelne Handler**, einer je Typ, ohne `case` über den Befehlstyp |
| Windowskin/Speichern/Menü/Begegnung meldeten nur ein Ereignis | Eigener `ISystemSettingsService`; Kampf-BGM/-End-ME und Textoptionen erreichen echte Ports |
| Ausführungszustand nur intern | `program_counter`, `call_stack`, `depth`, `current_command`, `wait_kind` sind lesbar |

## Aufbau der Schicht

```
game/engine/rpg/event_commands/
  layout.rb        Bauplan der drei Tabs (Reihenfolge, Spalte, Abschnitt, Namen)
  models.rb        Field, Definition, Command, Program, CatalogData (unveränderlich)
  catalog.rb       Feldschema je Befehl: Typ, Pflicht, Vorgabe, Bereich, Referenz
  typed.rb         90 Command-Klassen + 90 Parameterklassen, aus dem Schema gebaut
  ports.rb         Interfaces + Null-, Memory- und Recording-Adapter, PortSet (DI)
  handlers.rb      90 Handler + Registry, Bedingungen und Variablenquellen
  interpreter.rb   Befehlszeiger, Blockstapel, Marken, Unterprogramme, Wartezustände
  loader.rb        JSON -> Modelle, sammelt alle Befunde, wirft ValidationError
  support.rb       Fehler, Reporter, Dateisystem-Adapter (auch In-Memory für Tests)
```

Aufruf von außen (alles explizit, kein Singleton, keine Globalen):

```ruby
katalog     = Rpg.load_event_commands                  # game/config/event_commands.json
interpreter = Rpg.build_event_interpreter(catalog: katalog, ports: meine_ports)
interpreter.start(katalog.program(1))
interpreter.update(dt)          # pro Frame; blockiert bei Text, Route, Übergang, Zeit
interpreter.program_counter     # Befehlszeiger des obersten Frames
interpreter.call_stack          # Programm/Block/Schleife/Common Event mit Zeiger
katalog.typed(befehl)           # typisiertes Command-Objekt
Rpg::EventCommands::Layout.als_text   # Bauplan als Textbaum für Doku/Editor
```

## Die drei Tabs (Bauplan)

Quelle ist `layout.rb`; die Tabellen unten sind daraus und aus dem Feldschema
erzeugt. `?` = optional, `=Wert` = Vorgabe. „Interpreter“ in der letzten Spalte
heißt: der Befehl wirkt auf den Ablauf, nicht auf einen Engine-Port.

#### Tab 1 · linke Spalte · Abschnitt `dialog`

| Code | `commandType` | XP-Name | Beschriftung | Parameter (typisiert) | Port-Aufruf |
|---|---|---|---|---|---|
| 101 | `show_text` | Show Text | Text anzeigen | `lines`:string_list, `speaker`:string?, `face_key`:key?, `background`:enum?="window", `position`:enum?="bottom" | `ui.message` |
| 102 | `show_choices` | Show Choices | Auswahl anzeigen | `choices`:string_list, `cancel_index`:integer?=-1, `cancel_behavior`:enum?="cancel", `default_index`:integer?=0, `position`:enum?="center", `background`:enum?="window" — Blöcke: `choice_0`, `choice_1`, `choice_2`, `choice_3`, `cancel` | `ui.choices` |
| 103 | `input_number` | Input Number | Zahl eingeben | `variable_id`:ref, `digits`:integer?=4 | `ui.input_number`, `variables.set` |
| 104 | `change_text_options` | Change Text Options | Textoptionen ändern | `background`:enum, `position`:enum, `speed`:integer, `no_fast`:bool?=false | `ui.text_options` |
| 105 | `button_input_processing` | Button Input Processing | Tastendruck abfragen | `variable_id`:ref | `ui.button_input`, `variables.set` |
| 106 | `wait` | Wait | Warten | `seconds`:number?, `frames`:integer? | Interpreter |
| 108 | `comment` | Comment | Kommentar | `text`:string | Interpreter |

#### Tab 1 · linke Spalte · Abschnitt `flow`

| Code | `commandType` | XP-Name | Beschriftung | Parameter (typisiert) | Port-Aufruf |
|---|---|---|---|---|---|
| 111 | `conditional_branch` | Conditional Branch | Bedingte Verzweigung | `kind`:enum, `switch_id`:ref?, `variable_id`:ref?, `comparator`:enum?, `value`:number?, `value_variable_id`:ref?, `letter`:enum?, `seconds`:number?, `actor_id`:actor_ref?, `enemy_slot`:enemy_slot?, `item_id`:ref?, `weapon_id`:ref?, `armor_id`:ref?, `button`:integer?, `script_key`:key?, `expression`:string? — Blöcke: `then`, `else` | Interpreter |
| 112 | `loop` | Loop | Schleife | — — Blöcke: `body` | Interpreter |
| 113 | `break_loop` | Break Loop | Schleife verlassen | — | Interpreter |
| 115 | `exit_event_processing` | Exit Event Processing | Eventverarbeitung beenden | — | Interpreter |
| 116 | `erase_event` | Erase Event | Event löschen | — | `map.erase_event` |
| 117 | `call_common_event` | Call Common Event | Common Event aufrufen | `common_event_id`:ref | Interpreter |
| 118 | `label` | Label | Sprungmarke setzen | `name`:string | Interpreter |
| 119 | `jump_to_label` | Jump to Label | Zu Sprungmarke springen | `name`:string | Interpreter |

#### Tab 1 · rechte Spalte · Abschnitt `state`

| Code | `commandType` | XP-Name | Beschriftung | Parameter (typisiert) | Port-Aufruf |
|---|---|---|---|---|---|
| 121 | `control_switches` | Control Switches | Schalter steuern | `from_id`:ref, `to_id`:ref, `value`:bool | `switches.set` |
| 122 | `control_variables` | Control Variables | Variablen steuern | `from_id`:ref, `to_id`:ref, `operation`:enum, `source`:enum, `value`:number?, `random_min`:integer?, `random_max`:integer?, `source_variable_id`:ref?, `item_id`:ref?, `actor_id`:actor_ref?, `enemy_slot`:enemy_slot?, `other`:enum? | `variables.get`, `variables.set` |
| 123 | `control_self_switch` | Control Self Switch | Lokalen Schalter steuern | `letter`:enum, `value`:bool | `self_switches.set` |
| 124 | `control_timer` | Control Timer | Timer steuern | `operation`:enum, `seconds`:number? | `timer.change` |
| 125 | `change_gold` | Change Gold | Gold ändern | `operation`:enum, `amount`:integer | `party.change_gold` |
| 126 | `change_items` | Change Items | Items ändern | `item_id`:ref, `operation`:enum, `amount`:integer | `inventory.change` (items) |
| 127 | `change_weapons` | Change Weapons | Waffen ändern | `weapon_id`:ref, `operation`:enum, `amount`:integer, `include_equipment`:bool?=false | `inventory.change` (weapons) |
| 128 | `change_armors` | Change Armor | Rüstungen ändern | `armor_id`:ref, `operation`:enum, `amount`:integer, `include_equipment`:bool?=false | `inventory.change` (armors) |
| 129 | `change_party_member` | Change Party Member | Partymitglied ändern | `actor_id`:actor_ref, `operation`:enum, `initialize`:bool?=false | `party.change_member` |

#### Tab 1 · rechte Spalte · Abschnitt `system`

| Code | `commandType` | XP-Name | Beschriftung | Parameter (typisiert) | Port-Aufruf |
|---|---|---|---|---|---|
| 131 | `change_windowskin` | Change Windowskin | Fenstergrafik ändern | `skin_key`:key | `system_settings.windowskin` |
| 132 | `change_battle_bgm` | Change Battle BGM | Kampf-BGM ändern | `bgm_key`:key | `audio.set_battle_bgm` |
| 133 | `change_battle_end_me` | Change Battle End ME | Kampf-End-ME ändern | `me_key`:key | `audio.set_battle_end_me` |
| 134 | `change_save_access` | Change Save Access | Speichern erlauben/sperren | `allowed`:bool | `system_settings.save_access` |
| 135 | `change_menu_access` | Change Menu Access | Menü erlauben/sperren | `allowed`:bool | `system_settings.menu_access` |
| 136 | `change_encounter` | Change Encounter | Begegnungen erlauben/sperren | `allowed`:bool | `system_settings.encounter` |

#### Tab 2 · linke Spalte · Abschnitt `map`

| Code | `commandType` | XP-Name | Beschriftung | Parameter (typisiert) | Port-Aufruf |
|---|---|---|---|---|---|
| 201 | `transfer_player` | Transfer Player | Spieler versetzen | `map_id`:ref, `x`:integer, `y`:integer, `direction`:enum?="down", `fade`:enum?="none" | `map.transfer` |
| 202 | `set_event_location` | Set Event Location | Event-Position setzen | `target`:enum, `event_id`:event_ref?, `x`:integer, `y`:integer | `map.set_event_location` |
| 203 | `scroll_map` | Scroll Map | Karte scrollen | `direction`:enum, `distance`:integer, `speed`:integer | `map.scroll` |
| 204 | `change_map_settings` | Change Map Settings | Karteneinstellungen ändern | `target`:enum, `asset_key`:string, `color`:color? | `map.settings` |
| 205 | `change_fog_color_tone` | Change Fog Color Tone | Nebel-Farbton ändern | `color`:color | `map.fog_tone` |
| 206 | `change_fog_opacity` | Change Fog Opacity | Nebel-Deckkraft ändern | `opacity`:integer | `map.fog_opacity` |
| 207 | `show_animation` | Show Animation | Animation zeigen | `target`:enum, `event_id`:event_ref?, `animation_key`:key | `animation.play` |
| 208 | `change_transparent_flag` | Change Transparent Flag | Transparenz setzen | `transparent`:bool | `map.transparent` |
| 209 | `set_move_route` | Set Move Route | Bewegungsroute setzen | `target`:enum, `event_id`:event_ref?, `steps`:move_steps, `wait`:bool?=false, `repeat`:bool?=false, `ignore`:bool?=false | `map.move_route` |
| 210 | `wait_for_move_completion` | Wait for Move's Completion | Auf Bewegung warten | — | Interpreter |

#### Tab 2 · linke Spalte · Abschnitt `screen`

| Code | `commandType` | XP-Name | Beschriftung | Parameter (typisiert) | Port-Aufruf |
|---|---|---|---|---|---|
| 221 | `prepare_transition` | Prepare for Transition | Übergang vorbereiten | `transition_key`:key | `transition.prepare` |
| 222 | `execute_transition` | Execute Transition | Übergang ausführen | `transition_key`:key | `transition.execute`, `transition.busy?` |
| 223 | `change_screen_tone` | Change Screen Color Tone | Bildschirm-Farbton ändern | `color`:color | `screen.tone` |
| 224 | `screen_flash` | Screen Flash | Bildschirm-Blitz | `color`:color, `duration`:number, `no_wait`:bool?=false | `screen.flash` |
| 225 | `screen_shake` | Screen Shake | Bildschirm wackeln | `strength`:integer, `speed`:integer, `duration`:number, `no_wait`:bool?=false | `screen.shake` |

#### Tab 2 · rechte Spalte · Abschnitt `picture`

| Code | `commandType` | XP-Name | Beschriftung | Parameter (typisiert) | Port-Aufruf |
|---|---|---|---|---|---|
| 231 | `show_picture` | Show Picture | Bild zeigen | `picture_slot_id`:integer, `picture_key`:key, `x`:integer, `y`:integer, `scale_x`:integer?=100, `scale_y`:integer?=100, `opacity`:integer?=255, `blend`:enum?="normal" | `pictures.show` |
| 232 | `move_picture` | Move Picture | Bild bewegen | `picture_slot_id`:integer, `picture_key`:key, `x`:integer, `y`:integer, `scale_x`:integer?=100, `scale_y`:integer?=100, `opacity`:integer?=255, `blend`:enum?="normal", `duration`:number?=0.0, `no_wait`:bool?=false | `pictures.move` |
| 233 | `rotate_picture` | Rotate Picture | Bild drehen | `picture_slot_id`:integer, `picture_key`:key, `duration`:number?=0.0, `no_wait`:bool?=false, `degrees`:number | `pictures.rotate` |
| 234 | `change_picture_tone` | Change Picture Color Tone | Bild-Farbton ändern | `picture_slot_id`:integer, `picture_key`:key, `duration`:number?=0.0, `no_wait`:bool?=false, `color`:color | `pictures.tone` |
| 235 | `erase_picture` | Erase Picture | Bild löschen | `picture_slot_id`:integer, `picture_key`:key | `pictures.erase` |
| 236 | `set_weather_effects` | Set Weather Effects | Wetter setzen | `kind`:enum, `intensity`:integer, `duration`:number?=0.0 | `weather.set` |

#### Tab 2 · rechte Spalte · Abschnitt `audio`

| Code | `commandType` | XP-Name | Beschriftung | Parameter (typisiert) | Port-Aufruf |
|---|---|---|---|---|---|
| 241 | `play_bgm` | Play BGM | BGM abspielen | `bgm_key`:key, `volume`:integer?=80, `pitch`:integer?=100 | `audio.play_bgm` |
| 242 | `fade_out_bgm` | Fade Out BGM | BGM ausblenden | `bgm_key`:key, `fade_seconds`:number?=1.0 | `audio.fade_out_bgm` |
| 245 | `play_bgs` | Play BGS | BGS abspielen | `bgs_key`:key, `volume`:integer?=80, `pitch`:integer?=100 | `audio.play_bgs` |
| 246 | `fade_out_bgs` | Fade Out BGS | BGS ausblenden | `bgs_key`:key, `fade_seconds`:number?=1.0 | `audio.fade_out_bgs` |
| 247 | `memorize_bgm_bgs` | Memorize BGM/BGS | BGM/BGS merken | — | `audio.memorize_bgm_bgs` |
| 248 | `restore_bgm_bgs` | Restore BGM/BGS | BGM/BGS wiederherstellen | — | `audio.restore_bgm_bgs` |
| 249 | `play_me` | Play ME | ME abspielen | `me_key`:key, `volume`:integer?=80, `pitch`:integer?=100 | `audio.play_me` |
| 250 | `play_se` | Play SE | SE abspielen | `se_key`:key, `volume`:integer?=80, `pitch`:integer?=100 | `audio.play_se` |
| 251 | `stop_se` | Stop SE | SE stoppen | `se_key`:key | `audio.stop_se` |

#### Tab 3 · linke Spalte · Abschnitt `battle`

| Code | `commandType` | XP-Name | Beschriftung | Parameter (typisiert) | Port-Aufruf |
|---|---|---|---|---|---|
| 301 | `battle_processing` | Battle Processing | Kampf verarbeiten | `troop_id`:ref, `can_escape`:bool?=true, `can_lose`:bool?=false — Blöcke: `win`, `escape`, `lose` | `battle.start` |
| 302 | `shop_processing` | Shop Processing | Laden verarbeiten | `shop_key`:key, `goods`:goods | `ui.shop` |
| 303 | `name_input_processing` | Name Input Processing | Namenseingabe | `actor_id`:actor_ref, `max_length`:integer?=8 | `ui.name_input` |

#### Tab 3 · linke Spalte · Abschnitt `actor`

| Code | `commandType` | XP-Name | Beschriftung | Parameter (typisiert) | Port-Aufruf |
|---|---|---|---|---|---|
| 311 | `change_hp` | Change HP | HP ändern | `actor_id`:actor_ref, `operation`:enum, `amount`:integer, `allow_knockout`:bool?=false | `actors.change_hp` |
| 312 | `change_sp` | Change SP | SP ändern | `actor_id`:actor_ref, `operation`:enum, `amount`:integer | `actors.change_sp` |
| 313 | `change_state` | Change State | Zustand ändern | `actor_id`:actor_ref, `state_id`:ref, `operation`:enum | `actors.change_state` |
| 314 | `recover_all` | Recover All | Vollständig heilen | `actor_id`:actor_ref | `actors.recover_all` |
| 315 | `change_exp` | Change EXP | Erfahrung ändern | `actor_id`:actor_ref, `operation`:enum, `amount`:integer | `actors.change_exp` |
| 316 | `change_level` | Change Level | Stufe ändern | `actor_id`:actor_ref, `operation`:enum, `amount`:integer | `actors.change_level` |
| 317 | `change_parameters` | Change Parameters | Parameter ändern | `actor_id`:actor_ref, `operation`:enum, `amount`:integer, `parameter`:enum | `actors.change_parameters` |
| 318 | `change_skills` | Change Skills | Fertigkeiten ändern | `actor_id`:actor_ref, `skill_id`:ref, `operation`:enum | `actors.change_skills` |
| 319 | `change_equipment` | Change Equipment | Ausrüstung ändern | `actor_id`:actor_ref, `slot`:enum, `item_id`:equipment_ref | `actors.change_equipment` |
| 320 | `change_actor_name` | Change Actor Name | Heldennamen ändern | `actor_id`:actor_ref, `name`:string | `actors.change_actor_name` |
| 321 | `change_actor_class` | Change Actor Class | Klasse ändern | `actor_id`:actor_ref, `class_id`:ref | `actors.change_actor_class` |
| 322 | `change_actor_graphic` | Change Actor Graphic | Grafik ändern | `actor_id`:actor_ref, `character_key`:key, `battler_key`:key, `face_key`:key | `actors.change_actor_graphic` |

#### Tab 3 · rechte Spalte · Abschnitt `enemy`

| Code | `commandType` | XP-Name | Beschriftung | Parameter (typisiert) | Port-Aufruf |
|---|---|---|---|---|---|
| 331 | `change_enemy_hp` | Change Enemy HP | Gegner-HP ändern | `slot`:enemy_slot, `operation`:enum, `amount`:integer, `allow_knockout`:bool?=false | `battle.enemy` |
| 332 | `change_enemy_sp` | Change Enemy SP | Gegner-SP ändern | `slot`:enemy_slot, `operation`:enum, `amount`:integer | `battle.enemy` |
| 333 | `change_enemy_state` | Change Enemy State | Gegner-Zustand ändern | `slot`:enemy_slot, `state_id`:ref, `operation`:enum | `battle.enemy` |
| 334 | `enemy_recover_all` | Enemy Recover All | Gegner vollständig heilen | `slot`:enemy_slot | `battle.enemy` |
| 335 | `enemy_appearance` | Enemy Appearance | Gegner erscheinen lassen | `slot`:enemy_slot | `battle.enemy` |
| 336 | `enemy_transform` | Enemy Transform | Gegner verwandeln | `slot`:enemy_slot, `enemy_id`:ref | `battle.enemy` |
| 337 | `show_battle_animation` | Show Battle Animation | Kampfanimation zeigen | `slot`:enemy_slot, `animation_key`:key | `battle.enemy` |
| 338 | `deal_damage` | Deal Damage | Schaden austeilen | `slot`:enemy_slot, `amount`:integer, `element_id`:ref | `battle.enemy` |
| 339 | `force_action` | Force Action | Aktion erzwingen | `slot`:enemy_slot, `action_key`:key, `target_slot`:enemy_slot | `battle.force_action` |
| 340 | `abort_battle` | Abort Battle | Kampf abbrechen | — | `battle.abort` |

#### Tab 3 · rechte Spalte · Abschnitt `system`

| Code | `commandType` | XP-Name | Beschriftung | Parameter (typisiert) | Port-Aufruf |
|---|---|---|---|---|---|
| 351 | `call_menu_screen` | Call Menu Screen | Menü aufrufen | — | `ui.menu` |
| 352 | `call_save_screen` | Call Save Screen | Speichern-Bildschirm aufrufen | — | `ui.save` |
| 353 | `game_over` | Game Over | Game Over | — | `ui.game_over` |
| 354 | `return_to_title_screen` | Return to Title Screen | Zum Titel zurück | — | `ui.title` |
| 355 | `script` | Script | Script-Hook | `script_key`:key?, `expression`:string?, `code`:string? | `script.run` |

## Typisierte Command-Objekte

Jeder Befehl ist ein eigenes, unveränderliches Objekt — kein Array, kein
loser Hash. Die Klassen entstehen aus dem Feldschema des Katalogs, damit es
nur eine Quelle der Wahrheit gibt:

```ruby
befehl = katalog.program(1).commands.first     # Models::Command (Rohdatensatz)
text   = katalog.typed(befehl)                 # Typed::Commands::ShowText

text.commandType   # => "show_text"   (kanonisches Feld)
text.code          # => 101          (XP-Code)
text.indent        # => 0            (Einrückungstiefe)
text.lines         # => ["Kael: Das Siegel antwortet.", "Mira: Der Riegel wartet."]
text.speaker       # => nil, wenn nicht gesetzt
text.background    # => "window"    (Vorgabe aus dem Schema)
text.params        # => Typed::Params::ShowText (ebenfalls unveränderlich)
text.to_h          # => verlustfreier Rohdatensatz inkl. blocks
```

Regeln:

- **Typen werden erzwungen.** `integer`/`number`/`bool`/`enum`/`ref`/`key`/
  `string_list`/`color`/`move_steps`/`goods` werden beim Cast geprüft und
  umgewandelt. Ein fehlendes Pflichtfeld oder ein falscher Typ wirft
  `ArgumentError`, statt still durchzugehen.
- **Vorgaben werden aufgefüllt**, fehlende optionale Felder bleiben `nil` und
  fehlen auch in `to_h` — der Rohdatensatz bleibt bytegleich zum Loader.
- **Blöcke sind typisierte Kinder.** `show_choices` hat `choice_0..3` und
  `cancel`, `conditional_branch` hat `then`/`else`, `loop` hat `body`,
  `battle_processing` hat `win`/`escape`/`lose`. `text.block('then')` liefert
  wieder typisierte Command-Objekte.
- **Ruby-sichere Namen.** Ein Feld darf nicht mit Rubys Konstruktor oder einer
  Command-Methode kollidieren: `initialize` heißt als Leser `initialize_actor`
  (JSON bleibt `initialize`), das `code`-Feld des Script-Befehls heißt
  `param_code`, weil `code` bereits der XP-Code ist.
- **Unbekannte Typen** werfen `Typed::UnknownCommand`.

## Handler-Registry (Strategy)

```ruby
register = Rpg::EventCommands::Handlers::Registry.new   # 90 Handler
register.for(katalog.definition('play_se'))             # => CommandHandler "play_se"
register.size                                           # => 90
register.register('play_se', eigener_handler)           # DI: Typ gezielt ersetzen
```

- Jeder der 90 Typen hat **genau eine** Handler-Instanz mit passender
  `command_type`-Identität. Es gibt keine geteilte Instanz zwischen Typen und
  keine `case`-Verzweigung über `commandType`.
- Ein Handler bekommt `(command, interpreter)` und gibt ein `Result` zurück:
  `next`, `push(block)`, `call_sub(id)`, `wait(kind, seconds:)`,
  `control(:jump/:break_loop/:exit_frame/:exit_all)`.
- Seiteneffekte gehen ausschließlich über `interpreter.ports`; Meldungen über
  `interpreter.emit`. Kein Handler kennt Renderer, Fenster oder `$game_*`.
- Gemeinsame Rechenwege stehen als kleine Funktionen daneben:
  `Conditions.erfuellt?` (12 Bedingungsarten), `Values.quellenwert` /
  `Values.anwenden` (Variablenquellen und Operationen), `Aufrufe` (Port-Aufruf
  plus Ereignis in drei Zeilen).

## Entkoppelte Engine-Interfaces

Alle Ports liegen in `ports.rb`. Zu jedem Interface gibt es einen Null-Adapter
(stumm, für Trockenläufe), meist einen Memory-Adapter (echter Zustand) und
einen Recording-Adapter (zeichnet Aufrufe auf, für Tests).

| Interface | Alias | Zweck | Beispiel-Adapter |
|---|---|---|---|
| `ISwitchStore` | — | globale Schalter | `MemorySwitchStore` |
| `IVariableStore` | — | globale Variablen | `MemoryVariableStore` |
| `ISelfSwitchStore` | — | lokale Schalter A–D je Event | `MemorySelfSwitchStore` |
| `ITimerPort` | `ITimerService` | Timer starten/stoppen | `MemoryTimerPort` |
| `IInventoryPort` | `IInventoryService` | Items/Waffen/Rüstungen | `MemoryInventoryPort` |
| `IPartyPort` | `IPartyService` | Gold, Mitglieder | `MemoryPartyPort` |
| `IActorPort` | `IActorService` | zwölf Heldenbefehle | `RecordingActorPort` |
| `IMapController` | — | Transfer, Scroll, Nebel, Route, Transparenz | `RecordingMapController` |
| `IAnimationPort` | `IAnimationService` | Animation auf Ziel | `RecordingAnimationPort` |
| `IPicturePort` | `IPictureService` | Bild-Slots 1..n | `RecordingPicturePort` |
| `IWeatherPort` | `IWeatherService` | Wetter | `RecordingWeatherPort` |
| `ITransitionPort` | `ITransitionService` | Übergang vorbereiten/ausführen | `RecordingTransitionPort` |
| `IScreenEffectPort` | `IScreenEffectService` | Farbton, Blitz, Wackeln | `RecordingScreenEffectPort` |
| `IAudioService` | — | BGM/BGS/ME/SE, Kampf-BGM/-End-ME | `RecordingAudioService` |
| `IBattleController` | — | Kampf, Gegnerwerte, Force Action, Abbruch | `RecordingBattleController` |
| `IUINavigationService` | — | Text, Wahl, Eingaben, Laden, Screens | `RecordingUINavigationService` |
| `ISystemSettingsService` | — | Windowskin, Speichern/Menü/Begegnung | `RecordingSystemSettingsService` |
| `IScriptPort` | `IScriptService` | entkoppelter Script-Hook | `RecordingScriptPort` |

`PortSet` bündelt sie und wird injiziert:

```ruby
ports = Rpg::EventCommands::Ports::PortSet.new(
  switches: MemorySwitchStore.new,
  audio:    MeineAudioAnbindung.new,     # echte Engine
  ui:       RecordingUINavigationService.new(busy: true)
)
Rpg.build_event_interpreter(catalog: katalog, ports: ports)
```

Ohne Injektion entstehen Null-Adapter: ein Programm läuft dann vollständig
durch, ohne eine Engine zu berühren (Trockenlauf, Editor-Vorschau, CI).

## Interpreter

- **Befehlszeiger** (`program_counter`, XP: `@index`) je Frame. Nach jedem
  ausgeführten Befehl wird er erhöht — auch bei Warten, genau wie XP.
- **Blockstapel**: `conditional_branch`, `show_choices`, `loop` und
  `battle_processing` legen einen Kind-Frame mit eigener Einrückung und
  eigenen Marken an. `indent` beschreibt die Verschachtelung im Datensatz.
- **Marken**: `label` sammelt Sprungziele pro Frame, `jump_to_label` setzt den
  Zeiger und wirft darüberliegende Frames ab.
- **Unterprogramme**: `call_common_event` legt einen Common-Event-Frame an;
  `call_stack` zeigt Aufrufer und Unterprogramm mit eigenem Zeiger,
  `common_event_depth` die Tiefe. Grenze: `MAX_DEPTH = 16`, danach Meldung
  `common_event.recursion_limit`.
- **Wartezustände**: `:time` (Wait, Blitz, Wackeln, Bildbewegung), `:move`
  (Route), `:transition`, `:ui` (Text, Wahl, Zahlen-/Namenseingabe, Laden).
  `wait_kind` und `busy?` machen sie lesbar; ein frisch gesetztes Warten
  bekommt das Delta desselben Updates (XP-Verhalten).
- **Schutzgrenzen**: `MAX_STEPS_PER_UPDATE = 200`, `MAX_LOOP_REPEATS = 10_000`
  mit Meldung `loop.limit`.

## Eigene Beispielwerte

`game/config/event_commands.json` enthält Ressourcen mit eigenen IDs und Keys
sowie vier Beispielprogramme, die zusammen **alle 90 Befehle** mindestens
einmal zeigen (Programm 1 „Torritual“, 2 „Kartenritual“, 3 „Kampfritual“,
9 „Lagerfeuer“ als Common Event).

| Ressource | ID | Key | Name |
|---|---|---|---|
| Schalter | 101 | `switch/tor_offen` | Tor_Offen |
| Variable | 202 | `variable/quest_phase` | Quest_Phase |
| Karte | 12 | `map/ruinenkammer` | Ruinenkammer — Transfer an (8, 14) |
| Event | 7 | `event/waechter` | Wächter |
| Held | 1 | `actor/kael` | Kael |
| Common Event | 9 | `common_event/lagerfeuer` | Lagerfeuer |
| Item / Waffe / Rüstung | 3 / 4 / 5 | `item/vital_tonic`, `weapon/rune_blade`, `armor/warded_cloak` | Vital Tonic, Rune Blade, Warded Cloak |
| Truppe / Gegner | 9 / 8 | `troop/rift_pack`, `enemy/ashen_wraith` | Rift Pack, Ashen Wraith |
| BGM / BGS | 24 / 25 | `bgm_tavern_night`, `bgs_rain_on_stone` | Tavern Night, Rain on Stone |
| ME / SE | 26 / 27 | `me_gate_chime`, `se_door_latch` | Gate Chime, Door Latch |
| Bild / Laden / Script | 20 / 21 / 22 | `pic_crest_seal`, `shop/rune_market`, `script/check_gate` | Crest Seal, Rune Market, Check Gate |
| Windowskin / Übergang | 23 / 34 | `skin/obsidian`, `transition/rune_fade` | Obsidian, Rune Fade |

## Prüfungen

- `ruby tools/event_commands_arch_test.rb` → **1542 Prüfungen, 0 Fehler**:
  Bauplan gegen die XP-Vorgabe (3 Tabs, 6 Spalten, 12 Abschnitte, 90 Befehle in
  Reihenfolge), Katalog ↔ Layout, 90 Command- und 90 Parameterklassen,
  verlustfreier Roundtrip aller Beispielbefehle samt Blöcken, 90 getrennte
  Handler, Handler-Austausch per DI, alle Schnittstellen und Adapter,
  Beispielwerte (Schalter 101, Variable 202, Karte 12 an (8, 14),
  `bgm_tavern_night`, `se_door_latch`), Befehlszeiger, Aufrufstapel,
  Wartezustände und Rekursionsgrenze.
- `ruby tools/event_commands_test.rb` → **107 Prüfungen, 0 Fehler**
  (Loader-Validierung, Negativfälle, Interpreter-Verhalten) — unverändert grün.
- `ruby tools/lint_repo.rb` → ohne Befund.

## CI-Ergebnis

[Lauf 45985842278](https://github.com/Kenk-JADev/Game_Engine-V2/actions/runs/45985842278)
(`ce1743b`) ist **grün in allen vier Jobs**: Windows-Build **60 Tests OK /
0 Fehler / 0 übersprungen**, Ruby-Engine 59 OK / 0 Fehler / 1 übersprungen
(`tex_import_test` braucht die gebaute EXE), C++-Logik und Repo-Hygiene ohne
Befund. Die Schicht ist damit auf Windows und Linux geprüft — ein GUI-Test ist
das nicht, denn diese Etappe enthält keine UI.

## Bewusst nicht verändert / offen

- **Keine UI.** Diese Etappe liefert Bauplan und Typen; die Registerkarten im
  C++-Editor (F6) bleiben wie sie sind. Wer sie bauen will, liest
  `Layout.tabs` und erzeugt daraus zwei Spalten mit je zwei Abschnitten.
- **Keine neuen `cmd`-Strings** im bestehenden Event-System: `docs/befehle.json`,
  `MapCheck::CMD_TYPES`, `ISSUE_CMD_TYPES`, `EV_TYPES` und `interpreter.rb`
  bleiben unangetastet. Die `Rpg`-Schicht ist additiv und hat eigene Typnamen.
- **Noch nicht an die laufende Engine angeschlossen.** Die Ports sind
  Schnittstellen mit Null-/Memory-/Recording-Adaptern; die Anbindung an
  `Party`, `Battle`, `Audio`, `Pictures` usw. ist der nächste Schritt
  (Backlog: „Rpg-Schicht an das laufende Spiel anbinden").
- Keine Änderung an `game/data/*`, Karten oder Spielständen.
