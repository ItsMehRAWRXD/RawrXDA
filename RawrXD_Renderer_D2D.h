// RawrXD_Renderer_D2D.h — Stub for build compatibility
#pragma once
#include <windows.h>
#include <string>

// Minimal D2D renderer stub for Win32IDE_VSCodeUI.cpp compilation
namespace RawrXD {
namespace Renderer {

class D2DRenderer {
public:
    bool Initialize(HWND /*hwnd*/) { return false; }
    void Shutdown() {}
    void BeginDraw() {}
    void EndDraw() {}
    void Clear(uint32_t /*color*/) {}
    void DrawText(const std::string& /*text*/, float /*x*/, float /*y*/, uint32_t /*color*/) {}
    void DrawRect(float /*x*/, float /*y*/, float /*w*/, float /*h*/, uint32_t /*color*/) {}
    void DrawLine(float /*x1*/, float /*y1*/, float /*x2*/, float /*y2*/, uint32_t /*color*/) {}
    bool IsInitialized() const { return false; }
};

} // namespace Renderer
} // namespace RawrXD
