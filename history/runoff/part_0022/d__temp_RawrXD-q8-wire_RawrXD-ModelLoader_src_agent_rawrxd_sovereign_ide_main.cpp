#include <iostream>
#include <windows.h>
#include <thread>
#include "RawrXD_GGUFProxyServer.hpp"
#include "RawrXD_AgenticFailureDetector.hpp"
#include "RawrXD_AgenticPuppeteer.hpp"

/**
 * RawrXD-SovereignIDE: Pure Win32/C++20 Autonomous IDE
 * 
 * Zero Qt Framework Dependencies
 * Features:
 *  - GGUF model proxy server (Winsock2 native TCP)
 *  - Agentic failure detection and correction
 *  - Native Windows UI (Win32 API)
 *  - Autonomous code generation and execution
 */

int main(int argc, char* argv[])
{
    std::cout << "========================================\n";
    std::cout << "RawrXD-SovereignIDE v1.0 (Pure Win32)\n";
    std::cout << "Autonomous AI Code Generation IDE\n";
    std::cout << "Zero Qt Framework Dependencies\n";
    std::cout << "========================================\n\n";

    try {
        // Initialize GGUF Proxy Server (Winsock2)
        std::cout << "[*] Initializing GGUF Proxy Server...\n";
        RawrXD::GGUFProxyServer proxy_server(8888, "127.0.0.1", 8889);
        
        std::thread proxy_thread([&proxy_server]() {
            proxy_server.Start();
        });
        proxy_thread.detach();
        
        std::cout << "[+] GGUF Proxy Server started on port 8888\n";
        
        // Initialize Agentic Failure Detector
        std::cout << "[*] Initializing Agentic Failure Detector...\n";
        RawrXD::AgenticFailureDetector detector;
        std::cout << "[+] Agentic Failure Detector initialized\n";
        
        // Initialize Agentic Puppeteer (Response Corrector)
        std::cout << "[*] Initializing Agentic Puppeteer...\n";
        RawrXD::AgenticPuppeteer puppeteer;
        std::cout << "[+] Agentic Puppeteer initialized\n";
        
        // Test failure detection and correction
        std::cout << "\n[*] Testing failure detection pipeline...\n";
        std::string test_response = "I can't help with that. This is a refusal response.";
        
        auto failure_info = detector.DetectFailure(test_response);
        if (failure_info.type != RawrXD::FailureType::None) {
            std::cout << "[!] Detected failure type: " 
                      << static_cast<int>(failure_info.type) 
                      << " (confidence: " << failure_info.confidence << ")\n";
            
            std::string corrected = puppeteer.CorrectResponse(
                test_response, 
                failure_info
            );
            std::cout << "[+] Corrected response: " << corrected << "\n";
        }
        
        // Get server statistics
        std::cout << "\n[*] GGUF Proxy Server Statistics:\n";
        auto stats = proxy_server.GetServerStatistics();
        std::cout << "    " << stats.dump(2) << "\n";
        
        // Main event loop
        std::cout << "\n[+] RawrXD-SovereignIDE is running...\n";
        std::cout << "[*] Press Ctrl+C to exit\n";
        
        while (true) {
            Sleep(1000);
        }
        
        proxy_server.Stop();
        std::cout << "[+] Graceful shutdown complete\n";
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] " << e.what() << "\n";
        return 1;
    }
}
