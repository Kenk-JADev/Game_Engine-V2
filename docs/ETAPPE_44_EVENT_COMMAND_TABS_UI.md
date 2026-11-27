# Etappe 44: F6 als drei XP-Registerkarten (Befehlsauswahl)

**Ziel:** Der Event-Editor (F6) bekommt eine Befehlsauswahl wie RPG Maker XP:
**drei Registerkarten**, je **zwei Spalten** und **zwei Abschnitten**, mit allen
**90 Befehlen** in XP-Reihenfolge. Ein Klick wählt den Befehl, „Übernehmen“
stellt ihn im Befehlstyp-Feld des Event-Editors ein. Reine C++/Win32-Oberfläche
auf der portablen Katalog-Schicht aus Etappe 42/43.

**Vorgänger:** Etappe 42 (`docs/ETAPPE_42_EVENT_COMMAND_TABS.md`) hat den Bauplan
(`layout.rb`: drei Tabs, zwölf Abschnitte, 90 Einträge) und die typisierten
Commands geliefert; Etappe 43 (`docs/ETAPPE_43_ENGINE_PORTS.md`) hat die 19 Ports
an die echte Engine gebunden. Bisher gab es im F6-Dialog nur eine flache
ComboBox mit den ~72 bestehenden Event-Strings — kein XP-Tabellenbild.

| Vorher (43) | Jetzt (44) |
|---|---|
| F6: eine ComboBox, ~72 Event-Strings, keine XP-Gruppierung | F6: ComboBox **plus** „XP…“-Button → eigener Dialog mit drei Tabs |
| Bauplan nur in Ruby (`layout.rb`) | Zusätzlich portabler C++-Katalog (`event_command_catalog.h`, generiert) |
| Keine Geometrie für die Befehlsauswahl | `event_command_layout.h`: Spalten/Abschnitte/Zeilen, kopflos geprüft |
| 90 Befehle nur in der Rpg-Schicht ausführbar | 60 davon wählbar und ins Karten-Event übernehmbar; 30 als „vorbereitet“ gekennzeichnet |

## Neue Dateien

```
src/editor/event_command_catalog.h        (generiert, 90 Befehle, ohne Win32)
src/editor/event_command_layout.h         (Geometrie in 96-DPI-Einheiten, ohne Win32)
src/editor/event_command_dialog_win.h     (Win32-Dialog, in editor_main.cpp eingebunden)
tools/gen_event_command_catalog.rb        (liest Rpg::EventCommands::Layout, gibt JSON aus)
tools/gen_event_command_catalog.py        (JSON → C++-Header)
tools/event_command_catalog_test.cpp      (kopfloser Test: 32 Prüfungen)
```

Geändert: `src/editor/editor_main.cpp` (Button `IDEV_XP_PICK`, ComboBox schmaler,
Include des Dialog-Headers), `.github/workflows/ci.yml` (neuer Prüfschritt).

## Katalog reproduzierbar erzeugen

Der Header wird **nicht von Hand gepflegt**. Quelle ist der Bauplan der
Rpg-Schicht; die Pipeline ist reproduzierbar (byte-identisch):

```
node .cache/rbc/rbtest.mjs tools/gen_event_command_catalog.rb 2>/dev/null \
  | python3 tools/gen_event_command_catalog.py
# → "generated 90 commands", schreibt src/editor/event_command_catalog.h
```

`gen_event_command_catalog.rb` läuft im WASI-Runner und gibt den Bauplan als JSON
auf **stdout** aus (das WASI-Dateisystem ist In-Memory, `File.write` erzeugt keine
Host-Dateien). `gen_event_command_catalog.py` liest stdin und schreibt den Header.

## Die drei Registerkarten

Jeder Tab hat zwei Spalten mit je zwei Abschnitten. Anzahl der Befehle je
Abschnitt (verbindlich aus `layout.rb`):

