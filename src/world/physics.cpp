#include "world/physics.h"

#include <algorithm>
#include <cmath>

using namespace rune::math;

namespace rune::world {
namespace {
constexpr float kMaxStep = 0.35f; // Stufen bis zu dieser Höhe werden erklommen
}

void Physics::clear_solids() { m_solids.clear(); }

// Etappe 19k.2: Block-Zonen - unsichtbare Wände über alle Höhen.
void Physics::clear_blocked() { m_blocked.clear(); }

void Physics::add_blocked(float cx, float cz, float w, float h) {
    if (w <= 0.0f || h <= 0.0f) return;
    BlockedRect r{};
    r.x0 = cx - w * 0.5f;
    r.x1 = cx + w * 0.5f;
    r.z0 = cz - h * 0.5f;
    r.z1 = cz + h * 0.5f;
    m_blocked.push_back(r);
}

bool Physics::is_blocked(float x, float z) const {
    for (const BlockedRect& r : m_blocked) {
        if (x >= r.x0 && x <= r.x1 && z >= r.z0 && z <= r.z1) return true;
    }
    return false;
}

// Etappe 19k.3: liegt der Punkt in einem Objekt-Hindernis?
bool Physics::solid_at(float x, float y, float z) const {
    for (const Aabb& b : m_solids) {
        if (x >= b.min.x && x <= b.max.x &&
            y >= b.min.y && y <= b.max.y &&
            z >= b.min.z && z <= b.max.z) {
            return true;
        }
    }
    return false;
}

// Überlappt das Rechteck (x0..x1, z0..z1) eine Block-Zone?
bool Physics::blocked_overlaps(float x0, float z0, float x1, float z1) const {
    for (const BlockedRect& r : m_blocked) {
        if (x0 < r.x1 && x1 > r.x0 && z0 < r.z1 && z1 > r.z0) return true;
    }
    return false;
}

void Physics::add_solid(Vec3 center, Vec3 size) {
    Aabb b{};
    b.min = Vec3{center.x - size.x * 0.5f, center.y - size.y * 0.5f, center.z - size.z * 0.5f};
    b.max = Vec3{center.x + size.x * 0.5f, center.y + size.y * 0.5f, center.z + size.z * 0.5f};
    m_solids.push_back(b);
}

void Physics::teleport(Vec3 p) {
    m_pos = p;
    m_vel = Vec3{0.0f, 0.0f, 0.0f};
    m_on_ground = false;
}

void Physics::push_out(float cx, float cz, float radius) {
    const float dx = m_pos.x - cx;
    const float dz = m_pos.z - cz;
    const float d = std::sqrt(dx * dx + dz * dz);
    if (d >= radius || d < 1e-5f) return;

    const Vec3 old = m_pos;
    const float f = radius / d;
    m_pos.x = cx + dx * f;
    m_pos.z = cz + dz * f;
    if (overlaps_any(m_pos)) m_pos = old;   // nicht in Wände drücken
}

bool Physics::overlaps_any(Vec3 pos) const {
    const float x0 = pos.x - radius, x1 = pos.x + radius;
    const float y0 = pos.y + 0.02f, y1 = pos.y + height;
    const float z0 = pos.z - radius, z1 = pos.z + radius;

    for (const Aabb& b : m_solids) {
        if (x0 < b.max.x && x1 > b.min.x &&
            y0 < b.max.y && y1 > b.min.y &&
            z0 < b.max.z && z1 > b.min.z) {
            return true;
        }
    }
    // Etappe 19k.2: Block-Zonen ignorieren die Höhe (unsichtbare Wände).
    if (blocked_overlaps(x0, z0, x1, z1)) return true;
    return false;
}

float Physics::terrain_floor(Vec3 pos, const Terrain& terrain) const {
    float floor_h = terrain.ready() ? terrain.height_at(pos.x, pos.z) : 0.0f;

    for (const Aabb& b : m_solids) {
        const bool inside_xz = pos.x + radius > b.min.x && pos.x - radius < b.max.x &&
                               pos.z + radius > b.min.z && pos.z - radius < b.max.z;
        if (!inside_xz) continue;
        // Nur Deckel, die knapp über den Füssen liegen, zählen als Boden.
        if (b.max.y <= pos.y + kMaxStep && b.max.y > floor_h) floor_h = b.max.y;
    }
    return floor_h;
}

void Physics::update(float dt, const core::InputState& in, float yaw, const Terrain& terrain) {
    dt = std::clamp(dt, 0.0f, 0.05f);

    // --- Wunschrichtung aus WASD, relativ zur Blickrichtung
    float fwd = 0.0f, side = 0.0f;
    if (in.down('W')) fwd += 1.0f;
    if (in.down('S')) fwd -= 1.0f;
    if (in.down('D')) side += 1.0f;
    if (in.down('A')) side -= 1.0f;

    const float sy = std::sin(yaw);
    const float cy = std::cos(yaw);
    Vec3 wish{0.0f, 0.0f, 0.0f};
    if (fwd != 0.0f || side != 0.0f) {
        wish = Vec3{sy * fwd + cy * side, 0.0f, cy * fwd - sy * side};
        const float len = std::sqrt(wish.x * wish.x + wish.z * wish.z);
        if (len > 1e-5f) {
            wish.x /= len;
            wish.z /= len;
        }
    }

    float sp = speed;
    if (in.down(VK_SHIFT)) sp *= run_multiplier;
    m_vel.x = wish.x * sp;
    m_vel.z = wish.z * sp;

    // --- Sprung
    if (in.pressed(VK_SPACE) && m_on_ground) {
        m_vel.y = jump_speed;
        m_on_ground = false;
    }

    // --- Gravitation
    m_vel.y -= gravity * dt;
    if (m_vel.y < -60.0f) m_vel.y = -60.0f;

    // --- Bewegung achsenweise, damit man an Wänden entlangrutscht
    m_pos.x += m_vel.x * dt;
    if (overlaps_any(m_pos)) {
        m_pos.x -= m_vel.x * dt;
        m_vel.x = 0.0f;
    }

    m_pos.z += m_vel.z * dt;
    if (overlaps_any(m_pos)) {
        m_pos.z -= m_vel.z * dt;
        m_vel.z = 0.0f;
    }

    m_pos.y += m_vel.y * dt;
    if (overlaps_any(m_pos)) {
        m_pos.y -= m_vel.y * dt;
        m_vel.y = 0.0f;
    }

    // --- Boden: Terrain oder Deckel einer Box
    const float floor_h = terrain_floor(m_pos, terrain);
    m_ground_h = floor_h;

    if (m_pos.y <= floor_h) {
        m_pos.y = floor_h;
        if (m_vel.y < 0.0f) m_vel.y = 0.0f;
        m_on_ground = true;
    } else {
        m_on_ground = false;
    }

    // Etappe 18l: Kartengrenze - der Spieler darf die Karte nicht verlassen.
    // Das Terrain ist auf (0,0) zentriert ([-w/2, w/2] x [-h/2, h/2]); ausser-
    // halb klemmt height_at() ohnehin, aber die Physik liess einen trotzdem
    // ins Leere laufen. Clamp mit Spieler-Radius Rand.
    if (terrain.ready()) {
        const float hx = terrain.half_w() - radius;
        const float hz = terrain.half_h() - radius;
        m_pos.x = std::clamp(m_pos.x, -hx, hx);
        m_pos.z = std::clamp(m_pos.z, -hz, hz);
    }

    // Sicherheitsnetz: nicht unter die Welt fallen
    if (m_pos.y < -200.0f) {
        teleport(Vec3{0.0f, 2.0f, 0.0f});
    }
}

} // namespace rune::world
