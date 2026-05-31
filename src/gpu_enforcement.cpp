// d:/rawrxd/src/gpu_enforcement.cpp
// =============================================================================
// RawrXD GPU Enforcement Gate — implementation.
// No-toggle, fail-closed GPU requirement for IDE / CLI / Model / Codec.
// =============================================================================
#include "gpu_enforcement.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>

#if defined(_WIN32)
  #include <windows.h>
#endif

#include <string_view>

// Vulkan via ggml backend. The ggml-vulkan target may or may not be linked
// into the final binary; we weakly reference the Vulkan probe symbols and
// fall back to a stub returning 0 when the Vulkan backend is absent.
extern "C" {
    int  ggml_backend_vk_get_device_count(void);
    void ggml_backend_vk_get_device_description(int device, char * description, size_t description_size);
    void ggml_backend_vk_get_device_memory(int device, size_t * free, size_t * total);
}

#if defined(_MSC_VER)
extern "C" int  rxd_gpu_vk_device_count_stub(void) { return 0; }
extern "C" void rxd_gpu_vk_device_description_stub(int, char * d, size_t n) { if (d && n) d[0] = '\0'; }
extern "C" void rxd_gpu_vk_device_memory_stub(int, size_t * f, size_t * t) { if (f) *f = 0; if (t) *t = 0; }
#pragma comment(linker, "/alternatename:ggml_backend_vk_get_device_count=rxd_gpu_vk_device_count_stub")
#pragma comment(linker, "/alternatename:ggml_backend_vk_get_device_description=rxd_gpu_vk_device_description_stub")
#pragma comment(linker, "/alternatename:ggml_backend_vk_get_device_memory=rxd_gpu_vk_device_memory_stub")
#endif

// CUDA / HIP probes are weakly referenced; if the symbol is missing at link
// time, the corresponding probe simply returns 0. On MSVC we use /alternatename
// fallbacks for the optional symbols.
#if defined(_MSC_VER)
extern "C" int ggml_rxd_backend_cuda_get_device_count(void);
extern "C" int ggml_rxd_backend_hip_get_device_count(void);
extern "C" int rxd_gpu_cuda_device_count_stub(void) { return 0; }
extern "C" int rxd_gpu_hip_device_count_stub(void)  { return 0; }
#pragma comment(linker, "/alternatename:ggml_rxd_backend_cuda_get_device_count=rxd_gpu_cuda_device_count_stub")
#pragma comment(linker, "/alternatename:ggml_rxd_backend_hip_get_device_count=rxd_gpu_hip_device_count_stub")
#else
extern "C" __attribute__((weak)) int ggml_rxd_backend_cuda_get_device_count(void) { return 0; }
extern "C" __attribute__((weak)) int ggml_rxd_backend_hip_get_device_count(void)  { return 0; }
#endif

