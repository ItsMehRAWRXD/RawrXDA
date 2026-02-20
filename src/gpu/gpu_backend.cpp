#include "gpu_backend.h"
#include <cstdio>
#include <cstdint>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

// Static member initialization
GpuBackend::Backend GpuBackend::s_currentBackend = GpuBackend::CPU;
bool GpuBackend::s_initialized = false;

bool GpuBackend::initialize(Backend backend)
{
    if (s_initialized && s_currentBackend == backend) {
        fprintf(stderr, "[GpuBackend] Already initialized: %s\n", backendName(backend));
        return true;
    }

    switch (backend) {
    case Vulkan:
        if (initializeVulkan()) {
            s_currentBackend = Vulkan;
            s_initialized = true;
            return true;
        }
        fprintf(stderr, "[GpuBackend] WARNING: Failed to initialize Vulkan, falling back to CPU\n");
        return initializeCpu();

    case CUDA:
        if (initializeCuda()) {
            s_currentBackend = CUDA;
            s_initialized = true;
            return true;
        }
        fprintf(stderr, "[GpuBackend] WARNING: Failed to initialize CUDA, falling back to CPU\n");
        return initializeCpu();

    case CPU:
        return initializeCpu();
    }

    return false;
}

GpuBackend::Backend GpuBackend::currentBackend()
{
    return s_currentBackend;
}

bool GpuBackend::isBackendAvailable(Backend backend)
{
    switch (backend) {
    case Vulkan: {
        fprintf(stderr, "[GpuBackend] Probing Vulkan runtime...\n");
#ifdef _WIN32
        HMODULE hLib = LoadLibraryA("vulkan-1.dll");
        if (hLib) {
            // Verify we can resolve vkCreateInstance
            auto pfn = GetProcAddress(hLib, "vkCreateInstance");
            FreeLibrary(hLib);
            bool avail = (pfn != nullptr);
            fprintf(stderr, "[GpuBackend] Vulkan %s\n", avail ? "AVAILABLE" : "NOT AVAILABLE (missing vkCreateInstance)");
            return avail;
        }
        fprintf(stderr, "[GpuBackend] Vulkan NOT AVAILABLE (vulkan-1.dll not found)\n");
        return false;
#else
        void* lib = dlopen("libvulkan.so.1", RTLD_LAZY);
        if (lib) {
            void* pfn = dlsym(lib, "vkCreateInstance");
            dlclose(lib);
            bool avail = (pfn != nullptr);
            fprintf(stderr, "[GpuBackend] Vulkan %s\n", avail ? "AVAILABLE" : "NOT AVAILABLE");
            return avail;
        }
        fprintf(stderr, "[GpuBackend] Vulkan NOT AVAILABLE (libvulkan.so.1 not found)\n");
        return false;
#endif
    }
    case CUDA: {
        fprintf(stderr, "[GpuBackend] Probing CUDA runtime...\n");
#ifdef _WIN32
        HMODULE hLib = LoadLibraryA("nvcuda.dll");
        if (hLib) {
            auto pfn = GetProcAddress(hLib, "cuInit");
            FreeLibrary(hLib);
            bool avail = (pfn != nullptr);
            fprintf(stderr, "[GpuBackend] CUDA %s\n", avail ? "AVAILABLE" : "NOT AVAILABLE (missing cuInit)");
            return avail;
        }
        fprintf(stderr, "[GpuBackend] CUDA NOT AVAILABLE (nvcuda.dll not found)\n");
        return false;
#else
        void* lib = dlopen("libcuda.so", RTLD_LAZY);
        if (lib) {
            void* pfn = dlsym(lib, "cuInit");
            dlclose(lib);
            bool avail = (pfn != nullptr);
            fprintf(stderr, "[GpuBackend] CUDA %s\n", avail ? "AVAILABLE" : "NOT AVAILABLE");
            return avail;
        }
        fprintf(stderr, "[GpuBackend] CUDA NOT AVAILABLE (libcuda.so not found)\n");
        return false;
#endif
    }
    case CPU:
        return true;  // CPU always available
    }
    return false;
}

const char* GpuBackend::backendName(Backend backend)
{
    switch (backend) {
    case CPU:
        return "CPU";
    case Vulkan:
        return "Vulkan";
    case CUDA:
        return "CUDA";
    }
    return "Unknown";
}

bool GpuBackend::initializeWithFallback()
{
    // Try Vulkan first
    if (isBackendAvailable(Vulkan) && initializeVulkan()) {
        s_currentBackend = Vulkan;
        s_initialized = true;
        fprintf(stderr, "[GpuBackend] OK: GPU Backend initialized with Vulkan\n");
        return true;
    }

    // Fall back to CUDA
    if (isBackendAvailable(CUDA) && initializeCuda()) {
        s_currentBackend = CUDA;
        s_initialized = true;
        fprintf(stderr, "[GpuBackend] OK: GPU Backend initialized with CUDA\n");
        return true;
    }

    // Final fallback to CPU
    fprintf(stderr, "[GpuBackend] WARNING: No GPU backends available, falling back to CPU (slower performance)\n");
    return initializeCpu();
}

