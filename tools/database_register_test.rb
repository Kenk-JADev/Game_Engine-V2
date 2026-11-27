#encoding: utf-8
# ============================================================================
#  Kopfloser Test für die 13 F9-Register aus Etappe 41 (Datenbank wie XP).
#  Lässt sich ohne Engine starten:
#     ruby -I game/engine tools/database_register_test.rb
#  Exit-Code 0 = alles grün.
#
#  Der Editor schreibt seit Etappe 41 alle 13 XP-Register in EINE Datei
#  (database.json). Dieser Test prüft die Engine-Seite davon:
#    - eine Datenbank mit allen 13 Registern lädt und stört nichts
#    - die sechs vorbereiteten Register (classes/weapons/armors/animations/
#      tilesets/system) bleiben für die Engine wirkungslos
#    - die Vorgaben, die F9 bei "Neu" erzeugt, sind spielbar (Item, Zustand,
#      Fertigkeit, gemeinsames Event) und melden keine MapCheck-Fehler
#    - alte Projekte ohne die neuen Register laufen unverändert
#    - "system" in database.json ist NICHT system.json (Projektstart)
# ============================================================================

# --- Minimal-Attrappe der Engine-Schnittstelle -----------------------------
module Rune
  ROOT = File.expand_path('..', __dir__)

  module Log
    def self.info(m) = nil
    def self.warn(m) = nil
    def self.error(m) = puts("ERROR #{m}")
  end

  module Sound
    def self.play(name, volume = 1.0) = nil
    def self.stop(name) = nil
  end
end

$LOAD_PATH.unshift(File.join(Rune::ROOT, 'game', 'engine'))
require 'data'
require 'state'
require 'issues'
require 'party'
require 'common_events'
require 'battle'
require 'mapcheck'

$fail = 0
def check(name)
  ok = yield
  puts(ok ? "  ok   #{name}" : "  FAIL #{name}")
  $fail += 1 unless ok
rescue StandardError => e
  puts "  FAIL #{name} (Ausnahme: #{e.class}: #{e.message})"
  $fail += 1
end

# Datenbank in den echten Lade-Cache legen: Database.database liest denselben
# Weg wie im Spiel, nur ohne Platte.
def datenbank_setzen(db)
  Database.reset!
  Database.instance_variable_get(:@cache)['database.json'] = db
  db
end

# Vorgaben wie F9 "Neu" (src/editor/database_catalog.h, database_default_entry).
F9_HELD = { 'id' => 'held_1', 'name' => 'held_1', 'class' => 'Krieger', 'level' => 1,
            'hp' => 100, 'mp' => 20, 'atk' => 10, 'def' => 8, 'spi' => 6, 'agi' => 8,
            'exp' => 0, 'skills' => [] }.freeze
F9_ITEM = { 'id' => 'item_1', 'name' => 'item_1', 'kind' => 'heal', 'power' => 20,
            'price' => 10, 'start' => 0, 'desc' => '' }.freeze
F9_SKILL = { 'id' => 'skill_1', 'name' => 'skill_1', 'kind' => 'damage', 'scope' => 'enemy',
             'mp' => 3, 'power' => 10, 'state' => '', 'state_rate' => 1.0, 'turns' => 3,
             'desc' => '' }.freeze
F9_ZUSTAND = { 'id' => 'zustand_1', 'name' => 'zustand_1', 'kind' => 'poison',
               'power' => 5, 'turns' => 3, 'desc' => '' }.freeze
F9_GEGNER = { 'id' => 'gegner_1', 'name' => 'gegner_1', 'hp' => 30, 'mp' => 0,
              'atk' => 10, 'def' => 8, 'spi' => 5, 'agi' => 8, 'exp' => 10, 'gold' => 0,
              'drop' => '', 'drop_rate' => 0.0, 'color' => 13_209_632, 'desc' => '' }.freeze
F9_TRUPP = { 'id' => 'trupp_1', 'name' => 'trupp_1', 'enemies' => ['gegner_1'], 'desc' => '' }.freeze
F9_EVENT = { 'id' => 1, 'name' => 'ereignis_1', 'trigger' => 'none', 'switch' => '',
             'commands' => [] }.freeze

