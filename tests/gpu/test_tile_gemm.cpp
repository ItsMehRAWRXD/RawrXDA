// =============================================================================
// test_tile_gemm.cpp - Standalone Vulkan harness for fused_q4k_tile_gemm_vec.
// =============================================================================
// Phase 1 vertical slice. Goals (in order):
//   1. Load fused_q4k_tile_gemm_vec.spv into a fresh Vulkan compute pipeline.
//   2. Bind 3 SSBOs + 6-uint push constants exactly as the shader expects.
//   3. Dispatch on a small synthetic Q4_K tensor + FP32 activation vector.
//   4. Compare GPU output against a CPU reference (the same byte-exact ggml
//      Q4_K dequant we already proved bit-identical in q4k_dequant_parity_test).
//
// Non-goals:
//   - Touching vulkan_compute.cpp / Pyre / engine paths.
//   - Q8_1 activations.
//   - LDS / wave optimization.
//
// Build (one option, Ninja under the existing RawrXD CMake toolchain):
//   cmake -S d:\rawrxd\tests\gpu -B d:\rawrxd\tests\gpu\build -G Ninja
//   cmake --build d:\rawrxd\tests\gpu\build --target tile_gemm_test
//   d:\rawrxd\tests\gpu\build\tile_gemm_test.exe
// =============================================================================

#include <vulkan/vulkan.h>

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>
#include <string>
#include <fstream>
#include <random>

// ---------------------------------------------------------------------------
// Error-handling helpers
// ---------------------------------------------------------------------------
#define VK_CHECK(expr) do { \
    VkResult _r = (expr); \
    if (_r != VK_SUCCESS) { \
        std::fprintf(stderr, "VK_CHECK failed: %s = %d (line %d)\n", #expr, (int)_r, __LINE__); \
        std::exit(2); \
    } \
} while (0)

static void die(const char* msg) {
    std::fprintf(stderr, "FATAL: %s\n", msg);
    std::exit(2);
}

// ---------------------------------------------------------------------------
// CPU-side Q4_K helpers — must match fused_q4k_tile_gemm_vec.comp exactly.
// (Same code that q4k_dequant_parity_test.c proved bit-identical to ggml.)
// ---------------------------------------------------------------------------
static constexpr int QK_K = 256;

#pragma pack(push, 1)
struct block_q4_K {
    uint16_t d;           // half bits
    uint16_t dmin;        // half bits
    uint8_t  scales[12];
    uint8_t  qs[128];
};
#pragma pack(pop)
static_assert(sizeof(block_q4_K) == 144, "block_q4_K must be 144 bytes");

static float half_to_float(uint16_t h) {
    uint32_t s = (h >> 15) & 0x1u;
    uint32_t e = (h >> 10) & 0x1Fu;
    uint32_t m = h & 0x3FFu;
    uint32_t f;
    if (e == 0) {
        if (m == 0) f = s << 31;
        else {
            while ((m & 0x400u) == 0) { m <<= 1; e -= 1; }
            e += 1; m &= 0x3FFu;
            f = (s << 31) | ((e + 112u) << 23) | (m << 13);
        }
    } else if (e == 31) {
        f = (s << 31) | 0x7F800000u | (m << 13);
    } else {
        f = (s << 31) | ((e + 112u) << 23) | (m << 13);
    }
    float out; std::memcpy(&out, &f, 4); return out;
}

static uint16_t float_to_half(float v) {
    uint32_t f; std::memcpy(&f, &v, 4);
    uint32_t s = (f >> 31) & 0x1u;
    int32_t  e = (int32_t)((f >> 23) & 0xFFu) - 127 + 15;
    uint32_t m = f & 0x7FFFFFu;
    uint16_t h;
    if (e <= 0) {
        if (e < -10) h = (uint16_t)(s << 15);
        else {
            m |= 0x800000u;
            uint32_t shift = (uint32_t)(14 - e);
            uint32_t round = (m >> (shift - 1)) & 1u;
            h = (uint16_t)((s << 15) | ((m >> shift) + round));
        }
    } else if (e >= 31) {
        h = (uint16_t)((s << 15) | 0x7C00u | (m ? 1u : 0u));
    } else {
        uint32_t round = (m >> 12) & 1u;
        h = (uint16_t)((s << 15) | ((uint32_t)e << 10) | ((m >> 13) + round));
    }
    return h;
}

