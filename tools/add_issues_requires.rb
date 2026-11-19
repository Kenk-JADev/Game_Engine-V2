#encoding: utf-8
# Etappe 18h: require 'issues' in alle Kopflos-Tests einziehen, die
# Engine-Module direkt laden (ohne boot.rb). Einmalig; idempotent.
files = %w[editor_test effects_test event18f_test event_pages_test event_test
           npc_test party_test save_test script_test weather_test]
files.each do |f|
  path = File.join('tools', f + '.rb')
  src = File.read(path, encoding: 'utf-8')
  next if src.include?("require 'issues'")
  if src.include?("require 'state'")
    src.sub!("require 'state'") { |m| m + "\nrequire 'issues'" }
  else
    src.sub!(/require '(textures|interpreter)'/) { |m| m + "\nrequire 'issues'" }
  end
  File.write(path, src, encoding: 'utf-8')
  puts "patched #{path}"
end
