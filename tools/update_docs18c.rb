# Aktualisiert docs/architecture.md um Etappe 18c
# (Tileset-Editor: Boden-Textur + Kachelgrösse pro Karte, eigene Kacheln)
# sowie den Umlaut-Fix der Fenstertexte.
# Aufruf: ruby tools/update_docs18c.rb
path = File.join(__dir__, '..', 'docs', 'architecture.md')
text = File.read(path, encoding: 'UTF-8')

anchor = '## Projekt-Auswahl im Editor (Etappe 18b)'
raise 'Anker für 18c fehlt' unless text.include?(anchor)

section = <<~MD
  ## Tileset-Editor (Etappe 18c)

  Neuer modusloser Dialog `RuneEditorTileDlg` (Grafik -> Tileset-Editor...,
  ID_MENU_TILESET 115, Taste F7; IDs 4301-4312):

  **Boden pro Karte:** Das Karten-JSON bekommt zwei neue Felder in
  `terrain`: `texture` (Name der Boden-Textur, Standard "gras") und
  `tile` (Kachelgrösse in Metern, Standard 2.0; erst seit dieser Etappe
  einstellbar, vorher auf 2 m in die Terrain-UVs gebacken). Der Shader
  skaliert die Terrain-UV mit 2.0/tile (gParams.z; Boxen nutzen weiter
  gParams.y, Shadow-Pass beide 0 - sauber getrennt).

  **Dialog:** Liste aller Texturen (8 prozedurale + Projekt-Importe,
  mat_all_names), links Vorschau (RGBA -> 32bpp-DIB -> StretchBlt) der
  angewählten Kachel, rechts Kachelgrösse + "Übernehmen (Karte)":
  schreibt terrain.texture/tile, wendet sie live an (set_terrain_tex/
  set_terrain_tile), markiert die Karte dirty. Status "Boden übernommen".

  **Eigene Kacheln:** "Kachel erzeugen" macht aus Basisfarbe + Zweitfarbe
  + Muster (kein/punkte/streifen/rauschen) eine 64x64-RGBA-Kachel
  (deterministischer LCG gleicher Eingabe = gleiche Kachel), meldet sie
  im Renderer an (sofort in Vorschau und Viewport testbar). "Als
  Projekt-Textur speichern" schreibt sie als .rgba + textures.json-Eintrag
  (Materialimport-Syntax; das Spiel lädt sie beim Start über
  Textures.register_project).

  **Renderer-Zugriff:** add_texture führt jetzt Pixel + Grösse mit
  (m_tex_pixels/m_tex_sizes); texture_rgba(name, out, w, h) liefert sie
  für Vorschauen zurück. Rune::Gfx.terrain_texture kennt ein zweites,
  optionales Argument (Kachelgrösse); game/main.rb liest terrain.texture/
  tile der Startkarte statt fix 'gras' zu setzen. Der Editor wendet beide
  Werte beim Laden einer Karte an.

  **Umlaut-Fix:** Die sichtbaren Fenstertexte (L"-Literale) sind jetzt
  einheitlich echte Umlaute ("Löschen", "Schließen", "Ausgewählt",
  "Übernehmen (Karte)" zeigt korrekt "Übernehmen"); nur Log-Strings
  bleiben transliteriert, weil die Live-Tests darauf matchen. Die Quelle
  ist durchgängig UTF-8, GDI/GDI-Text kommt korrekt als UTF-16 in die
  EXE (Tools: scan_encoding.rb, check_binary_umlaute.rb, fix_umlaute.rb).

  **Geprüft:** Live-Test log/live18c.ps1 (Dialog offen, Liste,
  Boden-Wechsel auf "weg" + 4-m-Kachel landet nach Strg+S im Karten-JSON,
  Kachel erzeugen + als Projekt-Textur speichern, Aufräumen), alle 13
  Tests grün, 8-s-Spielrun ohne engine.log-Fehler.

MD

text = text.sub(anchor, section + anchor)
File.write(path, text, mode: 'wb')
puts 'architecture.md: 18c ergänzt'
