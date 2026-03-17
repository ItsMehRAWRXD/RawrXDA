// Test compilation of Win32_InferenceEngine.hpp
#include "Win32_InferenceEngine.hpp"

int main() {
    RawrXD::Win32::InferenceEngine engine;
    
    // Test callback registration
    engine.SetOnModelLoaded([]() {
        // Model loaded
    });
    
    engine.SetOnError([](RawrXD::Win32::InferenceErrorCode code, const RawrXD::String& msg) {
        // Error occurred
    });
    
    engine.SetOnInferenceComplete([](const RawrXD::Win32::InferenceResult& result) {
        // Inference complete
    });
    
    return 0;
}
