#pragma once

#include <windows.h>
#include <d3d11_1.h>
#include <dxgi1_2.h>
#include <dcomp.h>
#include <dwmapi.h>
#include <d2d1_1.h>
#include <d2d1_1helper.h>
#include <dwrite.h>
#include <wrl/client.h>
#include <d3dcompiler.h>
#include <cmath>
#include <chrono>
#include <string>
#include <vector>
#include <mutex>
#include "renderer.h"

#pragma comment(lib, "d3dcompiler.lib")

// Target: 540Hz @ 3840x2160 (4K UHD)
constexpr UINT TARGET_WIDTH = 3840;
constexpr UINT TARGET_HEIGHT = 2160;
constexpr UINT TARGET_REFRESH_HZ = 540;
constexpr float FRAME_TIME_MS = 1000.0f / TARGET_REFRESH_HZ; // ~1.85ms per frame

// Chromatic/Neon color cycling parameters
struct ChromaticConfig {
    float hueSpeed = 120.0f;       // Degrees per second
    float saturation = 1.0f;
    float brightness = 1.0f;
    float neonGlow = 2.5f;         // Glow intensity multiplier
    float chromaticShift = 0.02f;  // RGB channel offset for chromatic aberration
};

// Wave effect parameters
struct WaveConfig {
    float amplitude = 0.015f;      // Wave height (relative to screen)
    float frequency = 3.0f;        // Number of waves across screen
    float speed = 2.0f;            // Wave scroll speed
    int layers = 4;                // Number of overlapping wave layers
    float phaseOffset = 0.7854f;   // Phase difference between layers (45 degrees)
};

// Vertex structure for wave rendering
struct WaveVertex {
    float x, y, z;
    float r, g, b, a;
};

// Constant buffer for shader uniforms
struct WaveConstants {
    float time;
    float amplitude;
    float frequency;
    float padding;
};

class TransparentRenderer : public IRenderer
{
public:
    TransparentRenderer();
    ~TransparentRenderer();

    bool initialize(HWND hwnd) override;
    void resize(UINT width, UINT height);
    void render() override;
    void setClearColor(float r, float g, float b, float a);
    void updateEditorText(const std::wstring& text, const RECT& editorRect,
                         size_t caretIndex, size_t caretLine, size_t caretColumn);
    
    // Chromatic/Neon text rendering
    void renderChromaticText(HDC hdc, const wchar_t* text, int x, int y, int fontSize);
    void setTextChromaticConfig(const ChromaticConfig& config) { m_chromaConfig = config; }
    ChromaticConfig& getTextChromaticConfig() { return m_chromaConfig; }
    
    // Wave background effects
    void renderWaveBackground();
    void setWaveConfig(const WaveConfig& config) { m_waveConfig = config; }
    WaveConfig& getWaveConfig() { return m_waveConfig; }
    
    // High refresh rate timing
    void setTargetRefreshRate(UINT hz) { m_targetHz = hz; }
    UINT getTargetRefreshRate() const { return m_targetHz; }
    float getFrameTime() const { return 1000.0f / m_targetHz; }
    double getAnimationTime() const { return m_animationTime; }
    
    // Resolution control
    void setTargetResolution(UINT w, UINT h);
    
    // Color utilities (public for external use)
    void hsvToRgb(float h, float s, float v, float& r, float& g, float& b);
    void getChromaticColor(float timeOffset, float& r, float& g, float& b);
    COLORREF getChromaticColorRef(float timeOffset);
    
    // Access
    ID3D11Device* getDevice() { return m_device.Get(); }
    ID3D11DeviceContext* getContext() { return m_context.Get(); }
    HWND getHwnd() const { return m_hwnd; }

private:
    void cleanupSwapChain();
    bool createDevice();
    bool createSwapChain(UINT width, UINT height);
    bool createCompositionTarget();
    bool createRenderTargetView();
    bool createWaveResources();
    bool createD2DResources();
    bool createD2DTargetBitmap();
    void releaseD2DTarget();
    bool rebuildTextLayout(const std::wstring& text, const RECT& editorRect);
    void renderEditorOverlay();
    void enableGlassEffect();
    
    // Wave rendering
    void updateWaveVertices();
    void renderSingleWaveLayer(int layer);
    void updateTextBrushColor();

    HWND m_hwnd;
    UINT m_width;
    UINT m_height;
    float m_clearColor[4];
    
    // High refresh rate support
    UINT m_targetHz = TARGET_REFRESH_HZ;
    double m_animationTime = 0.0;
    std::chrono::high_resolution_clock::time_point m_lastFrameTime;
    
    // Chromatic/Neon config
    ChromaticConfig m_chromaConfig;
    WaveConfig m_waveConfig;
    
    // Wave geometry
    std::vector<WaveVertex> m_waveVertices;
    static constexpr int WAVE_SEGMENTS = 128;

    // D3D11 resources
    Microsoft::WRL::ComPtr<ID3D11Device> m_device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_context;
    Microsoft::WRL::ComPtr<IDXGISwapChain1> m_swapChain;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_rtv;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_waveVB;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_waveCB;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> m_waveVS;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> m_wavePS;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> m_waveLayout;
    Microsoft::WRL::ComPtr<ID3D11BlendState> m_blendState;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> m_rasterizerState;

    // DirectComposition
    Microsoft::WRL::ComPtr<IDCompositionDevice> m_dcompDevice;
    Microsoft::WRL::ComPtr<IDCompositionTarget> m_dcompTarget;
    Microsoft::WRL::ComPtr<IDCompositionVisual> m_rootVisual;

    // Direct2D / DirectWrite for GPU text overlay
    Microsoft::WRL::ComPtr<ID2D1Factory1> m_d2dFactory;
    Microsoft::WRL::ComPtr<ID2D1Device> m_d2dDevice;
    Microsoft::WRL::ComPtr<ID2D1DeviceContext> m_d2dContext;
    Microsoft::WRL::ComPtr<ID2D1Bitmap1> m_d2dTargetBitmap;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_textBrush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_caretBrush;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> m_backgroundBrush;
    Microsoft::WRL::ComPtr<IDWriteFactory> m_dwriteFactory;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_textFormat;
    Microsoft::WRL::ComPtr<IDWriteTextLayout> m_textLayout;

    // Editor state shared with IDE
    std::wstring m_editorText;
    RECT m_editorRect{};
    size_t m_caretIndex = 0;
    size_t m_caretLine = 0;
    size_t m_caretColumn = 0;
    bool m_layoutDirty = true;
    bool m_editorDirty = false;
    std::mutex m_editorMutex;
};
