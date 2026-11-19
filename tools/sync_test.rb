#encoding: utf-8
# ============================================================================
#  Sync-Test (Etappe 19j): die Doppel-Definitionen zwischen Ruby und C++
#  maschinell abgleichen. Bisher war die „Sync-Pflicht" (HANDOFF) Handarbeit:
#  MapCheck::CMD_TYPES (Ruby) musste zu ISSUE_CMD_TYPES + EV_TYPES (C++)
#  passen,Trigger/Operatoren/Wetter/Self-Switches ebenfalls. Dieser Test
#  parst src/editor/editor_main.cpp als Text und vergleicht die Listen —
#  jede neue Befehlsart kippt hier, solange eine Seite vergessen wurde.
#
#  Kopflos ohne Engine-Stubs lauffähig (MapCheck braucht nur json):
#     ruby tools/sync_test.rb
#  Exit-Code 0 = alles grün. Läuft über tools/test_all.rb in der CI.
# ============================================================================
require 'json'

ROOT = File.expand_path('..', __dir__)
$LOAD_PATH.unshift(File.join(ROOT, 'game', 'engine'))
require 'mapcheck'

CPP    = File.read(File.join(ROOT, 'src', 'editor', 'editor_main.cpp'),
                   encoding: 'utf-8')
INTERP = File.read(File.join(ROOT, 'game', 'engine', 'interpreter.rb'),
                   encoding: 'utf-8')
DATEN  = JSON.parse(File.read(File.join(ROOT, 'game', 'data', 'database.json'),
                              encoding: 'utf-8'))

$fail = 0
def check(name)
  ok = yield
  puts(ok ? "  ok   #{name}" : "  FAIL #{name}")
  $fail += 1 unless ok
rescue StandardError => e
  puts "  FAIL #{name} (Ausnahme: #{e.class}: #{e.message})"
  $fail += 1
end

# --- Extraktions-Helfer für die C++-Quelle ----------------------------------

# Benanntes Array: const char* const NAME[] = { "a", "b" };
# bzw. const wchar_t* const NAME[ET_COUNT] = { L"a", ... };
def c_array(name, wide: false)
  muster = /const\s+(?:char|wchar_t)\*\s*const\s+#{Regexp.escape(name)}\s*\[[^\]]*\]\s*=\s*\{(.*?)\};/m
  treffer = CPP.match(muster)
  raise "C++-Array #{name} nicht gefunden" unless treffer
  treffer[1].scan(wide ? /L"([^"]*)"/ : /"([^"]*)"/).flatten
end

# Enum-Einträge in Reihenfolge: enum EvType { ET_TEXT = 0, ... };
def c_enum(name)
  treffer = CPP.match(/enum\s+#{Regexp.escape(name)}\s*\{(.*?)\};/m)
  raise "C++-Enum #{name} nicht gefunden" unless treffer
  treffer[1].scan(/\bET_([A-Z0-9_]+)\b/).flatten
end

# Inline-Liste: for (... : { L"kein", L"regen", L"schnee" }).
# Zwei Anker, weil z. B. „kein" auch in der Materialmuster-Liste vorkommt.
def c_inline_liste(first, second)
  treffer = CPP.match(/\{\s*(L"#{Regexp.escape(first)}",\s*L"#{Regexp.escape(second)}"[^}]*)\}/)
  raise "C++-Inline-Liste #{first}/#{second} nicht gefunden" unless treffer
  treffer[1].scan(/L"([^"]*)"/).flatten
end

