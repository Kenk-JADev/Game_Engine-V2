#encoding: utf-8
# ============================================================================
#  Kopfloser Test für den Laden (Etappe 20e, XP 302 "Shop-Processing").
#  Lässt sich ohne Engine starten:
#     ruby -I game/engine tools/shop_test.rb
#  Exit-Code 0 = alles grün.
#
#  Geprüft wird:
#    - Shop.start: Normalisierung (Preis aus Befehl/Datenbank), unbekannte
#      Items raus + Backlog, leere Liste öffnet nichts
#    - kaufpreis/verkaufpreis: halber Datenbankpreis, durch den Ladenpreis
#      gedeckelt (kein Gold-Generator)
#    - kaufen: Gold/Inventar, :kein_gold, :unbekannt, :menge, Meldung
#    - verkaufen: Inventar/Gold, :kein_item, :unbekannt, :menge
#    - Fenster-Bedienung: hoch/runter (Umlauf), wechsle_modus!, bestaetigen,
#      zurueck (Esc erst zu den Waren, dann schließen)
#    - Interpreter: Befehl shop blockiert bis Shop.close, Liste läuft weiter
#    - MapCheck: goods Pflicht/leer, Item unbekannt, Preis negativ/keine Zahl,
#      doppelte Ware
#    - UI.shop: zeichnet Reiter, Ware, Gold und nimmt Tasten an
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

  # Eingabe-Attrappe: die UI fragt pressed?(:up) usw ab.
  module Input
    @gedrueckt = []
    def self.setze!(keys) = (@gedrueckt = Array(keys))
    def self.pressed?(key) = @gedrueckt.include?(key)
  end

  # Zeichen-Attrappe: sammelt Aufrufe, damit das Fenster prüfbar ist.
  module Gfx
    @texte = []
    @rects = []
    def self.text(*args) = (@texte << args)
    def self.rect(*args) = (@rects << args)
    def self.reset!
      @texte = []
      @rects = []
    end
    def self.texte = @texte
    def self.rects = @rects
    def self.gesammelt = @texte.map { |t| t[2].to_s }
  end
end

$LOAD_PATH.unshift(File.join(Rune::ROOT, 'game', 'engine'))
require 'data'
require 'state'
require 'issues'
require 'party'
require 'shop'
require 'ui'
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

DB = {
  'gold' => 100,
  'actors' => [{ 'id' => 'arik', 'name' => 'Arik', 'level' => 1, 'hp' => 100, 'mp' => 20 }],
  'items' => [
    { 'id' => 'heiltrank', 'name' => 'Heiltrank', 'kind' => 'heal', 'power' => 40, 'price' => 25 },
    { 'id' => 'apfel', 'name' => 'Apfel', 'kind' => 'heal', 'power' => 5, 'price' => 3 },
    { 'id' => 'ohne_preis', 'name' => 'Fundstueck', 'kind' => 'heal', 'power' => 1 }
  ]
}.freeze

# Die Items des Tests müssen auch für Party.item_def/MapCheck sichtbar sein -
# die lesen Database.database (im Test die Tabelle unten statt der Repo-Datei).
module Database
  def self.database = DB
end

def neu!
  Party.setup(DB)
  Shop.close
  Issues.all.clear
end

def lauf(interp, frames = 3, dt = 0.016)
  frames.times { interp.update(dt) }
  interp
end

puts '== Warenliste =='
neu!
check('start übernimmt Befehlspreis und Datenbankpreis') do
  Shop.start([{ 'item' => 'heiltrank', 'price' => 20 }, { 'item' => 'apfel' }]) &&
    Shop.waren.map { |w| [w[:id], w[:preis]] } == [['heiltrank', 20], ['apfel', 3]]
end
check('start ohne Waren öffnet nichts') do
  Shop.close
  Shop.start([]) == false && !Shop.active?
end
check('unbekanntes Item fliegt raus und landet im Backlog') do
  Shop.start([{ 'item' => 'gibtsnicht' }, { 'item' => 'apfel' }]) &&
    Shop.waren.map { |w| w[:id] } == ['apfel'] &&
    Issues.all.any? { |e| e['message'].include?('gibtsnicht') }
end
check('Ware ohne price nimmt den Datenbankpreis, ohne Preis 0') do
  Shop.start([{ 'item' => 'ohne_preis' }]) && Shop.waren.first[:preis].zero?
end
check('close gibt das Fenster frei') do
  Shop.close
  !Shop.active? && Shop.waren.empty?
end

