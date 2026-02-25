// RawrXD Memory Manager - Pure Win32/C++ Implementation
// Agent state and memory management

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <vector>
#include <map>

// ============================================================================
// MEMORY STRUCTURES
// ============================================================================

struct MemoryBlock {
    int block_id;
    void* address;
    SIZE_T size;
    int owner_id;  // agent id
    DWORD allocated_at;
    DWORD accessed_at;
    wchar_t tag[128];
};

struct MemoryStats {
    SIZE_T total_allocated;
    SIZE_T total_freed;
    int block_count;
    int access_count;
};

// ============================================================================
// MEMORY MANAGER
// ============================================================================

class MemoryManager {
private:
    CRITICAL_SECTION m_cs;
    std::vector<MemoryBlock> m_blocks;
    std::map<int, int> m_ownerBlockCount;
    int m_nextBlockId;
    SIZE_T m_totalAllocated;
    SIZE_T m_totalFreed;
    int m_accessCount;
    int m_maxBlocks;
    
public:
    MemoryManager(int maxBlocks = 10000)
        : m_nextBlockId(1),
          m_totalAllocated(0),
          m_totalFreed(0),
          m_accessCount(0),
          m_maxBlocks(maxBlocks) {
        InitializeCriticalSection(&m_cs);
    }
    
    ~MemoryManager() {
        Shutdown();
        DeleteCriticalSection(&m_cs);
    }
    
    void Initialize() {
        EnterCriticalSection(&m_cs);
        m_blocks.clear();
        m_ownerBlockCount.clear();
        m_nextBlockId = 1;
        m_totalAllocated = 0;
        m_totalFreed = 0;
        LeaveCriticalSection(&m_cs);
        OutputDebugStringW(L"[MemoryManager] Initialized\n");
    }
    
    void Shutdown() {
        EnterCriticalSection(&m_cs);
        
        // Free all blocks
        for (auto& block : m_blocks) {
            if (block.address) {
                free(block.address);
                m_totalFreed += block.size;
            }
        }
        
        m_blocks.clear();
        m_ownerBlockCount.clear();
        
        LeaveCriticalSection(&m_cs);
    }
    
    // Allocate memory and track it
    void* AllocateMemory(int ownerId, SIZE_T size, const wchar_t* tag) {
        if (size == 0 || size > 1024 * 1024 * 100) return NULL;  // Max 100 MB per block
        
        void* ptr = malloc(size);
        if (!ptr) return NULL;
        
        EnterCriticalSection(&m_cs);
        
        if ((int)m_blocks.size() >= m_maxBlocks) {
            LeaveCriticalSection(&m_cs);
            free(ptr);
            return NULL;
        }
        
        MemoryBlock block;
        block.block_id = m_nextBlockId++;
        block.address = ptr;
        block.size = size;
        block.owner_id = ownerId;
        block.allocated_at = GetTickCount();
        block.accessed_at = block.allocated_at;
        wcscpy_s(block.tag, 128, tag ? tag : L"");
        
        m_blocks.push_back(block);
        m_totalAllocated += size;
        m_ownerBlockCount[ownerId]++;
        
        LeaveCriticalSection(&m_cs);
        
        return ptr;
    }
    
    // Free tracked memory
    BOOL FreeMemory(void* ptr) {
        if (!ptr) return FALSE;
        
        EnterCriticalSection(&m_cs);
        
        BOOL found = FALSE;
        for (int i = 0; i < (int)m_blocks.size(); i++) {
            if (m_blocks[i].address == ptr) {
                m_totalFreed += m_blocks[i].size;
                int ownerId = m_blocks[i].owner_id;
                m_ownerBlockCount[ownerId]--;
                
                free(ptr);
                
                m_blocks.erase(m_blocks.begin() + i);
                found = TRUE;
                break;
            }
        }
        
        LeaveCriticalSection(&m_cs);
        return found;
    }
    
    // Access memory and record usage
    BOOL AccessMemory(void* ptr) {
        if (!ptr) return FALSE;
        
        EnterCriticalSection(&m_cs);
        
        BOOL found = FALSE;
        for (auto& block : m_blocks) {
            if (block.address == ptr) {
                block.accessed_at = GetTickCount();
                m_accessCount++;
                found = TRUE;
                break;
            }
        }
        
        LeaveCriticalSection(&m_cs);
        return found;
    }
    
