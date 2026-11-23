#pragma once
#include <string>

namespace rune::ruby {

// Startet die eingebettete CRuby-VM; root ist die Engine-Wurzel als UTF-8.
bool init(int argc, char** argv, const std::string& root);

// Lädt einen UTF-8-Skriptpfad relativ zur Engine-Wurzel oder absolut. Fehler
// landen im Log; die Engine läuft weiter (wichtig für Hot-Reload).
bool load_script(const std::string& path);

// Ruft Rune.tick(dt) auf; Exceptions werden abgefangen und geloggt.
void tick(float dt);

void shutdown();

} // namespace rune::ruby
