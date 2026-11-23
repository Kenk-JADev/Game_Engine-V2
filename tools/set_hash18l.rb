# -*- coding: utf-8 -*-
# Korrigiert im Handoff den "Datum des Stands"-Absatz auf 18l (der Satz
# behauptete noch 18h) und setzt den echten 18l-Commit-Hash nach.

require 'open3'

GIT = 'C:/Users/jusagu/Tools/PortableGit/cmd/git.exe'

def git(*args)
  out, _st = Open3.capture2e(GIT, *args)
  out.strip
end

path = File.expand_path('../docs/HANDOFF_Naechste_KI.md', __dir__)
text = File.read(path, encoding: 'UTF-8')

old = text[/^\*\*Datum des Stands:\*\*.*$/]
new = "**Datum des Stands:** HEAD (nach Commit) — Etappe 18l: Höhen-Malwerkzeug + Kartengrenze"
if old && old != new
  text.sub!(old, new)
  puts "Stand-Satz aktualisiert"
end

if text.include?('COMMIT18L')
  hash = nil
  # Hash erst nach dem Etappen-Commit verfügbar - erst suchen, dann ersetzen.
  log = git('log', '--oneline', '-5')
  m = log.match(/([0-9a-f]{7}) Etappe 18l/)
  hash = m[1] if m
  raise 'Etappe-18l-Commit noch nicht da - Commit vor dem Nachtrag ausführen' unless hash
  text.sub!('COMMIT18L', hash)
  puts "18l-Hash nachgetragen: #{hash}"
else
  puts 'Hash bereits eingetragen'
end

File.write(path, text, encoding: 'UTF-8')
puts 'ok'
