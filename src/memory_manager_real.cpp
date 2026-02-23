#include <iostream>
#include <map>
#include <vector>
#include <cstring>
#include <windows.h>
#include <psapi.h>

#include "logging/logger.h"
static Logger s_logger("memory_manager_real");
#pragma comment(lib, "psapi.lib")

// Real Memory Manager Implementation
class MemoryManagerReal {
private:
    struct MemoryBlock {
        void* address;
        size_t size;
        std::string context;
        bool allocated;
    };
    
    std::vector<MemoryBlock> m_blocks;
    size_t m_total_allocated;
    size_t m_context_size_limit;
    CRITICAL_SECTION m_cs;
    
public:
    MemoryManagerReal() 
        : m_total_allocated(0), m_context_size_limit(32 * 1024 * 1024) { // 32MB default
        InitializeCriticalSection(&m_cs);
    }
    
    ~MemoryManagerReal() {
        EnterCriticalSection(&m_cs);
        for (auto& block : m_blocks) {
            if (block.allocated && block.address) {
                VirtualFree(block.address, 0, MEM_RELEASE);
            }
        }
        LeaveCriticalSection(&m_cs);
        DeleteCriticalSection(&m_cs);
    }
    
    void SetContextSize(unsigned long long size) {
        EnterCriticalSection(&m_cs);
        m_context_size_limit = size;
        s_logger.info("[MEMORY] Context size set to ");
        LeaveCriticalSection(&m_cs);
    }
    
    std::vector<unsigned long long> GetAvailableSizes() {
        return {512, 2048, 8192, 32768, 131072, 1048576};
    }
    
    std::string GetStatsString() {
        EnterCriticalSection(&m_cs);
        
        PROCESS_MEMORY_COUNTERS pmc = {};
        if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
            char buffer[512];
            sprintf_s(buffer, sizeof(buffer),
                "Memory Statistics:\n"
                "  Working Set: %.2f MB\n"
                "  Peak Working Set: %.2f MB\n"
                "  Pagefile: %.2f MB\n"
                "  Peak Pagefile: %.2f MB\n"
                "  Allocated Blocks: %zu\n"
                "  Total Internal: %.2f MB\n"
                "  Context Limit: %.2f MB",
                pmc.WorkingSetSize / (1024.0 * 1024.0),
                pmc.PeakWorkingSetSize / (1024.0 * 1024.0),
                pmc.PagefileUsage / (1024.0 * 1024.0),
                pmc.PeakPagefileUsage / (1024.0 * 1024.0),
                m_blocks.size(),
                m_total_allocated / (1024.0 * 1024.0),
                m_context_size_limit / (1024.0 * 1024.0)
            );
            
            std::string result(buffer);
            LeaveCriticalSection(&m_cs);
            return result;
        }
        
        LeaveCriticalSection(&m_cs);
        return "Memory stats unavailable";
    }
    
    void ProcessWithContext(const std::string& context_data) {
        EnterCriticalSection(&m_cs);
        
        // Allocate temporary memory for context processing
        if (context_data.size() < m_context_size_limit) {
            void* context_memory = VirtualAlloc(nullptr, context_data.size(), MEM_COMMIT, PAGE_READWRITE);
            if (context_memory) {
                memcpy(context_memory, context_data.data(), context_data.size());
                
                MemoryBlock block;
                block.address = context_memory;
                block.size = context_data.size();
                block.context = "processing_context";
                block.allocated = true;
                
                m_blocks.push_back(block);
                m_total_allocated += context_data.size();
                
                s_logger.info("[MEMORY] Context processed: ");
                
                // Note: In production, this would be freed after processing
            }
        }
        
        LeaveCriticalSection(&m_cs);
    }
};

static MemoryManagerReal* g_memory_mgr = nullptr;

// Public MemoryModule class
class MemoryModuleReal {
public:
    std::string GetName() const { return "SystemMemory"; }
    size_t GetMaxTokens() const { return 131072; }
};

static MemoryModuleReal g_module;

// Exported C++ API
extern "C" {
    void memory_manager_init() {
        if (!g_memory_mgr) {
            g_memory_mgr = new MemoryManagerReal();
            s_logger.info("[MEMORY MANAGER] Initialized\n");
        }
    }
    
    void memory_manager_cleanup() {
        if (g_memory_mgr) {
            delete g_memory_mgr;
            g_memory_mgr = nullptr;
        }
    }
    
    const char* memory_manager_stats() {
        static std::string stats;
        if (g_memory_mgr) {
            stats = g_memory_mgr->GetStatsString();
        }
        return stats.c_str();
    }
}
