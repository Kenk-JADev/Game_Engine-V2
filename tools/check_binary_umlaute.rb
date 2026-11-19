# Prüft, wie die Umlaute in der gebauten RuneEditor.exe landen:
#  - korrekt als UTF-16LE (Anzeige im UI wäre richtig), oder
#  - byteweit verbreitert (UTF-8-Bytes einzeln zu UTF-16 -> kaputte Anzeige).
# Aufruf: ruby tools/check_binary_umlaute.rb
bin = File.binread('build/RuneEditor.exe')

proben = {
  'Ausgewählt korrekt (UTF-16)' => 'Ausgewählt',
  'Löschen korrekt (UTF-16)'    => 'Löschen',
  'Öffnen korrekt (UTF-16)'     => 'Öffnen',
  'Übernehmen korrekt (UTF-16)' => 'Übernehmen',
  'Schließen korrekt (UTF-16)'  => 'Schließen',
  'Karte löschen (UTF-16)'      => 'Karte löschen'
}.freeze

proben.each do |name, txt|
  wide = txt.encode('UTF-16LE').force_encoding('BINARY')
  puts format('%-34s %s', name, bin.include?(wide) ? 'JA' : 'nein')
end

# Kaputte Variante: UTF-8-Bytess einzeln zu UTF-16 verbreitert.
puts '---'
%w[ä ö ü Ä Ö Ü ß].each do |c|
  utf8 = c.dup.force_encoding('UTF-8').b
  verbreitert = utf8.bytes.map { |b| b.chr + "\x00" }.join
  n = bin.scan(verbreitert).size
  puts format('kaputt verbreitert %-4s x%d', c, n) if n.positive?
end
puts '--- Fertig'
