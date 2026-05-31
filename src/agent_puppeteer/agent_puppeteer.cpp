// ============================================================================
// agent_puppeteer.cpp — Multi-Agent Puppeteering Implementation
// ============================================================================

#include "agent_puppeteer.hpp"
#include <sstream>
#include <algorithm>
#include <future>
#include <thread>

namespace rawrxd {
namespace agents {

// ============================================================================
// DependencyGraph Implementation
// ============================================================================

void DependencyGraph::add_agent(const std::string& id,
                                 const std::vector<std::string>& dependencies) {
    graph_[id] = dependencies;
    if (in_degree_.find(id) == in_degree_.end()) {
        in_degree_[id] = 0;
    }
    in_degree_[id] = static_cast<int>(dependencies.size());

    // Ensure all dependencies are in the graph
    for (const auto& dep : dependencies) {
        if (graph_.find(dep) == graph_.end()) {
            graph_[dep] = {};
            in_degree_[dep] = 0;
        }
    }
}

std::vector<std::string> DependencyGraph::topological_sort() const {
    std::vector<std::string> result;
    std::unordered_map<std::string, int> degree = in_degree_;
    std::queue<std::string> queue;

    // Find all nodes with no dependencies
    for (const auto& [id, deg] : degree) {
        if (deg == 0) {
            queue.push(id);
        }
    }

    while (!queue.empty()) {
        std::string current = queue.front();
        queue.pop();
        result.push_back(current);

        // Find nodes that depend on current
        for (const auto& [id, deps] : graph_) {
            if (std::find(deps.begin(), deps.end(), current) != deps.end()) {
                degree[id]--;
                if (degree[id] == 0) {
                    queue.push(id);
                }
            }
        }
    }

    return result;
}

std::vector<std::vector<std::string>> DependencyGraph::get_execution_levels() const {
    std::vector<std::vector<std::string>> levels;
    std::unordered_map<std::string, int> degree = in_degree_;
    std::unordered_set<std::string> processed;

    while (processed.size() < graph_.size()) {
        std::vector<std::string> current_level;

        for (const auto& [id, deg] : degree) {
            if (deg == 0 && processed.find(id) == processed.end()) {
                current_level.push_back(id);
            }
        }

        if (current_level.empty()) {
            break; // Cycle detected or error
        }

        levels.push_back(current_level);

        for (const auto& id : current_level) {
            processed.insert(id);

            // Reduce degree for dependents
            for (const auto& [agent_id, deps] : graph_) {
                if (std::find(deps.begin(), deps.end(), id) != deps.end()) {
                    degree[agent_id]--;
                }
            }
        }
    }

    return levels;
}

bool DependencyGraph::has_cycle() const {
    auto order = topological_sort();
    return order.size() < graph_.size();
}

std::vector<std::string> DependencyGraph::get_dependencies(const std::string& id) const {
    auto it = graph_.find(id);
    if (it != graph_.end()) {
        return it->second;
    }
    return {};
}

bool DependencyGraph::has_agent(const std::string& id) const {
    return graph_.find(id) != graph_.end();
}

void DependencyGraph::clear() {
    graph_.clear();
    in_degree_.clear();
}

size_t DependencyGraph::size() const {
    return graph_.size();
}

// ============================================================================
// MockLLM Implementation
// ============================================================================

MockLLM::MockLLM(std::chrono::milliseconds simulated_delay)
    : simulated_delay_(simulated_delay) {
    // Initialize role templates
    role_templates_[AgentRole::Planner] =
        "## Execution Plan\n\n"
        "### Phase 1: Analysis\n"
        "- Parse and validate input requirements\n"
        "- Identify key components and dependencies\n"
        "- Estimate complexity and resources needed\n\n"
        "### Phase 2: Design\n"
        "- Create architectural overview\n"
        "- Define interfaces and data structures\n"
        "- Plan module organization\n\n"
        "### Phase 3: Implementation\n"
        "- Core functionality development\n"
        "- Integration points setup\n"
        "- Error handling implementation\n\n"
        "### Phase 4: Testing \u0026 Validation\n"
        "- Unit test creation\n"
        "- Integration testing\n"
        "- Performance validation\n\n"
        "### Estimated Effort: Medium\n"
        "### Risk Level: Low\n"
        "### Recommended Priority: Normal\n";

    role_templates_[AgentRole::Coder] =
        "// Implementation based on requirements\n"
        "#include <iostream>\n"
        "#include <string>\n"
        "#include <vector>\n\n"
        "class Solution {\n"
        "public:\n"
        "    void process(const std::string& input) {\n"
        "        auto data = parse_input(input);\n"
        "        auto result = transform(data);\n"
        "        output_result(result);\n"
        "    }\n\n"
        "private:\n"
        "    std::vector<std::string> parse_input(const std::string& input) {\n"
        "        std::vector<std::string> tokens;\n"
        "        // Tokenize input\n"
        "        return tokens;\n"
        "    }\n\n"
        "    std::string transform(const std::vector<std::string>& data) {\n"
        "        std::string result;\n"
        "        for (const auto& item : data) {\n"
        "            result += process_item(item) + \"\\n\";\n"
        "        }\n"
        "        return result;\n"
        "    }\n\n"
        "    std::string process_item(const std::string& item) {\n"
        "        return \"processed: \" + item;\n"
        "    }\n\n"
        "    void output_result(const std::string& result) {\n"
        "        std::cout << result << std::endl;\n"
        "    }\n"
        "};\n";

    role_templates_[AgentRole::Reviewer] =
        "## Code Review Summary\n\n"
        "### \u2713 Passed Checks\n"
        "- [x] Proper includes and namespaces\n"
        "- [x] RAII compliance\n"
        "- [x] Memory safety (no raw pointers, smart pointers used)\n"
        "- [x] Exception safety\n"
        "- [x] Const correctness\n\n"
        "### \u26a0 Suggestions\n"
        "1. Consider adding noexcept where appropriate\n"
        "2. Add documentation for public methods\n"
        "3. Consider using std::string_view for read-only string params\n\n"
        "### \u2717 Issues Found\n"
        "- Minor: Magic numbers should be constants\n"
        "- Minor: Consider extracting helper functions\n\n"
        "### Overall Assessment: APPROVED with suggestions\n"
        "### Confidence: 85%\n";

    role_templates_[AgentRole::Analyst] =
        "## Analysis Report\n\n"
        "### Input Summary\n"
        "Request processed: <input_type>\n"
        "Key components identified: 3\n"
        "Dependencies mapped: 2\n\n"
        "### Findings\n"
        "1. Primary objective clarity: HIGH\n"
        "2. Implementation complexity: MEDIUM\n"
        "3. Risk assessment: LOW\n"
        "4. Resource requirements: MODERATE\n\n"
        "### Metrics\n"
        "- Lines of code estimate: 500-750\n"
        "- Estimated time: 4-6 hours\n"
        "- Test coverage needed: 80%+\n\n"
        "### Recommendations\n"
        "- Start with core functionality\n"
        "- Implement tests alongside development\n"
        "- Consider edge cases early\n\n"
        "### Confidence Score: 87%\n";
}

MockLLM::Response MockLLM::complete(const Request& request) {
    Response response;

    // Simulate processing delay
    std::this_thread::sleep_for(simulated_delay_);

    // Generate mock response based on role
    response.content = generate_mock_response(request);
    response.tokens_generated = static_cast<uint32_t>(response.content.length() / 4);
    response.success = true;
    response.inference_time = simulated_delay_;

    return response;
}

std::future<MockLLM::Response> MockLLM::complete_async(const Request& request) {
    return std::async(std::launch::async, [this, request]() {
        return complete(request);
    });
}

void MockLLM::set_simulated_delay(std::chrono::milliseconds delay) {
    simulated_delay_ = delay;
}

void MockLLM::set_role_template(AgentRole role, const std::string& template_str) {
    role_templates_[role] = template_str;
}

std::string MockLLM::generate_mock_response(const Request& request) {
    // Detect role from system prompt
    AgentRole detected_role = AgentRole::Custom;
    for (const auto& [role, template_str] : role_templates_) {
        if (request.system_prompt.find(template_str.substr(0, 20)) != std::string::npos) {
            detected_role = role;
            break;
        }
    }

    switch (detected_role) {
        case AgentRole::Planner:   return generate_planner_response(request);
        case AgentRole::Coder:     return generate_coder_response(request);
        case AgentRole::Reviewer:  return generate_reviewer_response(request);
        case AgentRole::Analyst:   return generate_analyst_response(request);
        default:                   return generate_default_response(request);
    }
}

std::string MockLLM::generate_planner_response(const Request& request) {
    auto it = role_templates_.find(AgentRole::Planner);
    if (it != role_templates_.end()) {
        return it->second;
    }
    return "Plan generated for: " + request.prompt.substr(0, 100);
}

std::string MockLLM::generate_coder_response(const Request& request) {
    auto it = role_templates_.find(AgentRole::Coder);
    if (it != role_templates_.end()) {
        return it->second;
    }
    return "Code generated for: " + request.prompt.substr(0, 100);
}

std::string MockLLM::generate_reviewer_response(const Request& request) {
    auto it = role_templates_.find(AgentRole::Reviewer);
    if (it != role_templates_.end()) {
        return it->second;
    }
    return "Review completed for: " + request.prompt.substr(0, 100);
}

std::string MockLLM::generate_analyst_response(const Request& request) {
    auto it = role_templates_.find(AgentRole::Analyst);
    if (it != role_templates_.end()) {
        return it->second;
    }
    return "Analysis completed for: " + request.prompt.substr(0, 100);
}

std::string MockLLM::generate_default_response(const Request& request) {
    return "Processed: " + request.prompt.substr(0, 100) + "...\n\n"
           "Result: Mock response generated for testing.";
}

// ============================================================================
// AgentPuppeteer Implementation
// ============================================================================

AgentPuppeteer::AgentPuppeteer(SubAgentManager* subagent_manager,
                                AgenticExecutorController* controller,
                                Config config)
    : subagent_manager_(subagent_manager)
    , controller_(controller)
    , config_(std::move(config))
    , running_(false)
    , active_tasks_(0) {
}

AgentPuppeteer::~AgentPuppeteer() {
    // Ensure all tasks complete
    while (active_tasks_.load() > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void AgentPuppeteer::register_agent(const AgentRoleConfig& config) {
    std::lock_guard<std::mutex> lock(context_mutex_);

    agents_[config.id] = config;

    // Add to dependency graph
    std::vector<std::string> deps;
    for (const auto& dep : config.dependencies) {
        deps.push_back(agent_role_to_string(dep));
    }
    dependency_graph_.add_agent(config.id, deps);

    log_info("Registered agent: " + config.id + " (role: " + agent_role_to_string(config.role) + ")");
}

void AgentPuppeteer::register_default_agents() {
    // Planner
    AgentRoleConfig planner;
    planner.role = AgentRole::Planner;
    planner.id = "planner";
    planner.name = "Planner";
    planner.system_prompt =
        "You are a strategic planner. Your role is to:\n"
        "1. Analyze tasks and requirements\n"
        "2. Break down complex problems into actionable steps\n"
        "3. Identify dependencies and risks\n"
        "4. Create structured execution plans\n\n"
        "Your output should be clear, actionable, and thorough.";
    planner.dependencies = {};
    register_agent(planner);

    // Coder
    AgentRoleConfig coder;
    coder.role = AgentRole::Coder;
    coder.id = "coder";
    coder.name = "Coder";
    coder.system_prompt =
        "You are an expert coder. Your role is to:\n"
        "1. Implement solutions based on plans and requirements\n"
        "2. Write clean, efficient, and maintainable code\n"
        "3. Follow language-specific best practices\n"
        "4. Document your implementation decisions\n\n"
        "Your code should be production-ready and well-tested.";
    coder.dependencies = {AgentRole::Planner};
    register_agent(coder);

    // Reviewer
    AgentRoleConfig reviewer;
    reviewer.role = AgentRole::Reviewer;
    reviewer.id = "reviewer";
    reviewer.name = "Reviewer";
    reviewer.system_prompt =
        "You are a meticulous code reviewer. Your role is to:\n"
        "1. Evaluate code quality and correctness\n"
        "2. Identify bugs, security issues, and anti-patterns\n"
        "3. Provide constructive feedback\n"
        "4. Ensure best practices are followed\n\n"
        "Be thorough but fair in your assessment.";
    reviewer.dependencies = {AgentRole::Coder};
    register_agent(reviewer);

    // Analyst
    AgentRoleConfig analyst;
    analyst.role = AgentRole::Analyst;
    analyst.id = "analyst";
    analyst.name = "Analyst";
    analyst.system_prompt =
        "You are an analytical agent. Your role is to:\n"
        "1. Analyze data and identify patterns\n"
        "2. Provide quantitative insights\n"
        "3. Assess risks and opportunities\n"
        "4. Generate actionable recommendations\n\n"
        "Your analysis should be data-driven and objective.";
    analyst.dependencies = {};
    register_agent(analyst);

    // Tester
    AgentRoleConfig tester;
    tester.role = AgentRole::Tester;
    tester.id = "tester";
    tester.name = "Tester";
    tester.system_prompt =
        "You are a test engineer. Your role is to:\n"
        "1. Design comprehensive test strategies\n"
        "2. Write unit, integration, and end-to-end tests\n"
        "3. Identify edge cases and boundary conditions\n"
        "4. Verify requirements are met\n\n"
        "Think adversarially - how would this break?";
    tester.dependencies = {AgentRole::Coder};
    register_agent(tester);

    // Architect
    AgentRoleConfig architect;
    architect.role = AgentRole::Architect;
    architect.id = "architect";
    architect.name = "Architect";
    architect.system_prompt =
        "You are a system architect. Your role is to:\n"
        "1. Design high-level system architecture\n"
        "2. Ensure consistency across components\n"
        "3. Identify scalability and performance concerns\n"
        "4. Propose design patterns and abstractions\n\n"
        "Think in terms of maintainability and extensibility.";
    architect.dependencies = {};
    register_agent(architect);

    // Security
    AgentRoleConfig security;
    security.role = AgentRole::Security;
    security.id = "security";
    security.name = "Security";
    security.system_prompt =
        "You are a security expert. Your role is to:\n"
        "1. Identify potential security vulnerabilities\n"
        "2. Check for injection, XSS, CSRF issues\n"
        "3. Review authentication and authorization\n"
        "4. Verify data validation and sanitization\n\n"
        "Be paranoid - what could go wrong?";
    security.dependencies = {AgentRole::Coder};
    register_agent(security);

    // Optimizer
    AgentRoleConfig optimizer;
    optimizer.role = AgentRole::Optimizer;
    optimizer.id = "optimizer";
    optimizer.name = "Optimizer";
    optimizer.system_prompt =
        "You are a performance engineer. Your role is to:\n"
        "1. Profile and identify bottlenecks\n"
        "2. Optimize algorithms for time/space complexity\n"
        "3. Suggest caching strategies\n"
        "4. Identify memory leaks\n\n"
        "Measure before optimizing.";
    optimizer.dependencies = {AgentRole::Coder};
    register_agent(optimizer);

    log_info("Registered 8 default agents");
}

void AgentPuppeteer::unregister_agent(const std::string& id) {
    std::lock_guard<std::mutex> lock(context_mutex_);
    agents_.erase(id);
    // Note: dependency_graph_ doesn't support removal, would need rebuild
}

bool AgentPuppeteer::has_agent(const std::string& id) const {
    std::lock_guard<std::mutex> lock(context_mutex_);
    return agents_.find(id) != agents_.end();
}

std::vector<AgentRoleConfig> AgentPuppeteer::get_registered_agents() const {
    std::lock_guard<std::mutex> lock(context_mutex_);
    std::vector<AgentRoleConfig> result;
    for (const auto& [id, config] : agents_) {
        result.push_back(config);
    }
    return result;
}

// ============================================================================
// Task Execution
// ============================================================================

std::vector<PuppeteerAgentResult> AgentPuppeteer::execute_task(
    const std::string& input,
    const std::vector<std::string>& agent_ids,
    AggregationMode aggregation) {

    active_tasks_++;
    running_ = true;

    std::vector<PuppeteerAgentResult> results;

    try {
        // Resolve execution order based on dependencies
        std::vector<std::string> execution_order;
        if (agent_ids.empty()) {
            execution_order = dependency_graph_.topological_sort();
        } else {
            // Build subgraph for requested agents
            DependencyGraph subgraph;
            for (const auto& id : agent_ids) {
                auto it = agents_.find(id);
                if (it != agents_.end()) {
                    std::vector<std::string> deps;
                    for (const auto& dep : it->second.dependencies) {
                        deps.push_back(agent_role_to_string(dep));
                    }
                    subgraph.add_agent(id, deps);
                }
            }
            execution_order = subgraph.topological_sort();
        }

        // Check for cycles
        if (dependency_graph_.has_cycle()) {
            log_error("Dependency cycle detected - aborting execution");
            active_tasks_--;
            running_ = false;
            return results;
        }

        // Execute based on mode
        if (config_.enable_parallel_execution) {
            results = execute_agents_parallel(input, execution_order);
        } else {
            results = execute_agents_sequential(input, execution_order);
        }

        // Aggregate if requested
        if (aggregation != AggregationMode::Union && !results.empty()) {
            auto aggregated = aggregate_results(results, aggregation);
            // Replace results with aggregated synthesis
            PuppeteerAgentResult synth_result;
            synth_result.agent_id = "synthesis";
            synth_result.agent_role = "synthesis";
            synth_result.output = aggregated.synthesis;
            synth_result.success = true;
            results = {synth_result};
        }

    } catch (const std::exception& e) {
        log_error("Task execution failed: " + std::string(e.what()));
        if (error_callback_) {
            error_callback_("orchestrator", e.what());
        }
    }

    active_tasks_--;
    if (active_tasks_.load() == 0) {
        running_ = false;
    }

    return results;
}

std::future<std::vector<PuppeteerAgentResult>> AgentPuppeteer::execute_task_async(
    const std::string& input,
    const std::vector<std::string>& agent_ids,
    AggregationMode aggregation) {

    return std::async(std::launch::async, [this, input, agent_ids, aggregation]() {
        return execute_task(input, agent_ids, aggregation);
    });
}

// ============================================================================
// Pipeline Builder
// ============================================================================

AgentPuppeteer::PipelineBuilder AgentPuppeteer::pipeline() {
    return PipelineBuilder(this);
}

AgentPuppeteer::PipelineBuilder::PipelineBuilder(AgentPuppeteer* puppeteer)
    : puppeteer_(puppeteer) {
}

AgentPuppeteer::PipelineBuilder& AgentPuppeteer::PipelineBuilder::add(const std::string& agent_id) {
    agent_ids_.push_back(agent_id);
    return *this;
}

AgentPuppeteer::PipelineBuilder& AgentPuppeteer::PipelineBuilder::planner() {
    return add("planner");
}

AgentPuppeteer::PipelineBuilder& AgentPuppeteer::PipelineBuilder::coder() {
    return add("coder");
}

AgentPuppeteer::PipelineBuilder& AgentPuppeteer::PipelineBuilder::reviewer() {
    return add("reviewer");
}

AgentPuppeteer::PipelineBuilder& AgentPuppeteer::PipelineBuilder::analyst() {
    return add("analyst");
}

AgentPuppeteer::PipelineBuilder& AgentPuppeteer::PipelineBuilder::tester() {
    return add("tester");
}

AgentPuppeteer::PipelineBuilder& AgentPuppeteer::PipelineBuilder::architect() {
    return add("architect");
}

AgentPuppeteer::PipelineBuilder& AgentPuppeteer::PipelineBuilder::security() {
    return add("security");
}

AgentPuppeteer::PipelineBuilder& AgentPuppeteer::PipelineBuilder::optimizer() {
    return add("optimizer");
}

AgentPuppeteer::PipelineBuilder& AgentPuppeteer::PipelineBuilder::documenter() {
    return add("documenter");
}

std::vector<PuppeteerAgentResult> AgentPuppeteer::PipelineBuilder::execute(const std::string& input) {
    return puppeteer_->execute_task(input, agent_ids_);
}

std::future<std::vector<PuppeteerAgentResult>> AgentPuppeteer::PipelineBuilder::execute_async(const std::string& input) {
    return puppeteer_->execute_task_async(input, agent_ids_);
}

// ============================================================================
// Aggregation
// ============================================================================

AggregatedResult AgentPuppeteer::aggregate_results(
    const std::vector<PuppeteerAgentResult>& results,
    AggregationMode mode) const {

    AggregatedResult aggregated;
    aggregated.mode = mode;
    aggregated.all_results = results;

    switch (mode) {
        case AggregationMode::Union:
            aggregated.synthesis = synthesize_outputs(results);
            break;

        case AggregationMode::Consensus:
            aggregated.consensus = find_consensus(results);
            aggregated.synthesis = aggregated.consensus;
            break;

        case AggregationMode::Synthesis:
            aggregated.synthesis = synthesize_outputs(results);
            break;

        case AggregationMode::BestMatch: {
            const PuppeteerAgentResult* best = nullptr;
            double best_confidence = -1.0;
            for (const auto& result : results) {
                if (result.confidence > best_confidence) {
                    best_confidence = result.confidence;
                    best = &result;
                }
            }
            if (best) {
                aggregated.synthesis = best->output;
                aggregated.overall_confidence = best->confidence;
            }
            break;
        }

        case AggregationMode::Refinement:
            // Use last result (each agent improved previous)
            if (!results.empty()) {
                aggregated.synthesis = results.back().output;
            }
            break;
    }

    aggregated.action_items = extract_action_items(results);
    aggregated.conflicts = detect_conflicts(results);

    // Calculate overall confidence
    if (!results.empty()) {
        double total = 0.0;
        for (const auto& result : results) {
            total += result.confidence;
        }
        aggregated.overall_confidence = total / results.size();
    }

    return aggregated;
}

// ============================================================================
// Callbacks
// ============================================================================

void AgentPuppeteer::set_progress_callback(ProgressCallback cb) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    progress_callback_ = std::move(cb);
}

void AgentPuppeteer::set_result_callback(ResultCallback cb) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    result_callback_ = std::move(cb);
}

void AgentPuppeteer::set_error_callback(ErrorCallback cb) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    error_callback_ = std::move(cb);
}

// ============================================================================
// Context Management
// ============================================================================

void AgentPuppeteer::set_global_context(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(context_mutex_);
    global_context_[key] = value;
}

std::optional<std::string> AgentPuppeteer::get_global_context(const std::string& key) const {
    std::lock_guard<std::mutex> lock(context_mutex_);
    auto it = global_context_.find(key);
    if (it != global_context_.end()) {
        return it->second;
    }
    return std::nullopt;
}

void AgentPuppeteer::clear_global_context() {
    std::lock_guard<std::mutex> lock(context_mutex_);
    global_context_.clear();
}

// ============================================================================
// Private Helpers
// ============================================================================

std::vector<PuppeteerAgentResult> AgentPuppeteer::execute_agents_sequential(
    const std::string& input,
    const std::vector<std::string>& agent_ids) {

    std::vector<PuppeteerAgentResult> results;
    std::unordered_map<std::string, std::string> accumulated_context = global_context_;

    for (const auto& agent_id : agent_ids) {
        auto it = agents_.find(agent_id);
        if (it == agents_.end()) {
            log_error("Agent not found: " + agent_id);
            continue;
        }

        // Notify progress
        if (progress_callback_) {
            progress_callback_(agent_id, "Executing...");
        }

        // Execute agent
        auto result = execute_single_agent(input, it->second, accumulated_context);
        results.push_back(result);

        // Update context
        if (result.success && config_.enable_context_accumulation) {
            for (const auto& [key, value] : result.context_updates) {
                accumulated_context[key] = value;
            }
        }

        // Notify result
        if (result_callback_) {
            result_callback_(result);
        }

        // Notify progress
        if (progress_callback_) {
            progress_callback_(agent_id, result.success ? "Completed" : "Failed");
        }
    }

    return results;
}

std::vector<PuppeteerAgentResult> AgentPuppeteer::execute_agents_parallel(
    const std::string& input,
    const std::vector<std::string>& agent_ids) {

    // For shared model, "parallel" is sequential but fast
    // (Could use multiple model instances for true parallel if VRAM allows)
    return execute_agents_sequential(input, agent_ids);
}

PuppeteerAgentResult AgentPuppeteer::execute_single_agent(
    const std::string& input,
    const AgentRoleConfig& agent_config,
    const std::unordered_map<std::string, std::string>& context) {

    PuppeteerAgentResult result;
    result.agent_id = agent_config.id;
    result.agent_role = agent_role_to_string(agent_config.role);

    auto start_time = std::chrono::steady_clock::now();

    // Check VRAM availability
    if (config_.enable_vram_throttling && !check_vram_available()) {
        result.success = false;
        result.error_message = "VRAM throttled - cannot execute agent";
        return result;
    }

    try {
        // Build prompt
        std::string prompt = agent_config.system_prompt + "\n\n";

        // Add context
        if (!context.empty()) {
            prompt += "## Context:\n";
            for (const auto& [key, value] : context) {
                prompt += "- " + key + ": " + value + "\n";
            }
            prompt += "\n";
        }

        prompt += "## Task:\n" + input;

        // Use existing SubAgentManager for execution
        if (subagent_manager_) {
            // Create a swarm task for this agent
            std::vector<std::string> prompts = {prompt};
            SwarmConfig config;
            config.maxParallel = 1;
            config.timeoutMs = agent_config.timeout_seconds * 1000;
            config.mergeStrategy = "concatenate";

            std::string output = subagent_manager_->executeSwarm(
                agent_config.id, prompts, config);

            result.output = output;
            result.success = true;
            result.tokens_used = static_cast<uint32_t>(output.length() / 4);
            result.confidence = 0.85;

            // Extract artifacts (code blocks, etc.)
            // Simple extraction - could be more sophisticated
            size_t code_start = output.find("```");
            while (code_start != std::string::npos) {
                size_t code_end = output.find("```", code_start + 3);
                if (code_end != std::string::npos) {
                    result.artifacts.push_back(output.substr(code_start + 3, code_end - code_start - 3));
                    code_start = output.find("```", code_end + 3);
                } else {
                    break;
                }
            }

            // Context updates
            result.context_updates[agent_config.id + "_output"] = output;
            result.context_updates[agent_config.id + "_status"] = "completed";

        } else {
            // Fallback: use mock LLM
            MockLLM mock;
            MockLLM::Request request;
            request.prompt = prompt;
            request.system_prompt = agent_config.system_prompt;

            auto response = mock.complete(request);
            result.output = response.content;
            result.success = response.success;
            result.tokens_used = response.tokens_generated;
            result.confidence = 0.75;
        }

    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = e.what();

        if (error_callback_) {
            error_callback_(agent_config.id, e.what());
        }
    }

    auto end_time = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    return result;
}

// ============================================================================
// Aggregation Helpers
// ============================================================================

std::string AgentPuppeteer::synthesize_outputs(
    const std::vector<PuppeteerAgentResult>& results) const {

    std::ostringstream synthesis;
    synthesis << "# Synthesized Output\n\n";

    // Group by role
    std::map<std::string, std::vector<std::string>> role_outputs;
    for (const auto& result : results) {
        role_outputs[result.agent_role].push_back(result.output);
    }

    // Add sections by role
    for (const auto& [role, outputs] : role_outputs) {
        synthesis << "## " << role << "\n\n";
        for (const auto& output : outputs) {
            // Summarize (first 500 chars)
            synthesis << output.substr(0, 500);
            if (output.length() > 500) {
                synthesis << "...";
            }
            synthesis << "\n\n";
        }
    }

    return synthesis.str();
}

std::string AgentPuppeteer::find_consensus(
    const std::vector<PuppeteerAgentResult>& results) const {

    // Extract key points from each result
    std::vector<std::vector<std::string>> all_points;
    for (const auto& result : results) {
        all_points.push_back(extract_key_points(result.output));
    }

    // Find points that appear in majority of outputs
    std::map<std::string, int> point_counts;
    for (const auto& points : all_points) {
        for (const auto& point : points) {
            point_counts[point]++;
        }
    }

    size_t threshold = results.size() / 2;
    std::ostringstream consensus;
    for (const auto& [point, count] : point_counts) {
        if (count >= threshold) {
            consensus << "- " << point << "\n";
        }
    }

    return consensus.str();
}

std::vector<std::string> AgentPuppeteer::extract_action_items(
    const std::vector<PuppeteerAgentResult>& results) const {

    std::vector<std::string> items;

    for (const auto& result : results) {
        std::istringstream iss(result.output);
        std::string line;
        while (std::getline(iss, line)) {
            if (line.find("- [ ]") != std::string::npos ||
                line.find("TODO:") != std::string::npos ||
                line.find("FIXME:") != std::string::npos) {
                items.push_back(line);
            }
        }
    }

    return items;
}

std::vector<std::string> AgentPuppeteer::detect_conflicts(
    const std::vector<PuppeteerAgentResult>& results) const {

    std::vector<std::string> conflicts;

    // Simple conflict detection: look for contradictory statements
    // (Would need NLP for proper detection)
    std::vector<std::string> shoulds;
    std::vector<std::string> should_nots;

    for (const auto& result : results) {
        std::istringstream iss(result.output);
        std::string line;
        while (std::getline(iss, line)) {
            if (line.find("should ") != std::string::npos && line.find("should not") == std::string::npos) {
                shoulds.push_back(line);
            }
            if (line.find("should not") != std::string::npos) {
                should_nots.push_back(line);
            }
        }
    }

    if (!shoulds.empty() && !should_nots.empty()) {
        conflicts.push_back("Potential contradiction between recommendations");
    }

    return conflicts;
}

std::vector<std::string> AgentPuppeteer::extract_key_points(const std::string& output) const {
    std::vector<std::string> points;

    // Extract bullet points and numbered lists
    std::regex point_regex(R"(^\s*[-*]\s+(.+)$|^\s*\d+\.\s+(.+)$)");
    std::istringstream iss(output);
    std::string line;

    while (std::getline(iss, line)) {
        std::smatch match;
        if (std::regex_search(line, match, point_regex)) {
            points.push_back(match[1].matched ? match[1].str() : match[2].str());
        }
    }

    return points;
}

// ============================================================================
// Context Helpers
// ============================================================================

std::unordered_map<std::string, std::string> AgentPuppeteer::build_agent_context(
    const AgentRoleConfig& config,
    const std::vector<PuppeteerAgentResult>& previous_results) const {

    std::unordered_map<std::string, std::string> context = global_context_;

    // Add previous results
    for (const auto& result : previous_results) {
        context[result.agent_id + "_output"] = result.output;
        context[result.agent_id + "_status"] = result.success ? "completed" : "failed";
    }

    return context;
}

// ============================================================================
// VRAM Throttling
// ============================================================================

bool AgentPuppeteer::check_vram_available() const {
    if (!controller_) {
        return true; // No controller = no throttling
    }

    // Check if aperture is throttled
    return !controller_->IsApertureThrottled();
}

// ============================================================================
// Logging
// ============================================================================

void AgentPuppeteer::log_info(const std::string& msg) const {
    if (subagent_manager_) {
        // Use SubAgentManager's log callback
        // Note: SubAgentManager doesn't expose log callback directly
        // This would need to be wired through the existing system
    }
}

void AgentPuppeteer::log_error(const std::string& msg) const {
    if (subagent_manager_) {
        // Use SubAgentManager's log callback
    }
}

// ============================================================================
// Utility Functions
// ============================================================================

std::string agent_role_to_string(AgentRole role) {
    switch (role) {
        case AgentRole::Planner:    return "planner";
        case AgentRole::Coder:      return "coder";
        case AgentRole::Reviewer:   return "reviewer";
        case AgentRole::Analyst:    return "analyst";
        case AgentRole::Tester:     return "tester";
        case AgentRole::Architect:  return "architect";
        case AgentRole::Security:   return "security";
        case AgentRole::Optimizer:  return "optimizer";
        case AgentRole::Documenter: return "documenter";
        case AgentRole::Custom:     return "custom";
    }
    return "unknown";
}

AgentRole string_to_agent_role(const std::string& str) {
    if (str == "planner")    return AgentRole::Planner;
    if (str == "coder")      return AgentRole::Coder;
    if (str == "reviewer")   return AgentRole::Reviewer;
    if (str == "analyst")    return AgentRole::Analyst;
    if (str == "tester")     return AgentRole::Tester;
    if (str == "architect")  return AgentRole::Architect;
    if (str == "security")   return AgentRole::Security;
    if (str == "optimizer")  return AgentRole::Optimizer;
    if (str == "documenter") return AgentRole::Documenter;
    return AgentRole::Custom;
}

} // namespace agents
} // namespace rawrxd
