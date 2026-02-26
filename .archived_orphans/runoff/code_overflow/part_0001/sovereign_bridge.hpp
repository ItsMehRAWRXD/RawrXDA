#ifndef SOVEREIGN_BRIDGE_HPP
#define SOVEREIGN_BRIDGE_HPP

#ifdef _WIN32
#include <windows.h>
#else
#define HANDLE void*
#endif

#include <vector>
/**
 * @class SovereignBridge
 * @brief Bridge to the high-performance Sovereign Kernel stack (800B Loader controls)
 * 
 * Allows legacy IDE components to read real-time thermal stats, sparse skipping rates,
 * and tier status from the kernel's memory mapped file (MMF).
 */
class SovereignBridge {
public:
    struct SovereignStats {
        double temps[5];        // NVMe array temperatures
        unsigned int tier;      // 0=70B, 1=120B, 2=800B
        unsigned int sparsePct; // TurboSparse skip percentage (efficiency)
        unsigned int gpuSplit;  // GPU workload %
        bool isThrottled;       // True if thermal throttling active
    };

    static SovereignStats getStats() {
        SovereignStats stats = { {0.0}, 0, 0, 0, false };
        
#ifdef _WIN32
        // Try to open the global MMF created by the kernel/Oracle service
        HANDLE hMMF = OpenFileMappingA(FILE_MAP_READ, FALSE, "Global\\SOVEREIGN_NVME_TEMPS");
        if (hMMF) {
            void* pView = MapViewOfFile(hMMF, FILE_MAP_READ, 0, 0, 256);
            if (pView) {
                unsigned int* data = static_cast<unsigned int*>(pView);
                
                // Verify signature "SOVE" (0x534F5645)
                if (data[0] == 0x534F5645) {
                    // Offset 0x10 (4 ints) starts the temperature array (usually ints in MMF, scaled or raw)
                    // ThermalDashboardWidget logic: temps at data+4 (offset 16 bytes)
                    // It casts them to int then to double.
                    
                    int* rawTemps = reinterpret_cast<int*>(data + 4);
                    double maxTemp = 0;
                    
                    for (int i = 0; i < 5; ++i) {
                        stats.temps[i] = static_cast<double>(rawTemps[i]);
                        if (stats.temps[i] > maxTemp) maxTemp = stats.temps[i];
                    }

                    // Heuristic for other fields if available in MMF (checking widget logic fallback)
                    // The widget falls back to pure temps if DLL fails.
                    // But if the kernel writes struct, it might be further down.
                    // For now, we assume simple thermal throttle check.
                    
                    if (maxTemp > 65.0) stats.isThrottled = true;
                    
                    // Future expansion: Read tier/sparse from other MMF offsets if documented
                }
                UnmapViewOfFile(pView);
            }
            CloseHandle(hMMF);
        }
#endif
        return stats;
    }

    /**
     * @brief Check if the system is running hot and background tasks should yield
     */
    static bool shouldYield() {
        SovereignStats stats = getStats();
        // Aggressive yielding if over 60 degrees to allow inference priority
        return stats.isThrottled || (stats.temps[0] > 60.0); 
    }
};

#endif // SOVEREIGN_BRIDGE_HPP

