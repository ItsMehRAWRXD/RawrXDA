/* ═══════════════════════════════════════════════════════════════════════════
   RAWRXD_SWARM Test Suite
   
   Tests for multi-agent parallel execution system:
   - Worktree isolation
   - Agent lifecycle
   - Swarm coordination
   - Best-of-N selection
   - Chain of thought
   - Model cascading
   - Token budget management
   ═══════════════════════════════════════════════════════════════════════════ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "rawrxd_swarm.h"

/* Test counters */
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) static void test_##name(void)
#define RUN_TEST(name) do { \
    printf("  [TEST] %s... ", #name); \
    test_##name(); \
    printf("PASSED\n"); \
    tests_passed++; \
} while(0)

#define ASSERT(cond) do { \
    if (!(cond)) { \
        printf("FAILED at line %d: %s\n", __LINE__, #cond); \
        tests_failed++; \
        return; \
    } \
} while(0)

#define ASSERT_EQ(a, b) ASSERT((a) == (b))
#define ASSERT_NE(a, b) ASSERT((a) != (b))
#define ASSERT_GT(a, b) ASSERT((a) > (b))
#define ASSERT_LT(a, b) ASSERT((a) < (b))
#define ASSERT_STR_EQ(a, b) ASSERT(strcmp((a), (b)) == 0)
#define ASSERT_STR_NE(a, b) ASSERT(strcmp((a), (b)) != 0)

/* ═══════════════════════════════════════════════════════════════════════════
   WORKTREE TESTS
   ═══════════════════════════════════════════════════════════════════════════ */

TEST(worktree_manager_init) {
    rxd_worktree_manager_init("/test/repo");
    
    ASSERT_STR_EQ(g_wtm.base_repo, "/test/repo");
    ASSERT_EQ(g_wtm.active_count, 0);
    
    /* Cleanup */
    memset(&g_wtm, 0, sizeof(g_wtm));
}

TEST(worktree_create) {
    rxd_worktree_manager_init("/test/repo");
    
    RXDWorktree* tree = rxd_worktree_create("test_agent_1");
    
    ASSERT(tree != NULL);
    ASSERT_STR_EQ(tree->id, "test_agent_1");
    ASSERT(tree->is_active);
    ASSERT_EQ(g_wtm.active_count, 1);
    
    /* Cleanup */
    rxd_worktree_destroy(tree);
}

TEST(worktree_write_read) {
    rxd_worktree_manager_init("/tmp/test_worktrees");
    
    RXDWorktree* tree = rxd_worktree_create("test_io");
    ASSERT(tree != NULL);
    
    const char* test_content = "Hello, Swarm World!";
    bool written = rxd_worktree_write_file(tree, "test.md", test_content, strlen(test_content));
    ASSERT(written);
    ASSERT(tree->is_dirty);
    ASSERT_EQ(tree->files_modified, 1);
    
    size_t len = 0;
    char* read_content = rxd_worktree_read_file(tree, "test.md", &len);
    ASSERT(read_content != NULL);
    ASSERT_EQ(len, strlen(test_content));
    ASSERT_STR_EQ(read_content, test_content);
    
    free(read_content);
    rxd_worktree_destroy(tree);
}

TEST(worktree_copy_file) {
    /* Create a temp file to copy */
    FILE* f = fopen("/tmp/source_test.txt", "wb");
    ASSERT(f != NULL);
    fprintf(f, "Source content for copy test");
    fclose(f);
    
    rxd_worktree_manager_init("/tmp/test_copy");
    RXDWorktree* tree = rxd_worktree_create("copy_test");
    ASSERT(tree != NULL);
    
    bool copied = rxd_worktree_copy_file(tree, "/tmp/source_test.txt");
    ASSERT(copied);
    ASSERT(tree->is_dirty);
    
    rxd_worktree_destroy(tree);
}

TEST(worktree_multiple_agents) {
    rxd_worktree_manager_init("/tmp/multi_agent");
    
    RXDWorktree* tree1 = rxd_worktree_create("agent_1");
    RXDWorktree* tree2 = rxd_worktree_create("agent_2");
    RXDWorktree* tree3 = rxd_worktree_create("agent_3");
    
    ASSERT(tree1 != NULL);
    ASSERT(tree2 != NULL);
    ASSERT(tree3 != NULL);
    ASSERT_EQ(g_wtm.active_count, 3);
    
    /* Each should have unique IDs */
    ASSERT_STR_NE(tree1->id, tree2->id);
    ASSERT_STR_NE(tree2->id, tree3->id);
    
    rxd_worktree_destroy(tree1);
    rxd_worktree_destroy(tree2);
    rxd_worktree_destroy(tree3);
}

