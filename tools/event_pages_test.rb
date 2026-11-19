#encoding: utf-8
# ============================================================================
#  Kopfloser Test fuer Event-Seiten (Etappe 18g, RPG-Maker-XP-Feature).
#  Laesst sich ohne Engine starten:
#     ruby -I game/engine tools/event_pages_test.rb
#  Exit-Code 0 = alles gruen.
#
#  Geprueft wird die Spielseite (game/engine/event.rb + main.rb-Logik):
#    - altes Format ohne 'pages' laeuft weiter (implizite Seite 0)
#    - Seiten-Normalisierung (fehlende Felder, kaputte Eintraege)
#    - hoechste Seite mit erfuellten Bedingungen gewinnt
#    - Bedingungen: Schalter (an/aus), Variable (alle Operatoren)
#    - Seitenwechsel setzt 'fired' zurueck (touch feuert erneut)
#    - manual_fire startet die Kommandos der aktiven Seite
# ============================================================================

# --- Minimal-Attrappe der Engine-Schnittstelle -----------------------------
module Rune
  ROOT = File.expand_path('..', __dir__)

  module Log
    def self.info(m) = nil
    def self.warn(m) = puts("WARN  #{m}")
    def self.error(m) = puts("ERROR #{m}")
  end

  module Gfx
    def self.box(*_a) = nil
  end
end

$LOAD_PATH.unshift(File.join(Rune::ROOT, 'game', 'engine'))
require 'data'
require 'state'
require 'issues'
require 'event'
require 'interpreter'

$fail = 0
def check(name)
  ok = yield
  puts(ok ? "  ok   #{name}" : "  FAIL #{name}")
  $fail += 1 unless ok
end

GameState.reset!({})

# --- Altes Format (ohne pages) ---------------------------------------------
alt = Event.new('id' => 9, 'name' => 'Alt', 'x' => 1, 'z' => 2,
                'trigger' => 'touch',
                'commands' => [{ 'cmd' => 'text', 'lines' => ['alt'] }])
check('altes Format: eine implizite Seite') { alt.pages.size == 1 }
check('altes Format: Trigger aus Top-Level') { alt.trigger == 'touch' }
check('altes Format: Kommandos aus Top-Level') { alt.commands.size == 1 }
check('altes Format: Seitenindex 0') { alt.page_index == 0 }
alt.refresh_page!
check('altes Format: Seite 0 bleibt aktiv') { alt.page_index == 0 }

# --- Normalisierung kaputter Seiten ----------------------------------------
kaputt = Event.new('id' => 10, 'name' => 'Kaputt',
                   'pages' => [nil, 'quatsch',
                               { 'commands' => 'kein-array' },
                               { 'trigger' => 42, 'conditions' => 'kein-hash' }])
check('kaputte Seiten: Anzahl erhalten') { kaputt.pages.size == 4 }
check('kaputte Seiten: Ersatzseite action/leer') do
  kaputt.pages[0].trigger == 'action' && kaputt.pages[0].commands.empty? &&
    kaputt.pages[0].conditions.empty?
end
check('kaputte Seiten: trigger 42 -> action') { kaputt.pages[3].trigger == 'action' }
check('kaputte Seiten: commands kein Array -> leer') { kaputt.pages[2].commands.empty? }

# --- hoechste erfuellte Seite gewinnt --------------------------------------
ev = Event.new(
  'id' => 1, 'name' => 'Wache', 'x' => 0, 'z' => 0,
  'pages' => [
    { 'conditions' => {},
      'trigger' => 'action',
      'commands' => [{ 'cmd' => 'text', 'lines' => ['Seite 0: Wer bist du?'] }] },
    { 'conditions' => { 'switch' => { 'name' => 'auftrag', 'value' => true } },
      'trigger' => 'action',
      'commands' => [{ 'cmd' => 'text', 'lines' => ['Seite 1: Gute Reise!'] }] },
    { 'conditions' => { 'switch' => { 'name' => 'auftrag', 'value': true }.transform_keys(&:to_s),
                        'variable' => { 'name' => 'kristalle', 'op' => '>=', 'value' => 3 } },
      'trigger' => 'auto',
      'commands' => [{ 'cmd' => 'text', 'lines' => ['Seite 2: Held des Dorfes!'] }] }
  ]
)
ev.refresh_page!
check('ohne Bedingungen: Seite 0 aktiv') { ev.page_index == 0 && ev.trigger == 'action' }

GameState.set_switch('auftrag', true)
ev.refresh_page!
check('Schalter erfuellt: Seite 1 aktiv') { ev.page_index == 1 }

GameState.set_variable('kristalle', 3)
ev.refresh_page!
check('Schalter + Variable erfuellt: Seite 2 aktiv') { ev.page_index == 2 && ev.trigger == 'auto' }

GameState.set_variable('kristalle', 2)
ev.refresh_page!
check('Variable wieder zu niedrig: zurueck auf Seite 1') { ev.page_index == 1 }

GameState.set_switch('auftrag', false)
ev.refresh_page!
check('Schalter aus: zurueck auf Seite 0') { ev.page_index == 0 }

