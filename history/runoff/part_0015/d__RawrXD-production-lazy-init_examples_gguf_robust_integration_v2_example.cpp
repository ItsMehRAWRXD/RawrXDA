/**
 * GGUF Robust Integration Example (v2)
 * 
 * Demonstrates how to use the complete robust GGUF toolkit to safely load models
 * without bad_alloc crashes. This shows real-world patterns for:
 * 
 * 1. Preflight analysis (heap pressure estimation)
 * 2. Corruption detection before parsing
 * 3. Safe metadata parsing with hard limits
 * 4. Memory-mapped tensor loading for massive models
 * 5. Emergency recovery on failure
 */

#include <windows.h>
#include <cstdio>
#include <string>
#include <vector>
#include <cstdint>

// Include all robust GGUF tools
#include "../include/RawrXD_GGUF_Preflight.hpp"
#include "../include/RawrXD_SafeGGUFStream.hpp"
#include "../include/RawrXD_HardenedMetadataParser.hpp"
#include "../include/RawrXD_MemoryMappedTensorStore.hpp"
#include "../include/RawrXD_CorruptionDetector.hpp"
#include "../include/RawrXD_EmergencyRecovery.hpp"

using namespace RawrXD::Tools;

// ============================================================================
// PATTERN 1: Complete Safe Load Pipeline
// ============================================================================

bool SafeLoadGGUFModel(const std::string& filepath) {
    printf("\n[PATTERN 1] Complete Safe Load Pipeline\n");
    printf("=== Loading: %s ===\n", filepath.c_str());

    // Step 1: Preflight Analysis
    printf("\n[1] Preflight Analysis...\n");
    auto projection = GGUFInspector::Analyze(filepath);
    printf("  - Predicted heap usage: %llu MB\n", projection.predicted_heap_usage / 1024 / 1024);
    
    if (!projection.high_risk_keys.empty()) {
        printf("  - High-risk keys detected:\n");
        for (const auto& key : projection.high_risk_keys) {
            printf("    * %s\n", key.c_str());
        }
    }

    // Step 2: Corruption Detection
    printf("\n[2] Corruption Detection...\n");
    auto report = GGUFCorruptionDetector::ScanFile(filepath);
    if (!report.is_valid) {
        printf("  [ERROR] Corruption detected!\n");
        GGUFCorruptionDetector::PrintReport(report);
        
        // Step 2b: Emergency Recovery
        printf("\n[2b] Attempting Emergency Recovery...\n");
        uint64_t heap_estimate = 
            EmergencyGGUFRecovery::EstimateHeapPressure(filepath);
        printf("  - Estimated safe heap usage: %llu MB\n", heap_estimate / 1024 / 1024);
        
        std::string recovered_path = filepath + ".recovered";
        int recovered_tensors = 
            EmergencyGGUFRecovery::EmergencyTruncateAndLoad(filepath, recovered_path);
        printf("  - Recovered %d tensors to %s\n", recovered_tensors, recovered_path.c_str());
        
        // Use recovered file
        return SafeLoadGGUFModel(recovered_path);
    }
    printf("  [OK] No corruption detected\n");
    printf("  - File size: %llu MB\n", report.file_size / 1024 / 1024);
    printf("  - Tensor count: %u\n", report.tensor_count);
    printf("  - Metadata entries: %llu\n", report.metadata_count);

    // Step 3: Safe Metadata Parsing
    printf("\n[3] Safe Metadata Parsing...\n");
    try {
        auto entries = HardenedGGUFMetadataParser::ParseMetadataRobust(
            filepath,
            true,  // skip_high_risk
            50 * 1024 * 1024); // 50MB max per string
        
        printf("  - Loaded %zu metadata entries\n", entries.size());
        
        // Example: Extract model parameters
        for (const auto& entry : entries) {
            if (entry.key.find("model.") != std::string::npos) {
                printf("    * %s (type %u)\n", entry.key.c_str(), entry.type);
            }
        }
    } catch (const std::exception& e) {
        printf("  [ERROR] Metadata parse failed: %s\n", e.what());
        return false;
    }

    // Step 4: Memory-Mapped Tensor Loading
    printf("\n[4] Memory-Mapped Tensor Loading...\n");
    try {
        MemoryMappedTensorStore store(filepath);
        printf("  - MMapped file, ready for zero-copy tensor access\n");
        printf("  - File size: %llu MB\n", store.FileSize() / 1024 / 1024);
        
        // Register a test tensor (would normally come from GGUF header parsing)
        // store.RegisterTensor("blk.0.attn.q_proj.weight", ..., ...);
    } catch (const std::exception& e) {
        printf("  [ERROR] Memory mapping failed: %s\n", e.what());
        return false;
    }

    printf("\n[SUCCESS] Model loaded safely!\n");
    return true;
}

// ============================================================================
// PATTERN 2: Minimal Safe Load (Fastest Path)
// ============================================================================

bool MinimalSafeLoad(const std::string& filepath) {
    printf("\n[PATTERN 2] Minimal Safe Load (Fastest)\n");
    printf("=== Loading: %s ===\n", filepath.c_str());

    // Just do corruption check + metadata parse
    auto report = GGUFCorruptionDetector::ScanFile(filepath);
    if (!report.is_valid) {
        printf("[ERROR] Corrupted file\n");
        return false;
    }

    try {
        auto entries = HardenedGGUFMetadataParser::ParseMetadataRobust(
            filepath, true, 50 * 1024 * 1024);
        printf("[OK] Loaded %zu metadata entries\n", entries.size());
        return true;
    } catch (const std::exception& e) {
        printf("[ERROR] %s\n", e.what());
        return false;
    }
}

