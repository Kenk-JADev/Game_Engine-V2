#encoding: utf-8
# ============================================================================
#  Kopfloser Test für MapCheck (Etappe 19a):
#     ruby -I game/engine tools/mapcheck_test.rb
#  Exit-Code 0 = alles grün.
#
#  MapCheck ist die Ruby-Variante des Editor-Karten-Scans (issues_scan_map in
#  src/editor/editor_main.cpp). Beide müssen dieselben Regeln haben, deshalb
#  prüft dieser Test auch die mitgelieferten Karten (game/data, projekte/).
# ============================================================================

ROOT = File.expand_path('..', __dir__)
$LOAD_PATH.unshift(File.join(ROOT, 'game', 'engine'))
require 'json'
require 'fileutils'
require 'tmpdir'
require 'mapcheck'
require 'prefabs'   # echte Prefab-Ids für die Daten-Prüfung

$fail = 0
def check(name)
  ok = yield
  puts(ok ? "  ok   #{name}" : "  FAIL #{name}")
  $fail += 1 unless ok
rescue StandardError => e
  puts "  FAIL #{name} (Ausnahme: #{e.class}: #{e.message})"
  $fail += 1
end

# Funde bequem durchsuchen: Enthält der Fund (Ort + Meldung, also Finding#to_s)
# das Fragment? Gegen to_s und nicht gegen message prüfen, weil die Stelle
# ("karte terrain.heights") im where-Feld steht, der Grund im message-Feld.
def hat?(funde, fragment, severity = nil)
  funde.any? do |f|
    f.to_s.include?(fragment) && (severity.nil? || f.severity == severity)
  end
end

def fehler(funde) = funde.select(&:fehler?)
def warnungen(funde) = funde.select(&:warnung?)

# Eine Karte, die sauber sein muss.
def saubere_karte
  {
    'name' => 'Testkarte',
    'terrain' => { 'width' => 120.0, 'height' => 90.0, 'resolution' => 64,
                   'amplitude' => 4.0, 'heights' => [{ 'x' => 1.0, 'z' => 2.0,
                                                       'r' => 3.0, 'h' => 2.0 }],
                   'splats' => [{ 'tex' => 'weg', 'x' => 0.0, 'z' => 0.0,
                                  'w' => 10.0, 'h' => 4.0 }] },
    'objects' => [{ 'type' => 'haus', 'x' => 1.0, 'z' => 2.0, 'rot' => 0.0 }],
    'events' => [{ 'id' => 1, 'name' => 'Wache', 'x' => 0.0, 'z' => 3.0,
                   'radius' => 2.5,
                   'pages' => [{ 'conditions' => { 'switch' => { 'name' => 'auftrag',
                                                                 'value' => true } },
                                 'trigger' => 'action',
                                 'commands' => [{ 'cmd' => 'text',
                                                  'lines' => ['Hallo'] }] }] }],
    'npcs' => [{ 'name' => 'Wanderer', 'x' => 1.0, 'z' => 1.0, 'speed' => 1.2,
                 'path' => [[2.0, 2.0], [4.0, 1.0]],
                 'commands' => [{ 'cmd' => 'text', 'lines' => ['Hi'] }] }],
    'fog' => { 'on' => false, 'far' => 100.0, 'color' => 0xE8E8E8 },
    'weather' => { 'type' => 'kein', 'intensity' => 1.0 }
  }
end

OPTS = { prefabs: %w[haus baum], materials: %w[wand weg], scripts: %w[intro] }.freeze

puts '== MapCheck: saubere Karte =='
check('saubere Karte hat keine Funde') { MapCheck.scan(saubere_karte, **OPTS).empty? }

puts '== MapCheck: Grundlagen =='
check('Wurzel kein Objekt -> Fehler') do
  hat?(MapCheck.scan('text'), 'Wurzel ist kein Objekt', MapCheck::FEHLER)
end
check("fehlender 'name' -> Warnung") do
  k = saubere_karte
  k.delete('name')
  hat?(MapCheck.scan(k, **OPTS), "'name' fehlt", MapCheck::WARNUNG)
end
check('Startpunkt ohne x -> Fehler') do
  k = saubere_karte
  k['start'] = { 'z' => 1.0 }
  hat?(MapCheck.scan(k, **OPTS), "'x' fehlt", MapCheck::FEHLER)
end

