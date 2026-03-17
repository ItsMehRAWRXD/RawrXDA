// Quick integration test
#include "Win32_InferenceEngine.hpp"
#include "Win32_InferenceEngine_Integration.hpp"

int main() {
    // Test manager
    auto& manager = RawrXD::Win32::InferenceEngineManager::GetInstance();
    
    // Test aggregator
    RawrXD::Win32::InferenceResultAggregator agg;
    
    // Test event handler
    RawrXD::Win32::InferenceEventHandler handler;
    
    return 0;
}
