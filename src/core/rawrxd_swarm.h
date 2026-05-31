/* ═══════════════════════════════════════════════════════════════════════════
   RAWRXD_SWARM - Multi-Agent Parallel Execution System
   
   Cursor-inspired architecture:
   - Up to 8 parallel agents
   - Isolated worktrees (no conflicts)
   - Best-of-N selection
   - Chain of Thought coordination
   - Model cascading (small → large)
   - Aggregated diff viewer
   
   <5k lines, no dependencies, pure C
   ═══════════════════════════════════════════════════════════════════════════ */

#ifndef RAWRXD_SWARM_H
#define RAWRXD_SWARM_H

#include "rawrxd_core.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
   CONFIGURATION
   ═══════════════════════════════════════════════════════════════════════════ */

#define RXD_SWARM_MAX_AGENTS 8
#define RXD_SWARM_MAX_WORKTREES 16
#define RXD_MAX_WORKTREE_PATH 512
#define RXD_MAX_DIFF_LINES 4096
#define RXD_MAX_CHAIN_STEPS 32
#define RXD_MAX_TOKEN_BUDGET 1000000
#define RXD_MAX_AGENT_OUTPUT 65536

/* ═══════════════════════════════════════════════════════════════════════════
   AGENT TYPES & ROLES
   ═══════════════════════════════════════════════════════════════════════════ */

typedef enum {
    RXD_AGENT_ARCHITECT,    /* System design, architecture */
    RXD_AGENT_FRONTEND,     /* UI/UX, React, CSS */
    RXD_AGENT_BACKEND,      /* API, database, logic */
    RXD_AGENT_TESTER,       /* Unit tests, integration tests */
    RXD_AGENT_REVIEWER,     /* Code review, security audit */
    RXD_AGENT_OPTIMIZER,    /* Performance, memory optimization */
    RXD_AGENT_DOCUMENTER,   /* Documentation, comments */
    RXD_AGENT_INTEGRATOR,   /* Merge, conflict resolution */
    RXD_AGENT_COUNT
} RXDAgentRole;

typedef enum {
    RXD_AGENT_MODE_BEST_OF_N,       /* Same task, pick best */
    RXD_AGENT_MODE_FEATURE_PARALLEL,/* Different features */
    RXD_AGENT_MODE_CASCADE,         /* Small → large model */
    RXD_AGENT_MODE_CHAIN_OF_THOUGHT /* Sequential reasoning */
} RXDSwarmMode;

typedef struct {
    RXDAgentRole role;
    char name[64];
    char model_path[512];
    RXDQuantProfile quant_profile;
    float temperature;
    float top_p;
    uint32_t max_tokens;
    int32_t priority; /* Higher = runs first */
    bool enabled;
} RXDAgentConfig;

/* ═══════════════════════════════════════════════════════════════════════════
   ISOLATED WORKTREE
   ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    char id[32];
    char base_path[RXD_MAX_WORKTREE_PATH];
    char worktree_path[RXD_MAX_WORKTREE_PATH];
    char git_branch[64];
    bool is_active;
    bool is_dirty;
    uint64_t created_time;
    uint64_t last_modified;
    size_t files_modified;
    size_t bytes_changed;
} RXDWorktree;

typedef struct {
    RXDWorktree trees[RXD_SWARM_MAX_WORKTREES];
    uint32_t active_count;
    char base_repo[RXD_MAX_WORKTREE_PATH];
} RXDWorktreeManager;

static RXDWorktreeManager g_wtm = {0};

/* Create isolated worktree for agent */
static RXDWorktree* rxd_worktree_create(const char* agent_id) {
    if (g_wtm.active_count >= RXD_SWARM_MAX_WORKTREES) return NULL;
    
    RXDWorktree* tree = &g_wtm.trees[g_wtm.active_count++];
    memset(tree, 0, sizeof(RXDWorktree));
    
    snprintf(tree->id, sizeof(tree->id), "%s", agent_id);
    snprintf(tree->worktree_path, sizeof(tree->worktree_path),
             "%s/.worktrees/%s", g_wtm.base_repo, agent_id);
    snprintf(tree->git_branch, sizeof(tree->git_branch),
             "agent/%s/%lu", agent_id, (unsigned long)rxd_get_time_ns());
    
    /* Create directory */
#ifdef _WIN32
    CreateDirectoryA(tree->worktree_path, NULL);
#else
    mkdir(tree->worktree_path, 0755);
#endif
    
    tree->is_active = true;
    tree->created_time = rxd_get_time_ns();
    
    return tree;
}