# when-Zeilen fester Einrückung im Interpreter (6 Leerzeichen = Befehls-case,
# 4 Leerzeichen = Vergleichsoperator-case). Nur das erste Literal pro Zeile -
# dahinter steht oft schon Code mit eigenen Quotes (cmd['name'] ...).
def interp_whens(einzug)
  INTERP.lines.filter_map { |z| z[/^ {#{einzug}}when\s+'([^']+)'/, 1] }
end

puts '== Befehlstypen: Ruby == C++ =='
cmd_ruby  = MapCheck::CMD_TYPES
ev_types  = c_array('EV_TYPES')
issue_cmd = c_array('ISSUE_CMD_TYPES')

check("EV_TYPES (C++) == MapCheck::CMD_TYPES (#{cmd_ruby.size} Typen, Reihenfolge)") do
  ev_types == cmd_ruby
end
check('ISSUE_CMD_TYPES (C++) == MapCheck::CMD_TYPES (als Menge)') do
  issue_cmd.sort == cmd_ruby.sort
end
check('EV_TYPE_LABELS hat genau einen Eintrag pro Befehlstyp') do
  c_array('EV_TYPE_LABELS', wide: true).size == cmd_ruby.size
end
check('Enum EvType passt 1:1 (Name + Reihenfolge) zu EV_TYPES') do
  c_enum('EvType').reject { |e| e == 'COUNT' }.map { |e| e.downcase } == ev_types
end
check('Interpreter behandelt jeden Befehlstyp (when-Zeilen, 6er-Einzug)') do
  interp_whens(6).sort == cmd_ruby.sort
end

puts '== Kleine Vokabular-Listen =='
check('EV_TRIGGERS (C++) == MapCheck::TRIGGER') do
  c_array('EV_TRIGGERS', wide: true) == MapCheck::TRIGGER
end
check('EV_OPS (C++) == MapCheck::VERGLEICH') do
  c_array('EV_OPS', wide: true) == MapCheck::VERGLEICH
end
check('Interpreter-Vergleiche (when-Zeilen, 4er-Einzug) == MapCheck::VERGLEICH') do
  interp_whens(4).sort == MapCheck::VERGLEICH.sort
end
check('Wetter-Combo (C++) == MapCheck::WETTER') do
  c_inline_liste('kein', 'regen') == MapCheck::WETTER
end
check('Self-Switch-Combo (C++) == keins + MapCheck::SELF_SWITCH_BUCHSTABEN') do
  c_inline_liste('keins', 'A') == ['keins'] + MapCheck::SELF_SWITCH_BUCHSTABEN
end
check('Effekt-Beschriftung (C++) nennt alle MapCheck::EFFEKTE') do
  beschriftung = CPP[/L"Effekt \(([^)]*)\)"/, 1].to_s
  MapCheck::EFFEKTE.all? { |e| beschriftung.include?(e) }
end

puts '== Datenbank-Dialog =='
check('Dialog-Felder KEYS (C++) existieren in database.json (actors)') do
  keys = c_array('KEYS')
  actor_keys = DATEN.fetch('actors', []).flat_map(&:keys).uniq
  keys.all? { |k| actor_keys.include?(k) } && keys.first == 'id'
end
check('ACTOR_LABELS: ein Label pro Dialog-Feld') do
  c_array('ACTOR_LABELS', wide: true).size == c_array('KEYS').size
end
check('ITEM_LABELS: ein Label pro Dialog-Feld') do
  c_array('ITEM_LABELS', wide: true).size == c_array('KEYS').size
end

puts '== Gegner/Truppen seit 19i =='
check('database.json hat enemies und troops mit Pflichtfeldern') do
  enemies = DATEN.fetch('enemies', [])
  troops  = DATEN.fetch('troops', [])
  !enemies.empty? && !troops.empty? &&
    enemies.all? { |e| %w[id name hp atk def].all? { |k| e.key?(k) } } &&
    troops.all? { |t| t.key?('id') && t['enemies'].is_a?(Array) }
end
check('jede Truppe nennt nur bekannte Gegner') do
  ids = DATEN.fetch('enemies', []).map { |e| e['id'] }
  DATEN.fetch('troops', []).all? { |t| (t['enemies'] - ids).empty? }
end

puts($fail.zero? ? 'SYNC-TEST OK (0 Fehler)' : "SYNC-TEST FEHLGESCHLAGEN (#{$fail} Fehler)")
exit($fail.zero? ? 0 : 1)
