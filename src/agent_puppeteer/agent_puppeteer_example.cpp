// ============================================================================
// agent_puppeteer_example.cpp — Usage Examples for Agent Puppeteer
// ============================================================================

#include "agent_puppeteer.hpp"
#include <iostream>
#include <thread>
#include <chrono>

using namespace rawrxd::agents;

// ============================================================================
// Example 1: Basic Usage with Default Agents
// ============================================================================
void demo_basic_usage() {
    std::cout << "\n=== Basic Usage Demo ===\n\n";

    // Create puppeteer (no SubAgentManager for standalone demo)
    AgentPuppeteer::Config config;
    config.max_workers = 2;
    config.enable_context_accumulation = true;

    AgentPuppeteer puppeteer(nullptr, nullptr, config);

    // Register default agents
    puppeteer.register_default_agents();

    // Execute task with planner and coder
    auto results = puppeteer.execute_task(
        "Create a simple calculator application with basic arithmetic operations",
        {"planner", "coder"}
    );

    // Display results
    for (const auto& result : results) {
        std::cout << "\n--- " << result.agent_id << " ---\n";
        std::cout << "Status: " << (result.success ? "success" : "failed") << "\n";
        std::cout << "Confidence: " << (result.confidence * 100) << "%\n";
        std::cout << "Duration: " << result.duration.count() << "ms\n";

        if (!result.output.empty()) {
            std::cout << "\nOutput (first 200 chars):\n";
            std::cout << result.output.substr(0, 200) << "...\n";
        }
    }
}

// ============================================================================
// Example 2: Pipeline Builder
// ============================================================================
void demo_pipeline_builder() {
    std::cout << "\n=== Pipeline Builder Demo ===\n\n";

    AgentPuppeteer puppeteer(nullptr);
    puppeteer.register_default_agents();

    // Build pipeline using fluent API
    auto results = puppeteer.pipeline()
        .planner()
        .coder()
        .reviewer()
        .execute("Design and implement a logging system for a web server");

    std::cout << "Pipeline completed with " << results.size() << " agent executions\n";

    for (const auto& result : results) {
        std::cout << "- " << result.agent_id << ": "
                  << (result.success ? "success" : "failed")
                  << " (" << result.duration.count() << "ms)\n";
    }
}

// ============================================================================
// Example 3: Context Sharing
// ============================================================================
void demo_context_sharing() {
    std::cout << "\n=== Context Sharing Demo ===\n\n";

    AgentPuppeteer puppeteer(nullptr);
    puppeteer.register_default_agents();

    // Set global context
    puppeteer.set_global_context("project_name", "MyApp");
    puppeteer.set_global_context("language", "C++");
    puppeteer.set_global_context("style_guide", "Google C++ Style Guide");

    std::cout << "Global context set:\n";
    std::cout << "- project_name: " << puppeteer.get_global_context("project_name").value_or("") << "\n";
    std::cout << "- language: " << puppeteer.get_global_context("language").value_or("") << "\n";
    std::cout << "- style_guide: " << puppeteer.get_global_context("style_guide").value_or("") << "\n";

    // Execute - agents will have access to global context
    auto results = puppeteer.execute_task(
        "Implement a configuration manager class",
        {"planner", "coder"}
    );

    std::cout << "\nTask completed. Agents had access to global context.\n";
}

// ============================================================================
// Example 4: Dependency Resolution
// ============================================================================
void demo_dependency_resolution() {
    std::cout << "\n=== Dependency Resolution Demo ===\n\n";

    DependencyGraph graph;

    // Add agents with dependencies
    graph.add_agent("validator", {"reviewer"});
    graph.add_agent("reviewer", {"coder"});
    graph.add_agent("coder", {"planner"});
    graph.add_agent("planner", {});
    graph.add_agent("analyst", {"planner"});

    std::cout << "Agents and dependencies:\n";
    std::cout << "- planner: no dependencies\n";
    std::cout << "- coder: depends on planner\n";
    std::cout << "- reviewer: depends on coder\n";
    std::cout << "- validator: depends on reviewer\n";
    std::cout << "- analyst: depends on planner\n\n";

    // Get execution order
    auto order = graph.topological_sort();
    std::cout << "Topological execution order: ";
    for (const auto& id : order) {
        std::cout << id << " -> ";
    }
    std::cout << "done\n\n";

    // Get parallel execution levels
    auto levels = graph.get_execution_levels();
    std::cout << "Parallel execution levels:\n";
    for (size_t i = 0; i < levels.size(); ++i) {
        std::cout << "Level " << i << ": ";
        for (const auto& id : levels[i]) {
            std::cout << id << " ";
        }
        std::cout << "\n";
    }
}

