#encoding: utf-8
# ==============================================================================
# Kopfloser Test des Legacy-CommandValidator + MapCheck-Anbindung (Etappe 48).
#   ruby -I game/engine tools/event_command_legacy_validator_test.rb
# Maps/Common Events nutzen das Legacy-Format ({"cmd":...}). Geprüft werden
# Ressourcen-Referenzen (IDs/Asset-Keys), Block-Struktur und die Integration in
# MapCheck.scan / scan_database — plus Kompatibilität der mitgelieferten Daten.
# ==============================================================================

require 'json'
require 'rpg'
require 'mapcheck'
require 'prefabs'

V = Rpg::EventCommands::CommandValidator
$pruefungen = 0
$fehler = 0

def check(text)
  $pruefungen += 1
  ok = yield
  puts(ok ? "  ok   #{text}" : "  FAIL #{text}")
  $fehler += 1 unless ok
  ok
rescue StandardError => e
  $pruefungen += 1
  $fehler += 1
  puts "  FAIL #{text} (#{e.class}: #{e.message})"
end

DB = {
  'actors' => [{ 'id' => 'arik', 'name' => 'Arik' }],
  'items' => [{ 'id' => 'heiltrank', 'name' => 'Heiltrank' }],
  'weapons' => [{ 'id' => 'rune_blade', 'name' => 'Rune Blade', 'key' => 'weapon/rune_blade' }],
  'armors' => [{ 'id' => 'warded_cloak', 'name' => 'Warded Cloak' }],
  'classes' => [{ 'id' => 'vanguard', 'name' => 'Vanguard' }],
  'troops' => [{ 'id' => 'waldweg', 'name' => 'Waldweg' }],
  'states' => [{ 'id' => 'gift', 'name' => 'Gift' }],
  'skills' => [{ 'id' => 'kraftschlag', 'name' => 'Kraftschlag' }],
  'enemies' => [{ 'id' => 'wolf', 'name' => 'Wolf' }],
  'common_events' => [{ 'id' => 1, 'name' => 'hinweis', 'commands' => [] }]
}.freeze
RES = V.resources_from_database(DB)

# --- gültige Legacy-Befehle --------------------------------------------------
gut = [
  { 'cmd' => 'item', 'name' => 'heiltrank', 'count' => 1 },
  { 'cmd' => 'battle', 'name' => 'waldweg' },
  { 'cmd' => 'change_weapons', 'weapon_id' => 'rune_blade', 'amount' => 1, 'operation' => 'add' },
  { 'cmd' => 'change_actor_class', 'actor_id' => 'arik', 'class_id' => 'vanguard' },
  { 'cmd' => 'enemy_transform', 'slot' => 0, 'enemy_id' => 'wolf' }
]
check('gültige Legacy-Befehle: keine Befunde') { V.validate_legacy(gut, resources: RES).empty? }
check('legacy_valid? ist true') { V.legacy_valid?(gut, resources: RES) }

# --- unbekannte Referenzen ---------------------------------------------------
check('unbekanntes Item wird gemeldet') do
  V.validate_legacy([{ 'cmd' => 'item', 'name' => 'phantom' }], resources: RES)
   .any? { |m| m.include?('items') && m.include?('phantom') }
end
check('unbekannte Waffe wird gemeldet') do
  V.validate_legacy([{ 'cmd' => 'change_weapons', 'weapon_id' => 'ghost' }], resources: RES)
   .any? { |m| m.include?('weapons') }
end
check('unbekannter Trupp wird gemeldet') do
  V.validate_legacy([{ 'cmd' => 'battle', 'name' => 'nix' }], resources: RES)
   .any? { |m| m.include?('troops') }
end
check('unbekannte Klasse wird gemeldet') do
  V.validate_legacy([{ 'cmd' => 'change_actor_class', 'actor_id' => 'arik', 'class_id' => 'leer' }], resources: RES)
   .any? { |m| m.include?('classes') }
end
check('Referenz über Name statt ID wird akzeptiert') do
  V.validate_legacy([{ 'cmd' => 'item', 'name' => 'Heiltrank' }], resources: RES).empty?
end
check('leere Referenz wird nicht gemeldet') do
  V.validate_legacy([{ 'cmd' => 'state', 'actor' => '' }], resources: RES).empty?