# --- Bedingungen-Operatoren -------------------------------------------------
GameState.set_variable('gold', 50)
[
  ['==', 50, true], ['==', 49, false], ['!=', 49, true], ['!=', 50, false],
  ['<', 51, true], ['<', 50, false], ['<=', 50, true], ['<=', 49, false],
  ['>', 49, true], ['>', 50, false], ['>=', 50, true], ['>=', 51, false]
].each do |op, val, soll|
  c = { 'variable' => { 'name' => 'gold', 'op' => op, 'value' => val } }
  check("Variable #{op} #{val} -> #{soll}") { Event.conditions_met?(c) == soll }
end
check('leere Bedingungen immer erfuellt') { Event.conditions_met?({}) }
check('nil-Bedingungen immer erfuellt') { Event.conditions_met?(nil) }
check('unbekannter Operator blockiert nicht') do
  Event.conditions_met?('variable' => { 'name' => 'gold', 'op' => '~', 'value' => 1 })
end
check('Schalter an erwartet, aus vorhanden') do
  !Event.conditions_met?('switch' => { 'name' => 'auftrag', 'value' => true })
end
check('fehlender Schalter gilt als aus') do
  Event.conditions_met?('switch' => { 'name' => 'gibtsnicht', 'value' => false })
end

# --- Seitenwechsel resettet 'fired' (touch feuert erneut) -------------------
touch = Event.new(
  'id' => 2, 'name' => 'Tor', 'x' => 0, 'z' => 0,
  'pages' => [
    { 'conditions' => {}, 'trigger' => 'touch',
      'commands' => [{ 'cmd' => 'text', 'lines' => ['verschlossen'] }] },
    { 'conditions' => { 'switch' => { 'name' => 'auftrag', 'value' => true } },
      'trigger' => 'touch',
      'commands' => [{ 'cmd' => 'text', 'lines' => ['offen!'] }] }
  ]
)
touch.refresh_page!
touch.fired = true
check('Seite 0 gefeuert') { touch.fired }
GameState.set_switch('auftrag', true)
touch.refresh_page!
check('Seitenwechsel resettet fired') { !touch.fired && touch.page_index == 1 }
touch.fired = true
GameState.set_switch('auftrag', false)
touch.refresh_page!
check('Rueckwechsel resettet fired ebenfalls') { !touch.fired && touch.page_index == 0 }
touch.refresh_page!
check('gleiche Seite laesst fired unangetastet') { !touch.fired }
touch.fired = true
touch.refresh_page!
check('refresh ohne Wechsel: fired bleibt true') { touch.fired }

# --- manual_fire nutzt die aktive Seite ------------------------------------
interp = Interpreter.new
GameState.set_switch('auftrag', true)
GameState.set_variable('kristalle', 3)
ev.refresh_page!
check('aktive Seite ist 2') { ev.page_index == 2 }
ev.manual_fire(interp)
interp.update(0.016)
check('manual_fire: Kommandos von Seite 2') { interp.message == ['Seite 2: Held des Dorfes!'] }
interp = Interpreter.new
GameState.set_switch('auftrag', false)
GameState.set_variable('kristalle', 0)
ev.refresh_page!
ev.manual_fire(interp)
interp.update(0.016)
check('manual_fire: Kommandos von Seite 0') { interp.message == ['Seite 0: Wer bist du?'] }

# --- Interpreter laeuft ueber Seitenkommandos (Integration) ----------------
GameState.reset!({})
seq = Event.new(
  'id' => 3, 'name' => 'Kiste', 'x' => 0, 'z' => 0,
  'pages' => [
    { 'conditions' => {},
      'trigger' => 'action',
      'commands' => [
        { 'cmd' => 'switch', 'name' => 'kiste_offen', 'value' => true },
        { 'cmd' => 'text', 'lines' => ['Du oeffnest die Kiste.'] }
      ] },
    { 'conditions' => { 'switch' => { 'name' => 'kiste_offen', 'value' => true } },
      'trigger' => 'action',
      'commands' => [{ 'cmd' => 'text', 'lines' => ['Die Kiste ist leer.'] }] }
  ]
)
seq.refresh_page!
interp = Interpreter.new
seq.manual_fire(interp)
interp.update(0.016)
check('Kiste: erster Text') { interp.message == ['Du oeffnest die Kiste.'] }
press_like = lambda {
  interp.update(0.016) # Textfenster wartet auf E
}
# Eingabe simulieren: Interpreter liest Rune::Input.pressed? - hier per
# direktem Vortreiben der Nachrichtenschlange (letzte Zeile schliesst).
until interp.message.nil?
  # advance_text benoetigt pressed? - ohne Stub-Input schliessen wir direkt:
  interp.instance_variable_set(:@message, nil)
  interp.update(0.016)
end
check('Kiste: Schalter gesetzt') { GameState.switch('kiste_offen') }
check('Interpreter fertig') { !interp.running? }
seq.refresh_page!
check('Kiste: jetzt Seite 1 aktiv') { seq.page_index == 1 }
interp = Interpreter.new
seq.manual_fire(interp)
interp.update(0.016)
check('Kiste: zweite Seite spricht') { interp.message == ['Die Kiste ist leer.'] }

puts($fail.zero? ? 'EVENT-PAGES-TEST OK (0 Fehler)' : "EVENT-PAGES-TEST FEHLGESCHLAGEN (#{$fail} Fehler)")
exit($fail.zero? ? 0 : 1)