static inline void get_scale_min_k4(int j, const uint8_t* q, uint8_t& d, uint8_t& m) {
    if (j < 4) { d = q[j] & 63;        m = q[j + 4] & 63; }
    else       { d = (q[j+4] & 0xF) | ((q[j-4] >> 6) << 4);
                 m = (q[j+4] >>  4)  | ((q[j  ] >> 6) << 4); }
}

static void dequantize_block_q4_K(const block_q4_K& x, float* y) {
    const uint8_t* q = x.qs;
    const float d   = half_to_float(x.d);
    const float min = half_to_float(x.dmin);
    int is = 0; uint8_t sc, m;
    for (int j = 0; j < QK_K; j += 64) {
        get_scale_min_k4(is + 0, x.scales, sc, m);
        const float d1 = d * sc;  const float m1 = min * m;
        get_scale_min_k4(is + 1, x.scales, sc, m);
        const float d2 = d * sc;  const float m2 = min * m;
        for (int l = 0; l < 32; ++l) y[j + l]      = d1 * (q[l] & 0xF) - m1;
        for (int l = 0; l < 32; ++l) y[j + 32 + l] = d2 * (q[l] >>  4) - m2;
        q += 32; is += 2;
    }
}

static void synth_block(block_q4_K& b, uint32_t seed) {
    uint32_t s = seed * 2654435761u + 1u;
    b.d    = float_to_half(0.0125f);
    b.dmin = float_to_half(0.0040f);
    for (int i = 0; i < 12; ++i)  { s = s * 1664525u + 1013904223u; b.scales[i] = (uint8_t)(s >> 24); }
    for (int i = 0; i < 128; ++i) { s = s * 1664525u + 1013904223u; b.qs[i]     = (uint8_t)(s >> 24); }
}

// ---------------------------------------------------------------------------
// Vulkan helpers
// ---------------------------------------------------------------------------
static std::vector<uint32_t> read_spv(const char* path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) { std::fprintf(stderr, "cannot open SPIR-V: %s\n", path); std::exit(2); }
    auto sz = (size_t)f.tellg();
    if (sz == 0 || (sz % 4) != 0) die("bad SPIR-V size");
    std::vector<uint32_t> code(sz / 4);
    f.seekg(0); f.read(reinterpret_cast<char*>(code.data()), (std::streamsize)sz);
    return code;
}

struct Buf {
    VkBuffer       buf = VK_NULL_HANDLE;
    VkDeviceMemory mem = VK_NULL_HANDLE;
    void*          map = nullptr;
    VkDeviceSize   size = 0;
};

static uint32_t find_mem_type(VkPhysicalDevice phys, uint32_t typeBits, VkMemoryPropertyFlags want) {
    VkPhysicalDeviceMemoryProperties mp{};
    vkGetPhysicalDeviceMemoryProperties(phys, &mp);
    for (uint32_t i = 0; i < mp.memoryTypeCount; ++i) {
        if ((typeBits & (1u << i)) && (mp.memoryTypes[i].propertyFlags & want) == want) return i;
    }
    die("no suitable memory type"); return 0;
}

