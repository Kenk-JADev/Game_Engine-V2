#pragma once

#include <vector>

#include "core/window.h"
#include "math/math.h"
#include "world/terrain.h"

namespace rune::world {

// Achsenparallele Box als Hindernis (Häuser, Zäune, Pfosten ...).
struct Aabb {
    math::Vec3 min;
    math::Vec3 max;
};

// Etappe 19k.2: Block-Zone (Passierbarkeit). Rechteck in der XZ-Ebene,
// das über ALLE Höhen blockiert (unsichtbare Wand - kein Überspringen).
// Datenquelle: "blocked" im Karten-JSON ({x,z,w,h}, x/z = Mitte).
struct BlockedRect {
    float x0, z0, x1, z1;
};

// Spieler mit Gravitation, Sprung und Kollision. Bewusst einfach gehalten:
// Zylinder-Ersatz als AABB, achsenweise Auflösung - reicht für ein Dorf.
class Physics {
public:
    void clear_solids();
    void add_solid(math::Vec3 center, math::Vec3 size);
    int  solid_count() const { return static_cast<int>(m_solids.size()); }
    const std::vector<Aabb>& solids() const { return m_solids; }

    // Etappe 19k.2: Block-Zonen (Karten-JSON "blocked").
    void clear_blocked();
    void add_blocked(float cx, float cz, float w, float h);
    int  blocked_count() const { return static_cast<int>(m_blocked.size()); }
    const std::vector<BlockedRect>& blocked() const { return m_blocked; }
    // Punkt-Test (XZ) - auch für RGSS passable? und den Editor.
    bool is_blocked(float x, float z) const;

    // Etappe 19k.3: Punkt-Test gegen die Objekt-Hindernisse (solids).
    // Ruby fragt damit ab, ob an einer Stelle etwas Höheres als die
    // Steiggrenze steht (RGSS passable?, NPC-Routen) - dieselben AABBs,
    // die auch den Spieler physikalisch stoppen.
    bool solid_at(float x, float y, float z) const;

    void update(float dt, const core::InputState& in, float yaw, const Terrain& terrain);

    math::Vec3 position() const { return m_pos; }
    void teleport(math::Vec3 p);
    // Spieler aus einem Kreis (XZ) um (cx,cz) herausschieben — NPC-Kollision.
    // Ignoriert den Schub, wenn er den Spieler in ein Hindernis drücken würde.
    void push_out(float cx, float cz, float radius);

    bool  on_ground() const { return m_on_ground; }
    float fall_speed() const { return m_vel.y; }
    // Terrain, auf dem man gerade steht (oder 0, wenn in der Luft).
    float ground_height() const { return m_ground_h; }

    float speed           = 6.0f;   // m/s
    float run_multiplier  = 1.9f;   // mit Shift
    // Etappe 39: manueller Sprung (Leertaste) ist standardmässig aus.
    // Move-Route-Sprünge setzen die Position direkt und nutzen dieses Flag
    // bewusst nicht (Regieaktion, keine Spielerbewegung).
    bool  jump_allowed    = false;
    float jump_speed      = 6.4f;
    float gravity         = 22.0f;
    float eye_height      = 1.62f;
    float radius          = 0.34f;
    float height          = 1.75f;

private:
    bool overlaps_any(math::Vec3 pos) const;
    bool blocked_overlaps(float x0, float z0, float x1, float z1) const;
    float terrain_floor(math::Vec3 pos, const Terrain& terrain) const;

    math::Vec3 m_pos{0.0f, 0.0f, 0.0f};
    math::Vec3 m_vel{0.0f, 0.0f, 0.0f};
    bool  m_on_ground = false;
    float m_ground_h = 0.0f;
    std::vector<Aabb> m_solids;
    std::vector<BlockedRect> m_blocked;   // Etappe 19k.2
};

} // namespace rune::world
