# Etappe 43: Die 90 Event-Befehle an der echten Engine

**Ziel:** Die typisierte Event-Command-Schicht aus Etappe 42 wirkt im laufenden
Spiel. Jeder der 19 Ports bekommt einen Adapter auf die vorhandenen
Engine-Module — kopflos testbar, ohne UI, ohne neue Globalen.

**Vorgänger:** Etappe 42 (`docs/ETAPPE_42_EVENT_COMMAND_TABS.md`) hat Bauplan,
Typen, Handler und Interfaces geliefert, aber gegen Null-/Memory-Adapter
gearbeitet. Ein `play_se` oder `change_hp` veränderte das Spiel noch nicht.

| Vorher (42) | Jetzt (43) |
|---|---|
| `PortSet` mit Null-/Memory-/Recording-Adaptern | Zusätzlich `EnginePorts.build` → `PortSet` aus echten Modulen |
| Seiteneffekte nur aufgezeichnet | `GameState`, `Party`, `Battle`, `$game_system`, `Pictures`, `ScreenFx`, `Weather`, `Fade`, `Mover`, `Shop`, `NameInput`, `MessageWindow`, `$game_temp` |
| Wahl/Zahl/Taste/Name lasen ihr Ergebnis sofort | Blockierende Fenster warten frame-weise und werden dann erneut gelesen (`erneut: true`) |
| Grenzen standen nur im Kopf des Autors | `EnginePorts.fehlt(befehl, grund)` meldet jede Lücke ins Log und in eine Liste |

## Neue Datei

```
game/engine/rpg/event_commands/engine_ports.rb   (~950 Zeilen)
```

Require-Reihenfolge in `event_commands.rb`:
support → layout → models → catalog → typed → ports → handlers → loader →
interpreter → **engine_ports**.

Aufruf von außen:

```ruby
katalog = Rpg.load_event_commands
engine  = Rpg.build_engine_event_interpreter(
  catalog: katalog,
  bindungen: { 'actors' => { 1 => 'arik' }, 'items' => { 3 => 'heiltrank' },
               'troops' => { 9 => 'waldweg' }, 'maps' => { 12 => 'dorf' } },
  interpreter: $interpreter          # laufender Engine-Interpreter für Fenster
)
engine.start(katalog.program(1))
engine.update(dt)                    # pro Frame, zusammen mit den Engine-Modulen
```

Alle Engine-Objekte sind injizierbar (`zustand:`, `party:`, `kampf:`, `system:`,
`bilder:`, `wetter:`, `fade:`, `effekte:`, `mover:`, `laden:`, `namenseingabe:`,
`fenster:`, `eingabe:`, `scripts:`, `temp:`, `spiel:`, `spieler:`). Ohne Angabe
greift das echte Modul. Damit ist jeder Adapter einzeln mit Attrappen prüfbar.

## Die 19 Adapter

