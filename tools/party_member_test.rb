#encoding: utf-8
# ============================================================================
#  Kopfloser Test für "Mitglieder ändern" (Etappe 20d, XP 129).
#  Lässt sich ohne Engine starten:
#     ruby -I game/engine tools/party_member_test.rb
#  Exit-Code 0 = alles grün.
#
#  Geprüft wird:
#    - Party.setup legt @katalog an und nimmt zunächst alle Helden ins Team
#    - Party.member_add/member_remove: ok/mitglied/unbekannt/kein_mitglied/
#      letzter, Reihenfolge und Sichtbarkeit über find/actors
#    - Spielstand: snapshot/restore! führt die Mitgliederliste mit; alte Saves
#      ohne 'actors' lassen das Team stehen, Werte kommen weiter mit
#    - Interpreter: Befehl party_member (add/remove, unbekannter Modus, ohne
#      name, unbekannter Held - der Rest der Liste läuft weiter)
#    - MapCheck: Modus-Vokabular, Pflichtfeld name, unbekannter Actor
#    - sync: C++-Moderation und MapCheck::PARTY_MODI nennen dieselben Wörter
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

$LOAD_PATH.unshift(File.join(Rune::ROOT, 'game', 'engine'))
require 'data'
require 'state'
require 'issues'
require 'party'
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
  'gold' => 10,
  'actors' => [
    { 'id' => 'arik', 'name' => 'Arik', 'level' => 2, 'hp' => 100, 'mp' => 20 },
    { 'id' => 'mira', 'name' => 'Mira', 'level' => 1, 'hp' => 80, 'mp' => 40 },
    { 'id' => 'noa',  'name' => 'Noa',  'level' => 1, 'hp' => 60, 'mp' => 10 }
  ],
  'items' => []
}.freeze

def neu! = Party.setup(DB)

def lauf(interp, frames = 3, dt = 0.016)
  frames.times { interp.update(dt) }
  interp
end

def ids = Party.actors.map(&:id)

puts '== Mitgliederliste =='
neu!
check('nach setup sind alle Datenbank-Helden im Team') do
  ids == %w[arik mira noa] && Party.member?('arik') && Party.find('noa').name == 'Noa'
end
check('member_add nimmt einen entfernten Helden wieder auf') do
  Party.member_remove('mira') == :ok && Party.member_add('mira') == :ok && Party.member?('mira')
end
check('doppeltes add meldet :mitglied und doppelt nicht') do
  Party.member_add('arik') == :mitglied && ids.count('arik') == 1
end
check('unbekannter Held -> :unbekannt, Team unverändert') do
  vorher = ids
  Party.member_add('gibtsnicht') == :unbekannt && ids == vorher
end
check('remove nimmt nur aus dem Team, nicht aus der Datenbank') do
  Party.member_remove('noa') == :ok && !Party.member?('noa') && Party.find('noa').nil?
end
check('remove ohne Mitglied -> :kein_mitglied') { Party.member_remove('noa') == :kein_mitglied }
check('Reihenfolge: neues Mitglied landet hinten') do
  Party.member_add('noa') == :ok && ids.last == 'noa'
end
check('das letzte Mitglied bleibt im Team') do
  neu!
  Party.member_remove('mira') == :ok && Party.member_remove('noa') == :ok
  Party.member_remove('arik') == :letzter && ids == %w[arik]
end
check('actor_im_team? folgt der Mitgliederliste (XP 111)') do
  neu!
  Party.member_remove('mira')
  i = Interpreter.new
  i.start([{ 'cmd' => 'if_actor', 'name' => 'mira',
             'then' => [{ 'cmd' => 'variable', 'name' => 'x', 'value' => 1 }], 'else' => [] }])
  lauf(i)
  GameState.variable('x') != 1.0
end

puts '== Spielstand =='
neu!
Party.member_remove('noa')
Party.find('arik').hp = 42
snap = Party.snapshot
check('snapshot führt nur die Mitglieder') { snap['actors'].map { |h| h['id'] } == %w[arik mira] }
neu!
check('setup danach: wieder alle im Team') { ids == %w[arik mira noa] }
Party.restore!(snap)
check('restore! stellt die Mitgliederliste wieder her') { ids == %w[arik mira] }
check('restore! übernimmt die Werte der Mitglieder') { Party.find('arik').hp == 42 }
check('restore! lässt alte Saves (ohne actors) unangetastet') do
  neu!
  Party.restore!({ 'gold' => 5, 'items' => {} })
  ids == %w[arik mira noa] && Party.find('arik').hp == Party.find('arik').max_hp
end

