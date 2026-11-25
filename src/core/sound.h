#pragma once
// Sound: winmm/waveOut-Mixer. Ruby erzeugt PCM (16 Bit mono, 44100 Hz) und
// meldet es per Name an; play() legt eine Stimme an, update() mischt pro
// Frame die aktiven Stimmen in die freien waveOut-Puffer.
//
// Seit Etappe 21 gibt es zusätzlich einen Datei-Lader (`load_audio`, WAV und
// OGG Vorbis); alles andere kommt weiterhin prozedural aus Ruby (register_sound).

#include <cstdint>
#include <string>

#include <windows.h>
#include <mmsystem.h>

namespace rune::core::sound {

bool init();
void shutdown();

// PCM anmelden (16 Bit signed mono). rate != 44100 wird linear resampelt.
void register_sound(const std::string& name, int rate, const int16_t* samples, size_t count);

// Datei-Bytes (WAV oder OGG Vorbis) laden und unter `name` anmelden
// (Etappe 21, XP-BGM/SE). Das Format wird an den ersten Bytes erkannt.
// `pitch` in Prozent (50..150) verändert die Abspielgeschwindigkeit wie in XP.
// Rückgabe false bei unbekanntem Format; der Grund steht im Log.
bool load_audio(const std::string& name, const unsigned char* bytes, size_t size,
                int pitch = 100);

// Ist ein Klang dieses Namens angemeldet?
bool has(const std::string& name);

// Stimme starten (0.0..1.0, lauter als 1.0 erlaubt aber geclampt).
void play(const std::string& name, float volume);

// Endlosschleife starten (z. B. Musik). Ersetzt bereits laufende Loops des
// selben Namens — mehrfaches loop() ist also harmlos.
void loop(const std::string& name, float volume);

// Alle Stimmen eines Namens stoppen (auch Loops).
void stop(const std::string& name);

// Lautstärke laufender Stimmen eines Namens ändern (Etappe 21, BGM-Fade).
// 0..1; unbekannte Namen sind harmlos.
void set_volume(const std::string& name, float volume);

// Aktuelle Lautstärke eines Namens (0, wenn keine Stimme läuft) - für Tests/Skripte.
float volume_of(const std::string& name);

// Einmal pro Frame aufrufen: füllt freigewordene Puffer neu.
void update();

bool ready();

} // namespace rune::core::sound