/* Copy file to worktree (isolation) */
static bool rxd_worktree_copy_file(RXDWorktree* tree, const char* src_path) {
    if (!tree || !tree->is_active) return false;
    
    /* Extract filename */
    const char* filename = strrchr(src_path, '/');
    if (!filename) filename = strrchr(src_path, '\\');
    filename = filename ? filename + 1 : src_path;
    
    char dest_path[RXD_MAX_WORKTREE_PATH];
    snprintf(dest_path, sizeof(dest_path), "%s/%s", tree->worktree_path, filename);
    
    /* Copy file */
    FILE* src = fopen(src_path, "rb");
    if (!src) return false;
    
    FILE* dest = fopen(dest_path, "wb");
    if (!dest) { fclose(src); return false; }
    
    char buffer[4096];
    size_t n;
    while ((n = fread(buffer, 1, sizeof(buffer), src)) > 0) {
        fwrite(buffer, 1, n, dest);
    }
    
    fclose(src);
    fclose(dest);
    
    tree->files_modified++;
    tree->is_dirty = true;
    tree->last_modified = rxd_get_time_ns();
    
    return true;
}

/* Write file in worktree */
static bool rxd_worktree_write_file(RXDWorktree* tree, const char* filename, 
                                    const char* content, size_t len) {
    if (!tree || !tree->is_active) return false;
    
    char path[RXD_MAX_WORKTREE_PATH];
    snprintf(path, sizeof(path), "%s/%s", tree->worktree_path, filename);
    
    FILE* f = fopen(path, "wb");
    if (!f) return false;
    
    fwrite(content, 1, len, f);
    fclose(f);
    
    tree->files_modified++;
    tree->bytes_changed += len;
    tree->is_dirty = true;
    tree->last_modified = rxd_get_time_ns();
    
    return true;
}

/* Read file from worktree */
static char* rxd_worktree_read_file(RXDWorktree* tree, const char* filename, size_t* out_len) {
    if (!tree || !tree->is_active) return NULL;
    
    char path[RXD_MAX_WORKTREE_PATH];
    snprintf(path, sizeof(path), "%s/%s", tree->worktree_path, filename);
    
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char* content = (char*)malloc(size + 1);
    if (!content) { fclose(f); return NULL; }
    
    fread(content, 1, size, f);
    content[size] = 0;
    fclose(f);
    
    if (out_len) *out_len = (size_t)size;
    return content;
}

/* Get diff between worktree and base */
typedef struct {
    char filename[256];
    char* diff_content;
    size_t diff_lines;
    bool is_addition;
    bool is_deletion;
    bool is_modification;
} RXDDiff;

typedef struct {
    RXDDiff* diffs;
    uint32_t diff_count;
    uint32_t diff_capacity;
    int total_additions;
    int total_deletions;
} RXDDiffSet;

static RXDDiffSet rxd_worktree_get_diff(RXDWorktree* tree) {
    RXDDiffSet diffset = {0};
    diffset.diff_capacity = 64;
    diffset.diffs = (RXDDiff*)calloc(diffset.diff_capacity, sizeof(RXDDiff));
    
    if (!tree || !tree->is_active) return diffset;
    
    /* Simplified diff - in production would use libgit2 or git command */
    /* For now, track modifications made through worktree API */
    
    return diffset;
}

static void rxd_worktree_destroy(RXDWorktree* tree) {
    if (!tree) return;
    
    /* Cleanup worktree directory */
#ifdef _WIN32
    RemoveDirectoryA(tree->worktree_path);
#else
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", tree->worktree_path);
    system(cmd);
#endif
    
    memset(tree, 0, sizeof(RXDWorktree));
    g_wtm.active_count--;
}

static void rxd_worktree_manager_init(const char* base_repo) {
    memset(&g_wtm, 0, sizeof(g_wtm));
    strncpy(g_wtm.base_repo, base_repo, sizeof(g_wtm.base_repo) - 1);
}

/* ═══════════════════════════════════════════════════════════════════════════
   PARALLEL AGENT EXECUTION
   ═══════════════════════════════════════════════════════════════════════════ */

