#pragma once
// Sound: winmm/waveOut-Mixer. Ruby erzeugt PCM (16 Bit mono, 44100 Hz) und
// meldet es per Name an; play() legt eine Stimme an, update() mischt pro
// Frame die aktiven Stimmen in die freien waveOut-Puffer.
//
// Kein Thread, kein Datei-Loader — wie die Texturen komplett prozedural.

#include <cstdint>
#include <string>

#include <windows.h>
#include <mmsystem.h>

namespace rune::core::sound {

bool init();
void shutdown();

// PCM anmelden (16 Bit signed mono). rate != 44100 wird linear resampelt.
void register_sound(const std::string& name, int rate, const int16_t* samples, size_t count);

// Stimme starten (0.0..1.0, lauter als 1.0 erlaubt aber geclampt).
void play(const std::string& name, float volume);

// Endlosschleife starten (z. B. Musik). Ersetzt bereits laufende Loops des
// selben Namens — mehrfaches loop() ist also harmlos.
void loop(const std::string& name, float volume);

// Alle Stimmen eines Namens stoppen (auch Loops).
void stop(const std::string& name);

// Einmal pro Frame aufrufen: fuellt freigewordene Puffer neu.
void update();

bool ready();

} // namespace rune::core::sound
