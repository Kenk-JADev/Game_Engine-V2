#encoding: utf-8
# ============================================================================
#  Kopfloser Test für die $game_temp-Aufträge (Etappe 20d, LOGIK E.3).
#  Lässt sich ohne Engine starten:
#     ruby -I game/engine tools/temp_flags_test.rb
#  Exit-Code 0 = alles grün.
#
#  Geprüft wird:
#    - TempActions.tick: battle_calling/menu_calling/save_calling/
#      player_transferring/to_title, jeweils genau ein Auftrag pro Aufruf
#      und Flag danach gelöscht (kein Dauerfeuer)
#    - Kartenauflösung: Name direkt, 1-basierte Position, Unbekanntes
#    - fehlender Handler / unbekannte Karte: Warnung statt Absturz
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
require 'graphics'
require 'rgss'          # RGSS::GameTemp
require 'temp_actions'

$fail = 0
def check(name)
  ok = yield
  puts(ok ? "  ok   #{name}" : "  FAIL #{name}")
  $fail += 1 unless ok
rescue StandardError => e
  puts "  FAIL #{name} (Ausnahme: #{e.class}: #{e.message})"
  $fail += 1
end

# Kartenliste fest vorgeben, damit der Test nicht von game/data abhängt.
def Database.map_ids = %w[dorf haus welt]

def frisch
  $game_temp = RGSS::GameTemp.new
end

def sammler
  @log = []
  {
    battle: ->(troop, esc, lose) { @log << [:battle, troop, esc, lose] },
    menu: ->(speichern) { @log << [:menu, speichern] },
    transfer: ->(karte, x, z, yaw) { @log << [:transfer, karte, x, z, yaw] },
    title: -> { @log << [:title] }
  }
end

def log = @log

puts '== TempActions: Kampf =='
frisch
$game_temp.battle_calling = true
$game_temp.battle_troop_id = 'wölfe'
$game_temp.battle_can_escape = true
check('battle_calling startet den Kampf und löscht das Flag') do
  sammler
  ok = TempActions.tick(@handler = sammler)
  ok && !$game_temp.battle_calling && log == [[:battle, 'wölfe', true, false]]
end
check('kein zweiter Kampf ohne neues Flag') do
  TempActions.tick(@handler) == false && @log.size == 1
end

puts '== TempActions: Menü und Speichern =='
frisch
$game_temp.menu_calling = true
check('menu_calling öffnet das Menü') do
  sammler
  TempActions.tick(sammler) && log == [[:menu, false]] && !$game_temp.menu_calling
end
frisch
$game_temp.save_calling = true
check('save_calling wünscht den Speichern-Eintrag') do
  sammler
  TempActions.tick(sammler) && log == [[:menu, true]] && !$game_temp.save_calling
end
frisch
$game_temp.menu_calling = true
$game_temp.save_calling = true
check('beide Flags: ein Aufruf, Speichern gewinnt') do
  sammler
  TempActions.tick(sammler) && log == [[:menu, true]] &&
    !$game_temp.menu_calling && !$game_temp.save_calling
end

puts '== TempActions: Kartenwechsel =='
frisch
$game_temp.player_transferring = true
$game_temp.player_new_map_id = 'haus'
check('Name der Karte wechselt direkt') do
  sammler
  TempActions.tick(sammler) && log == [[:transfer, 'haus', nil, nil, nil]] &&
    !$game_temp.player_transferring
end
frisch
$game_temp.player_transferring = true
$game_temp.player_new_map_id = 3
check('XP-Nummer 3 wählt die dritte Karte (welt)') do
  sammler
  TempActions.tick(sammler) && log == [[:transfer, 'welt', nil, nil, nil]]
end
check('Unbekannte Nummer: Warnung, kein Wechsel') do
  frisch
  $game_temp.player_transferring = true
  $game_temp.player_new_map_id = 9
  sammler
  TempActions.tick(sammler) && log.empty? && !$game_temp.player_transferring
end
check('Startposition und Richtung kommen mit') do
  frisch
  $game_temp.player_transferring = true
  $game_temp.player_new_map_id = 'dorf'
  $game_temp.player_new_x = 12
  $game_temp.player_new_y = 34
  $game_temp.player_new_direction = 6
  sammler
  TempActions.tick(sammler) && log == [[:transfer, 'dorf', 12.0, 34.0, Math::PI / 2]]
end
check('XP-Richtungen 2/4/6/8 auf Yaw abgebildet') do
  TempActions.yaw_fuer(8) == Math::PI && TempActions.yaw_fuer(2).zero? &&
    TempActions.yaw_fuer(6) == Math::PI / 2 && TempActions.yaw_fuer(4) == -Math::PI / 2 &&
    TempActions.yaw_fuer(0).nil? && TempActions.yaw_fuer(nil).nil?
end
check('Position 0/0 bedeutet: bisherige Stelle behalten') do
  frisch
  $game_temp.player_new_x = 0
  $game_temp.player_new_y = 0
  TempActions.startposition($game_temp) == [nil, nil]
end
check('karte_fuer: Name vor Nummer, 0 ungueltig') do
  TempActions.karte_fuer('dorf') == 'dorf' && TempActions.karte_fuer(1) == 'dorf' &&
    TempActions.karte_fuer(0).nil? && TempActions.karte_fuer('gibtsnicht').nil?
end

puts '== TempActions: Titel und Randfälle =='
frisch
$game_temp.to_title = true
check('to_title ruft den Titel-Handler') do
  sammler
  TempActions.tick(sammler) && log == [[:title]] && !$game_temp.to_title
end
frisch
check('ohne offenes Flag: false') { TempActions.tick(sammler) == false }
$game_temp = nil
check('ohne $game_temp: false statt Absturz') { TempActions.tick({}) == false }
check('fehlender Handler wird nur gewarnt') do
  frisch
  $game_temp.menu_calling = true
  TempActions.tick({}) && !$game_temp.menu_calling
end
check('Kampf hat Vorrang vor Menü') do
  frisch
  $game_temp.battle_calling = true
  $game_temp.menu_calling = true
  sammler
  TempActions.tick(sammler) && log == [[:battle, 0, false, false]] && $game_temp.menu_calling
end

puts($fail.zero? ? 'ALLE CHECKS GRUEN' : "#{$fail} CHECK(S) FEHLGESCHLAGEN")
exit($fail.zero? ? 0 : 1)
