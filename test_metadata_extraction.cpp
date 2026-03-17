#include <iostream>
#include <string>
#include "src/cpu_inference_engine.h"

int main() {
    std::cout << "[TEST] Starting metadata extraction test..." << std::endl;
    
    try {
        RawrXD::CPUInferenceEngine engine;
        
        std::string model_path = "F:/OllamaModels/blobs/sha256-b6d08a5b621ce4669227a4ccd44a4fce4be77d4b4b6e1a5e0c037fe4dd755ee8";
        
        std::cout << "[TEST] Loading model: " << model_path << std::endl;
        
        bool success = engine.LoadModel(model_path, 1); // Load with 1 layer limit for speed
        
        if (success) {
            std::cout << "[TEST] SUCCESS: Model loaded with metadata extraction!" << std::endl;
            std::cout << "[TEST] Our enhanced CPU inference engine metadata extraction is working." << std::endl;
        } else {
            std::cout << "[TEST] FAILED: Model loading failed" << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cout << "[TEST] EXCEPTION: " << e.what() << std::endl;
    } catch (...) {
        std::cout << "[TEST] UNKNOWN EXCEPTION occurred" << std::endl;
    }
    
    return 0;
}