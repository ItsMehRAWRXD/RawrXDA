// =============================================================================
// native_debugger_engine.cpp — Phase 12: Native Debugger Engine Implementation
// =============================================================================
// Full DbgEng.dll COM interop for process-level debugging.
// Interfaces: IDebugClient7, IDebugControl7, IDebugSymbols5,
//             IDebugRegisters2, IDebugDataSpaces4, IDebugSystemObjects4.
//
// Provides:
//   - Launch / Attach / Detach / Terminate
//   - Event-driven debug loop (breakpoints, exceptions, module events)
//   - Software (INT3) and Hardware (DR0–DR3) breakpoints
//   - Register capture & modification
//   - Stack walking with symbol resolution
//   - Memory read / write / search / hex dump
//   - x64 disassembly (Intel syntax via DbgEng)
//   - Expression evaluation
//   - Watch expressions with auto-update
//   - Module & thread enumeration
//   - JSON serialization for all state
//
// Architecture: C++20 | Win64 | No exceptions (structured DebugResult) | No Qt
// Build:        Linked with dbghelp.lib + dbgeng.lib (already in CMake)
// Rule:         NO SOURCE FILE IS TO BE SIMPLIFIED
// =============================================================================

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <dbgeng.h>
#include <dbghelp.h>
#include <psapi.h>
#include <tlhelp32.h>

#include "native_debugger_engine.h"
#include "native_debugger_types.h"
#include "../win32app/Win32IDE.h"

#include <sstream>
#include <iomanip>
#include <algorithm>
#include <chrono>
#include <cstring>

// Link pragma — dbghelp always available; dbgeng loaded dynamically to
// avoid 0xC0000005 on systems where DbgEng runtime is absent/incompatible.
// #pragma comment(lib, "dbgeng.lib")  // REMOVED — now resolved via LoadLibrary
#pragma comment(lib, "dbghelp.lib")

// Dynamic DebugCreate — resolved at runtime
typedef HRESULT (STDAPICALLTYPE *PFN_DebugCreate)(REFIID InterfaceId, PVOID* Interface);
static HMODULE       g_hDbgEng       = nullptr;
static PFN_DebugCreate g_pfnDebugCreate = nullptr;

using namespace RawrXD::Debugger;

// =============================================================================
//                    Forward: ASM kernel externs
// =============================================================================

extern "C" {
    uint32_t Dbg_InjectINT3(uint64_t targetAddress, uint8_t* outOriginalByte);
    uint32_t Dbg_RestoreINT3(uint64_t targetAddress, uint8_t originalByte);
    uint32_t Dbg_SetHardwareBreakpoint(uint64_t threadHandle, uint32_t slotIndex,
                                        uint64_t address, uint32_t condition, uint32_t sizeBytes);
    uint32_t Dbg_ClearHardwareBreakpoint(uint64_t threadHandle, uint32_t slotIndex);
    uint32_t Dbg_EnableSingleStep(uint64_t threadHandle);
    uint32_t Dbg_DisableSingleStep(uint64_t threadHandle);
    uint32_t Dbg_CaptureContext(uint64_t threadHandle, void* outContextBuffer, uint32_t bufferSize);
    uint32_t Dbg_SetRegister(uint64_t threadHandle, uint32_t registerIndex, uint64_t value);
    uint32_t Dbg_WalkStack(uint64_t processHandle, uint64_t threadHandle,
                            uint64_t* outFrames, uint32_t maxFrames, uint32_t* outFrameCount);
    uint32_t Dbg_ReadMemory(uint64_t processHandle, uint64_t address,
                             void* outBuffer, uint64_t size, uint64_t* outBytesRead);
    uint32_t Dbg_WriteMemory(uint64_t processHandle, uint64_t address,
                              const void* buffer, uint64_t size, uint64_t* outBytesWritten);
    uint32_t Dbg_MemoryScan(uint64_t processHandle, uint64_t startAddress, uint64_t regionSize,
                             const void* pattern, uint32_t patternLen, uint64_t* outFoundAddress);
    uint32_t Dbg_MemoryCRC32(uint64_t processHandle, uint64_t address, uint64_t size,
                              uint32_t* outCRC);
    uint64_t Dbg_RDTSC(void);
}

// =============================================================================
//            DbgEng COM Callback Implementations
// =============================================================================

// IDebugEventCallbacksWide — receives debug events from DbgEng
class DebugEventCallbacks : public IDebugEventCallbacksWide {
public:
    DebugEventCallbacks(NativeDebuggerEngine* engine) : m_engine(engine), m_refCount(1) {}

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (riid == __uuidof(IUnknown) || riid == __uuidof(IDebugEventCallbacksWide)) {
            *ppv = this;
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&m_refCount); }
    STDMETHODIMP_(ULONG) Release() override {
        ULONG c = InterlockedDecrement(&m_refCount);
        if (c == 0) delete this;
        return c;
    }

    // Interest mask — which events we want
    STDMETHODIMP GetInterestMask(PULONG mask) override {
        *mask = DEBUG_EVENT_BREAKPOINT |
                DEBUG_EVENT_EXCEPTION |
                DEBUG_EVENT_CREATE_THREAD |
                DEBUG_EVENT_EXIT_THREAD |
                DEBUG_EVENT_CREATE_PROCESS |
                DEBUG_EVENT_EXIT_PROCESS |
                DEBUG_EVENT_LOAD_MODULE |
                DEBUG_EVENT_UNLOAD_MODULE |
                DEBUG_EVENT_SESSION_STATUS |
                DEBUG_EVENT_CHANGE_ENGINE_STATE |
                DEBUG_EVENT_CHANGE_DEBUGGEE_STATE;
        return S_OK;
    }

    // Breakpoint callback
    STDMETHODIMP Breakpoint(PDEBUG_BREAKPOINT2 bp) override {
        (void)bp;
        return DEBUG_STATUS_BREAK;
    }

    // Exception callback
    STDMETHODIMP Exception(PEXCEPTION_RECORD64 exception, ULONG firstChance) override {
        (void)exception;
        (void)firstChance;
        return DEBUG_STATUS_BREAK;
    }

    // Thread create
    STDMETHODIMP CreateThread(ULONG64 handle, ULONG64 dataOffset, ULONG64 startOffset) override {
        (void)handle; (void)dataOffset; (void)startOffset;
        return DEBUG_STATUS_NO_CHANGE;
    }

    // Thread exit
    STDMETHODIMP ExitThread(ULONG exitCode) override {
        (void)exitCode;
        return DEBUG_STATUS_NO_CHANGE;
    }

    // Process create
    STDMETHODIMP CreateProcess(ULONG64 imageFileHandle, ULONG64 handle,
                               ULONG64 baseOffset, ULONG moduleSize,
                               PCWSTR moduleName, PCWSTR imageName,
                               ULONG checkSum, ULONG timeDateStamp,
                               ULONG64 initialThreadHandle,
                               ULONG64 threadDataOffset,
                               ULONG64 startOffset) override {
        (void)imageFileHandle; (void)handle; (void)baseOffset;
        (void)moduleSize; (void)moduleName; (void)imageName;
        (void)checkSum; (void)timeDateStamp; (void)initialThreadHandle;
        (void)threadDataOffset; (void)startOffset;
        return DEBUG_STATUS_NO_CHANGE;
    }

    // Process exit
    STDMETHODIMP ExitProcess(ULONG exitCode) override {
        (void)exitCode;
        return DEBUG_STATUS_NO_CHANGE;
    }

    // Module load
    STDMETHODIMP LoadModule(ULONG64 imageFileHandle, ULONG64 baseOffset,
                            ULONG moduleSize, PCWSTR moduleName,
                            PCWSTR imageName, ULONG checkSum,
                            ULONG timeDateStamp) override {
        (void)imageFileHandle; (void)baseOffset; (void)moduleSize;
        (void)moduleName; (void)imageName; (void)checkSum; (void)timeDateStamp;
        return DEBUG_STATUS_NO_CHANGE;
    }

    // Module unload
    STDMETHODIMP UnloadModule(PCWSTR imageName, ULONG64 baseOffset) override {
        (void)imageName; (void)baseOffset;
        return DEBUG_STATUS_NO_CHANGE;
    }

    // Session status change
    STDMETHODIMP SessionStatus(ULONG status) override {
        (void)status;
        return S_OK;
    }

    // Engine state change
    STDMETHODIMP ChangeEngineState(ULONG flags, ULONG64 argument) override {
        (void)flags; (void)argument;
        return S_OK;
    }

    // Debuggee state change
    STDMETHODIMP ChangeDebuggeeState(ULONG flags, ULONG64 argument) override {
        (void)flags; (void)argument;
        return S_OK;
    }

    // Symbol state change
    STDMETHODIMP ChangeSymbolState(ULONG flags, ULONG64 argument) override {
        (void)flags; (void)argument;
        return S_OK;
    }

    // System error
    STDMETHODIMP SystemError(ULONG error, ULONG level) override {
        (void)error; (void)level;
        return DEBUG_STATUS_BREAK;
    }

private:
    NativeDebuggerEngine* m_engine;
    volatile LONG         m_refCount;
};

// IDebugOutputCallbacksWide — receives output text from DbgEng
class DebugOutputCallbacksImpl : public IDebugOutputCallbacksWide {
public:
    DebugOutputCallbacksImpl(NativeDebuggerEngine* engine) : m_engine(engine), m_refCount(1) {}

    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (riid == __uuidof(IUnknown) || riid == __uuidof(IDebugOutputCallbacksWide)) {
            *ppv = this;
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&m_refCount); }
    STDMETHODIMP_(ULONG) Release() override {
        ULONG c = InterlockedDecrement(&m_refCount);
        if (c == 0) delete this;
        return c;
    }

    STDMETHODIMP Output(ULONG mask, PCWSTR text) override {
        (void)mask; (void)text;
        return S_OK;
    }

private:
    NativeDebuggerEngine* m_engine;
    volatile LONG         m_refCount;
};

// =============================================================================
//                        Singleton
// =============================================================================

NativeDebuggerEngine& NativeDebuggerEngine::Instance() {
    static NativeDebuggerEngine s_instance;
    return s_instance;
}

NativeDebuggerEngine::NativeDebuggerEngine() {
    // Zero HW slots
    for (int i = 0; i < 4; i++) {
        m_hwSlots[i].slotIndex = static_cast<uint32_t>(i);
        m_hwSlots[i].active = false;
    }
}

NativeDebuggerEngine::~NativeDebuggerEngine() {
    if (m_initialized.load()) {
        shutdown();
    }
}

// =============================================================================
//                        Lifecycle
// =============================================================================

