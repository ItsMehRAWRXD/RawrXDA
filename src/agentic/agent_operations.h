// ============================================================================
// agent_operations.h
// Production Agent Operations - Complete Backend Implementations
// Unified interface for all 7 agent capabilities with dual-mode access
// ============================================================================

#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>


namespace RawrXD::Agentic
{

// ============================================================================
// Agent Operation Results
// ============================================================================

struct AgentOperationResult
{
    bool success = false;
    std::string output;
    std::string error_message;
    int64_t duration_ms = 0;
    std::map<std::string, std::string> metadata;  // Tool-specific metadata
};

// ============================================================================
// 1. CONTEXT COMPACTION OPERATION
// ============================================================================

struct CompactConversationParams
{
    std::string text;
    size_t target_tokens = 2048;
    bool preserve_semantics = true;
};

class ContextCompactor
{
  public:
    static AgentOperationResult compact(const CompactConversationParams& params);

  private:
    static size_t estimateTokens(const std::string& text);
    static std::string removeRedundantWhitespace(const std::string& text);
    static std::string collapseRepeatedSections(const std::string& text);
};

// ============================================================================
// 2. TOOL OPTIMIZER OPERATION
// ============================================================================

struct ToolOptimizerParams
{
    std::string current_intent;
    std::vector<std::string> available_tools;
    std::vector<std::pair<std::string, float>> historical_success_rates;
};

struct ToolRecommendation
{
    std::string tool_name;
    float relevance_score = 0.0f;
    std::string reasoning;
};

class ToolOptimizer
{
  public:
    static AgentOperationResult optimize(const ToolOptimizerParams& params);
    static std::vector<ToolRecommendation> rankTools(const std::string& intent,
                                                     const std::vector<std::string>& available_tools);

  private:
    static float scoreToolRelevance(const std::string& tool, const std::string& intent);
};

// ============================================================================
// 3. SYMBOL RESOLVER OPERATION
// ============================================================================

struct SymbolInfo
{
    std::string name;
    std::string filepath;
    size_t line = 0;
    size_t column = 0;
    std::string symbol_type;  // "class", "function", "variable", etc.
    std::string scope;        // namespace/class context
};

struct SymbolResolverParams
{
    std::string symbol_name;
    std::string context_file;
    std::vector<std::string> search_paths;
};

class SymbolResolver
{
  public:
    static AgentOperationResult resolve(const SymbolResolverParams& params);
    static std::vector<SymbolInfo> findSymbols(const std::string& symbol_name,
                                               const std::vector<std::string>& search_paths);

  private:
    static bool isValidSymbolChar(char c);
    static std::vector<SymbolInfo> searchCppFiles(const std::string& symbol_name,
                                                  const std::vector<std::string>& paths);
};

// ============================================================================
// 4. TARGETED FILE READER OPERATION
// ============================================================================

struct FileReaderParams
{
    std::string filepath;
    size_t start_line = 0;
    size_t end_line = 0;  // 0 = read to end
    bool include_line_numbers = true;
    size_t max_tokens_per_read = 4096;
};

class TargetedFileReader
{
  public:
    static AgentOperationResult readFileSlice(const FileReaderParams& params);
    static std::string readLinesWithContext(const std::string& filepath, size_t start_line, size_t end_line,
                                            size_t context_lines = 3);

  private:
    static size_t countTokensInText(const std::string& text);
    static std::vector<std::string> readFileLines(const std::string& filepath);
};

// ============================================================================
// 5. CODE EXPLORATION PLANNER OPERATION
// ============================================================================

struct CodeExplorationPlan
{
    std::string entry_point;
    std::vector<std::string> dependencies;
    std::vector<std::string> suggested_files;
    std::string exploration_strategy;  // "breadth-first", "depth-first", "risk-aware"
};

struct ExplorationPlannerParams
{
    std::string root_path;
    std::string initial_query;
    std::vector<std::string> file_extensions;
};

class CodeExplorationPlanner
{
  public:
    static AgentOperationResult planExploration(const ExplorationPlannerParams& params);
    static CodeExplorationPlan generatePlan(const std::string& root_path, const std::string& query);

  private:
    static std::vector<std::string> findRelevantFiles(const std::string& root_path, const std::string& query);
    static std::vector<std::string> buildDependencyGraph(const std::vector<std::string>& files);
};

// ============================================================================
// 6. FILE SEARCH OPERATION
// ============================================================================

struct FileSearchParams
{
    std::string search_pattern;  // glob or regex
    std::vector<std::string> root_paths;
    std::vector<std::string> exclude_patterns;
    bool use_regex = false;
    size_t max_results = 1000;
};

struct FileSearchResult
{
    std::string filepath;
    size_t match_line = 0;
    std::string match_context;
    float relevance_score = 0.0f;
};

class FileSearcher
{
  public:
    static AgentOperationResult search(const FileSearchParams& params);
    static std::vector<FileSearchResult> findMatches(const std::string& pattern,
                                                     const std::vector<std::string>& root_paths,
                                                     bool use_regex = false);

  private:
    static bool patternMatches(const std::string& filename, const std::string& pattern);
    static bool isExcluded(const std::string& filepath, const std::vector<std::string>& excludes);
};

// ============================================================================
// 7. CHECKPOINT MANAGER OPERATION
// ============================================================================

struct CheckpointInfo
{
    std::string checkpoint_id;
    std::string description;
    std::vector<std::string> files_in_checkpoint;
    std::string created_at;
};

struct CheckpointManagerParams
{
    std::string action;  // "create", "restore", "list", "delete"
    std::string checkpoint_id;
    std::string root_path;
    std::string description;
};

class CheckpointManager
{
  public:
    static AgentOperationResult executeCheckpointAction(const CheckpointManagerParams& params);
    static std::string createCheckpoint(const std::string& root_path, const std::string& description);
    static bool restoreCheckpoint(const std::string& checkpoint_id, const std::string& root_path);
    static std::vector<CheckpointInfo> listCheckpoints();

  private:
    static std::string getCheckpointDir();
    static std::string computeFileHash(const std::string& filepath);
};

// ============================================================================
// UNIFIED AGENT OPERATIONS HANDLER
// ============================================================================

class AgentOperations
{
  public:
    // Register all operations
    static void initializeOperations();

    // Execute any agent operation by name
    static AgentOperationResult executeOperation(const std::string& operation_name, const std::string& params_json);

    // Get list of available operations
    static std::vector<std::string> listAvailableOperations();

    // Get operation description
    static std::string getOperationDescription(const std::string& operation_name);

  private:
    using OperationHandler = std::function<AgentOperationResult(const std::string&)>;
    static std::map<std::string, OperationHandler>& operationRegistry();
};

}  // namespace RawrXD::Agentic
