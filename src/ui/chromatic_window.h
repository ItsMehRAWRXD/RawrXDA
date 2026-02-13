#pragma once
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi.h>
#include <DirectXMath.h>
#include <string>
#include <memory>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxgi.lib")

namespace RawrXD {
namespace UI {

// Chromatic/Neon Text Window with Wave Background Effects
// Designed for 540Hz @ 3840x2160 (4K UHD)
class ChromaticWindow {
public:
    // Target specs
    static constexpr int TARGET_WIDTH = 3840;
    static constexpr int TARGET_HEIGHT = 2160;
    static constexpr int TARGET_REFRESH_HZ = 540;
    
    ChromaticWindow();
    ~ChromaticWindow();
    
    // Create the window and initialize D3D11
    bool create(HINSTANCE hInstance, int width = TARGET_WIDTH, int height = TARGET_HEIGHT);
    void destroy();
    
    // Main render loop (call in message pump)
    void render();
    void present();
    
    // Wave effect parameters
    void setWaveAmplitude(float amp) { m_waveAmplitude = amp; }
    void setWaveFrequency(float freq) { m_waveFrequency = freq; }
    void setWaveSpeed(float speed) { m_waveSpeed = speed; }
    
    // Chromatic/neon text parameters
    void setChromaticSpeed(float speed) { m_chromaSpeed = speed; }
    void setChromaticSaturation(float sat) { m_chromaSat = sat; }
    void setChromaticBrightness(float bright) { m_chromaBright = bright; }
    
    // Text to render with chromatic effect
    void setText(const std::wstring& text) { m_displayText = text; }
    
    // Get window handle
    HWND getHWND() const { return m_hwnd; }
    bool isValid() const { return m_hwnd != nullptr && m_device != nullptr; }
    
    // Window procedure
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    
private:
    bool initD3D11(int width, int height);
    bool createShaders();
    bool createBuffers();
    void updateConstants();
    
    // Window
    HWND m_hwnd = nullptr;
    HINSTANCE m_hInstance = nullptr;
    int m_width = TARGET_WIDTH;
    int m_height = TARGET_HEIGHT;
    
    // D3D11 core
    ID3D11Device* m_device = nullptr;
    ID3D11DeviceContext* m_context = nullptr;
    IDXGISwapChain* m_swapChain = nullptr;
    ID3D11RenderTargetView* m_rtv = nullptr;
    
    // Shaders
    ID3D11VertexShader* m_vertexShader = nullptr;
    ID3D11PixelShader* m_pixelShader = nullptr;
    ID3D11InputLayout* m_inputLayout = nullptr;
    
    // Buffers
    ID3D11Buffer* m_vertexBuffer = nullptr;
    ID3D11Buffer* m_constantBuffer = nullptr;
    
    // Wave parameters
    float m_waveAmplitude = 15.0f;
    float m_waveFrequency = 0.02f;
    float m_waveSpeed = 2.0f;
    
    // Chromatic parameters
    float m_chromaSpeed = 1.5f;
    float m_chromaSat = 1.0f;
    float m_chromaBright = 1.0f;
    
    // Timing
    float m_time = 0.0f;
    LARGE_INTEGER m_lastTime{};
    LARGE_INTEGER m_frequency{};
    
    // Display text
    std::wstring m_displayText = L"RawrXD IDE - Chromatic Mode";
    
    // Constant buffer structure (must match shader)
    struct alignas(16) ConstantData {
        float time;
        float waveAmp;
        float waveFreq;
        float waveSpeed;
        float chromaSpeed;
        float chromaSat;
        float chromaBright;
        float aspectRatio;
        float screenWidth;
        float screenHeight;
        float padding[2];
    };
};

} // namespace UI
} // namespace RawrXD
