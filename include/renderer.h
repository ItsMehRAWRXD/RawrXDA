#pragma once

#include <windows.h>
#include <string>

class IRenderer {
public:
    virtual ~IRenderer() = default;
    virtual bool initialize(HWND hwnd) = 0;
    virtual void resize(UINT w, UINT h) = 0;
    virtual void render() = 0;
    virtual void setClearColor(float r, float g, float b, float a) = 0;
    virtual void updateEditorText(const std::wstring& text, const RECT& editorRect, size_t caretIndex, size_t caretLine, size_t caretColumn) = 0;
};

// Optional Vulkan backend factory (implemented when ENABLE_VULKAN is ON)
IRenderer* CreateVulkanRenderer();
