#pragma once
#include <windows.h>
#include <vulkan/vulkan.h>
#include <string>
#include <vector>
#include <memory>
#include <queue>
#include <atomic>

namespace RawrXD::Rendering {

// ============================================================================
// Vulkan Token Glyph Cache — GPU-resident font atlas and rendering state
// ============================================================================

class VulkanTokenRenderer {
public:
    struct TokenRenderJob {
        std::string text;
        uint32_t durationMs;
        uint32_t bufferOffsetX;
        uint32_t bufferOffsetY;
        float colorR, colorG, colorB, colorA;  // RGBA for token color
        std::atomic<bool> rendered{false};
    };
    
    VulkanTokenRenderer();
    ~VulkanTokenRenderer();
    
    // Initialize Vulkan resources (call once at startup)
    bool InitializeVulkan(HWND hwndTarget);
    
    // Queue a token for immediate GPU rendering
    // Returns a job handle for tracking completion
    uint64_t QueueTokenRender(const std::string& token, uint32_t positionX, 
                             uint32_t positionY, float durationMs = 0.0f);
    
    // Render all queued tokens to backbuffer this frame
    // Called from GUI thread before swap
    bool RenderPendingTokens();
    
    // Wait for token to be rendered
    bool WaitForTokenRendered(uint64_t jobHandle, uint32_t timeoutMs = 1000);
    
    // Cleanup Vulkan resources
    void ShutdownVulkan();
    
private:
    // Vulkan core handles
    VkInstance vkInstance;
    VkPhysicalDevice vkPhysicalDevice;
    VkDevice vkDevice;
    VkQueue vkGraphicsQueue;
    uint32_t vkGraphicsQueueFamily;
    
    // Swapchain and framebuffers
    VkSurfaceKHR vkSurface;
    VkSwapchainKHR vkSwapchain;
    std::vector<VkImage> vkSwapchainImages;
    std::vector<VkImageView> vkSwapchainImageViews;
    std::vector<VkFramebuffer> vkFramebuffers;
    VkRenderPass vkRenderPass;
    VkPipeline vkPipeline;
    VkPipelineLayout vkPipelineLayout;
    
    // Command execution
    VkCommandPool vkCommandPool;
    VkCommandBuffer vkCommandBuffer;
    VkSemaphore vkSemaphoreImageAvailable;
    VkSemaphore vkSemaphoreRenderFinished;
    VkFence vkFenceInFlight;
    
    // Font atlas and glyph data (resident in GPU VRAM)
    VkImage vkFontAtlasImage;
    VkDeviceMemory vkFontAtlasMemory;
    VkImageView vkFontAtlasView;
    VkSampler vkFontSampler;
    
    // Uniform buffers for per-glyph transforms
    VkBuffer vkUniformBuffer;
    VkDeviceMemory vkUniformMemory;
    
    // Descriptor sets for font atlas binding
    VkDescriptorSetLayout vkDescriptorSetLayout;
    VkDescriptorPool vkDescriptorPool;
    VkDescriptorSet vkDescriptorSet;
    
    // Glyph vertex buffer (precalculated quad geometry for entire ASCII)
    VkBuffer vkGlyphVertexBuffer;
    VkDeviceMemory vkGlyphVertexMemory;
    uint32_t vkGlyphVertexCount;
    
    // Job queue and tracking
    struct RenderJobInternal {
        uint64_t jobId;
        TokenRenderJob data;
        bool completed;
    };
    
    std::queue<RenderJobInternal> pendingJobs;
    std::vector<RenderJobInternal*> inFlightJobs;
    uint64_t nextJobId = 1;
    std::atomic<bool> vulkanReady{false};
    
    // Helper methods
    bool CreateVulkanInstance();
    bool SelectPhysicalDevice();
    bool CreateLogicalDevice();
    bool CreateSwapchain(HWND hwnd);
    bool CreateRenderPass();
    bool CreateGraphicsPipeline();
    bool CreateFontAtlas();
    bool CreateGlyphGeometry();
    bool CreateCommandBuffers();
    
    VkShaderModule LoadShader(const std::string& source);
    void RecordCommandBuffer(uint32_t imageIndex);
    void SubmitRenderJob(uint32_t imageIndex);
};

// ============================================================================
// Inline helper: Convert UTF-8 token to glyph indices
// ============================================================================

inline std::vector<uint32_t> TokenToGlyphIndices(const std::string& token) {
    std::vector<uint32_t> glyphs;
    for (unsigned char c : token) {
        if (c >= 32 && c < 127) {  // Printable ASCII
            glyphs.push_back(c - 32);  // Glyph index in atlas
        }
    }
    return glyphs;
}

// ============================================================================
// Fragment Shader (compiles to SPIR-V)
// Renders glyph quads with streaming token colors
// ============================================================================

constexpr const char* GLYPH_FRAGMENT_SHADER = R"(
#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform sampler2D fontAtlas;

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec4 fragColor;

layout(location = 0) out vec4 outColor;

void main() {
    // Sample alpha from font atlas
    float alpha = texture(fontAtlas, fragTexCoord).r;
    
    // Apply token color with font atlas alpha
    outColor = vec4(fragColor.rgb, fragColor.a * alpha);
}
)";

// ============================================================================
// Vertex Shader - generates glyph quad geometry
// ============================================================================

constexpr const char* GLYPH_VERTEX_SHADER = R"(
#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0, std140) uniform TransformBlock {
    vec2 position;
    vec2 scale;
    vec4 color;
} transform;

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inTexCoord;

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec4 fragColor;

void main() {
    // Transform glyph quad to screen space
    vec2 screenPos = inPosition * transform.scale + transform.position;
    gl_Position = vec4(screenPos * 2.0 - 1.0, 0.0, 1.0);
    
    fragTexCoord = inTexCoord;
    fragColor = transform.color;
}
)";

} // namespace RawrXD::Rendering
