#pragma once

#include <vector>

#include "core/window.h"
#include "math/math.h"
#include "world/terrain.h"

namespace rune::world {

// Achsenparallele Box als Hindernis (Haeuser, Zaeune, Pfosten ...).
struct Aabb {
    math::Vec3 min;
    math::Vec3 max;
};

// Spieler mit Gravitation, Sprung und Kollision. Bewusst einfach gehalten:
// Zylinder-Ersatz als AABB, achsenweise Aufloesung - reicht fuer ein Dorf.
class Physics {
public:
    void clear_solids();
    void add_solid(math::Vec3 center, math::Vec3 size);
    int  solid_count() const { return static_cast<int>(m_solids.size()); }
    const std::vector<Aabb>& solids() const { return m_solids; }

    void update(float dt, const core::InputState& in, float yaw, const Terrain& terrain);

    math::Vec3 position() const { return m_pos; }
    void teleport(math::Vec3 p);
    // Spieler aus einem Kreis (XZ) um (cx,cz) herausschieben — NPC-Kollision.
    // Ignoriert den Schub, wenn er den Spieler in ein Hindernis druecken wuerde.
    void push_out(float cx, float cz, float radius);

    bool  on_ground() const { return m_on_ground; }
    float fall_speed() const { return m_vel.y; }
    // Terrain, auf dem man gerade steht (oder 0, wenn in der Luft).
    float ground_height() const { return m_ground_h; }

    float speed           = 6.0f;   // m/s
    float run_multiplier  = 1.9f;   // mit Shift
    float jump_speed      = 6.4f;
    float gravity         = 22.0f;
    float eye_height      = 1.62f;
    float radius          = 0.34f;
    float height          = 1.75f;

private:
    bool overlaps_any(math::Vec3 pos) const;
    float terrain_floor(math::Vec3 pos, const Terrain& terrain) const;

    math::Vec3 m_pos{0.0f, 0.0f, 0.0f};
    math::Vec3 m_vel{0.0f, 0.0f, 0.0f};
    bool  m_on_ground = false;
    float m_ground_h = 0.0f;
    std::vector<Aabb> m_solids;
};

} // namespace rune::world
