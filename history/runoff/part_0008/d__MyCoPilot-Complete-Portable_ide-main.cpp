// ide-main.cpp
// Standalone C++ IDE with native performance

#include "systems-ide-minimal.hpp"
#include <cstring>
#include <iostream>
#include <string>
#include <sstream>
#include <thread>
#include <chrono>

class StandaloneIDE {
private:
    std::unique_ptr<SystemsIDECore> core_;
    bool running_;

public:
    StandaloneIDE() : core_(std::make_unique<SystemsIDECore>()), running_(false) {}

    bool Start() {
        std::cout << "========================================\n";
        std::cout << "Systems Engineering IDE - C++ Edition\n";
        std::cout << "Native Performance with Assembly Optimization\n";
        std::cout << "========================================\n\n";

        if (!core_->Initialize()) {
            std::cerr << "Failed to initialize IDE core!\n";
            return false;
        }

        running_ = true;
        ShowWelcomeScreen();
        return true;
    }

    void ShowWelcomeScreen() {
        std::cout << "Hardware Features Detected:\n";
        
        uint32_t eax = 0, ebx = 0, ecx = 0, edx = 0;
        cpuid_info(1, &eax, &ebx, &ecx, &edx);
        
        std::cout << "- SSE4.2: " << ((ecx & (1 << 20)) ? "✓" : "✗") << "\n";
        std::cout << "- AVX2: " << ((ecx & (1 << 28)) ? "✓" : "✗") << "\n";
        std::cout << "- Hardware CRC32: " << ((ecx & (1 << 20)) ? "✓" : "✗") << "\n";
        
        // Show performance stats
        auto perf = core_->GetPerfCounters();
        std::cout << "\nPerformance Counters:\n";
        std::cout << "- CPU Cycles: " << perf.cycles_start << "\n";
        std::cout << "- Cache Hit Ratio: " << (core_->GetCacheHitRatio() * 100.0) << "%\n";
        
        std::cout << "\nD: Drive Access: Ready\n";
        std::cout << "Network Server: localhost:8080\n\n";
    }

    void RunCommandLoop() {
        std::string input;
        std::cout << "Systems IDE> ";
        
        while (running_ && std::getline(std::cin, input)) {
            ProcessCommand(input);
            std::cout << "Systems IDE> ";
        }
    }

    void ProcessCommand(const std::string& command) {
        std::istringstream iss(command);
        std::string cmd;
        iss >> cmd;

        if (cmd == "ls" || cmd == "dir") {
            std::string path;
            iss >> path;
            if (path.empty()) path = "D:\\";
            
            ListDirectory(path);
        }
        else if (cmd == "read" || cmd == "cat") {
            std::string file;
            iss >> file;
            ReadFile(file);
        }
        else if (cmd == "search" || cmd == "find") {
            std::string pattern, root;
            iss >> pattern >> root;
            if (root.empty()) root = "D:\\";
            
            SearchFiles(pattern, root);
        }
        else if (cmd == "perf" || cmd == "stats") {
            ShowPerformanceStats();
        }
        else if (cmd == "help") {
            ShowHelp();
        }
        else if (cmd == "exit" || cmd == "quit") {
            running_ = false;
            std::cout << "Shutting down IDE...\n";
        }
        else if (!cmd.empty()) {
            std::cout << "Unknown command: " << cmd << "\n";
            std::cout << "Type 'help' for available commands.\n";
        }
    }

    void ListDirectory(const std::string& path) {
        auto start = std::chrono::high_resolution_clock::now();
        
        auto files = core_->ListDirectory(path);
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        std::cout << "Directory: " << path << "\n";
        std::cout << "Files found: " << files.size() << "\n";
        std::cout << "Scan time: " << duration.count() << " µs\n\n";
        
        for (const auto& file : files) {
            std::cout << file << "\n";
        }
        std::cout << "\n";
    }

