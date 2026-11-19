#pragma once
//
// Minimale lineare Algebra fuer Rune3D.
// Konvention: Matrizen sind row-major im Speicher und werden mit Zeilenvektoren
// multipliziert (v' = v * M). Die HLSL-Seite sieht durch das Standard-Packing
// (column_major) die Transponierte, deshalb steht dort mul(M, v).
//
#include <cmath>

namespace rune::math {

constexpr float PI = 3.14159265358979f;

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Vec4 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 0.0f;
};

struct Mat4 {
    float m[16] = {1, 0, 0, 0,
                   0, 1, 0, 0,
                   0, 0, 1, 0,
                   0, 0, 0, 1};
};

// ---------------------------------------------------------------- Vektoren

inline Vec3 make_vec3(float x, float y, float z) { return Vec3{x, y, z}; }

inline Vec3 add(Vec3 a, Vec3 b) { return Vec3{a.x + b.x, a.y + b.y, a.z + b.z}; }
inline Vec3 sub(Vec3 a, Vec3 b) { return Vec3{a.x - b.x, a.y - b.y, a.z - b.z}; }
inline Vec3 scale(Vec3 a, float s) { return Vec3{a.x * s, a.y * s, a.z * s}; }

inline float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

inline Vec3 cross(Vec3 a, Vec3 b) {
    return Vec3{a.y * b.z - a.z * b.y,
                a.z * b.x - a.x * b.z,
                a.x * b.y - a.y * b.x};
}

inline Vec3 normalize(Vec3 a) {
    const float len = std::sqrt(dot(a, a));
    if (len <= 1e-8f) return Vec3{0.0f, 0.0f, 0.0f};
    return scale(a, 1.0f / len);
}

// ---------------------------------------------------------------- Matrizen

inline Mat4 identity() { return Mat4{}; }

inline Mat4 mul(const Mat4& a, const Mat4& b) {
    Mat4 r;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k) sum += a.m[i * 4 + k] * b.m[k * 4 + j];
            r.m[i * 4 + j] = sum;
        }
    }
    return r;
}

inline Mat4 scaling(float x, float y, float z) {
    Mat4 r;
    r.m[0] = x;
    r.m[5] = y;
    r.m[10] = z;
    return r;
}

inline Mat4 translation(float x, float y, float z) {
    Mat4 r;
    r.m[12] = x;
    r.m[13] = y;
    r.m[14] = z;
    return r;
}

inline Mat4 rotation_x(float a) {
    const float s = std::sin(a), c = std::cos(a);
    Mat4 r;
    r.m[5] = c;  r.m[6] = s;
    r.m[9] = -s; r.m[10] = c;
    return r;
}

inline Mat4 rotation_y(float a) {
    const float s = std::sin(a), c = std::cos(a);
    Mat4 r;
    r.m[0] = c; r.m[2] = -s;
    r.m[8] = s; r.m[10] = c;
    return r;
}

inline Mat4 rotation_z(float a) {
    const float s = std::sin(a), c = std::cos(a);
    Mat4 r;
    r.m[0] = c;  r.m[1] = s;
    r.m[4] = -s; r.m[5] = c;
    return r;
}

inline Mat4 rotation_roll_pitch_yaw(float rx, float ry, float rz) {
    return mul(mul(rotation_x(rx), rotation_y(ry)), rotation_z(rz));
}

// Linkshaendiges Projektions- und Sichtsystem (Direct3D-Konvention).
inline Mat4 perspective_fov_lh(float fov_y, float aspect, float z_near, float z_far) {
    const float h = 1.0f / std::tan(fov_y * 0.5f);
    const float w = h / (aspect > 1e-6f ? aspect : 1.0f);
    const float range = z_far - z_near;

    Mat4 r;
    r.m[0]  = w;
    r.m[5]  = h;
    r.m[10] = z_far / range;
    r.m[11] = 1.0f;
    r.m[14] = -z_near * z_far / range;
    r.m[15] = 0.0f;
    return r;
}

inline Mat4 look_to_lh(Vec3 eye, Vec3 dir, Vec3 up) {
    const Vec3 zaxis = normalize(dir);
    const Vec3 xaxis = normalize(cross(up, zaxis));
    const Vec3 yaxis = cross(zaxis, xaxis);

    Mat4 r;
    r.m[0]  = xaxis.x; r.m[1]  = yaxis.x; r.m[2]  = zaxis.x; r.m[3]  = 0.0f;
    r.m[4]  = xaxis.y; r.m[5]  = yaxis.y; r.m[6]  = zaxis.y; r.m[7]  = 0.0f;
    r.m[8]  = xaxis.z; r.m[9]  = yaxis.z; r.m[10] = zaxis.z; r.m[11] = 0.0f;
    r.m[12] = -dot(xaxis, eye);
    r.m[13] = -dot(yaxis, eye);
    r.m[14] = -dot(zaxis, eye);
    r.m[15] = 1.0f;
    return r;
}

inline Mat4 orthographic_lh(float w, float h, float z_near, float z_far) {
    const float range = z_far - z_near;
    Mat4 r;
    r.m[0]  = 2.0f / w;
    r.m[5]  = 2.0f / h;
    r.m[10] = 1.0f / range;
    r.m[14] = -z_near / range;
    r.m[15] = 1.0f;
    return r;
}

inline Mat4 look_at_lh(Vec3 eye, Vec3 target, Vec3 up) {
    return look_to_lh(eye, sub(target, eye), up);
}

} // namespace rune::math
