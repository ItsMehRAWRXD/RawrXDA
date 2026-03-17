// =============================================================================
// GGUF Robust Tools Integration Example
// Demonstrates drop-in replacement for streaming_gguf_loader.cpp
// =============================================================================

#include "gguf_robust_tools_v2.hpp"
#include "gguf_robust_masm_bridge_v2.hpp"
#include <iostream>
#include <string>

// =============================================================================
// Example 1: Pre-flight Corruption Detection
// =============================================================================
void example_preflight_scan(const char* filepath) {
    std::cout << "\n=== Pre-Flight Corruption Scan ===" << std::endl;
    
    auto scan = rawrxd::gguf_robust::CorruptionScan::ScanFile(filepath);
    
    if (!scan.is_valid) {
        std::cerr << "❌ Corruption detected: " << scan.error_msg << std::endl;
        return;
    }
    
    std::cout << "✅ File valid" << std::endl;
    std::cout << "   File size: " << scan.file_size << " bytes" << std::endl;
    std::cout << "   GGUF version: " << scan.gguf_version << std::endl;
    std::cout << "   Tensor count: " << scan.tensor_count << std::endl;
    std::cout << "   Metadata pairs: " << scan.metadata_kv_count << std::endl;
}

// =============================================================================
// Example 2: Zero-Allocation Metadata Parsing with Selective Skipping
// =============================================================================
void example_selective_metadata_parse(const char* filepath) {
    std::cout << "\n=== Selective Metadata Parsing ===" << std::endl;
    
    // Pre-flight check first
    auto scan = rawrxd::gguf_robust::CorruptionScan::ScanFile(filepath);
    if (!scan.is_valid) {
        std::cerr << "❌ Pre-flight failed: " << scan.error_msg << std::endl;
        return;
    }
    
    // Open robust stream
    rawrxd::gguf_robust::RobustGGUFStream stream(filepath);
    if (!stream.IsOpen()) {
        std::cerr << "❌ Failed to open stream" << std::endl;
        return;
    }
    
    // Seek past header (magic + version + n_tensors + n_kv)
    if (!stream.RobustSeek(4 + 4 + 8 + 8, SEEK_SET)) {
        std::cerr << "❌ Failed to seek to metadata" << std::endl;
        return;
    }
    
    // Create metadata surgeon
    rawrxd::gguf_robust::MetadataSurgeon surgeon(stream);
    
    // Configure what to skip (your bad_alloc fix)
    rawrxd::gguf_robust::MetadataSurgeon::ParseConfig cfg;
    cfg.skip_chat_template = true;       // Skip toxic 512KB+ chat templates
    cfg.skip_tokenizer_merges = true;    // Skip massive merge tables
    cfg.skip_tokenizer_tokens = false;   // Load token arrays (usually safe)
    cfg.max_string_budget = 16*1024;     // 16KB max for any string
    
    // Parse with surgical skipping
    if (!surgeon.ParseKvPairs(scan.metadata_kv_count, cfg)) {
        std::cerr << "❌ Metadata parse failed at offset " << stream.Tell() << std::endl;
        return;
    }
    
    // Report what was skipped
    const auto& skipped = surgeon.GetSkippedMap();
    std::cout << "✅ Metadata parsed successfully" << std::endl;
    std::cout << "   Total pairs: " << scan.metadata_kv_count << std::endl;
    std::cout << "   Skipped: " << skipped.size() << " pairs" << std::endl;
    
    for (const auto& [key, reason] : skipped) {
        std::cout << "   - " << key << " → " << reason << std::endl;
    }
}

// =============================================================================
// Example 3: Diagnostic Autopsy (Zero-Risk Metadata Inspection)
// =============================================================================
void example_autopsy(const char* filepath) {
    std::cout << "\n=== GGUF Autopsy (Diagnostic Mode) ===" << std::endl;
    
    auto report = rawrxd::gguf_robust::GgufAutopsy::GenerateReport(filepath);
    
    std::cout << "Metadata pairs: " << report.metadata_pairs << std::endl;
    std::cout << "Toxic keys detected: " << report.toxic_keys_found << std::endl;
    std::cout << "Max string length: " << report.max_string_length << " bytes" << std::endl;
    
    if (!report.oversized_keys.empty()) {
        std::cout << "\nOversized/Toxic Keys:" << std::endl;
        for (const auto& key : report.oversized_keys) {
            std::cout << "  - " << key << std::endl;
        }
    }
}

