#include "TransparentRenderer.h"
#include <array>
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Embedded HLSL shaders for wave effect
static const char* g_waveVS = R"(
struct VS_INPUT {
    float3 pos : POSITION;
    float4 color : COLOR;
};
struct PS_INPUT {
    float4 pos : SV_POSITION;
    float4 color : COLOR;
};
PS_INPUT main(VS_INPUT input) {
    PS_INPUT output;
    output.pos = float4(input.pos, 1.0);
    output.color = input.color;
    return output;
}
)";

static const char* g_wavePS = R"(
struct PS_INPUT {
    float4 pos : SV_POSITION;
    float4 color : COLOR;
};
float4 main(PS_INPUT input) : SV_TARGET {
    return input.color;
}
)";

TransparentRenderer::TransparentRenderer()
    : m_hwnd(nullptr), m_width(TARGET_WIDTH), m_height(TARGET_HEIGHT), 
      m_clearColor{0.01f, 0.01f, 0.02f, 0.25f}, m_targetHz(TARGET_REFRESH_HZ)
{
    m_lastFrameTime = std::chrono::high_resolution_clock::now();
    m_waveVertices.resize((WAVE_SEGMENTS + 1) * 2);
}

TransparentRenderer::~TransparentRenderer()
{
    cleanupSwapChain();
}

bool TransparentRenderer::initialize(HWND hwnd)
{
    if (!hwnd) return false;
    m_hwnd = hwnd;

    // Default to 4K UHD @ 540Hz
    m_width = TARGET_WIDTH;
    m_height = TARGET_HEIGHT;

    RECT rc{};
    GetClientRect(hwnd, &rc);
    if (rc.right > 0 && rc.bottom > 0) {
        m_width = rc.right - rc.left;
        m_height = rc.bottom - rc.top;
    }

    if (!createDevice()) return false;
    if (!createSwapChain(m_width, m_height)) return false;
    if (!createCompositionTarget()) return false;
    if (!createRenderTargetView()) return false;
    if (!createWaveResources()) return false;
    createD2DResources();

    enableGlassEffect();
    return true;
}

void TransparentRenderer::setTargetResolution(UINT w, UINT h)
{
    if (w > 0 && h > 0) {
        resize(w, h);
    }
}

void TransparentRenderer::resize(UINT width, UINT height)
{
    if (!m_swapChain) return;
    if (width == 0 || height == 0) return;

    m_width = width;
    m_height = height;

    // Release render target before resize
    m_rtv.Reset();
    m_context->OMSetRenderTargets(0, nullptr, nullptr);
    releaseD2DTarget();
    
    HRESULT hr = m_swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
    if (SUCCEEDED(hr)) {
        createRenderTargetView();
    }
}

// ============================================================================
// HSV to RGB conversion for chromatic color cycling
// ============================================================================
void TransparentRenderer::hsvToRgb(float h, float s, float v, float& r, float& g, float& b)
{
    h = fmodf(h, 360.0f);
    if (h < 0) h += 360.0f;
    
    float c = v * s;
    float x = c * (1.0f - fabsf(fmodf(h / 60.0f, 2.0f) - 1.0f));
    float m = v - c;
    
    if (h < 60)       { r = c; g = x; b = 0; }
    else if (h < 120) { r = x; g = c; b = 0; }
    else if (h < 180) { r = 0; g = c; b = x; }
    else if (h < 240) { r = 0; g = x; b = c; }
    else if (h < 300) { r = x; g = 0; b = c; }
    else              { r = c; g = 0; b = x; }
    
    r += m; g += m; b += m;
}

void TransparentRenderer::getChromaticColor(float timeOffset, float& r, float& g, float& b)
{
    float hue = fmodf((float)m_animationTime * m_chromaConfig.hueSpeed + timeOffset, 360.0f);
    hsvToRgb(hue, m_chromaConfig.saturation, m_chromaConfig.brightness, r, g, b);
    
    // Apply neon glow boost
    float glow = m_chromaConfig.neonGlow;
    r = fminf(r * glow, 1.0f);
    g = fminf(g * glow, 1.0f);
    b = fminf(b * glow, 1.0f);
}

COLORREF TransparentRenderer::getChromaticColorRef(float timeOffset)
{
    float r, g, b;
    getChromaticColor(timeOffset, r, g, b);
    return RGB((int)(r * 255), (int)(g * 255), (int)(b * 255));
}

