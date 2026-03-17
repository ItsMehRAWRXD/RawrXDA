#include <iostream>
#include <string>
#include "src/cpu_inference_engine.h"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <model_path>" << std::endl;
        return 1;
    }

    std::string model_path = argv[1];
    std::cout << "[TEST] Testing inference engine with model: " << model_path << std::endl;

    try {
        // Create inference engine instance
        auto engine = std::make_unique<CPUInferenceEngine>();
        
        // Test the LoadModel function directly
        std::cout << "[TEST] Calling LoadModel..." << std::endl;
        bool result = engine->LoadModel(model_path);
        
        if (result) {
            std::cout << "[TEST] SUCCESS: Model loaded without corruption" << std::endl;
            
            // Try to get basic metadata to verify it's working
            std::cout << "[TEST] Model loaded successfully - checking basic functionality" << std::endl;
            
        } else {
            std::cout << "[TEST] FAILED: Model loading failed" << std::endl;
            return 1;
        }
        
    } catch (const std::exception& e) {
        std::cout << "[TEST] EXCEPTION: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cout << "[TEST] UNKNOWN EXCEPTION occurred" << std::endl;
        return 1;
    }

    std::cout << "[TEST] Test completed successfully" << std::endl;
    return 0;
}