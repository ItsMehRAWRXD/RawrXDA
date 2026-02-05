// Minimal Vulkan compute stub - actual GPU code deferred to later phase
// This allows InferenceEngine to link without full Vulkan SDK dependencies

#include "../include/vulkan_compute_stub.hpp"

VulkanCompute::VulkanCompute() = default;
VulkanCompute::~VulkanCompute() = default;

bool VulkanCompute::Initialize() {
    return false;
}

void VulkanCompute::Cleanup() {
}
