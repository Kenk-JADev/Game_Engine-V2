# Stufenplan: Editor-Umbau und XP-Defaultscript-Kompatibilität

Stand: Etappe 40b (Event-Command-Architektur; Code fertig, manuell zu prüfen; Editor-F9 weiterhin offen). Grundregel: eine Stufe = Code + Tests + Doku + manuelle
Prüfschritte; `docs/XP_VERGLEICH.md` danach synchronisieren. Keine Stufe
ersetzt vorhandene F5/F6-, Undo-, Dateischutz- oder Datentreuepfade.

## A. Editor

| Stufe | Inhalt | Prüfung | Status |
|---|---|---|---|
| 26 | Kartenbaum oben, Palette darunter | – | durch 29 ersetzt (entsprach nicht XP) |
| 29 | Recherche XP-Editor (`docs/XP_EDITOR_RECHERCHE.md`); Layout wie XP: Palette oben, Kartenbaum unten | `json_test` Geometrie | Code, ungetestet |
| 35 (Himmel/Licht) | Wolken mit Schatten, Himmelsverlauf + Sonne + Sterne, Tag/Nacht (`time`), XP-Tönung (`tone`), schärfere kamerafolgende Schatten; F2: Ton-Gruppe ohne Abschneiden, Farbwähler. Details `docs/ETAPPE_35_HIMMEL.md` | `sky_tests` | ✅ gebaut/getestet, Sichtprüfung offen |
| 36 (Fenster) | F2 mit Reitern inkl. „Himmel & Licht“, Script-Editor wie XP (nummerierte Liste, Suche/Ersetzen, Rechtsklick), DPI-Koordinaten aller Dialoge, Nebel am Horizont. Details `docs/ETAPPE_36_FENSTER.md` | `script_editor_tests`, `build.ps1 -Test` | ✅ gebaut/getestet, Sichtprüfung offen |
| 37 (Symbole/Listen) | Werkzeug-Icons in der Toolbar mit Palette/Modusmenü gekoppelt; neutrales Auswahlwerkzeug; F5 mit 2D-Prefab-Vorschau, Material, Import und neuem Typ; Kartenbaum-Icons und Startkarte fett; Objektliste mit Typ-, Materialspalte und Zustands-Icons. Details `docs/ETAPPE_37_SYMBOLE_LISTEN.md` | `prefab_preview_tests`, `prefab_custom_tests`, `object_list_view_tests`, `map_tree_icons_tests`, `build.ps1 -Test` | ✅ gebaut/getestet, Sichtprüfung offen |
| 38 (Listen/Kontextmenü) | Event-/NPC-Liste mit Zählern, Klick = Kamera dorthin, Doppelklick = F6/F5; doppelte Menüs und Textwerkzeuge entfernt; Objektaktionen per Rechtsklick, Drehen sichtbar; Fehlerindicator oben rechts; Objekt-Material mit Projektordnern. Details `docs/ETAPPE_38_LISTEN_KONTEXTMENUE.md` | `event_npc_list_tests`, `material_display_tests`, `build.ps1 -Test` | gebaut/getestet; Sichtprüfung offen |
| 39 (Sprung/Main.rb) | F2-Flag `jump_allowed` nur für den manuellen Spielersprung; echter Move-Route-Sprung `jump:dx:dz` für Events/Cutscenes; `089 - Main` aus `scripts.json` als frame-sicherer Fiber-Einstieg. Details `docs/ETAPPE_39_SPRUNG_MAIN_RB.md` | `json_test`, `move_route_test`, `script_test`, `xp_scripts_test`, `build.ps1 -Test` | Code/Tests gebaut; Sichtprüfung offen |
| 40 (Datenbank F9) | XP-Reiter von Actors bis System mit Listen-/Bearbeitungsregeln; vorhandene JSON-Datenwege und Rückfragen bleiben erhalten | `database_tests`, `build.ps1 -Test`, `windows_runtime_smoke.ps1` | Editor geplant; Engine-Schicht ✅ gebaut/getestet in `docs/ETAPPE_40_RPG_DATABASE.md` (`rpg_database_test`) |
| 40b (Event-Commands) | 90 typisierte Befehle der XP-Tabs 1–3 mit Handler-Registry, injizierten Ports, Interpreter-Stack, Wait/Label/Common Events und eigener Validierung; bestehende `cmd`-Register bleiben unberührt. Details `docs/ETAPPE_40B_EVENT_COMMANDS.md` | `event_commands_test`, `test_all.rb --strict`, `lint_repo.rb` | Code fertig; manuell zu bauen/prüfen |
| 41 (Höhen-/Blockwerkzeug) | vorhandener Höhenmodus bekommt Hoch/Runter und Pinselgröße (klein/groß), ein Undo pro Strich; kein zweites Höhensystem | `brush_tools_tests`, `build.ps1 -Test` | geplant |
| 42 (Auswahl-Gizmo) | X/Y-Pfeile, Drehkreis und Z-Pfeil am gewählten Objekt; Auswahl bleibt ebene-neutral | `gizmo_tests`, `build.ps1 -Test` | geplant |
| 43 (Tileset-Editor) | Grafikvorschau im 8-Spalten-Raster, eigene Grafiken, Autotile-/Terrain-Felder | `tileset_editor_tests`, `build.ps1 -Test` | geplant |
| 44 (RGSS-Syntaxfarben) | F11 farbt `def`, `if`, Klassen, Kommentare und Strings wie im XP; Suche/Ersetzen und Kontextmenü aus Etappe 36 bleiben | `script_editor_tests`, `build.ps1 -Test` | geplant |
| 45 (XP-Skriptgerüste) | eigene, funktional gleichwertige RGSS-Grundgerüste in XP-Reihenfolge/Architektur für die spätere Engine; keine 1:1-Kopie; `089 - Main` bleibt gesperrt im Editor, läuft aber als Fiber-Einstieg | `xp_manifest_test`, `xp_scripts_test` | geplant |
| 34 (Werkzeuge + Sicht) | **Malen/Höhe als Strich** bei gedrückter Maus (Stempelabstand = halbe Pinselgröße, keine Doppelstempel je Rasterzelle, Gelände-Neuaufbau gedrosselt 60 ms, ein Undo pro Strich). **Vorschau im 3D:** Ring mit echtem Radius für den runden Höhenpinsel (Senke blau), Rahmen an der gerasterten Stelle für den Bodenpinsel (Radierer rot), lokales Raster um den Cursor, Mauszeiger je Werkzeug. **Verschieben:** Events/NPCs ziehen (Modus Events oder Verschieben; gesperrt bei offenem F5/F6), Pfeiltasten schieben das gewählte Objekt kamerarelativ um eine Rasterzelle. `src/editor/brush_tools.h`, `map_marker_move` | `brush_tools_tests` | Code, ungetestet; offen: Glätten/Ebnen-Pinsel (braucht neuen Pinseltyp in `world/terrain`), Höhe (y) per Maus ziehen |
| 33 (XP-Event-Bedienung) | Modus Events: Klick daneben setzt/malt nichts; Doppelklick auf freien Boden legt über `ev_new_event` ein Event an der Stelle an; Event-/NPC-Marker mit größerem Trefferrand. Vorhanden und bestätigt: Doppelklick auf Event → Event-Editor, auf NPC → F5; Kartenbaum-Klick wechselt Karte | `editor_modes_tests` | Code, ungetestet; offen: Event ziehen, Event-Kontextmenü, Karten per Drag umhängen |
| 32 (Screenshot-Befund) | **Moderne Windows-Steuerelemente:** Manifest in `RuneEditor.rc` eingebettet (lose Datei wurde wegen MinGW-Standardmanifest ignoriert → Windows-2000-Optik). **Geisterschrift/schwarze Streifen:** `WS_CLIPCHILDREN` am Seitenleisten-Panel entfernt (Gruppenrahmen-Innenraum wurde nie gelöscht). **Unsinnige Knöpfe entfernt:** „Neu“/„Eigenschaften“ über dem Kartenbaum (abgeschnitten, doppelt; XP: Rechtsklick), Undo/Redo aus der Palette → Toolbar-Pfeile; Kartenhinweis gekürzt; „Werkzeug (Modus)“ → „Werkzeug“ | `json_test` Layout | Code, ungetestet; Sichtprüfung nötig |
| 31 (Grafik) | Toolbar-Icons mit echtem Alphakanal (vorher weiße Kästchen auf grauer Leiste: Icons auf Weiß gezeichnet, Maske voll deckend), `src/editor/icon_alpha.h`; Modus-Gruppe in der Toolbar wie XP-Ebenenleiste (Hügel, Kiste, blauer Würfel wie XP-Ereignisebene), einrastend und synchron mit Menü/Palette | `icon_alpha_tests` + Sichtprüfung | Code, ungetestet |
| 30 | Menü „Modus“ (Gelände/Objekte/Events, Haken folgt dem Werkzeug) und „Maßstab“ (1/1, 1/2, 1/4 als Kamerahöhe/-neigung), `src/editor/editor_modes.h`. Design-Aufräumen: ausgegraute Pseudo-Menüeinträge (Kamera-Hinweise, „Karten weiter unten“) entfernt, Hinweise in F1; „Extras“ → „Werkzeuge“ wie XP | `editor_modes_tests` | Code, ungetestet; Toolbar-Knöpfe für Modi offen (brauchen Icons) |
| 27a | Inspektor-Kontext: Überschrift „Karte: …“ / „Objekt n: …“, Kartenübersicht (Objekte/Events/NPCs) mit F2/F5-Wegweiser | `inspector_context_tests` | ✅ gebaut/getestet, Sichtprüfung offen |
| 27b | Knöpfe „NPCs/Objekte (F5)“ und „Events (F6)“ im Inspektor, posten nur vorhandene `WM_COMMAND`s | `inspector_button_command`, Inspektor-Geometrie | umgesetzt (Ergebnis s. Abschlussbericht), Sichtprüfung offen |
| 27c | Event-/NPC-Marker in der 3D-Ansicht auswählbar; Auswahl öffnet F5 mit diesem Event | Auswahlidentität bleibt an Index+Name gebunden | offen |
| 28 | Toolbar/Menü neu gruppiert: Projekt/Karte · Bearbeiten · Events · Ressourcen/Datenbank · Ansicht; Tooltips mit Kürzel | Tabelle Menü↔Kürzel eindeutig (Test) | offen |
| 29 | Gemeinsamer Dialograhmen (`event_layout`-Muster) für F2/F7/F9/F11: DPI, Mindestgröße, Scrollen statt Abschneiden | Geometrietests wie F6 | offen |
| 30 | Diagnose schwarze Streifen: Log von Client-Rect der Ansicht vs. Swapchain-`ResizeBuffers` und Viewport bei Resize/DPI-Wechsel; erst danach Fix | Log-Skript + Sichtprüfung | offen, keine Ursache behauptet |