puts '== Preise =='
neu!
Shop.start([{ 'item' => 'heiltrank', 'price' => 20 }, { 'item' => 'apfel' }])
check('kaufpreis kommt aus der Warenliste') { Shop.kaufpreis('heiltrank') == 20 }
check('kaufpreis ohne Ware in der Liste: Datenbankpreis') { Shop.kaufpreis('ohne_preis').zero? }
check('verkaufspreis ist der halbe Datenbankpreis') { Shop.verkaufspreis('apfel') == 1 }
check('verkaufspreis wird vom Ladenpreis gedeckelt') do
  # Heiltrank: DB 25 -> 12, Laden 20 -> 10; ohne Deckel waere es ein
  # Gold-Generator (fuer 20 kaufen, fuer 12 verkaufen waere egal, aber
  # Laden 3 / DB 25 waere 12 vs 3).
  Shop.start([{ 'item' => 'heiltrank', 'price' => 3 }])
  Shop.verkaufspreis('heiltrank') == 1
end

puts '== Kaufen =='
neu!
Shop.start([{ 'item' => 'heiltrank', 'price' => 20 }])
check('kaufen zieht Gold ab und legt das Item ins Inventar') do
  Shop.kaufen('heiltrank') == :ok && Party.gold == 80 && Party.item_count('heiltrank') == 1
end
check('kaufen mehrerer Stück rechnet mal Anzahl') do
  Shop.kaufen('heiltrank', 3) == :ok && Party.gold == 20 && Party.item_count('heiltrank') == 4
end
check('zu wenig Gold -> :kein_gold, nichts passiert') do
  vorher = Party.item_count('heiltrank')
  Shop.kaufen('heiltrank', 2) == :kein_gold && Party.gold == 20 &&
    Party.item_count('heiltrank') == vorher && Shop.meldung.include?('Gold')
end
check('unbekanntes Item -> :unbekannt') { Shop.kaufen('gibtsnicht') == :unbekannt }
check('Menge 0 oder negativ -> :menge') do
  Shop.kaufen('heiltrank', 0) == :menge && Shop.kaufen('heiltrank', -2) == :menge
end

puts '== Verkaufen =='
neu!
Party.add_item('heiltrank', 2)
Shop.start([{ 'item' => 'heiltrank', 'price' => 20 }])
check('verkaufen bringt Gold und nimmt das Item') do
  Shop.verkaufen('heiltrank') == :ok && Party.item_count('heiltrank') == 1 &&
    Party.gold == 110
end
check('nicht im Inventar -> :kein_item') do
  Party.take_item('heiltrank', 5)
  Shop.verkaufen('heiltrank') == :kein_item && Party.gold == 110
end
check('zu viele Stück -> :kein_item') do
  Party.add_item('apfel', 1)
  Shop.verkaufen('apfel', 2) == :kein_item && Party.item_count('apfel') == 1
end
check('unbekanntes Item -> :unbekannt') { Shop.verkaufen('gibtsnicht') == :unbekannt }

puts '== Fenster-Bedienung =='
neu!
Shop.start([{ 'item' => 'heiltrank', 'price' => 20 }, { 'item' => 'apfel' }])
check('runter/hoch läuft im Kreis') do
  Shop.runter && Shop.sel == 1 && Shop.runter && Shop.sel.zero? && Shop.hoch && Shop.sel == 1
end
check('wechsle_modus! zeigt das Inventar und setzt den Cursor zurück') do
  Party.add_item('apfel', 2)
  Shop.wechsle_modus! == :verkaufen && Shop.liste.map { |w| w[:id] } == ['apfel'] &&
    Shop.sel.zero? && Shop.liste.first[:anzahl] == 2
end
check('bestaetigen verkauft im Verkaufs-Reiter') do
  Shop.bestaetigen == :ok && Party.item_count('apfel') == 1
end
check('zurueck geht erst zu den Waren, dann schließt es') do
  Shop.zurueck == false && Shop.modus == :kaufen && Shop.zurueck == true && !Shop.active?
end
check('bestaetigen ohne Ware meldet das nur') do
  neu!
  Shop.start([{ 'item' => 'apfel' }])
  Shop.wechsle_modus!                      # Inventar ist leer
  Shop.bestaetigen.nil? && Shop.meldung.include?('Keine Ware')
end

puts '== Interpreter: shop-Befehl =='
neu!
i = Interpreter.new
i.start([{ 'cmd' => 'shop', 'goods' => [{ 'item' => 'heiltrank', 'price' => 20 }] },
         { 'cmd' => 'variable', 'name' => 'nachher', 'value' => 1 }])
lauf(i)
check('shop öffnet den Laden und blockiert') do
  Shop.active? && i.running? && GameState.variable('nachher').zero?
end
Shop.kaufen('heiltrank')
Shop.close
lauf(i)
check('nach dem Schließen läuft die Liste weiter') do
  GameState.variable('nachher') == 1.0 && !i.running? && Party.item_count('heiltrank') == 1
end
i = Interpreter.new
i.start([{ 'cmd' => 'shop', 'goods' => [] },
         { 'cmd' => 'variable', 'name' => 'leer', 'value' => 2 }])
