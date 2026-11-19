#include "gfx/ui.h"

#include <algorithm>
#include <cstring>

#include <d3dcompiler.h>

#include "core/log.h"

namespace rune::gfx {

namespace {
const char* kShaderSource = R"(
cbuffer CB2D : register(b0) { float2 screen; float use_tex; float pad; };

struct VSIn  { float2 pos : POSITION; float2 uv : TEXCOORD0; float4 col : COLOR; };
struct VSOut { float4 clip : SV_POSITION; float2 uv : TEXCOORD0; float4 col : COLOR; };

Texture2D tex0 : register(t0);
SamplerState samp0 : register(s0);

VSOut VSMain(VSIn i) {
    VSOut o;
    o.clip = float4(i.pos.x / screen.x * 2.0f - 1.0f,
                    1.0f - i.pos.y / screen.y * 2.0f,
                    0.0f, 1.0f);
    o.uv  = i.uv;
    o.col = i.col;
    return o;
}

float4 PSMain(VSOut i) : SV_TARGET {
    float4 c = i.col;
    if (use_tex > 0.5f) {
        float a = tex0.Sample(samp0, i.uv).r;  // weisse Glyphen -> Luminanz als Alpha
        c.rgb *= a;
        c.a   *= a;
    }
    return c;
}
)";

} // namespace

bool Ui::init(ID3D11Device* dev, ID3D11DeviceContext* ctx, int width, int height) {
    m_dev = dev;
    m_ctx = ctx;
    m_w = width;
    m_h = height;

    if (!create_pipeline()) return false;
    if (!create_text_surface(width, height)) return false;

    // 1x1 weisse Textur für Rechtecke
    {
        const uint32_t px = 0xFFFFFFFF;
        D3D11_TEXTURE2D_DESC td{};
        td.Width = 1; td.Height = 1; td.MipLevels = 1; td.ArraySize = 1;
        td.Format = DXGI_FORMAT_R8G8B8A8_UNORM; td.SampleDesc.Count = 1;
        td.Usage = D3D11_USAGE_IMMUTABLE; td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA sd{};
        sd.pSysMem = &px; sd.SysMemPitch = 4;
        if (FAILED(m_dev->CreateTexture2D(&td, &sd, &m_white_tex))) {
            core::log_error("Ui: weisse Textur fehlgeschlagen");
            return false;
        }
        m_dev->CreateShaderResourceView(m_white_tex, nullptr, &m_white_srv);
        if (!m_white_srv) core::log_error("Ui: weisses SRV fehlgeschlagen");
    }

    // Dynamischer Vertexpuffer (6 Vertices pro Quad)
    {
        D3D11_BUFFER_DESC bd{};
        bd.Usage = D3D11_USAGE_DYNAMIC;
        bd.ByteWidth = sizeof(Vertex) * 6 * 4096;
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        if (FAILED(m_dev->CreateBuffer(&bd, nullptr, &m_vb))) {
            core::log_error("Ui: Vertexpuffer fehlgeschlagen");
            return false;
        }
    }

    {
        D3D11_SAMPLER_DESC sd{};
        sd.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
        sd.AddressU = sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        m_dev->CreateSamplerState(&sd, &m_sampler);
        if (!m_sampler) core::log_error("Ui: Sampler fehlgeschlagen");
    }

    core::log_info("Ui-Overlay bereit (" + std::to_string(width) + "x" + std::to_string(height) + ")");
    return true;
}