static Buf make_host_buffer(VkDevice dev, VkPhysicalDevice phys, VkDeviceSize size) {
    Buf b{}; b.size = size;
    VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bi.size = size;
    bi.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VK_CHECK(vkCreateBuffer(dev, &bi, nullptr, &b.buf));

    VkMemoryRequirements mr{};
    vkGetBufferMemoryRequirements(dev, b.buf, &mr);

    VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ai.allocationSize = mr.size;
    ai.memoryTypeIndex = find_mem_type(phys, mr.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    VK_CHECK(vkAllocateMemory(dev, &ai, nullptr, &b.mem));
    VK_CHECK(vkBindBufferMemory(dev, b.buf, b.mem, 0));
    VK_CHECK(vkMapMemory(dev, b.mem, 0, size, 0, &b.map));
    return b;
}

// ---------------------------------------------------------------------------
// Test driver
// ---------------------------------------------------------------------------
int main(int argc, char** argv) {
    // Fixed problem geometry for the slice.
    const uint32_t M = 64;       // output rows
    const uint32_t N = 1;        // single token
    const uint32_t K = 256;      // 1 Q4_K block per row
    const uint32_t kblocks = K / 256;

    const char* spv_path =
        (argc > 1) ? argv[1]
                   : "d:\\rawrxd\\src\\gpu\\shaders\\_spv\\fused_q4k_tile_gemm_vec.spv";

    std::printf("[harness] M=%u N=%u K=%u kblocks=%u\n", M, N, K, kblocks);
    std::printf("[harness] SPIR-V: %s\n", spv_path);

    // ---- 1. Instance --------------------------------------------------------
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.pApplicationName = "tile_gemm_test";
    app.apiVersion = VK_API_VERSION_1_2;
    VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ici.pApplicationInfo = &app;
    VkInstance inst = VK_NULL_HANDLE;
    VK_CHECK(vkCreateInstance(&ici, nullptr, &inst));

    // ---- 2. Pick first physical device with compute -------------------------
    uint32_t devc = 0;
    VK_CHECK(vkEnumeratePhysicalDevices(inst, &devc, nullptr));
    if (devc == 0) die("no Vulkan devices");
    std::vector<VkPhysicalDevice> devs(devc);
    VK_CHECK(vkEnumeratePhysicalDevices(inst, &devc, devs.data()));

    VkPhysicalDevice phys = VK_NULL_HANDLE;
    uint32_t qfam = UINT32_MAX;
    for (auto d : devs) {
        uint32_t qc = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(d, &qc, nullptr);
        std::vector<VkQueueFamilyProperties> qprops(qc);
        vkGetPhysicalDeviceQueueFamilyProperties(d, &qc, qprops.data());
        for (uint32_t i = 0; i < qc; ++i) {
            if (qprops[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
                phys = d; qfam = i; break;
            }
        }
        if (phys) break;
    }
    if (!phys) die("no compute queue family");

    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(phys, &props);
    std::printf("[harness] device: %s (api 0x%x)\n", props.deviceName, props.apiVersion);

    // ---- 3. Logical device with required 8/16-bit + float16/int8 features --
    VkPhysicalDeviceVulkan12Features f12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
    f12.storageBuffer8BitAccess           = VK_TRUE;
    f12.uniformAndStorageBuffer8BitAccess = VK_TRUE;
    f12.shaderFloat16                     = VK_TRUE;
    f12.shaderInt8                        = VK_TRUE;

    VkPhysicalDeviceVulkan11Features f11{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES};
    f11.storageBuffer16BitAccess           = VK_TRUE;
    f11.uniformAndStorageBuffer16BitAccess = VK_TRUE;
    f11.pNext = &f12;

    VkPhysicalDeviceFeatures2 f2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
    f2.pNext = &f11;

    float qprio = 1.0f;
    VkDeviceQueueCreateInfo qci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    qci.queueFamilyIndex = qfam;
    qci.queueCount = 1;
    qci.pQueuePriorities = &qprio;

    VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    dci.queueCreateInfoCount = 1;
    dci.pQueueCreateInfos = &qci;
    dci.pNext = &f2;

    VkDevice dev = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDevice(phys, &dci, nullptr, &dev));

    VkQueue queue = VK_NULL_HANDLE;
    vkGetDeviceQueue(dev, qfam, 0, &queue);

    // ---- 4. Buffers ---------------------------------------------------------
    const VkDeviceSize a_bytes = (VkDeviceSize)M * kblocks * sizeof(block_q4_K);
    const VkDeviceSize b_bytes = (VkDeviceSize)K * sizeof(float);
    const VkDeviceSize d_bytes = (VkDeviceSize)M * sizeof(float);

    Buf bufA = make_host_buffer(dev, phys, a_bytes);
    Buf bufB = make_host_buffer(dev, phys, b_bytes);
    Buf bufD = make_host_buffer(dev, phys, d_bytes);

    // Fill weights (synthetic Q4_K blocks, deterministic per row).
    auto* a_host = static_cast<block_q4_K*>(bufA.map);
    for (uint32_t r = 0; r < M; ++r) {
        for (uint32_t b = 0; b < kblocks; ++b) {
            synth_block(a_host[r * kblocks + b], r * 7919u + b);
        }
    }
    // Fill activations (deterministic float vector in [-1, 1]).
    auto* b_host = static_cast<float*>(bufB.map);
    {
        std::mt19937 rng(0xC0FFEEu);
        std::uniform_real_distribution<float> U(-1.0f, 1.0f);
        for (uint32_t i = 0; i < K; ++i) b_host[i] = U(rng);
    }
    // Zero output.
    std::memset(bufD.map, 0, (size_t)d_bytes);

    // ---- 5. Shader module ---------------------------------------------------
    auto code = read_spv(spv_path);
    VkShaderModuleCreateInfo smci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    smci.codeSize = code.size() * sizeof(uint32_t);
    smci.pCode = code.data();
    VkShaderModule shader = VK_NULL_HANDLE;
    VK_CHECK(vkCreateShaderModule(dev, &smci, nullptr, &shader));

    // ---- 6. Descriptor set layout (3 storage buffers) -----------------------
    VkDescriptorSetLayoutBinding binds[3]{};
    for (int i = 0; i < 3; ++i) {
        binds[i].binding         = (uint32_t)i;
        binds[i].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        binds[i].descriptorCount = 1;
        binds[i].stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT;
    }
    VkDescriptorSetLayoutCreateInfo dslci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    dslci.bindingCount = 3; dslci.pBindings = binds;
    VkDescriptorSetLayout dsl = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDescriptorSetLayout(dev, &dslci, nullptr, &dsl));

    // ---- 7. Pipeline layout (6 uint push constants) -------------------------
    VkPushConstantRange pcr{};
    pcr.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pcr.offset = 0;
    pcr.size   = 6 * sizeof(uint32_t);

    VkPipelineLayoutCreateInfo plci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    plci.setLayoutCount = 1; plci.pSetLayouts = &dsl;
    plci.pushConstantRangeCount = 1; plci.pPushConstantRanges = &pcr;
    VkPipelineLayout playout = VK_NULL_HANDLE;
    VK_CHECK(vkCreatePipelineLayout(dev, &plci, nullptr, &playout));

    // ---- 8. Compute pipeline -------------------------------------------------
    VkPipelineShaderStageCreateInfo ss{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    ss.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    ss.module = shader;
    ss.pName = "main";

    VkComputePipelineCreateInfo cpci{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    cpci.stage = ss; cpci.layout = playout;
    VkPipeline pipe = VK_NULL_HANDLE;
    VK_CHECK(vkCreateComputePipelines(dev, VK_NULL_HANDLE, 1, &cpci, nullptr, &pipe));

    // ---- 9. Descriptor pool + set --------------------------------------------
    VkDescriptorPoolSize psz{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3};
    VkDescriptorPoolCreateInfo dpci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    dpci.maxSets = 1; dpci.poolSizeCount = 1; dpci.pPoolSizes = &psz;
    VkDescriptorPool pool = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDescriptorPool(dev, &dpci, nullptr, &pool));

    VkDescriptorSetAllocateInfo dsai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    dsai.descriptorPool = pool; dsai.descriptorSetCount = 1; dsai.pSetLayouts = &dsl;
    VkDescriptorSet dset = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateDescriptorSets(dev, &dsai, &dset));

    VkDescriptorBufferInfo bi[3] = {
        {bufA.buf, 0, VK_WHOLE_SIZE},
        {bufB.buf, 0, VK_WHOLE_SIZE},
        {bufD.buf, 0, VK_WHOLE_SIZE},
    };
    VkWriteDescriptorSet ws[3]{};
    for (int i = 0; i < 3; ++i) {
        ws[i].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        ws[i].dstSet          = dset;
        ws[i].dstBinding      = (uint32_t)i;
        ws[i].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        ws[i].descriptorCount = 1;
        ws[i].pBufferInfo     = &bi[i];
    }
    vkUpdateDescriptorSets(dev, 3, ws, 0, nullptr);

    // ---- 10. Command buffer + record ----------------------------------------
    VkCommandPoolCreateInfo cpi{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    cpi.queueFamilyIndex = qfam;
    VkCommandPool cpool = VK_NULL_HANDLE;
    VK_CHECK(vkCreateCommandPool(dev, &cpi, nullptr, &cpool));

    VkCommandBufferAllocateInfo cbai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cbai.commandPool = cpool; cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; cbai.commandBufferCount = 1;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateCommandBuffers(dev, &cbai, &cmd));

    VkCommandBufferBeginInfo cbi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    cbi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(cmd, &cbi));

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipe);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, playout, 0, 1, &dset, 0, nullptr);

    // Push constants: M, N, K, stride_a_blocks, stride_b, stride_out
    uint32_t pcv[6] = { M, N, K, kblocks, K, M };
    vkCmdPushConstants(cmd, playout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pcv), pcv);

    // groups_x covers M / 64 (workgroup local_size_x = 64).
    uint32_t gx = (M + 63) / 64;
    vkCmdDispatch(cmd, gx, 1, 1);

    // Make output visible to host read.
    VkBufferMemoryBarrier bmb{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
    bmb.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    bmb.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    bmb.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bmb.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bmb.buffer = bufD.buf;
    bmb.offset = 0; bmb.size = VK_WHOLE_SIZE;
    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0,
        0, nullptr, 1, &bmb, 0, nullptr);

    VK_CHECK(vkEndCommandBuffer(cmd));

    // ---- 11. Submit + wait --------------------------------------------------
    VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    VkFence fence = VK_NULL_HANDLE;
    VK_CHECK(vkCreateFence(dev, &fci, nullptr, &fence));

    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.commandBufferCount = 1; si.pCommandBuffers = &cmd;
    VK_CHECK(vkQueueSubmit(queue, 1, &si, fence));
    VK_CHECK(vkWaitForFences(dev, 1, &fence, VK_TRUE, UINT64_MAX));

    // ---- 12. CPU reference ---------------------------------------------------
    std::vector<float> cpu_out(M, 0.0f);
    std::vector<float> tmp(K);
    for (uint32_t r = 0; r < M; ++r) {
        for (uint32_t b = 0; b < kblocks; ++b) {
            dequantize_block_q4_K(a_host[r * kblocks + b], tmp.data() + b * 256);
        }
        float acc = 0.0f;
        for (uint32_t k = 0; k < K; ++k) acc += tmp[k] * b_host[k];
        cpu_out[r] = acc;
    }

    // ---- 13. Compare ---------------------------------------------------------
    auto* gpu_out = static_cast<float*>(bufD.map);
    float worst = 0.0f, worst_rel = 0.0f;
    int   fails = 0;
    bool  any_nonzero = false;
    for (uint32_t r = 0; r < M; ++r) {
        float diff = std::fabs(gpu_out[r] - cpu_out[r]);
        float ref  = std::fabs(cpu_out[r]);
        if (gpu_out[r] != 0.0f) any_nonzero = true;
        if (diff > worst) worst = diff;
        float rel = (ref > 1e-30f) ? diff / ref : diff;
        if (rel > worst_rel) worst_rel = rel;
        if (diff > 1e-3f && rel > 1e-3f) ++fails;
    }
    std::printf("[harness] any_nonzero=%d  worst_abs=%.6g  worst_rel=%.6g  fails=%d/%u\n",
                (int)any_nonzero, worst, worst_rel, fails, M);
    for (uint32_t r = 0; r < std::min<uint32_t>(M, 4); ++r) {
        std::printf("  row %u: gpu=%.6f cpu=%.6f diff=%.3e\n",
                    r, gpu_out[r], cpu_out[r], gpu_out[r] - cpu_out[r]);
    }

    int rc = (any_nonzero && fails == 0) ? 0 : 1;
    std::printf(rc == 0 ? "PASS\n" : "FAIL\n");

    // ---- 14. Cleanup ---------------------------------------------------------
    vkDestroyFence(dev, fence, nullptr);
    vkDestroyCommandPool(dev, cpool, nullptr);
    vkDestroyDescriptorPool(dev, pool, nullptr);
    vkDestroyPipeline(dev, pipe, nullptr);
    vkDestroyPipelineLayout(dev, playout, nullptr);
    vkDestroyDescriptorSetLayout(dev, dsl, nullptr);
    vkDestroyShaderModule(dev, shader, nullptr);
    auto destroy_buf = [&](Buf& b){
        vkUnmapMemory(dev, b.mem);
        vkDestroyBuffer(dev, b.buf, nullptr);
        vkFreeMemory(dev, b.mem, nullptr);
    };
    destroy_buf(bufA); destroy_buf(bufB); destroy_buf(bufD);
    vkDestroyDevice(dev, nullptr);
    vkDestroyInstance(inst, nullptr);
    return rc;
}