bool GpuBackend::initializeVulkan()
{
    fprintf(stderr, "[GpuBackend] Attempting to initialize Vulkan...\n");

#ifdef _WIN32
    HMODULE hVulkan = LoadLibraryA("vulkan-1.dll");
    if (!hVulkan) {
        fprintf(stderr, "[GpuBackend] FAILED: vulkan-1.dll not found (error %lu)\n", GetLastError());
        return false;
    }

    // Resolve vkCreateInstance
    typedef int (*PFN_vkCreateInstance)(const void*, const void*, void**);
    auto pfnCreateInstance = (PFN_vkCreateInstance)GetProcAddress(hVulkan, "vkCreateInstance");
    if (!pfnCreateInstance) {
        fprintf(stderr, "[GpuBackend] FAILED: vkCreateInstance not found in vulkan-1.dll\n");
        FreeLibrary(hVulkan);
        return false;
    }

    // Attempt minimal Vulkan instance creation to validate driver
    struct VkApplicationInfo {
        int sType; const void* pNext; const char* pAppName; uint32_t appVer;
        const char* pEngineName; uint32_t engineVer; uint32_t apiVer;
    };
    struct VkInstanceCreateInfo {
        int sType; const void* pNext; uint32_t flags;
        const VkApplicationInfo* pAppInfo;
        uint32_t layerCount; const char* const* ppLayers;
        uint32_t extCount; const char* const* ppExts;
    };

    VkApplicationInfo appInfo = {};
    appInfo.sType = 0; // VK_STRUCTURE_TYPE_APPLICATION_INFO
    appInfo.pAppName = "RawrXD";
    appInfo.apiVer = (1 << 22) | (0 << 12); // VK 1.0

    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = 1; // VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO
    createInfo.pAppInfo = &appInfo;

    void* instance = nullptr;
    int vkResult = pfnCreateInstance(&createInfo, nullptr, &instance);

    if (vkResult != 0 || !instance) {
        fprintf(stderr, "[GpuBackend] FAILED: vkCreateInstance returned %d\n", vkResult);
        FreeLibrary(hVulkan);
        return false;
    }

    // Clean up the test instance
    typedef void (*PFN_vkDestroyInstance)(void*, const void*);
    auto pfnDestroy = (PFN_vkDestroyInstance)GetProcAddress(hVulkan, "vkDestroyInstance");
    if (pfnDestroy && instance) {
        pfnDestroy(instance, nullptr);
    }

    // Keep vulkan-1.dll loaded for future use
    fprintf(stderr, "[GpuBackend] OK: Vulkan initialized successfully (driver validated)\n");
    return true;
#else
    // POSIX: dlopen libvulkan
    void* lib = dlopen("libvulkan.so.1", RTLD_NOW);
    if (!lib) {
        fprintf(stderr, "[GpuBackend] FAILED: libvulkan.so.1 not found\n");
        return false;
    }
    fprintf(stderr, "[GpuBackend] OK: Vulkan library loaded\n");
    return true;
#endif
}

bool GpuBackend::initializeCuda()
{
    fprintf(stderr, "[GpuBackend] Attempting to initialize CUDA...\n");

#ifdef _WIN32
    HMODULE hCuda = LoadLibraryA("nvcuda.dll");
    if (!hCuda) {
        fprintf(stderr, "[GpuBackend] WARNING: CUDA not available (nvcuda.dll not found, error %lu)\n", GetLastError());
        return false;
    }

    typedef int (*PFN_cuInit)(unsigned int);
    auto pfnCuInit = (PFN_cuInit)GetProcAddress(hCuda, "cuInit");
    if (!pfnCuInit) {
        fprintf(stderr, "[GpuBackend] WARNING: cuInit not found in nvcuda.dll\n");
        FreeLibrary(hCuda);
        return false;
    }

    int cuResult = pfnCuInit(0);
    if (cuResult != 0) {
        fprintf(stderr, "[GpuBackend] WARNING: cuInit failed with error %d\n", cuResult);
        FreeLibrary(hCuda);
        return false;
    }

    fprintf(stderr, "[GpuBackend] OK: CUDA initialized successfully\n");
    return true;
#else
    void* lib = dlopen("libcuda.so", RTLD_NOW);
    if (!lib) {
        fprintf(stderr, "[GpuBackend] WARNING: CUDA not available (libcuda.so not found)\n");
        return false;
    }
    fprintf(stderr, "[GpuBackend] OK: CUDA library loaded\n");
    return true;
#endif
}

bool GpuBackend::initializeCpu()
{
    fprintf(stderr, "[GpuBackend] Initializing CPU backend...\n");
    s_currentBackend = CPU;
    s_initialized = true;
    fprintf(stderr, "[GpuBackend] OK: CPU backend ready (models will run on CPU - slower performance)\n");
    return true;
}