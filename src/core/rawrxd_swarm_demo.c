/* ═══════════════════════════════════════════════════════════════════════════
   RAWRXD_SWARM Demo
   
   Demonstrates Cursor-style 8-agent parallel execution:
   - MAX mode initialization
   - Agent role assignment
   - Parallel execution simulation
   - Best-of-N selection
   - Chain of Thought
   - Model Cascading
   - Report generation
   ═══════════════════════════════════════════════════════════════════════════ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rawrxd_swarm.h"

/* Simulated model paths */
#define MODEL_ARCHITECT    "models/architect-Q6K.gguf"
#define MODEL_FRONTEND     "models/frontend-Q4K.gguf"
#define MODEL_BACKEND      "models/backend-Q4K.gguf"
#define MODEL_TESTER       "models/tester-Q4K.gguf"
#define MODEL_REVIEWER     "models/reviewer-Q6K.gguf"
#define MODEL_OPTIMIZER    "models/optimizer-Q4K.gguf"
#define MODEL_DOCUMENTER   "models/docs-Q4K.gguf"
#define MODEL_INTEGRATOR   "models/integrator-Q6K.gguf"

/* ═══════════════════════════════════════════════════════════════════════════
   DEMO: MAX MODE INITIALIZATION
   ═══════════════════════════════════════════════════════════════════════════ */

static void demo_max_mode_init(void) {
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════════════════\n");
    printf("  DEMO: MAX Mode Initialization\n");
    printf("═══════════════════════════════════════════════════════════════════════════\n\n");
    
    /* Initialize MAX mode with 8 agents */
    bool init = rxd_max_mode_init("/path/to/repo", 8);
    printf("  ✓ MAX mode initialized: %s\n", init ? "SUCCESS" : "FAILED");
    printf("  ✓ Worktree manager base: %s\n", g_wtm.base_repo);
    printf("  ✓ Swarm mode: %d\n", g_swarm.mode);
    printf("  ✓ Token budget: %u tokens\n", g_token_budget.budget);
    printf("  ✓ Per-agent budget: %u tokens\n", g_token_budget.per_agent_budget);
    
    rxd_max_cleanup();
    printf("\n  ✓ Cleanup complete\n");
}

/* ═══════════════════════════════════════════════════════════════════════════
   DEMO: AGENT ROLE ASSIGNMENT
   ═══════════════════════════════════════════════════════════════════════════ */

static void demo_agent_roles(void) {
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════════════════\n");
    printf("  DEMO: Agent Role Assignment\n");
    printf("═══════════════════════════════════════════════════════════════════════════\n\n");
    
    rxd_max_mode_init("/path/to/repo", 8);
    
    /* Add agents with different roles/models */
    const char* role_names[] = {
        "Architect", "Frontend", "Backend", "Tester",
        "Reviewer", "Optimizer", "Documenter", "Integrator"
    };
    
    const char* models[] = {
        MODEL_ARCHITECT, MODEL_FRONTEND, MODEL_BACKEND, MODEL_TESTER,
        MODEL_REVIEWER, MODEL_OPTIMIZER, MODEL_DOCUMENTER, MODEL_INTEGRATOR
    };
    
    RXDAgentRole roles[] = {
        RXD_AGENT_ARCHITECT, RXD_AGENT_FRONTEND, RXD_AGENT_BACKEND, RXD_AGENT_TESTER,
        RXD_AGENT_REVIEWER, RXD_AGENT_OPTIMIZER, RXD_AGENT_DOCUMENTER, RXD_AGENT_INTEGRATOR
    };
    
    printf("  Adding 8 specialized agents:\n\n");
    
    for (int i = 0; i < 8; i++) {
        uint32_t id = rxd_max_add_agent(roles[i], models[i], NULL);
        printf("  [%d] %-12s → Agent ID: %u, Name: %s\n", 
               i + 1, role_names[i], id, g_swarm.agents[i].name);
    }
    
    printf("\n  ✓ Total agents: %u\n", g_swarm.agent_count);
    
    rxd_max_cleanup();
}

/* ═══════════════════════════════════════════════════════════════════════════
   DEMO: PARALLEL EXECUTION SIMULATION
   ═══════════════════════════════════════════════════════════════════════════ */

