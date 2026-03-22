#include "Win32IDE.h"
// VulkanRenderer is declared in VulkanRenderer.cpp
class VulkanRenderer;
#include <windows.h>
#include <memory>

RawrXD::IRenderer* CreateVulkanRenderer();

// Handler for Vulkan Renderer feature
void HandleVulkanRenderer(void* idePtr) {
    Win32IDE* ide = static_cast<Win32IDE*>(idePtr);
    if (!ide) return;

    // Route through the existing owner path that updates menu/check state.
    SendMessageA(ide->getMainWindow(), WM_COMMAND, 2027, 0);
}