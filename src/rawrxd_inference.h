#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <limits>
#include <iostream>
#include <memory>
#include <string>
#include <vector>


#ifdef RAWR_ENABLE_VULKAN
#include <vulkan/vulkan.h>
#else
// Vulkan stubs for CPU mode
#ifndef VK_NULL_HANDLE
#define VK_NULL_HANDLE 0
#endif
#ifndef VK_STRUCTURE_TYPE_APPLICATION_INFO
#define VK_STRUCTURE_TYPE_APPLICATION_INFO 0
#define VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO 0
#define VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO 0
#define VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO 0
#define VK_MAKE_VERSION(a, b, c) 0
#define VK_API_VERSION_1_2 0
#define VK_SUCCESS 0
#define VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU 0
#define VK_QUEUE_COMPUTE_BIT 0
#endif
typedef void* VkInstance;
typedef void* VkPhysicalDevice;
typedef void* VkDevice;
typedef void* VkQueue;
typedef struct
{
    int dummy;
} VkApplicationInfo;
typedef struct
{
    int dummy;
} VkInstanceCreateInfo;
typedef struct
{
    int dummy;
} VkDeviceQueueCreateInfo;
typedef struct
{
    int dummy;
} VkDeviceCreateInfo;
typedef struct
{
    int dummy;
} VkPhysicalDeviceProperties;
typedef struct
{
    uint32_t queueFlags;
} VkQueueFamilyProperties;
#endif
#include "core/gguf_swarm_plan_builder.hpp"
#include "rawrxd_model_loader.h"
#include "rawrxd_sampler.h"
#include "rawrxd_tokenizer.h"
#include "rawrxd_transformer.h"
#include "swarm_scheduler.hpp"
#include "titan_token_trace.h"
#include "utf8_validator.h"

/// Snapshot of MoE grouped pack cache + async prepack counters (Win32IDE HUD / staging telemetry).
struct MoEPackHudMetrics
{
    std::uint64_t packHits = 0;
    std::uint64_t packMisses = 0;
    std::uint64_t groupedFallbacks = 0;
    std::uint64_t syncPackInserts = 0;
    std::uint64_t groupedWeightedApplies = 0;
    std::uint64_t groupedSingleExpertApplies = 0;
    std::uint64_t groupedWeightedFallbacks = 0;
    std::uint64_t groupedSingleExpertFallbacks = 0;
    std::uint64_t prepackInserts = 0;
    std::uint64_t prepackQueueDropped = 0;
    std::uint64_t prepackSkippedNotResident = 0;
    std::uint64_t packEvictedByPlanRow = 0;
    std::size_t prepackQueueDepthApprox = 0;
    std::size_t packCachePackedBytes = 0;
    std::uint64_t packCacheEvictions = 0;
    std::uint64_t packCacheSelectiveRowInvalidations = 0;
};

inline void NormalizeMoEPackHudMetrics(MoEPackHudMetrics& m) noexcept
{
    const std::uint64_t splitFallbacks = m.groupedWeightedFallbacks + m.groupedSingleExpertFallbacks;
    if (splitFallbacks > m.groupedFallbacks)
        m.groupedFallbacks = splitFallbacks;
}

// RawrXD Real Inference Orchestrator
// One-shot: Load model -> Tokenize -> Forward -> Sample -> Detokenize

class RawrXDInference
{
    RawrXDModelLoader loader;
    RawrXDTransformer transformer;
    RawrXDTokenizer tokenizer;
    RawrXDSampler sampler;
    std::unique_ptr<RawrXD::Swarm::SwarmScheduler> m_swarmScheduler;
    bool m_initialized = false;
    uint32_t m_contextLimit = 0;
    std::vector<float> m_lastLogits;
    std::string m_lastLoadErrorMessage;
    TitanTokenTraceBuffer<256> m_tokenTraceBuffer;  // Token pipeline tracing (256-entry ring buffer)
    VkInstance m_vkInstance = VK_NULL_HANDLE;
    VkPhysicalDevice m_vkPhysicalDevice = VK_NULL_HANDLE;
    VkDevice m_vkDevice = VK_NULL_HANDLE;

    void ResetOwnedVulkanContext() noexcept
    {
#ifdef RAWR_ENABLE_VULKAN
        if (m_vkDevice != VK_NULL_HANDLE)
        {
            vkDestroyDevice(m_vkDevice, nullptr);
            m_vkDevice = VK_NULL_HANDLE;
        }
        if (m_vkInstance != VK_NULL_HANDLE)
        {
            vkDestroyInstance(m_vkInstance, nullptr);
            m_vkInstance = VK_NULL_HANDLE;
        }
#endif
        m_vkPhysicalDevice = VK_NULL_HANDLE;
    }