bool Ui::create_pipeline() {
    auto compile = [&](const char* src, const char* entry, const char* target,
                       ID3D10Blob** blob) -> bool {
        ID3D10Blob* err = nullptr;
        const HRESULT hr = D3DCompile(src, std::strlen(src), nullptr, nullptr, nullptr,
                                      entry, target, D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, blob, &err);
        if (FAILED(hr)) {
            if (err) {
                core::log_error("Ui-Shader-Fehler: " +
                                std::string(static_cast<const char*>(err->GetBufferPointer())));
                err->Release();
            }
            return false;
        }
        if (err) err->Release();
        return true;
    };

    ID3D10Blob* vs = nullptr;
    ID3D10Blob* ps = nullptr;
    if (!compile(kShaderSource, "VSMain", "vs_4_0", &vs)) return false;
    if (!compile(kShaderSource, "PSMain", "ps_4_0", &ps)) { vs->Release(); return false; }

    if (FAILED(m_dev->CreateVertexShader(vs->GetBufferPointer(), vs->GetBufferSize(), nullptr, &m_vs))) {
        core::log_error("Ui: CreateVertexShader fehlgeschlagen");
        vs->Release(); ps->Release();
        return false;
    }
    if (FAILED(m_dev->CreatePixelShader(ps->GetBufferPointer(), ps->GetBufferSize(), nullptr, &m_ps))) {
        core::log_error("Ui: CreatePixelShader fehlgeschlagen");
        vs->Release(); ps->Release();
        return false;
    }

    D3D11_INPUT_ELEMENT_DESC elems[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8,  D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };
    m_dev->CreateInputLayout(elems, 3, vs->GetBufferPointer(), vs->GetBufferSize(), &m_layout);
    vs->Release();
    ps->Release();
    if (!m_layout) return false;

    m_cb = nullptr;
    {
        D3D11_BUFFER_DESC bd{};
        bd.Usage = D3D11_USAGE_DEFAULT;
        bd.ByteWidth = sizeof(CB2D);
        bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        if (FAILED(m_dev->CreateBuffer(&bd, nullptr, &m_cb))) {
            core::log_error("Ui: Constant buffer fehlgeschlagen");
            return false;
        }
    }

    D3D11_RASTERIZER_DESC rd{};
    rd.FillMode = D3D11_FILL_SOLID;
    rd.CullMode = D3D11_CULL_NONE;
    rd.DepthClipEnable = TRUE;
    m_dev->CreateRasterizerState(&rd, &m_raster);

    D3D11_DEPTH_STENCIL_DESC dd{};
    dd.DepthEnable = FALSE;
    dd.StencilEnable = FALSE;
    m_dev->CreateDepthStencilState(&dd, &m_depth_off);

    D3D11_BLEND_DESC bd{};
    bd.RenderTarget[0].BlendEnable = TRUE;
    bd.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    bd.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    m_dev->CreateBlendState(&bd, &m_blend);

    return true;
}

bool Ui::create_text_surface(int width, int height) {
    width = std::max(width, 64);
    height = std::max(height, 64);

    HDC screen = GetDC(nullptr);
    m_dc = CreateCompatibleDC(screen);

    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = width;
    bi.bmiHeader.biHeight = -height; // top-down
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    m_dib = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &m_bits, nullptr, 0);
    ReleaseDC(nullptr, screen);
    if (!m_dib || !m_bits) {
        core::log_error("Ui: GDI-Fläche fehlgeschlagen");
        return false;
    }
    SelectObject(m_dc, m_dib);

    D3D11_TEXTURE2D_DESC td{};
    td.Width = width; td.Height = height; td.MipLevels = 1; td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM; td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT; td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    if (FAILED(m_dev->CreateTexture2D(&td, nullptr, &m_text_tex))) {
        core::log_error("Ui: Text-Textur fehlgeschlagen");
        return false;
    }
    m_dev->CreateShaderResourceView(m_text_tex, nullptr, &m_text_srv);
    if (!m_text_srv) core::log_error("Ui: Text-SRV fehlgeschlagen");

    // Vorbereiten: schwarz füllen
    std::memset(m_bits, 0, static_cast<size_t>(width) * height * 4);
    return true;
}

HFONT Ui::font_for(float size) {
    const int key = static_cast<int>(size * 10.0f);
    auto it = m_fonts.find(key);
    if (it != m_fonts.end()) return it->second;

    // CLEARTYPE vermeiden (Farbfransen), stattdessen echtes Antialiasing.
    LOGFONTW lf{};
    lf.lfHeight = -static_cast<LONG>(size);
    lf.lfWeight = FW_NORMAL;
    lf.lfQuality = ANTIALIASED_QUALITY;
    lf.lfCharSet = DEFAULT_CHARSET;
    std::wcscpy(lf.lfFaceName, L"Segoe UI");
    HFONT f = CreateFontIndirectW(&lf);
    m_fonts[key] = f;
    return f;
}

void Ui::clear() {
    m_rects.clear();
    m_texts.clear();
}

void Ui::rect(float x, float y, float w, float h, uint32_t rgb, float alpha) {
    m_rects.push_back(RectCmd{x, y, w, h, rgb, alpha});
}

