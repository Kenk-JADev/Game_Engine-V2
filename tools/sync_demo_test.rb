#encoding: utf-8
# ============================================================================
#  Sync-Demo-Test (Runde 13 / Y1+Y2): tools/sync_demo.rb und
#  tools/add_demo_script_events.rb abdecken. Kopflos, ohne Engine:
#     ruby tools/sync_demo_test.rb
#  Exit-Code 0 = alles grün. Läuft über tools/test_all.rb in der CI.
# ============================================================================

require 'json'
require 'fileutils'
require 'tmpdir'

ROOT = File.expand_path('..', __dir__)
load File.join(ROOT, 'tools', 'sync_demo.rb')

$fail = 0
def check(name)
  ok = yield
  puts(ok ? "  ok   #{name}" : "  FAIL #{name}")
  $fail += 1 unless ok
rescue StandardError => e
  puts "  FAIL #{name} (Ausnahme: #{e.class}: #{e.message})"
  $fail += 1
end

def schreibe(pfad, daten)
  FileUtils.mkdir_p(File.dirname(pfad))
  File.write(pfad, JSON.pretty_generate(daten) + "\n", encoding: 'UTF-8')
end

# --- Fixture: Vorlage und Demo mit typischen Drift-Ständen -----------------
Dir.mktmpdir('sync_demo') do |tmp|
  vorlage = File.join(tmp, 'game', 'data')
  demo    = File.join(tmp, 'projekte', 'dorf', 'data')

  schreibe(File.join(vorlage, 'system.json'),
           { 'name' => 'Vorlage', 'start' => { 'map' => 'welt01' }, 'neu_key' => 1 })
  schreibe(File.join(vorlage, 'database.json'),
           { 'gold' => 120, 'actors' => [{ 'id' => 'held' }],
             'enemies' => [{ 'id' => 'goblin' }], 'troops' => [{ 'id' => 'waldweg' }] })
  schreibe(File.join(vorlage, 'scripts.json'),
           { 'scripts' => [{ 'name' => 'gruss', 'code' => "say ['hi']" }] })
  schreibe(File.join(vorlage, 'maps', 'welt01.json'),
           { 'name' => 'Welt', 'encounters' => { 'troops' => ['waldweg'], 'steps' => 30 },
             'terrain' => { 'amplitude' => 4.0 }, 'events' => [] })
  schreibe(File.join(vorlage, 'prefabs.json'), { 'prefabs' => [] })

  schreibe(File.join(demo, 'system.json'),
           { 'name' => 'Demo', 'start' => { 'map' => 'welt01' }, 'last_played' => '2026-01-01 00:00' })
  schreibe(File.join(demo, 'database.json'),
           { 'gold' => 120, 'actors' => [{ 'id' => 'held' }] })
  schreibe(File.join(demo, 'scripts.json'), { 'scripts' => [] })
  schreibe(File.join(demo, 'maps', 'welt01.json'),
           { 'name' => 'Welt gespielt', 'terrain' => { 'amplitude' => 4.0, 'heights' => [1] },
             'events' => [], 'fog' => { 'enabled' => true } })
  FileUtils.mkdir_p(File.join(demo, 'saves'))
  File.write(File.join(demo, 'saves', 'slot1.json'), '{}', encoding: 'UTF-8')

  aenderungen = SyncDemo.sync(template: vorlage, demo: demo, verbose: false)

  check('Änderungen gemeldet (database, scripts, system, Karte)') do
    aenderungen.any? { |c| c.include?('database.json') } &&
      aenderungen.any? { |c| c.include?('scripts.json') } &&
      aenderungen.any? { |c| c.include?('system.json') } &&
      aenderungen.any? { |c| c.include?('welt01.json') }
  end
  db = JSON.parse(File.read(File.join(demo, 'database.json'), encoding: 'UTF-8'))
  check('Demo-Datenbank enthält Gegner und Truppen der Vorlage') do
    db['enemies'].map { |e| e['id'] } == ['goblin'] &&
      db['troops'].map { |t| t['id'] } == ['waldweg'] &&
      db['actors'].map { |a| a['id'] } == ['held']
  end
  sys = JSON.parse(File.read(File.join(demo, 'system.json'), encoding: 'UTF-8'))
  check('system.json: last_played bleibt, fehlende Schlüssel kommen dazu') do
    sys['last_played'] == '2026-01-01 00:00' && sys['neu_key'] == 1 && sys['name'] == 'Demo'
  end
  karte = JSON.parse(File.read(File.join(demo, 'maps', 'welt01.json'), encoding: 'UTF-8'))
  check('Karte: encounters ergänzt, Demo-Werte (heights/fog/name) bleiben') do
    karte.dig('encounters', 'troops') == ['waldweg'] &&
      karte.dig('terrain', 'heights') == [1] &&
      karte.dig('fog', 'enabled') == true &&
      karte['name'] == 'Welt gespielt'
  end
  check('prefabs.json wird nicht ins Demo-Projekt kopiert (global)') do
    !File.exist?(File.join(demo, 'prefabs.json'))
  end
  check('saves/ bleibt unberührt') do
    File.read(File.join(demo, 'saves', 'slot1.json'), encoding: 'UTF-8') == '{}'
  end

  zweiter = SyncDemo.sync(template: vorlage, demo: demo, verbose: false)
  check('zweiter Lauf ist leer (inhaltlich idempotent)') { zweiter.empty? }
