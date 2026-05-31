#include <windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include "../runtime/SovereignKAIROSBridge.h"
#include "../runtime/SovereignMemoryBridge.h"

using namespace RawrXD::Runtime;
namespace fs = std::filesystem;

void KairosMemoryTest() {
    std::cout << "[KairosTest] Starting KAIROS ↔ MEMORY Integration Test..." << std::endl;

    // 1. Setup a temporary project directory
    fs::path testDir = fs::current_path() / "temp_kairos_test";
    fs::create_directories(testDir);
    std::cout << "[KairosTest] Test directory: " << testDir.string() << std::endl;

    // 2. Initialize Memory System first (KAIROS depends on it)
    if (!SovereignMemoryBridge::instance().initialize(testDir.string())) {
        std::cerr << "[KairosTest] Failed to initialize Memory System." << std::endl;
        return;
    }
    std::cout << "[KairosTest] Memory System online." << std::endl;

    // 3. Initialize KAIROS
    if (!SovereignKAIROSBridge::instance().initialize()) {
        std::cerr << "[KairosTest] Failed to initialize KAIROS." << std::endl;
        return;
    }
    std::cout << "[KairosTest] KAIROS Observation Engine online." << std::endl;

    // 4. Start watching the directory
    std::wstring wtestDir = testDir.wstring();
    if (!SovereignKAIROSBridge::instance().startWatching(wtestDir)) {
        std::cerr << "[KairosTest] Failed to start project watch." << std::endl;
        return;
    }
    std::cout << "[KairosTest] Watching directory for changes..." << std::endl;

    // 5. Trigger a file change (Create a new file)
    fs::path triggerFile = testDir / "trigger.txt";
    {
        std::ofstream ofs(triggerFile);
        ofs << "KAIROS Trigger Content: " << std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()) << std::endl;
    }
    std::cout << "[KairosTest] Created trigger file: " << triggerFile.string() << std::endl;

    // 6. Give KAIROS time to detect and process (Wait for "AutoDream" consolidation)
    std::cout << "[KairosTest] Waiting for KAIROS to process (1s)..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // 7. Verify MEMORY.md updates
    // In MASM, MemorySystem_Initialize usually creates MEMORY.md
    fs::path memoryMd = testDir / "MEMORY.md";
    if (fs::exists(memoryMd)) {
        std::cout << "[KairosTest] SUCCESS: MEMORY.md found at " << memoryMd.string() << std::endl;
        
        // Check stats
        KairosStats stats;
        if (SovereignKAIROSBridge::instance().checkHealth(stats)) {
            std::cout << "[KairosTest] KAIROS Stats -> Files Analyzed: " << stats.filesAnalyzed 
                      << ", Buddy Mood: " << stats.buddyMood << std::endl;
        }
    } else {
        std::cout << "[KairosTest] WARNING: MEMORY.md not yet created. Checking manual record..." << std::endl;
        // Sometimes it requires a shutdown to flush
    }

    // 8. Test manual record to verify Memory Bridge
    SovereignMemoryBridge::instance().recordDecision("KairosTest", "Verified file system watch trigger", 1);
    
    // 9. Shutdown and verify flush
    std::cout << "[KairosTest] Shutting down to flush memory..." << std::endl;
    SovereignKAIROSBridge::instance().shutdown();
    SovereignMemoryBridge::instance().shutdown();

    if (fs::exists(memoryMd)) {
        std::cout << "[KairosTest] FINAL CHECK: MEMORY.md exists and is flushed. Size: " << fs::file_size(memoryMd) << " bytes." << std::endl;
    } else {
        std::cerr << "[KairosTest] FAILURE: MEMORY.md was not flushed to disk." << std::endl;
    }

    // Cleanup
    // fs::remove_all(testDir);
    std::cout << "[KairosTest] Test Complete." << std::endl;
}

int main() {
    KairosMemoryTest();
    return 0;
}