DebugResult NativeDebuggerEngine::initialize(const DebugConfig& config) {
    if (m_initialized.load()) {
        return DebugResult::ok("Already initialized");
    }

    m_config = config;

    // Initialize DbgEng COM interfaces
    DebugResult r = initDbgEng();
    if (!r.success) {
        return r;
    }

    // Set symbol path
    if (m_debugSymbols && !config.symbolPath.empty()) {
        HRESULT hr = m_debugSymbols->SetSymbolPathWide(
            std::wstring(config.symbolPath.begin(), config.symbolPath.end()).c_str());
        if (FAILED(hr)) {
            OutputDebugStringA("WARNING: Failed to set symbol path\n");
        }
    }

    m_initialized.store(true, std::memory_order_release);
    transitionState(DebugSessionState::Idle);

    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_stats.sessionsStarted++;
    }

    return DebugResult::ok("Native debugger engine initialized");
}

DebugResult NativeDebuggerEngine::shutdown() {
    if (!m_initialized.load()) {
        return DebugResult::ok("Not initialized");
    }

    // Stop event loop
    m_eventLoopRunning.store(false);
    m_eventCV.notify_all();
    if (m_eventThread.joinable()) {
        m_eventThread.join();
    }

    // Detach if still attached
    if (m_state.load() != DebugSessionState::Idle &&
        m_state.load() != DebugSessionState::Terminated) {
        detach();
    }

    // Release DbgEng COM
    releaseDbgEng();

    // Clear state
    {
        std::lock_guard<std::mutex> lock(m_bpMutex);
        m_breakpoints.clear();
    }
    {
        std::lock_guard<std::mutex> lock(m_watchMutex);
        m_watches.clear();
    }
    {
        std::lock_guard<std::mutex> lock(m_moduleMutex);
        m_modules.clear();
    }
    {
        std::lock_guard<std::mutex> lock(m_threadMutex);
        m_threads.clear();
    }
    {
        std::lock_guard<std::mutex> lock(m_historyMutex);
        m_eventHistory.clear();
    }

    m_initialized.store(false, std::memory_order_release);
    transitionState(DebugSessionState::Idle);

    return DebugResult::ok("Native debugger engine shut down");
}

// =============================================================================
//                    DbgEng COM Initialization
// =============================================================================

DebugResult NativeDebuggerEngine::initDbgEng() {
    HRESULT hr;

    // --- Dynamic load dbgeng.dll (prevents 0xC0000005 if runtime missing) ---
    if (!g_hDbgEng) {
        g_hDbgEng = LoadLibraryA("dbgeng.dll");
        if (!g_hDbgEng) {
            return DebugResult::error("dbgeng.dll not found — install Debugging Tools for Windows", (int)GetLastError());
        }
        g_pfnDebugCreate = (PFN_DebugCreate)GetProcAddress(g_hDbgEng, "DebugCreate");
        if (!g_pfnDebugCreate) {
            FreeLibrary(g_hDbgEng);
            g_hDbgEng = nullptr;
            return DebugResult::error("DebugCreate export not found in dbgeng.dll", (int)GetLastError());
        }
    }

    // Create IDebugClient
    hr = g_pfnDebugCreate(__uuidof(IDebugClient7), (void**)&m_debugClient);
    if (FAILED(hr)) {
        // Fallback to IDebugClient5
        hr = g_pfnDebugCreate(__uuidof(IDebugClient5), (void**)&m_debugClient);
        if (FAILED(hr)) {
            return DebugResult::error("Failed to create IDebugClient", hr);
        }
    }

    // QueryInterface for all needed interfaces
    hr = m_debugClient->QueryInterface(__uuidof(IDebugControl7), (void**)&m_debugControl);
    if (FAILED(hr)) {
        // Fallback to IDebugControl4
        hr = m_debugClient->QueryInterface(__uuidof(IDebugControl4), (void**)&m_debugControl);
        if (FAILED(hr)) {
            releaseDbgEng();
            return DebugResult::error("Failed to get IDebugControl", hr);
        }
    }

    hr = m_debugClient->QueryInterface(__uuidof(IDebugSymbols5), (void**)&m_debugSymbols);
    if (FAILED(hr)) {
        hr = m_debugClient->QueryInterface(__uuidof(IDebugSymbols3), (void**)&m_debugSymbols);
        if (FAILED(hr)) {
            releaseDbgEng();
            return DebugResult::error("Failed to get IDebugSymbols", hr);
        }
    }

    hr = m_debugClient->QueryInterface(__uuidof(IDebugRegisters2), (void**)&m_debugRegisters);
    if (FAILED(hr)) {
        releaseDbgEng();
        return DebugResult::error("Failed to get IDebugRegisters", hr);
    }

    hr = m_debugClient->QueryInterface(__uuidof(IDebugDataSpaces4), (void**)&m_debugDataSpaces);
    if (FAILED(hr)) {
        releaseDbgEng();
        return DebugResult::error("Failed to get IDebugDataSpaces", hr);
    }

    hr = m_debugClient->QueryInterface(__uuidof(IDebugSystemObjects4), (void**)&m_debugSysObjects);
    if (FAILED(hr)) {
        releaseDbgEng();
        return DebugResult::error("Failed to get IDebugSystemObjects", hr);
    }

    // Set event callbacks
    auto* evtCb = new DebugEventCallbacks(this);
    m_debugClient->SetEventCallbacksWide(evtCb);
    evtCb->Release(); // Client now holds ref

    // Set output callbacks
    auto* outCb = new DebugOutputCallbacksImpl(this);
    m_debugClient->SetOutputCallbacksWide(outCb);
    outCb->Release();

    return DebugResult::ok("DbgEng COM initialized");
}

void NativeDebuggerEngine::releaseDbgEng() {
    if (m_debugAdvanced)    { m_debugAdvanced->Release();   m_debugAdvanced     = nullptr; }
    if (m_debugSysObjects)  { m_debugSysObjects->Release(); m_debugSysObjects   = nullptr; }
    if (m_debugDataSpaces)  { m_debugDataSpaces->Release(); m_debugDataSpaces   = nullptr; }
    if (m_debugRegisters)   { m_debugRegisters->Release();  m_debugRegisters    = nullptr; }
    if (m_debugSymbols)     { m_debugSymbols->Release();    m_debugSymbols      = nullptr; }
    if (m_debugControl)     { m_debugControl->Release();    m_debugControl      = nullptr; }
    if (m_debugClient)      { m_debugClient->Release();     m_debugClient       = nullptr; }
}

// =============================================================================
//                    Session Control
// =============================================================================

DebugResult NativeDebuggerEngine::launchProcess(const std::string& exePath,
                                                 const std::string& args,
                                                 const std::string& workingDir) {
    if (!m_initialized.load()) return DebugResult::error("Engine not initialized");
    if (m_state.load() != DebugSessionState::Idle &&
        m_state.load() != DebugSessionState::Terminated) {
        return DebugResult::error("Session already active — detach first");
    }

    transitionState(DebugSessionState::Launching);

    // Build command line
    std::string cmdLine = "\"" + exePath + "\"";
    if (!args.empty()) {
        cmdLine += " " + args;
    }

    // CreateProcess via DbgEng
    HRESULT hr = m_debugClient->CreateProcessA(
        0,                                  // Server (local = 0)
        const_cast<char*>(cmdLine.c_str()), // Command line
        DEBUG_PROCESS | DEBUG_ONLY_THIS_PROCESS   // Create flags
    );

    if (FAILED(hr)) {
        transitionState(DebugSessionState::Idle);
        return DebugResult::error("Failed to launch process", hr);
    }

    // Wait for initial event (CREATE_PROCESS)
    hr = m_debugControl->WaitForEvent(DEBUG_WAIT_DEFAULT, 10000); // 10s timeout
    if (FAILED(hr)) {
        transitionState(DebugSessionState::Error);
        return DebugResult::error("Timeout waiting for process creation event", hr);
    }

    // Get PID
    ULONG pid = 0;
    if (m_debugSysObjects) {
        m_debugSysObjects->GetCurrentProcessSystemId(&pid);
    }
    m_targetPID = pid;
    m_targetPath = exePath;

    // Extract name from path
    size_t lastSlash = exePath.find_last_of("\\/");
    m_targetName = (lastSlash != std::string::npos) ? exePath.substr(lastSlash + 1) : exePath;

    // Auto-load symbols if configured
    if (m_config.autoLoadSymbols && m_debugSymbols) {
        m_debugSymbols->Reload("");
    }

    // Start event loop thread
    m_eventLoopRunning.store(true);
    m_eventThread = std::thread(&NativeDebuggerEngine::eventLoopThread, this);

    // If breakOnEntry, stay broken; otherwise go
    if (m_config.breakOnEntry) {
        transitionState(DebugSessionState::Broken);
    } else {
        go();
    }

    return DebugResult::ok("Process launched and attached");
}

DebugResult NativeDebuggerEngine::attachToProcess(uint32_t pid) {
    if (!m_initialized.load()) return DebugResult::error("Engine not initialized");
    if (m_state.load() != DebugSessionState::Idle &&
        m_state.load() != DebugSessionState::Terminated) {
        return DebugResult::error("Session already active — detach first");
    }

    transitionState(DebugSessionState::Attaching);

    HRESULT hr = m_debugClient->AttachProcess(
        0,          // Server (local = 0)
        pid,
        DEBUG_ATTACH_DEFAULT
    );

    if (FAILED(hr)) {
        transitionState(DebugSessionState::Idle);
        return DebugResult::error("Failed to attach to process", hr);
    }

    // Wait for attach event
    hr = m_debugControl->WaitForEvent(DEBUG_WAIT_DEFAULT, 10000);
    if (FAILED(hr)) {
        transitionState(DebugSessionState::Error);
        return DebugResult::error("Timeout waiting for attach", hr);
    }

    m_targetPID = pid;

    // Get process name
    char nameBuffer[MAX_PATH] = {};
    ULONG nameSize = 0;
    if (m_debugSysObjects) {
        m_debugSysObjects->GetCurrentProcessExecutableName(nameBuffer, sizeof(nameBuffer), &nameSize);
        m_targetName = nameBuffer;
        m_targetPath = nameBuffer;
    }

    // Auto-load symbols
    if (m_config.autoLoadSymbols && m_debugSymbols) {
        m_debugSymbols->Reload("");
    }

    // Start event loop
    m_eventLoopRunning.store(true);
    m_eventThread = std::thread(&NativeDebuggerEngine::eventLoopThread, this);

    transitionState(DebugSessionState::Broken);

    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_stats.sessionsStarted++;
    }

    return DebugResult::ok("Attached to process");
}

DebugResult NativeDebuggerEngine::detach() {
    if (!m_initialized.load()) return DebugResult::error("Engine not initialized");

    transitionState(DebugSessionState::Detaching);

    // Stop event loop
    m_eventLoopRunning.store(false);
    m_eventCV.notify_all();
    if (m_eventThread.joinable()) {
        m_eventThread.join();
    }

    // Remove all breakpoints (restore INT3 patches)
    {
        std::lock_guard<std::mutex> lock(m_bpMutex);
        for (auto& bp : m_breakpoints) {
            if (bp.type == BreakpointType::Software && bp.state != BreakpointState::Removed) {
                removeSoftwareBreakpoint(bp);
            } else if (bp.type == BreakpointType::Hardware && bp.state != BreakpointState::Removed) {
                removeHardwareBreakpoint(bp);
            }
        }
    }

    // Detach via DbgEng
    if (m_debugClient) {
        m_debugClient->DetachProcesses();
    }

    m_targetPID = 0;
    m_targetName.clear();
    m_targetPath.clear();
    m_processHandle = 0;

    transitionState(DebugSessionState::Idle);

    return DebugResult::ok("Detached from process");
}

