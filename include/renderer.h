#pragma once

#include <vector>
#include <string>
#include <Windows.h>

namespace RawrXD {

    // Forward declaration if needed, or define shared structs
    #ifndef RAWRXD_RENDER_COMMAND_DEFINED
    #define RAWRXD_RENDER_COMMAND_DEFINED
    struct RenderCommand {
        std::string type;
        // extended fields...
    };
    #endif

    #ifndef RAWRXD_RENDERER_INTERFACE_DEFINED
    #define RAWRXD_RENDERER_INTERFACE_DEFINED
    class IRenderer {
    public:
        virtual ~IRenderer() = default;

        virtual bool Initialize(HWND hWnd) = 0;
        virtual void Render() = 0;
        virtual void Resize(UINT width, UINT height) = 0;
        virtual void SetTransparency(float alpha) = 0;
        
        // Advanced features stubbed initially
        virtual void DrawText(const std::wstring& text, float x, float y, float size, uint32_t color) = 0;
        virtual void DrawRect(float x, float y, float w, float h, uint32_t color) = 0;
        
        // Enterprise features
        virtual void BeginFrame() = 0;
        virtual void EndFrame() = 0;
    };
    #endif

}
