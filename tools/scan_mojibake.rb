# Findet doppelt kodierte UTF-8-Umlaute (Mojibake "Ae.."-Zeichenfolgen wie
# "MenÃ¼" = ehemalige "Menü") in allen Quellen.
# Aufruf: ruby tools/scan_mojibake.rb
Dir['src/**/*.{cpp,h}'] + Dir['game/**/*.rb'] + Dir['docs/*.md'].sort.each do |f|
  s = File.read(f, encoding: 'UTF-8')
  hits = s.scan(/[\u00C0-\u00FF]{2,}/)
  unless hits.empty?
    puts "#{f}: #{hits.size} Verdachts-Stellen"
    hits.first(6).each { |h| puts "   #{h.inspect}" }
  end
end
