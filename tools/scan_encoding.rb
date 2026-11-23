# Etappe 18c/Grösse: Prüft alle Quell- und Daten-Dateien auf Kodierung.
#  1) Ist die Datei valides UTF-8? (Wenn nein: gemischte/kaputte Bytess)
#  2) Welche Umlaute (UTF-8) und rohen CP1252-Bytes stecken drin?
# Aufruf: ruby tools/scan_encoding.rb
files = Dir['src/**/*.{cpp,h}'] + Dir['game/**/*.rb'] + Dir['game/data/**/*.{json,rb}']
invalid = []
files.each do |f|
  raw = File.binread(f)
  invalid << f unless raw.force_encoding('UTF-8').valid_encoding?
end
puts "Dateien gesamt: #{files.size}, invalide UTF-8: #{invalid.size}"
invalid.each { |f| puts "  INVALID: #{f}" }
puts '---'

# UTF-8-Umlaute und CP1252-Rohbytes je Datei zählen.
utf8_pat = {
  "\xC3\xA4".b => 'ae', "\xC3\xB6".b => 'oe', "\xC3\xBC".b => 'ue',
  "\xC3\x84".b => 'Ae', "\xC3\x96".b => 'Oe', "\xC3\x9C".b => 'Ue',
  "\xC3\x9F".b => 'ss'
}.freeze
raw1252 = [0xE4, 0xF6, 0xFC, 0xC4, 0xD6, 0xDC, 0xDF].freeze
files.each do |f|
  bin = File.binread(f)
  hits = []
  utf8_pat.each do |pat, tr|
    n = bin.scan(pat).size
    hits << "UTF8-#{tr} x#{n}" if n.positive?
  end
  raw1252.each do |b|
    # CP1252-Rohbyte nur zählen, wenn es NICHT Teil einer UTF-8-Folge ist;
    # einfache Zählung reicht hier zur Diagnose.
    n = bin.count(b.chr)
    hits << "RAW1252-0x#{b.to_s(16)} x#{n}" if n.positive?
  end
  puts "#{f}: #{hits.join(', ')}" unless hits.empty?
end
