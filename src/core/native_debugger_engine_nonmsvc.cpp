#if !defined(_MSC_VER)

#include "native_debugger_engine.h"

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

}  // namespace RawrXD::Debugger

#endif  // !defined(_MSC_VER)
