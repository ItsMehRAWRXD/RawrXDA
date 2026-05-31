#include "runtime/RuntimeProvider.h"

#include "standalone_llama_runtime.hpp"

#include <windows.h>
#include <intrin.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cwctype>
#include <cstdint>
#include <thread>
#include <vector>

#if __has_include(<vulkan/vulkan.h>)
#include <vulkan/vulkan.h>
#define RAWRXD_RUNTIME_HAS_VULKAN 1
#else
#define RAWRXD_RUNTIME_HAS_VULKAN 0
#endif

namespace RawrXD::Runtime {

namespace {

constexpr uint32_t kAmdVendorId = 0x1002;

struct VulkanProbeResult {
    bool found = false;
    bool is7800Class = false;
    uint64_t deviceLocalHeapBytes = 0;
    int recommendedGpuLayers = 0;
    std::string backendLabel;
};

struct Win32TokenMessengerState {
    std::mutex mu;
    HWND hwnd = nullptr;
    UINT messageId = 0;
};

Win32TokenMessengerState g_win32TokenMessenger;
std::atomic<uint64_t> g_runtimeStreamSessionSeq{1};

std::string ToUpperAscii(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
    return s;
}

std::string WideToUtf8(const std::wstring& ws) {
    if (ws.empty()) {
        return {};
    }
    int bytes = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), static_cast<int>(ws.size()), nullptr, 0, nullptr, nullptr);
    if (bytes <= 0) {
        return {};
    }
    std::string out(static_cast<size_t>(bytes), '\0');
    WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), static_cast<int>(ws.size()), out.data(), bytes, nullptr, nullptr);
    return out;
}

bool SupportsOsAvx512State() {
#if defined(_M_X64) || defined(__x86_64__)
    int cpuInfo[4] = {0, 0, 0, 0};
    __cpuidex(cpuInfo, 1, 0);
    const bool osxsave = (cpuInfo[2] & (1 << 27)) != 0;
    if (!osxsave) {
        return false;
    }

    const unsigned long long xcr0 = _xgetbv(0);
    constexpr unsigned long long kAvx512Mask = (1ull << 1) | (1ull << 2) | (1ull << 5) | (1ull << 6) | (1ull << 7);
    return (xcr0 & kAvx512Mask) == kAvx512Mask;
#else
    return false;
#endif
}

int RecommendGpuLayersFromVramBytes(uint64_t bytes) {
    const uint64_t gib = bytes / (1024ull * 1024ull * 1024ull);
    if (gib >= 14ull) return 99;
    if (gib >= 12ull) return 90;
    if (gib >= 10ull) return 80;
    if (gib >= 8ull) return 64;
    if (gib >= 6ull) return 48;
    if (gib >= 4ull) return 32;
    return 16;
}

uint64_t GetFileSizeBytes(const std::wstring& path) {
    WIN32_FILE_ATTRIBUTE_DATA attrs{};
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &attrs)) {
        return 0;
    }
    ULARGE_INTEGER size{};
    size.HighPart = attrs.nFileSizeHigh;
    size.LowPart = attrs.nFileSizeLow;
    return static_cast<uint64_t>(size.QuadPart);
}

std::vector<int> BuildAdaptiveGpuLayerAttempts(int recommended) {
    std::vector<int> attempts;
    int layer = std::clamp(recommended, 1, 99);
    attempts.push_back(layer);

    while (layer > 1) {
        if (layer > 64) {
            layer -= 16;
        } else if (layer > 32) {
            layer -= 8;
        } else {
            layer -= 4;
        }
        if (layer < 1) {
            layer = 1;
        }
        if (attempts.back() != layer) {
            attempts.push_back(layer);
        }
    }

    return attempts;
}

