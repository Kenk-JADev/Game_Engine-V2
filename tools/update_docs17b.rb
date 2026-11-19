# Aktualisiert docs/architecture.md fuer Etappe 17b (Maps & Eigenschaften).
# Aufruf: ruby tools\update_docs17b.rb
# Muster: Anker-basiertes Einfuegen, mit raise statt stiller Korrektur.

PFAD = 'docs/architecture.md'

text = File.read(PFAD, encoding: 'UTF-8')

# 1) API-Tabelle: World.generate auf Breite/Hoehe aktualisieren + Gfx.fog
alter_api = '| `Rune::World.generate(size: 240, resolution: 192, seed: 7, amplitude: 18.0)` | Gelaende bauen |'
raise 'API-Zeile nicht gefunden' unless text.include?(alter_api)
neue_api = "| `Rune::World.generate(width: 240, height: 240, resolution: 192, seed: 7, amplitude: 18.0)` | Gelaende bauen (rechteckig, height entfaellt = Quadrat) |\n" \
           "| `Rune::Gfx.fog(an, farbe, sichtweite)` | Distanznebel an/aus, Nebelfarbe, Distanz |"
text.sub!(alter_api, neue_api)

# 2) 17b-Abschnitt nach dem 17a-Block (vor "## Ruby-API") einfuegen
anker = "## Ruby-API (Stand Etappe 2)"
raise 'Ruby-API-Anker nicht gefunden' unless text.include?(anker)

abschnitt = <<~MD
  ## Karten & Karten-Eigenschaften (Etappe 17b)

  **Rechteckige Karten:** `Terrain::generate(width, height, resolution,
  seed, amplitude)` baut jetzt Rechtecke (Zellen bleiben quadratisch,
  `res_z` wird proportional abgeleitet). Alte Karten mit `size` laufen
  weiter (Fallback in boot.rb, launcher.rb, main.rb, Editor).

  **Karten-JSON (neue Felder, alle optional):**

  ```json
  "terrain":  { "width": 240, "height": 240, "resolution": 192,
                "seed": 7, "amplitude": 18.0 },
  "fog":      { "on": true, "far": 120, "color": 15263976 },
  "panorama": { "color": 4026293 },
  "note":     "Kommentar zur Karte"
  ```

  `panorama.color` ersetzt den Himmelverlauf durch eine Vollfarbe,
  `fog` blendet die Ferne ab `0.45 * far` sanft zur Nebelfarbe
  (smoothstep, im Pixel-Shader ueber Weltposition minus Kameraposition).
  `Project.create` legt neue Projekte **flach** an (amplitude 0) —
  Berge pro Karte im Eigenschaften-Dialog zuschaltbar.

  **Editor:** Menue Karte hat jetzt *Neue Karte...*,
  *Eigenschaften...* und *Karte loeschen* plus die dynamische Karten-
  Liste (Radio-Markierung der aktiven Karte). Der Eigenschaften-Dialog
  (natives Win32-Fenster, moduslos) editiert Name, Breite/Hoehe,
  Berge an/aus + Seed + Hoehe, Nebel an/aus + Sichtweite + Farbe,
  Panorama-Farbe und einen freien Kommentar; OK regeneriert das
  Gelaende sofort. Neue Karten heissen welt01, welt02, ...

  **Geprüft:** Eigenschaften-Dialog (Nebel an, 120 m, 0xE8E8E8) →
  Nebel im Editor-Viewport sichtbar; Strg+S → fog/panorama/note in
  der JSON, events/npcs/objects unangetastet; Neue Karte 320x240
  ohne Berge → flache Ebene + welt01.json korrekt; Rune3D startet in
  dorf und zeigt denselben Nebel (Karten-Atmosphaere gilt im Spiel).

MD

text.sub!(anker, abschnitt + anker)

# 3) Pitfall #41: haengt direkt an der letzten Pitfall-Zeile (#40-Block).
#    Wird hier NICHT mehr eingefuegt — Pitfall #41 wurde direkt in der
#    Datei nachgetragen (siehe git history).

File.write(PFAD, text, encoding: 'UTF-8')
puts 'architecture.md aktualisiert (17b)'
