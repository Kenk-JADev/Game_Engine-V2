#encoding: utf-8
# ============================================================================
#  Kopfloser Test für Datenbank + GameState + Event + Interpreter.
#  Lässt sich ohne Engine starten:
#     ruby -I game/engine tools/event_test.rb
#  Exit-Code 0 = alles grün.
# ============================================================================

# --- Minimal-Attrappe der Engine-Schnittstelle -----------------------------
module Rune
  ROOT = File.expand_path('..', __dir__)

  @press = false
  @pos = [0.0, 0.0, 0.0]
  @yaw = 0.0

  def self.press=(v); @press = v; end
  def self.pos=(v); @pos = v; end
  def self.yaw; @yaw; end

  module Log
    def self.info(m) = nil
    def self.warn(m) = puts("WARN  #{m}")
    def self.error(m) = puts("ERROR #{m}")
  end

  module Input
    def self.pressed?(_key) = Rune.instance_variable_get(:@press)
  end

  module Player
    def self.teleport(x, y, z) = (Rune.pos = [x, y, z])
    def self.position = Rune.instance_variable_get(:@pos)
  end

  module Camera
    def self.yaw=(v); Rune.instance_variable_set(:@yaw, v); end
  end

  module Sound
    PLAYED = []
    def self.play(name, _v = 1.0)
      PLAYED << name
    end
  end
end

$LOAD_PATH.unshift(File.join(Rune::ROOT, 'game', 'engine'))
require 'data'
require 'state'
require 'issues'
require 'event'
require 'interpreter'
require 'effects'   # dorf.json enthält seit Etappe 14c 'effect'-Befehle
require 'scripts'   # Runde 13/Y2: Demo-Events rufen Projekt-Scripts auf
                    # (scripts.rb zieht battle als Context-Helfer mit)

module Rune
  module Gfx
    def self.box(*_a) = nil
  end
end

$fail = 0
def check(name)
  ok = yield
  puts(ok ? "  ok   #{name}" : "  FAIL #{name}")
  $fail += 1 unless ok
end

def press!
  Rune.press = true
  $interp.update(0.016)
  Rune.press = false
end

# Interpreter laufen lassen, bis er blockiert (Textfenster) oder fertig ist.
def drain(limit = 200)
  limit.times do
    break if $interp.message
    break unless $interp.running?
    $interp.update(0.016)
  end
end

# --- Daten laden -----------------------------------------------------------
check('system.json gelesen') { Database.system['name'].to_s.include?('Rune3D') }
map = Database.map('dorf')
check('dorf.json gelesen') { map['events'].is_a?(Array) }
check('Projekt-Scripts geladen (dorf_begruessung, kristalle_zaehlen)') do
  Scripts.load_project &&
    %w[dorf_begruessung kristalle_zaehlen].all? do |n|
      Scripts.instance_variable_get(:@defs).key?(n)
    end
end

GameState.reset!(Database.system)
check('Schalter-Startwerte') { GameState.switch('kristall_aktiv') == false }
check('Variablen-Startwerte') { GameState.variable('kristalle') == 0.0 }

events = map['events'].map { |h| Event.new(h) }
mara   = events.find { |e| e.id == 1 }
krist0 = events.find { |e| e.id == 2 }
check('6 Events geladen') { events.size == 6 }
check('Touch-Event vorhanden') { events.count { |e| e.trigger == 'touch' } == 1 }
check('Mara in Reichweite bei (0,5.0)') { mara.near?(0.0, 5.0) }
check('Mara ausser Reichweite bei (0,12.0)') { !mara.near?(0.0, 12.0) }

# --- Erstes Gespräch: noch keine Kristalle --------------------------------
# Y2: Wächter Mara ruft zuerst das Projekt-Script dorf_begruessung auf
# (log + add_variable + say), danach kommt der Quest-Dialog wie bisher.
$interp = Interpreter.new
$interp.start(mara.commands)
drain
check('Projekt-Script dorf_begruessung läuft vor dem Dialog') do
  $interp.message == ['Das ist ein Projekt-Script.',
                      'Bearbeitet wird es im Script-Editor des Editors.'] &&
    GameState.variable('script_aufrufe') == 1.0
