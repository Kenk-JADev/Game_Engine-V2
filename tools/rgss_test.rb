#encoding: utf-8
# ============================================================================
#  Kopfloser Test für die RGSS-Kompatibilitätsschicht (Etappe 19e).
#  Lässt sich ohne Engine starten:
#     ruby -I game/engine tools/rgss_test.rb
#  Exit-Code 0 = alles grün.
#
#  Geprüft wird game/engine/rgss.rb:
#    - $game_*-Globale nach RGSS.setup
#    - Schalter/Variablen/Self-Switches (XP-Schlüssel [karte, id, letter])
#    - $game_party, $game_player, $game_map, $game_screen
#    - Input (RGSS-Konstanten, trigger?/press?/dir4)
#    - Graphics (fadeout/fadein in Frames, brightness, freeze/transition)
#    - Audio (bgm/se mit 0..100-Lautstärke, Pfad-/Endungs-Kürzung)
#    - load_data/save_data (JSON-Rundlauf)
#    - Etappe 19h: $game_temp, $data_*-Globale, passable? (Grenzen/Stufe)
# ============================================================================

require 'tmpdir'
require 'fileutils'

# --- Minimal-Attrappe der Engine-Schnittstelle -----------------------------
module Rune
  ROOT = File.expand_path('..', __dir__)

  module Log
    def self.info(m) = nil
    def self.warn(m) = puts("WARN  #{m}")
    def self.error(m) = puts("ERROR #{m}")
  end

  module Player
    @pos = [0.0, 0.0, 0.0]
    class << self
      attr_accessor :pos
    end
    def self.teleport(x, y, z) = (self.pos = [x, y, z])
    def self.position = pos
  end

  module Camera
    @yaw = 0.0
    class << self
      attr_accessor :yaw
    end
  end

  module Camera
    @yaw = 0.0
    class << self
      attr_accessor :yaw
    end
  end

  module Sound
    CALLS = []
    def self.play(name, volume = 1.0) = (CALLS << [:play, name, volume])
    def self.loop(name, volume = 1.0) = (CALLS << [:loop, name, volume])
    def self.stop(name) = (CALLS << [:stop, name])
  end

  module Input
    @pressed = []
    @down = []
    class << self
      attr_accessor :pressed, :down
    end
    def self.pressed?(key) = @pressed.include?(key)
    def self.down?(key) = @down.include?(key)
  end

  # Physik-Attrappe (Etappe 19k.3): solids = Liste von AABBs
  # [x0, y0, z0, x1, y1, z1]; solid_at? ist der Punkt-Test, den Passage
  # über der Steiggrenze benutzt.
  module Physics
    @solids = []
    class << self
      attr_accessor :solids
    end

    def self.solid_at?(x, y, z)
      @solids.any? do |b|
        x >= b[0] && x <= b[3] && y >= b[1] && y <= b[4] && z >= b[2] && z <= b[5]
      end
    end
  end
end

# Game-Attrappe: $game_map/$game_player brauchen Karte, Events und gy.
module Game
  @map = { 'name' => 'Testkarte', 'terrain' => { 'width' => 120.0, 'height' => 90.0 } }
  @map_id = 'welt01'
  @events = []
  @npcs = []
  class << self
    attr_accessor :map, :map_id, :events, :npcs
  end

  def self.gy(_x, _z) = 0.0
end

$LOAD_PATH.unshift(File.join(Rune::ROOT, 'game', 'engine'))
require 'state'
require 'issues'
require 'party'
require 'weather'
require 'fade'
require 'data'      # Database: RGSS.setup liest seit 19h $data_* aus JSON
require 'rgss'

$fail = 0
def check(name)
  ok = yield
  puts(ok ? "  ok   #{name}" : "  FAIL #{name}")
  $fail += 1 unless ok
rescue StandardError => e
  puts "  FAIL #{name} (Ausnahme: #{e.class}: #{e.message})"
  $fail += 1
end

FakeEvent = Struct.new(:id, :name, :x, :z)

puts '== RGSS.setup: $game_*-Globale =='
RGSS.setup
check('alle Globalen besetzt') do
  [$game_system, $game_switches, $game_variables, $game_self_switches,
   $game_party, $game_player, $game_map, $game_screen].none?(&:nil?)
end
check('System-Schalter starten offen') do
  !$game_system.menu_disabled && !$game_system.save_disabled
end

puts '== $game_switches / $game_variables =='
$game_switches['auftrag'] = true
check('Schalter per Name gesetzt') { $game_switches['auftrag'] == true }
$game_switches[7] = true
check('Schalter per Integer-Index (to_s)') { $game_switches[7] && GameState.switch('7') }
$game_variables['kristalle'] = 3
check('Variable gesetzt') { $game_variables['kristalle'] == 3.0 }