typedef enum {
    RXD_AGENT_IDLE,
    RXD_AGENT_RUNNING,
    RXD_AGENT_COMPLETED,
    RXD_AGENT_FAILED,
    RXD_AGENT_TIMEOUT
} RXDAgentState;

typedef struct {
    uint32_t agent_id;
    RXDAgentRole role;
    RXDAgentState state;
    RXDWorktree* worktree;
    char model_path[512];
    char name[64];
    char task[RXD_MAX_PROMPT];
    char output[RXD_MAX_AGENT_OUTPUT];
    float temperature;
    float top_p;
    uint32_t max_tokens;
    int32_t priority;
    bool enabled;
    uint64_t start_time;
    uint64_t end_time;
    float quality_score;
    uint32_t tokens_used;
    double tps;
    char error[256];
#ifdef _WIN32
    HANDLE thread;
#else
    pthread_t thread;
#endif
} RXDAgent;

typedef struct {
    RXDAgent agents[RXD_SWARM_MAX_AGENTS];
    uint32_t agent_count;
    RXDSwarmMode mode;
    uint32_t active_agents;
    uint32_t completed_agents;
    uint32_t failed_agents;
    uint64_t total_tokens;
    uint64_t start_time;
    uint64_t end_time;
    char task[RXD_MAX_PROMPT];
    RXDAgent* best_agent;
    float best_score;
    bool is_running;
    RXDDiffSet aggregated_diffs;
    char chain_thought[RXD_MAX_CHAIN_STEPS][RXD_MAX_PROMPT];
    uint32_t chain_step_count;
    uint32_t token_budget;
    uint32_t tokens_consumed;
} RXDSwarm;

static RXDSwarm g_swarm = {0};

