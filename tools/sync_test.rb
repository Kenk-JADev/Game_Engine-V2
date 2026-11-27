#encoding: utf-8
# ============================================================================
#  Sync-Test (Etappe 19j): die Doppel-Definitionen zwischen Ruby und C++
#  maschinell abgleichen. Bisher war die „Sync-Pflicht" (HANDOFF) Handarbeit:
#  MapCheck::CMD_TYPES (Ruby) musste zu ISSUE_CMD_TYPES + EV_TYPES (C++)
#  passen,Trigger/Operatoren/Wetter/Self-Switches ebenfalls. Dieser Test
#  parst editor_main.cpp und event_form.h als Text und vergleicht die Listen —
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

CPP    = %w[editor_main.cpp event_form.h].map do |name|
  File.read(File.join(ROOT, 'src', 'editor', name), encoding: 'utf-8')
end.join("\n")
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
check('Variable-Operationen (C++ EVENT_VAR_OPS) == MapCheck::VARIABLE_OPS') do
  c_array('EVENT_VAR_OPS') == MapCheck::VARIABLE_OPS
end
check('Variable-Quellen (C++ EVENT_SOURCES) == MapCheck::VARIABLE_QUELLEN') do
  c_array('EVENT_SOURCES') == MapCheck::VARIABLE_QUELLEN
end
check('Quellen-Combo in editor_main.cpp nennt dieselben Quellen') do
  main = File.read(File.join(ROOT, 'src', 'editor', 'editor_main.cpp'), encoding: 'utf-8')
  liste = main[/EV_SOURCES\[5\] = \{([^}]*)\}/, 1].to_s
  liste.scan(/L"([^"]+)"/).flatten == MapCheck::VARIABLE_QUELLEN
end
check('Heldenwert-Quellen (F6-Modell) == MapCheck::ACTOR_WERT_QUELLEN') do
  c_array('EVENT_ACTOR_SOURCES') == MapCheck::ACTOR_WERT_QUELLEN
end
check('Heldenwert-Quellen (Editor-Combo) == MapCheck::ACTOR_WERT_QUELLEN') do
  c_array('EV_ACTOR_SOURCES', wide: true) == MapCheck::ACTOR_WERT_QUELLEN
end
check('party_member-Modi (C++-Moderation) == MapCheck::PARTY_MODI') do
  form = File.read(File.join(ROOT, 'src', 'editor', 'event_form.h'), encoding: 'utf-8')
  # Die Moderation in event_form.h prüft genau diese beiden Wörter.
  form.include?('modus != "add" && modus != "remove"') &&
    MapCheck::PARTY_MODI == %w[add remove]
end
check('Combos in editor_main.cpp nennen dieselben Variable-Operationen') do
  main = File.read(File.join(ROOT, 'src', 'editor', 'editor_main.cpp'), encoding: 'utf-8')
  liste = main[/EV_VAR_OPS\[6\] = \{([^}]*)\}/, 1].to_s
  liste.scan(/L"([^"]+)"/).flatten == MapCheck::VARIABLE_OPS
end
check('portables F6-Modell verwendet dieselben Trigger und Operatoren wie die Win32-Combos') do
  c_array('EVENT_TRIGGERS') == c_array('EV_TRIGGERS', wide: true) &&
    c_array('EVENT_OPS') == c_array('EV_OPS', wide: true)
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

puts '== Datenbank-Dialog (F9) =='
# Seit Etappe 41 steht der Register-Katalog portabel in database_catalog.h:
# 13 Bereiche wie RPG Maker XP, pro Bereich eine Feldtabelle. Dieser Test
# vergleicht sie mit der echten database.json und mit dem Vokabular der
# Engine (battle.rb, common_events.rb), damit der Dialog nichts ins Leere
# editiert und keine Auswahl anbietet, die das Spiel nicht versteht.
KATALOG = File.read(File.join(ROOT, 'src', 'editor', 'database_catalog.h'), encoding: 'utf-8')
BATTLE  = File.read(File.join(ROOT, 'game', 'engine', 'battle.rb'), encoding: 'utf-8')
require 'common_events'

