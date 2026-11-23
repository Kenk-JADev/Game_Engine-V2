#pragma once

#include "math/math.h"

namespace rune::gfx {

// Das Terrain und der Renderer teilen sich dieses Format. Es hat keine
// Win32-Abhängigkeit, damit die echte Geländegenerierung kopflos testbar ist.
struct MeshVertex {
    math::Vec3 pos;
    math::Vec3 nrm;
    math::Vec3 col;
    float      uv[2];
};

} // namespace rune::gfx