| Tab | Spalte links | Spalte rechts |
|---|---|---|
| **1** Dialog, Flusskontrolle, Variablen & System | Dialog & Eingabe (7), Flusskontrolle (8) | Zustand & Inventar (9), Laufzeit-System (6) |
| **2** Karte, Bewegung, Bilder, Screen & Audio | Karte & Bewegung (10), Übergang & Bildschirm (5) | Bilder & Wetter (6), Audio (9) |
| **3** Kampf, Charakterwerte, Gegner, Screens & Script | Kampf & Interaktion (3), Party & Charaktere (12) | Gegner & Kampfsteuerung (10), System-Screens & Hook (5) |

XP-Codes: Tab 1 = 101–136, Tab 2 = 201–251, Tab 3 = 301–355. Insgesamt 90 Befehle
mit 235 Feldern. Die Feldarten (`EcFieldKind`: Integer, Enum, Key, Ref, Bool,
ActorRef, Number, EnemySlot, String, Color, EventRef, StringList, MoveSteps,
Goods, EquipmentRef) stehen vollständig im Katalog, auch wenn der Win32-Dialog
sie noch nicht als Eingabefelder rendert — sie sind die Grundlage für die
nächste Ausbaustufe (typisierte Parameterformulare je Befehl).

## Bedienung

1. F6 öffnet den Event-Editor. In der Gruppe „Befehle“ steht neben der
   Befehlstyp-ComboBox der Button **„XP…“**.
2. Klick öffnet den Dialog „Befehl wählen“ (780×560 @ 96 DPI, auf die
   Arbeitsfläche des aktuellen Monitors begrenzt und am Hauptfenster zentriert —
   dieselbe portable Rechnung wie beim Datenbank-Dialog).
3. Die Registerleiste (`SysTabControl32`) schaltet zwischen den drei Tabs um;
   jeder Tab baut seine zwei Spalten mit Abschnittsüberschriften und
   Befehlszeilen neu auf.
4. Klick auf eine Befehlszeile wählt ihn; die Hinweiszeile zeigt Code, deutsche
   Beschriftung, XP-Namen und den zugeordneten `cmd`-String.
5. **„Übernehmen“** stellt die ComboBox des Event-Editors auf den passenden
   Event-String und löst `CBN_SELCHANGE` aus (die bestehende Feld-Sichtbarkeit
   `ev_cmd_visibility` läuft dadurch ganz normal). Der Dialog schließt sich.
6. **„Abbrechen“** / Schließen verwirft die Auswahl.

## Ehrliche Grenzen: 60 verdrahtet, 30 vorbereitet

Der Katalog ordnet jedem XP-Typ entweder einen **bestehenden Event-String** zu
(`cmd`, in `EV_TYPES` vorhanden) oder `nullptr`. Von den 90 Befehlen sind
**60 verdrahtet** und **30 vorbereitet**. Vorbereitete Befehle erscheinen im
Dialog mit dem Zusatz „(vorbereitet)“; „Übernehmen“ meldet dann freundlich, dass
die Rpg-Schicht sie führt, das Karten-Event-System aber noch nicht (Verweis auf
`docs/ETAPPE_43_ENGINE_PORTS.md`, Abschnitt „Ehrliche Grenzen“). Sie werden
**nicht** stillschweigend in eine Karte geschrieben.

Vollständig vorbereitet ist der Abschnitt **Gegner & Kampfsteuerung** (alle 10).
Weitere vorbereitete Befehle: Bedingte Verzweigung; Waffen/Rüstungen ändern;
Fenstergrafik, Kampf-BGM, Kampf-End-ME; SE stoppen; Karte scrollen,
Karteneinstellungen, Nebel-Farbton/-Deckkraft, Animation zeigen, Transparenz
setzen, Auf Bewegung warten; Übergang vorbereiten; Ausrüstung/Klasse/Grafik
ändern; Game Over, Zum Titel zurück.

Das ist bewusst so: Etappe 44 liefert die **Auswahl-Oberfläche** über den
vollständigen XP-Bauplan. Die Zuordnung zu ausführbaren Karten-Events wächst mit
den Engine-Ports; der Katalog macht den jeweiligen Stand sichtbar, statt ihn zu
verstecken.

## Tests

Kopflos (Linux/CI, ohne Win32):

