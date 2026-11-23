#encoding: utf-8
# ============================================================================
#  Kopfloser Test für Projekt-Scripts (Etappe 17e).
#     ruby -I game/engine tools/script_test.rb
#  Exit-Code 0 = alles grün.
# ============================================================================

require 'tmpdir'
require 'fileutils'

# --- Minimal-Attrappe der Engine-Schnittstelle -----------------------------
module Rune
  ROOT = File.expand_path('..', __dir__)

  @pos = [1.0, 0.0, 2.0]

  def self.pos=(v)
    @pos = v
  end
  def self.pos = @pos
  def self.yaw=(v)
    @yaw = v
  end
  def self.yaw = @yaw

  module Log
    def self.info(_m) = nil
    def self.warn(m) = puts("WARN  #{m}")
    def self.error(m) = puts("ERROR #{m}")
  end

  module Player
    def self.teleport(x, y, z) = (Rune.pos = [x, y, z])
    def self.position = Rune.pos
  end

  module Camera
    def self.yaw=(v)
      Rune.yaw = v
    end
  end

  module Sound
    PLAYED = []
    def self.play(name, _v = 1.0) = (PLAYED << name)
  end
end

module Game
  def self.gy(_x, _z) = 0.0
end

$LOAD_PATH.unshift(File.join(Rune::ROOT, 'game', 'engine'))
require 'data'
require 'state'
require 'issues'
require 'interpreter'
require 'scripts'
require 'fade'      # Etappe 19e: fade-Helfer im Context
require 'mover'     # Etappe 19g: move_route-Helfer im Context
require 'party'     # Etappe 19i: battle-Helfer braucht die Party
require 'battle'    # Etappe 19i: Kampf-Grundgerüst

module Effects
  LOG = []
  def self.sparkle(x, y, z) = (LOG << [:sparkle, x, y, z])
  def self.dust(x, y, z) = (LOG << [:dust, x, y, z])
  def self.burst(x, y, z, color: 0) = (LOG << [:burst, x, y, z, color])
end

$fail = 0
def check(name)
  ok = yield
  puts(ok ? "  ok   #{name}" : "  FAIL #{name}")
  $fail += 1 unless ok
end

def drain(interp, limit = 100)
  limit.times do
    break if interp.message
    break unless interp.running?
    interp.update(0.016)
  end
end

def press(interp)
  Rune::Input.instance_variable_set(:@press, true)
  interp.update(0.016)
  Rune::Input.instance_variable_set(:@press, false)
end

module Rune
  module Input
    def self.pressed?(_key) = Rune::Input.instance_variable_get(:@press)
  end
end

