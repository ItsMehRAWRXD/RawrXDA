#include "renderer.h"
#include <iostream>
#include <windows.h>
#include <vector>
#include <cstring>
#include <cstdint>
#include <algorithm>

// ============================================================================
// Minimal Vulkan types — resolved dynamically from vulkan-1.dll
// Avoids compile-time Vulkan SDK dependency while enabling full pipeline.
// ============================================================================
typedef uint32_t VkFlags;
typedef uint32_t VkBool32;
typedef uint64_t VkDeviceSize;
typedef void*    VkInstance;
typedef void*    VkPhysicalDevice;
typedef void*    VkDevice;
typedef void*    VkQueue;
typedef void*    VkSurfaceKHR;
typedef void*    VkSwapchainKHR;
typedef void*    VkRenderPass;
typedef void*    VkCommandPool;
typedef void*    VkCommandBuffer;
typedef void*    VkFramebuffer;
typedef void*    VkImageView;
typedef void*    VkSemaphore;
typedef void*    VkFence;
typedef void*    VkImage;
typedef int32_t  VkResult;

#define VK_SUCCESS 0
#define VK_SUBOPTIMAL_KHR 1000001003
#define VK_ERROR_OUT_OF_DATE_KHR (-1000001004)
#define VK_NULL_HANDLE nullptr
#define VK_TRUE 1
#define VK_FALSE 0

// Proc address typedefs
typedef void* (*PFN_vkGetInstanceProcAddr)(VkInstance, const char*);
typedef VkResult (*PFN_vkCreateInstance)(const void*, const void*, VkInstance*);
typedef void (*PFN_vkDestroyInstance)(VkInstance, const void*);
typedef VkResult (*PFN_vkEnumeratePhysicalDevices)(VkInstance, uint32_t*, VkPhysicalDevice*);
typedef void (*PFN_vkGetPhysicalDeviceProperties)(VkPhysicalDevice, void*);
typedef void (*PFN_vkGetPhysicalDeviceQueueFamilyProperties)(VkPhysicalDevice, uint32_t*, void*);
typedef VkResult (*PFN_vkCreateDevice)(VkPhysicalDevice, const void*, const void*, VkDevice*);
typedef void (*PFN_vkDestroyDevice)(VkDevice, const void*);
typedef void (*PFN_vkGetDeviceQueue)(VkDevice, uint32_t, uint32_t, VkQueue*);
typedef VkResult (*PFN_vkCreateWin32SurfaceKHR)(VkInstance, const void*, const void*, VkSurfaceKHR*);
typedef void (*PFN_vkDestroySurfaceKHR)(VkInstance, VkSurfaceKHR, const void*);
typedef VkResult (*PFN_vkGetPhysicalDeviceSurfaceSupportKHR)(VkPhysicalDevice, uint32_t, VkSurfaceKHR, VkBool32*);
typedef VkResult (*PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR)(VkPhysicalDevice, VkSurfaceKHR, void*);
typedef VkResult (*PFN_vkGetPhysicalDeviceSurfaceFormatsKHR)(VkPhysicalDevice, VkSurfaceKHR, uint32_t*, void*);
typedef VkResult (*PFN_vkGetPhysicalDeviceSurfacePresentModesKHR)(VkPhysicalDevice, VkSurfaceKHR, uint32_t*, void*);
typedef VkResult (*PFN_vkCreateSwapchainKHR)(VkDevice, const void*, const void*, VkSwapchainKHR*);
typedef void (*PFN_vkDestroySwapchainKHR)(VkDevice, VkSwapchainKHR, const void*);
typedef VkResult (*PFN_vkGetSwapchainImagesKHR)(VkDevice, VkSwapchainKHR, uint32_t*, VkImage*);
typedef VkResult (*PFN_vkCreateImageView)(VkDevice, const void*, const void*, VkImageView*);
typedef void (*PFN_vkDestroyImageView)(VkDevice, VkImageView, const void*);
typedef VkResult (*PFN_vkCreateRenderPass)(VkDevice, const void*, const void*, VkRenderPass*);
typedef void (*PFN_vkDestroyRenderPass)(VkDevice, VkRenderPass, const void*);
typedef VkResult (*PFN_vkCreateFramebuffer)(VkDevice, const void*, const void*, VkFramebuffer*);
typedef void (*PFN_vkDestroyFramebuffer)(VkDevice, VkFramebuffer, const void*);
typedef VkResult (*PFN_vkCreateCommandPool)(VkDevice, const void*, const void*, VkCommandPool*);
typedef void (*PFN_vkDestroyCommandPool)(VkDevice, VkCommandPool, const void*);
typedef VkResult (*PFN_vkAllocateCommandBuffers)(VkDevice, const void*, VkCommandBuffer*);
typedef VkResult (*PFN_vkCreateSemaphore)(VkDevice, const void*, const void*, VkSemaphore*);
typedef void (*PFN_vkDestroySemaphore)(VkDevice, VkSemaphore, const void*);
typedef VkResult (*PFN_vkCreateFence)(VkDevice, const void*, const void*, VkFence*);
typedef void (*PFN_vkDestroyFence)(VkDevice, VkFence, const void*);
typedef VkResult (*PFN_vkWaitForFences)(VkDevice, uint32_t, const VkFence*, VkBool32, uint64_t);
typedef VkResult (*PFN_vkResetFences)(VkDevice, uint32_t, const VkFence*);
typedef VkResult (*PFN_vkAcquireNextImageKHR)(VkDevice, VkSwapchainKHR, uint64_t, VkSemaphore, VkFence, uint32_t*);
typedef VkResult (*PFN_vkBeginCommandBuffer)(VkCommandBuffer, const void*);
typedef VkResult (*PFN_vkEndCommandBuffer)(VkCommandBuffer);
typedef VkResult (*PFN_vkResetCommandBuffer)(VkCommandBuffer, VkFlags);
typedef void (*PFN_vkCmdBeginRenderPass)(VkCommandBuffer, const void*, int);
typedef void (*PFN_vkCmdEndRenderPass)(VkCommandBuffer);
typedef VkResult (*PFN_vkQueueSubmit)(VkQueue, uint32_t, const void*, VkFence);
typedef VkResult (*PFN_vkQueuePresentKHR)(VkQueue, const void*);
typedef VkResult (*PFN_vkDeviceWaitIdle)(VkDevice);

