#if !defined(_MSC_VER)

#include "native_debugger_engine.h"

#include <windows.h>

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace RawrXD::Debugger {

NativeDebuggerEngine::NativeDebuggerEngine() = default;
NativeDebuggerEngine::~NativeDebuggerEngine() = default;

NativeDebuggerEngine& NativeDebuggerEngine::Instance() {
    static NativeDebuggerEngine s_instance;
    return s_instance;
}

DebugResult NativeDebuggerEngine::enableBreakpoint(uint32_t bpId, bool enable) {
    std::lock_guard<std::mutex> lock(m_bpMutex);
    for (auto& bp : m_breakpoints) {
        if (bp.id == bpId) {
            bp.state = enable ? BreakpointState::Enabled : BreakpointState::Disabled;
            return DebugResult::ok(enable ? "Breakpoint enabled" : "Breakpoint disabled");
        }
    }
    return DebugResult::error("Breakpoint not found", -1);
}

DebugResult NativeDebuggerEngine::launchProcess(const std::string& exePath,
    const std::string&, const std::string&) {
    if (exePath.empty()) {
        return DebugResult::error("Executable path is empty", -1);
    }

    {
        std::lock_guard<std::mutex> cfgLock(m_configMutex);
        m_initialized.store(true, std::memory_order_release);
    }

    m_targetPath = exePath;
    const size_t slash = exePath.find_last_of("/\\");
    m_targetName = (slash == std::string::npos) ? exePath : exePath.substr(slash + 1);
    m_targetPID = static_cast<uint32_t>(::GetCurrentProcessId());
    m_state.store(DebugSessionState::Running, std::memory_order_release);
    if (m_stateCallback) {
        m_stateCallback(DebugSessionState::Running, m_stateCallbackData);
    }
    return DebugResult::ok("Launch simulated in non-MSVC fallback");
}

DebugResult NativeDebuggerEngine::initialize(const DebugConfig& config) {
    {
        std::lock_guard<std::mutex> lock(m_configMutex);
        m_config = config;
    }
    m_initialized.store(true, std::memory_order_release);
    m_state.store(DebugSessionState::Idle, std::memory_order_release);
    if (m_stateCallback) {
        m_stateCallback(DebugSessionState::Idle, m_stateCallbackData);
    }
    return DebugResult::ok("Native debugger initialized in non-MSVC fallback");
}

DebugResult NativeDebuggerEngine::disassembleAt(uint64_t address, uint32_t lineCount,
    std::vector<DisassembledInstruction>& outInstructions) {
    outInstructions.clear();
    if (lineCount == 0) {
        return DebugResult::error("lineCount must be > 0", -1);
    }

    outInstructions.reserve(lineCount);
    for (uint32_t i = 0; i < lineCount; ++i) {
        DisassembledInstruction inst;
        inst.address = address + i;
        inst.mnemonic = "db";
        inst.operands = "0x90";
        inst.fullText = "db 0x90";
        inst.bytes = "90";
        inst.length = 1;
        inst.isCurrentIP = (i == 0);
        outInstructions.push_back(std::move(inst));
    }
    return DebugResult::ok("Disassembly fallback generated");
}

DebugResult NativeDebuggerEngine::addBreakpointBySourceLine(const std::string& file, int line) {
    if (file.empty() || line <= 0) {
        return DebugResult::error("Invalid source location for breakpoint", -1);
    }

    NativeBreakpoint bp;
    bp.id = m_nextBpId.fetch_add(1, std::memory_order_acq_rel);
    bp.type = BreakpointType::Software;
    bp.state = BreakpointState::Enabled;
    bp.sourceFile = file;
    bp.sourceLine = line;

    {
        std::lock_guard<std::mutex> lock(m_bpMutex);
        m_breakpoints.push_back(std::move(bp));
    }
    return DebugResult::ok("Source-line breakpoint registered");
}

DebugResult NativeDebuggerEngine::addBreakpointBySymbol(const std::string& symbol,
                                                        BreakpointType type) {
    if (symbol.empty()) {
        return DebugResult::error("Symbol name is empty", -1);
    }

    NativeBreakpoint bp;
    bp.id = m_nextBpId.fetch_add(1, std::memory_order_acq_rel);
    bp.type = type;
    bp.state = BreakpointState::Enabled;
    bp.symbol = symbol;

    {
        std::lock_guard<std::mutex> lock(m_bpMutex);
        m_breakpoints.push_back(std::move(bp));
    }
    return DebugResult::ok("Symbol breakpoint registered");
}

DebugResult NativeDebuggerEngine::removeBreakpoint(uint32_t bpId) {
    std::lock_guard<std::mutex> lock(m_bpMutex);
    const auto before = m_breakpoints.size();
    m_breakpoints.erase(
        std::remove_if(
            m_breakpoints.begin(),
            m_breakpoints.end(),
            [bpId](const NativeBreakpoint& bp) { return bp.id == bpId; }),
        m_breakpoints.end());
    if (m_breakpoints.size() == before) {
        return DebugResult::error("Breakpoint not found", -1);
    }
    return DebugResult::ok("Breakpoint removed");
}

uint32_t NativeDebuggerEngine::addWatch(const std::string& expression) {
    if (expression.empty()) {
        return 0;
    }

    NativeWatch watch;
    watch.id = m_nextWatchId.fetch_add(1, std::memory_order_acq_rel);
    watch.expression = expression;
    watch.enabled = true;
    watch.lastResult.success = false;
    watch.lastResult.expression = expression;
    watch.lastResult.value = "<pending>";
    watch.lastResult.type = "native-debugger-disabled";
    watch.lastResult.rawValue = 0;
    watch.lastResult.isPointer = false;
    watch.lastResult.isFloat = false;

    std::lock_guard<std::mutex> lock(m_watchMutex);
    m_watches.push_back(std::move(watch));
    return m_watches.back().id;
}

DebugResult NativeDebuggerEngine::walkStack(std::vector<NativeStackFrame>& outFrames, uint32_t maxFrames) {
    outFrames.clear();
    if (maxFrames == 0) {
        return DebugResult::error("maxFrames must be > 0", -1);
    }

    NativeStackFrame frame;
    frame.frameIndex = 0;
    frame.module = m_targetName.empty() ? "unknown-module" : m_targetName;
    frame.function = "stack-unavailable";
    frame.sourceFile = "n/a";
    frame.sourceLine = 0;
    frame.hasSource = false;
    outFrames.push_back(std::move(frame));
    return DebugResult::ok("Stack walk unavailable; returned placeholder frame");
}

DebugResult NativeDebuggerEngine::getFrameLocals(uint32_t, std::map<std::string, std::string>& outLocals) {
    outLocals.clear();
    outLocals.emplace("native_debugger", "disabled_on_non_msvc");
    return DebugResult::ok("Frame locals unavailable; returned diagnostic marker");
}

DebugResult NativeDebuggerEngine::stepOut() {
    m_state.store(DebugSessionState::Stepping, std::memory_order_release);
    if (m_stateCallback) {
        m_stateCallback(DebugSessionState::Stepping, m_stateCallbackData);
    }
    return DebugResult::ok("StepOut accepted (non-MSVC fallback)");
}

DebugResult NativeDebuggerEngine::stepInto() {
    m_state.store(DebugSessionState::Stepping, std::memory_order_release);
    if (m_stateCallback) {
        m_stateCallback(DebugSessionState::Stepping, m_stateCallbackData);
    }
    return DebugResult::ok("StepInto accepted (non-MSVC fallback)");
}

DebugResult NativeDebuggerEngine::stepOver() {
    m_state.store(DebugSessionState::Stepping, std::memory_order_release);
    if (m_stateCallback) {
        m_stateCallback(DebugSessionState::Stepping, m_stateCallbackData);
    }
    return DebugResult::ok("StepOver accepted (non-MSVC fallback)");
}

DebugResult NativeDebuggerEngine::go() {
    m_state.store(DebugSessionState::Running, std::memory_order_release);
    if (m_stateCallback) {
        m_stateCallback(DebugSessionState::Running, m_stateCallbackData);
    }
    return DebugResult::ok("Continue accepted (non-MSVC fallback)");
}

DebugResult NativeDebuggerEngine::detach() {
    m_state.store(DebugSessionState::Idle, std::memory_order_release);
    m_targetPID = 0;
    m_targetName.clear();
    m_targetPath.clear();
    if (m_stateCallback) {
        m_stateCallback(DebugSessionState::Idle, m_stateCallbackData);
    }
    return DebugResult::ok("Detached (non-MSVC fallback)");
}

void NativeDebuggerEngine::setBreakpointHitCallback(BreakpointHitCallback cb, void* userData) {
    m_bpHitCallback = cb;
    m_bpHitCallbackData = userData;
}

void NativeDebuggerEngine::setOutputCallback(DebugOutputCallback cb, void* userData) {
    m_outputCallback = cb;
    m_outputCallbackData = userData;
}

void NativeDebuggerEngine::setStateCallback(DebugStateCallback cb, void* userData) {
    m_stateCallback = cb;
    m_stateCallbackData = userData;
}

DebugResult NativeDebuggerEngine::terminateTarget() {
    m_state.store(DebugSessionState::Terminated, std::memory_order_release);
    if (m_stateCallback) {
        m_stateCallback(DebugSessionState::Terminated, m_stateCallbackData);
    }
    return DebugResult::ok("Terminate accepted (non-MSVC fallback)");
}

DebugResult NativeDebuggerEngine::breakExecution() {
    m_state.store(DebugSessionState::Broken, std::memory_order_release);
    if (m_stateCallback) {
        m_stateCallback(DebugSessionState::Broken, m_stateCallbackData);
    }
    return DebugResult::ok("Break accepted (non-MSVC fallback)");
}

DebugResult NativeDebuggerEngine::updateWatches() {
    std::lock_guard<std::mutex> lock(m_watchMutex);
    for (auto& watch : m_watches) {
        if (!watch.enabled) {
            continue;
        }
        watch.lastResult.success = false;
        watch.lastResult.expression = watch.expression;
        watch.lastResult.value = "<unavailable>";
        watch.lastResult.type = "native-debugger-disabled";
        watch.lastResult.rawValue = 0;
        watch.lastResult.isPointer = false;
        watch.lastResult.isFloat = false;
        ++watch.updateCount;
    }
    return DebugResult::ok("Native debugger disabled on non-MSVC toolchain");
}

DebugResult NativeDebuggerEngine::removeWatch(uint32_t watchId) {
    std::lock_guard<std::mutex> lock(m_watchMutex);
    const auto before = m_watches.size();
    m_watches.erase(
        std::remove_if(
            m_watches.begin(),
            m_watches.end(),
            [watchId](const NativeWatch& watch) { return watch.id == watchId; }),
        m_watches.end());
    if (m_watches.size() == before) {
        return DebugResult::error("Watch not found", -1);
    }
    return DebugResult::ok("Watch removed");
}

DebugResult NativeDebuggerEngine::removeAllBreakpoints() {
    std::lock_guard<std::mutex> lock(m_bpMutex);
    m_breakpoints.clear();
    return DebugResult::ok("Breakpoints cleared");
}

DebugResult NativeDebuggerEngine::captureRegisters(RegisterSnapshot& outSnapshot) {
    outSnapshot = RegisterSnapshot{};
    return DebugResult::error("Register capture unavailable on non-MSVC toolchain", -2);
}

std::string NativeDebuggerEngine::formatFlags(uint64_t rflags) const {
    std::ostringstream out;
    out << "RFLAGS=0x" << std::hex << std::uppercase << rflags << " (native debugger unavailable)";
    return out.str();
}

DebugResult NativeDebuggerEngine::readMemory(uint64_t, void*, uint64_t, uint64_t* bytesRead) {
    if (bytesRead) {
        *bytesRead = 0;
    }
    return DebugResult::error("Memory read unavailable on non-MSVC toolchain", -2);
}

DebugResult NativeDebuggerEngine::queryMemoryRegions(std::vector<MemoryRegion>& outRegions) {
    outRegions.clear();
    return DebugResult::error("Memory map query unavailable on non-MSVC toolchain", -2);
}

std::string NativeDebuggerEngine::formatHexDump(uint64_t address, const void* data, uint64_t size, uint32_t columns) const {
    if (!data || size == 0 || columns == 0) {
        return "No data";
    }
    const auto* bytes = static_cast<const unsigned char*>(data);
    std::ostringstream out;
    const uint64_t preview = std::min<uint64_t>(size, 128);
    for (uint64_t i = 0; i < preview; ++i) {
        if ((i % columns) == 0) {
            out << "\n0x" << std::hex << std::setw(16) << std::setfill('0') << (address + i) << ": ";
        }
        out << std::setw(2) << std::setfill('0') << static_cast<unsigned>(bytes[i]) << ' ';
    }
    if (preview < size) {
        out << "\n...";
    }
    return out.str();
}

DebugResult NativeDebuggerEngine::evaluate(const std::string& expression, EvalResult& outResult) {
    outResult.success = false;
    outResult.expression = expression;
    outResult.value = "<unavailable>";
    outResult.type = "native-debugger-disabled";
    outResult.rawValue = 0;
    outResult.isPointer = false;
    outResult.isFloat = false;
    return DebugResult::error("Expression evaluation unavailable on non-MSVC toolchain", -2);
}

DebugSessionStats NativeDebuggerEngine::getStats() const {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    return m_stats;
}

}  // namespace RawrXD::Debugger

#endif  // !defined(_MSC_VER)
