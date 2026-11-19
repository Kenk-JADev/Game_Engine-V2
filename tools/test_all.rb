#encoding: utf-8
# ============================================================================
#  Zentrale Test-Suite (Etappe 19a).
#
#     ruby tools/test_all.rb                 alle Tests, die hier laufen koennen
#     ruby tools/test_all.rb -v              Ausgaben der Tests immer anzeigen
#     ruby tools/test_all.rb --strict        Uebersprungene Tests = Fehlschlag
#     ruby tools/test_all.rb --only party    nur Tests, deren Name passt
#     ruby tools/test_all.rb --list          nur auflisten, nichts ausfuehren
#
#  Warum das Skript? Bisher stand die Liste der 16 Tests als einzeiliger Befehl
#  im Handoff — jeder neue Test musste dort von Hand eingetragen werden, und
#  tests, die eine gebaute EXE brauchen (tex_import_test.rb), kippten auf einem
#  frischen Checkout mit einer raise-Meldung um. Hier gibt es eine Quelle:
#  gefunden wird per Glob, Voraussetzungen stehen in VORAUSSETZUNGEN, und der
#  Exit-Code ist 0 genau dann, wenn kein Test fehlschlug.
#
#  Laeuft unter Windows (build.ps1 -Test) und in CI auf Linux/macOS.
# ============================================================================
require 'rbconfig'

ROOT = File.expand_path('..', __dir__)

# GitHub Actions schreibt Meldungen aus dem Protokoll in die Oberfläche
# (Annotations) — dort sieht man den Grund, ohne das Log aufklappen zu müssen.
# Ausserhalb von CI (GITHUB_ACTIONS nicht gesetzt) passiert nichts.
# Eigenschaften (title=...) brauchen das volle Escaping, die Nachricht selbst
# nur % / CR / LF — sonst stehen %2C und %3A sichtbar in der Meldung.
def ci_wert(text)
  text.to_s.gsub('%', '%25').gsub("\r", '%0D').gsub("\n", '%0A')
          .gsub(':', '%3A').gsub(',', '%2C')
end

def ci_text(text)
  text.to_s.gsub('%', '%25').gsub("\r", '%0D').gsub("\n", '%0A')
end

def ci_meldung(stufe, titel, text)
  return unless ENV['GITHUB_ACTIONS'] == 'true'

  puts "::#{stufe} title=#{ci_wert(titel)}::#{ci_text(text)}"
end

# Aus der Testausgabe die wichtigen Zeilen holen (FAIL-Zeilen + letzte Zeile).
def ci_auszug(ausgabe, maximal = 1200)
  zeilen = ausgabe.to_s.lines.map(&:rstrip)
  treffer = zeilen.select { |z| z.include?('FAIL') || z.include?('Error') || z.include?('error') }
  treffer = zeilen.last(6) if treffer.empty?
  text = treffer.first(12).join("\n")
  text = text[0, maximal] + ' [...]' if text.size > maximal
  text
end

# Tests, die mehr brauchen als Ruby: Pfad relativ zu ROOT. Fehlt eine
# Voraussetzung, wird der Test uebersprungen (mit Grund) statt rot zu werden —
# auf dem Build-Rechner bzw. im Windows-Job der CI ist er dann dabei.
VORAUSSETZUNGEN = {
  'tex_import_test.rb' => [File.join('build', 'RuneEditor.exe')]
}.freeze

optionen = { verbose: false, strict: false, only: nil, list: false }
args = ARGV.dup
until args.empty?
  arg = args.shift
  case arg
  when '-v', '--verbose' then optionen[:verbose] = true
  when '--strict'        then optionen[:strict] = true
  when '--list'          then optionen[:list] = true
  when '--only'          then optionen[:only] = args.shift
  when '-h', '--help'
    puts File.read(__FILE__, encoding: 'utf-8').lines.grep(/^#\s{2,4}/).map { |l| l.sub(/^#\s*/, '') }.join
    exit 0
  else
    warn "Unbekannte Option: #{arg} (--help zeigt die Optionen)"
    exit 2
  end
end

Dir.chdir(ROOT)
tests = Dir[File.join('tools', '*_test.rb')].sort.map { |p| File.basename(p) }
tests.select! { |t| t.include?(optionen[:only]) } if optionen[:only]

if tests.empty?
  warn 'Keine Tests gefunden (tools/*_test.rb).'
  exit(optionen[:only] ? 1 : 0)
end

if optionen[:list]
  tests.each do |t|
    bedarf = VORAUSSETZUNGEN[t]
    puts(bedarf ? "#{t}  (braucht: #{bedarf.join(', ')})" : t)
  end
  exit 0
end

# Ein Test laeuft als eigener Prozess: ein Absturz (exit!/Speicherfehler) faellt
# dann nicht die ganze Suite, und die Stubs der Tests bleiben getrennt.
def test_laeuft?(datei)
  fehlend = (VORAUSSETZUNGEN[datei] || []).reject { |p| File.exist?(File.join(ROOT, p)) }
  fehlend.empty? ? nil : fehlend
end

puts "Rune3D-Testsuite: #{tests.size} Test-Datei(en), Ruby #{RUBY_VERSION} (#{RUBY_PLATFORM})"
puts '-' * 68

ergebnis = []
tests.each do |t|
  pfad = File.join('tools', t)
  fehlend = test_laeuft?(t)

  if fehlend
    puts format('%-24s UEBERSPRUNGEN  (fehlt: %s)', t, fehlend.join(', '))
    ci_meldung('warning', "Test uebersprungen: #{t}",
               "Voraussetzung fehlt: #{fehlend.join(', ')}. Build mit .\\build.ps1, dann erneut laufen lassen.")
    ergebnis << [:skip, t, 0.0, "fehlt: #{fehlend.join(', ')}"]
    next
  end

  start = Time.now
  ausgabe = IO.popen([RbConfig.ruby, '-I', File.join('game', 'engine'), pfad],
                     err: [:child, :out], &:read)
  status = $?.exitstatus
  dauer = Time.now - start

  if status.zero?
    puts format('%-24s OK      %6.2f s', t, dauer)
    ergebnis << [:ok, t, dauer, '']
  else
    puts format('%-24s FEHLER  %6.2f s  (Exit %s)', t, dauer, status.inspect)
    puts ausgabe.to_s.gsub(/^/, '    | ')
    ci_meldung('error', "Test fehlgeschlagen: #{t}", ci_auszug(ausgabe))
    ergebnis << [:fail, t, dauer, ausgabe.to_s]
  end

  puts ausgabe.to_s.gsub(/^/, '    | ') if optionen[:verbose] && status.zero?
end

ok    = ergebnis.count { |e| e[0] == :ok }
kaputt = ergebnis.count { |e| e[0] == :fail }
ueber = ergebnis.count { |e| e[0] == :skip }
zeit  = ergebnis.sum { |e| e[2] }

puts '-' * 68
puts format('Ergebnis: %d OK, %d fehlgeschlagen, %d uebersprungen (%.1f s)',
            ok, kaputt, ueber, zeit)
ci_meldung(kaputt.positive? ? 'error' : 'notice', 'Rune3D-Testsuite',
           format('%d OK, %d fehlgeschlagen, %d uebersprungen (%.1f s, Ruby %s)',
                  ok, kaputt, ueber, zeit, RUBY_VERSION))
if ueber.positive? && !optionen[:strict]
  puts 'Hinweis: uebersprungene Tests brauchen eine gebaute EXE (build.ps1).'
end

exit(kaputt.positive? || (optionen[:strict] && ueber.positive?) ? 1 : 0)
