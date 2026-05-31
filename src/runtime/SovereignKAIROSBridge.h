#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <cstdint>

/**
 * @file SovereignKAIROSBridge.h
 * @brief Unified C++ ↔ MASM Background Agent (KAIROS) Bootstrapping.
 */

namespace RawrXD::Runtime {

struct KairosStats {
    uint64_t filesAnalyzed;
    uint64_t suggestionsMade;
    uint32_t buddyMood;
};

struct KairosConfig {
    uint8_t enableAutoSuggest;
    uint8_t enableAutoFix;
    uint32_t maxConcurrentTasks;
};

struct KairosContext {
    uint32_t currentState;
    uint32_t operationMode;
    HANDLE hThread;
    HANDLE hStopEvent;
    HANDLE hWakeEvent;
    HANDLE hCompletionPort;
    HANDLE hTaskMutex;
    uint32_t taskCount;
    KairosStats stats;
    KairosConfig config;
};

extern "C" {
    // MASM Exports from RawrXD_KAIROS.asm
    BOOL KAIROS_Initialize(HWND hWnd, void* memoryContext, KairosContext* context);
    BOOL KAIROS_AddDirectoryWatch(KairosContext* context, const wchar_t* directoryPath, uint32_t watchFlags);
}

class SovereignKAIROSBridge {
public:
    static SovereignKAIROSBridge& instance();

    bool initialize();
    bool shutdown();

    // High-level C++ wrappers
    bool startWatching(const std::wstring& projectDir);
    bool checkHealth(KairosStats& stats);

    KairosContext* getContext() { return &m_context; }

private:
    SovereignKAIROSBridge() = default;
    KairosContext m_context;
    bool m_initialized = false;
};

} // namespace RawrXD::Runtime