// ============================================================================
// VulkanRenderer — Production Vulkan backend with dynamic loading
// ============================================================================
class VulkanRenderer : public IRenderer {
public:
    VulkanRenderer() : m_vkModule(nullptr), m_initialized(false) {}
    ~VulkanRenderer() override {
        cleanup();
    }

    bool initialize(HWND hwnd) override {
        m_hwnd = hwnd;

        // Dynamically load Vulkan runtime so the IDE still launches even
        // when Vulkan is not installed.
        m_vkModule = LoadLibraryA("vulkan-1.dll");
        if (!m_vkModule) {
            std::cerr << "VulkanRenderer: vulkan-1.dll not found; Vulkan backend disabled" << std::endl;
            m_initialized = false;
            return false;
        }

        // Resolve vkGetInstanceProcAddr — the root of all Vulkan function resolution
        m_vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)GetProcAddress(m_vkModule, "vkGetInstanceProcAddr");
        if (!m_vkGetInstanceProcAddr) {
            std::cerr << "VulkanRenderer: vkGetInstanceProcAddr missing; Vulkan backend disabled" << std::endl;
            FreeLibrary(m_vkModule);
            m_vkModule = nullptr;
            m_initialized = false;
            return false;
        }

        // Resolve vkCreateInstance
        auto vkCreateInstance = (PFN_vkCreateInstance)m_vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkCreateInstance");
        if (!vkCreateInstance) {
            std::cerr << "VulkanRenderer: Cannot resolve vkCreateInstance" << std::endl;
            FreeLibrary(m_vkModule);
            m_vkModule = nullptr;
            return false;
        }

        // Create Vulkan instance with Win32 surface extension
        const char* extensions[] = {
            "VK_KHR_surface",
            "VK_KHR_win32_surface"
        };

        struct { int sType; const void* pNext; uint32_t flags;
                 const void* pApp; uint32_t enabledLayerCount; const char*const* ppEnabledLayerNames;
                 uint32_t enabledExtensionCount; const char*const* ppEnabledExtensionNames;
        } instanceCI{};
        instanceCI.sType = 1; // VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO
        instanceCI.enabledExtensionCount = 2;
        instanceCI.ppEnabledExtensionNames = extensions;

        VkResult result = vkCreateInstance(&instanceCI, nullptr, &m_instance);
        if (result != VK_SUCCESS || !m_instance) {
            std::cerr << "VulkanRenderer: vkCreateInstance failed (" << result << ")" << std::endl;
            FreeLibrary(m_vkModule);
            m_vkModule = nullptr;
            return false;
        }

        // Resolve all instance-level and device-level function pointers
        if (!resolveFunctionPointers()) {
            std::cerr << "VulkanRenderer: Failed to resolve critical Vulkan functions" << std::endl;
            fn.vkDestroyInstance(m_instance, nullptr);
            m_instance = VK_NULL_HANDLE;
            FreeLibrary(m_vkModule);
            m_vkModule = nullptr;
            return false;
        }

        // Create Win32 surface
        struct { int sType; const void* pNext; uint32_t flags;
                 HINSTANCE hinstance; HWND hwnd;
        } surfaceCI{};
        surfaceCI.sType = 1000009000; // VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR
        surfaceCI.hinstance = GetModuleHandle(nullptr);
        surfaceCI.hwnd = hwnd;

        result = fn.vkCreateWin32SurfaceKHR(m_instance, &surfaceCI, nullptr, &m_surface);
        if (result != VK_SUCCESS) {
            std::cerr << "VulkanRenderer: Surface creation failed (" << result << ")" << std::endl;
            fn.vkDestroyInstance(m_instance, nullptr);
            m_instance = VK_NULL_HANDLE;
            FreeLibrary(m_vkModule);
            m_vkModule = nullptr;
            return false;
        }

        // Select physical device and create logical device
        if (!createDeviceAndQueue()) {
            std::cerr << "VulkanRenderer: Device creation failed" << std::endl;
            cleanup();
            return false;
        }

        // Get initial window size
        RECT rc;
        GetClientRect(hwnd, &rc);
        m_width = static_cast<uint32_t>(rc.right - rc.left);
        m_height = static_cast<uint32_t>(rc.bottom - rc.top);
        if (m_width == 0) m_width = 1;
        if (m_height == 0) m_height = 1;