end
press!
check('Script-Zeile 2 sichtbar') do
  $interp.message == ['Bearbeitet wird es im Script-Editor des Editors.']
end
press!
check('Script-Fenster zu') { $interp.message.nil? }
drain
check('Mara begrüsst (2 Zeilen)') { $interp.message ==
  ['Mara: Drei Kristalle trägt der Weg nach Norden.', 'Hol sie dir, dann öffnen sich die Tore.'] }
press!
check('Zeile 2 sichtbar') { $interp.message == ['Hol sie dir, dann öffnen sich die Tore.'] }
press!
check('Fenster zu') { $interp.message.nil? }
drain
check('Danach else-Zweig (noch nicht genug)') { $interp.message ==
  ['Mara: Noch nicht genug. Zähle sie selbst nach.'] }
press!
drain
check('Interpreter fertig, Schalter unverändert') { !$interp.running? && !GameState.switch('kristall_aktiv') }

# --- Kristall einsammeln ---------------------------------------------------
$interp = Interpreter.new
$interp.start(krist0.commands)
drain
press!
drain
check('kristall_0_genommen gesetzt') { GameState.switch('kristall_0_genommen') }
check('kristalle = 1') { GameState.variable('kristalle') == 1.0 }

$interp = Interpreter.new
$interp.start(krist0.commands)
drain
check('zweites Ansprechen: "schon geborgen"') { $interp.message == ['Der Kristall ist schon geborgen.'] }
press!
drain
check('kristalle bleibt 1') { GameState.variable('kristalle') == 1.0 }

# --- Quest abschliessen ----------------------------------------------------
GameState.set_variable('kristalle', 3)
$interp = Interpreter.new
$interp.start(mara.commands)
drain
press! # Script-Fenster (dorf_begruessung) schliessen
press!
drain  # jetzt das erste Quest-Fenster
press!
press!
drain
check('Mara gratuliert') { $interp.message == ['Mara: Du hast alle drei!', 'Mara: Ich aktiviere den Schutzkreis.'] }
press!
press!
drain
check('kristall_aktiv wurde gesetzt') { GameState.switch('kristall_aktiv') }
check('Interpreter fertig') { !$interp.running? }

# --- Danksagung nach Abschluss --------------------------------------------
$interp = Interpreter.new
$interp.start(mara.commands)
drain
press! # Script-Fenster (dorf_begruessung) schliessen
press!
drain
check('Dankeszweig aktiv') { $interp.message == ['Mara: Die Kristalle singen wieder.', 'Du hast das Dorf gerettet.'] }

# --- Warte-Kommando --------------------------------------------------------
$interp = Interpreter.new
$interp.start([{ 'cmd' => 'wait', 'seconds' => 0.5 }, { 'cmd' => 'text', 'lines' => ['fertig'] }])
$interp.update(0.016)
check('wait blockiert') { $interp.message.nil? && $interp.running? }
$interp.update(0.6)
$interp.update(0.016)
check('nach wait kommt Text') { $interp.message == ['fertig'] }

# --- Unbekanntes Kommando blockiert nicht ---------------------------------
$interp = Interpreter.new
$interp.start([{ 'cmd' => 'quatsch' }, { 'cmd' => 'text', 'lines' => ['weiter'] }])
drain
check('unbekanntes Kommando übersprungen') { $interp.message == ['weiter'] }

# --- Sound-Kommando ---------------------------------------------------------
$interp = Interpreter.new
$interp.start([{ 'cmd' => 'sound', 'name' => 'jingle', 'volume' => 0.9 }])
drain
check("Sound-Kommando spielt 'jingle'") { Rune::Sound::PLAYED.include?('jingle') }

# --- Ton-Befehle 241/242/245/246 (Etappe 21) --------------------------------
# Audio-Attrappe erst hier: der Interpreter sucht `Audio` zur Laufzeit.
module Audio
  CALLS = []
  def self.bgm_play(name, volume = 100, pitch = 100) = (CALLS << [:bgm_play, name, volume, pitch])
  def self.bgs_play(name, volume = 100, pitch = 100) = (CALLS << [:bgs_play, name, volume, pitch])
  def self.bgm_fade(time) = (CALLS << [:bgm_fade, time])
  def self.bgs_fade(time) = (CALLS << [:bgs_fade, time])
