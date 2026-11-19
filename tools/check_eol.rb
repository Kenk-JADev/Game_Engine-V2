# Zählt Zeilenenden (CRLF vs. LF) in den geänderten Dateien.
%w[docs/architecture.md docs/HANDOFF_Naechste_KI.md game/main.rb
   src/editor/editor_main.cpp src/gfx/renderer.cpp src/gfx/renderer.h
   src/ruby/vm.cpp src/core/window.cpp].each do |f|
  bin = File.binread(f)
  crlf = bin.force_encoding('BINARY').scan("\r\n").size
  lf = bin.count("\n") - crlf
  puts format('%-32s CRLF=%d LF=%d', f, crlf, lf)
end
