// ============================================================================
// agent_puppeteer.hpp — Multi-Agent Puppeteering Extension for RawrXD
// ============================================================================
// Extends the existing SubAgentManager with:
//   - Agent roles with system prompts (planner, coder, reviewer, analyst)
//   - Dependency graph with topological sort
//   - Result aggregation (union, consensus, synthesis)
//   - Pipeline builder fluent API
//   - Per-agent callback system
//   - Mock LLM for testing
//
// Integrates with existing:
//   - SubAgentManager (chains, swarms, todo)
//   - AgenticExecutorController (VRAM throttling, watchdog)
//   - AgenticBridge (tool call detection)
//   - Win32IDE_SubAgent (UI callbacks, streaming)
//
// No duplication — extends existing infrastructure.
// ============================================================================

#pragma once

#include "../subagent_core.h"
#include "../agent_correction_system.h"
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <functional>
#include <variant>
#include <optional>
#include <regex>

namespace rawrxd {
namespace agents {

// ============================================================================
// Agent Role Definition
// ============================================================================

enum class AgentRole : uint32_t {
    Planner   = 0,   // Strategic planning, task decomposition
    Coder     = 1,   // Implementation, code generation
    Reviewer  = 2,   // Code review, quality assurance
    Analyst   = 3,   // Data analysis, pattern detection
    Tester    = 4,   // Test generation, verification
    Architect = 5,   // System design, architecture
    Security  = 6,   // Security audit, vulnerability detection
    Optimizer = 7,   // Performance optimization
    Documenter= 8,   // Documentation generation
    Custom    = 9    // User-defined role
};

struct AgentRoleConfig {
    AgentRole role;
    std::string id;
    std::string name;
    std::string system_prompt;
    std::vector<AgentRole> dependencies;  // Which roles must run before this one
    uint32_t priority = 1;                // 0=low, 1=normal, 2=high, 3=critical
    bool enabled = true;
    uint32_t max_retries = 3;
    uint32_t timeout_seconds = 300;
    bool share_context = true;
    bool accumulate_context = true;
};

// ============================================================================
// Dependency Graph
// ============================================================================

class DependencyGraph {
public:
    void add_agent(const std::string& id, const std::vector<std::string>& dependencies);
    std::vector<std::string> topological_sort() const;
    std::vector<std::vector<std::string>> get_execution_levels() const;
    bool has_cycle() const;
    std::vector<std::string> get_dependencies(const std::string& id) const;
    bool has_agent(const std::string& id) const;
    void clear();
    size_t size() const;

private:
    std::unordered_map<std::string, std::vector<std::string>> graph_;
    std::unordered_map<std::string, int> in_degree_;
};

// ============================================================================
// Agent Result
// ============================================================================

struct PuppeteerAgentResult {
    std::string agent_id;
    std::string agent_role;
    std::string output;
    std::vector<std::string> artifacts;
    std::unordered_map<std::string, std::string> context_updates;
    double confidence = 0.0;
    uint32_t tokens_used = 0;
    std::chrono::milliseconds duration{0};
    bool success = false;
    std::string error_message;
};

// ============================================================================
// Aggregation Modes
// ============================================================================

enum class AggregationMode : uint32_t {
    Union     = 0,   // Concatenate all outputs
    Consensus = 1,   // Find common points across outputs
    Synthesis = 2,   // Combine into coherent narrative
    BestMatch = 3,   // Select highest confidence output
    Refinement= 4    // Each agent improves previous output
};

struct AggregatedResult {
    AggregationMode mode;
    std::string synthesis;
    std::string consensus;
    std::vector<PuppeteerAgentResult> all_results;
    std::vector<std::string> action_items;
    std::vector<std::string> conflicts;
    double overall_confidence = 0.0;
};

// ============================================================================
// Mock LLM for Testing
// ============================================================================

class MockLLM {
public:
    struct Response {
        std::string content;
        uint32_t tokens_generated = 0;
        bool success = true;
        std::string error;
        std::chrono::milliseconds inference_time{100};
    };

    struct Request {
        std::string prompt;
        std::string system_prompt;
        double temperature = 0.7;
        uint32_t max_tokens = 2048;
    };

    MockLLM(std::chrono::milliseconds simulated_delay = std::chrono::milliseconds(100));

    Response complete(const Request& request);
    std::future<Response> complete_async(const Request& request);

    void set_simulated_delay(std::chrono::milliseconds delay);
    void set_role_template(AgentRole role, const std::string& template_str);

private:
    std::chrono::milliseconds simulated_delay_;
    std::unordered_map<AgentRole, std::string> role_templates_;

    std::string generate_mock_response(const Request& request);
    std::string generate_planner_response(const Request& request);
    std::string generate_coder_response(const Request& request);
    std::string generate_reviewer_response(const Request& request);
    std::string generate_analyst_response(const Request& request);
    std::string generate_default_response(const Request& request);
};

// ============================================================================
// Agent Puppeteer — Main Orchestrator
// ============================================================================

class AgentPuppeteer {
public:
    struct Config {
        uint32_t max_workers = 4;
        uint32_t task_timeout_seconds = 300;
        bool enable_parallel_execution = true;
        bool enable_context_accumulation = true;
        bool enable_vram_throttling = true;  // Integrate with AgenticExecutorController
    };

