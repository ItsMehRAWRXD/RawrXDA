// D:\rawrxd\generator\generator.hpp
// Core Generator Interface - Sovereign Code Generation System

#pragma once
#include "ast.hpp"
#include "code_builder.hpp"
#include "file_emitter.hpp"
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <expected>

namespace RawrXD::CodeGen {

struct GeneratorConfig {
    std::string output_directory = "./generated";
    std::string namespace_name = "RawrXD::Generated";
    bool generate_header = true;
    bool generate_source = true;
    bool use_modules = false;           // C++20 modules support
    bool atomic_transactions = true;    // Wrap in atomic scope
    size_t max_file_lines = 5000;       // Split large files
};

struct GeneratedFile {
    std::string relative_path;   // e.g., "core/layer_offload.hpp"
    std::string content;
    bool is_header;
};

enum class GeneratorError {
    SchemaNotFound,
    InvalidAST,
    FileWriteFailed,
    CircularDependency,
    UnsupportedFeature
};

class IGenerator {
public:
    virtual ~IGenerator() = default;
    virtual std::expected<std::vector<GeneratedFile>, GeneratorError> 
        Generate(const AST::Module& schema) = 0;
    virtual void SetConfig(const GeneratorConfig& config) = 0;
};

class SovereignGenerator : public IGenerator {
public:
    explicit SovereignGenerator(const GeneratorConfig& config = {});
    ~SovereignGenerator() override;
    
    std::expected<std::vector<GeneratedFile>, GeneratorError> 
        Generate(const AST::Module& schema) override;
    
    void SetConfig(const GeneratorConfig& config) override { config_ = config; }
    
    // Incremental generation (only changed files)
    std::expected<std::vector<GeneratedFile>, GeneratorError>
        GenerateIncremental(const AST::Module& schema, 
                           const std::vector<std::string>& changed_types);
    
    // Self-hosting: generate the generator itself
    static std::expected<std::vector<GeneratedFile>, GeneratorError>
        GenerateSelf(const GeneratorConfig& config);

private:
    GeneratorConfig config_;
    std::unique_ptr<CodeBuilder> builder_;
    std::unique_ptr<FileEmitter> emitter_;
    
    std::expected<void, GeneratorError> ValidateAST(const AST::Module& schema);
    std::expected<GeneratedFile, GeneratorError> GenerateHeader(const AST::Type& type);
    std::expected<GeneratedFile, GeneratorError> GenerateSource(const AST::Type& type);
    std::string GenerateMethodSignature(const AST::Method& method, bool is_header);
    std::string GenerateMethodBody(const AST::Method& method, const std::string& class_name);
    
    // Atomic transaction wrapper generation
    std::string WrapInAtomicScope(const std::string& code, const std::string& tx_name);
    
    // Memory barrier insertion for Sovereign safety
    std::string InsertMemoryBarrier(const std::string& location);
};

} // namespace RawrXD::CodeGen
