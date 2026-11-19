# Aktualisiert docs/HANDOFF_Naechste_KI.md um Etappe 18j
# (Resize-Rendering + parallel lesbares Editor-Log).
# Aufruf: ruby tools/update_handoff18j.rb
path = File.join(__dir__, '..', 'docs', 'HANDOFF_Naechste_KI.md')
text = File.read(path, encoding: 'UTF-8')

# 1) Etappen-Tabelle um 18j erweitern (hinter der 18i-Zeile).
row_18i = '| 18i | `614ab38` | Responsives Editor-Layout: Spalten wachsen mit, Werkzeugblock verteilt sich, Undo/Redo volle Breite, Viewport-Log |'
raise '18i-Zeile fehlt' unless text.include?(row_18i)
row_18j = '| 18j | ETAPPEN_HASH | Resize-Rendering (16-ms-Timer in WM_ENTERSIZEMOVE), Clear+Present nach ResizeBuffers, editor.log parallel lesbar (_SH_DENYNO) |'
text = text.sub(row_18i, row_18i + "\n" + row_18j)

# 2) Live-Test-Abschnitt: live18j als aktuelle Vorlage eintragen.
old_live = 'log/live18i.ps1` ist die aktuellste funktionierende Vorlage'
new_live = 'log/live18j.ps1` ist die aktuellste funktionierende Vorlage (Resize-Timer: MAUSFREI, nur PostMessage von WM_ENTERSIZEMOVE/WM_EXITSIZEMOVE - kein SetCursorPos/mouse_event! Drossel-Falle: DWM drosselt Present fuer Hintergrund-Fenster auf ~8 fps, deshalb 2 s zwischen ENTER und EXIT warten sonst zaehlt der Timer 0 Frames). Danach ' + old_live.sub('log/live18i.ps1` ist die aktuellste funktionierende Vorlage', 'log/live18i.ps1`')
raise 'Live-Test-Absatz fehlt' unless text.include?(old_live)
text = text.sub(old_live, new_live)

File.write(path, text, mode: 'wb')
puts 'HANDOFF: 18j eingetragen (Hash nach Commit nachtragen!)'