/* ═══════════════════════════════════════════════════════════════════════════
   AGENT TESTS
   ═══════════════════════════════════════════════════════════════════════════ */

TEST(swarm_init) {
    rxd_swarm_init(RXD_AGENT_MODE_BEST_OF_N);
    
    ASSERT_EQ(g_swarm.mode, RXD_AGENT_MODE_BEST_OF_N);
    ASSERT_EQ(g_swarm.agent_count, 0);
    ASSERT_EQ(g_swarm.token_budget, RXD_MAX_TOKEN_BUDGET);
    ASSERT(!g_swarm.is_running);
    
    memset(&g_swarm, 0, sizeof(g_swarm));
}

TEST(swarm_add_agent) {
    rxd_swarm_init(RXD_AGENT_MODE_BEST_OF_N);
    
    uint32_t id1 = rxd_swarm_add_agent(RXD_AGENT_ARCHITECT, NULL, NULL);
    uint32_t id2 = rxd_swarm_add_agent(RXD_AGENT_FRONTEND, NULL, NULL);
    uint32_t id3 = rxd_swarm_add_agent(RXD_AGENT_BACKEND, NULL, NULL);
    
    ASSERT_EQ(id1, 0);
    ASSERT_EQ(id2, 1);
    ASSERT_EQ(id3, 2);
    ASSERT_EQ(g_swarm.agent_count, 3);
    
    /* Check agent properties */
    ASSERT_EQ(g_swarm.agents[0].role, RXD_AGENT_ARCHITECT);
    ASSERT_EQ(g_swarm.agents[1].role, RXD_AGENT_FRONTEND);
    ASSERT_EQ(g_swarm.agents[2].role, RXD_AGENT_BACKEND);
    
    ASSERT(g_swarm.agents[0].enabled);
    ASSERT_EQ(g_swarm.agents[0].state, RXD_AGENT_IDLE);
    
    memset(&g_swarm, 0, sizeof(g_swarm));
}

TEST(swarm_max_agents) {
    rxd_swarm_init(RXD_AGENT_MODE_BEST_OF_N);
    
    /* Add max agents */
    for (int i = 0; i < RXD_SWARM_MAX_AGENTS; i++) {
        uint32_t id = rxd_swarm_add_agent(RXD_AGENT_ARCHITECT, NULL, NULL);
        ASSERT_EQ(id, (uint32_t)i);
    }
    
    /* Should fail to add more */
    uint32_t id = rxd_swarm_add_agent(RXD_AGENT_ARCHITECT, NULL, NULL);
    ASSERT_EQ(id, UINT32_MAX);
    
    memset(&g_swarm, 0, sizeof(g_swarm));
}

TEST(agent_role_names) {
    rxd_swarm_init(RXD_AGENT_MODE_BEST_OF_N);
    
    rxd_swarm_add_agent(RXD_AGENT_ARCHITECT, NULL, NULL);
    rxd_swarm_add_agent(RXD_AGENT_FRONTEND, NULL, NULL);
    rxd_swarm_add_agent(RXD_AGENT_BACKEND, NULL, NULL);
    rxd_swarm_add_agent(RXD_AGENT_TESTER, NULL, NULL);
    rxd_swarm_add_agent(RXD_AGENT_REVIEWER, NULL, NULL);
    rxd_swarm_add_agent(RXD_AGENT_OPTIMIZER, NULL, NULL);
    rxd_swarm_add_agent(RXD_AGENT_DOCUMENTER, NULL, NULL);
    rxd_swarm_add_agent(RXD_AGENT_INTEGRATOR, NULL, NULL);
    
    /* Check names contain role identifiers */
    ASSERT(strstr(g_swarm.agents[0].name, "architect") != NULL);
    ASSERT(strstr(g_swarm.agents[1].name, "frontend") != NULL);
    ASSERT(strstr(g_swarm.agents[2].name, "backend") != NULL);
    ASSERT(strstr(g_swarm.agents[3].name, "tester") != NULL);
    ASSERT(strstr(g_swarm.agents[4].name, "reviewer") != NULL);
    ASSERT(strstr(g_swarm.agents[5].name, "optimizer") != NULL);
    ASSERT(strstr(g_swarm.agents[6].name, "documenter") != NULL);
    ASSERT(strstr(g_swarm.agents[7].name, "integrator") != NULL);
    
    memset(&g_swarm, 0, sizeof(g_swarm));
}

