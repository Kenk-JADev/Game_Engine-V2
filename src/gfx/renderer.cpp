#include "gfx/renderer.h"
#include "core/log.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

#include <d3dcompiler.h>

using namespace rune::core;
using namespace rune::math;

namespace rune::gfx {
namespace {

const char* kShaderSource = R"(
cbuffer FrameCB : register(b0)
{
    float4x4 gViewProj;
    float4   gSun;      // xyz = Richtung zum Licht, w = Ambient (0..1)
    float4x4 gLightVP;  // Licht-Kamera (Schatten)
    float4   gShadow;   // x = an (0/1), y = Tiefen-Bias
    float4   gFog;      // x = an (0/1), y = Distanz, zw = Nebelfarbe R,G
    float4   gCamPos;   // xyz = Kameraposition, w = Nebelfarbe B
};

cbuffer ObjectCB : register(b1)
{
    float4x4 gWorld;
    float4   gColor;
    float4   gSize;     // xyz = Grösse der Box in Metern
    float4   gParams;   // x = Textur-Slot (<0 = keine), y = Kachel in Metern (<=0 = UV roh)
};

Texture2D    gTex       : register(t0);
SamplerState gSamp      : register(s0);
Texture2D    gShadowMap : register(t1);
SamplerComparisonState gShadowSamp : register(s1);

struct VSIn
{
    float3 pos  : POSITION;
    float3 nrm  : NORMAL;
    float3 vcol : COLOR;
    float2 uv   : TEXCOORD0;
};

struct VSOut
{
    float4 clip : SV_POSITION;
    float3 nrm  : NORMAL;
    float4 col  : COLOR;
    float2 uv   : TEXCOORD0;
    float4 lpos : TEXCOORD1;   // Position im Licht-Raum (Schatten)
    float3 wpos : TEXCOORD2;   // Weltposition (Nebel-Distanz)
};

VSOut VSMain(VSIn i)
{
    VSOut o;
    float4 worldPos = mul(gWorld, float4(i.pos, 1.0f));
    o.clip = mul(gViewProj, worldPos);
    float3 n = normalize(mul((float3x3)gWorld, i.nrm));
    o.nrm  = n;
    o.col  = float4(i.vcol * gColor.rgb, gColor.a);
    o.uv   = i.uv;

    // Schatten: Position im Licht-Raum, leicht entlang der Normale versetzt
    // (gegen Shadow-Acne).
    o.lpos = mul(gLightVP, float4(worldPos.xyz + n * 0.06f, 1.0f));
    o.wpos = worldPos.xyz;

    // Boxen: UV an der Fläche in Weltmetern skalieren, damit Texturen
    // überall gleichmassig kacheln (Hauswand wie Zaunlatte).
    if (gParams.y > 0.0f)
    {
        float3 an = abs(n);
        float2 ext = (an.y >= an.x && an.y >= an.z)
                         ? float2(gSize.x, gSize.z)
                         : ((an.x >= an.z) ? float2(gSize.z, gSize.y)
                                           : float2(gSize.x, gSize.y));
        o.uv = i.uv * ext / gParams.y;
    }
    // Gelände (Etappe 18c): UV-Faktor in gParams.z - 1.0 = Standard
    // (2 m pro Kachel, gebacken), kleiner = grössere Kacheln.
    if (gParams.z > 0.0f)
    {
        o.uv = i.uv * gParams.z;
    }
    return o;
}

float4 PSMain(VSOut i) : SV_TARGET
{
    float3 base = i.col.rgb;
    if (gParams.x >= 0.0f)
    {
        base *= gTex.Sample(gSamp, i.uv).rgb;
    }

    float3 L = normalize(gSun.xyz);
    float ndl = saturate(dot(normalize(i.nrm), L));

    // Schatten: Tiefenvergleich im Licht-Raum, 2x2-PCF durch bilineares
    // SampleCmp. Ausserhalb der Licht-Abdeckung: beleuchtet.
    float shadow = 1.0f;
    if (gShadow.x > 0.5f)
    {
        float3 p = i.lpos.xyz / i.lpos.w;
        float2 suv = float2(p.x * 0.5f + 0.5f, 0.5f - p.y * 0.5f);
        if (suv.x > 0.0f && suv.x < 1.0f && suv.y > 0.0f && suv.y < 1.0f)
        {
            shadow = gShadowMap.SampleCmp(gShadowSamp, suv, p.z - gShadow.y);
        }
    }

    float lit = gSun.w + (1.0f - gSun.w) * ndl * shadow;

    // Dezente Himmels-/Bodenstreuung: von oben kühl, von unten warm.
    float hemi = i.nrm.y * 0.5f + 0.5f;
    float3 bounce = lerp(float3(0.06f, 0.05f, 0.03f), float3(0.07f, 0.09f, 0.13f), hemi);

    float3 outc = base * (lit + bounce);

    // Distanznebel (Etappe 17b): sanfte Überblendung zur Nebelfarbe.
    if (gFog.x > 0.5f)
    {
        float d = distance(i.wpos, gCamPos.xyz);
        float f = smoothstep(gFog.y * 0.45f, gFog.y, d);
        outc = lerp(outc, float3(gFog.z, gFog.w, gCamPos.w), f);
    }

    return float4(outc, 1.0f);
}

// ---------------------------------------------------------------- Himmel

cbuffer SkyCB : register(b0)
{
    float4 gTop;
    float4 gBottom;
};

struct SkyVSOut
{
    float4 clip : SV_POSITION;
    float2 uv   : TEXCOORD0;
};

SkyVSOut SkyVS(uint id : SV_VertexID)
{
    SkyVSOut o;
    float2 uv = float2((id << 1) & 2, id & 2);   // 0,0 / 2,0 / 0,2 -> Bildschirm-Dreieck
    o.clip = float4(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f, 1.0f, 1.0f);
    o.uv = uv;
    return o;
}

float4 SkyPS(SkyVSOut i) : SV_TARGET
{
    return lerp(gTop, gBottom, saturate(i.uv.y));
}

// ---------------------------------------------------------------- Schatten

// Nur-Tiefen-Pass: Position im Licht-Raum, mehr nicht.
struct ShadowVSOut
{
    float4 clip : SV_POSITION;
};

ShadowVSOut ShadowVS(VSIn i)
{
    ShadowVSOut o;
    float4 worldPos = mul(gWorld, float4(i.pos, 1.0f));
    o.clip = mul(gLightVP, worldPos);
    return o;
}
)";

struct BoxVertex {
    Vec3 pos;
    Vec3 nrm;
    float uv[2];
};

struct FrameCB {
    float view_proj[16];
    float sun[4];
    float light_vp[16];
    float shadow[4];
    float fog[4];
    float cam_pos[4];
};

struct ObjectCB {
    float world[16];
    float color[4];
    float size[4];
    float params[4];
};

struct SkyCB {
    float top[4];
    float bottom[4];
};

std::string hresult_text(HRESULT hr) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "0x%08lX", static_cast<unsigned long>(hr));
    return std::string(buf);
}

