# -*- coding: utf-8 -*-
# Traegt in docs/BACKLOG.md ein:
# 1. Erledigt: Kartengrenze (Physik-Clamp) - war User-Wunsch, in 18l behoben
# 2. Offen: User-Wunsch Objektliste verbessern + Platzier-Vorschau (Geist)
# 3. Offen: User-Wunsch Editor-Tool zum Platzieren von NPCs (x/y/z/Text)

path = File.expand_path('../docs/BACKLOG.md', __dir__)
text = File.read(path, encoding: 'UTF-8')

open_marker = "## Offen"
done_marker = "## Erledigt"

neu_offen = <<~MD

### [Editor] Objektliste verbessen + Platzier-Vorschau statt Sofort-Einfügen — offen (User-Wunsch)
- Symptom: Objektliste zeigt nur einzeilige Kurztexte; Platzieren im Setzen-Modus
  fügt beim ersten Klick sofort ein — keine Vorschau des ausgewählten Objekts.
- Wunsch: Objekt in der Liste auswählen, Vorschau (Geist des Prefabs) folgt dem
  Cursor im Viewport, Klick bestätigt das Platzieren.
- Betroffene Stellen: `src/editor/editor_main.cpp` (Objektliste, `push_hover_marker`,
  `place_object`).
- Gefunden in: User-Rückmeldung nach Etappe 18k/18l.

### [Editor] NPC-Editor / Platzier-Tool für viele Dinge (x/y/z, Text) — offen (User-Wunsch)
- Symptom: NPCs sind im Editor gar nicht editierbar (nur im Karten-JSON);
  Text/Schild-Objekte fehlen als platzierbarer Typ.
- Wunsch: Editor-Werkzeug (Dialog) zum Platzieren vieler Dinge mit X/Y/Z-Feldern:
  NPCs (Name, Position, Wegpunkte), Text-Objekte, Objekte.
- Betroffene Stellen: `src/editor/editor_main.cpp` (neuer Dialog nötig),
  `projekte/*/data/maps/*.json` ("npcs"-Schlüssel existiert, Engine seit 17d).
- Gefunden in: User-Rückmeldung nach Etappe 18k/18l.
MD

erledigt = <<~MD

### [Engine] Spieler konnte die Karte verlassen — erledigt (Etappe 18l)
- Symptom: Physik ließ den Spieler über den Kartenrand ins Leere laufen.
- Fix: `Physics::update` klemmt die Spielerposition auf
  ±(Kartenbreite/2 − Radius) × ±(Kartenhöhe/2 − Radius);
  Terrain-Bau erfolgt seit 18l pro Kartenwechsel (`enter_map → build_terrain`),
  damit die Grenzen der geladenen Karte gelten.
- Gefunden in: User-Rückmeldung („grenze der Map kann man nicht verlassen“).
MD

raise 'Offen-Marker fehlt' unless text.include?(open_marker)
text.sub!(open_marker, open_marker + neu_offen) unless text.include?('Platzier-Vorschau statt Sofort-Einfügen')

if text.include?(done_marker) && !text.include?('Spieler konnte die Karte verlassen')
  text.sub!(done_marker, done_marker + erledigt)
else
  # Kein Erledigt-Abschnitt: ans Ende
  text += "\n" + erledigt unless text.include?('Spieler konnte die Karte verlassen')
end

File.write(path, text, encoding: 'UTF-8')
puts 'BACKLOG ergänzt (2 offene User-Wünsche, 1 erledigt)'
