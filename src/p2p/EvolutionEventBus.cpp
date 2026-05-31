#include "EvolutionEventBus.h"
#include "AutonomousNarrationLayer.h"
#include <windows.h>
#include <fstream>
#include <iostream>

uint64_t EvolutionEventBus::GetTimestamp() {
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    return (static_cast<uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
}

uint64_t EvolutionEventBus::IncrementEventId() {
    return InterlockedIncrement64((volatile LONG64*)&m_globalEventId);
}

void EvolutionEventBus::TraceToDisk(const EvolutionEvent& evt) {
    // Narrate the event into human-readable tactical summaries
    std::string narration = AutonomousNarrationLayer::Narrate(evt.type, evt.nodeId, evt.payloadJson);
    std::cout << "[Sovereign Narration] #" << evt.eventId << " " << narration << std::endl;

    // Append-only JSONL trace for the dashboard to consume
    static std::ofstream logFile("d:/evolution_trace.jsonl", std::ios::app);
    if (logFile.is_open()) {
        logFile << "{\"id\":" << evt.eventId
                << ",\"ts\":" << evt.timestamp 
                << ",\"type\":\"" << evt.type << "\""
                << ",\"node\":\"" << evt.nodeId << "\""
                << ",\"data\":" << evt.payloadJson << "}" << std::endl;
    }
}
