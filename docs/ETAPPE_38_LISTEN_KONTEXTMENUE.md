# Etappe 38: Event-/NPC-Liste, Kontextmenü und UI-Aufräumung

Stand: Code gebaut und getestet; interaktive Sichtprüfung offen. Diese Etappe
baut auf Etappe 37 auf und ersetzt keine F5/F6-, Undo-, Dateischutz-,
Event-Seiten- oder JSON-Datenwege.

## 1. Recherche vor der Implementierung

XP-Verhalten wurde vor dem Code erneut geprüft:

- RPG Maker MZ, Eventliste: Auswahl in der Liste markiert das Event auf der
  Karte, Doppelklick öffnet den Event-Editor
  <https://rmmz.neocities.org/01_09_02>. Das ist die nächste dokumentierte
  Entsprechung zur gewünschten „hinporten“-Navigation; XP selbst zeigt Events
  als Marker auf der Ereignisebene, nicht als eigenes Listenfenster.
- Offizielle XP-Hilfe „Events“: Map Events sind die Ereignisse auf der Karte,
  Doppelklick auf der Karte erzeugt/öffnet sie; Common Events und Battle Events
  sind getrennt
  <https://www.rpg-maker.fr/dl/monos/aide/xp/source/rpgxp/event.html>.
- Offizielle XP-Hilfe „Database“: F9/Datenbank ist ein eigener Dialog mit
  Reitern und Rechtsklick-/Listenoperationen
  <https://www.rpg-maker.fr/dl/monos/aide/xp/source/rpgxp/database.html>.
- Offizielle XP-Hilfe „Script editor“: Rechtsklick im Skripteditor steuert
  Neu/Löschen/Kopieren/Einfügen
  <https://www.rpg-maker.fr/dl/monos/aide/xp/source/rpgxp/script.html>.
- RGSS-/Ruby-Hintergrund: <https://gdu.one/forums/topic/8392-dubealex%E2%80%B2s-rgss-and-ruby-tutorials>,
  <https://eltrasterorpg.blogspot.com/2010/04/tutorial-rpg-maker-xp-parte-1.html>.

### XP-Befund → Rune3D-Entscheidung

| XP/Befund | Umsetzung in Etappe 38 |
|---|---|
| Eventauswahl in einer Liste markiert die Karte; Doppelklick öffnet den Event-Editor | Neue Liste „Events / NPCs“ im Inspektor mit Zählern. Klick setzt Auswahl und richtet die Kamera auf die gespeicherte Position; Doppelklick öffnet F6 bei Events und F5 bei NPCs. |
| Ein Event ist ein einzelner Map-Eintrag mit mehreren Seiten | Es gibt weiterhin genau einen Event-Editor pro markiertem Event; kein globales Event-Menü. Die neue Liste ist nur Navigator. |
| F9/F11/F5/F6 sind in XP funktional, aber in Rune3D schon als Toolbar-Symbole und F-Tasten vorhanden | Menüs „Events“, „Scripts“, „Datenbank“ und „Werkzeuge“ entfernt. F5/F6/F9/F11 und die Toolbar bleiben; der Grafik-Menüpunkt bleibt, weil Import/F3/F7 dort gebündelt sind. |
| Objektaktionen gehören an Kontextmenüs, Drehen bleibt ein direktes Werkzeug | „Objekt-Aktionen“, „Löschen“ und „Duplizieren“ aus der Palette entfernt; Rechtsklickmenü von Objektliste/Viewport übernimmt sie. „Drehen (R)“ bleibt sichtbar. |
| Fehlerhinweise gehören nahe ans Editorfenster | Kleiner „!“-Knopf oben rechts im Clientbereich, direkt unter dem Windows-Schließen-Symbol. Klick öffnet den vorhandenen F10-Problemedialog. |
| Materialien kommen aus Projektordnern | F5-Materialliste zeigt `ordner/name` aus `MaterialInfo.file` bzw. `.source`; gespeichert wird weiterhin der reine Materialname. Palettengruppe heißt jetzt „Objekt-Material“. |
| „Neue Welt“ ist kein eigenes Editor-Konzept mehr | Default-Name und leerer Name erzeugen „Neue Karte“. Die Datei-ID `welt01` bleibt aus Kompatibilitätsgründen erhalten. |

