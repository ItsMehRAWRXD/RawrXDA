// =============================================================================
// bench_q4k_q8_1.cpp - Phase 2A microbenchmark.
//   Measures GPU-side execution time of fused_q4k_q8_1_gemv via VkQueryPool
//   timestamps (no CPU parity here; that is what test_q4k_q8_1.cpp is for).
//
// Reports min / median / mean kernel time in microseconds, plus derived
// effective bandwidth (GB/s on Q4_K weights) and arithmetic throughput
// (GFLOPS counted as 2 * M * K).  This is the baseline number we will
// compare LDS-tiled and vectorized variants against.
// =============================================================================

#include <vulkan/vulkan.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <vector>

#define VK_CHECK(expr) do { VkResult _r = (expr); if (_r != VK_SUCCESS) { \
    std::fprintf(stderr, "VK_CHECK failed: %s = %d (line %d)\n", #expr, (int)_r, __LINE__); \
    std::exit(2); } } while (0)
static void die(const char* msg) { std::fprintf(stderr, "FATAL: %s\n", msg); std::exit(2); }

// Mirror ggml POD layouts (only sizes matter here -- contents are random).
#pragma pack(push, 1)
struct block_q4_K { uint16_t d, dmin; uint8_t scales[12]; uint8_t qs[128]; };
struct block_q8_1 { uint16_t d, s;     int8_t  qs[32]; };
#pragma pack(pop)
static_assert(sizeof(block_q4_K) == 144, "q4k size");
static_assert(sizeof(block_q8_1) == 36,  "q81 size");

static std::vector<uint32_t> read_spv(const char* path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) { std::fprintf(stderr, "cannot open: %s\n", path); std::exit(2); }
    size_t sz = (size_t)f.tellg();
    if (sz == 0 || (sz % 4)) die("bad SPIR-V");
    std::vector<uint32_t> v(sz / 4);
    f.seekg(0); f.read((char*)v.data(), (std::streamsize)sz);
    return v;
}

struct Buf { VkBuffer buf=VK_NULL_HANDLE; VkDeviceMemory mem=VK_NULL_HANDLE; VkDeviceSize size=0; };
static uint32_t mtype(VkPhysicalDevice p, uint32_t bits, VkMemoryPropertyFlags want) {
    VkPhysicalDeviceMemoryProperties mp{}; vkGetPhysicalDeviceMemoryProperties(p, &mp);
    for (uint32_t i = 0; i < mp.memoryTypeCount; ++i)
        if ((bits & (1u << i)) && (mp.memoryTypes[i].propertyFlags & want) == want) return i;
    die("no memtype"); return 0;
}
static Buf mkbuf(VkDevice d, VkPhysicalDevice p, VkDeviceSize sz, VkMemoryPropertyFlags want, VkBufferUsageFlags usage) {
    Buf b{}; b.size = sz;
    VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bi.size = sz; bi.usage = usage; bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VK_CHECK(vkCreateBuffer(d, &bi, nullptr, &b.buf));
    VkMemoryRequirements mr{}; vkGetBufferMemoryRequirements(d, b.buf, &mr);
    VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ai.allocationSize = mr.size; ai.memoryTypeIndex = mtype(p, mr.memoryTypeBits, want);
    VK_CHECK(vkAllocateMemory(d, &ai, nullptr, &b.mem));
    VK_CHECK(vkBindBufferMemory(d, b.buf, b.mem, 0));
    return b;
}