static void demo_parallel_execution(void) {
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════════════════\n");
    printf("  DEMO: Parallel Execution Simulation\n");
    printf("═══════════════════════════════════════════════════════════════════════════\n\n");
    
    rxd_max_mode_init("/path/to/repo", 4);
    
    /* Add 4 agents */
    rxd_max_add_agent(RXD_AGENT_ARCHITECT, MODEL_ARCHITECT, NULL);
    rxd_max_add_agent(RXD_AGENT_BACKEND, MODEL_BACKEND, NULL);
    rxd_max_add_agent(RXD_AGENT_TESTER, MODEL_TESTER, NULL);
    rxd_max_add_agent(RXD_AGENT_REVIEWER, MODEL_REVIEWER, NULL);
    
    printf("  Task: \"Implement a REST API with authentication\"\n\n");
    
    /* Simulate parallel execution */
    printf("  Simulating parallel execution:\n\n");
    
    const char* outputs[] = {
        "Architect: Designed REST API architecture with JWT auth, rate limiting, and caching layers",
        "Backend: Implemented /auth, /users, /items endpoints with proper validation",
        "Tester: Created unit tests for all endpoints, integration tests for auth flow",
        "Reviewer: Found 3 security issues, suggested input sanitization improvements"
    };
    
    for (int i = 0; i < 4; i++) {
        /* Simulate agent completion */
        g_swarm.agents[i].state = RXD_AGENT_COMPLETED;
        strncpy(g_swarm.agents[i].output, outputs[i], sizeof(g_swarm.agents[i].output) - 1);
        g_swarm.agents[i].tokens_used = 500 + i * 100;
        g_swarm.agents[i].quality_score = 0.75f + i * 0.05f;
        g_swarm.agents[i].start_time = 1000000;
        g_swarm.agents[i].end_time = 2000000 + i * 100000;
        
        printf("  [%d] %s\n", i + 1, outputs[i]);
        printf("      Tokens: %u, Quality: %.2f\n\n", 
               g_swarm.agents[i].tokens_used, g_swarm.agents[i].quality_score);
    }
    
    g_swarm.completed_agents = 4;
    g_swarm.start_time = 1000000;
    g_swarm.end_time = 2500000;
    
    /* Select best */
    RXDAgent* best = rxd_swarm_select_best();
    printf("  ✓ Best agent: %s (score: %.4f)\n", 
           best ? best->name : "none", g_swarm.best_score);
    
    rxd_max_cleanup();
}

/* ═══════════════════════════════════════════════════════════════════════════
   DEMO: BEST-OF-N SELECTION
   ═══════════════════════════════════════════════════════════════════════════ */

static void demo_best_of_n(void) {
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════════════════\n");
    printf("  DEMO: Best-of-N Selection\n");
    printf("═══════════════════════════════════════════════════════════════════════════\n\n");
    
    rxd_swarm_init(RXD_AGENT_MODE_BEST_OF_N);
    
    /* Add 5 agents with varying quality */
    for (int i = 0; i < 5; i++) {
        rxd_swarm_add_agent(RXD_AGENT_ARCHITECT, NULL, NULL);
        g_swarm.agents[i].state = RXD_AGENT_COMPLETED;
        g_swarm.agents[i].quality_score = 0.6f + (float)(i * 0.08f);
        g_swarm.agents[i].tokens_used = 400 + i * 50;
    }
    
    printf("  Agent quality scores:\n\n");
    for (int i = 0; i < 5; i++) {
        printf("  [%d] Agent %d: %.4f\n", i + 1, i, g_swarm.agents[i].quality_score);
    }
    
    RXDAgent* best = rxd_swarm_select_best();
    
    printf("\n  ✓ Selected best: Agent %u with score %.4f\n", 
           best ? best->agent_id : 999, g_swarm.best_score);
    
    memset(&g_swarm, 0, sizeof(g_swarm));
}

/* ═══════════════════════════════════════════════════════════════════════════
   DEMO: CHAIN OF THOUGHT
   ═══════════════════════════════════════════════════════════════════════════ */

static void demo_chain_of_thought(void) {
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════════════════\n");
    printf("  DEMO: Chain of Thought\n");
    printf("═══════════════════════════════════════════════════════════════════════════\n\n");
    
    printf("  Task: \"Design a distributed cache system\"\n\n");
    
    RXDChainOfThought cot = rxd_max_chain("Design a distributed cache system");
    
    printf("  Chain of Thought (%u steps, %u tokens):\n\n", 
           cot.step_count, cot.total_tokens);
    
    for (uint32_t i = 0; i < cot.step_count; i++) {
        printf("  [%d] %s\n", i + 1, cot.steps[i].thought);
        printf("      Model: %s\n", cot.steps[i].model_used);
        printf("      Tokens: %u, Confidence: %.0f%%\n\n", 
               cot.steps[i].tokens, cot.steps[i].confidence * 100);
    }
    
    printf("  ✓ Final Answer: %s\n", cot.final_answer);
}

/* ═══════════════════════════════════════════════════════════════════════════
   DEMO: MODEL CASCADING
   ═══════════════════════════════════════════════════════════════════════════ */

