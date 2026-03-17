#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <functional>
#include "rawrxd_ipc_protocol.h"

namespace rawrxd::debug {

class DebuggerCore {
public:
    static DebuggerCore& getInstance() {
        static DebuggerCore instance;
        return instance;
    }

    // Phase 3.1: Launch a process for debugging
    bool launchProcess(const std::wstring& exePath, const std::wstring& cmdLine = L"");

    // Phase 3.2: The primary debug event loop (runs in a dedicated thread)
    void debugLoop();

    // Event handlers
    void onProcessCreated(const CREATE_PROCESS_DEBUG_INFO& info);
    void onThreadCreated(const CREATE_THREAD_DEBUG_INFO& info);
    void onModuleLoaded(const LOAD_DLL_DEBUG_INFO& info);
    void onException(const EXCEPTION_DEBUG_INFO& info, DWORD threadId);

    // Phase 3.2: Breakpoint management
    bool setBreakpoint(uint64_t address);
    bool removeBreakpoint(uint64_t address);

    // Phase 3.3+: Memory Exploration, Patching & Symbol Resolution
    struct MemoryChunk {
        uint64_t address;
        std::vector<uint8_t> data;
    };
    bool patchMemory(uint64_t address, const std::vector<uint8_t>& data);
    uint64_t resolveExport(uint64_t moduleBase, const std::string& name);
    std::string resolveAddressToName(uint64_t address);
    uint64_t resolveNameToAddress(const std::string& name);

    struct Registers {
        uint64_t rax, rbx, rcx, rdx, rsi, rdi, rbp, rsp;
        uint64_t r8, r9, r10, r11, r12, r13, r14, r15;
        uint64_t rip;
        uint64_t rflags;
    };
    bool getThreadRegisters(DWORD threadId, Registers& regs);
    std::vector<uint8_t> readMemory(uint64_t address, size_t size);

    // Phase 3.4: Watch List Management
    struct WatchEntry {
        uint64_t address;
        size_t size;
        std::string label;
        std::vector<uint8_t> lastValue;
    };
    void addWatch(uint64_t address, size_t size, const std::string& label);
    void removeWatch(uint64_t address);
    std::vector<WatchEntry> getWatchList() const { return m_watches; }
    void refreshWatches();

    struct WatchSymbol {
        uint64_t moduleBase;
        std::string name;
        uint64_t resolvedAddress;
        std::vector<uint8_t> lastValue;
        size_t sizeToRead;
    };
    void addWatch(uint64_t moduleBase, const std::string& name, size_t size = 8);
    void removeWatch(const std::string& name);
    std::vector<WatchSymbol> getWatchSymbolList() const { return m_watchList; }
    void refreshWatchValues();

    bool assembleAndInject(uint64_t address, const std::string& asmSource);

private:
    DebuggerCore() : m_hProcess(nullptr), m_dwProcessId(0), m_isDebugging(false) {}
    ~DebuggerCore() { stop(); }

    void stop();

    struct Breakpoint {
        uint64_t address;
        uint8_t originalByte;
        bool enabled;
    };

    HANDLE m_hProcess;
    DWORD m_dwProcessId;
    bool m_isDebugging;
    std::wstring m_targetPath;
    std::vector<Breakpoint> m_breakpoints;
    std::vector<WatchSymbol> m_watchList;
    std::vector<WatchEntry> m_watches;
};

} // namespace rawrxd::debug
