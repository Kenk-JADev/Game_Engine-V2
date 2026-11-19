#encoding: utf-8
# ============================================================================
#  RuneEditor - Texturen/Sound-Boot (Etappe 17, erweitert 17h).
#
#  Der Editor ist ein eigenes C++-Programm und nutzt die Game-Engine nicht.
#  Einzig die prozeduralen Texturen werden einmalig hier erzeugt und an den
#  Renderer gereicht - danach übernimmt die C++-Schleife. Kein Spielcode,
#  kein tick, keine Szene.
#
#  Etappe 17h: zusätzlich die Spiel-Sounds anmelden (Sounds.register_all),
#  damit der Sound-Test-Dialog sie abspielen kann. Die Sound-Engine (waveOut)
#  sitzt in der gemeinsam gelinkten Core-Bibliothek.
# ============================================================================

$LOAD_PATH << File.expand_path('../engine', __dir__)
require 'textures'
require 'sounds'

Textures.register_all
Sounds.register_all
Rune::Log.info('Editor-Texturen und Sounds registriert')
true