puts '== MapCheck: Objekte =='
check('Objekt ohne type -> Fehler') do
  k = saubere_karte
  k['objects'] = [{ 'x' => 0.0, 'z' => 0.0 }]
  hat?(MapCheck.scan(k, **OPTS), "ohne 'type'", MapCheck::FEHLER)
end
check('unbekannter Prefab-Typ -> Warnung') do
  k = saubere_karte
  k['objects'] = [{ 'type' => 'wolke', 'x' => 0.0, 'z' => 0.0 }]
  hat?(MapCheck.scan(k, **OPTS), "unbekannter Prefab-Typ 'wolke'", MapCheck::WARNUNG)
end
check('Objekt-x als Text -> Fehler') do
  k = saubere_karte
  k['objects'] = [{ 'type' => 'haus', 'x' => 'links', 'z' => 0.0 }]
  hat?(MapCheck.scan(k, **OPTS), "'x' fehlt oder ist keine Zahl", MapCheck::FEHLER)
end
check('unbekanntes Material am Objekt -> Warnung') do
  k = saubere_karte
  k['objects'] = [{ 'type' => 'haus', 'x' => 0.0, 'z' => 0.0, 'tex' => 'glas' }]
  hat?(MapCheck.scan(k, **OPTS), "Material 'glas' ist nicht geladen", MapCheck::WARNUNG)
end
check('ohne Prefab-Liste kein Typ-Fund') do
  k = saubere_karte
  k['objects'] = [{ 'type' => 'wolke', 'x' => 0.0, 'z' => 0.0 }]
  MapCheck.scan(k, prefabs: nil).none? { |f| f.message.include?('Prefab-Typ') }
end

puts '== MapCheck: Terrain =='
check('Breite 0 -> Warnung') do
  k = saubere_karte
  k['terrain']['width'] = 0.0
  hat?(MapCheck.scan(k, **OPTS), "'width'/'size' fehlt oder <= 0", MapCheck::WARNUNG)
end
check('Höhen-Bürste ohne r -> Fehler') do
  k = saubere_karte
  k['terrain']['heights'] = [{ 'x' => 1.0, 'z' => 1.0, 'h' => 2.0 }]
  hat?(MapCheck.scan(k, **OPTS), "'r' fehlt oder ist keine Zahl", MapCheck::FEHLER)
end
check('Höhen-Bürste mit Radius 0 -> Warnung') do
  k = saubere_karte
  k['terrain']['heights'] = [{ 'x' => 1.0, 'z' => 1.0, 'r' => 0.0, 'h' => 2.0 }]
  hat?(MapCheck.scan(k, **OPTS), "'r' (Radius) muss > 0 sein", MapCheck::WARNUNG)
end
check('Splat ohne Material -> Fehler') do
  k = saubere_karte
  k['terrain']['splats'] = [{ 'x' => 0.0, 'z' => 0.0, 'w' => 4.0, 'h' => 4.0 }]
  hat?(MapCheck.scan(k, **OPTS), "ohne Material ('tex')", MapCheck::FEHLER)
end
check('Splat mit Breite 0 -> Warnung') do
  k = saubere_karte
  k['terrain']['splats'] = [{ 'tex' => 'weg', 'x' => 0.0, 'z' => 0.0, 'w' => 0.0, 'h' => 2.0 }]
  hat?(MapCheck.scan(k, **OPTS), "'w' muss > 0 sein", MapCheck::WARNUNG)
end
check('heights als Hash statt Liste -> Fehler') do
  k = saubere_karte
  k['terrain']['heights'] = { 'x' => 1.0 }
  hat?(MapCheck.scan(k, **OPTS), 'terrain.heights', MapCheck::FEHLER)
end

puts '== MapCheck: Events und Seiten =='
check('Event ohne Position -> Fehler') do
  k = saubere_karte
  k['events'] = [{ 'id' => 1, 'name' => 'A' }]
  hat?(MapCheck.scan(k, **OPTS), "'z' fehlt oder ist keine Zahl", MapCheck::FEHLER)
end
check('doppelte Event-Id -> Warnung') do
  k = saubere_karte
  k['events'] = [{ 'id' => 7, 'name' => 'A', 'x' => 0.0, 'z' => 0.0 },
                 { 'id' => 7, 'name' => 'B', 'x' => 1.0, 'z' => 1.0 }]
  hat?(MapCheck.scan(k, **OPTS), 'Event-Id 7 kommt 2x vor', MapCheck::WARNUNG)
