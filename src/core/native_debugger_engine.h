// =============================================================================
// native_debugger_engine.h — Phase 12: Native Debugger Engine (DbgEng COM)
// =============================================================================
// Windows Debug Engine interop via dbgeng.dll COM interfaces:
//   IDebugClient7, IDebugControl7, IDebugSymbols5, IDebugRegisters2,
//   IDebugDataSpaces4, IDebugSystemObjects4, IDebugAdvanced3
//
// Provides:
//   - Process launch / attach / detach
//   - Debug event loop (breakpoints, exceptions, DLL load/unload)
//   - Software & hardware breakpoint management
//   - Register snapshot & modification
//   - Stack walking with symbol resolution
//   - Memory read / write / search
//   - Disassembly (Intel/ATT/MASM syntax)
//   - Expression evaluation
//   - Module & thread enumeration
//   - Watch expressions with auto-update
//
// Architecture: C++20 | Win64 | No exceptions | No Qt | Singleton
// Build:        Linked with dbghelp.lib + dbgeng.lib
// Rule:         NO SOURCE FILE IS TO BE SIMPLIFIED
// =============================================================================

#ifndef RAWRXD_NATIVE_DEBUGGER_ENGINE_H
#define RAWRXD_NATIVE_DEBUGGER_ENGINE_H

#include "native_debugger_types.h"
#include <string>
#include <vector>
#include <map>
#include <deque>
#include <mutex>
#include <atomic>
#include <thread>
#include <condition_variable>

// Forward declarations — avoid pulling in full dbgeng.h/windows.h in header
struct IDebugClient7;
struct IDebugControl7;
struct IDebugSymbols5;
struct IDebugRegisters2;
struct IDebugDataSpaces4;
struct IDebugSystemObjects4;
struct IDebugAdvanced3;

namespace RawrXD {
namespace Debugger {

// =============================================================================
//                    NativeDebuggerEngine (Singleton)
// =============================================================================

class NativeDebuggerEngine {
public:
    // ---- Singleton ----
    static NativeDebuggerEngine& Instance();

    // ---- Lifecycle ----
    DebugResult initialize(const DebugConfig& config);
    DebugResult shutdown();
    bool isInitialized() const { return m_initialized.load(std::memory_order_acquire); }

    // ---- Session Control ----
    DebugResult launchProcess(const std::string& exePath, const std::string& args = "",
                               const std::string& workingDir = "");
    DebugResult attachToProcess(uint32_t pid);
    DebugResult detach();
    DebugResult terminateTarget();
    DebugSessionState getState() const { return m_state.load(std::memory_order_acquire); }
    const std::string& getTargetName() const { return m_targetName; }
    const std::string& getTargetPath() const { return m_targetPath; }
    uint32_t getTargetPID() const { return m_targetPID; }

    // ---- Execution Control ----
    DebugResult go();                                   // Continue execution
    DebugResult stepOver();                             // Step over (source or asm)
    DebugResult stepInto();                             // Step into
    DebugResult stepOut();                              // Step out (run to return)
    DebugResult stepToAddress(uint64_t address);        // Run to address
    DebugResult breakExecution();                       // Async break (DebugBreakProcess)
    DebugResult runToLine(const std::string& file, int line);

    // ---- Breakpoint Management ----
    DebugResult addBreakpoint(uint64_t address, BreakpointType type = BreakpointType::Software);
    DebugResult addBreakpointBySymbol(const std::string& symbol,
                                       BreakpointType type = BreakpointType::Software);
    DebugResult addBreakpointBySourceLine(const std::string& file, int line);
    DebugResult addConditionalBreakpoint(uint64_t address, const std::string& condition);
    DebugResult addDataBreakpoint(uint64_t address, uint32_t sizeBytes, uint32_t condition);
    DebugResult removeBreakpoint(uint32_t bpId);
    DebugResult enableBreakpoint(uint32_t bpId, bool enable);
    DebugResult removeAllBreakpoints();
    const std::vector<NativeBreakpoint>& getBreakpoints() const { return m_breakpoints; }
    const NativeBreakpoint* findBreakpointById(uint32_t id) const;

    // ---- Register Access ----
    DebugResult captureRegisters(RegisterSnapshot& outSnapshot);
    DebugResult setRegister(const std::string& regName, uint64_t value);
    std::string formatRegisters(const RegisterSnapshot& snap) const;
    std::string formatFlags(uint64_t rflags) const;