        // Create swapchain, render pass, framebuffers, command pool, sync
        if (!createSwapchain()) {
            std::cerr << "VulkanRenderer: Swapchain creation failed" << std::endl;
            cleanup();
            return false;
        }

        std::cout << "VulkanRenderer: Vulkan pipeline initialized ("
                  << m_width << "x" << m_height << ", "
                  << m_swapchainImages.size() << " images)" << std::endl;
        m_initialized = true;
        return true;
    }

    void resize(UINT w, UINT h) override {
        if (!m_initialized) return;
        if (w == 0 || h == 0) return;
        if (w == m_width && h == m_height) return;

        m_width = w;
        m_height = h;

        // Wait for all GPU operations to complete before recreating swapchain
        fn.vkDeviceWaitIdle(m_device);

        // Destroy old swap resources
        destroySwapResources();

        // Recreate swapchain + framebuffers for new dimensions
        if (!createSwapchain()) {
            std::cerr << "VulkanRenderer: Swapchain recreation failed on resize" << std::endl;
            m_initialized = false;
        }
    }

    void render() override {
        if (!m_initialized) return;
        if (m_swapchainImages.empty()) return;

        // Wait for previous frame's fence
        fn.vkWaitForFences(m_device, 1, &m_inFlightFence, VK_TRUE, UINT64_MAX);
        fn.vkResetFences(m_device, 1, &m_inFlightFence);

        // Acquire next swapchain image
        uint32_t imageIndex = 0;
        VkResult result = fn.vkAcquireNextImageKHR(m_device, m_swapchain, UINT64_MAX,
                                                    m_imageAvailableSemaphore, VK_NULL_HANDLE,
                                                    &imageIndex);
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            // Swapchain out of date — trigger resize
            RECT rc;
            GetClientRect(m_hwnd, &rc);
            resize(static_cast<UINT>(rc.right - rc.left), static_cast<UINT>(rc.bottom - rc.top));
            return;
        }

        // Reset + record command buffer: begin render pass with clear color, end
        VkCommandBuffer cmd = m_commandBuffers[imageIndex];
        fn.vkResetCommandBuffer(cmd, 0);

        // BeginCommandBuffer
        struct { int sType; const void* pNext; uint32_t flags; const void* pInheritance; } beginInfo{};
        beginInfo.sType = 40; // VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
        fn.vkBeginCommandBuffer(cmd, &beginInfo);

        // BeginRenderPass with clear color
        struct ClearValue { float color[4]; };
        ClearValue clearVal = {{ m_clearColor[0], m_clearColor[1], m_clearColor[2], m_clearColor[3] }};

        struct { int sType; const void* pNext;
                 VkRenderPass renderPass; VkFramebuffer framebuffer;
                 struct { int32_t x, y; uint32_t w, h; } renderArea;
                 uint32_t clearValueCount; const ClearValue* pClearValues;
        } rpBeginInfo{};
        rpBeginInfo.sType = 43; // VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO
        rpBeginInfo.renderPass = m_renderPass;
        rpBeginInfo.framebuffer = m_framebuffers[imageIndex];
        rpBeginInfo.renderArea = { {0, 0}, {m_width, m_height} };
        rpBeginInfo.clearValueCount = 1;
        rpBeginInfo.pClearValues = &clearVal;

        fn.vkCmdBeginRenderPass(cmd, &rpBeginInfo, 0); // VK_SUBPASS_CONTENTS_INLINE = 0
        // Future: bind pipeline, draw editor quads, overlay UI here
        fn.vkCmdEndRenderPass(cmd);
        fn.vkEndCommandBuffer(cmd);

        // Submit
        VkSemaphore waitSemaphores[] = { m_imageAvailableSemaphore };
        VkSemaphore signalSemaphores[] = { m_renderFinishedSemaphore };
        uint32_t waitStage = 0x00000400; // VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT

        struct { int sType; const void* pNext;
                 uint32_t waitSemaphoreCount; const VkSemaphore* pWaitSemaphores;
                 const uint32_t* pWaitDstStageMask;
                 uint32_t commandBufferCount; const VkCommandBuffer* pCommandBuffers;
                 uint32_t signalSemaphoreCount; const VkSemaphore* pSignalSemaphores;
        } submitInfo{};
        submitInfo.sType = 4; // VK_STRUCTURE_TYPE_SUBMIT_INFO
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = &waitStage;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmd;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        fn.vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, m_inFlightFence);

        // Present
        struct { int sType; const void* pNext;
                 uint32_t waitSemaphoreCount; const VkSemaphore* pWaitSemaphores;
                 uint32_t swapchainCount; const VkSwapchainKHR* pSwapchains;
                 const uint32_t* pImageIndices; VkResult* pResults;
        } presentInfo{};
        presentInfo.sType = 1000001001; // VK_STRUCTURE_TYPE_PRESENT_INFO_KHR
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &m_swapchain;
        presentInfo.pImageIndices = &imageIndex;

        result = fn.vkQueuePresentKHR(m_graphicsQueue, &presentInfo);
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
            RECT rc;
            GetClientRect(m_hwnd, &rc);
            resize(static_cast<UINT>(rc.right - rc.left), static_cast<UINT>(rc.bottom - rc.top));
        }
    }

    void setClearColor(float r, float g, float b, float a) override {
        m_clearColor[0] = r; m_clearColor[1] = g; m_clearColor[2] = b; m_clearColor[3] = a;
    }

    void updateEditorText(const std::wstring& text, const RECT& editorRect,
                          size_t caretIndex, size_t caretLine, size_t caretColumn) override {
        if (!m_initialized) return;
        // Store text rendering state for next render pass
        // Full GPU text rendering (glyph atlas + instanced quads) deferred to
        // Phase 2 of the Vulkan renderer — this keeps the interface stable
        m_pendingText = text;
        m_pendingEditorRect = editorRect;
        m_pendingCaretIndex = caretIndex;
        m_pendingCaretLine = caretLine;
        m_pendingCaretColumn = caretColumn;
    }