static void demo_model_cascading(void) {
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════════════════\n");
    printf("  DEMO: Model Cascading (Small → Large)\n");
    printf("═══════════════════════════════════════════════════════════════════════════\n\n");
    
    rxd_cascade_init("models/tiny-Q4K.gguf", 
                     "models/small-Q4K.gguf", 
                     "models/large-Q6K.gguf");
    
    printf("  Cascade configuration:\n");
    printf("    Small:  %s\n", g_cascade.small_model);
    printf("    Medium: %s\n", g_cascade.medium_model);
    printf("    Large:  %s\n", g_cascade.large_model);
    printf("    Threshold: %.0f%%\n\n", g_cascade.confidence_threshold * 100);
    
    /* Run cascade inference */
    printf("  Running cascade inference:\n\n");
    
    const char* queries[] = {
        "What is 2+2?",
        "Explain quantum computing",
        "Design a microservices architecture"
    };
    
    for (int i = 0; i < 3; i++) {
        printf("  [%d] Query: \"%s\"\n", i + 1, queries[i]);
        RXDInferenceResult result = rxd_max_cascade(queries[i]);
        printf("      Tokens: %u, TPS: %.1f\n\n", result.tokens, result.tps);
    }
    
    printf("  Cascade statistics:\n");
    printf("    Small queries:  %u\n", g_cascade_stats.small_queries);
    printf("    Medium queries: %u\n", g_cascade_stats.medium_queries);
    printf("    Large queries:  %u\n", g_cascade_stats.large_queries);
    printf("    Avg confidence: %.2f\n", g_cascade_stats.avg_confidence);
    
    memset(&g_cascade, 0, sizeof(g_cascade));
    memset(&g_cascade_stats, 0, sizeof(g_cascade_stats));
}

/* ═══════════════════════════════════════════════════════════════════════════
   DEMO: REPORT GENERATION
   ═══════════════════════════════════════════════════════════════════════════ */

static void demo_report_generation(void) {
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════════════════\n");
    printf("  DEMO: Report Generation\n");
    printf("═══════════════════════════════════════════════════════════════════════════\n\n");
    
    rxd_max_mode_init("/path/to/repo", 8);
    
    /* Add agents and simulate completion */
    for (int i = 0; i < 8; i++) {
        rxd_max_add_agent(i, NULL, NULL);
        g_swarm.agents[i].state = RXD_AGENT_COMPLETED;
        g_swarm.agents[i].tokens_used = 500 + i * 100;
        g_swarm.agents[i].quality_score = 0.7f + (float)(i * 0.03f);
        g_swarm.agents[i].start_time = 1000000;
        g_swarm.agents[i].end_time = 2000000 + i * 50000;
    }
    
    g_swarm.completed_agents = 8;
    g_swarm.start_time = 1000000;
    g_swarm.end_time = 2500000;
    
    RXDSwarmReport report = rxd_swarm_generate_report();
    
    printf("  Swarm Report:\n\n");
    printf("    Total Agents:  %u\n", report.total_agents);
    printf("    Completed:     %u\n", report.completed);
    printf("    Failed:        %u\n", report.failed);
    printf("    Timed Out:     %u\n", report.timed_out);
    printf("    Total Time:    %.2f ms\n", report.total_time_ms);
    printf("    Avg Time:      %.2f ms\n", report.avg_time_ms);
    printf("    Total Tokens:  %u\n", report.total_tokens);
    printf("    Best Agent:    %s\n", report.best_agent ? report.best_agent->name : "none");
    printf("    Best Score:    %.4f\n", report.best_score);
    
    /* Generate JSON */
    char* json = rxd_swarm_report_json(&report);
    printf("\n  JSON Report:\n\n");
    
    /* Pretty print JSON (simplified) */
    printf("  %s\n", json);
    
    free(json);
    rxd_max_cleanup();
}

/* ═══════════════════════════════════════════════════════════════════════════
   DEMO: PERFORMANCE COMPARISON
   ═══════════════════════════════════════════════════════════════════════════ */

