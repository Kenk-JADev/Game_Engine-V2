# Etappe 18c (Umlaut-Fix): Vereinheitlicht alle sichtbaren Fenstertexte
# (L"..."-Literale) auf echte Umlaute. Ältere Dialoge zeigten bereits
# "Löschen"/"Ausgewählt", neuere "Löschen"/"gewählt" - Mischmasch.
# Log-Strings (core::log_*) bleiben transliteriert, weil die Live-Tests
# (log/live*.ps1) darauf matchen.
# Aufruf: ruby tools/fix_umlaute.rb
BETROFFEN = ['src/editor/editor_main.cpp'].freeze

# Wortliste (längere Wörter zürst, damit Verbindungen sicher treffen).
# "Segoe UI" (Font) und Typ-Token wie "money" bleiben unberührt: nur
# ganze Wörter werden ersetzt.
MAPPE = {
  'geöffneten'   => 'geöffneten',
  'ausgewähltes' => 'ausgewähltes',
  'ausgewählte'  => 'ausgewählte',
  'Auswählen'    => 'Auswählen',
  'Rückgängig'  => 'Rückgängig',
  'rückgängig'  => 'rückgängig',
  'zurücksetzen' => 'zurücksetzen',
  'übernommen'   => 'übernommen',
  'überschreibt' => 'überschreibt',
  'übernehmen'   => 'übernehmen',
  'Übernehmen'   => 'Übernehmen',
  'unberührt'    => 'unberührt',
  'Kachelgrösse' => 'Kachelgröße',
  'Lautstärke'   => 'Lautstärke',
  'Intensität'   => 'Intensität',
  'ausführen'    => 'ausführen',
  'Schliessen'    => 'Schließen',
  'Änderungen'   => 'Änderungen',
  'auswählen'    => 'auswählen',
  'ungültig'     => 'ungültig',
  'Öffnen'       => 'Öffnen',
  'öffnen'       => 'öffnen',
  'Gelände'      => 'Gelände',
  'gewählt'      => 'gewählt',
  'Löschen'      => 'Löschen',
  'löschen'      => 'löschen',
  'Stärke'       => 'Stärke',
  'grösse'       => 'größe',
  'Höhe'         => 'Höhe',
  'höhe'         => 'höhe',
  'Menü'         => 'Menü'
}.freeze

BETROFFEN.each do |pfad|
  src = File.binread(pfad).force_encoding('UTF-8')
  orig = src.dup
  n = 0

  # Nur innerhalb von L"..."-Literalen ersetzen (ganze Wörter).
  src = src.gsub(/L"([^"]*)"/) do
    lit = Regexp.last_match(1)
    neu = lit.dup
    MAPPE.each do |alt, neu_wort|
      neu = neu.gsub(/\b#{Regexp.escape(alt)}\b/, neu_wort)
    end
    n += 1 if neu != lit
    "L\"#{neu}\""
  end

  if src != orig
    File.binwrite(pfad, src)
    puts "#{pfad}: #{n} Literale mit Umlauten versehen"
  else
    puts "#{pfad}: nichts zu ändern"
  end
end

# Kontrolle: verbleibende Transliterationen in L-Literalen auflisten.
puts '--- Kontrolle (sollte nur Font-/Token-Funde zeigen):'
BETROFFEN.each do |pfad|
  File.read(pfad, encoding: 'utf-8').scan(/L"([^"]*)"/).each do |m|
    lit = m[0]
    next unless lit =~ /\b(ae|oe|ue|ss|Ae|Oe|Ue)\b/

    puts "  Rest: L\"#{lit}\""
  end
end
