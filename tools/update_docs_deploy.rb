# Haengt docs/architecture.md einen Deployment-Abschnitt an
# (Doppelklick-Start ohne Ruby-PATH; Fix aus Etappe 18e-Nacharbeit).
# Aufruf: ruby tools/update_docs_deploy.rb
path = File.join(__dir__, '..', 'docs', 'architecture.md')
text = File.read(path, encoding: 'UTF-8')

section = <<~MD

  ## Doppelklick-Deployment (Nacharbeit zu 18e)

  Vorher starteten beide EXEn nur, wenn der MSYS2/Ruby-PATH gesetzt war —
  per Explorer-Doppelklick kam „libwinpthread-1.dll nicht gefunden" bzw.
  Aktivierungsfehler 0xc0150002. Drei Ursachen, drei Fixes:

  **1. libwinpthread statisch:** Der g++-Driver haengt HINTER den
  Nutzer-Flags `-lpthread` (dynamisch) an; ein einfaches `-Bstatic`
  vor `-lwinpthread` wirkt deshalb nicht. Loesung in build.ps1:
  `-Wl,-Bstatic,--whole-archive -lwinpthread -Wl,--no-whole-archive,-Bdynamic`
  — whole-archive erzwingt die statische libwinpthread.a, egal was
  danach kommt. Beide EXEn importieren die DLL nicht mehr.

  **2. XCopy-DLLs:** Die Ruby-DLL (x64-ucrt-ruby340.dll) importiert
  libgmp-10.dll und libwinpthread-1.dll und fordert per eingebettetem
  Manifest die private SxS-Assembly „ruby_builtin_dlls" an. build.ps1
  kopiert deshalb diese drei Dateien plus den Ordner
  `ruby_builtin_dlls\` (Manifest + DLLs, liegt in C:\\Ruby34-x64\\bin)
  nach build/. Fehlt der Ordner: Fehler 0xc0150002 (Ereignisprotokoll
  SideBySide, Quelle der Diagnose).

  **3. RUBYLIB in vm.cpp:** Die Ruby-DLL berechnet ihr lib-Praefix
  relativ zum eigenen Speicherort. Liegt sie in build/, findet sie
  stdlib und Gems ('json') nicht mehr — das Spiel startet dann ohne
  Spielskripte („LoadError: json", 0 Draws). ruby::ruby::init() setzt
  deshalb RUBYLIB auf die acht Installationspfade (site_ruby,
  vendor_ruby, 3.4.0, x64-mingw-ucrt) — nur falls RUBYLIB nicht schon
  gesetzt ist. ruby_init_loadpath() nimmt die Variable in $LOAD_PATH auf.

  **Tauefalle fuer manuelle Link-Tests:** g++/gcc ohne den Toolchain-PATH
  schlagen mit Exit 1 und VOLLKOMMEN leerer Ausgabe fehl (cc1.exe kann
  nicht starten — Loader-Fehler, der auf stderr nicht ankommt). Immer
  vorher `$env:PATH = "C:\\Ruby34-x64\\msys64\\ucrt64\\bin;..."` setzen.
  PowerShell-Array-Parameter mit `-Wl,a,b` sind ok, inline nach
  `& $cmd -flag` wird an Kommas zerpflueckt („Argument in der
  Parameterliste fehlt").

  **Verifiziert:** Doppelklick-Simulation ohne Ruby-PATH (PATH=
  C:\\Windows\\System32;C:\\Windows): Editor-Fenster steht (Titel
  „RuneEditor - dorf - Dorf"), Spiel rendert (79 Draws, 0 Fehler), mit
  Ruby-PATH alle 13 Tests + live18e/18d/18c gruen.

MD
File.open(path, 'a') { |f| f.write(section) }
puts 'architecture.md: Deployment-Abschnitt angehaengt'
