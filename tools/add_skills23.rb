#encoding: utf-8
# ============================================================================
#  Etappe 20a: Fertigkeiten (Skills) und Zustände (States) für die Datenbank.
#  Legt die beiden neuen Bereiche an und gibt den Helden ihre Start-Skills:
#    game/data/database.json            (Vorlage für neue Projekte)
#    projekte/dorf/data/database.json   (Demo-Projekt)
#  aufruf: ruby tools/add_skills23.rb
#
#  Das Skript ist idempotent: vorhandene Einträge bleiben unangetastet, nur
#  fehlende IDs kommen dazu (eigene Skills/Zustände des Nutzers überleben).
# ============================================================================

require 'json'
require 'fileutils'

ROOT = File.expand_path('..', __dir__)

STATES = [
  { 'id' => 'gift', 'name' => 'Gift', 'kind' => 'poison', 'power' => 6,
    'turns' => 3, 'desc' => 'Verliert 6 HP pro Runde (3 Runden).' },
  { 'id' => 'brand', 'name' => 'Brand', 'kind' => 'poison', 'power' => 4,
    'turns' => 2, 'desc' => 'Verliert 4 HP pro Runde (2 Runden).' },
  { 'id' => 'regen', 'name' => 'Regeneration', 'kind' => 'regen', 'power' => 10,
    'turns' => 3, 'desc' => 'Heilt 10 HP pro Runde (3 Runden).' },
  { 'id' => 'schutz', 'name' => 'Schutz', 'kind' => 'def_up', 'power' => 5,
    'turns' => 3, 'desc' => '+5 Verteidigung (3 Runden).' },
  { 'id' => 'wut', 'name' => 'Wut', 'kind' => 'atk_up', 'power' => 6,
    'turns' => 3, 'desc' => '+6 Angriff (3 Runden).' }
].freeze

# kind: damage (Schaden), heal (Heilung), buff (Zustand auf das Ziel)
# scope: enemy (Standard), ally (Held), self (wirkt auf den Anwender)
SKILLS = [
  { 'id' => 'kraftschlag', 'name' => 'Kraftschlag', 'kind' => 'damage',
    'scope' => 'enemy', 'mp' => 5, 'power' => 20,
    'desc' => 'Wuchtiger Hieb mit 20 Grundschaden.' },
  { 'id' => 'wutausbruch', 'name' => 'Wutausbruch', 'kind' => 'buff',
    'scope' => 'self', 'mp' => 3, 'power' => 0, 'state' => 'wut',
    'state_rate' => 1.0, 'turns' => 3,
    'desc' => 'Steigert den eigenen Angriff um 6 (3 Runden).' },
  { 'id' => 'schutzhaltung', 'name' => 'Schutzhaltung', 'kind' => 'buff',
    'scope' => 'ally', 'mp' => 4, 'power' => 0, 'state' => 'schutz',
    'state_rate' => 1.0, 'turns' => 3,
    'desc' => 'Erhöht die Verteidigung eines Helden um 5 (3 Runden).' },
  { 'id' => 'feuerball', 'name' => 'Feuerball', 'kind' => 'damage',
    'scope' => 'enemy', 'mp' => 6, 'power' => 16, 'state' => 'brand',
    'state_rate' => 0.4, 'turns' => 2,
    'desc' => 'Feuerschaden; 40 % Chance auf Brand.' },
  { 'id' => 'eisspeer', 'name' => 'Eisspeer', 'kind' => 'damage',
    'scope' => 'enemy', 'mp' => 4, 'power' => 12,
    'desc' => 'Günstiger Eiszauber mit 12 Grundschaden.' },
  { 'id' => 'heilung', 'name' => 'Heilung', 'kind' => 'heal',
    'scope' => 'ally', 'mp' => 5, 'power' => 40,
    'desc' => 'Heilt 40 HP plus halbe Weisheit.' },
  { 'id' => 'regeneration', 'name' => 'Regeneration', 'kind' => 'buff',
    'scope' => 'ally', 'mp' => 6, 'power' => 0, 'state' => 'regen',
    'state_rate' => 1.0, 'turns' => 3,
    'desc' => 'Heilt 10 HP pro Runde (3 Runden).' }
].freeze

LERNT = {
  'arik' => %w[kraftschlag wutausbruch schutzhaltung],
  'mira' => %w[feuerball eisspeer heilung regeneration]
}.freeze

def ergaenze(db, schluessel, eintraege)
  liste = db[schluessel]
  liste = [] unless liste.is_a?(Array)
  neu = 0
  eintraege.each do |eintrag|
    next if liste.any? { |h| h.is_a?(Hash) && h['id'].to_s == eintrag['id'].to_s }

    liste << eintrag
    neu += 1
  end
  db[schluessel] = liste
  neu
end

targets = [
  File.join(ROOT, 'game', 'data', 'database.json'),
  File.join(ROOT, 'projekte', 'dorf', 'data', 'database.json')
]

targets.each do |path|
  unless File.exist?(path)
    puts "fehlt (übersprungen): #{path}"
    next
  end
  db = JSON.parse(File.read(path, encoding: 'UTF-8'))
  n_states = ergaenze(db, 'states', STATES)
  n_skills = ergaenze(db, 'skills', SKILLS)
  # Helden bekommen ihre Start-Fertigkeiten (nur ergänzen, nie löschen).
  (db['actors'] || []).each do |held|
    next unless held.is_a?(Hash)

    ids = LERNT[held['id'].to_s]
    next unless ids

    held['skills'] = Array(held['skills']).map(&:to_s)
    ids.each { |id| held['skills'] << id unless held['skills'].include?(id) }
  end
  File.write(path, JSON.pretty_generate(db) + "\n", encoding: 'UTF-8')
  puts format('%s: +%d Zustände, +%d Fertigkeiten', path, n_states, n_skills)
end

puts 'fertig'
