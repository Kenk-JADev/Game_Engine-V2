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
$interp = Interpreter.new
$interp.start(mara.commands)
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

puts($fail.zero? ? 'EVENT-TEST OK (0 Fehler)' : "EVENT-TEST FEHLGESCHLAGEN (#{$fail} Fehler)")
exit($fail.zero? ? 0 : 1)