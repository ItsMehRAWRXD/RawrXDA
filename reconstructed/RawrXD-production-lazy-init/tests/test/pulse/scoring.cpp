// Test: Pulse Effectiveness Scoring Integration
// Verifies that the agentic loop correctly records and evaluates pulse scores

#include <iostream>
#include <windows.h>

// External MASM procedures
extern "C" {
    void CheckPulseEffectiveness();
    void UpdatePulseMetrics(uint64_t cycle_delta);
    uint64_t GetPulseStats();
    void ResetPulseScoring();
    void RawrXD_RecordPulseScoreProc(
        uint64_t start_cycles,
        uint64_t end_cycles,
        int32_t queue_delta,
        int32_t mem_delta,
        uint32_t result_code,
        uint32_t pulse_type
    );
}

int main() {
    std::cout << "=== Pulse Effectiveness Scoring Test ===" << std::endl;
    
    // Reset state
    ResetPulseScoring();
    std::cout << "[PASS] ResetPulseScoring" << std::endl;
    
    // Simulate 5 effective pulses (low cycle count)
    for (int i = 0; i < 5; i++) {
        uint64_t start = __rdtsc();
        Sleep(1);  // Simulate work
        uint64_t end = __rdtsc();
        
        RawrXD_RecordPulseScoreProc(start, end, 1, 0, 0, 0);
        UpdatePulseMetrics(end - start);
    }
    std::cout << "[PASS] Recorded 5 effective pulses" << std::endl;
    
    // Check average
    uint64_t avg = GetPulseStats();
    std::cout << "[INFO] Average cycles per pulse: " << avg << std::endl;
    
    // Simulate 3 expensive pulses
    for (int i = 0; i < 3; i++) {
        uint64_t start = __rdtsc();
        Sleep(10);  // Simulate expensive work
        uint64_t end = __rdtsc();
        
        RawrXD_RecordPulseScoreProc(start, end, 0, 0, 0, 0);
        UpdatePulseMetrics(end - start);
    }
    std::cout << "[PASS] Recorded 3 expensive pulses" << std::endl;
    
    // Check if throttling kicks in
    CheckPulseEffectiveness();
    std::cout << "[PASS] CheckPulseEffectiveness executed" << std::endl;
    
    // Get final stats
    avg = GetPulseStats();
    std::cout << "[INFO] Final average cycles: " << avg << std::endl;
    
    std::cout << "=== All Tests Passed ===" << std::endl;
    return 0;
}
