#encoding: utf-8
# ============================================================================
#  Kopfloser Test für die Bildschirmblende (Etappe 19d, RPG-Maker-XP-Feature).
#  Lässt sich ohne Engine starten:
#     ruby -I game/engine tools/fade_test.rb
#  Exit-Code 0 = alles grün.
#
#  Geprüft wird:
#    - Fade: Start/Ziel/Richtung, zeitlicher Verlauf (update), Minimaldauer,
#      reset!, sichtbarkeit und draw (UI-Panel mit Alpha)
#    - Interpreter: {"cmd":"fade",...} startet die Blende
#    - MapCheck: unbekannte Richtung/negative Dauer -> Warnung
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

# UI-Attrappe: Panel-Aufrufe sammeln, damit draw prüfbar wird.
module UI
  @panels = []

  def self.panel(*args)
    @panels << args
  end

  def self.panels = @panels
  def self.reset_panels! = @panels = []
end

$LOAD_PATH.unshift(File.join(Rune::ROOT, 'game', 'engine'))
require 'data'
require 'state'
require 'issues'
require 'fade'
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

puts '== Fade: Grundlagen =='
Fade.reset!
check('Startzustand: hell') { Fade.alpha.zero? && !Fade.sichtbar? && !Fade.laeuft? }

Fade.start('aus', 1.0)
check('Fade-out läuft') { Fade.laeuft? }
Fade.update(0.5)
check('nach halber Zeit: alpha ~ 0.5') { (Fade.alpha - 0.5).abs < 0.001 }
check('Bild teilweise abgedunkelt') { Fade.sichtbar? }
Fade.update(0.6)
check('über das Ziel hinaus: alpha bleibt 1.0') { Fade.alpha == 1.0 }
check('angekommen: läuft nicht mehr') { !Fade.laeuft? }

Fade.start('ein', 0.5)
Fade.update(0.25)
check('Fade-in halbe Zeit: alpha ~ 0.5') { (Fade.alpha - 0.5).abs < 0.001 }
Fade.update(1.0)
check('Fade-in fertig: alpha 0') { Fade.alpha.zero? && !Fade.sichtbar? }

puts '== Fade: Sonderfälle =='
Fade.start('aus', 0.0)   # Minimaldauer 0.05s greift
Fade.update(0.05)
check('Sekunden 0 -> sofort dunkel (Minimaldauer)') { Fade.alpha == 1.0 }
Fade.reset!
check('reset! -> hell und idle') { Fade.alpha.zero? && !Fade.laeuft? }
Fade.update(0.5)
check('update im Ruhezustand ändert nichts') { Fade.alpha.zero? }

UI.reset_panels!
Fade.draw(1280, 720)
check('draw bei alpha 0 zeichnet nichts') { UI.panels.empty? }
Fade.start('aus', 1.0)
Fade.update(0.4)
UI.reset_panels!
Fade.draw(1280, 720)
check('draw legt ein Vollbild-Panel mit Alpha') do
  UI.panels.size == 1 && UI.panels[0][0] == 0 && UI.panels[0][1] == 0 &&
    UI.panels[0][2] == 1280 && UI.panels[0][3] == 720 &&
    UI.panels[0][4] == 0x000000 && (UI.panels[0][5] - 0.4).abs < 0.01
end
Fade.reset!

puts '== Interpreter: fade-Befehl =='
interp = Interpreter.new
interp.start([{ 'cmd' => 'fade', 'direction' => 'aus', 'seconds' => 0.5 }])
interp.update(0.016)
check('fade-Befehl startet die Blende') { Fade.laeuft? }
Fade.update(0.5)
check('Blende wird dunkel') { Fade.alpha == 1.0 }
Fade.reset!   # schon dunkel würde "läuft" nie auslösen - fairer Neustart
interp2 = Interpreter.new
interp2.start([{ 'cmd' => 'fade' }])   # Richtung/Dauer fehlen -> Defaults
interp2.update(0.016)
check('fade ohne Parameter: Defaults aus/0.5s laufen') { Fade.laeuft? }
Fade.reset!

puts '== MapCheck: fade-Prüfung =='
OPTS = { prefabs: %w[haus], materials: %w[weg], scripts: [] }.freeze

def karte_mit(cmds)
  {
    'name' => 'Testkarte',
    'terrain' => { 'width' => 120.0, 'height' => 90.0, 'resolution' => 64,
                   'amplitude' => 0.0 },
    'objects' => [],
    'events' => [{ 'id' => 1, 'name' => 'Blende', 'x' => 0.0, 'z' => 0.0,
                   'radius' => 2.5,
                   'pages' => [{ 'conditions' => {}, 'trigger' => 'action',
                                 'commands' => cmds }] }]
  }
end

def hat?(funde, fragment, severity = nil)
  funde.any? do |f|
    f.to_s.include?(fragment) && (severity.nil? || f.severity == severity)
  end
end

check('gültiger fade-Befehl: keine Funde') do
  MapCheck.scan(karte_mit([{ 'cmd' => 'fade', 'direction' => 'ein',
                             'seconds' => 0.8 }]), **OPTS).empty?
end
check('unbekannte Richtung -> Warnung') do
  hat?(MapCheck.scan(karte_mit([{ 'cmd' => 'fade', 'direction' => 'seitlich' }]),
                     **OPTS),
       'unbekannte Richtung', MapCheck::WARNUNG)
end
check('negative Dauer -> Warnung') do
  hat?(MapCheck.scan(karte_mit([{ 'cmd' => 'fade', 'seconds' => -1.0 }]), **OPTS),
       "'seconds' muss eine Zahl >= 0 sein", MapCheck::WARNUNG)
end

puts($fail.zero? ? 'ALLE CHECKS GRUEN' : "#{$fail} CHECK(S) FEHLGESCHLAGEN")
exit($fail.zero? ? 0 : 1)