    // Get memory stats
    MemoryStats GetStats() {
        MemoryStats stats;
        stats.total_allocated = 0;
        stats.total_freed = 0;
        stats.block_count = 0;
        stats.access_count = 0;
        
        EnterCriticalSection(&m_cs);
        stats.total_allocated = m_totalAllocated;
        stats.total_freed = m_totalFreed;
        stats.block_count = (int)m_blocks.size();
        stats.access_count = m_accessCount;
        LeaveCriticalSection(&m_cs);
        
        return stats;
    }
    
    // Clean up old/unused blocks for owner
    int CleanupOwnerMemory(int ownerId, DWORD ageMs) {
        EnterCriticalSection(&m_cs);
        
        DWORD now = GetTickCount();
        int freed = 0;
        
        for (int i = (int)m_blocks.size() - 1; i >= 0; i--) {
            if (m_blocks[i].owner_id == ownerId) {
                if ((now - m_blocks[i].accessed_at) > ageMs) {
                    m_totalFreed += m_blocks[i].size;
                    free(m_blocks[i].address);
                    m_blocks.erase(m_blocks.begin() + i);
                    freed++;
                }
            }
        }
        
        LeaveCriticalSection(&m_cs);
        return freed;
    }
    
    const wchar_t* GetStatus() {
        static wchar_t status[512];
        MemoryStats stats = GetStats();
        
        swprintf_s(status, 512,
            L"MemoryManager: Allocated=%lld, Freed=%lld, Active=%d blocks",
            (long long)stats.total_allocated, (long long)stats.total_freed, stats.block_count);
        
        return status;
    }
};

// ============================================================================
// C INTERFACE
// ============================================================================

extern "C" {

__declspec(dllexport) MemoryManager* __stdcall CreateMemoryManager(int maxBlocks) {
    return new MemoryManager(maxBlocks);
}

__declspec(dllexport) void __stdcall DestroyMemoryManager(MemoryManager* mgr) {
    if (mgr) delete mgr;
}

__declspec(dllexport) void __stdcall MemoryManager_Initialize(MemoryManager* mgr) {
    if (mgr) mgr->Initialize();
}

__declspec(dllexport) void __stdcall MemoryManager_Shutdown(MemoryManager* mgr) {
    if (mgr) mgr->Shutdown();
}

__declspec(dllexport) void* __stdcall MemoryManager_Allocate(MemoryManager* mgr,
    int ownerId, SIZE_T size, const wchar_t* tag) {
    return mgr ? mgr->AllocateMemory(ownerId, size, tag) : NULL;
}

__declspec(dllexport) BOOL __stdcall MemoryManager_Free(MemoryManager* mgr, void* ptr) {
    return mgr ? mgr->FreeMemory(ptr) : FALSE;
}

__declspec(dllexport) BOOL __stdcall MemoryManager_Access(MemoryManager* mgr, void* ptr) {
    return mgr ? mgr->AccessMemory(ptr) : FALSE;
}

__declspec(dllexport) int __stdcall MemoryManager_Cleanup(MemoryManager* mgr,
    int ownerId, DWORD ageMs) {
    return mgr ? mgr->CleanupOwnerMemory(ownerId, ageMs) : 0;
}

__declspec(dllexport) const wchar_t* __stdcall MemoryManager_GetStatus(MemoryManager* mgr) {
    return mgr ? mgr->GetStatus() : L"Not initialized";
}

} // extern "C"

// ============================================================================
// DLL ENTRY
// ============================================================================

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    (void)hinstDLL;
    (void)lpvReserved;
    
    switch (fdwReason) {
    case DLL_PROCESS_ATTACH:
        OutputDebugStringW(L"[RawrXD_MemoryManager] DLL loaded\n");
        break;
    case DLL_PROCESS_DETACH:
        OutputDebugStringW(L"[RawrXD_MemoryManager] DLL unloaded\n");
        break;
    }
    return TRUE;
}
