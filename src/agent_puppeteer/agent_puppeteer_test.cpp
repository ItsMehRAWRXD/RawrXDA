// ============================================================================
// agent_puppeteer_test.cpp — Unit Tests for Agent Puppeteer
// ============================================================================

#include "agent_puppeteer.hpp"
#include <assert>
#include <iostream>
#include <string>

using namespace rawrxd::agents;

// ============================================================================
// Test Helpers
// ============================================================================

static int g_testsPassed = 0;
static int g_testsFailed = 0;

#define TEST(name) void test_##name()
#define RUN_TEST(name) do { \
    std::cout << "Running " #name "... "; \
    try { \
        test_##name(); \
        std::cout << "PASSED\n"; \
        g_testsPassed++; \
    } catch (const std::exception& e) { \
        std::cout << "FAILED: " << e.what() << "\n"; \
        g_testsFailed++; \
    } \
} while(0)

#define ASSERT_TRUE(expr) do { \
    if (!(expr)) { \
        throw std::runtime_error("Assertion failed: " #expr); \
    } \
} while(0)

#define ASSERT_FALSE(expr) ASSERT_TRUE(!(expr))
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_NE(a, b) ASSERT_TRUE((a) != (b))

// ============================================================================
// DependencyGraph Tests
// ============================================================================

TEST(dependency_graph_basic) {
    DependencyGraph graph;
    graph.add_agent("a", {});
    graph.add_agent("b", {"a"});
    graph.add_agent("c", {"a", "b"});

    auto order = graph.topological_sort();
    ASSERT_EQ(order.size(), 3);
    ASSERT_EQ(order[0], "a");
    ASSERT_EQ(order[1], "b");
    ASSERT_EQ(order[2], "c");
}

TEST(dependency_graph_cycle_detection) {
    DependencyGraph graph;
    graph.add_agent("a", {"c"});
    graph.add_agent("b", {"a"});
    graph.add_agent("c", {"b"}); // Cycle: a -> c -> b -> a

    ASSERT_TRUE(graph.has_cycle());
}

TEST(dependency_graph_no_cycle) {
    DependencyGraph graph;
    graph.add_agent("a", {});
    graph.add_agent("b", {"a"});
    graph.add_agent("c", {"b"});

    ASSERT_FALSE(graph.has_cycle());
}

TEST(dependency_graph_execution_levels) {
    DependencyGraph graph;
    graph.add_agent("a", {});
    graph.add_agent("b", {});
    graph.add_agent("c", {"a", "b"});
    graph.add_agent("d", {"c"});

    auto levels = graph.get_execution_levels();
    ASSERT_EQ(levels.size(), 3);
    ASSERT_EQ(levels[0].size(), 2); // a, b
    ASSERT_EQ(levels[1].size(), 1); // c
    ASSERT_EQ(levels[2].size(), 1); // d
}

// ============================================================================
// MockLLM Tests
// ============================================================================

TEST(mock_llm_basic) {
    MockLLM llm;
    MockLLM::Request request;
    request.prompt = "Test prompt";
    request.system_prompt = "You are a planner";

    auto response = llm.complete(request);
    ASSERT_TRUE(response.success);
    ASSERT_TRUE(response.tokens_generated > 0);
    ASSERT_TRUE(response.inference_time.count() >= 100);
}

TEST(mock_llm_async) {
    MockLLM llm;
    MockLLM::Request request;
    request.prompt = "Test prompt";
    request.system_prompt = "You are a coder";

    auto future = llm.complete_async(request);
    auto response = future.get();
    ASSERT_TRUE(response.success);
}

TEST(mock_llm_role_detection) {
    MockLLM llm;

    // Planner
    MockLLM::Request planner_req;
    planner_req.system_prompt = "planner";
    auto planner_resp = llm.complete(planner_req);
    ASSERT_TRUE(planner_resp.content.find("Execution Plan") != std::string::npos);

    // Coder
    MockLLM::Request coder_req;
    coder_req.system_prompt = "coder";
    auto coder_resp = llm.complete(coder_req);
    ASSERT_TRUE(coder_resp.content.find("class Solution") != std::string::npos);
}

// ============================================================================
// AgentPuppeteer Tests
// ============================================================================

TEST(puppeteer_register_default_agents) {
    AgentPuppeteer puppeteer(nullptr);
    puppeteer.register_default_agents();

    auto agents = puppeteer.get_registered_agents();
    ASSERT_EQ(agents.size(), 8); // planner, coder, reviewer, analyst, tester, architect, security, optimizer
}

TEST(puppeteer_has_agent) {
    AgentPuppeteer puppeteer(nullptr);
    puppeteer.register_default_agents();

    ASSERT_TRUE(puppeteer.has_agent("planner"));
    ASSERT_TRUE(puppeteer.has_agent("coder"));
    ASSERT_FALSE(puppeteer.has_agent("nonexistent"));
}

TEST(puppeteer_execute_task) {
    AgentPuppeteer puppeteer(nullptr);
    puppeteer.register_default_agents();

    auto results = puppeteer.execute_task(
        "Test task",
        {"planner", "coder"}
    );

    ASSERT_EQ(results.size(), 2);
    ASSERT_TRUE(results[0].success);
    ASSERT_TRUE(results[1].success);
}

