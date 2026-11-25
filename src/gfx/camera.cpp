#include "gfx/camera.h"

#include <algorithm>
#include <cmath>

using namespace rune::math;

namespace rune::gfx {
namespace {
constexpr float kMaxPitch = PI * 0.49f;
} // namespace

Vec3 Camera::forward() const {
    const float cp = std::cos(pitch);
    return Vec3{std::sin(yaw) * cp, std::sin(pitch), std::cos(yaw) * cp};
}

void Camera::update(const core::InputState& in, float dt) {
    // Maus: Gieren und Nicken
    if (in.mouse_captured) {
        yaw   += in.mouse_dx * mouse_sensitivity;
        pitch -= in.mouse_dy * mouse_sensitivity;
        pitch = std::clamp(pitch, -kMaxPitch, kMaxPitch);

        if (yaw >  PI) yaw -= 2.0f * PI;
        if (yaw < -PI) yaw += 2.0f * PI;

        if (in.wheel != 0) {
            dist -= static_cast<float>(in.wheel) / 120.0f * 0.6f;
            dist = std::clamp(dist, 1.5f, 30.0f);
        }
    }

    // Kamera-Kollision: Abstand weich auf die freie Sichtlinie bringen.
    // Reinziehen sofort (kein Clipping), Rausziehen gegättet.
    const float want = (collision_dist >= 0.0f) ? std::min(dist, collision_dist) : dist;
    if (want < m_applied_dist) {
        m_applied_dist = want;
    } else {
        m_applied_dist += (want - m_applied_dist) * std::min(1.0f, dt * 5.0f);
    }
    const float wlift = collision_lift;
    if (wlift < m_applied_lift) {
        m_applied_lift = wlift;
    } else {
        m_applied_lift += (wlift - m_applied_lift) * std::min(1.0f, dt * 5.0f);
    }

    // Bewegung übernimmt Physics; die Kamera schaut nur.
}

Vec3 Camera::eye_position() const {
    const Vec3 fwd = forward();

    if (mode == CameraMode::FirstPerson) {
        return Vec3{target.x, target.y + eye_height, target.z};
    }
    return sub(Vec3{target.x, target.y + eye_height + 0.6f + m_applied_lift, target.z},
               scale(fwd, m_applied_dist));
}

Mat4 Camera::view_proj(int fb_width, int fb_height) const {    const float aspect = (fb_height > 0) ? static_cast<float>(fb_width) / static_cast<float>(fb_height) : 1.0f;

    const Mat4 proj = perspective_fov_lh(fov, aspect, 0.08f, 600.0f);
    const Vec3 up{0.0f, 1.0f, 0.0f};
    const Vec3 fwd = forward();

    // Etappe 20f: Bildschirmwackeln (XP 225) verschiebt das Auge entlang der
    // Kamera-Rechts-/Hochachse; Blickrichtung und Ziel bleiben gleich, damit
    // das Bild nur zittert statt zu schwenken.
    Vec3 wackeln{0.0f, 0.0f, 0.0f};
    if (shake_x != 0.0f || shake_y != 0.0f) {
        Vec3 rechts = normalize(cross(up, fwd));
        if (dot(rechts, rechts) < 1e-8f) rechts = Vec3{1.0f, 0.0f, 0.0f};
        const Vec3 hoch = cross(fwd, rechts);
        wackeln = add(scale(rechts, shake_x), scale(hoch, shake_y));
    }

    const Vec3 eye = add(eye_position(), wackeln);

    Mat4 view;
    if (mode == CameraMode::FirstPerson) {
        view = look_to_lh(eye, fwd, up);
    } else {
        view = look_at_lh(eye, add(Vec3{target.x, target.y + eye_height, target.z}, wackeln), up);
    }
    return mul(view, proj);
}

Ray Camera::pick_ray(int fb_width, int fb_height, float screen_x, float screen_y) const {
    const float w = static_cast<float>(fb_width > 0 ? fb_width : 1);
    const float h = static_cast<float>(fb_height > 0 ? fb_height : 1);
    const float aspect = w / h;

    // Blickrichtung wie in view_proj (LH):
    const Vec3 eye = eye_position();
    Vec3 view_dir = forward();
    if (mode == CameraMode::ThirdPerson) {
        view_dir = normalize(sub(Vec3{target.x, target.y + eye_height, target.z}, eye));
    }

    const Vec3 up{0.0f, 1.0f, 0.0f};
    const Vec3 xaxis = normalize(cross(up, view_dir));
    const Vec3 yaxis = cross(view_dir, xaxis);

    // Pixel -> NDC (-1..1), y zeigt nach unten.
    const float ndc_x = 2.0f * (screen_x / w) - 1.0f;
    const float ndc_y = 1.0f - 2.0f * (screen_y / h);

    const float tan_half = std::tan(fov * 0.5f);
    const Vec3 dir = normalize(add(view_dir,
                                   add(scale(xaxis, ndc_x * tan_half * aspect),
                                       scale(yaxis, ndc_y * tan_half))));

    Ray r;
    r.origin = eye;
    r.dir = dir;
    return r;
}

} // namespace rune::gfx