/* ═══════════════════════════════════════════════════════════════════════════
   TOKEN BUDGET TESTS
   ═══════════════════════════════════════════════════════════════════════════ */

TEST(token_budget_init) {
    rxd_token_budget_init(1000000, 8);
    
    ASSERT_EQ(g_token_budget.budget, 1000000);
    ASSERT_EQ(g_token_budget.used, 0);
    ASSERT_EQ(g_token_budget.per_agent_budget, 125000);
    ASSERT(!g_token_budget.exceeded);
    
    memset(&g_token_budget, 0, sizeof(g_token_budget));
}

TEST(token_budget_reserve) {
    rxd_token_budget_init(1000, 2);
    
    RXDAgent agent = {0};
    strcpy(agent.name, "test_agent");
    
    /* Reserve within budget */
    bool reserved = rxd_token_budget_reserve(&agent, 500);
    ASSERT(reserved);
    ASSERT_EQ(g_token_budget.used, 500);
    ASSERT_EQ(g_token_budget.reserved, 500);
    
    /* Reserve remaining */
    reserved = rxd_token_budget_reserve(&agent, 500);
    ASSERT(reserved);
    ASSERT_EQ(g_token_budget.used, 1000);
    
    /* Should fail - over budget */
    reserved = rxd_token_budget_reserve(&agent, 1);
    ASSERT(!reserved);
    ASSERT(g_token_budget.exceeded);
    
    memset(&g_token_budget, 0, sizeof(g_token_budget));
}

TEST(token_budget_release) {
    rxd_token_budget_init(1000, 1);
    
    RXDAgent agent = {0};
    rxd_token_budget_reserve(&agent, 500);
    
    ASSERT_EQ(g_token_budget.reserved, 500);
    
    rxd_token_budget_release(200);
    ASSERT_EQ(g_token_budget.reserved, 300);
    
    rxd_token_budget_release(300);
    ASSERT_EQ(g_token_budget.reserved, 0);
    
    memset(&g_token_budget, 0, sizeof(g_token_budget));
}

/* ═══════════════════════════════════════════════════════════════════════════
   CHAIN OF THOUGHT TESTS
   ═══════════════════════════════════════════════════════════════════════════ */

TEST(chain_of_thought_run) {
    RXDChainOfThought cot = rxd_run_chain_of_thought("Test task");
    
    ASSERT_EQ(cot.step_count, 5);
    ASSERT(cot.total_tokens > 0);
    ASSERT(cot.total_time_ns > 0);
    ASSERT_STR_EQ(cot.final_answer, "Chain of thought complete.");
    
    /* Check each step */
    ASSERT_STR_EQ(cot.steps[0].thought, "Analyzing problem requirements...");
    ASSERT_STR_EQ(cot.steps[0].model_used, "Q4_0-small");
    ASSERT_EQ(cot.steps[0].tokens, 256);
    
    ASSERT_STR_EQ(cot.steps[4].thought, "Reviewing and refining...");
    ASSERT_STR_EQ(cot.steps[4].model_used, "Q6_K-reviewer");
}

TEST(chain_of_thought_steps) {
    memset(&g_cot, 0, sizeof(g_cot));
    
    rxd_cot_add_step("Step 1", "model_a", 100, 0.9f);
    rxd_cot_add_step("Step 2", "model_b", 200, 0.8f);
    rxd_cot_add_step("Step 3", "model_c", 300, 0.7f);
    
    ASSERT_EQ(g_cot.step_count, 3);
    ASSERT_EQ(g_cot.total_tokens, 600);
    
    ASSERT_STR_EQ(g_cot.steps[0].thought, "Step 1");
    ASSERT_STR_EQ(g_cot.steps[0].model_used, "model_a");
    ASSERT_EQ(g_cot.steps[0].tokens, 100);
    ASSERT_EQ(g_cot.steps[0].confidence, 0.9f);
}

/* ═══════════════════════════════════════════════════════════════════════════
   CASCADE TESTS
   ═══════════════════════════════════════════════════════════════════════════ */