bool compile_stage(const char* entry, const char* target, ID3DBlob** out) {
    ID3DBlob* errors = nullptr;
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3;
    HRESULT hr = D3DCompile(kShaderSource, std::strlen(kShaderSource), "rune_shader.hlsl",
                            nullptr, nullptr, entry, target, flags, 0, out, &errors);

    if (FAILED(hr)) {
        std::string msg = "Shader-Fehler in ";
        msg += entry;
        msg += " (";
        msg += hresult_text(hr);
        msg += "): ";
        if (errors) msg += static_cast<const char*>(errors->GetBufferPointer());
        log_error(msg);
        if (errors) errors->Release();
        return false;
    }
    if (errors) errors->Release();
    return true;
}

// Würfel mit UV 0..1 je Fläche (Reihenfolge wie die Indizes darunter).
const BoxVertex kCubeVertices[24] = {
    // +Z
    {{-0.5f, -0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}, {0.0f, 1.0f}},
    {{ 0.5f, -0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}, {1.0f, 1.0f}},
    {{ 0.5f,  0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}, {1.0f, 0.0f}},
    {{-0.5f,  0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}, {0.0f, 0.0f}},
    // -Z
    {{ 0.5f, -0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}, {0.0f, 1.0f}},
    {{-0.5f, -0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}, {1.0f, 1.0f}},
    {{-0.5f,  0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}, {1.0f, 0.0f}},
    {{ 0.5f,  0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}, {0.0f, 0.0f}},
    // +X
    {{ 0.5f, -0.5f,  0.5f}, { 1.0f,  0.0f,  0.0f}, {0.0f, 1.0f}},
    {{ 0.5f, -0.5f, -0.5f}, { 1.0f,  0.0f,  0.0f}, {1.0f, 1.0f}},
    {{ 0.5f,  0.5f, -0.5f}, { 1.0f,  0.0f,  0.0f}, {1.0f, 0.0f}},
    {{ 0.5f,  0.5f,  0.5f}, { 1.0f,  0.0f,  0.0f}, {0.0f, 0.0f}},
    // -X
    {{-0.5f, -0.5f, -0.5f}, {-1.0f,  0.0f,  0.0f}, {0.0f, 1.0f}},
    {{-0.5f, -0.5f,  0.5f}, {-1.0f,  0.0f,  0.0f}, {1.0f, 1.0f}},
    {{-0.5f,  0.5f,  0.5f}, {-1.0f,  0.0f,  0.0f}, {1.0f, 0.0f}},
    {{-0.5f,  0.5f, -0.5f}, {-1.0f,  0.0f,  0.0f}, {0.0f, 0.0f}},
    // +Y
    {{-0.5f,  0.5f,  0.5f}, { 0.0f,  1.0f,  0.0f}, {0.0f, 0.0f}},
    {{ 0.5f,  0.5f,  0.5f}, { 0.0f,  1.0f,  0.0f}, {1.0f, 0.0f}},
    {{ 0.5f,  0.5f, -0.5f}, { 0.0f,  1.0f,  0.0f}, {1.0f, 1.0f}},
    {{-0.5f,  0.5f, -0.5f}, { 0.0f,  1.0f,  0.0f}, {0.0f, 1.0f}},
    // -Y
    {{-0.5f, -0.5f, -0.5f}, { 0.0f, -1.0f,  0.0f}, {0.0f, 0.0f}},
    {{ 0.5f, -0.5f, -0.5f}, { 0.0f, -1.0f,  0.0f}, {1.0f, 0.0f}},
    {{ 0.5f, -0.5f,  0.5f}, { 0.0f, -1.0f,  0.0f}, {1.0f, 1.0f}},
    {{-0.5f, -0.5f,  0.5f}, { 0.0f, -1.0f,  0.0f}, {0.0f, 1.0f}},
};

const uint16_t kCubeIndices[36] = {
     0,  1,  2,  0,  2,  3,
     4,  5,  6,  4,  6,  7,
     8,  9, 10,  8, 10, 11,
    12, 13, 14, 12, 14, 15,
    16, 17, 18, 16, 18, 19,
    20, 21, 22, 20, 22, 23,
};

} // namespace

bool Renderer::init(HWND hwnd, int width, int height) {
    m_hwnd = hwnd;
    m_width = width;
    m_height = height;

    D3D_FEATURE_LEVEL wanted[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0};
    D3D_FEATURE_LEVEL got = D3D_FEATURE_LEVEL_10_0;

    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, wanted,
                                   static_cast<UINT>(sizeof(wanted) / sizeof(wanted[0])),
                                   D3D11_SDK_VERSION, &m_device, &got, &m_context);
    if (FAILED(hr)) {
        log_error("D3D11CreateDevice fehlgeschlagen: " + hresult_text(hr));
        return false;
    }

    char feat[96];
    std::snprintf(feat, sizeof(feat), "D3D11-Geraet erstellt, Feature-Level 0x%X",
                  static_cast<unsigned>(got));
    log_info(feat);

    IDXGIDevice* dxgi_device = nullptr;
    if (FAILED(m_device->QueryInterface(IID_IDXGIDevice, reinterpret_cast<void**>(&dxgi_device)))) {
        log_error("IDXGIDevice nicht verfügbar");
        return false;
    }

    IDXGIAdapter* adapter = nullptr;
    dxgi_device->GetAdapter(&adapter);
    dxgi_device->Release();

    IDXGIFactory* factory = nullptr;
    if (adapter) {
        adapter->GetParent(IID_IDXGIFactory, reinterpret_cast<void**>(&factory));
        adapter->Release();
    }
    if (!factory) {
        log_error("DXGI-Factory nicht verfügbar");
        return false;
    }

    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Width = static_cast<UINT>(width);
    sd.BufferDesc.Height = static_cast<UINT>(height);
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    hr = factory->CreateSwapChain(m_device, &sd, &m_swap);
    if (FAILED(hr)) {
        log_error("CreateSwapChain fehlgeschlagen: " + hresult_text(hr));
        factory->Release();
        return false;
    }
    factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);
    factory->Release();

    if (!create_targets(width, height)) return false;
    if (!create_shaders()) return false;
    if (!create_cube()) return false;

    m_width = width;
    m_height = height;

    if (!m_ui.init(m_device, m_context, width, height)) {
        log_error("Ui-Overlay konnte nicht initialisiert werden");
        return false;
    }

    log_info("Renderer bereit");
    return true;
}

