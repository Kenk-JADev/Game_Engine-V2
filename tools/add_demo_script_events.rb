#encoding: utf-8
# ============================================================================
#  add_demo_script_events.rb (Runde 13 / Y2): Demo-Events rufen die
#  Projekt-Scripts auf. Bisher behaupteten die Kommentare in scripts.json,
#  ein Event rufe dorf_begruessung per {"cmd":"script",...} auf - in
#  keiner der beiden Datenbäume existierte ein script-Kommando (Y2,
#  docs/LOGIK_PRUEFUNG.md). Jetzt:
#
#    Wächter Mara      -> {"cmd":"script","name":"dorf_begruessung"}
#    Kristall (Mitte)  -> {"cmd":"script","name":"kristalle_zaehlen"}
#
#  Beide Bäume (game/data Vorlage + projekte/dorf Demo) erhalten denselben
#  Befehl als ERSTEN Befehl der ersten Seite bzw. der Legacy-Kommandoliste.
#  Idempotent: vorhandene script-Befehle werden übersprungen.
#
#  Aufruf: ruby tools/add_demo_script_events.rb
# ============================================================================

require 'json'

ROOT = File.expand_path('..', __dir__)

# Eventname => Script-Name
EINSATZ = {
  'Wächter Mara'     => 'dorf_begruessung',
  'Kristall (Mitte)' => 'kristalle_zaehlen'
}.freeze

ZIELE = [
  File.join(ROOT, 'game', 'data', 'maps', 'dorf.json'),
  File.join(ROOT, 'projekte', 'dorf', 'data', 'maps', 'dorf.json')
].freeze

def kommandoliste(event)
  # Editor-Seitenformat (pages) oder Legacy (top-level commands).
  if event['pages'].is_a?(Array) && !event['pages'].empty?
    event['pages'][0]['commands']
  else
    event['commands']
  end
end

def eintragen(pfad)
  karte = JSON.parse(File.read(pfad, encoding: 'UTF-8'))
  geaendert = false
  (karte['events'] || []).each do |ev|
    skript = EINSATZ[ev['name'].to_s]
    next unless skript
    liste = kommandoliste(ev)
    next unless liste.is_a?(Array)
    schon_da = liste.any? { |c| c.is_a?(Hash) && c['cmd'] == 'script' && c['name'] == skript }
    next if schon_da
    liste.unshift('cmd' => 'script', 'name' => skript)
    geaendert = true
  end
  if geaendert
    File.write(pfad, JSON.pretty_generate(karte) + "\n", encoding: 'UTF-8')
    puts "  add_demo_script_events: #{pfad} ergänzt"
  end
  geaendert
end

if __FILE__ == $PROGRAM_NAME
  ZIELE.each { |z| eintragen(z) }
  puts 'add_demo_script_events: fertig (idempotent).'
end