// ============================================================================
// Wave background rendering with D3D11
// ============================================================================
void TransparentRenderer::updateWaveVertices()
{
    float basePhase = (float)(m_animationTime * m_waveConfig.speed * 2.0 * M_PI);
    
    for (int i = 0; i <= WAVE_SEGMENTS; ++i) {
        float t = (float)i / WAVE_SEGMENTS;
        float x = t * 2.0f - 1.0f; // NDC x: -1 to 1
        float waveY = m_waveConfig.amplitude * sinf(t * m_waveConfig.frequency * 2.0f * (float)M_PI + basePhase);
        
        // Top vertex of strip
        m_waveVertices[i * 2].x = x;
        m_waveVertices[i * 2].y = waveY;
        m_waveVertices[i * 2].z = 0.0f;
        
        // Bottom vertex of strip (goes to bottom of screen)
        m_waveVertices[i * 2 + 1].x = x;
        m_waveVertices[i * 2 + 1].y = -1.0f;
        m_waveVertices[i * 2 + 1].z = 0.0f;
    }
}

void TransparentRenderer::renderSingleWaveLayer(int layer)
{
    // Get chromatic color for this layer
    float r, g, b;
    getChromaticColor(layer * 60.0f, r, g, b);
    float alpha = 0.15f - (layer * 0.025f);
    
    // Update vertex colors
    for (auto& v : m_waveVertices) {
        v.r = r;
        v.g = g;
        v.b = b;
        v.a = alpha;
    }
    
    // Offset the wave phase for this layer
    float layerPhase = layer * m_waveConfig.phaseOffset;
    float basePhase = (float)(m_animationTime * m_waveConfig.speed * 2.0 * M_PI) + layerPhase;
    
    for (int i = 0; i <= WAVE_SEGMENTS; ++i) {
        float t = (float)i / WAVE_SEGMENTS;
        float waveY = m_waveConfig.amplitude * (0.8f - layer * 0.15f) * 
                      sinf(t * m_waveConfig.frequency * 2.0f * (float)M_PI + basePhase);
        m_waveVertices[i * 2].y = waveY - 0.2f * layer;
    }
    
    // Update vertex buffer
    D3D11_MAPPED_SUBRESOURCE mapped;
    if (SUCCEEDED(m_context->Map(m_waveVB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
        memcpy(mapped.pData, m_waveVertices.data(), m_waveVertices.size() * sizeof(WaveVertex));
        m_context->Unmap(m_waveVB.Get(), 0);
    }
    
    // Draw
    UINT stride = sizeof(WaveVertex);
    UINT offset = 0;
    m_context->IASetVertexBuffers(0, 1, m_waveVB.GetAddressOf(), &stride, &offset);
    m_context->Draw((UINT)m_waveVertices.size(), 0);
}

void TransparentRenderer::renderWaveBackground()
{
    if (!m_waveVS || !m_wavePS || !m_waveVB) return;
    
    // Set shaders
    m_context->VSSetShader(m_waveVS.Get(), nullptr, 0);
    m_context->PSSetShader(m_wavePS.Get(), nullptr, 0);
    m_context->IASetInputLayout(m_waveLayout.Get());
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    
    // Enable alpha blending
    float blendFactor[4] = {0, 0, 0, 0};
    m_context->OMSetBlendState(m_blendState.Get(), blendFactor, 0xFFFFFFFF);
    
    // Render each wave layer
    for (int layer = 0; layer < m_waveConfig.layers; ++layer) {
        renderSingleWaveLayer(layer);
    }
}

// ============================================================================
// Chromatic/Neon text rendering via GDI
// ============================================================================
void TransparentRenderer::renderChromaticText(HDC hdc, const wchar_t* text, int x, int y, int fontSize)
{
    if (!hdc || !text) return;
    
    // Create neon font
    HFONT font = CreateFontW(
        fontSize, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_MODERN, L"Consolas");
    
    HFONT oldFont = (HFONT)SelectObject(hdc, font);
    SetBkMode(hdc, TRANSPARENT);
    
    int shift = (int)(m_chromaConfig.chromaticShift * fontSize);
    
    // Chromatic aberration: draw R, G, B channels with offset
    // Red channel (left)
    SetTextColor(hdc, getChromaticColorRef(0.0f) & 0x0000FF);
    TextOutW(hdc, x - shift, y, text, (int)wcslen(text));
    
    // Green channel (center)
    SetTextColor(hdc, getChromaticColorRef(120.0f) & 0x00FF00);
    TextOutW(hdc, x, y, text, (int)wcslen(text));
    
    // Blue channel (right)
    SetTextColor(hdc, getChromaticColorRef(240.0f) & 0xFF0000);
    TextOutW(hdc, x + shift, y, text, (int)wcslen(text));
    
    // Main neon color on top
    SetTextColor(hdc, getChromaticColorRef(0.0f));
    TextOutW(hdc, x, y, text, (int)wcslen(text));
    
    SelectObject(hdc, oldFont);
    DeleteObject(font);
}

// ============================================================================
// Main render loop - optimized for 540Hz
// ============================================================================
void TransparentRenderer::render()
{
    if (!m_context || !m_rtv) return;
    
    // Update animation time with high precision
    auto now = std::chrono::high_resolution_clock::now();
    double deltaTime = std::chrono::duration<double>(now - m_lastFrameTime).count();
    m_lastFrameTime = now;
    m_animationTime += deltaTime;

    // Clear with transparent dark background
    const float clear[4] = {m_clearColor[0], m_clearColor[1], m_clearColor[2], m_clearColor[3]};
    m_context->OMSetRenderTargets(1, m_rtv.GetAddressOf(), nullptr);
    m_context->ClearRenderTargetView(m_rtv.Get(), clear);
    
    // Set viewport
    D3D11_VIEWPORT vp = {};
    vp.Width = (float)m_width;
    vp.Height = (float)m_height;
    vp.MaxDepth = 1.0f;
    m_context->RSSetViewports(1, &vp);
    m_context->RSSetState(m_rasterizerState.Get());
    
    // Render wave background
    renderWaveBackground();

    // Overlay GPU text surface if available
    renderEditorOverlay();
    
    // Present with minimal latency (no vsync for 540Hz+)
    m_swapChain->Present(0, 0);
    if (m_dcompDevice) m_dcompDevice->Commit();
}

void TransparentRenderer::setClearColor(float r, float g, float b, float a)
{
    m_clearColor[0] = r;
    m_clearColor[1] = g;
    m_clearColor[2] = b;
    m_clearColor[3] = a;
}

void TransparentRenderer::updateEditorText(const std::wstring& text, const RECT& editorRect,
                                           size_t caretIndex, size_t caretLine, size_t caretColumn)
{
    std::lock_guard<std::mutex> lock(m_editorMutex);
    m_editorText = text;
    m_editorRect = editorRect;
    size_t limited = std::min<size_t>(caretIndex, m_editorText.length());
    m_caretIndex = limited;
    m_caretLine = caretLine;
    m_caretColumn = caretColumn;
    m_layoutDirty = true;
    m_editorDirty = true;
}

bool TransparentRenderer::createD2DResources()
{
    if (!m_device) return false;

    if (!m_d2dFactory) {
        D2D1_FACTORY_OPTIONS options{};
#if defined(_DEBUG)
        options.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif
        if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_MULTI_THREADED, 
                                     __uuidof(ID2D1Factory1), 
                                     &options, 
                                     reinterpret_cast<void**>(m_d2dFactory.GetAddressOf())))) {
            return false;
        }
    }

    Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
    if (FAILED(m_device.As(&dxgiDevice))) {
        return false;
    }

    if (!m_d2dDevice) {
        if (FAILED(m_d2dFactory->CreateDevice(dxgiDevice.Get(), &m_d2dDevice))) {
            return false;
        }
    }

    if (!m_d2dContext) {
        if (FAILED(m_d2dDevice->CreateDeviceContext(
                D2D1_DEVICE_CONTEXT_OPTIONS_ENABLE_MULTITHREADED_OPTIMIZATIONS,
                &m_d2dContext))) {
            return false;
        }
        m_d2dContext->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
    }

    if (!m_dwriteFactory) {
        if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
                                       __uuidof(IDWriteFactory),
                                       reinterpret_cast<IUnknown**>(m_dwriteFactory.GetAddressOf())))) {
            return false;
        }
    }

    if (!m_textFormat && m_dwriteFactory) {
        if (FAILED(m_dwriteFactory->CreateTextFormat(
                L"Consolas",
                nullptr,
                DWRITE_FONT_WEIGHT_SEMI_LIGHT,
                DWRITE_FONT_STYLE_NORMAL,
                DWRITE_FONT_STRETCH_NORMAL,
                18.0f,
                L"en-us",
                &m_textFormat))) {
            return false;
        }
        m_textFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        m_textFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
        m_textFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        m_textFormat->SetLineSpacing(DWRITE_LINE_SPACING_METHOD_UNIFORM, 20.0f, 0.0f);
    }

    if (m_d2dContext) {
        if (!m_textBrush) {
            m_d2dContext->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.95f), &m_textBrush);
        }
        if (!m_caretBrush) {
            m_d2dContext->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.85f), &m_caretBrush);
        }
        if (!m_backgroundBrush) {
            m_d2dContext->CreateSolidColorBrush(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f), &m_backgroundBrush);
        }
    }

    createD2DTargetBitmap();
    return true;
}

