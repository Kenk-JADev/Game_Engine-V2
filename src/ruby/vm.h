#pragma once
#include <string>

namespace rune::ruby {

// Startet die eingebettete CRuby-VM und registriert das Modul "Rune".
bool init(int argc, char** argv, const std::string& root);

// Laedt eine Ruby-Datei relativ zum Arbeitsverzeichnis. Fehler landen im Log,
// die Engine laeuft weiter (wichtig fuer Hot-Reload).
bool load_script(const std::string& path);

// Ruft Rune.tick(dt) auf; Exceptions werden abgefangen und geloggt.
void tick(float dt);

void shutdown();

} // namespace rune::ruby