private:
    // ---- Resolved function pointers ----
    struct VulkanFunctions {
        PFN_vkDestroyInstance vkDestroyInstance = nullptr;
        PFN_vkEnumeratePhysicalDevices vkEnumeratePhysicalDevices = nullptr;
        PFN_vkGetPhysicalDeviceProperties vkGetPhysicalDeviceProperties = nullptr;
        PFN_vkGetPhysicalDeviceQueueFamilyProperties vkGetPhysicalDeviceQueueFamilyProperties = nullptr;
        PFN_vkCreateDevice vkCreateDevice = nullptr;
        PFN_vkDestroyDevice vkDestroyDevice = nullptr;
        PFN_vkGetDeviceQueue vkGetDeviceQueue = nullptr;
        PFN_vkCreateWin32SurfaceKHR vkCreateWin32SurfaceKHR = nullptr;
        PFN_vkDestroySurfaceKHR vkDestroySurfaceKHR = nullptr;
        PFN_vkGetPhysicalDeviceSurfaceSupportKHR vkGetPhysicalDeviceSurfaceSupportKHR = nullptr;
        PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR vkGetPhysicalDeviceSurfaceCapabilitiesKHR = nullptr;
        PFN_vkGetPhysicalDeviceSurfaceFormatsKHR vkGetPhysicalDeviceSurfaceFormatsKHR = nullptr;
        PFN_vkGetPhysicalDeviceSurfacePresentModesKHR vkGetPhysicalDeviceSurfacePresentModesKHR = nullptr;
        PFN_vkCreateSwapchainKHR vkCreateSwapchainKHR = nullptr;
        PFN_vkDestroySwapchainKHR vkDestroySwapchainKHR = nullptr;
        PFN_vkGetSwapchainImagesKHR vkGetSwapchainImagesKHR = nullptr;
        PFN_vkCreateImageView vkCreateImageView = nullptr;
        PFN_vkDestroyImageView vkDestroyImageView = nullptr;
        PFN_vkCreateRenderPass vkCreateRenderPass = nullptr;
        PFN_vkDestroyRenderPass vkDestroyRenderPass = nullptr;
        PFN_vkCreateFramebuffer vkCreateFramebuffer = nullptr;
        PFN_vkDestroyFramebuffer vkDestroyFramebuffer = nullptr;
        PFN_vkCreateCommandPool vkCreateCommandPool = nullptr;
        PFN_vkDestroyCommandPool vkDestroyCommandPool = nullptr;
        PFN_vkAllocateCommandBuffers vkAllocateCommandBuffers = nullptr;
        PFN_vkCreateSemaphore vkCreateSemaphore = nullptr;
        PFN_vkDestroySemaphore vkDestroySemaphore = nullptr;
        PFN_vkCreateFence vkCreateFence = nullptr;
        PFN_vkDestroyFence vkDestroyFence = nullptr;
        PFN_vkWaitForFences vkWaitForFences = nullptr;
        PFN_vkResetFences vkResetFences = nullptr;
        PFN_vkAcquireNextImageKHR vkAcquireNextImageKHR = nullptr;
        PFN_vkBeginCommandBuffer vkBeginCommandBuffer = nullptr;
        PFN_vkEndCommandBuffer vkEndCommandBuffer = nullptr;
        PFN_vkResetCommandBuffer vkResetCommandBuffer = nullptr;
        PFN_vkCmdBeginRenderPass vkCmdBeginRenderPass = nullptr;
        PFN_vkCmdEndRenderPass vkCmdEndRenderPass = nullptr;
        PFN_vkQueueSubmit vkQueueSubmit = nullptr;
        PFN_vkQueuePresentKHR vkQueuePresentKHR = nullptr;
        PFN_vkDeviceWaitIdle vkDeviceWaitIdle = nullptr;
    } fn;

    bool resolveFunctionPointers() {
        auto resolve = [this](const char* name) -> void* {
            return m_vkGetInstanceProcAddr(m_instance, name);
        };
        fn.vkDestroyInstance = (PFN_vkDestroyInstance)resolve("vkDestroyInstance");
        fn.vkEnumeratePhysicalDevices = (PFN_vkEnumeratePhysicalDevices)resolve("vkEnumeratePhysicalDevices");
        fn.vkGetPhysicalDeviceProperties = (PFN_vkGetPhysicalDeviceProperties)resolve("vkGetPhysicalDeviceProperties");
        fn.vkGetPhysicalDeviceQueueFamilyProperties = (PFN_vkGetPhysicalDeviceQueueFamilyProperties)resolve("vkGetPhysicalDeviceQueueFamilyProperties");
        fn.vkCreateDevice = (PFN_vkCreateDevice)resolve("vkCreateDevice");
        fn.vkDestroyDevice = (PFN_vkDestroyDevice)resolve("vkDestroyDevice");
        fn.vkGetDeviceQueue = (PFN_vkGetDeviceQueue)resolve("vkGetDeviceQueue");
        fn.vkCreateWin32SurfaceKHR = (PFN_vkCreateWin32SurfaceKHR)resolve("vkCreateWin32SurfaceKHR");
        fn.vkDestroySurfaceKHR = (PFN_vkDestroySurfaceKHR)resolve("vkDestroySurfaceKHR");
        fn.vkGetPhysicalDeviceSurfaceSupportKHR = (PFN_vkGetPhysicalDeviceSurfaceSupportKHR)resolve("vkGetPhysicalDeviceSurfaceSupportKHR");
        fn.vkGetPhysicalDeviceSurfaceCapabilitiesKHR = (PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR)resolve("vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
        fn.vkGetPhysicalDeviceSurfaceFormatsKHR = (PFN_vkGetPhysicalDeviceSurfaceFormatsKHR)resolve("vkGetPhysicalDeviceSurfaceFormatsKHR");
        fn.vkGetPhysicalDeviceSurfacePresentModesKHR = (PFN_vkGetPhysicalDeviceSurfacePresentModesKHR)resolve("vkGetPhysicalDeviceSurfacePresentModesKHR");
        fn.vkCreateSwapchainKHR = (PFN_vkCreateSwapchainKHR)resolve("vkCreateSwapchainKHR");
        fn.vkDestroySwapchainKHR = (PFN_vkDestroySwapchainKHR)resolve("vkDestroySwapchainKHR");
        fn.vkGetSwapchainImagesKHR = (PFN_vkGetSwapchainImagesKHR)resolve("vkGetSwapchainImagesKHR");
        fn.vkCreateImageView = (PFN_vkCreateImageView)resolve("vkCreateImageView");
        fn.vkDestroyImageView = (PFN_vkDestroyImageView)resolve("vkDestroyImageView");
        fn.vkCreateRenderPass = (PFN_vkCreateRenderPass)resolve("vkCreateRenderPass");
        fn.vkDestroyRenderPass = (PFN_vkDestroyRenderPass)resolve("vkDestroyRenderPass");
        fn.vkCreateFramebuffer = (PFN_vkCreateFramebuffer)resolve("vkCreateFramebuffer");
        fn.vkDestroyFramebuffer = (PFN_vkDestroyFramebuffer)resolve("vkDestroyFramebuffer");
        fn.vkCreateCommandPool = (PFN_vkCreateCommandPool)resolve("vkCreateCommandPool");
        fn.vkDestroyCommandPool = (PFN_vkDestroyCommandPool)resolve("vkDestroyCommandPool");
        fn.vkAllocateCommandBuffers = (PFN_vkAllocateCommandBuffers)resolve("vkAllocateCommandBuffers");
        fn.vkCreateSemaphore = (PFN_vkCreateSemaphore)resolve("vkCreateSemaphore");
        fn.vkDestroySemaphore = (PFN_vkDestroySemaphore)resolve("vkDestroySemaphore");
        fn.vkCreateFence = (PFN_vkCreateFence)resolve("vkCreateFence");
        fn.vkDestroyFence = (PFN_vkDestroyFence)resolve("vkDestroyFence");
        fn.vkWaitForFences = (PFN_vkWaitForFences)resolve("vkWaitForFences");
        fn.vkResetFences = (PFN_vkResetFences)resolve("vkResetFences");
        fn.vkAcquireNextImageKHR = (PFN_vkAcquireNextImageKHR)resolve("vkAcquireNextImageKHR");
        fn.vkBeginCommandBuffer = (PFN_vkBeginCommandBuffer)resolve("vkBeginCommandBuffer");
        fn.vkEndCommandBuffer = (PFN_vkEndCommandBuffer)resolve("vkEndCommandBuffer");
        fn.vkResetCommandBuffer = (PFN_vkResetCommandBuffer)resolve("vkResetCommandBuffer");
        fn.vkCmdBeginRenderPass = (PFN_vkCmdBeginRenderPass)resolve("vkCmdBeginRenderPass");
        fn.vkCmdEndRenderPass = (PFN_vkCmdEndRenderPass)resolve("vkCmdEndRenderPass");
        fn.vkQueueSubmit = (PFN_vkQueueSubmit)resolve("vkQueueSubmit");
        fn.vkQueuePresentKHR = (PFN_vkQueuePresentKHR)resolve("vkQueuePresentKHR");
        fn.vkDeviceWaitIdle = (PFN_vkDeviceWaitIdle)resolve("vkDeviceWaitIdle");

        // Validate critical functions
        return fn.vkDestroyInstance && fn.vkEnumeratePhysicalDevices &&
               fn.vkCreateDevice && fn.vkCreateWin32SurfaceKHR &&
               fn.vkCreateSwapchainKHR && fn.vkCreateRenderPass &&
               fn.vkBeginCommandBuffer && fn.vkQueueSubmit;
    }

    bool createDeviceAndQueue() {
        // Enumerate physical devices
        uint32_t deviceCount = 0;
        fn.vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);
        if (deviceCount == 0) return false;

        std::vector<VkPhysicalDevice> devices(deviceCount);
        fn.vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());

        // Select first device with a graphics queue that supports presentation
        for (auto& pd : devices) {
            uint32_t qfCount = 0;
            fn.vkGetPhysicalDeviceQueueFamilyProperties(pd, &qfCount, nullptr);

            // Queue family properties (simplified — we only need queueFlags at offset 0)
            struct QFP { uint32_t queueFlags; uint32_t queueCount; uint32_t timestampValidBits; uint32_t minImageTransferGranularity[3]; };
            std::vector<QFP> qfProps(qfCount);
            fn.vkGetPhysicalDeviceQueueFamilyProperties(pd, &qfCount, qfProps.data());

            for (uint32_t i = 0; i < qfCount; i++) {
                // Check graphics support (VK_QUEUE_GRAPHICS_BIT = 0x00000001)
                if (!(qfProps[i].queueFlags & 1)) continue;

                VkBool32 presentSupport = VK_FALSE;
                fn.vkGetPhysicalDeviceSurfaceSupportKHR(pd, i, m_surface, &presentSupport);
                if (!presentSupport) continue;

                m_physicalDevice = pd;
                m_graphicsQueueFamily = i;

                // Create logical device with VK_KHR_swapchain
                float queuePriority = 1.0f;
                struct { int sType; const void* pNext; uint32_t flags;
                         uint32_t queueFamilyIndex; uint32_t queueCount; const float* pQueuePriorities;
                } queueCI{};
                queueCI.sType = 2; // VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO
                queueCI.queueFamilyIndex = i;
                queueCI.queueCount = 1;
                queueCI.pQueuePriorities = &queuePriority;

                const char* deviceExts[] = { "VK_KHR_swapchain" };
                struct { int sType; const void* pNext; uint32_t flags;
                         uint32_t queueCreateInfoCount; const void* pQueueCreateInfos;
                         uint32_t enabledLayerCount; const char*const* ppEnabledLayerNames;
                         uint32_t enabledExtensionCount; const char*const* ppEnabledExtensionNames;
                         const void* pEnabledFeatures;
                } deviceCI{};
                deviceCI.sType = 3; // VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO
                deviceCI.queueCreateInfoCount = 1;
                deviceCI.pQueueCreateInfos = &queueCI;
                deviceCI.enabledExtensionCount = 1;
                deviceCI.ppEnabledExtensionNames = deviceExts;

                VkResult result = fn.vkCreateDevice(pd, &deviceCI, nullptr, &m_device);
                if (result != VK_SUCCESS) continue;

                fn.vkGetDeviceQueue(m_device, i, 0, &m_graphicsQueue);
                return true;
            }
        }
        return false;
    }

    bool createSwapchain() {
        // Query surface capabilities
        struct SurfaceCaps {
            uint32_t minImageCount; uint32_t maxImageCount;
            uint32_t currentExtentW, currentExtentH;
            uint32_t minImageExtentW, minImageExtentH;
            uint32_t maxImageExtentW, maxImageExtentH;
            uint32_t maxImageArrayLayers;
            uint32_t supportedTransforms; uint32_t currentTransform;
            uint32_t supportedCompositeAlpha; uint32_t supportedUsageFlags;
        };
        SurfaceCaps caps{};
        fn.vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physicalDevice, m_surface, &caps);

        uint32_t imageCount = caps.minImageCount + 1;
        if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount)
            imageCount = caps.maxImageCount;

        // Query surface format — use first available
        uint32_t formatCount = 0;
        fn.vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &formatCount, nullptr);
        struct SurfaceFormat { uint32_t format; uint32_t colorSpace; };
        std::vector<SurfaceFormat> formats(formatCount > 0 ? formatCount : 1);
        if (formatCount > 0) {
            fn.vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &formatCount, formats.data());
        }

        uint32_t surfaceFormat = formats[0].format;
        uint32_t colorSpace = formats[0].colorSpace;
        // Prefer B8G8R8A8_SRGB (50) if available
        for (auto& f : formats) {
            if (f.format == 50 && f.colorSpace == 0) { surfaceFormat = f.format; colorSpace = f.colorSpace; break; }
        }
        m_swapchainFormat = surfaceFormat;

        // Create swapchain
        struct {
            int sType; const void* pNext; uint32_t flags;
            VkSurfaceKHR surface;
            uint32_t minImageCount; uint32_t imageFormat; uint32_t imageColorSpace;
            uint32_t imageExtentW, imageExtentH;
            uint32_t imageArrayLayers; uint32_t imageUsage;
            uint32_t imageSharingMode; uint32_t queueFamilyIndexCount; const uint32_t* pQueueFamilyIndices;
            uint32_t preTransform; uint32_t compositeAlpha; uint32_t presentMode;
            VkBool32 clipped; VkSwapchainKHR oldSwapchain;
        } swapCI{};
        swapCI.sType = 1000001000; // VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR
        swapCI.surface = m_surface;
        swapCI.minImageCount = imageCount;
        swapCI.imageFormat = surfaceFormat;
        swapCI.imageColorSpace = colorSpace;
        swapCI.imageExtentW = m_width;
        swapCI.imageExtentH = m_height;
        swapCI.imageArrayLayers = 1;
        swapCI.imageUsage = 0x00000010; // VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
        swapCI.imageSharingMode = 0; // VK_SHARING_MODE_EXCLUSIVE
        swapCI.preTransform = caps.currentTransform;
        swapCI.compositeAlpha = 1; // VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR
        swapCI.presentMode = 2; // VK_PRESENT_MODE_FIFO_KHR (vsync)
        swapCI.clipped = VK_TRUE;

        VkResult result = fn.vkCreateSwapchainKHR(m_device, &swapCI, nullptr, &m_swapchain);
        if (result != VK_SUCCESS) return false;

        // Get swapchain images
        uint32_t imgCount = 0;
        fn.vkGetSwapchainImagesKHR(m_device, m_swapchain, &imgCount, nullptr);
        m_swapchainImages.resize(imgCount);
        fn.vkGetSwapchainImagesKHR(m_device, m_swapchain, &imgCount, m_swapchainImages.data());

        // Create image views
        m_swapchainImageViews.resize(imgCount);
        for (uint32_t i = 0; i < imgCount; i++) {
            struct {
                int sType; const void* pNext; uint32_t flags;
                VkImage image; uint32_t viewType; uint32_t format;
                uint32_t compR, compG, compB, compA; // component mapping (identity=0)
                struct { uint32_t aspectMask; uint32_t baseMipLevel; uint32_t levelCount;
                         uint32_t baseArrayLayer; uint32_t layerCount; } subresourceRange;
            } viewCI{};
            viewCI.sType = 15; // VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO
            viewCI.image = m_swapchainImages[i];
            viewCI.viewType = 1; // VK_IMAGE_VIEW_TYPE_2D
            viewCI.format = surfaceFormat;
            viewCI.subresourceRange = { 1, 0, 1, 0, 1 }; // VK_IMAGE_ASPECT_COLOR_BIT

            fn.vkCreateImageView(m_device, &viewCI, nullptr, &m_swapchainImageViews[i]);
        }

        // Create render pass (single color attachment, clear on load, store)
        struct {
            uint32_t format; uint32_t samples;
            uint32_t loadOp; uint32_t storeOp;
            uint32_t stencilLoadOp; uint32_t stencilStoreOp;
            uint32_t initialLayout; uint32_t finalLayout;
        } colorAttach{};
        colorAttach.format = surfaceFormat;
        colorAttach.samples = 1; // VK_SAMPLE_COUNT_1_BIT
        colorAttach.loadOp = 0;  // VK_ATTACHMENT_LOAD_OP_CLEAR
        colorAttach.storeOp = 0; // VK_ATTACHMENT_STORE_OP_STORE
        colorAttach.stencilLoadOp = 2; // VK_ATTACHMENT_LOAD_OP_DONT_CARE
        colorAttach.stencilStoreOp = 1; // VK_ATTACHMENT_STORE_OP_DONT_CARE
        colorAttach.initialLayout = 0; // VK_IMAGE_LAYOUT_UNDEFINED
        colorAttach.finalLayout = 1000001002; // VK_IMAGE_LAYOUT_PRESENT_SRC_KHR

        struct { uint32_t attachment; uint32_t layout; } colorRef = { 0, 2 }; // VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL

        struct {
            uint32_t flags; uint32_t pipelineBindPoint;
            uint32_t inputAttachmentCount; const void* pInputAttachments;
            uint32_t colorAttachmentCount; const void* pColorAttachments;
            const void* pResolveAttachments;
            const void* pDepthStencilAttachment;
            uint32_t preserveAttachmentCount; const uint32_t* pPreserveAttachments;
        } subpass{};
        subpass.pipelineBindPoint = 0; // VK_PIPELINE_BIND_POINT_GRAPHICS
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorRef;

        struct {
            int sType; const void* pNext; uint32_t flags;
            uint32_t attachmentCount; const void* pAttachments;
            uint32_t subpassCount; const void* pSubpasses;
            uint32_t dependencyCount; const void* pDependencies;
        } rpCI{};
        rpCI.sType = 38; // VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO
        rpCI.attachmentCount = 1;
        rpCI.pAttachments = &colorAttach;
        rpCI.subpassCount = 1;
        rpCI.pSubpasses = &subpass;

        result = fn.vkCreateRenderPass(m_device, &rpCI, nullptr, &m_renderPass);
        if (result != VK_SUCCESS) return false;

        // Create framebuffers
        m_framebuffers.resize(imgCount);
        for (uint32_t i = 0; i < imgCount; i++) {
            struct {
                int sType; const void* pNext; uint32_t flags;
                VkRenderPass renderPass; uint32_t attachmentCount; const VkImageView* pAttachments;
                uint32_t width; uint32_t height; uint32_t layers;
            } fbCI{};
            fbCI.sType = 37; // VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO
            fbCI.renderPass = m_renderPass;
            fbCI.attachmentCount = 1;
            fbCI.pAttachments = &m_swapchainImageViews[i];
            fbCI.width = m_width;
            fbCI.height = m_height;
            fbCI.layers = 1;

            fn.vkCreateFramebuffer(m_device, &fbCI, nullptr, &m_framebuffers[i]);
        }

        // Create command pool + buffers
        struct { int sType; const void* pNext; uint32_t flags; uint32_t queueFamilyIndex; } poolCI{};
        poolCI.sType = 39; // VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO
        poolCI.flags = 2;  // VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
        poolCI.queueFamilyIndex = m_graphicsQueueFamily;
        fn.vkCreateCommandPool(m_device, &poolCI, nullptr, &m_commandPool);

        m_commandBuffers.resize(imgCount);
        struct { int sType; const void* pNext;
                 VkCommandPool commandPool; uint32_t level; uint32_t commandBufferCount;
        } allocInfo{};
        allocInfo.sType = 40; // VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO — reuses same value as BEGIN
        allocInfo.sType = 44; // Correct: VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO
        allocInfo.commandPool = m_commandPool;
        allocInfo.level = 0; // VK_COMMAND_BUFFER_LEVEL_PRIMARY
        allocInfo.commandBufferCount = imgCount;
        fn.vkAllocateCommandBuffers(m_device, &allocInfo, m_commandBuffers.data());

        // Create sync objects
        struct { int sType; const void* pNext; uint32_t flags; } semCI{};
        semCI.sType = 9; // VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
        fn.vkCreateSemaphore(m_device, &semCI, nullptr, &m_imageAvailableSemaphore);
        fn.vkCreateSemaphore(m_device, &semCI, nullptr, &m_renderFinishedSemaphore);

        struct { int sType; const void* pNext; uint32_t flags; } fenceCI{};
        fenceCI.sType = 8; // VK_STRUCTURE_TYPE_FENCE_CREATE_INFO
        fenceCI.flags = 1; // VK_FENCE_CREATE_SIGNALED_BIT
        fn.vkCreateFence(m_device, &fenceCI, nullptr, &m_inFlightFence);

        return true;
    }

    void destroySwapResources() {
        if (m_inFlightFence) { fn.vkDestroyFence(m_device, m_inFlightFence, nullptr); m_inFlightFence = VK_NULL_HANDLE; }
        if (m_renderFinishedSemaphore) { fn.vkDestroySemaphore(m_device, m_renderFinishedSemaphore, nullptr); m_renderFinishedSemaphore = VK_NULL_HANDLE; }
        if (m_imageAvailableSemaphore) { fn.vkDestroySemaphore(m_device, m_imageAvailableSemaphore, nullptr); m_imageAvailableSemaphore = VK_NULL_HANDLE; }
        if (m_commandPool) { fn.vkDestroyCommandPool(m_device, m_commandPool, nullptr); m_commandPool = VK_NULL_HANDLE; }
        m_commandBuffers.clear();
        for (auto fb : m_framebuffers) { if (fb) fn.vkDestroyFramebuffer(m_device, fb, nullptr); }
        m_framebuffers.clear();
        if (m_renderPass) { fn.vkDestroyRenderPass(m_device, m_renderPass, nullptr); m_renderPass = VK_NULL_HANDLE; }
        for (auto iv : m_swapchainImageViews) { if (iv) fn.vkDestroyImageView(m_device, iv, nullptr); }
        m_swapchainImageViews.clear();
        m_swapchainImages.clear();
        if (m_swapchain) { fn.vkDestroySwapchainKHR(m_device, m_swapchain, nullptr); m_swapchain = VK_NULL_HANDLE; }
    }

    void cleanup() {
        if (m_device) {
            fn.vkDeviceWaitIdle(m_device);
            destroySwapResources();
            fn.vkDestroyDevice(m_device, nullptr);
            m_device = VK_NULL_HANDLE;
        }
        if (m_surface && m_instance) {
            fn.vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
            m_surface = VK_NULL_HANDLE;
        }
        if (m_instance) {
            fn.vkDestroyInstance(m_instance, nullptr);
            m_instance = VK_NULL_HANDLE;
        }
        if (m_vkModule) {
            FreeLibrary(m_vkModule);
            m_vkModule = nullptr;
        }
        m_initialized = false;
    }

    // ---- State ----
    HMODULE m_vkModule = nullptr;
    HWND m_hwnd = nullptr;
    bool m_initialized = false;
    float m_clearColor[4] = { 0.f, 0.f, 0.f, 1.f };
    uint32_t m_width = 0, m_height = 0;

    // Vulkan handles
    PFN_vkGetInstanceProcAddr m_vkGetInstanceProcAddr = nullptr;
    VkInstance m_instance = VK_NULL_HANDLE;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    VkQueue m_graphicsQueue = VK_NULL_HANDLE;
    uint32_t m_graphicsQueueFamily = 0;
    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
    uint32_t m_swapchainFormat = 0;
    VkRenderPass m_renderPass = VK_NULL_HANDLE;
    VkCommandPool m_commandPool = VK_NULL_HANDLE;
    VkSemaphore m_imageAvailableSemaphore = VK_NULL_HANDLE;
    VkSemaphore m_renderFinishedSemaphore = VK_NULL_HANDLE;
    VkFence m_inFlightFence = VK_NULL_HANDLE;

    std::vector<VkImage> m_swapchainImages;
    std::vector<VkImageView> m_swapchainImageViews;
    std::vector<VkFramebuffer> m_framebuffers;
    std::vector<VkCommandBuffer> m_commandBuffers;

    // Pending text state for GPU text rendering
    std::wstring m_pendingText;
    RECT m_pendingEditorRect{};
    size_t m_pendingCaretIndex = 0;
    size_t m_pendingCaretLine = 0;
    size_t m_pendingCaretColumn = 0;
};

// Factory helper
IRenderer* CreateVulkanRenderer() {
    return new VulkanRenderer();
}
