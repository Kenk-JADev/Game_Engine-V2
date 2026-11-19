# Aktualisiert docs/architecture.md um Etappe 17f (Grafik-Import + Materialbase).
# Aufruf: ruby tools/update_docs17f.rb
path = File.join(__dir__, '..', 'docs', 'architecture.md')
text = File.read(path, encoding: 'UTF-8')

anchor = "## Ruby-API (Stand Etappe 2)"
raise 'Anker fuer 17f fehlt' unless text.include?(anchor)

section = <<~MD
  ## Grafik-Import und Materialbase (Etappe 17f)

  **Projektdateien:** Importierte Grafiken liegen unter
  `projekte/<p>/data/textures/<name>.rgba` (rohe 8-Bit-RGBA-Bytes, Zeilen von
  oben nach unten, 4 Bytes pro Pixel). Die Metadaten stehen in
  `projekte/<p>/data/textures.json`:

  ```json
  { "textures": [
      { "name": "wand_bunt", "file": "wand_bunt.rgba", "w": 64, "h": 64,
        "tile": 2.0, "source": "C:/bilder/wand.png" } ] }
  ```

  **Editor:** `Grafik -> Grafik importieren...`
  (`ID_MENU_IMPORT_TEX = 109`) oeffnet den Windows-Dateidialog (PNG/BMP),
  dekodiert per WIC (`decode_image_wic`, `-lwindowscodecs`) und schreibt
  `.rgba` + `textures.json` (`mat_import_file`). Der Name wird aus dem
  Dateinamen abgeleitet (`mat_slug`: klein, nur `a-z0-9_`, eindeutig).

  `Grafik -> Materialbase...` (`ID_MENU_MATERIALS = 110`) oeffnet das
  moduslose Fenster `RuneEditorMatDlg` (IDs 7001-7013): Liste aller Prefab-
  und importierten Texturen, Kachelgroesse aendern, importieren, loeschen
  (nur importierte), auf das ausgewaehlte Objekt anwenden (`obj["tex"]`)
  oder den Objekt-Override entfernen.

  Das Eigenschaften-Panel hat dafuer ein Feld **Material**
  (`IDC_ED_TEX = 3023`); leer = Prefab-Texturen, sonst Materialname.
  `push_object` zeichnet dann alle Teile des Objekts mit dieser Textur.

  **Spiel:** `Textures.register_project` liest `textures.json`, prueft
  `w * h * 4` Bytes und meldet jede Datei ueber `Rune::Gfx.texture` an. Der
  Aufruf steht in `Game.setup` direkt nach `Textures.register_all` (also
  auch nach F5). `Textures.tile_of(name)` liefert die Kachelgroesse aus den
  Metadaten. `Prefabs.instantiate` wertet `obj["tex"]` aus und ueberschreibt
  Textur und Kachel aller Prefab-Teile.

  **Kopflos testen:** `build/RuneEditor.exe --import-tex <bild> [name]`
  importiert ohne Fenster (Projekt `dorf`) und gibt `OK:`/`FEHLER:` aus.

  **Wichtig beim Linken:** Die Ruby-DLL exportiert eigene CRT-Symbole
  (`fclose`, `fwrite`, ...). Ohne `-lucrtbase` **vor** der
  Ruby-Importbibliothek werden unsere stdio-Aufrufe auf die Ruby-Exporte
  umgebogen; `fclose` stuerzt dann mit Access Violation ab. `build.ps1`
  linkt deshalb `-lucrtbase` zuerst.

  **Geprueft:** `ruby tools/tex_import_test.rb` (BMP -> RGBA, Zeilenordnung,
  Farben, Metadaten, Aufraeumen) und `ruby tools/project_tex_test.rb`
  (Spielseite: Metadaten, Groessenpruefung, Registrierung, `tile_of`) sowie
  alle bisherigen Tests gruen.

MD

text = text.sub(anchor, section + anchor)
File.write(path, text, mode: 'wb')
puts 'architecture.md: 17f ergaenzt'
