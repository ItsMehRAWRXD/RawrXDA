#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <memory>
#include <functional>
#include <unordered_map>

// ============================================================================
// CATEGORY 3: AI Engines (~20 symbols)
// ============================================================================

// ============================================================================
// AgenticDeepThinkingEngine
// ============================================================================

struct ThinkingContext {
    std::string initial_prompt;
    uint32_t max_thinking_steps;
    uint32_t max_tokens;
    float temperature;
    uint32_t num_agents;
};

struct ThinkingResult {
    std::string final_response;
    std::vector<std::string> reasoning_steps;
    uint32_t total_tokens_used;
    bool success;
    std::string error_message;
};

struct ThinkingStats {
    uint32_t total_invocations;
    uint32_t average_steps;
    uint64_t total_tokens;
    uint64_t total_time_ms;
};

struct MultiAgentResult {
    std::string consensus_response;
    std::vector<std::string> individual_responses;
    uint32_t voting_rounds;
    bool success;
};

class AgenticDeepThinkingEngine {
public:
    AgenticDeepThinkingEngine();
    ~AgenticDeepThinkingEngine();
    
    AgenticDeepThinkingEngine(const AgenticDeepThinkingEngine&) = delete;
    AgenticDeepThinkingEngine& operator=(const AgenticDeepThinkingEngine&) = delete;

    ThinkingResult think(const ThinkingContext& context);
    MultiAgentResult thinkMultiAgent(const ThinkingContext& context);
    
    ThinkingStats getStats() const;
    void clearMemory();
    void saveThinkingResult(const std::string& session_id, const ThinkingResult& result);
};

// ============================================================================
// DeepIterationEngine
// ============================================================================

class AgenticEngine;
class SubAgentManager;

struct DeepIterationConfig {
    uint32_t max_iterations;
    uint32_t max_depth;
    float threshold;
    std::string strategy;
};

struct DeepIterationStats {
    uint32_t completed_iterations;
    uint32_t max_depth_reached;
    uint64_t total_time_ms;
    uint32_t convergence_step;
};

class DeepIterationEngine {
public:
    DeepIterationEngine(const DeepIterationEngine&) = delete;
    DeepIterationEngine& operator=(const DeepIterationEngine&) = delete;

    static DeepIterationEngine& instance();
    
    bool run(const std::string& prompt, const std::string& model_name, std::string* output);
    
    DeepIterationConfig getConfig() const;
    void setConfig(const DeepIterationConfig& config);
    
    const DeepIterationStats& getStats() const;
    std::string getStatusString() const;
    
    void setAgenticEngine(AgenticEngine* engine);
    void setSubAgentManager(SubAgentManager* manager);

private:
    DeepIterationEngine() = default;
    ~DeepIterationEngine() = default;
};

// ============================================================================
// rawrxd::inference::AutonomousInferenceEngine
// ============================================================================

namespace rawrxd {
namespace inference {

struct InferenceConfig {
    std::string model_path;
    uint32_t batch_size;
    uint32_t max_sequence_length;
    float temperature;
    bool use_cache;
};

class AutonomousInferenceEngine {
public:
    explicit AutonomousInferenceEngine(const InferenceConfig& config);
    ~AutonomousInferenceEngine();
    
    AutonomousInferenceEngine(const AutonomousInferenceEngine&) = delete;
    AutonomousInferenceEngine& operator=(const AutonomousInferenceEngine&) = delete;

    bool loadModelAutomatic(const std::string& model_identifier);
    void infer(const std::vector<int>& token_ids, 
               std::function<void(const std::string&)> callback,
               uint64_t max_inference_time_ms);
};

}  // namespace inference
}  // namespace rawrxd

// ============================================================================
// RawrXD::ModelSourceResolver
// ============================================================================