bool TransparentRenderer::createD2DTargetBitmap()
{
    if (!m_d2dContext || !m_swapChain) {
        return false;
    }

    releaseD2DTarget();

    Microsoft::WRL::ComPtr<IDXGISurface> surface;
    HRESULT hr = m_swapChain->GetBuffer(0, IID_PPV_ARGS(&surface));
    if (FAILED(hr)) {
        return false;
    }

    D2D1_BITMAP_PROPERTIES1 props = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
        96.0f,
        96.0f);

    hr = m_d2dContext->CreateBitmapFromDxgiSurface(surface.Get(), &props, &m_d2dTargetBitmap);
    if (FAILED(hr)) {
        return false;
    }

    m_d2dContext->SetTarget(m_d2dTargetBitmap.Get());
    return true;
}

void TransparentRenderer::releaseD2DTarget()
{
    if (m_d2dContext) {
        m_d2dContext->SetTarget(nullptr);
        m_d2dContext->Flush();
    }
    m_d2dTargetBitmap.Reset();
    m_layoutDirty = true;
}

bool TransparentRenderer::rebuildTextLayout(const std::wstring& text, const RECT& editorRect)
{
    if (!m_dwriteFactory || !m_textFormat) {
        return false;
    }

    const LONG widthPx = std::max<LONG>(1, editorRect.right - editorRect.left);
    const LONG heightPx = std::max<LONG>(1, editorRect.bottom - editorRect.top);

    const wchar_t* buffer = text.empty() ? L"" : text.c_str();
    Microsoft::WRL::ComPtr<IDWriteTextLayout> layout;
    HRESULT hr = m_dwriteFactory->CreateTextLayout(
        buffer,
        static_cast<UINT32>(text.length()),
        m_textFormat.Get(),
        static_cast<FLOAT>(widthPx),
        static_cast<FLOAT>(heightPx),
        &layout);
    if (FAILED(hr)) {
        m_textLayout.Reset();
        return false;
    }

    m_textLayout = layout;
    m_layoutDirty = false;
    return true;
}

