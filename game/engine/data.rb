#encoding: utf-8
# ============================================================================
#  Datenbank: lädt die JSON-Dateien des aktiven Projekts (System + Karten).
#
#  Projekte liegen in  projekte/<name>/data/...  (wie beim RPG Maker jedes
#  Spiel ein eigener Ordner ist). Ohne Projekt gilt game/data als Wurzel -
#  das ist zugleich die Vorlage für das Demo-Projekt.
#  Alle Dateien werden als UTF-8 gelesen, damit Umlaute und Sonderzeichen
#  im Text korrekt durchkommen.
# ============================================================================

require 'json'

module Database
  DEFAULT_ROOT = File.join(Rune::ROOT, 'game', 'data')

  @cache = {}

  # Datenwurzel des aktiven Projekts (vom Titelbildschirm gesetzt).
  def self.root
    $project_root || DEFAULT_ROOT
  end

  # Liest eine JSON-Datei (mit Cache). Pfad relativ zur Datenwurzel.
  def self.load(path)
    return @cache[path] if @cache.key?(path)
    full = File.join(root, path)
    raise "Datei fehlt: #{full}" unless File.exist?(full)
    raw = File.read(full, encoding: 'UTF-8')
    @cache[path] = JSON.parse(raw)
  rescue => e
    Rune::Log.error("Datenbank: #{path} konnte nicht geladen werden: #{e.message}")
    nil
  end

  def self.system
    load('system.json') || {}
  end

  # Etappe 17g: Datenbank (Helden/Items/Gold). Optional - fehlt die Datei,
  # gilt ein leerer Katalog (keine Helden, kein Start-Inventar).
  def self.database
    load('database.json') || {}
  end

  def self.map(id)
    m = load(File.join('maps', "#{id}.json"))
    m || { 'name' => id.to_s, 'terrain' => {}, 'events' => [] }
  end

  # Alle Karten-IDs des Projekts, sortiert.
  def self.map_ids
    dir = File.join(root, 'maps')
    return [] unless Dir.exist?(dir)
    Dir.children(dir).grep(/\.json$/i).map { |f| File.basename(f, '.json') }.sort
  end

  def self.reset!
    @cache.clear
  end
end
