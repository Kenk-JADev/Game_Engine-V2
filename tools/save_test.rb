#encoding: utf-8
# ============================================================================
#  Kopfloser Test für Speichern/Laden (Etappe 10): GameState-Snapshot,
#  Projekt-Spielstände, fired-Flags über JSON-Roundtrip.
#     ruby -I game/engine tools/save_test.rb
# ============================================================================

$LOAD_PATH.unshift(File.join(File.expand_path('..', __dir__), 'game', 'engine'))

module Rune
  ROOT = File.expand_path('..', __dir__)
  module Log
    def self.info(m) = nil
    def self.warn(m) = puts("WARN  #{m}")
    def self.error(m) = puts("ERROR #{m}")
  end
end

require 'state'
require 'issues'
require 'project'
require 'json'

$fail = 0
def check(name)
  ok = yield
  puts(ok ? "  ok   #{name}" : "  FAIL #{name}")
  $fail += 1 unless ok
end

# --- GameState: Snapshot und Restore ---------------------------------------
GameState.reset!({ 'switches' => { 'a' => true, 'b' => false },
                   'variables' => { 'kristalle' => 2 } })
GameState.set_switch('b', true)
GameState.add_variable('kristalle', 1)

snap = GameState.snapshot
check('Snapshot enthält Schalter') { snap['switches'] == { 'a' => true, 'b' => true } }
check('Snapshot enthält Variablen') { snap['variables'] == { 'kristalle' => 3.0 } }

# JSON-Roundtrip (wie beim echten Speichern/Laden)
snap2 = JSON.parse(JSON.generate(snap))
GameState.reset!({ 'switches' => {}, 'variables' => {} })
GameState.restore!(snap2)
check('Restore nach Reset: Schalter') { GameState.switch('a') && GameState.switch('b') }
check('Restore nach Reset: Variable') { GameState.variable('kristalle') == 3.0 }

# Leerer Snapshot ist unschädlich
GameState.restore!({})
check('Leerer Snapshot: keine Schalter') { GameState.switch('a') == false }
check('Leerer Snapshot: Variable 0') { GameState.variable('kristalle') == 0.0 }

# --- Projekt-Spielstände ----------------------------------------------------
name = Project.create('SaveTest')
begin
  check('Neues Projekt ohne Spielstände') { Project.has_saves?(name) == false }
  check('latest_save nil') { Project.latest_save(name).nil? }
  check('load_save leerer Slot nil') { Project.load_save(name, 2).nil? }

  data1 = { 'map' => 'welt01', 'saved' => '2025-01-01 10:00', 'time' => 42.5,
            'player' => { 'x' => 1.5, 'y' => 0.2, 'z' => -3.0, 'yaw' => 3.14, 'mode' => 'third' },
            'state' => { 'switches' => { 'x' => true }, 'variables' => { 'kristalle' => 1.0 } },
            'fired' => ['Wegweiser am Zaun'] }
  Project.save(name, 1, data1)
  check('has_saves? nach Speichern') { Project.has_saves?(name) }

  d = Project.load_save(name, 1)
  check('Roundtrip: Karte') { d['map'] == 'welt01' }
  check('Roundtrip: Position') { d.dig('player', 'x') == 1.5 && d.dig('player', 'z') == -3.0 }
  check('Roundtrip: Blick') { d.dig('player', 'yaw') == 3.14 }
  check('Roundtrip: Schalter') { d.dig('state', 'switches', 'x') == true }
  check('Roundtrip: fired-Flags') { d['fired'] == ['Wegweiser am Zaun'] }

  check('save_info liefert Zeitstempel und Spielzeit') do
    info = Project.save_info(name, 1)
    info['saved'] == '2025-01-01 10:00' && info['time'] == 42.5
  end
  check('save_info leerer Slot nil') { Project.save_info(name, 3).nil? }

  Project.save(name, 3, { 'saved' => '2025-01-02 11:00' })
  check('latest_save = 3') { Project.latest_save(name) == 3 }
  Project.save(name, 2, { 'saved' => '2025-01-02 12:00' })
  check('latest_save bleibt 3') { Project.latest_save(name) == 3 }

  info = Project.info(name)
  check('info zählt Spielstände') { info['saves'] == 3 }
ensure
  Project.delete(name)
end
check('Testprojekt aufgeräumt') { !Project.valid?(name) }

puts($fail.zero? ? 'SAVE-TEST OK (0 Fehler)' : "SAVE-TEST FEHLGESCHLAGEN (#{$fail} Fehler)")
exit($fail.zero? ? 0 : 1)