puts '== $game_self_switches =='
Game.events = [FakeEvent.new(7, 'Wache', 0.0, 0.0)]
$game_self_switches[['welt01', 7, 'A']] = true
check('XP-Schlüssel [karte, id, letter] setzt per Event-ID') do
  GameState.self_switch('welt01|Wache', 'A')
end
check('Lesen mit demselben Schlüssel') { $game_self_switches[['welt01', 7, 'A']] == true }
$game_self_switches[['welt01', 'Wache', 'B']] = true
check('Event-Name statt ID geht auch') { $game_self_switches[['welt01', 'Wache', 'B']] }
$game_self_switches['welt01|Wache|C'] = true
check('interner String-Schlüssel') { GameState.self_switch('welt01|Wache', 'C') }
check('unbekannte Event-ID -> false, kein Absturz') do
  $game_self_switches[['welt01', 999, 'A']] = true
  $game_self_switches[['welt01', 999, 'A']] == false
end

puts '== $game_party / $game_player / $game_map =='
$game_party.gain_gold(50)
$game_party.lose_gold(20)
check('Gold gain/lose') { $game_party.gold == 30 }
$game_party.gain_item('heiltrank', 2)
check('Item gegeben') { $game_party.item_count('heiltrank') == 2 }
$game_party.lose_item('heiltrank', 1)
check('Item genommen') { $game_party.item_count('heiltrank') == 1 }

$game_player.moveto(2.0, 3.0)
check('moveto setzt Spieler + Bodenhöhe') { Rune::Player.position == [2.0, 0.2, 3.0] }
check('x/z-Leser') { $game_player.x == 2.0 && $game_player.z == 3.0 }

check('$game_map: Karte/Name/Masse') do
  $game_map.map_id == 'welt01' && $game_map.display_name == 'Testkarte' &&
    $game_map.width == 120.0 && $game_map.height == 90.0
end
check('$game_map.event per ID und Name') do
  $game_map.event(7)&.name == 'Wache' && $game_map.event('Wache')&.id == 7
end

puts '== $game_screen (Wetter) =='
$game_screen.weather(1, 2.0)
check('RGSS-Zahl 1 = Regen') { Weather.type == 'regen' && Weather.intensity == 2.0 }
$game_screen.weather('schnee', 1.5)
check('Name deutsch = Schnee') { Weather.type == 'schnee' }
$game_screen.weather('none')
check('none/0 = kein Wetter') { Weather.type == 'kein' }
check('weather_type-Leser (19h.2: XP-Zahl)') { $game_screen.weather_type.zero? }

puts '== Input (RGSS-Konstanten) =='
check('Konstanten wie RGSS1') { Input::DOWN == 2 && Input::C == 13 && Input::F9 == 29 }
Rune::Input.pressed = [:enter]
Rune::Input.down = []
check('Enter löst C (Bestätigen) aus') { Input.trigger?(Input::C) }
check('Enter ist nicht B (Abbrechen)') { !Input.trigger?(Input::B) }
Rune::Input.pressed = [:escape]
check('Esc löst B aus') { Input.trigger?(Input::B) }
Rune::Input.pressed = [:shift]
check('Shift löst A aus') { Input.trigger?(Input::A) }
check('Symbol-Argument geht direkt') do
  Rune::Input.pressed = [:e]
  Input.trigger?(:e) && Input.trigger?(Input::C)
end
Rune::Input.down = [:left]
check('dir4: links = 4') { Input.dir4 == 4 }
Rune::Input.down = [:up]
check('dir4: oben = 8') { Input.dir4 == 8 }
Rune::Input.down = []
check('dir4: keine Richtung = 0') { Input.dir4.zero? }
check('press? nutzt down?') do
  Rune::Input.down = [:down]
  Input.press?(Input::DOWN)
end
check('repeat? wie trigger?') do
  Rune::Input.pressed = [:enter]
  Input.repeat?(Input::C)
end

puts '== Graphics (Frames -> Sekunden) =='
Fade.reset!
check('frame_rate-Default 40 (RGSS1)') { Graphics.frame_rate == 40 }
Graphics.fadeout(20)
check('fadeout(20) = 0.5 s: Blende läuft') { Fade.laeuft? }
Fade.update(0.5)
check('nach 0.5 s schwarz') { Fade.alpha == 1.0 }
Graphics.fadein(40)
Fade.update(1.0)
check('fadein(40) = 1.0 s: wieder hell') { Fade.alpha.zero? }
Graphics.brightness = 128
check('brightness= 128 -> halbe Abdunklung') { (Fade.alpha - (1.0 - 128 / 255.0)).abs < 0.001 }
check('brightness-Leser rundet zurück') { Graphics.brightness == 128 }
Graphics.brightness = 255
check('brightness 255 = hell') { Fade.alpha.zero? }
Graphics.freeze
check('freeze/frozen?') { Graphics.frozen? }
Graphics.transition
check('transition löst freeze') { !Graphics.frozen? }
c0 = Graphics.frame_count
Graphics.update
check('update zählt Frames') { Graphics.frame_count == c0 + 1 }