int main(int argc, char** argv) {
    // Bench dims: M output rows, K inner. Configurable via argv for sweeps.
    uint32_t M = 4096, K = 4096;
    int iters = 200, warmup = 20;
    const char* spv_path = "d:\\rawrxd\\src\\gpu\\shaders\\_spv\\fused_q4k_q8_1_gemv.spv";
    for (int i = 1; i < argc; ++i) {
        if      (!std::strcmp(argv[i], "-M") && i+1 < argc) M = (uint32_t)std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "-K") && i+1 < argc) K = (uint32_t)std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "-n") && i+1 < argc) iters = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "-w") && i+1 < argc) warmup = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--spv") && i+1 < argc) spv_path = argv[++i];
    }
    if (K % 256 != 0) die("K must be multiple of 256");

    const uint32_t kblocks    = K / 256;
    const uint32_t kblocks_q8 = K / 32;
    std::printf("[bench] Q4_K x Q8_1 GEMV  M=%u K=%u  iters=%d warmup=%d\n", M, K, iters, warmup);
    std::printf("[bench] SPIR-V: %s\n", spv_path);

    // --- Instance + device ---------------------------------------------------
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.pApplicationName = "bench_q4k_q8_1"; app.apiVersion = VK_API_VERSION_1_2;
    VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO}; ici.pApplicationInfo = &app;
    VkInstance inst; VK_CHECK(vkCreateInstance(&ici, nullptr, &inst));

    uint32_t devc = 0; vkEnumeratePhysicalDevices(inst, &devc, nullptr);
    if (!devc) die("no devices");
    std::vector<VkPhysicalDevice> pds(devc);
    vkEnumeratePhysicalDevices(inst, &devc, pds.data());
    VkPhysicalDevice phys = VK_NULL_HANDLE; uint32_t qfam = UINT32_MAX;
    for (auto d : pds) {
        uint32_t qc = 0; vkGetPhysicalDeviceQueueFamilyProperties(d, &qc, nullptr);
        std::vector<VkQueueFamilyProperties> qp(qc);
        vkGetPhysicalDeviceQueueFamilyProperties(d, &qc, qp.data());
        for (uint32_t i = 0; i < qc; ++i) if (qp[i].queueFlags & VK_QUEUE_COMPUTE_BIT) { phys = d; qfam = i; break; }
        if (phys) break;
    }
    if (!phys) die("no compute queue");
    VkPhysicalDeviceProperties props{}; vkGetPhysicalDeviceProperties(phys, &props);
    const float ts_period_ns = props.limits.timestampPeriod;
    if (ts_period_ns <= 0.0f) die("device does not support timestamps");
    std::printf("[bench] device: %s  timestampPeriod=%.3f ns\n", props.deviceName, ts_period_ns);

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
    VkDevice dev; VK_CHECK(vkCreateDevice(phys, &dci, nullptr, &dev));
    VkQueue queue; vkGetDeviceQueue(dev, qfam, 0, &queue);

    // --- Buffers (DEVICE_LOCAL; we don't read them back) ---------------------
    const VkDeviceSize a_bytes = (VkDeviceSize)M * kblocks    * sizeof(block_q4_K);
    const VkDeviceSize b_bytes = (VkDeviceSize)kblocks_q8     * sizeof(block_q8_1);
    const VkDeviceSize d_bytes = (VkDeviceSize)M               * sizeof(float);
    const auto USAGE = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    Buf bufA = mkbuf(dev, phys, a_bytes, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, USAGE);
    Buf bufB = mkbuf(dev, phys, b_bytes, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, USAGE);
    Buf bufD = mkbuf(dev, phys, d_bytes, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, USAGE);

    // --- Pipeline ------------------------------------------------------------
    auto code = read_spv(spv_path);
    VkShaderModuleCreateInfo smci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    smci.codeSize = code.size() * 4; smci.pCode = code.data();
    VkShaderModule sm; VK_CHECK(vkCreateShaderModule(dev, &smci, nullptr, &sm));

    VkDescriptorSetLayoutBinding binds[3]{};
    for (int i = 0; i < 3; ++i) {
        binds[i] = { (uint32_t)i, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr };
    }
    VkDescriptorSetLayoutCreateInfo dslci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    dslci.bindingCount = 3; dslci.pBindings = binds;
    VkDescriptorSetLayout dsl; VK_CHECK(vkCreateDescriptorSetLayout(dev, &dslci, nullptr, &dsl));

    VkPushConstantRange pcr{VK_SHADER_STAGE_COMPUTE_BIT, 0, 6 * sizeof(uint32_t)};
    VkPipelineLayoutCreateInfo plci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    plci.setLayoutCount = 1; plci.pSetLayouts = &dsl;
    plci.pushConstantRangeCount = 1; plci.pPushConstantRanges = &pcr;
    VkPipelineLayout pl; VK_CHECK(vkCreatePipelineLayout(dev, &plci, nullptr, &pl));

    VkPipelineShaderStageCreateInfo ss{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    ss.stage = VK_SHADER_STAGE_COMPUTE_BIT; ss.module = sm; ss.pName = "main";
    VkComputePipelineCreateInfo cpci{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    cpci.stage = ss; cpci.layout = pl;
    VkPipeline pipe; VK_CHECK(vkCreateComputePipelines(dev, VK_NULL_HANDLE, 1, &cpci, nullptr, &pipe));

    VkDescriptorPoolSize psz{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3};
    VkDescriptorPoolCreateInfo dpci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    dpci.maxSets = 1; dpci.poolSizeCount = 1; dpci.pPoolSizes = &psz;
    VkDescriptorPool dpool; VK_CHECK(vkCreateDescriptorPool(dev, &dpci, nullptr, &dpool));
    VkDescriptorSetAllocateInfo dsai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    dsai.descriptorPool = dpool; dsai.descriptorSetCount = 1; dsai.pSetLayouts = &dsl;
    VkDescriptorSet dset; VK_CHECK(vkAllocateDescriptorSets(dev, &dsai, &dset));

    VkDescriptorBufferInfo bi[3] = {
        {bufA.buf, 0, VK_WHOLE_SIZE}, {bufB.buf, 0, VK_WHOLE_SIZE}, {bufD.buf, 0, VK_WHOLE_SIZE} };
    VkWriteDescriptorSet ws[3]{};
    for (int i = 0; i < 3; ++i) {
        ws[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET; ws[i].dstSet = dset;
        ws[i].dstBinding = (uint32_t)i; ws[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        ws[i].descriptorCount = 1; ws[i].pBufferInfo = &bi[i];
    }
    vkUpdateDescriptorSets(dev, 3, ws, 0, nullptr);

    // --- Query pool: 2 timestamps per iteration -----------------------------
    const uint32_t QCOUNT = (uint32_t)((iters + warmup) * 2);
    VkQueryPoolCreateInfo qpci{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
    qpci.queryType = VK_QUERY_TYPE_TIMESTAMP; qpci.queryCount = QCOUNT;
    VkQueryPool qpool; VK_CHECK(vkCreateQueryPool(dev, &qpci, nullptr, &qpool));

    VkCommandPoolCreateInfo cpi{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO}; cpi.queueFamilyIndex = qfam;
    VkCommandPool cpool; VK_CHECK(vkCreateCommandPool(dev, &cpi, nullptr, &cpool));
    VkCommandBufferAllocateInfo cbai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cbai.commandPool = cpool; cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; cbai.commandBufferCount = 1;
    VkCommandBuffer cmd; VK_CHECK(vkAllocateCommandBuffers(dev, &cbai, &cmd));
    VkCommandBufferBeginInfo cbi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    cbi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(cmd, &cbi));
    vkCmdResetQueryPool(cmd, qpool, 0, QCOUNT);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipe);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pl, 0, 1, &dset, 0, nullptr);
    uint32_t pcv[6] = { M, 1u, K, kblocks, kblocks_q8, M };
    vkCmdPushConstants(cmd, pl, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pcv), pcv);
    const uint32_t gx = (M + 63) / 64;
    for (int it = 0; it < iters + warmup; ++it) {
        vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,    qpool, (uint32_t)it * 2u);
        vkCmdDispatch(cmd, gx, 1, 1);
        vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, qpool, (uint32_t)it * 2u + 1u);
        // serialize next dispatch's read of the same buffers (compute->compute)
        VkMemoryBarrier mb{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
        mb.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        mb.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0,
            1, &mb, 0, nullptr, 0, nullptr);
    }
    VK_CHECK(vkEndCommandBuffer(cmd));

    VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    VkFence fence; VK_CHECK(vkCreateFence(dev, &fci, nullptr, &fence));
    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO}; si.commandBufferCount = 1; si.pCommandBuffers = &cmd;
    auto t0 = std::chrono::steady_clock::now();
    VK_CHECK(vkQueueSubmit(queue, 1, &si, fence));
    VK_CHECK(vkWaitForFences(dev, 1, &fence, VK_TRUE, UINT64_MAX));
    auto t1 = std::chrono::steady_clock::now();
    double wall_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    std::vector<uint64_t> ts(QCOUNT);
    VK_CHECK(vkGetQueryPoolResults(dev, qpool, 0, QCOUNT, QCOUNT * sizeof(uint64_t), ts.data(),
                                   sizeof(uint64_t), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT));

    // Skip warmup; collect per-iteration deltas in microseconds.
    std::vector<double> us; us.reserve((size_t)iters);
    for (int it = warmup; it < iters + warmup; ++it) {
        uint64_t a = ts[(size_t)it * 2 + 0], b = ts[(size_t)it * 2 + 1];
        double ns = (double)(b - a) * (double)ts_period_ns;
        us.push_back(ns / 1000.0);
    }
    std::sort(us.begin(), us.end());
    double mn = us.front(), mx = us.back();
    double med = us[us.size() / 2];
    double mean = 0; for (double v : us) mean += v; mean /= (double)us.size();

    // Effective metrics (per iteration).
    // Bytes touched: A (Q4_K weights, M*K/2 nibbles ≈ M*K/2 bytes) + B (Q8_1, K bytes)
    //                + D (M*4 bytes).  Quote weight bandwidth which is dominant.
    double weight_bytes = (double)M * (double)K * 0.5;            // Q4_K ~ 4.5 bits, ~0.5 B/elem
    double weight_GBps  = (weight_bytes / (med * 1e-6)) / 1e9;
    double flops_per    = 2.0 * (double)M * (double)K;
    double GFLOPS       = (flops_per / (med * 1e-6)) / 1e9;

    std::printf("[bench] iters=%d  min=%.2f us  median=%.2f us  mean=%.2f us  max=%.2f us\n",
                iters, mn, med, mean, mx);
    std::printf("[bench] weight-BW (median) = %.2f GB/s    GEMV (2*M*K) = %.2f GFLOPS\n",
                weight_GBps, GFLOPS);
    std::printf("[bench] wall (submit->fence) = %.2f ms over %d kernels\n", wall_ms, iters + warmup);

    // Cleanup
    vkDestroyFence(dev, fence, nullptr);
    vkDestroyCommandPool(dev, cpool, nullptr);
    vkDestroyQueryPool(dev, qpool, nullptr);
    vkDestroyDescriptorPool(dev, dpool, nullptr);
    vkDestroyPipeline(dev, pipe, nullptr);
    vkDestroyPipelineLayout(dev, pl, nullptr);
    vkDestroyDescriptorSetLayout(dev, dsl, nullptr);
    vkDestroyShaderModule(dev, sm, nullptr);
    for (auto* b : { &bufA, &bufB, &bufD }) {
        vkDestroyBuffer(dev, b->buf, nullptr); vkFreeMemory(dev, b->mem, nullptr);
    }
    vkDestroyDevice(dev, nullptr);
    vkDestroyInstance(inst, nullptr);
    return 0;
}
