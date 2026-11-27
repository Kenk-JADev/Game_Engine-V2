# Etappe 45: XP-Einstellfenster je Befehl, nummerierte Listen, Asset-Ordnerwahl, XP-Befehlsliste

**Ziel:** Der Event-Editor (F6) bedient sich wie RPG Maker XP:
1. **Jeder Befehl bekommt ein eigenes Einstellfenster** mit genau seinen Feldern —
   eingestellt wird über passende Bedienelemente, nicht über rohe Freitextfelder.
2. **Schalter und Variablen werden über eine nummerierte Liste 1..999 gewählt**
   (mit Namen), nicht eingetippt. Alle anderen Felder likewise: Datenbank-Auswahl,
   feste Combos, An/Aus, geprüfte Zahlen, Mehrzeiler.
3. **Audio und Bilder werden aus einem Ordner gewählt** („Durchsuchen…“); eine
   extern gewählte Datei wird in den Projekt-Asset-Ordner kopiert.
4. **Die Befehlsliste ist XP-artig bedienbar:** anklicken = auswählen,
   Doppelklick = Bearbeiten (Einstellfenster), Rechtsklick = Menü
   (Bearbeiten… / Einfügen… / Löschen).

**Vorgänger:** Etappe 44 (`docs/ETAPPE_44_EVENT_COMMAND_TABS_UI.md`) hat die
Befehlsauswahl als drei Tabs geliefert, aber „Übernehmen“ setzte nur den Typ.
Etappe 45 hängt das Einstellfenster dahinter und macht die Liste bedienbar.

| Vorher (44) | Jetzt (45) |
|---|---|
| „Übernehmen“ setzt nur die Befehlstyp-ComboBox | „Übernehmen“ öffnet das Einstellfenster des Befehls |
| Schalter/Variable = Freitext-Name | nummerierte Liste 1..999 + Namensfeld (Register in `database.json`) |
| Items/Helden/Truppen = Freitext | Auswahl aus der Datenbank |
| Audio/Bilder = Freitext | Ordnerwahl + Dateiliste, Kopie ins Projekt |
| Befehlsliste nur Auswahl + Buttons | Doppelklick = Bearbeiten, Rechtsklick = Menü |

## Neue Dateien

```
src/editor/event_numbered_registry.h     (portabel: Register 1..999, Roundtrip)
src/editor/event_command_settings.h      (portabel: Einstell-Schema je Typ + Asset-Filter)
src/editor/event_command_settings_win.h  (Win32: Einstellfenster, rendert das Schema)
src/editor/event_asset_browser_win.h     (Win32: Ordnerwahl + Dateiliste + Kopie ins Projekt)
tools/event_command_settings_test.cpp    (kopflos: 29 Prüfungen)
```

Geändert: `src/editor/editor_main.cpp` (Doppelklick, Kontextmenü, Picker→Einstellfenster,
Includes), `src/editor/event_command_dialog_win.h` (Picker öffnet Einstellfenster),
`.github/workflows/ci.yml` (neuer Prüfschritt).

## Wichtig: Serialisierung bleibt der getestete Pfad

Das Einstellfenster erfindet **keine** neue Speicherung. Beim „Übernehmen“ werden
die Werte in die bestehenden F6-Formularcontrols geschrieben und dann
`ev_insert_command()` / `ev_update_command()` ausgelöst — also genau
`event_command_prepare` mit verlustfreiem Mergen, Validierung, Undo und
Engine-Lesbarkeit. Das Schema (`event_command_settings.h`) folgt deshalb dem
**laufenden Karten-Event-System** (`event_command_keys` in `event_form.h`), nicht
dem typisierten Rpg-Katalog: nur so bleibt jeder geschriebene Befehl ausführbar.

## Nummerierte Register (Schalter / Variablen)

`NumberedRegistry` hält je Nummer 1..999 einen Namen. Persistenz in
`database.json["switches"]` / `["variables"]` als Objekt `{"101":"Tor_Offen",...}`
(oder Liste von Namen, Index = Nummer-1). Fehlende Schlüssel => leeres Register;
der Editor legt sie beim ersten Benennen an, **unberührte database-Felder bleiben
erhalten**. Die Engine arbeitet weiter mit Namen (`GameState.switch/set_switch`);
die Auswahl liefert den Namen der gewählten Nummer. Eine unbenannte Nummer bekommt
beim Übernehmen einen sprechenden Default (`Schalter_0101` / `Variable_0202`) und
wird registriert, damit kein leerer Name in der Karte landet.

Beschriftung wie XP: `0101: Tor_Offen`, unbenannt `0001: (unbenannt)`.

## Bedienelemente je Feldart

