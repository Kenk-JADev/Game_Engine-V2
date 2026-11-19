#encoding: utf-8
# ============================================================================
#  Repo-Hygiene (Etappe 19a):  ruby tools/lint_repo.rb [--strict]
#
#  Prüft alles, was nichts mit Programmlogik zu tun hat, aber beim Arbeiten
#  immer wieder kaputtgeht — und zwar BEVOR es im Commit landet:
#
#    1. JSON-Dateien parsebar (Karten, Datenbank, System, Scripte, Prefabs)
#    2. Textdateien sind gültiges UTF-8 und haben kein BOM
#    3. keine Doppelkodierung (Mojibake, z. B. "Ã¼" statt "ü") — der bekannte
#       PowerShell-Fallstrick aus Etappe 18c
#    4. Zeilenenden nach Konvention: LF für alles, CRLF nur für
#       *.ps1/*.psm1/*.manifest. Sonst kippt der Editor beim Speichern die
#       Zeilenenden und die Versionsverwaltung zeigt ganze Dateien als Änderung.
#    5. boot.rb require't nur Module, die es gibt (und umgekehrt)
#    6. keine ignorierten Dateien im Index (build/, log/, Spielstände) und
#       keine Binär-Artefakte (*.exe/*.dll/*.o)
#    7. Grundausstattung: README.md, docs/, Manifest, CI-Workflow
#    8. Auffälliges als Info: übergrosse Quellen, TODO-Markierungen
#
#  Mit --strict zählen auch Warnungen als Fehlschlag.
#  Läuft ohne Git (die Index-Prüfung fällt dann mit einer Meldung aus).
# ============================================================================
require 'json'

ROOT = File.expand_path('..', __dir__)
Dir.chdir(ROOT)

STRICT = ARGV.include?('--strict')

QUELL_VERZEICHNISSE = %w[src game tools docs projekte .github].freeze
WURZEL_DATEIEN      = %w[README.md build.ps1 .gitignore .gitattributes].freeze

# Dateien, in Mojibake-Muster ABSICHTLICH stehen: Reparatur-/Scan-Werkzeuge und
# die Fallstrick-Doku, die das Muster als Beispiel zeigt.
MOJIBAKE_WHITELIST = %w[
  tools/repair_mojibake.rb
  tools/scan_mojibake.rb
  tools/scan_mojibake_bytes.rb
  tools/lint_repo.rb
  docs/BACKLOG.md
  docs/HANDOFF_Naechste_KI.md
].freeze

# CRLF ist nur hier erlaubt (Windows-Skripte und das Editor-Manifest).
CRLF_ERLAUBT = %w[.ps1 .psm1 .manifest].freeze

BINAER_ENDUNGEN = %w[.exe .dll .o .obj .a .lib .pdb .png .bmp .ico .rgba
                     .wav .ogg .zip .7z .lib].freeze

# Typische Doppelkodierung: UTF-8-Bytes, die als CP1252 gelesen und erneut als
# UTF-8 gespeichert wurden. Gesucht wird nach Byte-Folgen, nicht nach einzelnen
# Zeichen, damit ein echtes "ü" nicht auffällt.
MOJIBAKE = [
  "\u00C3\u00A4", "\u00C3\u00B6", "\u00C3\u00BC", "\u00C3\u009F",
  "\u00C3\u0084", "\u00C3\u0096", "\u00C3\u009C",
  "\u00E2\u0082\u00AC", "\u00E2\u0080\u009C", "\u00E2\u0080\u0094"
].freeze

$fehler  = 0
$warnung = 0

def fehler(text)
  $fehler += 1
  puts "  FEHLER   #{text}"
end

def warnung(text)
  $warnung += 1
  puts "  Warnung  #{text}"
end

def info(text) = puts("  info     #{text}")
def ok(text)   = puts("  ok       #{text}")

def relativ(pfad)
  pfad.sub(%r{^\./}, '').sub(ROOT + File::SEPARATOR, '').gsub(File::SEPARATOR, '/')
end

