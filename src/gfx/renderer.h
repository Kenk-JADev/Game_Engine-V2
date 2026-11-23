#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include <windows.h>
#include <d3d11.h>

#include "math/math.h"
#include "gfx/mesh_vertex.h"
#include "gfx/ui.h"
#include "world/atlas.h"
#include "world/terrain_limits.h"
#include "world/texture_limits.h"

namespace rune::gfx {

struct DrawCmd {
    math::Vec3 pos;
    math::Vec3 size;
    math::Vec3 rot;
    math::Vec4 color;
    int        tex;   // Index in die Texturliste, -1 = untexturiert
    float      tile;  // Kachelgrösse in Metern (<=0: UV wie geliefert)
};

// Direct3D-11-Renderer: sammelt Box-Draws während eines Frames und zeichnet sie
// danach in einem Rutsch (Ruby füllt die Draw-List).
class Renderer {
public:
    // WARP ist ein expliziter Software-Testpfad; normale Programme verwenden Hardware.
    bool init(HWND hwnd, int width, int height, bool warp = false);
    void shutdown();
    void resize(int width, int height);

    void begin_frame(const math::Mat4& view_proj, const math::Vec3& eye = math::Vec3{});
    void push_box(const math::Vec3& pos, const math::Vec3& size,
                  const math::Vec3& rot, uint32_t rgb,
                  int tex = -1, float tile = 0.0f);

    // RGBA-Pixel (4 Bytes pro Pixel, zeilenweise, oben links) als Textur anmelden.
    // Rückgabe: Index für push_box, -1 bei Fehler.
    int  add_texture(const std::string& name, int w, int h, const uint8_t* rgba);
    int  texture_index(const std::string& name) const;
    std::vector<std::string> texture_names() const;

    // Projekttexturen überlagern eingebaute Namen, verändern deren Slots aber
    // nicht. Beim Wechsel werden Projekt-Slots und Atlas freigegeben; vor
    // weiteren Draws muss das Terrain neu aufgebaut werden.
    void begin_project_textures();
    void end_project_textures();
    bool remove_project_texture(const std::string& name);
    std::size_t texture_storage_slots() const { return m_tex_srvs.size(); }
    std::size_t texture_resident_bytes() const { return m_tex_resident_bytes; }

    // Himmel-Verlauf (Screen-Space) und Sonnenlicht; beides von Ruby aus gesetzt.
    void set_sky(uint32_t top_rgb, uint32_t bottom_rgb) { m_sky_top = top_rgb; m_sky_bottom = bottom_rgb; m_sky_on = true; }
    void set_sun(const math::Vec3& dir, float ambient) { m_sun = dir; m_ambient = ambient; }
    void set_fog(bool on, uint32_t rgb, float far_dist) {
        m_fog_on = on; m_fog_rgb = rgb;
        m_fog_far = world::atmosphere_valid_fog_far(far_dist) ? far_dist : 180.0f;
    }
    void set_terrain_tex(int index) { m_terrain_tex = index; }
    // Ein zu kleiner Wert würde im Shader 2/tile überlaufen.
    void set_terrain_tile(float meters) {
        m_terrain_tile = world::terrain_valid_tile_float(meters) ? meters : 2.0f;
    }
    // Pixel einer angemeldeten Textur (RGBA) für Dialog-Vorschauen; false
    // wenn Name unbekannt.
    bool texture_rgba(const std::string& name, std::vector<uint8_t>& out,
                      int& w, int& h) const;
    void set_shadows(bool on) { m_shadows_on = on; }

    // Ersetzt das Gelände-Mesh (wird beim Start und bei jedem F5 neu gebaut).
    bool set_terrain(const std::vector<MeshVertex>& verts, const std::vector<uint32_t>& indices);
    void clear_terrain();

    // Begrenzt den 64x64-Atlas und verwendet ihn bei unveränderten Texturen
    // wieder. Ohne passenden/sicheren Atlas ist valid = false.
    world::AtlasInfo update_atlas();

    // Sichert den aktuellen Backbuffer als BMP (F12).
    bool save_screenshot(const std::string& path);
    void request_screenshot(const std::string& path) { m_shot_path = path; }

    void end_frame();

    void set_clear_color(uint32_t rgb) { m_clear_rgb = rgb; }

    int  draw_count() const { return static_cast<int>(m_cmds.size()); }

