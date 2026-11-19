#pragma once

#include <windows.h>
#include <d3d11.h>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace rune::gfx {

// 2D-Overlay ueber der 3D-Szene: farbige Rechtecke und Text.
// Text wird mit GDI in eine Bildflaeche gerastert (weisse Glyphen auf Schwarz)
// und als Textur gezeichnet - die Farbe kommt beim Zeichnen als Tint dazu.
// Hinweis: es wird erst *alle* Rechtecke gezeichnet, dann *aller* Text.
class Ui {
public:
    bool init(ID3D11Device* dev, ID3D11DeviceContext* ctx, int width, int height);
    void shutdown();
    void resize(int width, int height);

    void clear();
    void rect(float x, float y, float w, float h, uint32_t rgb, float alpha = 1.0f);
    void text(float x, float y, const std::string& s, uint32_t rgb = 0xFFFFFF, float size = 18.0f);
    void render();

    int rect_count() const { return static_cast<int>(m_rects.size()); }
    int text_count() const { return static_cast<int>(m_texts.size()); }

private:
    struct RectCmd {
        float x, y, w, h;
        uint32_t rgb;
        float alpha;
    };
    struct TextCmd {
        float x, y, size;
        uint32_t rgb;
        std::string s;
        float w = 0.0f; // gemessene Breite/Hoehe (nach dem Rastern)
        float h = 0.0f;
    };
    struct Vertex {
        float x, y, u, v;
        float r, g, b, a;
    };
    struct CB2D {
        float screen[2];
        float use_tex;
        float pad;
    };

    bool create_pipeline();
    bool create_text_surface(int width, int height);
    bool rebuild_text_texture();
    static std::wstring to_wide(const std::string& s);
    HFONT font_for(float size);
    void push_quad(std::vector<Vertex>& out, float x, float y, float w, float h,
                   float u0, float v0, float u1, float v1, uint32_t rgb, float alpha);

    ID3D11Device*        m_dev = nullptr;
    ID3D11DeviceContext* m_ctx = nullptr;
    int                  m_w = 0;
    int                  m_h = 0;

    ID3D11VertexShader*      m_vs = nullptr;
    ID3D11PixelShader*       m_ps = nullptr;
    ID3D11InputLayout*       m_layout = nullptr;
    ID3D11Buffer*            m_cb = nullptr;
    ID3D11Buffer*            m_vb = nullptr;
    ID3D11RasterizerState*   m_raster = nullptr;
    ID3D11DepthStencilState* m_depth_off = nullptr;
    ID3D11BlendState*        m_blend = nullptr;
    ID3D11SamplerState*      m_sampler = nullptr;

    ID3D11Texture2D*          m_white_tex = nullptr;
    ID3D11ShaderResourceView* m_white_srv = nullptr;

    // Textflaeche (GDI + D3D11-Textur)
    HDC                       m_dc = nullptr;
    HBITMAP                   m_dib = nullptr;
    void*                     m_bits = nullptr;
    ID3D11Texture2D*          m_text_tex = nullptr;
    ID3D11ShaderResourceView* m_text_srv = nullptr;
    std::string               m_text_cache;

    std::unordered_map<int, HFONT> m_fonts;
    std::vector<RectCmd> m_rects;
    std::vector<TextCmd> m_texts;
    std::vector<Vertex>  m_verts;
};

} // namespace rune::gfx
