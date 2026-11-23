#encoding: utf-8
# ============================================================================
# F6-Brücke: Die C++-Formularlogik erzeugt echte Änderungen an einem alten
# Event; die Ruby-Laufzeit lädt und interpretiert genau diese JSON-Daten.
#   ruby tools/event_bridge_check.rb <pfad-zu-json_test>
# Alternativ: --fixture <datei> für eine bereits erzeugte Brückendatei.
# ============================================================================
require 'json'
require 'tmpdir'
require 'open3'

ROOT = File.expand_path('..', __dir__)
module Rune
  ROOT = File.expand_path('..', __dir__)
  module Log
    @messages = []
    class << self
      attr_reader :messages
      def info(message) = @messages << message
      def warn(message) = @messages << message
      def error(message) = @messages << message
    end
  end
  module Input
    @keys = []
    class << self
      attr_accessor :keys
      def pressed?(key) = @keys.include?(key)
    end
  end
  module Sound
    def self.play(*) = nil
  end
end

$LOAD_PATH.unshift(File.join(ROOT, 'game', 'engine'))
require 'data'
require 'state'
require 'issues'
require 'event'
require 'interpreter'

$fail = 0
$count = 0
def check(name)
  $count += 1
  ok = yield
  puts(ok ? "  ok   #{name}" : "  FAIL #{name}")
  $fail += 1 unless ok
end

def pruefe(path)
  data = JSON.parse(File.read(path, encoding: 'utf-8'))
  old = data.fetch('old')
  migrated = data.fetch('migrated')
  added = data.fetch('added')
  copy = data.fetch('copy')
  trigger = data.fetch('trigger')
  GameState.reset!({})

  alte_seite = Event.build_pages(old).first
  migrierte_seite = Event.build_pages(migrated).first
  check('Altformat lädt weiterhin Trigger und Befehle') do
    old['pages'].nil? && alte_seite.trigger == 'touch' && alte_seite.commands == old['commands']
  end
  check('Triggeränderung bleibt im Altformat und ist zur Laufzeit sichtbar') do
    !trigger.key?('pages') && Event.new(trigger).trigger == 'parallel' && trigger['commands'] == old['commands']
  end
  check('Migration lässt Top-Level-Altdaten und fremde Eventfelder stehen') do
    migrated['trigger'] == old['trigger'] && migrated['commands'] == old['commands'] &&
      migrated['visual'] == 'schild' && migrated['extra'] == old['extra']
  end
  check('Ruby nutzt nach Migration die bearbeitete Seite, nicht alte Top-Level-Befehle') do
    migrated['name'] == 'Brücke neu' && migrierte_seite.trigger == 'touch' &&
      migrierte_seite.commands[0]['then'][0]['lines'] == ['Neu'] &&
      old['commands'][0]['then'][0]['lines'] == ['Alt']
  end
  check('präsentierte Ganzzahl, Präzision und verschachtelte Metadaten sind exakt') do
    migrated['z'] == 9_007_199_254_740_993 && migrated['x'] == old['x'] &&
      migrierte_seite.commands[0]['condition_meta'] == 42 &&
      migrierte_seite.commands[0]['then'][0]['extra']['integer'] == 9_007_199_254_740_993
  end
  check('Seitenbedingung ist für Ruby lesbar und abhängig vom Schalter') do
    c = migrierte_seite.conditions
    !Event.conditions_met?(c) && (GameState.set_switch('offen', true); Event.conditions_met?(c))
  end
  check('Neue Seite übernimmt die alte Seite unverändert') do
    pages = Event.build_pages(added)
    pages.size == 2 && pages.first.trigger == alte_seite.trigger &&
      pages.first.commands == alte_seite.commands && pages.last.commands.empty?
  end
  check('höchste erfüllte neu eingefügte Seite gewinnt weiterhin') do
    ev = Event.new(added)
    ev.refresh_page!
    ev.page_index == 1 && ev.trigger == 'action'
  end
  check('Duplikat bleibt altformat-kompatibel und inkrementiert große Position exakt') do
    copy['id'] == 8 && copy['name'] == 'Brücke (Kopie)' && copy['z'] == 9_007_199_254_740_994 &&
      !copy.key?('pages') && Event.build_pages(copy).first.commands == alte_seite.commands && copy['extra'] == old['extra']
  end
  original_event = Event.new(old)
  unique_event = Event.new(data.fetch('unique_copy'))
  GameState.set_self_switch(original_event.self_switch_key, 'A', true)
  check('Mehrfachkopie bekommt einen eindeutigen Laufzeit-Schlüssel für Self-Switches') do
    unique_event.name == 'Brücke (Kopie) 3' &&
      GameState.self_switch(original_event.self_switch_key, 'A') &&
      !GameState.self_switch(unique_event.self_switch_key, 'A')
  end
  renamed_event = Event.new(migrated)
  check('Umbenennung ändert den Self-Switch-Schlüssel und migriert alte Werte nicht') do
    renamed_event.self_switch_key != original_event.self_switch_key &&
      GameState.self_switch(original_event.self_switch_key, 'A') &&
      !GameState.self_switch(renamed_event.self_switch_key, 'A')
  end
  interpreter = Interpreter.new
  interpreter.start([data.fetch('number')])
  interpreter.update(0.016)
  check('Validierter Zahleneingabe-Startwert erscheint unverändert in der Laufzeit') do
    interpreter.num_input && interpreter.num_input[:digits] == 2 &&
      interpreter.num_input[:ziffern] == [9, 9] && !interpreter.num_input[:cancel]
  end

  [true, false].each do |wert|
    GameState.set_switch('quest', wert)
    interpreter = Interpreter.new
    interpreter.start(migrierte_seite.commands)
    interpreter.update(0.016)
    check("Interpreter führt nach Migration den #{wert ? 'Dann' : 'Sonst'}-Zweig aus") do
      interpreter.message == [wert ? 'Neu' : 'Zu']
    end
  end
  reordered = data.fetch('choice_reordered')
  check('Wahloptionen wurden geändert, sämtliche Zweige und Metadaten nach Index erhalten') do
    reordered['options'] == %w[Nein Ja] && reordered['branches'] == data['choice_old']['branches'] &&
      reordered['branches'].size == 3 && reordered['meta'] == data['choice_old']['meta']
  end
  interpreter = Interpreter.new
  interpreter.start([reordered])
  interpreter.update(0.016)
  check('Interpreter sieht die neue Reihenfolge der Wahloptionen') do
    interpreter.choices && interpreter.choices[:options] == %w[Nein Ja]
  end
  Rune::Input.keys = [:enter]
  interpreter.update(0.016)
  Rune::Input.keys = []
  interpreter.update(0.016)
  check('Erste Option führt weiterhin Zweig mit Index 0 aus') { Rune::Log.messages.include?('[Event] eins') }
end

if ARGV[0] == '--fixture'
  abort('Aufruf: --fixture <datei>') unless ARGV.size == 2
  pruefe(ARGV[1])
else
  binary = ARGV[0]
  abort('Aufruf: ruby tools/event_bridge_check.rb <pfad-zu-json_test>') unless binary && File.file?(binary)
  Dir.mktmpdir('rune_event_bruecke') do |tmp|
    output = File.join(tmp, 'events.json')
    log, status = Open3.capture2e(binary, '--event-bridge', output)
    abort("C++-Event-Brücke fehlgeschlagen: #{log}") unless status.success? && File.file?(output)
    pruefe(output)
  end
end

puts "EVENT-BRÜCKE: #{$count} Checks, #{$fail} Fehler"
exit($fail.zero? ? 0 : 1)
