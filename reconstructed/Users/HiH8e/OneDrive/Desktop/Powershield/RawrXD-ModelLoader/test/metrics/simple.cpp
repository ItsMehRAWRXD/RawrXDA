#include "include/telemetry/ai_metrics.h"
#include <iostream>

using namespace RawrXD::Telemetry;

int main() {
    std::cout << "Testing AI Metrics basic functionality...\n" << std::endl;
    
    try {
        std::cout << "1. Getting metrics collector..." << std::endl;
        auto& metrics = GetMetricsCollector();
        std::cout << "   ✓ Collector initialized\n" << std::endl;
        
        std::cout << "2. Recording Ollama request..." << std::endl;
        metrics.recordOllamaRequest("llama3.2", 150, true, 100, 250);
        std::cout << "   ✓ Request recorded\n" << std::endl;
        
        std::cout << "3. Recording tool invocation..." << std::endl;
        metrics.recordToolInvocation("file_read", 25, true);
        std::cout << "   ✓ Tool invocation recorded\n" << std::endl;
        
        std::cout << "4. Getting display metrics..." << std::endl;
        auto display = metrics.getDisplayMetrics();
        std::cout << "   ✓ Display metrics retrieved" << std::endl;
        std::cout << "   Total requests: " << display.total_requests << std::endl;
        std::cout << "   Success rate: " << display.success_rate << "%" << std::endl;
        
        std::cout << "\n✅ All basic tests passed!" << std::endl;
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "\n❌ Exception: " << e.what() << std::endl;
        return 1;
    }
    catch (...) {
        std::cerr << "\n❌ Unknown exception!" << std::endl;
        return 1;
    }
}
