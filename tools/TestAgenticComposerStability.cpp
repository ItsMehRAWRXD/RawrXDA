#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>
#include "d:/rawrxd/src/AgenticComposer.cpp"
#include "d:/rawrxd/src/MultiFileRewriteEngine.cpp"

namespace fs = std::filesystem;

int main() {
    std::cout << "--- RawrXD AgenticComposer Stability & Goal Execution Test ---" << std::endl;

    const std::string testRoot = "d:/rawrxd_agentic_test";
    if (fs::exists(testRoot)) {
        fs::remove_all(testRoot);
    }
    fs::create_directory(testRoot);

    std::vector<std::string> testFiles;
    for (int i = 0; i < 20; ++i) {
        std::string fileName = testRoot + "/target_" + std::to_string(i) + ".cpp";
        std::ofstream ofs(fileName);
        ofs << "// File " << i << " content\n";
        testFiles.push_back(fileName);
    }

    RawrXD::IDE::AgenticComposer composer;
    
    std::cout << "[Step 1] Starting goal: Coordinated Refactor of 20 Files" << std::endl;
    composer.startGoal("Global refactor to inject performance-bridging stubs", testFiles);

    // Wait for the async planning phase
    while (composer.getState() == RawrXD::IDE::ComposerState::Planning) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (composer.getState() == RawrXD::IDE::ComposerState::ReviewingChange) {
        std::cout << "[Step 2] Plan Generated. Reviewing..." << std::endl;
        
        // Approve the step
        composer.approveStep();

        // Wait for the async apply phase
        while (composer.getState() == RawrXD::IDE::ComposerState::Applying) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        if (composer.getState() == RawrXD::IDE::ComposerState::Success) {
            std::cout << "[PASS] Agentic goal completed successfully across 20 files." << std::endl;
        } else {
            std::cout << "[FAIL] Agentic goal execution failed. State: " << static_cast<int>(composer.getState()) << std::endl;
            return 1;
        }
    } else {
        std::cout << "[FAIL] Composer did not transition to ReviewingChange state." << std::endl;
        return 1;
    }

    // Rollback stability check
    std::cout << "[Step 3] Verifying stability during atomic rollback..." << std::endl;
    composer.rollbackAll();
    
    for (const auto& file : testFiles) {
        std::ifstream ifs(file);
        std::string content;
        std::getline(ifs, content);
        if (content.find("// File") == std::string::npos) {
            std::cout << "[FAIL] Rollback integrity breach in file: " << file << std::endl;
            return 1;
        }
    }
    std::cout << "[PASS] Rollback integrity intact." << std::endl;

    // Cleanup
    fs::remove_all(testRoot);
    std::cout << "--- Stability Test Complete ---" << std::endl;

    return 0;
}
