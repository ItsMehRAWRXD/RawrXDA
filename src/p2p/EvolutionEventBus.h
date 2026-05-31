#pragma once
#include <stdint.h>
#include <string>
#include <vector>
#include <functional>
#include <mutex>

/**
 * @file EvolutionEventBus.h
 * @brief Zero-dependency, thread-safe event bus for real-time evolution telemetry.
 */

struct EvolutionEvent {
    uint64_t eventId;
    uint64_t timestamp;
    const char* type;      // KernelPromoted, ValidationResult, SealEvent, PulseCycleComplete
    const char* nodeId;
    const char* payloadJson;
};

class EvolutionEventBus {
public:
    using EventCallback = std::function<void(const EvolutionEvent&)>;

    static EvolutionEventBus& Instance() {
        static EvolutionEventBus instance;
        return instance;
    }

    void Emit(const char* type, const char* nodeId, const char* payloadJson) {
        EvolutionEvent evt;
        evt.eventId = IncrementEventId();
        evt.timestamp = GetTimestamp();
        evt.type = type;
        evt.nodeId = nodeId;
        evt.payloadJson = payloadJson;

        std::lock_guard<std::mutex> lock(m_mutex);
        for (const auto& cb : m_callbacks) {
            cb(evt);
        }
        
        // Log to internal trace for persistence if needed
        TraceToDisk(evt);
    }

    void Subscribe(EventCallback cb) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_callbacks.push_back(cb);
    }

private:
    EvolutionEventBus() : m_globalEventId(0) {}
    
    uint64_t GetTimestamp();
    uint64_t IncrementEventId();
    void TraceToDisk(const EvolutionEvent& evt);

    std::mutex m_mutex;
    std::vector<EventCallback> m_callbacks;
    uint64_t m_globalEventId;
};