lauf(i)
check('leere Warenliste blockiert nicht') { GameState.variable('leer') == 2.0 && !Shop.active? }

puts '== MapCheck =='
OPTS = { prefabs: [], materials: [], scripts: [] }.freeze

def karte_mit(befehle)
  {
    'name' => 'Testkarte',
    'terrain' => { 'width' => 60.0, 'height' => 60.0, 'resolution' => 64,
                   'heights' => [], 'splats' => [] },
    'objects' => [], 'npcs' => [],
    'fog' => { 'on' => false, 'far' => 100.0, 'color' => 0xE8E8E8 },
    'weather' => { 'type' => 'kein', 'intensity' => 1.0 },
    'events' => [{ 'id' => 1, 'name' => 'Test', 'x' => 0.0, 'z' => 0.0, 'radius' => 2.5,
                   'pages' => [{ 'conditions' => {}, 'trigger' => 'action',
                                 'commands' => Array(befehle) }] }]
  }
end

def hat?(funde, text, schwere)
  funde.any? { |f| f.severity == schwere && f.message.include?(text) }
end

check('gültiger shop: keine Funde') do
  MapCheck.scan(karte_mit([{ 'cmd' => 'shop',
                             'goods' => [{ 'item' => 'heiltrank', 'price' => 20 },
                                         { 'item' => 'apfel' }] }]), **OPTS).empty?
end
check('goods fehlt -> Fehler') do
  hat?(MapCheck.scan(karte_mit([{ 'cmd' => 'shop' }]), **OPTS),
       "'goods' fehlt oder ist leer", MapCheck::FEHLER)
end
check('ware ohne item -> Fehler') do
  hat?(MapCheck.scan(karte_mit([{ 'cmd' => 'shop', 'goods' => [{ 'price' => 5 }] }]), **OPTS),
       "'item' fehlt oder ist leer", MapCheck::FEHLER)
end
check('unbekanntes Item -> Warnung') do
  hat?(MapCheck.scan(karte_mit([{ 'cmd' => 'shop', 'goods' => [{ 'item' => 'gibtsnicht' }] }]), **OPTS),
       "Item 'gibtsnicht' nicht in database.json", MapCheck::WARNUNG)
end
check('negativer Preis -> Fehler') do
  hat?(MapCheck.scan(karte_mit([{ 'cmd' => 'shop',
                                  'goods' => [{ 'item' => 'heiltrank', 'price' => -1 }] }]), **OPTS),
       "'price' darf nicht negativ sein", MapCheck::FEHLER)
end
check('Preis keine Zahl -> Fehler') do
  hat?(MapCheck.scan(karte_mit([{ 'cmd' => 'shop',
                                  'goods' => [{ 'item' => 'heiltrank', 'price' => 'teuer' }] }]), **OPTS),
       "'price' ist keine Zahl", MapCheck::FEHLER)
end
check('doppelte Ware -> Warnung') do
  hat?(MapCheck.scan(karte_mit([{ 'cmd' => 'shop',
                                  'goods' => [{ 'item' => 'apfel' }, { 'item' => 'Apfel' }] }]), **OPTS),
       'Ware doppelt im Angebot', MapCheck::WARNUNG)
end

puts '== Fenster (UI.shop) =='
neu!
Shop.start([{ 'item' => 'heiltrank', 'price' => 20 }])
check('UI.shop zeichnet Reiter, Ware und Gold') do
  Rune::Gfx.reset!
  Rune::Input.setze!([])
  UI.shop(Shop, input: false) == false &&
    %w[Kaufen Verkaufen Heiltrank].all? { |t| Rune::Gfx.gesammelt.include?(t) } &&
    Rune::Gfx.gesammelt.any? { |t| t.include?('Gold: 100') }
end
check('UI.shop kauft mit E') do
  Rune::Input.setze!([:e])
  UI.shop(Shop) == false && Party.item_count('heiltrank') == 1 && Party.gold == 80
end
check('UI.shop wechselt mit rechts den Reiter') do
  Rune::Input.setze!([:right])
  UI.shop(Shop) == false && Shop.modus == :verkaufen
end
check('UI.shop schließt im Verkaufs-Reiter erst eine Ebene') do
  Rune::Input.setze!([:escape])
  UI.shop(Shop) == false && Shop.modus == :kaufen && Shop.active?
  UI.shop(Shop) == true && !Shop.active?
end
Rune::Input.setze!([])

check('alle 65 Befehlstypen im Register der Prüfung') { MapCheck::CMD_TYPES.size == 65 }

puts($fail.zero? ? 'ALLE CHECKS GRUEN' : "#{$fail} CHECK(S) FEHLGESCHLAGEN")
exit($fail.zero? ? 0 : 1)