DebugResult NativeDebuggerEngine::terminateTarget() {
    if (!m_initialized.load()) return DebugResult::error("Engine not initialized");
    if (m_state.load() == DebugSessionState::Idle) return DebugResult::ok("No target");

    // Stop event loop first
    m_eventLoopRunning.store(false);
    m_eventCV.notify_all();
    if (m_eventThread.joinable()) {
        m_eventThread.join();
    }

    if (m_debugClient) {
        m_debugClient->TerminateCurrentProcess();
        m_debugClient->DetachProcesses();
    }

    m_targetPID = 0;
    m_targetName.clear();
    transitionState(DebugSessionState::Terminated);

    return DebugResult::ok("Target terminated");
}

// =============================================================================
//                    Execution Control
// =============================================================================

DebugResult NativeDebuggerEngine::go() {
    if (!m_debugControl) return DebugResult::error("No debug control interface");

    HRESULT hr = m_debugControl->SetExecutionStatus(DEBUG_STATUS_GO);
    if (FAILED(hr)) {
        return DebugResult::error("Failed to continue execution", hr);
    }

    transitionState(DebugSessionState::Running);
    return DebugResult::ok("Execution continued");
}

DebugResult NativeDebuggerEngine::stepOver() {
    if (!m_debugControl) return DebugResult::error("No debug control interface");

    transitionState(DebugSessionState::Stepping);

    HRESULT hr;
    if (m_config.enableSourceStepping) {
        hr = m_debugControl->SetExecutionStatus(DEBUG_STATUS_STEP_OVER);
    } else {
        hr = m_debugControl->SetExecutionStatus(DEBUG_STATUS_STEP_OVER);
    }

    if (FAILED(hr)) {
        return DebugResult::error("Step over failed", hr);
    }

    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_stats.totalSteps++;
    }

    return DebugResult::ok("Step over");
}

DebugResult NativeDebuggerEngine::stepInto() {
    if (!m_debugControl) return DebugResult::error("No debug control interface");

    transitionState(DebugSessionState::Stepping);

    HRESULT hr = m_debugControl->SetExecutionStatus(DEBUG_STATUS_STEP_INTO);
    if (FAILED(hr)) {
        return DebugResult::error("Step into failed", hr);
    }

    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_stats.totalSteps++;
    }

    return DebugResult::ok("Step into");
}

DebugResult NativeDebuggerEngine::stepOut() {
    if (!m_debugControl) return DebugResult::error("No debug control interface");

    // Execute ".stepout" command via DbgEng
    HRESULT hr = m_debugControl->Execute(
        DEBUG_OUTCTL_ALL_CLIENTS,
        "gu",   // "go up" — step out of current function
        DEBUG_EXECUTE_DEFAULT
    );

    if (FAILED(hr)) {
        return DebugResult::error("Step out failed", hr);
    }

    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_stats.totalSteps++;
    }

    return DebugResult::ok("Step out");
}

DebugResult NativeDebuggerEngine::stepToAddress(uint64_t address) {
    if (!m_debugControl) return DebugResult::error("No debug control interface");

    // Set temporary breakpoint at address, then go
    std::ostringstream cmd;
    cmd << "g 0x" << std::hex << address;

    HRESULT hr = m_debugControl->Execute(
        DEBUG_OUTCTL_ALL_CLIENTS,
        cmd.str().c_str(),
        DEBUG_EXECUTE_DEFAULT
    );

    if (FAILED(hr)) {
        return DebugResult::error("Step to address failed", hr);
    }

    return DebugResult::ok("Running to address");
}

DebugResult NativeDebuggerEngine::breakExecution() {
    if (!m_debugControl) return DebugResult::error("No debug control interface");

    HRESULT hr = m_debugControl->SetInterrupt(DEBUG_INTERRUPT_ACTIVE);
    if (FAILED(hr)) {
        return DebugResult::error("Failed to break execution", hr);
    }

    return DebugResult::ok("Break requested");
}

DebugResult NativeDebuggerEngine::runToLine(const std::string& file, int line) {
    // Resolve source line to address
    uint64_t addr = 0;
    if (m_debugSymbols) {
        ULONG64 offset = 0;
        std::wstring wFile(file.begin(), file.end());
        HRESULT hr = m_debugSymbols->GetOffsetByLineWide(
            static_cast<ULONG>(line),
            wFile.c_str(),
            &offset
        );
        if (FAILED(hr)) {
            return DebugResult::error("Cannot resolve source line to address", hr);
        }
        addr = offset;
    }

    return stepToAddress(addr);
}

// =============================================================================
//                    Breakpoint Management
// =============================================================================

uint32_t NativeDebuggerEngine::allocateBreakpointId() {
    return m_nextBpId.fetch_add(1, std::memory_order_relaxed);
}

DebugResult NativeDebuggerEngine::addBreakpoint(uint64_t address, BreakpointType type) {
    NativeBreakpoint bp;
    bp.id       = allocateBreakpointId();
    bp.address  = address;
    bp.type     = type;
    bp.state    = BreakpointState::Enabled;

    // Resolve symbol if possible
    resolveBreakpointSymbol(bp);

    DebugResult r;
    if (type == BreakpointType::Hardware || type == BreakpointType::DataWatch) {
        r = applyHardwareBreakpoint(bp);
    } else {
        r = applySoftwareBreakpoint(bp);
    }

    if (!r.success) return r;

    {
        std::lock_guard<std::mutex> lock(m_bpMutex);
        m_breakpoints.push_back(bp);
    }

    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_stats.totalBreakpointsSet++;
    }

    return DebugResult::ok("Breakpoint added");
}

DebugResult NativeDebuggerEngine::addBreakpointBySymbol(const std::string& symbol,
                                                         BreakpointType type) {
    uint64_t address = 0;
    DebugResult r = resolveAddress(symbol, address);
    if (!r.success) {
        // Create pending breakpoint
        NativeBreakpoint bp;
        bp.id       = allocateBreakpointId();
        bp.symbol   = symbol;
        bp.type     = type;
        bp.state    = BreakpointState::Pending;

        {
            std::lock_guard<std::mutex> lock(m_bpMutex);
            m_breakpoints.push_back(bp);
        }

        return DebugResult::ok("Pending breakpoint added (symbol not yet resolved)");
    }

    return addBreakpoint(address, type);
}

DebugResult NativeDebuggerEngine::addBreakpointBySourceLine(const std::string& file, int line) {
    if (!m_debugSymbols) return DebugResult::error("No symbol interface");

    ULONG64 offset = 0;
    std::wstring wFile(file.begin(), file.end());
    HRESULT hr = m_debugSymbols->GetOffsetByLineWide(
        static_cast<ULONG>(line),
        wFile.c_str(),
        &offset
    );

    if (FAILED(hr)) {
        // Create pending BP
        NativeBreakpoint bp;
        bp.id           = allocateBreakpointId();
        bp.sourceFile   = file;
        bp.sourceLine   = line;
        bp.type         = BreakpointType::Software;
        bp.state        = BreakpointState::Pending;

        {
            std::lock_guard<std::mutex> lock(m_bpMutex);
            m_breakpoints.push_back(bp);
        }

        return DebugResult::ok("Pending breakpoint (source not resolved)");
    }

    NativeBreakpoint bp;
    bp.id           = allocateBreakpointId();
    bp.address      = offset;
    bp.sourceFile   = file;
    bp.sourceLine   = line;
    bp.type         = BreakpointType::Software;
    bp.state        = BreakpointState::Enabled;

    resolveBreakpointSymbol(bp);
    DebugResult r = applySoftwareBreakpoint(bp);
    if (!r.success) return r;

    {
        std::lock_guard<std::mutex> lock(m_bpMutex);
        m_breakpoints.push_back(bp);
    }

    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_stats.totalBreakpointsSet++;
    }

    return DebugResult::ok("Source breakpoint set");
}

DebugResult NativeDebuggerEngine::addConditionalBreakpoint(uint64_t address,
                                                            const std::string& condition) {
    NativeBreakpoint bp;
    bp.id        = allocateBreakpointId();
    bp.address   = address;
    bp.type      = BreakpointType::Conditional;
    bp.condition = condition;
    bp.state     = BreakpointState::Enabled;

    resolveBreakpointSymbol(bp);
    DebugResult r = applySoftwareBreakpoint(bp);
    if (!r.success) return r;

    {
        std::lock_guard<std::mutex> lock(m_bpMutex);
        m_breakpoints.push_back(bp);
    }

    return DebugResult::ok("Conditional breakpoint set");
}

DebugResult NativeDebuggerEngine::addDataBreakpoint(uint64_t address, uint32_t sizeBytes,
                                                     uint32_t condition) {
    NativeBreakpoint bp;
    bp.id                   = allocateBreakpointId();
    bp.address              = address;
    bp.type                 = BreakpointType::DataWatch;
    bp.state                = BreakpointState::Enabled;
    bp.hwSlot.sizeBytes     = sizeBytes;
    bp.hwSlot.condition     = condition;

    DebugResult r = applyHardwareBreakpoint(bp);
    if (!r.success) return r;

    {
        std::lock_guard<std::mutex> lock(m_bpMutex);
        m_breakpoints.push_back(bp);
    }

    return DebugResult::ok("Data breakpoint set");
}

DebugResult NativeDebuggerEngine::removeBreakpoint(uint32_t bpId) {
    std::lock_guard<std::mutex> lock(m_bpMutex);

    auto it = std::find_if(m_breakpoints.begin(), m_breakpoints.end(),
        [bpId](const NativeBreakpoint& bp) { return bp.id == bpId; });

    if (it == m_breakpoints.end()) {
        return DebugResult::error("Breakpoint not found");
    }

    if (it->type == BreakpointType::Hardware || it->type == BreakpointType::DataWatch) {
        removeHardwareBreakpoint(*it);
    } else {
        removeSoftwareBreakpoint(*it);
    }

    it->state = BreakpointState::Removed;
    m_breakpoints.erase(it);

    return DebugResult::ok("Breakpoint removed");
}

DebugResult NativeDebuggerEngine::enableBreakpoint(uint32_t bpId, bool enable) {
    std::lock_guard<std::mutex> lock(m_bpMutex);

    auto it = std::find_if(m_breakpoints.begin(), m_breakpoints.end(),
        [bpId](const NativeBreakpoint& bp) { return bp.id == bpId; });

    if (it == m_breakpoints.end()) {
        return DebugResult::error("Breakpoint not found");
    }

    it->state = enable ? BreakpointState::Enabled : BreakpointState::Disabled;
    return DebugResult::ok(enable ? "Breakpoint enabled" : "Breakpoint disabled");
}