end
check('unbekannter Trigger -> Warnung') do
  k = saubere_karte
  k['events'][0]['pages'][0]['trigger'] = 'klick'
  hat?(MapCheck.scan(k, **OPTS), "unbekannter Trigger 'klick'", MapCheck::WARNUNG)
end
check('Seite ohne commands -> Fehler') do
  k = saubere_karte
  k['events'][0]['pages'][0].delete('commands')
  hat?(MapCheck.scan(k, **OPTS), "'commands' fehlt/keine Liste", MapCheck::FEHLER)
end
check('leere Seitenliste -> Warnung') do
  k = saubere_karte
  k['events'][0]['pages'] = []
  hat?(MapCheck.scan(k, **OPTS), "'pages' ist leer", MapCheck::WARNUNG)
end
check('unbekannte Bedingung -> Warnung') do
  k = saubere_karte
  k['events'][0]['pages'][0]['conditions'] = { 'item' => { 'name' => 'x' } }
  hat?(MapCheck.scan(k, **OPTS), "unbekannte Bedingung 'item'", MapCheck::WARNUNG)
end
check('Variablen-Bedingung ohne op -> Warnung') do
  k = saubere_karte
  k['events'][0]['pages'][0]['conditions'] = { 'variable' => { 'name' => 'n', 'value' => 1 } }
  hat?(MapCheck.scan(k, **OPTS), "unbekannter Vergleich ''", MapCheck::WARNUNG)
end
check('Bedingung ohne name -> Fehler') do
  k = saubere_karte
  k['events'][0]['pages'][0]['conditions'] = { 'switch' => { 'value' => true } }
  hat?(MapCheck.scan(k, **OPTS), "'name' fehlt oder ist leer", MapCheck::FEHLER)
end
check('altes Format ohne pages wird erkannt') do
  k = saubere_karte
  k['events'] = [{ 'id' => 1, 'name' => 'Alt', 'x' => 0.0, 'z' => 0.0, 'trigger' => 'touch',
                   'commands' => [{ 'cmd' => 'quatsch' }] }]
  hat?(MapCheck.scan(k, **OPTS), "unbekannter Befehlstyp 'quatsch'", MapCheck::FEHLER)
end
check('Event ganz ohne Befehle -> Warnung') do
  k = saubere_karte
  k['events'] = [{ 'id' => 1, 'name' => 'Leer', 'x' => 0.0, 'z' => 0.0 }]
  hat?(MapCheck.scan(k, **OPTS), "weder 'pages' noch 'commands'", MapCheck::WARNUNG)
end

puts '== MapCheck: Befehle =='
def karte_mit(befehl)
  k = saubere_karte
  k['events'][0]['pages'][0]['commands'] = [befehl]
  k
end

check("Befehl ohne 'cmd' -> Fehler") do
  hat?(MapCheck.scan(karte_mit({ 'text' => 'x' }), **OPTS), "ohne 'cmd'", MapCheck::FEHLER)
end
check('Befehl ist keine Objektkarte -> Fehler') do
  k = saubere_karte
  k['events'][0]['pages'][0]['commands'] = ['text']
  hat?(MapCheck.scan(k, **OPTS), 'Befehl ist kein Objekt', MapCheck::FEHLER)
end
check('text ohne lines -> Warnung') do
  hat?(MapCheck.scan(karte_mit({ 'cmd' => 'text' }), **OPTS), "'lines' fehlt/leer", MapCheck::WARNUNG)
end
check('choices ohne options -> Fehler') do
  hat?(MapCheck.scan(karte_mit({ 'cmd' => 'choices', 'branches' => [] }), **OPTS),
       "'options' fehlt/leer", MapCheck::FEHLER)
end
check('choices mit zu wenigen Zweigen -> Warnung') do
  f = MapCheck.scan(karte_mit({ 'cmd' => 'choices', 'options' => %w[a b],
                                'branches' => [[]] }), **OPTS)
  hat?(f, 'weniger Zweige als Optionen', MapCheck::WARNUNG)
end
check('if_switch ohne else -> Warnung') do
  f = MapCheck.scan(karte_mit({ 'cmd' => 'if_switch', 'name' => 's', 'value' => true,
                                'then' => [] }), **OPTS)
  hat?(f, "'else' fehlt/keine Liste", MapCheck::WARNUNG)