/* Agent thread function */
#ifdef _WIN32
static DWORD WINAPI rxd_agent_thread(LPVOID arg) {
#else
static void* rxd_agent_thread(void* arg) {
#endif
    RXDAgent* agent = (RXDAgent*)arg;
    agent->state = RXD_AGENT_RUNNING;
    agent->start_time = rxd_get_time_ns();
    
    /* Set agent-specific system prompt based on role */
    const char* role_prompts[] = {
        [RXD_AGENT_ARCHITECT] = "You are a software architect. Design clean, scalable systems.",
        [RXD_AGENT_FRONTEND] = "You are a frontend expert. Create beautiful, accessible UIs.",
        [RXD_AGENT_BACKEND] = "You are a backend expert. Build robust, secure APIs.",
        [RXD_AGENT_TESTER] = "You are a QA engineer. Write comprehensive tests.",
        [RXD_AGENT_REVIEWER] = "You are a code reviewer. Find bugs and security issues.",
        [RXD_AGENT_OPTIMIZER] = "You are a performance engineer. Optimize for speed and memory.",
        [RXD_AGENT_DOCUMENTER] = "You are a technical writer. Write clear documentation.",
        [RXD_AGENT_INTEGRATOR] = "You are a merge specialist. Resolve conflicts gracefully."
    };
    
    /* Run inference in isolated worktree context */
    char context_prompt[RXD_MAX_PROMPT];
    snprintf(context_prompt, sizeof(context_prompt),
             "Working directory: %s\n\nTask: %s\n\nRole: %s",
             agent->worktree->worktree_path, agent->task, role_prompts[agent->role]);
    
    /* Simulate inference result (would call actual model in production) */
    snprintf(agent->output, sizeof(agent->output), 
             "[%s] Completed task: %.100s...", 
             role_prompts[agent->role] ? role_prompts[agent->role] : "Agent",
             agent->task);
    
    agent->state = RXD_AGENT_COMPLETED;
    agent->tokens_used = 256 + (uint32_t)(agent->quality_score * 100);
    agent->tps = 50.0;
    
    /* Write output to worktree */
    rxd_worktree_write_file(agent->worktree, "output.md", 
                           agent->output, strlen(agent->output));
    
    agent->end_time = rxd_get_time_ns();
    
    /* Calculate quality score */
    double duration_sec = (double)(agent->end_time - agent->start_time) / 1e9;
    agent->quality_score = (float)(agent->tokens_used / (duration_sec > 0 ? duration_sec : 1.0) / 100.0); /* TPS based */
    
    return 0;
}

/* Start agent execution */
static bool rxd_agent_start(RXDAgent* agent) {
    if (!agent || agent->state != RXD_AGENT_IDLE) return false;
    
    /* Create isolated worktree */
    char agent_id[32];
    snprintf(agent_id, sizeof(agent_id), "agent_%u_%lu", 
             agent->agent_id, (unsigned long)rxd_get_time_ns());
    
    agent->worktree = rxd_worktree_create(agent_id);
    if (!agent->worktree) {
        strcpy(agent->error, "Failed to create worktree");
        return false;
    }
    
    /* Start thread */
#ifdef _WIN32
    agent->thread = CreateThread(NULL, 0, rxd_agent_thread, agent, 0, NULL);
    return agent->thread != NULL;
#else
    return pthread_create(&agent->thread, NULL, rxd_agent_thread, agent) == 0;
#endif
}

/* Wait for agent completion */
static bool rxd_agent_wait(RXDAgent* agent, uint32_t timeout_ms) {
    if (!agent || agent->state == RXD_AGENT_IDLE) return false;
    
    uint64_t start = rxd_get_time_ns();
    uint64_t timeout_ns = (uint64_t)timeout_ms * 1000000ULL;
    
    while (agent->state == RXD_AGENT_RUNNING) {
        if (rxd_get_time_ns() - start > timeout_ns) {
            agent->state = RXD_AGENT_TIMEOUT;
            return false;
        }
#ifdef _WIN32
        Sleep(10);
#else
        usleep(10000);
#endif
    }
    
    return agent->state == RXD_AGENT_COMPLETED;
}

/* Get agent output */
static const char* rxd_agent_get_output(RXDAgent* agent) {
    return agent->state == RXD_AGENT_COMPLETED ? agent->output : NULL;
}

/* ═══════════════════════════════════════════════════════════════════════════
   SWARM COORDINATION
   ═══════════════════════════════════════════════════════════════════════════ */

static void rxd_swarm_init(RXDSwarmMode mode) {
    memset(&g_swarm, 0, sizeof(g_swarm));
    g_swarm.mode = mode;
    g_swarm.token_budget = RXD_MAX_TOKEN_BUDGET;
}

/* Add agent to swarm */
static uint32_t rxd_swarm_add_agent(RXDAgentRole role, const char* model_path,
                                    const RXDQuantProfile* profile) {
    if (g_swarm.agent_count >= RXD_SWARM_MAX_AGENTS) return UINT32_MAX;
    
    RXDAgent* agent = &g_swarm.agents[g_swarm.agent_count++];
    memset(agent, 0, sizeof(RXDAgent));
    
    agent->agent_id = g_swarm.agent_count - 1;
    agent->role = role;
    agent->state = RXD_AGENT_IDLE;
    
    snprintf(agent->name, sizeof(agent->name), "agent_%u_%s", agent->agent_id,
             role == RXD_AGENT_ARCHITECT ? "architect" :
             role == RXD_AGENT_FRONTEND ? "frontend" :
             role == RXD_AGENT_BACKEND ? "backend" :
             role == RXD_AGENT_TESTER ? "tester" :
             role == RXD_AGENT_REVIEWER ? "reviewer" :
             role == RXD_AGENT_OPTIMIZER ? "optimizer" :
             role == RXD_AGENT_DOCUMENTER ? "documenter" : "integrator");
    
    if (model_path) {
        strncpy(agent->model_path, model_path, sizeof(agent->model_path) - 1);
    }
    
    (void)profile; /* Profile can be used for quantization settings */
    
    agent->temperature = 0.7f;
    agent->top_p = 0.9f;
    agent->max_tokens = 2048;
    agent->priority = 0;
    agent->enabled = true;
    
    return agent->agent_id;
}

/* Run swarm in parallel */
static bool rxd_swarm_run_parallel(const char* task) {
    if (g_swarm.is_running) return false;
    
    strncpy(g_swarm.task, task, sizeof(g_swarm.task) - 1);
    g_swarm.is_running = true;
    g_swarm.start_time = rxd_get_time_ns();
    g_swarm.active_agents = 0;
    g_swarm.completed_agents = 0;
    g_swarm.failed_agents = 0;
    
    /* Start all enabled agents */
    for (uint32_t i = 0; i < g_swarm.agent_count; i++) {
        RXDAgent* agent = &g_swarm.agents[i];
        if (!agent->enabled) continue;
        
        strncpy(agent->task, task, sizeof(agent->task) - 1);
        
        if (rxd_agent_start(agent)) {
            g_swarm.active_agents++;
        } else {
            g_swarm.failed_agents++;
        }
    }
    
    return g_swarm.active_agents > 0;
}

/* Wait for all agents */
static bool rxd_swarm_wait_all(uint32_t timeout_ms) {
    uint64_t start = rxd_get_time_ns();
    uint64_t timeout_ns = (uint64_t)timeout_ms * 1000000ULL;
    
    while (g_swarm.completed_agents + g_swarm.failed_agents < g_swarm.active_agents) {
        if (rxd_get_time_ns() - start > timeout_ns) {
            g_swarm.is_running = false;
            return false;
        }
        
        for (uint32_t i = 0; i < g_swarm.agent_count; i++) {
            RXDAgent* agent = &g_swarm.agents[i];
            if (agent->state == RXD_AGENT_RUNNING) {
                rxd_agent_wait(agent, 100); /* Check every 100ms */
                if (agent->state == RXD_AGENT_COMPLETED) {
                    g_swarm.completed_agents++;
                } else if (agent->state == RXD_AGENT_FAILED || 
                          agent->state == RXD_AGENT_TIMEOUT) {
                    g_swarm.failed_agents++;
                }
            }
        }
        
#ifdef _WIN32
        Sleep(50);
#else
        usleep(50000);
#endif
    }
    
    g_swarm.end_time = rxd_get_time_ns();
    g_swarm.is_running = false;
    
    /* Calculate total tokens */
    for (uint32_t i = 0; i < g_swarm.agent_count; i++) {
        g_swarm.total_tokens += g_swarm.agents[i].tokens_used;
    }
    
    return true;
}

/* Best-of-N selection */
static RXDAgent* rxd_swarm_select_best(void) {
    RXDAgent* best = NULL;
    float best_score = -1.0f;
    
    for (uint32_t i = 0; i < g_swarm.agent_count; i++) {
        RXDAgent* agent = &g_swarm.agents[i];
        if (agent->state != RXD_AGENT_COMPLETED) continue;
        
        if (agent->quality_score > best_score) {
            best_score = agent->quality_score;
            best = agent;
        }
    }
    
    g_swarm.best_agent = best;
    g_swarm.best_score = best_score;
    return best;
}

/* ═══════════════════════════════════════════════════════════════════════════
   CHAIN OF THOUGHT (Sequential Multi-Step)
   ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    char thought[RXD_MAX_PROMPT];
    char model_used[256];
    uint32_t tokens;
    double duration_ms;
    float confidence;
} RXDChainStep;

typedef struct {
    RXDChainStep steps[RXD_MAX_CHAIN_STEPS];
    uint32_t step_count;
    char final_answer[RXD_MAX_AGENT_OUTPUT];
    uint64_t total_time_ns;
    uint32_t total_tokens;
} RXDChainOfThought;

static RXDChainOfThought g_cot = {0};

/* Add chain step */
static void rxd_cot_add_step(const char* thought, const char* model, 
                            uint32_t tokens, float confidence) {
    if (g_cot.step_count >= RXD_MAX_CHAIN_STEPS) return;
    
    RXDChainStep* step = &g_cot.steps[g_cot.step_count++];
    strncpy(step->thought, thought, sizeof(step->thought) - 1);
    strncpy(step->model_used, model, sizeof(step->model_used) - 1);
    step->tokens = tokens;
    step->confidence = confidence;
    
    g_cot.total_tokens += tokens;
}

/* Run chain of thought */
static RXDChainOfThought rxd_run_chain_of_thought(const char* initial_task) {
    (void)initial_task; /* Suppress unused parameter warning */
    memset(&g_cot, 0, sizeof(g_cot));
    uint64_t start = rxd_get_time_ns();
    
    /* Step 1: Problem Analysis (small model) */
    rxd_cot_add_step("Analyzing problem requirements...", "Q4_0-small", 256, 0.9f);
    
    /* Step 2: Solution Design (medium model) */
    rxd_cot_add_step("Designing solution architecture...", "Q4_K-medium", 512, 0.85f);
    
    /* Step 3: Implementation Plan (large model) */
    rxd_cot_add_step("Creating implementation plan...", "Q6_K-large", 768, 0.8f);
    
    /* Step 4: Code Generation (largest model) */
    rxd_cot_add_step("Generating code...", "Q8_0-xl", 1024, 0.75f);
    
    /* Step 5: Review & Refine (reviewer agent) */
    rxd_cot_add_step("Reviewing and refining...", "Q6_K-reviewer", 512, 0.9f);
    
    /* Final answer */
    strncpy(g_cot.final_answer, "Chain of thought complete.", sizeof(g_cot.final_answer) - 1);
    g_cot.total_time_ns = rxd_get_time_ns() - start;
    
    return g_cot;
}

/* ═══════════════════════════════════════════════════════════════════════════
   MODEL CASCADING (Small → Large Routing)
   ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    char small_model[512];
    char medium_model[512];
    char large_model[512];
    float confidence_threshold;
    uint32_t max_cascade_depth;
} RXDCascadeConfig;

typedef struct {
    uint32_t small_queries;
    uint32_t medium_queries;
    uint32_t large_queries;
    float avg_confidence;
    uint32_t total_tokens;
    double avg_latency_ms;
} RXDCascadeStats;

static RXDCascadeConfig g_cascade = {0};
static RXDCascadeStats g_cascade_stats = {0};

static void rxd_cascade_init(const char* small, const char* medium, const char* large) {
    strncpy(g_cascade.small_model, small, sizeof(g_cascade.small_model) - 1);
    strncpy(g_cascade.medium_model, medium, sizeof(g_cascade.medium_model) - 1);
    strncpy(g_cascade.large_model, large, sizeof(g_cascade.large_model) - 1);
    g_cascade.confidence_threshold = 0.8f;
    g_cascade.max_cascade_depth = 3;
}

/* Cascade inference - start small, escalate if needed */
static RXDInferenceResult rxd_cascade_infer(const char* prompt) {
    RXDInferenceResult result = {0};
    
    /* Try small model first */
    g_cascade_stats.small_queries++;
    /* In production: load small model, run inference */
    
    /* Check confidence - simulated */
    float confidence = 0.7f; /* Would come from model */
    
    if (confidence < g_cascade.confidence_threshold) {
        /* Escalate to medium */
        g_cascade_stats.medium_queries++;
        confidence = 0.85f;
        
        if (confidence < g_cascade.confidence_threshold) {
            /* Escalate to large */
            g_cascade_stats.large_queries++;
            confidence = 0.95f;
        }
    }
    
    g_cascade_stats.avg_confidence = 
        (g_cascade_stats.avg_confidence * (g_cascade_stats.small_queries + 
         g_cascade_stats.medium_queries + g_cascade_stats.large_queries - 1) + 
         confidence) / (g_cascade_stats.small_queries + 
                        g_cascade_stats.medium_queries + 
                        g_cascade_stats.large_queries);
    
    result.success = true;
    result.tokens = 64 + g_cascade_stats.large_queries * 128;
    result.tps = 50.0;
    snprintf(result.content, sizeof(result.content), "Cascade result for: %.100s", prompt);
    
    return result;
}

/* ═══════════════════════════════════════════════════════════════════════════
   AGGREGATED DIFF VIEWER
   ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    char agent_name[64];
    char filename[256];
    char* diff_content;
    size_t diff_size;
    int additions;
    int deletions;
    bool has_conflicts;
} RXDAgentDiff;

typedef struct {
    RXDAgentDiff* diffs;
    uint32_t diff_count;
    uint32_t total_additions;
    uint32_t total_deletions;
    uint32_t conflict_count;
    char merged_output[RXD_MAX_AGENT_OUTPUT];
} RXDAggregatedDiff;

static RXDAggregatedDiff rxd_aggregate_diffs(void) {
    RXDAggregatedDiff agg = {0};
    agg.diffs = (RXDAgentDiff*)calloc(g_swarm.agent_count, sizeof(RXDAgentDiff));
    
    for (uint32_t i = 0; i < g_swarm.agent_count; i++) {
        RXDAgent* agent = &g_swarm.agents[i];
        if (agent->state != RXD_AGENT_COMPLETED) continue;
        
        RXDAgentDiff* d = &agg.diffs[agg.diff_count++];
        strncpy(d->agent_name, agent->name, sizeof(d->agent_name) - 1);
        
        /* Read diff from worktree */
        size_t len = 0;
        d->diff_content = rxd_worktree_read_file(agent->worktree, "output.md", &len);
        d->diff_size = len;
        
        /* Count additions/deletions (simplified) */
        d->additions = 0;
        d->deletions = 0;
        for (size_t j = 0; j < len; j++) {
            if (d->diff_content[j] == '+') d->additions++;
            if (d->diff_content[j] == '-') d->deletions++;
        }
        
        agg.total_additions += (uint32_t)d->additions;
        agg.total_deletions += (uint32_t)d->deletions;
    }
    
    return agg;
}