TEST(cascade_init) {
    rxd_cascade_init("small.gguf", "medium.gguf", "large.gguf");
    
    ASSERT_STR_EQ(g_cascade.small_model, "small.gguf");
    ASSERT_STR_EQ(g_cascade.medium_model, "medium.gguf");
    ASSERT_STR_EQ(g_cascade.large_model, "large.gguf");
    ASSERT_EQ(g_cascade.confidence_threshold, 0.8f);
    ASSERT_EQ(g_cascade.max_cascade_depth, 3);
    
    memset(&g_cascade, 0, sizeof(g_cascade));
}

TEST(cascade_infer) {
    rxd_cascade_init("small.gguf", "medium.gguf", "large.gguf");
    
    RXDInferenceResult result = rxd_cascade_infer("What is 2+2?");
    
    ASSERT(result.success);
    ASSERT(result.tokens > 0);
    ASSERT(result.tps > 0);
    
    /* Stats should be updated */
    ASSERT(g_cascade_stats.small_queries > 0);
    ASSERT(g_cascade_stats.avg_confidence > 0);
    
    memset(&g_cascade, 0, sizeof(g_cascade));
    memset(&g_cascade_stats, 0, sizeof(g_cascade_stats));
}

/* ═══════════════════════════════════════════════════════════════════════════
   MAX MODE API TESTS
   ═══════════════════════════════════════════════════════════════════════════ */

TEST(max_mode_init) {
    bool init = rxd_max_mode_init("/test/repo", 8);
    
    ASSERT(init);
    ASSERT_STR_EQ(g_wtm.base_repo, "/test/repo");
    ASSERT_EQ(g_swarm.mode, RXD_AGENT_MODE_BEST_OF_N);
    ASSERT_EQ(g_token_budget.budget, RXD_MAX_TOKEN_BUDGET);
    
    rxd_max_cleanup();
}

TEST(max_mode_add_agents) {
    rxd_max_mode_init("/test/repo", 8);
    
    uint32_t id = rxd_max_add_agent(RXD_AGENT_ARCHITECT, "architect.gguf", NULL);
    ASSERT_EQ(id, 0);
    
    id = rxd_max_add_agent(RXD_AGENT_FRONTEND, "frontend.gguf", NULL);
    ASSERT_EQ(id, 1);
    
    ASSERT_EQ(g_swarm.agent_count, 2);
    
    rxd_max_cleanup();
}

TEST(max_mode_status) {
    rxd_max_mode_init("/test/repo", 4);
    
    rxd_max_add_agent(RXD_AGENT_ARCHITECT, NULL, NULL);
    rxd_max_add_agent(RXD_AGENT_FRONTEND, NULL, NULL);
    
    RXDSwarmStatus status = rxd_max_get_status();
    
    ASSERT_EQ(status.active_agents, 0);
    ASSERT_EQ(status.completed, 0);
    ASSERT_EQ(status.failed, 0);
    ASSERT(!status.is_running);
    ASSERT_EQ(status.progress, 0.0f);
    ASSERT_EQ(status.tokens_used, 0);
    ASSERT_EQ(status.tokens_remaining, RXD_MAX_TOKEN_BUDGET);
    
    rxd_max_cleanup();
}

TEST(max_mode_cleanup) {
    rxd_max_mode_init("/test/repo", 8);
    
    for (int i = 0; i < 8; i++) {
        rxd_max_add_agent(RXD_AGENT_ARCHITECT, NULL, NULL);
    }
    
    ASSERT_EQ(g_swarm.agent_count, 8);
    
    rxd_max_cleanup();
    
    ASSERT_EQ(g_swarm.agent_count, 0);
    ASSERT_EQ(g_cot.step_count, 0);
    ASSERT_EQ(g_token_budget.budget, 0);
}

/* ═══════════════════════════════════════════════════════════════════════════
   REPORT GENERATION TESTS
   ═══════════════════════════════════════════════════════════════════════════ */