void Ui::text(float x, float y, const std::string& s, uint32_t rgb, float size) {
    m_texts.push_back(TextCmd{x, y, size, rgb, s});
}

void Ui::push_quad(std::vector<Vertex>& out, float x, float y, float w, float h,
                   float u0, float v0, float u1, float v1, uint32_t rgb, float alpha) {
    const float r = ((rgb >> 16) & 0xFF) / 255.0f;
    const float g = ((rgb >> 8) & 0xFF) / 255.0f;
    const float b = (rgb & 0xFF) / 255.0f;
    Vertex v[4] = {
        {x,     y,     u0, v0, r, g, b, alpha},
        {x + w, y,     u1, v0, r, g, b, alpha},
        {x + w, y + h, u1, v1, r, g, b, alpha},
        {x,     y + h, u0, v1, r, g, b, alpha},
    };
    const unsigned idx[6] = {0, 1, 2, 0, 2, 3};
    for (unsigned k : idx) out.push_back(v[k]);
}

bool Ui::rebuild_text_texture() {
    std::memset(m_bits, 0, static_cast<size_t>(m_w) * m_h * 4);
    SetBkMode(m_dc, TRANSPARENT);
    SetTextColor(m_dc, RGB(255, 255, 255));
    SetTextAlign(m_dc, TA_TOP | TA_LEFT);

    // 1) IMMER messen: Die Befehle werden pro Frame neu erzeugt (w=0).
    //    Nur das Rastern/Updaten der Textur ist gecacht.
    std::string key;
    for (TextCmd& t : m_texts) {
        const std::wstring ws = to_wide(t.s);
        if (ws.empty()) { t.w = 0.0f; t.h = t.size; }
        else {
            HFONT f = font_for(t.size);
            if (f) SelectObject(m_dc, f);
            SIZE sz{};
            GetTextExtentPoint32W(m_dc, ws.c_str(), static_cast<int>(ws.size()), &sz);
            t.w = static_cast<float>(sz.cx);
            t.h = static_cast<float>(sz.cy);
        }
        key += std::to_string(static_cast<int>(t.x));
        key += ",";
        key += std::to_string(static_cast<int>(t.y));
        key += ",";
        key += std::to_string(static_cast<int>(t.size));
        key += ":";
        key += t.s;
        key += "\n";
    }
    if (key == m_text_cache) return true;

    // 2) Textur nur neu rastern, wenn sich Inhalt/Position geändert hat.
    for (TextCmd& t : m_texts) {
        const std::wstring ws = to_wide(t.s);
        if (ws.empty()) continue;
        HFONT f = font_for(t.size);
        if (f) SelectObject(m_dc, f);
        const int ox = static_cast<int>(t.x);
        const int oy = static_cast<int>(t.y);
        if (ox < m_w && oy < m_h) {
            TextOutW(m_dc, ox, oy, ws.c_str(), static_cast<int>(ws.size()));
        }
    }

    m_ctx->UpdateSubresource(m_text_tex, 0, nullptr, m_bits, m_w * 4, 0);
    m_text_cache = key;
    return true;
}

std::wstring Ui::to_wide(const std::string& s) {
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    if (n <= 1) return {};
    std::wstring ws(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &ws[0], n);
    ws.pop_back();
    return ws;
}