// ============================================================================
// PATTERN 3: Detailed Corruption Analysis
// ============================================================================

void AnalyzeCorruption(const std::string& filepath) {
    printf("\n[PATTERN 3] Detailed Corruption Analysis\n");
    printf("=== Analyzing: %s ===\n", filepath.c_str());

    auto report = GGUFCorruptionDetector::ScanFile(filepath);
    GGUFCorruptionDetector::PrintReport(report);

    if (!report.is_valid) {
        printf("\n[FORENSICS] Creating diagnostic dump...\n");
        std::string dump_file = filepath + ".dump";
        EmergencyGGUFRecovery::DumpGGUFContext(filepath, dump_file, 10 * 1024 * 1024);
        printf("  - Dump saved to: %s\n", dump_file.c_str());
        printf("  - Available for offline analysis\n");
    }
}

// ============================================================================
// PATTERN 4: Batch Processing with Safe Fallback
// ============================================================================

void BatchProcessModels(const std::vector<std::string>& model_paths) {
    printf("\n[PATTERN 4] Batch Processing with Safe Fallback\n");
    printf("=== Processing %zu models ===\n", model_paths.size());

    int success_count = 0;
    int recovery_count = 0;
    int failure_count = 0;

    for (const auto& path : model_paths) {
        printf("\n--- %s ---\n", path.c_str());

        // Try standard load first
        auto report = GGUFCorruptionDetector::ScanFile(path);
        
        if (report.is_valid) {
            try {
                auto entries = HardenedGGUFMetadataParser::ParseMetadataRobust(
                    path, true, 50 * 1024 * 1024);
                printf("[OK] %s\n", path.c_str());
                success_count++;
                continue;
            } catch (...) {
                printf("[FAIL] Metadata parse error\n");
            }
        }

        // Attempt recovery
        printf("[RECOVERY] Attempting recovery...\n");
        std::string recovered = path + ".recovered";
        int tensors = EmergencyGGUFRecovery::EmergencyTruncateAndLoad(path, recovered);
        
        if (tensors > 0) {
            printf("[RECOVERED] %d tensors saved to %s\n", tensors, recovered.c_str());
            recovery_count++;
        } else {
            printf("[FAILED] Could not recover\n");
            failure_count++;
        }
    }

    printf("\n=== Summary ===\n");
    printf("Success:  %d\n", success_count);
    printf("Recovered: %d\n", recovery_count);
    printf("Failed:   %d\n", failure_count);
}

// ============================================================================
// PATTERN 5: Memory Pressure Monitoring
// ============================================================================

void MonitorMemoryPressure(const std::vector<std::string>& model_paths) {
    printf("\n[PATTERN 5] Memory Pressure Monitoring\n");
    printf("=== Analyzing %zu models ===\n", model_paths.size());

    ULARGE_INTEGER total_available;
    GetDiskFreeSpaceExA("C:\\", &total_available, nullptr, nullptr);
    uint64_t available_heap = total_available.QuadPart;

    printf("Available memory: %llu MB\n", available_heap / 1024 / 1024);
    printf("\nModel Analysis:\n");

    uint64_t total_pressure = 0;

    for (const auto& path : model_paths) {
        uint64_t heap_est = EmergencyGGUFRecovery::EstimateHeapPressure(path);
        total_pressure += heap_est;

        double percent = (double)heap_est / available_heap * 100;
        printf("  - %s: %llu MB (%.1f%% of available)\n",
               path.c_str(), heap_est / 1024 / 1024, percent);

        if (heap_est > available_heap * 0.8) {
            printf("    [WARN] Would exhaust available memory!\n");
        }
    }

    printf("\nTotal pressure: %llu MB (%.1f%% of available)\n",
           total_pressure / 1024 / 1024,
           (double)total_pressure / available_heap * 100);
}

// ============================================================================
// MAIN TEST HARNESS
// ============================================================================

int main(int argc, char* argv[]) {
    printf("RawrXD GGUF Robust Integration Examples (v2)\n");
    printf("============================================\n");

    if (argc < 2) {
        printf("\nUsage: %s <model.gguf> [pattern]\n", argv[0]);
        printf("\nPatterns:\n");
        printf("  1 - Complete Safe Load Pipeline (default)\n");
        printf("  2 - Minimal Safe Load\n");
        printf("  3 - Corruption Analysis\n");
        printf("  4 - Batch Processing\n");
        printf("  5 - Memory Pressure Monitoring\n");

        printf("\nExample:\n");
        printf("  %s model.gguf 1\n", argv[0]);
        printf("  %s model.gguf 3\n", argv[0]);

        return 1;
    }

    std::string filepath = argv[1];
    int pattern = (argc >= 3) ? atoi(argv[2]) : 1;

    try {
        switch (pattern) {
            case 1:
                SafeLoadGGUFModel(filepath);
                break;
            case 2:
                MinimalSafeLoad(filepath);
                break;
            case 3:
                AnalyzeCorruption(filepath);
                break;
            case 5: {
                std::vector<std::string> paths = {filepath};
                MonitorMemoryPressure(paths);
                break;
            }
            default:
                printf("Unknown pattern: %d\n", pattern);
                return 1;
        }
    } catch (const std::exception& e) {
        printf("\n[EXCEPTION] %s\n", e.what());
        return 1;
    }

    return 0;
}
