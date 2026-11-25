#encoding: utf-8
# ============================================================================
#  Kopfloser Test für den Spieltimer (Etappe 20d, XP 124 + Window_Timer).
#  Lässt sich ohne Engine starten:
#     ruby -I game/engine tools/timer_test.rb
#  Exit-Code 0 = alles grün.
#
#  Geprüft wird:
#    - RGSS::GameSystem: timer_start/update/timer_seconds (Frames wie XP),
#      Ablauf auf 0 und die Arbeitsmarke
#    - Interpreter: Befehl timer (start/stop, Sekunden 0, unbekannter Modus,
#      ohne $game_system)
#    - UI: Timer-Fenster zeichnet mm:ss (Panel + Text)
#    - MapCheck: Modus-Vokabular und Sekunden-Prüfung
# ============================================================================

# --- Minimal-Attrappe der Engine-Schnittstelle -----------------------------
module Rune
  ROOT = File.expand_path('..', __dir__)

  module Log
    def self.info(m) = nil
    def self.warn(m) = puts("WARN  #{m}")
    def self.error(m) = puts("ERROR #{m}")
  end

  module Window
    def self.width = 1280
    def self.height = 720
  end
end

# Zeichen-Attrappe: die echte UI-Schicht ruft Rune::Gfx.rect/text; hier werden
# die Aufrufe gesammelt, damit die Timer-Anzeige prüfbar ist.
module Rune
  module Gfx
    @rects = []
    @texte = []

    def self.rect(*args) = (@rects << args)
    def self.text(*args) = (@texte << args)
    def self.reset!
      @rects = []
      @texte = []
    end
    def self.rects = @rects
    def self.texte = @texte
  end
end

$LOAD_PATH.unshift(File.join(Rune::ROOT, 'game', 'engine'))
require 'data'
require 'state'
require 'issues'
require 'graphics'   # Graphics.frame_rate
require 'ui'
require 'rgss'
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

def lauf(interp, frames = 3, dt = 0.016)
  frames.times { interp.update(dt) }
  interp
end

GameState.reset!({})

puts '== GameSystem: Timer-Grundlagen =='
sys = RGSS::GameSystem.new
check('frisch: kein Timer aktiv') { sys.timer.zero? && !sys.timer_working }
sys.timer_start(90)
check('timer_start rechnet Sekunden in Frames') do
  sys.timer_working && (sys.timer - 90 * Graphics.frame_rate).abs < 1e-6
end
check('timer_seconds liefert die Restzeit in Sekunden') { (sys.timer_seconds - 90).abs < 1e-6 }
sys.update(10.0)
check('update zählt herunter') { (sys.timer_seconds - 80).abs < 1e-6 && sys.timer_working }
sys.update(200.0)
check('unter 0 bleibt bei 0 und stoppt') { sys.timer.zero? && !sys.timer_working }

puts '== Interpreter: timer-Befehl =='
$game_system = RGSS::GameSystem.new
i = Interpreter.new
i.start([{ 'cmd' => 'timer', 'mode' => 'start', 'seconds' => 120 }])
lauf(i)
check('timer start setzt 120 s') do
  $game_system.timer_working && ($game_system.timer_seconds - 120).abs < 1e-6
end
i = Interpreter.new
i.start([{ 'cmd' => 'timer', 'mode' => 'stop' }])
lauf(i)
check('timer stop löscht die Restzeit') do
  !$game_system.timer_working && $game_system.timer.zero?
end
i = Interpreter.new
i.start([{ 'cmd' => 'timer', 'mode' => 'start', 'seconds' => 0 }])
lauf(i)
check('timer start ohne Sekunden stoppt statt zu blinken') { !$game_system.timer_working }
i = Interpreter.new
i.start([{ 'cmd' => 'timer', 'mode' => 'start' }])
lauf(i)
check('timer ohne mode startet (Default start)') { !$game_system.timer_working }   # 0 s -> sofort aus
i = Interpreter.new
i.start([{ 'cmd' => 'timer', 'mode' => 'manchmal', 'seconds' => 5 },
         { 'cmd' => 'variable', 'name' => 'danach', 'value' => 1 }])
lauf(i)
check('unbekannter Modus wird gemeldet, der Rest läuft') do
  GameState.variable('danach') == 1.0 && !i.running?
end
$game_system = nil
i = Interpreter.new
i.start([{ 'cmd' => 'timer', 'mode' => 'start', 'seconds' => 5 },
         { 'cmd' => 'variable', 'name' => 'ohne_system', 'value' => 1 }])
lauf(i)
check('ohne $game_system: Warnung statt Absturz') { GameState.variable('ohne_system') == 1.0 && !i.running? }

puts '== UI: Timer-Fenster =='
Rune::Gfx.reset!
UI.timer(90.4)
check('zeichnet ein Panel oben rechts') do
  r = Rune::Gfx.rects.first
  r[0] + r[2] <= Rune::Window.width && r[0] > Rune::Window.width / 2
end
check('zeigt mm:ss (01:30)') { Rune::Gfx.texte.any? { |t| t[2] == '01:30' } }
Rune::Gfx.reset!
UI.timer(0)
check('zeigt 00:00 bei abgelaufenem Timer') { Rune::Gfx.texte.any? { |t| t[2] == '00:00' } }
Rune::Gfx.reset!
UI.timer(-5)
check('negative Restzeit wird nicht angezeigt (00:00)') { Rune::Gfx.texte.any? { |t| t[2] == '00:00' } }

puts '== MapCheck: timer-Prüfung =='
OPTS = { prefabs: %w[haus], materials: %w[weg], scripts: [] }.freeze

def karte_mit(cmds)
  { 'name' => 'Testkarte',
    'terrain' => { 'width' => 120.0, 'height' => 90.0, 'resolution' => 64, 'amplitude' => 0.0 },
    'objects' => [],
    'events' => [{ 'id' => 1, 'name' => 'Timer', 'x' => 0.0, 'z' => 0.0, 'radius' => 2.5,
                   'pages' => [{ 'conditions' => {}, 'trigger' => 'action', 'commands' => cmds }] }] }
end

def hat?(funde, fragment, severity = nil)
  funde.any? do |f|
    f.to_s.include?(fragment) && (severity.nil? || f.severity == severity)
  end
end

check('gültiger timer: keine Funde') do
  MapCheck.scan(karte_mit([{ 'cmd' => 'timer', 'mode' => 'start', 'seconds' => 60 }]), **OPTS).empty?
end
check('unbekannter Modus -> Fehler') do
  hat?(MapCheck.scan(karte_mit([{ 'cmd' => 'timer', 'mode' => 'weiter' }]), **OPTS),
       "unbekannter timer-Modus 'weiter'", MapCheck::FEHLER)
end
check('negative Sekunden -> Warnung') do
  hat?(MapCheck.scan(karte_mit([{ 'cmd' => 'timer', 'mode' => 'start', 'seconds' => -1 }]), **OPTS),
       "'seconds' muss eine Zahl >= 0 sein", MapCheck::WARNUNG)
end
check('start ohne Sekunden -> Warnung') do
  hat?(MapCheck.scan(karte_mit([{ 'cmd' => 'timer', 'mode' => 'start' }]), **OPTS),
       'ohne Sekunden', MapCheck::WARNUNG)
end
check('alle 51 Befehlstypen im Register der Prüfung') { MapCheck::CMD_TYPES.size == 51 }

puts($fail.zero? ? 'ALLE CHECKS GRUEN' : "#{$fail} CHECK(S) FEHLGESCHLAGEN")
exit($fail.zero? ? 0 : 1)