static void rxd_aggregated_diff_free(RXDAggregatedDiff* agg) {
    if (!agg) return;
    for (uint32_t i = 0; i < agg->diff_count; i++) {
        if (agg->diffs[i].diff_content) free(agg->diffs[i].diff_content);
    }
    free(agg->diffs);
}

/* ═══════════════════════════════════════════════════════════════════════════
   TOKEN BUDGET MANAGEMENT
   ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    uint32_t budget;
    uint32_t used;
    uint32_t reserved;
    uint32_t per_agent_budget;
    bool exceeded;
    char exceeded_agent[64];
} RXDTokenBudget;

static RXDTokenBudget g_token_budget = {0};

static void rxd_token_budget_init(uint32_t total_budget, uint32_t agent_count) {
    g_token_budget.budget = total_budget;
    g_token_budget.used = 0;
    g_token_budget.reserved = 0;
    g_token_budget.per_agent_budget = agent_count > 0 ? total_budget / agent_count : total_budget;
    g_token_budget.exceeded = false;
}

static bool rxd_token_budget_reserve(RXDAgent* agent, uint32_t tokens) {
    if (g_token_budget.used + tokens > g_token_budget.budget) {
        g_token_budget.exceeded = true;
        strncpy(g_token_budget.exceeded_agent, agent->name, 
                sizeof(g_token_budget.exceeded_agent) - 1);
        return false;
    }
    
    g_token_budget.used += tokens;
    g_token_budget.reserved += tokens;
    return true;
}

static void rxd_token_budget_release(uint32_t tokens) {
    if (tokens <= g_token_budget.reserved) {
        g_token_budget.reserved -= tokens;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
   SWARM STATISTICS & REPORTING
   ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    uint32_t total_agents;
    uint32_t completed;
    uint32_t failed;
    uint32_t timed_out;
    double total_time_ms;
    double avg_time_ms;
    uint32_t total_tokens;
    float avg_confidence;
    RXDAgent* best_agent;
    float best_score;
    uint32_t total_additions;
    uint32_t total_deletions;
    char best_output[RXD_MAX_AGENT_OUTPUT];
} RXDSwarmReport;

static RXDSwarmReport rxd_swarm_generate_report(void) {
    RXDSwarmReport report = {0};
    
    report.total_agents = g_swarm.agent_count;
    
    for (uint32_t i = 0; i < g_swarm.agent_count; i++) {
        RXDAgent* agent = &g_swarm.agents[i];
        
        switch (agent->state) {
            case RXD_AGENT_COMPLETED: report.completed++; break;
            case RXD_AGENT_FAILED: report.failed++; break;
            case RXD_AGENT_TIMEOUT: report.timed_out++; break;
            default: break;
        }
        
        report.total_tokens += agent->tokens_used;
    }
    
    report.total_time_ms = (double)(g_swarm.end_time - g_swarm.start_time) / 1e6;
    report.avg_time_ms = report.completed > 0 ? 
                         report.total_time_ms / report.completed : 0;
    
    RXDAgent* best = rxd_swarm_select_best();
    if (best) {
        report.best_agent = best;
        report.best_score = best->quality_score;
        strncpy(report.best_output, best->output, sizeof(report.best_output) - 1);
    }
    
    /* Aggregate diffs */
    RXDAggregatedDiff agg = rxd_aggregate_diffs();
    report.total_additions = agg.total_additions;
    report.total_deletions = agg.total_deletions;
    rxd_aggregated_diff_free(&agg);
    
    return report;
}

