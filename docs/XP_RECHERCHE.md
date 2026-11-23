# Recherche: RPG Maker XP — Was es gibt, wie es funktioniert, wie Skripte laufen

**Zweck:** Grundlage vor der Umstellung der 3D-Engine (v. a. Scene_Title statt
Launcher, Skript-Override wie XP). Kein Meinungsstück — Fakten mit Quelle.
**Quellen:** `XP-Defaultscript/` (90 Original-RGSS1-Skripte im Repo, primär),
Web-Recherche (RGSS-Architektur, Script-Editor-Verhalten, XP-Editor-Features),
dazu die vorhandenen Vergleiche `docs/XP_VERGLEICH.md`/`LOGIK_PRUEFUNG.md`.

---

## 1. Was es gibt (Inventar)

### 1.1 Editor (Werkzeug)
- **Mapping:** Kacheln 32×32 px, Tileset 8 Kacheln breit, beliebig hoch
  (~5000 max), bis 999 Tilesets; **3 Tile-Ebenen + Event-Ebene**; Event
  kann als Kachel „dienen". Pro Karte: Tileset, Panorama, Nebel,
  Begegnungsliste/-intervall, Startpunkt, BGM.
- **Datenbank (F9), 13 Tabs, je max. 999 Einträge:** Actors, Classes,
  Skills, Items, Weapons, Armors, Enemies, Troops, States, Animations,
  Tilesets, Common Events, System (BGM/SE/ME + Titelbild + Startparty …).
