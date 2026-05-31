// =============================================================================
// test_q4k_q8_1.cpp - Phase 2A vertical slice harness.
//   Q4_K weights x Q8_1 activations -> FP32 output, GPU vs CPU parity.
// Uses the same locked ABI as the FP32 slice but binding 1 is now Q8_1 SSBO.
// =============================================================================

#include <vulkan/vulkan.h>

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>
#include <fstream>
#include <chrono>
#include <random>
#include <string>

#define VK_CHECK(expr) do { VkResult _r = (expr); if (_r != VK_SUCCESS) { \
    std::fprintf(stderr, "VK_CHECK failed: %s = %d (line %d)\n", #expr, (int)_r, __LINE__); \
    std::exit(2); } } while (0)

static void die(const char* msg) { std::fprintf(stderr, "FATAL: %s\n", msg); std::exit(2); }

// ---------------------------------------------------------------------------
// Q4_0, Q4_K, Q5_K + Q8_1 byte-exact ggml layouts and dequant helpers
// ---------------------------------------------------------------------------
static constexpr int QK4_0 = 32;
static constexpr int QK_K  = 256;
static constexpr int QK8_1 = 32;

#pragma pack(push, 1)
struct block_q4_0 {
    uint16_t d;
    uint8_t  qs[QK4_0 / 2];  // 16 bytes, two 4-bit values per byte
};
struct block_q4_K {
    uint16_t d;
    uint16_t dmin;
    uint8_t  scales[12];
    uint8_t  qs[128];
};
struct block_q5_K {
    uint16_t d;
    uint16_t dmin;
    uint8_t  scales[12];
    uint8_t  qh[32];
    uint8_t  qs[128];
};
struct block_q6_K {
    uint8_t ql[128];      // lower 4 bits
    uint8_t qh[64];       // upper 2 bits
    int8_t  scales[16];   // signed scales
    uint16_t d;
};
struct block_q8_1 {
    uint16_t d;
    uint16_t s;
    int8_t   qs[QK8_1];
};
#pragma pack(pop)
static_assert(sizeof(block_q4_0) == 18,  "block_q4_0 must be 18 bytes");
static_assert(sizeof(block_q4_K) == 144, "block_q4_K must be 144 bytes");
static_assert(sizeof(block_q5_K) == 176, "block_q5_K must be 176 bytes");
static_assert(sizeof(block_q6_K) == 210, "block_q6_K must be 210 bytes");
static_assert(sizeof(block_q8_1) == 36,  "block_q8_1 must be 36 bytes");