| Feldart | Bedienelement |
|---|---|
| NumberedSwitch / NumberedVariable | Combo 1..999 + Namensfeld |
| Ref (items, actors, troops, states, skills, common_events) | Combo aus der Datenbank |
| Asset (bgms, bgss, ses, pictures, faces, …) | Combo + „Durchsuchen…“ (Ordnerwahl) |
| Combo (op, source, Wetter, Effekt, Modus, Richtung, Position, …) | feste Auswahl |
| Bool | An/Aus |
| Int / Number | einzeilige, geprüfte Zahl (Validierung im bestehenden Pfad) |
| Text / Multiline | einzeilig / mehrzeilig |

Asset-Ordner: `audio\bgm`, `audio\bgs`, `audio\se`, `pictures`, `pictures\faces`, …
Endungen: Audio `ogg/mp3/wav/mid/midi`, Bild `png/jpg/jpeg/bmp` (case-insensitiv).
Der zurückgegebene Schlüssel ist relativ mit Slash, z. B. `audio/bgm/dorf.ogg`.
Eine extern gewählte Datei wird per `CopyFileW` in den Projekt-Asset-Ordner kopiert
(nicht überschrieben); scheitert die Kopie, wird gemeldet statt still zu verweisen.

## Tests

Kopflos (Linux/CI, ohne Win32):

```
g++ -std=c++20 -O2 -Wall -Wextra -Werror -Isrc src/editor/json.cpp \
  tools/event_command_settings_test.cpp -o event_command_settings_test
./event_command_settings_test        # 29 Prüfungen, 0 Fehler
```

Prüft: Register-Grenzen/Namen/Nummer-Suche/Beschriftung, Roundtrip Objekt+Liste,
fehlender Schlüssel => leer; Schalter/Variable = nummerierte Liste, Item = Ref,
BGM = Asset, Text = Mehrzeiler; **alle 73 Typen** liefern 1..8 sinnvolle Felder ohne
`Hidden` mit gültigen Slots; Asset-Unterordner, Endungsfilter, Slash-Schlüssel.

Cross-Kompilat des Editors (zig, Windows-Ziel) sauber; nur die vorbestehende
`-Wdangling-assignment-gsl`-Warnung an alter Stelle. Bestehende Suiten grün:
`json_test` **1031/0**, `befehle_test` 0, `event_commands_test` **107/0**,
`event_commands_arch_test` **1542/0**, `event_commands_engine_test` **110/0**,
`event_command_catalog_test` **32/0**.

## CI

<!-- CI_ABSCHNITT -->
Wird nach dem Push nachgetragen (Lauf-ID, alle vier Jobs). Neuer Schritt
„Einstell-Schema + nummerierte Register portabel prüfen (Etappe 45)“ im Job `cpp-logik`.

## Sicht-/DPI-Test auf dem Windows-Rechner

Die Sandbox cross-kompiliert nur; CI-Grün (auch Windows-Job) ersetzt keinen
interaktiven GUI-/DPI-Test. Bitte prüfen:
- F6 → Befehl doppelklicken → Einstellfenster mit genau seinen Feldern; Rechtsklick
  → Menü Bearbeiten/Einfügen/Löschen; einfacher Klick wählt nur.
- Schalter/Variable: nummerierte Liste 1..999, Namensfeld bearbeiten, Übernehmen
  schreibt den Namen; `database.json` erhält `switches`/`variables`, andere Felder
  bleiben unverändert.
- Audio/Bild: „Durchsuchen…“ listet den Projektordner, „Ordner wählen…“ wechselt,
  externe Datei wird kopiert; Schlüssel erscheint im Feld.
- Picker (XP…) → Übernehmen öffnet direkt das Einstellfenster.
- 96/120/144/192 DPI: Fensterhöhe passt sich dem Inhalt an, nichts abgeschnitten.

## Bewusste Grenzen / nächster Ausbau

- Das Einstellfenster deckt die Felder des **laufenden** Karten-Event-Systems ab.
  Die 30 „vorbereiteten“ XP-Befehle aus Etappe 44 (ohne `cmd`) haben weiterhin kein
  Karten-Event und bleiben im Picker gekennzeichnet; ihr Einstellfenster folgt,
  sobald sie an die Engine-Ports angebunden sind.
- Komplexe Unterlisten (Wegpunkte, Waren, Wahlzweige) werden als Mehrzeiler
  bearbeitet; ein grafischer Listen-Editor je Zweig ist ein eigener Ausbau.
- Nummerierte Register: aktuell 1..999 wie XP; eine F9-Registerkarte zum Pflegen
  aller Namen auf einmal ist ein sinnvoller nächster Schritt.