- **Tileset-Flags:** Passage (alle Seiten), Passage (4 Richtungen),
  Priority (Vordergrund-Ebene), Bush, Counter („über den Tresen greifen"),
  Terrain Tag (0–7 im Editor; Script-defined).
- **Event-Editor:** Trigger **0 Action Button, 1 Player Touch,
  2 Event Touch, 3 Autorun, 4 Parallel Process**; **Seiten** mit
  Bedingungen (2 Schalter, 1 Variable, 1 Self-Switch A–D); ~**92
  Event-Befehle** in6 Gruppen (Ablauf/Steuerung, Objekt-Bewegung,
  Bildschirm, Daten, Sonstiges, Kontrolle); Move Routes (Codes 1–45);
  **Common Events** (Befehl117, eigene Datenhaltung).
- **Ressourcen:** Ordner `Graphics/*`, `Audio/*` (BGM/BGS/SE/ME);
  **Playtest** (F11 im Editor) startet die EXE des Projekts;
  **Script-Editor** (Dialog mit Skriptbaum, siehe3).
- Auslieferung: `Game.rgssad`-Archiv oder lose `Data/`-Ordner.

### 1.2 Laufzeit (das Spiel)
- **16 Szenen** (`Scene_Title`, `Scene_Map`, `Scene_Menu`, `Scene_Item`,
  `Scene_Skill`, `Scene_Equip`, `Scene_Status`, `Scene_File`, `Scene_Save`,
  `Scene_Load`, `Scene_End`, `Scene_Battle`, `Scene_Shop`, `Scene_Name`,
  `Scene_Gameover`, `Scene_Debug`).
- **Objektmodell `Game_*` (Dateien000–022):** Temp, System, Switches,
  Variables, SelfSwitches, Screen, Picture, Battler×3, BattleAction,
  Actor, Enemy, Actors, Party, Troop, Map, CommonEvent, Character×3,
  Event, Player — alle Zustandsobjekte hängen an `$game_*`-Globalen.
- **Fenster-/Sprite-Schicht (023–062):**31 `Window_*`-Klassen (Basis
  `Window_Base`/`Window_Selectable`/`Window_Command` …), `Sprite_*`,
  `Arrow_*` für Kampfziele.
- **Event-Laufzeit:** `Interpreter` in **7 Teilen** (063–069), Tiefe ≤100
  (Common-Event-Aufrufe als Child-Interpreter); Haupt-Interpreter lebt in
  `$game_system.map_interpreter`, Kampf-Interpreter in
  `$game_system.battle_interpreter`, **jedes Parallel-Event besitzt
  eigenen Interpreter** (021: `@interpreter`).
- **Kachel-Laufzeit:** Passabilities-/Prioritäts-Tabellen, Display-Scroll
  (`display_x/y` in16-px-Einheiten), `RPG::Weather` (Wetter0–3),
  Timer (Frames), `$game_screen` (Tone/Flash/Shake/Pictures/Wetter).
- **Kampf:** Runden mit AGI-Reihenfolge (`agi + rand(10 + agi/4)`),
  Befehle Attacke/Skill/Verteidigen/Item, Treffer/Miss-Formeln,
  Zustände (inkl. Slip-Damage), EXP/Level-Up in `exp=`, Fluchtformel,
  **Truppen-Battle-Events** (Befehle331–340), `can_lose`-Sonderweg.
- **Speichern:** `Scene_Save` schreibt per **Marshal** (`Save1..3.rxdata`)
  alle `$game_*` + `Graphics.frame_count`; `save_count`/`magic_number`
  in `$game_system` (Kompatibilitäts-/Zähl-Flags); Ladezeit-Timeout.
- **Audio:** nativ; `RPG::AudioFile`-Objekte (name/volume/pitch),
  `$game_system` merkt sich spielende BGM (`bgm_memorize/restore` nach
  Kampf/Menü).

---

## 2. Wie es funktioniert (Architektur)

### 2.1 Datenhaltung
- **Alles Ruby-Marshal** unter `Data/`: `Actors.rxdata`,
  `Classes.rxdata`, `Skills.rxdata`, `Items.rxdata`, `Weapons.rxdata`,
  `Armors.rxdata`, `Enemies.rxdata`, `Troops.rxdata`, `States.rxdata`,
  `Animations.rxdata`, `Tilesets.rxdata`, `CommonEvents.rxdata`,
  `System.rxdata`, `MapInfos.rxdata`, `Map001….rxdata`,
  **`Scripts.rxdata`** (Skriptbaum), dazu `Save*.rxdata`.
- Beim Start lädt `Scene_Title#main` **alle `$data_*`-Tabellen** und
  erzeugt `$game_system`; New Game erzeugt den Rest der `$game_*`
  (`Scene_Title#command_new_game`), Continue baut sie aus dem Save
  (`Scene_Load`).
- Event-Befehle in den Karten: `[code, indent, parameters]`-Arrays —
  z. B. `101` + fortlaufende `401` (Textzeilen), `102/402/403/404`
  (Wahlen), `355/655` (Script), `117` (Common Event).

### 2.2 Szenenmaschine (das Herz)
```ruby
# Main.rb (089) — wörtlich:
Graphics.freeze
$scene = Scene_Title.new
while $scene != nil
  $scene.main
end
Graphics.transition(20)
```
- Jede Szene hat eine **blockierende** `main`-Methode mit eigenem
  `loop { Graphics.update; Input.update; update; break if $scene != self }`.
  Szenenwechsel = `$scene = AndereSzene.new` (die laufende `main`
  bricht ab, die Schleife holt die nächste).
- **Was nativ (C/DLL) ist und NICHT in den90 Default-Skripten steht**
  (geprüft): `Graphics`, `Input`, `Audio`, `Sprite`, `Window`, `Viewport`,
  `Bitmap`, `Color`, `Rect`, `Font`, `RPG::Cache`, `RPG::Weather`,
  `Win32API`, `rgss_main` (Fehlerbehandlungs-Wrapper), `load_data`/
  `save_data` (Marshal).

### 2.3 Event- und Kartenwechsel-Flow
- `Scene_Map#update`: `$game_map.update`, Event-Trigger (Action/Touch,
  Auto, Parallel), Encounter-Check, Menü-Eingabe — **unterdrückt, solange
  `message_window_showing`/Interpreter läuft**.
- Transfer: `$game_temp.player_transferring`-Flags →
  `Scene_Map#perform_transfer` (neue Karte, `make_encounter_count`,
  Centering).
- Nachrichten laufen über `$game_temp.message_text`/`message_proc` —
  **Wahlen und Zahleneingabe hängen am selben Fenster** (s. LOGIK M).

### 2.4 Speicher-/Ladefluss
- `Scene_Save#write_save_data`: Marshal von `$game_switches`,
  `$game_variables`, `$game_self_switches`, `$game_system`,
  `$game_screen`, `$game_actors`, `$game_party`, `$game_troop`,
  `$game_map`, `$game_player` + `Graphics.frame_count` + `save_count`.
- `Scene_Load` liest, prüft Existenz/`magic_number`, setzt alle Globalen,
  `Scene_Map` neu.

---

## 3. Wie Skripte funktionieren (Kern der Umstellungsfrage)

### 3.1 Aufbau & Reihenfolge
- Der **Script-Editor hält EINEN sortierten Baum** von Abschnitten
  (Name + Code), gespeichert in `Data/Scripts.rxdata`. Beim Spielstart
  wird **jeder Abschnitt der Reihe nach im Top-Level ausgeführt**
  („every entry gets eval'd in sequence" — Forum/Plugin-Praxis identisch
  zum Default-Ablauf).
- **Reihenfolge = Bedeutung:** Defaults stehen oben (hier000–089),
  **Nutzerskripte kommen nach unten, aber OBHALB von `Main` (089)**.
  Klassen werden dadurch ein zweites Mal geöffnet (`class Scene_Title`
  … erneut) — **die spätere `def`-Definition gewinnt**. So ersetzt man
  ganze Szenen oder einzelne Methoden.
- **„Anything after main is completely ignored":** `Main.rb` startet die
  Szenenschleife und kehrt nicht zurück — Skripte NACH Main werden nie
  ausgeführt. (Deshalb gilt die Regel: eigene Skripte *vor* Main einfügen.)
- Skripte laufen **einmal beim Start**; zur Laufzeit fliegt nur Code aus
  Events (355/655, Move-Route45, ggf. Common Events) — es gibt **kein
  Skript-Reload** (Rune3D-F5 ist eine Rune3D-Erweiterung).
- Es gibt **keine Sandbox**: Skripte dürfen alles (Globals definieren,
  Klassen patchen, `Win32API`, Dateien).

### 3.2 Konkrete Override-Muster (aus der Community belegt)
1. **Ganze Szene ersetzen:** `class Scene_Title` … eigenen `main` mit
   eigenem Command-Window (Forum-Beispiel: „Light/Dark"-Auswahl statt
   New Game/Continue) — dieselbe Klasse, neue Methoden, `Main`-Schleife
   bleibt unverändert.
2. **Einzelne Methode ersetzen:** `def command_new_game` neu definieren,
   `super` aufrufen, um die Originallogik zu erweitern.
3. **Klassen erweitern:** `class Game_Picture`/`Game_Battler` um Methoden
   ergänzen; Mixins/Module über `include`.
4. **Defaults deaktivieren:** Abschnitt im Script-Editor leeren —
   Reihenfolge bleibt, die Klasse fehlt → Fehler oder eigene Definition
   darunter.

### 3.3 Typischer XP-Skript-Start (Zusammenfassung)
```ruby
# (alles einmalig, top-level, in Reihenfolge)
# … Defaults (Game_*/Window_*/Scene_*) …
# ---- Eigene Skripte (hier unten, vor Main) ----
class Scene_Title
  def main
    # eigene Titellogik …
    $scene = Scene_Map.new     # oder command_new_game wie gehabt
  end
end
# ---- Main (089): $scene = Scene_Title.new; while $scene … end ----
```

---

## 4. Konsequenzen für Rune3D (Umsetzungs-Checks)

| XP-Mechanik | Rune3D-Ist | Umsetzung für die Umstellung |
|---|---|---|
| Defaults + Nutzerskripte werden **alle vor Main eval'd**, Reihenfolge entscheidet | `Scripts` registriert nur (`@defs`); nur `autoload`-Skripte laufen (`run_startup`, TOPLEVEL). Event-Skripte laufen bei Bedarf | Für Override-Mechanik: **Projekt-Skripte vor dem ersten `$scene`-Aufbau auswerten** (XP-Modus), nicht nur `autoload`. Danach `$scene = Scene_Title.new` |
| `Main`: `while $scene != nil; $scene.main; end` | `Graphics.scene_update` ruft `$scene.main` **pro Frame** (frame-basiert) | Dokumentierte **Frame-Konvention** + `Scene`-Basis (`update`/`draw`), damit XP-Skripte mit blockierendem `loop` nicht das Tick einfrieren (Adapter oder klare Fehlermeldung) |
| Szenenwechsel = `$scene = X.new` | Modus-Maschine `@mode` (`:launcher`/`:game`) + `Launcher.activate` | Menü „Zum Titel" & Game-Over → `$scene = Scene_Title.new`; Launcher raus (AB) |
| `Scene_Title` (New Game/Continue/Shutdown) + `Scene_File`-Slot-Wähler | Launcher-Projektliste, `L` lädt direkt | Engine-`Scene_Title` + `Scene_File`; Weitergabe an **Projekt-Skripte** zum Umschreiben |
| `$data_*`-Tabellen (13) in `Scene_Title#main` geladen | `Database.database` (JSON, Teilmenge: actors/items/troops/enemies) | Entspricht Roadmap20a; Titel braucht nur `system` (Name/Bild/BGM) — Rest beim New Game |
| Speichern per Marshal mit `save_count`/`magic_number` | JSON-Slots (dokumentierte Abweichung) | Bleibt JSON; `save_count` gehört in `$game_system`-Save (J) |
| Kein Skript-Reload | F5 lädt `main.rb` neu | Beibehalten (Erweiterung), aber dokumentieren: XP-Skripte kennen das nicht |
| Kachel-Flags/Prioritäten/Counter | 3D-`Passage` (Block-Zonen + Physik) | Kein Ersatz nötig — bewusste 3D-Äquivalente (Q) |

**Merksatz für die Umstellung:** XP ist *„eine Datei, die von oben nach
unten durchgekaut wird, am Ende startet Main die Szenenschleife"* —
Rune3D braucht dieselbe **Kette** (Defaults → Projekt-Skripte → Boot →
`$scene`), nur frame-basiert statt blockierend. Dann ist `Scene_Title`
genauso umschreibbar wie in XP.

---

## 5. Quellen (Auswahl)

- `XP-Defaultscript/` (90 Dateien, primär): `070 Scene_Title`,
  `089 Main`, `001 Game_System`, `063–069 Interpreter`, `021 Game_Event`,
  `071 Scene_Map`, `078 Scene_Save`, `079 Scene_Load`.
- Script-Editor/Reihenfolge: rgss-script-editor (VS Code, GitHub
  `SnowSzn/rgss-script-editor` — Load-Order-Datei, Enable/Disable je
  Abschnitt), RPGMakerWeb-Forum „External Scripts Framework"
  („eval'd in sequence … as long as the order is set correctly"),
  GDUnlimited „Beginner's guide to learning RGSS" („reads the scripts in
  the order they are in the script editor; anything after main is
  completely ignored").
- Szenen-/Datenmodell: Neoseeker/Chaos-Project-Forum-Beispiele
  (vollständige `Scene_Title`-Overrides, `rgss_main`-Wrapper),
  `runetranslate.com` (RXDATA/RGSSAD-Übersicht),
  GDUnlimited „RPG Maker XP" (13 Datenbank-Tabs,999er-Limits),
  EssentialsDocs „Tilesets" (32-px-Kacheln, Flag-Liste, Terrain Tags).
