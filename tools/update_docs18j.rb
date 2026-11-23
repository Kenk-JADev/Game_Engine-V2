# Aktualisiert docs/architecture.md um Etappe 18j
# (Resize-Rendering: Timer während des interaktiven Ziehens + sofortiger
#  Frame nach ResizeBuffers + parallel lesbares Log).
# Aufruf: ruby tools/update_docs18j.rb
path = File.join(__dir__, '..', 'docs', 'architecture.md')
text = File.read(path, encoding: 'UTF-8')

anchor = '## Responsives Editor-Layout (Etappe 18i)'
raise 'Anker für 18j fehlt' unless text.include?(anchor)

section = <<~MD
  ## Resize-Rendering und lesbares Editor-Log (Etappe 18j)

  Zwei Fixes rund ums Fenster-Resizen und die Live-Tests:

  **1. Rendern während des interaktiven Resizes:** Beim Ziehen der Fenster-
  kante (oder Minimieren/Restore) läuft die modale Schleife von Windows -
  die Render-Hauptschleife des Editors pausiert. Neue Flächen blieben
  schwarz oder zeigten den Hintergrund (im Live-Test: das Spielfenster).
  - `WM_ENTERSIZEMOVE` setzt einen 16-ms-Timer (`IDT_RESIZE`), dessen
    `WM_TIMER` `render_frame()` zieht und `g.resize_ticks` zählt.
  - `WM_EXITSIZEMOVE` killt den Timer, legt Layout frisch an, rendert und
    protokolliert `Resize-Timer: N Frames während des Ziehens gerendert`
    (Anker für Live-Tests).
  - `WM_SIZE` rendert jetzt zusätzlich sofort einen frischen Frame
    (Reentrancy-Guard `in_size`), damit DWM nicht den falsch skalierten
    Altfrahmen zeigt.
  - `Renderer::resize()` cleart RTV+DSV und präsentiert einmal direkt nach
    `ResizeBuffers` - der Backbuffer-Inhalt ist danach UNDEFINIERT, ohne
    Clear stand Müll im Bild.

  **2. editor.log parallel lesbar (leicht gebrochene Live-Test-Kette):**
  `core::log_open` nutzt jetzt `_wfsopen(..., _SH_DENYNO)` statt
  `fopen_s("w")` - fopen_s sperrt exklusiv, weshalb kein Live-Test das Log
  während des Editor-Laufs lesen konnte (alle alten Skripte mussten den
  Prozess erst beenden; die Diagnose-Skripte dieser Etappe scheiterten
  daran zweimal, bevor die Ursache im Log-Code stand).

  **Live-Test-Wissen (wichtig für künftige Skripte):**
  - WM_ENTERSIZEMOVE/WM_EXITSIZEMOVE einfach per PostMessage senden - das
    ist dieselbe Nachrichtenfolge wie beim echten Zug an der Fensterkante,
    komplett ohne Maus (kein pixelgenaues Treffen der Resize-Border, kein
    Klauen des Desktop-Cursors - vorherige Versuche mit SetCursorPos/
    mouse_event-Drag scheiterten serienweise an SetForegroundWindow-
    Einschränkungen und falschen Hit-Tests: SC_SIZE|HTBOTTOMRIGHT ist
    0xF011, nicht 0xF008 = HTMINBUTTON).
  - Drossel-Falle: DWM drosselt Present für verdeckte Hintergrund-Fenster
    auf ~8 fps - der Timer liefert im Test also nur ~1 Frame pro 120 ms.
    Tests müssen >= 2 s zwischen ENTER- und EXIT-Nachricht warten, sonst
    zählt der Handler 0 Frames (gilt nur für Tests; real steht der
    Editor beim Ziehen im Vordergrund).
  - Log dieser Läufe zeigt: Editor-Start, Timer-Protokoll und Beenden
    landen sauber in editor.log, das Log bleibt dabei lesbar.

  **Geprüft:** Live-Test log/live18j.ps1 (6 Checks: Fenster gefunden,
  simulierte Resize-Schleife überlebt, Timer protokolliert 47 Frames,
  programmatisches Resize + Viewport-Log, kein [ERROR]), 16 headless
  Tests grün, Build ohne Warnungen.

MD

text = text.sub(anchor, section + anchor)
File.write(path, text, mode: 'wb')
puts 'architecture.md: 18j ergänzt'