// ============================================================================
// Example 5: Aggregation Modes
// ============================================================================
void demo_aggregation_modes() {
    std::cout << "\n=== Aggregation Modes Demo ===\n\n";

    AgentPuppeteer puppeteer(nullptr);
    puppeteer.register_default_agents();

    // Execute with multiple agents
    auto results = puppeteer.execute_task(
        "Create a REST API with authentication",
        {"planner", "coder", "reviewer", "analyst"}
    );

    // Try different aggregation modes
    std::vector<AggregationMode> modes = {
        AggregationMode::Union,
        AggregationMode::Consensus,
        AggregationMode::Synthesis,
        AggregationMode::BestMatch
    };

    for (const auto& mode : modes) {
        std::cout << "\n--- " << static_cast<int>(mode) << " ---\n";
        auto aggregated = puppeteer.aggregate_results(results, mode);
        std::cout << "Synthesis (first 200 chars):\n"
                  << aggregated.synthesis.substr(0, 200) << "...\n";
        std::cout << "Overall confidence: " << (aggregated.overall_confidence * 100) << "%\n";
        std::cout << "Action items: " << aggregated.action_items.size() << "\n";
        std::cout << "Conflicts: " << aggregated.conflicts.size() << "\n";
    }
}

// ============================================================================
// Example 6: Callbacks and Progress
// ============================================================================
void demo_callbacks() {
    std::cout << "\n=== Callbacks Demo ===\n\n";

    AgentPuppeteer puppeteer(nullptr);
    puppeteer.register_default_agents();

    // Set up callbacks
    puppeteer.set_progress_callback([](const std::string& agent_id, const std::string& status) {
        std::cout << "[PROGRESS] " << agent_id << ": " << status << "\n";
    });

    puppeteer.set_result_callback([](const PuppeteerAgentResult& result) {
        std::cout << "[RESULT] " << result.agent_id
                  << " completed with " << result.tokens_used << " tokens\n";
    });

    puppeteer.set_error_callback([](const std::string& agent_id, const std::string& error) {
        std::cout << "[ERROR] " << agent_id << ": " << error << "\n";
    });

    // Execute task
    auto results = puppeteer.execute_task(
        "Implement a thread-safe queue data structure",
        {"planner", "coder", "reviewer"}
    );

    std::cout << "\nTask completed with " << results.size() << " agent executions\n";
}

// ============================================================================
// Example 7: Custom Agent
// ============================================================================
void demo_custom_agent() {
    std::cout << "\n=== Custom Agent Demo ===\n\n";

    AgentPuppeteer puppeteer(nullptr);
    puppeteer.register_default_agents();

    // Register custom agent
    AgentRoleConfig custom;
    custom.role = AgentRole::Custom;
    custom.id = "custom_validator";
    custom.name = "Custom Validator";
    custom.system_prompt =
        "You are a custom validation agent. Your role is to:\n"
        "1. Validate final outputs against requirements\n"
        "2. Check for completeness\n"
        "3. Verify no critical issues remain\n"
        "4. Provide final approval or rejection\n\n"
        "Be thorough and objective.";
    custom.dependencies = {AgentRole::Reviewer};
    custom.priority = 2; // High priority

    puppeteer.register_agent(custom);

    // Execute with custom agent
    auto results = puppeteer.execute_task(
        "Create a user authentication system",
        {"planner", "coder", "reviewer", "custom_validator"}
    );

    std::cout << "Custom agent execution completed with " << results.size() << " results\n";
}

// ============================================================================
// Example 8: Async Execution
// ============================================================================
void demo_async_execution() {
    std::cout << "\n=== Async Execution Demo ===\n\n";

    AgentPuppeteer puppeteer(nullptr);
    puppeteer.register_default_agents();

    // Execute task asynchronously
    auto future = puppeteer.execute_task_async(
        "Design a database schema for an e-commerce application",
        {"planner", "architect", "analyst"}
    );

    std::cout << "Task submitted asynchronously...\n";

    // Do other work while waiting
    std::cout << "Doing other work...\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Get results
    auto results = future.get();
    std::cout << "Async task completed with " << results.size() << " results\n";
}

// ============================================================================
// Main
// ============================================================================
int main() {
    std::cout << "╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║         Agent Puppeteering System - C++ Demo               ║\n";
    std::cout << "║              RawrXD Integration Extension                    ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n";

    try {
        demo_basic_usage();
        demo_pipeline_builder();
        demo_context_sharing();
        demo_dependency_resolution();
        demo_aggregation_modes();
        demo_callbacks();
        demo_custom_agent();
        demo_async_execution();

        std::cout << "\n\u2705 All demos completed successfully!\n";

    } catch (const std::exception& e) {
        std::cerr << "\u274c Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