# --- Testprojekt mit scripts.json ------------------------------------------
tmp = Dir.mktmpdir('rune_scripts')
begin
  FileUtils.mkdir_p(File.join(tmp, 'maps'))
  File.write(File.join(tmp, 'system.json'), '{"name":"Test","switches":{},"variables":{}}',
             encoding: 'UTF-8')
  File.write(File.join(tmp, 'scripts.json'), <<~JSON, encoding: 'UTF-8')
    {"scripts":[
      {"name":"zaehlen","code":"set_switch 'script_an'\\nadd_variable 'zaehler', 2\\nlog 'gezählt'"},
      {"name":"gruss","code":"say ['Hallo aus dem Script.','Zweite Zeile.']"},
      {"name":"kaputt","code":"das_ist_kein_ruby("}
    ]}
  JSON

  $project_root = tmp
  Database.reset!
  check('scripts.json geladen') { Scripts.load_project }
  check('drei Scripts bekannt') { Scripts.names == %w[gruss kaputt zaehlen] }
  check('defined?') { Scripts.defined?('gruss') && !Scripts.defined?('fehlt') }

  GameState.reset!(Database.system)

  # 1) Script setzt Schalter/Variable
  interp = Interpreter.new
  check('script-Kommando startet') do
    interp.start([{ 'cmd' => 'script', 'name' => 'zaehlen' },
                  { 'cmd' => 'text', 'lines' => ['weiter'] }])
  end
  drain(interp)
  check('Schalter aus Script gesetzt') { GameState.switch('script_an') == true }
  check('Variable aus Script erhöht') { GameState.variable('zaehler') == 2.0 }
  check('danach normaler Text') { interp.message == ['weiter'] }
  press(interp)
  drain(interp)
  check('Interpreter fertig') { !interp.running? }

  # 2) say() blockiert und liefert die Zeilen
  interp = Interpreter.new
  interp.start([{ 'cmd' => 'script', 'name' => 'gruss' }])
  drain(interp)
  check('say zeigt erste Zeile') { interp.message == ['Hallo aus dem Script.', 'Zweite Zeile.'] }
  press(interp)
  check('say zeigt zweite Zeile') { interp.message == ['Zweite Zeile.'] }
  press(interp)
  drain(interp)
  check('nach say fertig') { !interp.running? }

  # 3) Fehlerhaftes Script bricht den Interpreter nicht ab
  interp = Interpreter.new
  interp.start([{ 'cmd' => 'script', 'name' => 'kaputt' },
                { 'cmd' => 'text', 'lines' => ['trotzdem da'] }])
  drain(interp)
  check('kaputtes Script übersprungen') { interp.message == ['trotzdem da'] }
  press(interp)
  drain(interp)
  check('danach fertig') { !interp.running? }

  # 4) Inline-Code aus einem Event-Befehl
  interp = Interpreter.new
  interp.start([{ 'cmd' => 'script', 'name' => 'inline',
                  'code' => "set_variable 'inline_wert', 42\n sound 'klick', 0.5" }])
  drain(interp)
  check('Inline-Code Variable') { GameState.variable('inline_wert') == 42.0 }
  check('Inline-Code Sound') { Rune::Sound::PLAYED.include?('klick') }

  # 5) Context-Helfer: effect/teleport
  ev = Struct.new(:x, :z).new(3.0, 4.0)
  interp = Interpreter.new
  Scripts.run_code('helfer', "effect 'sparkle'\nteleport 1.5, 2.5, yaw: 1.0", interp, ev)
  check('effect am Event') { Effects::LOG.last[0] == :sparkle && Effects::LOG.last[1] == 3.0 }
  check('teleport gesetzt') { Rune.pos == [1.5, 0.2, 2.5] }
  check('yaw gesetzt') { Rune.yaw.to_f == 1.0 }

  # 6) Context-Helfer (Etappe 19e): fade + self_switch. Die RGSS-Schicht
  #    ($game_*, Graphics, Input, Audio) prüft tools/rgss_test.rb separat.
  Fade.reset!
  interp = Interpreter.new
  Scripts.run_code('blende', "fade 'aus', 0.5", interp, ev)
  check('fade-Helfer startet die Blende') { Fade.laeuft? }
  Fade.reset!

  evs = Struct.new(:x, :z).new(1.0, 2.0)
  def evs.self_switch_key
    'welt01|TestEvent'
  end
  Scripts.run_code('setzen', "self_switch 'B', true", interp, evs)
  check('self_switch-Helfer setzt am Besitzer-Event') do
    GameState.self_switch('welt01|TestEvent', 'B')
  end
  Scripts.run_code('lesen', "$rgss_gelesen = self_switch 'B'", interp, evs)
  check('self_switch-Helfer liest ohne Argument') { $rgss_gelesen == true }
  Scripts.run_code('ohne_event', "$rgss_ohne = self_switch 'C'", interp, ev)
  check('self_switch ohne Besitzer-Event -> nil') { $rgss_ohne.nil? }

  # 7) Context-Helfer (Etappe 19g): move_route startet eine Bewegungsroute.
  Mover.reset!
  Scripts.run_code('route', "move_route 'spieler', %w[up]", interp, ev)
  check('move_route-Helfer startet Route') { Mover.active? }
  Mover.reset!

  # 8) Context-Helfer (Etappe 19i): battle startet einen Kampf. run_code
  # liefert true/false (Ausnahme = false), den Script-Wert fangen wir per
  # Global ein. Achtung: $project_root zeigt hier auf das tmp-Projekt OHNE
  # database.json - für die Party kurz auf das echte Repo umschalten.
  altes_root = $project_root
  $project_root = nil
  Database.reset!
  Party.setup(Database.database)
  Scripts.run_code('kampf_unbekannt', "$rgss_kampf1 = battle 'gibt_es_nicht'", interp, ev)
  check('battle-Helfer: unbekannte Truppe -> false') { $rgss_kampf1 == false }
  ok = Scripts.run_code('kampf', "$rgss_kampf2 = battle 'waldweg'", interp, ev)
  check('battle-Helfer startet echten Kampf') do
    ok == true && $rgss_kampf2 == true && Battle.active?
  end
  Battle.abort!
  $project_root = altes_root
  Database.reset!

  # Opt-in-Startscripts laufen im Top-Level wie eine RGSS-Scriptliste, in
  # Reihenfolge und erneut nach F5. Nicht markierte Event-Scripts laufen nie
  # automatisch. Ein fehlerhafter Eintrag verhindert die folgenden nicht.
  File.write(File.join(tmp, 'scripts.json'), JSON.generate('scripts' => [
    { 'name' => 'erstes', 'autoload' => true,
      'code' => "$start_order = ['eins']\nclass RuneRGSSProbe; def self.ok; true; end; end" },
    { 'name' => 'event', 'code' => "$start_order << 'unerwartet'" },
    { 'name' => 'kaputt_start', 'autoload' => true, 'code' => 'syntax(' },
    { 'name' => 'zweites', 'autoload' => true, 'code' => "$start_order << 'zwei'" }
  ]), encoding: 'UTF-8')
  check('Startscript-Liste geladen') { Scripts.load_project }
  check('Startscriptfehler wird gemeldet, folgende Scripte laufen weiter') { Scripts.run_startup == false }
  check('nur markierte Scripte laufen in Dateireihenfolge') { $start_order == %w[eins zwei] }
  check('Top-Level-Klassendefinition wie bei RGSS möglich') { RuneRGSSProbe.ok }
  check('Neustart führt markierte Scripte erneut aus') { Scripts.run_startup == false && $start_order == %w[eins zwei] }
  File.write(File.join(tmp, 'scripts.json'), '{', encoding: 'UTF-8')
  check('ungültiges neues Dokument verwirft alte Startliste') { !Scripts.load_project && Scripts.run_startup }
  Object.send(:remove_const, :RuneRGSSProbe) if Object.const_defined?(:RuneRGSSProbe)
ensure
  FileUtils.remove_entry(tmp)
end

puts($fail.zero? ? 'SCRIPT-TEST OK (0 Fehler)' : "SCRIPT-TEST FEHLGESCHLAGEN (#{$fail} Fehler)")
exit($fail.zero? ? 0 : 1)