#if RAWRXD_RUNTIME_HAS_VULKAN
VulkanProbeResult ProbeAmdVulkanDevice() {
    VulkanProbeResult out;

    HMODULE vulkan = LoadLibraryW(L"vulkan-1.dll");
    if (!vulkan) {
        return out;
    }

    auto vkGetInstanceProcAddrFn = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
        GetProcAddress(vulkan, "vkGetInstanceProcAddr"));
    if (!vkGetInstanceProcAddrFn) {
        FreeLibrary(vulkan);
        return out;
    }

    auto vkCreateInstanceFn = reinterpret_cast<PFN_vkCreateInstance>(
        vkGetInstanceProcAddrFn(VK_NULL_HANDLE, "vkCreateInstance"));
    if (!vkCreateInstanceFn) {
        FreeLibrary(vulkan);
        return out;
    }

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "RawrXD RuntimeProvider";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "RawrXD";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    VkInstance instance = VK_NULL_HANDLE;
    if (vkCreateInstanceFn(&createInfo, nullptr, &instance) != VK_SUCCESS || instance == VK_NULL_HANDLE) {
        FreeLibrary(vulkan);
        return out;
    }

    auto vkDestroyInstanceFn = reinterpret_cast<PFN_vkDestroyInstance>(
        vkGetInstanceProcAddrFn(instance, "vkDestroyInstance"));
    auto vkEnumeratePhysicalDevicesFn = reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(
        vkGetInstanceProcAddrFn(instance, "vkEnumeratePhysicalDevices"));
    auto vkGetPhysicalDevicePropertiesFn = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(
        vkGetInstanceProcAddrFn(instance, "vkGetPhysicalDeviceProperties"));
    auto vkGetPhysicalDeviceMemoryPropertiesFn = reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(
        vkGetInstanceProcAddrFn(instance, "vkGetPhysicalDeviceMemoryProperties"));

    if (!vkDestroyInstanceFn || !vkEnumeratePhysicalDevicesFn || !vkGetPhysicalDevicePropertiesFn ||
        !vkGetPhysicalDeviceMemoryPropertiesFn) {
        vkDestroyInstanceFn(instance, nullptr);
        FreeLibrary(vulkan);
        return out;
    }

    uint32_t deviceCount = 0;
    if (vkEnumeratePhysicalDevicesFn(instance, &deviceCount, nullptr) != VK_SUCCESS || deviceCount == 0) {
        vkDestroyInstanceFn(instance, nullptr);
        FreeLibrary(vulkan);
        return out;
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    if (vkEnumeratePhysicalDevicesFn(instance, &deviceCount, devices.data()) != VK_SUCCESS) {
        vkDestroyInstanceFn(instance, nullptr);
        FreeLibrary(vulkan);
        return out;
    }

    uint64_t bestHeapBytes = 0;
    bool bestIs7800 = false;
    std::string bestName;

    for (const auto& dev : devices) {
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDevicePropertiesFn(dev, &props);

        if (props.vendorID != kAmdVendorId || props.deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            continue;
        }

        VkPhysicalDeviceMemoryProperties memProps{};
        vkGetPhysicalDeviceMemoryPropertiesFn(dev, &memProps);

        uint64_t localHeapMax = 0;
        for (uint32_t i = 0; i < memProps.memoryHeapCount; ++i) {
            if ((memProps.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) != 0) {
                localHeapMax = (std::max)(localHeapMax, static_cast<uint64_t>(memProps.memoryHeaps[i].size));
            }
        }

        const std::string name = props.deviceName;
        const std::string upperName = ToUpperAscii(name);
        const bool is7800 = (upperName.find("7800") != std::string::npos && upperName.find("XT") != std::string::npos) ||
                            (upperName.find("RX 7800") != std::string::npos);

        const bool isBetter =
            (!out.found) ||
            (is7800 && !bestIs7800) ||
            (is7800 == bestIs7800 && localHeapMax > bestHeapBytes);

        if (isBetter) {
            out.found = true;
            bestIs7800 = is7800;
            out.is7800Class = is7800;
            bestHeapBytes = localHeapMax;
            bestName = name;
        }
    }

    if (out.found) {
        out.deviceLocalHeapBytes = bestHeapBytes;
        out.recommendedGpuLayers = RecommendGpuLayersFromVramBytes(bestHeapBytes);
        out.backendLabel = "Vulkan: " + bestName;
        if (out.is7800Class) {
            out.backendLabel += " [7800 XT lane]";
        }
    }

    vkDestroyInstanceFn(instance, nullptr);
    FreeLibrary(vulkan);
    return out;
}
#endif

}  // namespace