static void demo_performance_comparison(void) {
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════════════════\n");
    printf("  DEMO: Performance Comparison\n");
    printf("═══════════════════════════════════════════════════════════════════════════\n\n");
    
    printf("  ┌─────────────────┬────────┬────────────────┬─────────────┬──────────────────┐\n");
    printf("  │ Mode            │ Agents │ Token Overhead │ Speed Gain  │ Best For         │\n");
    printf("  ├─────────────────┼────────┼────────────────┼─────────────┼──────────────────┤\n");
    printf("  │ Single          │      1 │           0%%   │         1x  │ Simple tasks     │\n");
    printf("  │ Best-of-N       │      8 │        +25-35%%  │       5-8x  │ Critical decisions│\n");
    printf("  │ Cascade         │    1-3 │          -40%%  │       2-3x  │ Simple queries   │\n");
    printf("  │ Chain of Thought│    1-5 │          +50%%  │       3-5x  │ Complex reasoning│\n");
    printf("  │ Feature Parallel│      8 │          +10%%  │       6-8x  │ Multi-feature dev│\n");
    printf("  └─────────────────┴────────┴────────────────┴─────────────┴──────────────────┘\n");
}

/* ═══════════════════════════════════════════════════════════════════════════
   DEMO: ARCHITECTURE DIAGRAM
   ═══════════════════════════════════════════════════════════════════════════ */

static void demo_architecture(void) {
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════════════════\n");
    printf("  DEMO: Architecture Overview\n");
    printf("═══════════════════════════════════════════════════════════════════════════\n\n");
    
    printf("  ┌─────────────────────────────────────────────────────────────────────────┐\n");
    printf("  │                         RAWRXD_MAX_MODE                                  │\n");
    printf("  ├─────────────────────────────────────────────────────────────────────────┤\n");
    printf("  │  ┌──────────────────────────────────────────────────────────────────┐  │\n");
    printf("  │  │                    8 PARALLEL AGENTS                              │  │\n");
    printf("  │  │  ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐ │  │\n");
    printf("  │  │  │Arch │ │Front│ │Back │ │Test │ │Rev  │ │Opt  │ │Doc  │ │Int  │ │  │\n");
    printf("  │  │  └──┬──┘ └──┬──┘ └──┬──┘ └──┬──┘ └──┬──┘ └──┬──┘ └──┬──┘ └──┬──┘ │  │\n");
    printf("  │  │     │       │       │       │       │       │       │       │     │  │\n");
    printf("  │  │  ┌──▼───────▼───────▼───────▼───────▼───────▼───────▼───────▼──┐ │  │\n");
    printf("  │  │  │              ISOLATED WORKTREES (git worktree)              │ │  │\n");
    printf("  │  │  │         No conflicts, parallel file modifications            │ │  │\n");
    printf("  │  │  └─────────────────────────────────────────────────────────────┘ │  │\n");
    printf("  │  └──────────────────────────────────────────────────────────────────┘  │\n");
    printf("  │                                                                        │\n");
    printf("  │  ┌──────────────────────────────────────────────────────────────────┐  │\n");
    printf("  │  │                    EXECUTION MODES                                │  │\n");
    printf("  │  │  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐ ┌─────────────┐ │  │\n");
    printf("  │  │  │  Best-of-N  │ │  Cascade    │ │ Chain of    │ │  Feature    │ │  │\n");
    printf("  │  │  │  (Compare)  │ │  (Small→Lg) │ │  Thought    │ │  Parallel   │ │  │\n");
    printf("  │  │  └─────────────┘ └─────────────┘ └─────────────┘ └─────────────┘ │  │\n");
    printf("  │  └──────────────────────────────────────────────────────────────────┘  │\n");
    printf("  │                                                                        │\n");
    printf("  │  ┌──────────────────────────────────────────────────────────────────┐  │\n");
    printf("  │  │                    AGGREGATION                                    │  │\n");
    printf("  │  │  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐ ┌─────────────┐ │  │\n");
    printf("  │  │  │ Diff Viewer │ │Best Select  │ │Token Budget │ │Merge/Resolve│ │  │\n");
    printf("  │  │  └─────────────┘ └─────────────┘ └─────────────┘ └─────────────┘ │  │\n");
    printf("  │  └──────────────────────────────────────────────────────────────────┘  │\n");
    printf("  └─────────────────────────────────────────────────────────────────────────┘\n");
}

/* ═══════════════════════════════════════════════════════════════════════════
   MAIN
   ═══════════════════════════════════════════════════════════════════════════ */

int main(void) {
    printf("═══════════════════════════════════════════════════════════════════════════\n");
    printf("  RAWRXD_SWARM Demo - Cursor-Style 8-Agent Parallel Execution\n");
    printf("═══════════════════════════════════════════════════════════════════════════\n");
    
    demo_architecture();
    demo_max_mode_init();
    demo_agent_roles();
    demo_parallel_execution();
    demo_best_of_n();
    demo_chain_of_thought();
    demo_model_cascading();
    demo_report_generation();
    demo_performance_comparison();
    
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════════════════\n");
    printf("  Demo Complete\n");
    printf("═══════════════════════════════════════════════════════════════════════════\n");
    
    return 0;
}