end
check('if_variable mit falschem op -> Warnung') do
  f = MapCheck.scan(karte_mit({ 'cmd' => 'if_variable', 'name' => 'v', 'op' => '~=',
                                'then' => [], 'else' => [] }), **OPTS)
  hat?(f, "unbekannter Vergleich '~='", MapCheck::WARNUNG)
end
check('Fehler in einer Dann-Liste wird gefunden (Rekursion)') do
  f = MapCheck.scan(karte_mit({ 'cmd' => 'if_switch', 'name' => 's', 'value' => true,
                                'then' => [{ 'cmd' => 'gibtsnicht' }], 'else' => [] }), **OPTS)
  hat?(f, "unbekannter Befehlstyp 'gibtsnicht'", MapCheck::FEHLER) &&
    f.any? { |x| x.where.include?('(Dann)') }
end
check('wait mit negativen Sekunden -> Warnung') do
  hat?(MapCheck.scan(karte_mit({ 'cmd' => 'wait', 'seconds' => -1.0 }), **OPTS),
       "'seconds' muss eine Zahl >= 0 sein", MapCheck::WARNUNG)
end
check('variable ohne name -> Fehler') do
  hat?(MapCheck.scan(karte_mit({ 'cmd' => 'variable', 'value' => 1 }), **OPTS),
       "'name' fehlt oder ist leer", MapCheck::FEHLER)
end
check('variable ohne Wirkung -> Warnung') do
  hat?(MapCheck.scan(karte_mit({ 'cmd' => 'variable', 'name' => 'v' }), **OPTS),
       "weder 'value' noch 'add'", MapCheck::WARNUNG)
end
check('script ohne name und code -> Fehler') do
  hat?(MapCheck.scan(karte_mit({ 'cmd' => 'script' }), **OPTS),
       "weder 'name' noch 'code'", MapCheck::FEHLER)
end
check('unbekanntes Script -> Warnung') do
  hat?(MapCheck.scan(karte_mit({ 'cmd' => 'script', 'name' => 'unbekannt' }), **OPTS),
       "Script 'unbekannt' ist nicht definiert", MapCheck::WARNUNG)
end
check('bekanntes Script -> keine Warnung') do
  MapCheck.scan(karte_mit({ 'cmd' => 'script', 'name' => 'intro' }), **OPTS).none? do |f|
    f.message.include?('ist nicht definiert')
  end
end
check('unbekanntes Wetter -> Warnung') do
  hat?(MapCheck.scan(karte_mit({ 'cmd' => 'weather', 'name' => 'hagel' }), **OPTS),
       "unbekanntes Wetter 'hagel'", MapCheck::WARNUNG)
end
check('unbekannter Effekt -> Warnung') do
  hat?(MapCheck.scan(karte_mit({ 'cmd' => 'effect', 'name' => 'feuer' }), **OPTS),
       "unbekannter Effekt 'feuer'", MapCheck::WARNUNG)
end
check('sound ohne name -> Fehler') do
  hat?(MapCheck.scan(karte_mit({ 'cmd' => 'sound' }), **OPTS),
       "'name' fehlt oder ist leer", MapCheck::FEHLER)
end
check('item ohne name -> Fehler') do
  hat?(MapCheck.scan(karte_mit({ 'cmd' => 'item', 'count' => 2 }), **OPTS),
       "'name' fehlt oder ist leer", MapCheck::FEHLER)
end
check('gold mit amount 0 -> Warnung') do
  hat?(MapCheck.scan(karte_mit({ 'cmd' => 'gold', 'amount' => 0 }), **OPTS),
       "'amount' fehlt oder ist 0", MapCheck::WARNUNG)
end
check('save_game mit Slot 9 -> Warnung') do
  hat?(MapCheck.scan(karte_mit({ 'cmd' => 'save_game', 'slot' => 9 }), **OPTS),
       "'slot' muss 1..3 sein", MapCheck::WARNUNG)
end
check('teleport ohne z -> Fehler') do
  hat?(MapCheck.scan(karte_mit({ 'cmd' => 'teleport', 'x' => 1.0 }), **OPTS),
       "'z' fehlt oder ist keine Zahl", MapCheck::FEHLER)
end
check('teleport auf unbekannte Karte -> Fehler') do
  f = MapCheck.scan(karte_mit({ 'cmd' => 'teleport', 'x' => 1.0, 'z' => 2.0,
                                'map' => 'atlantis' }), **OPTS.merge(maps: %w[dorf]))
  hat?(f, "Zielkarte 'atlantis' existiert nicht", MapCheck::FEHLER)