    // ---- Stack Walking ----
    DebugResult walkStack(std::vector<NativeStackFrame>& outFrames, uint32_t maxFrames = 256);
    DebugResult getFrameLocals(uint32_t frameIndex, std::map<std::string, std::string>& outLocals);
    std::string formatStackTrace(const std::vector<NativeStackFrame>& frames) const;

    // ---- Memory Operations ----
    DebugResult readMemory(uint64_t address, void* buffer, uint64_t size, uint64_t* bytesRead = nullptr);
    DebugResult writeMemory(uint64_t address, const void* buffer, uint64_t size);
    DebugResult readString(uint64_t address, std::string& outStr, uint32_t maxLen = 512);
    DebugResult readPointer(uint64_t address, uint64_t& outValue);
    std::string formatHexDump(uint64_t address, const void* data, uint64_t size, uint32_t columns = 16) const;
    DebugResult queryMemoryRegions(std::vector<MemoryRegion>& outRegions);
    DebugResult searchMemory(uint64_t startAddr, uint64_t size,
                              const void* pattern, uint32_t patternLen,
                              std::vector<uint64_t>& outMatches);

    // ---- Disassembly ----
    DebugResult disassembleAt(uint64_t address, uint32_t lineCount,
                               std::vector<DisassembledInstruction>& outInstructions);
    DebugResult disassembleFunction(const std::string& symbol,
                                     std::vector<DisassembledInstruction>& outInstructions);
    DebugResult setDisasmSyntax(DisasmSyntax syntax);

    // ---- Symbol Resolution ----
    DebugResult resolveSymbol(uint64_t address, std::string& outSymbol, uint64_t& outDisplacement);
    DebugResult resolveAddress(const std::string& symbol, uint64_t& outAddress);
    DebugResult loadSymbols(const std::string& moduleName);
    DebugResult setSymbolPath(const std::string& path);

    // ---- Expression Evaluation ----
    DebugResult evaluate(const std::string& expression, EvalResult& outResult);

    // ---- Watch Management ----
    uint32_t addWatch(const std::string& expression);
    DebugResult removeWatch(uint32_t watchId);
    DebugResult updateWatches();
    const std::vector<NativeWatch>& getWatches() const { return m_watches; }

    // ---- Module Enumeration ----
    DebugResult enumerateModules(std::vector<DebugModule>& outModules);
    const DebugModule* findModuleByAddress(uint64_t address) const;
    const DebugModule* findModuleByName(const std::string& name) const;

    // ---- Thread Enumeration ----
    DebugResult enumerateThreads(std::vector<DebugThread>& outThreads);
    DebugResult switchThread(uint32_t threadId);
    uint32_t getCurrentThreadId() const { return m_currentThreadId; }

    // ---- Event History ----
    const std::deque<DebugEvent>& getEventHistory() const { return m_eventHistory; }
    const DebugEvent* getLastEvent() const;
    void clearEventHistory();

    // ---- Callbacks ----
    void setEventCallback(DebugEventCallback cb, void* userData);
    void setOutputCallback(DebugOutputCallback cb, void* userData);
    void setStateCallback(DebugStateCallback cb, void* userData);
    void setBreakpointHitCallback(BreakpointHitCallback cb, void* userData);

    // ---- Statistics ----
    DebugSessionStats getStats() const;
    void resetStats();

    // ---- Configuration ----
    const DebugConfig& getConfig() const { return m_config; }
    void updateConfig(const DebugConfig& newConfig);

    // ---- JSON Serialization ----
    std::string toJsonStatus() const;
    std::string toJsonBreakpoints() const;
    std::string toJsonRegisters() const;
    std::string toJsonStack() const;
    std::string toJsonModules() const;
    std::string toJsonThreads() const;
    std::string toJsonDisassembly(uint64_t address, uint32_t lines) const;
    std::string toJsonMemory(uint64_t address, uint64_t size) const;
    std::string toJsonEvents(uint32_t lastN = 50) const;
    std::string toJsonWatches() const;

private:
    NativeDebuggerEngine();
    ~NativeDebuggerEngine();
    NativeDebuggerEngine(const NativeDebuggerEngine&) = delete;
    NativeDebuggerEngine& operator=(const NativeDebuggerEngine&) = delete;

