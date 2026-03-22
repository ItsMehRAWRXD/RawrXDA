// ============================================================================
// Win32IDE_AgentEnhancements.h
// ============================================================================
// Declares the 7 autonomous-agent enhancements added to Win32IDE:
//
//   1. ContextBudget      — token-window tracking + auto-truncation
//   2. ToolValidation     — schema validation before dispatch
//   3. PlanDAG            — dependency graph + parallel batch execution
//   4. Scratchpad         — persistent working memory across plan steps
//   5. StreamingOutput    — live token streaming into plan dialog
//   6. TokenBudget        — per-task token budget enforcement
//   7. ModelRouter        — multi-model routing with fallback
//
// All structs are POD-compatible (no virtual, no exceptions).
// All methods are added to Win32IDE via the existing member-function pattern.
// ============================================================================

#pragma once
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <atomic>
#include <chrono>
#include <cstdint>

// ============================================================================
// Enhancement 1 — Context budget
// ============================================================================
struct ContextBudgetState {
    int  windowSize   = 4096;
    int  usedTokens   = 0;
    bool warnFired    = false;
    int  truncations  = 0;
};

// ============================================================================
// Enhancement 2 — Tool validation
// ============================================================================
struct ToolValidationResult {
    bool                     valid       = true;
    std::string              toolName;
    std::string              errorMessage;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
};

// ============================================================================
// Enhancement 3 — Plan DAG
// (PlanStep gains a `dependsOn` field — added via Win32IDE_Types.h patch)
// ============================================================================

// ============================================================================
// Enhancement 4 — Scratchpad
// ============================================================================
struct ScratchpadEntry {
    std::string value;
    std::string stepContext;
    uint64_t    timestampMs = 0;
};

// ============================================================================
// Enhancement 5 — Streaming output
// (Uses WM_PLAN_STREAM_TOKEN posted per token)
// ============================================================================

// ============================================================================
// Enhancement 6 — Token budget
// ============================================================================
struct PlanTokenBudgetState {
    int  totalBudget = 8192;
    int  usedTokens  = 0;
    int  overruns    = 0;
    bool active      = false;
};

// ============================================================================
// Enhancement 7 — Model router
// ============================================================================
struct AgentModelRoute {
    std::string stepType;
    std::string selectedTier;   // "local_gguf" | "ollama" | "cloud_stub"
    std::string modelName;
    bool        fallbackUsed = false;
};

// ============================================================================
// Win32IDE method declarations (added via Win32IDE.h include guard extension)
// ============================================================================
// These are declared here and defined in Win32IDE_AgentEnhancements.cpp.
// Win32IDE.h includes this header at the bottom of the class declaration.
//
// Enhancement 1
//   void initContextBudget(int contextWindowTokens);
//   std::string applyContextBudget(const std::string& prompt, const std::string& history);
//   std::string getContextBudgetStatus() const;
//   ContextBudgetState m_contextBudget;
//
// Enhancement 2
//   ToolValidationResult validateToolCall(const std::string& toolName, const std::string& argsJson);
//   bool validateAndDispatchToolCall(const std::string& toolName, const std::string& argsJson, std::string& toolResult);
//
// Enhancement 3
//   std::vector<std::vector<int>> buildPlanExecutionBatches();
//   void executePlanBatch(const std::vector<int>& batch, bool dryRun);
//
// Enhancement 4
//   void scratchpadWrite(const std::string& key, const std::string& value, const std::string& stepContext);
//   std::string scratchpadRead(const std::string& key) const;
//   bool scratchpadHas(const std::string& key) const;
//   void scratchpadClear();
//   std::string scratchpadToJSON() const;
//   void persistScratchpad();
//   std::map<std::string, ScratchpadEntry> m_scratchpad;
//   mutable std::mutex m_scratchpadMutex;
//
// Enhancement 5
//   void onPlanStreamToken(int stepIndex, const char* token);
//   void resetPlanStreamCounters();
//   int m_planStreamTokenCount = 0;
//   std::chrono::steady_clock::time_point m_planStreamStart;
//
// Enhancement 6
//   void initPlanTokenBudget(int totalTokens);
//   bool checkStepTokenBudget(int stepIndex, const std::string& output);
//   std::string getPlanTokenBudgetStatus() const;
//   PlanTokenBudgetState m_planTokenBudget;
//
// Enhancement 7
//   AgentModelRoute routeStepToModel(const PlanStep& step);
//   std::string getModelRouterStatus() const;
//
// Unified init
//   void initAllAgentEnhancements(int contextWindow, int tokenBudget);
