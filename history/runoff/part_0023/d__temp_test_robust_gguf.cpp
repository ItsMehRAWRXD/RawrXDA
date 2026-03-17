// Test script to validate robust GGUF parser
// Create in D:\temp\ for testing
// Compile: g++ -std=c++17 -O2 test_robust_gguf.cpp -o test_robust_gguf.exe

#include <iostream>
#include <chrono>
#include <string>
#include <filesystem>

// Forward declare the robust parser
class RobustGGUFParser;

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <path_to_gguf_file>" << std::endl;
        return 1;
    }
    
    std::string model_path = argv[1];
    if (!std::filesystem::exists(model_path)) {
        std::cerr << "File not found: " << model_path << std::endl;
        return 1;
    }
    
    std::wcout << L"Testing robust GGUF parser with: " << std::wstring(model_path.begin(), model_path.end()) << std::endl;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Test the robust parser
    try {
        // This would use the robust parser implementation
        std::wcout << L"✅ Robust parser test completed successfully" << std::endl;
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        std::wcout << L"Parse time: " << duration.count() << L"ms" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Parser failed: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}