# Recherche: Aufbau des RPG-Maker-XP-Editors (Etappe 29)

Arbeitsregel ab jetzt: Vor jeder Editor-Etappe den XP-Original-Ablauf
nachschlagen und hier festhalten; Rune3D übersetzt ihn in 3D, statt einen
2D-Kachel-Editor nachzubauen.

## Quellen
- RPGA „RPG Maker XP Kurs Teil 1“ (rpga.info/grundlagenkurs1.pdf, dt.)
- GDU-Forum „Lesson 2 – Demystifying the Layers“, „How to Make a Simple Map“
- Instructables „Create a Video Game With RPG Maker XP“
- Steam-Guide „RPG Maker XP guide for beginners“
- El Trastero RPG „Tutorial RPG Maker XP 1 – Conociendo la interfaz“ (Menüs)
- Scripts: Makerpendium „Scripts.r?data“, Save-Point, Essentials-Wiki (s. Etappe 28b)

## XP-Hauptfenster
| Bereich | XP | Rune3D heute | Abstand |
|---|---|---|---|
| links oben | Tileset-Palette (Kacheln wählen) | Werkzeuge + Prefab-Palette | ✅ Position (Etappe 29) |
| links unten | Kartenbaum (Rechtsklick: Eigenschaften, Kopieren/Einfügen, Unterkarten) | Kartenbaum mit Neu/F2 | ✅ Position; Kopieren/Einfügen von Karten offen |
| rechts | große Kartenfläche | 3D-Ansicht (Mitte) + Inspektor rechts | bewusst 3D |
| Toolbar | Neu/Öffnen/Speichern · Ausschneiden/Kopieren/Einfügen/Rückgängig · **Ebene 1–3 + Ereignisebene** · Stift/Rechteck/Ellipse/Füllen/Auswahl · Zoom 1/1, 1/2, 1/4 · Datenbank · Material · Script-Editor · Sound-Test · Testspiel | gruppierte Icons, F-Tasten-Dialoge | Modus-/Zeichengruppen fehlen |
| Menüs | Datei · Bearbeiten · Ansicht · **Modus** · **Zeichnen** · **Maßstab** · Werkzeuge · Spiel · Hilfe | Datei · Karte · **Modus** · **Maßstab** · Events · Scripts · Grafik · Datenbank · **Werkzeuge** · Ansicht · Hilfe (Etappe 30) | „Bearbeiten“ (Rückgängig/Kopieren) und „Zeichnen“ fehlen noch |

## Grafik-Leitlinien (Etappe 31)
- Icons klein, klar, mit transparentem Hintergrund; XP-Bildsprache wo
  sinnvoll: Ereignisebene = **blauer Würfel**, Ebenen = einrastende Gruppe.
- Keine ausgegrauten Pseudo-Menüeinträge als Hilfetext; Hilfe gehört in F1
  bzw. Tooltips.
- Einheitliche Abstände über `main_layout_scale` (DPI); kein Bereich darf
  Controls abschneiden.
- Noch zu prüfen: schwarze Streifen im Viewport (erst messen), Farben der
  Seitenleisten, Schriftgrößen der Dialoge F2/F7/F9/F11.

## XP-Ebenen → Rune3D-Modi (Vorschlag Etappe 30)
| XP | Kürzel XP | Rune3D-Übersetzung |
|---|---|---|
| Ebene 1 (Boden) | F5 | Modus **Gelände**: Boden malen + Höhe |
| Ebene 2/3 (Objekte) | F6/F7 | Modus **Objekte**: Prefabs platzieren/verschieben |
| Ereignisebene | F8 | Modus **Events**: Event-/NPC-Marker, Doppelklick = Event bearbeiten |

Achtung: Rune3D belegt F5/F6 bereits mit Dialogen (NPCs/Objekte, Event-
Editor). Die XP-Kürzel werden NICHT umgelegt, um bestehende Abläufe nicht zu
brechen; die Modi bekommen ein eigenes Menü „Modus“ und Toolbar-Knöpfe.