    // ---- Internal: DbgEng COM Lifecycle ----
    DebugResult initDbgEng();
    void        releaseDbgEng();

    // ---- Internal: Event Loop ----
    void        eventLoopThread();
    void        processDebugEvent(const DebugEvent& event);
    void        transitionState(DebugSessionState newState);

    // ---- Internal: Breakpoint Helpers ----
    uint32_t    allocateBreakpointId();
    DebugResult applySoftwareBreakpoint(NativeBreakpoint& bp);
    DebugResult applyHardwareBreakpoint(NativeBreakpoint& bp);
    DebugResult removeSoftwareBreakpoint(NativeBreakpoint& bp);
    DebugResult removeHardwareBreakpoint(NativeBreakpoint& bp);
    int         findFreeHWSlot() const;

    // ---- Internal: Symbol Helpers ----
    void        resolveBreakpointSymbol(NativeBreakpoint& bp);
    void        onModuleLoad(const DebugModule& mod);
    void        onModuleUnload(const std::string& name);

    // ---- Internal: Disassembly ----
    DebugResult disassembleRange(uint64_t start, uint64_t end,
                                  std::vector<DisassembledInstruction>& outInstructions);

    // ---- Internal: Formatting ----
    std::string formatAddress(uint64_t addr) const;
    std::string formatExceptionCode(uint32_t code) const;

    // ---- DbgEng COM pointers ----
    IDebugClient7*          m_debugClient       = nullptr;
    IDebugControl7*         m_debugControl      = nullptr;
    IDebugSymbols5*         m_debugSymbols      = nullptr;
    IDebugRegisters2*       m_debugRegisters    = nullptr;
    IDebugDataSpaces4*      m_debugDataSpaces   = nullptr;
    IDebugSystemObjects4*   m_debugSysObjects   = nullptr;
    IDebugAdvanced3*        m_debugAdvanced     = nullptr;

    // ---- Session State ----
    std::atomic<bool>               m_initialized{false};
    std::atomic<DebugSessionState>  m_state{DebugSessionState::Idle};
    std::string                     m_targetName;
    std::string                     m_targetPath;
    uint32_t                        m_targetPID             = 0;
    uint32_t                        m_currentThreadId       = 0;
    uint64_t                        m_processHandle         = 0;

    // ---- Event Loop ----
    std::thread             m_eventThread;
    std::atomic<bool>       m_eventLoopRunning{false};
    std::condition_variable m_eventCV;
    mutable std::mutex      m_eventMutex;

    // ---- Breakpoints ----
    std::vector<NativeBreakpoint>   m_breakpoints;
    std::atomic<uint32_t>           m_nextBpId{1};
    HardwareBreakpointSlot          m_hwSlots[4] = {};
    mutable std::mutex              m_bpMutex;

    // ---- Watch Expressions ----
    std::vector<NativeWatch>    m_watches;
    std::atomic<uint32_t>       m_nextWatchId{1};
    mutable std::mutex          m_watchMutex;

    // ---- Modules & Threads ----
    std::vector<DebugModule>    m_modules;
    std::vector<DebugThread>    m_threads;
    mutable std::mutex          m_moduleMutex;
    mutable std::mutex          m_threadMutex;

    // ---- Event History ----
    std::deque<DebugEvent>      m_eventHistory;
    mutable std::mutex          m_historyMutex;

    // ---- Callbacks ----
    DebugEventCallback          m_eventCallback         = nullptr;
    void*                       m_eventCallbackData     = nullptr;
    DebugOutputCallback         m_outputCallback        = nullptr;
    void*                       m_outputCallbackData    = nullptr;
    DebugStateCallback          m_stateCallback         = nullptr;
    void*                       m_stateCallbackData     = nullptr;
    BreakpointHitCallback       m_bpHitCallback         = nullptr;
    void*                       m_bpHitCallbackData     = nullptr;

    // ---- Statistics ----
    mutable DebugSessionStats   m_stats;
    mutable std::mutex          m_statsMutex;

    // ---- Configuration ----
    DebugConfig                 m_config;
    mutable std::mutex          m_configMutex;

    // ---- Cached disassembly syntax ----
    DisasmSyntax                m_currentSyntax         = DisasmSyntax::Intel;
};

} // namespace Debugger
} // namespace RawrXD

#endif // RAWRXD_NATIVE_DEBUGGER_ENGINE_H