TEST(swarm_report_json) {
    rxd_swarm_init(RXD_AGENT_MODE_BEST_OF_N);
    
    /* Add some agents */
    rxd_swarm_add_agent(RXD_AGENT_ARCHITECT, NULL, NULL);
    rxd_swarm_add_agent(RXD_AGENT_FRONTEND, NULL, NULL);
    
    /* Simulate completion */
    g_swarm.agents[0].state = RXD_AGENT_COMPLETED;
    g_swarm.agents[0].tokens_used = 1000;
    g_swarm.agents[0].quality_score = 0.85f;
    g_swarm.agents[0].start_time = 1000000;
    g_swarm.agents[0].end_time = 2000000;
    
    g_swarm.agents[1].state = RXD_AGENT_COMPLETED;
    g_swarm.agents[1].tokens_used = 1200;
    g_swarm.agents[1].quality_score = 0.90f;
    g_swarm.agents[1].start_time = 1000000;
    g_swarm.agents[1].end_time = 2500000;
    
    g_swarm.completed_agents = 2;
    g_swarm.start_time = 1000000;
    g_swarm.end_time = 2500000;
    
    RXDSwarmReport report = rxd_swarm_generate_report();
    
    ASSERT_EQ(report.total_agents, 2);
    ASSERT_EQ(report.completed, 2);
    ASSERT_EQ(report.failed, 0);
    ASSERT(report.total_time_ms > 0);
    ASSERT(report.total_tokens > 0);
    
    char* json = rxd_swarm_report_json(&report);
    ASSERT(json != NULL);
    ASSERT(strstr(json, "\"total_agents\":2") != NULL);
    ASSERT(strstr(json, "\"completed\":2") != NULL);
    
    free(json);
    rxd_max_cleanup();
}

/* ═══════════════════════════════════════════════════════════════════════════
   DIFF AGGREGATION TESTS
   ═══════════════════════════════════════════════════════════════════════════ */

TEST(aggregated_diff) {
    rxd_worktree_manager_init("/tmp/diff_test");
    rxd_swarm_init(RXD_AGENT_MODE_BEST_OF_N);
    
    /* Add agent and simulate completion */
    rxd_swarm_add_agent(RXD_AGENT_ARCHITECT, NULL, NULL);
    
    g_swarm.agents[0].state = RXD_AGENT_COMPLETED;
    g_swarm.agents[0].worktree = rxd_worktree_create("diff_test_agent");
    
    /* Write some output */
    const char* test_diff = "++ Added line\n-- Removed line\n++ Another addition";
    rxd_worktree_write_file(g_swarm.agents[0].worktree, "output.md", test_diff, strlen(test_diff));
    
    RXDAggregatedDiff agg = rxd_aggregate_diffs();
    
    ASSERT(agg.diff_count > 0);
    ASSERT(agg.total_additions > 0);
    ASSERT(agg.total_deletions > 0);
    
    rxd_aggregated_diff_free(&agg);
    rxd_max_cleanup();
}

/* ═══════════════════════════════════════════════════════════════════════════
   MAIN
   ═══════════════════════════════════════════════════════════════════════════ */

int main(void) {
    printf("═══════════════════════════════════════════════════════════════════════════\n");
    printf("  RAWRXD_SWARM Test Suite\n");
    printf("═══════════════════════════════════════════════════════════════════════════\n\n");
    
    printf("─── Worktree Tests ───\n");
    RUN_TEST(worktree_manager_init);
    RUN_TEST(worktree_create);
    RUN_TEST(worktree_write_read);
    RUN_TEST(worktree_copy_file);
    RUN_TEST(worktree_multiple_agents);
    
    printf("\n─── Agent Tests ───\n");
    RUN_TEST(swarm_init);
    RUN_TEST(swarm_add_agent);
    RUN_TEST(swarm_max_agents);
    RUN_TEST(agent_role_names);
    
    printf("\n─── Token Budget Tests ───\n");
    RUN_TEST(token_budget_init);
    RUN_TEST(token_budget_reserve);
    RUN_TEST(token_budget_release);
    
    printf("\n─── Chain of Thought Tests ───\n");
    RUN_TEST(chain_of_thought_run);
    RUN_TEST(chain_of_thought_steps);
    
    printf("\n─── Cascade Tests ───\n");
    RUN_TEST(cascade_init);
    RUN_TEST(cascade_infer);
    
    printf("\n─── MAX Mode API Tests ───\n");
    RUN_TEST(max_mode_init);
    RUN_TEST(max_mode_add_agents);
    RUN_TEST(max_mode_status);
    RUN_TEST(max_mode_cleanup);
    
    printf("\n─── Report Tests ───\n");
    RUN_TEST(swarm_report_json);
    
    printf("\n─── Diff Tests ───\n");
    RUN_TEST(aggregated_diff);
    
    printf("\n═══════════════════════════════════════════════════════════════════════════\n");
    printf("  Results: %d passed, %d failed\n", tests_passed, tests_failed);
    printf("═══════════════════════════════════════════════════════════════════════════\n");
    
    return tests_failed > 0 ? 1 : 0;
}