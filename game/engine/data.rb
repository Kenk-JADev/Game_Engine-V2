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

  # Verzeichnisse namens *.json sind keine Karten. Dateilinks werden wie bei
  # normalen Ruby-Dateizugriffen gefolgt; dies ist keine Sandbox-Prüfung.
  # Die Endung kann auch .JSON heißen (wie im RuneEditor).
  def self.map_files
    dir = File.join(root, 'maps')
    return [] unless Dir.exist?(dir)

    Dir.children(dir).select { |f| f.match?(/\.json\z/i) && File.file?(File.join(dir, f)) }.sort
  rescue SystemCallError => e
    Rune::Log.warn("Kartenverzeichnis nicht lesbar: #{e.message}")
    []
  end

  # Die ID muss exakt zum Dateinamen passen. Mehrdeutige Endungen wie
  # karte.json und karte.JSON werden nicht für einen Playtest erraten.
  def self.map_filename(id)
    return nil unless id.is_a?(String) && !id.empty?

    found = map_files.select { |f| f[0...-5] == id }
    found.one? ? found.first : nil
  end

  def self.map(id)
    filename = map_filename(id) || "#{id}.json"
    m = load(File.join('maps', filename))
    m || { 'name' => id.to_s, 'terrain' => {}, 'events' => [] }
  end

  # Für eine ausdrücklich gewählte F4-Karte keinen leeren Ersatz erzeugen:
  # fehlende, mehrdeutige oder nicht als Objekt lesbare Karten sind Fehler.
  def self.playtest_map(id)
    filename = map_filename(id)
    return nil unless filename

    # F5 im Playtest liest die Datei erneut statt einen alten oder inzwischen
    # reparierten Cache-Eintrag zu übernehmen. Andere Daten bleiben wie bisher.
    path = File.join('maps', filename)
    @cache.delete(path)
    m = load(path)
    m if m.is_a?(Hash)
  end

  # Alle Karten-IDs des Projekts, sortiert und ohne die tatsächliche Endung.
  def self.map_ids
    map_files.map { |f| f[0...-5] }.sort
  end

  def self.reset!
    @cache.clear
  end
end
