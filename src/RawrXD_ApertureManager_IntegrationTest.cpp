// ============================================================================
// RawrXD_ApertureManager_IntegrationTest.cpp
// Validates manifest parsing, snapshot verification, and tri-factor integrity
// against the sealed v1.2.5-fused archive.
// ============================================================================

#include "../include/RawrXD_ApertureManager.h"
#include <iostream>
#include <iomanip>
#include <chrono>

using namespace RawrXD;

// ============================================================================
// Test Harness
// ============================================================================

struct TestResult {
    std::string test_name;
    bool passed;
    std::string error_message;
    std::chrono::milliseconds elapsed_ms;
};

void printTestResult(const TestResult& result) {
    const char* status = result.passed ? "✓ PASS" : "✗ FAIL";
    std::cout << std::left << std::setw(40) << result.test_name
              << " [ " << status << " ] "
              << result.elapsed_ms.count() << "ms";
    if (!result.error_message.empty()) {
        std::cout << "\n  Error: " << result.error_message;
    }
    std::cout << "\n";
}

// ============================================================================
// Test: Load Sealed Manifest
// ============================================================================

TestResult testLoadSealedManifest() {
    TestResult result;
    result.test_name = "Load Sealed Manifest";
    result.passed = false;

    auto start = std::chrono::high_resolution_clock::now();

    ApertureManager mgr;
    std::string dist_root = "D:\\dist-archives\\v1.2.5-fused-SEALED-20260403-203241";
    std::string error;

    if (!mgr.loadSealedManifest(dist_root, error)) {
        result.error_message = error;
        auto end = std::chrono::high_resolution_clock::now();
        result.elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        return result;
    }

    const auto& manifest = mgr.getManifest();
    if (manifest.version.empty() || !manifest.isSealed()) {
        result.error_message = "Manifest not sealed or version empty";
        auto end = std::chrono::high_resolution_clock::now();
        result.elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        return result;
    }

    std::cout << "    ✓ Version: " << manifest.version << "\n";
    std::cout << "    ✓ Golden Seal Status: " << manifest.golden_seal_status << "\n";
    std::cout << "    ✓ Sealed By: " << manifest.sealed_by << "\n";
    std::cout << "    ✓ Sealed At: " << manifest.sealed_at_utc << "\n";
    std::cout << "    ✓ Artifacts Count: " << manifest.artifacts.size() << "\n";

    result.passed = true;
    auto end = std::chrono::high_resolution_clock::now();
    result.elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    return result;
}

// ============================================================================
// Test: Load Recursive Snapshot
// ============================================================================

TestResult testLoadRecursiveSnapshot() {
    TestResult result;
    result.test_name = "Load Recursive Snapshot";
    result.passed = false;

    auto start = std::chrono::high_resolution_clock::now();

    ApertureManager mgr;
    std::string dist_root = "D:\\dist-archives\\v1.2.5-fused-SEALED-20260403-203241";
    std::string error;

    if (!mgr.loadSealedManifest(dist_root, error)) {
        result.error_message = "Failed to load manifest: " + error;
        auto end = std::chrono::high_resolution_clock::now();
        result.elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        return result;
    }

    if (!mgr.loadRecursiveSnapshot(dist_root, error)) {
        result.error_message = error;
        auto end = std::chrono::high_resolution_clock::now();
        result.elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        return result;
    }

    const auto& snapshot = mgr.getSnapshot();
    std::cout << "    ✓ Snapshot Entries: " << snapshot.size() << "\n";
    if (snapshot.size() > 0) {
        std::cout << "    ✓ First Entry: " << snapshot[0].path << " (" << snapshot[0].bytes << " bytes)\n";
    }

    result.passed = true;
    auto end = std::chrono::high_resolution_clock::now();
    result.elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    return result;
}

// ============================================================================
// Test: Tri-Factor Integrity Validation
// ============================================================================

TestResult testTriFactorIntegrity() {
    TestResult result;
    result.test_name = "Tri-Factor Integrity (Manifest→Snapshot→Disk)";
    result.passed = false;

    auto start = std::chrono::high_resolution_clock::now();

    ApertureManager mgr;
    std::string dist_root = "D:\\dist-archives\\v1.2.5-fused-SEALED-20260403-203241";
    std::string error;

    if (!mgr.loadSealedManifest(dist_root, error)) {
        result.error_message = "Failed to load manifest: " + error;
        auto end = std::chrono::high_resolution_clock::now();
        result.elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        return result;
    }

    if (!mgr.loadRecursiveSnapshot(dist_root, error)) {
        result.error_message = "Failed to load snapshot: " + error;
        auto end = std::chrono::high_resolution_clock::now();
        result.elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        return result;
    }

    std::cout << "    ✓ Factor 1: Manifest Sealing... ";
    if (!mgr.validateTriFactorIntegrity(error)) {
        result.error_message = error;
        auto end = std::chrono::high_resolution_clock::now();
        result.elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        return result;
    }

    std::cout << "PASS\n";
    std::cout << "    ✓ Factor 2: Snapshot Against Disk (hash validation)... PASS\n";
    std::cout << "    ✓ Factor 3: Manifest Artifacts vs Snapshot... PASS\n";

    result.passed = true;
    auto end = std::chrono::high_resolution_clock::now();
    result.elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    return result;
}