# Die sechs vorbereiteten Register: der Editor pflegt sie, die Engine nicht.
VORBEREITET = {
  'classes' => [{ 'id' => 'klasse_1', 'name' => 'klasse_1', 'position' => 'front',
                  'weapons' => [], 'armors' => [], 'desc' => '' }],
  'weapons' => [{ 'id' => 'waffe_1', 'name' => 'waffe_1', 'kind' => 'sword',
                  'atk' => 5, 'def' => 0, 'spi' => 0, 'agi' => 0, 'price' => 50, 'desc' => '' }],
  'armors' => [{ 'id' => 'ruestung_1', 'name' => 'ruestung_1', 'slot' => 'shield',
                 'atk' => 0, 'def' => 5, 'spi' => 0, 'agi' => 0, 'price' => 50, 'desc' => '' }],
  'animations' => [{ 'id' => 'animation_1', 'name' => 'animation_1', 'sheet' => '',
                     'position' => 'top', 'frames' => 4, 'duration' => 20, 'se' => '', 'desc' => '' }],
  'tilesets' => [{ 'id' => 'tileset_1', 'name' => 'tileset_1', 'texture' => '',
                   'tile' => 1.0, 'splats' => [], 'desc' => '' }],
  'system' => { 'party' => ['held_1'], 'title_bgm' => '', 'battle_bgm' => '',
                'gameover_me' => '', 'cursor_se' => '', 'cancel_se' => '' }
}.freeze

VOLLE_DB = {
  'gold' => 120,
  'actors' => [F9_HELD],
  'skills' => [F9_SKILL],
  'items' => [F9_ITEM],
  'enemies' => [F9_GEGNER],
  'troops' => [F9_TRUPP],
  'states' => [F9_ZUSTAND],
  'common_events' => [F9_EVENT]
}.merge(VORBEREITET).freeze

REGISTER = %w[actors classes skills items weapons armors enemies troops states
              animations tilesets common_events system].freeze

puts '== F9-Datenbank mit 13 Registern =='
datenbank_setzen(VOLLE_DB)
check('Database.database liefert alle 13 Register') do
  db = Database.database
  db.is_a?(Hash) && REGISTER.all? { |k| db.key?(k) }
end
check('Startgold bleibt ein Wurzelfeld') { Database.database['gold'] == 120 }
check('System in database.json ersetzt nicht system.json') do
  # Database.system liest eine eigene Datei; der Datenbank-Abschnitt ist nur
  # eine Vorbereitung und darf den Projektstart nicht verändern.
  Database.database['system'].is_a?(Hash) && !Database.system.equal?(Database.database['system'])
end
check('gemeinsame Events werden gelesen, auch mit leerem Schalterfeld') do
  Database.common_events.map { |e| e['name'] } == ['ereignis_1'] &&
    Database.common_event('ereignis_1').is_a?(Hash)
end

puts '== Vorbereitete Register bleiben wirkungslos =='
Party.setup(VOLLE_DB)
check('Party entsteht nur aus Helden, nicht aus Klassen oder Ausrüstung') do
  Party.actors.size == 1 && Party.actors.first.id == 'held_1' &&
    Party.actors.first.max_hp == 100 && Party.actors.first.klass == 'Krieger'
end
check('Waffen und Rüstungen verändern die Heldenwerte nicht') do
  held = Party.actors.first
  held.atk == 10 && held.def == 8 && held.spi == 6 && held.agi == 8
end
check('Startparty im System-Abschnitt ändert die Mitglieder nicht') do
  Party.actors.map(&:id) == ['held_1']
end
check('Animationen und Tilesets werfen keine Fehler beim Zugriff') do
  Database.database['animations'].first['frames'] == 4 &&
    Database.database['tilesets'].first['tile'] == 1.0
end

puts '== F9-Vorgaben sind spielbar =='
check('neues Item ist bekannt, aber ohne Startmenge nicht im Inventar') do
  Party.item_def('item_1').is_a?(Hash) && Party.item_count('item_1').zero?
end
check('neues Heil-Item wirkt, sobald es im Inventar liegt') do
  Party.add_item('item_1', 1)
  held = Party.actors.first
  held.hp = 40
  Party.use_item('item_1', target: held) && held.hp == 60 && Party.item_count('item_1').zero?
end
check('neue Fertigkeit ist bekannt und kostet die eingestellten MP') do
  skill = Party.skill_def('skill_1')
  skill.is_a?(Hash) && Battle.skill_cost(skill) == 3 && Battle.skill_scope(skill) == 'enemy'