void TransparentRenderer::updateTextBrushColor()
{
    if (!m_textBrush) {
        return;
    }
    float r, g, b;
    getChromaticColor(0.0f, r, g, b);
    m_textBrush->SetColor(D2D1::ColorF(r, g, b, 0.95f));
}

void TransparentRenderer::renderEditorOverlay()
{
    if (!m_d2dContext || !m_d2dTargetBitmap || !m_textFormat) {
        return;
    }

    RECT rectCopy{};
    size_t caretIndex = 0;
    size_t textLength = 0;
    bool layoutDirty = false;
    std::wstring textForLayout;

    {
        std::lock_guard<std::mutex> lock(m_editorMutex);
        rectCopy = m_editorRect;
        caretIndex = m_caretIndex;
        textLength = m_editorText.length();
        layoutDirty = m_layoutDirty;
        if (layoutDirty) {
            textForLayout = m_editorText;
            textLength = textForLayout.length();
        }
    }

    if (rectCopy.right <= rectCopy.left || rectCopy.bottom <= rectCopy.top) {
        return;
    }

    if (layoutDirty) {
        rebuildTextLayout(textForLayout, rectCopy);
    }

    if (!m_textLayout || !m_textBrush) {
        return;
    }

    updateTextBrushColor();

    const FLOAT originX = static_cast<FLOAT>(rectCopy.left) + 12.0f;
    const FLOAT originY = static_cast<FLOAT>(rectCopy.top) + 10.0f;

    m_d2dContext->BeginDraw();
    m_d2dContext->SetTransform(D2D1::Matrix3x2F::Identity());

    D2D1_RECT_F clipRect = D2D1::RectF(
        static_cast<FLOAT>(rectCopy.left),
        static_cast<FLOAT>(rectCopy.top),
        static_cast<FLOAT>(rectCopy.right),
        static_cast<FLOAT>(rectCopy.bottom));
    m_d2dContext->PushAxisAlignedClip(clipRect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

    if (m_backgroundBrush) {
        m_backgroundBrush->SetColor(D2D1::ColorF(0.01f, 0.01f, 0.05f, 0.32f));
        m_d2dContext->FillRectangle(clipRect, m_backgroundBrush.Get());
    }

    m_d2dContext->DrawTextLayout(D2D1::Point2F(originX, originY),
                                 m_textLayout.Get(),
                                 m_textBrush.Get(),
                                 D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);

    if (m_caretBrush) {
        FLOAT caretX = 0.0f;
        FLOAT caretY = 0.0f;
        DWRITE_HIT_TEST_METRICS metrics{};
        size_t caretPos = std::min<size_t>(caretIndex, textLength);
        if (SUCCEEDED(m_textLayout->HitTestTextPosition(static_cast<UINT32>(caretPos), FALSE,
                                                        &caretX, &caretY, &metrics))) {
            D2D1_RECT_F caretRect = D2D1::RectF(
                originX + caretX,
                originY + caretY,
                originX + caretX + 2.25f,
                originY + caretY + metrics.height);
            m_caretBrush->SetColor(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.9f));
            m_d2dContext->FillRectangle(caretRect, m_caretBrush.Get());
        }
    }

    m_d2dContext->PopAxisAlignedClip();
    HRESULT hr = m_d2dContext->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        createD2DTargetBitmap();
    }
}