## B. XP-Defaultscripts (90 Dateien, `XP-Defaultscript/`)

Grundsatz: Kein ungeprüftes Laden. Ein Manifest listet jede Datei mit Status
`geladen` / `adaptiert` / `ersetzt` / `gesperrt`. `089 - Main.rb` bleibt im
Editor `gesperrt` und ist nicht in `@startup`; seit Etappe 39 startet
`Scripts.start_main` seinen Code als Fiber, während der Rune3D-Takt den Frame
vorgibt.

| Stufe | Inhalt | Prüfung |
|---|---|---|
| S1 ✅ | `game/engine/xp_manifest.rb`: Stufen `:definition` (51, Klassenrumpf ohne RGSS ladbar), `:needs_rgss` (38: 023–026, 029–062), `:blocked` (089). Konflikte mit Engine-Klassen: `Interpreter`, `Window_Selectable`, `Scene_Title/File/Gameover` – Laden im Engine-Prozess per Guard verboten | `tools/xp_manifest_test.rb` lädt im Kindprozess |
| S2 (Code fertig, ungetestet) | 000–004 werden **nicht geladen**, sondern durch die live an `GameState` gekoppelten Wrapper `RGSS::GameTemp/GameSystem/GameSwitches/GameVariables/GameSelfSwitches` ersetzt (`XPManifest::REPLACED_BY`). XPs reine Speicher-Arrays würden die Engine-Kopplung zerstören. `XPManifest.public_api` liest die XP-API statisch, `missing_api` meldet Lücken | `tools/xp_contract_test.rb`: 002–004 vollständig (harter Vertrag), 000/001 Lücken nur berichtet; Semantik „unbekannter Schalter = false“ |
| E28 (Code, ungetestet) | F11 zeigt alle 90 Defaults (`scripts.json`, `xp_default`), Laden vor Spielstart im Namensraum `XP`, Backtrace-Fenster `Rune::App.error_box` – Details `docs/ETAPPE_28_XP_SCRIPTS.md` | `tools/xp_scripts_test.rb` |
| S3 | `RPG::*`-Datenmodell als Adapter über JSON (String-IDs ↔ Index-Zugriff), inkl. `$data_classes/weapons/armors` | Rundlauf JSON → RPG-Objekt → JSON verlustfrei |
| S4 | `Bitmap`: Dateiladen, `blt`, `fill_rect`, `draw_text`, Pixelzugriff; `Sprite`/`Viewport`-Lücken | C++-Bindungstests + Ruby-Test |
| S5 | `Graphics.freeze/transition`, Audio-Pitch | Zeitgesteuerter Test ohne blockierende Schleife |
| S6 ✅ (Etappe 39) | Szenenadapter: `$scene.main` wird in `update`-Schritte pro Engine-Tick zerlegt (Fiber), sodass `loop do … Graphics.update` nicht blockiert | `script_test`/`xp_scripts_test`: Main-Fiber yieldet pro Frame, kein Root-Fiber-`FiberError` |
| S7 | Window_*-Klassen (015–031) auf Rune3D-Fenster | je Klasse Ladetest + Sichtprüfung |
| S8 | `Tilemap`/`Plane`: bewusst Adapter auf 3D-Terrain bzw. Hintergrundebene, nur dokumentierter Teil | Grenzen in `XP_VERGLEICH.md` |

Volle RGSS-Kompatibilität wird erst behauptet, wenn S1–S8 getestet sind.
Ergänzend zu S1–S8 liefert Etappe 45 eigene, funktional gleichwertige
Grundgerüste in der dokumentierten XP-Architektur. Der Nutzer braucht diese
Struktur später für die Engine; urheberrechtlich geschützte XP-Originalquellen
werden nicht wörtlich kopiert.