namespace rxd::gpu {

namespace {

using VkGetDeviceCountFn = int (*)(void);
using VkGetDeviceDescriptionFn = void (*)(int, char *, size_t);
using VkGetDeviceMemoryFn = void (*)(int, size_t *, size_t *);

struct VulkanProbe {
    HMODULE module = nullptr;
    VkGetDeviceCountFn get_device_count = nullptr;
    VkGetDeviceDescriptionFn get_device_description = nullptr;
    VkGetDeviceMemoryFn get_device_memory = nullptr;
};

VulkanProbe load_vulkan_probe() {
    VulkanProbe probe{};
#if defined(_WIN32)
    probe.module = LoadLibraryA("ggml-vulkan.dll");
    if (!probe.module) {
        std::fprintf(stderr, "[RawrXD][GPU] Failed to load ggml-vulkan.dll: %lu\n", GetLastError());
        return probe;
    }

    probe.get_device_count = reinterpret_cast<VkGetDeviceCountFn>(
        GetProcAddress(probe.module, "ggml_backend_vk_get_device_count"));
    probe.get_device_description = reinterpret_cast<VkGetDeviceDescriptionFn>(
        GetProcAddress(probe.module, "ggml_backend_vk_get_device_description"));
    probe.get_device_memory = reinterpret_cast<VkGetDeviceMemoryFn>(
        GetProcAddress(probe.module, "ggml_backend_vk_get_device_memory"));

    if (!probe.get_device_count || !probe.get_device_description || !probe.get_device_memory) {
        std::fprintf(stderr,
            "[RawrXD][GPU] ggml-vulkan.dll missing required probe exports"
            " (count=%p desc=%p mem=%p)\n",
            reinterpret_cast<void*>(probe.get_device_count),
            reinterpret_cast<void*>(probe.get_device_description),
            reinterpret_cast<void*>(probe.get_device_memory));
        FreeLibrary(probe.module);
        probe = {};
        return probe;
    }

    std::fprintf(stderr, "[RawrXD][GPU] Loaded ggml-vulkan.dll for Vulkan backend detection\n");
#endif
    return probe;
}

std::once_flag         g_once;
std::atomic<bool>      g_active{false};
Status                 g_status{};

[[noreturn]] void fatal_no_gpu() {
    const char* msg =
        "[RawrXD][FATAL] No GPU backend available (Vulkan/CUDA/HIP). "
        "GPU inference is REQUIRED and cannot be disabled. "
        "Install Vulkan runtime or a supported GPU driver and retry.\n";
    std::fputs(msg, stderr);
#if defined(_WIN32)
    OutputDebugStringA(msg);
#endif
    std::fflush(stderr);
    std::abort();
}

void detect_locked() {
    g_status.active = Backend::None;
    g_status.device_count = 0;
    g_status.device_name[0] = '\0';
    g_status.vram_total_bytes = 0;
    g_status.vram_free_bytes  = 0;

    // 0. Parity CPU fallback — explicit opt-in for hardware-free parity tests.
    //    Activated only when RAWRXD_PARITY_CPU=1 is set. Bypasses the fail-closed
    //    GPU gate so CLI/UI deterministic-trace diffing can run on any machine.
    {
        const char* pc = std::getenv("RAWRXD_PARITY_CPU");
        if (pc && pc[0] != '\0' && std::strcmp(pc, "0") != 0)
        {
            g_status.active       = Backend::None;
            g_status.device_count = 1;
            std::snprintf(g_status.device_name, sizeof(g_status.device_name),
                          "ParityCpuFallback (RAWRXD_PARITY_CPU)");
            g_active.store(true, std::memory_order_release);
            std::fprintf(stderr,
                "[RawrXD][GPU] Parity CPU fallback ACTIVE — GPU gate bypassed for deterministic trace test only.\n");
            return;
        }
    }

    // 1. Vulkan first (broadest hardware coverage incl. AMD RDNA3).
    // Resolve probe symbols from ggml-vulkan.dll at runtime so we don't
    // silently fall back to the MSVC alternatename stubs when the import
    // library is not linked into the final executable.
    const VulkanProbe vk = load_vulkan_probe();
    int vk_count = 0;
    try {
        vk_count = vk.get_device_count ? vk.get_device_count() : 0;
    } catch (...) {
        vk_count = 0;
    }
    if (vk_count > 0) {
        g_status.active       = Backend::Vulkan;
        g_status.device_count = vk_count;
        vk.get_device_description(0, g_status.device_name, sizeof(g_status.device_name));
        size_t fr = 0, tot = 0;
        vk.get_device_memory(0, &fr, &tot);
        g_status.vram_free_bytes  = fr;
        g_status.vram_total_bytes = tot;
        g_active.store(true, std::memory_order_release);
        std::fprintf(stderr, "[RawrXD][GPU] Vulkan locked: %s (%llu MiB free / %llu MiB total) across %d device(s)\n",
                     g_status.device_name,
                     (unsigned long long)(g_status.vram_free_bytes  >> 20),
                     (unsigned long long)(g_status.vram_total_bytes >> 20),
                     g_status.device_count);
        return;
    }

    // 2. CUDA.
    int cu = 0;
    try { cu = ggml_rxd_backend_cuda_get_device_count(); } catch (...) { cu = 0; }
    if (cu > 0) {
        g_status.active       = Backend::Cuda;
        g_status.device_count = cu;
        std::snprintf(g_status.device_name, sizeof(g_status.device_name), "CUDA device 0");
        g_active.store(true, std::memory_order_release);
        std::fprintf(stderr, "[RawrXD][GPU] CUDA locked: %d device(s)\n", cu);
        return;
    }

    // 3. HIP / ROCm.
    int hp = 0;
    try { hp = ggml_rxd_backend_hip_get_device_count(); } catch (...) { hp = 0; }
    if (hp > 0) {
        g_status.active       = Backend::Hip;
        g_status.device_count = hp;
        std::snprintf(g_status.device_name, sizeof(g_status.device_name), "HIP device 0");
        g_active.store(true, std::memory_order_release);
        std::fprintf(stderr, "[RawrXD][GPU] HIP locked: %d device(s)\n", hp);
        return;
    }

    fatal_no_gpu();
}

} // namespace

void require() {
    std::call_once(g_once, detect_locked);
    if (!g_active.load(std::memory_order_acquire)) {
        fatal_no_gpu();
    }
}

const Status& status() {
    require();
    return g_status;
}

bool active() {
    return g_active.load(std::memory_order_acquire);
}

int sanitize_n_gpu_layers(int requested) {
    require();
    if (requested != forced_n_gpu_layers()) {
        std::fprintf(stderr,
            "[RawrXD][GPU] n_gpu_layers override (%d) ignored — locked at %d.\n",
            requested, forced_n_gpu_layers());
    }
    return forced_n_gpu_layers();
}

bool sanitize_use_gpu(bool requested) {
    require();
    if (!requested) {
        std::fprintf(stderr,
            "[RawrXD][GPU] use_gpu=false override ignored — GPU inference is mandatory.\n");
    }
    return true;
}

} // namespace rxd::gpu

// =============================================================================
// C ABI — for legacy / MASM / extern callers.
// =============================================================================
extern "C" {

void rxd_gpu_require(void)                  { rxd::gpu::require(); }
int  rxd_gpu_active(void)                   { return rxd::gpu::active() ? 1 : 0; }
int  rxd_gpu_force_n_gpu_layers(int req)    { return rxd::gpu::sanitize_n_gpu_layers(req); }
int  rxd_gpu_force_use_gpu(int req)         { return rxd::gpu::sanitize_use_gpu(req != 0) ? 1 : 0; }
const char* rxd_gpu_device_name(void)       { return rxd::gpu::status().device_name; }
int  rxd_gpu_backend(void) {
    return static_cast<int>(rxd::gpu::status().active);
}

} // extern "C"
