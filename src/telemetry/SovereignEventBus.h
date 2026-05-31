// ==================================================
// SovereignEventBus.h — Low-overhead, lock-free telemetry for the organism
// ==================================================
#pragma once

#include <cstdint>
#include <string>
#include <atomic>
#include <vector>

namespace RawrXD {
namespace Telemetry {

enum class EventType {
    KERNEL_CANDIDATE_RECEIVED,
    KERNEL_PROMOTED,
    VALIDATION_SUCCESS,
    VALIDATION_FAILURE,
    SEAL_EVENT,
    ROLLBACK_EVENT,
    PULSE_CYCLE_COMPLETE
};

struct EvolutionEvent {
    uint64_t timestamp;
    EventType type;
    char nodeId[32];
    char kernelName[64];
    char details[256]; // JSON payload
};

class SovereignEventBus {
public:
    static SovereignEventBus& Instance();

    void Emit(EventType type, const std::string& nodeId, const std::string& kernel, const std::string& jsonDetails);
    
    // Retrieval for the Dashboard
    std::vector<EvolutionEvent> PollEvents();

private:
    SovereignEventBus() : m_writeIdx(0) {}
    static constexpr size_t BUFFER_SIZE = 1024;
    EvolutionEvent m_buffer[BUFFER_SIZE];
    std::atomic<size_t> m_writeIdx;
    size_t m_readIdx = 0;
};

} // namespace Telemetry
} // namespace RawrXD
