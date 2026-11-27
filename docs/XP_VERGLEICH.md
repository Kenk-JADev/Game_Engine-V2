# Systemvergleich: RPG Maker XP ↔ Rune3D

**Zweck:** Vollständige Gegenüberstellung, was RPG Maker XP kann und wo
Rune3D (Stand nach 19y) steht — mit Blick darauf, was sich sinngemäß in 3D
übersetzen lässt und was eine echte Lücke ist.

**Quellen:**
- `XP-Defaultscript/` — 90 XP-Defaultscripts, 15.217 Zeilen (Referenz im Repo)
- `game/engine/*.rb`, `game/main.rb` — Engine-Stand
- `src/editor/` — Editor-Stand (F2/F5/F6/F7/F9/F11)
- `README.md`, `docs/BACKLOG.md`

**Statussymbole:** ✅ vorhanden (sinngemäß) · ⚠️ teilweise/anders · ❌ fehlt

---

## 1. Event-Befehle — 92 XP-Befehle, 39 in Rune3D

XP hat **92 Top-Level-Ereignis-Codes** (101–355 plus 601–603). Rune3D hat
**73 Befehlstypen**. Bilanz: **57 ✅ · 5 ⚠️ · 31 ❌**.

### A. Nachricht & Ablaufsteuerung (101–119)