    using ProgressCallback = std::function<void(const std::string& agent_id, const std::string& status)>;
    using ResultCallback = std::function<void(const PuppeteerAgentResult& result)>;
    using ErrorCallback = std::function<void(const std::string& agent_id, const std::string& error)>;

    explicit AgentPuppeteer(SubAgentManager* subagent_manager,
                            AgenticExecutorController* controller = nullptr,
                            Config config = {});
    ~AgentPuppeteer();

    // ---- Agent Registration ----
    void register_agent(const AgentRoleConfig& config);
    void register_default_agents();
    void unregister_agent(const std::string& id);
    bool has_agent(const std::string& id) const;
    std::vector<AgentRoleConfig> get_registered_agents() const;

    // ---- Task Execution ----
    std::vector<PuppeteerAgentResult> execute_task(
        const std::string& input,
        const std::vector<std::string>& agent_ids,
        AggregationMode aggregation = AggregationMode::Union
    );

    std::future<std::vector<PuppeteerAgentResult>> execute_task_async(
        const std::string& input,
        const std::vector<std::string>& agent_ids,
        AggregationMode aggregation = AggregationMode::Union
    );

    // ---- Pipeline Builder (Fluent API) ----
    class PipelineBuilder {
    public:
        explicit PipelineBuilder(AgentPuppeteer* puppeteer);

        PipelineBuilder& add(const std::string& agent_id);
        PipelineBuilder& planner();
        PipelineBuilder& coder();
        PipelineBuilder& reviewer();
        PipelineBuilder& analyst();
        PipelineBuilder& tester();
        PipelineBuilder& architect();
        PipelineBuilder& security();
        PipelineBuilder& optimizer();
        PipelineBuilder& documenter();

        std::vector<PuppeteerAgentResult> execute(const std::string& input);
        std::future<std::vector<PuppeteerAgentResult>> execute_async(const std::string& input);

    private:
        AgentPuppeteer* puppeteer_;
        std::vector<std::string> agent_ids_;
    };

    PipelineBuilder pipeline();

    // ---- Aggregation ----
    AggregatedResult aggregate_results(
        const std::vector<PuppeteerAgentResult>& results,
        AggregationMode mode
    ) const;

    // ---- Callbacks ----
    void set_progress_callback(ProgressCallback cb);
    void set_result_callback(ResultCallback cb);
    void set_error_callback(ErrorCallback cb);

    // ---- Context Management ----
    void set_global_context(const std::string& key, const std::string& value);
    std::optional<std::string> get_global_context(const std::string& key) const;
    void clear_global_context();

    // ---- Status ----
    bool is_running() const { return running_.load(); }
    size_t active_task_count() const;

    // ---- Integration with existing SubAgentManager ----
    SubAgentManager* get_subagent_manager() const { return subagent_manager_; }

private:
    SubAgentManager* subagent_manager_;
    AgenticExecutorController* controller_;
    Config config_;

    std::unordered_map<std::string, AgentRoleConfig> agents_;
    DependencyGraph dependency_graph_;

    std::unordered_map<std::string, std::string> global_context_;
    mutable std::mutex context_mutex_;

    std::atomic<bool> running_{false};
    std::atomic<size_t> active_tasks_{0};

    ProgressCallback progress_callback_;
    ResultCallback result_callback_;
    ErrorCallback error_callback_;
    mutable std::mutex callback_mutex_;

    // Execution
    std::vector<PuppeteerAgentResult> execute_agents_sequential(
        const std::string& input,
        const std::vector<std::string>& agent_ids
    );

    std::vector<PuppeteerAgentResult> execute_agents_parallel(
        const std::string& input,
        const std::vector<std::string>& agent_ids
    );

    PuppeteerAgentResult execute_single_agent(
        const std::string& input,
        const AgentRoleConfig& agent_config,
        const std::unordered_map<std::string, std::string>& context
    );

    // Aggregation helpers
    std::string synthesize_outputs(const std::vector<PuppeteerAgentResult>& results) const;
    std::string find_consensus(const std::vector<PuppeteerAgentResult>& results) const;
    std::vector<std::string> extract_action_items(const std::vector<PuppeteerAgentResult>& results) const;
    std::vector<std::string> detect_conflicts(const std::vector<PuppeteerAgentResult>& results) const;

    // Context helpers
    std::unordered_map<std::string, std::string> build_agent_context(
        const AgentRoleConfig& config,
        const std::vector<PuppeteerAgentResult>& previous_results
    ) const;

    // VRAM throttling check
    bool check_vram_available() const;

    // Logging
    void log_info(const std::string& msg) const;
    void log_error(const std::string& msg) const;
};

} // namespace agents
} // namespace rawrxd
