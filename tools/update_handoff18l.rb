# -*- coding: utf-8 -*-
# Aktualisiert docs/HANDOFF_Naechste_KI.md auf Stand 18l:
# - Titel: Stand nach Etappe 18l
# - Etappen-Tabelle: Vollbild-Fix (8abfdd2) + 18l ergaenzen
# - Editor-IDs: Hinweis auf 3029 (Hoehe malen) im Control-ID-Absatz

path = File.expand_path('../docs/HANDOFF_Naechste_KI.md', __dir__)
text = File.read(path, encoding: 'UTF-8')

# 1) Titel
old_title = text[/^# .*$/]
new_title = "# Handoff für die nächste KI - Rune3D, Stand nach Etappe 18l"
text.sub!(old_title, new_title)

# 2) Etappen-Tabelle: 18l-Zeile nach der 18k-Zeile
row_18k = "| 18k | `03d23f7` | Kartenbaum (XP-Kartenliste): TreeView links oben, Doppelklick = Eigenschaften, Rechtsklick-Kontextmenue mit Inplace-Umbenennen |"
row_fs  = "| — | `8abfdd2` | Fix Vollbild-Resize: hbrBackground im Hauptfenster (schwarze Ränder beim Maximieren weg) |"
row_18l = "| 18l | `COMMIT18L` | Höhen-Malwerkzeug („Höhe malen“, ID 3029): Berge/Senken per Maus, terrain.heights im Karten-JSON; Kartengrenze (Physics-Clamp); Terrain-Bau pro Kartenwechsel (enter_map→build_terrain) |"
raise "18k-Zeile nicht gefunden" unless text.include?(row_18k)
text.sub!(row_18k, row_18k + "\n" + row_fs + "\n" + row_18l)

# 3) Control-IDs-Satz erweitern (3029)
old_ids = "Haupt-Controls 3001–3026 (Toolbar 3025, **Boden-malen-Radio 3026**)"
if text.include?(old_ids)
  text.sub!(old_ids,
            "Haupt-Controls 3001–3029 (Toolbar 3025, Boden-malen-Radio 3026, " \
            "Kartenbaum 3027, **Höhe-malen-Radio 3029**)")
else
  puts "WARNUNG: ID-Satz nicht gefunden (Muster geaendert?) - nicht ersetzt"
end

File.write(path, text, encoding: 'UTF-8')
puts "Handoff aktualisiert: Titel, Tabelle (Vollbild-Fix + 18l), IDs"
