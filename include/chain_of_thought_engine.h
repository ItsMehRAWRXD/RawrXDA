#pragma once
// ============================================================================
// chain_of_thought_engine.h — Chain-of-Thought Multi-Model Review Engine
// Phase 32A: Backend implementation of the CoT system
//
// Provides a sequential multi-step reasoning chain where 1–8 models/roles
// review a user query. Each step receives the original query plus all prior
// step outputs, enabling cumulative reasoning.
//
// Roles: reviewer, auditor, thinker, researcher, debater_for, debater_against,
//        critic, synthesizer, brainstorm, verifier, refiner, summarizer
//
// Presets: review, audit, think, research, debate, custom
//
// NO exceptions. Returns structured results. Thread-safe.
// ============================================================================

#ifndef RAWRXD_CHAIN_OF_THOUGHT_ENGINE_H
#define RAWRXD_CHAIN_OF_THOUGHT_ENGINE_H

#include <string>
#include <vector>
#include <functional>
#include <atomic>
#include <mutex>
#include <map>

// ============================================================================
// CoT Role — one of 12 predefined reasoning personas
// ============================================================================
enum class CoTRoleId {
    Reviewer = 0,
    Auditor,
    Thinker,
    Researcher,
    DebaterFor,
    DebaterAgainst,
    Critic,
    Synthesizer,
    Brainstorm,
    Verifier,
    Refiner,
    Summarizer,
    COUNT
};

struct CoTRoleInfo {
    CoTRoleId   id;
    const char* name;        // e.g. "reviewer"
    const char* label;       // e.g. "Reviewer"
    const char* icon;        // e.g. "[R]"
    const char* instruction; // system prompt for this role
};

// Get info for a specific role
const CoTRoleInfo& getCoTRoleInfo(CoTRoleId id);

// Get info by string name (e.g. "reviewer"), returns nullptr if not found
const CoTRoleInfo* getCoTRoleByName(const std::string& name);

// Get all roles
const std::vector<CoTRoleInfo>& getAllCoTRoles();

// ============================================================================
// CoT Step — one step in the chain
// ============================================================================
struct CoTStep {
    CoTRoleId   role         = CoTRoleId::Thinker;
    std::string model;       // model override (empty = use default)
    std::string instruction; // custom instruction override (empty = use role default)
    bool        skip         = false;
};

// ============================================================================
// CoT Preset — a named preset configuration
// ============================================================================
struct CoTPreset {
    const char*          name;   // e.g. "review"
    const char*          label;  // e.g. "Review"
    std::vector<CoTStep> steps;
};

// Get a preset by name. Returns nullptr if not found.
const CoTPreset* getCoTPreset(const std::string& name);

// Get all preset names
std::vector<std::string> getCoTPresetNames();

// ============================================================================
// CoT Step Result — result of executing one step
// ============================================================================
struct CoTStepResult {
    int         stepIndex    = 0;
    CoTRoleId   role         = CoTRoleId::Thinker;
    std::string roleName;
    std::string model;       // model actually used
    std::string instruction; // instruction actually sent
    std::string output;      // LLM response text
    bool        success      = false;
    bool        skipped      = false;
    std::string error;       // error message if failed
    int         latencyMs    = 0;
    int         tokenCount   = 0;   // approximate
};

// ============================================================================
// CoT Chain Result — full chain execution result
// ============================================================================
struct CoTChainResult {
    bool                       success       = false;
    std::string                userQuery;    // original user query
    std::vector<CoTStepResult> stepResults;
    std::string                finalOutput;  // last successful step output
    int                        totalLatencyMs = 0;
    int                        stepsCompleted = 0;
    int                        stepsSkipped   = 0;
    int                        stepsFailed    = 0;
    std::string                error;
};

// ============================================================================
// Inference callback — the engine calls this to get LLM completions
// Signature: response = fn(systemPrompt, userMessage, model)
// ============================================================================
using CoTInferenceCallback = std::function<std::string(
    const std::string& systemPrompt,
    const std::string& userMessage,
    const std::string& model
)>;

// ============================================================================
// Step progress callback — called after each step completes
// ============================================================================
using CoTStepCallback = std::function<void(const CoTStepResult& result)>;

// ============================================================================
// ChainOfThoughtEngine — the core engine
// ============================================================================
class ChainOfThoughtEngine {
public:
    ChainOfThoughtEngine();
    ~ChainOfThoughtEngine();

    // ---- Configuration ----
    void setInferenceCallback(CoTInferenceCallback cb);
    void setStepCallback(CoTStepCallback cb);
    void setDefaultModel(const std::string& model);
    void setMaxSteps(int max);  // 1–8
    int  getMaxSteps() const { return m_maxSteps; }

    // ---- Chain building ----
    void clearSteps();
    void addStep(const CoTStep& step);
    void addStep(CoTRoleId role, const std::string& model = "",
                 const std::string& instruction = "");
    void setSteps(const std::vector<CoTStep>& steps);
    const std::vector<CoTStep>& getSteps() const { return m_steps; }

    // Apply a preset (replaces current steps)
    bool applyPreset(const std::string& presetName);

    // ---- Execution ----
    // Execute the chain synchronously (blocks until complete or cancelled)
    CoTChainResult executeChain(const std::string& userQuery);

    // Cancel a running chain
    void cancel();
    bool isRunning() const { return m_running.load(); }

    // ---- Statistics ----
    struct CoTStats {
        int totalChains         = 0;
        int successfulChains    = 0;
        int failedChains        = 0;
        int totalStepsExecuted  = 0;
        int totalStepsSkipped   = 0;
        int totalStepsFailed    = 0;
        int avgLatencyMs        = 0;
        std::map<CoTRoleId, int> roleUsage;
    };
    CoTStats getStats() const;
    void resetStats();

    // ---- Serialization (JSON-like string) ----
    std::string getStatusJSON() const;
    std::string getStepsJSON() const;
    std::string getPresetsJSON() const;

    // ---- Singleton ----
    static ChainOfThoughtEngine& instance();

private:
    std::string buildSystemPrompt(const CoTStep& step) const;
    std::string buildUserMessage(const std::string& userQuery,
                                 const std::vector<CoTStepResult>& priorResults) const;

    CoTInferenceCallback          m_inferenceCallback;
    CoTStepCallback               m_stepCallback;
    std::string                   m_defaultModel;
    int                           m_maxSteps = 8;
    std::vector<CoTStep>          m_steps;
    std::atomic<bool>             m_running{false};
    std::atomic<bool>             m_cancelled{false};
    mutable std::mutex            m_mutex;

    // Stats
    mutable std::mutex            m_statsMutex;
    CoTStats                      m_stats;
};

#endif // RAWRXD_CHAIN_OF_THOUGHT_ENGINE_H