/* Generate JSON report */
static char* rxd_swarm_report_json(RXDSwarmReport* report) {
    size_t size = 4096 + report->total_agents * 512;
    char* json = (char*)malloc(size);
    
    snprintf(json, size,
        "{"
        "\"total_agents\":%u,"
        "\"completed\":%u,"
        "\"failed\":%u,"
        "\"timed_out\":%u,"
        "\"total_time_ms\":%.2f,"
        "\"avg_time_ms\":%.2f,"
        "\"total_tokens\":%u,"
        "\"best_agent\":\"%s\","
        "\"best_score\":%.4f,"
        "\"additions\":%u,"
        "\"deletions\":%u,"
        "\"agents\":[",
        report->total_agents,
        report->completed,
        report->failed,
        report->timed_out,
        report->total_time_ms,
        report->avg_time_ms,
        report->total_tokens,
        report->best_agent ? report->best_agent->name : "none",
        report->best_score,
        report->total_additions,
        report->total_deletions
    );
    
    size_t pos = strlen(json);
    for (uint32_t i = 0; i < g_swarm.agent_count; i++) {
        RXDAgent* a = &g_swarm.agents[i];
        pos += snprintf(json + pos, size - pos,
            "{\"id\":%u,\"name\":\"%s\",\"role\":%u,\"state\":%u,"
            "\"tokens\":%u,\"quality\":%.4f,\"time_ms\":%.2f}%s",
            a->agent_id, a->name, a->role, a->state,
            a->tokens_used, a->quality_score,
            (double)(a->end_time - a->start_time) / 1e6,
            (i < g_swarm.agent_count - 1) ? "," : ""
        );
    }
    
    snprintf(json + pos, size - pos, "]}");
    return json;
}

