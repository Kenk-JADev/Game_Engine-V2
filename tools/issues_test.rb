#encoding: utf-8
# ============================================================================
#  Kopfloser Test fuer das Laufzeit-Backlog des Spiels (Etappe 18h).
#  Laesst sich ohne Engine starten:
#     ruby -I game/engine tools/issues_test.rb
#  Exit-Code 0 = alles gruen.
#
#  Geprueft: Issues-Modul (report/clear/dump), Interpreter meldet unbekannte
#  Kommandos, Scripts meldet fehlende/defekte Scripts, Party meldet
#  unbekannte Items, Weather meldet unbekannte Typen, issues.json entsteht.
# ============================================================================

module Rune
  ROOT = File.expand_path('..', __dir__)
  LOGGED = []
  @press = false

  def self.press=(v)
    @press = v
  end

  module Log
    def self.info(m) = LOGGED << "info  #{m}"
    def self.warn(m) = LOGGED << "warn  #{m}"
    def self.error(m) = LOGGED << "error #{m}"
  end

  module Input
    def self.pressed?(_key) = Rune.instance_variable_get(:@press)
  end

  module Gfx
    def self.box(*_a) = nil
    def self.texture(*_a) = nil
  end
end

$LOAD_PATH.unshift(File.join(Rune::ROOT, 'game', 'engine'))
require 'data'
require 'state'
require 'issues'
require 'party'
require 'event'
require 'interpreter'
require 'effects'
require 'weather'
require 'scripts'

$fail = 0
def check(name)
  ok = yield
  puts(ok ? "  ok   #{name}" : "  FAIL #{name}")
  $fail += 1 unless ok
end

# In ein temporaeres Arbeitsverzeichnis wechseln, damit issues.json dort landet.
require 'tmpdir'
Dir.mktmpdir('issues_test') do |tmp|
  Dir.chdir(tmp) do
    GameState.reset!({})

    # --- Grundfunktionen -----------------------------------------------------
    Issues.clear!
    check('Start: Backlog leer') { Issues.count.zero? }
    check('Start: keine issues.json') { !File.exist?('log/issues.json') }

    e = Issues.report('Test', 'etwas stimmt nicht')
    check('report liefert Eintrag') { e.is_a?(Hash) && e['area'] == 'Test' }
    check('Backlog zaehlt 1') { Issues.count == 1 }
    check('warn im Log') { Rune::LOGGED.grep(/Backlog.*Test/).any? }
    check('issues.json geschrieben') { File.exist?('log/issues.json') }
    check('log/ Verzeichnis angelegt') { Dir.exist?('log') }

    Issues.report('Test', 'schlimmer', severity: 'error')
    check('error-Eintrag im Log') { Rune::LOGGED.grep(/error .*Backlog.*schlimmer/).any? }
    data = JSON.parse(File.read('log/issues.json'))
    check('issues.json: 2 Eintraege') { data['issues'].size == 2 }
    check('issues.json: severity error') { data['issues'][1]['severity'] == 'error' }

    Issues.clear!
    check('clear! leert und loescht') { Issues.count.zero? && !File.exist?('log/issues.json') }

    # --- Interpreter meldet unbekannte Kommandos -----------------------------
    interp = Interpreter.new
    interp.start([{ 'cmd' => 'quatsch' }, { 'cmd' => 'text', 'lines' => ['weiter'] }])
    interp.update(0.016)
    check('Interpreter lief weiter') { interp.message == ['weiter'] }
    Rune.press = true
    interp.update(0.016)
    Rune.press = false
    check('Interpreter meldete unbekanntes Kommando') do
      Issues.all.any? { |i| i['area'] == 'Interpreter' && i['message'].include?('quatsch') }
    end

    # --- Scripts meldet fehlende und defekte Scripts -------------------------
    Scripts.instance_variable_set(:@defs, {})
    ok = Scripts.run('gibtsnicht')
    check('fehlendes Script -> false') { ok == false }
    check('fehlendes Script gemeldet') do
      Issues.all.any? { |i| i['area'] == 'Scripts' && i['message'].include?('gibtsnicht') }
    end
    ok = Scripts.run_code('kaputt', 'raise "boom"')
    check('defektes Inline-Script -> false') { ok == false }
    check('defektes Script als error gemeldet') do
      Issues.all.any? { |i| i['severity'] == 'error' && i['message'].include?('boom') }
    end

    # --- Party meldet unbekannte Items ---------------------------------------
    Party.setup({ 'actors' => [], 'items' => [] })
    Party.use_item('gibtsnicht')
    check('unbekanntes Item gemeldet') do
      Issues.all.any? { |i| i['area'] == 'Party' && i['message'].include?('gibtsnicht') }
    end

    # --- Weather meldet unbekannte Typen -------------------------------------
    Weather.set('orkan')
    check('unbekannter Wetter-Typ gemeldet') do
      Issues.all.any? { |i| i['area'] == 'Wetter' && i['message'].include?('orkan') }
    end
    check('Wetter faellt auf kein zurueck') { Weather.type == 'kein' }
    Weather.set('regen')
    check('bekannter Typ meldet nichts') do
      Issues.all.none? { |i| i['area'] == 'Wetter' && i['message'].include?('regen') }
    end

    # --- Abschlussstand in der Datei -----------------------------------------
    Issues.dump!
    data = JSON.parse(File.read('log/issues.json'))
    check('issues.json enthaelt alle Bereiche') do
      (data['issues'].map { |i| i['area'] } & %w[Interpreter Scripts Party Wetter]).size == 4
    end
  end
end

puts($fail.zero? ? 'ISSUES-TEST OK (0 Fehler)' : "ISSUES-TEST FEHLGESCHLAGEN (#{$fail} Fehler)")
exit($fail.zero? ? 0 : 1)
