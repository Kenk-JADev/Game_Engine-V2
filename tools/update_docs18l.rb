# -*- coding: utf-8 -*-
# Ergaenzt docs/architecture.md um den Abschnitt zu Etappe 18l
# (Hoehen-Malwerkzeug + Kartengrenze). Legt die Aenderungen VOR dem
# Ruby-API-Anker ab, wie in den vorherigen Etappen.

ANCHOR  = "## Ruby-API (Stand Etappe 2)"
SECTION = <<~MD

## Höhen-Malwerkzeug & Kartengrenze (Etappe 18l)

**Berge direkt anmalen statt Seed-Fummelei:** Das Karten-JSON bekommt unter
`terrain` eine optionale Liste `heights` mit weichen Höhen-Pinseln:

```json
"terrain": {
  "width": 240.0, "height": 240.0, "resolution": 192,
  "seed": 7, "amplitude": 0.0,
  "heights": [ { "x": 45.0, "z": -79.0, "r": 4.0, "h": 2.0 } ]
}
```

- `x`/`z` = Weltposition, `r` = Pinselradius in Metern, `h` = Höhen-Änderung
  (negativ = Senke). Der `Terrain::compute_heights` addiert die Pinsel als
  weiche Kegel (cos-Falloff, Mitte volle Höhe, Rand 0) **nach** dem FBM-Rauschen
  — sie wirken auch im sonst abgeflachten Dorfkern und sind vollständig
  deterministisch reproduzierbar (F5, Saveload, Playtest).
- Default: Neue Karten sind flach (`amplitude` 0). `game/main.rb` und der
  Launcher erzeugen ohne Angabe Amplitude 0 — Berge entstehen durch Malen.

**Editor:** Fünftes Werkzeug „Höhe malen" (ID 3029). Linksklick hebt um
+2 m pro Klick, Shift+Klick senkt um −2 m, Mehrfachklicken stapelt (klickt man
auf den frisch gehobenen Hang, wächst der Berg breiter; Klicks in dieselbe
Rasterzelle werden zu einem Eintrag zusammengeführt). `+/-` ändert die
Pinselgröße wie beim Boden-Malen.

**Kartengrenze:** `Physics::update` klemmt die Spielerposition auf
`[-width/2 + radius, width/2 - radius] × [-height/2 + radius, height/2 - radius]`
— die Karte kann nicht verlassen werden. Da das Terrain seit 18l beim
Kartenwechsel (`main.rb: enter_map → build_terrain`) neu aufgebaut wird, gelten
die Grenzen der jeweils geladenen Karte.

**Ruby:** `Rune::World.generate(width:, height:, resolution:, seed:,
amplitude:, height_brushes:)` — `height_brushes` = Liste `[x, z, r, h]`
(setzt `Rune::World.height_brushes=` vor dem Generieren). Die Engine baut die
Pinsel in `compute_heights` ein; `height_at`/Kollision/sichtbare Geometrie
stimmen automatisch.

MD

path = File.expand_path('../docs/architecture.md', __dir__)
text  = File.read(path, encoding: 'UTF-8')
raise "Anker nicht gefunden" unless text.include?(ANCHOR)
raise "Abschnitt existiert schon" if text.include?("## Höhen-Malwerkzeug & Kartengrenze")
text.sub!(ANCHOR, SECTION + "\n" + ANCHOR)
File.write(path, text, encoding: 'UTF-8')
puts "architecture.md ergänzt (Etappe 18l)"