void TransparentRenderer::cleanupSwapChain()
{
    m_rtv.Reset();
    m_swapChain.Reset();
    m_rootVisual.Reset();
    m_dcompTarget.Reset();
    m_dcompDevice.Reset();
    releaseD2DTarget();
    m_textLayout.Reset();
    m_textFormat.Reset();
    m_dwriteFactory.Reset();
    m_textBrush.Reset();
    m_caretBrush.Reset();
    m_backgroundBrush.Reset();
    m_d2dContext.Reset();
    m_d2dDevice.Reset();
    m_d2dFactory.Reset();
}

bool TransparentRenderer::createDevice()
{
    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#if defined(_DEBUG)
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    std::array<D3D_FEATURE_LEVEL, 3> featureLevels = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_0
    };

    Microsoft::WRL::ComPtr<ID3D11Device> device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
    D3D_FEATURE_LEVEL obtained = D3D_FEATURE_LEVEL_11_0;

    HRESULT hr = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        flags,
        featureLevels.data(),
        static_cast<UINT>(featureLevels.size()),
        D3D11_SDK_VERSION,
        &device,
        &obtained,
        &context);

    if (FAILED(hr)) {
        hr = D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_WARP,
            nullptr,
            flags,
            featureLevels.data(),
            static_cast<UINT>(featureLevels.size()),
            D3D11_SDK_VERSION,
            &device,
            &obtained,
            &context);
        if (FAILED(hr)) return false;
    }

    m_device = device;
    m_context = context;
    return true;
}

bool TransparentRenderer::createSwapChain(UINT width, UINT height)
{
    if (!m_device) return false;

    Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
    if (FAILED(m_device.As(&dxgiDevice))) return false;

    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
    if (FAILED(dxgiDevice->GetAdapter(&adapter))) return false;

    Microsoft::WRL::ComPtr<IDXGIFactory2> factory;
    if (FAILED(adapter->GetParent(__uuidof(IDXGIFactory2), &factory))) return false;

    DXGI_SWAP_CHAIN_DESC1 desc{};
    desc.Width = width;
    desc.Height = height;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 3;  // Triple buffering for 540Hz
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;

    Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain;
    HRESULT hr = factory->CreateSwapChainForComposition(m_device.Get(), &desc, nullptr, &swapChain);
    if (FAILED(hr)) {
        desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        hr = factory->CreateSwapChainForComposition(m_device.Get(), &desc, nullptr, &swapChain);
        if (FAILED(hr)) return false;
    }

    m_swapChain = swapChain;
    return true;
}