end

# --- Zustand der mitgelieferten Datenbäume (Y1/Y2 nachgezogen) -------------
puts '== Mitgelieferte Datenbäume =='
demo_db = JSON.parse(File.read(File.join(ROOT, 'projekte', 'dorf', 'data', 'database.json'),
                               encoding: 'UTF-8'))
check('Demo-Datenbank hat Gegner und Truppen (Y1)') do
  demo_db['enemies'].is_a?(Array) && !demo_db['enemies'].empty? &&
    demo_db['troops'].is_a?(Array) && !demo_db['troops'].empty?
end

demo_karte = JSON.parse(File.read(File.join(ROOT, 'projekte', 'dorf', 'data', 'maps', 'dorf.json'),
                                  encoding: 'UTF-8'))
vorl_karte = JSON.parse(File.read(File.join(ROOT, 'game', 'data', 'maps', 'dorf.json'),
                                  encoding: 'UTF-8'))
check('Demo-Karte hat encounters mit bekannten Truppen (Y1)') do
  troops = demo_karte.dig('encounters', 'troops')
  ids = demo_db['troops'].map { |t| t['id'] }
  troops.is_a?(Array) && !troops.empty? && troops.all? { |t| ids.include?(t) }
end

def script_cmd_ev(karte, name)
  ev = karte['events'].find { |e| e['name'] == name }
  liste = if ev['pages'].is_a?(Array) && !ev['pages'].empty?
            ev['pages'][0]['commands']
          else
            ev['commands']
          end
  liste.is_a?(Array) ? liste.find { |c| c['cmd'] == 'script' } : nil
end

check('Wächter Mara ruft dorf_begruessung (Y2, beide Bäume)') do
  [vorl_karte, demo_karte].all? do |k|
    script_cmd_ev(k, 'Wächter Mara')&.[]('name') == 'dorf_begruessung'
  end
end
check('Kristall (Mitte) ruft kristalle_zaehlen (Y2, beide Bäume)') do
  [vorl_karte, demo_karte].all? do |k|
    script_cmd_ev(k, 'Kristall (Mitte)')&.[]('name') == 'kristalle_zaehlen'
  end
end

beide_scripts = %w[game/data projekte/dorf/data].flat_map do |rel|
  data = JSON.parse(File.read(File.join(ROOT, rel, 'scripts.json'), encoding: 'UTF-8'))
  data['scripts'].map { |s| s['name'] }
end
check('beide Scripts in beiden scripts.json definiert') do
  %w[dorf_begruessung kristalle_zaehlen].all? { |n| beide_scripts.count(n) == 2 }
end

# Der Repo-Zustand muss exakt dem entsprechen, was sync_demo erzeugt -
# sonst driftet die Demo wieder auseinander, ohne dass jemand es merkt.
# dry: der Prüflauf schreibt nichts.
echt = SyncDemo.sync(template: File.join(ROOT, 'game', 'data'),
                     demo: File.join(ROOT, 'projekte', 'dorf', 'data'),
                     verbose: false, dry: true)
unless echt.empty?
  puts "    offen: #{echt.join(', ')}"
end
check('sync_demo meldet für die mitgelieferten Bäume keine Änderung') do
  echt.empty?
end

puts($fail.zero? ? 'SYNC-DEMO-TEST OK (0 Fehler)' : "SYNC-DEMO-TEST FEHLGESCHLAGEN (#{$fail} Fehler)")
exit($fail.zero? ? 0 : 1)
