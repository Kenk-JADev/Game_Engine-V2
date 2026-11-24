#encoding: utf-8
# ============================================================================
#  Kopfloser Test für die erweiterten Bedingungen (Etappe 20d, XP 111/134–136).
#  Lässt sich ohne Engine starten:
#     ruby -I game/engine tools/bedingungen_test.rb
#  Exit-Code 0 = alles grün.
#
#  Geprüft wird:
#    - Interpreter: if_gold/if_item/if_script wählen Dann-/Sonst-Zweig,
#      unbekannter Operator -> Sonst-Zweig, Script-Fehler -> falsch
#    - Interpreter: save_access/menu_access/encounter_access setzen die
#      Sperren in $game_system (und ohne $game_system: Warnung statt Absturz)
#    - MapCheck: Zweig-Pflicht, Operatoren, Item-Name, allowed-Typ
# ============================================================================

# --- Minimal-Attrappe der Engine-Schnittstelle -----------------------------
module Rune
  ROOT = File.expand_path('..', __dir__)

  module Log
    def self.info(m) = nil
    def self.warn(m) = puts("WARN  #{m}")
    def self.error(m) = puts("ERROR #{m}")
  end
end

$LOAD_PATH.unshift(File.join(Rune::ROOT, 'game', 'engine'))
require 'data'
require 'state'
require 'issues'
require 'scripts'
require 'interpreter'
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

def lauf(interp, frames = 5, dt = 0.016)
  frames.times { interp.update(dt) }
  interp
end

# Party-Attrappe: Gold/Items ohne Datenbank (die Laufzeit fragt nur diese ab).
module Party
  @gold = 100
  @items = { 'heiltrank' => 2 }
  class << self
    attr_accessor :gold, :items
  end
  def self.item_count(id) = @items[id.to_s].to_i
end

def zweig(cmd)
  GameState.reset!({})
  i = Interpreter.new
  i.start([cmd, { 'cmd' => 'variable', 'name' => 'nachher', 'value' => 1 }])
  lauf(i)
  { dann: GameState.variable('dann_zweig'), sonst: GameState.variable('sonst_zweig'),
    nachher: GameState.variable('nachher') }
end

DANN = [{ 'cmd' => 'variable', 'name' => 'dann_zweig', 'value' => 1 }].freeze
SONST = [{ 'cmd' => 'variable', 'name' => 'sonst_zweig', 'value' => 1 }].freeze

puts '== Interpreter: Wenn Gold =='
check('Gold >= 100 ist wahr') do
  zweig({ 'cmd' => 'if_gold', 'op' => '>=', 'value' => 100, 'then' => DANN, 'else' => SONST })[:dann] == 1.0
end
check('Gold > 100 ist falsch -> Sonst-Zweig') do
  zweig({ 'cmd' => 'if_gold', 'op' => '>', 'value' => 100, 'then' => DANN, 'else' => SONST })[:sonst] == 1.0
end
check('Gold != 0 ist wahr') do
  zweig({ 'cmd' => 'if_gold', 'op' => '!=', 'value' => 0, 'then' => DANN, 'else' => SONST })[:dann] == 1.0
end

puts '== Interpreter: Wenn Item =='
check('Item-Menge 2 >= 2 ist wahr') do
  zweig({ 'cmd' => 'if_item', 'name' => 'heiltrank', 'op' => '>=', 'value' => 2,
          'then' => DANN, 'else' => SONST })[:dann] == 1.0
end
check('Item-Menge 0 >= 1 ist falsch') do
  zweig({ 'cmd' => 'if_item', 'name' => 'unbekannt', 'op' => '>=', 'value' => 1,
          'then' => DANN, 'else' => SONST })[:sonst] == 1.0
end
check('Befehle nach der Verzweigung laufen weiter') do
  zweig({ 'cmd' => 'if_item', 'name' => 'heiltrank', 'op' => '==', 'value' => 2,
          'then' => DANN, 'else' => SONST })[:nachher] == 1.0
end

puts '== Interpreter: Wenn Script =='
check('wahrer Ausdruck -> Dann-Zweig') do
  zweig({ 'cmd' => 'if_script', 'code' => '1 + 1 == 2', 'then' => DANN,
          'else' => SONST })[:dann] == 1.0
end
check('false -> Sonst-Zweig') do
  zweig({ 'cmd' => 'if_script', 'code' => 'false', 'then' => DANN,
          'else' => SONST })[:sonst] == 1.0