static float half_to_float(uint16_t h) {
    uint32_t s = (h >> 15) & 0x1u, e = (h >> 10) & 0x1Fu, m = h & 0x3FFu, f;
    if (e == 0) {
        if (m == 0) f = s << 31;
        else { while ((m & 0x400u) == 0) { m <<= 1; e -= 1; } e += 1; m &= 0x3FFu;
               f = (s << 31) | ((e + 112u) << 23) | (m << 13); }
    } else if (e == 31) f = (s << 31) | 0x7F800000u | (m << 13);
    else                f = (s << 31) | ((e + 112u) << 23) | (m << 13);
    float out; std::memcpy(&out, &f, 4); return out;
}
static uint16_t float_to_half(float v) {
    uint32_t f; std::memcpy(&f, &v, 4);
    uint32_t s = (f >> 31) & 0x1u;
    int32_t  e = (int32_t)((f >> 23) & 0xFFu) - 127 + 15;
    uint32_t m = f & 0x7FFFFFu; uint16_t h;
    if (e <= 0) {
        if (e < -10) h = (uint16_t)(s << 15);
        else { m |= 0x800000u; uint32_t shift = (uint32_t)(14 - e);
               uint32_t round = (m >> (shift - 1)) & 1u;
               h = (uint16_t)((s << 15) | ((m >> shift) + round)); }
    } else if (e >= 31) h = (uint16_t)((s << 15) | 0x7C00u | (m ? 1u : 0u));
    else { uint32_t round = (m >> 12) & 1u;
           h = (uint16_t)((s << 15) | ((uint32_t)e << 10) | ((m >> 13) + round)); }
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

static void dequantize_block_q4_0(const block_q4_0& x, float* y) {
    const float d = half_to_float(x.d);
    for (int l = 0; l < QK4_0; ++l) {
        uint8_t qbyte = x.qs[l >> 1];
        uint8_t nib = (l & 1) ? (qbyte >> 4) : (qbyte & 0xF);
        y[l] = (float(int(nib) - 8)) * d;
    }
}

static void dequantize_block_q5_K(const block_q5_K& x, float* y) {
    const float d   = half_to_float(x.d);
    const float dmin = half_to_float(x.dmin);
    int is = 0;
    for (int seg = 0; seg < 4; ++seg) {
        for (int half = 0; half < 2; ++half) {
            uint8_t sc, mn;
            get_scale_min_k4(is, x.scales, sc, mn);
            const float fsc = d * sc;
            const float fmn = dmin * mn;
            for (int l = 0; l < 32; ++l) {
                uint8_t qbyte = x.qs[seg * 32 + l];
                uint8_t qlow = half ? (qbyte >> 4) : (qbyte & 0xF);
                uint8_t bitmask = (half ? 2 : 1) << (seg << 1);
                uint8_t qh_add = (x.qh[l] & bitmask) ? 16 : 0;
                uint8_t q = qlow + qh_add;
                int k = seg * 64 + half * 32 + l;
                y[k] = fsc * float(q) - fmn;
            }
            ++is;
        }
    }
}

static void dequantize_block_q6_K(const block_q6_K& x, float* y) {
    const float d = half_to_float(x.d);
    const uint8_t* ql = x.ql;
    const uint8_t* qh = x.qh;
    const int8_t*  sc = x.scales;
    
    for (int n = 0; n < QK_K; n += 128) {
        for (int l = 0; l < 32; ++l) {
            int is = l / 16;
            const int8_t q1 = (int8_t)((ql[l +  0] & 0xF) | (((qh[l] >> 0) & 3) << 4)) - 32;
            const int8_t q2 = (int8_t)((ql[l + 32] & 0xF) | (((qh[l] >> 2) & 3) << 4)) - 32;
            const int8_t q3 = (int8_t)((ql[l +  0]  >> 4) | (((qh[l] >> 4) & 3) << 4)) - 32;
            const int8_t q4 = (int8_t)((ql[l + 32]  >> 4) | (((qh[l] >> 6) & 3) << 4)) - 32;
            y[l +  0] = d * sc[is + 0] * q1;
            y[l + 32] = d * sc[is + 2] * q2;
            y[l + 64] = d * sc[is + 4] * q3;
            y[l + 96] = d * sc[is + 6] * q4;
        }
        y  += 128;
        ql += 64;
        qh += 32;
        sc += 8;
    }
}

// ggml-style quantize_row_q8_1: per 32-element block compute amax, derive d,
// q[i] = round(x[i]/d), s = d * sum(q[i]).
static void quantize_row_q8_1(const float* x, block_q8_1* y, int k) {
    int nb = k / QK8_1;
    for (int b = 0; b < nb; ++b) {
        float amax = 0.0f;
        for (int i = 0; i < QK8_1; ++i) { float a = std::fabs(x[b*QK8_1 + i]); if (a > amax) amax = a; }
        float d = amax / 127.0f;
        float id = (d != 0.0f) ? 1.0f / d : 0.0f;
        int   isum = 0;
        for (int i = 0; i < QK8_1; ++i) {
            int q = (int)std::lrintf(x[b*QK8_1 + i] * id);
            if (q >  127) q =  127;
            if (q < -128) q = -128;
            y[b].qs[i] = (int8_t)q;
            isum += q;
        }
        y[b].d = float_to_half(d);
        y[b].s = float_to_half(d * (float)isum);
    }
}

static void synth_block(block_q4_K& b, uint32_t seed) {
    uint32_t s = seed * 2654435761u + 1u;
    b.d    = float_to_half(0.0125f);
    b.dmin = float_to_half(0.0040f);
    for (int i = 0; i < 12; ++i)  { s = s * 1664525u + 1013904223u; b.scales[i] = (uint8_t)(s >> 24); }
    for (int i = 0; i < 128; ++i) { s = s * 1664525u + 1013904223u; b.qs[i]     = (uint8_t)(s >> 24); }
}

static void synth_block(block_q4_0& b, uint32_t seed) {
    uint32_t s = seed * 2654435761u + 1u;
    b.d = float_to_half(0.0125f);
    for (int i = 0; i < 16; ++i) { s = s * 1664525u + 1013904223u; b.qs[i] = (uint8_t)(s >> 24); }
}

static void synth_block(block_q5_K& b, uint32_t seed) {
    uint32_t s = seed * 2654435761u + 1u;
    b.d    = float_to_half(0.0125f);
    b.dmin = float_to_half(0.0040f);
    for (int i = 0; i < 12; ++i)  { s = s * 1664525u + 1013904223u; b.scales[i] = (uint8_t)(s >> 24); }
    for (int i = 0; i < 32; ++i)  { s = s * 1664525u + 1013904223u; b.qh[i]     = (uint8_t)(s >> 24); }
    for (int i = 0; i < 128; ++i) { s = s * 1664525u + 1013904223u; b.qs[i]     = (uint8_t)(s >> 24); }
}

static void synth_block(block_q6_K& b, uint32_t seed) {
    uint32_t s = seed * 2654435761u + 1u;
    for (int i = 0; i < 128; ++i) { s = s * 1664525u + 1013904223u; b.ql[i] = (uint8_t)(s >> 24); }
    for (int i = 0; i < 64; ++i)  { s = s * 1664525u + 1013904223u; b.qh[i] = (uint8_t)(s >> 24); }
    for (int i = 0; i < 16; ++i)  { s = s * 1664525u + 1013904223u; b.scales[i] = (int8_t)((s >> 24) & 0x7F) - 64; }
    b.d = float_to_half(0.0125f);
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

struct Buf { VkBuffer buf=VK_NULL_HANDLE; VkDeviceMemory mem=VK_NULL_HANDLE; void* map=nullptr; VkDeviceSize size=0; };

static uint32_t find_mem_type(VkPhysicalDevice phys, uint32_t typeBits, VkMemoryPropertyFlags want) {
    VkPhysicalDeviceMemoryProperties mp{}; vkGetPhysicalDeviceMemoryProperties(phys, &mp);
    for (uint32_t i = 0; i < mp.memoryTypeCount; ++i)
        if ((typeBits & (1u << i)) && (mp.memoryTypes[i].propertyFlags & want) == want) return i;
    die("no suitable memory type"); return 0;
}
static Buf make_host_buffer(VkDevice dev, VkPhysicalDevice phys, VkDeviceSize size) {
    Buf b{}; b.size = size;
    VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bi.size = size; bi.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT; bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VK_CHECK(vkCreateBuffer(dev, &bi, nullptr, &b.buf));
    VkMemoryRequirements mr{}; vkGetBufferMemoryRequirements(dev, b.buf, &mr);
    VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ai.allocationSize = mr.size;
    ai.memoryTypeIndex = find_mem_type(phys, mr.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    VK_CHECK(vkAllocateMemory(dev, &ai, nullptr, &b.mem));
    VK_CHECK(vkBindBufferMemory(dev, b.buf, b.mem, 0));
    VK_CHECK(vkMapMemory(dev, b.mem, 0, size, 0, &b.map));
    return b;
}

int main(int argc, char** argv) {
    uint32_t M = 64, N = 1, K = 256;
    int bench_iters = 0;
    const char* spv_path = nullptr;
    std::string kernel_name = "q4k_q8_1";
    for (int ai = 1; ai < argc; ++ai) {
        if (std::strcmp(argv[ai], "--M") == 0 && ai + 1 < argc) { M = (uint32_t)std::atoi(argv[++ai]); }
        else if (std::strcmp(argv[ai], "--K") == 0 && ai + 1 < argc) { K = (uint32_t)std::atoi(argv[++ai]); }
        else if (std::strcmp(argv[ai], "--bench") == 0 && ai + 1 < argc) { bench_iters = std::atoi(argv[++ai]); }
        else if (std::strcmp(argv[ai], "--spv") == 0 && ai + 1 < argc) { spv_path = argv[++ai]; }
        else if (std::strcmp(argv[ai], "--kernel") == 0 && ai + 1 < argc) { kernel_name = argv[++ai]; }
        else if (!spv_path) { spv_path = argv[ai]; }
    }
    if (!spv_path) spv_path = "d:\\rawrxd\\src\\gpu\\shaders\\_spv\\fused_q4k_q8_1_gemv.spv";
    if (K % 256 != 0) { std::fprintf(stderr, "K must be multiple of 256\n"); return 1; }
    const uint32_t kblocks    = K / 256;
    const uint32_t kblocks_q8 = K / 32;

    std::printf("[harness] Q4_K x Q8_1: M=%u N=%u K=%u kblocks=%u kblocks_q8=%u\n",
                M, N, K, kblocks, kblocks_q8);
    std::printf("[harness] kernel: %s\n", kernel_name.c_str());
    std::printf("[harness] SPIR-V: %s\n", spv_path);

    // Determine weight format from kernel name
    bool is_q4_0 = (kernel_name == "q4_0_u32");
    bool is_q5_k = (kernel_name == "q5_k_u32");
    bool is_q6_k = (kernel_name == "q6_k_u32");
    bool is_q4_k = (kernel_name == "q4k_q8_1_u32" || kernel_name == "q4k_q8_1");
    
    // Compute buffer sizes based on format
    const uint32_t kblocks_q4_0 = K / QK4_0;  // blocks for Q4_0
    const uint32_t kblocks_q5_k = K / QK_K;  // blocks for Q5_K (same as Q4_K)
    const uint32_t kblocks_q6_k = K / QK_K;  // blocks for Q6_K (same as Q4_K)
    
    VkDeviceSize a_bytes;
    if (is_q4_0) {
        a_bytes = (VkDeviceSize)M * kblocks_q4_0 * sizeof(block_q4_0);
    } else if (is_q5_k) {
        a_bytes = (VkDeviceSize)M * kblocks_q5_k * sizeof(block_q5_K);
    } else if (is_q6_k) {
        a_bytes = (VkDeviceSize)M * kblocks_q6_k * sizeof(block_q6_K);
    } else {
        a_bytes = (VkDeviceSize)M * kblocks * sizeof(block_q4_K);
    }
    const VkDeviceSize b_bytes = (VkDeviceSize)kblocks_q8 * sizeof(block_q8_1);
    const VkDeviceSize d_bytes = (VkDeviceSize)M * sizeof(float);

    // Instance
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.pApplicationName = "test_q4k_q8_1"; app.apiVersion = VK_API_VERSION_1_2;
    VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO}; ici.pApplicationInfo = &app;
    VkInstance inst = VK_NULL_HANDLE; VK_CHECK(vkCreateInstance(&ici, nullptr, &inst));

    // Pick first compute device
    uint32_t devc = 0; VK_CHECK(vkEnumeratePhysicalDevices(inst, &devc, nullptr));
    if (devc == 0) die("no Vulkan devices");
    std::vector<VkPhysicalDevice> devs(devc);
    VK_CHECK(vkEnumeratePhysicalDevices(inst, &devc, devs.data()));
    VkPhysicalDevice phys = VK_NULL_HANDLE; uint32_t qfam = UINT32_MAX;
    for (auto d : devs) {
        uint32_t qc = 0; vkGetPhysicalDeviceQueueFamilyProperties(d, &qc, nullptr);
        std::vector<VkQueueFamilyProperties> qprops(qc);
        vkGetPhysicalDeviceQueueFamilyProperties(d, &qc, qprops.data());
        for (uint32_t i = 0; i < qc; ++i) if (qprops[i].queueFlags & VK_QUEUE_COMPUTE_BIT) { phys = d; qfam = i; break; }
        if (phys) break;
    }
    if (!phys) die("no compute queue family");
    VkPhysicalDeviceProperties props{}; vkGetPhysicalDeviceProperties(phys, &props);
    VkPhysicalDeviceSubgroupProperties sgp{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES};
    VkPhysicalDeviceProperties2 props2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2}; props2.pNext = &sgp;
    vkGetPhysicalDeviceProperties2(phys, &props2);
    std::printf("[harness] device: %s  subgroupSize=%u\n", props.deviceName, sgp.subgroupSize);

    // Device with required features
    VkPhysicalDeviceVulkan12Features f12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
    f12.storageBuffer8BitAccess = VK_TRUE; f12.uniformAndStorageBuffer8BitAccess = VK_TRUE;
    f12.shaderFloat16 = VK_TRUE; f12.shaderInt8 = VK_TRUE;
    VkPhysicalDeviceVulkan11Features f11{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES};
    f11.storageBuffer16BitAccess = VK_TRUE; f11.uniformAndStorageBuffer16BitAccess = VK_TRUE;
    f11.pNext = &f12;
    VkPhysicalDeviceFeatures2 f2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2}; f2.pNext = &f11;
    float qprio = 1.0f;
    VkDeviceQueueCreateInfo qci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    qci.queueFamilyIndex = qfam; qci.queueCount = 1; qci.pQueuePriorities = &qprio;
    VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    dci.queueCreateInfoCount = 1; dci.pQueueCreateInfos = &qci; dci.pNext = &f2;
    VkDevice dev = VK_NULL_HANDLE; VK_CHECK(vkCreateDevice(phys, &dci, nullptr, &dev));
    VkQueue queue; vkGetDeviceQueue(dev, qfam, 0, &queue);

    // Buffers
    Buf bufA = make_host_buffer(dev, phys, a_bytes);
    Buf bufB = make_host_buffer(dev, phys, b_bytes);
    Buf bufD = make_host_buffer(dev, phys, d_bytes);

    // Synthesize weights based on format
    if (is_q4_0) {
        auto* a_host = static_cast<block_q4_0*>(bufA.map);
        for (uint32_t r = 0; r < M; ++r)
            for (uint32_t b = 0; b < kblocks_q4_0; ++b)
                synth_block(a_host[r * kblocks_q4_0 + b], r * 7919u + b);
    } else if (is_q5_k) {
        auto* a_host = static_cast<block_q5_K*>(bufA.map);
        for (uint32_t r = 0; r < M; ++r)
            for (uint32_t b = 0; b < kblocks_q5_k; ++b)
                synth_block(a_host[r * kblocks_q5_k + b], r * 7919u + b);
    } else if (is_q6_k) {
        auto* a_host = static_cast<block_q6_K*>(bufA.map);
        for (uint32_t r = 0; r < M; ++r)
            for (uint32_t b = 0; b < kblocks_q6_k; ++b)
                synth_block(a_host[r * kblocks_q6_k + b], r * 7919u + b);
    } else {
        auto* a_host = static_cast<block_q4_K*>(bufA.map);
        for (uint32_t r = 0; r < M; ++r)
            for (uint32_t b = 0; b < kblocks; ++b)
                synth_block(a_host[r * kblocks + b], r * 7919u + b);
    }

    // Generate FP32 activation vector, then quantize to Q8_1.
    std::vector<float> b_fp32(K);
    {
        std::mt19937 rng(0xC0FFEEu);
        std::uniform_real_distribution<float> U(-1.0f, 1.0f);
        for (uint32_t i = 0; i < K; ++i) b_fp32[i] = U(rng);
    }
    auto* b_host = static_cast<block_q8_1*>(bufB.map);
    quantize_row_q8_1(b_fp32.data(), b_host, (int)K);
    std::memset(bufD.map, 0, (size_t)d_bytes);

    // Shader + pipeline
    auto code = read_spv(spv_path);
    VkShaderModuleCreateInfo smci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    smci.codeSize = code.size() * 4; smci.pCode = code.data();
    VkShaderModule shader; VK_CHECK(vkCreateShaderModule(dev, &smci, nullptr, &shader));

    VkDescriptorSetLayoutBinding binds[3]{};
    for (int i = 0; i < 3; ++i) {
        binds[i].binding = (uint32_t)i;
        binds[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        binds[i].descriptorCount = 1;
        binds[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    }
    VkDescriptorSetLayoutCreateInfo dslci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    dslci.bindingCount = 3; dslci.pBindings = binds;
    VkDescriptorSetLayout dsl; VK_CHECK(vkCreateDescriptorSetLayout(dev, &dslci, nullptr, &dsl));

    VkPushConstantRange pcr{VK_SHADER_STAGE_COMPUTE_BIT, 0, 5 * sizeof(uint32_t)};
    VkPipelineLayoutCreateInfo plci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    plci.setLayoutCount = 1; plci.pSetLayouts = &dsl;
    plci.pushConstantRangeCount = 1; plci.pPushConstantRanges = &pcr;
    VkPipelineLayout playout; VK_CHECK(vkCreatePipelineLayout(dev, &plci, nullptr, &playout));

    VkPipelineShaderStageCreateInfo ss{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    ss.stage = VK_SHADER_STAGE_COMPUTE_BIT; ss.module = shader; ss.pName = "main";
    VkComputePipelineCreateInfo cpci{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    cpci.stage = ss; cpci.layout = playout;
    VkPipeline pipe; VK_CHECK(vkCreateComputePipelines(dev, VK_NULL_HANDLE, 1, &cpci, nullptr, &pipe));

    VkDescriptorPoolSize psz{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3};
    VkDescriptorPoolCreateInfo dpci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    dpci.maxSets = 1; dpci.poolSizeCount = 1; dpci.pPoolSizes = &psz;
    VkDescriptorPool pool; VK_CHECK(vkCreateDescriptorPool(dev, &dpci, nullptr, &pool));
    VkDescriptorSetAllocateInfo dsai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    dsai.descriptorPool = pool; dsai.descriptorSetCount = 1; dsai.pSetLayouts = &dsl;
    VkDescriptorSet dset; VK_CHECK(vkAllocateDescriptorSets(dev, &dsai, &dset));

    VkDescriptorBufferInfo bi[3] = {
        {bufA.buf, 0, VK_WHOLE_SIZE}, {bufB.buf, 0, VK_WHOLE_SIZE}, {bufD.buf, 0, VK_WHOLE_SIZE}
    };
    VkWriteDescriptorSet ws[3]{};
    for (int i = 0; i < 3; ++i) {
        ws[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET; ws[i].dstSet = dset;
        ws[i].dstBinding = (uint32_t)i; ws[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        ws[i].descriptorCount = 1; ws[i].pBufferInfo = &bi[i];
    }
    vkUpdateDescriptorSets(dev, 3, ws, 0, nullptr);

    VkCommandPoolCreateInfo cpi{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO}; cpi.queueFamilyIndex = qfam;
    VkCommandPool cpool; VK_CHECK(vkCreateCommandPool(dev, &cpi, nullptr, &cpool));
    VkCommandBufferAllocateInfo cbai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cbai.commandPool = cpool; cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; cbai.commandBufferCount = 1;
    VkCommandBuffer cmd; VK_CHECK(vkAllocateCommandBuffers(dev, &cbai, &cmd));
    VkCommandBufferBeginInfo cbi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    cbi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(cmd, &cbi));
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipe);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, playout, 0, 1, &dset, 0, nullptr);
    // Push constants: M, N, K, stride_a, stride_b
    uint32_t stride_a = is_q4_0 ? kblocks_q4_0 : (is_q5_k ? kblocks_q5_k : (is_q6_k ? kblocks_q6_k : kblocks));
    uint32_t pcv[5] = { M, N, K, stride_a, kblocks_q8 };
    vkCmdPushConstants(cmd, playout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pcv), pcv);
    // Subgroup-shuffle variant computes 1 row per wave. Workgroup is 64
    // threads = (64 / subgroupSize) waves. All other variants are 1 row per
    // thread (64 rows per workgroup). Detect via shader filename.
    bool one_row_per_wg = false;
    if (std::strstr(spv_path, "_sg") || std::strstr(spv_path, "subgroup")) {
        one_row_per_wg = true;
    }
    if (kernel_name == "q4k_q8_1_u32" || kernel_name == "q4_0_u32" ||
        kernel_name == "q5_k_u32" || kernel_name == "q6_k_u32") {
        one_row_per_wg = true;
    }

    uint32_t rows_per_wg = one_row_per_wg ? (64u / sgp.subgroupSize) : 64u;
    if (rows_per_wg == 0u) rows_per_wg = 1u;
    uint32_t gx = (M + rows_per_wg - 1) / rows_per_wg;
    std::printf("[harness] rows_per_wg=%u gx=%u\n", rows_per_wg, gx);
    vkCmdDispatch(cmd, gx, 1, 1);
    VkBufferMemoryBarrier bmb{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
    bmb.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT; bmb.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    bmb.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; bmb.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bmb.buffer = bufD.buf; bmb.offset = 0; bmb.size = VK_WHOLE_SIZE;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0,
                         0, nullptr, 1, &bmb, 0, nullptr);
    VK_CHECK(vkEndCommandBuffer(cmd));

    VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    VkFence fence; VK_CHECK(vkCreateFence(dev, &fci, nullptr, &fence));
    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO}; si.commandBufferCount = 1; si.pCommandBuffers = &cmd;
    VK_CHECK(vkQueueSubmit(queue, 1, &si, fence));
    VK_CHECK(vkWaitForFences(dev, 1, &fence, VK_TRUE, UINT64_MAX));

    // CPU reference: dequant weights and Q8_1 activations, then dot.
    std::vector<float> a_dq(K), b_dq(K), cpu_out(M, 0.0f);
    for (uint32_t i = 0; i < kblocks_q8; ++i) {
        float d_a = half_to_float(b_host[i].d);
        for (int l = 0; l < QK8_1; ++l) b_dq[i * QK8_1 + l] = d_a * (float)b_host[i].qs[l];
    }
    for (uint32_t r = 0; r < M; ++r) {
        if (is_q4_0) {
            auto* a_host = static_cast<block_q4_0*>(bufA.map);
            for (uint32_t b = 0; b < kblocks_q4_0; ++b)
                dequantize_block_q4_0(a_host[r * kblocks_q4_0 + b], a_dq.data() + b * QK4_0);
        } else if (is_q5_k) {
            auto* a_host = static_cast<block_q5_K*>(bufA.map);
            for (uint32_t b = 0; b < kblocks_q5_k; ++b)
                dequantize_block_q5_K(a_host[r * kblocks_q5_k + b], a_dq.data() + b * QK_K);
        } else if (is_q6_k) {
            auto* a_host = static_cast<block_q6_K*>(bufA.map);
            for (uint32_t b = 0; b < kblocks_q6_k; ++b)
                dequantize_block_q6_K(a_host[r * kblocks_q6_k + b], a_dq.data() + b * QK_K);
        } else {
            auto* a_host = static_cast<block_q4_K*>(bufA.map);
            for (uint32_t b = 0; b < kblocks; ++b)
                dequantize_block_q4_K(a_host[r * kblocks + b], a_dq.data() + b * QK_K);
        }
        float acc = 0.0f;
        for (uint32_t k = 0; k < K; ++k) acc += a_dq[k] * b_dq[k];
        cpu_out[r] = acc;
    }

    // Compare
    auto* gpu_out = static_cast<float*>(bufD.map);
    int    fails = 0; int    nonzero = 0;
    double worst_abs = 0.0, worst_rel = 0.0;
    const float TOL_ABS = 1e-3f, TOL_REL = 1e-3f;
    for (uint32_t r = 0; r < M; ++r) {
        float g = gpu_out[r], c = cpu_out[r];
        if (g != 0.0f) ++nonzero;
        double d = std::fabs((double)g - (double)c);
        double mag = std::max(std::fabs((double)g), std::fabs((double)c));
        double rel = mag > 0 ? d / mag : 0.0;
        if (d > worst_abs) worst_abs = d;
        if (rel > worst_rel) worst_rel = rel;
        if (d > TOL_ABS && rel > TOL_REL) ++fails;
    }
    std::printf("[harness] any_nonzero=%d  worst_abs=%.4g  worst_rel=%.4g  fails=%d/%u\n",
                nonzero ? 1 : 0, worst_abs, worst_rel, fails, M);
    for (int i = 0; i < 4; ++i)
        std::printf("  row %d: gpu=%.6f cpu=%.6f diff=%.3e\n",
                    i, gpu_out[i], cpu_out[i], gpu_out[i] - cpu_out[i]);
    std::puts(fails == 0 ? "PASS" : "FAIL");

    // ---- Optional benchmark mode: record N back-to-back dispatches in one cmdbuf ----
    if (bench_iters > 0 && fails == 0) {
        VkCommandBuffer bcmd; VK_CHECK(vkAllocateCommandBuffers(dev, &cbai, &bcmd));
        VkCommandBufferBeginInfo bcbi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        VK_CHECK(vkBeginCommandBuffer(bcmd, &bcbi));
        vkCmdBindPipeline(bcmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipe);
        vkCmdBindDescriptorSets(bcmd, VK_PIPELINE_BIND_POINT_COMPUTE, playout, 0, 1, &dset, 0, nullptr);
        vkCmdPushConstants(bcmd, playout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pcv), pcv);
        for (int it = 0; it < bench_iters; ++it) {
            vkCmdDispatch(bcmd, gx, 1, 1);
            VkMemoryBarrier mb{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
            mb.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            mb.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            vkCmdPipelineBarrier(bcmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0,
                                 1, &mb, 0, nullptr, 0, nullptr);
        }
        VK_CHECK(vkEndCommandBuffer(bcmd));
        // Warmup
        VkFence bfence; VK_CHECK(vkCreateFence(dev, &fci, nullptr, &bfence));
        VkSubmitInfo bsi{VK_STRUCTURE_TYPE_SUBMIT_INFO}; bsi.commandBufferCount = 1; bsi.pCommandBuffers = &bcmd;
        VK_CHECK(vkQueueSubmit(queue, 1, &bsi, bfence));
        VK_CHECK(vkWaitForFences(dev, 1, &bfence, VK_TRUE, UINT64_MAX));
        VK_CHECK(vkResetFences(dev, 1, &bfence));
        // Timed run
        auto t0 = std::chrono::steady_clock::now();
        VK_CHECK(vkQueueSubmit(queue, 1, &bsi, bfence));
        VK_CHECK(vkWaitForFences(dev, 1, &bfence, VK_TRUE, UINT64_MAX));
        auto t1 = std::chrono::steady_clock::now();
        double sec = std::chrono::duration<double>(t1 - t0).count();
        double per_us = sec * 1e6 / bench_iters;
        // Each dispatch: M rows * K weights * 2 (mul+add) ops ~= 2*M*K FLOPs
        double gflops = (2.0 * (double)M * (double)K) / (per_us * 1e3);
        std::printf("[bench] iters=%d  total=%.3f ms  per_dispatch=%.2f us  ~%.2f GFLOPs\n",
                    bench_iters, sec * 1e3, per_us, gflops);
        vkDestroyFence(dev, bfence, nullptr);
        vkFreeCommandBuffers(dev, cpool, 1, &bcmd);
    }

    // Cleanup
    vkDestroyFence(dev, fence, nullptr);
    vkDestroyCommandPool(dev, cpool, nullptr);
    vkDestroyDescriptorPool(dev, pool, nullptr);
    vkDestroyPipeline(dev, pipe, nullptr);
    vkDestroyPipelineLayout(dev, playout, nullptr);
    vkDestroyDescriptorSetLayout(dev, dsl, nullptr);
    vkDestroyShaderModule(dev, shader, nullptr);
    for (auto* b : { &bufA, &bufB, &bufD }) {
        vkUnmapMemory(dev, b->mem);
        vkDestroyBuffer(dev, b->buf, nullptr);
        vkFreeMemory(dev, b->mem, nullptr);
    }
    vkDestroyDevice(dev, nullptr);
    vkDestroyInstance(inst, nullptr);
    return fails == 0 ? 0 : 1;
}