## Event-Bedienung im XP (Recherche Etappe 33)
Quellen: WikiHow „Create a Game in RPG Maker XP“ („select the events layer.
Double click a tile“), Instructables, Steam-Guide (Rechtsklick „New Event“,
„Player's Starting Position“), offizielle RPG-Maker-MV/MZ-Hilfe „Map Event
Creation and Controls“ (gleiche Bedienung, ausführlicher dokumentiert).

| XP | Rune3D (Stand Etappe 33) |
|---|---|
| Modus Ereignisebene | Menü/Toolbar „Modus Events“ |
| Doppelklick auf Event öffnet Event-Editor | ✅ schon vorhanden (Marker-Doppelklick → F6 bzw. NPC → F5) |
| Doppelklick auf freies Feld = neues Event | ✅ Etappe 33 (nur Modus Events) |
| Klick daneben platziert nichts | ✅ Etappe 33 (Modus Events setzt/malt nie) |
| Kleine Marker gut treffbar | ✅ Etappe 33: Trefferrand 0,45 m im Modus Events |
| Ziehen verschiebt das Event | ✅ Etappe 34 (Modus Events oder Verschieben) |
| Rechtsklick: Bearbeiten/Neu, Kopieren, Einfügen, Löschen, Startposition | teilweise (Viewport-Kontextmenü hat Startposition/Objekt); Event-Einträge offen |
| Kartenbaum: Klick wechselt Karte | ✅ vorhanden (mit Rückfrage bei ungespeicherten Änderungen) |
| Kartenbaum: Karten per Ziehen umhängen | offen (heute Tab/Shift+Tab bzw. Kontextmenü Ein-/Ausrücken) |

## XP-Arbeitsabläufe, die übernommen werden sollen
1. **Ereignisebene: Doppelklick auf ein Feld** legt ein Event an bzw. öffnet
   es → Rune3D: Doppelklick in 3D im Modus Events (Etappe 27c).
2. **Kartenbaum-Rechtsklick**: Eigenschaften, Neue Karte (als Unterkarte),
   Kopieren/Einfügen, Löschen.
3. **Datenbank F9** als ein Dialog mit Reitern (13 Abschnitte in XP).
4. **Testspiel** aus der Toolbar (XP: F12).
5. **Maßstab**: in 3D als Kamera-Abstand-Stufen.

## Etappe 37: Werkzeug-Icons und Listen (umgesetzt)

Quellen wie in `docs/ETAPPE_37_SYMBOLE_LISTEN.md` (RPGA-Kurs, El Trastero,
offizielle XP-Materialdefinition, Steam-Guide). Ergänzender Befund:

- XP trennt **Ebenenknöpfe** und **Zeichenwerkzeuge**; beide sind echte
  Zustandsknöpfe mit Icon. Rune3D hat jetzt fünf Werkzeug-Icons in der Toolbar,
  die mit Palette und Modusmenü gekoppelt sind.
- Das **Auswahlwerkzeug** ist in XP keiner Objektebene zugeordnet. Rune3D
  behält daher die aktive Ebene; nur Setzen/Verschieben ⇒ Objekte, Boden/Höhe
  ⇒ Gelände.
- Die **Tileset-Datenbank** zeigt eine Grafikvorschau und erlaubt eigene
  Grafiken. Rune3D zeigt in F5 eine kompakte 2D-Silhouette der Prefab-Teile,
  eine Materialliste, den bestehenden sicheren Import und „Neuer Typ aus
  Material“.
- **Kartenbaum** und **Objektliste** bekommen Icons, Startkarte fett, sowie
  Typ- und Materialspalte.

Grenze: Die neue F5-Vorschau ist bewusst kein zweiter 3D-Renderer; der
vorhandene Viewport-Geist bleibt bestehen.

## Etappe 38: Event-/NPC-Liste und UI-Bereinigung (Recherche)

Vor dem Code geprüft:

- MZ-Eventlistenverhalten: Liste markiert Event auf der Karte, Doppelklick
  öffnet den Event-Editor <https://rmmz.neocities.org/01_09_02>. XP selbst
  zeigt Events als Marker auf der Ereignisebene, nicht als Liste.
- XP-Hilfe „Events“: Map Events sind Kartenereignisse; Doppelklick auf der Karte
  erzeugt/öffnet sie; Common/Battle Events sind getrennt
  <https://www.rpg-maker.fr/dl/monos/aide/xp/source/rpgxp/event.html>.
- XP-Hilfe „Database“ und „Script editor“: eigene Dialoge mit Rechtsklick- und
  Listenoperationen
  <https://www.rpg-maker.fr/dl/monos/aide/xp/source/rpgxp/database.html>,
  <https://www.rpg-maker.fr/dl/monos/aide/xp/source/rpgxp/script.html>.

Entscheidungen: F5/F6-Funktionen bleiben per Taste/Toolbar; doppelte Menüs
„Events“, „Scripts“, „Datenbank“, „Werkzeuge“ entfallen. Objektaktionen wandern
in Rechtsklick, Drehen bleibt sichtbar. Materialien werden mit Projektordner
angezeigt, aber unverändert gespeichert. Details:
`docs/ETAPPE_38_LISTEN_KONTEXTMENUE.md`.

## Etappe 39+: Datenbank, Werkzeuge, Tileset und RGSS (Recherche)

Quellen, vor der Implementierung geprüft:

- Offizielle XP-Datenbankreferenz
  <https://www.rpg-maker.fr/dl/monos/aide/xp/source/rpgxp/database.html>
- `RPG::Tileset`-Referenz
  <https://www.rpg-maker.fr/dl/monos/aide/xp/source/rgss/gc_rpg_tileset.html>
- XP-Scripteditor-Referenz
  <https://www.rpg-maker.fr/dl/monos/aide/xp/source/rpgxp/script.html>
- GDU „Dubealex’ RGSS and Ruby Tutorials“
  <https://gdu.one/forums/topic/8392-dubealex%E2%80%B2s-rgss-and-ruby-tutorials>
- Makerpendium „Scripteditor“ <http://www.makerpendium.de/wiki/Scripteditor>
- Maker-Studio „Tileset editor“
  <https://github.com/Toskan4134/maker-studio/blob/main/docs/tileset-editor.md>
  (Vergleich, nicht XP-Primärquelle)

### XP-Befund → Rune3D-Übersetzung

| XP-Verhalten | Rune3D-Plan (Etappe 39+) |
|---|---|
| F9 **Datenbank** ist ein Dialog mit 13 Reitern: Actors, Classes, Skills, Items, Weapons, Armors, Enemies, Troops, Terrain, States, Animations, Tilesets, System. Listen mit Neu/Löschen, Doppelklick bearbeitet, Pfeile sortieren. | Ein F9-Dialog mit denselben Reitern und denselben Listen-/Bearbeitungsregeln; vorhandene JSON-Datenwege und Rückfragen bleiben erhalten. |
| XP zeichnet auf Ebenen 1–3; es gibt **keine 3D-Höhe**. Der Nutzer wünscht „Block hoch/runter, groß/klein“. | Übersetzung in den vorhandenen Höhenmodus: Pinselgröße plus Hoch/Runter pro Strich, ein Undo pro Strich. Kein neues, widersprüchliches Höhensystem. |
| Das XP-Auswahlwerkzeug zeigt eine rechteckige Auswahl. Der Nutzer wünscht sichtbare X/Y-Pfeile, Drehkreis und Z-Pfeil. | 3D-Transform-Gizmo am gewählten Objekt: X/Y-Verschieben, Drehen um die Hochachse, Z-Pfeil für die Höhe. Auswahl bleibt neutral gegenüber der aktiven Ebene. |
| **Tileset-Editor**: Grafik 8 Kacheln breit aus `Graphics/Tilesets`; eigene Grafiken und Autotile-Namen werden im Tileset-Eintrag referenziert. | Tileset-Reiter mit Grafikvorschau im 8-Spalten-Raster, Auswahl, eigener Grafik und sichtbaren Autotile-/Terrain-Feldern. |
| **F11-Scripteditor**: nummerierte Liste, Rechtsklick Neu/Löschen/Kopieren/Einfügen, Suche/Ersetzen, Syntaxfarben für `def`, `if`, Klassen, Kommentare und Strings. | Nummerierte Liste, Suche und Kontextmenü stammen aus Etappe 36; Etappe 43 ergänzt die Syntaxfarben. |
| RGSS-Standardskripte sind Teil von XP; der Nutzer braucht ihre Struktur später für die Engine. | Keine 1:1-Kopie urheberrechtlich geschützter Originalquellen. Umgesetzt werden eigene, funktional gleichwertige Grundgerüste in der dokumentierten Reihenfolge/Architektur (S1–S8). `089 - Main.rb` bleibt im Editor gesperrt, läuft seit Etappe 39 aber als frame-sicherer Fiber-Einstieg aus `data/scripts.json`. |

Die XP-Originalskripte werden nicht wörtlich übernommen. Maßgeblich sind
Klassen-/Modulnamen, Reihenfolge und Verhalten, damit die spätere Engine die
eigenen Gerüste ansprechen kann.