puts '== Etappe 19h: $game_temp, $data_*, passable? =='
check('$game_temp besetzt und frisch') do
  $game_temp.is_a?(RGSS::GameTemp) && $game_temp.message_text.nil? &&
    $game_temp.player_passable?
end
check('$game_temp: XP-Defaultwerte (000 - Game_Temp.rb)') do
  $game_temp.choice_start == 99 && $game_temp.num_input_start == 99 &&
    $game_temp.battle_event_flags == {} && $game_temp.battleback_name == '' &&
    $game_temp.gameover == false && $game_temp.to_title == false
end
check('$game_system: Timer (XP-Frames)') do
  $game_system.timer_start(2)
  $game_system.update(1.0)
  $game_system.timer_working && ($game_system.timer_seconds - 1.0).abs < 0.01
end
check('$game_system: se_play delegiert zu Audio') do
  Rune::Sound::CALLS.clear
  $game_system.se_play({ 'name' => 'klick', 'volume' => 50 })
  Rune::Sound::CALLS.last == [:play, 'klick', 0.5]
end
check('$game_map.valid? wie XP') do
  $game_map.valid?(10, 10) && !$game_map.valid?(500, 10) &&
    !$game_map.valid?(-1, 10)
end
check('$game_player: direction/steps wie XP') do
  $game_player.direction = 6
  d = $game_player.direction
  s0 = $game_player.steps
  $game_player.increase_steps
  d == 6 && $game_player.steps == s0 + 1
end
check('$data_actors: Hash mit String-IDs aus database.json') do
  $data_actors.key?('arik') && $data_actors.size == 2
end
check('$data_items/$data_system besetzt') do
  $data_items.key?('heiltrank') && $data_system.is_a?(Hash)
end
check('passable?: flach in der Karte -> true') { $game_player.passable?(5.0, 5.0) }
check('passable?: Richtung 6 über die Kartengrenze -> false') do
  !$game_player.passable?($game_map.width, 5.0, 6)
end
# Etappe 19k.2: Block-Zonen ("blocked" im Karten-JSON) - unsichtbare Wände.
Game.map['blocked'] = [{ 'x' => 60.0, 'z' => 45.0, 'w' => 10.0, 'h' => 6.0 }]
check('blocked_zones: Liste aus dem Karten-JSON') { $game_map.blocked_zones.size == 1 }
check('blocked?: Punkt in der Zone (Rand gehört dazu)') do
  $game_map.blocked?(60.0, 45.0) && $game_map.blocked?(65.0, 48.0)
end
check('blocked?: Punkt ausserhalb') do
  !$game_map.blocked?(54.9, 45.0) && !$game_map.blocked?(60.0, 48.1)
end
check('passable?: Ziel in der Block-Zone -> false') { !$game_player.passable?(60.0, 45.0) }
check('passable?: Richtung 6 in die Zone -> false') { !$game_player.passable?(59.0, 45.0, 6) }
check('passable?: neben der Zone -> true') { $game_player.passable?(10.0, 10.0) }
Game.map.delete('blocked')
check('ohne blocked-Schlüssel: alles passierbar') { !$game_map.blocked?(60.0, 45.0) }

# Etappe 19k.3: Objekt-Hindernisse (Physik-Abfrage über der Steiggrenze).
Rune::Physics.solids = [[29.0, 0.0, 29.0, 31.0, 1.5, 31.0]]
check('passable?: hohes Objekt im Weg -> false') { !$game_player.passable?(30.0, 30.0) }
check('$game_map.hindernis? sieht das Objekt') { $game_map.hindernis?(30.0, 30.0) }
check('hindernis? daneben -> false') { !$game_map.hindernis?(32.0, 30.0) }
Rune::Physics.solids = [[29.0, 0.0, 29.0, 31.0, 0.3, 31.0]]
check('passable?: flacher Kasten ist eine Stufe -> true') { $game_player.passable?(30.0, 30.0) }
Rune::Physics.solids = [[29.0, 0.8, 29.0, 31.0, 2.0, 31.0]]
check('passable?: schwebender Kasten (Körperhöhe) -> false') do
  !$game_player.passable?(30.0, 30.0)
end
Rune::Physics.solids = []
check('ohne Hindernisse wieder passierbar') { $game_player.passable?(30.0, 30.0) }
$game_temp.passable = false
check('passable? abgeschaltet -> immer true') { $game_player.passable?(999.0, 999.0) }
$game_temp.passable = true