bool Renderer::create_shaders() {
    ID3DBlob* vs_blob = nullptr;
    ID3DBlob* ps_blob = nullptr;

    if (!compile_stage("VSMain", "vs_5_0", &vs_blob)) return false;
    if (!compile_stage("PSMain", "ps_5_0", &ps_blob)) {
        vs_blob->Release();
        return false;
    }

    HRESULT hr = m_device->CreateVertexShader(vs_blob->GetBufferPointer(), vs_blob->GetBufferSize(),
                                             nullptr, &m_vs);
    if (FAILED(hr)) {
        log_error("CreateVertexShader fehlgeschlagen: " + hresult_text(hr));
        vs_blob->Release();
        ps_blob->Release();
        return false;
    }

    hr = m_device->CreatePixelShader(ps_blob->GetBufferPointer(), ps_blob->GetBufferSize(), nullptr, &m_ps);
    ps_blob->Release();
    if (FAILED(hr)) {
        log_error("CreatePixelShader fehlgeschlagen: " + hresult_text(hr));
        vs_blob->Release();
        return false;
    }

    D3D11_INPUT_ELEMENT_DESC elems[] = {
        {"POSITION",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"NORMAL",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR",     0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD",  0, DXGI_FORMAT_R32G32_FLOAT,    0, 36, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };

    hr = m_device->CreateInputLayout(elems, 4, vs_blob->GetBufferPointer(), vs_blob->GetBufferSize(), &m_layout);
    vs_blob->Release();
    if (FAILED(hr)) {
        log_error("CreateInputLayout fehlgeschlagen: " + hresult_text(hr));
        return false;
    }

    D3D11_BUFFER_DESC cb{};
    cb.Usage = D3D11_USAGE_DYNAMIC;
    cb.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cb.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    cb.ByteWidth = sizeof(FrameCB);
    if (FAILED(m_device->CreateBuffer(&cb, nullptr, &m_frame_cb))) {
        log_error("FrameCB konnte nicht erstellt werden");
        return false;
    }

    D3D11_SAMPLER_DESC sd{};
    // Pixel-Schärfe (Etappe 14b): Point-Filterung mit harten Mip-Wechseln —
    // Texel bleiben auch aus der Ferne krisselig sichtbar (Retro-Look),
    // statt linear zu verschwimmen.
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    sd.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    sd.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    sd.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sd.MaxAnisotropy = 1;
    sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sd.MinLOD = 0.0f;
    sd.MaxLOD = D3D11_FLOAT32_MAX;
    if (FAILED(m_device->CreateSamplerState(&sd, &m_samp))) {
        log_error("Sampler konnte nicht erstellt werden");
        return false;
    }

    cb.ByteWidth = sizeof(ObjectCB);
    if (FAILED(m_device->CreateBuffer(&cb, nullptr, &m_object_cb))) {
        log_error("ObjectCB konnte nicht erstellt werden");
        return false;
    }

    // Himmel-Shader + Konstantpuffer + Tiefenzustand (Tiefe aus, immer zeichnen).
    if (!compile_stage("SkyVS", "vs_5_0", &vs_blob)) return false;
    if (!compile_stage("SkyPS", "ps_5_0", &ps_blob)) {
        vs_blob->Release();
        return false;
    }
    if (FAILED(m_device->CreateVertexShader(vs_blob->GetBufferPointer(), vs_blob->GetBufferSize(), nullptr, &m_sky_vs)) ||
        FAILED(m_device->CreatePixelShader(ps_blob->GetBufferPointer(), ps_blob->GetBufferSize(), nullptr, &m_sky_ps))) {
        log_error("Himmel-Shader konnte nicht erstellt werden");
        vs_blob->Release();
        ps_blob->Release();
        return false;
    }
    vs_blob->Release();
    ps_blob->Release();

    cb.ByteWidth = sizeof(SkyCB);
    if (FAILED(m_device->CreateBuffer(&cb, nullptr, &m_sky_cb))) {
        log_error("SkyCB konnte nicht erstellt werden");
        return false;
    }

    D3D11_DEPTH_STENCIL_DESC dss{};
    dss.DepthEnable = FALSE;
    dss.StencilEnable = FALSE;
    if (FAILED(m_device->CreateDepthStencilState(&dss, &m_sky_dss))) {
        log_error("Himmel-Tiefenzustand konnte nicht erstellt werden");
        return false;
    }

    // Schatten: Depth-Shader + Vergleichs-Sampler + 2048er-Tiefenkarte.
    if (!compile_stage("ShadowVS", "vs_5_0", &vs_blob)) return false;
    HRESULT shr = m_device->CreateVertexShader(vs_blob->GetBufferPointer(), vs_blob->GetBufferSize(),
                                               nullptr, &m_shadow_vs);
    vs_blob->Release();
    if (FAILED(shr)) {
        log_error("Schatten-Vertexshader konnte nicht erstellt werden");
        return false;
    }

    D3D11_SAMPLER_DESC ssd{};
    ssd.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
    ssd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    ssd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    ssd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    ssd.MaxAnisotropy = 1;
    ssd.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
    ssd.MinLOD = 0.0f;
    ssd.MaxLOD = D3D11_FLOAT32_MAX;
    if (FAILED(m_device->CreateSamplerState(&ssd, &m_shadow_samp))) {
        log_error("Schatten-Sampler konnte nicht erstellt werden");
        return false;
    }

    D3D11_TEXTURE2D_DESC std_{};
    std_.Width = 2048;
    std_.Height = 2048;
    std_.MipLevels = 1;
    std_.ArraySize = 1;
    std_.Format = DXGI_FORMAT_R32_TYPELESS;
    std_.SampleDesc.Count = 1;
    std_.Usage = D3D11_USAGE_DEFAULT;
    std_.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
    HRESULT hr0 = m_device->CreateTexture2D(&std_, nullptr, &m_shadow_tex);
    if (FAILED(hr0)) {
        log_error("Schatten-Tiefenkarte konnte nicht erstellt werden: " + hresult_text(hr0));
        return false;
    }
    D3D11_DEPTH_STENCIL_VIEW_DESC dvd{};
    dvd.Format = DXGI_FORMAT_D32_FLOAT;
    dvd.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    D3D11_SHADER_RESOURCE_VIEW_DESC svd{};
    svd.Format = DXGI_FORMAT_R32_FLOAT;
    svd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    svd.Texture2D.MipLevels = 1;
    if (FAILED(m_device->CreateDepthStencilView(m_shadow_tex, &dvd, &m_shadow_dsv)) ||
        FAILED(m_device->CreateShaderResourceView(m_shadow_tex, &svd, &m_shadow_srv))) {
        log_error("Schatten-Views konnten nicht erstellt werden");
        return false;
    }
    return true;
}

bool Renderer::create_cube() {
    std::vector<MeshVertex> verts;
    verts.reserve(24);
    for (const BoxVertex& v : kCubeVertices) {
        verts.push_back(MeshVertex{v.pos, v.nrm, Vec3{1.0f, 1.0f, 1.0f}, {v.uv[0], v.uv[1]}});
    }

    D3D11_BUFFER_DESC bd{};
    bd.Usage = D3D11_USAGE_IMMUTABLE;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.ByteWidth = static_cast<UINT>(sizeof(MeshVertex) * verts.size());
    D3D11_SUBRESOURCE_DATA vd{};
    vd.pSysMem = verts.data();
    if (FAILED(m_device->CreateBuffer(&bd, &vd, &m_vb))) {
        log_error("Vertexbuffer konnte nicht erstellt werden");
        return false;
    }

    D3D11_BUFFER_DESC ibd{};
    ibd.Usage = D3D11_USAGE_IMMUTABLE;
    ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    ibd.ByteWidth = sizeof(kCubeIndices);
    D3D11_SUBRESOURCE_DATA id{};
    id.pSysMem = kCubeIndices;
    if (FAILED(m_device->CreateBuffer(&ibd, &id, &m_ib))) {
        log_error("Indexbuffer konnte nicht erstellt werden");
        return false;
    }

    m_index_count = 36;
    return true;
}

bool Renderer::create_targets(int width, int height) {
    if (width <= 0 || height <= 0) return false;

    ID3D11Texture2D* back_buffer = nullptr;
    HRESULT hr = m_swap->GetBuffer(0, IID_ID3D11Texture2D, reinterpret_cast<void**>(&back_buffer));
    if (FAILED(hr) || !back_buffer) {
        log_error("Backbuffer nicht verfügbar: " + hresult_text(hr));
        return false;
    }
    hr = m_device->CreateRenderTargetView(back_buffer, nullptr, &m_rtv);
    back_buffer->Release();
    if (FAILED(hr)) {
        log_error("CreateRenderTargetView fehlgeschlagen: " + hresult_text(hr));
        return false;
    }

    D3D11_TEXTURE2D_DESC dd{};
    dd.Width = static_cast<UINT>(width);
    dd.Height = static_cast<UINT>(height);
    dd.MipLevels = 1;
    dd.ArraySize = 1;
    dd.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dd.SampleDesc.Count = 1;
    dd.Usage = D3D11_USAGE_DEFAULT;
    dd.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    hr = m_device->CreateTexture2D(&dd, nullptr, &m_depth_tex);
    if (FAILED(hr)) {
        log_error("Tiefentextur fehlgeschlagen: " + hresult_text(hr));
        return false;
    }
    hr = m_device->CreateDepthStencilView(m_depth_tex, nullptr, &m_dsv);
    if (FAILED(hr)) {
        log_error("CreateDepthStencilView fehlgeschlagen: " + hresult_text(hr));
        return false;
    }

    m_context->OMSetRenderTargets(1, &m_rtv, m_dsv);

    D3D11_VIEWPORT vp{};
    vp.Width = static_cast<float>(width);
    vp.Height = static_cast<float>(height);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    m_context->RSSetViewports(1, &vp);

    m_width = width;
    m_height = height;
    return true;
}

void Renderer::release_targets() {
    if (m_context) m_context->OMSetRenderTargets(0, nullptr, nullptr);
    if (m_dsv) { m_dsv->Release(); m_dsv = nullptr; }
    if (m_depth_tex) { m_depth_tex->Release(); m_depth_tex = nullptr; }
    if (m_rtv) { m_rtv->Release(); m_rtv = nullptr; }
}

void Renderer::resize(int width, int height) {
    if (!m_swap || width <= 0 || height <= 0) return;

    release_targets();
    HRESULT hr = m_swap->ResizeBuffers(0, static_cast<UINT>(width), static_cast<UINT>(height),
                                      DXGI_FORMAT_UNKNOWN, 0);
    if (FAILED(hr)) {
        log_error("ResizeBuffers fehlgeschlagen: " + hresult_text(hr));
        return;
    }
    create_targets(width, height);
    m_width = width;
    m_height = height;
    m_ui.resize(width, height);

    // Etappe 18j: Nach ResizeBuffers ist der Inhalt des Backbuffers UNDEFINIERT.
    // Beim Ziehen der Fensterkante (Windows-Modalschleife) pausiert unsere
    // Render-Schleife, deshalb blieben schwarze/garbage-Flächen stehen.
    // Einmal leeren + präsentieren liefert sofort ein sauberes Bild.
    if (m_rtv) {
        const float c[4] = {
            ((m_clear_rgb >> 16) & 0xFF) / 255.0f,
            ((m_clear_rgb >> 8) & 0xFF) / 255.0f,
            (m_clear_rgb & 0xFF) / 255.0f,
            1.0f,
        };
        m_context->ClearRenderTargetView(m_rtv, c);
        if (m_dsv) m_context->ClearDepthStencilView(
            m_dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
        m_swap->Present(0, 0);
    }
}

void Renderer::apply_pipeline() {
    const UINT stride = sizeof(MeshVertex);
    const UINT offset = 0;

    // Zustand zurücksetzen, den die UI-Schicht ändern kann (Blend, Tiefe, Raster).
    m_context->OMSetDepthStencilState(nullptr, 0);
    m_context->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
    m_context->RSSetState(nullptr);

    m_context->IASetInputLayout(m_layout);
    m_context->IASetVertexBuffers(0, 1, &m_vb, &stride, &offset);
    m_context->IASetIndexBuffer(m_ib, DXGI_FORMAT_R16_UINT, 0);
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_context->VSSetShader(m_vs, nullptr, 0);
    m_context->PSSetShader(m_ps, nullptr, 0);
    m_context->VSSetConstantBuffers(0, 1, &m_frame_cb);
    m_context->VSSetConstantBuffers(1, 1, &m_object_cb);
    m_context->PSSetConstantBuffers(0, 1, &m_frame_cb);  // gSun/gShadow werden im PS gebraucht
    m_context->PSSetConstantBuffers(1, 1, &m_object_cb);
    m_context->PSSetSamplers(0, 1, &m_samp);
    m_context->PSSetSamplers(1, 1, &m_shadow_samp);
    if (m_shadow_srv) m_context->PSSetShaderResources(1, 1, &m_shadow_srv);
}

void Renderer::begin_frame(const Mat4& view_proj, const Vec3& eye) {
    if (!m_context || !m_rtv) return;
    m_eye = eye;

    const float c[4] = {
        ((m_clear_rgb >> 16) & 0xFF) / 255.0f,
        ((m_clear_rgb >> 8) & 0xFF) / 255.0f,
        (m_clear_rgb & 0xFF) / 255.0f,
        1.0f,
    };
    m_context->ClearRenderTargetView(m_rtv, c);
    m_context->ClearDepthStencilView(m_dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (SUCCEEDED(m_context->Map(m_frame_cb, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
        FrameCB cb;
        std::memcpy(cb.view_proj, view_proj.m, sizeof(cb.view_proj));
        Vec3 sun = normalize(m_sun);
        cb.sun[0] = sun.x; cb.sun[1] = sun.y; cb.sun[2] = sun.z;
        cb.sun[3] = m_ambient;

        // Licht-Kamera: Sonne scheint auf die Terrainmitte, Ortho überdeckt
        // das ganze Gelände (bis 250 m Kantenlänge).
        Vec3 center{0.0f, 10.0f, 0.0f};
        Vec3 eye = add(center, scale(sun, 180.0f));
        Mat4 lview = look_at_lh(eye, center, Vec3{0.0f, 1.0f, 0.0f});
        Mat4 lproj = orthographic_lh(360.0f, 360.0f, 1.0f, 400.0f);
        m_light_vp = mul(lview, lproj);
        std::memcpy(cb.light_vp, m_light_vp.m, sizeof(cb.light_vp));
        cb.shadow[0] = (m_shadows_on && m_shadow_dsv && m_shadow_srv) ? 1.0f : 0.0f;
        cb.shadow[1] = 0.00035f;
        cb.shadow[2] = cb.shadow[3] = 0.0f;
        cb.fog[0] = m_fog_on ? 1.0f : 0.0f;
        cb.fog[1] = m_fog_far;
        cb.fog[2] = ((m_fog_rgb >> 16) & 0xFF) / 255.0f;
        cb.fog[3] = ((m_fog_rgb >> 8) & 0xFF) / 255.0f;
        cb.cam_pos[0] = m_eye.x; cb.cam_pos[1] = m_eye.y; cb.cam_pos[2] = m_eye.z;
        cb.cam_pos[3] = (m_fog_rgb & 0xFF) / 255.0f;
        std::memcpy(mapped.pData, &cb, sizeof(cb));
        m_context->Unmap(m_frame_cb, 0);
    }

    apply_pipeline();
    m_cmds.clear();
    m_ui.clear();
}

int Renderer::add_texture(const std::string& name, int w, int h, const uint8_t* rgba) {
    if (!m_device || w <= 0 || h <= 0 || !rgba) return -1;

    D3D11_TEXTURE2D_DESC td{};
    td.Width = static_cast<UINT>(w);
    td.Height = static_cast<UINT>(h);
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_IMMUTABLE;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA sd{};
    sd.pSysMem = rgba;
    sd.SysMemPitch = static_cast<UINT>(w) * 4;

    ID3D11Texture2D* tex = nullptr;
    if (FAILED(m_device->CreateTexture2D(&td, &sd, &tex)) || !tex) {
        log_error("Textur '" + name + "' konnte nicht erstellt werden");
        return -1;
    }

    ID3D11ShaderResourceView* srv = nullptr;
    HRESULT hr = m_device->CreateShaderResourceView(tex, nullptr, &srv);
    tex->Release();
    if (FAILED(hr) || !srv) {
        log_error("Textur-View '" + name + "' konnte nicht erstellt werden");
        return -1;
    }

    const int index = static_cast<int>(m_tex_srvs.size());
    m_tex_srvs.push_back(srv);
    m_tex_ids[name] = index;
    // Etappe 18c: Pixel für Tileset-Vorschau mitführen.
    m_tex_pixels.emplace_back(rgba, rgba + static_cast<size_t>(w) * h * 4);
    m_tex_sizes.emplace_back(w, h);
    log_info("Textur '" + name + "' registriert (" + std::to_string(w) + "x" + std::to_string(h) + ")");
    return index;
}

int Renderer::texture_index(const std::string& name) const {
    auto it = m_tex_ids.find(name);
    return it == m_tex_ids.end() ? -1 : it->second;
}

bool Renderer::texture_rgba(const std::string& name, std::vector<uint8_t>& out,
                           int& w, int& h) const {
    auto it = m_tex_ids.find(name);
    if (it == m_tex_ids.end()) return false;
    const int index = it->second;
    if (index < 0 || index >= static_cast<int>(m_tex_pixels.size())) return false;
    out = m_tex_pixels[index];
    w = m_tex_sizes[index].first;
    h = m_tex_sizes[index].second;
    return true;
}

// Etappe 18d: Atlas aller 64x64-Texturen. Zellen 64 px, Raster ~quadratisch;
// der Atlas meldet sich unter "atlas" an (F5 => neuer Slot, wie alle Texturen).
world::AtlasInfo Renderer::update_atlas() {
    world::AtlasInfo info;
    std::vector<std::pair<std::string, int>> entries;   // Name -> Pixelindex
    for (const auto& [name, idx] : m_tex_ids) {
        if (idx < 0 || idx >= static_cast<int>(m_tex_sizes.size())) continue;
        if (m_tex_sizes[idx].first == 64 && m_tex_sizes[idx].second == 64) {
            entries.emplace_back(name, idx);
        }
    }
    if (entries.empty()) return info;

    std::sort(entries.begin(), entries.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });
    const int n = static_cast<int>(entries.size());
    const int cols = std::max(1, static_cast<int>(std::ceil(std::sqrt(static_cast<double>(n)))));
    const int rows = (n + cols - 1) / cols;

    std::vector<uint8_t> px(static_cast<size_t>(cols * 64) * rows * 64 * 4, 0);
    for (int e = 0; e < n; ++e) {
        const auto& pixels = m_tex_pixels[entries[e].second];
        const int col = e % cols, row = e / cols;
        for (int y = 0; y < 64; ++y) {
            const size_t dst = (static_cast<size_t>(row * 64 + y) * cols * 64 + col * 64) * 4;
            std::memcpy(&px[dst], &pixels[static_cast<size_t>(y) * 64 * 4], 64 * 4);
        }
        info.cell_of[entries[e].first] = e;
    }

    info.tex_index = add_texture("atlas", cols * 64, rows * 64, px.data());
    info.cols = cols;
    info.rows = rows;
    info.valid = info.tex_index >= 0;
    if (info.valid) {
        log_info("Atlas gebaut: " + std::to_string(n) + " Kacheln, " +
                 std::to_string(cols * 64) + "x" + std::to_string(rows * 64) + " px");
    }
    return info;
}

bool Renderer::set_terrain(const std::vector<MeshVertex>& verts, const std::vector<uint32_t>& indices) {
    clear_terrain();
    if (!m_device || verts.empty() || indices.empty()) return false;

    D3D11_BUFFER_DESC bd{};
    bd.Usage = D3D11_USAGE_IMMUTABLE;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.ByteWidth = static_cast<UINT>(sizeof(MeshVertex) * verts.size());
    D3D11_SUBRESOURCE_DATA vd{};
    vd.pSysMem = verts.data();
    if (FAILED(m_device->CreateBuffer(&bd, &vd, &m_terrain_vb))) {
        log_error("Gelände-Vertexbuffer konnte nicht erstellt werden");
        return false;
    }

    D3D11_BUFFER_DESC ibd{};
    ibd.Usage = D3D11_USAGE_IMMUTABLE;
    ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    ibd.ByteWidth = static_cast<UINT>(sizeof(uint32_t) * indices.size());
    D3D11_SUBRESOURCE_DATA id{};
    id.pSysMem = indices.data();
    if (FAILED(m_device->CreateBuffer(&ibd, &id, &m_terrain_ib))) {
        log_error("Gelände-Indexbuffer konnte nicht erstellt werden");
        clear_terrain();
        return false;
    }

    m_terrain_index_count = static_cast<int>(indices.size());
    return true;
}

void Renderer::clear_terrain() {
    m_terrain_index_count = 0;
    if (m_terrain_ib) { m_terrain_ib->Release(); m_terrain_ib = nullptr; }
    if (m_terrain_vb) { m_terrain_vb->Release(); m_terrain_vb = nullptr; }
}

bool Renderer::save_screenshot(const std::string& path) {
    if (!m_swap || !m_device || !m_context) return false;

    ID3D11Texture2D* back = nullptr;
    if (FAILED(m_swap->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&back))) || !back) {
        log_error("Screenshot: Backbuffer nicht verfügbar");
        return false;
    }

    D3D11_TEXTURE2D_DESC desc{};
    back->GetDesc(&desc);

    D3D11_TEXTURE2D_DESC sd = desc;
    sd.Usage = D3D11_USAGE_STAGING;
    sd.BindFlags = 0;
    sd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    sd.MiscFlags = 0;
    sd.MipLevels = 1;
    sd.ArraySize = 1;

    ID3D11Texture2D* staging = nullptr;
    if (FAILED(m_device->CreateTexture2D(&sd, nullptr, &staging)) || !staging) {
        back->Release();
        log_error("Screenshot: Staging-Textur fehlgeschlagen");
        return false;
    }

    m_context->CopyResource(staging, back);
    back->Release();

    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(m_context->Map(staging, 0, D3D11_MAP_READ, 0, &mapped))) {
        staging->Release();
        log_error("Screenshot: Map fehlgeschlagen");
        return false;
    }

    const int w = static_cast<int>(desc.Width);
    const int h = static_cast<int>(desc.Height);
    const bool swizzle = (desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM);

    BITMAPFILEHEADER fh{};
    BITMAPINFOHEADER ih{};
    const uint32_t row_bytes = static_cast<uint32_t>(w) * 4;
    fh.bfType = 0x4D42; // "BM"
    fh.bfOffBits = sizeof(fh) + sizeof(ih);
    fh.bfSize = fh.bfOffBits + row_bytes * static_cast<uint32_t>(h);
    ih.biSize = sizeof(ih);
    ih.biWidth = w;
    ih.biHeight = -h; // top-down
    ih.biPlanes = 1;
    ih.biBitCount = 32;
    ih.biCompression = BI_RGB;
    ih.biSizeImage = row_bytes * static_cast<uint32_t>(h);

    bool ok = false;
    if (std::FILE* f = std::fopen(path.c_str(), "wb")) {
        std::fwrite(&fh, sizeof(fh), 1, f);
        std::fwrite(&ih, sizeof(ih), 1, f);

        std::vector<uint8_t> row(row_bytes);
        for (int y = 0; y < h; ++y) {
            const uint8_t* src = static_cast<const uint8_t*>(mapped.pData) +
                                 static_cast<size_t>(y) * mapped.RowPitch;
            if (swizzle) {
                for (int x = 0; x < w; ++x) {
                    row[x * 4 + 0] = src[x * 4 + 2];
                    row[x * 4 + 1] = src[x * 4 + 1];
                    row[x * 4 + 2] = src[x * 4 + 0];
                    row[x * 4 + 3] = 255;
                }
                std::fwrite(row.data(), 1, row_bytes, f);
            } else {
                std::fwrite(src, 1, row_bytes, f);
            }
        }
        std::fclose(f);
        ok = true;
    }

    m_context->Unmap(staging, 0);
    staging->Release();

    if (ok) log_info("Screenshot gespeichert: " + path);
    else log_error("Screenshot: Datei konnte nicht geschrieben werden");
    return ok;
}

void Renderer::push_box(const Vec3& pos, const Vec3& size, const Vec3& rot, uint32_t rgb,
                        int tex, float tile) {
    DrawCmd cmd{};
    cmd.pos = pos;
    cmd.size = size;
    cmd.rot = rot;
    cmd.color = Vec4{((rgb >> 16) & 0xFF) / 255.0f,
                     ((rgb >> 8) & 0xFF) / 255.0f,
                     (rgb & 0xFF) / 255.0f,
                     1.0f};
    cmd.tex = tex;
    cmd.tile = tile;
    m_cmds.push_back(cmd);
}

void Renderer::end_frame() {
    if (!m_context) return;

    // --- Schatten-Pass: Szene aus Sicht der Sonne in die Tiefenkarte ---
    if (m_shadows_on && m_shadow_dsv && m_shadow_vs &&
        (m_terrain_index_count > 0 || !m_cmds.empty())) {
        D3D11_VIEWPORT svp{0.0f, 0.0f, 2048.0f, 2048.0f, 0.0f, 1.0f};
        m_context->RSSetViewports(1, &svp);
        m_context->OMSetRenderTargets(0, nullptr, m_shadow_dsv);
        m_context->ClearDepthStencilView(m_shadow_dsv, D3D11_CLEAR_DEPTH, 1.0f, 0);
        m_context->VSSetShader(m_shadow_vs, nullptr, 0);
        m_context->PSSetShader(nullptr, nullptr, 0);
        m_context->VSSetConstantBuffers(0, 1, &m_frame_cb);
        m_context->VSSetConstantBuffers(1, 1, &m_object_cb);
        m_context->IASetInputLayout(m_layout);
        m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        const UINT stride = sizeof(MeshVertex);
        const UINT offset = 0;

        auto draw_object = [&](const Mat4& world) {
            D3D11_MAPPED_SUBRESOURCE m2{};
            if (FAILED(m_context->Map(m_object_cb, 0, D3D11_MAP_WRITE_DISCARD, 0, &m2))) return;
            ObjectCB cb;
            std::memcpy(cb.world, world.m, sizeof(cb.world));
            cb.color[0] = cb.color[1] = cb.color[2] = cb.color[3] = 1.0f;
            cb.size[0] = cb.size[1] = cb.size[2] = cb.size[3] = 0.0f;
            cb.params[0] = cb.params[1] = cb.params[2] = cb.params[3] = 0.0f;
            std::memcpy(m2.pData, &cb, sizeof(cb));
            m_context->Unmap(m_object_cb, 0);
        };

        if (m_terrain_vb && m_terrain_ib && m_terrain_index_count > 0) {
            m_context->IASetVertexBuffers(0, 1, &m_terrain_vb, &stride, &offset);
            m_context->IASetIndexBuffer(m_terrain_ib, DXGI_FORMAT_R32_UINT, 0);
            draw_object(identity());
            m_context->DrawIndexed(static_cast<UINT>(m_terrain_index_count), 0, 0);
        }
        m_context->IASetVertexBuffers(0, 1, &m_vb, &stride, &offset);
        m_context->IASetIndexBuffer(m_ib, DXGI_FORMAT_R16_UINT, 0);
        for (const DrawCmd& cmd : m_cmds) {
            const Mat4 world = mul(mul(scaling(cmd.size.x, cmd.size.y, cmd.size.z),
                                       rotation_roll_pitch_yaw(cmd.rot.x, cmd.rot.y, cmd.rot.z)),
                                   translation(cmd.pos.x, cmd.pos.y, cmd.pos.z));
            draw_object(world);
            m_context->DrawIndexed(static_cast<UINT>(m_index_count), 0, 0);
        }

        // Haupt-Renderziel + Pipeline zurück.
        m_context->OMSetRenderTargets(1, &m_rtv, m_dsv);
        D3D11_VIEWPORT vp{0.0f, 0.0f, static_cast<float>(m_width),
                          static_cast<float>(m_height), 0.0f, 1.0f};
        m_context->RSSetViewports(1, &vp);
        apply_pipeline();
    }

    // Himmel zürst: Fullscreen-Dreieck ohne Tiefentest, danach normale Pipeline.
    if (m_sky_on && m_sky_vs && m_sky_ps) {
        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (SUCCEEDED(m_context->Map(m_sky_cb, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
            SkyCB cb;
            cb.top[0] = ((m_sky_top >> 16) & 0xFF) / 255.0f;
            cb.top[1] = ((m_sky_top >> 8) & 0xFF) / 255.0f;
            cb.top[2] = (m_sky_top & 0xFF) / 255.0f;
            cb.top[3] = 1.0f;
            cb.bottom[0] = ((m_sky_bottom >> 16) & 0xFF) / 255.0f;
            cb.bottom[1] = ((m_sky_bottom >> 8) & 0xFF) / 255.0f;
            cb.bottom[2] = (m_sky_bottom & 0xFF) / 255.0f;
            cb.bottom[3] = 1.0f;
            std::memcpy(mapped.pData, &cb, sizeof(cb));
            m_context->Unmap(m_sky_cb, 0);
        }
        m_context->OMSetDepthStencilState(m_sky_dss, 0);
        m_context->VSSetShader(m_sky_vs, nullptr, 0);
        m_context->PSSetShader(m_sky_ps, nullptr, 0);
        m_context->VSSetConstantBuffers(0, 1, &m_sky_cb);
        m_context->PSSetConstantBuffers(0, 1, &m_sky_cb);
        m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        m_context->Draw(3, 0);

        // Szenen-Pipeline zurück (Shader, CBs, Tiefe).
        apply_pipeline();
    }

    // Gelände zürst (eigenes Mesh, Weltmatrix = Identität, Farbe weiss).
    if (m_terrain_vb && m_terrain_ib && m_terrain_index_count > 0) {
        const UINT stride = sizeof(MeshVertex);
        const UINT offset = 0;
        m_context->IASetVertexBuffers(0, 1, &m_terrain_vb, &stride, &offset);
        m_context->IASetIndexBuffer(m_terrain_ib, DXGI_FORMAT_R32_UINT, 0);

        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (SUCCEEDED(m_context->Map(m_object_cb, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
            ObjectCB cb;
            const Mat4 ident = identity();
            std::memcpy(cb.world, ident.m, sizeof(cb.world));
            cb.color[0] = cb.color[1] = cb.color[2] = cb.color[3] = 1.0f;
            cb.size[0] = cb.size[1] = cb.size[2] = cb.size[3] = 0.0f;
            cb.params[0] = static_cast<float>(m_terrain_tex);  // UV steckt in den Vertices
            cb.params[1] = 0.0f;
            // Etappe 18c: Kachelgrösse des Bodens (Terrain-UV ist auf
            // 2 m gebacken; Faktor 2/tile skaliert auf die Wunschgrösse).
            cb.params[2] = 2.0f / (m_terrain_tile > 0.0f ? m_terrain_tile : 2.0f);
            cb.params[3] = 0.0f;
            std::memcpy(mapped.pData, &cb, sizeof(cb));
            m_context->Unmap(m_object_cb, 0);
        }
        if (m_terrain_tex >= 0 && m_terrain_tex < static_cast<int>(m_tex_srvs.size())) {
            ID3D11ShaderResourceView* srv = m_tex_srvs[m_terrain_tex];
            m_context->PSSetShaderResources(0, 1, &srv);
        }
        m_context->DrawIndexed(static_cast<UINT>(m_terrain_index_count), 0, 0);

        // Box-Puffer wieder binden
        m_context->IASetVertexBuffers(0, 1, &m_vb, &stride, &offset);
        m_context->IASetIndexBuffer(m_ib, DXGI_FORMAT_R16_UINT, 0);
    }

    int bound_tex = -2;  // ungültig, damit der erste Draw bindet
    for (const DrawCmd& cmd : m_cmds) {
        const Mat4 world = mul(mul(scaling(cmd.size.x, cmd.size.y, cmd.size.z),
                                   rotation_roll_pitch_yaw(cmd.rot.x, cmd.rot.y, cmd.rot.z)),
                               translation(cmd.pos.x, cmd.pos.y, cmd.pos.z));

        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (FAILED(m_context->Map(m_object_cb, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) continue;
        ObjectCB cb;
        std::memcpy(cb.world, world.m, sizeof(cb.world));
        cb.color[0] = cmd.color.x;
        cb.color[1] = cmd.color.y;
        cb.color[2] = cmd.color.z;
        cb.color[3] = cmd.color.w;
        cb.size[0] = cmd.size.x;
        cb.size[1] = cmd.size.y;
        cb.size[2] = cmd.size.z;
        cb.size[3] = 0.0f;
        cb.params[0] = static_cast<float>(cmd.tex);
        cb.params[1] = cmd.tile;
        cb.params[2] = cb.params[3] = 0.0f;
        std::memcpy(mapped.pData, &cb, sizeof(cb));
        m_context->Unmap(m_object_cb, 0);

        if (cmd.tex >= 0 && cmd.tex < static_cast<int>(m_tex_srvs.size()) && cmd.tex != bound_tex) {
            ID3D11ShaderResourceView* srv = m_tex_srvs[cmd.tex];
            m_context->PSSetShaderResources(0, 1, &srv);
            bound_tex = cmd.tex;
        }

        m_context->DrawIndexed(static_cast<UINT>(m_index_count), 0, 0);
    }

    // 2D-Overlay (Rechtecke + Text) ganz zum Schluss
    m_ui.render();

    if (!m_shot_path.empty()) {
        const std::string path = m_shot_path;
        m_shot_path.clear();
        save_screenshot(path);
    }

    if (m_swap) m_swap->Present(1, 0);
}

void Renderer::shutdown() {
    release_targets();
    m_ui.shutdown();
    clear_terrain();
    if (m_shadow_srv) { m_shadow_srv->Release(); m_shadow_srv = nullptr; }
    if (m_shadow_dsv) { m_shadow_dsv->Release(); m_shadow_dsv = nullptr; }
    if (m_shadow_tex) { m_shadow_tex->Release(); m_shadow_tex = nullptr; }
    if (m_shadow_samp) { m_shadow_samp->Release(); m_shadow_samp = nullptr; }
    if (m_shadow_vs) { m_shadow_vs->Release(); m_shadow_vs = nullptr; }
    for (ID3D11ShaderResourceView* srv : m_tex_srvs) if (srv) srv->Release();
    m_tex_srvs.clear();
    m_tex_ids.clear();
    if (m_sky_dss) { m_sky_dss->Release(); m_sky_dss = nullptr; }
    if (m_sky_cb) { m_sky_cb->Release(); m_sky_cb = nullptr; }
    if (m_sky_ps) { m_sky_ps->Release(); m_sky_ps = nullptr; }
    if (m_sky_vs) { m_sky_vs->Release(); m_sky_vs = nullptr; }
    if (m_samp) { m_samp->Release(); m_samp = nullptr; }
    if (m_ib) { m_ib->Release(); m_ib = nullptr; }
    if (m_vb) { m_vb->Release(); m_vb = nullptr; }
    if (m_object_cb) { m_object_cb->Release(); m_object_cb = nullptr; }
    if (m_frame_cb) { m_frame_cb->Release(); m_frame_cb = nullptr; }
    if (m_layout) { m_layout->Release(); m_layout = nullptr; }
    if (m_ps) { m_ps->Release(); m_ps = nullptr; }
    if (m_vs) { m_vs->Release(); m_vs = nullptr; }
    if (m_swap) { m_swap->Release(); m_swap = nullptr; }
    if (m_context) { m_context->Release(); m_context = nullptr; }
    if (m_device) { m_device->Release(); m_device = nullptr; }
}

} // namespace rune::gfx
