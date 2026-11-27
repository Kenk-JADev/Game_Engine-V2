# Etappe 48: Die 30 vorbereiteten XP-Befehle an die Engine anbinden

**Einordnung:** Etappe 44 hat den F6-Picker auf 90 Befehle erweitert und 30 neue
XP-Befehle als *vorbereitete* Einträge angelegt (Katalog, Layout, Editor-Enum,
Register). Sie waren serialisierbar, aber die Engine wirkte noch nicht und die
Einstellfenster waren unvollständig. Etappe 48 schließt diese Lücke: alle 90
Befehle sind jetzt vollständig — Katalog ↔ Editor ↔ Engine ↔ Prüfung synchron.

**Registerstand:** 103 Befehle in 11 Bereichen (73 Alt + 30 neu). Quelle der
Wahrheit bleibt `docs/befehle.json`; die fünf Register stehen in identischer
Reihenfolge: `MapCheck::CMD_TYPES`, `EvType`/`EV_TYPES` (C++), `ISSUE_CMD_TYPES`
(C++), die `when`-Literale in `interpreter.rb` und `cmd_map` (C++-Brücke).
`tools/befehle_test.rb` prüft Menge **und** Reihenfolge, `tools/sync_test.rb`
zusätzlich die C++-Listen gegen Ruby.

## 1. Engine-Anbindung (30 neue Befehle)

Die Wirkung liegt je Typ in der **EventRuntime-Registry**
(`game/engine/event_runtime.rb`, `TYPES` + `HANDLERS`), nicht im Interpreter-
Switch. Der Interpreter hat pro neuem Typ eine eigene `when`-Zeile (Register-
Sync) mit gemeinsamem Rumpf `EventRuntime.execute(cmd, self)`.

- **Welt/Bild:** `scroll_map` (zeitbasiert über `Rune::Camera.scroll` +
  `MapEffects.update`), `change_map_settings` (panorama/fog/battleback),
  `change_fog_color_tone`/`change_fog_opacity`, `show_animation`,
  `prepare_transition`, `change_transparent_flag`.
- **Party:** `change_equipment` (weapon/armor/accessory, `item_id` als Text,
  leer = ausziehen), `change_actor_class` (`class_id` als Text),
  `change_actor_graphic` (character/battler/face).
- **Inventar:** `change_weapons`/`change_armors` (`operation` add|subtract).
- **Kampf:** `change_enemy_hp`/`change_enemy_sp` (add|subtract),
  `change_enemy_state` (add|remove), `enemy_recover_all`, `enemy_appearance`,
  `enemy_transform`, `show_battle_animation` (`target_slot` −1 = alle),
  `deal_damage` (`target_slot`), `force_action`, `abort_battle`.
- **Ton/System:** `change_windowskin`, `change_battle_bgm`, `change_battle_end_me`,
  `stop_se`, `game_over`, `return_to_title_screen`.
- **Fluss:** `conditional_branch`, `wait_for_move_completion`.

Neue Engine-Bausteine: `game/engine/map_effects.rb` (Scroll/Fog/Transition/
Windowskin, `update(dt)`), `battle.rb` (`enemy_command`, `forced_actions`,
`enemy_slot`, `animation_at`, `battleback`, `visible?`/`transform`), `party.rb`
(equipment/class/graphic, Waffen-/Rüstungs-Bestand), `mover.rb`/`weather.rb`
(frequency/repeat/skip, storm→regen), `camera.h/.cpp` + `vm.cpp` (Scroll).

## 2. Alle 90 Einstellfenster (F6) vervollständigt

`src/editor/event_command_settings.h` liefert je Typ die Felder konsistent zu
den Katalog-Feldern; `src/editor/event_form.h` liest (`event_command_form_read`)
und serialisiert (`event_command_prepare`) alle 30 neuen Typen. Combo-Listen:
`EVENT_OPERATION` = add|subtract, `EVENT_STATE_MODE` = add|remove,
`EVENT_SCROLL_DIR`, `EVENT_MAP_TARGET`, `EVENT_EQUIP_SLOT`, `EVENT_ANIM_TARGET`,
`EVENT_COMPARATOR`. Slot 5 trägt je Typ kind/operation/direction/target/slot.