RuntimeProvider::RuntimeProvider() = default;

RuntimeProvider::~RuntimeProvider() {
    UnloadModel();
}

void RuntimeProvider::ConfigureWin32TokenMessenger(void* hwnd, unsigned int messageId) {
    std::lock_guard<std::mutex> lock(g_win32TokenMessenger.mu);
    g_win32TokenMessenger.hwnd = reinterpret_cast<HWND>(hwnd);
    g_win32TokenMessenger.messageId = static_cast<UINT>(messageId);
}

bool RuntimeProvider::LoadModel(const std::wstring& modelPath) {
    const auto start = std::chrono::steady_clock::now();

    std::lock_guard<std::mutex> lock(m_mutex);
    m_lastError.clear();
    m_telemetry = RuntimeTelemetry{};

    auto runtime = std::make_shared<Standalone::LlamaRuntime>();
    std::string error;
    if (!runtime->ensure_initialized(error)) {
        m_lastError = error.empty() ? "Runtime initialization failed" : error;
        m_isLoaded = false;
        return false;
    }

    m_policy = AutoDetectHardware();

    bool loaded = false;
    int selectedGpuLayers = 0;

    if (m_policy.gpuOffload) {
        const auto attempts = BuildAdaptiveGpuLayerAttempts(m_policy.gpuLayers);
        for (const int candidateLayers : attempts) {
            error.clear();
            if (runtime->ensure_model_loaded(modelPath, candidateLayers, error)) {
                loaded = true;
                selectedGpuLayers = candidateLayers;
                break;
            }
        }
    } else {
        loaded = runtime->ensure_model_loaded(modelPath, 0, error);
    }

    if (!loaded) {
        m_lastError = error.empty() ? "Model load failed" : error;
        m_isLoaded = false;
        return false;
    }

    if (m_policy.gpuOffload) {
        m_policy.gpuLayers = selectedGpuLayers;
    }

    m_runtime = std::move(runtime);
    m_loadedModelPath = modelPath;
    m_isLoaded = true;

    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);
    m_telemetry.loadTimeMs = static_cast<long long>(elapsed.count());
    m_telemetry.activeBackend = m_policy.backendLabel;
    if (m_policy.gpuOffload) {
        m_telemetry.activeBackend += " (layers=" + std::to_string(m_policy.gpuLayers) + ")";
    }
    m_telemetry.vramTotalBytes = m_policy.vramTotalBytes;

    if (m_policy.gpuOffload) {
        const uint64_t modelBytes = GetFileSizeBytes(modelPath);
        if (m_policy.vramTotalBytes > 0) {
            m_telemetry.vramUsageBytes = (std::min)(modelBytes, static_cast<uint64_t>(m_policy.vramTotalBytes));
        } else {
            m_telemetry.vramUsageBytes = modelBytes;
        }
    }

    return true;
}

void RuntimeProvider::UnloadModel() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_runtime.reset();
    m_isLoaded = false;
    m_loadedModelPath.clear();
    m_policy = HardwarePolicy{};
    m_telemetry = RuntimeTelemetry{};
    m_lastError.clear();
}