end
check('nil gilt als falsch') do
  zweig({ 'cmd' => 'if_script', 'code' => 'nil', 'then' => DANN,
          'else' => SONST })[:sonst] == 1.0
end
check('kaputter Ausdruck gilt als falsch (kein Absturz)') do
  zweig({ 'cmd' => 'if_script', 'code' => 'gibts_nicht_als_methode(1)', 'then' => DANN,
          'else' => SONST })[:sonst] == 1.0
end
check('leerer Ausdruck -> Sonst-Zweig') do
  zweig({ 'cmd' => 'if_script', 'code' => '  ', 'then' => DANN, 'else' => SONST })[:sonst] == 1.0
end

puts '== Interpreter: Wenn Actor / Wenn Taste (XP 111) =='
# Party-Attrappe um Actors erweitern (find liefert nur bekannte IDs).
module Party
  class << self
    def find(id) = [{ id: 'arik' }].find { |a| a[:id] == id.to_s }
  end
end
check('Actor im Team -> Dann-Zweig') do
  zweig({ 'cmd' => 'if_actor', 'name' => 'arik', 'then' => DANN, 'else' => SONST })[:dann] == 1.0
end
check('Actor nicht im Team -> Sonst-Zweig') do
  zweig({ 'cmd' => 'if_actor', 'name' => 'mira', 'then' => DANN, 'else' => SONST })[:sonst] == 1.0
end
check('leerer Actor-Name -> Sonst-Zweig') do
  zweig({ 'cmd' => 'if_actor', 'name' => '  ', 'then' => DANN, 'else' => SONST })[:sonst] == 1.0
end

# Eingabe-Attrappe: nur die Taste "e" gilt als gedrueckt.
module Rune
  module Input
    def self.gedrueckt = %i[e]
    def self.pressed?(taste) = gedrueckt.include?(taste)
  end
end
check('gedrueckte Taste -> Dann-Zweig') do
  zweig({ 'cmd' => 'if_button', 'name' => 'e', 'then' => DANN, 'else' => SONST })[:dann] == 1.0
end
check('nicht gedrueckte Taste -> Sonst-Zweig') do
  zweig({ 'cmd' => 'if_button', 'name' => 'q', 'then' => DANN, 'else' => SONST })[:sonst] == 1.0
end

puts '== Interpreter: Sperr-Befehle (XP 134/135/136) =='
module RGSS
  class GameSystem
    attr_accessor :menu_disabled, :save_disabled, :encounter_disabled
    def initialize
      @menu_disabled = false
      @save_disabled = false
      @encounter_disabled = false
    end
  end
end
$game_system = RGSS::GameSystem.new

def sperren(cmd)
  i = Interpreter.new
  i.start([cmd])
  lauf(i)
end

sperren({ 'cmd' => 'save_access', 'allowed' => false })
check('save_access aus sperrt das Speichern') { $game_system.save_disabled == true }
sperren({ 'cmd' => 'save_access', 'allowed' => true })
check('save_access an erlaubt es wieder') { $game_system.save_disabled == false }
sperren({ 'cmd' => 'menu_access', 'allowed' => false })
check('menu_access aus sperrt das Menü') { $game_system.menu_disabled == true }
sperren({ 'cmd' => 'encounter_access', 'allowed' => false })
check('encounter_access aus sperrt Zufallskämpfe') { $game_system.encounter_disabled == true }
sperren({ 'cmd' => 'encounter_access' })
check('ohne allowed wird erlaubt (XP-Default)') { $game_system.encounter_disabled == false }
$game_system = nil
check('ohne $game_system: Warnung statt Absturz') do
  i = Interpreter.new
  i.start([{ 'cmd' => 'save_access', 'allowed' => false },
           { 'cmd' => 'variable', 'name' => 'danach2', 'value' => 1 }])
  lauf(i)
  !i.running? && GameState.variable('danach2') == 1.0
end

puts '== MapCheck: erweiterte Bedingungen =='
OPTS = { prefabs: %w[haus], materials: %w[weg], scripts: [] }.freeze

def karte_mit(cmds)
  { 'name' => 'Testkarte',
    'terrain' => { 'width' => 120.0, 'height' => 90.0, 'resolution' => 64, 'amplitude' => 0.0 },
    'objects' => [],
    'events' => [{ 'id' => 1, 'name' => 'Bedingung', 'x' => 0.0, 'z' => 0.0, 'radius' => 2.5,
                   'pages' => [{ 'conditions' => {}, 'trigger' => 'action', 'commands' => cmds }] }] }
