#pragma once
// TransparentRenderer.h - Minimal stub for Win32 IDE build without D3D11

#include <windows.h>
#include <string>
#include <vector>

struct ChromaticConfig {
    float hueSpeed = 120.0f;
    float saturation = 1.0f;
    float brightness = 1.0f;
    float neonGlow = 2.5f;
    float chromaticShift = 0.02f;
};

struct WaveConfig {
    float amplitude = 0.015f;
    float frequency = 3.0f;
    float speed = 2.0f;
    int layers = 4;
    float phaseOffset = 0.7854f;
};

class TransparentRenderer {
public:
    TransparentRenderer() = default;
    ~TransparentRenderer() = default;
    
    bool initialize(HWND hwnd) { m_hwnd = hwnd; return true; }
    void shutdown() {}
    void render() {}
    void resize(int width, int height) { m_width = width; m_height = height; }
    
    void setChromatic(const ChromaticConfig& config) { m_chromatic = config; }
    void setWave(const WaveConfig& config) { m_wave = config; }
    
    bool isInitialized() const { return m_hwnd != nullptr; }
    
    void beginFrame() {}
    void endFrame() {}
    
    void drawText(const std::wstring& text, float x, float y, COLORREF color) {}
    void drawRect(float x, float y, float w, float h, COLORREF color) {}
    void fillRect(float x, float y, float w, float h, COLORREF color) {}

private:
    HWND m_hwnd = nullptr;
    int m_width = 0;
    int m_height = 0;
    ChromaticConfig m_chromatic;
    WaveConfig m_wave;
};