DebugResult NativeDebuggerEngine::removeAllBreakpoints() {
    std::lock_guard<std::mutex> lock(m_bpMutex);

    for (auto& bp : m_breakpoints) {
        if (bp.type == BreakpointType::Hardware || bp.type == BreakpointType::DataWatch) {
            removeHardwareBreakpoint(bp);
        } else {
            removeSoftwareBreakpoint(bp);
        }
    }
    m_breakpoints.clear();

    return DebugResult::ok("All breakpoints removed");
}

const NativeBreakpoint* NativeDebuggerEngine::findBreakpointById(uint32_t id) const {
    std::lock_guard<std::mutex> lock(m_bpMutex);
    for (const auto& bp : m_breakpoints) {
        if (bp.id == id) return &bp;
    }
    return nullptr;
}

// Internal: apply software breakpoint via ASM kernel or DbgEng
DebugResult NativeDebuggerEngine::applySoftwareBreakpoint(NativeBreakpoint& bp) {
    if (bp.address == 0) return DebugResult::error("Invalid address for software BP");

    // Try ASM kernel first (direct INT3 injection)
    uint8_t origByte = 0;
    uint32_t result = Dbg_InjectINT3(bp.address, &origByte);
    if (result != 0) {
        // Fallback: use DbgEng breakpoint API
        if (m_debugControl) {
            IDebugBreakpoint* dbgBp = nullptr;
            HRESULT hr = m_debugControl->AddBreakpoint(DEBUG_BREAKPOINT_CODE, DEBUG_ANY_ID, &dbgBp);
            if (SUCCEEDED(hr) && dbgBp) {
                dbgBp->SetOffset(bp.address);
                dbgBp->AddFlags(DEBUG_BREAKPOINT_ENABLED);
                bp.originalByte = 0; // DbgEng manages it
                bp.state = BreakpointState::Enabled;
                return DebugResult::ok("Software BP via DbgEng");
            }
        }
        return DebugResult::error("Failed to inject INT3");
    }

    bp.originalByte = origByte;
    bp.state = BreakpointState::Enabled;
    return DebugResult::ok("Software BP applied");
}

DebugResult NativeDebuggerEngine::applyHardwareBreakpoint(NativeBreakpoint& bp) {
    int slot = findFreeHWSlot();
    if (slot < 0) {
        return DebugResult::error("No free hardware breakpoint slots (DR0–DR3 all in use)");
    }

    // Get thread handle for HW BP
    uint64_t threadHandle = 0;
    if (m_debugSysObjects) {
        ULONG64 handle = 0;
        m_debugSysObjects->GetCurrentThreadHandle(&handle);
        threadHandle = handle;
    }

    uint32_t result = Dbg_SetHardwareBreakpoint(
        threadHandle,
        static_cast<uint32_t>(slot),
        bp.address,
        bp.hwSlot.condition,
        bp.hwSlot.sizeBytes
    );

    if (result != 0) {
        return DebugResult::error("Failed to set hardware breakpoint");
    }

    bp.hwSlot.slotIndex = static_cast<uint32_t>(slot);
    bp.hwSlot.address   = bp.address;
    bp.hwSlot.active    = true;
    m_hwSlots[slot]     = bp.hwSlot;

    return DebugResult::ok("Hardware BP set");
}

DebugResult NativeDebuggerEngine::removeSoftwareBreakpoint(NativeBreakpoint& bp) {
    if (bp.originalByte != 0 && bp.address != 0) {
        Dbg_RestoreINT3(bp.address, bp.originalByte);
    }
    bp.state = BreakpointState::Removed;
    return DebugResult::ok("Software BP removed");
}

DebugResult NativeDebuggerEngine::removeHardwareBreakpoint(NativeBreakpoint& bp) {
    if (bp.hwSlot.active) {
        uint64_t threadHandle = 0;
        if (m_debugSysObjects) {
            ULONG64 handle = 0;
            m_debugSysObjects->GetCurrentThreadHandle(&handle);
            threadHandle = handle;
        }
        Dbg_ClearHardwareBreakpoint(threadHandle, bp.hwSlot.slotIndex);
        m_hwSlots[bp.hwSlot.slotIndex].active = false;
        bp.hwSlot.active = false;
    }
    bp.state = BreakpointState::Removed;
    return DebugResult::ok("Hardware BP removed");
}

int NativeDebuggerEngine::findFreeHWSlot() const {
    for (int i = 0; i < 4; i++) {
        if (!m_hwSlots[i].active) return i;
    }
    return -1;
}

void NativeDebuggerEngine::resolveBreakpointSymbol(NativeBreakpoint& bp) {
    if (!m_debugSymbols || bp.address == 0) return;

    char nameBuf[512] = {};
    ULONG nameSize = 0;
    ULONG64 displacement = 0;

    HRESULT hr = m_debugSymbols->GetNameByOffset(
        bp.address, nameBuf, sizeof(nameBuf), &nameSize, &displacement);

    if (SUCCEEDED(hr)) {
        bp.symbol = nameBuf;
    }

    // Try to get source file + line
    char fileBuf[MAX_PATH] = {};
    ULONG fileSize = 0;
    ULONG line = 0;

    hr = m_debugSymbols->GetLineByOffset(
        bp.address, &line, fileBuf, sizeof(fileBuf), &fileSize, nullptr);

    if (SUCCEEDED(hr)) {
        bp.sourceFile = fileBuf;
        bp.sourceLine = static_cast<int>(line);
    }
}

// =============================================================================
//                    Register Access
// =============================================================================

DebugResult NativeDebuggerEngine::captureRegisters(RegisterSnapshot& outSnapshot) {
    if (!m_debugRegisters) return DebugResult::error("No register interface");

    // Use DbgEng to get register values
    ULONG numRegs = 0;
    m_debugRegisters->GetNumberRegisters(&numRegs);

    // Map register indices — standard x64 layout
    auto getRegValue = [&](const char* name) -> uint64_t {
        ULONG index = 0;
        HRESULT hr = m_debugRegisters->GetIndexByName(name, &index);
        if (FAILED(hr)) return 0;

        DEBUG_VALUE val = {};
        hr = m_debugRegisters->GetValue(index, &val);
        if (FAILED(hr)) return 0;

        return val.I64;
    };

    outSnapshot.rax     = getRegValue("rax");
    outSnapshot.rbx     = getRegValue("rbx");
    outSnapshot.rcx     = getRegValue("rcx");
    outSnapshot.rdx     = getRegValue("rdx");
    outSnapshot.rsi     = getRegValue("rsi");
    outSnapshot.rdi     = getRegValue("rdi");
    outSnapshot.rbp     = getRegValue("rbp");
    outSnapshot.rsp     = getRegValue("rsp");
    outSnapshot.r8      = getRegValue("r8");
    outSnapshot.r9      = getRegValue("r9");
    outSnapshot.r10     = getRegValue("r10");
    outSnapshot.r11     = getRegValue("r11");
    outSnapshot.r12     = getRegValue("r12");
    outSnapshot.r13     = getRegValue("r13");
    outSnapshot.r14     = getRegValue("r14");
    outSnapshot.r15     = getRegValue("r15");
    outSnapshot.rip     = getRegValue("rip");
    outSnapshot.rflags  = getRegValue("efl");

    return DebugResult::ok("Registers captured");
}

DebugResult NativeDebuggerEngine::setRegister(const std::string& regName, uint64_t value) {
    if (!m_debugRegisters) return DebugResult::error("No register interface");

    ULONG index = 0;
    HRESULT hr = m_debugRegisters->GetIndexByName(regName.c_str(), &index);
    if (FAILED(hr)) return DebugResult::error("Unknown register name");

    DEBUG_VALUE val = {};
    val.Type = DEBUG_VALUE_INT64;
    val.I64 = value;

    hr = m_debugRegisters->SetValue(index, &val);
    if (FAILED(hr)) return DebugResult::error("Failed to set register", hr);

    return DebugResult::ok("Register set");
}

std::string NativeDebuggerEngine::formatRegisters(const RegisterSnapshot& snap) const {
    std::ostringstream oss;
    oss << std::hex << std::uppercase;
    oss << "RAX=" << std::setfill('0') << std::setw(16) << snap.rax
        << " RBX=" << std::setw(16) << snap.rbx
        << " RCX=" << std::setw(16) << snap.rcx << "\n";
    oss << "RDX=" << std::setw(16) << snap.rdx
        << " RSI=" << std::setw(16) << snap.rsi
        << " RDI=" << std::setw(16) << snap.rdi << "\n";
    oss << "RBP=" << std::setw(16) << snap.rbp
        << " RSP=" << std::setw(16) << snap.rsp
        << " RIP=" << std::setw(16) << snap.rip << "\n";
    oss << " R8=" << std::setw(16) << snap.r8
        << "  R9=" << std::setw(16) << snap.r9
        << " R10=" << std::setw(16) << snap.r10 << "\n";
    oss << "R11=" << std::setw(16) << snap.r11
        << " R12=" << std::setw(16) << snap.r12
        << " R13=" << std::setw(16) << snap.r13 << "\n";
    oss << "R14=" << std::setw(16) << snap.r14
        << " R15=" << std::setw(16) << snap.r15 << "\n";
    oss << "RFLAGS=" << std::setw(16) << snap.rflags
        << " [" << formatFlags(snap.rflags) << "]\n";
    return oss.str();
}

std::string NativeDebuggerEngine::formatFlags(uint64_t rflags) const {
    std::string s;
    if (rflags & 0x001) s += "CF ";
    if (rflags & 0x004) s += "PF ";
    if (rflags & 0x010) s += "AF ";
    if (rflags & 0x040) s += "ZF ";
    if (rflags & 0x080) s += "SF ";
    if (rflags & 0x100) s += "TF ";
    if (rflags & 0x200) s += "IF ";
    if (rflags & 0x400) s += "DF ";
    if (rflags & 0x800) s += "OF ";
    if (!s.empty() && s.back() == ' ') s.pop_back();
    return s;
}

// =============================================================================
//                    Stack Walking
// =============================================================================

