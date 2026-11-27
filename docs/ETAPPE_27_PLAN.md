# Etappe 27 – Kontextabhängige Arbeitsbereiche (Plan)

Voraussetzung: Etappe 26 (Kartenbaum oben, Palette darunter) gebaut und getestet.

## Ziel
Der Inspektor rechts zeigt je nach Auswahl einen passenden Arbeitsbereich,
ohne F5/F6, Undo, Formularentwürfe oder Dateischutz zu duplizieren.

| Kontext | Auslöser | Inhalt rechts | Weiterleitung |
|---|---|---|---|
| Karte | keine Auswahl | Kartenname, Größe, BGM, Begegnungen (Lesesicht) | F2 Karteneigenschaften |
| Objekt | Prefab/Objekt gewählt | Position, Rotation, Skalierung (bestehende 4 Felder) | Undo-Pfad unverändert |
| Event | Objekt mit Event-Seiten | Seitenanzahl, Auslöser, Bedingungen, Befehlszahl | F5 öffnet bestehenden Event-Dialog |
| NPC | Event mit Figur/Bewegungsroute | Grafik, Bewegungsart, Dialogvorschau | F6 Befehlsformular |

## Umgesetzt: 27a
`src/editor/inspector_context.h` (Kontext Karte/Objekt/ungültig, Titel,
Übersicht); `sync_property_fields` setzt Gruppentitel und Kartenhinweis.
Korrektur gegenüber dem ursprünglichen Plan: Events/NPCs sind eigene
Arrays der Karte (`events`, `npcs`) und nicht in der 3D-Objektauswahl;
der Event-/NPC-Kontext folgt daher erst mit 27c (Marker-Auswahl).
Zu prüfen: Die Zahlen aktualisieren sich bei jedem Auswahl-/Listenabgleich;
ob F5-Änderungen sofort erscheinen, ist noch nicht interaktiv geprüft. Gesamtplan:
`docs/STUFENPLAN_EDITOR_UND_XP_SCRIPTS.md`.

## Schritte (je einzeln testbar)
1. **Reine Logik** `src/editor/inspector_context.h`: `InspectorContext
   inspector_context_for(const Selection&, const MapData&)` ohne Win32.
   Tests in `tools/json_test.cpp`: leere Auswahl → Karte, Objekt ohne
   Event → Objekt, Event mit Grafik+Route → NPC, Mehrfachauswahl → Objekt.
2. **Layout** `main_inspector_layout(width, height, dpi, context)`: Feldzahl je
   Kontext; Geometrietests wie bisher (100–200 % DPI, keine Überlappung).
3. **Anbindung** in `editor_main.cpp`: Kontextwechsel nur nach erfolgreichem
   Übernehmen bzw. Verwerfen offener Entwürfe (bestehende Rückfrage nutzen).
4. **Knöpfe** „Event bearbeiten (F5)“ / „Befehle (F6)“ rufen exakt die
   vorhandenen Menübefehle auf (`WM_COMMAND`), keine eigenen Dialoge.
5. **Doku/Tests**: `WINDOWS_MAIN_LIVE_TEST.md`, `architecture.md`,
   `XP_VERGLEICH.md` (nur teilweise Punkte), `test_all.rb --strict`.

## Nicht-Ziele
Kein 2D-Kachel-Editor, keine neuen Befehlstypen, keine Änderung des
Map-JSON-Formats.

## Offene Diagnose
Schwarze Streifen im Screenshot: erst mit Viewport-/Swapchain-Logs
(`ResizeBuffers`-Größe vs. Client-Rechteck des Viewports) vermessen, bevor
eine Ursache benannt wird.