end
check('log ohne text -> Warnung') do
  hat?(MapCheck.scan(karte_mit({ 'cmd' => 'log' }), **OPTS),
       "'text' fehlt oder ist leer", MapCheck::WARNUNG)
end
check('alle 23 Befehlstypen sind bekannt') do
  MapCheck::CMD_TYPES.size == 23 &&
    MapCheck.scan(karte_mit({ 'cmd' => 'heal' }), **OPTS).empty? &&
    MapCheck.scan(karte_mit({ 'cmd' => 'move_player', 'x' => 1.0, 'z' => 1.0 }), **OPTS).empty? &&
    MapCheck.scan(karte_mit({ 'cmd' => 'toggle_switch', 'name' => 's' }), **OPTS).empty? &&
    MapCheck.scan(karte_mit({ 'cmd' => 'load_game', 'slot' => 2 }), **OPTS).empty? &&
    MapCheck.scan(karte_mit({ 'cmd' => 'effect', 'name' => 'burst' }), **OPTS).empty?
end

puts '== MapCheck: NPCs =='
check('NPC ohne name -> Warnung') do
  k = saubere_karte
  k['npcs'] = [{ 'x' => 0.0, 'z' => 0.0 }]
  hat?(MapCheck.scan(k, **OPTS), "'name' fehlt oder ist leer", MapCheck::WARNUNG)
end
check('NPC mit speed 0 -> Warnung') do
  k = saubere_karte
  k['npcs'][0]['speed'] = 0.0
  hat?(MapCheck.scan(k, **OPTS), "'speed' muss > 0 sein", MapCheck::WARNUNG)
end
check('Wegpunkt als Dreierliste -> Fehler') do
  k = saubere_karte
  k['npcs'][0]['path'] = [[1.0, 2.0, 3.0]]
  hat?(MapCheck.scan(k, **OPTS), 'muss ein Paar [x, z] aus Zahlen sein', MapCheck::FEHLER)
end
check('nur ein Wegpunkt -> Warnung') do
  k = saubere_karte
  k['npcs'][0]['path'] = [[1.0, 2.0]]
  hat?(MapCheck.scan(k, **OPTS), 'nur ein Wegpunkt', MapCheck::WARNUNG)
end
check('NPC-Befehle werden geprüft') do
  k = saubere_karte
  k['npcs'][0]['commands'] = [{ 'cmd' => 'unbekannt' }]
  hat?(MapCheck.scan(k, **OPTS), "unbekannter Befehlstyp 'unbekannt'", MapCheck::FEHLER)
end

puts '== MapCheck: Atmosphäre =='
check('Nebel-Farbe zu gross -> Warnung') do
  k = saubere_karte
  k['fog']['color'] = 0x1FFFFFF
  hat?(MapCheck.scan(k, **OPTS), "'color' muss 0x000000..0xFFFFFF sein", MapCheck::WARNUNG)
end
check('Wetter-Typ der Karte unbekannt -> Warnung') do
  k = saubere_karte
  k['weather']['type'] = 'sturm'
  hat?(MapCheck.scan(k, **OPTS), "unbekannter Typ 'sturm'", MapCheck::WARNUNG)
end
check('panorama als Liste -> Fehler') do
  k = saubere_karte
  k['panorama'] = [1, 2]
  hat?(MapCheck.scan(k, **OPTS), 'panorama', MapCheck::FEHLER)
end