| Port | Engine-Ziel | Bemerkung |
|---|---|---|
| `ISwitchStore` | `GameState.switch/set_switch` | ID wird zum String-Key |
| `IVariableStore` | `GameState.variable/set_variable` | |
| `ISelfSwitchStore` | `GameState.self_switch/set_self_switch` | Key `karte\|event`, Buchstabe A–D |
| `ITimerPort` | `$game_system.timer_start/timer/timer_working` | Sekunden, Engine rechnet in Frames |
| `IInventoryService` | `Party.add_item/take_item/item_count` | nur Items; Waffen/Rüstungen gemeldet |
| `IPartyService` | `Party.add_gold/member_add/member_remove/heal_all` | |
| `IActorService` | `Party.change_hp/mp/exp/level/state/param/skill/rename` | XP-Operation → Vorzeichen; Knockout-Schutz |
| `IMapController` | `$game_temp.player_*`, `Game.events`, `Mover.start`, `$game_player.transparent` | Kartenwechsel als Auftrag, `TempActions.tick` führt ihn aus |
| `IAnimationService` | — | gemeldet (F9-Register vorbereitet, kein Renderer) |
| `IPictureService` | `Pictures.show/move/rotate/tint/erase` | Sekunden → Frames über `Graphics.frame_rate` |
| `IWeatherService` | `Weather.set` | rain→regen, snow→schnee, storm→regen (gemeldet); Intensität 0..10 |
| `ITransitionService` | `Fade.start/laeuft?` | nur aus/ein; benannter Übergang gemeldet |
| `IScreenEffectService` | `ScreenFx.tone/flash/shake` | RGB-Blitz → nächster Farbname (gemeldet) |
| `IAudioService` | `$game_system.bgm_*/bgs_*/me_play/se_play/battle_bgm/battle_end_me` | Fade in Sekunden |
| `IBattleController` | `Battle.start/abort!/enemies/result`, `Battler#hp/mp/add_state/remove_state/clear_states/damage` | :sieg/:niederlage/:flucht → win/lose/escape |
| `IUINavigationService` | `Interpreter#say/choose/number_input`, `Shop`, `NameInput`, `MessageWindow`, `$game_temp` | blockierende Fenster, siehe unten |
| `IScriptService` | `Scripts.eval_code/run` | entkoppelter Hook-Key/Ausdruck |
| `ISystemSettingsService` | `$game_system.windowskin_name/save_disabled/menu_disabled/encounter_disabled` | |
| `EventDispatcher` | unverändert aus `ports.rb` | |

## Ressourcen: Rpg-ID → Engine-Name

Die Rpg-Schicht nutzt eigene Nummern (1, 3, 9), die Engine Namen (`arik`,
`heiltrank`, `waldweg`). `EnginePorts::Ressourcen#engine_id(sammlung, wert)`
löst in dieser Reihenfolge auf:

1. Bindungstabelle des Projekts (`bindungen['actors'][1] = 'arik'`),
2. Katalog-Ressource mit passender `id` → Key-Endteil (`item/heiltrank` → `heiltrank`),
3. der Wert selbst, wenn er schon ein Name ist.

## Blockierende Fenster (XP-getreu)

XP lässt einen Befehl erst `false` liefern (Fenster offen) und beim nächsten
Durchgang sein Ergebnis. Genau so arbeitet die Schicht jetzt:

- `Result.wait(:ui, erneut: true)` setzt den Befehlszeiger **nicht** weiter.
- Der Interpreter wartet, bis `ui.busy?` false ist, und liest denselben Befehl
  nochmal. Der Adapter erkennt am eigenen Zustand (`@wahl_offen`, `@zahl_offen`,
  `@taste_offen`, `@name_offen`), ob er öffnen oder Ergebnis liefern soll.
- `show_choices` pusht danach den gewählten Zweig (`choice_0` … `cancel`).
- `input_number` schreibt die Zahl in die Zielvariable.
- `button_input_processing` wartet auf eine der XP-Tasten (2/4/6/8/11/12/13/14).
- `name_input_processing` liest den bestätigten Namen und meldet `name.changed`.
- `shop_processing` wartet, bis `Shop.close`.

`Interpreter#choose` und `#number_input` öffnen dieselben Fenster wie die
bestehenden `cmd`-Befehle; `#choice_index` liefert die gewählte Option. Ein
leere Wahlzweig bekommt keinen Stack-Rahmen mehr, damit der Interpreter nicht
einen Frame lang als belegt gilt.

## Ehrliche Grenzen

Was die Engine nicht kann, wird **gemeldet statt vorgetäuscht**. Jeder Aufruf
von `EnginePorts.fehlt(befehl, grund)` schreibt eine Warnung ins `Rune::Log`
und in `EnginePorts.warnungen` (`[{command:, grund:}]`). `fehlt?(typ)` fragt die
Liste ab, `reset_warnungen!` leert sie.