XP_REIHENFOLGE = %w[actors classes skills items weapons armors enemies troops
                    states animations tilesets common_events system].freeze

def katalog_bereiche
  KATALOG.scan(/\{\s*"([a-z_]+)",\s*"([^"]+)",\s*DatabaseAreaKind::(\w+),\s*
                 DatabaseEngineState::(\w+),\s*"([a-z_]+)",\s*(true|false),/x)
         .map do |key, label, art, engine, prefix, numerisch|
    { key: key, label: label, art: art, engine: engine,
      prefix: prefix, numerisch: numerisch == 'true' }
  end
end

# Feldtabelle eines Bereichs; der Kommentar "// <Nummer> <Name>" trennt sie.
def katalog_felder(index)
  block = KATALOG[/\/\/ #{index} [^\n]*\n\s*\{\n(.*?)\n\s*\},/m, 1]
  raise "Feldtabelle #{index} nicht gefunden" unless block
  block.scan(/database_(text|int|decimal|ids|json|choice)\("([a-z_]+)"/)
       .map { |art, key| { art: art, key: key, optional: false } }
       .each_with_index do |feld, i|
    zeile = block.lines.reject { |z| z.strip.start_with?('//') }[i].to_s
    feld[:optional] = zeile.include?('database_optional(')
  end
end

def katalog_werte(name)
  treffer = KATALOG[/#{name}\(\)\s*\{\s*static const std::vector<std::string> values\{(.*?)\};/m, 1]
  raise "Werteliste #{name} nicht gefunden" unless treffer
  treffer.scan(/"([a-z_]+)"/).flatten
end

BEREICHE = katalog_bereiche

check('F9 hat alle 13 XP-Register in XP-Reihenfolge') do
  BEREICHE.map { |b| b[:key] } == XP_REIHENFOLGE
end
check('nur System ist ein Einzeldatensatz, alle anderen sind Listen') do
  BEREICHE.select { |b| b[:art] == 'Object' }.map { |b| b[:key] } == ['system']
end
check('jedes Register hat Felder, Listen beginnen mit der ID') do
  BEREICHE.each_with_index.all? do |bereich, index|
    felder = katalog_felder(index)
    !felder.empty? && (bereich[:art] == 'Object' || felder.first[:key] == 'id')
  end
end
check('Common Events zählen numerisch wie XP, alle anderen benennen IDs') do
  BEREICHE.select { |b| b[:numerisch] }.map { |b| b[:key] } == ['common_events']
end
check('verdrahtete Register editieren nur echte database.json-Schlüssel') do
  BEREICHE.each_with_index.all? do |bereich, index|
    next true unless bereich[:engine] == 'Wired'

    daten_keys = DATEN.fetch(bereich[:key], []).flat_map(&:keys).uniq
    katalog_felder(index).all? { |f| f[:optional] || daten_keys.include?(f[:key]) }
  end
end
check('optionale Felder sind ausdrücklich als optional markiert') do
  optional = BEREICHE.each_with_index.flat_map do |_b, index|
    katalog_felder(index).select { |f| f[:optional] }.map { |f| f[:key] }
  end
  optional == ['switch']
end
check('jedes verdrahtete Register existiert in der Demo-Datenbank') do
  BEREICHE.select { |b| b[:engine] == 'Wired' }.all? { |b| DATEN.key?(b[:key]) }
end

puts '== F9-Auswahlwerte gegen die Engine =='
check('Fertigkeitsziele == Battle.skill_scope (enemy/ally/self)') do
  ziele = BATTLE[/%w\[([a-z ]+)\]\.include\?\(art\)/, 1].to_s.split + ['enemy']
  katalog_werte('database_skill_scopes').sort == ziele.sort
end
check('Zustandsarten == Gift/Regeneration + Boni aus Battler#state_bonus') do
  werte = BATTLE.scan(/state_bonus\('([a-z]+)'\)/).flatten.uniq
  erwartet = %w[poison regen] + werte.flat_map { |w| ["#{w}_up", "#{w}_down"] }
  katalog_werte('database_state_kinds').sort == erwartet.sort
end
check('Common-Event-Auslöser == CommonEvents::TRIGGER') do
  katalog_werte('database_common_triggers') == CommonEvents::TRIGGER
end
check('Katalog bietet jede Art/Ziel/Auslöser der Demo-Daten an') do
  paare = { 'skills' => { 'kind' => 'database_skill_kinds', 'scope' => 'database_skill_scopes' },
            'items' => { 'kind' => 'database_item_kinds' },
            'states' => { 'kind' => 'database_state_kinds' },
            'common_events' => { 'trigger' => 'database_common_triggers' } }
  paare.all? do |sammlung, felder|
    felder.all? do |feld, liste|
      benutzt = DATEN.fetch(sammlung, []).map { |e| e[feld] }.compact.uniq
      (benutzt - katalog_werte(liste)).empty?
    end
  end
end
check('F9 nennt für jede Demo-Fertigkeit einen bekannten Zustand oder keinen') do
  zustaende = DATEN.fetch('states', []).map { |s| s['id'] }
  DATEN.fetch('skills', []).all? { |s| s['state'].to_s.empty? || zustaende.include?(s['state']) }
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

puts '== Fertigkeiten/Zustände seit 20a =='
check('database.json hat skills und states mit Pflichtfeldern') do
  skills = DATEN.fetch('skills', [])
  states = DATEN.fetch('states', [])
  !skills.empty? && !states.empty? &&
    skills.all? { |s| %w[id name mp kind].all? { |k| s.key?(k) } } &&
    states.all? { |s| %w[id name kind turns].all? { |k| s.key?(k) } }
end
check('jede Fertigkeit nennt nur bekannte Zustände') do
  ids = DATEN.fetch('states', []).map { |s| s['id'] }
  DATEN.fetch('skills', []).all? { |s| s['state'].nil? || ids.include?(s['state']) }
end
check('jeder Held lernt nur bekannte Fertigkeiten') do
  ids = DATEN.fetch('skills', []).map { |s| s['id'] }
  DATEN.fetch('actors', []).all? { |a| (Array(a['skills']) - ids).empty? }
end

puts '== Terrain-/Atmosphären-Grenzen =='
limits_cpp = File.read(File.join(ROOT, 'src', 'world', 'terrain_limits.h'), encoding: 'utf-8')
%w[TERRAIN_MIN_SIZE TERRAIN_MAX_SIZE TERRAIN_MIN_RES TERRAIN_MAX_RES TERRAIN_MAX_QUADS
   TERRAIN_MAX_AMPLITUDE TERRAIN_MAX_COORD TERRAIN_MIN_RADIUS TERRAIN_MAX_BRUSHES
   TERRAIN_MAX_BRUSH_SAMPLES TERRAIN_MAX_SPLATS TERRAIN_MAX_SPLAT_CHECKS TERRAIN_MAX_TEXTURE_BYTES
   TERRAIN_MIN_TILE TERRAIN_MAX_TILE FOG_MIN_FAR FOG_MAX_FAR WEATHER_MAX_INTENSITY].each do |name|
  check("#{name}: Ruby == C++") do
    literal = limits_cpp[/\b#{Regexp.escape(name)}\s*=\s*([\d.]+)/, 1]
    literal && literal.to_f == MapCheck.const_get(name).to_f
  end
end
weather_rb = File.read(File.join(ROOT, 'game', 'engine', 'weather.rb'), encoding: 'utf-8')
check('Wetter-Partikelgrenze == MapCheck und C++') do
  weather_rb[/MAX_INTENSITY\s*=\s*([\d.]+)/, 1].to_f == MapCheck::WEATHER_MAX_INTENSITY
end

puts($fail.zero? ? 'SYNC-TEST OK (0 Fehler)' : "SYNC-TEST FEHLGESCHLAGEN (#{$fail} Fehler)")
exit($fail.zero? ? 0 : 1)
