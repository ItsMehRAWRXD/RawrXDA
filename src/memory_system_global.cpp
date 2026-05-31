// memory_system_global.cpp — Production memory system initialization

#include "memory_system_global.h"
#include <windows.h>
#include <cstdio>

MemoryCore g_memory_system;

extern "C" void memory_system_init(size_t tier_size) {
    // Initialize the global memory system with the specified tier size
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    
    size_t actualSize = tier_size;
    if (actualSize == 0) {
        // Default: use 75% of physical memory or 8GB, whichever is smaller
        MEMORYSTATUSEX memStatus = {};
        memStatus.dwLength = sizeof(memStatus);
        if (GlobalMemoryStatusEx(&memStatus)) {
            actualSize = static_cast<size_t>(memStatus.ullTotalPhys * 3 / 4);
            if (actualSize > 8ULL * 1024 * 1024 * 1024) {
                actualSize = 8ULL * 1024 * 1024 * 1024;
            }
        } else {
            actualSize = 2ULL * 1024 * 1024 * 1024; // 2GB fallback
        }
    }
    
    // Align to page size
    actualSize = (actualSize + sysInfo.dwPageSize - 1) & ~(static_cast<size_t>(sysInfo.dwPageSize) - 1);
    
    g_memory_system.initialize(actualSize);
    
    char buf[256];
    snprintf(buf, sizeof(buf), "[MemorySystem] Initialized with tier size: %zu MB\n", actualSize / (1024 * 1024));
    OutputDebugStringA(buf);
}