puts '== MapCheck: Dateien und Verzeichnisse =='
tmp = Dir.mktmpdir('rune_mapcheck')
begin
  gut   = File.join(tmp, 'maps', 'gut.json')
  kaputt = File.join(tmp, 'maps', 'kaputt.json')
  FileUtils.mkdir_p(File.join(tmp, 'maps'))
  File.write(gut, JSON.generate(saubere_karte), encoding: 'utf-8')
  File.write(kaputt, '{ "name": ', encoding: 'utf-8')

  check('scan_file: saubere Datei ohne Funde') { MapCheck.scan_file(gut, **OPTS).empty? }
  check('scan_file: Parse-Fehler wird EIN Fehler') do
    f = MapCheck.scan_file(kaputt, **OPTS)
    f.size == 1 && f.first.fehler? && f.first.message.include?('JSON-Parse-Fehler')
  end
  check('scan_file: fehlende Datei -> Fehler') do
    f = MapCheck.scan_file(File.join(tmp, 'maps', 'gibtsnicht.json'), **OPTS)
    f.size == 1 && f.first.fehler? && f.first.message.include?('nicht lesbar')
  end
  check('scan_tree findet beide Karten') do
    MapCheck.scan_tree(tmp, **OPTS).keys.sort.map { |p| File.basename(p) } == %w[gut.json kaputt.json]
  end
  check('scan_tree: nur_funde lässt saubere Karte weg') do
    MapCheck.scan_tree(tmp, nur_funde: true, **OPTS).keys.size == 1
  end
  check('map_ids ignoriert Datenbank/Scripte/Spielstände') do
    File.write(File.join(tmp, 'database.json'), '{}', encoding: 'utf-8')
    FileUtils.mkdir_p(File.join(tmp, 'saves'))
    File.write(File.join(tmp, 'saves', 'slot1.json'), '{}', encoding: 'utf-8')
    MapCheck.map_ids(tmp).sort == %w[gut kaputt]
  end
ensure
  FileUtils.remove_entry(tmp) if Dir.exist?(tmp)
end

puts '== MapCheck: Ausgabe und Backlog =='
check('summary zählt Fehler und Warnungen') do
  f = [MapCheck::Finding.new(MapCheck::FEHLER, 'a', 'x'),
       MapCheck::Finding.new(MapCheck::WARNUNG, 'b', 'y')]
  MapCheck.summary(f) == '1 Fehler, 1 Warnung'
end
check('Finding#to_s nennt Schweregrad und Stelle') do
  s = MapCheck::Finding.new(MapCheck::FEHLER, 'dorf.json Objekt #2', 'kaputt').to_s
  s.start_with?('[Fehler]') && s.include?('dorf.json Objekt #2: kaputt')
end
check('report! ohne Issues ist still (0)') { MapCheck.report!([]) == 0 }

# Issues-Double. Erst JETZT definieren: der Check davor braucht die Situation
# "kein Issues geladen" (MapCheck soll ohne Engine-Module funktionieren).
module Issues
  class << self
    attr_accessor :gesammelt

    def report(area, msg, severity: 'warn')
      (@gesammelt ||= []) << [area, msg, severity]
    end
  end
end

check('report! schreibt in ein Issues-Double') do
  Issues.gesammelt = []
  funde = MapCheck.scan(karte_mit({ 'cmd' => 'unbekannt' }), **OPTS)
  MapCheck.report!(funde) == funde.size && Issues.gesammelt.size == funde.size &&
    Issues.gesammelt.first[2] == 'error'
end

puts '== MapCheck: mitgelieferte Karten (game/data + projekte) =='
alle = MapCheck.scan_tree(File.join(ROOT, 'game', 'data', 'maps'),
                          File.join(ROOT, 'projekte'),
                          prefabs: Prefabs.ids)
check('mindestens 3 Karten gefunden') { alle.size >= 3 }
check('keine Karte hat Parse-Fehler') do
  alle.none? { |_pfad, f| f.any? { |x| x.message.include?('JSON-Parse-Fehler') } }
end
check('mitgelieferte Karten sind fehlerfrei (Schweregrad 2)') do
  bad = alle.reject { |_pfad, f| fehler(f).empty? }
  bad.each { |pfad, f| puts "    #{File.basename(pfad)}: #{fehler(f).map(&:to_s).join(' | ')}" }
  bad.empty?
end
check('Warnungen der mitgelieferten Karten bleiben unter 10') do
  anzahl = alle.values.flat_map { |f| warnungen(f) }.size
  puts "    #{anzahl} Warnung(en) in den mitgelieferten Karten"
  anzahl < 10
end
check('alle Objekttypen der Dorf-Karte sind bekannte Prefabs') do
  dorf = JSON.parse(File.read(File.join(ROOT, 'projekte', 'dorf', 'data', 'maps', 'dorf.json'),
                              encoding: 'utf-8'))
  dorf['objects'].all? { |o| Prefabs.by_id(o['type']) }
end

puts($fail.zero? ? 'MAPCHECK-TEST OK (0 Fehler)' : "MAPCHECK-TEST FEHLGESCHLAGEN (#{$fail} Fehler)")
exit($fail.zero? ? 0 : 1)