    // Helpers
    VkInstance CreateVulkanInstance()
    {
#ifndef RAWR_ENABLE_VULKAN
        return VK_NULL_HANDLE;
#else
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "RawrXD Inference";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "RawrXD Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_2;

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledLayerCount = 0;
        createInfo.enabledExtensionCount = 0;

        VkInstance instance = VK_NULL_HANDLE;
        if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS)
        {
            printf("Failed to create Vulkan instance\n");
            return VK_NULL_HANDLE;
        }
        return instance;
#endif
    }

    VkPhysicalDevice SelectPhysicalDevice(VkInstance instance)
    {
#ifndef RAWR_ENABLE_VULKAN
        (void)instance;
        return VK_NULL_HANDLE;
#else
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
        if (deviceCount == 0)
            return VK_NULL_HANDLE;

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

        for (const auto& device : devices)
        {
            VkPhysicalDeviceProperties deviceProperties;
            vkGetPhysicalDeviceProperties(device, &deviceProperties);
            if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
            {
                printf("Selected GPU: %s\n", deviceProperties.deviceName);
                return device;
            }
        }
        return devices[0];
#endif
    }

    VkDevice CreateLogicalDevice(VkPhysicalDevice physDevice)
    {
#ifndef RAWR_ENABLE_VULKAN
        (void)physDevice;
        return VK_NULL_HANDLE;
#else
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physDevice, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physDevice, &queueFamilyCount, queueFamilies.data());

        int computeFamily = -1;
        for (uint32_t i = 0; i < queueFamilyCount; i++)
        {
            if (queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT)
            {
                computeFamily = static_cast<int>(i);
                break;
            }
        }

        if (computeFamily < 0)
        {
            printf("failed to find compute queue family!\n");
            return VK_NULL_HANDLE;
        }

        float queuePriority = 1.0f;
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = computeFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;

        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.pQueueCreateInfos = &queueCreateInfo;
        createInfo.queueCreateInfoCount = 1;
        VkPhysicalDeviceFeatures deviceFeatures{};
        createInfo.pEnabledFeatures = &deviceFeatures;

        VkDevice device = VK_NULL_HANDLE;
        if (vkCreateDevice(physDevice, &createInfo, nullptr, &device) != VK_SUCCESS)
        {
            printf("failed to create logical device!\n");
            return VK_NULL_HANDLE;
        }
        return device;