end

def hat?(funde, fragment, severity = nil)
  funde.any? do |f|
    f.to_s.include?(fragment) && (severity.nil? || f.severity == severity)
  end
end

check('gültige if_gold/if_item/if_script: keine Funde') do
  MapCheck.scan(karte_mit([
                            { 'cmd' => 'if_gold', 'op' => '>=', 'value' => 10, 'then' => [], 'else' => [] },
                            { 'cmd' => 'if_item', 'name' => 'heiltrank', 'op' => '>=', 'value' => 1,
                              'then' => [], 'else' => [] },
                            { 'cmd' => 'if_script', 'code' => 'true', 'then' => [], 'else' => [] }
                          ]), **OPTS).empty?
end
check('if_gold ohne Zweige -> Warnung') do
  hat?(MapCheck.scan(karte_mit([{ 'cmd' => 'if_gold', 'op' => '>=', 'value' => 1 }]), **OPTS),
       "'then' fehlt/keine Liste", MapCheck::WARNUNG)
end
check('if_gold mit unbekanntem Operator -> Warnung') do
  hat?(MapCheck.scan(karte_mit([{ 'cmd' => 'if_gold', 'op' => '<>', 'value' => 1,
                                  'then' => [], 'else' => [] }]), **OPTS),
       "unbekannter Vergleich '<>'", MapCheck::WARNUNG)
end
check('if_item ohne Namen -> Fehler') do
  hat?(MapCheck.scan(karte_mit([{ 'cmd' => 'if_item', 'op' => '>=', 'value' => 1,
                                  'then' => [], 'else' => [] }]), **OPTS),
       "'name' fehlt oder ist leer", MapCheck::FEHLER)
end
check('if_script ohne Code -> Fehler') do
  hat?(MapCheck.scan(karte_mit([{ 'cmd' => 'if_script', 'then' => [], 'else' => [] }]), **OPTS),
       "'code' fehlt oder ist leer", MapCheck::FEHLER)
end
check('Fehler im Dann-Zweig wird rekursiv gefunden') do
  hat?(MapCheck.scan(karte_mit([{ 'cmd' => 'if_gold', 'op' => '>=', 'value' => 1, 'else' => [],
                                  'then' => [{ 'cmd' => 'if_item', 'op' => '>=', 'value' => 1,
                                               'then' => [], 'else' => [] }] }]), **OPTS),
       "'name' fehlt oder ist leer", MapCheck::FEHLER)
end
check('sperren mit allowed=true/false: keine Funde') do
  MapCheck.scan(karte_mit([{ 'cmd' => 'save_access', 'allowed' => true },
                           { 'cmd' => 'menu_access', 'allowed' => false },
                           { 'cmd' => 'encounter_access', 'allowed' => true }]), **OPTS).empty?
end
check('allowed als Text -> Warnung') do
  hat?(MapCheck.scan(karte_mit([{ 'cmd' => 'save_access', 'allowed' => 'nein' }]), **OPTS),
       "'allowed' muss true oder false sein", MapCheck::WARNUNG)
end
check('if_actor/if_button: keine Funde bei gueltigen Daten') do
  MapCheck.scan(karte_mit([
                            { 'cmd' => 'if_actor', 'name' => 'arik', 'then' => [], 'else' => [] },
                            { 'cmd' => 'if_button', 'name' => 'e', 'then' => [], 'else' => [] }
                          ]), **OPTS).empty?
end
check('if_actor mit unbekanntem Actor -> Warnung') do
  hat?(MapCheck.scan(karte_mit([{ 'cmd' => 'if_actor', 'name' => 'niemand',
                                  'then' => [], 'else' => [] }]), **OPTS),
       "Actor 'niemand' nicht in database.json", MapCheck::WARNUNG)
end
check('if_button ohne Taste -> Fehler') do
  hat?(MapCheck.scan(karte_mit([{ 'cmd' => 'if_button', 'then' => [], 'else' => [] }]), **OPTS),
       "'name' fehlt oder ist leer", MapCheck::FEHLER)
end
check('alle 48 Befehlstypen im Register der Prüfung') { MapCheck::CMD_TYPES.size == 48 }

puts($fail.zero? ? 'ALLE CHECKS GRUEN' : "#{$fail} CHECK(S) FEHLGESCHLAGEN")
exit($fail.zero? ? 0 : 1)