```
g++ -std=c++20 -O2 -Wall -Wextra -Werror -Isrc \
  tools/event_command_catalog_test.cpp -o event_command_catalog_test
./event_command_catalog_test          # 32 Prüfungen, 0 Fehler
```

Der Test prüft: 90 Befehle / 3 Tabs / 12 Abschnitte; Abschnittsgrößen exakt nach
Bauplan; je Spalte zwei Abschnitte; XP-Codes je Tab im Bereich; Typen eindeutig
und über `ec_find` auffindbar; 235 Felder gesamt; jede `cmd`-Zuordnung existiert
in `EV_TYPES`; Enum-Felder tragen ihre Auswahlwerte; Geometrie überlappungsfrei
und im Fenster; Fensterhöhe reicht für die höchste Spalte; `ec_dialog_fit` hält
das Fenster in der Arbeitsfläche (auch bei negativen Koordinaten); DPI-Skalierung
rundet wie `MulDiv` (100 @ 144 DPI → 150).

Cross-Kompilat des gesamten Editors (zig, Windows-Ziel):

```
.cache/venv/bin/python -m ziglang c++ -std=c++20 -target x86_64-windows-gnu \
  -c -O1 -DUNICODE -D_UNICODE -DNOMINMAX -DWIN32_LEAN_AND_MEAN \
  -Isrc src/editor/editor_main.cpp -o /tmp/editor_main.obj
```

Bestehende Suiten bleiben grün: `json_test` **1031/0**, `befehle_test` (73 Befehle,
0 Fehler — keine neuen `cmd`-Strings, nur Auswahl über bestehende),
`event_commands_test` **107/0**, `event_commands_arch_test` **1542/0**,
`event_commands_engine_test` **110/0**.

## CI

**CI grün:** Lauf [45997114648](https://github.com/Kenk-JADev/Game_Engine-V2/actions/runs/45997114648)
(Commit `1b3fa44`), alle vier Jobs erfolgreich:

| Job | Ergebnis |
|---|---|
| C++-Logik (JSON-Schicht) | ✓ — inkl. neuem Schritt „XP-Befehlskatalog portabel prüfen (Etappe 44)“ (`-Werror`, 32/0) |
| Repo-Hygiene | ✓ |
| Windows-Build (Rune3D + RuneEditor) | ✓ — `RuneEditor.exe` mit MSYS2-ucrt64-g++ gebaut, neuer Dialog kompiliert; 61 OK/0/0, D3D11-WARP/UI-Smoke |
| Ruby-Engine (Tests + Karten) | ✓ — 60 OK, 0 fehlgeschlagen, 1 übersprungen (nur `tex_import`) |

Der Windows-Job beweist, dass `event_command_dialog_win.h` auch mit der echten
Windows-Toolchain kompiliert (nicht nur im zig-Cross-Kompilat). Er ersetzt trotzdem
keinen interaktiven Sicht-/DPI-Test (siehe unten).

## Sicht-/DPI-Test auf dem Windows-Rechner

Die Sandbox kann den Win32-Dialog nur **cross-kompilieren**, nicht anzeigen.
CI-Grün (auch der Windows-Job mit D3D11-WARP und verstecktem Fenster) ersetzt
**keinen interaktiven GUI-/DPI-Test**. Bitte auf dem echten Rechner prüfen:

- F6 → „XP…“ öffnet den Dialog; drei Tabs sind umschaltbar, je zwei Spalten mit
  Abschnittsüberschriften, keine überlappenden oder abgeschnittenen Zeilen.
- 96 / 120 / 144 / 192 DPI: Beschriftungen lesbar, Buttons erreichbar, Fenster
  bleibt in der Arbeitsfläche (auch bei Monitor links/oberhalb des Hauptmonitors).
- Befehl wählen → „Übernehmen“ stellt die ComboBox richtig ein und die
  Feld-Sichtbarkeit aktualisiert sich; „(vorbereitet)“-Befehle melden die Grenze.
- Mehrzeiliger Tab-Umbruch bei schmalem Fenster / hoher DPI (`TCS_MULTILINE` ist
  gesetzt; die Registerbeschriftungen sind lang).
