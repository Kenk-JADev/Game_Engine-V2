#encoding: utf-8
# ============================================================================
#  Quellen-Register der Event-Befehle (Etappe Z8).
#
#  docs/befehle.json ist die gemeinsame Quelle. Dieser Test hält die vier
#  Stellen im Baum daran:
#
#    game/engine/mapcheck.rb      CMD_TYPES          (Sprache, MapCheck)
#    game/engine/interpreter.rb   Ausführung         (jeder Typ kommt vor)
#    src/editor/editor_main.cpp   ISSUE_CMD_TYPES    (Issue-Scan)
#    src/editor/event_form.h      EvType + EV_TYPES  (F6-Formular)
#
#  Ein neuer Befehl ist erst fertig, wenn er in ALLEN Stellen steht — der Test
#  zeigt die Lücke vor dem Commit statt im Editor.
#     ruby tools/befehle_test.rb
# ============================================================================
require 'json'

ROOT = File.expand_path('..', __dir__)
Dir.chdir(ROOT)

$fail = 0
def check(name)
  ok = yield
  puts(ok ? "  ok   #{name}" : "  FAIL #{name}")
  $fail += 1 unless ok
  ok
end

def lies(pfad)
  File.read(pfad, encoding: 'UTF-8')
rescue SystemCallError => e
  puts "  FAIL #{pfad}: #{e.message}"
  $fail += 1
  ''
end

# Zeichenketten eines C++-Blocks. Befehlsnamen sind reines ASCII ohne
# Sonderzeichen, daher genügt das einfache Muster ohne Escape-Behandlung.
def cpp_namen(block)
  block.to_s.scan(/"([^"\\]*)"/).flatten
end

# Text zwischen einer Startzeile und dem ersten '};' dahinter.
def cpp_block(text, start)
  i = text.index(start)
  return nil unless i
  j = text.index('};', i + start.size)
  return nil unless j
  text[(i + start.size)...j]
end

# Mengenvergleich mit konkreter Meldung; true, wenn beide Listen gleich sind.
def vergleich(label, quelle, register)
  fehlt = register - quelle
  extra = quelle - register
  puts "    #{label}: fehlt #{fehlt.join(', ')}" unless fehlt.empty?
  puts "    #{label}: zusätzlich #{extra.join(', ')}" unless extra.empty?
  fehlt.empty? && extra.empty?
end

# --- Register ---------------------------------------------------------------
register = begin
  JSON.parse(lies('docs/befehle.json'))
rescue StandardError => e
  puts "  FAIL docs/befehle.json: #{e.class}: #{e.message}"
  $fail += 1
  {}
end

roh = register['befehle']
check('Register: befehle ist eine nichtleere Liste') { roh.is_a?(Array) && !roh.empty? }
roh = [] unless roh.is_a?(Array)
check('Register: jeder Eintrag ist ein Objekt mit ASCII-Typ') do
  !roh.empty? && roh.all? { |b| b.is_a?(Hash) && b['typ'].to_s.match?(/\A[a-z][a-z0-9_]*\z/) }
end
befehle = roh.select { |b| b.is_a?(Hash) }
typen = befehle.map { |b| b['typ'].to_s }
check('Register: keine doppelten Typen') { !typen.empty? && typen.uniq.size == typen.size }

bereiche = register['bereiche'].is_a?(Array) ? register['bereiche'] : []
check('Register: bereiche ist eine eindeutige Liste') do
  bereiche.size >= 2 && bereiche.uniq.size == bereiche.size
end
check('Register: jeder Eintrag nennt einen bekannten Bereich') do
  befehle.all? { |b| bereiche.include?(b['bereich']) }
end
check('Register: jeder Bereich wird von einem Befehl genutzt') do
  genutzt = befehle.map { |b| b['bereich'] }
  bereiche.all? { |ber| genutzt.include?(ber) }
end
check('Register: pflicht/optional sind String-Listen') do
  befehle.all? do |b|
    ['pflicht', 'optional'].all? do |feld|
      liste = b[feld]
      liste.is_a?(Array) && liste.all? { |x| x.is_a?(String) && !x.empty? }
    end
  end
end
check('Register: kein Feld steht doppelt in pflicht und optional') do
  befehle.all? { |b| (Array(b['pflicht']) & Array(b['optional'])).empty? }
end
check('Register: jede Beschreibung ist erklärender Text') do
  befehle.all? { |b| b['beschreibung'].is_a?(String) && b['beschreibung'].length >= 20 }
end

# --- 1. MapCheck::CMD_TYPES -------------------------------------------------
mapcheck = lies('game/engine/mapcheck.rb')
cmd_block = mapcheck[/(CMD_TYPES\s*=\s*%w\[)(.*?)(\]\s*\.freeze)/m, 2]
cmd_types = cmd_block ? cmd_block.split : []
check('MapCheck: CMD_TYPES gefunden und passt zum Register') do
  !cmd_types.empty? && vergleich('CMD_TYPES', cmd_types, typen)
end

# --- 2. F6-Formular: EvType + EV_TYPES --------------------------------------
form = lies('src/editor/event_form.h')
ev_types = cpp_namen(cpp_block(form, 'EV_TYPES[ET_COUNT] = {'))
enum_block = form[/enum\s+EvType\s*\{(.*?)\}/m, 1].to_s
et_namen = enum_block.split(',').map { |e| e.split('=').first.to_s.strip }
                     .select { |n| n.start_with?('ET_') } - ['ET_COUNT']
check('F6: EV_TYPES gefunden und passt zum Register') do
  !ev_types.empty? && vergleich('EV_TYPES', ev_types, typen)
end
check('F6: EvType hat für jeden Typ genau einen Eintrag') do
  et_namen.size == ev_types.size && !et_namen.empty?
end

# --- 3. Issue-Scan im Editor ------------------------------------------------
main_cpp = lies('src/editor/editor_main.cpp')
issue_types = cpp_namen(cpp_block(main_cpp, 'ISSUE_CMD_TYPES[] = {'))
check('Issue-Scan: ISSUE_CMD_TYPES gefunden und passt zum Register') do
  !issue_types.empty? && vergleich('ISSUE_CMD_TYPES', issue_types, typen)
end

# --- 4. Ausführung ----------------------------------------------------------
interpreter = lies('game/engine/interpreter.rb')
fehlend = typen.reject { |t| interpreter.include?("'#{t}'") }
puts "    Ausführung: fehlt #{fehlend.join(', ')}" unless fehlend.empty?
check('Interpreter: jeder Register-Typ kommt in interpreter.rb vor') { fehlend.empty? }

# --- 5. Gemeinsame Reihenfolge ---------------------------------------------
# Die vier Stellen halten die Register-Reihenfolge ein, damit ein Diff zweier
# Listen Zeile für Zeile lesbar bleibt (neu angehängte Typen ans Ende).
check('Reihenfolge überall wie im Register') do
  [cmd_types, ev_types, issue_types].all? { |liste| liste == typen }
end

puts
puts "Register: #{typen.size} Befehle, #{befehle.map { |b| b['bereich'] }.uniq.size} Bereiche"
puts($fail.zero? ? 'BEFEHLS-REGISTER OK (0 Fehler)' : "BEFEHLS-REGISTER FEHLGESCHLAGEN (#{$fail} Fehler)")
exit($fail.zero? ? 0 : 1)
