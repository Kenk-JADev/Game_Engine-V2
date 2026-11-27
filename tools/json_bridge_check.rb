#encoding: utf-8
# ============================================================================
#  JSON-Brückenprüfung (Etappe 19a): C++-Schreiber gegen Ruby-Parser.
#
#     ruby tools/json_bridge_check.rb [pfad-zu-json_test] [--strict-format]
#
#  Editor (C++, src/editor/json.cpp) und Spiel (Ruby, JSON.parse) müssen sich
#  über dieselben Dateien einig sein. Der Editor schreibt seine Karten mit
#  json_write bewusst im Format von Ruby JSON.pretty_generate — sonst erzeugt
#  jedes Speichern eine Diff-Flut in der Versionsverwaltung, und im schlimmen
#  Fall gehen Werte beim Hin- und Herwechseln verloren (int wird float,
#  Reihenfolge kippt, Umlaute werden zu Escapes).
#
#  Prüft für jede JSON-Datei des Repos:
#    1. SEMANTIK (hart): Inhalt UND Typen aus Ruby JSON.parse sind identisch.
#       Auch generierte Beispiele mit Ganzzahlen weit jenseits von 2^53 sind
#       dabei; Ruby-Integer sind beliebig groß (anders als C++-double).
#    2. FORMAT   (Info): bytegleich zu JSON.pretty_generate + Zeilenumbruch?
#                        Mit --strict-format wird auch das zum Fehlschlag.
#
#  Voraussetzung: das Testprogramm aus tools/json_test.cpp, z. B.
#     g++ -std=c++20 -O1 -Isrc src/editor/json.cpp tools/json_test.cpp -o build/json_test
# ============================================================================
require 'json'
require 'fileutils'
require 'tmpdir'
require 'open3'

ROOT = File.expand_path('..', __dir__)

# Erste Stelle, an der sich zwei JSON-Strukturen unterscheiden (als Text).
def erste_abweichung(a, b, pfad = '$')
  return "#{pfad}: Typ #{a.class} != #{b.class}" if a.class != b.class

  case a
  when Hash
    (a.keys | b.keys).each do |k|
      return "#{pfad}.#{k}: Schlüssel fehlt auf einer Seite" unless a.key?(k) && b.key?(k)

      fund = erste_abweichung(a[k], b[k], "#{pfad}.#{k}")
      return fund if fund
    end
    "#{pfad}: Schlüssel-Reihenfolge #{a.keys.first(4)} != #{b.keys.first(4)}" if a.keys != b.keys
  when Array
    return "#{pfad}: Länge #{a.size} != #{b.size}" if a.size != b.size

    a.each_index do |i|
      fund = erste_abweichung(a[i], b[i], "#{pfad}[#{i}]")
      return fund if fund
    end
    nil
  else
    a == b ? nil : "#{pfad}: #{a.inspect} != #{b.inspect}"
  end
end

# Zeigt die ersten unterschiedlichen Zeilen zweier Texte.
def format_diff(erwartet, bekommen, kontext = 2)
  a = erwartet.lines
  b = bekommen.lines
  erste = (0...[a.size, b.size].max).find { |i| a[i] != b[i] }
  return "nur Zeilenanzahl unterschiedlich: Ruby #{a.size}, C++ #{b.size}" if erste.nil?

  von = [erste - kontext, 0].max
  bis = [erste + kontext, [a.size, b.size].max - 1].min
  (von..bis).map do |i|
    markierung = i == erste ? '>>' : '  '
    "#{markierung} Zeile #{i + 1}: Ruby=#{a[i].to_s.chomp.inspect} C++=#{b[i].to_s.chomp.inspect}"
  end.join("\n")
end

strict_format = !ARGV.delete('--strict-format').nil?
binary = ARGV.find { |a| File.exist?(a) } ||
         [File.join(ROOT, 'build', 'json_test'),
          File.join(ROOT, 'build', 'json_test.exe'),
          File.join(ROOT, 'build-tests', 'json_test')].find { |p| File.exist?(p) }

unless binary
  puts 'ÜBERSPRUNGEN: json_test nicht gefunden.'
  puts '  Bau: g++ -std=c++20 -O1 -Isrc src/editor/json.cpp tools/json_test.cpp -o build/json_test'
  exit(strict_format ? 1 : 0)
end

# Alle JSON-Dateien, die der Editor anfasst: Karten, Datenbank, System,
# Scripte, Prefabs, Material-Metadaten und Spielstände.
dateien = (Dir.glob(File.join(ROOT, 'game', 'data', '**', '*.json')) +
           Dir.glob(File.join(ROOT, 'projekte', '**', '*.json'))).sort.uniq

if dateien.empty?
  puts 'Keine JSON-Dateien gefunden — nichts zu prüfen.'
  exit 1
end

puts "JSON-Brücke: #{dateien.size} Datei(en), C++-Schreiber = #{binary}"
puts '-' * 68

semantik_fehler   = 0
format_abweichung = 0
laufzeit_fehler   = 0

Dir.mktmpdir('rune_json_bruecke') do |tmp|
  beispiel = File.join(tmp, 'grosse_ganzzahlen.json')
  zahlen = { 'int53' => 9_007_199_254_740_993, 'grenze64' => 9_223_372_036_854_775_808,
             'negativ' => -18_446_744_073_709_551_615,
             'sehr_gross' => 10**399 + 12345, 'null' => 0,
             'float' => 42.0, 'geschachtelt' => [{ 'extra' => -(10**399 + 7) }] }
  File.write(beispiel, JSON.pretty_generate(zahlen) + "\n", encoding: 'utf-8')
  (dateien + [beispiel]).each_with_index do |pfad, i|
    rel = pfad == beispiel ? '[synthetische grosse Ganzzahlen]' :
          pfad.sub(ROOT + File::SEPARATOR, '').gsub(File::SEPARATOR, '/')
    neu = File.join(tmp, format('%03d_%s', i, File.basename(rel)))
    out, status = Open3.capture2e(binary, '--roundtrip', pfad, neu)

    unless status.success? && File.exist?(neu)
      laufzeit_fehler += 1
      puts "  FAIL #{rel}: C++-Roundtrip fehlgeschlagen (#{out.strip})"
      next
    end

    begin
      links  = JSON.parse(File.read(pfad, encoding: 'utf-8'))
      rechts = JSON.parse(File.read(neu, encoding: 'utf-8'))
    rescue JSON::ParserError => e
      semantik_fehler += 1
      puts "  FAIL #{rel}: Parse-Fehler (#{e.message})"
      next
    end

    if (abweichung = erste_abweichung(links, rechts))
      semantik_fehler += 1
      puts "  FAIL #{rel}: Inhalt oder Typ nach C++-Roundtrip anders"
      puts "    #{abweichung}"
      next
    end

    erwartet = JSON.pretty_generate(links) + "\n"
    cplusplus = File.read(neu, encoding: 'utf-8')
    if erwartet == cplusplus
      puts "  ok   #{rel}"
    else
      format_abweichung += 1
      puts "  INFO #{rel}: Inhalt gleich, Format weicht von JSON.pretty_generate ab"
      puts format_diff(erwartet, cplusplus).gsub(/^/, '    ')
    end
  end
end

puts '-' * 68
puts format('Ergebnis: %d Datei(en), %d Semantik-Fehler, %d Format-Abweichung(en), %d Laufzeit-Fehler',
            dateien.size + 1, semantik_fehler, format_abweichung, laufzeit_fehler)

fehlschlag = semantik_fehler.positive? || laufzeit_fehler.positive? ||
             (strict_format && format_abweichung.positive?)
exit(fehlschlag ? 1 : 0)
