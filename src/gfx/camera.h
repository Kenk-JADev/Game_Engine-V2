#pragma once

#include "core/window.h"
#include "math/math.h"

namespace rune::gfx {

enum class CameraMode { FirstPerson, ThirdPerson };

// Strahl in Weltkoordinaten (Ursprung + normierte Richtung).
struct Ray {
    math::Vec3 origin{};
    math::Vec3 dir{0.0f, 0.0f, 1.0f};
};

// Frei steuerbare Kamera mit zwei Modi und einem "Spieler"-Zielpunkt.
class Camera {
public:
    void update(const core::InputState& in, float dt);
    math::Mat4 view_proj(int fb_width, int fb_height) const;
    math::Vec3 eye_position() const;

    // Bildet Bildschirmpixel auf einen Weltstrahl ab (Maus-Picking).
    Ray pick_ray(int fb_width, int fb_height, float screen_x, float screen_y) const;

    math::Vec3 target{0.0f, 1.0f, 0.0f};  // Spielerposition (Fuss)
    float yaw              = 0.0f;        // Radiant, 0 = Blick nach +Z
    float pitch            = 0.0f;
    float dist             = 6.0f;        // Third-Person-Abstand
    float eye_height       = 1.6f;
    float move_speed       = 6.0f;
    float mouse_sensitivity = 0.0022f;
    float fov              = 1.0472f;     // 60 Grad
    CameraMode mode        = CameraMode::FirstPerson;

    // Kamera-Kollision (Etappe 14a): vom Spielerkopf zur Wunschposition
    // gecastet; < 0 = frei. Wird pro Frame von main.cpp gesetzt.
    float collision_dist = -1.0f;
    // > 0: Kamera wurde über ein niedriges Hindernis gehoben (Zaun, Brunnen).
    float collision_lift = 0.0f;

    // Bildschirmwackeln (Etappe 20f, XP 225): Versatz in Metern entlang der
    // Kamera-Rechts- und -Hochachse. Wird nur beim Bildaufbau addiert, damit
    // Picking und Spielerposition unberührt bleiben; ScreenFx setzt die Werte
    // pro Frame aus Stärke/Speed/Dauer.
    float shake_x = 0.0f;
    float shake_y = 0.0f;

    // Etappe 48 (XP 203 Scroll Map): freier Kamera-Versatz in Welt-XZ,
    // unabhängig von der Spielerposition. MapEffects setzt/animiert ihn;
    // ein Kartenwechsel setzt ihn zurück. Picking nutzt denselben Versatz.
    float scroll_x = 0.0f;
    float scroll_z = 0.0f;

    // Blickrichtung (auch für den Kamera-Kollisions-Check in main.cpp).
    math::Vec3 forward() const;

    // Ist-Zustand für HUD/Debug (Etappe 14a).
    float applied_dist() const { return m_applied_dist; }
    float applied_lift() const { return m_applied_lift; }

private:
    float m_applied_dist = 6.0f;   // geglätteter Abstand (Kollision/Kamera-Zoom)
    float m_applied_lift = 0.0f;   // geglättete Anhebung über Hindernisse
};

} // namespace rune::gfx
