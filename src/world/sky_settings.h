#pragma once
// Etappe 35: Himmel, Wolken, Tag/Nacht. Reine Rechenlogik ohne D3D, damit
// Editor-Vorschau und Spiel EXAKT denselben Himmel zeigen und kopflos
// getestet werden kann (tools/json_test.cpp).
//
// XP-Vorbild (recherchiert, docs/XP_EDITOR_RECHERCHE.md):
//   - „Fog-Grafik“ (Wolken/Nebel über der Karte, Deckkraft, SX/SY-Tempo)
//     -> 3D: ziehende Wolkendecke, die echte Schatten aufs Gelände wirft.
//   - „Screen Tone“ (R/G/B -255..255, Grau 0..255) für Tag/Nacht/Stimmung
//     -> 3D: Tönung des Bildes + passendes Licht.
//   - „Panorama“ (Hintergrund) -> Grundfarbe des Himmelsverlaufs.
//
// Karten-JSON (alles optional; ohne "sky" = bisheriges Aussehen + Verlauf):
//   "sky": {"clouds":0.4, "cloud_speed":[3,1], "cloud_color":16777215,
//           "cloud_shadow":0.45, "time":17.5, "tone":[-20,-10,10,0]}

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace rune::world {

struct SkySettings {
    float clouds = 0.0f;          // Bedeckung 0..1 (0 = keine Wolken)
    float cloud_speed_x = 3.0f;   // m/s (XP: SX)
    float cloud_speed_z = 1.0f;   // m/s (XP: SY)
    std::uint32_t cloud_rgb = 0xFFFFFF;
    float cloud_shadow = 0.45f;   // Abdunklung im Wolkenschatten 0..1
    bool  time_set = false;       // Tageszeit steuert Licht/Himmel
    float hour = 12.0f;           // 0..24
    float tone[4] = {0, 0, 0, 0}; // XP-Tönung: R,G,B -255..255, Grau 0..255
};

// Was der Renderer pro Frame braucht (alles schon fertig gerechnet).
struct SkyLook {
    std::uint32_t top = 0x3D6FB5, horizon = 0xBFD8EC;
    float sun_dir[3] = {-0.45f, 0.85f, -0.30f}; // zum Licht hin
    float sun_rgb[3] = {1.0f, 0.97f, 0.90f};    // Farbe/Stärke des Direktlichts
    float ambient = 0.30f;
    float ambient_rgb[3] = {1.0f, 1.0f, 1.0f};
    float night = 0.0f;                          // 0 Tag .. 1 Nacht (Sterne)
    float clouds = 0.0f, cloud_speed[2] = {3.0f, 1.0f}, cloud_shadow = 0.45f;
    float cloud_rgb[3] = {1.0f, 1.0f, 1.0f};
    float tone[4] = {0, 0, 0, 0};                // normiert: RGB -1..1, Grau 0..1
};

inline float sky_clamp(float v, float lo, float hi, float fallback) {
    return std::isfinite(v) ? std::clamp(v, lo, hi) : fallback;
}

inline std::uint32_t sky_rgb(float r, float g, float b) {
    auto c = [](float v) { return static_cast<std::uint32_t>(std::lround(std::clamp(v, 0.0f, 1.0f) * 255.0f)); };
    return (c(r) << 16) | (c(g) << 8) | c(b);
}

inline void sky_split(std::uint32_t rgb, float& r, float& g, float& b) {
    r = ((rgb >> 16) & 0xFF) / 255.0f;
    g = ((rgb >> 8) & 0xFF) / 255.0f;
    b = (rgb & 0xFF) / 255.0f;
}

inline std::uint32_t sky_mix(std::uint32_t a, std::uint32_t b, float t) {
    float ar, ag, ab, br, bg, bb;
    sky_split(a, ar, ag, ab);
    sky_split(b, br, bg, bb);
    t = sky_clamp(t, 0.0f, 1.0f, 0.0f);
    return sky_rgb(ar + (br - ar) * t, ag + (bg - ag) * t, ab + (bb - ab) * t);
}

// Aus EINER Panorama-Farbe einen echten Verlauf machen: Zenit kräftiger
// und dunkler, Horizont heller/dunstiger (vorher war der Himmel flach).
inline std::uint32_t sky_zenith_from(std::uint32_t base) { return sky_mix(base, 0x0B1E4A, 0.30f); }
inline std::uint32_t sky_horizon_from(std::uint32_t base) { return sky_mix(base, 0xE8F0F8, 0.62f); }

// Sonnenhöhe über dem Horizont (-1..1) für eine Uhrzeit: 6 Uhr Aufgang,
// 12 Uhr höchster Stand, 18 Uhr Untergang.
inline float sky_sun_elevation(float hour) {
    const float h = std::fmod(std::fmod(hour, 24.0f) + 24.0f, 24.0f);
    return std::sin((h - 6.0f) / 12.0f * 3.14159265f);
}

inline SkyLook sky_look(const SkySettings& s, std::uint32_t panorama_rgb) {
    SkyLook out;
    out.top = sky_zenith_from(panorama_rgb);
    out.horizon = sky_horizon_from(panorama_rgb);
    out.clouds = sky_clamp(s.clouds, 0.0f, 1.0f, 0.0f);
    out.cloud_speed[0] = sky_clamp(s.cloud_speed_x, -100.0f, 100.0f, 3.0f);
    out.cloud_speed[1] = sky_clamp(s.cloud_speed_z, -100.0f, 100.0f, 1.0f);
    out.cloud_shadow = sky_clamp(s.cloud_shadow, 0.0f, 1.0f, 0.45f);
    sky_split(s.cloud_rgb & 0xFFFFFF, out.cloud_rgb[0], out.cloud_rgb[1], out.cloud_rgb[2]);
    for (int i = 0; i < 3; ++i) out.tone[i] = sky_clamp(s.tone[i], -255.0f, 255.0f, 0.0f) / 255.0f;
    out.tone[3] = sky_clamp(s.tone[3], 0.0f, 255.0f, 0.0f) / 255.0f;
    if (!s.time_set || !std::isfinite(s.hour)) return out;

    // --- Tag/Nacht ---------------------------------------------------------
    const float h = std::fmod(std::fmod(s.hour, 24.0f) + 24.0f, 24.0f);
    const float elev = sky_sun_elevation(h);
    const float day = std::clamp(elev * 4.0f + 0.25f, 0.0f, 1.0f);       // weich über den Horizont
    const float dusk = std::clamp(1.0f - std::fabs(elev) * 5.0f, 0.0f, 1.0f); // Dämmerung
    out.night = 1.0f - day;
    // Sonne wandert von Osten (+x) über Süden nach Westen; nachts Mondlicht
    // von der Gegenseite, damit Schatten nicht verschwinden.
    const float az = (h - 6.0f) / 12.0f * 3.14159265f;
    float dx = std::cos(az), dy = std::max(0.12f, std::fabs(elev)), dz = -0.35f;
    if (elev < 0.0f) dx = -dx;
    const float len = std::sqrt(dx * dx + dy * dy + dz * dz);
    out.sun_dir[0] = dx / len; out.sun_dir[1] = dy / len; out.sun_dir[2] = dz / len;

    // Himmel: Tagverlauf -> Dämmerung (warm am Horizont) -> Nacht.
    const std::uint32_t night_top = 0x050A1C, night_hor = 0x1A2340;
    std::uint32_t top = sky_mix(night_top, out.top, day);
    std::uint32_t hor = sky_mix(night_hor, out.horizon, day);
    hor = sky_mix(hor, 0xF29A55, dusk * 0.75f);
    top = sky_mix(top, 0x3A3A78, dusk * 0.35f);
    out.top = top;
    out.horizon = hor;

    // Licht: Tag weißlich, Dämmerung orange, Nacht schwach bläulich (Mond).
    float sr = 1.0f, sg = 0.97f, sb = 0.90f;
    sr = sr + (1.0f - sr) * dusk; sg = sg + (0.62f - sg) * dusk; sb = sb + (0.38f - sb) * dusk;
    const float strength = 0.18f + 0.82f * day;
    const float moon = out.night;
    out.sun_rgb[0] = (sr * day + 0.35f * moon) * strength;
    out.sun_rgb[1] = (sg * day + 0.42f * moon) * strength;
    out.sun_rgb[2] = (sb * day + 0.65f * moon) * strength;
    out.ambient = 0.10f + 0.22f * day;
    out.ambient_rgb[0] = 0.55f + 0.45f * day;
    out.ambient_rgb[1] = 0.62f + 0.38f * day;
    out.ambient_rgb[2] = 0.85f + 0.15f * day;
    // Wolken nachts dunkel, in der Dämmerung angeleuchtet.
    for (int i = 0; i < 3; ++i) out.cloud_rgb[i] *= 0.25f + 0.75f * day;
    out.cloud_rgb[0] = std::min(1.0f, out.cloud_rgb[0] + 0.25f * dusk);
    out.cloud_rgb[1] = std::min(1.0f, out.cloud_rgb[1] + 0.08f * dusk);
    return out;
}

} // namespace rune::world
