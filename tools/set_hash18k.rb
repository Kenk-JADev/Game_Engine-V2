# Tragt den Etappen-Hash von 18k im Handoff nach (nach dem Commit).
# Aufruf: ruby tools/set_hash18k.rb <hash>
path = File.join(__dir__, '..', 'docs', 'HANDOFF_Naechste_KI.md')
text = File.read(path, encoding: 'UTF-8')
raise 'Platzhalter fehlt' unless text.include?('| 18k | ETAPPEN_HASH |')
hash = ARGV[0]
raise 'Hash als Argument geben' if hash.to_s.empty?
text = text.sub('| 18k | ETAPPEN_HASH |', "| 18k | `#{hash}` |")
File.write(path, text, mode: 'wb')
puts "Hash #{hash} nachgetragen"