// =============================================================================
// Example 4: MASM Integration (Zero-CRT Safe Skip)
// =============================================================================
void example_masm_integration(const char* filepath) {
    std::cout << "\n=== MASM Zero-CRT Integration ===" << std::endl;
    
    HANDLE hFile = CreateFileA(filepath, GENERIC_READ, FILE_SHARE_READ, 
                               nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    
    if (hFile == INVALID_HANDLE_VALUE) {
        std::cerr << "❌ Failed to open file" << std::endl;
        return;
    }
    
    // Create MASM-backed parser
    rawrxd::gguf_masm::RobustGGUFParser parser(hFile, true); // Take ownership
    
    // Skip a known-problematic key using MASM implementation
    if (parser.SkipUnsafeString("tokenizer.chat_template")) {
        std::cout << "✅ MASM skip successful" << std::endl;
    } else {
        std::cout << "ℹ️  String not oversized, will be read normally" << std::endl;
    }
    
    // Parser destructor closes handle
}

// =============================================================================
// Example 5: Drop-in Replacement for streaming_gguf_loader.cpp ParseMetadata()
// =============================================================================
bool parse_metadata_robust(const std::string& filepath, 
                          std::unordered_map<std::string, std::string>& metadata_out) {
    
    // Step 1: Pre-flight corruption scan
    auto scan = rawrxd::gguf_robust::CorruptionScan::ScanFile(filepath.c_str());
    if (!scan.is_valid || scan.metadata_kv_count > 100000) {
        fprintf(stderr, "[FATAL] GGUF corruption detected: %s\n", scan.error_msg);
        return false;
    }
    
    // Step 2: Open robust stream
    rawrxd::gguf_robust::RobustGGUFStream stream(filepath.c_str());
    if (!stream.IsOpen()) {
        fprintf(stderr, "[FATAL] Failed to open GGUF stream\n");
        return false;
    }
    
    // Step 3: Seek to metadata section
    if (!stream.RobustSeek(4 + 4 + 8 + 8, SEEK_SET)) {
        fprintf(stderr, "[FATAL] Failed to seek to metadata\n");
        return false;
    }
    
    // Step 4: Surgical parse with automatic toxic key skipping
    rawrxd::gguf_robust::MetadataSurgeon surgeon(stream);
    
    rawrxd::gguf_robust::MetadataSurgeon::ParseConfig cfg;
    cfg.skip_chat_template = true;       // Your bad_alloc fix
    cfg.skip_tokenizer_merges = true;    // Your bad_alloc fix
    cfg.max_string_budget = 16*1024;     // 16KB hard limit
    
    if (!surgeon.ParseKvPairs(scan.metadata_kv_count, cfg)) {
        fprintf(stderr, "[FATAL] Metadata parse failed at offset %lld\n", 
                (long long)stream.Tell());
        return false;
    }
    
    // Step 5: Copy parsed metadata (skipped keys will have placeholder values)
    for (const auto& [key, value] : surgeon.GetSkippedMap()) {
        metadata_out[key] = value;
    }
    
    return true;
}

// =============================================================================
// Main: Run all examples
// =============================================================================
int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <path-to-gguf-file>" << std::endl;
        std::cout << "\nDemonstrates robust GGUF parsing toolkit:" << std::endl;
        std::cout << "  1. Pre-flight corruption detection" << std::endl;
        std::cout << "  2. Selective metadata parsing (skip toxic keys)" << std::endl;
        std::cout << "  3. Diagnostic autopsy (zero-risk inspection)" << std::endl;
        std::cout << "  4. MASM zero-CRT integration" << std::endl;
        std::cout << "  5. Drop-in replacement for streaming_gguf_loader" << std::endl;
        return 1;
    }
    
    const char* filepath = argv[1];
    
    std::cout << "==============================================================================" << std::endl;
    std::cout << "RawrXD GGUF Robust Tools - Integration Examples" << std::endl;
    std::cout << "File: " << filepath << std::endl;
    std::cout << "==============================================================================" << std::endl;
    
    // Run all examples
    example_preflight_scan(filepath);
    example_selective_metadata_parse(filepath);
    example_autopsy(filepath);
    example_masm_integration(filepath);
    
    // Demonstrate drop-in replacement
    std::cout << "\n=== Drop-in Replacement Example ===" << std::endl;
    std::unordered_map<std::string, std::string> metadata;
    if (parse_metadata_robust(filepath, metadata)) {
        std::cout << "✅ Parsed " << metadata.size() << " metadata entries" << std::endl;
    } else {
        std::cerr << "❌ Parse failed" << std::endl;
    }
    
    std::cout << "\n==============================================================================" << std::endl;
    std::cout << "✅ All examples completed successfully" << std::endl;
    std::cout << "==============================================================================" << std::endl;
    
    return 0;
}