def textdateien
  liste = WURZEL_DATEIEN.select { |p| File.file?(p) }
  QUELL_VERZEICHNISSE.each do |verzeichnis|
    next unless File.directory?(verzeichnis)

    liste += Dir.glob(File.join(verzeichnis, '**', '*'), File::FNM_DOTMATCH).select do |p|
      File.file?(p) && !BINAER_ENDUNGEN.include?(File.extname(p).downcase)
    end
  end
  liste.map { |p| relativ(p) }.uniq.sort
end

def git?
  system('git', '--version', out: File::NULL, err: File::NULL)
end

dateien = textdateien

puts "Repo-Hygiene: #{dateien.size} Datei(en) in #{ROOT}"
puts '-' * 72

# ---------------------------------------------------------------- 1. JSON
puts '== JSON-Dateien =='
jsons = dateien.select { |p| File.extname(p).downcase == '.json' }
json_fehler = 0
jsons.each do |p|
  JSON.parse(File.read(p, encoding: 'utf-8'))
rescue StandardError => e
  json_fehler += 1
  fehler("#{p}: #{e.class}: #{e.message}")
end
ok("#{jsons.size} JSON-Datei(en) parsebar") if json_fehler.zero?

# ------------------------------------------------- 2./3. Kodierung + Mojibake
puts '== Kodierung (UTF-8, kein BOM, keine Doppelkodierung) =='
kodierung_fehler = 0
dateien.each do |p|
  bin = File.binread(p)
  # PowerShell 5.1 liest Skripte mit Nicht-ASCII-Zeichen ohne BOM als ANSI —
  # dort ist das BOM richtig. Für C++, Ruby, JSON und Markdown ist es Gift
  # (MSYS2-g++ gibt es als Zeichen aus, Ruby hängt es an den ersten Token).
  if bin.start_with?("\xEF\xBB\xBF".b) && !CRLF_ERLAUBT.include?(File.extname(p).downcase)
    kodierung_fehler += 1
    fehler("#{p}: UTF-8-BOM am Dateianfang")
  end

  text = bin.dup.force_encoding('UTF-8')
  unless text.valid_encoding?
    kodierung_fehler += 1
    fehler("#{p}: kein gültiges UTF-8")
    next
  end

  next if MOJIBAKE_WHITELIST.include?(p)

  treffer = MOJIBAKE.select { |muster| text.include?(muster) }
  next if treffer.empty?

  kodierung_fehler += 1
  fehler("#{p}: Doppelkodierung #{treffer.map(&:inspect).join(', ')} " \
         '(Reparatur: ruby tools/repair_mojibake.rb)')
end
ok('alle Dateien UTF-8, ohne BOM und ohne Doppelkodierung') if kodierung_fehler.zero?

# ------------------------------------------------------------- 4. Zeilenenden
puts '== Zeilenenden (LF; CRLF nur für .ps1/.psm1/.manifest) =='
eol_fehler = 0
crlf_dateien = []
dateien.each do |p|
  bin  = File.binread(p)
  crlf = bin.scan("\r\n").size
  lf   = bin.count("\n") - crlf
  crlf_erwartet = CRLF_ERLAUBT.include?(File.extname(p).downcase)

  if crlf.positive? && lf.positive?
    eol_fehler += 1
    fehler("#{p}: gemischte Zeilenenden (CRLF=#{crlf}, LF=#{lf})")
    next
  end

  if crlf.positive?
    crlf_dateien << p
    unless crlf_erwartet
      eol_fehler += 1
      fehler("#{p}: CRLF statt LF (#{crlf} Zeilen)")
    end
  elsif lf.positive? && crlf_erwartet
    warnung("#{p}: LF statt CRLF (unkritisch, aber uneinheitlich)")
  end
end
ok("Zeilenenden einheitlich (#{crlf_dateien.size} Windows-Skript(e) mit CRLF)") if eol_fehler.zero?