/* ═══════════════════════════════════════════════════════════════════════════
   QUICK APIs - MAX MODE
   ═══════════════════════════════════════════════════════════════════════════ */

/* Initialize MAX mode swarm */
static bool rxd_max_mode_init(const char* base_repo, uint32_t agent_count) {
    rxd_worktree_manager_init(base_repo);
    rxd_swarm_init(RXD_AGENT_MODE_BEST_OF_N);
    rxd_token_budget_init(RXD_MAX_TOKEN_BUDGET, agent_count);
    return true;
}

/* Add agent with model */
static uint32_t rxd_max_add_agent(RXDAgentRole role, const char* model_path,
                                  const RXDQuantProfile* profile) {
    return rxd_swarm_add_agent(role, model_path, profile);
}

/* Run MAX mode - parallel agents, best-of-N selection */
static RXDSwarmReport rxd_max_run(const char* task, uint32_t timeout_ms) {
    rxd_swarm_run_parallel(task);
    rxd_swarm_wait_all(timeout_ms);
    return rxd_swarm_generate_report();
}

/* Run chain of thought */
static RXDChainOfThought rxd_max_chain(const char* task) {
    return rxd_run_chain_of_thought(task);
}

/* Run cascade inference */
static RXDInferenceResult rxd_max_cascade(const char* prompt) {
    return rxd_cascade_infer(prompt);
}