#endif
    }

  public:
        ~RawrXDInference() { ResetOwnedVulkanContext(); }

        RawrXDInference() = default;
        RawrXDInference(const RawrXDInference&) = delete;
        RawrXDInference& operator=(const RawrXDInference&) = delete;
        RawrXDInference(RawrXDInference&&) = delete;
        RawrXDInference& operator=(RawrXDInference&&) = delete;

    const std::string& GetLastLoadErrorMessage() const { return m_lastLoadErrorMessage; }

    bool Initialize(const std::string& modelPath)
    {
        m_initialized = false;
        m_contextLimit = 0;
        m_lastLogits.clear();
        m_swarmScheduler.reset();
        ResetOwnedVulkanContext();

        if (modelPath.empty())
        {
            m_lastLoadErrorMessage = "init_args: missing modelPath";
            return false;
        }

        m_lastLoadErrorMessage.clear();
        loader.SetLoadErrorCallback([this](const std::string& stage, const std::string& message)
                                    { m_lastLoadErrorMessage = stage + ": " + message; });
#ifdef RAWR_ENABLE_VULKAN
        VkInstance instance = CreateVulkanInstance();
        VkPhysicalDevice physDevice = VK_NULL_HANDLE;
        VkDevice device = VK_NULL_HANDLE;
        auto cleanupVulkanInit = [&]()
        {
            if (device != VK_NULL_HANDLE)
            {
                vkDestroyDevice(device, nullptr);
                device = VK_NULL_HANDLE;
            }
            if (instance != VK_NULL_HANDLE)
            {
                vkDestroyInstance(instance, nullptr);
                instance = VK_NULL_HANDLE;
            }
        };

        if (!instance)
            return false;

        physDevice = SelectPhysicalDevice(instance);
        if (!physDevice)
        {
            cleanupVulkanInit();
            return false;
        }

        device = CreateLogicalDevice(physDevice);
        if (!device)
        {
            cleanupVulkanInit();
            return false;
        }
#else
        // CPU-only mode — no GPU required
        VkDevice device = VK_NULL_HANDLE;
        VkPhysicalDevice physDevice = VK_NULL_HANDLE;
        printf("[RawrXD] CPU-only mode (Vulkan disabled)\n");
#endif

        printf("[RawrXD] Stage: loader.Load\n");
        int wideLength = MultiByteToWideChar(CP_UTF8, 0, modelPath.c_str(), -1, nullptr, 0);
        if (wideLength <= 0)
            wideLength = MultiByteToWideChar(CP_ACP, 0, modelPath.c_str(), -1, nullptr, 0);
        if (wideLength <= 0)
        {
            m_lastLoadErrorMessage = "loader_load: model path conversion failed";
#ifdef RAWR_ENABLE_VULKAN
            cleanupVulkanInit();
#endif
            return false;
        }

        const size_t wideCapacity = static_cast<size_t>(wideLength);
        std::wstring wideModelPath(wideCapacity, L'\0');
        int converted = MultiByteToWideChar(CP_UTF8, 0, modelPath.c_str(), -1, wideModelPath.data(), wideLength);
        if (converted <= 0)
            converted = MultiByteToWideChar(CP_ACP, 0, modelPath.c_str(), -1, wideModelPath.data(), wideLength);
        if (converted <= 0)
        {
            m_lastLoadErrorMessage = "loader_load: model path conversion failed";
#ifdef RAWR_ENABLE_VULKAN
            cleanupVulkanInit();
#endif
            return false;
        }

        if (wideModelPath.empty() || wideModelPath[0] == L'\0')
        {
            m_lastLoadErrorMessage = "loader_load: model path conversion produced empty path";
#ifdef RAWR_ENABLE_VULKAN
            cleanupVulkanInit();
#endif
            return false;
        }

        if (converted > 0)
            wideModelPath.resize(static_cast<size_t>(converted - 1));

        printf("[RawrXD] Loading model from: %ls\n", wideModelPath.c_str());
        if (!loader.Load(wideModelPath.c_str(), device, physDevice))
        {
            if (m_lastLoadErrorMessage.empty())
            {
                m_lastLoadErrorMessage = loader.GetLastLoadErrorMessage();
            }
            printf("[RawrXD] Failed to load model\n");
#ifdef RAWR_ENABLE_VULKAN
            cleanupVulkanInit();
#endif
            return false;
        }

        RawrXDTransformer::Config cfg{};  // Zero-init all fields
        cfg.dim = loader.getDim();
        cfg.n_layers = loader.getLayers();
        cfg.n_heads = loader.getHeads();
        cfg.n_kv_heads = loader.getKVHeads();
        cfg.vocab_size = loader.getVocabSize();

        if (cfg.vocab_size == 0)
            cfg.vocab_size = 32000;
        if (cfg.dim == 0)
            cfg.dim = 4096;
        if (cfg.n_layers == 0)
            cfg.n_layers = 32;
        if (cfg.n_heads == 0)
            cfg.n_heads = 32;
        if (cfg.n_kv_heads == 0)
            cfg.n_kv_heads = cfg.n_heads;

        cfg.hidden_dim = (loader.getFFNDim() > 0) ? loader.getFFNDim() : cfg.dim * 4;
        cfg.n_ctx = 2048;  // Conservative context for CPU-only mode
        cfg.seq_len = 2048;
        cfg.rope_theta = 10000.0f;
        cfg.rms_norm_eps = 1e-5f;

        if (const char* ctxEnv = std::getenv("RAWRXD_INFERENCE_CTX"))
        {
            char* endPtr = nullptr;
            const unsigned long requested = std::strtoul(ctxEnv, &endPtr, 10);
            if (endPtr != ctxEnv && *endPtr == '\0')
            {
                constexpr uint32_t kMinCtx = 32;
                constexpr uint32_t kMaxCtx = 131072;
                const uint32_t clamped = static_cast<uint32_t>(std::min<unsigned long>(
                    kMaxCtx,
                    std::max<unsigned long>(kMinCtx, requested)));
                cfg.n_ctx = clamped;
                cfg.seq_len = clamped;
                printf("[RawrXD] Context override active via RAWRXD_INFERENCE_CTX=%u\n", clamped);
            }
        }

        // Staging: sovereign MoE grouped pack + async prepack (see RawrXDTransformer::Config). Off unless env set.
        if (const char* moeEnv = std::getenv("RAWRXD_MOE_GROUPED_INTEGRATION"))
        {
            if (moeEnv && moeEnv[0] != '\0' && (moeEnv[0] == '1' || moeEnv[0] == 't' || moeEnv[0] == 'T' || moeEnv[0] == 'y' || moeEnv[0] == 'Y'))
            {
                cfg.moe_down_enable_grouped_integration = true;
                cfg.moe_down_grouped_async_prepack = true;
                cfg.moe_down_grouped_sync_pack_on_miss = false;
            }
        }

        // Validate configuration bounds before passing to transformer
        // Reject pathologically large or zero values for critical dimensions
        const uint32_t MAX_REASONABLE_DIM = 32768;
        const uint32_t MAX_REASONABLE_LAYERS = 256;
        const uint32_t MAX_REASONABLE_HEADS = 512;
        const uint32_t MAX_REASONABLE_VOCAB = 1000000;
        
        if (cfg.dim == 0 || cfg.dim > MAX_REASONABLE_DIM ||
            cfg.n_layers == 0 || cfg.n_layers > MAX_REASONABLE_LAYERS ||
            cfg.n_heads == 0 || cfg.n_heads > MAX_REASONABLE_HEADS ||
            cfg.n_kv_heads == 0 || cfg.n_kv_heads > cfg.n_heads ||
            cfg.vocab_size == 0 || cfg.vocab_size > MAX_REASONABLE_VOCAB)
        {
            m_lastLoadErrorMessage = "config_validation: transformer config values out of bounds (dim/layers/heads/vocab invalid)";
            printf("[RawrXD] Config validation failed: dim=%d layers=%d heads=%d kv_heads=%d vocab=%d\n", 
                   cfg.dim, cfg.n_layers, cfg.n_heads, cfg.n_kv_heads, cfg.vocab_size);
#ifdef RAWR_ENABLE_VULKAN
            cleanupVulkanInit();
#endif
            return false;
        }

        printf("[RawrXD] Config: dim=%d layers=%d heads=%d kv_heads=%d vocab=%d hidden=%d ctx=%d\n", cfg.dim,
               cfg.n_layers, cfg.n_heads, cfg.n_kv_heads, cfg.vocab_size, cfg.hidden_dim, cfg.n_ctx);

        printf("[RawrXD] Stage: tokenizer.Load\n");
        if (!tokenizer.LoadFromGGUF(modelPath))
        {
            m_lastLoadErrorMessage = "tokenizer_load: failed to load vocab from GGUF";
#ifdef RAWR_ENABLE_VULKAN
            cleanupVulkanInit();
#endif
            return false;
        }

        printf("[RawrXD] Stage: transformer.Initialize\n");
        try {
            transformer.Initialize(device, physDevice, cfg, &loader);
        }
        catch (const std::exception& ex) {
            m_lastLoadErrorMessage = std::string("transformer_init: exception during Initialize: ") + ex.what();
            printf("[RawrXD] Transformer initialization threw exception: %s\n", ex.what());
#ifdef RAWR_ENABLE_VULKAN
            cleanupVulkanInit();
#endif
            return false;
        }
        catch (...) {
            m_lastLoadErrorMessage = "transformer_init: unknown exception during Initialize";
            printf("[RawrXD] Transformer initialization threw unknown exception\n");
#ifdef RAWR_ENABLE_VULKAN
            cleanupVulkanInit();
#endif
            return false;
        }

        m_swarmScheduler = RawrXD::Swarm::makeSwarmSchedulerWithLoader(&loader);
        if (!m_swarmScheduler)
        {
            m_lastLoadErrorMessage = "swarm_init: scheduler allocation failed";
#ifdef RAWR_ENABLE_VULKAN
            cleanupVulkanInit();
#endif
            return false;
        }
        RawrXD::Swarm::SchedulerConfig swarmCfg;
        swarmCfg.enableAsyncPrefetchThread = true;
        swarmCfg.admitFirstSliceOnExecutePlan = false;
        swarmCfg.prefetchAheadLayers = 2;
        swarmCfg.prefetchIoPollMs = 4;
        if (auto configureResult = m_swarmScheduler->configure(swarmCfg); !configureResult.has_value())
        {
            m_lastLoadErrorMessage = std::string("swarm_configure: ") +
                                     RawrXD::Swarm::schedulerErrorMessage(configureResult.error());
#ifdef RAWR_ENABLE_VULKAN
            cleanupVulkanInit();
#endif
            return false;
        }

        std::vector<RawrXD::Swarm::ModelSlice> swarmPlan;
        const uint64_t fileSz = loader.GetFileSizeBytes();
        const uint32_t nl = static_cast<uint32_t>(cfg.n_layers);
        
        // Validate file size and layer count are reasonable before division
        if (fileSz == 0 || nl == 0)
        {
            printf("[RawrXD] Warning: Swarm plan skipped (fileSz=%llu nl=%u)\n", fileSz, nl);
        }
        else if (fileSz > 0 && nl > 0)
        {
            swarmPlan = RawrXD::Swarm::buildLayerSlicesFromGGUF(loader, nl);
            if (!swarmPlan.empty())
            {
                std::uint64_t planTotalBytes = 0;
                std::uint64_t planMaxSlice = 0;
                for (const auto& s : swarmPlan)
                {
                    planTotalBytes += s.byteSize;
                    planMaxSlice = std::max(planMaxSlice, s.byteSize);
                }
                const double planTotalGiB = static_cast<double>(planTotalBytes) / (1024.0 * 1024.0 * 1024.0);
                const double planMaxMiB = static_cast<double>(planMaxSlice) / (1024.0 * 1024.0);
                const double planAvgMiB =
                    static_cast<double>(planTotalBytes) / static_cast<double>(swarmPlan.size()) / (1024.0 * 1024.0);
                printf("[RawrXD] Swarm plan: GGUF coalesced (%zu slices total=%.2f GiB max=%.2f MiB avg=%.2f MiB)\n",
                       swarmPlan.size(), planTotalGiB, planMaxMiB, planAvgMiB);
            }
            else
            {
                // Fallback: striped plan (only if fileSz/nl won't overflow)
                const uint64_t per = fileSz / static_cast<uint64_t>(nl);
                for (uint32_t li = 0; li < nl; ++li)
                {
                    RawrXD::Swarm::ModelSlice s;
                    s.id.modelIndex = 0;
                    s.id.layerStart = li;
                    s.id.layerEnd = li + 1u;
                    s.id.expertIndex = 0xFFFFFFFFu;
                    s.fileOffsetBytes = per * static_cast<uint64_t>(li);
                    s.byteSize = (li + 1u == nl) ? (fileSz - s.fileOffsetBytes) : per;
                    swarmPlan.push_back(std::move(s));
                }
                printf("[RawrXD] Swarm plan: file/layer stripe fallback (%u slices)\n", nl);
            }
        }
        if (auto submitResult = m_swarmScheduler->submitPlan(std::move(swarmPlan)); !submitResult.has_value())
        {
            m_lastLoadErrorMessage = std::string("swarm_submit_plan: ") +
                                     RawrXD::Swarm::schedulerErrorMessage(submitResult.error());
#ifdef RAWR_ENABLE_VULKAN
            cleanupVulkanInit();
#endif
            return false;
        }
        if (auto executeResult = m_swarmScheduler->executePlan(); !executeResult.has_value())
        {
            m_lastLoadErrorMessage = std::string("swarm_execute_plan: ") +
                                     RawrXD::Swarm::schedulerErrorMessage(executeResult.error());
#ifdef RAWR_ENABLE_VULKAN
            cleanupVulkanInit();
#endif
            return false;
        }
        transformer.SetSwarmScheduler(m_swarmScheduler.get());

        m_contextLimit = static_cast<uint32_t>(cfg.n_ctx);
        m_lastLogits.clear();

    #ifdef RAWR_ENABLE_VULKAN
        m_vkInstance = instance;
        m_vkPhysicalDevice = physDevice;
        m_vkDevice = device;
    #endif

        printf("[RawrXD] Inference engine READY\n");
        m_initialized = true;
        return true;
    }

    bool IsInitialized() const { return m_initialized; }

    /** Forwarded to RawrXDTransformer (stdout/ODS unchanged). */
    void SetLayerProgressCallback(std::function<void(const std::string&)> cb)
    {
        transformer.SetProgressCallback(std::move(cb));
    }

    /// Loader VMM / multi-slot pressure (for IDE telemetry).
    [[nodiscard]] RawrXDModelLoader::SlidingWindowTelemetry loaderSlidingWindowTelemetry() const
    {
        return loader.slidingWindowTelemetrySnapshot();
    }
    /// Swarm prefetch / eviction counters (empty scheduler returns zeros).
    [[nodiscard]] RawrXD::Swarm::SwarmRuntimeStats swarmRuntimeStats() const
    {
        if (!m_swarmScheduler)
            return {};
        return m_swarmScheduler->runtimeStats();
    }

    /// Increments on each successful `submitPlan`; use to skip `refreshSwarmPlanSliceIndex()` when unchanged.
    [[nodiscard]] std::uint64_t swarmPlanGeneration() const
    {
        if (!m_swarmScheduler)
            return 0;
        return m_swarmScheduler->planGeneration();
    }

    /// MoE mixture pack cache + prepack worker counters (for IDE output / status HUD).
    [[nodiscard]] MoEPackHudMetrics moEPackHudMetrics() const
    {
        MoEPackHudMetrics m;
        m.packHits = transformer.moeGroupedPackCacheHits();
        m.packMisses = transformer.moeGroupedPackCacheMisses();
        m.groupedFallbacks = transformer.moeGroupedFallbacks();
        m.syncPackInserts = transformer.moeGroupedSyncPackInserts();
        m.groupedWeightedApplies = transformer.moeGroupedWeightedApplies();
        m.groupedSingleExpertApplies = transformer.moeGroupedSingleExpertApplies();
        m.groupedWeightedFallbacks = transformer.moeGroupedWeightedFallbacks();
        m.groupedSingleExpertFallbacks = transformer.moeGroupedSingleExpertFallbacks();
        NormalizeMoEPackHudMetrics(m);
        m.prepackInserts = transformer.moePrepackInserts();
        m.prepackQueueDropped = transformer.moePrepackQueueDropped();
        m.prepackSkippedNotResident = transformer.moePrepackSkippedNotResident();
        m.packEvictedByPlanRow = transformer.moePackEvictedByPlanRow();
        m.prepackQueueDepthApprox = transformer.moePrepackQueueDepthApprox();
        m.packCachePackedBytes = transformer.moeMixturePackCacheCurrentPackedBytes();
        m.packCacheEvictions = transformer.moeMixturePackCacheEvictions();
        m.packCacheSelectiveRowInvalidations = transformer.moeMixturePackCacheSelectiveRowInvalidations();
        return m;
    }

    /// MoE / swarm residency grid for Win32IDE (single mutex pass on the scheduler).
    [[nodiscard]] bool CaptureSwarmExpertHeatmap(const RawrXD::Swarm::ExpertHeatmapCaptureParams& params,
                                                 RawrXD::Swarm::ExpertHeatmapSnapshot& out)
    {
        if (!m_swarmScheduler)
        {
            out = {};
            return false;
        }
        return m_swarmScheduler->captureExpertHeatmapSnapshot(params, out);
    }

    /// EMA-to-SDMA Binding: Predict next expert ID for kinetic prefetching.
    /// Queries SwarmScheduler's m_recentExpertPins to identify high-probability expert.
    /// @param current_layer: Layer index currently executing
    /// @param model_index: Model ID (multi-model swarm support)
    /// @return Predicted expert ordinal (0xFFFFFFFF if no prediction available)
    [[nodiscard]] std::uint32_t PredictNextExpertId(std::uint32_t current_layer, std::uint32_t model_index = 0) const
    {
        if (!m_swarmScheduler)
            return 0xFFFFFFFFu;  // No EMA predictor available
        
        // Query SwarmScheduler for most likely speculative expert in next layer
        // (This is a simplified heuristic - full implementation would query isLikelySpeculativeExpert_)
        // For now, return first expert in MoE layer as baseline (layer-local ordinal 0)
        // TODO: Expose SwarmScheduler::isLikelySpeculativeExpert_() or equivalent prediction API
        return 0;  // Baseline: always predict expert 0 (temporal locality)
    }

    // Expose loader metadata to facade
    int getVocabSize() const { return loader.getVocabSize(); }
    int getDim() const { return loader.getDim(); }
    int getLayers() const { return loader.getLayers(); }
    int getHeads() const { return loader.getHeads(); }
    int getKVHeads() const { return loader.getKVHeads(); }
    uint32_t getContextLimit() const { return m_contextLimit; }

    std::vector<uint32_t> Tokenize(const std::string& text)
    {
        if (!m_initialized)
            return {};
        return tokenizer.Encode(text);
    }

    std::string Detokenize(const std::vector<uint32_t>& tokens)
    {
        if (!m_initialized)
            return {};
        return tokenizer.Decode(tokens);
    }

    // Token pipeline tracing API
    TitanTokenTraceBuffer<256>& GetTokenTraceBuffer() { return m_tokenTraceBuffer; }

    // Dump recent traces to JSON summary (useful for diagnostics)
    std::string DumpTokenTraceSummary(size_t lastNTokens = 128)
    {
        TitanTokenTrace::Stalls avg_stalls = {};
        uint8_t dominant_cause = 0;
        m_tokenTraceBuffer.compute_aggregates(lastNTokens, avg_stalls, dominant_cause);

        char buffer[512];
        snprintf(buffer, sizeof(buffer),
            "{\n"
            "  \"avg_tps\": %.2f,\n"
            "  \"avg_compute_ms\": %u,\n"
            "  \"avg_weight_ms\": %u,\n"
            "  \"avg_sched_ms\": %u,\n"
            "  \"avg_kv_ms\": %u,\n"
            "  \"dominant_cause\": \"%s\"\n"
            "}\n",
            (avg_stalls.total_time_us > 0) ? (1000000.0f / avg_stalls.total_time_us) : 0.0f,
            avg_stalls.compute_time_us / 1000,
            avg_stalls.weight_latency_us / 1000,
            avg_stalls.sched_delay_us / 1000,
            avg_stalls.kv_latency_us / 1000,
            TitanTokenTrace::cause_name(dominant_cause));

        return std::string(buffer);
    }

    // Dump all token traces to CSV file for offline analysis
    void DumpTokenTracesToCSV(const char* filepath)
    {
        m_tokenTraceBuffer.dump_csv(filepath);
    }

    // Diagnose a specific token for corruption/misalignment issues
    std::string DiagnoseToken(uint32_t token_id)
    {
        if (!m_initialized)
            return "ERROR: inference not initialized";

        // Attempt to decode the token
        std::vector<uint32_t> tokens = {token_id};
        std::string decoded = tokenizer.DecodeSafe(tokens);

        // Check bounds
        int vocab_size = loader.getVocabSize();
        bool id_valid = UTF8Validator::is_valid_token_id(token_id, vocab_size);

        // Check for replacement characters
        bool has_replacement = UTF8Validator::contains_replacement_char(decoded);

        // Check UTF-8 validity
        bool valid_utf8 = UTF8Validator::is_valid_utf8_string(decoded.c_str());

        char buffer[256];
        snprintf(buffer, sizeof(buffer),
            "token_id=%u vocab_size=%d valid_id=%d decoded_len=%zu valid_utf8=%d has_replacement=%d",
            token_id,
            vocab_size,
            id_valid ? 1 : 0,
            decoded.length(),
            valid_utf8 ? 1 : 0,
            has_replacement ? 1 : 0);

        return std::string(buffer);
    }

    std::vector<float> ForwardTokens(const std::vector<uint32_t>& tokens, uint32_t startPos = 0)
    {
        if (!m_initialized || tokens.empty())
        {
            return {};
        }
        if (startPos > static_cast<uint32_t>(std::numeric_limits<int>::max()))
        {
            m_lastLoadErrorMessage = "forward_tokens: startPos exceeds transformer int range";
            return {};
        }

        try
        {
            m_lastLogits = transformer.Forward(tokens, static_cast<int>(startPos));
        }
        catch (const std::exception& ex)
        {
            m_lastLoadErrorMessage = std::string("forward_tokens: ") + ex.what();
            m_lastLogits.clear();
            return {};
        }
        catch (...)
        {
            m_lastLoadErrorMessage = "forward_tokens: unknown exception";
            m_lastLogits.clear();
            return {};
        }
        return m_lastLogits;
    }

    const std::vector<float>& LastLogits() const { return m_lastLogits; }

    std::vector<uint32_t> GenerateFromTokens(const std::vector<uint32_t>& promptTokens, uint32_t maxTokens = 512,
                                             std::function<void(uint32_t, const std::string&)> callback = nullptr)
    {
        std::vector<uint32_t> generated;
        if (!m_initialized || maxTokens == 0 || promptTokens.empty())
        {
            return generated;
        }
        maxTokens = std::min<uint32_t>(maxTokens, 8192);

        std::vector<uint32_t> tokens = promptTokens;
        const uint32_t vocabSize = std::max(1, loader.getVocabSize());

        // Keep right-most context when prompt exceeds model context.
        if (m_contextLimit > 0 && tokens.size() > m_contextLimit)
        {
            tokens.erase(tokens.begin(), tokens.end() - m_contextLimit);
        }
        
        // Initialize trace for this generation session
        TitanTokenTrace currentTrace = {};
        currentTrace.batch_id = static_cast<uint16_t>((uintptr_t)this & 0xFFFF);  // Use object address as session ID
        currentTrace.vocab_size = vocabSize;
        currentTrace.layer_count = loader.getLayers();
        currentTrace.expert_count = 0;  // TODO: extract from MoE config if present
        currentTrace.record_t0();  // Mark generation loop start
        
        // Validate all input prompt tokens are within vocab bounds
        for (auto& t : tokens)
        {
            if (t >= vocabSize) {
                m_lastLoadErrorMessage = "generate_from_tokens: input prompt contains invalid token (token=" 
                    + std::to_string(t) + ", vocabSize=" + std::to_string(vocabSize) + ")";
                m_lastLogits.clear();
                return generated;
            }
        }

        std::vector<float> logits;
        try
        {
            currentTrace.record_t1();  // Scheduler dispatch
            logits = transformer.Forward(tokens, 0);
            currentTrace.record_t2();  // Weights ready
        }
        catch (const std::exception& ex)
        {
            m_lastLoadErrorMessage = std::string("generate_from_tokens: initial forward failed: ") + ex.what();
            m_lastLogits.clear();
            return generated;
        }
        catch (...)
        {
            m_lastLoadErrorMessage = "generate_from_tokens: initial forward failed with unknown exception";
            m_lastLogits.clear();
            return generated;
        }
        m_lastLogits = logits;
        uint32_t absolutePos = static_cast<uint32_t>(tokens.size());

        // Emit one prefill-stage trace row so diagnostics survive long prefill runs
        // even if generation does not reach the first sampled token yet.
        {
            TitanTokenTrace prefillTrace = currentTrace;
            prefillTrace.token_id = tokens.empty() ? 0u : tokens.back();
            prefillTrace.record_t3();
            prefillTrace.record_t4();
            prefillTrace.record_t5();
            prefillTrace.record_t6();
            prefillTrace.compute_stalls();
            m_tokenTraceBuffer.push(prefillTrace);
        }

        for (uint32_t i = 0; i < maxTokens; i++)
        {
            if (logits.empty())
                break;
            
            // Validation: logits array must contain exactly vocabSize elements
            // Mismatch indicates Forward() error or model config corruption
            if (static_cast<uint32_t>(logits.size()) != vocabSize) {
                m_lastLoadErrorMessage = "generate_from_tokens: logits size mismatch (got " 
                    + std::to_string(logits.size()) + ", expected " + std::to_string(vocabSize) + ")";
                m_lastLogits.clear();
                return generated;
            }

            bool hasFinite = false;
            for (float v : logits)
            {
                if (std::isfinite(v))
                {
                    hasFinite = true;
                    break;
                }
            }
            if (!hasFinite)
                break;

            uint32_t nextToken = 0;
            try
            {
                currentTrace.record_t3();  // Compute begin (pre-sample)
                nextToken = sampler.Sample(logits.data(), logits.size(), tokens);
                currentTrace.record_t4();  // Compute end (post-sample softmax)
            }
            catch (const std::exception& ex)
            {
                m_lastLoadErrorMessage = std::string("generate_from_tokens: sampler failed: ") + ex.what();
                m_lastLogits.clear();
                return generated;
            }
            catch (...)
            {
                m_lastLoadErrorMessage = "generate_from_tokens: sampler failed with unknown exception";
                m_lastLogits.clear();
                return generated;
            }
            
            // Token must be within vocab bounds (no silent wrapping allowed)
            if (nextToken >= vocabSize)
            {
                m_lastLoadErrorMessage = "generate_from_tokens: sampler returned invalid token (token=" 
                    + std::to_string(nextToken) + ", vocabSize=" + std::to_string(vocabSize) + ")";
                m_lastLogits.clear();
                return generated;
            }
            
            currentTrace.token_id = nextToken;
            currentTrace.record_t5();  // KV cache updated
            
            tokens.push_back(nextToken);
            if (m_contextLimit > 0 && tokens.size() > m_contextLimit)
            {
                tokens.erase(tokens.begin(), tokens.end() - m_contextLimit);
            }
            generated.push_back(nextToken);
            
            // Record token emission and compute stall attribution
            currentTrace.record_t6();
            currentTrace.compute_stalls();
            m_tokenTraceBuffer.push(currentTrace);
            
            // Prepare for next iteration
            currentTrace.record_t0();

            if (nextToken == 2)
                break;

            std::vector<uint32_t> nextTokVec = {nextToken};
            if (absolutePos > static_cast<uint32_t>(std::numeric_limits<int>::max()))
            {
                m_lastLoadErrorMessage = "generate_from_tokens: absolute position exceeds transformer int range";
                m_lastLogits.clear();
                return generated;
            }
            try
            {
                logits = transformer.Forward(nextTokVec, static_cast<int>(absolutePos));
            }
            catch (const std::exception& ex)
            {
                m_lastLoadErrorMessage = std::string("generate_from_tokens: incremental forward failed: ") + ex.what();
                m_lastLogits.clear();
                return generated;
            }
            catch (...)
            {
                m_lastLoadErrorMessage = "generate_from_tokens: incremental forward failed with unknown exception";
                m_lastLogits.clear();
                return generated;
            }
            if (absolutePos == std::numeric_limits<uint32_t>::max())
            {
                m_lastLoadErrorMessage = "generate_from_tokens: absolute position overflow";
                m_lastLogits.clear();
                return generated;
            }
            absolutePos++;
            m_lastLogits = logits;

            if (callback)
            {
                const std::string piece = tokenizer.DecodeSafe({nextToken});
                try
                {
                    callback(nextToken, piece);
                }
                catch (...)
                {
                    // Callback failures should not crash inference.
                }
            }
        }

        return generated;
    }

    std::string Generate(const std::string& prompt, uint32_t maxTokens = 512,
                         std::function<void(const std::string&)> callback = nullptr)
    {
        if (!m_initialized || maxTokens == 0)
        {
            return {};
        }
        maxTokens = std::min<uint32_t>(maxTokens, 8192);

        const auto tokens = tokenizer.Encode(prompt);
        if (tokens.empty())
        {
            return {};
        }

        std::string fullResponse;
        auto generated = GenerateFromTokens(tokens, maxTokens,
                                            [&](uint32_t, const std::string& piece)
                                            {
                                                if (callback)
                                                    callback(piece);
                                                fullResponse += piece;
                                            });
        (void)generated;
        return fullResponse;
    }
};