    ID3D11Device* device() const { return m_device; }
    Ui&           ui() { return m_ui; }

private:
    // Der interne Atlas hat einen eigenen Slot und keinen öffentlichen Namen;
    // eine Projekttextur namens "atlas" bleibt dadurch unberührt.
    int add_texture_impl(const std::string& name, int w, int h, const uint8_t* rgba, bool internal_atlas);
    void release_texture_slot(int index);
    void unbind_texture();
    bool create_shaders();
    bool create_cube();
    bool create_targets(int width, int height);
    void release_targets();
    void apply_pipeline();

    HWND                     m_hwnd = nullptr;
    int                      m_width = 0;
    int                      m_height = 0;
    uint32_t                 m_clear_rgb = 0x101820;
    std::vector<DrawCmd>     m_cmds;

    ID3D11Device*            m_device = nullptr;
    ID3D11DeviceContext*     m_context = nullptr;
    IDXGISwapChain*          m_swap = nullptr;
    ID3D11RenderTargetView*  m_rtv = nullptr;
    ID3D11Texture2D*         m_depth_tex = nullptr;
    ID3D11DepthStencilView*  m_dsv = nullptr;

    ID3D11VertexShader*      m_vs = nullptr;
    ID3D11PixelShader*       m_ps = nullptr;
    ID3D11InputLayout*       m_layout = nullptr;
    ID3D11Buffer*            m_frame_cb = nullptr;
    ID3D11Buffer*            m_object_cb = nullptr;
    ID3D11Buffer*            m_vb = nullptr;
    ID3D11Buffer*            m_ib = nullptr;
    ID3D11SamplerState*      m_samp = nullptr;

    // Himmel: eigener Mini-Shader (Fullscreen-Dreieck ohne Vertexbuffer).
    ID3D11VertexShader*      m_sky_vs = nullptr;
    ID3D11PixelShader*       m_sky_ps = nullptr;
    ID3D11Buffer*            m_sky_cb = nullptr;
    ID3D11DepthStencilState* m_sky_dss = nullptr;
    bool                     m_sky_on = false;
    uint32_t                 m_sky_top = 0x3D6FB5;
    uint32_t                 m_sky_bottom = 0xBFD8EC;

    // Schatten: Tiefenkarte aus Sicht der Sonne + eigener Depth-Pass.
    ID3D11Texture2D*         m_shadow_tex = nullptr;
    ID3D11DepthStencilView*  m_shadow_dsv = nullptr;
    ID3D11ShaderResourceView* m_shadow_srv = nullptr;
    ID3D11VertexShader*      m_shadow_vs = nullptr;
    ID3D11SamplerState*      m_shadow_samp = nullptr;
    math::Mat4               m_light_vp;
    bool                     m_shadows_on = true;

    // Texturen: RGBA8, von Ruby aus angemeldet (Name -> Slot).
    std::vector<ID3D11ShaderResourceView*> m_tex_srvs;
    std::map<std::string, int>             m_base_tex_ids;
    std::map<std::string, int>             m_project_tex_ids;
    bool                                    m_project_textures = false;
    // Freie Slots werden wiederverwendet; Prefabs speichern keine Slotnummern.
    std::vector<int>                        m_tex_free;
    std::size_t                             m_tex_resident_bytes = 0;
    // Etappe 18c: Pixel + Grössen für Vorschauen/G_tileset-Zugriff.
    std::vector<std::vector<uint8_t>>       m_tex_pixels;
    std::vector<std::pair<int, int>>         m_tex_sizes;
    world::AtlasInfo                         m_atlas_cache;
    int                                      m_atlas_slot = -1;
    bool                                     m_atlas_dirty = true;

    math::Vec3               m_sun{ -0.45f, 0.85f, -0.30f };
    float                    m_ambient = 0.20f;
    int                      m_terrain_tex = -1;
    float                    m_terrain_tile = 2.0f;   // Meter pro Kachel

    // Distanznebel (Etappe 17b)
    bool                     m_fog_on = false;
    uint32_t                 m_fog_rgb = 0xBFD8EC;
    float                    m_fog_far = 180.0f;
    math::Vec3               m_eye{};

    ID3D11Buffer*            m_terrain_vb = nullptr;
    ID3D11Buffer*            m_terrain_ib = nullptr;

    Ui                       m_ui;
    std::string              m_shot_path;
    int                      m_terrain_index_count = 0;

    int                      m_index_count = 0;
};

} // namespace rune::gfx
