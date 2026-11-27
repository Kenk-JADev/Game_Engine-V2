# Etappe 37: Werkzeug-Symbole, Objekt-Typ-Vorschau und Listen-Layout

Stand: Code, Logiktests und beide EXEs gebaut/getestet (26.11.2026). Die
Sichtprüfung im echten Windows-Fenster bleibt offen. Diese Etappe folgt der
Regel aus `docs/XP_EDITOR_RECHERCHE.md`:
Erst XP-Verhalten nachschlagen, dann in Rune3D übersetzen. Die Nutzeranfrage
lautete: „Logic macht bei Werkzeug kein Sinn wenn Symbole verwendet muss man
ändern, Objekt-Typ muss man auch verbessern mit Preview fenster wie aussiseht
+ ob man sein stoff selber einfügen kann. Karten Liste fehlt auch Grafik Design
und Objektliste muss auch verbessert werden.“

Auf Rückfrage hat der Nutzer präzisiert:

- „Symbole“ meint die **Toolbar-Werkzeuge**: die Icons sollen das aktive
  Werkzeug/die Ebene direkt und konsistent steuern, nicht nur Dekoration sein.
- „eigener Stoff“ meint **beides**: Objekt-Material-Override **und** ein neuer
  Objekt-Typ mit eigener Grafik.

## Quellen (vor der Implementierung geprüft)

1. RPGA „RPG Maker XP Kurs Teil 1“ <http://www.rpga.info/grundlagenkurs1.pdf>
   – vier Ebenen (1–3 plus Ereignisebene), Umschalten per Icon in der Leiste
   oder F5/F6/F7/F8; auf einer editierbaren Ebene wählt man links ein Tileset
   und oben eines der Zeichenwerkzeuge.
2. El Trastero RPG „Tutorial RPG Maker XP 1 – Conociendo la interfaz“
   <https://eltrasterorpg.blogspot.com/2010/04/tutorial-rpg-maker-xp-parte-1.html>
   – Menü **Modo** mit Capa 1/2/3 und Eventos; Menü **Dibujar** mit
   Rechteck/Ellipse/Füllen; Kartenbaum mit Rechtsklick-Eigenschaften.
3. Offizielle XP-Materialdefinition
   <https://www.rpg-maker.fr/dl/monos/aide/xp/source/rpgxp/material.html>
   – Tilesets sind 8 Kacheln breite PNG-Grafiken; eigene Grafiken kommen aus
   `Graphics/Tilesets`, Autotiles aus `Graphics/Autotiles`.
4. Steam-Guide „RPG Maker XP guide for beginners“
   <https://steamcommunity.com/sharedfiles/filedetails?id=239704501>
   – Ebenen-Icons sind die linken Knöpfe der Werkzeugleiste; der Tileset-Reiter
   der Datenbank zeigt Grafik- und Nebelvorschau samt Optionen.
5. Game8 „How to Use the Editor | RPG MAKER WITH“
   <https://game8.co/games/RPG-MAKER-WITH/archives/465401>
   – nur als neuerer Vergleich: Map Mode hat Layer Selector plus
   Stift/Rechteck/Ellipse/Füllen; Map List ist eine Hierarchie. Nicht als
   XP-Primärquelle verwendet.

## XP-Befund und Rune3D-Entscheidung

| XP-Verhalten | Rune3D vorher | Rune3D in Etappe 37 |
|---|---|---|
| Ebenenknöpfe und Zeichenwerkzeuge sind getrennt, beide aber echte Zustandsknöpfe mit Icon | Modi waren Icons in der Toolbar; die fünf Werkzeuge nur Text-Radios in der Palette | fünf Werkzeug-Icons zusätzlich in der Toolbar, gekoppelt mit Palette und Modusmenü |
| Auswahlwerkzeug gehört nicht automatisch zu den Objektebenen; es belässt die aktive Ebene | `editor_layer_for_tool(2, Gelände)` sprang nach `Objekte` – unlogisch beim Markieren auf Gelände | Auswahl (2) ist neutral und behält Gelände/Objekte/Events; Setzen/Verschieben ⇒ Objekte, Boden/Höhe ⇒ Gelände |
| Tileset-Datenbank zeigt Grafikvorschau und erlaubt eigene Grafiken | F5 zeigte nur den Prefab-Namen, keine Form und keinen Materialweg | F5 zeigt eine 2D-Vorschau der Prefab-Teile, Material-Auswahl, Import und „Neuer Typ aus Material“ |
| Kartenbaum ist eine hierarchische Liste mit klarer Grafik | reiner TreeView ohne Symbole | Ordner-/Karten-/Startkarten-Icons, Startkarte fett |
| Objektliste zeigt schnell Typ und Zustand | sechs Textspalten ohne Symbole | Typspalte plus Icons für bekannten Typ, unbekannten Typ und Material-Override |

## Grenzen

- Die 3D-Vorschau im Viewport bleibt bestehen; die neue F5-Vorschau ist eine
  kompakte 2D-Silhouette der Prefab-Teile (wie ein Tileset-Vorschaubild), kein
  zweiter Renderer.
- Ein importiertes Bild landet über den vorhandenen, sicheren F3-Import in der
  Materialbase. Der neue Objekt-Typ wird als Kopie des gewählten Prefab-Eintrags
  in `game/data/prefabs.json` angehängt; unbekannte JSON-Felder bleiben erhalten.
- F5 bleibt transaktional: Import und Typ-Anlage sind ausdrückliche
  Ressourcenaktionen; Kartenänderungen werden erst mit OK übernommen.
- Bestehende F5/F6-, Undo-, Dateischutz-, Event-Seiten-, Auswahl- und
  JSON-Datenwege werden nicht umgelegt.

## Prüfungen

- Kopflogik: `prefab_preview_tests`, `prefab_custom_tests`,
  `object_list_view_tests`, `map_tree_icons_tests`, `editor_modes_tests`,
  `placement_form_tests` in `tools/json_test.cpp`.
- Danach: `powershell -ExecutionPolicy Bypass -File .\build.ps1`,
  `build.ps1 -Test`, `ruby tools/test_all.rb --strict`, `ruby tools/lint_repo.rb`.
- Ergebnis (26.11.2026): `build.ps1 -Test` grün mit `1022 Checks, 0 Fehler`,
  `EDITOR-ARBEITSABLAUF: 67 Prüfungen, 0 Fehler`, Ruby `55 OK, 0 fehlgeschlagen`;
  `lint_repo.rb`: `0 Fehler, 2 Warnung(en)` (bekannte Hinweise). Beide EXEs
  wurden danach neu gebaut (`Rune3D.exe`, `RuneEditor.exe`).
- Sichtprüfung der Icons/Vorschau weiterhin nur im echten Windows-Fenster möglich.
