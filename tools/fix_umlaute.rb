# Etappe 18c (Umlaut-Fix): Vereinheitlicht alle sichtbaren Fenstertexte
# (L"..."-Literale) auf echte Umlaute. Aeltere Dialoge zeigten bereits
# "Löschen"/"Ausgewählt", neuere "Loeschen"/"gewaehlt" - Mischmasch.
# Log-Strings (core::log_*) bleiben transliteriert, weil die Live-Tests
# (log/live*.ps1) darauf matchen.
# Aufruf: ruby tools/fix_umlaute.rb
BETROFFEN = ['src/editor/editor_main.cpp'].freeze

# Wortliste (laengere Woerter zuerst, damit Verbindungen sicher treffen).
# "Segoe UI" (Font) und Typ-Token wie "money" bleiben unberuehrt: nur
# ganze Woerter werden ersetzt.
MAPPE = {
  'geoeffneten'   => 'geöffneten',
  'ausgewaehltes' => 'ausgewähltes',
  'ausgewaehlte'  => 'ausgewählte',
  'Auswaehlen'    => 'Auswählen',
  'Rueckgaengig'  => 'Rückgängig',
  'rueckgaengig'  => 'rückgängig',
  'zuruecksetzen' => 'zurücksetzen',
  'uebernommen'   => 'übernommen',
  'ueberschreibt' => 'überschreibt',
  'uebernehmen'   => 'übernehmen',
  'Uebernehmen'   => 'Übernehmen',
  'unberuehrt'    => 'unberührt',
  'Kachelgroesse' => 'Kachelgröße',
  'Lautstaerke'   => 'Lautstärke',
  'Intensitaet'   => 'Intensität',
  'ausfuehren'    => 'ausführen',
  'Schliessen'    => 'Schließen',
  'Aenderungen'   => 'Änderungen',
  'auswaehlen'    => 'auswählen',
  'ungueltig'     => 'ungültig',
  'Oeffnen'       => 'Öffnen',
  'oeffnen'       => 'öffnen',
  'Gelaende'      => 'Gelände',
  'gewaehlt'      => 'gewählt',
  'Loeschen'      => 'Löschen',
  'loeschen'      => 'löschen',
  'Staerke'       => 'Stärke',
  'groesse'       => 'größe',
  'Hoehe'         => 'Höhe',
  'hoehe'         => 'höhe',
  'Menue'         => 'Menü'
}.freeze

BETROFFEN.each do |pfad|
  src = File.binread(pfad).force_encoding('UTF-8')
  orig = src.dup
  n = 0

  # Nur innerhalb von L"..."-Literalen ersetzen (ganze Woerter).
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
    puts "#{pfad}: nichts zu aendern"
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