bool RuntimeProvider::Generate(const GenParams& params) {
    if (params.prompt.empty() || params.maxTokens <= 0) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_lastError = "Invalid generation request";
        return false;
    }

    std::shared_ptr<Standalone::LlamaRuntime> runtime;
    HardwarePolicy policy;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_isLoaded || !m_runtime) {
            m_lastError = "Model is not loaded";
            return false;
        }
        runtime = m_runtime;
        policy = m_policy;
    }

    const uint64_t streamSessionId = g_runtimeStreamSessionSeq.fetch_add(1, std::memory_order_relaxed);

    const auto result = runtime->generate(
        params.prompt,
        params.maxTokens,
        [&](const std::string& token)
        {
            DispatchWin32Token(token, streamSessionId);
            if (params.onToken) {
                params.onToken(token);
            }
        });
    if (!result.ok) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_lastError = result.error.empty() ? "Generation failed" : result.error;
        return false;
    }

    const float ttft = (result.generated_tokens > 0)
                           ? static_cast<float>(std::max(0.0, result.ttft_ms) / 1000.0)
                           : 0.0f;
    const float tps = (result.generated_tokens > 0 && result.t_gen_ms > 0.0)
                          ? static_cast<float>(static_cast<double>(result.generated_tokens) / (result.t_gen_ms / 1000.0))
                          : 0.0f;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_telemetry.timeToFirstTokenS = ttft;
        m_telemetry.tokensPerSecond = tps;
        if (m_telemetry.activeBackend.empty()) {
            m_telemetry.activeBackend = policy.backendLabel;
        }
        m_lastError.clear();
    }

    return true;
}

bool RuntimeProvider::IsModelLoaded() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_isLoaded;
}

std::string RuntimeProvider::GetLastError() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_lastError;
}

RuntimeProvider::HardwarePolicy RuntimeProvider::AutoDetectHardware() const {
    HardwarePolicy policy;
    const char* disableVkEnv = std::getenv("GGML_RXD_VK_DISABLE");
    const bool disableVk = disableVkEnv && disableVkEnv[0] != '\0' && disableVkEnv[0] != '0';

    bool hasAvx512F = false;
    bool hasAvx512BW = false;
    const bool avx512 = DetectAvx512(hasAvx512F, hasAvx512BW);

    policy.avx512f = hasAvx512F;
    policy.avx512bw = hasAvx512BW;

    const int cores = DetectPhysicalCoreCount();
    policy.cpuThreads = std::max(4, cores - 1);

    bool gpuCandidate = false;
    int gpuLayers = 0;
    std::string gpuLabel;
    uint64_t gpuVramBytes = 0;

#if RAWRXD_RUNTIME_HAS_VULKAN
    if (!disableVk) {
        const VulkanProbeResult probe = ProbeAmdVulkanDevice();
        gpuCandidate = probe.found;
        gpuLayers = probe.recommendedGpuLayers;
        gpuLabel = probe.backendLabel;
        gpuVramBytes = probe.deviceLocalHeapBytes;
    }
#else
    if (!disableVk) {
        gpuCandidate = DetectAmdAdapter();
        gpuLayers = 99;
        gpuLabel = "Vulkan (AMD)";
        gpuVramBytes = 0;
    }
#endif

    gpuCandidate = gpuCandidate && LlamaSupportsGpuOffload();
    if (gpuCandidate) {
        policy.gpuOffload = true;
        policy.gpuLayers = std::clamp(gpuLayers, 1, 99);
        policy.vramTotalBytes = static_cast<unsigned long long>(gpuVramBytes);
        policy.backendLabel = gpuLabel.empty() ? "Vulkan (AMD)" : gpuLabel;
    } else if (avx512) {
        policy.backendLabel = disableVk ? "CPU (AVX-512, Vulkan disabled)" : "CPU (AVX-512)";
    } else {
        policy.backendLabel = disableVk ? "CPU (Fallback, Vulkan disabled)" : "CPU (Fallback)";
        policy.cpuThreads = std::clamp(policy.cpuThreads, 4, 8);
    }

    return policy;
}