void Ui::render() {
    if (!m_ctx || (m_rects.empty() && m_texts.empty())) return;

    CB2D cb;
    cb.screen[0] = static_cast<float>(m_w);
    cb.screen[1] = static_cast<float>(m_h);
    m_ctx->UpdateSubresource(m_cb, 0, nullptr, &cb, 0, 0);

    m_ctx->IASetInputLayout(m_layout);
    m_ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_ctx->VSSetShader(m_vs, nullptr, 0);
    m_ctx->PSSetShader(m_ps, nullptr, 0);
    m_ctx->VSSetConstantBuffers(0, 1, &m_cb);
    m_ctx->PSSetConstantBuffers(0, 1, &m_cb);
    m_ctx->PSSetSamplers(0, 1, &m_sampler);
    m_ctx->OMSetDepthStencilState(m_depth_off, 0);
    m_ctx->OMSetBlendState(m_blend, nullptr, 0xFFFFFFFF);
    m_ctx->RSSetState(m_raster);

    // Vertexliste bauen: erst Rechtecke, dann Text.
    std::vector<Vertex> verts;
    verts.reserve((m_rects.size() + m_texts.size()) * 6);

    for (const RectCmd& r : m_rects) {
        push_quad(verts, r.x, r.y, r.w, r.h, 0.0f, 0.0f, 0.0f, 0.0f, r.rgb, r.alpha);
    }
    const size_t rect_vcount = verts.size();

    if (!m_texts.empty() && rebuild_text_texture()) {
        const float iw = 1.0f / static_cast<float>(m_w);
        const float ih = 1.0f / static_cast<float>(m_h);
        for (const TextCmd& t : m_texts) {
            if (t.w <= 0.0f) continue;
            push_quad(verts, t.x, t.y, t.w, t.h,
                      t.x * iw, t.y * ih, (t.x + t.w) * iw, (t.y + t.h) * ih,
                      t.rgb, 1.0f);
        }
    }

    const UINT stride = sizeof(Vertex);
    const UINT offset = 0;
    m_ctx->IASetVertexBuffers(0, 1, &m_vb, &stride, &offset);

    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(m_ctx->Map(m_vb, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) return;
    const size_t bytes = verts.size() * sizeof(Vertex);
    std::memcpy(mapped.pData, verts.data(), std::min<size_t>(bytes, sizeof(Vertex) * 6 * 4096));
    m_ctx->Unmap(m_vb, 0);

    // Pass 1: Rechtecke (weisse Textur, use_tex=0)
    {
        cb.use_tex = 0.0f;
        m_ctx->UpdateSubresource(m_cb, 0, nullptr, &cb, 0, 0);
        ID3D11ShaderResourceView* srv = m_white_srv;
        m_ctx->PSSetShaderResources(0, 1, &srv);
        if (rect_vcount > 0) m_ctx->Draw(static_cast<UINT>(rect_vcount), 0);
    }
    // Pass 2: Text (Textfläche, use_tex=1)
    {
        cb.use_tex = 1.0f;
        m_ctx->UpdateSubresource(m_cb, 0, nullptr, &cb, 0, 0);
        ID3D11ShaderResourceView* srv = m_text_srv;
        m_ctx->PSSetShaderResources(0, 1, &srv);
        const size_t text_vcount = verts.size() - rect_vcount;
        if (text_vcount > 0) m_ctx->Draw(static_cast<UINT>(text_vcount), static_cast<UINT>(rect_vcount));
    }

    ID3D11ShaderResourceView* none[1] = {nullptr};
    m_ctx->PSSetShaderResources(0, 1, none);
}

void Ui::resize(int width, int height) {
    width = std::max(width, 64);
    height = std::max(height, 64);
    m_w = width;
    m_h = height;

    // GDI-Fläche neu erzeugen
    if (m_text_tex) { m_text_srv->Release(); m_text_tex->Release(); m_text_tex = nullptr; m_text_srv = nullptr; }
    if (m_dib) { DeleteObject(m_dib); m_dib = nullptr; }
    m_bits = nullptr;
    m_text_cache.clear();
    create_text_surface(width, height);
}

void Ui::shutdown() {
    for (auto& [k, f] : m_fonts) DeleteObject(f);
    m_fonts.clear();
    if (m_text_srv) { m_text_srv->Release(); m_text_srv = nullptr; }
    if (m_text_tex) { m_text_tex->Release(); m_text_tex = nullptr; }
    if (m_dib) { DeleteObject(m_dib); m_dib = nullptr; }
    if (m_dc) { DeleteDC(m_dc); m_dc = nullptr; }
    if (m_white_srv) { m_white_srv->Release(); m_white_srv = nullptr; }
    if (m_white_tex) { m_white_tex->Release(); m_white_tex = nullptr; }
    if (m_sampler) { m_sampler->Release(); m_sampler = nullptr; }
    if (m_blend) { m_blend->Release(); m_blend = nullptr; }
    if (m_depth_off) { m_depth_off->Release(); m_depth_off = nullptr; }
    if (m_raster) { m_raster->Release(); m_raster = nullptr; }
    if (m_cb) { m_cb->Release(); m_cb = nullptr; }
    if (m_vb) { m_vb->Release(); m_vb = nullptr; }
    if (m_layout) { m_layout->Release(); m_layout = nullptr; }
    if (m_ps) { m_ps->Release(); m_ps = nullptr; }
    if (m_vs) { m_vs->Release(); m_vs = nullptr; }
}

} // namespace rune::gfx