end
check('nicht geführte Sammlung (switches) wird nicht gemeldet') do
  V.validate_legacy([{ 'cmd' => 'switch', 'name' => '101' }], resources: RES).empty?
end

# --- Block-Struktur ----------------------------------------------------------
check('then muss eine Liste sein') do
  V.validate_legacy([{ 'cmd' => 'if_switch', 'name' => '101', 'then' => 'kaputt' }], resources: RES)
   .any? { |m| m.include?('then') }
end
check('branches muss eine Liste von Zweigen sein') do
  V.validate_legacy([{ 'cmd' => 'choices', 'branches' => 'kaputt' }], resources: RES)
   .any? { |m| m.include?('branches') }
end
check('Referenz im Dann-Zweig wird geprüft') do
  cmds = [{ 'cmd' => 'if_switch', 'name' => '101',
            'then' => [{ 'cmd' => 'item', 'name' => 'phantom' }] }]
  V.validate_legacy(cmds, resources: RES).any? { |m| m.include?('items') }
end
check('Referenz im Wahl-Zweig wird geprüft') do
  cmds = [{ 'cmd' => 'choices', 'branches' => [[{ 'cmd' => 'battle', 'name' => 'nix' }]] }]
  V.validate_legacy(cmds, resources: RES).any? { |m| m.include?('troops') }
end
check('conditional_branch switch_id wird geprüft') do
  cmds = [{ 'cmd' => 'conditional_branch', 'kind' => 'item', 'item_id' => 'phantom' }]
  V.validate_legacy(cmds, resources: RES).any? { |m| m.include?('items') }
end

# --- MapCheck-Integration ----------------------------------------------------
karte = {
  'name' => 'Test', 'terrain' => { 'width' => 120.0, 'height' => 90.0 },
  'events' => [{ 'id' => 1, 'name' => 'Händler', 'x' => 0.0, 'z' => 0.0,
                 'commands' => [{ 'cmd' => 'item', 'name' => 'phantom' }] }]
}
funde = MapCheck.scan(karte, name: 'test', resources: RES)
check('MapCheck.scan meldet unbekannte Item-Referenz') do
  funde.any? { |f| f.fehler? && f.to_s.include?('items') && f.to_s.include?('phantom') }
end
funde_ohne = MapCheck.scan(karte, name: 'test')
check('MapCheck.scan ohne Index meldet die Referenz nicht') do
  funde_ohne.none? { |f| f.to_s.include?('items-Referenz') }
end

db_bad = { 'common_events' => [{ 'id' => 1, 'name' => 'hinweis', 'trigger' => 'none',
                                 'commands' => [{ 'cmd' => 'item', 'name' => 'phantom' }] }],
           'items' => DB['items'] }
db_funde = MapCheck.scan_database(db_bad, name: 'db')
check('MapCheck.scan_database meldet Common-Event-Referenz') do
  db_funde.any? { |f| f.fehler? && f.to_s.include?('items') }
end

# --- Kompatibilität der mitgelieferten Daten ---------------------------------
root = File.expand_path('..', __dir__)
ship_db = JSON.parse(File.read(File.join(root, 'game/data/database.json'), encoding: 'UTF-8'))
ship_res = V.resources_from_database(ship_db)
ship_map = JSON.parse(File.read(File.join(root, 'game/data/maps/dorf.json'), encoding: 'UTF-8'))
ship_befunde = []
Array(ship_map['events']).each do |ev|
  ship_befunde.concat(V.validate_legacy(ev['commands'], resources: ship_res, name: ev['name'].to_s)) if ev['commands'].is_a?(Array)
end
Array(ship_map['npcs']).each do |n|
  ship_befunde.concat(V.validate_legacy(n['commands'], resources: ship_res, name: n['name'].to_s)) if n['commands'].is_a?(Array)
end
Array(ship_db['common_events']).each do |ce|
  ship_befunde.concat(V.validate_legacy(ce['commands'], resources: ship_res, name: ce['name'].to_s)) if ce['commands'].is_a?(Array)
end
check('mitgelieferte Karte + Common Events sind referenz-sauber') { ship_befunde.empty? }

puts
puts "Legacy-CommandValidator: #{$pruefungen} Prüfungen, #{$fehler} Fehler"
puts($fehler.zero? ? 'LEGACY-VALIDATOR OK' : 'LEGACY-VALIDATOR FEHLGESCHLAGEN')
exit($fehler.zero? ? 0 : 1)