bool RuntimeProvider::DetectAvx512(bool& hasF, bool& hasBW) {
    hasF = false;
    hasBW = false;

#if defined(_M_X64) || defined(__x86_64__)
    if (!SupportsOsAvx512State()) {
        return false;
    }

    int regs[4] = {0, 0, 0, 0};
    __cpuidex(regs, 0, 0);
    if (regs[0] < 7) {
        return false;
    }

    __cpuidex(regs, 7, 0);
    hasF = (regs[1] & (1 << 16)) != 0;
    hasBW = (regs[1] & (1 << 30)) != 0;
    return hasF && hasBW;
#else
    return false;
#endif
}

bool RuntimeProvider::LlamaSupportsGpuOffload() {
    HMODULE module = GetModuleHandleW(L"llama.dll");
    bool loadedHere = false;
    if (!module) {
        module = LoadLibraryW(L"llama.dll");
        loadedHere = module != nullptr;
    }

    if (!module) {
        return false;
    }

    using FnSupportsGpuOffload = bool (*)();
    auto fn = reinterpret_cast<FnSupportsGpuOffload>(GetProcAddress(module, "llama_supports_gpu_offload"));
    const bool supported = (fn != nullptr) ? fn() : false;

    if (loadedHere) {
        FreeLibrary(module);
    }
    return supported;
}

bool RuntimeProvider::DetectAmdAdapter() {
    DISPLAY_DEVICEW dd = {};
    dd.cb = sizeof(dd);
    for (DWORD i = 0; EnumDisplayDevicesW(nullptr, i, &dd, 0); ++i) {
        if ((dd.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP) == 0) {
            dd = DISPLAY_DEVICEW{};
            dd.cb = sizeof(dd);
            continue;
        }

        std::wstring name = dd.DeviceString;
        std::transform(name.begin(), name.end(), name.begin(), [](wchar_t ch) {
            return static_cast<wchar_t>(std::towupper(ch));
        });

        if (name.find(L"AMD") != std::wstring::npos || name.find(L"RADEON") != std::wstring::npos) {
            return true;
        }

        dd = DISPLAY_DEVICEW{};
        dd.cb = sizeof(dd);
    }
    return false;
}

int RuntimeProvider::DetectPhysicalCoreCount() {
    DWORD len = 0;
    GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &len);
    if (len == 0) {
        const unsigned hc = std::thread::hardware_concurrency();
        return hc > 0 ? static_cast<int>(hc) : 8;
    }

    std::vector<unsigned char> buffer(len);
    auto* info = reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buffer.data());
    if (!GetLogicalProcessorInformationEx(RelationProcessorCore, info, &len)) {
        const unsigned hc = std::thread::hardware_concurrency();
        return hc > 0 ? static_cast<int>(hc) : 8;
    }

    int cores = 0;
    DWORD offset = 0;
    while (offset < len) {
        auto* entry = reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buffer.data() + offset);
        if (entry->Relationship == RelationProcessorCore) {
            ++cores;
        }
        if (entry->Size == 0) {
            break;
        }
        offset += entry->Size;
    }

    if (cores <= 0) {
        const unsigned hc = std::thread::hardware_concurrency();
        return hc > 0 ? static_cast<int>(hc) : 8;
    }

    return cores;
}

void RuntimeProvider::DispatchWin32Token(const std::string& token, uint64_t sessionId) {
    if (token.empty()) {
        return;
    }

    HWND target = nullptr;
    UINT messageId = 0;
    {
        std::lock_guard<std::mutex> lock(g_win32TokenMessenger.mu);
        target = g_win32TokenMessenger.hwnd;
        messageId = g_win32TokenMessenger.messageId;
    }

    if (!target || messageId == 0 || !IsWindow(target)) {
        return;
    }

    char* payload = _strdup(token.c_str());
    if (!payload) {
        return;
    }

    if (!PostMessageA(target, messageId, static_cast<WPARAM>(sessionId), reinterpret_cast<LPARAM>(payload))) {
        free(payload);
    }
}

}  // namespace RawrXD::Runtime
