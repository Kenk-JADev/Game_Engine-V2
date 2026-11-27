#encoding: utf-8
# ==============================================================================
# Kopfloser Test des CommandValidator (Etappe 47).
#   ruby -I game/engine tools/event_command_validator_test.rb
# Prüft: IDs, Asset-Keys, Block-/Indent-Konsistenz, Sprungmarken — und dass der
# Validator den bestehenden Loader als einzige Prüfquelle wiederverwendet.
# ==============================================================================

require 'json'
require 'rpg'

V = Rpg::EventCommands::CommandValidator
$pruefungen = 0
$fehler = 0

def check(text)
  $pruefungen += 1
  ok = yield
  puts(ok ? "  ok   #{text}" : "  FAIL #{text}")
  $fehler += 1 unless ok
  ok
end

def tief_kopie(obj) = Marshal.load(Marshal.dump(obj))

def finde(commands, typ)
  commands.each do |c|
    return c if c['commandType'] == typ
    (c['blocks'] || {}).each_value do |liste|
      treffer = finde(liste, typ)
      return treffer if treffer
    end
  end
  nil
end

KATALOG = Rpg.load_event_commands
ROH = JSON.parse(File.read('game/config/event_commands.json', encoding: 'UTF-8'))['programs'][0]['commands']
RES = KATALOG.resources

# --- gültiges Programm (neue Beispielwerte) ----------------------------------
check('gültiges Programm hat keine Befunde') { V.validate(ROH, resources: RES).empty? }
check('valid? ist true') { V.valid?(ROH, resources: RES) }

# --- unbekannte Switch-ID ----------------------------------------------------
bad = tief_kopie(ROH)
sw = finde(bad, 'control_switches')
sw['params']['from_id'] = 999
sw['params']['to_id'] = 999
check('unbekannte Switch-ID wird gemeldet') do
  V.validate(bad, resources: RES).any? { |m| m.include?('999') || m.include?('switches') }
end

# --- unbekannter Asset-Key ---------------------------------------------------
bad = tief_kopie(ROH)
bgm = finde(bad, 'change_battle_bgm')
bgm['params']['bgm_key'] = 'bgm_gibtsnicht'
check('unbekannter Asset-Key wird gemeldet') do
  V.validate(bad, resources: RES).any? { |m| m.include?('bgm_gibtsnicht') }
end

# --- inkonsistenter indent-Level ---------------------------------------------
bad = tief_kopie(ROH)
branch = finde(bad, 'conditional_branch')
branch['blocks']['then'][0]['indent'] = 0   # muss indent+1 = 1 sein
check('falscher indent-Level wird gemeldet') do
  V.validate(bad, resources: RES).any? { |m| m.downcase.include?('indent') }
end

# --- unaufloesbare Sprungmarke -----------------------------------------------
bad = tief_kopie(ROH)
jump = finde(bad, 'jump_to_label')
jump['params']['name'] = 'marke_gibtsnicht'
check('fehlende Sprungmarke wird gemeldet') do
  V.validate(bad, resources: RES).any? { |m| m.include?('marke_gibtsnicht') }
end

# --- Ressourcen-Index aus database.json --------------------------------------
DB = {
  'switches' => [{ 'id' => 101, 'name' => 'Tor_Smaragdwald_Offen' }],
  'variables' => [{ 'id' => 205, 'name' => 'Alchemie_Punkte' }],
  'maps' => [{ 'id' => 7, 'name' => 'Smaragdwald' }],
  'actors' => [{ 'id' => 1, 'name' => 'Kael' }],
  'items' => [{ 'id' => 3, 'name' => 'Heiltrank' }],
  'troops' => [{ 'id' => 9, 'name' => 'Waldweg' }],
  'common_events' => [{ 'id' => 9, 'name' => 'Lagerfeuer' }]
}.freeze
ASSETS = { 'bgms' => ['bgm_ancient_citadel'], 'ses' => ['se_crystal_chime'] }.freeze
INDEX = V.resources_from_database(DB, assets: ASSETS)

check('Index enthält alle Pflicht-Sammlungen') do
  Rpg::EventCommands::Loader::RESOURCE_KEYS.all? { |k| INDEX.key?(k) }
end
check('Index mappt database-IDs') { INDEX['switches'].first['id'] == 101 }
check('Index mappt Asset-Keys') { INDEX['bgms'].first['key'] == 'bgm_ancient_citadel' }

klein = [
  { 'commandType' => 'control_switches', 'code' => 121, 'indent' => 0,
    'params' => { 'from_id' => 101, 'to_id' => 101, 'value' => true } },
  { 'commandType' => 'play_bgm', 'code' => 241, 'indent' => 0,
    'params' => { 'bgm_key' => 'bgm_ancient_citadel', 'volume' => 80, 'pitch' => 100 } },
  { 'commandType' => 'play_se', 'code' => 250, 'indent' => 0,
    'params' => { 'se_key' => 'se_crystal_chime', 'volume' => 70, 'pitch' => 100 } }
]
check('database-Index: gültiges Kleinprogramm') { V.validate(klein, resources: INDEX).empty? }

bad = tief_kopie(klein)
bad[0]['params']['from_id'] = 4242
bad[0]['params']['to_id'] = 4242
check('database-Index: unbekannte ID gemeldet') do
  V.validate(bad, resources: INDEX).any? { |m| m.include?('4242') }
end

bad = tief_kopie(klein)
bad[1]['params']['bgm_key'] = 'bgm_fehlt'
check('database-Index: unbekannter Asset-Key gemeldet') do
  V.validate(bad, resources: INDEX).any? { |m| m.include?('bgm_fehlt') }
end

# --- Rpg.validate_event_commands Komfort-API ---------------------------------
check('Rpg.validate_event_commands liefert Befunde') do
  Rpg.validate_event_commands(bad, resources: INDEX).any?
end

puts
puts "CommandValidator: #{$pruefungen} Prüfungen, #{$fehler} Fehler"
puts($fehler.zero? ? 'COMMAND-VALIDATOR OK' : 'COMMAND-VALIDATOR FEHLGESCHLAGEN')
exit($fehler.zero? ? 0 : 1)