bool TransparentRenderer::createCompositionTarget()
{
    if (!m_swapChain) return false;

    Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
    if (FAILED(m_device.As(&dxgiDevice))) return false;

    Microsoft::WRL::ComPtr<IDCompositionDevice> dcompDevice;
    HRESULT hr = DCompositionCreateDevice(
        dxgiDevice.Get(),
        __uuidof(IDCompositionDevice),
        reinterpret_cast<void**>(dcompDevice.GetAddressOf()));
    if (FAILED(hr)) return false;

    dcompDevice->CreateTargetForHwnd(m_hwnd, TRUE, &m_dcompTarget);
    if (!m_dcompTarget) return false;

    dcompDevice->CreateVisual(&m_rootVisual);
    if (!m_rootVisual) return false;

    m_rootVisual->SetContent(m_swapChain.Get());
    m_dcompTarget->SetRoot(m_rootVisual.Get());

    m_dcompDevice = dcompDevice;
    m_dcompDevice->Commit();
    return true;
}

bool TransparentRenderer::createRenderTargetView()
{
    if (!m_swapChain || !m_device) return false;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
    HRESULT hr = m_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
    if (FAILED(hr)) return false;

    hr = m_device->CreateRenderTargetView(backBuffer.Get(), nullptr, &m_rtv);
    if (FAILED(hr)) return false;

    createD2DTargetBitmap();
    return true;
}

bool TransparentRenderer::createWaveResources()
{
    // Compile vertex shader
    Microsoft::WRL::ComPtr<ID3DBlob> vsBlob, psBlob, errorBlob;
    
    HRESULT hr = D3DCompile(g_waveVS, strlen(g_waveVS), "WaveVS", nullptr, nullptr,
        "main", "vs_4_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &vsBlob, &errorBlob);
    if (FAILED(hr)) return false;
    
    hr = D3DCompile(g_wavePS, strlen(g_wavePS), "WavePS", nullptr, nullptr,
        "main", "ps_4_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &psBlob, &errorBlob);
    if (FAILED(hr)) return false;
    
    hr = m_device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &m_waveVS);
    if (FAILED(hr)) return false;
    
    hr = m_device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &m_wavePS);
    if (FAILED(hr)) return false;
    
    // Input layout
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0}
    };
    hr = m_device->CreateInputLayout(layout, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &m_waveLayout);
    if (FAILED(hr)) return false;
    
    // Vertex buffer (dynamic for wave animation)
    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.ByteWidth = (UINT)(m_waveVertices.size() * sizeof(WaveVertex));
    vbDesc.Usage = D3D11_USAGE_DYNAMIC;
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    
    hr = m_device->CreateBuffer(&vbDesc, nullptr, &m_waveVB);
    if (FAILED(hr)) return false;
    
    // Blend state for transparency
    D3D11_BLEND_DESC blendDesc = {};
    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    
    hr = m_device->CreateBlendState(&blendDesc, &m_blendState);
    if (FAILED(hr)) return false;
    
    // Rasterizer state (no culling for waves)
    D3D11_RASTERIZER_DESC rastDesc = {};
    rastDesc.FillMode = D3D11_FILL_SOLID;
    rastDesc.CullMode = D3D11_CULL_NONE;
    rastDesc.DepthClipEnable = TRUE;
    
    hr = m_device->CreateRasterizerState(&rastDesc, &m_rasterizerState);
    return SUCCEEDED(hr);
}

void TransparentRenderer::enableGlassEffect()
{
    if (!m_hwnd) return;

    DWM_BLURBEHIND blur{};
    blur.dwFlags = DWM_BB_ENABLE;
    blur.fEnable = TRUE;
    blur.hRgnBlur = nullptr;
    DwmEnableBlurBehindWindow(m_hwnd, &blur);

    LONG_PTR exStyle = GetWindowLongPtr(m_hwnd, GWL_EXSTYLE);
    if (!(exStyle & WS_EX_LAYERED)) {
        SetWindowLongPtr(m_hwnd, GWL_EXSTYLE, exStyle | WS_EX_LAYERED);
    }
    SetLayeredWindowAttributes(m_hwnd, 0, 255, LWA_ALPHA);
}
