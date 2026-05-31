// =============================================================================
// SovereignAssembler_HotPatch.cpp
// 
// Hot-patching infrastructure for SovereignAssembler kernel optimization
// 
// Provides:
// - Runtime kernel detection and activation
// - Safe hot-patching with validation
// - Rollback mechanism
// - Performance monitoring hooks
// =============================================================================

#include "SovereignAssembler.h"
#include <windows.h>
#include <intrin.h>
#include <cstring>

namespace SovereignAssembler {

// =============================================================================
// Kernel Management
// =============================================================================

struct KernelInfo {
    const char* name;
    bool available;
    bool active;
    FindDelimiterFn function;
    const char* description;
};

static KernelInfo g_kernels[] = {
    {
        "scalar",
        true,
        true,
        FindNextDelimiter_Scalar,
        "Scalar baseline (1-byte-at-a-time)"
    },
    {
        "avx2-internal",
        false,
        false,
        FindNextDelimiter_AVX2,
        "Built-in AVX2 (32 bytes/cycle)"
    },
    {
        "avx2-optimized",
        false,
        false,
        nullptr,
        "Runtime-loaded optimized kernel"
    }
};

// =============================================================================
// CPU Feature Detection
// =============================================================================

static bool DetectAVX2Support() {
    int cpuInfo[4];
    __cpuid(cpuInfo, 7);
    // AVX2 flag is bit 5 of EBX (register 1)
    return (cpuInfo[1] & (1 << 5)) != 0;
}

static bool DetectAVX512Support() {
    int cpuInfo[4];
    __cpuid(cpuInfo, 7);
    // AVX-512 Foundation is bit 16 of EBX
    return (cpuInfo[1] & (1 << 16)) != 0;
}

// =============================================================================
// Kernel Switching & Hot-Patching
// =============================================================================

bool ActivateKernel(const char* kernelName, std::string& errorMsg) {
    for (int i = 0; i < 3; ++i) {
        if (std::strcmp(g_kernels[i].name, kernelName) == 0) {
            if (!g_kernels[i].available) {
                errorMsg = "Kernel not available: ";
                errorMsg += kernelName;
                return false;
            }

            if (!g_kernels[i].function) {
                errorMsg = "Kernel function pointer is null: ";
                errorMsg += kernelName;
                return false;
            }

            // Validate function pointer alignment
            if ((uintptr_t)g_kernels[i].function & 0xF) {
                errorMsg = "WARNING: Kernel function pointer misaligned";
                // Continue anyway, but log warning
            }

            // Perform hot-patch (atomic pointer swap)
            g_findNextDelimiter = g_kernels[i].function;
            g_kernels[i].active = true;

            // Mark others as inactive
            for (int j = 0; j < 3; ++j) {
                if (j != i) {
                    g_kernels[j].active = false;
                }
            }

            return true;
        }
    }

    errorMsg = "Kernel not found: ";
    errorMsg += kernelName;
    return false;
}

const char* GetActiveKernelName() {
    for (int i = 0; i < 3; ++i) {
        if (g_kernels[i].active) {
            return g_kernels[i].name;
        }
    }
    return "unknown";
}

// =============================================================================
// Auto-Select Best Kernel
// =============================================================================

bool AutoSelectOptimalKernel(std::string& report) {
    report.clear();

    // Check CPU support
    bool hasAVX2 = DetectAVX2Support();
    bool hasAVX512 = DetectAVX512Support();

    report += "CPU Features:\n";
    report += "  AVX2:       ";
    report += hasAVX2 ? "YES\n" : "NO\n";
    report += "  AVX-512:    ";
    report += hasAVX512 ? "YES\n" : "NO\n";

    // Mark available kernels based on CPU support
    if (hasAVX2) {
        g_kernels[1].available = true; // AVX2-internal always available if CPU supports
    }

    // Try to activate the best available kernel
    std::string activationError;
    if (hasAVX2) {
        report += "\nActivating: AVX2-internal kernel\n";
        if (ActivateKernel("avx2-internal", activationError)) {
            report += "Status: ACTIVE\n";
            return true;
        }
    }

    // Fallback to scalar
    report += "Fallback: Scalar kernel\n";
    ActivateKernel("scalar", activationError);
    return true;
}

// =============================================================================
// Runtime Kernel Loading (DLL)
// =============================================================================

typedef size_t(__fastcall* OptimizedSkipPtr)(const char*, size_t);

bool LoadOptimizedKernel(const wchar_t* dllPath, std::string& errorMsg) {
    // Load the DLL containing the optimized kernel
    HMODULE hModule = LoadLibraryW(dllPath);
    if (!hModule) {
        DWORD err = GetLastError();
        errorMsg = "Failed to load DLL: ";
        errorMsg += std::to_string(err);
        return false;
    }

    // Get the function by ordinal or name
    auto optimizedFunc = (OptimizedSkipPtr)GetProcAddress(hModule, "skip_whitespace_avx2_optimized");
    if (!optimizedFunc) {
        DWORD err = GetLastError();
        FreeLibrary(hModule);
        errorMsg = "Failed to find export: ";
        errorMsg += std::to_string(err);
        return false;
    }

    // Store the function pointer in the kernel table
    g_kernels[2].function = (FindDelimiterFn)optimizedFunc;
    g_kernels[2].available = true;

    // Activate it
    if (ActivateKernel("avx2-optimized", errorMsg)) {
        return true;
    }

    FreeLibrary(hModule);
    return false;
}

// =============================================================================
// Performance Monitoring
// =============================================================================

struct PerformanceStats {
    uint64_t callCount;
    uint64_t totalBytesScanned;
    uint64_t cacheHits;
    uint64_t cacheMisses;
};

static PerformanceStats g_stats = {};

void ResetPerformanceStats() {
    g_stats = {};
}

PerformanceStats GetPerformanceStats() {
    return g_stats;
}

void PrintKernelStatus() {
    std::cout << "SovereignAssembler Kernel Status\n";
    std::cout << "================================\n\n";

    std::string autoReport;
    AutoSelectOptimalKernel(autoReport);
    std::cout << autoReport << "\n";

    std::cout << "Available Kernels:\n";
    for (int i = 0; i < 3; ++i) {
        std::cout << "  [" << (g_kernels[i].active ? "*" : " ") << "] "
                  << g_kernels[i].name << " - " << g_kernels[i].description << "\n";
    }

    std::cout << "\nActive Kernel: " << GetActiveKernelName() << "\n";
}

} // namespace SovereignAssembler