puts '== Interpreter: party_member =='
neu!
i = Interpreter.new
i.start([{ 'cmd' => 'party_member', 'name' => 'mira', 'mode' => 'remove' },
         { 'cmd' => 'variable', 'name' => 'weiter', 'value' => 1 }])
lauf(i)
check('remove entfernt den Helden und die Liste läuft weiter') do
  !Party.member?('mira') && GameState.variable('weiter') == 1.0
end
i = Interpreter.new
i.start([{ 'cmd' => 'party_member', 'name' => 'mira', 'mode' => 'add' }])
lauf(i)
check('add nimmt den Helden wieder auf') { Party.member?('mira') }
i = Interpreter.new
i.start([{ 'cmd' => 'party_member', 'name' => 'noa' }])
lauf(i)
check('ohne mode gilt add (Default)') { Party.member?('noa') }
i = Interpreter.new
i.start([{ 'cmd' => 'party_member', 'name' => 'arik', 'mode' => 'tauschen' },
         { 'cmd' => 'variable', 'name' => 'danach', 'value' => 2 }])
lauf(i)
check('unbekannter Modus wird gemeldet, der Rest läuft') do
  ids.include?('arik') && GameState.variable('danach') == 2.0
end
i = Interpreter.new
i.start([{ 'cmd' => 'party_member', 'mode' => 'add' },
         { 'cmd' => 'variable', 'name' => 'ohne_name', 'value' => 3 }])
lauf(i)
check('ohne name bleibt das Team stehen') do
  GameState.variable('ohne_name') == 3.0 && ids.sort == %w[arik mira noa]
end
i = Interpreter.new
i.start([{ 'cmd' => 'party_member', 'name' => 'gibtsnicht', 'mode' => 'add' }])
lauf(i)
check('unbekannter Held ändert das Team nicht') { ids.sort == %w[arik mira noa] }

puts '== MapCheck =='
OPTS = { prefabs: [], materials: [], scripts: [] }.freeze

def karte_mit(befehle)
  {
    'name' => 'Testkarte',
    'terrain' => { 'width' => 60.0, 'height' => 60.0, 'resolution' => 64,
                   'heights' => [], 'splats' => [] },
    'objects' => [],
    'npcs' => [],
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

check('gültiger party_member: keine Funde') do
  MapCheck.scan(karte_mit([{ 'cmd' => 'party_member', 'name' => 'arik', 'mode' => 'remove' }]),
                **OPTS).empty?
end
check('unbekannter Modus -> Fehler') do
  hat?(MapCheck.scan(karte_mit([{ 'cmd' => 'party_member', 'name' => 'arik', 'mode' => 'tausch' }]), **OPTS),
       "unbekannter party_member-Modus 'tausch'", MapCheck::FEHLER)
end
check('fehlender name -> Fehler') do
  hat?(MapCheck.scan(karte_mit([{ 'cmd' => 'party_member', 'mode' => 'add' }]), **OPTS),
       "'name' fehlt oder ist leer", MapCheck::FEHLER)
end
check('Modus wird kleingeschrieben akzeptiert') do
  MapCheck.scan(karte_mit([{ 'cmd' => 'party_member', 'name' => 'arik', 'mode' => 'ADD' }]),
                **OPTS).empty?
end
check('unbekannter Actor -> Warnung') do
  hat?(MapCheck.scan(karte_mit([{ 'cmd' => 'party_member', 'name' => 'zeta' }]), **OPTS),
       "Actor 'zeta' nicht in database.json", MapCheck::WARNUNG)
end
check('Actor per Anzeigename wird erkannt') do
  MapCheck.scan(karte_mit([{ 'cmd' => 'party_member', 'name' => 'Arik' }]), **OPTS).empty?
end

puts '== Sync =='
check('C++-Moderation prüft add/remove wie MapCheck::PARTY_MODI') do
  form = File.read(File.join(Rune::ROOT, 'src', 'editor', 'event_form.h'), encoding: 'utf-8')
  form.include?('modus != "add" && modus != "remove"') && MapCheck::PARTY_MODI == %w[add remove]
end
check('EV_TYPES (C++) nennt party_member an der Registerstelle') do
  form = File.read(File.join(Rune::ROOT, 'src', 'editor', 'event_form.h'), encoding: 'utf-8')
  form.include?('"party_member", "shop", "name_input"') &&
    MapCheck::CMD_TYPES.each_cons(3).include?(%w[party_member shop name_input])
end
check('alle 55 Befehlstypen im Register der Prüfung') { MapCheck::CMD_TYPES.size == 55 }

puts($fail.zero? ? 'ALLE CHECKS GRUEN' : "#{$fail} CHECK(S) FEHLGESCHLAGEN")
exit($fail.zero? ? 0 : 1)
