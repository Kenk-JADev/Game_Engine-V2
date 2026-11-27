# Etappe 28 – XP-Defaultscripts im Script-Editor, Laden vor Spielstart, Fehlerfenster

Status: **Code geschrieben, noch nicht gebaut/getestet.**

## Verhalten
1. **Script-Editor (F11)** zeigt die 90 XP-Defaultscripts als normale Einträge
   der `data/scripts.json`, oben in Nummernreihenfolge, danach die Projekt-Scripts:
   `{"name":"000 - Game_Temp","xp_default":"000","autoload":true,"code":"…"}`.
   Keine C++-Änderung am F11-Dialog nötig; unbekannte Schlüssel bleiben erhalten.
2. **Neue Projekte** (Editor, `create_project`) bekommen die Liste automatisch
   (`src/editor/xp_default_scripts.h`, Quelle: `<root>/XP-Defaultscript/`).
   **Bestehende Projekte:** `ruby tools/xp_scripts_import.rb projekte/dorf --write`
   (Vorschau ohne `--write`, legt `scripts.json.bak` an, idempotent).
3. **Laden vor dem Spielstart:** `Scripts.run_startup` (Titel und Neues Spiel)
   lädt alle aktiven Einträge in Listenreihenfolge: XP-Einträge per
   `XP.module_eval`, Projekt-Scripts auf oberster Ebene (seit 28b).
4. **Fehlerfenster** (seit 28b: erster Fehler stoppt das Laden, siehe unten)
   in einem modalen Fenster (`Rune::App.error_box`, `src/ruby/vm.cpp`)
   mit Script-Name, Zeile, Fehlerklasse, Meldung und Backtrace (nur Script-
   Frames). Derselbe Fehler wird beim Übergang Titel → Spielstart nicht
   doppelt gezeigt. Zusätzlich Log + F10-Issue wie bisher.

## Etappe 28b – an die echte RPG-Maker-XP-Logik angeglichen
Recherche (Makerpendium „Scripts.r?data“, Save-Point, Essentials-Wiki,
rpgmaker-xp-mcp/WISDOM.md): XP speichert die Scripts als **eine geordnete
Liste** `[id, Name, zlib-Code]` in `Scripts.rxdata`. Der Spielstart wertet
jeden Eintrag **in Listenreihenfolge** auf oberster Ebene aus; die Datei heißt
im Backtrace `Section###`. Eigene Scripts gehören **über Main**, Main ist der
letzte Eintrag und startet `$scene.main while $scene != nil`. Ein
unbehandelter Fehler zeigt `Script 'Name' line N: Klasse occurred.` und
beendet das Spiel; Community-Scripts bilden `Section###` auf Namen ab.

Umsetzung in Rune3D:
- **Eine Startliste** in Script-Editor-Reihenfolge (XP-Defaults und Projekt-
  Scripts gemischt, nicht mehr „erst Defaults, dann Rest“).
- Dateiname beim Auswerten `Section###`; Meldung und Backtrace zeigen
  stattdessen den Scriptnamen.
- **Erster Fehler bricht das Laden ab** (folgende Einträge laufen nicht).
  Verhaltensänderung gegenüber vorher, `tools/script_test.rb` angepasst.
- Meldung: `Script 'Name' Zeile N: NameError ist aufgetreten.` + Meldung +
  Backtrace.
- Import und neue Projekte: 000–088 oben, eigene Scripts darunter, 089 Main unten.

Bewusste Abweichungen von XP: Namensraum `XP` für Defaults; Main läuft nie;
das Spielfenster bleibt nach der Meldung offen (F11 korrigieren, F5 neu
laden), statt sich wie XP zu beenden; Haken „beim Spielstart ausführen“ als
Ein/Aus pro Eintrag (in XP nur per Auskommentieren möglich).

## Bewusste Grenzen
- **Namensraum `XP`:** Die XP-Klassen heißen `XP::Game_Party`, `XP::Scene_Map` …
  Sonst würden `Interpreter`, `Scene_Title`, `Scene_File`, `Scene_Gameover`
  der Engine überschrieben und `Window_Selectable` scheitert an
  „superclass mismatch“. Eigene Scripts sprechen sie mit `XP::` an.
- **Standardmäßig deaktiviert** (Haken „beim Spielstart ausführen“ aus):
  023–026, 029–062 (brauchen `RPG::Sprite`/Window-Verhalten, Stufen S4/S7)
  und **089 Main** – Main läuft auch mit Haken nie (eigene Hauptschleife).
  Wer die anderen aktiviert, sieht fehlende APIs im Fehlerfenster.
- Geladen heißt: **Klassen sind definiert.** Die Engine benutzt sie nicht als
  Spielablauf (`$scene`, `$game_*` bleiben die Rune3D-Wrapper). Das ist Stufe S6+.

## Prüfung (offen)
- `ruby tools/xp_scripts_test.rb`, `ruby tools/script_test.rb`, `test_all.rb --strict`
- Build beider EXEs; im Editor neues Projekt → F11 zeigt 90 Defaults.
- Spiel starten mit absichtlichem Fehler in einem Script → Fenster mit Backtrace,
  OK → Spiel läuft weiter, Maus bedienbar.