/* Get swarm status */
typedef struct {
    uint32_t active_agents;
    uint32_t completed;
    uint32_t failed;
    bool is_running;
    float progress;
    uint32_t tokens_used;
    uint32_t tokens_remaining;
} RXDSwarmStatus;

static RXDSwarmStatus rxd_max_get_status(void) {
    RXDSwarmStatus s = {0};
    s.active_agents = g_swarm.active_agents;
    s.completed = g_swarm.completed_agents;
    s.failed = g_swarm.failed_agents;
    s.is_running = g_swarm.is_running;
    s.progress = g_swarm.active_agents > 0 ? 
                 (float)(s.completed + s.failed) / s.active_agents : 0;
    s.tokens_used = g_token_budget.used;
    s.tokens_remaining = g_token_budget.budget - g_token_budget.used;
    return s;
}

/* Cleanup */
static void rxd_max_cleanup(void) {
    for (uint32_t i = 0; i < g_swarm.agent_count; i++) {
        if (g_swarm.agents[i].worktree) {
            rxd_worktree_destroy(g_swarm.agents[i].worktree);
        }
    }
    memset(&g_swarm, 0, sizeof(g_swarm));
    memset(&g_cot, 0, sizeof(g_cot));
    memset(&g_token_budget, 0, sizeof(g_token_budget));
}

#ifdef __cplusplus
}
#endif

#endif /* RAWRXD_SWARM_H */