# Etappe 19l: Zufallskämpfe (XP Game_Map/Game_Player, 016/022).
check('ohne encounters: leere Truppenliste, Schrittzahl 30') do
  $game_map.encounter_list.empty? && $game_map.encounter_step == 30
end
Game.map['encounters'] = { 'troops' => ['waldweg', 'banditen'], 'steps' => 10 }
check('encounter_list aus dem Karten-JSON') do
  $game_map.encounter_list == %w[waldweg banditen]
end
check('encounter_step aus dem Karten-JSON') { $game_map.encounter_step == 10 }
check('make_encounter_count: XP-Formel rand(n)+rand(n)+1 (1..2n-1)') do
  ok = true
  200.times do
    $game_player.make_encounter_count
    c = $game_player.encounter_count
    ok &&= (c >= 1 && c <= 19)
  end
  ok
end
check('decrease_encounter_count: zählt bis 0, dann bleibt er (kein Reset)') do
  $game_player.instance_variable_set(:@encounter_count, 3)
  $game_player.decrease_encounter_count
  a = $game_player.encounter_count
  $game_player.decrease_encounter_count
  b = $game_player.encounter_count
  $game_player.decrease_encounter_count
  c = $game_player.encounter_count
  $game_player.decrease_encounter_count
  a == 2 && b == 1 && c == 0 && $game_player.encounter_count == 0
end
Game.map['encounters'] = { 'troops' => ['waldweg'], 'steps' => 0 }
check('steps <= 0 -> Default 30') { $game_map.encounter_step == 30 }
Game.map['encounters'] = { 'troops' => 'waldweg', 'steps' => 'viel' }
check('kaputte encounters: Liste leer, Schrittzahl 30') do
  $game_map.encounter_list.empty? && $game_map.encounter_step == 30
end
Game.map['encounters'] = { 'troops' => ['waldweg', 42, nil, '', '  '] }
check('nur nicht-leere Text-IDs werden als Truppen übernommen') do
  $game_map.encounter_list == ['waldweg']
end
[0.5, 2.5, Float::INFINITY, Float::NAN].each do |wert|
  Game.map['encounters']['steps'] = wert
  check("ungültige Schrittzahl #{wert.inspect} -> Default 30") { $game_map.encounter_step == 30 }
end
Game.map['encounters']['steps'] = 2.0
check('ganzzahlige JSON-Kommazahl wird als Schrittzahl übernommen') do
  $game_map.encounter_step == 2
end
Game.map['encounters'] = ['waldweg']
check('encounters als Liste (kein Objekt) -> leer/Default') do
  $game_map.encounter_list.empty? && $game_map.encounter_step == 30
end
Game.map.delete('encounters')
check('encounter_count-Default 0') do
  $game_player.instance_variable_set(:@encounter_count, nil)
  $game_player.encounter_count == 0
end
RGSS.reset!
check('RGSS.reset!: $game_temp frisch, $scene nil') do
  $game_temp.is_a?(RGSS::GameTemp) && $scene.nil?
end

puts '== Audio =='
Rune::Sound::CALLS.clear
Audio.bgm_play('Audio/BGM/musik', 80)
check('bgm_play kürzt Pfad/Endung, 80 -> 0.8') do
  Rune::Sound::CALLS.last == [:loop, 'musik', 0.8]
end
Audio.bgm_stop
check('bgm_stop stoppt den laufenden Titel') { Rune::Sound::CALLS.last == [:stop, 'musik'] }
Audio.se_play('klick', 50)
check('se_play') { Rune::Sound::CALLS.last == [:play, 'klick', 0.5] }
Audio.me_play('fanfare')
check('me_play ohne Lautstärke = 1.0') { Rune::Sound::CALLS.last == [:play, 'fanfare', 1.0] }
Audio.bgs_play('wind', 40)
Audio.bgs_stop
check('bgs Play/Stop') { Rune::Sound::CALLS.last == [:stop, 'wind'] }

puts '== load_data / save_data (JSON) =='
Dir.mktmpdir('rune_rgss') do |tmp|
  pfad = File.join(tmp, 'daten.json')
  daten = { 'name' => 'Held', 'hp' => 30, 'items' => %w[heiltrank fackel],
            'notiz' => 'Grüße aus dem Script' }
  save_data(daten, pfad)
  check('Datei geschrieben') { File.exist?(pfad) }
  zurueck = load_data(pfad)
  check('Rundlauf: Werte gleich') { zurueck == daten }
  check('Rundlauf: Umlaute heil') { zurueck['notiz'] == 'Grüße aus dem Script' }
  check('load_data auf fehlende Datei -> nil') { load_data(File.join(tmp, 'nix.json')).nil? }
end

puts($fail.zero? ? 'RGSS-TEST OK (0 Fehler)' : "RGSS-TEST FEHLGESCHLAGEN (#{$fail} Fehler)")
exit($fail.zero? ? 0 : 1)