Spezifikations-Deltas: `input_number` digits **1..8** (Initial 0..99999999),
`picture_slot_id` **1..50**, `show_choices.cancel_behavior` ∈
cancel|disabled|branch + `cancel_index` −1..4, ControlVariables-Quelle `steps`.

## 3. CommandValidator im Lade-/Prüfablauf (Maps + Common Events)

Zwei Formen, eine Quelle der Wahrheit:

- **Typisiert (Rpg-Form):** `Rpg.validate_event_commands` →
  `CommandValidator.validate` (dünner Loader-Adapter, Etappe 47).
- **Legacy (Karten-/CE-Format `{"cmd":...}`):** neu
  `Rpg.validate_legacy_event_commands` → `CommandValidator.validate_legacy`.
  Prüft ~30 Typ-Referenzen (`LEGACY_REFS`) über id/name/key/Key-Endsegment
  (case-insensitive), Blocklisten (then/else/commands/cancel_branch/branches,
  Tiefe > 16) und `conditional_branch` kind→Referenz. Eine Referenz wird nur
  gemeldet, wenn die Sammlung nicht leer ist (leere Sammlung = nicht geführt).

**Verdrahtung:** `MapCheck::Sammler` nimmt einen `resources:`-Index;
`validiere_referenzen` läuft an allen vier Befehl-Einstiegen (Common Events,
flache Event-`commands`, Seiten-`commands`, NPC-`commands`). `MapCheck.scan`
reicht `resources:` durch, `scan_database` baut den Index aus der geprüften
Datenbank. `game/main.rb` (Lade-Hook) injiziert den Index aus
`Database.database`. Jeder Befund wird ein harter MapCheck-Fehler; ohne Index
bleibt der Prüfablauf unverändert (Kompatibilität).

## 4. Kompatibilität

Karten, Befehls-JSON und Spielstände brechen nicht: die Legacy-Validierung ist
additiv (nur mit Index aktiv), unberührte Felder werden nie zurückgeschrieben
(`json_test` 1121 Checks inkl. Roundtrip aller 103 Typen), und die mitgelieferte
Karte `dorf.json` + Common Events sind referenz-sauber
(`event_command_legacy_validator_test`, 18 Prüfungen).

## Tests (kopflos)

- `event_command_prepare48_test.cpp` — **65/0** (Serialisierung aller 30 neuen).
- `event_command_settings_test.cpp` — **29/0**; `event_command_catalog_test.cpp` — **32/0**.
- `json_test.cpp` — **1121/0** (Roundtrip + Beispiel je Typ + digits 1..8).
- `event_command_legacy_validator_test.rb` — **18/0** (Legacy + MapCheck-Integration).
- `event_commands_engine_test.rb` — **129/0**; `battle_test`/`battle_state_test` — 0.
- `befehle_test` — 103/11, 0; `sync_test` — 0; `event_command_validator_test` — 13/0.
- Register-Zähltests (ablauf/actor_skill/actor_wert/bedingungen/mapcheck/
  name_input/pictures/screen_fx/shop/timer/zustand/move_route/party_member) auf 103.
- Zig-Cross `editor_main.cpp` — EXIT 0 (nur Alt-Warnung Z.3595).

CI: neuer Job-Schritt „XP-Befehle Etappe 48 im Editor-Formular serialisieren"
(`event_command_prepare48_test`) im cpp-logik-Job; `test_all.rb` erfasst den
neuen Legacy-Validator-Test automatisch. **CI grün:** Lauf
[46019664148](https://github.com/Kenk-JADev/Game_Engine-V2/actions/runs/46019664148)
(`ac7fa9b`), alle vier Jobs (Hygiene, C++-Logik, Windows-Build, Ruby-Engine
62 OK/0/1). Nachzug: `projekte/dorf/data/database.json` an die Vorlage
gespiegelt (weapons/armors/classes), damit `sync_demo_test` sauber bleibt.

**Grenze:** CI-Grün ≠ Windows-/DPI-/GPU-Test. Die 12 WASI-bedingten Ausnahmen
(tmpdir/Open3/EXE) laufen im CRuby-CI-Job.
