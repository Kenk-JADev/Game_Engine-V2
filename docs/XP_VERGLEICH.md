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

## 1. Event-Befehle — 92 XP-Befehle, 26 in Rune3D

XP hat **92 Top-Level-Ereignis-Codes** (101–355 plus 601–603). Rune3D hat
**26 Befehlstypen**. Bilanz: **21 ✅ · 13 ⚠️ · 58 ❌**.

### A. Nachricht & Ablaufsteuerung (101–119)

| XP | Code | Rune3D | Status |
|---|---|---|---|
| Text anzeigen | 101 | `text` | ✅ |
| Wahlmöglichkeiten | 102 (+403) | `choices` mit `cancel`/`cancel_branch` (R16) | ✅ |
| Zahleneingabe | 103 | `input_number` | ✅ |
| Textoptionen (Schrift/Bild/Position) | 104 | — | ❌ |
| Tastaturabfrage (Button Input) | 105 | — | ❌ |
| Warte | 106 | `wait` | ✅ |
| Verzweigung (13 Bedingungsarten) | 111 | `if_switch` / `if_variable` / `if_self_switch` | ⚠️ nur 3 von 13 Arten (Schalter, Variable, Self-Switch; XP hat zusätzlich Timer, Actor, Enemy, Character, Gold, Item, Weapon, Armor, Button, Script) |
| Schleife | 112 | — | ❌ |
| Schleife verlassen | 113 | — | ❌ |
| Event-Ausführung beenden | 115 | — | ❌ |
| Event löschen | 116 | — | ❌ |
| Gemeinsames Event aufrufen | 117 | — | ❌ (keine Common Events) |
| Sprungmarke | 118 | — | ❌ |
| Zu Marke springen | 119 | — | ❌ |

### B. Schalter, Variablen, Party (121–136)

| XP | Code | Rune3D | Status |
|---|---|---|---|
| Schalter setzen | 121 | `switch` | ✅ |
| Variablen steuern (6 Operationen, 8 Quellen) | 122 | `variable` | ⚠️ nur `value`/`add`; XP zusätzlich − × ÷ Rest und Quellen Zufall/Item/Actor/Enemy/Charakter/selbst |
| Self-Switch | 123 | `self_switch` | ✅ |
| Timer steuern | 124 | — | ❌ (RGSS-Timer existiert, ohne Anzeige) |
| Gold ändern | 125 | `gold` | ✅ |
| Items ändern | 126 | `item` | ✅ |
| Waffen ändern | 127 | — | ❌ (keine Waffen in der DB) |
| Rüstungen ändern | 128 | — | ❌ (keine Rüstungen in der DB) |
| Party-Mitglied hinzufügen/entfernen | 129 | — | ❌ |
| Windowskin | 131 | — | ❌ |
| Kampf-BGM | 132 | — | ❌ |
| Kampf-Ende-ME | 133 | — | ❌ |
| Speichern erlauben/sperren | 134 | `$game_system.save_disabled` | ⚠️ Flag da, kein Editor-Befehl |
| Menü erlauben/sperren | 135 | `$game_system.menu_disabled` | ⚠️ Flag da, kein Editor-Befehl |
| Zufallskämpfe erlauben/sperren | 136 | `encounter_disabled` | ⚠️ Flag da, kein Editor-Befehl |

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
| Bildschirmton (Tone) | 223 | `$game_screen.tone` | ⚠️ Zustand da, kein Effekt im Renderer |
| Bildschirmblitz | 224 | `$game_screen.flash_color` | ⚠️ ohne Effekt |
| Bildschirmwackeln | 225 | `$game_screen.shake` | ⚠️ ohne Effekt |
| Bilder zeigen/bewegen/drehen/färben/löschen | 231–235 | `$game_screen.pictures` | ❌ nur Platzhalter-Hash |
| Wettereffekte | 236 | `weather` | ✅ (Regen/Schnee, 3D-Partikel) |

### E. Audio (241–251)

| XP | Code | Rune3D | Status |
|---|---|---|---|
| BGM abspielen | 241 | prozeduraler Musik-Loop „musik“ | ⚠️ keine BGM-Verwaltung pro Karte/Kampf |
| BGM ausblenden | 242 | — | ❌ |
| BGS abspielen/ausblenden | 245/246 | — | ❌ |
| BGS merken/wiederherstellen | 247/248 | — | ❌ |
| ME abspielen | 249 | — | ❌ |
| SE abspielen | 250 | `sound` | ✅ (7 prozedurale Sounds) |
| SE stoppen | 251 | — | ❌ |

### F. Kampf & Actor-Status (301–340)

