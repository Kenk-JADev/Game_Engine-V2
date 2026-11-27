#encoding: utf-8
# ============================================================================
#  sync_demo.rb (Runde 13 / Y1): Vorlage game/data -> Demo projekte/dorf.
#
#  Y1-Doku: docs/LOGIK_PRUEFUNG.md (Datenpipeline). Die Vorlage ist die
#  Quelle für database.json und scripts.json (fehlende Truppen/Gegner,
#  tote Skripte); Karten und system.json werden nur ADDITIV ergänzt, damit
#  demo-eigene Werte (last_played, heights, fog/panorama, migrierte Seiten)
#  erhalten bleiben. prefabs.json bleibt global (bewusst nicht pro
#  Projekt), saves/ wird nie angefasst.
#
#  Aufruf (inhaltlich idempotent - zweiter Lauf meldet keine Änderung):
#     ruby tools/sync_demo.rb
#  Als Bibliothek: require-Datei laden und SyncDemo.sync(template:, demo:)
#  aufrufen; die Rückgabe ist die Liste der geänderten Dateien/Namen.
# ============================================================================

require 'json'
require 'fileutils'

module SyncDemo
  module_function

  # Spiegelt die Vorlage in die Demo und liefert eine Liste der Änderungen
  # (leer = bereits im Zustand der Vorlage). Mit dry: true wird nur
  # gerechnet und nichts geschrieben (Prüflauf im sync_demo_test).
  def sync(template:, demo:, verbose: true, dry: false)
    changes = []

    # 1) database.json und scripts.json: die Vorlage besitzt diese Dateien
    #    vollständig (Vorlage = Quelle) - bei inhaltlicher Abweichung kopieren.
    %w[database.json scripts.json].each do |name|
      quelle = File.join(template, name)
      ziel   = File.join(demo, name)
      next unless File.exist?(quelle)
      if !File.exist?(ziel) || inhalt(quelle) != inhalt(ziel)
        FileUtils.cp(quelle, ziel) unless dry
        changes << name
      end
    end

    # 2) system.json: nur fehlende Top-Level-Schlüssel übernehmen; vorhandene
    #    Demo-Werte (z.B. last_played) bleiben unangetastet.
    %w[system.json].each do |name|
      quelle = File.join(template, name)
      ziel   = File.join(demo, name)
      next unless File.exist?(quelle)
      q = inhalt(quelle)
      z = File.exist?(ziel) ? inhalt(ziel) : {}
      fehlend = q.keys - z.keys
      next if fehlend.empty?
      unless dry
        File.write(ziel, JSON.pretty_generate(z.merge(q.select { |k, _| fehlend.include?(k) })) + "\n",
                   encoding: 'UTF-8')
      end
      changes << "#{name} (+#{fehlend.join(', ')})"
    end

    # 3) Karten: je Kartendatei der Vorlage fehlende Top-Level-Schlüssel
    #    einfügen (z.B. encounters). Fehlt die Datei im Demo komplett,
    #    wird sie übernommen; vorhandene Demo-Inhalte bleiben stehen.
    Dir[File.join(template, 'maps', '*.json')].sort.each do |quelle|
      ziel = File.join(demo, 'maps', File.basename(quelle))
      q = inhalt(quelle)
      if !File.exist?(ziel)
        unless dry
          FileUtils.mkdir_p(File.dirname(ziel))
          FileUtils.cp(quelle, ziel)
        end
        changes << "maps/#{File.basename(quelle)} (neu)"
        next
      end
      z = inhalt(ziel)
      fehlend = q.keys - z.keys
      next if fehlend.empty?
      unless dry
        File.write(ziel, JSON.pretty_generate(z.merge(q.select { |k, _| fehlend.include?(k) })) + "\n",
                   encoding: 'UTF-8')
      end
      changes << "maps/#{File.basename(quelle)} (+#{fehlend.join(', ')})"
    end

    # 4) prefabs.json (global) und saves/ bleiben unberührt.
    changes.each { |c| puts "  sync_demo: #{c}" } if verbose
    changes
  end

  # JSON-Datei einlesen (Strukturvergleich, Format egal).
  def inhalt(pfad)
    JSON.parse(File.read(pfad, encoding: 'UTF-8'))
  end
end

if __FILE__ == $PROGRAM_NAME
  root = File.expand_path('..', __dir__)
  changes = SyncDemo.sync(template: File.join(root, 'game', 'data'),
                          demo: File.join(root, 'projekte', 'dorf', 'data'))
  puts(changes.empty? ? 'sync_demo: Demo ist auf dem Stand der Vorlage.' :
       "sync_demo: #{changes.size} Änderung(en) übernommen.")
end