| Befehl | Grund |
|---|---|
| `change_weapons`, `change_armors` | Engine führt nur einen Item-Bestand (F9: Waffen/Rüstungen vorbereitet) |
| `change_equipment`, `change_actor_class`, `change_actor_graphic` | Ausrüstung, Klasse und Grafik werden nicht geführt |
| `scroll_map` | Kamera folgt dem Spieler; freies Scrollen fehlt |
| `change_map_settings` | Panorama/Nebel/Battleback nicht je Befehl |
| `change_fog_color_tone`, `change_fog_opacity` | keine Nebelschicht |
| `show_animation` | Animationsdatenbank vorbereitet, ohne Renderer |
| `force_action` | Kampfgerüst erlaubt keine erzwungene Aktion |
| `enemy_appearance`, `enemy_transform`, `show_battle_animation` | Verstecken/Verwandeln/Kampfanimation fehlen |
| `prepare_transition` | Engine kennt nur aus/ein |
| `set_weather_effects` (storm, Dauer) | Sturm nutzt Regen; Dauer wird nicht ausgewertet |
| `screen_flash` (RGB) | nächstliegender Farbname weiss/rot/gruen/blau/gelb |
| `set_move_route` (ignore, frequency, repeat, skip) | werden nicht ausgewertet |
| `change_text_options` (speed) | Textgeschwindigkeit wird nicht ausgewertet |
| `shop_processing` (weapon/armor, quantity) | Laden kennt nur Items, Lager ist unbegrenzt |
| `change_items` (include_equipment) | wird nicht ausgewertet |

## Kleine Engine-Erweiterungen

- `Battler#remove_state(id)` / `#clear_states` (XP 313/333/314/334).
- `Interpreter#choose(options, cancel:)`, `#choice_index`,
  `#number_input(variable:, digits:, initial:, cancel:)`.
- `advance_choices` pusht keinen leeren Zweig mehr.

## Prüfungen

Neuer Test `tools/event_commands_engine_test.rb` — lädt die echte Engine
(`GameState`, `Party`, `Battle`, `ScreenFx`, `Weather`, `Fade`, `Pictures`,
`Mover`, `MessageWindow`, `Shop`, `NameInput`, `UI`, RGSS, `Interpreter`) mit
Native-Attrappen und lässt die Rpg-Programme darüber laufen. Ein Frame tickt
auch die Engine-Module (`Mover`, `Fade`, `Pictures`, `ScreenFx`, `$game_system`),
damit Routen und Blenden wirklich fertig werden.

```
node .cache/rbc/rbtest.mjs tools/event_commands_engine_test.rb
EVENT-COMMANDS AN DER ENGINE OK (110 Prüfungen, 0 Fehler)
```

Geprüft werden unter anderem: Schalter/Variable/Self-Switch in `GameState`,
Gold und Bestand in `Party`, HP/SP/EXP/Stufe/Zustand/Fertigkeit/Parameter/Name,
Knockout-Schutz, Kartenwechsel-Auftrag in `$game_temp`, Event-Versatz,
Bewegungsroute (Hash-Schritte → Mover-Text), Bilder, Bildschirmton/Blitz/Wackeln,
Wetter, Blende, BGM/BGS/ME/SE, Kampf und Gegnerwerte, Text/Wahl/Zahl/Taste/
Laden/Name als blockierende Fenster, Menü/Speichern/Game-Over/Titel, die
Gemeldet-Liste und Dependency Injection mit Attrappen.

Bestehende Suiten bleiben grün:

| Test | Ergebnis |
|---|---|
| `event_commands_arch_test.rb` | 1542 / 0 |
| `event_commands_test.rb` | 107 / 0 |
| `event18f_test.rb`, `ablauf_test.rb`, `bedingungen_test.rb`, `common_events_test.rb` | grün |
| `battle_test.rb`, `actor_wert_test.rb`, `actor_skill_test.rb` | grün |
| alle übrigen `tools/*_test.rb` | grün (12 WASI-Grenzen wie gehabt) |

## Bewusst nicht verändert / offen

- Der bestehende `interpreter.rb` führt weiter die Karten-Events; die neue
  Schicht ist additiv. Welcher Interpreter künftig die Events führt, entscheidet
  eine eigene Etappe.
- Kein UI: F6 zeigt den Bauplan weiter nicht (Etappe 44).
- CI-Grün ist kein Windows-/DPI-/GPU-Test.
