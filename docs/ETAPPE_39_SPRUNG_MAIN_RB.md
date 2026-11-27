# Etappe 39: Sprungsteuerung und `Main.rb` als Systemeinstieg

Stand: Code und Tests. Diese Etappe trennt den **manuellen Spielersprung** von
dem **Move-Route-Sprung für Event-/Cutscene-Abläufe** und macht `089 - Main`
aus `data/scripts.json` zum echten Ruby-Systemeinstieg – ohne die C++-Taktung
zu blockieren.

## XP-Recherche (vor der Umsetzung)

| Befund | Quelle |
|---|---|
| Der Move-Route-Befehl `jump` ist XP-Code 14; `Game_Character` ruft `jump(parameter[0], parameter[1])` mit zwei Offsets. In MV/MZ sind die Werte −100…100; die Bewegung geht durch Hindernisse und wird ggf. mit „Wait for Completion“ synchronisiert. | Official MV-Hilfe <https://rpgmakerofficial.com/product/MV_Help/page/01_10_06_01.html>, MZ-Hilfe <https://rpgmakerofficial.com/product/MZ_help-en/01_10_06_01.html>, XP-Defaultscript `019 - Game_Character 2.rb` (Code 14) und `020 - Game_Character 3.rb` (`jump`) im Repo |
| Cutscenes nutzen „Set Move Route“ plus „Wait for Completion“; der Sprung ist damit ein bewusst gesetzter Event-Schritt, keine freie Spieleraktion. | RPG Maker Web „Cutscene Basics“ <https://www.rpgmakerweb.com/blog/cutscene-basics>, RMRK <https://rmrk.net/index.php?topic=45431.0> |
| Skripte sind Abschnitte, laufen von oben nach unten; der letzte Abschnitt `Main` startet das Spiel. XP-`Main` ist `while $scene != nil; $scene.main; end`. | Offizielle XP-Skripthilfe <https://www.rpg-maker.fr/dl/monos/aide/xp/source/rpgxp/script.html>, `XP-Defaultscript/089 - Main.rb`, <https://zenn.dev/shimat/articles/9f26e52dbf7884>, <https://vectree.io/c/rgss> |

## Umsetzungsentscheidungen

### 1. Sprungsteuerung

- Neues Kartenfeld `"jump_allowed": false` auf Wurzelebene (echtes JSON-Bool).
  Fehlender Schlüssel bedeutet `false`; alte Karten bleiben gültig. Der Name
  `jump` wäre mit dem Event-Befehl `jump` (Sprung zu einer Marke) kollidiert.
- F2, Reiter **Allgemein**: Checkbox **„Springen erlauben (manuell)“**. Nur
  geänderte Felder werden geschrieben; unberührte Kartendaten und Undo bleiben
  wie in Etappe 38 erhalten.
- Die C++-Physik ist autoritativ für Leertaste/Space. `Physics::jump_allowed`
  (Default `false`) blockiert den manuellen Sprung **vor** der Impulsvergabe;
  `Rune::Physics.jump_allowed=` wird beim Kartenbetreten aus dem JSON gesetzt.
  Der Sprung-Sound in `game/main.rb` fragt denselben Flag ab, damit kein Sound
  ohne Bewegung entsteht.
- Move-Route-Sprünge sind davon unabhängig: Sie bleiben der Weg für Event-
  und Cutscene-Abläufe und führen jetzt eine echte Positionsänderung aus.
  Schreibweise: `"jump:dx:dz"` oder `"14:dx:dz"` (XP-Code 14), ganzzahlige
  Offsets −100…100 wie in XP. Ein nacktes `"jump"` bleibt aus Kompatibilitäts-
  gründen ein reiner Partikeleffekt und erhält eine MapCheck-Warnung.
- Wie in XP ignoriert der Route-Sprung Block-Zonen/Hindernisse; er ist eine
  bewusste Regieaktion, kein Physiksprung. Landung und Kamerafolge nutzen die
  normale `gy()`-Höhe.

### 2. `Main.rb` aus `data/scripts.json`

- `089 - Main` bleibt `autoload: false` und landet weiterhin **nicht** in
  `@startup` (`XP_BLOCKED = [89]`). `Scripts.load_project` merkt sich den Code
  zusätzlich als `@main_code`.
- `Scripts.start_main` wertet diesen Code in einem Fiber aus (Top-Level, damit
  `Scene_Title`/`Scene_Map` die Engine-Klassen sind). Er startet beim Setup und
  bei F5 neu; `Scripts.tick_main` resume't ihn genau **einmal pro Frame** aus
  `Rune.tick`.
- Die blockierende XP-Schleife wird nicht umgeschrieben: `Scene_Base#main` und
  `Graphics.update` enthalten einen **guarded Yield** (`Fiber.yield` nur, wenn
  der Main-Fiber wirklich läuft). Damit gibt jedes `$scene.main` pro Frame an
  den C++-Takt zurück; `FiberError` im Root-Fiber ist ausgeschlossen.
- Während der Main-Fiber aktiv ist, ruft `Game.update` nicht zusätzlich
  `Graphics.scene_update`/`Graphics.update` auf – sonst gäbe es doppelte
  Szenen-Updates und doppelte Frame-Zählung.
- Ergänzend: kleine `Scene_Map` als Repräsentation der Spielszene (die 3D-Welt
  selbst bleibt in `Game.update`), `Scene_Title.new` übernimmt `$project_name`
  als Default, damit XP-`Scene_Title.new` ohne Argumente funktioniert.
- Fehler im Main-Fiber laufen über `Scripts.startup_error`/`show_error_window`
  in dasselbe XP-Format wie Start-Scriptfehler; der C++-Loop bleibt lauffähig.
- Keine 1:1-Kopie der XP-Originalskripte: Es wird nur die dokumentierte
  Reihenfolge/Architektur nachgebaut.

## Prüfung

- `powershell -ExecutionPolicy Bypass -File .\build.ps1 -Test`
- `ruby tools/test_all.rb --strict`
- `ruby tools/lint_repo.rb`
- `powershell -ExecutionPolicy Bypass -File .\tools\windows_runtime_smoke.ps1`
- Beide EXEs neu bauen; vorher `RuneEditor.exe`/`Rune3D.exe` schließen.

## Offen / bewusst nicht verändert

- `089` ist im Editor weiterhin gesperrt; Bearbeitung erfolgt über den
  Script-Editor nur an aktiven Einträgen bzw. durch importierte Defaults.
- Ein Projekt ohne `089 - Main` läuft unverändert mit der bisherigen
  Frame-Konvention (`Graphics.scene_update` aus `Game.update`).
- Die XP-Defaultszenen im Namensraum `XP` bleiben Definitionen; der Main-Fiber
  benutzt die Engine-Szenen, nicht `XP::Scene_Title`.