DebugResult NativeDebuggerEngine::walkStack(std::vector<NativeStackFrame>& outFrames,
                                             uint32_t maxFrames) {
    if (!m_debugControl || !m_debugSymbols) {
        return DebugResult::error("No debug interfaces for stack walk");
    }

    outFrames.clear();

    // Use DbgEng stack walking
    std::vector<DEBUG_STACK_FRAME> frames(maxFrames);
    ULONG framesFilled = 0;

    HRESULT hr = m_debugControl->GetStackTrace(
        0, 0, 0,                    // Use current context
        frames.data(),
        static_cast<ULONG>(maxFrames),
        &framesFilled
    );

    if (FAILED(hr)) {
        // Fallback: use ASM kernel stack walker
        std::vector<uint64_t> returnAddrs(maxFrames);
        uint32_t frameCount = 0;
        uint32_t result = Dbg_WalkStack(m_processHandle, 0,
                                         returnAddrs.data(), maxFrames, &frameCount);
        if (result == 0) {
            for (uint32_t i = 0; i < frameCount; i++) {
                NativeStackFrame f;
                f.frameIndex        = i;
                f.instructionPtr    = returnAddrs[i];
                f.returnAddress     = (i + 1 < frameCount) ? returnAddrs[i + 1] : 0;

                // Resolve symbol
                std::string sym;
                uint64_t disp = 0;
                resolveSymbol(returnAddrs[i], sym, disp);
                f.function      = sym;
                f.displacement  = disp;

                outFrames.push_back(f);
            }
            return DebugResult::ok("Stack walked (ASM fallback)");
        }
        return DebugResult::error("Stack walk failed");
    }

    // Process DbgEng frames
    for (ULONG i = 0; i < framesFilled; i++) {
        NativeStackFrame f;
        f.frameIndex        = i;
        f.instructionPtr    = frames[i].InstructionOffset;
        f.returnAddress     = frames[i].ReturnOffset;
        f.framePointer      = frames[i].FrameOffset;
        f.stackPointer      = frames[i].StackOffset;

        // Resolve symbol
        char nameBuf[512] = {};
        ULONG nameSize = 0;
        ULONG64 displacement = 0;
        hr = m_debugSymbols->GetNameByOffset(
            frames[i].InstructionOffset, nameBuf, sizeof(nameBuf), &nameSize, &displacement);
        if (SUCCEEDED(hr)) {
            f.function = nameBuf;
            f.displacement = displacement;
        }

        // Resolve source line
        char fileBuf[MAX_PATH] = {};
        ULONG fileSize = 0;
        ULONG line = 0;
        hr = m_debugSymbols->GetLineByOffset(
            frames[i].InstructionOffset, &line, fileBuf, sizeof(fileBuf), &fileSize, nullptr);
        if (SUCCEEDED(hr)) {
            f.sourceFile    = fileBuf;
            f.sourceLine    = static_cast<int>(line);
            f.hasSource     = true;
        }

        // Get module name
        char modBuf[256] = {};
        ULONG modSize = 0;
        ULONG64 modBase = 0;
        hr = m_debugSymbols->GetModuleByOffset(
            frames[i].InstructionOffset, 0, nullptr, &modBase);
        if (SUCCEEDED(hr)) {
            m_debugSymbols->GetModuleNames(DEBUG_ANY_ID, modBase,
                nullptr, 0, nullptr,
                modBuf, sizeof(modBuf), &modSize,
                nullptr, 0, nullptr);
            f.module = modBuf;
        }

        outFrames.push_back(f);
    }

    return DebugResult::ok("Stack walked");
}

DebugResult NativeDebuggerEngine::getFrameLocals(uint32_t frameIndex,
                                                   std::map<std::string, std::string>& outLocals) {
    if (!m_debugSymbols) return DebugResult::error("No symbol interface");

    // Set scope to specific frame
    std::vector<DEBUG_STACK_FRAME> frames(frameIndex + 1);
    ULONG framesFilled = 0;
    HRESULT hr = m_debugControl->GetStackTrace(0, 0, 0, frames.data(),
                                                frameIndex + 1, &framesFilled);
    if (FAILED(hr) || framesFilled <= frameIndex) {
        return DebugResult::error("Invalid frame index");
    }

    hr = m_debugSymbols->SetScope(frames[frameIndex].InstructionOffset,
                                   &frames[frameIndex], nullptr, 0);
    if (FAILED(hr)) return DebugResult::error("Failed to set scope");

    // Enumerate locals via DbgEng
    IDebugSymbolGroup2* group = nullptr;
    hr = m_debugSymbols->GetScopeSymbolGroup2(DEBUG_SCOPE_GROUP_LOCALS, nullptr, &group);
    if (FAILED(hr) || !group) return DebugResult::error("Failed to get local symbols");

    ULONG numSymbols = 0;
    group->GetNumberSymbols(&numSymbols);

    for (ULONG i = 0; i < numSymbols; i++) {
        char nameBuf[256] = {};
        ULONG nameSize = 0;
        group->GetSymbolName(i, nameBuf, sizeof(nameBuf), &nameSize);

        char valBuf[512] = {};
        ULONG valSize = 0;
        group->GetSymbolValueText(i, valBuf, sizeof(valBuf), &valSize);

        outLocals[nameBuf] = valBuf;
    }

    group->Release();
    return DebugResult::ok("Locals retrieved");
}

std::string NativeDebuggerEngine::formatStackTrace(const std::vector<NativeStackFrame>& frames) const {
    std::ostringstream oss;
    for (const auto& f : frames) {
        oss << "#" << f.frameIndex << " "
            << formatAddress(f.instructionPtr) << " "
            << f.function;
        if (f.displacement > 0) {
            oss << "+0x" << std::hex << f.displacement;
        }
        if (f.hasSource) {
            oss << " (" << f.sourceFile << ":" << std::dec << f.sourceLine << ")";
        }
        if (!f.module.empty()) {
            oss << " [" << f.module << "]";
        }
        oss << "\n";
    }
    return oss.str();
}

// =============================================================================
//                    Memory Operations
// =============================================================================

DebugResult NativeDebuggerEngine::readMemory(uint64_t address, void* buffer,
                                              uint64_t size, uint64_t* bytesRead) {
    if (!m_debugDataSpaces) return DebugResult::error("No data spaces interface");

    ULONG br = 0;
    HRESULT hr = m_debugDataSpaces->ReadVirtual(address, buffer, static_cast<ULONG>(size), &br);

    if (bytesRead) *bytesRead = br;

    if (FAILED(hr)) {
        // Fallback: ASM kernel
        uint64_t asmBytesRead = 0;
        uint32_t result = Dbg_ReadMemory(m_processHandle, address, buffer, size, &asmBytesRead);
        if (result == 0) {
            if (bytesRead) *bytesRead = asmBytesRead;
            return DebugResult::ok("Memory read (ASM fallback)");
        }
        return DebugResult::error("Failed to read memory", hr);
    }

    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_stats.totalMemoryReads++;
    }

    return DebugResult::ok("Memory read");
}

DebugResult NativeDebuggerEngine::writeMemory(uint64_t address, const void* buffer, uint64_t size) {
    if (!m_debugDataSpaces) return DebugResult::error("No data spaces interface");

    ULONG bw = 0;
    HRESULT hr = m_debugDataSpaces->WriteVirtual(address, const_cast<void*>(buffer),
                                                  static_cast<ULONG>(size), &bw);
    if (FAILED(hr)) {
        // Fallback: ASM kernel with VirtualProtect
        uint64_t asmBytesWritten = 0;
        uint32_t result = Dbg_WriteMemory(m_processHandle, address, buffer, size, &asmBytesWritten);
        if (result == 0) return DebugResult::ok("Memory written (ASM fallback)");
        return DebugResult::error("Failed to write memory", hr);
    }

    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_stats.totalMemoryWrites++;
    }

    return DebugResult::ok("Memory written");
}

DebugResult NativeDebuggerEngine::readString(uint64_t address, std::string& outStr, uint32_t maxLen) {
    std::vector<char> buf(maxLen + 1, 0);
    uint64_t br = 0;
    DebugResult r = readMemory(address, buf.data(), maxLen, &br);
    if (!r.success) return r;

    // Find null terminator
    buf[maxLen] = 0;
    outStr = buf.data();
    return DebugResult::ok("String read");
}

DebugResult NativeDebuggerEngine::readPointer(uint64_t address, uint64_t& outValue) {
    return readMemory(address, &outValue, sizeof(uint64_t));
}

std::string NativeDebuggerEngine::formatHexDump(uint64_t address, const void* data,
                                                  uint64_t size, uint32_t columns) const {
    std::ostringstream oss;
    const uint8_t* bytes = static_cast<const uint8_t*>(data);

    for (uint64_t i = 0; i < size; i += columns) {
        // Address
        oss << formatAddress(address + i) << "  ";

        // Hex bytes
        for (uint32_t j = 0; j < columns; j++) {
            if (i + j < size) {
                oss << std::hex << std::setfill('0') << std::setw(2)
                    << static_cast<unsigned>(bytes[i + j]) << " ";
            } else {
                oss << "   ";
            }
            if (j == columns / 2 - 1) oss << " "; // Mid separator
        }

        oss << " |";

        // ASCII
        for (uint32_t j = 0; j < columns && (i + j) < size; j++) {
            char c = static_cast<char>(bytes[i + j]);
            oss << (c >= 0x20 && c < 0x7F ? c : '.');
        }

        oss << "|\n";
    }

    return oss.str();
}

DebugResult NativeDebuggerEngine::queryMemoryRegions(std::vector<MemoryRegion>& outRegions) {
    if (!m_debugDataSpaces) return DebugResult::error("No data spaces interface");

    outRegions.clear();
    ULONG64 addr = 0;

    while (true) {
        MEMORY_BASIC_INFORMATION64 mbi = {};
        HRESULT hr = m_debugDataSpaces->QueryVirtual(addr, &mbi);
        if (FAILED(hr)) break;

        MemoryRegion region;
        region.baseAddress  = mbi.BaseAddress;
        region.size         = mbi.RegionSize;
        region.protection   = static_cast<MemoryProtection>(mbi.Protect);
        region.state        = mbi.State;
        region.type         = mbi.Type;

        outRegions.push_back(region);

        addr = mbi.BaseAddress + mbi.RegionSize;
        if (addr == 0) break; // Wrap-around guard
    }

    return DebugResult::ok("Memory regions queried");
}

DebugResult NativeDebuggerEngine::searchMemory(uint64_t startAddr, uint64_t size,
                                                const void* pattern, uint32_t patternLen,
                                                std::vector<uint64_t>& outMatches) {
    outMatches.clear();

    // Try ASM kernel pattern scan first
    uint64_t foundAddr = 0;
    uint64_t searchAddr = startAddr;
    uint64_t remaining = size;

    while (remaining > 0) {
        uint32_t result = Dbg_MemoryScan(m_processHandle, searchAddr, remaining,
                                          pattern, patternLen, &foundAddr);
        if (result != 0 || foundAddr == 0) break;

        outMatches.push_back(foundAddr);
        uint64_t advanced = (foundAddr - searchAddr) + 1;
        if (advanced >= remaining) break;
        searchAddr = foundAddr + 1;
        remaining -= advanced;
    }

    if (!outMatches.empty()) {
        return DebugResult::ok("Pattern found");
    }

    // Fallback: DbgEng search
    if (m_debugDataSpaces) {
        ULONG64 matchOffset = 0;
        HRESULT hr = m_debugDataSpaces->SearchVirtual(
            startAddr, size,
            const_cast<void*>(pattern), patternLen,
            1, // Granularity
            &matchOffset
        );
        if (SUCCEEDED(hr)) {
            outMatches.push_back(matchOffset);
            return DebugResult::ok("Pattern found (DbgEng)");
        }
    }

    return DebugResult::error("Pattern not found");
}

