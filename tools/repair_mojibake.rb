# Repariert die doppelte UTF-8-Kodierung in src/editor/editor_main.cpp
# (PowerShell-Rewrite hatte die Datei als CP1252 gelesen und als UTF-8 mit
# BOM geschrieben: "ü" C3 BC -> "Ã¼" C3 83 C2 BC).
# Rückweg: UTF-8-Zeichen -> CP1252-Bytes -> als UTF-8 interpretieren.
# Aufruf: ruby tools/repair_mojibake.rb
# Ruft repair: editor_main.cpp UND renderer.h (letzterer nur BOM).
%w[src/editor/editor_main.cpp src/gfx/renderer.h].each do |path|
  bin = File.binread(path)
  had_bom = bin.start_with?("\xEF\xBB\xBF".b)
  bin = bin.delete_prefix("\xEF\xBB\xBF".b) if had_bom
  before = bin.scan("\xC3\x83".b).size
  if before.positive?
    fixed = bin.force_encoding('UTF-8').encode('Windows-1252').force_encoding('UTF-8')
    raise 'Rückkonvertierung ergab ungültiges UTF-8!' unless fixed.valid_encoding?
    File.binwrite(path, fixed.b)
    puts "#{path}: BOM=#{had_bom}, #{before} Doppelkodierungen repariert"
  else
    File.binwrite(path, bin)
    puts "#{path}: BOM=#{had_bom}, keine Doppelkodierung"
  end
end
