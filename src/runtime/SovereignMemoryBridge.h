#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <cstdint>

/**
 * @file SovereignMemoryBridge.h
 * @brief Unified C++ ↔ MASM Memory Binding for the Claude-Parity MemorySystem.
 * 
 * This header defines the interface for the Sovereign Memory System, allowing
 * the C++ runtime to map and interact with the three-layer persistent memory.
 */

namespace RawrXD::Runtime {

// Forward-declared memory structures matching MASM definitions
struct MemoryIndexEntry {
    uint32_t entryType;
    uint32_t priority;
    uint32_t topicHash;
    uint64_t lastAccessed;
    uint32_t accessCount;
    char title[128];
    char summary[512];
};

struct MemorySystemContext {
    MemoryIndexEntry* indexEntries;
    uint32_t indexCount;
    uint32_t indexCapacity;
    uint8_t indexModified;
    void* topicCache;
    uint32_t topicCacheSize;
    void* topicHashTable;
    HANDLE hMutex;
    HANDLE hIndexEvent;
    char basePath[260];
    char indexPath[260];
    uint32_t autoSaveInterval;
};

extern "C" {
    // MASM Exports from RawrXD_MemorySystem.asm
    BOOL MemorySystem_Initialize(HWND hWnd, const char* projectPath, MemorySystemContext* context);
    void* MemorySystem_AddEntry(MemorySystemContext* context, uint32_t type, uint32_t priority, const char* title, const char* summary);
    void* MemorySystem_LoadTopic(MemorySystemContext* context, uint32_t topicHash);
    uint64_t MemorySystem_Search(MemorySystemContext* context, const char* query, void* resultsArray, uint32_t maxResults);
}

class SovereignMemoryBridge {
public:
    static SovereignMemoryBridge& instance();

    bool initialize(const std::string& projectPath);
    bool shutdown();

    // High-level C++ wrappers
    bool recordDecision(const std::string& title, const std::string& summary, int priority = 5);
    bool recordPattern(const std::string& title, const std::string& pattern, int priority = 3);
    
    MemorySystemContext* getContext() { return &m_context; }

private:
    SovereignMemoryBridge() = default;
    MemorySystemContext m_context;
    bool m_initialized = false;
};

} // namespace RawrXD::Runtime