// =============================================================================
//                    Disassembly
// =============================================================================

DebugResult NativeDebuggerEngine::disassembleAt(uint64_t address, uint32_t lineCount,
                                                  std::vector<DisassembledInstruction>& outInstructions) {
    if (!m_debugControl) return DebugResult::error("No debug control interface");

    outInstructions.clear();
    ULONG64 currentAddr = address;

    for (uint32_t i = 0; i < lineCount; i++) {
        char disasmBuf[256] = {};
        ULONG disasmSize = 0;
        ULONG64 endOffset = 0;

        HRESULT hr = m_debugControl->Disassemble(
            currentAddr, 0,
            disasmBuf, sizeof(disasmBuf), &disasmSize,
            &endOffset
        );

        if (FAILED(hr)) break;

        DisassembledInstruction inst;
        inst.address    = currentAddr;
        inst.fullText   = disasmBuf;
        inst.length     = static_cast<uint32_t>(endOffset - currentAddr);

        // Parse mnemonic + operands from full text
        std::string text(disasmBuf);
        // Strip address prefix if present
        size_t spacePos = text.find(' ');
        if (spacePos != std::string::npos && spacePos < 20) {
            text = text.substr(spacePos);
        }
        // Trim leading whitespace
        size_t firstNonSpace = text.find_first_not_of(" \t");
        if (firstNonSpace != std::string::npos) {
            text = text.substr(firstNonSpace);
        }

        size_t mnemonicEnd = text.find_first_of(" \t");
        if (mnemonicEnd != std::string::npos) {
            inst.mnemonic = text.substr(0, mnemonicEnd);
            size_t opStart = text.find_first_not_of(" \t", mnemonicEnd);
            if (opStart != std::string::npos) {
                inst.operands = text.substr(opStart);
                // Trim trailing whitespace/newline
                while (!inst.operands.empty() &&
                       (inst.operands.back() == '\n' || inst.operands.back() == '\r' ||
                        inst.operands.back() == ' ')) {
                    inst.operands.pop_back();
                }
            }
        } else {
            inst.mnemonic = text;
        }

        // Classify instruction
        std::string mnLower = inst.mnemonic;
        std::transform(mnLower.begin(), mnLower.end(), mnLower.begin(), ::tolower);
        inst.isCall     = (mnLower == "call");
        inst.isJump     = (mnLower.size() >= 1 && mnLower[0] == 'j');
        inst.isReturn   = (mnLower == "ret" || mnLower == "retn");

        // Check if this address has a breakpoint
        {
            std::lock_guard<std::mutex> lock(m_bpMutex);
            inst.hasBreakpoint = std::any_of(m_breakpoints.begin(), m_breakpoints.end(),
                [currentAddr](const NativeBreakpoint& bp) {
                    return bp.address == currentAddr && bp.state == BreakpointState::Enabled;
                });
        }

        // Read instruction bytes
        uint8_t instrBytes[16] = {};
        ULONG br = 0;
        if (m_debugDataSpaces) {
            m_debugDataSpaces->ReadVirtual(currentAddr, instrBytes, inst.length, &br);
        }
        std::ostringstream bytesOss;
        for (uint32_t b = 0; b < inst.length && b < 16; b++) {
            bytesOss << std::hex << std::setfill('0') << std::setw(2)
                     << static_cast<unsigned>(instrBytes[b]);
            if (b < inst.length - 1) bytesOss << " ";
        }
        inst.bytes = bytesOss.str();

        // Resolve symbol at address
        char symBuf[256] = {};
        ULONG symSize = 0;
        ULONG64 disp = 0;
        if (m_debugSymbols) {
            HRESULT symHr = m_debugSymbols->GetNameByOffset(currentAddr, symBuf, sizeof(symBuf),
                                                             &symSize, &disp);
            if (SUCCEEDED(symHr) && disp == 0) {
                inst.symbol = symBuf;
            }
        }

        outInstructions.push_back(inst);
        currentAddr = endOffset;
    }

    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_stats.totalDisassembled += outInstructions.size();
    }

    return DebugResult::ok("Disassembly complete");
}

DebugResult NativeDebuggerEngine::disassembleFunction(const std::string& symbol,
                                                        std::vector<DisassembledInstruction>& outInstructions) {
    uint64_t address = 0;
    DebugResult r = resolveAddress(symbol, address);
    if (!r.success) return r;

    // Disassemble up to 512 instructions or until ret
    return disassembleAt(address, 512, outInstructions);
}

DebugResult NativeDebuggerEngine::setDisasmSyntax(DisasmSyntax syntax) {
    m_currentSyntax = syntax;

    if (m_debugControl) {
        ULONG options = 0;
        m_debugControl->GetAssemblyOptions(&options);

        switch (syntax) {
            case DisasmSyntax::Intel:
                // Default for DbgEng
                break;
            case DisasmSyntax::ATT:
                // Not directly supported by DbgEng — would need a wrapper
                break;
            case DisasmSyntax::MASM:
                // Similar to Intel in DbgEng
                break;
        }
    }

    return DebugResult::ok("Disassembly syntax updated");
}

// =============================================================================
//                    Symbol Resolution
// =============================================================================

DebugResult NativeDebuggerEngine::resolveSymbol(uint64_t address, std::string& outSymbol,
                                                 uint64_t& outDisplacement) {
    if (!m_debugSymbols) return DebugResult::error("No symbol interface");

    char nameBuf[512] = {};
    ULONG nameSize = 0;
    ULONG64 displacement = 0;

    HRESULT hr = m_debugSymbols->GetNameByOffset(
        address, nameBuf, sizeof(nameBuf), &nameSize, &displacement);

    if (FAILED(hr)) {
        outSymbol = formatAddress(address);
        outDisplacement = 0;
        return DebugResult::error("Symbol not found");
    }

    outSymbol = nameBuf;
    outDisplacement = displacement;
    return DebugResult::ok("Symbol resolved");
}

DebugResult NativeDebuggerEngine::resolveAddress(const std::string& symbol, uint64_t& outAddress) {
    if (!m_debugSymbols) return DebugResult::error("No symbol interface");

    ULONG64 offset = 0;
    HRESULT hr = m_debugSymbols->GetOffsetByName(symbol.c_str(), &offset);
    if (FAILED(hr)) return DebugResult::error("Cannot resolve symbol to address");

    outAddress = offset;
    return DebugResult::ok("Address resolved");
}

DebugResult NativeDebuggerEngine::loadSymbols(const std::string& moduleName) {
    if (!m_debugSymbols) return DebugResult::error("No symbol interface");

    std::string cmd = ".reload /f " + moduleName;
    HRESULT hr = m_debugControl->Execute(DEBUG_OUTCTL_ALL_CLIENTS, cmd.c_str(), DEBUG_EXECUTE_DEFAULT);
    if (FAILED(hr)) return DebugResult::error("Failed to reload symbols");

    return DebugResult::ok("Symbols loaded");
}

DebugResult NativeDebuggerEngine::setSymbolPath(const std::string& path) {
    if (!m_debugSymbols) return DebugResult::error("No symbol interface");

    HRESULT hr = m_debugSymbols->SetSymbolPath(path.c_str());
    if (FAILED(hr)) return DebugResult::error("Failed to set symbol path");

    m_config.symbolPath = path;
    return DebugResult::ok("Symbol path set");
}

// =============================================================================
//                    Expression Evaluation
// =============================================================================

DebugResult NativeDebuggerEngine::evaluate(const std::string& expression, EvalResult& outResult) {
    if (!m_debugControl) return DebugResult::error("No debug control interface");

    outResult.expression = expression;

    DEBUG_VALUE val = {};
    ULONG remainderIndex = 0;

    HRESULT hr = m_debugControl->Evaluate(
        expression.c_str(),
        DEBUG_VALUE_INT64,
        &val,
        &remainderIndex
    );

    if (FAILED(hr)) {
        outResult.success = false;
        outResult.value = "<error>";
        return DebugResult::error("Expression evaluation failed");
    }

    outResult.success   = true;
    outResult.rawValue  = val.I64;

    std::ostringstream oss;
    oss << "0x" << std::hex << val.I64 << " (" << std::dec << static_cast<int64_t>(val.I64) << ")";
    outResult.value = oss.str();
    outResult.type = "int64";

    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_stats.totalEvalsPerformed++;
    }

    return DebugResult::ok("Expression evaluated");
}

// =============================================================================
//                    Watch Management
// =============================================================================

uint32_t NativeDebuggerEngine::addWatch(const std::string& expression) {
    NativeWatch w;
    w.id            = m_nextWatchId.fetch_add(1);
    w.expression    = expression;
    w.enabled       = true;

    {
        std::lock_guard<std::mutex> lock(m_watchMutex);
        m_watches.push_back(w);
    }

    // Immediately evaluate
    updateWatches();

    return w.id;
}

DebugResult NativeDebuggerEngine::removeWatch(uint32_t watchId) {
    std::lock_guard<std::mutex> lock(m_watchMutex);
    auto it = std::find_if(m_watches.begin(), m_watches.end(),
        [watchId](const NativeWatch& w) { return w.id == watchId; });
    if (it == m_watches.end()) return DebugResult::error("Watch not found");
    m_watches.erase(it);
    return DebugResult::ok("Watch removed");
}

DebugResult NativeDebuggerEngine::updateWatches() {
    std::lock_guard<std::mutex> lock(m_watchMutex);
    for (auto& w : m_watches) {
        if (!w.enabled) continue;
        evaluate(w.expression, w.lastResult);
        w.updateCount++;
    }
    return DebugResult::ok("Watches updated");
}

// =============================================================================
//                    Module Enumeration
// =============================================================================

DebugResult NativeDebuggerEngine::enumerateModules(std::vector<DebugModule>& outModules) {
    if (!m_debugSymbols) return DebugResult::error("No symbol interface");

    outModules.clear();

    ULONG loaded = 0, unloaded = 0;
    m_debugSymbols->GetNumberModules(&loaded, &unloaded);

    for (ULONG i = 0; i < loaded; i++) {
        ULONG64 base = 0;
        m_debugSymbols->GetModuleByIndex(i, &base);

        DEBUG_MODULE_PARAMETERS params = {};
        m_debugSymbols->GetModuleParameters(1, &base, 0, &params);

        char nameBuf[256] = {};
        char pathBuf[MAX_PATH] = {};
        ULONG nameSize = 0, pathSize = 0;
        m_debugSymbols->GetModuleNames(i, 0,
            pathBuf, sizeof(pathBuf), &pathSize,
            nameBuf, sizeof(nameBuf), &nameSize,
            nullptr, 0, nullptr);

        DebugModule mod;
        mod.name            = nameBuf;
        mod.path            = pathBuf;
        mod.baseAddress     = base;
        mod.size            = params.Size;
        mod.timestamp       = params.TimeDateStamp;
        mod.checksum        = params.Checksum;
        mod.symbolsLoaded   = (params.SymbolType != DEBUG_SYMTYPE_NONE &&
                               params.SymbolType != DEBUG_SYMTYPE_DEFERRED);

        outModules.push_back(mod);
    }

    {
        std::lock_guard<std::mutex> lock(m_moduleMutex);
        m_modules = outModules;
    }

    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_stats.modulesLoaded = outModules.size();
    }

    return DebugResult::ok("Modules enumerated");
}

