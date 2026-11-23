# Aktualisiert docs/architecture.md um Etappe 18d
# (Splat-Böden: mehrere Boden-Texturen pro Karte, Atlas, Flächen-Editor).
# Aufruf: ruby tools/update_docs18d.rb
path = File.join(__dir__, '..', 'docs', 'architecture.md')
text = File.read(path, encoding: 'UTF-8')

anchor = '## Tileset-Editor (Etappe 18c)'
raise 'Anker für 18d fehlt' unless text.include?(anchor)

section = <<~MD
  ## Splat-Böden (Etappe 18d)

  Eine Karte zeigt jetzt mehrere Boden-Texturen gleichzeitig: rechteckige
  **Flächen** im Karten-JSON (`terrain.splats`: Liste von
  `{"tex","x","z","w","h"}` in Weltmetern, Karte auf 0 zentriert; der
  spätere Eintrag gewinnt).

  **Technik:** Der Renderer baut aus allen registrierten 64x64-Texturen
  einen Atlas (`Renderer::update_atlas()`, Raster aus 64-px-Zellen, neuer
  Slot "atlas" pro Aufruf - F5-sicher). `Terrain::generate_splat()` legt
  pro Quad vier eigene Vertices an (nicht geteilt) mit UVs innerhalb
  genau einer Atlas-Zelle (halb-Pixel-Inset gegen bilineares Bluten);
  die Quad-Mitte entscheidet die Zelle (letzter Treffer gewinnt). Ohne
  splats bleibt der alte Ein-Textur-Pfad byte-identisch
  (Welt-UV, Kachelgröße via gParams.z) - alte Karten und live18c
  unverändert.

  **API:** `Rune::Gfx.terrain_splats(base_tex, list)` (String- ODER
  Symbol-Schlüssel; Terrain wird mit den letzten generate-Parametern neu
  gebaut, Atlas-Slot wird als Terrain-Textur gesetzt, tile-Skalierung 1).
  `game/main.rb` nutzt den Splat-Pfad automatisch, wenn die Startkarte
  `terrain.splats` hat.

  **Editor:** `regen_terrain()` ist die eine Stelle, die beide Pfade
  entscheidet (und von `load_map` sowie dem Tileset-Dialog aufgerufen
  wird). Der Tileset-Dialog hat eine dritte Spalte "Boden-Flächen":
  Liste, X/Z/Breite/Tiefe-Felder, "Hinzufügen" (mit gewählter Textur),
  "Entfernen" (markierter Eintrag; leere Liste wird ganz aus dem JSON
  entfernt). Nach jeder Aktion regeneriert der Boden sofort.

  **Bekannte Grenze:** Jedes Terrain-Quad zeigt genau EINE Textur (kein
  Weich-Übergang); die Kacheldichte hängt an der Terrain-Auflösung
  (Quadgröße), `terrain.tile` wirkt im Splat-Pfad nicht.

  **Geprüft:** Live-Test log/live18d.ps1 (Fläche hinzufügen/entfernen,
  Splat-Log, JSON-Checks, Editor lebt), Spiel-Smoke mit 2 Splats
  (Atlas + Splat-Boden im engine.log, 0 Fehler), alle 13 Tests grün,
  live18c-Regression grün (Log-Format angepasst: "Kachel N m, N Fläche(n)").

MD

text = text.sub(anchor, section + anchor)
File.write(path, text, mode: 'wb')
puts 'architecture.md: 18d ergänzt'
