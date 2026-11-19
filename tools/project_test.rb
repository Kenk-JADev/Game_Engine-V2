#encoding: utf-8
# ============================================================================
#  Kopfloser Test fuer das Projekt-System: Liste, Slug, Anlegen, neue Karten.
#     ruby -I game/engine tools/project_test.rb
#  Legt echte Ordner unter projekte/ an - Testprojekte werden wieder geloescht.
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

require 'json'
require 'data'
require 'project'

$fail = 0
def check(name)
  ok = yield
  puts(ok ? "  ok   #{name}" : "  FAIL #{name}")
  $fail += 1 unless ok
end

def near(a, b, eps = 1e-3) = (a - b).abs <= eps

# --- Demo-Projekt ---------------------------------------------------------
Project.ensure_demo
check('Demo-Projekt dorf existiert') { Project.valid?('dorf') }
check('dorf hat system.json') { File.exist?(File.join(Project.data_path('dorf'), 'system.json')) }
Project.ensure_demo
check('ensure_demo ist idempotent') { Project.list.size >= 1 }

# --- Slug -----------------------------------------------------------------
check('Slug: "Meine Welt 2" -> meine_welt_2') { Project.slug('Meine Welt 2') == 'meine_welt_2' }
check('Slug: Umlaute werden ersetzt') { Project.slug('Wäldchen') == 'waldchen' }
check('Slug: leer -> projekt') { Project.slug('   ') == 'projekt' }

# --- Neues Projekt --------------------------------------------------------
name = Project.create('Test Welt')
check('Projekt angelegt, Slug eindeutig') { name == 'test_welt' }
check('system.json vorhanden') { File.exist?(File.join(Project.data_path(name), 'system.json')) }
check('Karte welt01 vorhanden') { File.exist?(File.join(Project.data_path(name), 'maps', 'welt01.json')) }

sys = JSON.parse(File.read(File.join(Project.data_path(name), 'system.json'), encoding: 'UTF-8'))
check('Start zeigt auf welt01 bei 0/0') {
  sys['start']['map'] == 'welt01' && sys['start']['x'] == 0.0 && sys['start']['z'] == 0.0
}

map = JSON.parse(File.read(File.join(Project.data_path(name), 'maps', 'welt01.json'), encoding: 'UTF-8'))
check('Neue Welt ist leer (nur Gras)') { map['objects'].empty? && map['events'].empty? }
check('Neue Welt hat Terrain + Startpunkt') {
  # Etappe 17b: neue Karten rechteckig (width/height) und flach (amplitude 0)
  map['terrain']['width'] == 240 && map['terrain']['height'] == 240 &&
    map['terrain']['amplitude'] == 0.0 && map['start']['x'] == 0.0 &&
    map['fog'].is_a?(Hash) && map['panorama'].is_a?(Hash) && map.key?('note')
}

name2 = Project.create('Test Welt')
check('Zweites gleichnamiges Projekt bekommt _2') { name2 == 'test_welt_2' }

# --- Karten-IDs / neue Karte ---------------------------------------------
$project_root = Project.data_path(name)
Database.reset!
check('map_ids des neuen Projekts') { Database.map_ids == ['welt01'] }
id = Project.create_map
check('create_map liefert welt02') { id == 'welt02' }
Database.reset!
check('map_ids jetzt welt01+welt02') { Database.map_ids == %w[welt01 welt02] }
check('Karte welt02 ist leer') {
  m = Database.map('welt02')
  m['objects'].empty? && m['events'].empty? && m['start']['x'] == 0.0
}
check('naechste freie ID ist welt03') { Project.next_map_id == 'welt03' }

# --- Infos / zuletzt gespielt / loeschen ----------------------------------
Project.touch(name)
info = Project.info(name)
check('info liefert Titel') { info['title'] == 'Test Welt' }
check('info zaehlt 2 Karten') { info['maps'] == 2 }
check('touch vermerkt zuletzt gespielt') { info['last'].is_a?(String) && !info['last'].empty? }
check('title liefert Anzeigename') { Project.title(name) == 'Test Welt' }
Project.delete(name)
check('delete entfernt das Projekt') { !Project.valid?(name) }

# --- Aufraeumen -----------------------------------------------------------
FileUtils.rm_rf(Project.path('test_welt'))
FileUtils.rm_rf(Project.path('test_welt_2'))
check('Testprojekte wieder entfernt') { !Project.valid?('test_welt') && !Project.valid?('test_welt_2') }

puts($fail.zero? ? 'PROJECT-TEST OK (0 Fehler)' : "PROJECT-TEST FEHLGESCHLAGEN (#{$fail} Fehler)")
exit($fail.zero? ? 0 : 1)