const DebugModule* NativeDebuggerEngine::findModuleByAddress(uint64_t address) const {
    std::lock_guard<std::mutex> lock(m_moduleMutex);
    for (const auto& mod : m_modules) {
        if (address >= mod.baseAddress && address < mod.baseAddress + mod.size) {
            return &mod;
        }
    }
    return nullptr;
}

const DebugModule* NativeDebuggerEngine::findModuleByName(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_moduleMutex);
    for (const auto& mod : m_modules) {
        if (mod.name == name) return &mod;
    }
    return nullptr;
}

// =============================================================================
//                    Thread Enumeration
// =============================================================================

DebugResult NativeDebuggerEngine::enumerateThreads(std::vector<DebugThread>& outThreads) {
    if (!m_debugSysObjects) return DebugResult::error("No system objects interface");

    outThreads.clear();

    ULONG numThreads = 0;
    m_debugSysObjects->GetNumberThreads(&numThreads);

    ULONG currentId = 0;
    m_debugSysObjects->GetCurrentThreadSystemId(&currentId);

    for (ULONG i = 0; i < numThreads; i++) {
        ULONG engineId = 0;
        ULONG systemId = 0;
        m_debugSysObjects->GetThreadIdsByIndex(i, 1, &engineId, &systemId);

        DebugThread t;
        t.threadId  = systemId;
        t.isCurrent = (systemId == currentId);

        outThreads.push_back(t);
    }

    {
        std::lock_guard<std::mutex> lock(m_threadMutex);
        m_threads = outThreads;
    }

    return DebugResult::ok("Threads enumerated");
}

DebugResult NativeDebuggerEngine::switchThread(uint32_t threadId) {
    if (!m_debugSysObjects) return DebugResult::error("No system objects interface");

    HRESULT hr = m_debugSysObjects->SetCurrentThreadId(threadId);
    if (FAILED(hr)) return DebugResult::error("Failed to switch thread", hr);

    m_currentThreadId = threadId;
    return DebugResult::ok("Switched to thread");
}

// =============================================================================
//                    Event History
// =============================================================================

const DebugEvent* NativeDebuggerEngine::getLastEvent() const {
    std::lock_guard<std::mutex> lock(m_historyMutex);
    if (m_eventHistory.empty()) return nullptr;
    return &m_eventHistory.back();
}

void NativeDebuggerEngine::clearEventHistory() {
    std::lock_guard<std::mutex> lock(m_historyMutex);
    m_eventHistory.clear();
}

// =============================================================================
//                    Callbacks
// =============================================================================

void NativeDebuggerEngine::setEventCallback(DebugEventCallback cb, void* userData) {
    m_eventCallback = cb;
    m_eventCallbackData = userData;
}

void NativeDebuggerEngine::setOutputCallback(DebugOutputCallback cb, void* userData) {
    m_outputCallback = cb;
    m_outputCallbackData = userData;
}

void NativeDebuggerEngine::setStateCallback(DebugStateCallback cb, void* userData) {
    m_stateCallback = cb;
    m_stateCallbackData = userData;
}

void NativeDebuggerEngine::setBreakpointHitCallback(BreakpointHitCallback cb, void* userData) {
    m_bpHitCallback = cb;
    m_bpHitCallbackData = userData;
}

// =============================================================================
//                    Statistics
// =============================================================================

DebugSessionStats NativeDebuggerEngine::getStats() const {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    return m_stats;
}

void NativeDebuggerEngine::resetStats() {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    uint64_t sessions = m_stats.sessionsStarted; // preserve
    m_stats = DebugSessionStats{};
    m_stats.sessionsStarted = sessions;
}

void NativeDebuggerEngine::updateConfig(const DebugConfig& newConfig) {
    std::lock_guard<std::mutex> lock(m_configMutex);
    m_config = newConfig;
}

// =============================================================================
//                    Internal: Event Loop
// =============================================================================

void NativeDebuggerEngine::eventLoopThread() {
    while (m_eventLoopRunning.load()) {
        if (!m_debugControl) break;

        HRESULT hr = m_debugControl->WaitForEvent(DEBUG_WAIT_DEFAULT, 100); // 100ms poll
        if (hr == S_OK) {
            // Event received — determine what happened
            ULONG type = 0, procId = 0, threadId = 0;
            m_debugControl->GetLastEventInformation(
                &type, &procId, &threadId,
                nullptr, 0, nullptr,
                nullptr, 0, nullptr
            );

            DebugEvent event;
            event.timestamp = GetTickCount64();
            event.processId = procId;
            event.threadId  = threadId;

            switch (type) {
                case DEBUG_EVENT_BREAKPOINT:
                    event.type = DebugEventType::Breakpoint;
                    event.description = "Breakpoint hit";
                    transitionState(DebugSessionState::Broken);
                    break;
                case DEBUG_EVENT_EXCEPTION:
                    event.type = DebugEventType::Exception;
                    event.description = "Exception occurred";
                    transitionState(DebugSessionState::Broken);
                    break;
                case DEBUG_EVENT_CREATE_THREAD:
                    event.type = DebugEventType::CreateThread;
                    event.description = "Thread created";
                    break;
                case DEBUG_EVENT_EXIT_THREAD:
                    event.type = DebugEventType::ExitThread;
                    event.description = "Thread exited";
                    break;
                case DEBUG_EVENT_CREATE_PROCESS:
                    event.type = DebugEventType::CreateProcess;
                    event.description = "Process created";
                    break;
                case DEBUG_EVENT_EXIT_PROCESS:
                    event.type = DebugEventType::ExitProcess;
                    event.description = "Process exited";
                    transitionState(DebugSessionState::Terminated);
                    m_eventLoopRunning.store(false);
                    break;
                case DEBUG_EVENT_LOAD_MODULE:
                    event.type = DebugEventType::LoadDLL;
                    event.description = "Module loaded";
                    break;
                case DEBUG_EVENT_UNLOAD_MODULE:
                    event.type = DebugEventType::UnloadDLL;
                    event.description = "Module unloaded";
                    break;
                default:
                    event.type = DebugEventType::None;
                    break;
            }

            // Capture registers on break
            if (event.type == DebugEventType::Breakpoint ||
                event.type == DebugEventType::Exception) {
                captureRegisters(event.registers);
                walkStack(event.callStack);
                event.address = event.registers.rip;

                // Update watches
                updateWatches();

                {
                    std::lock_guard<std::mutex> lock(m_statsMutex);
                    if (event.type == DebugEventType::Breakpoint) {
                        m_stats.totalBreakpointHits++;
                    } else {
                        m_stats.totalExceptions++;
                    }
                }
            }

            processDebugEvent(event);
        }
    }
}

void NativeDebuggerEngine::processDebugEvent(const DebugEvent& event) {
    // Add to history
    {
        std::lock_guard<std::mutex> lock(m_historyMutex);
        m_eventHistory.push_back(event);
        while (m_eventHistory.size() > m_config.maxEventHistory) {
            m_eventHistory.pop_front();
        }
    }

    // Fire callback
    if (m_eventCallback) {
        m_eventCallback(&event, m_eventCallbackData);
    }

    // Fire breakpoint-specific callback
    if (event.type == DebugEventType::Breakpoint && m_bpHitCallback) {
        std::lock_guard<std::mutex> lock(m_bpMutex);
        for (auto& bp : m_breakpoints) {
            if (bp.address == event.address && bp.state == BreakpointState::Enabled) {
                bp.hitCount++;
                bp.state = BreakpointState::Hit;
                m_bpHitCallback(&bp, &event.registers, m_bpHitCallbackData);

                // Auto-delete one-shot breakpoints
                if (bp.autoDelete) {
                    if (bp.type == BreakpointType::Software) {
                        removeSoftwareBreakpoint(bp);
                    } else {
                        removeHardwareBreakpoint(bp);
                    }
                }
                break;
            }
        }
    }
}

void NativeDebuggerEngine::transitionState(DebugSessionState newState) {
    DebugSessionState oldState = m_state.exchange(newState);
    if (oldState != newState && m_stateCallback) {
        m_stateCallback(newState, m_stateCallbackData);
    }
}

// =============================================================================
//                    Internal: Symbol Helpers
// =============================================================================

void NativeDebuggerEngine::onModuleLoad(const DebugModule& mod) {
    {
        std::lock_guard<std::mutex> lock(m_moduleMutex);
        m_modules.push_back(mod);
    }

    // Resolve pending breakpoints
    std::lock_guard<std::mutex> lock(m_bpMutex);
    for (auto& bp : m_breakpoints) {
        if (bp.state != BreakpointState::Pending) continue;

        if (!bp.symbol.empty()) {
            uint64_t addr = 0;
            if (resolveAddress(bp.symbol, addr).success) {
                bp.address = addr;
                if (bp.type == BreakpointType::Hardware || bp.type == BreakpointType::DataWatch) {
                    applyHardwareBreakpoint(bp);
                } else {
                    applySoftwareBreakpoint(bp);
                }
            }
        }
    }
}

void NativeDebuggerEngine::onModuleUnload(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_moduleMutex);
    m_modules.erase(
        std::remove_if(m_modules.begin(), m_modules.end(),
            [&name](const DebugModule& mod) { return mod.name == name; }),
        m_modules.end()
    );
}

// =============================================================================
//                    Internal: Formatting
// =============================================================================

std::string NativeDebuggerEngine::formatAddress(uint64_t addr) const {
    std::ostringstream oss;
    oss << "0x" << std::hex << std::setfill('0') << std::setw(16) << std::uppercase << addr;
    return oss.str();
}

std::string NativeDebuggerEngine::formatExceptionCode(uint32_t code) const {
    switch (code) {
        case 0x80000003: return "EXCEPTION_BREAKPOINT";
        case 0x80000004: return "EXCEPTION_SINGLE_STEP";
        case 0xC0000005: return "EXCEPTION_ACCESS_VIOLATION";
        case 0xC00000FD: return "EXCEPTION_STACK_OVERFLOW";
        case 0xC0000094: return "EXCEPTION_INT_DIVIDE_BY_ZERO";
        case 0xC000001D: return "EXCEPTION_ILLEGAL_INSTRUCTION";
        case 0xC0000096: return "EXCEPTION_PRIV_INSTRUCTION";
        case 0xC00000C9: return "EXCEPTION_STACK_BUFFER_OVERRUN";
        case 0xC0000409: return "STATUS_STACK_BUFFER_OVERRUN";
        default: {
            std::ostringstream oss;
            oss << "0x" << std::hex << std::setfill('0') << std::setw(8) << code;
            return oss.str();
        }
    }
}

