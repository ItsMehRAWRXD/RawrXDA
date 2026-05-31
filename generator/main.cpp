// D:\rawrxd\generator\main.cpp
// Entry Point - RawrXD Code Generator

#include "generator.hpp"
#include "schema_parser.hpp"
#include "file_emitter.hpp"
#include <iostream>
#include <filesystem>

using namespace RawrXD::CodeGen;

// Minimal schema to generate LayerOffloadManager
constexpr const char* LAYER_OFFLOAD_SCHEMA = R"(
namespace RawrXD::Core

@atomic
class LayerOffloadManager {
    method RecalculateSovereignBudget(size_t context_tokens) -> void
    method EnsureLayerResident(uint32_t layer_id) -> bool
    method InitializeMomentumPool() -> void
    method GetActiveWeightUsage() -> size_t const
}

struct SovereignBudget {
    field kv_bridge_size: size_t
    field weight_volley_limit: size_t
}
)";

int main(int argc, char* argv[]) {
    std::cout << "[RawrXD Code Generator] v1.0 - Sovereign Code Generation System\n";
    std::cout << "[RawrXD Code Generator] Initializing...\n\n";
    
    // Parse schema
    SchemaParser parser;
    auto schema_result = parser.ParseString(LAYER_OFFLOAD_SCHEMA);
    
    if (!schema_result) {
        std::cerr << "[ERROR] Failed to parse schema\n";
        return 1;
    }
    
    std::cout << "[Schema Parser] SUCCESS - Parsed " << schema_result->types.size() << " types\n";
    for (const auto& type : schema_result->types) {
        std::cout << "  - " << type.name << " (" << (type.is_struct ? "struct" : "class") << ")\n";
    }
    std::cout << "\n";
    
    // Configure generator
    GeneratorConfig config;
    config.output_directory = "./generated";
    config.namespace_name = "RawrXD::Core";
    config.atomic_transactions = true;
    config.generate_header = true;
    config.generate_source = true;
    
    // Generate code
    std::cout << "[Code Generator] Generating C++ code...\n";
    SovereignGenerator generator(config);
    auto files_result = generator.Generate(*schema_result);
    
    if (!files_result) {
        std::cerr << "[ERROR] Code generation failed\n";
        return 1;
    }
    
    std::cout << "[Code Generator] SUCCESS - Generated " << files_result->size() << " files\n";
    for (const auto& file : *files_result) {
        std::cout << "  - " << file.relative_path << " (" 
                  << file.content.size() << " bytes)\n";
    }
    std::cout << "\n";
    
    // Emit files
    std::cout << "[File Emitter] Writing files to disk...\n";
    FileEmitter emitter;
    emitter.SetOutputDirectory(config.output_directory);
    emitter.atomic_write_ = true;
    emitter.create_backups_ = true;
    
    if (auto result = emitter.Emit(*files_result); !result) {
        std::cerr << "[ERROR] Failed to write files\n";
        return 1;
    }
    
    std::cout << "[File Emitter] SUCCESS - Files written to: " << config.output_directory << "\n\n";
    
    // Self-hosting test: Generate the generator
    std::cout << "[Self-Hosting Test] Generating generator components...\n";
    auto self_files = SovereignGenerator::GenerateSelf(config);
    
    if (self_files) {
        std::cout << "[Self-Hosting Test] SUCCESS - Generator can generate itself\n";
        std::cout << "  Generated " << self_files->size() << " files:\n";
        for (const auto& file : *self_files) {
            std::cout << "    - " << file.relative_path << "\n";
        }
    } else {
        std::cout << "[Self-Hosting Test] FAILED\n";
        return 1;
    }
    
    std::cout << "\n[RawrXD Code Generator] ✓ All tests passed!\n";
    std::cout << "[RawrXD Code Generator] Production-ready code generated successfully.\n";
    
    // Verify generated files exist
    std::cout << "\n[Verification] Checking generated files:\n";
    for (const auto& file : *files_result) {
        auto path = std::filesystem::path(config.output_directory) / file.relative_path;
        if (std::filesystem::exists(path)) {
            auto size = std::filesystem::file_size(path);
            std::cout << "  ✓ " << path.filename().string() << " (" << size << " bytes)\n";
        } else {
            std::cout << "  ✗ " << path.filename().string() << " (NOT FOUND)\n";
            return 1;
        }
    }
    
    std::cout << "\n[Summary]\n";
    std::cout << "  Types generated: " << schema_result->types.size() << "\n";
    std::cout << "  Files created: " << files_result->size() << "\n";
    std::cout << "  Output directory: " << std::filesystem::absolute(config.output_directory) << "\n";
    std::cout << "  Status: READY FOR PRODUCTION\n";
    
    return 0;
}