| XP | Code | Rune3D | Status |
|---|---|---|---|
| Kampf starten | 301 | `battle` | ✅ (ohne Ergebniszweige) |
| Shop | 302 | — | ❌ (kein Shop) |
| Namenseingabe | 303 | — | ❌ |
| HP ändern | 311 | `heal` | ⚠️ nur Heilen, ganze Party; XP pro Ziel + Schaden möglich |
| SP ändern | 312 | — | ❌ |
| Zustand ändern | 313 | — | ❌ (keine Zustände) |
| Alles wiederherstellen | 314 | `heal` ohne Betrag | ✅ |
| EXP ändern | 315 | — | ❌ |
| Level ändern | 316 | — | ❌ |
| Parameter ändern | 317 | — | ❌ |
| Skills ändern | 318 | — | ❌ |
| Ausrüstung ändern | 319 | — | ❌ |
| Name ändern | 320 | — | ❌ |
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
| Menü öffnen | 351 | — | ❌ (Menü nur per Esc, kein Befehl) |
| Speicherbildschirm | 352 | `save_game` | ✅ |
| Game Over | 353 | `gameover`-Flag | ✅ |
| Zum Titelbildschirm | 354 | `to_title` | ✅ |
| Script | 355 | `script` | ✅ |
| Wenn Sieg / Flucht / Niederlage | 601–603 | — | ❌ (`battle` hat `can_lose`, aber keine Befehlszweige) |

**Rune3D-Befehle ohne XP-Entsprechung:** `log`, `toggle_switch`, `effect`
(Partikel), `move_player` (relativ) — sinnvolle Eigenheiten, bleiben.

---

## 2. Datenbank — XP: 13 Sammlungen, Rune3D: 4

| XP (`$data_*`) | Rune3D (F9, `database.json`) | Status |
|---|---|---|
| Actors | Helden (9 Felder) | ✅ |
| Classes | — (Klasse nur als Textfeld) | ❌ |
| Skills | — | ❌ |
| Items | Items (7 Felder) | ✅ |
| Weapons | — | ❌ |
| Armors | — | ❌ |
| Enemies | Gegner (12 Felder) | ✅ |
| Troops | Truppen (4 Felder) | ✅ |
| States | — | ❌ |
| Animations | — | ❌ |
| Tilesets | eigener Tileset-Editor (F7) | ⚠️ andere Mechanik (3D-Splat) |
| Common Events | — | ❌ |
| System | `system.json` | ⚠️ Teilmenge |

---

## 3. Szenen & Menü — XP: 16 Szenen, Rune3D: ~5

| XP-Szene | Rune3D | Status |
|---|---|---|
| Scene_Title (New Game/Continue/Shutdown, per Script überschreibbar) | `Scene_Title` + `Scene_File` (Continue → Slot-Wähler), Launcher entfernt, Override via Projekt-autoload vor dem Szenenaufbau (LOGIK_PRUEFUNG AB, Runde 12) | ✅ |
| Scene_Map | Game (3D-Karte) | ✅ |
| Scene_Menu (Item/Skill/Equip/Status/Save/End + PlayTime/Steps/Gold) | Esc-Menü: Speichern/Laden/Titel + Party-Panel | ⚠️ stark reduziert |
| Scene_Item (Items außerhalb des Kampfs) | — | ❌ |
| Scene_Skill | — | ❌ |
| Scene_Equip | — | ❌ |
| Scene_Status | Party-Zeilen (HP/MP/Level) | ⚠️ ohne Detailwerte (ATK/DEF/EVA, EXP-Balken) |
| Scene_File / Save / Load | Slots im Menü | ✅ sinngemäß |
| Scene_End (Beenden) | — | ❌ |
| Scene_Battle | `battle.rb` | ⚠️ Grundgerüst |
| Scene_Shop | — | ❌ |
| Scene_Name (Namenseingabe) | — | ❌ |
| Scene_Gameover | eigene Szene `Scene_Gameover` (Game-Over-Panel, Taste → Titel) | ✅ |
| Scene_Debug | — | ❌ |

---

## 4. Kampf im Detail

| Merkmal | RPG Maker XP | Rune3D |
|---|---|---|
| Befehle je Held | Angreifen, **Skill**, Item, **Wache**, Flucht | Angreifen, Item, Flucht |
| Skills/Magie | `$data_skills`: MP-Kosten, Elemente, Erfolgsrate, Status-Plus/Minus, Zieltyp | ❌ (MP existiert, wird nie verbraucht) |
| Zustände | `$data_states`: Vergiftung, Schlaf, Stille, Paralyse … mit Rang, Auto-Heilung, Slip-Damage, Sperr-Regeln (`restriction`) | ❌ |
| Parameter-Logik | Str/Dex/Agi/Int → Atk/Def/Hit/Eva; Ausrüstung wirkt auf alle Werte | Basiswerte direkt, keine Ausrüstung |
| Gegner-KI | Aktionstabelle mit Bedingungen (HP-%, Runde, Zustand, Ziele) | greift zufälligen lebenden Helden an |
| Rundenlogik | Runde, Initiative nach AGI | Runde, Eingabereihenfolge |
| EXP/Level | `exp=` löst **Level-Up** mit Wachstum und Klassen-Skills aus | EXP wird gutgeschrieben, **aber nie ein Level-Up** (tote Zahl) |
| Beute | Gold + Drop mit Chance | ✅ gleich |
| Wache (Guard) | ✅ | ❌ |
| Animationen je Aktion | Animation1/Animation2 pro Battler | ❌ |
| Battleback, BGM/ME | ✅ | ❌ |
| Ergebniszweige 601–603 | Sieg/Flucht/Niederlage als Befehlszweige | ❌ (nur `can_lose`) |
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
| Bewegungsroute | ✅ | ✅ (XP-Schrittnamen + Codes 0–45, F1 geprüft) |
| Spieler-Start/Teleport | ✅ | ✅ |