## 2. Bildbefund und Grenzen

Der Nutzer hat drei Bilder angehängt. In dieser Sitzung konnte nur ein Bild
(284×487) aus der Zwischenablage gelesen werden; dieses Modell kann Bilder
nicht anzeigen. Windows-OCR lieferte daraus nur:

> Ob'ekt-T Haus Hütte Zaun (2 m) Baum Fels Laterne Kristall Brunnen Weqstück Prefab

Deshalb werden keine genauen Pixelpositionen der drei Bilder behauptet. Umgesetzt
wurden die eindeutigen Textanforderungen. Die tatsächliche Anordnung ist weiter
per Sichtprüfung am laufenden Editor zu bestätigen; ein zweites/tertes Bild war
nicht verfügbar.

## 3. Codebereiche

- `src/editor/event_npc_list.h` (neu): reine Zähler-, Zeilen- und LPARAM-Kodierung.
- `src/editor/material_document.h`: `material_display_folder()` /
  `material_display_name()`.
- `src/editor/main_layout.h`: Palette ohne Werkzeugradios/Aktionen, Inspektor
  mit zweiter Liste.
- `src/editor/editor_main.cpp`: Menübereinigung, Listenbau, Fokus-/Doppelklick-
  Nachrichten, Fehlerindicator, Default-Name, `mat_all_display()`.
- `src/editor/placement_dialog.h`: F5-Beschriftungen und ordnerqualifizierte
  Materialanzeige.
- `tools/json_test.cpp`: neue Logiktests und angepasste Geometrietests.

## 4. Bewahrte Wege

- F5 öffnet weiterhin den Platzierdialog; F6 den Event-Editor; F9/F11/F10/F1
  bleiben per Taste und Toolbar erreichbar.
- Undo/Redo arbeiten weiter mit vollständigen Karten-Snapshots.
- Das Karten-JSON behält unbekannte Felder; Materialwerte werden nicht durch
  Anzeigelabels ersetzt.
- Eventseiten, Befehle, NPCs, Spielerstart, Import und Dateischutz sind
  unverändert.

## 5. Prüfung (tatsächlich ausgeführt)

- `powershell -ExecutionPolicy Bypass -File .\build.ps1 -Test`: **ALLE TESTS
  GRÜN**; `1028 Checks, 0 Fehler`. Enthalten: Editor-Arbeitsabläufe 67/0,
  Terrain 51/0, Material 57/0, Dokumenttransaktionen 58/0, Windows-Pfade 39/0,
  WAV 18/0, OGG/MP3 ohne Fehler, Ruby 55 OK, JSON-Brücke 15 Dateien ohne
  Abweichung, Event-Brücke 19/0.
- `ruby tools/test_all.rb --strict`: **55 OK, 0 fehlgeschlagen, 0 übersprungen**.
- `ruby tools/lint_repo.rb`: **0 Fehler, 2 Warnungen** (bekannte LF-Datei
  `src/Rune3D.exe.manifest` und offene TODO/FIXME-Markierungen in Vendor-/Doku-
  dateien; keine Etappe-38-Datei betroffen).
- `powershell -ExecutionPolicy Bypass -File .\build.ps1`: beide EXEs neu
  gelinkt (`build\Rune3D.exe` 1332,4 KB, `build\RuneEditor.exe` 2650,9 KB).
  Die bekannten `-Wcast-function-type`-Warnungen aus
  `src/core/project_assets_win.h` sind nicht fatal. Keine `*.old.exe` vorhanden.
- `powershell -ExecutionPolicy Bypass -File .\tools\windows_runtime_smoke.ps1`:
  isolierter Start beider EXEs und echter Editor-UI-Smoke
  (**36 Prüfungen, 0 Fehler**). Damit bleiben Maus-/Doppelklick-, Kontextmenü-
  und F11-Abläufe portabel geprüft. Kein Ersatz für die weiterhin offene
  Sichtprüfung mit Hardware-GPU und echten Dia/DPI-Wechseln.

Offen: interaktive Windows-/DPI-/GPU-Sichtprüfung der genauen Bildanordnung.
