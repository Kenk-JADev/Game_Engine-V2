# Byte-Ebene: zählt echte Mojibake-Sequenzen (doppelt kodierte Umlaute).
# "Menü" korrekt = C3 BC; doppelt kodiert ("MenÃ¼") = C3 83 C2 BC.
# Aufruf: ruby tools/scan_mojibake_bytes.rb
files = Dir['src/**/*.{cpp,h}'] + Dir['game/**/*.rb']
files.each do |f|
  bin = File.binread(f)
  n83 = bin.scan("\xC3\x83".b).size        # "Ã" - Zeichen doppelt kodiert
  nc2 = bin.scan("\xC2\xA4".b).size + bin.scan("\xC2\xBC".b).size +
        bin.scan("\xC2\xB6".b).size + bin.scan("\xC2\x84".b).size
  bom = bin[0..2] == "\xEF\xBB\xBF".b
  puts format('%-34s mojibake83=%d c2=%d bom=%s', f, n83, nc2, bom) if n83.positive? || nc2.positive? || bom
end
puts 'Fertig'