namespace RawrXD {

struct OllamaBlobInfo {
    std::string blob_name;
    uint64_t size_bytes;
    std::string digest;
    std::string path;
};

struct ResolvedModelPath {
    std::string absolute_path;
    std::string model_type;
    bool is_local;
    uint64_t size_bytes;
};

struct ModelDownloadProgress {
    uint64_t bytes_downloaded;
    uint64_t total_bytes;
    float progress_percent;
    std::string current_file;
    int eta_seconds;
};

enum class ModelSourceType : unsigned char {
    Source_Local = 0,
    Source_Ollama = 1,
    Source_HuggingFace = 2,
    Source_Unknown = 3
};

class ModelSourceResolver {
public:
    ModelSourceResolver();
    ~ModelSourceResolver();
    
    ModelSourceResolver(const ModelSourceResolver&) = delete;
    ModelSourceResolver& operator=(const ModelSourceResolver&) = delete;

    ResolvedModelPath Resolve(
        const std::string& model_identifier,
        std::function<void(const ModelDownloadProgress&)> progress_callback);
    
    ModelSourceType DetectSourceType(const std::string& identifier) const;
    std::vector<OllamaBlobInfo> FindOllamaBlobs();
};

}  // namespace RawrXD

// ============================================================================
// RawrXD::ReverseEngineering::BinaryAnalyzer
// ============================================================================

namespace RawrXD {
namespace ReverseEngineering {

struct BinaryInfo {
    std::string file_path;
    std::string architecture;
    uint64_t file_size;
    uint32_t section_count;
    bool is_pe;
    bool is_64bit;
};

class BinaryAnalyzer {
public:
    static BinaryInfo AnalyzePE(const std::string& file_path);
    static std::vector<unsigned char> ExtractSection(const std::string& file_path, 
                                                      const std::string& section_name);
    static std::string GenerateReport(const BinaryInfo& info);
};

// ============================================================================
// RawrXD::ReverseEngineering::NativeDisassembler
// ============================================================================

struct Instruction {
    uint64_t address;
    std::string mnemonic;
    std::string operands;
    std::vector<unsigned char> bytes;
};

struct Function {
    uint64_t start_address;
    uint64_t end_address;
    std::string name;
    std::vector<Instruction> instructions;
};

class NativeDisassembler {
public:
    static std::vector<Instruction> DisassembleX64(const unsigned char* code, 
                                                     uint64_t code_size, 
                                                     uint64_t base_address);
    
    static std::vector<Function> AnalyzeFunctions(const std::vector<Instruction>& instructions);
    
    static std::vector<std::string> ExtractStrings(const unsigned char* data, uint64_t data_size);
    
    static std::unordered_map<std::string, uint64_t> AnalyzeExports(const std::string& file_path);
    static std::unordered_map<std::string, uint64_t> AnalyzeImports(const std::string& file_path);
};

// ============================================================================
// RawrXD::ReverseEngineering::RECodex
// ============================================================================

struct Pattern {
    std::string pattern_hex;
    std::string pattern_name;
    std::string description;
    uint32_t confidence;
};

class RECodex {
public:
    static std::string AnalyzeWithAI(const std::string& disassembly, 
                                      const std::string& context);
    
    static std::vector<Pattern> GetCompilerPatterns();
    static std::vector<Pattern> GetMalwarePatterns();
    
    static std::vector<std::pair<uint64_t, std::string>> ScanForPatterns(
        const unsigned char* data, 
        uint64_t data_size,
        const std::vector<Pattern>& patterns);
};

// ============================================================================
// RawrXD::ReverseEngineering::NativeCompiler
// ============================================================================

struct CompileOptions {
    std::string target_architecture;
    std::string optimization_level;
    bool generate_debug_info;
    std::vector<std::string> include_paths;
};

struct CompileResult {
    std::vector<unsigned char> machine_code;
    std::string assembly_output;
    bool success;
    std::string error_message;
};

class NativeCompiler {
public:
    static CompileResult CompileToNative(const std::string& high_level_code, 
                                          const CompileOptions& options);
};

}  // namespace ReverseEngineering
}  // namespace RawrXD

// ============================================================================
// Supporting classes
// ============================================================================

class AgenticEngine {
public:
    virtual ~AgenticEngine() = default;
};

class SubAgentManager {
public:
    virtual ~SubAgentManager() = default;
    virtual std::string executeSwarm(const std::string& prompt, 
                                      const std::vector<std::string>& agents,
                                      const void* config) = 0;
};