| XP | Code | Rune3D | Status |
|---|---|---|---|
| Text anzeigen | 101 | `text` | ✅ |
| Wahlmöglichkeiten | 102 (+403) | `choices` mit `cancel`/`cancel_branch` (R16) | ✅ |
| Zahleneingabe | 103 | `input_number` | ✅ |
| Textoptionen (Position/Rahmen/Transparenz) | 104 | `text_options` (R27) | ✅ (Schriftart/Windowskin fehlen weiter: 131) |
| Tastaturabfrage (Button Input) | 105 | `button_input` (R27) | ✅ (speichert die 1-basierte Position in der Tastenliste — LOGIK AC.2) |
| Warte | 106 | `wait` | ✅ |
| Verzweigung (13 Bedingungsarten) | 111 | `if_switch` / `if_variable` / `if_self_switch` / **`if_gold` / `if_item` / `if_script` / `if_actor` / `if_button` (R28)** | ⚠️ 8 von 13 Arten; es fehlen Timer, Enemy, Character, Weapon, Armor |
| Schleife | 112 (+413) | `loop` mit `commands`-Zweig (R27) | ✅ |
| Schleife verlassen | 113 | `break` (R27) | ✅ (auch aus Wenn-Zweigen innerhalb der Schleife) |
| Event-Ausführung beenden | 115 | `exit_event` (R27) | ✅ |
| Event löschen | 116 | `erase_event` (R27) | ✅ (nur das auslösende Event, bis zum Kartenwechsel) |
| Gemeinsames Event aufrufen | 117 | `common_event` + `common_events` in `database.json` (R27) | ✅ (Trigger none/autorun/parallel; „parallel" läuft im einen Interpreter nacheinander — LOGIK AC.6) |
| Sprungmarke | 118 | `label` (R27) | ✅ |
| Zu Marke springen | 119 | `jump` (R27) | ✅ (Marke im selben Befehlsblock — LOGIK AC.3) |

### B. Schalter, Variablen, Party (121–136)

| XP | Code | Rune3D | Status |
|---|---|---|---|
| Schalter setzen | 121 | `switch` | ✅ |
| Variablen steuern (6 Operationen, 8 Quellen) | 122 | `variable` mit `op` + `value` + `source`/`src` (R28) | ⚠️ alle sechs Operationen und die Quellen const/Variable/Zufall/Item/self (Altform `value`/`add` bleibt gültig); es fehlen die XP-Quellen Actor/Enemy/Character |
| Self-Switch | 123 | `self_switch` | ✅ |
| Timer steuern | 124 | `timer` (R28) mit `mode` start/stop + `seconds` | ✅ (Restzeit als `mm:ss` oben rechts, im Spielstand gesichert) |
| Gold ändern | 125 | `gold` | ✅ |
| Items ändern | 126 | `item` | ✅ |
| Waffen ändern | 127 | — | ❌ (keine Waffen in der DB) |
| Rüstungen ändern | 128 | — | ❌ (keine Rüstungen in der DB) |
| Party-Mitglied hinzufügen/entfernen | 129 | `party_member` (R29) mit `name` + `mode` add/remove | ⚠️ Aufnehmen/Entfernen wie XP; bewusst anders: das letzte Mitglied bleibt (XP erlaubt ein leeres Team) |
| Windowskin | 131 | — | ❌ |
| Kampf-BGM | 132 | — | ❌ |
| Kampf-Ende-ME | 133 | — | ❌ |
| Speichern erlauben/sperren | 134 | `save_access` (R28) → `$game_system.save_disabled` | ✅ |
| Menü erlauben/sperren | 135 | `menu_access` (R28) → `$game_system.menu_disabled` | ✅ |
| Zufallskämpfe erlauben/sperren | 136 | `encounter_access` (R28) → `encounter_disabled` | ✅ |

### C. Karte (201–210)

| XP | Code | Rune3D | Status |
|---|---|---|---|
| Spieler teleportieren | 201 | `teleport` | ✅ |
| Event positionieren | 202 | — | ❌ |
| Karte scrollen | 203 | — | ❌ (3D: Kamera-Entsprechung) |
| Karteneinstellungen (Parallax/Nebel/Battleback) | 204 | F2-Karteneigenschaften | ⚠️ Daten da, kein Befehl zur Laufzeit |
| Nebelfarbe/-ton | 205 | Karten-Atmosphäre | ⚠️ nur statisch in der Karten-JSON |
| Nebel-Deckkraft | 206 | — | ⚠️/❌ |
| Animation zeigen | 207 | — | ❌ |
| Transparenz schalten | 208 | — | ❌ |
| Bewegungsroute | 209 | `move_route` | ✅ |
| Warte auf Routenende | 210 | `move_route` + `wait` | ✅ |

### D. Bildschirm (221–236)

| XP | Code | Rune3D | Status |
|---|---|---|---|
| Übergang vorbereiten/ausführen | 221/222 | `fade` | ✅ |
| Bildschirmton (Tone) | 223 | `screen_tone` (R32) → `ScreenFx`-Overlay | ⚠️ Verlauf über `frames` wie XP, aber als Farb-/Dunkel-Schleier genähert (kein Shader-Kanalmapping) |
| Bildschirmblitz | 224 | `flash` (R32) → `ScreenFx`-Overlay | ✅ weiss/rot/grün/blau/gelb, Stärke 0–255, Abklingen über `frames` |
| Bildschirmwackeln | 225 | `shake` (R32) → `ScreenFx` verschiebt `Rune::Camera.shake` | ✅ Stärke/Tempo 1–9, Dauer in Frames; sinusförmig mit abnehmender Auslenkung statt XP-Zufallssprüngen |
| Bilder zeigen/bewegen/drehen/färben/löschen | 231–235 | `show_picture`/`move_picture`/`rotate_picture`/`tint_picture`/`erase_picture` (R33) | ✅ Bilder 1–50, Zoom/Deckkraft/Blend/Ursprung, Drehung in Grad/Frame, Tönung über Frames; PNG/BMP (kein JPEG), Blend 1/2 zeichnen wie 0 |
| Wettereffekte | 236 | `weather` | ✅ (Regen/Schnee, 3D-Partikel) |

### E. Audio (241–251)

| XP | Code | Rune3D | Status |
|---|---|---|---|
| BGM abspielen | 241 | `play_bgm` (R35) → `Audio.bgm_play` → `AudioFiles` (WAV/OGG/MP3) mit Rückfall auf den prozeduralen Loop | ✅ Datei + Lautstärke 0–100 + Pitch 50–150 %, neue Musik ersetzt die alte; Karten-BGM läuft seit R37 beim Kartenwechsel (`MapAudio`) |
| BGM ausblenden | 242 | `fadeout_bgm` (R35) → `AudioFiles.fade_ms` im Engine-Takt | ✅ Dauer in Sekunden, 0 = sofort, nicht blockierend |
| BGM sichern/zurückholen | 243/244 | `save_bgm`/`resume_bgm` (R41) → `$game_system.bgm_memorize`/`bgm_restore` | ✅ merkt die laufende Musik und holt sie wieder; Merker flüchtig, `resume_bgm` ohne Merker stoppt |
| BGS abspielen/ausblenden | 245/246 | `play_bgs`/`fadeout_bgs` (R35) → `Audio.bgs_play`/`AudioFiles.fade_ms` | ✅ wie BGM, mit den Ordnern `audio/bgs/` und `audio/me/` (R40) |
| BGM/BGS merken und wiederherstellen | 247/248 | `memorize_audio`/`restore_audio` (R41) → `$game_system.bgm_memorize`/`bgs_memorize` bzw. `*_restore` | ✅ beide Spuren in einem Befehl, wie XP; die Kampfmusik nutzt dieselben Methoden (R40) |
| ME abspielen | 249 | `Audio.me_play` (R40) → Datei (`audio/me/`) oder prozedural | ✅ verstummt wie RGSS die BGM, ohne sie fortzusetzen; `Audio.me_stop` stoppt das ME |
| SE abspielen | 250 | `sound` | ✅ (7 prozedurale Sounds) |
| SE stoppen | 251 | `Audio.se_stop` (R40) | ✅ stoppt alle gestarteten Einmal-Sounds (Datei und prozedural) |

### F. Kampf & Actor-Status (301–340)

| XP | Code | Rune3D | Status |
|---|---|---|---|
| Kampf starten | 301 | `battle` | ✅ (ohne Ergebniszweige) |
| Shop | 302 | `shop` (R30) mit `goods` (item/price) + Laden-Fenster (`UI.shop`) | ✅ Kaufen/Verkaufen, halber Preis beim Verkauf, Gold/Inventar wie XP; bewusst: unbegrenzter Bestand, Esc geht erst zurück zu den Waren |
| Namenseingabe | 303 | `name_input` (R30) mit NameInput-Fenster (`UI.name_input`) | ⚠️ A-Z/a-z/0-9/Leerzeichen, `max` 1–16 (Default 8), Esc nur bei `cancel`; Umlaute fehlen (Tasten lesen Ein-Zeichen-Namen) |
| HP ändern | 311 | `actor_hp` (R28) + `heal` | ✅ Ziel `alle` oder ein Actor, vorzeichenbehaftet (negativ = Schaden), geklemmt auf 0..Max; Operand const oder `source=variable` + `src` (R43) |
| SP ändern | 312 | `actor_sp` (R28) | ✅ wie `actor_hp`, Rune3D nennt das Feld `mp` (XP: SP); Operand const/Variable (R43) |
| Zustand ändern | 313 | `state` (R42) | ✅ Ziel `alle`/Held, Zustand per ID oder Name, `mode` add/remove, `turns` optional; Zustände hängen am Helden (Spielstand, Menü) und wandern in den Kampf |
| Alles wiederherstellen | 314 | `heal` ohne Betrag | ✅ heilt voll und entfernt alle Zustände wie XP `recover_all`; mit `amount` nur Teilheilung (Rune3D-Zusatz, Zustände bleiben) |
| EXP ändern | 315 | `actor_exp` (R28) | ✅ läuft über `Party.set_exp`, Stufenaufstiege inklusive; Operand const/Variable (R43) |
| Level ändern | 316 | `actor_level` (R28) | ✅ Aufstieg über `level_up!` (Wachstum wie im Kampf), Abstieg senkt nur die Stufe (keine Klassentabellen); Operand const/Variable (R43) |
| Parameter ändern | 317 | `actor_param` (R42) | ✅ max_hp/max_mp/atk/def/spi/agi vorzeichenbehaftet, HP/MP werden bei sinkendem Maximum gekappt (XP kennt nur drei Parameter); Operand const/Variable (R43) |
| Skills ändern | 318 | `actor_skill` (R44) | ✅ Fertigkeit per ID/Name bei einem Held oder allen lernen/vergessen (`mode` add/remove); Party-Snapshot speichert gelernte IDs |
| Ausrüstung ändern | 319 | — | ❌ |
| Name ändern | 320 | `actor_name` (R42) | ✅ genau ein Held, neuer Name in Menü, Kampf und Spielstand |
| Klasse ändern | 321 | — | ❌ |
| Grafik ändern | 322 | — | ❌ |
| Gegner HP/SP/Zustand/Heilen | 331–334 | — | ❌ |
| Gegner erscheinen/verwandeln | 335/336 | — | ❌ |
| Kampf-Animation | 337 | — | ❌ |
| Schaden zufügen | 338 | — | ❌ |
| Aktion erzwingen | 339 | — | ❌ |
| Kampf abbrechen | 340 | — | ❌ |

### G. System (351–355, 601–603)

| XP | Code | Rune3D | Status |
|---|---|---|---|
| Menü öffnen | 351 | `menu` (R32) | ✅ öffnet das Spielmenü über `$game_temp.menu_calling` (kein Doppel-Öffnen, Esc bleibt) |
| Speicherbildschirm | 352 | `save_game` | ✅ |
| Game Over | 353 | `gameover`-Flag | ✅ |
| Zum Titelbildschirm | 354 | `to_title` | ✅ |
| Script | 355 | `script` | ✅ |
| Wenn Sieg / Flucht / Niederlage | 601–603 | `if_battle_result` (R32) | ✅ ein Befehl mit `result`: sieg/flucht/niederlage, Zweige wie `if_gold`; ohne Kampf gilt sonst |

**Rune3D-Befehle ohne XP-Entsprechung:** `log`, `toggle_switch`, `effect`
(Partikel), `move_player` (relativ) — sinnvolle Eigenheiten, bleiben.

---

## 2. Datenbank — XP: 13 Register, Rune3D: 13 Register (7 mit Engine-Wirkung)

Seit **Etappe 41** hat F9 alle 13 XP-Register als Registerkarten in
XP-Reihenfolge, mit Liste links und Formular rechts. „Engine" sagt, ob das
Spiel die Daten heute auswertet; „vorbereitet" heißt: vollständig gepflegt und
gespeichert, aber noch ohne Spielwirkung (siehe `docs/BACKLOG.md`).
Details: `docs/ETAPPE_41_F9_DATENBANK.md`.

| XP (`$data_*`) | Rune3D (F9, `database.json`) | Engine | Status |
|---|---|---|---|
| Actors | Helden (12 Felder, neu: Stufe/Exp/Fertigkeiten) | liest | ✅ |
| Classes | Klassen (6 Felder, Kampfposition, erlaubte Ausrüstung) | vorbereitet | ⚠️ |
| Skills | Fertigkeiten (10 Felder, Art/Ziel als Auswahl) | liest | ✅ |
| Items | Items (7 Felder, Art als Auswahl) | liest | ✅ |
| Weapons | Waffen (9 Felder) | vorbereitet | ⚠️ |
| Armors | Rüstungen (9 Felder, Platz: shield/head/body/accessory) | vorbereitet | ⚠️ |
| Enemies | Gegner (14 Felder, neu: Farbe/Beschreibung) | liest | ✅ |
| Troops | Truppen (4 Felder) | liest | ✅ |
| States | Zustände (6 Felder, 10 Arten) | liest | ✅ |
| Animations | Animationen (8 Felder) | vorbereitet | ⚠️ |
| Tilesets | Tilesets (6 Felder); Karten-Gelände weiter in F7 | vorbereitet | ⚠️ |
| Common Events | Gem. Events (5 Felder inkl. Befehlsliste als JSON) | liest | ✅ |
| System | System als Einzeldatensatz (6 Felder); Projektstart weiter in `system.json` | vorbereitet | ⚠️ |

Bewusste Abweichungen: Rune3D adressiert Daten beim Namen (nur gemeinsame
Events zählen numerisch wie XP), Auswahlfelder sind Textfelder mit Werteliste
statt Dropdowns (unbekannte Altdaten bleiben sichtbar), und ein „Change
Maximum" für die Listengröße gibt es nicht — Neu/Löschen bleibt der Weg.

---

## 3. Szenen & Menü — XP: 16 Szenen, Rune3D: ~5

| XP-Szene | Rune3D | Status |
|---|---|---|
| Scene_Title (New Game/Continue/Shutdown, per Script überschreibbar) | `Scene_Title` + `Scene_File` (Continue → Slot-Wähler), Launcher entfernt, Override via Projekt-autoload vor dem Szenenaufbau (LOGIK_PRUEFUNG AB, Runde 12) | ✅ |
| Scene_Map | Game (3D-Karte) | ✅ |
| Scene_Menu (Item/Skill/Equip/Status/Save/End + PlayTime/Steps/Gold + Gesichter) | Esc-Menü: Gegenstand/Fertigkeit/Status, Speichern/Laden, Titel; Party-Panel mit **prozeduralen Gesichtern** (Farbe + Initialen + HP-Streifen, 20b-Nacharbeit) | ⚠️ Equip fehlt (keine Waffen/Rüstungen), Gesichter sind keine Bilddateien |
| Scene_Item (Items außerhalb des Kampfs) | — | ❌ |
| Scene_Skill | Menüpunkt „Fertigkeit": Held → Fertigkeiten mit MP-Kosten und Beschreibung (nur Ansehen wie XP) | ⚠️ kein Anwenden außerhalb des Kampfes |
| Scene_Equip | — | ❌ |
| Scene_Status | Party-Zeilen (HP/MP/Level) | ⚠️ ohne Detailwerte (ATK/DEF/EVA, EXP-Balken) |
| Scene_File / Save / Load | Slots im Menü | ✅ sinngemäß |
| Scene_End (Beenden) | — | ❌ |
| Scene_Battle | `battle.rb` | ⚠️ Grundgerüst |
| Scene_Shop | — | ❌ |
| Scene_Name (Namenseingabe) | — | ⚠️ als Fenster `UI.name_input` (R30), keine eigene Szene |
| Scene_Gameover | eigene Szene `Scene_Gameover` (Game-Over-Panel, Taste → Titel) | ✅ |
| Scene_Debug | — | ❌ |

---

## 4. Kampf im Detail

| Merkmal | RPG Maker XP | Rune3D |
|---|---|---|
| Befehle je Held | Angreifen, **Skill**, Item, **Wache**, Flucht | Angreifen, **Fertigkeit**, Item, Flucht (20a; Wache fehlt) |
| Skills/Magie | `$data_skills`: MP-Kosten, Elemente, Erfolgsrate, Status-Plus/Minus, Zieltyp | **seit 20a**: `skills` mit `mp`, `power`, `kind` (damage/heal/buff), `scope` (enemy/ally/self), Zustandschance; Elemente und Erfolgsrate fehlen |
| Zustände | `$data_states`: Vergiftung, Schlaf, Stille, Paralyse … mit Rang, Auto-Heilung, Slip-Damage, Sperr-Regeln (`restriction`) | **seit 20a**: `states` mit `kind` (poison/regen/atk_up/def_up), Schaden/Heilung pro Runde, Dauer, Werte-Boni; kein Schlaf/Stille/Paralyse, keine Sperr-Regeln |
| Parameter-Logik | Str/Dex/Agi/Int → Atk/Def/Hit/Eva; Ausrüstung wirkt auf alle Werte | Basiswerte direkt, keine Ausrüstung |
| Gegner-KI | Aktionstabelle mit Bedingungen (HP-%, Runde, Zustand, Ziele) | greift zufälligen lebenden Helden an |
| Rundenlogik | Runde, Initiative nach AGI | Runde, Eingabereihenfolge |
| EXP/Level | `exp=` löst **Level-Up** mit Wachstum und Klassen-Skills aus | EXP gutgeschrieben; **seit Runde 22 Level-Up** (Kurve 2.7, Gesamt-Exp, prozentuales Wachstum, `actor.exp = n`); Fertigkeiten hängen seit 20a am Helden, nicht an einer Klasse |
| Beute | Gold + Drop mit Chance | ✅ gleich |
| Wache (Guard) | ✅ | ❌ |
| Animationen je Aktion | Animation1/Animation2 pro Battler | ❌ |
| Battleback, BGM/ME | ✅ | ❌ |
| Ergebniszweige 601–603 | Sieg/Flucht/Niederlage als Befehlszweige | ✅ `if_battle_result` (ein Befehl statt drei Codes) |
| Flucht, Game Over, Items | ✅ | ✅ (gezielte Items, KO/Wiederbelebung) |

---

## 5. Karte, Tileset & Passierbarkeit

| Merkmal | RPG Maker XP | Rune3D |
|---|---|---|
| Kartenformat | Kachel-Raster | rechteckige Karte mit **3D-Gelände** |
| Passierbarkeit | pro Kachel 4 Richtungen + Terrain-Tags | `blocked`-Zonen + `solid`-Objekte (`Passage`) ⚠️ keine Kachel-4-Richtungen |
| Boden-Darstellung | Tileset-Kacheln, Autotiles | Splat-Texturen (Atlas), harter Rand (kein Weich-Übergang) |
| Höhen | ❌ (flach) | ✅ Höhen-Malwerkzeug, Berge |
| Nebel/Panorama/Battleback | pro Karte + Befehle | Karten-JSON (Nebel/Panorama) ⚠️, kein Battleback |
| Wetter | ✅ | ✅ (3D-Partikel) |
| Karte scrollen | ✅ | 3D-Kamera (andere Mechanik) |
| Common Events | ✅ | ❌ |
| Event-Seiten/Bedingungen | ✅ | ✅ (inkl. Self-Switches A–D) |
| Event-Trigger | `auto` = Autorun (bis Seitenwechsel), `touch` = Wiederbetreten | ✅ (Z6/D1+D2, LOGIK Runde 17) |
| Kartenbaum | Baum mit Gruppen/Elternkarten | ✅ (Z8: Einrücktiefe in `system.json` → `map_tree`) |
| Karte während einer Nachricht | Karte läuft weiter (Events/Parallel), Spieler steht | ✅ (Z8/D3, Runde 20) |
| Nachrichtenfenster | Text + Wahl + Zahleneingabe in EINEM Fenster (Window_Message/Choice) | ✅ (Runde 21: Kombi-Fenster) |
| Editor-Undo | XP hat keins | ✅ (Z8: ganze Karte, 50 Schritte) |
| Bewegungsroute | ✅ | ✅ (XP-Schrittnamen + Codes 0–45, F1 geprüft) |
| Spieler-Start/Teleport | ✅ | ✅ |

---

## 6. RGSS-Schicht

| XP | Rune3D | Status |
|---|---|---|
| `$game_system/$game_switches/$game_variables/$game_self_switches` | ✅ | ✅ |
| `$game_party/$game_player/$game_map/$game_screen/$game_temp` | ✅ (Screen-Tone seit Etappe 35 sichtbar, sofort statt mit Übergangsdauer; Flash/Shake/Pictures ohne Wirkung; **Calling-Flags von `$game_temp` seit R28 wirksam**: battle/menu/save_calling, player_transferring, to_title) | ⚠️ |
| `$data_actors/$data_items/$data_system` | ✅ (String-IDs, JSON statt Marshal) | ⚠️ bewusst anders |
| `$data_skills/$data_states/$data_common_events` | ✅ (Hash über die IDs, 20a/20c) | ⚠️ bewusst anders: Zugriff über `$data_states['gift']` statt Index 1 |
| `$data_classes/$data_weapons/$data_armors/$data_animations/$data_tilesets` | — | ❌ |
| `Graphics/Input/Audio`, `Sprite/Window/Viewport/Bitmap` | ✅ | ⚠️ kein Bitmap-Dateiladen, kein freeze/transition, Audio ohne Pitch |
| `$scene` + Szenen | ✅ (Scripts können Szenen stellen) | ⚠️ Engine-Takt statt eigener Hauptschleife |
| Tilemap/Plane | — | ❌ bewusst (3D-Terrain ersetzt Kachel-Ebenen) |
| XP-Defaultscripts (90 Dateien) direkt nutzbar | Manifest `game/engine/xp_manifest.rb` (S1): Reihenfolge, Stufen, Engine-Konflikte; 51 Klassenrümpfe laden isoliert, 38 brauchen RGSS-Grafik, 089 Main gesperrt; 000–004 durch Engine-Wrapper ersetzt, API-Vertrag in `tools/xp_contract_test.rb` (S2); Etappe 28 (ungetestet): F11 zeigt alle 90 Defaults, Start lädt aktive Klassenrümpfe im Namensraum `XP`, Fehler im Backtrace-Fenster; **als Spielablauf läuft noch keine XP-Datei** | ❌ Laufzeit offen (S2–S8, `docs/STUFENPLAN_EDITOR_UND_XP_SCRIPTS.md`) |

---

## 7. Wie sich XP in 3D übersetzen lässt (Design-Notiz)

| XP (2D) | Rune3D (3D) |
|---|---|
| Tilemap/Autotiles | Splat-Weightmap mit Weich-Übergang im Terrain-Shader (Backlog: harter Rand) |
| Charakter-Grafiken (4-Richtungen-Bitmap) | Billboard-Sprites im 3D-Raum statt Quader |
| Ballons/Animationen | Billboard-Overlay + Partikel über dem Ziel (`effects.rb` hat Partikelbasis) |
| Pictures (231–235) | Screen-Space-Overlay über dem 3D-Viewport (2D-Blits wie UI-Schicht) |
| Tone/Flash/Shake | Post-Processing (Vollbild-Tint/Blitz) + Kamera-Shake |
| Karte scrollen | Kamera-Scroll/Fokus auf Position |
| Battleback | 3D-Kampfumgebung (Karte als Kulisse) + UI-Fenster |
| Nebel/Panorama | vorhanden (Karten-JSON + Wetter) |
| Kachel-Passierbarkeit 4 Richtungen | `blocked`-Zonen + Objekt-Hindernisse (3D-native Lösung) |

---

## 8. Priorisierte Etappen-Roadmap (Vorschlag: Etappe 20)

| Etappe | Inhalt | Warum jetzt |
|---|---|---|
| **20a Kampf: Skills & Zustände** | ~~DB `skills`+`states`~~ ✅, ~~`battle.rb`: Skill-Befehl mit MP~~ ✅, ~~Zustände pro Runde (Vergiftung usw.)~~ ✅, ~~**Level-Up bei EXP (D4)**~~ ✅ — offen: F9-Reiter für `skills`/`states`, Gegner-Fertigkeiten und -Aktionen mit Bedingungen | Größte Spiel-Lücke, Kern seit Runde 23 geschlossen |
| **20b Menü wie XP** | ~~Item (außerhalb Kampf)~~ ✅, ~~Skill~~ ✅, ~~Status-Detail~~ ✅, ~~Parteigesichter~~ ✅ (prozedural) — offen: Equip (keine Ausrüstung), Fortschritts-/Quest-Anzeige, Gesichter in Spielstand-Slots | Kern seit Runde 24 umgesetzt |
| **20c Event-Befehle Welle 1 (Ablauf)** | ~~`loop`/`break`/`label`/`jump`/`exit_event`/`erase_event`/`button_input`/`text_options`~~ ✅, ~~**Common Events**~~ ✅ (Runde 27, 35 Befehlstypen: Datenhaltung, Aufruf 117, Trigger, MapCheck, RGSS) — offen: eigener Editor-Reiter für Common Events (F9/F6) | Struktur, die fast jedes XP-Projekt braucht |
| **20d Event-Befehle Welle 2 (Daten)** | ~~erweiterte Bedingungen `if_gold`/`if_item`/`if_script`~~ ✅, ~~134/135/136 als Befehle (`save_access`/`menu_access`/`encounter_access`)~~ ✅ (R28, 46 Befehlstypen; Timer samt Anzeige, HP/SP/EXP/Stufe erledigt) — offen: `party_member`, ~~`$game_temp`-Calling-Flags auswerten~~ ✅ (R28, LOGIK E.3) | Verdoppelt die Aussagekraft von Variablen/Schaltern; XP-Scripts, die Szenen über `$game_temp` starten, funktionieren dann |
| **20e Shop & Name** | ~~Befehl 302 + Shop-Fenster (kaufen/verkaufen/Preis)~~ ✅, ~~303 Namenseingabe~~ ✅ (R30; Umlaute offen) | XP-typisch, eigenständig nutzbar |
| **20f Bildschirm-Ebenen** | ~~Tone 223~~ ✅, ~~Flash 224~~ ✅, ~~Shake 225~~ ✅ (R32), ~~Pictures 231–235~~ ✅ (R33), ~~601–603~~ ✅, ~~351~~ ✅ | **Gruppe fertig** |
| **20g Darstellung** | Figuren-Billboards, Ballons, Kampf-/Event-Animationen (207/337) | Sichtbarster XP-Optik-Unterschied |
| **20h Editor & DX** | Dialoge F6/F9 XP-orientiert (Screenshots), Undo/Redo über Splats/NPCs/Events | Bedienung wie im Vorbild |
| **20i Ton (Audit „20x“)** | Datei-Lader (WAV/OGG) in `src/core/sound.cpp`, Karten-BGM aus der Karten-JSON, Event-Befehle 241/242/245/246, Pitch pro Aufruf, `bgm_fade` über Zeit (N) | RGSS-`Audio.*` ist vorhanden, aber ohne Datei-Lader substanzlos; XP-Skripte spielen sonst wortlos nichts |
| **20j Panorama-Scroll + Wetter-Dauer** | Panorama als scrollende Bildebene (Bild, Loop, Speed, Hue) statt nur `panorama.color`; `weather.duration` in der Karten-JSON (O) | Zwei kleine, sichtbare Datenlücken in derselben Kartenstruktur |
| **20k Durchlässigkeit & Sprung** | Mover `through` (Wände ignorieren, wie XP `$game_player.through = true`) und Routen-`jump` als echte Verlagerung statt No-Op (Q) | Zwei als No-Op dokumentierte RGSS-/Routen-Bausteine, die XP-Scripts erwarten |
| **20l Grenzwerte & Messpunkte** | Seiten-Refresh per Dirty-Flag ab ~hunderten Events, Draw-Batching messen, Menü-/Listen-Rendering unter Last (S) | Kein Hotspot heute — Messpunkte festlegen, bevor die Karten groß werden |

**Abhängigkeiten:** 20b braucht 20a · 20d-Teile (Zustands-Befehle) brauchen
20a · 20e unabhängig · 20f/20g/20h unabhängig · 20i/20j/20k/20l unabhängig
und klein (jederzeit einschiebbar; 20i zuerst, weil es drei Schichten
berührt: `sound.cpp`, Karten-JSON, Befehlslisten).

**Reihenfolge-Empfehlung (Z7):** 20a vor 20d (Level-Up macht EXP/Level-Befehle
sinnvoll) · 20d vor 20c-Teilen mit Bedingungen · 20i unabhängig vorziehen,
wenn Ton gebraucht wird. Das Quellen-Register `docs/befehle.json` (Z8) macht
jede dieser Etappen billiger: neue Befehle landen zuerst dort, dann in
`CMD_TYPES`/`ISSUE_CMD_TYPES`/`EV_TYPES`/`interpreter.rb` — `tools/befehle_test.rb`
meldet jede vergessene Stelle.

---

## 9. Was Rune3D bereits dicker hat als XP

- 3D-Gelände mit Höhen, Malwerkzeug für Boden und Berge
- Third-Person-Physik (Schwerkraft, Sprung, Hindernisse)
- Splat-Texturen aus der Materialbasis (PNG/BMP-Import, Drag & Drop)
- Wetter-Partikel in 3D, Prozedur-Sounds ohne Dateien
- Block-Zonen als 3D-Wände (Kachel-Logik nicht nötig)
- Hot-reloadbare echte Ruby-Skripte (F5), eigene Projektverwaltung
- Zufallskämpfe pro Karte mit XP-Würfel, gezielte Kampf-Items mit KO-Reanimierung
