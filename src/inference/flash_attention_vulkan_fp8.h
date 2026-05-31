// ============================================================================
// flash_attention_vulkan_fp8.h — Parallel Vulkan + FP8 attention dispatch
// ============================================================================
// Intentionally separate from flash_attention_bridge.h (FP32 CPU path).
//
// Push constants: C++ `FlashAttentionFP8PushConstants` must match the GLSL
// `layout(push_constant)` block in `shaders/flash_attention_fp8_e4m3_dequant_template.comp`
// byte-for-byte (Vulkan std430 scalar rules — no hidden padding between members below).
//
// Descriptor set (compute): 4 × storage buffer, single set, bindings 0–3
//   0 = Q (read), 1 = K (read), 2 = V (read), 3 = O (read/write)
// Use `VK_SHADER_STAGE_COMPUTE_BIT` for all.
//
// Pager / VirtualAlloc → GPU:
//   `SovereignPager` pages are CPU-resident (`VirtualAllocExNuma`). Vulkan does not
//   consume a raw `void*` from the pager as a device buffer. Typical sovereign paths:
//   (a) `VkBuffer` backed by `HOST_VISIBLE | HOST_COHERENT` memory, `memcpy` from
//       `ExpertWeights()` into mapped pointer, then dispatch; or
//   (b) `vkCmdCopyBuffer` from a host-visible staging `VkBuffer` to `DEVICE_LOCAL`
//       Q/K/V/O; or (c) Win32 external-memory import (advanced). Document which lane
//       you use in the integration that replaces `DispatchFlashAttentionVulkanFP8` stub.
// ============================================================================

#pragma once

#include <cstdint>

extern "C"
{
    typedef struct VkBuffer_T* VkBuffer;
}

namespace RawrXD
{

/// Compute storage-buffer bindings (single descriptor set).
inline constexpr uint32_t kFlashAttentionFP8BindingQ = 0;
inline constexpr uint32_t kFlashAttentionFP8BindingK = 1;
inline constexpr uint32_t kFlashAttentionFP8BindingV = 2;
inline constexpr uint32_t kFlashAttentionFP8BindingO = 3;

/// `FlashAttentionFP8PushConstants::flags`
inline constexpr uint32_t kFlashAttentionFP8FlagDecodeNoRefusal = 1u << 0;
/// Reserved: force WMMA fast path after parity with reference decode (future).
inline constexpr uint32_t kFlashAttentionFP8FlagReservedWmma = 1u << 1;

/// Push constants for FP8 attention — order and types must match GLSL `Push` block.
struct FlashAttentionFP8PushConstants
{
    uint32_t seqLenM = 0;
    uint32_t seqLenN = 0;
    uint32_t headDim = 0;
    uint32_t numHeads = 0;
    uint32_t numKVHeads = 0;
    uint32_t flags = 0;
    float qScale = 1.f;
    float kScale = 1.f;
    float vScale = 1.f;
};

static_assert(sizeof(FlashAttentionFP8PushConstants) == 36u,
              "FlashAttentionFP8PushConstants must stay in sync with GLSL push_constant layout");
static_assert(sizeof(FlashAttentionFP8PushConstants) <= 128u,
              "Vulkan push constant range must cover FlashAttentionFP8PushConstants");

bool DispatchFlashAttentionVulkanFP8(VkBuffer qBuffer, VkBuffer kBuffer, VkBuffer vBuffer, VkBuffer oBuffer,
                                     const FlashAttentionFP8PushConstants& constants);

}  // namespace RawrXD