// ============================================================================
// Test: Subdivision Table Building
// ============================================================================

TestResult testSubdivisionTableBuilding() {
    TestResult result;
    result.test_name = "Subdivision Table Building (Phase 1)";
    result.passed = false;

    auto start = std::chrono::high_resolution_clock::now();

    ApertureManager mgr;
    std::string dist_root = "D:\\dist-archives\\v1.2.5-fused-SEALED-20260403-203241";
    std::string error;

    if (!mgr.loadSealedManifest(dist_root, error)
        || !mgr.loadRecursiveSnapshot(dist_root, error)
        || !mgr.validateTriFactorIntegrity(error)) {
        result.error_message = "Validation failed: " + error;
        auto end = std::chrono::high_resolution_clock::now();
        result.elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        return result;
    }

    uint64_t max_aperture = 2ULL * 1024ULL * 1024ULL * 1024ULL; // 2GB
    if (!mgr.buildSubdivisionTable(max_aperture, error)) {
        result.error_message = error;
        auto end = std::chrono::high_resolution_clock::now();
        result.elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        return result;
    }

    const auto& table = mgr.getSubdivisionTable();
    std::cout << "    ✓ Subdivision Entries: " << table.entries.size() << "\n";
    std::cout << "    ✓ Total Mapped Bytes: " << table.total_mapped_bytes << "\n";
    std::cout << "    ✓ Max Aperture: " << max_aperture << "\n";

    result.passed = true;
    auto end = std::chrono::high_resolution_clock::now();
    result.elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    return result;
}

// ============================================================================
// Test: Convenience Builder
// ============================================================================

TestResult testConvenienceBuilder() {
    TestResult result;
    result.test_name = "Convenience Builder (buildApertureFromManifest)";
    result.passed = false;

    auto start = std::chrono::high_resolution_clock::now();

    std::string dist_root = "D:\\dist-archives\\v1.2.5-fused-SEALED-20260403-203241";
    uint64_t max_aperture = 2ULL * 1024ULL * 1024ULL * 1024ULL;
    std::string error;

    auto mgr = buildApertureFromManifest(dist_root, max_aperture, error);
    if (!mgr) {
        result.error_message = error;
        auto end = std::chrono::high_resolution_clock::now();
        result.elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        return result;
    }

    std::cout << "    ✓ Full pipeline completed successfully\n";
    std::cout << "    ✓ Manifest Version: " << mgr->getManifest().version << "\n";
    std::cout << "    ✓ Snapshot Entries: " << mgr->getSnapshot().size() << "\n";

    result.passed = true;
    auto end = std::chrono::high_resolution_clock::now();
    result.elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    return result;
}

// ============================================================================
// Main Test Suite
// ============================================================================

int main() {
    std::cout << "\n========================================\n";
    std::cout << "RawrXD ApertureManager Integration Tests\n";
    std::cout << "========================================\n\n";

    std::vector<TestResult> results;

    results.push_back(testLoadSealedManifest());
    std::cout << "\n";
    results.push_back(testLoadRecursiveSnapshot());
    std::cout << "\n";
    results.push_back(testTriFactorIntegrity());
    std::cout << "\n";
    results.push_back(testSubdivisionTableBuilding());
    std::cout << "\n";
    results.push_back(testConvenienceBuilder());

    std::cout << "\n========================================\n";
    std::cout << "Test Summary\n";
    std::cout << "========================================\n\n";

    int passed = 0;
    int failed = 0;
    for (const auto& result : results) {
        printTestResult(result);
        if (result.passed) {
            passed++;
        } else {
            failed++;
        }
    }

    std::cout << "\nTotal: " << passed << " passed, " << failed << " failed\n";
    std::cout << "Status: " << (failed == 0 ? "✓ ALL TESTS PASSED" : "✗ SOME TESTS FAILED") << "\n\n";

    return failed == 0 ? 0 : 1;
}