end
check('neuer Zustand ist bekannt und wird mit Dauer geführt') do
  defi = Party.state_def('zustand_1')
  defi.is_a?(Hash) && defi['kind'] == 'poison' && defi['turns'] == 3
end
check('leere Fertigkeitenliste eines neuen Helden ist gültig') do
  Party.skills(Party.actors.first).empty?
end

GameState.reset!({})
check('neues gemeinsames Event mit trigger none startet nicht') do
  interp = Object.new
  def interp.running? = false
  def interp.start(*_a) = raise('dürfte nicht starten')
  CommonEvents.tick(interp) == false
end
check('autorun mit leerem Schalterfeld läuft immer, mit Schalter nur bei an') do
  immer = { 'trigger' => 'autorun', 'switch' => '', 'commands' => [] }
  gesperrt = { 'trigger' => 'autorun', 'switch' => 'tor', 'commands' => [] }
  CommonEvents.aktiv?(immer) && !CommonEvents.aktiv?(gesperrt) &&
    (GameState.set_switch('tor', true); CommonEvents.aktiv?(gesperrt))
end

puts '== Kampf mit F9-Vorgaben =='
Party.setup(VOLLE_DB)
check('Truppe aus neuen Gegnern startet einen Kampf') do
  Battle.start('trupp_1') && Battle.active?
end
check('neue Fertigkeit trifft den neuen Gegner') do
  held = Party.actors.first
  gegner = Battle.enemies.first
  vorher = gegner.hp
  Battle.wende_skill_an(held, Party.skill_def('skill_1'), gegner) && gegner.hp < vorher
end
check('Kampf lässt sich abbrechen, ohne Daten zu beschädigen') do
  Battle.abort!
  !Battle.active? && Database.database['enemies'].first['hp'] == 30
end

puts '== MapCheck: F9-Vorgaben sind sauber =='
check('scan_database meldet für die Vorgaben keine Fehler') do
  funde = MapCheck.scan_database(VOLLE_DB, name: 'database.json')
  funde.none? { |f| f.severity == MapCheck::FEHLER }
end
check('unbekannte Register erzeugen keine MapCheck-Fehler') do
  mit_zusatz = VOLLE_DB.merge('zukunft' => [{ 'id' => 'x' }])
  MapCheck.scan_database(mit_zusatz, name: 'database.json')
          .none? { |f| f.severity == MapCheck::FEHLER }
end
check('kaputte Befehlsliste wird weiterhin gemeldet') do
  kaputt = VOLLE_DB.merge('common_events' => [F9_EVENT.merge('commands' => 'kein Array')])
  MapCheck.scan_database(kaputt, name: 'database.json')
          .any? { |f| f.severity == MapCheck::FEHLER }
end

puts '== Alte Projekte bleiben gültig =='
ALT = { 'gold' => 50,
        'actors' => [{ 'id' => 'arik', 'name' => 'Arik', 'class' => 'Krieger',
                       'hp' => 120, 'mp' => 15, 'atk' => 14, 'def' => 10 }],
        'items' => [{ 'id' => 'heiltrank', 'name' => 'Heiltrank', 'kind' => 'heal',
                      'power' => 40, 'price' => 25, 'start' => 2 }] }.freeze
datenbank_setzen(ALT)
Party.setup(ALT)
check('Datenbank ohne neue Register lädt und baut die Party') do
  Database.database.keys.sort == %w[actors gold items] &&
    Party.actors.first.max_hp == 120 && Party.item_count('heiltrank') == 2
end
check('fehlende Sammlungen ergeben leere Listen statt Fehler') do
  Database.common_events.empty? && Battle.troops_def.empty? &&
    Party.skill_def('gibtsnicht').nil? && Party.state_def('gibtsnicht').nil?
end
check('Heiltrank aus altem Datenbestand wirkt weiter') do
  held = Party.actors.first
  held.hp = 60
  Party.use_item('heiltrank', target: held) && held.hp == 100
end

datenbank_setzen(VOLLE_DB)
puts
if $fail.zero?
  puts 'DATABASE-REGISTER-TEST OK (0 Fehler)'
else
  puts "DATABASE-REGISTER-TEST: #{$fail} Fehler"
end
exit($fail.zero? ? 0 : 1)
