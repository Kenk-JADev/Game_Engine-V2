#pragma once
#include <string>

#include "core/window.h"
#include "gfx/camera.h"
#include "gfx/renderer.h"
#include "world/physics.h"
#include "world/terrain.h"

namespace rune {

// Gemeinsamer Zustand, den die Ruby-Bindings brauchen.
struct App {
    core::Window*   window   = nullptr;
    gfx::Renderer*  renderer = nullptr;
    gfx::Camera*    camera   = nullptr;
    world::Terrain* terrain  = nullptr;
    world::Physics* physics  = nullptr;
    std::string     root;             // Engine-Wurzel als UTF-8
    float           dt  = 0.0f;
    float           fps = 0.0f;
    bool            sim_paused = false;   // z. B. im Kampf: Physik eingefroren
};

App& app();

} // namespace rune
