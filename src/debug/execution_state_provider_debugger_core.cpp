#include "execution_state_provider_debugger_core.hpp"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <string>
#include <windows.h>

namespace {
std::string toHex(uint64_t value) {
    std::ostringstream stream;
    stream << "0x" << std::hex << std::uppercase << value;
    return stream.str();
}

std::string bytesToHex(const std::vector<uint8_t>& data, size_t maxBytes) {
    if (data.empty() || maxBytes == 0) {
        return {};
    }

    std::ostringstream stream;
    stream << "0x";
    const size_t count = std::min(maxBytes, data.size());
    for (size_t i = 0; i < count; ++i) {
        stream << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(data[i]);
    }
    if (data.size() > count) {
        stream << "...";
    }
    return stream.str();
}
}  // namespace

DebuggerCoreExecutionStateProvider::DebuggerCoreExecutionStateProvider(rawrxd::debug::DebuggerCore* debugger)
    : m_debugger(debugger) {
}

bool DebuggerCoreExecutionStateProvider::getLatestSnapshot(ExecutionStateSnapshot& outSnapshot) {
    if (!m_debugger) {
        return false;
    }

    outSnapshot = ExecutionStateSnapshot{};
    outSnapshot.capturedAtUnixMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    outSnapshot.notes = "debugger_core_snapshot";
    const DWORD threadId = GetCurrentThreadId();
    outSnapshot.threadId = std::to_string(threadId);

    rawrxd::debug::DebuggerCore::Registers regs{};
    if (m_debugger->getThreadRegisters(threadId, regs)) {
        outSnapshot.registers.emplace_back("rip", toHex(regs.rip));
        outSnapshot.registers.emplace_back("rsp", toHex(regs.rsp));
        outSnapshot.registers.emplace_back("rbp", toHex(regs.rbp));
        outSnapshot.registers.emplace_back("rax", toHex(regs.rax));
        outSnapshot.registers.emplace_back("rbx", toHex(regs.rbx));
    }

    const auto watchSymbols = m_debugger->getWatchSymbolList();
    const size_t watchSymbolLimit = std::min<size_t>(8, watchSymbols.size());
    for (size_t i = 0; i < watchSymbolLimit; ++i) {
        outSnapshot.registers.emplace_back(
            "watch:" + watchSymbols[i].name,
            bytesToHex(watchSymbols[i].lastValue, 8));
    }

    const auto watchEntries = m_debugger->getWatchList();
    const size_t watchEntryLimit = std::min<size_t>(8, watchEntries.size());
    for (size_t i = 0; i < watchEntryLimit; ++i) {
        outSnapshot.registers.emplace_back(
            "mem:" + watchEntries[i].label,
            bytesToHex(watchEntries[i].lastValue, 8));
    }

    outSnapshot.valid = !outSnapshot.registers.empty() || !outSnapshot.stackFrames.empty();
    return outSnapshot.valid;
}