end

Audio::CALLS.clear
$interp = Interpreter.new
$interp.start([
                { 'cmd' => 'play_bgm', 'name' => 'bgm/dorf.wav', 'volume' => 80, 'pitch' => 120 },
                { 'cmd' => 'play_bgs', 'name' => 'bgs/regen.wav' },
                { 'cmd' => 'fadeout_bgm', 'seconds' => 2 },
                { 'cmd' => 'fadeout_bgs', 'seconds' => 0.5 }
              ])
drain
check('play_bgm reicht Name, Lautstärke und Pitch weiter') do
  Audio::CALLS[0] == [:bgm_play, 'bgm/dorf.wav', 80.0, 120]
end
check('play_bgs ohne Angaben nutzt die XP-Defaults') do
  Audio::CALLS[1] == [:bgs_play, 'bgs/regen.wav', 100.0, 100]
end
check('fadeout_bgm rechnet Sekunden in RGSS-Millisekunden') do
  Audio::CALLS[2] == [:bgm_fade, 2000.0]
end
check('fadeout_bgs rechnet Sekunden in Millisekunden') do
  Audio::CALLS[3] == [:bgs_fade, 500.0]
end
check('Lautstärke und Pitch werden auf die XP-Grenzen geklemmt') do
  Audio::CALLS.clear
  $interp = Interpreter.new
  $interp.start([{ 'cmd' => 'play_bgm', 'name' => 'x', 'volume' => 250, 'pitch' => 20 }])
  drain
  Audio::CALLS.first == [:bgm_play, 'x', 100.0, 50]
end
check('play_bgm ohne Namen startet nichts') do
  Audio::CALLS.clear
  $interp = Interpreter.new
  $interp.start([{ 'cmd' => 'play_bgm', 'name' => '   ' }])
  drain
  Audio::CALLS.empty?
end

# --- Musik sichern/merken 243/244/247/248 (Etappe 23) -----------------------
# XP ruft dafuer $game_system.bgm_memorize/bgm_restore/bgs_*; der Interpreter
# haelt sich an dasselbe Ziel. Hier eine Attrappe statt der RGSS-Schicht.
class TonMerkerAttrappe
  attr_reader :calls

  def initialize = (@calls = [])

  def bgm_memorize = (@calls << :bgm_memorize)
  def bgm_restore  = (@calls << :bgm_restore)
  def bgs_memorize = (@calls << :bgs_memorize)
  def bgs_restore  = (@calls << :bgs_restore)
end

$game_system = TonMerkerAttrappe.new
$interp = Interpreter.new
$interp.start([{ 'cmd' => 'save_bgm' }, { 'cmd' => 'resume_bgm' },
              { 'cmd' => 'memorize_audio' }, { 'cmd' => 'restore_audio' }])
drain
check('save_bgm merkt die Musik (XP 243)') { $game_system.calls[0] == :bgm_memorize }
check('resume_bgm holt sie zurueck (XP 244)') { $game_system.calls[1] == :bgm_restore }
check('memorize_audio merkt Musik und Hintergrundklang (XP 247)') do
  $game_system.calls[2, 2] == %i[bgm_memorize bgs_memorize]
end
check('restore_audio holt beide zurueck (XP 248)') do
  $game_system.calls[4, 2] == %i[bgm_restore bgs_restore]
end
check('die vier Befehle laufen ohne Felder durch') { $game_system.calls.size == 6 }

$game_system = nil
check('ohne $game_system bleiben die Merker-Befehle still') do
  $interp = Interpreter.new
  $interp.start([{ 'cmd' => 'save_bgm' }, { 'cmd' => 'restore_audio' },
                 { 'cmd' => 'text', 'lines' => ['weiter'] }])
  drain
  $interp.message == ['weiter']
end

puts($fail.zero? ? 'EVENT-TEST OK (0 Fehler)' : "EVENT-TEST FEHLGESCHLAGEN (#{$fail} Fehler)")
exit($fail.zero? ? 0 : 1)