TEST(puppeteer_pipeline_builder) {
    AgentPuppeteer puppeteer(nullptr);
    puppeteer.register_default_agents();

    auto results = puppeteer.pipeline()
        .planner()
        .coder()
        .execute("Test task");

    ASSERT_EQ(results.size(), 2);
}

TEST(puppeteer_aggregation_union) {
    AgentPuppeteer puppeteer(nullptr);
    puppeteer.register_default_agents();

    auto results = puppeteer.execute_task(
        "Test task",
        {"planner", "coder"}
    );

    auto aggregated = puppeteer.aggregate_results(results, AggregationMode::Union);
    ASSERT_TRUE(!aggregated.synthesis.empty());
    ASSERT_TRUE(aggregated.overall_confidence > 0);
}

TEST(puppeteer_aggregation_consensus) {
    AgentPuppeteer puppeteer(nullptr);
    puppeteer.register_default_agents();

    auto results = puppeteer.execute_task(
        "Test task",
        {"planner", "coder", "reviewer"}
    );

    auto aggregated = puppeteer.aggregate_results(results, AggregationMode::Consensus);
    // Consensus may be empty for mock data, but should not crash
    ASSERT_TRUE(aggregated.all_results.size() > 0);
}

TEST(puppeteer_context_management) {
    AgentPuppeteer puppeteer(nullptr);

    puppeteer.set_global_context("key1", "value1");
    auto value = puppeteer.get_global_context("key1");
    ASSERT_TRUE(value.has_value());
    ASSERT_EQ(value.value(), "value1");

    puppeteer.clear_global_context();
    value = puppeteer.get_global_context("key1");
    ASSERT_FALSE(value.has_value());
}

TEST(puppeteer_custom_agent) {
    AgentPuppeteer puppeteer(nullptr);
    puppeteer.register_default_agents();

    AgentRoleConfig custom;
    custom.role = AgentRole::Custom;
    custom.id = "custom_agent";
    custom.name = "Custom Agent";
    custom.system_prompt = "You are a custom agent";
    custom.dependencies = {};

    puppeteer.register_agent(custom);
    ASSERT_TRUE(puppeteer.has_agent("custom_agent"));
}

TEST(puppeteer_dependency_resolution) {
    AgentPuppeteer puppeteer(nullptr);
    puppeteer.register_default_agents();

    // coder depends on planner, reviewer depends on coder
    auto results = puppeteer.execute_task(
        "Test task",
        {"reviewer", "coder", "planner"} // Out of order
    );

    ASSERT_EQ(results.size(), 3);
    // Should execute in dependency order: planner -> coder -> reviewer
    ASSERT_EQ(results[0].agent_id, "planner");
    ASSERT_EQ(results[1].agent_id, "coder");
    ASSERT_EQ(results[2].agent_id, "reviewer");
}

TEST(puppeteer_async_execution) {
    AgentPuppeteer puppeteer(nullptr);
    puppeteer.register_default_agents();

    auto future = puppeteer.execute_task_async(
        "Test task",
        {"planner", "coder"}
    );

    auto results = future.get();
    ASSERT_EQ(results.size(), 2);
    ASSERT_TRUE(results[0].success);
}

TEST(puppeteer_progress_callback) {
    AgentPuppeteer puppeteer(nullptr);
    puppeteer.register_default_agents();

    int progress_count = 0;
    puppeteer.set_progress_callback([&progress_count](const std::string&, const std::string&) {
        progress_count++;
    });

    puppeteer.execute_task("Test task", {"planner"});
    ASSERT_TRUE(progress_count > 0);
}

// ============================================================================
// Main
// ============================================================================
int main() {
    std::cout << "╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║         Agent Puppeteer — Unit Tests                       ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n\n";

    RUN_TEST(dependency_graph_basic);
    RUN_TEST(dependency_graph_cycle_detection);
    RUN_TEST(dependency_graph_no_cycle);
    RUN_TEST(dependency_graph_execution_levels);
    RUN_TEST(mock_llm_basic);
    RUN_TEST(mock_llm_async);
    RUN_TEST(mock_llm_role_detection);
    RUN_TEST(puppeteer_register_default_agents);
    RUN_TEST(puppeteer_has_agent);
    RUN_TEST(puppeteer_execute_task);
    RUN_TEST(puppeteer_pipeline_builder);
    RUN_TEST(puppeteer_aggregation_union);
    RUN_TEST(puppeteer_aggregation_consensus);
    RUN_TEST(puppeteer_context_management);
    RUN_TEST(puppeteer_custom_agent);
    RUN_TEST(puppeteer_dependency_resolution);
    RUN_TEST(puppeteer_async_execution);
    RUN_TEST(puppeteer_progress_callback);

    std::cout << "\n══════════════════════════════════════════════════════════════\n";
    std::cout << "Results: " << g_testsPassed << " passed, " << g_testsFailed << " failed\n";
    std::cout << "══════════════════════════════════════════════════════════════\n";

    return g_testsFailed > 0 ? 1 : 0;
}
