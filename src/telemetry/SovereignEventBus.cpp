// ==================================================
// SovereignEventBus.cpp — Implementation of the Decision Trace
// ==================================================
#include "SovereignEventBus.h"
#include <chrono>
#include <cstring>

namespace RawrXD {
namespace Telemetry {

SovereignEventBus& SovereignEventBus::Instance() {
    static SovereignEventBus instance;
    return instance;
}

void SovereignEventBus::Emit(EventType type, const std::string& nodeId, const std::string& kernel, const std::string& jsonDetails) {
    size_t idx = m_writeIdx.fetch_add(1) % BUFFER_SIZE;
    EvolutionEvent& ev = m_buffer[idx];
    
    ev.timestamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    ev.type = type;
    
    strncpy_s(ev.nodeId, nodeId.c_str(), _TRUNCATE);
    strncpy_s(ev.kernelName, kernel.c_str(), _TRUNCATE);
    strncpy_s(ev.details, jsonDetails.c_str(), _TRUNCATE);
}

std::vector<EvolutionEvent> SovereignEventBus::PollEvents() {
    std::vector<EvolutionEvent> events;
    size_t currentWrite = m_writeIdx.load() % BUFFER_SIZE;
    
    while (m_readIdx != currentWrite) {
        events.push_back(m_buffer[m_readIdx]);
        m_readIdx = (m_readIdx + 1) % BUFFER_SIZE;
    }
    return events;
}

} // namespace Telemetry
} // namespace RawrXD