---

## 6. RGSS-Schicht

| XP | Rune3D | Status |
|---|---|---|
| `$game_system/$game_switches/$game_variables/$game_self_switches` | ✅ | ✅ |
| `$game_party/$game_player/$game_map/$game_screen/$game_temp` | ✅ (Screen-Tone/Flash/Shake/Pictures ohne Wirkung) | ⚠️ |
| `$data_actors/$data_items/$data_system` | ✅ (String-IDs, JSON statt Marshal) | ⚠️ bewusst anders |
| `$data_classes/$data_skills/$data_weapons/$data_armors/$data_enemies/$data_troops/$data_states/$data_animations/$data_tilesets/$data_common_events` | — | ❌ |
| `Graphics/Input/Audio`, `Sprite/Window/Viewport/Bitmap` | ✅ | ⚠️ kein Bitmap-Dateiladen, kein freeze/transition, Audio ohne Pitch |
| `$scene` + Szenen | ✅ (Scripts können Szenen stellen) | ⚠️ Engine-Takt statt eigener Hauptschleife |
| Tilemap/Plane | — | ❌ bewusst (3D-Terrain ersetzt Kachel-Ebenen) |

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
| **20a Kampf: Skills & Zustände** | DB `skills`+`states` (F9-Reiter), `battle.rb`: Skill-Befehl mit MP, Zustände pro Runde (Vergiftung usw.), Gegner-Aktionen mit Bedingungen, **Level-Up bei EXP** | Größte Spiel-Lücke; ohne Skills bleibt MP tot, EXP eine tote Zahl |
| **20b Menü wie XP** | Item (außerhalb Kampf), Skill, Status-Detail, Gold/Fortschritt — Scene_Menu als echtes XP-Menü | Braucht 20a fürs Skill-Fenster |
| **20c Event-Befehle Welle 1 (Ablauf)** | `loop`/`break`/`label`/`jump`/`exit_event`/`erase_event`/`button_input`/`text_options` + **Common Events** (Datenhaltung + F6 + 117) | Struktur, die fast jedes XP-Projekt braucht |
| **20d Event-Befehle Welle 2 (Daten)** | Variable-Operationen/Quellen, Timer (+Anzeige), erweiterte Bedingungen (Gold/Item/Actor/Button/Script), `party_member`, HP/SP/EXP/Level-Befehle, 134/135/136 als Befehle | Verdoppelt die Aussagekraft von Variablen/Schaltern |
| **20e Shop & Name** | Befehl 302 + Shop-Fenster (kaufen/verkaufen/preis), 303 Namenseingabe | XP-typisch, eigenständig nutzbar |
| **20f Bildschirm-Ebenen** | Pictures 231–235 als Overlay, Flash/Shake/Tone als PostFX, Ergebniszweige 601–603, Befehl 351 (Menü) | Macht die „Bildschirm“-Gruppe echt |
| **20g Darstellung** | Figuren-Billboards, Ballons, Kampf-/Event-Animationen (207/337) | Sichtbarster XP-Optik-Unterschied |
| **20h Editor & DX** | Dialoge F6/F9 XP-orientiert (Screenshots), Undo/Redo über Splats/NPCs/Events | Bedienung wie im Vorbild |

**Abhängigkeiten:** 20b braucht 20a · 20d-Teile (Zustands-Befehle) brauchen
20a · 20e unabhängig · 20f/20g/20h unabhängig.

---

## 9. Was Rune3D bereits dicker hat als XP

- 3D-Gelände mit Höhen, Malwerkzeug für Boden und Berge
- Third-Person-Physik (Schwerkraft, Sprung, Hindernisse)
- Splat-Texturen aus der Materialbasis (PNG/BMP-Import, Drag & Drop)
- Wetter-Partikel in 3D, Prozedur-Sounds ohne Dateien
- Block-Zonen als 3D-Wände (Kachel-Logik nicht nötig)
- Hot-reloadbare echte Ruby-Skripte (F5), eigene Projektverwaltung
- Zufallskämpfe pro Karte mit XP-Würfel, gezielte Kampf-Items mit KO-Reanimierung
