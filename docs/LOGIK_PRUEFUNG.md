# Logikprüfung: Rune3D-Engine & Editor vs. RPG Maker XP

**Zweck:** Prüfen, ob die Laufzeitlogik semantisch dem RPX-Maker-XP-Original
entspricht (nur in 3D übersetzt) und ob die Editor-Logik sinnvoll ist.
**Methode:** Engine-Code Zeile für Zeile gegen die hochgeladenen
`XP-Defaultscript/` (90 Dateien) gelesen; Befehls-Codes unabhängig gegen die
 dokumentierten XP-/RGSS-Konstanten (`ROUTE_*`, `Game_Character`) verifiziert.
**Ergebnis in Kürze:** Die Architektur ist durchdacht und die meiste Logik ist
XP-gleich oder ein bewusstes 3D-Äquivalent. **Zwei klare Logikfehler** wurden
gefunden (F1, F2), dazu **neun dokumentierte Semantik-Abweichungen** (D1–D9),
von denen drei eine Entscheidung brauchen. Die Editor-Logik ist solide
(transaktionale Formulare), hat aber zwei strukturelle Schwachstellen.
**Erweiterte Prüfung (Teile E–H):** RGSS-Layer, Karten-System,
Editor-Umschaltung und Script-Ausführung — ergeben **eine dritte Fix-Lücke
(F3: fehlende XP-API `$game_party.actors` usw.)** und eine Reihe bewusster
3D-Abweichungen, die dokumentiert sind.
**Prüfrunde 3 (Teile J–L):** Speicherstände, Titel/Launcher/Game-Over,
Kampf-Ablauf — neue Fix-Lücken **F4** (stiller Karten-Fallback beim Laden)
und fehlender Game-Over-Bildschirm; `$game_system`-Zustand nicht im Save.
**Prüfrunde 4 (Teile M–O):** Dialoge/Textfenster, Ton/Audio, Eingabe &
Physik — neue Fix-Lücke **F5** (Spieler laufbar bei offenem Text/Menü),
Wahl-Abbruch (cancel/Else) fehlt, kein Datei-Audio/Pitch/BGM-Befehle,
Panorama-Scroll und Wetter-`duration` offen.
**Prüfrunde 5 (Teile Q–T):** Kollisions-/Weglogik, RGSS-Koordinaten,
Performance, Editor-Undo — neue Fix-Lücke **F6** (`$game_player.y`
liefert Höhe statt Kartenebene, Events ohne `y`, Player-Methoden
straighten/turn_*/through fehlen), Mover `through`/`jump` als No-Op,
kein Performance-Hotspot (Skalierungshinweise), Undo bestätigt
objekt-only.
**Prüfrunde 6 (Teile V–W):** Esc-Menü vs. `Scene_Menu`, Zufallskämpfe,
Teardown-Audit — Menü parst pro Frame bis zu 12 Save-JSONs (Cache
fehlt), Encounter-Kadenz seit Runde 26 pro Kachel statt pro Schritt-Sound,
Slip-Damage fehlt (Zustände seit 20a nur im Kampf), Teardown/F5
**sauber ohne Leak** ✓.
**Prüfrunde 8 (Teil AC, Ablauf-Befehle 20c):** `loop`/`break`,
`label`/`jump`, `exit_event`, `erase_event`, `button_input` und
`text_options` sind umgesetzt (34 Befehlstypen, Runde 27); sechs bewusste
Abweichungen/Grenzen sind in **AC** dokumentiert (Marke nur im selben
Block, Tastenposition statt XP-Tastencode, Schleife ohne Ausgang,
Lebensdauer von `erase_event`, Fenster-Optionen als Modul, `break`
außerhalb einer Schleife). Gemeinsame Events (117) samt Datenhaltung,
Trigger und MapCheck-Prüfung sind ebenfalls fertig; ein eigener
Editor-Reiter dafür bleibt offen.
**Prüfrunde 7 (Teil Y, Datenpipeline End-to-End):** Editor↔Laufzeit
feldweise konsistent (26 Typen, Seiten/Bedingungen, NPC-Roundtrip,
Legacy-Migration, Brücke getestet) — neue Befunde **Y1 Demo-Daten-Drift**
(`projekte/dorf` ohne Kämpfe: encounters+Truppen fehlen, `ensure_demo`
aktualisiert nie) und **Y2 tote Demo-Skripte** — beides in Runde 13
erledigt (`tools/sync_demo.rb`, Script-Befehle in beiden Bäumen).
**Prüfrunde 8 (Teil AA, Neues-Projekt-Flow):** Name→leere Karte existiert
in Launcher und Editor ✓ — aber **Y3** neue Projekte starten mit 0 Helden
(Editor-Kommentar verspricht „Start-Held", Code schreibt leer) und **Y4**
zwei widersprüchliche Anlage-Konventionen (Slug vs. `[A-Za-z0-9_]`,
Karte `welt01` vs. `<projektname>`). Priorität: Y3/Y4 vor Y1.
**Nutzer-Vorgabe (Teil AB):** Game-Engine **ohne Launcher** → echte
**Scene_Title** (New Game/Continue/Shutdown), per Script-Editor
austauschbar; **neue Projekte nur im Editor**. Migrationsplan in AB;
K listet nur noch den Ist-Zustand.
**Runde 11 (Fix-Batch Z1): F1–F6 + D9 umgesetzt.** XP-Routen-Codes auf
0–45 neu tabelliert (Quellen: tonbi.jp CommandTable, rpg-maker.fr,
RGSS `ROUTE_*`; `play_se` ergänzt, CODE_PFLICHT normalisiert), Events
ohne erfüllte Seitenbedingung sind inaktiv, RGSS-API ergänzt
(`$game_party.actors/steps/item_number`, `$game_map.events` als Hash,
`Input.dir8`, `GamePlayer`-No-Ops, `$game_player.y` = Kartenebene,
`Event#y`), `load_game` bricht bei fehlender Karte ab, die Simulation
pausiert bei Text/Wahl/Zahleneingabe/Menü, Mausklick feuert nur aktive
`action`-Events. Tests in move_route/event_pages/rgss/party/runtime_boot
ergänzt; die Einträge unten sind jeweils als behoben markiert.

---

## A. Was semantisch zu XP passt ✅

| Bereich | Befund |
|---|---|
| Event-Seitenwahl | Höchster Index mit erfüllten Bedingungen gewinnt — identisch zu `Game_Event#refresh` (`pages.reverse`, erstes Treffer-break). `event.rb:103` nutzt `rindex`. |
| Seitenwechsel → Neuauslösung | `fired` wird bei Seitenwechsel zurückgesetzt — entspricht XP `clear_starting` + `check_event_trigger_auto` (Seitenwechsel lässt touch/auto erneut feuern). |
| Bedingungs-Operatoren | Alle sechs (`== != < <= > >=`) — Superset von XP (XP kann nur „Variable ≥ Wert“). Unbekannter Operator blockiert nicht (dokumentiert). |
| Self-Switches A–D | Buchstabe normalisiert, ungültige werden verworfen, Schlüssel pro Karte+Event — XP-`[map_id, event_id, ch]`-Pendant auf Namensbasis (bewusst, dokumentiert). |
| Altes Seitenformat | Events ohne `pages` laufen als implizite Seite 0 weiter — Migration sicher. |
| Interpreter-Modell | Frame-Stack für Verschachtelung (`then`/`else` push/pop) — semantisch gleich zu XP `@branch`+`@index`, nur robuster. Unbekannte Kommandos werden geloggt und gemeldet (`Issues`), nicht stilles Cliffhanger. |
| Blockaden | `text`, `choices`, `input_number`, `battle`, `move_route wait`, `script`-mit-`say` blockieren den Interpreter wie XP-Ereignis-Command-Wartezeiten. |
| Zahleneingabe (103) | Schreibt wie XP in eine Variable, Ziffernzahl + Abbruch — nur Einheiten/Sekunden statt Frames (D6). |
| Bewegungsroute: Namen & Logik | XP-Namen 1:1 (`move_down` … `script`), Diagonalen normiert, `turn_90d_*`-Tabellen korrekt, `repeat`-Verhalten, Ersetzung alter Route (`force_move_route`-Pendant). **Nur die CODES sind falsch — siehe F1.** |
| Richtungen | XP-Richtungscodes 2/4/6/8 in `YAW`/`RICHTUNGEN` korrekt (2=+z unten, 8=−z oben). |
| Passierbarkeit | 3D-Übersetzung (blocked-Zonen + `Passage` + Steiggrenze) ist intern konsistent und eine Quelle für Spieler/NPC/Routen/`passable?` — kein Kachel-4-Richtungs-Modell, aber als 3D-Äquivalent sinnvoll (19k.3). |
| Kampf-Grundstruktur | Runde, Eingabe je Held, KO bleibt KO, Items mit Reservierung, Sieg/Niederlage/Flucht, Exp/Gold/Beute — Aufbau wie XP-Phasen (vereinfachte Details: D4). |
| Seiten-Refresh-Kadenz | Rune3D aktualisiert Seiten jeden freien Frame — XP nur bei `need_refresh` (Schalter-/Variablenbefehle setzen das). Rune3D ist damit *nicht* weniger richtig, eher konsistenter. |
| Warte-/Takt-Einheiten | `wait` in Sekunden, Bewegung in Metern — konsequent auf die 3D-Laufzeit übersetzt, dokumentiert. |

---

## B. Klare Logikfehler (sollten gefixt werden)

### F1 — XP-Bewegungsroute-Codes sind um +10 verschoben — **behoben (Runde 11)**

- **Befund:** `Mover::XP_CODES` (`game/engine/mover.rb:71-92`) und
  `MapCheck::MOVER_XP_CODES` (`game/engine/mapcheck.rb:112-116`) mappen
  `'11' => move_down` … `'57' => script`.
- **XP-Tatsache:** `XP-Defaultscript/019 - Game_Character 2.rb` dispatcht
  `when 1 # Move down` … `when 15 # Wait` … `when 44 # Play SE`,
  `when 45 # Script` (Ende-Marker `code == 0`). Unabhängig bestätigt durch die
  dokumentierten Konstanten `Game_Character::ROUTE_MOVE_DOWN = 1` …
  `ROUTE_SCRIPT = 45` (RGSS/XP, mehrere Quellen).
- **Konkrete Fehlwirkungen:**
  - XP-Code `15` (warte) → führt still `move_lower_left` aus.
  - XP-Code `1`–`10` → „unbekannter Schritt", wird übersprungen.
  - XP-Code `45` (script) → führt `direction_fix_on` aus.
  - XP-Code `44` (play SE) fehlt komplett in Tabelle und Schrittliste.
  - XP-Code `0` (Ende-Marker, XP hängt ihn immer an) wird als unbekannt gewarnt.
- **Warum die Wächter das nicht fingen:** `move_route_test.rb:125` und die
  MapCheck-Liste prüfen nur die *Inter-Sync* (MapCheck == Mover), nicht die
  Übereinstimmung mit der XP-Referenz. Beide Seiten sind also identisch falsch.
- **Bestandsdaten unkritisch:** Keine Karte im Repo nutzt numerische Schritte
  (Demo/`projekte` verwenden ausschließlich Namen).
- **Fix-Skizze:**
  1. `Mover::XP_CODES` auf `'0'=>''`, `'1'..'45'` korrigieren
     (`'44' => 'play_se'`).
  2. `Mover::SCHRITTE` + `MapCheck::MOVER_SCHRITTE` um `play_se` ergänzen;
     `ausfuehren` spielt den Ton aus `r.code` (Namensstring),
     `MOVER_CODE_PFLICHT` += `play_se`.
  3. `MapCheck::MOVER_XP_CODES` angleichen; CODE_PFLICHT-Prüfung
     (`mapcheck.rb:837-842`) prüft derzeit nur Rohtext-Schritte — vorher
     über die XP-Code-Tabelle normalisieren, sonst fehlen „braucht code“-Warnungen
     bei XP-Codes `27/28/29/45`.
  4. Kommentare in `mover.rb` („XP-Codes 11..46") und `mapcheck.rb`
     („'11'..'57'") korrigieren.
  5. `move_route_test.rb` um eine **XP-Referenz-Assertion** ergänzen
     (hartkodierte 1–45-Tabelle gegen `Mover::XP_CODES`), damit der Wächter
     künftig echten XP-Abgleich liefert.

### F2 — Event bleibt aktiv, obwohl **keine** Seite erfüllt ist — **behoben (Runde 11)**

- **Befund:** `Event#refresh_page!` (`game/engine/event.rb:103-106`):
  `idx = @pages.rindex { … } || 0` — findet keine erfüllte Seite, wird
  **Seite 0 trotzdem aktiv** (Trigger + Kommandos laufen).
- **XP-Tatsache:** `Game_Event#refresh` (`021`): `new_page` bleibt `nil` →
  `@page = nil` → `@trigger = nil`, `@list = nil` — das Event ist **inaktiv**:
  kein Auslöser, keine Kommandos, keine Grafik.
- **Folge in Rune3D:** Ein Event mit einer einzigen *bedingten* Seite (z. B.
  „Zeige Text, wenn Schalter an") läuft vor Erfüllung der Bedingung — im XP
  passiert gar nichts. Auch Klick/Maus-Picking kann es starten
  (`manual_fire` prüft nicht).
- **Bestandsdaten unkritisch:** Keine Karte im Repo hat Bedingungen auf
  Seite 0 (geprüft: `game/data`, `projekte/*/data`) — der Fix ändert das
  Verhalten bestehender Demoprojekte nicht.
- **Fix-Skizze:**
  1. `refresh_page!`: ohne Treffer `@page_idx = nil` (inaktiv); `fired` wie XP
     `clear_starting` bei jedem Wechsel (auch auf inaktiv) zurücksetzen.
  2. `active_page` nil-sicher (`@pages[nil]` wäre TypeError), `trigger` →
     `active_page&.trigger` (nil = inaktiv; `main.rb`-Schleifen vergleichen
     nur `==`/`include?` — nil-sicher), `commands` → `[]`.
  3. `manual_fire` startet nichts, wenn keine aktive Seite.
  4. `page_index` darf `nil` liefern; `event_pages_test.rb` um „keine erfüllte
     Seite → inaktiv“ ergänzen (der aktuelle Test deckt den Fall nicht ab).

---

## C. Semantik-Abweichungen (Entscheidung nötig — nicht still ändern)

| # | Thema | XP | Rune3D | Empfehlung |
|---|---|---|---|---|
| D1 | **`auto`-Trigger** | **Autorun:** startet jeden Frame neu, läuft endlos bis Seitenwechsel/`erase` (klassisch: Self-Switch am Ende, sonst Endlosschleife) — `021:214` + `check_event_trigger_auto` | feuert **einmal pro Seite** (`fired`) | Für XP-Exaktheit: auf Autorun umstellen. Demo hat kein `auto`-Event → unkritisch. Alternative: einmalig lassen und als Abweichung dokumentieren. **→ entschieden (Z6): XP-treu umgesetzt in Runde 17** (Neustart in `main.rb`, `fired` nur noch Log-Marke). |
| D2 | **`touch`-Trigger** | feuert bei **jeder** Ankunft des Spielers auf dem Event-Feld erneut (`Game_Player`, `check_event_trigger_here([1,2])` bei `last_moving`); Event-Touch feuert dauernd, solange Event auf Spielerfeld | **einmal pro Seite** (RADIUS + `fired`), nie wieder vor Seitenwechsel | XP-treu: bei Wiederbetreten erneut feuern (Self-Switch macht es einmalig — wie XP-Autoren es gewöhnt sind). **→ entschieden (Z6): XP-treu umgesetzt in Runde 17** (Rearm beim Verlassen der Nähe in `main.rb`). |
| D3 | **NPCs/Events während Text** | Karten-Update dreht Events **permanent** weiter (eigene Parallel-Interpreter je Event, autonome Bewegung auch bei offenem Textfenster) | **Ein** Interpreter; Seiten-Refreshs, NPC-Bewegung und NPC-Kollision laufen seit **Runde 20** auch bei offenem Text (Auslöser auto/touch/E erst danach); parallel-Events können weiterhin nicht *während* eines Textes starten (ein Interpreter) | **→ umgesetzt (Runde 20):** Update-Reihenfolge in `game/main.rb` entkoppelt, geprüft von `tools/encounter_test.rb` (NPC läuft, Seitenwechsel, auto wartet, Spieler bleibt eingefroren). Rest-Grenze (echte Parallel-Interpreter je Event) steht im Backlog. |
| D4 | **Kampf-Details** | Reihenfolge: alle Kämpfer gemischt nach `agi + rand(10+agi/4)`; Formel mit Treffer/Miss, `str`-Skalierung, Elementen, Kritik `4*dex/agi` (×2), Wache (÷2), Varianz; Flucht `50*actors_agi/enemies_agi`; EXP für alle lebenden + `cant_get_exp?`-Freigabe; **Level-Up in `exp=`** | Reihenfolge: erst Held-Block (nach agi), dann Gegner; vereinfachte Formel (kein Miss/Wache/Element, Kritik agi-basiert ×3/×1.25); Flucht fest 50 %; EXP nur für lebende Held; **Level-Up seit Runde 22** (XP-Kurve 2.7, Gesamt-Exp, prozentuales Wachstum) | **→ Level-Up umgesetzt (Runde 22):** `Party.gain_exp`/`Party.set_exp`/`level_up!`, RGSS-Route `actor.exp = n`, Kampf nutzt `gain_exp`. **Fertigkeiten/Zustände seit Runde 23 (20a):** Befehl `Fertigkeit` (XP-Reihenfolge), MP-Kosten, Schaden/Heilung/Buff, Zustände mit Dauer und Werte-Boni, Gift/Regeneration pro Runde. Offen: gemischte Aktionsreihenfolge (`agi + rand`), Wache, Treffer/Elemente, Flucht nach Agi-Verhältnis. |
| D5 | **Wahl-Abbruch** | Show Choices mit optionalem Abbruch-Zweig (403) | cancel-Feld + Abbruch-Zweig vorhanden; Esc gehört während Eingaben der Eingabe, nicht dem Menü | → **umgesetzt (Runde 16):** `cancel`/`cancel_branch` in Engine+Editor+MapCheck+Tests; XP-Default „nicht erlaubt“ bleibt Standard. |
| D6 | **Einheiten** | `wait` in Frames (40/60 s), Variablen-Command mit 6 Operationen/8 Quellen | Sekunden; `variable` nur `value`/`add` | Bewusst; Operationen/Quellen sind Teil von 20d. |
| D7 | **Action-Trigger** | Taste C: Event auf eigenem Feld **oder direkt davor** (Blickrichtung, 1 Kachel) | Reichweiten-Kugel (Default 2 m) + E + Mausklick-Picking | 3D-Adaption, sinnvoll — Radius ist der „davor“. So belassen. |
| D8 | **Route an der Wand** | `skippable=false` (**XP-Default**): Route **stockt** am Hindernis (Index nicht erhoben, versucht dauerhaft) — `019:241` | Blockierter Schritt gilt als erledigt, Route macht weiter (19k.3: NPCs sollen Wegpunkte überspringen) | Bewusste Verbesserung gegenüber XP (vermeidet hängende NPCs). Optional `skippable` als Routenfeld ergänzen, Default bleibt Rune3D-Verhalten — sonst brechen 19k.3-Patrouillen. |
| D9 | **Mausklick** | existiert nicht | Klick feuert **jedes** gehoverte Event (auch `touch`/`auto`/`parallel`, ohne `fired`-Prüfung) — `main.rb:815-820` | → **behoben (Runde 11):** nur aktive `action`-Events starten per Klick; inaktive/andere Trigger werden nur geloggt. |

---

## D. Editor-Logik — Bewertung

### Solide ✅
- **Transaktionale Formulare** (19o.1–19o.6): F2/F5/F6/F7/F9 lesen nichtmutierend,
  übernehmen nur geänderte Felder, blockieren bei Fehlern, Rückfragen bei
  kaputten Altdaten, bytegenaue Dateiprüfung (`document_store_win.h`).
  Das ist *besser* als das, was viele XP-Dialoge tun (die schreiben bei Abbruch
  teils trotzdem um).
- **Parität Engine/Editor:** `MapCheck` (Ruby) und `issues_scan_map` (C++)
  prüfen dieselben Regeln; `tools/sync_test.rb` und `move_route_test` wachen
  maschinell über die Doppellisten (Befehle, Trigger, Operatoren, Mover-Schritte).
- **Probleme-Dialog F10** und Karten-Scan nach jedem Laden — gute
  Datenqualitäts-Kontrolle, die XP so nicht hat.

### Strukturelle Schwachstellen / Verbesserungen
1. **Vier parallele Befehlsregister:** `EV_TYPES` (`event_form.h:37`),
   `EV_TYPE_LABELS` (`editor_main.cpp:6798`), `MapCheck::CMD_TYPES`
   (`mapcheck.rb:62`), `when`-Liste (`interpreter.rb`). `sync_test` fängt
   Vergessen, aber jede neue Liste bedeutet 3–4 Stellen + Test-Anpassung.
   **Verbesserung:** eine gemeinsame Quelle (`game/data/befehle.json`,
   Backlog-Idee) — C++ und Ruby lesen dieselbe Datei; Doppel-Pflege fällt weg.
2. ~~**Undo/Redo nur für `objects`** (`editor_main.cpp:3157-3160`): Splats,
   NPCs, Events, Höhen nicht rückholbar — XP hat zwar auch kein Editor-Undo,
   aber für den 3D-Arbeitsfluss ist es der spürbarste Editor-Mangel (Backlog).~~
   **→ behoben in Runde 19 (Z8/T):** Schnappschuss der ganzen Karte.
3. **Route-Schritte im F6 ungeprüft:** `MapCheck` validiert erst bei
   Kartenladen/F10; das Einfügen-Formular zeigt Fehler erst später an.
   **Verbesserung:** Validierung direkt beim Übernehmen (gleiche Listen,
   kurze Meldung unter der Liste).
4. **XP-Dialog-Design:** F6/F9/F11 sind funktional, entsprechen aber noch
   nicht dem XP-Screenshot-Aufbau (Kategorienleiste, Liste+Formular,
   Seitenreiter) — offener Design-Auftrag mit deinen Screenshots (Backlog).
5. **Klick-Trigger (D9)** betrifft auch den Editor-Playtest-Fluss — beim
   Fix mitnehmen.

---

## E. RGSS-Schicht — nutzt das Spiel den Ruby-Code wie XP? (`rgss.rb`)

### E.1 Was exakt zu XP passt ✅
- **Timer:** XP `command_124` setzt `timer = Sekunden * Graphics.frame_rate`
  und `Game_System#update` zieht 1 Frame pro Takt ab
  (`066:196`, `001:207`). Rune3D `timer_start`/`update(dt)` rechnet
  identisch (Frames, `timer_seconds` zurück) — **wörtlich dieselbe
  Semantik**, nur Echtzeit-treu statt Frames. (Ohne HUD — Backlog.)
- **Input-Konstanten** A…F9 = Werte 11…29, Richtungen 2/4/6/8 — identisch
  zu RGSS1; `dir4` korrekt; `trigger?`/`press?` auf Engine-Polling.
- **Schalter/Variablen:** `[]`/`[]=` wie XP (Default 0/`false`), Integer
  werden zu Strings — beide Formen funktionieren. Operatoren der
  Seiten-/Verzweigungsprüfung vollständig.
- **Self-Switches:** XP-Schlüsselformat `[karte, event, buchstabe]` wird
  verstanden (Buchstabe normalisiert, A–D geprüft); interner String-Schlüssel
  als zweite Form.
- **`$game_screen`:** `start_tone_change/start_flash/start_shake` mit
  XP-Argumenten; Wetter-Zahlen 0/1/2/3 wie XP (`weather_type` mappt zurück).
- **Graphics:** `fadeout/fadein` rechnen Frames über `frame_rate` wie RGSS;
  `brightness` 0…255; `frame_rate` Default **40 = XP-Framerate**.
- **Audio-API** vollständig (bgm/bgs/me/se + fade), Lautstärke 0…100.
- **`$game_temp`:** Feldliste 1:1 aus `000 - Game_Temp.rb` (inkl. Defaults).
- **`$game_map`:** `valid?`, `encounter_list/encounter_step` (XP-Default 30),
  `map_id`, `blocked?`/`hindernis?` als 3D-Ersatz für Kachel-Prüfungen.
- **`$game_player`:** `direction` 2/4/6/8, `increase_steps`,
  `make_encounter_count` = XP-Formel `rand(n)+rand(n)+1` (`022`),
  `passable?` mit XP-Richtungsvektoren + Kartengrenze + Block-Zonen +
  Geländestufe.
- **Setup-Reihenfolge:** `RGSS.setup` → Karte → `Scripts.run_startup` —
  Start-Scripts laufen mit Karte und Globalen bereit, vor dem ersten Frame
  (XP: Scripts vor `Scene_Title`; Rune3D hat sogar mehr vorbereitet).

### E.2 Fehlende RGSS-API — **F3 behoben (Runde 11: actors/steps/item_number, events-Hash, dir8)**
| Lücke | XP-Nutzung | Wirkung in Rune3D |
|---|---|---|
| `$game_party.actors` | überall (`014:12 attr_reader`, `Game_Player`, `Spriteset_Battle`, …) | **NoMethodError** in XP-Scripts; intern ungenutzt → additive Methode sicher |
| `$game_party.steps` | `Window_Steps`, `Game_Player:69` | ebenso |
| `$game_party.item_number` | XP-Name für Item-Anzahl | wir haben nur `item_count` — Alias ergänzen |
| `$game_map.events` | **Hash** `{id => Event}` (`events[id]`, `events.each {\|id, ev\|}`) | wir liefern ein Array → `events[3]` ist das 4. Event, `each`-Blockform zerfällt. Editor vergibt eindeutige IDs (Demo 1–6, `event_next_id`) → Hash-Ansicht machbar; interne Nutzer gibt es keine (geprüft) |
| `Input.dir8` | RGSS-API (Defaultscripts nutzen es nicht) | fehlt — geringe Priorität |

### E.3 Bewusste/dokumentierte Abweichungen ⚠️
- **Calling-Flags (seit Runde 28 ausgewertet ✅):** `$game_temp.battle_calling`,
  `menu_calling`, `save_calling`, `player_transferring` und `to_title` laufen
  jetzt über `TempActions.tick` (aufgerufen aus `Game.update` vor der
  Menü-/Interpreter-Logik). XP-Reihenfolge: Kampf vor Menü/Speichern vor
  Transfer; pro Frame genau ein Auftrag, das Flag wird vorher gelöscht (kein
  Dauerfeuer, wenn der Kampf abgelehnt wird). Details:
  - `battle_calling` → `Battle.start(troop_id, can_escape, can_lose)`.
  - `menu_calling`/`save_calling` → Menü öffnen (respektiert
    `$game_system.menu_disabled`); bei `save_calling` steht der Zeiger auf dem
    ersten Speichern-Eintrag. XP hätte eine eigene Szene — hier reicht der
    Menüeintrag, weil Speichern/Laden dort liegt.
  - `player_transferring` → `Game.change_map`; `player_new_map_id` darf der
    Kartenname sein oder die 1-basierte Position in `Database.map_ids`
    (XP-Karten sind numerisch). Unbekannt → Warnung + Issue, kein Wechsel.
    `player_new_x/y` gelten als Weltkoordinaten (`0/0` = bisherige Stelle
    behalten, weil XP-Events die Felder nicht immer setzen),
    `player_new_direction` 2/4/6/8 → Kamera-Yaw über `TempActions::RICHTUNG_YAW`
    (8 = π = Engine-Standard, 2 = 0, 6 = +π/2, 4 = −π/2). Bewusste Abweichung:
    XP rechnet in Kacheln, Rune3D in Weltkoordinaten — Projekte geben also
    Weltkoordinaten an.
- `$game_switches[1]=true` aus einem Script bewirkt bei XP **nicht**
  automatisch einen Seiten-Refresh (XP braucht `$game_map.need_refresh`);
  Rune3D aktualisiert Seiten jeden Frame — hier sind wir *nicht* XP-gleich,
  aber konsistenter (kein XP-Quirk).
- `Input.repeat?` = `trigger?` (XP hat Taktung) — Menüs, die auf Repeat
  aufbauen, laufen schneller.
- `Audio.bgm_fade` = sofort stoppen; Pitch ignoriert; `se_stop` No-Op.
- `load_data/save_data` = JSON statt Marshal (dokumentiert).
- Event-Inline-Scripts laufen in `Context.instance_eval`
  (`scripts.rb:110`), XP `command_355` wertet auf Top-Level aus:
  `$game_*`/Konstanten/Helfer funktionieren, aber `def` in Event-Scripts
  definiert anders (Singleton statt Object-Privatmethode). Start-Scripts
  laufen korrekt über `TOPLEVEL_BINDING` ✓.
- Kein Karten-BGM-Autoplay (XP `autoplay_bgm`); `map_bgm` fehlt in
  `GameSystem` — hängt am fehlenden Audio-Dateisystem.
- `$data_*` Teilmenge (actors/items/system; classes/skills/weapons/… fehlen
  — Roadmap 20a).

---

## F. Karten-System — Editor-Karten ↔ XP-Kartendaten ↔ Laufzeit

### Feldvergleich F2 (Karteneigenschaften) ↔ XP-Dialog
| XP-Karteigenschaft | Rune3D (F2/`map_props_form.h`) | Status |
|---|---|---|
| Name | `name` | ✅ |
| Tileset | F7-Modell (Boden-Textur/Kacheln/Splats) | ⚠️ anders (3D) |
| Width/Height (Kacheln) | `terrain.width/height` (Meter) | ✅ sinngemäß |
| Scroll Type | — (Kamera) | n/a 3D |
| Parallax (Bild, Loop, Speed) | `panorama.color` (nur Farbe) | ⚠️ Bild fehlt |
| Fog (Farbe/Tone/Opacity/Zoom) | `fog.on/far/color` | ⚠️ Teilmenge |
| Battleback | — (3D-Kampfumgebung) | n/a 3D |
| Autoplay BGM/BGS | — (kein Karten-Audio) | ❌ |
| Encounter Steps / Specify Encounter | `encounters.steps` + gewichtete Truppenliste | ✅ (besser: Gewichte) |
| Spieler-Start (Editor-Klick) | `start {x,z,yaw}` + Projektstart | ✅ |
| — | `weather`, `blocked`, `note`, `terrain.seed/amplitude` | Rune3D-Zusatz |

### Laufzeit & Baum
- `Game.enter_map` (`main.rb:150-220`) baut in XP-Reihenfolge auf:
  Events → Terrain → Atmosphäre → Block-Zonen → Interpreter →
  Encounter-Wurf → NPCs → Wetter → MapCheck → Objekte → Startpunkt
  (inkl. Warnung bei Start in der Block-Zone). Solide.
- **Kartenbaum ist flach** (`TVI_ROOT`, `editor_main.cpp:2727`) — XP hat
  echte Hierarchie (`parent_id` in der Kartensystemtabelle). Für große
  Projekte der spürbare Unterschied; eigener Ausbau-Punkt.
- Datenhaltung pro Karte eine JSON-Datei statt `MapXXX.rxdata` +
  Info-Tabelle — für Versionskontrolle und Editor besser; IDs = Dateiname
  (XP: numerisch, Name in Info-Tabelle). `Database.map` cached; F5 liest
  `system.json` neu, Playtest läuft als eigener Prozess (kein
  Cache-Konflikt).

---

## G. Editor-Design & Umschaltung

- **Werkzeug-Modi:** `set_mode` (`editor_main.cpp:2909`) zentriert fünf
  Radiomodi (Setzen/Boden/Höhe/…) — entspricht XP-Ebenenumschaltung
  (Boden-/Event-Ebene), nur als 3D-Werkzeuge. Konsistent, ein Haken.
- **Dialog-Umschaltung:** Pending-Schutz ist real implementiert:
  `object_form_pending()`/`event_editor_pending()` (`:3098`, `:7407`),
  `data_prompt` sperrt andere Fenster, F7-Listen sperren sich bei
  unvollständigen Entwürfen, Kartenbaum-Aktionen warten auf abgeschlossene
  Formulare (F2/F6/F5-Richtung geprüft, 19o/19p). **Besser als XP**, das
  nur modale Dialoge kennt — aber nur, solange die Guards gewartet werden
  (drei Stellen: Objekt-, Event-, Datengrundlage).
- **Offen (Backlog):** XP-Screenshot-Design für F6/F9/F11, Spaltenbreiten/
  Splitter-Persistenz (Undo-Scope und Kartenbaum-Hierarchie sind seit
  Runde 19 erledigt). Keine Logikfehler gefunden — die
  Schwachstellen sind Doppelstrukturen (D.1) und Fehlende (D.2).

---

## H. System des Rubycodes (Script-Editor F11 + Projekt-Scripts)

- **Ausführung:** Start-Scripts in `TOPLEVEL_BINDING.eval` (Klassen,
  Monkey-Patches wirken wie XP-Scripts), Reihenfolge = Script-Editor-
  Reihenfolge, opt-in `autoload` (XP führt alle aus — bewusste
  Unterscheidung, dokumentiert: alte Event-Scripts laufen nur auf Anfrage).
- **Fehlerbehandlung:** `rescue` + `Issues.report` + Log statt Absturz;
  XP lässt das Spiel bei Scriptfehlern sterben. Rune3D ist hier robuster.
- **F5-Hot-Reload** (besser als XPs F12-Reset) — Gesamtzustand wird
  neu aufgebaut (`Game.setup`), `$game_*` frisch aus `system.json`.
- **Keine Sandbox** (wie XP) — dokumentiert.
- **Lücke:** Event-Inline-Scripts in `Context` statt Top-Level (siehe E.3);
  XP-nahe Scripts, die dort `def` oder `include` auf Top-Level erwarten,
  verhalten sich anders. Praktisch selten, aber beim Vergleich merken.

---

## J. Speicherstände — XP `Scene_Save`/`Scene_File` ↔ Rune3D

### Was XP speichert vs. Rune3D
| XP (Marshal-Objekt) | Rune3D (`slot<N>.json`) | Status |
|---|---|---|
| `$game_switches`/`$game_variables`/`$game_self_switches` | `state.snapshot` (alle drei) | ✅ |
| `$game_party` (Gold, Items, HP/MP/Level/EXP je Held) | `party.snapshot` | ✅ (max_hp wächst seit Runde 22 beim Level-Up, D4) |
| `$game_map` (Events: Positionen, Seiten, **gelöschte Events**, laufende Routen) | `fired`-Namen + NPC-Snapshots (x/z/Wegpunkt) | ⚠️ Seiten werden aus Schaltern neu berechnet (gleichwertig); **gelöschte Events** gibt es nicht (fehlender Befehl 116); laufende Bewegungs-Routen sind flüchtig (`Mover.reset!`) — XP stellt sie wieder her |
| `$game_player` (Position, **steps, encounter_count, Richtung**) | Position + Kamera | ⚠️ Schritte/Zähler werden beim Laden neu gewürfelt (XP bewahrt den Restwert) |
| `$game_system` (Timer, `save_count`, **menu/save/encounter_disabled**, Windowskin, BGM) | `'system'`-Snapshot im Spielstand (`Game.system_snapshot`: Sperr-Flags, Timer, `save_count`, das Save-Game zählt wie XP mit); alte Spielstände ohne `'system'` behalten die Defaults (`system_restore!` = Noop) | ✅ seit Runde 14 (Windowskin/BGM weiter nicht gespeichert — braucht Audio-Infrastruktur, s. N) |
| `$game_screen` (Tone/Flash/Shake, **Wetter**) | — | ⚠️ Laufzeit-Wetter-Änderungen gehen verloren (Karten-Wetter wird aus der Karten-JSON wiederhergestellt) |
| `Graphics.frame_count` (Spielzeit) | `time` | ✅ |
| Zeichen für die Slot-Vorschau (Gesichter) | `save_info` liefert `saved` + `map` + `time`; Menü und `Scene_File` zeigen Karte · Spielzeit (Runde 14). **Seit Runde 25** zeichnet das Menü prozedurale Parteigesichter (`UI.gesicht`: Farbe/Initialen/HP-Streifen); die Slot-Vorschau bleibt Text | ⚠️ Gesichter nur im Menü, nicht in den Slots |
| Editor-Schreibschutz | — | ⚠️ `Project.save` schreibt direkt (kein Temp+Rename) — Karten/F9 sind geschützt, Spielstände nicht (Backlog gilt); XP ist ebenso ungeschützt, wir haben aber die besseren Werkzeuge parat |

### Weitere Befunde
- **Silent-Karten-Fallback:** Fehlt die gespeicherte Karte, lädt `load_game`
  still die erste Karte — die Position wird dann auf fremdem Boden gesetzt.
  → **behoben (Runde 11):** fehlende/unlesbare Karte meldet `Issues.report`
  mit `error` und bricht ab, **bevor** Zustand/Position angefasst werden.
- **Mitten im Event laden:** Das Menü pausiert den Interpreter, das Laden
  baut einen neuen — XP stellt den laufenden Event-Interpreter aus dem
  Marshal wieder her. Abweichung, praktisch akzeptabel (XP-Verhalten gilt
  oft als Überraschung), aber dokumentieren.
- Lade-Robustheit: kaputte/leere Slots → `nil` + Warnung (XP crasht beim
  Marshal) — Rune3D hier robuster ✅.

## K. Launcher/Titel & Game-Over — XP `Scene_Title`/`Scene_File`/`Scene_Gameover`

> **Vorgabe (s. AB):** Der Spiel-Launcher entfällt! Die Game-Engine
> startet in eine **Scene_Title** (New Game/Continue/Shutdown), per
> Script-Editor austauschbar; **neue Projekte gibt es nur noch im
> Editor**. **Umgesetzt in Runde 12:** `launcher.rb` entfernt,
> `Scene_Base`/`Scene_Title`/`Scene_File` eingeführt (Details in AB).

| XP | Rune3D | Status |
|---|---|---|
| Titel: New Game / Continue / Shutdown | `Scene_Title`: Neues Spiel / Fortsetzen / Spiel beenden (`command_new_game/command_continue/command_shutdown`); Esc beendet nicht (XP-treu) | ✅ seit Runde 12 |
| Slot-Vorschau (Spielzeit, Zeitstempel, Party-Gesichter) | `Scene_File` und Esc-Menü zeigen Karte · Spielzeit (`save_info.time`, Format `Game.zeit_text`) · Zeitstempel; Party-Gesichter fehlen (keine Face-Infrastruktur) | ✅ seit Runde 14 (nur Gesichter offen, mit 20b) |
| `Scene_Gameover`: Game-Over-Bild, Tastendruck → Titel | **`Scene_Gameover`** (schwarz, „Game Over", „[Enter] Weiter zum Titel"); `process_temp_scene_change` zeigt erst den Screen, eine Taste ruft `to_title!` | ✅ seit Runde 14 |
| Shutdown | Menüpunkt „Spiel beenden" = `Rune::App.quit` (Esc auf dem Titel beendet bewusst nicht, XP-treu) | ✅ |
| Fortsetzen-Liste mit Beschriftung „File 1…" | `Scene_File` (Continue vom Titel) mit 3 Slots + Vorschau | ✅ seit Runde 12 (K-Lücke geschlossen; das Esc-Menü behält seine Inline-Slots) |

**Fix-Kandidaten (klein, sichtbar):** ~~Game-Over-Bildschirm, Slot-Anzeige
mit Spielzeit/Karte~~ **→ erledigt in Runde 14** (`Scene_Gameover`,
`save_info.time` + `Game.zeit_text` in Scene_File/Menü). Offen: nur noch
Bestätigung bei `L`, falls gewünscht.

## L. Kampf-Ablauf (Ergänzung zu D4)

- Phasenmodell (Eingabe je Held → Sammeln → Ausführung → Gegnerzug →
  Runde) entspricht XP-Phasen 2–4 strukturell; `can_lose` endet ohne
  `gameover`-Flag (wie XP `battle_can_lose`) ✅; Item-Reservierung pro
  Runde ist **besser** als XP (XP zieht erst bei Ausführung ab).
- Neu aus Runde J: **Niederlage ohne Game-Over-Screen** (siehe K).
- Unverändert offen (Roadmap): 601–603-Ergebniszweige, Skills/Wache,
  Gegner-KI, Battle-BGM/-ME, Befehle 331–340, Fluchtformel, EXP für
  tote Helden, Level-Up (D4).

---

## M. Dialoge & Textfenster — XP `Window_Message`/Befehle 101–104 ↔ Rune3D

| XP | Rune3D | Status |
|---|---|---|
| 101 Show Text + 401-Textzeilen (Fenster max. 4 Zeilen) | `text` mit `lines` (unbegrenzt, Fenster wächst mit) | ⚠️ keine 4-Zeilen-Begrenzung (XP-Editor begrenzt; Rune3D überläuft nicht, sondern wächst) |
| 101-Parameter: **Gesicht** (face_name/index), **Position** (oben/mitte/unten), **Hintergrund** (abdunkeln/Fenster/transparent), Fenster-Fade-in/out | — | ❌ keine Gesichter, feste Position unten, ein Stil, kein Fade |
| **Text + Auswahl/Zahl im selben Fenster**: 102/103 hängen an 101 (`choice_start = line_count`, Eingabefeld im Textfenster) | `text`/`choices`/`input_number` teilen sich seit **Runde 21** ein Fenster: der Text bleibt stehen, die Optionen erscheinen darunter, das Fenster schliesst beim ersten anderen Kommando | ✅ **umgesetzt (Runde 21):** `Interpreter#visible_lines`/`text_waiting?`, `UI.message` mit `choices`-Argument, `main.rb` zeichnet nur noch ein Fenster; geprüft von `tools/event18f_test.rb` (7 Kombi-Checks) |
| 102 + **cancel_type** + 403 Else (Abbruch-Zweig) | cancel-Feld `aus`/`verzweigung`/Optionsnummer + eigener Abbruch-Zweig (`cancel_branch`), Esc-Routing Menü↔Eingabe gesperrt | ✅ seit Runde 16 (Kombi-Fenster Text+Wahl s. Zeile darüber bleibt ❌) |
| 103 Input Number **im Textfenster**, ohne Abbruch-Taste | eigenes Ziffernfenster, **Esc=Abbruch(0)** als Default (editor `cancel`) | ⚠️ Rune3D erweiternd (XP kennt kein Esc hier); Position/Fade abweichend, Kernlogik ✓ |
| 104 Change Text Options (Position/Hintergrund mitten in der Meldung) | — | ❌ fehlt (Nische) |
| Pause-Zeiger ▾, wenn auf Bestätigung gewartet wird | blinkender `▾` unten rechts im Textfenster (`UI.message`, 2-Hz-Blinkleuchten) | ✅ seit Runde 14 |
| Effekt: Fenster-Fade, kein Tipp-Effekt (XP zeigt alles sofort) | sofort alles da | ✅ gleichwertig |
| Choices mit Pfeiltasten + OK/Abbruch-SE (decision/cancel SE) | Pfeiltasten/Klick + `klick`-Prozeduralton | ⚠️ funktionell ✓, Sound-Bestand s. N |

Ergänzungen zur Bewegungssperre: XP sperrt in `Scene_Map#movable?` die
Spielerbewegung, solange `message_window_showing` — Rune3D **nicht** (siehe O).

## N. Ton/Audio — XP `Audio`/`Game_System` ↔ Rune3D

| XP | Rune3D | Status |
|---|---|---|
| Dateibasiert: `Audio/BGM|BGS|SE|ME/*.ogg` werden geladen (`Game_System.bgm_play` mit `RPG::BGM`-Objekt, Volume+Pitch) | `src/core/sound.cpp` kennt **nur** per `Rune::Sound.register` angemeldete Puffer; `Audio.bgm_play("Audio/BGM/x.ogg")` → Basisname `x` → **unbekannter Name → stiller No-Op** (`sound.cpp:101`) | ❌ **kein Datei-Audio** — RGSS-Skripte mit echten XP-Dateinamen spielen wortlos nichts ab |
| BGM-Play/-Stop/-Fade mit Zeit (241/242/244) + gemerkter Zustand (`@playing_bgm`, Wiederherstellung nach Kampf/Menü) | `Audio.bgm_fade` = sofortiger Stop (dokumentiert), kein Playback-Gedächtnis jenseits `@bgm` | ⚠️ Fade sofort statt über Zeit; Wiederherstellen nach Kampf egal (kein Kampf-BGM) |
| Event-Befehle 241 BGM, 242 Fade, 245 SE, 246 Stop SE | Interpreter: nur `sound` (Einmal-Sound, Name+Volume) — **keine BGM-/Fade-/Stop-Befehle**; Editor ebenso | ❌ Befehle fehlen in allen drei Listen (Zusammenhang: 26 vs. 92 XP-Befehle) |
| Map-BGM aus Karten-/Tileset-Daten, automatischer Start bei Kartenwechsel | Karten-JSON hat kein `bgm`-Feld, kein Autoplay (s. F) | ❌ |
| Pitch pro Aufruf (BGM/SE/ME) | `_pitch` wird ignoriert (`rgss.rb`-Kommentar) | ⚠️ dokumentiert, aber API-Lücke |
| System-SE (Cursor, OK, Abbruch, Speichern …) aus `Audio/SE` | 7 prozedurale Sounds (schritt, klick, jingle, ablehnen, sprung, landung) + 1 Musik-Loop (`musik`) | ⚠️ eigenständiges Set; Menüs nutzen `klick`/`ablehnen` konsistent, aber kein Entsprechen zu XP-SE-Bestand |
| BGS-/ME-Delegation vorhanden | `Audio.bgs_*`/`me_play` vorhanden (gleiche No-Op-Gefahr) | ⚠️ API-Schale ✓, Substanz fehlt |
| Lautstärke 0–100 | `lautstaerke` skaliert korrekt ✓ | ✅ |

**Fazit Ton:** Die RGSS-Delegationsschale (`Audio.*`) existiert und ist
sauber kommentiert, aber substanzlos ohne Datei-Lader und Pitch. Ein
eigener „Ton-Etappe"-Block (WAV/OGG-Ladepfad in `sound.cpp`, BGM-Felder
in Karten-JSON, Event-Befehle 241/242/245/246) wäre ein klar umrissener
Roadmap-Baustein (20x analog zu 20f Bilder).

## O. Eingabe & Physik bei offenen Fenstern (neu)

- **Fund:** `physics.update` läuft in `main.cpp:232` immer dann, wenn
  `sim_paused` false ist — und das ist es **nur bei Kampf** (plus
  Playtest-Fehler). Während **Textfenster, Wahlen, Zahleneingabe und
  Esc-Menü** bleibt `paused=false` (Menü-Öffnen setzt es nicht, der
  normale Frame setzt es am Ende auf false): Der Spieler kann dort
  **weiterlaufen/springen**. Doppeldeutig: Leertaste ist gleichzeitig
  Sprung- und Text-Weiter-Taste.
- XP dagegen: `Scene_Map#movable?` sperrt Bewegung bei
  `message_window_showing`; `Scene_Menu` verarbeitet Spieler-Eingabe gar
  nicht. → **F5 behoben (Runde 11):** `Rune::Simulation.paused` wird pro
  Frame aus `busy_text?/busy_choices?/busy_number?` + offenem Menü
  gesetzt (Menü/Text pausieren die Physik; Launcher ebenfalls pausiert),
  `wait` in Routen pausiert bewusst **nicht** (XP-gleich).
- **Panorama:** XP hält `panorama_name`/`panorama_hue` (Tileset-Datei) und
  scrollt den Panorama-Plane mit dem Display — Rune3D nur eine statische
  Farbe (ohne Scroll, ohne Bild). Panorama-Scroll als Detail-Ergänzung
  zu F (Karten-Tabelle).
- **Wetter:** XP `weather(type, power, duration)` (Typen 0–3, weicher
  Übergang über `duration`, Sprite-Anzahl `(power+1)*4`, Befehl 236);
  Rune3D `weather` instant (kein Übergang), Typen `kein/regen/schnee`,
  Intensität 0..MAX. Event-Befehl `weather` + Kartenfeld existieren ✓,
  aber: kein `duration`, fehlender 4. Typ, Laufzeit-Änderung nicht im
  Savegame (s. J), `$game_screen.weather_*` wird nicht gespiegelt.

---

## Q. Kollisions- & Weglogik — XP-Passierbarkeit ↔ 3D-Physik

- ✅ **Eine Quelle statt dreier:** Das `Passage`-Modul (Block-Zonen +
  Physik-Hindernisse) speist RGSS `passable?`, NPC-Patrouillen und
  Bewegungs-Routen. Ein an der Wand gescheiterter Routen-Schritt endet
  den Schritt, die Route macht weiter (`mover.rb:446`) — wie XP, wo ein
  nicht passierbarer Schritt still scheitert. NPC wartet vor dem Spieler
  (Etappe 15) ✓; Wegpunkt innerhalb einer Block-Zone wird übersprungen ✓.
- ⚠️ **`through_on/off` ist dokumentierter No-Op** (`mover.rb:30–32,
  324–327`): XP ignoriert mit `through` die Passierbarkeit (Wände
  durchlaufen, `$game_player.through = true`-Trick); Rune3D kennt weder
  im Mover noch im RGSS ein `through`-Attribut.
- ⚠️ **Routen-`jump` bewegt das Event nicht** (`mover.rb:288–293`): nur
  ein Partikel-Burst am Ziel; XP verlagert das Event tatsächlich (Sprung
  mit Passierbarkeitsprüfung des Ziels).
- ⚠️ **Nicht-NPC-Events blockieren nicht** — bewusst so kommentiert
  (`event.rb:129–131`: Schild-Kästen „nur sichtbar, nicht anklickbar").
  XP sperrt über die Kachel-Passage; in 3D sind unsichtbare Sperren
  schlechter als sichtbare — Abweichung, aber vertretbar.
- ⚠️ `move_player` (Rune3D-Erweiterung) teleportiert **ohne**
  Passierbarkeitsprüfung (`interpreter.rb:162–168`); XP kennt den
  Befehl nicht (Spieler-Bewegung dort nur als Move-Route ✓ vorhanden).
- **Schwellen:** `$game_player.passable?` prüft Zielzellen-Block +
  `MAX_STUFE = 1,2 m` Geländehöhe, die Physik steigt aber nur
  `kMaxStep = 0,35 m` auf Kästen; reine Steilkanten können zwischen API
  und Physik auseinanderfallen (Objekte fängt die Hindernis-Prüfung ab).
  Intern hat `passable?` keine Aufrufer — WASD läuft direkt in der
  Physik; die API ist Skript-Schnittstelle.

## R. RGSS-Koordinaten & Player-API — **F6** (neu)

- **Falle `$game_player.y`:** liefert die **Welt-Höhe** (`rgss.rb:301`),
  XP meint die **Kartenebene**. XP-Idiome wie
  `moveto($game_player.x, $game_player.y)` (vgl. `079:88`) würden auf
  z ≈ 1,6 m teleportieren, Distanzformeln `sy = @y - $game_player.y`
  (`019:152`, `020:198 …`) liefern Unsinn. `moveto`'s zweites Argument
  ist dagegen die Ebene → **Reader/Writer inkonsistent**. Keine internen
  Nutzer von `.y` außerhalb `rgss.rb` (geprüft) → Umstellung
  additiv-sicher wie F3.
- **Events haben kein `y`:** `GameMap#event` liefert die rohen
  Engine-Events (`attr_accessor :x, :z`) — `$game_map.event(1).y` →
  **NoMethodError** (XP: Kachel-y; Auto-Trigger-Vergleiche `021:185`
  brechen damit).
- **Fehlende Player-Methoden** (in XP-Defaultscripts benutzt → jeweils
  NoMethodError): `straighten` (7×), `turn_up/down/left/right`,
  `transparent`, `moving`, `move_route_forcing`, `center`, `update`,
  `through`. Vorhanden ✓: `x/y/z`, `moveto`, `direction`,
  `increase_steps`, `make_encounter_count`, `passable?`, `refresh`.
- `$game_map.width/height` = Weltmeter; da eine Richtungs-Auslösung
  exakt 1 m beträgt (`rgss.rb:361–367`), entspricht das XP-Kachelzahlen
  im gleichen Zahlenbereich ✓ (kein Fehler, nur „Welt ist größer").
- → **F6 behoben (Runde 11):** `$game_player.y` liefert die Kartenebene
  (= Welt-z), `Event#y`/`y=` als Alias, `straighten/turn_*/center/update/
  moving?/transparent/through/move_route_forcing` als tolerante No-Ops/
  Minimal-Implementierungen; Third-Person-Körper respektiert
  `transparent`.

## S. Performance & Rendering — Befund: kein Hotspot, Skalierungshinweise

- `refresh_page!` läuft **jeden Frame für alle Events + NPCs**
  (`main.rb:689`) — XP aktualisiert ereignisgetrieben; korrekt, aber
  O(Events × Seiten) pro Frame. Bei Demo-Größe unsichtbar; ab einigen
  hundert Events lohnt ein Bedingungs-Dirty-Flag.
- UI-Text = GDI `TextOutW` pro Zeichenkette (`ui.cpp:301`) — dutzende
  Strings/Frame unkritisch; ein dichtes Menü mit hunderten Zeilen wäre
  der erste Engpass.
- Weltobjekte = Sofort-`DrawCmd`-Liste pro Frame (`push_box`,
  `renderer.cpp:1122`), kein Batching/keine Instanziierung — tausende
  Boxen in Ordnung, zehntausende zu messen.
- Terrain: ein Mesh pro Karte (`resolution` 192 → ~73k Dreiecke), kein
  LOD; Pick mit Kugel-Vorfilter vor dem Slab-Test (`pick.rb:93`) ✓;
  Sound: waveOut-Doppelpuffer ✓; Schattenpass + Sonnenlicht im
  `end_frame` ✓.
- **Fazit:** Kein Handlungsbedarf; Grenzwerte als BACKLOG-Notiz
  („ab N Events dirty-Flag, ab M Objekte Batching messen").

## T. Editor-Undo (Ergänzung zu G)

- Undo/Redo korrekt gepaart ✓ (`undo_stack`/`redo_stack`, Cap 50,
  `resolve_object_edits()` zieht offene Eigenschaftsfelder vor dem Undo
  nach ✓, Drag drückt beim ersten echten Versatz (`drag_undo_pushed`) ✓,
  Material-Dialog mit Alt-Wert-Bestätigung ✓.
- **Befund (Runde 17):** Scope war nur das `objects`-Array (11 Push-Orte:
  platzieren, löschen, drehen, duplizieren, Eigenschaften, Material, Drag).
  Events hatten kein einziges `undo` (`event_form.h` ohne Undo-Bezug),
  Karteneigenschaften/Terrain/Notiz/Scripts ebenso wenig — Backlog
  „undo nur Objekte" bestätigt und präzisiert.
- ~~**Ausbau (Z8/T):**~~ **→ umgesetzt in Runde 19.** Der Schnappschuss ist
  jetzt die **ganze Karte** (`g.map_root`, JSON, Cap 50), nicht nur
  `objects`. Push-Orte: Objektaktionen wie bisher, dazu F6 (Event/Seite/
  Befehl übernehmen, Seite anlegen/löschen, Befehl einfügen/ändern/löschen/
  verschieben, Event anlegen/duplizieren/löschen), Platzier-Dialog F5
  (NPCs/Schilder/Objekte), Karten-Eigenschaften (Name/Terrain/Nebel/
  Panorama/Wetter/Block-Zonen/Encounter/Notiz), Berg- und Boden-Pinsel,
  F7-Bodenwechsel und -Flächen, Umbenennen im Kartenbaum.
  `restore_map_snapshot()` holt Zeiger neu, baut Terrain, Atmosphäre,
  Marker, F7-Liste und F6 (`ev_refresh()`) neu auf und markiert die Karte
  als geändert. Scheiternde Aktionen legen keinen Schnappschuss ab
  (`push_undo_text` nur bei Erfolg). Grenzen bleiben: Karten anlegen/
  löschen und F2/F5-Dateitransaktionen (Dateioperationen) sowie der
  Projektstart in `system.json` (nicht Teil der Karte); Undo verweigert mit
  Statushinweis, solange der Eigenschaften-Dialog offen ist oder F6
  unübernommene Eingaben hat.

---

## V. Esc-Menü vs. XP `Scene_Menu`

| XP `Scene_Menu` (072) | Rune3D Esc-Menü | Status |
|---|---|---|
| 6 Befehle: Item, Skill, Equip, Status, Save, Game End | Gegenstand, Fertigkeit, Status, Speichern×3, Laden×3, Zurück, Titel (**Runde 24, 20b**) | ⚠️ Equip fehlt (keine Ausrüstung); Slots direkt statt Unterliste = sinnvolle Erweiterung |
| Save wird bei `$game_system.save_disabled` **grau** gezeigt | Aktion wird in `save_game` geblockt (Warnung + `ablehnen`-Ton), nicht optisch deaktiviert | ✅ funktional gleich, optisch anders |
| Nebenfenster: **PlayTime, Steps, Gold, MenuStatus** (Gesichter+HP) | Party-Panel: HP-Zeilen, Gold, Inventar (`Party.status_lines`) **+ Spielzeit (`@time`) + Schritte (`$game_party.steps`)** seit Runde 14; Gesichter fehlen | ✅ Playtime/Steps (Gesichter mit 20b) |
| `@@menu_index` bleibt zwischen Öffnungen erhalten | `@menu_sel = 0` bei jedem Öffnen | ⚠️ Kleinigkeit |
| Item/Skill/Status als eigene Szenen mit Ziel-/Heldenfenster | Unterfenster in `Game`: `@menu_screen` (`:item` → `:item_target`, `:skill` → `:skill_list`, `:status` → `:status_detail`), eine Ebene je Esc | ✅ **umgesetzt (Runde 24)** — `tools/menu_test.rb` (33 Checks) |
| Befehl **351 „Menü öffnen"** aus Events | nur Esc (roadmap 20f, `XP_VERGLEICH` ❌) | ❌ bekannt |
| Scene_Update friert die Karte (eigene Szene) | `update_menu; return` friert Interpreter/NPCs ✓ (Physik nicht — s. O/F5) | ✅ bis auf F5 |
| **Performance:** XP liest Spielstände einmal beim Szenenaufbau (`Scene_File#snapshots`) | ~~`menu_items` rief pro Frame bis zu 12× `save_info` (update+draw)~~ → **seit Runde 14 beim Öffnen in `@menu_items` gecacht** (Schließen/Szenenwechsel invalidieren, Speichern/Laden schliesst das Menü) | ✅ seit Runde 14 |

## W. Zufallskämpfe & Ressourcen-Teardown (F5/Kartenwechsel)

**Encounter (XP `022`/`071` ↔ Rune3D):**
- ✅ Formel `rand(n)+rand(n)+1` mit `encounter_step` (Default 30) identisch;
  Neuwürfelung beim Kartenstart/Transfer ✓; `encounter_disabled` wird
  geprüft ✓; Interpreter-läuft-Sperre und Event-vor-Kampf-Priorität ✓;
  `liste.sample` = XP `rand(list.size)` ✓.
- ✅ **Kadenz angeglichen (Runde 26, W):** XP dezimiert **pro fertigem
  Kachel-Schritt** (`022:264`). Rune3D zählte früher jeden Schritt-Sound
  (0,60 m, rennend 0,85 m) und lief damit je Meter ~40 % zu schnell; seit
  Runde 26 zählt `Game::KACHEL_METER = 1.0` einen Begegnungsschritt pro
  Meter gelaufenen Weges (Teleports/Sprünge ausgenommen). Der Schritt-Sound
  bleibt bei 0,60/0,85 m, `$game_party.steps` zählt weiterhin jeden
  Schritt-Sound. Geprüft von `tools/encounter_test.rb`
  („W: Begegnungen zählen pro Kachel", „W: Rennen verkürzt die
  Kachelstrecke nicht").
- ⚠️ **Slip-Damage fehlt:** XP ruft bei gerader `$game_party.steps`
  `check_map_slip_damage` (Zustands-Schaden, `014:292`). Die Zustände
  selbst gibt es seit **20a** (Runde 23) im Kampf; auf der Karte fehlen
  sie weiterhin, weil Zustände mit dem Kampf enden und der Schritt-Hook
  (`$game_party.steps`, F3) noch keinen Karten-Schaden kennt.

**Teardown-Audit — sauber, kein Leak gefunden ✅:**
- `enter_map`: `Fade/Mover/RGSS.reset!`, `Battle.abort!`,
  `Physics.clear_blocked` + neu, `Effects.clear`, `Weather.load_map`,
  `MapObjects.rebuild`, neue Szene + Placement-Visuals, frischer
  Interpreter — alles zurückgesetzt, was flüchtig ist.
- `Scene#refresh_solids` = `clear_solids` + Neuregistrierung (kein
  Akkumulieren alter Hindernisse); `Textures.end_project` vor
  `register_project` bei F5; Immediate-Mode-Draw (`push_box`) hält keine
  GPU-Objekte pro Node → alte Szenen sind reine Ruby-Objekte (GC).
- `Database`: `playtest_map` invalidiert sich selbst (Live-Edit des
  Editors unter F4/F5 ✓), `reload_system` bei F5, Karten-Cache lebt bis
  zum nächsten F5 — akzeptabel und dokumentiert.
- Einziger Beobachtungs-Wert: `Sounds.register_all` läuft einmal pro
  Prozess (statisch ✓), Musik-Loop überlebt Kartenwechsel (gewollt).

---

## Y. Datenpipeline: Editor → JSON → Laufzeit (End-to-End, Runde 7)

**Geprüft: Feld-für-Feld konsistent ✅**
- **26 Befehlstypen:** `EV_TYPES` (event_form.h) = `CMD_TYPES` (mapcheck)
  = `when`-Zweige (interpreter) — über `sync_test`/`mapcheck_test`
  zusätzlich erzwungen. Feldnamen je Typ einzeln gegengelesen und
  deckungsgleich: `text.lines`, `choices.options`+`branches[[…]]`
  (Array-von-Arrays parallel zu den Optionen), `log.text`, `wait.seconds`,
  `switch.name/value`, `toggle_switch.name`, `variable.name/value/add`,
  `if_switch`/`if_variable` (+`op`,`then`,`else`), `script.name/code`,
  `teleport.x/z/yaw`, `move_player.x/z/relative`, `heal.amount`,
  `weather.name/intensity`, `effect.name/height/color`,
  `sound.name/volume`, `item.name/count/take` (Editor: negative Menge =
  `take`, schreibt `count` **oder** `take` exklusiv — passt zu
  `cmd.key?('take')` im Interpreter ✓), `gold.amount`,
  `save_game`/`load_game.slot`, `self_switch.letter/value`,
  `if_self_switch` (+`then/else`), `fade.direction/seconds`,
  `move_route.target/steps/speed/wait/code/repeat`,
  `input_number.name/digits/initial/cancel`,
  `battle.name/can_escape/can_lose`.
- **Seiten & Bedingungen:** Editor schreibt `pages[]` mit
  `trigger/conditions/commands` (`EVENT_TRIGGERS` = dieselben 4 Werte wie
  Laufzeit) und **migriert Legacy-Events** (top-level `trigger/commands`)
  beim ersten Speichern automatisch in Seiten (`event_form.h:326–343`);
  `Event.build_pages` liest beide Formate; `conditions_met?` liest exakt
  die Editor-Gruppen `switch`/`variable`/`self_switch` (Buchstabe A–D,
  Operatorliste identisch zu `EVENT_OPS`).
- **NPCs:** Roundtrip `name/x/z/y/radius/trigger/commands/color/path/
  speed` ✓; Editor-Marker + Prüfung „Wegpunkt in Block-Zone" ✓;
  Laufzeit baut daraus ein normales Action-Event mit Live-Position ✓.
- **Terrain-Bau:** Editor schreibt `width`/`height`, `build_terrain`
  akzeptiert `size`-Fallback, MapCheck prüft beide Formen ✓.
- **Prefabs:** bewusst global (`game/data/prefabs.json`, nicht pro
  Projekt) — Fehlen in `projekte/dorf` ist korrekt ✓.
- **Skripte:** pro Projekt `data/scripts.json`, `autoload` opt-in,
  Ausführung über `TOPLEVEL_BINDING`; fehlende Datei kein Fehler ✓.
- **Die Brücke ist getestet:** `event_bridge_check.rb` lässt die
  C++-Formularlogik ein Event ändern und die Ruby-Laufzeit genau dieses
  JSON interpretieren; `event18f_test` deckt choices/Zweige ab;
  `runtime_boot_test` bootet Launcher+Karte+RGSS in einer Temp-Kopie;
  `mapcheck_test` scannt **beide** Datenbäume.
- **Demo-Beispiel** (`projekte/dorf`, Wächter Mara): verschachteltes
  `if_switch` → `else` → `text`/`switch`/`variable add`/`effect` — alle
  vorkommenden Befehle existieren im Interpreter, `add` ohne `value`
  wird korrekt als Addition gelesen ✓.

**Befunde ⚠️**

- **Y1 — Demo-Daten-Drift `game/data` ↔ `projekte/dorf`:**

  | | `game/data` (Vorlage) | `projekte/dorf` (gespieltes Demo) |
  |---|---|---|
  | Events | alle Legacy-Format | 2 migriert auf `pages`, 4 Legacy |
  | Objekte | 39 | 49 |
  | fog/panorama/weather/note/heights | **fehlen** | vorhanden |
  | `encounters` (Zufallskämpfe) | **vorhanden** (`troops: waldweg, banditen`) | **fehlt → keine Zufallskämpfe** |
  | `database.json` troops/enemies | **vorhanden** (3 Truppen, 4 Gegner) | **fehlt → `battle()` meldet „Truppe unbekannt"** |

  Folge: **Frisch-Installationen** (`ensure_demo` kopiert `game/data`)
  haben Kämpfe; **bestehende Arbeitskopien behalten den alten Stand** —
  `ensure_demo` ist idempotent und aktualisiert nie. Historisch schrieben
  Werkzeuge wie `add_database17g`/`add_scripts17e` beide Bäume, die
  Encounter-/Truppen-Ergänzung (19i/19l) wanderte dagegen nur in die
  Vorlage. Empfehlung: kleines Ruby-Werkzeug (`tools/sync_demo.rb`),
  das `game/data` → `projekte/dorf` spiegelt (Vorlage = Quelle, wie in
  der Doku dokumentiert), plus Truppen/encounters im Demo-Projekt
  nachziehen.
  **→ erledigt in Runde 13:** `tools/sync_demo.rb` übernimmt
  database/scripts von der Vorlage (Vorlage = Quelle), ergänzt in
  system.json/Karten nur fehlende Top-Level-Schlüssel (Demo-Werte wie
  `last_played`, `heights`, `pages` bleiben) und ist inhaltlich
  idempotent; `tools/sync_demo_test.rb` prüft das mit `dry:`-Prüflauf
  auch für die mitgelieferten Bäume. Truppen+encounters sind im Demo
  nachgezogen. **Bewusst verbleibende Unterschiede** (Demo-Veredelungen,
  kein Sync-Ziel): 39 vs. 49 Objekte, fog/panorama/weather/heights,
  migrierte `pages`-Events.
- **Y2 — Tote Demo-Skripte:** `dorf_begruessung` behauptet im Kommentar,
  ein Event rufe ihn per `{"cmd":"script","name":…}` auf — **kein Event
  in beiden Bäumen enthält ein `script`-Kommando**; `kristalle_zaehlen`
  ebenso unbenutzt. Entweder das Demo-Event ergänzen oder die Kommentare
  bereinigen (Verweis `update_docs17e` hält den Zustand otherwise fest).
  **→ erledigt in Runde 13:** beide Events rufen ihre Scripts als ersten
  Befehl (`tools/add_demo_script_events.rb`, beide Datenbäume; Wächter
  Mara → `dorf_begruessung`, Kristall (Mitte) → `kristalle_zaehlen`);
  `event_test` spielt das Script-Fenster vor Maras Dialog mit.

---

## AA. Neues-Projekt-Flow: Projektname → leere Karte (Runde 8)

**Der Flow existiert in beiden Oberflächen ✅** (genau „Projektname +
leere Map wie in anderen Engines"):

- **Launcher:** `N` → Namenseingabe (a–z, 0–9, Leerzeichen, `-_.`,
  max. 24) → `Project.create` → Slug-Ordner + `system.json` +
  **leere flache Startkarte `welt01`** (240×240, `amplitude` 0, Start in
  der Mitte, fog/panorama/note) → Spiel startet sofort. Gleichnamige
  Projekte bekommen `_2` (`unique_slug`).
- **Editor:** „Neues Projekt" (`create_project`, `editor_main.cpp:2355`)
  → Ordnergerüst (exklusiv angelegt, überschreibt nie) + `system.json`
  + leere flache Startkarte (240×240, fog/panorama/weather/note) +
  leere `scripts.json`/`database.json` + `saves/` → Projektwechsel.
- Beide Pfade legen wie andere Engines **einen Projektnamen und eine
  leere Karte** an; `project_test.rb` deckt den Ruby-Flow ab (Slug,
  `welt01`, leer, flach, Startpunkt), MapCheck akzeptiert beide Formen.

**Lücken gegen „wie andere Engines" ⚠️**

- **Y3 — Neues Projekt startet ohne Helden:** `Project.create` schreibt
  **keine** `database.json`; der Editor schreibt eine mit **leeren**
  `actors`/`items`-Arrays — obwohl der Kommentar dort
  `// … database.json mit Start-Held/Item.` verspricht. Ergebnis:
  `Party.setup` → „Party: 0 Helden"; das Party-Panel ist leer und
  `battle()` wird mit „keine Helden in der Party - Kampf abgesagt"
  abgewiesen (robust, aber unbrauchbar bis zur F9-Datenbank).
  **XP kopiert beim Neuprojekt die Default-Akteure** (`$data_actors`).
  → Fix: Starter-Held + Start-Item in *beide* Anlage-Pfade schreiben
  (Mini-Database, ein Actor reicht), Kommentar und Code angleichen.
  **→ erledigt (Runde 12):** `Project.starter_database` (held/Abenteurer
  + Heiltrank `start: 1`, leere `enemies`/`troops`) wird von Ruby-`create`
  und C++-`create_project` identisch geschrieben; `project_test` prüft
  beide Felder.
- **Y4 — Zwei unabhängige Anlagen mit verschiedenen Konventionen:**

  | | Ruby `Project.create` (Referenz/Tests) | Editor `create_project` (C++) |
  |---|---|---|
  | Namensregeln | Slug: a–z, äöüß→aous, Leerzeichen/`-_.`→`_`, max 32, Uniquifier `_2` | ~~nur `[A-Za-z0-9_]`, Ablehnung statt Uniquifier~~ → **Runde 12:** `project_slug` spiegelt Ruby inkl. `_2` |
  | Anzeigename | getippter Titel | = Ordnername (jetzt `system.json.name` = getippter Titel) |
  | Startkarten-ID | **`welt01`** (unabhängig, XP-Style wie `MAP001`) | ~~`<projektname>`~~ → **Runde 12:** `welt01` + `start.map=welt01` |
  | `database.json`/`scripts.json` | ~~fehlen komplett~~ → **Runde 12:** Mini-Database (Y3) | **Runde 12:** Mini-Database mit Helden/Item (Y3) |
  | `saves/` | erst beim ersten Speichern | sofort |

  **→ erledigt (Runde 12):** Konsolidierung erledigt (Regeln, Karten-ID
  `welt01`, Mini-Database in beiden Pfaden); `welt01` ist die
  XP-nähere Konvention. Offen bleibt nur die Redundanz zweier
  Implementierungen (siehe BACKLOG).
- **Priorität (vom Nutzer gesetzt):** Der Demo-Drift (Y1) ist Beiwerk —
  die Vorlage dient nur als Seed; der zentrale Flow ist
  **Name → leere Karte → bauen**. Y3/Y4 haben Vorrang vor Y1.

---

## AB. Spiel-Boot: Scene_Title statt Launcher (Nutzer-Vorgabe, Runde 9)

> **Recherche-Vorlage:** `docs/XP_RECHERCHE.md` (was XP alles kann,
> Architektur, wie Skripte/Overrides genau laufen) — vor der Umsetzung
> lesen; Abschnitt4 („Konsequenzen für Rune3D") ist die Checkliste.

**Vorgabe (wörtlich vom Nutzer):**
1. Die Game-Engine soll **keinen Launcher** haben, sondern eine
   **Scene_Title** — „mit oder ohne oder custom später umkau"bar wie bei
   allen anderen Engines, nämlich **über den Script-Editor (F11)**.
2. **Neue Projekte erstellt man über den Editor**, nicht im Game Engine.

**Ist-Zustand (davon abweichend):**
- Spielstart → `@mode = :launcher` (`main.rb:313`) → `Launcher` =
  Projektliste (Enter/L/N/D/Esc), Ersteinrichtung, Namenseingabe,
  Projekt löschen (`launcher.rb`).
- `docs/XP_VERGLEICH.md:165` führt „Scene_Title | Launcher/Titel | ✅" —
  **hiermit korrigiert:** der Launcher ist *kein* XP-Äquivalent (keine
  New-Game/Continue/Shutdown-Struktur, keine Slot-Auswahl, keine
  Script-Override-Kette).
- `to_title!` und Menü „Zum Titelbildschirm" → `Launcher.activate` (K).
- Szenen-Infrastruktur existiert halb: `$scene` + `Graphics.scene_update`
  (`graphics.rb:370`) ruft `$scene.main` **pro Frame** auf — XP-Szenen
  mit blockierendem `loop` würden hier einfrieren (tick kehrt nie zurück
  → kein Present). Braucht eine dokumentierte Frame-Konvention +
  `Scene`-Basis.

**Ziel-Zustand (Migrationsplan):**
1. **Projekt-Auflösung ohne UI:** Beim Start intern und unsichtbar:
   F4-Playtest-Vorgabe → sonst zuletzt gespieltes Projekt
   (`system.json.last_played`, `Project.touch` schreibt es schon) →
   sonst erstes Projekt. Keine Projektliste, keine `:name`/`:del`-Zustände
   im Spiel.
2. **`Scene_Title` (Engine-Standard):** New Game / Continue / Shutdown;
   **Continue → `Scene_File`** mit Slot-Wähler + Vorschau (schließt die
   K-Lücke „Slot-Wähler am Titel fehlt" mit); Titelbild/Titel-BGM
   kommen aus `system.json` (BGM mit der Ton-Etappe N). Shutdown =
   `Rune::App.quit`.
3. **Override per Script-Editor:** Projekt-Scripts (F11, Reihenfolge
   wie `Scripts.run_startup`) dürfen `Scene_Title` neu definieren —
   XP-Muster: Default-Skript läuft zuerst, Nutzerskripte danach.
   Laufzeit: `$scene = Scene_Title.new` nach dem Script-Laden;
   Frame-Konvention (`main` = ein Frame, kein blockierendes `loop`)
   dokumentieren und eine kleine `Scene`-Basis mit `update`/`draw`
   anbieten, damit handelsübliche XP-Skripte umschaltbar sind.
4. **Neues Projekt nur im Editor:** `create_project` (C++) ist der
   einzige Anlagepfad — dabei Y4 vereinheitlichen (Karten-ID `welt01`,
   Namensregeln, Mini-Database mit Starter-Held aus Y3). Löschen/
   Umbenennen ebenfalls Editor-Sache; die Launcher-Zustände
   `:first`/`:name`/`:del` entfallen aus dem Spiel.
5. **Menü-Eintrag „Zum Titelbildschirm"** → `$scene = Scene_Title.new`
   statt `Launcher.activate`.
6. **Offene Kleinentscheidung:** F4-Playtest startet weiter direkt auf
   der Karte oder künftig wie XP über die Titelszene?

**Umsetzung (Runde 12) — erledigt:**
- `launcher.rb` gelöscht; `scene_base.rb` (`Scene_Base`: `main` = **ein
  Frame pro Tick**, kein blockierendes `loop`), `scene_title.rb`
  (`Scene_Title` mit XP-Methoden `command_new_game/command_continue/
  command_shutdown`) und `scene_file.rb` (`Scene_File`, 3 Slots mit
  Vorschau Karte+Zeitstempel, Esc zurück) neu in `boot.rb`.
- Interne Projekt-Auflösung: `Project.preferred` (last_played → erstes
  alphabetisch) + `Game.resolve_internal_project!`; ohne Projekt zeigt
  der Titel den Editor-Hinweis (Anlage nur im Editor, `:first`/`:name`/
  `:del` entfallen).
- **Entscheidung Punkt 6:** F4-Playtest startet **weiter direkt auf der
  Karte** (kein Umweg über den Titel) — dokumentierte, bewusste Abweichung
  von XP; rund zehn Playtest-Tests hängen am Direktstart.
- Override-Kette: `enter_title!` lädt `Scripts.load_project` +
  `run_startup` **vor** `$scene ||= Scene_Title.new` — nur
  `autoload`-Skripte führen aus (die Invarianten „Event-Skripte laufen
  nie automatisch" bleiben gültig); danach darf ein Skript `Scene_Title`
  neu definieren oder `$scene` selbst setzen.
- Menü „Zum Titelbildschirm" und `$game_temp.to_title` → `to_title!` →
  `enter_title!`; `rahmen`/`abdunkeln` wanderten nach `UI` (auch Kampf).
- Titel-BGM: `system.json`-Schlüssel `title_bgm` (optional, sonst
  `musik`); Titelbild ist die 3D-Kulisse (erste Karte des Projekts).
- **Bewusst XP-treu:** Esc auf dem Titel beendet *nicht* (Beenden nur
  über „Spiel beenden" = `Rune::App.quit`).
- Y3/Y4 mitgezogen: beide Anlagepfade schreiben die Mini-Datenbank;
  C++ `create_project` slugt wie Ruby und legt die Startkarte `welt01`
  an (Details unter AA).

**Auswirkungen auf bisherige Befunde:** K beschreibt nur noch den
Ist-Zustand; Y3 (Starter-Helden) bleibt für „New Game" relevant; Y4
verschiebt sich vollständig in den Editor; „Projektliste/Neu/Löschen im
Spiel" entfällt (Doku: `XP_VERGLEICH`-Zeile165 korrigieren,
`HANDOFF`-Abschnitt ergänzen).

---

## AC. Ablauf-Befehle (Etappe 20c Welle 1, Runde 27)

Geprüft: `game/engine/interpreter.rb`, `game/engine/event.rb`,
`game/engine/message_window.rb`, `game/engine/mapcheck.rb`,
`src/editor/event_form.h`, `src/editor/editor_main.cpp` gegen
`XP-Defaultscript/` (u. a. `013 - Interpreter 1..3`) sowie
`tools/ablauf_test.rb` (30 Checks), `tools/json_test.cpp`,
`tools/befehle_test.rb`, `tools/sync_test.rb`.

| # | Thema | XP | Rune3D | Bewertung |
|---|---|---|---|---|
| AC.1 | **Schleife** | 112 Loop + 413 „Repeat Above“ als Sprungmarken-Paar in EINER Liste | `loop` mit eigenem `commands`-Zweig (`event_command_list_mutable`, Baumkopf „Schleife:“); `break` verlässt die innerste Schleife über einen `loop`-Rahmen im Interpreter-Stack | Gleichwertig, im Baum lesbarer als XP (kein Paar aus zwei Zeilen). Verschachtelung, `break` aus Wenn-Zweigen und Wiederaufsetzen nach dem Schleifenende geprüft. |
| AC.2 | **Tastaturabfrage** | 105 wartet auf A/B/C/X/Y/Z/L/R und schreibt `Input`-Konstante in die Variable, `digits` steuert die Anzeige | `button_input` wartet auf eine Liste freier Tastennamen (`e`, `space`, `escape`, `up`, `f1`, Buchstaben) und schreibt die **1-basierte Position** in der Liste | Bewusst (LOGIK-Muster D6): Positionen sind in Bedingungen lesbar (`== 2` = zweite Taste), XP-Codes sind für Autoren kryptisch. `digits` entfällt, weil kein Ziffernfenster nötig ist. MapCheck kennt das Tastenvokabular (`TASTEN`) und warnt bei Tippfehlern. |
| AC.3 | **Sprünge** | 118/119: Marke und Sprung müssen im selben Block (gleiche Einrückung) stehen | `jump` sucht die Marke nur im **aktuellen** Befehls-Array (Top-Level bzw. derselbe Wenn-/Schleifenzweig); fehlt sie, wird gewarnt, `Issues.report` schreibt ins Backlog und die Ausführung läuft weiter | XP-treu. Der Unterschied zu XP ist nur, dass ein Fehlziel das Event nicht aufhängt. MapCheck warnt „keine Marke 'x' in diesem Block“. |
| AC.4 | **Schleife ohne Ausgang** | 112 ohne 113 endet nie (bekannte XP-Falle) | Laufzeit verhält sich genauso (Stack bleibt gefüllt, Event blockiert); MapCheck warnt statisch, wenn weder `break`, `jump` noch `exit_event` im Körper (oder in dessen Zweigen) steht; eine leere Schleife wird beendet statt zu drehen | Bewusst: XP-Autoren rechnen mit Endlosschleifen bis zum Seitenwechsel; die Warnung ersetzt keine Laufzeitbremse. |
| AC.5 | **`erase_event`** | 116 löscht das Event bis zum Kartenwechsel (Karte lädt Events neu) | `Event#erase!` setzt `@erased`; `active?`/`commands`/`manual_fire`/Picking/Auslöser-Schleife überspringen das Event, die Schild-Nodes werden einmalig ausgeblendet; `enter_map` baut die Events neu | XP-treu. Nur das **auslösende** Event ist löschbar (kein Ziel-Feld), ohne Besitzer wird gewarnt statt zu stürzen. |
| AC.6 | **Gemeinsame Events (117)** | `$data_common_events` mit Integer-Indizes; Aufruf über `$game_temp.common_event_id`, Ausführung nach dem aktuellen Event; Trigger (none/autorun/parallel) + Schalterbedingung, `Game_CommonEvent` je Eintrag | `database.json` → `common_events` (Name, `trigger`, `switch`, `commands`); Befehl `common_event` legt die Liste als Rahmen auf den Stack; `CommonEvents.tick` startet autorun/parallel bei freiem Interpreter; `$data_common_events` als Hash Name → Eintrag | Bewusst: Namen statt Indizes (wie `$data_skills`), **ein** Interpreter („parallel“ läuft nacheinander, wie D3), Aufruftiefe 8 als Schutz gegen Selbstaufrufe. XP führt den Aufruf nach dem aktuellen Event aus, Rune3D inline (Unterschied nur bei `exit_event` im aufgerufenen Event). |
| AC.9 | **Editor für Gemeinsame Events** | eigener DB-Reiter (F9, „Common Events“) mit Befehlslister | nur der Aufruf-Befehl im F6-Dialog (Namensfeld + Hinweis); die Einträge selbst werden derzeit in `database.json` gepflegt | Offen (Backlog): Reiter im F9-Dialog nachrüsten, sobald die Editor-Oberfläche wieder gebaut/getestet werden kann. Datenmodell und Prüfungen stehen bereits. |
| AC.7 | **Fenster-Optionen (104)** | `$game_system.message_position/frame/transparent` | Modul `MessageWindow` (`game/engine/message_window.rb`) mit `set`/`reset!`; `UI.message` zeichnet oben/mitte/unten, ohne Rahmen oder mit 35 % Deckkraft | Bewusst außerhalb des Interpreters: Die Einstellung überlebt Kartenwechsel wie in XP, wird aber bei Neustart/F5 (`MessageWindow.reset!`) auf die XP-Defaults gesetzt. Unbekannte Positionen werden ignoriert (Warnung) statt das Fenster zu verschieben. |
| AC.8 | **`break`/`exit_event`/`erase_event` ohne Feld** | 113/115/116 haben keine Parameter | Formular zeigt nur einen Hinweistext; „Übernehmen“ ändert nichts (transaktional geprüft in `json_test`: „Befehl ohne Felder bleibt unverändert“) | Bewusst: Ein Feld ohne Bedeutung würde nur falsche Erwartungen wecken. |

**Offene Kleinigkeit (Backlog):** `erase_event` blendet nur die Schild-Nodes
(Event-Darstellung) aus; ein Event ohne Visual hat ohnehin nichts zu verstecken.
Ein eigener DB-Reiter für Common Events (F9/F6-Erweiterung) ist Teil des
nächsten Schritts.

---

## AD. Erweiterte Bedingungen & Sperren (Etappe 20d Teil 1, Runde 28)

| # | Thema | XP | Rune3D | Bewertung |
|---|---|---|---|---|
| AD.1 | **Bedingungsquellen** | 111 kennt 13 Seiten (Schalter, Variable, Self-Switch, Timer, Actor, Enemy, Character, Gold, Item, Weapon, Armor, Button, Script) | Schalter, Variable, Self-Switch (bisher) + **Gold, Item, Script** (neu) = 8 (R28) | Sinnvolle Reihenfolge: Die neuen Quellen braucht fast jedes Projekt. Offen bleiben Timer (braucht Befehl 124), Actor/Party, Gegner, Charakter, Waffen/Rüstungen (keine Daten) und Button (müsste die Tasteneingabe als Bedingung abbilden). |
| AD.2 | **Wenn Script** | Ausdruck im Interpreter-Kontext, `false`/`nil` = falsch | `Scripts.eval_code` (derselbe `Scripts::Context` wie der Script-Befehl); Fehler → Log + Issue, Zweig „Sonst" | XP-gleich. Wichtig: kein stiller Absturz, damit ein Tippfehler im Ausdruck das Event nicht aufhängt. |
| AD.3 | **134/135/136** | eigene Befehle, Flags in `$game_system` | `save_access`/`menu_access`/`encounter_access` mit `allowed` (Default: erlaubt), Flags in `$game_system`, im Spielstand | XP-gleich; die Sperren wirken sofort, weil Menü/Speichern/Zufallskämpfe dieselben Flags lesen. |
| AD.17 | **Bildschirm-Bilder (231–235)** | `Game_Screen#show_picture/move_picture/rotate_picture/tint_picture/erase_picture`; `Sprite_Picture` zeichnet in `Spriteset_Map`, also über der Karte und unter Fenstern | `Pictures` hält 1–50 Bilder (Nummer, Datei, Position, Zoom, Deckkraft, Blend, Ursprung, Winkel, Tönung); `ImageLoader` dekodiert PNG/BMP in Ruby und meldet die Pixel über `Rune::Gfx.image`; `ui.cpp` zeichnet sie im 2D-Durchgang zwischen Welt und Fenstern | Reihenfolge, Nummern, Dauer in Frames und Ursprung wie XP. Abweichungen: kein JPEG (eigener Codec nötig), Tint multipliziert statt Kanal-Mapping (positive Kanäle dämpfen die Gegenkanäle), Blend 1/2 zeichnen wie 0. Tests: `tools/image_loader_test.rb`, `tools/pictures_test.rb`. |
| AD.16 | **Bildschirmwackeln (225)** | `Game_Screen#start_shake(power, speed, duration)`; `Scene_Map` verschiebt das Bild um zufällige Sprünge, die über die Dauer abklingen | `ScreenFx.shake` rechnet Amplitude aus Stärke/Tempo, baut sie linear über `frames` ab und setzt `Rune::Camera.shake = [x, y]`; `camera.cpp` addiert den Versatz beim Bildaufbau auf Auge und Ziel | Gleiche Parameter und Dauer, weicherer Verlauf (Sinus statt Zufallssprünge). Der Versatz wirkt nur im Bild — Picking, Kamera-Kollision und Spielerposition bleiben unverändert. Test: `tools/screen_fx_test.rb`. |
| AD.15 | **Ergebnis-Codes 601–603** | `Game_Interpreter#command_601/602/603` hängen an das `battle`-Kommando: XP verzweigt direkt nach dem Kampf auf Sieg/Flucht/Niederlage | `Battle#ende` merkt `@result` (`:sieg`/`:flucht`/`:niederlage`); der Befehl `if_battle_result` vergleicht `result` und wählt den Zweig | Ein Befehl statt drei Codes, funktioniert aber auch nach `battle` in einem Common Event. Ohne Kampf gilt der Sonst-Zweig (XP zeigt dann ebenfalls nichts). Test: `tools/bedingungen_test.rb`. |
| AD.14 | **Menü öffnen (351)** | `Game_Interpreter#command_351` → `$game_temp.menu_calling = true`; `Scene_Map#update` liest das Flag und ruft `call_menu` | Interpreter setzt `$game_temp.menu_calling`; `TempActions.tick` im Map-Frame ruft den Menü-Handler und löscht das Flag | Gleicher Weg wie ein Script, genau ein Menü pro Befehl, funktioniert auch aus Common Events. Esc bleibt unverändert der Menü-Knopf. Test: `tools/temp_flags_test.rb`. |
| AD.13 | **Bildschirmton/Blitz (223/224)** | `Game_Screen#start_tone_change` (4 Kanäle, Dauer in Frames) und `#start_flash` (Farbe, Stärke, Dauer); `Scene_Map` zeichnet beides über der Karte | `ScreenFx` hält aktuellen Ton, laufenden Übergang und Blitz; `update(dt)` rechnet in XP-Frames (40/s), `draw` legt Vollbild-Overlays über Welt und UI | Verlauf und Dauer sind XP-gleich (`frames: 0` = sofort, Blitz klingt über die Frames ab). Bewusst genähert: Rune3D hat keinen Post-Processing-Pass, deshalb legen positive Kanäle einen Farb-Schleier auf und negative einen dunklen Schleier im Gegenanteil; `gray` legt einen grauen Schleier auf statt zu entsättigen. Kein Kanalmapping wie bei XP, dafür ohne Shader sichtbar. Test: `tools/screen_fx_test.rb`. |
| AD.12 | **Namenseingabe (303)** | `Game_Temp#name_actor`/`Window_NameInput`/`Scene_Name`: der Spieler tippt den Namen, Enter übernimmt, Esc bricht ab (wenn erlaubt), `$game_actors[id].name` wird geschrieben | `NameInput` (Modul) hält Held, Text und Grenze; Tippen über Ein-Zeichen-Tasten, `max` 1..16 (Default 8 aus XP), `Party.rename` schreibt in den Actor; `UI.name_input` zeichnet und bildet Tasten ab, der Interpreter blockiert über `@name_wait` | XP-gleich bei Wirkung, Grenze und Abbruch. Bewusst anders: nur `enter` bestätigt — „e" ist im Namensfeld ein Buchstabe (im Menü weiterhin Bestätigen), und Umlaute fehlen, weil die Engine Tasten nur über Ein-Zeichen-ASCII-Namen liest. Umbenannte Helden stehen jetzt im Spielstand (`party.actors[].name`); Nicht-Mitglieder behalten ihren Namen nur bis zum Neustart. Test: `tools/name_input_test.rb`. |
| AD.11 | **Laden (302)** | `Game_Temp#shop_goods` + `Scene_Shop`/`Window_ShopBuy/Sell/Command`: Warenliste mit Preis je Ware, Kauf nur bei genug Gold, Verkauf zum halben Datenbankpreis | `Shop` (Modul) hält Warenliste, Reiter (Kaufen/Verkaufen), Cursor und Meldung; `Party` liefert Gold/Inventar. `UI.shop` zeichnet nur und bildet Tasten auf `Shop`-Methoden ab; der Interpreter blockiert über `@shop_wait`, bis `Shop.active?` endet | XP-gleich bei Preisregeln (Ladenpreis sticht Datenbankpreis, Verkauf = halber DB-Preis) und bei der Bedienung (Esc erst zu den Waren, dann aus dem Laden). Bewusst anders: unbegrenzter Bestand (kein Lagerfeld), und der Verkaufspreis wird durch den Ladenpreis gedeckelt — sonst wäre „teuer kaufen, halb verkaufen" ein Gold-Generator. Fehlendes Gold/Item bleibt im Fenster als Meldung, kein Abbruch des Events. Test: `tools/shop_test.rb`. |
| AD.10 | **Mitglieder ändern (129)** | `Game_Party#add_actor/remove_actor`; `Game_Party#actors` ist die Mitgliederliste, das Menü zeigt sie, der Kampf baut seine Battler daraus | `Party` trennt `@katalog` (alle Datenbank-Helden) und `@actors` (Mitglieder); `member_add` → `:ok`/`:mitglied`/`:unbekannt`, `member_remove` → `:ok`/`:kein_mitglied`/`:letzter`. `snapshot['actors']` ist die Mitgliederliste, `restore!` baut `@actors` daraus in gespeicherter Reihenfolge neu auf (alte Saves ohne `actors` behalten das Team) | XP-gleich bei Wirkung und Reihenfolge (Beitritt hängt hinten an). Bewusst anders: das letzte Mitglied bleibt im Team, weil Kampf, Menü und `if_actor` sonst ohne Helden dastehen; Interpreter und MapCheck melden den Versuch. `if_actor` prüft seitdem die Mitgliederliste (AD.9). Test: `tools/party_member_test.rb`. |
| AD.9 | **Actor-/Button-Bedingung (111)** | `if_actor` fragt `$game_party.actors`, `if_button` fragt `Input.trigger?` | `if_actor` über `Party.find`, `if_button` über `Rune::Input.pressed?` (Fallback `down?`) | XP-gleich im Verhalten; bewusst: Rune3D führt alle Datenbank-Helden im Team, `if_actor` prüft darum nur die Existenz (kein „beitreten/entlassen", s. `party_member`). Seit AD.10 ist die geprüfte Liste die Mitgliederliste. Tasten-als-Bedingung gilt nur im Druckframe - Events, die eine Taste „abwarten" wollen, nutzen weiter die Tasteneingabe (XP 105). |
| AD.8 | **Variable-Operationen und -Quellen (122)** | 6 Operationen (`=`, `+`, `-`, `×`, `÷`, Rest), Operanden aus 8 Quellen | `variable` mit `op` + `value`/`source`/`src`: const, Variable, Zufall, Item, self; Altform `value`/`add` bleibt gültig und wird im Editor migriert | XP-gleich für Konstanten und die fünf Quellen, ÷0 lässt den Wert stehen (XP: ZeroDivisionError). Bewusst offen: Quellen Actor/Enemy/Character (keine Gegner-/Charakterdaten; Actor-Attribute bräuchten ein Feldauswahl-Combo). `src` bei `const`/`random`/`self` wird ignoriert und von MapCheck angemahnt. |
| AD.7 | **HP/SP/EXP/Stufe (311/312/315/316)** | eigene Befehle, Ziel wählbar, Operation `+`/`-`, Wert konstant oder Variable | `actor_hp`/`actor_sp`/`actor_exp`/`actor_level`: `target` (`alle`/Actor), `amount` vorzeichenbehaftet; EXP/Stufe über denselben Weg wie der Kampf | XP-Gleichheit bei Wirkung und Klemmen. Bewusst anders: eine Zahl statt Operation + Operand (weniger Formularfelder, gleiche Ausdruckskraft für Konstanten). Variablen als Operand fehlen noch (Roadmap: Variable-Quellen, Punkt 122). Stufen-Abstieg baut keine Werte zurück — Rune3D hat keine Klassentabellen. |
| AD.6 | **`$game_temp`-Calling-Flags** | `Scene_Map#update` fragt `battle_calling`/`menu_calling`/`save_calling`/`player_transferring`/`to_title` ab und wechselt die Szene | `TempActions.tick(handler)` im Engine-Ordner, von `Game.update` mit Lambdas verdrahtet; Kopf-Test `tools/temp_flags_test.rb` (14 Checks) | XP-Reihenfolge und „ein Auftrag pro Frame" übernommen; Flags werden vor der Aktion gelöscht, damit ein abgelehnter Kampf nicht jedes Frame erneut anfragt. Startposition/-richtung beim Transfer bleibt offen (AC.10). |
| AD.5 | **Timer** | 124 startet/stoppt, `Window_Timer` zeigt die Restzeit, Timer steht im Spielstand | `timer`-Befehl (start/stop, Sekunden), `GameSystem#update(dt)` zählt in XP-Frames (40/s) herunter, `UI.timer` zeichnet `mm:ss` oben rechts | XP-gleich; `start` mit 0 s wirkt wie `stop`, damit ein leerer Wert kein Dauerfeuer auslöst. Die Anzeige hängt nicht am Befehl, sondern an `timer_working` — wie bei XP kann auch ein Script den Timer starten. |
| AD.4 | **Item-Menge** | 126/111 zählen Inventar-Mengen | `Party.item_count(name)`; negative `value` wird von MapCheck als Warnung gemeldet, aber verglichen (XP erlaubt sie ebenfalls) | Bewusst tolerant: Die Datenprüfung warnt, die Laufzeit rechnet. |

---

## Z. Empfohlene Reihenfolge (aktualisiert)

1. **Etappe „XP-Logik-Fix" (klein, sicher):** ~~F1 (Routen-Codes 1–45) +
   F2 (keine erfüllte Seite → inaktiv) + **F3 (RGSS-API: `$game_party.
   actors/steps/item_number`, `$game_map.events` als Hash-Ansicht,
   optional `Input.dir8`)** + D9 (Klick nur `action`) + **F4 (Speichern:
   fehlende Karte = Fehler statt stiller Ersetzung)** + **F5 (Physik/
   Sprung gesperrt bei offenem Text/Wahl/Zahlen-Fenster und Menü —
   `sim_paused`-Gate, XP `movable?`)** + **F6 (RGSS-Koordinaten:
   `$game_player.y` = Kartenebene statt Höhe, `Event#y`, fehlende
   Player-Methoden straighten/turn_*/through/… als No-Ops)** + Tests
   (XP-Referenz-Assertion, Inaktiv-Fall, RGSS-Alias-Checks) + Doku.~~
   **→ umgesetzt in Runde 11** (Caveat: Routen-Codes heißen jetzt
   0–45 inkl. Ende-Marker `0`; Bestandsdaten betroffen: keine).
2. ~~**Scene_Title-Etappe (AB — Nutzer-Vorgabe):** Launcher aus dem Spiel
   nehmen, interne Projekt-Auflösung (F4 → `last_played` → erstes),
   Engine-`Scene_Title` (New Game/Continue/Shutdown) + `Scene_File`-
   Slot-Wähler, `$scene`-Override-Pfad über Script-Editor
   (Frame-Konvention + `Scene`-Basis), Menü „Zum Titel" →
   `$scene = Scene_Title.new`. Gleichzeitig **Y3/Y4** in
   `create_project` ziehen (Starter-Held, Karten-ID `welt01`,
   Namensregeln angleichen); `XP_VERGLEICH`-Zeile
   „Scene_Title | Launcher | ✅" korrigieren.~~
   **→ umgesetzt in Runde 12** (F4 bleibt bewusst Direktstart;
   `title_bgm` optional aus `system.json`, 2D-Titelgrafik + Editor-
   Rundlauf der Zusatzschlüssel im BACKLOG).
3. ~~**Datenpflege Demo (Y):** `tools/sync_demo.rb` (Vorlage `game/data`
   → `projekte/dorf`), Truppen+encounters nachziehen, tote
   Skript-Kommentare bereinigen oder Demo-Event mit `script`-Befehl
   ergänzen.~~ **→ umgesetzt in Runde 13** (sync_demo additiv mit
   `dry:`-Prüflauf + `sync_demo_test`, Troops/encounters im Demo
   nachgezogen, `add_demo_script_events` in beiden Bäumen).
4. ~~**Kleine Sichtbarkeits-Etappe:** Game-Over-Bildschirm (K),
   Slot-Vorschau mit Spielzeit/Karte (J), `$game_system`-Zustand
   (Timer/Sperr-Flags) ins Savegame übernehmen, Fortsetzungs-Zeiger ▾
   im Textfenster (M), **Menü-Items beim Öffnen cachen** (V,
   12 Save-Parings/Frame), Menü-Playtime/Steps ins Party-Panel (V).
   Optional als Entscheidung: Encounter-Kadenz auf 1 m/Schritt
   angleichen (W).~~ **→ umgesetzt in Runde 14** (echte
   `Scene_Gameover` statt Sofort-Titel, Karte+Spielzeit in Menü und
   `Scene_File`, ▾-Blinkzeiger, `@menu_items`-Cache, Playtime/Steps im
   Party-Panel, `'system'`-Snapshot ins Savegame inkl. `save_count`).
   **Encounter-Kadenz (W) umgesetzt in Runde 26**: eine Kachel = 1 m
   (`Game::KACHEL_METER`), Schritt-Sound unverändert.
5. ~~**Dialoge/XP-Parity klein:** Wahl-Abbruch (cancel/Else-Zweig) in
   Engine+Editor, optional Text+Wahl im selben Fenster (M).~~
   **→ umgesetzt in Runde 16** (`cancel`/`cancel_branch` in Engine+
   Editor mit „Wenn Abbruch:"-Kopfzeile, MapCheck+Issue-Scan,
   Esc-Routing Menü↔Eingabe, Tests event18f/mapcheck/runtime_boot/
   json; optional: Kombi-Fenster Text+Wahl (M) bleibt bewusst offen).
6. ~~**Designentscheidung mit dir:** D1 (`auto` = XP-Autorun) und D2
   (`touch` = Wiedereintritt) — Demo hat keine betroffenen Events.~~
   **→ entschieden und umgesetzt in Runde 17** (beide XP-treu: D1
   Autorun-Neustart bis Seitenwechsel, D2 Wiederbetreten mit Rearm
   beim Verlassen der Nähe; Tests D1/D2 in `encounter_test`, Auto-
   Save/Load-Check angepasst; Demo hat weiterhin keine betroffenen
   Events — nur der Wegweiser ist `touch`).
7. ~~**Roadmap `docs/XP_VERGLEICH.md`:** 20a Kampf (zieht D4/Level-Up),
   dann 20d (Variable-Operationen/Bedingungen) — dort auch die
   `$game_temp`-Calling-Flags sinnvoll anbinden; **neu aufgenommen:
   „20x Ton"** (Datei-Lader in `sound.cpp`, Karten-BGM, Event-Befehle
   241/242/245/246, Pitch — N), **Panorama-Scroll + Wetter-`duration`**
   (O), **Mover `through` + Routen-`jump`** (Q, echte Durchlässigkeit
   bzw. Verlagerung statt Partikel) und **Performance-Grenzwerte**
   (S: dirty-Flag für Seiten-Refresh ab ~hunderten Events, Draw-Batching
   messen).~~ **→ umgesetzt in Runde 18** (XP_VERGLEICH §8: 20a mit
   D4/Level-Up, 20d mit `$game_temp`-Calling-Flags, neue Etappen
   **20i Ton** (Audit „20x"), **20j Panorama-Scroll + Wetter-Dauer**,
   **20k `through`/Routen-`jump`**, **20l Grenzwerte/Messpunkte**;
   Abhängigkeiten + Reihenfolge-Empfehlung ergänzt).
8. **Editor:** XP-Dialogdesign (mit deinen Screenshots), Kartenbaum-Hierarchie
   wie XP, **Undo-Ausbau auf Events/Karteneigenschaften** (T),
   ein Quellen-Register für Befehle (`befehle.json`),
   atomares `Project.save` (Temp+Rename wie bei Karten).
   **Stand Runde 18:** Register `docs/befehle.json` + `tools/befehle_test.rb`
   (prüft Register ↔ `CMD_TYPES` ↔ `ISSUE_CMD_TYPES` ↔ `EV_TYPES` ↔
   `interpreter.rb`) **fertig**, atomares Schreiben **fertig**
   (`Project.write_json` für create/create_map/save, Editor-`write_file`
   → `editor_write_file_atomic`). **Stand Runde 19:** Undo-Ausbau (T) **fertig** (ganze Karte als
   Schnappschuss, Push-Orte in F6/F5/Eigenschaften/Pinsel/F7/Kartenbaum),
   **Kartenbaum-Hierarchie fertig** (`system.json` → `map_tree` mit
   Einrücktiefe je Karte, `Tab`/`Shift+Tab`, Kontextmenü; portable Regeln in
   `src/editor/map_tree.h`, geprüft von `tools/editor_workflow_test.cpp`).
   **Offen:** nur noch XP-Dialogdesign (braucht deine Screenshots).
