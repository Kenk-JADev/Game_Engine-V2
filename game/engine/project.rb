#encoding: utf-8
# ============================================================================
#  Projekte: jedes Spiel ist ein eigener Ordner unter projekte/<name>/ mit
#  einem data/-Verzeichnis (system.json + maps/*.json) - wie beim RPG Maker.
#
#  Ohne Adminrechte und ohne Engine testbar: ruby -I game/engine tools/project_test.rb
# ============================================================================

require 'fileutils'
require 'json'

module Project
  BASE = File.join(Rune::ROOT, 'projekte')

  module_function

  # Alle Projektnamen (Ordner mit data/system.json), sortiert.
  def list
    return [] unless Dir.exist?(BASE)
    Dir.children(BASE)
       .select { |d| File.exist?(File.join(BASE, d, 'data', 'system.json')) }
       .sort
  end

  def path(name) = File.join(BASE, name)
  def data_path(name) = File.join(path(name), 'data')

  def valid?(name) = list.include?(name)

  # Bevorzugtes Projekt für den internen Start (AB, ohne Launcher):
  # zuletzt gespielt (last_played aus system.json), sonst das erste
  # alphabetisch. nil, wenn kein Projekt existiert.
  def preferred
    names = list
    return nil if names.empty?

    best = nil
    best_last = ''
    names.each do |n|
      last = read_system(n)['last_played'].to_s
      next if last.empty? || last <= best_last

      best = n
      best_last = last
    end
    best || names.first
  end

  def read_system(name)
    p = File.join(data_path(name), 'system.json')
    return {} unless File.exist?(p)
    JSON.parse(File.read(p, encoding: 'UTF-8'))
  rescue StandardError
    {}
  end

  # Anzeigename des Projekts (aus system.json, sonst Ordnername).
  def title(name)
    read_system(name)['name'] || name
  end

  # Kurzinfo für den Titelbildschirm: Titel, Kartenanzahl, Spielstände,
  # zuletzt gespielt.
  def info(name)
    maps = begin
      Dir.children(File.join(data_path(name), 'maps')).count { |f| f =~ /\.json$/i }
    rescue StandardError
      0
    end
    saves = (1..3).count { |s| File.exist?(File.join(saves_dir(name), "slot#{s}.json")) }
    { 'title' => read_system(name)['name'] || name,
      'maps'  => maps,
      'saves' => saves,
      'last'  => read_system(name)['last_played'] }
  end

  # Beim Parsen doppelte Felder nicht still zusammenfallen lassen. Die
  # Startposition und unbekannte Projektdaten gehören dem Benutzer.
  class UniqueSystemObject < Hash
    def []=(key, value)
      raise JSON::ParserError, "doppeltes Feld #{key.inspect}" if key?(key)

      super
    end
  end

  # "Zuletzt gespielt" beim Start vermerken. Lesen und Schreiben passieren
  # am SELBEN offenen Handle: Unter Windows verhindert dessen fehlendes
  # Delete-Sharing den Austausch durch den Editor; der DocumentStore sperrt
  # umgekehrt dieses Schreib-Handle. Mehrere Spiele koordinieren per flock.
  # Der Zeitstempel ist optional: Bei Konflikt/ungültigem JSON nicht starten
  # verzögern und insbesondere niemals eine leere Ersatzdatei schreiben.
  def touch(name)
    path = File.join(data_path(name), 'system.json')
    File.open(path, 'r+b') do |file|
      unless file.flock(File::LOCK_EX | File::LOCK_NB)
        Rune::Log.warn('Projekt: Zeitstempel übersprungen, system.json wird verwendet')
        return false
      end
      source = file.read.force_encoding(Encoding::UTF_8)
      sys = JSON.parse(source, object_class: UniqueSystemObject)
      raise JSON::ParserError, 'system.json ist kein Objekt' unless sys.is_a?(Hash)

      sys.store('last_played', Time.now.strftime('%Y-%m-%d %H:%M'))
      output = JSON.pretty_generate(sys) + "\n"
      file.rewind
      file.write(output)
      file.truncate(file.pos)
      file.flush
      true
    end
  rescue JSON::JSONError, EncodingError, SystemCallError, IOError => e
    Rune::Log.warn("Projekt: Zeitstempel nicht gespeichert: #{e.message}")
    false
  end

  # Projektordner komplett entfernen.
  def delete(name)
    FileUtils.rm_rf(path(name))
  end

  # ---------------------------------------------------- Spielstände (Etappe 10)

  def saves_dir(name) = File.join(path(name), 'saves')

  # Spielstand ablegen: JSON unter projekte/<name>/saves/slot<N>.json.
  def save(name, slot, data)
    FileUtils.mkdir_p(saves_dir(name))
    File.write(File.join(saves_dir(name), "slot#{slot}.json"),
               JSON.pretty_generate(data) + "\n", encoding: 'UTF-8')
  end

  # Spielstand lesen; nil, wenn der Slot leer/kaputt ist.
  def load_save(name, slot)
    p = File.join(saves_dir(name), "slot#{slot}.json")
    return nil unless File.exist?(p)
    JSON.parse(File.read(p, encoding: 'UTF-8'))
  rescue StandardError
    nil
  end

  # Kurzinfo eines Slots (für Menü/Titelbildschirm): nil oder Zeit+Karte.
  def save_info(name, slot)
    d = load_save(name, slot)
    d && { 'saved' => d['saved'], 'map' => d['map'], 'time' => d['time'] }
  end

  # Höchster belegter Slot (3..1), nil wenn keine Spielstände existieren.
  def latest_save(name)
    3.downto(1) do |s|
      return s if File.exist?(File.join(saves_dir(name), "slot#{s}.json"))
    end
    nil
  end

  def has_saves?(name) = !latest_save(name).nil?

  # Aus einem Anzeigennamen einen Ordnernamen machen: "Meine Welt 2" -> meine_welt_2
  def slug(name)
    s = name.to_s.downcase
    s = s.tr('äöüß', 'aous')
    s = s.gsub(/[^a-z0-9]+/, '_').gsub(/_+/, '_').gsub(/^_|_$/, '')
    s.empty? ? 'projekt' : s[0, 32]
  end

  # Slug, der noch nicht existiert (notfalls mit _2, _3 ...).
  def unique_slug(name)
    base = slug(name)
    return base unless valid?(base)
    n = 2
    n += 1 while valid?("#{base}_#{n}")
    "#{base}_#{n}"
  end

  # Mini-Datenbank für neue Projekte (Y3/XP): ein Start-Held und ein
  # Start-Item. Leere Arrays würden die Party leer lassen und jeden Kampf
  # mit "keine Helden" abbrechen; XP kopiert die Default-Akteure ($data_actors).
  # Der Editor (create_project) spiegelt denselben Inhalt.
  def starter_database
    { 'gold' => 0,
      'actors' => [{ 'id' => 'held', 'name' => 'Held', 'class' => 'Abenteurer',
                     'level' => 1, 'hp' => 100, 'mp' => 20, 'atk' => 10,
                     'def' => 8, 'spi' => 8, 'agi' => 8, 'exp' => 0 }],
      'items' => [{ 'id' => 'heiltrank', 'name' => 'Heiltrank', 'kind' => 'heal',
                    'power' => 40, 'price' => 25, 'start' => 1,
                    'desc' => 'Heilt 40 HP eines Helden.' }],
      'enemies' => [],
      'troops' => [] }
  end

  # Neues Projekt anlegen: leere Welt (nur Gras), Startpunkt in der Mitte,
  # Mini-Datenbank mit Start-Held (Y3). Rückgabe: der Projektordnername.
  def create(title)
    name = unique_slug(title)
    Dir.mkdir(BASE) unless Dir.exist?(BASE)
    Dir.mkdir(path(name))
    Dir.mkdir(data_path(name))
    Dir.mkdir(File.join(data_path(name), 'maps'))

    system = {
      'name'      => title.to_s,
      'start'     => { 'map' => 'welt01', 'x' => 0.0, 'z' => 0.0, 'yaw' => Math::PI },
      'switches'  => {},
      'variables' => {},
    }
    File.write(File.join(data_path(name), 'system.json'),
               JSON.pretty_generate(system) + "\n", encoding: 'UTF-8')

    # Y3: Start-Held + Start-Item wie im Editor-Pfad (nie leere Arrays).
    File.write(File.join(data_path(name), 'database.json'),
               JSON.pretty_generate(starter_database) + "\n", encoding: 'UTF-8')

    map = {
      'name'     => 'Neue Welt',
      # Neue Karten sind flach (Etappe 17b): Berge pro Karte zuschaltbar.
      'terrain'  => { 'width' => 240, 'height' => 240, 'resolution' => 192,
                      'seed' => rand(1..9999), 'amplitude' => 0.0 },
      'objects'  => [],
      'events'   => [],
      'start'    => { 'x' => 0.0, 'z' => 0.0 },
      'fog'      => { 'on' => false, 'far' => 180.0, 'color' => 0xBFD8EC },
      'panorama' => { 'color' => 0x3D6FB5 },
      'note'     => '',
    }
    File.write(File.join(data_path(name), 'maps', 'welt01.json'),
               JSON.pretty_generate(map) + "\n", encoding: 'UTF-8')
    name
  end

  # Beim allerersten Start: das Demo-Projekt "dorf" aus game/data erzeugen,
  # damit der Titelbildschirm nicht leer ist.
  def ensure_demo
    return if list.any?
    Dir.mkdir(BASE) unless Dir.exist?(BASE)
    Dir.mkdir(path('dorf'))
    FileUtils.cp_r(File.join(Database::DEFAULT_ROOT), data_path('dorf'))
    'dorf'
  end

  # Nächste freie Karten-ID: welt01, welt02, ...
  def next_map_id
    ids = Database.map_ids
    n = 1
    n += 1 while ids.include?(format('welt%02d', n))
    format('welt%02d', n)
  end

  # Neue leere Karte im aktiven Projekt anlegen (flach, Etappe 17b) und als
  # JSON ablegen. Rückgabe: die Karten-ID.
  def create_map(name = 'Neue Welt')
    id = next_map_id
    dir = File.join(Database.root, 'maps')
    FileUtils.mkdir_p(dir)
    map = {
      'name'     => name,
      'terrain'  => { 'width' => 240, 'height' => 240, 'resolution' => 192,
                      'seed' => rand(1..9999), 'amplitude' => 0.0 },
      'objects'  => [],
      'events'   => [],
      'start'    => { 'x' => 0.0, 'z' => 0.0 },
      'fog'      => { 'on' => false, 'far' => 180.0, 'color' => 0xBFD8EC },
      'panorama' => { 'color' => 0x3D6FB5 },
      'note'     => '',
    }
    File.write(File.join(dir, "#{id}.json"),
               JSON.pretty_generate(map) + "\n", encoding: 'UTF-8')
    id
  end
end