    void ReadFile(const std::string& path) {
        auto start = std::chrono::high_resolution_clock::now();
        
        std::string content = core_->ReadFileOptimized(path);
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        if (!content.empty()) {
            std::cout << "File: " << path << "\n";
            std::cout << "Size: " << content.length() << " bytes\n";
            std::cout << "Read time: " << duration.count() << " µs\n\n";
            
            // Show first 1000 characters
            if (content.length() > 1000) {
                std::cout << content.substr(0, 1000) << "\n";
                std::cout << "... (truncated, " << (content.length() - 1000) << " more bytes)\n";
            } else {
                std::cout << content << "\n";
            }
        } else {
            std::cout << "Error: Could not read file " << path << "\n";
        }
        std::cout << "\n";
    }

    void SearchFiles(const std::string& pattern, const std::string& root) {
        auto start = std::chrono::high_resolution_clock::now();
        
        std::cout << "Searching for '" << pattern << "' in " << root << "...\n";
        
        auto results = core_->SearchFiles(pattern, root);
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        std::cout << "Found " << results.size() << " matches in " << duration.count() << " ms\n\n";
        
        for (const auto& result : results) {
            std::cout << result << "\n";
        }
        std::cout << "\n";
    }

    void ShowPerformanceStats() {
        auto perf = core_->GetPerfCounters();
        
        std::cout << "Performance Statistics:\n";
        std::cout << "======================\n";
        std::cout << "CPU Cycles: " << perf.cycles_start << "\n";
        std::cout << "Instructions Retired: " << perf.instructions_retired << "\n";
        std::cout << "Cache References: " << perf.cache_references << "\n";
        std::cout << "Cache Misses: " << perf.cache_misses << "\n";
        std::cout << "Branch Misses: " << perf.branch_misses << "\n";
        std::cout << "File Cache Hit Ratio: " << (core_->GetCacheHitRatio() * 100.0) << "%\n";
        
        // Calculate cache miss rate
        if (perf.cache_references > 0) {
            double miss_rate = (double)perf.cache_misses / perf.cache_references * 100.0;
            std::cout << "L3 Cache Miss Rate: " << miss_rate << "%\n";
        }
        
        // Calculate IPC (Instructions Per Cycle)
        if (perf.cycles_start > 0) {
            double ipc = (double)perf.instructions_retired / perf.cycles_start;
            std::cout << "Instructions Per Cycle: " << ipc << "\n";
        }
        
        std::cout << "\n";
    }

    void ShowHelp() {
        std::cout << "Systems Engineering IDE - Commands:\n";
        std::cout << "===================================\n";
        std::cout << "ls/dir [path]        - List directory contents\n";
        std::cout << "read/cat <file>      - Read file contents\n";
        std::cout << "search/find <pattern> [root] - Search for files\n";
        std::cout << "perf/stats          - Show performance statistics\n";
        std::cout << "help                - Show this help\n";
        std::cout << "exit/quit           - Exit IDE\n\n";
        
        std::cout << "Examples:\n";
        std::cout << "  ls D:\\              - List D: drive root\n";
        std::cout << "  read D:\\test.txt    - Read test.txt\n";
        std::cout << "  search *.cpp D:\\    - Find all .cpp files\n";
        std::cout << "  perf                - Show CPU performance counters\n\n";
    }

    void Stop() {
        running_ = false;
        core_->Shutdown();
    }
};

#ifdef STANDALONE_BUILD
int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    
    StandaloneIDE ide;
    
    if (!ide.Start()) {
        std::cerr << "Failed to start IDE!\n";
        return 1;
    }
    
    // Run web server in background thread
    std::thread web_thread([]() {
        system("node backend-server.js");
    });
    
    std::cout << "Type 'help' for commands, or 'exit' to quit.\n";
    std::cout << "Web interface available at http://localhost:8080\n\n";
    
    ide.RunCommandLoop();
    ide.Stop();
    
    // Clean shutdown
    system("taskkill /F /IM node.exe >nul 2>&1");
    
    return 0;
}
#endif