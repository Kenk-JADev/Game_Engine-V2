# Aktualisiert docs/architecture.md um Etappe 17g (Datenbank: Helden/Items/Gold).
# Aufruf: ruby tools/update_docs17g.rb
path = File.join(__dir__, '..', 'docs', 'architecture.md')
text = File.read(path, encoding: 'UTF-8')

anchor = "## Ruby-API (Stand Etappe 2)"
raise 'Anker fuer 17g fehlt' unless text.include?(anchor)

section = <<~MD
  ## Datenbank: Helden, Items, Gold (Etappe 17g)

  **Projektdatei:** `projekte/<p>/data/database.json` (optional - fehlt sie,
  gilt ein leerer Katalog):

  ```json
  { "gold": 120,
    "actors": [ { "id": "arik", "name": "Arik", "class": "Krieger",
                  "level": 1, "hp": 120, "mp": 15, "atk": 14, "def": 10,
                  "spi": 4, "agi": 8, "exp": 0 } ],
    "items":  [ { "id": "heiltrank", "name": "Heiltrank", "kind": "heal",
                  "power": 40, "price": 25, "start": 2, "desc": "..." } ] }
  ```

  **Spiel:** `Database.database` laedt die Datei, `Party.setup` baut daraus
  die Party (HP/MP starten voll, `start` = Startmenge). `Party` haelt den
  veraenderlichen Zustand: `add_item/take_item/use_item` (heal/mana wirken auf
  den ersten verletzten Helden, quest/money sind nicht benutzbar),
  `add_gold`, `heal_all`, `snapshot/restore!`. Spielstaende nehmen den
  Party-Snapshot mit (`'party'` in `save_game`/`load_game`). Im Esc-Menue
  zeigt ein Panel daneben Helden, Gold und Inventar (`Party.status_lines`).

  **Interpreter-Befehle** (zusaetzlich):
  `{"cmd":"item","name":"heiltrank","count":2}` bzw. `"take":1`,
  `{"cmd":"gold","amount":50}` (negativ = abziehen),
  `{"cmd":"heal"}` (voll) bzw. `{"cmd":"heal","amount":30}`.

  **Script-Sandbox** (`Scripts::Context`) kennt dafuer: `party`,
  `gold(amount)`/`gold()`, `items`, `item_count`, `add_item`, `take_item`,
  `use_item`, `heal`.

  **Editor:** Menue `Datenbank -> Datenbank (Helden/Items)...`
  (`ID_MENU_DATABASE = 111`) oeffnet das moduslose Fenster
  `RuneEditorDbDlg` (IDs 8001-8055): Reiter Helden/Items, Listen links,
  Formular rechts (ID/Name/Klasse/HP/MP/ATK/DEF/SPI/AGI bzw.
  ID/Name/Art/Staerke/Preis/Startmenge/Beschreibung), Gold-Feld,
  Neu/Loeschen/Speichern/Neu laden - schreibt `database.json`.

  **Event-Editor:** drei neue Befehlstypen *Item geben/nehmen* (negative
  Menge = nehmen), *Gold +/-*, *Heilen* (leer = voll).

  **Demo:** `game/data/database.json` (Vorlage, wandert per
  `Project.ensure_demo` in neue Projekte) und `projekte/dorf/data/database.json`
  mit Arik/Mira und Heiltrank/Manatrank/Wasserkristall/Goldbeutel, angelegt
  per `ruby tools/add_database17g.rb`.

  **Geprueft:** `ruby -I game/engine tools/party_test.rb` (Setup, Inventar,
  Benutzen, Gold, Heilung, Snapshot/Restore, Interpreter-Befehle,
  Sandbox-Helfer - 39 Checks) sowie alle bisherigen Tests gruen.

MD

text = text.sub(anchor, section + anchor)
File.write(path, text, mode: 'wb')
puts 'architecture.md: 17g ergaenzt'
