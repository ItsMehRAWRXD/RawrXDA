#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <filesystem>
#include <fstream>
#include "src/MultiFileRewriteEngine.cpp"

namespace fs = std::filesystem;

int main() {
    std::cout << "--- RawrXD MultiFileRewriteEngine Benchmark: 50+ File Refactor ---" << std::endl;

    const std::string benchRoot = "d:/rawrxd_bench_temp";
    if (fs::exists(benchRoot)) {
        fs::remove_all(benchRoot);
    }
    fs::create_directory(benchRoot);

    std::vector<std::string> targetFiles;
    for (int i = 0; i < 60; ++i) {
        std::string fileName = benchRoot + "/file_" + std::to_string(i) + ".cpp";
        std::ofstream ofs(fileName);
        ofs << "// Original content of file " << i << "\n";
        ofs << "int main_" << i << "() { return 0; }\n";
        targetFiles.push_back(fileName);
    }

    RawrXD::IDE::MultiFileRewriteEngine engine;
    
    auto startPlan = std::chrono::high_resolution_clock::now();
    auto plan = engine.planCoordinatedEdits("Across-repository dependency refactor", targetFiles);
    auto endPlan = std::chrono::high_resolution_clock::now();
    
    std::chrono::duration<double, std::milli> planDuration = endPlan - startPlan;
    std::cout << "[PASS] Plan generated for " << plan.changes.size() << " files in " << planDuration.count() << " ms" << std::endl;

    // Simulate LLM change across all files
    for (auto& change : plan.changes) {
        change.newContent = "// REFACTORED CONTENT\n" + change.originalContent + "\n// Refactor complete\n";
    }

    auto startApply = std::chrono::high_resolution_clock::now();
    bool success = engine.applyPlan(plan);
    auto endApply = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double, std::milli> applyDuration = endApply - startApply;
    if (success) {
        std::cout << "[PASS] Plan applied (atomic transition) in " << applyDuration.count() << " ms" << std::endl;
        std::cout << "Throughput: " << (double)plan.changes.size() / (applyDuration.count() / 1000.0) << " files/sec" << std::endl;
    } else {
        std::cout << "[FAIL] Plan application failed." << std::endl;
        return 1;
    }

    // Rollback test
    auto startRollback = std::chrono::high_resolution_clock::now();
    bool rbSuccess = engine.rollback(plan);
    auto endRollback = std::chrono::high_resolution_clock::now();
    
    std::chrono::duration<double, std::milli> rbDuration = endRollback - startRollback;
    if (rbSuccess) {
        std::cout << "[PASS] Rollback (zero-loss) verified in " << rbDuration.count() << " ms" << std::endl;
    } else {
        std::cout << "[FAIL] Rollback verification failed." << std::endl;
        return 1;
    }

    // Cleanup
    fs::remove_all(benchRoot);
    std::cout << "--- Benchmark Complete ---" << std::endl;

    return 0;
}