# ---------------------------------------------------------- 5. Engine-Module
puts '== Engine-Module (game/engine/boot.rb) =='
boot = File.read('game/engine/boot.rb', encoding: 'utf-8')
requires = boot.scan(/^\s*require\s+'([^']+)'/).flatten
STDLIB = %w[json fileutils tmpdir open3 set].freeze
module_fehler = 0
requires.each do |name|
  next if STDLIB.include?(name)

  pfad = File.join('game', 'engine', "#{name}.rb")
  next if File.exist?(pfad)

  module_fehler += 1
  fehler("boot.rb require't '#{name}', aber #{pfad} fehlt")
end

vorhanden = Dir.glob('game/engine/*.rb').map { |p| File.basename(p, '.rb') } - %w[boot]
main_rb = File.read('game/main.rb', encoding: 'utf-8')
ungenutzt = vorhanden - requires
ungenutzt = ungenutzt.reject { |m| main_rb.include?("require '#{m}'") }
warnung("Module ohne require in boot.rb/main.rb: #{ungenutzt.join(', ')}") if ungenutzt.any?
ok("#{requires.size} Requires aufgelöst, #{vorhanden.size} Module vorhanden") if module_fehler.zero?

# ------------------------------------------------------- 6. Index und Artefakte
puts '== Versionsverwaltung =='
if git?
  trackte = `git ls-files`.split("\n").map { |z| z.strip.sub(%r{^\./}, '') }.reject(&:empty?)
  info("#{trackte.size} Datei(en) im Index")

  binaer = trackte.select { |p| BINAER_ENDUNGEN.include?(File.extname(p).downcase) }
  if binaer.any?
    warnung("Binär-Artefakte im Index (gehören nach build/): #{binär.join(', ')}")
  else
    ok('keine Binär-Artefakte im Index')
  end

  # Ignorierte Dateien dürfen nicht trackt sein. Ausnahme: der Demo-Spielstand,
  # der bewusst als Beispiel im Repo liegt (siehe .gitignore).
  ausnahme = %w[projekte/dorf/saves/slot1.json]
  ignoriert = trackte.select do |p|
    !ausnahme.include?(p) && system('git', 'check-ignore', '-q', '--', p)
  end
  if ignoriert.any?
    warnung("Trackte Dateien, die .gitignore ausschliesst: #{ignoriert.join(', ')}")
  else
    ok('keine ignorierten Dateien im Index (Demo-Spielstand ist ausgenommen)')
  end
else
  warnung('git nicht verfügbar — Index-Prüfung übersprungen')
end

# ------------------------------------------------------------ 7. Grundausstattung
puts '== Grundausstattung =='
%w[README.md docs/BACKLOG.md docs/HANDOFF_Naechste_KI.md docs/architecture.md
   build.ps1 src/editor/RuneEditor.exe.manifest .github/workflows/ci.yml
   tools/test_all.rb tools/json_test.cpp].each do |p|
  if File.exist?(p)
    ok("#{p} vorhanden")
  else
    fehler("#{p} fehlt")
  end
end

# ------------------------------------------------------------ 8. Auffälliges
puts '== Auffälliges (Info) =='
dateien.each do |p|
  next unless File.extname(p).downcase == '.cpp'

  zeilen = File.readlines(p, encoding: 'utf-8').size
  info("#{p}: #{zeilen} Zeilen (Kandidat zum Aufteilen)") if zeilen > 2000
end

todo = []
dateien.each do |p|
  File.readlines(p, encoding: 'utf-8').each_with_index do |zeile, i|
    todo << "#{p}:#{i + 1}" if zeile.match?(/TODO|FIXME|XXX/)
  end
rescue StandardError
  next
end
if todo.any?
  warnung("offene TODO/FIXME-Markierungen: #{todo.first(8).join(', ')}")
else
  ok('keine TODO/FIXME-Markierungen')
end

puts '-' * 72
puts format('Ergebnis: %d Fehler, %d Warnung(en)%s', $fehler, $warnung, STRICT ? ' (strict)' : '')
exit($fehler.positive? || (STRICT && $warnung.positive?) ? 1 : 0)
