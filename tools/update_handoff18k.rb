# Aktualisiert docs/HANDOFF_Naechste_KI.md um Etappe 18k
# (Kartenbaum: XP-Kartenliste links im Editor).
# Aufruf: ruby tools/update_handoff18k.rb
path = File.join(__dir__, '..', 'docs', 'HANDOFF_Naechste_KI.md')
text = File.read(path, encoding: 'UTF-8')

# 1) Etappen-Tabelle um 18k erweitern (hinter der 18j-Zeile).
row_18j = '| 18j | cfe2860 | Resize-Rendering (16-ms-Timer in WM_ENTERSIZEMOVE), Clear+Present nach ResizeBuffers, editor.log parallel lesbar (_SH_DENYNO) |'
raise '18j-Zeile fehlt' unless text.include?(row_18j)
row_18k = '| 18k | `ETAPPEN_HASH` | Kartenbaum (XP-Kartenliste): TreeView links oben, Doppelklick = Eigenschaften, Rechtsklick-Kontextmenue mit Inplace-Umbenennen |'
text = text.sub(row_18j, row_18j + "\n" + row_18k)

# 2) Live-Test-Abschnitt: live18k als neue Vorlage davor.
old_live = '`log/live18j.ps1` ist die aktuellste funktionierende Vorlage'
new_live = '`log/live18k.ps1` (Kartenbaum: TVM_GETCOUNT/TVM_GETNEXTITEM pruefen, IntPtr-Rueckgabewerte von SendMessage mit .ToInt64() vergleichen!) und `log/live18j.ps1` sind die aktuellsten funktionierenden Vorlagen'
raise 'Live-Test-Absatz fehlt' unless text.include?(old_live)
text = text.sub(old_live, new_live)

File.write(path, text, mode: 'wb')
puts 'HANDOFF: 18k eingetragen (Hash nach Commit nachtragen!)'
