#include "MultiFileRewriteEngine.h"
#include <iostream>
#include <cassert>
#include <fstream>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;
using namespace RawrXD::IDE;

void test_atomic_rollback() {
    std::cout << "Testing MultiFileRewriteEngine Atomic Rollback..." << std::endl;

    std::string file1 = "test_file1.txt";
    std::string file2 = "test_file2.txt";

    // Setup initial state
    {
        std::ofstream ofs1(file1);
        ofs1 << "Original Content 1";
        std::ofstream ofs2(file2);
        ofs2 << "Original Content 2";
    }

    MultiFileRewriteEngine engine;
    MultiFilePlan plan;
    plan.planDescription = "Test Plan with failure simulation";

    MultiFileChange change1;
    change1.filePath = file1;
    change1.originalContent = "Original Content 1";
    change1.newContent = "Modified Content 1";
    
    MultiFileChange change2;
    change2.filePath = "INVALID_PATH/nonexistent.txt"; // Will cause write failure
    change2.originalContent = "Original Content 2";
    change2.newContent = "Modified Content 2";

    plan.changes.push_back(change1);
    plan.changes.push_back(change2);

    // Apply plan - should trigger rollback because change2 fails
    std::cout << "Applying plan (expecting rollback due to invalid path)..." << std::endl;
    bool success = engine.applyPlan(plan);
    
    // Check results
    assert(success == false);
    
    // Verify file1 is back to original state
    std::ifstream ifs1(file1);
    std::string content1;
    std::getline(ifs1, content1);
    std::cout << "File 1 content after rollback: " << content1 << std::endl;
    assert(content1 == "Original Content 1");

    // Cleanup
    fs::remove(file1);
    fs::remove(file2);

    std::cout << "Atomic Rollback Test PASSED!" << std::endl;
}

int main() {
    try {
        test_atomic_rollback();
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
