#encoding: utf-8
# ============================================================================
#  Issues — das Laufzeit-Backlog des SPIELS (Etappe 18h).
#
#  Fehler, die das Spiel beim Interpretieren/Laden erkennt (unbekannte
#  Kommandos, fehlende Scripts/Items, unbekannte Wetter-Typen, defekte
#  Projekt-Texturen), landen hier statt nur im Log. Der aktuelle Stand wird
#  nach jeder Meldung nach log/issues.json geschrieben — die Datei ist damit
#  immer der aktuelle Fehlerzustand der laufenden Session.
#
#  Aufrufe:
#    Issues.report('Interpreter', "unbekanntes Kommando 'quatsch'")
#    Issues.report('Party', "unbekanntes Item 'xyz'", severity: 'error')
#    Issues.clear!          # bei Game.setup (F5 startet die Sammlung neu)
#    Issues.count / Issues.all / Issues.dump!
# ============================================================================

module Issues
  PATH = File.join('log', 'issues.json')

  module_function

  def clear!
    @list = []
    File.delete(PATH) if File.exist?(PATH)
    nil
  end

  def all
    @list ||= []
  end

  def count
    all.size
  end

  # Eine Meldung aufnehmen: landet im Log UND im Backlog (issues.json).
  def report(area, msg, severity: 'warn')
    entry = {
      'area'     => area.to_s,
      'severity' => severity == 'error' ? 'error' : 'warn',
      'message'  => msg.to_s,
      'time'     => Time.now.strftime('%Y-%m-%d %H:%M:%S')
    }
    all << entry
    line = "[Backlog] #{entry['area']}: #{entry['message']}"
    severity == 'error' ? Rune::Log.error(line) : Rune::Log.warn(line)
    dump!
    entry
  end

  # Backlog nach log/issues.json schreiben (Verzeichnis notfalls anlegen).
  def dump!
    Dir.mkdir('log') unless Dir.exist?('log')
    File.write(PATH, JSON.pretty_generate('issues' => all))
    PATH
  rescue StandardError => e
    Rune::Log.warn("Issues: Konnte #{PATH} nicht schreiben: #{e.message}")
    nil
  end
end