// =============================================================================
//                    JSON Serialization
// =============================================================================

std::string NativeDebuggerEngine::toJsonStatus() const {
    std::ostringstream j;
    j << "{";
    j << "\"initialized\":" << (m_initialized.load() ? "true" : "false") << ",";
    j << "\"state\":\"" << static_cast<uint32_t>(m_state.load()) << "\",";
    j << "\"targetName\":\"" << m_targetName << "\",";
    j << "\"targetPID\":" << m_targetPID << ",";
    j << "\"breakpoints\":" << m_breakpoints.size() << ",";
    j << "\"watches\":" << m_watches.size() << ",";
    j << "\"modules\":" << m_modules.size() << ",";
    j << "\"threads\":" << m_threads.size() << ",";
    j << "\"events\":" << m_eventHistory.size();

    // Stats
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        j << ",\"stats\":{";
        j << "\"breakpointHits\":" << m_stats.totalBreakpointHits << ",";
        j << "\"exceptions\":" << m_stats.totalExceptions << ",";
        j << "\"steps\":" << m_stats.totalSteps << ",";
        j << "\"memoryReads\":" << m_stats.totalMemoryReads << ",";
        j << "\"memoryWrites\":" << m_stats.totalMemoryWrites << ",";
        j << "\"disassembled\":" << m_stats.totalDisassembled << ",";
        j << "\"evals\":" << m_stats.totalEvalsPerformed << ",";
        j << "\"sessions\":" << m_stats.sessionsStarted;
        j << "}";
    }

    j << "}";
    return j.str();
}

std::string NativeDebuggerEngine::toJsonBreakpoints() const {
    std::lock_guard<std::mutex> lock(m_bpMutex);
    std::ostringstream j;
    j << "[";
    for (size_t i = 0; i < m_breakpoints.size(); i++) {
        const auto& bp = m_breakpoints[i];
        if (i > 0) j << ",";
        j << "{";
        j << "\"id\":" << bp.id << ",";
        j << "\"type\":" << static_cast<uint32_t>(bp.type) << ",";
        j << "\"state\":" << static_cast<uint32_t>(bp.state) << ",";
        j << "\"address\":\"0x" << std::hex << bp.address << std::dec << "\",";
        j << "\"symbol\":\"" << bp.symbol << "\",";
        j << "\"sourceFile\":\"" << bp.sourceFile << "\",";
        j << "\"sourceLine\":" << bp.sourceLine << ",";
        j << "\"hitCount\":" << bp.hitCount << ",";
        j << "\"condition\":\"" << bp.condition << "\"";
        j << "}";
    }
    j << "]";
    return j.str();
}

std::string NativeDebuggerEngine::toJsonRegisters() const {
    RegisterSnapshot snap;
    const_cast<NativeDebuggerEngine*>(this)->captureRegisters(snap);

    std::ostringstream j;
    j << std::hex;
    j << "{";
    j << "\"rax\":\"0x" << snap.rax << "\",";
    j << "\"rbx\":\"0x" << snap.rbx << "\",";
    j << "\"rcx\":\"0x" << snap.rcx << "\",";
    j << "\"rdx\":\"0x" << snap.rdx << "\",";
    j << "\"rsi\":\"0x" << snap.rsi << "\",";
    j << "\"rdi\":\"0x" << snap.rdi << "\",";
    j << "\"rbp\":\"0x" << snap.rbp << "\",";
    j << "\"rsp\":\"0x" << snap.rsp << "\",";
    j << "\"r8\":\"0x" << snap.r8 << "\",";
    j << "\"r9\":\"0x" << snap.r9 << "\",";
    j << "\"r10\":\"0x" << snap.r10 << "\",";
    j << "\"r11\":\"0x" << snap.r11 << "\",";
    j << "\"r12\":\"0x" << snap.r12 << "\",";
    j << "\"r13\":\"0x" << snap.r13 << "\",";
    j << "\"r14\":\"0x" << snap.r14 << "\",";
    j << "\"r15\":\"0x" << snap.r15 << "\",";
    j << "\"rip\":\"0x" << snap.rip << "\",";
    j << "\"rflags\":\"0x" << snap.rflags << "\",";
    j << "\"flags\":\"" << formatFlags(snap.rflags) << "\"";
    j << "}";
    return j.str();
}

std::string NativeDebuggerEngine::toJsonStack() const {
    std::vector<NativeStackFrame> frames;
    const_cast<NativeDebuggerEngine*>(this)->walkStack(frames);

    std::ostringstream j;
    j << "[";
    for (size_t i = 0; i < frames.size(); i++) {
        if (i > 0) j << ",";
        const auto& f = frames[i];
        j << "{";
        j << "\"index\":" << f.frameIndex << ",";
        j << "\"ip\":\"0x" << std::hex << f.instructionPtr << std::dec << "\",";
        j << "\"function\":\"" << f.function << "\",";
        j << "\"module\":\"" << f.module << "\",";
        j << "\"sourceFile\":\"" << f.sourceFile << "\",";
        j << "\"sourceLine\":" << f.sourceLine << ",";
        j << "\"displacement\":" << f.displacement;
        j << "}";
    }
    j << "]";
    return j.str();
}

std::string NativeDebuggerEngine::toJsonModules() const {
    std::lock_guard<std::mutex> lock(m_moduleMutex);
    std::ostringstream j;
    j << "[";
    for (size_t i = 0; i < m_modules.size(); i++) {
        if (i > 0) j << ",";
        const auto& mod = m_modules[i];
        j << "{";
        j << "\"name\":\"" << mod.name << "\",";
        j << "\"path\":\"" << mod.path << "\",";
        j << "\"base\":\"0x" << std::hex << mod.baseAddress << std::dec << "\",";
        j << "\"size\":" << mod.size << ",";
        j << "\"symbolsLoaded\":" << (mod.symbolsLoaded ? "true" : "false");
        j << "}";
    }
    j << "]";
    return j.str();
}

std::string NativeDebuggerEngine::toJsonThreads() const {
    std::lock_guard<std::mutex> lock(m_threadMutex);
    std::ostringstream j;
    j << "[";
    for (size_t i = 0; i < m_threads.size(); i++) {
        if (i > 0) j << ",";
        const auto& t = m_threads[i];
        j << "{";
        j << "\"id\":" << t.threadId << ",";
        j << "\"name\":\"" << t.name << "\",";
        j << "\"isCurrent\":" << (t.isCurrent ? "true" : "false") << ",";
        j << "\"isSuspended\":" << (t.isSuspended ? "true" : "false");
        j << "}";
    }
    j << "]";
    return j.str();
}

std::string NativeDebuggerEngine::toJsonDisassembly(uint64_t address, uint32_t lines) const {
    std::vector<DisassembledInstruction> insts;
    const_cast<NativeDebuggerEngine*>(this)->disassembleAt(address, lines, insts);

    std::ostringstream j;
    j << "[";
    for (size_t i = 0; i < insts.size(); i++) {
        if (i > 0) j << ",";
        const auto& inst = insts[i];
        j << "{";
        j << "\"address\":\"0x" << std::hex << inst.address << std::dec << "\",";
        j << "\"mnemonic\":\"" << inst.mnemonic << "\",";
        j << "\"operands\":\"" << inst.operands << "\",";
        j << "\"bytes\":\"" << inst.bytes << "\",";
        j << "\"length\":" << inst.length << ",";
        j << "\"hasBreakpoint\":" << (inst.hasBreakpoint ? "true" : "false") << ",";
        j << "\"isCall\":" << (inst.isCall ? "true" : "false") << ",";
        j << "\"isJump\":" << (inst.isJump ? "true" : "false") << ",";
        j << "\"isReturn\":" << (inst.isReturn ? "true" : "false");
        if (!inst.symbol.empty()) {
            j << ",\"symbol\":\"" << inst.symbol << "\"";
        }
        j << "}";
    }
    j << "]";
    return j.str();
}

std::string NativeDebuggerEngine::toJsonMemory(uint64_t address, uint64_t size) const {
    std::vector<uint8_t> buf(size);
    uint64_t br = 0;
    const_cast<NativeDebuggerEngine*>(this)->readMemory(address, buf.data(), size, &br);

    std::ostringstream j;
    j << "{";
    j << "\"address\":\"0x" << std::hex << address << std::dec << "\",";
    j << "\"size\":" << br << ",";
    j << "\"hex\":\"";
    for (uint64_t i = 0; i < br; i++) {
        j << std::hex << std::setfill('0') << std::setw(2) << static_cast<unsigned>(buf[i]);
    }
    j << "\",\"ascii\":\"";
    for (uint64_t i = 0; i < br; i++) {
        char c = static_cast<char>(buf[i]);
        j << (c >= 0x20 && c < 0x7F ? c : '.');
    }
    j << "\"}";
    return j.str();
}

std::string NativeDebuggerEngine::toJsonEvents(uint32_t lastN) const {
    std::lock_guard<std::mutex> lock(m_historyMutex);

    std::ostringstream j;
    j << "[";
    size_t start = (m_eventHistory.size() > lastN) ? m_eventHistory.size() - lastN : 0;
    bool first = true;
    for (size_t i = start; i < m_eventHistory.size(); i++) {
        if (!first) j << ",";
        first = false;
        const auto& evt = m_eventHistory[i];
        j << "{";
        j << "\"type\":" << static_cast<uint32_t>(evt.type) << ",";
        j << "\"timestamp\":" << evt.timestamp << ",";
        j << "\"processId\":" << evt.processId << ",";
        j << "\"threadId\":" << evt.threadId << ",";
        j << "\"address\":\"0x" << std::hex << evt.address << std::dec << "\",";
        j << "\"description\":\"" << evt.description << "\"";
        j << "}";
    }
    j << "]";
    return j.str();
}

std::string NativeDebuggerEngine::toJsonWatches() const {
    std::lock_guard<std::mutex> lock(m_watchMutex);
    std::ostringstream j;
    j << "[";
    for (size_t i = 0; i < m_watches.size(); i++) {
        if (i > 0) j << ",";
        const auto& w = m_watches[i];
        j << "{";
        j << "\"id\":" << w.id << ",";
        j << "\"expression\":\"" << w.expression << "\",";
        j << "\"value\":\"" << w.lastResult.value << "\",";
        j << "\"type\":\"" << w.lastResult.type << "\",";
        j << "\"enabled\":" << (w.enabled ? "true" : "false") << ",";
        j << "\"updateCount\":" << w.updateCount;
        j << "}";
    }
    j << "]";
    return j.str();
}

#endif // _WIN32
