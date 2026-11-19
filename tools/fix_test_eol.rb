#encoding: utf-8
# Etappe 18h-Nacharbeit: die durch add_issues_requires.rb im Textmodus
# auf CRLF umgestellten Testdateien wieder auf LF normalisieren (Binär-I/O).
files = %w[editor_test effects_test event18f_test event_pages_test event_test
           npc_test party_test save_test script_test weather_test]
files.each do |f|
  path = File.join('tools', f + '.rb')
  data = File.binread(path)
  fixed = data.gsub("\r\n", "\n")
  next if fixed == data
  File.binwrite(path, fixed)
  puts "LF: #{path}"
end
