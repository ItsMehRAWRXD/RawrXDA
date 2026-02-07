// ============================================================================
// Win32IDE_LLMRouter.cpp — Phase 8C: LLM Router
// ============================================================================
// Task-based routing layer that sits ABOVE the Phase 8B Backend Switcher.
//
// Responsibilities:
//   1. Classify prompts by intent  (chat / code-gen / planning / tool / ...)
//   2. Select the optimal backend  (capability + failure + policy aware)
//   3. Execute with auditable fallback  (explicit, never silent)
//   4. Record every routing decision for explainability
//
// Guardrails:
//   - Does NOT replace routeInferenceRequest() — wraps it
//   - Does NOT mutate backend configs — reads them
//   - Does NOT auto-enable/disable backends — advisory only
//   - Fallback is always logged, never silent
//   - When disabled, all calls pass straight through to routeInferenceRequest()
// ============================================================================

#include "Win32IDE.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <chrono>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <iomanip>
#include <climits>

// nlohmann/json already included via Win32IDE.h

// ============================================================================
// INITIALIZATION & LIFECYCLE
// ============================================================================

void Win32IDE::initLLMRouter() {
    if (m_routerInitialized) return;

    logFunction("initLLMRouter");

    // ---- Initialize capability profiles for each backend -------------------
    initDefaultCapabilities();

    // ---- Initialize task routing preferences (sensible defaults) -----------
    //
    // Default: everything routes to the active backend (router off = passthrough).
    // When the router is enabled, these preferences guide selection.
    for (size_t i = 0; i < (size_t)LLMTaskType::Count; ++i) {
        auto& pref     = m_taskPreferences[i];
        pref.taskType  = (LLMTaskType)i;
        pref.preferredBackend  = AIBackendType::LocalGGUF;
        pref.fallbackBackend   = AIBackendType::Count;   // No fallback by default
        pref.allowFallback     = true;
        pref.maxFailuresBeforeSkip = 5;
    }

    // ---- Sensible overrides for specific task types ------------------------
    //
    // Code generation → prefer Claude/OpenAI if available (higher quality)
    m_taskPreferences[(size_t)LLMTaskType::CodeGeneration].preferredBackend  = AIBackendType::Claude;
    m_taskPreferences[(size_t)LLMTaskType::CodeGeneration].fallbackBackend   = AIBackendType::OpenAI;

    // Code review → prefer Claude (strong reasoning)
    m_taskPreferences[(size_t)LLMTaskType::CodeReview].preferredBackend  = AIBackendType::Claude;
    m_taskPreferences[(size_t)LLMTaskType::CodeReview].fallbackBackend   = AIBackendType::OpenAI;

    // Planning → prefer OpenAI (good structured output)
    m_taskPreferences[(size_t)LLMTaskType::Planning].preferredBackend  = AIBackendType::OpenAI;
    m_taskPreferences[(size_t)LLMTaskType::Planning].fallbackBackend   = AIBackendType::Claude;

    // Tool execution → prefer OpenAI (native function calling)
    m_taskPreferences[(size_t)LLMTaskType::ToolExecution].preferredBackend  = AIBackendType::OpenAI;
    m_taskPreferences[(size_t)LLMTaskType::ToolExecution].fallbackBackend   = AIBackendType::Gemini;

    // Chat → prefer LocalGGUF (low latency, free)
    m_taskPreferences[(size_t)LLMTaskType::Chat].preferredBackend  = AIBackendType::LocalGGUF;
    m_taskPreferences[(size_t)LLMTaskType::Chat].fallbackBackend   = AIBackendType::Ollama;

    // Research → prefer Gemini (large context window)
    m_taskPreferences[(size_t)LLMTaskType::Research].preferredBackend  = AIBackendType::Gemini;
    m_taskPreferences[(size_t)LLMTaskType::Research].fallbackBackend   = AIBackendType::Claude;

    // ---- Reset stats -------------------------------------------------------
    m_routerStats = {};
    m_consecutiveFailures = {};
    m_lastRoutingDecision = {};

    // ---- Load saved config (overrides defaults) ----------------------------
    loadRouterConfig();

    m_routerInitialized = true;

    logInfo("[LLMRouter] Initialized — router " + std::string(m_routerEnabled ? "ENABLED" : "DISABLED"));
}

void Win32IDE::shutdownLLMRouter() {
    if (!m_routerInitialized) return;
    logFunction("shutdownLLMRouter");
    saveRouterConfig();
    m_routerInitialized = false;
}

// ============================================================================
// CAPABILITY PROFILES
// ============================================================================

void Win32IDE::initDefaultCapabilities() {
    // LocalGGUF — free, local, limited context
    auto& local      = m_backendCapabilities[(size_t)AIBackendType::LocalGGUF];
    local.backend              = AIBackendType::LocalGGUF;
    local.maxContextTokens     = 4096;
    local.supportsToolCalls    = false;
    local.supportsStreaming    = true;
    local.supportsFunctionCalling = false;
    local.supportsJsonMode     = false;
    local.costTier             = 0;   // Free
    local.qualityScore         = 0.4f;
    local.notes                = "Native CPU inference, zero latency to network";

    // Ollama — free (local), moderate context
    auto& ollama     = m_backendCapabilities[(size_t)AIBackendType::Ollama];
    ollama.backend             = AIBackendType::Ollama;
    ollama.maxContextTokens    = 8192;
    ollama.supportsToolCalls   = false;
    ollama.supportsStreaming   = true;
    ollama.supportsFunctionCalling = false;
    ollama.supportsJsonMode    = true;
    ollama.costTier            = 0;   // Free (local)
    ollama.qualityScore        = 0.5f;
    ollama.notes               = "Local Ollama server, GPU-accelerated";

    // OpenAI — paid, large context, function calling
    auto& openai     = m_backendCapabilities[(size_t)AIBackendType::OpenAI];
    openai.backend             = AIBackendType::OpenAI;
    openai.maxContextTokens    = 128000;
    openai.supportsToolCalls   = true;
    openai.supportsStreaming   = true;
    openai.supportsFunctionCalling = true;
    openai.supportsJsonMode    = true;
    openai.costTier            = 2;   // Moderate
    openai.qualityScore        = 0.8f;
    openai.notes               = "GPT-4o: strong at structured output and tool use";

    // Claude — paid, very large context, strong reasoning
    auto& claude     = m_backendCapabilities[(size_t)AIBackendType::Claude];
    claude.backend             = AIBackendType::Claude;
    claude.maxContextTokens    = 200000;
    claude.supportsToolCalls   = true;
    claude.supportsStreaming   = true;
    claude.supportsFunctionCalling = true;
    claude.supportsJsonMode    = false;   // Claude uses XML-style structured output
    claude.costTier            = 3;       // Expensive
    claude.qualityScore        = 0.9f;
    claude.notes               = "Claude: best-in-class code reasoning";

    // Gemini — paid, massive context
    auto& gemini     = m_backendCapabilities[(size_t)AIBackendType::Gemini];
    gemini.backend             = AIBackendType::Gemini;
    gemini.maxContextTokens    = 1000000;
    gemini.supportsToolCalls   = true;
    gemini.supportsStreaming   = true;
    gemini.supportsFunctionCalling = true;
    gemini.supportsJsonMode    = true;
    gemini.costTier            = 1;       // Cheap (flash)
    gemini.qualityScore        = 0.7f;
    gemini.notes               = "Gemini Flash: huge context window, fast, cost-effective";
}

Win32IDE::BackendCapability Win32IDE::getBackendCapability(AIBackendType type) const {
    if (type >= AIBackendType::Count) return BackendCapability{};
    return m_backendCapabilities[(size_t)type];
}

void Win32IDE::setBackendCapability(AIBackendType type, const BackendCapability& cap) {
    if (type >= AIBackendType::Count) return;
    std::lock_guard<std::mutex> lock(m_routerMutex);
    m_backendCapabilities[(size_t)type] = cap;
}

// ============================================================================
// TASK CLASSIFICATION
// ============================================================================

Win32IDE::LLMTaskType Win32IDE::classifyTask(const std::string& prompt) const {
    if (prompt.empty()) return LLMTaskType::General;

    // Convert to lowercase for pattern matching
    std::string lower = prompt;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return (char)std::tolower(c); });

    // ---- Code Generation signals -------------------------------------------
    // "write", "create", "implement", "generate", "build", "make a function"
    {
        static const char* codeGenPatterns[] = {
            "write a ", "write me ", "create a function", "create a class",
            "implement ", "generate code", "generate a ", "build a ",
            "make a function", "make a class", "code that ", "code to ",
            "write code", "write the ", "scaffold ", "boilerplate ",
            "starter code", "template for", "skeleton for",
            nullptr
        };
        for (int i = 0; codeGenPatterns[i]; ++i) {
            if (lower.find(codeGenPatterns[i]) != std::string::npos)
                return LLMTaskType::CodeGeneration;
        }
    }

    // ---- Code Review signals -----------------------------------------------
    {
        static const char* codeReviewPatterns[] = {
            "review this", "review the code", "code review",
            "what's wrong with", "find bugs", "find the bug",
            "spot the error", "analyze this code", "critique ",
            "is this code correct", "any issues with",
            "security review", "vulnerability", "audit this",
            nullptr
        };
        for (int i = 0; codeReviewPatterns[i]; ++i) {
            if (lower.find(codeReviewPatterns[i]) != std::string::npos)
                return LLMTaskType::CodeReview;
        }
    }

    // ---- Code Edit signals -------------------------------------------------
    {
        static const char* codeEditPatterns[] = {
            "refactor ", "fix this", "fix the ", "modify ",
            "change this", "update the ", "edit this", "rewrite ",
            "optimize this code", "improve this code",
            "add error handling", "add logging", "add tests",
            "convert from ", "migrate ", "port this",
            nullptr
        };
        for (int i = 0; codeEditPatterns[i]; ++i) {
            if (lower.find(codeEditPatterns[i]) != std::string::npos)
                return LLMTaskType::CodeEdit;
        }
    }

    // ---- Planning signals --------------------------------------------------
    {
        static const char* planningPatterns[] = {
            "plan ", "step by step", "steps to ",
            "how should i ", "strategy for", "roadmap ",
            "design a system", "architecture for",
            "break down ", "decompose ", "outline ",
            "what order should", "sequence of ",
            nullptr
        };
        for (int i = 0; planningPatterns[i]; ++i) {
            if (lower.find(planningPatterns[i]) != std::string::npos)
                return LLMTaskType::Planning;
        }
    }

    // ---- Tool Execution signals --------------------------------------------
    {
        static const char* toolPatterns[] = {
            "call ", "execute ", "run the tool",
            "invoke ", "use the function", "api call",
            "fetch ", "query the ", "search for ",
            "tool:", "function_call", "tool_use",
            nullptr
        };
        for (int i = 0; toolPatterns[i]; ++i) {
            if (lower.find(toolPatterns[i]) != std::string::npos)
                return LLMTaskType::ToolExecution;
        }
    }

    // ---- Research signals --------------------------------------------------
    {
        static const char* researchPatterns[] = {
            "summarize ", "explain ", "what is ",
            "how does ", "why does ", "tell me about ",
            "describe ", "compare ", "difference between",
            "pros and cons", "advantages of",
            "documentation for", "lookup ", "research ",
            nullptr
        };
        for (int i = 0; researchPatterns[i]; ++i) {
            if (lower.find(researchPatterns[i]) != std::string::npos)
                return LLMTaskType::Research;
        }
    }

    // ---- Chat signals (catch-all conversational) ---------------------------
    {
        static const char* chatPatterns[] = {
            "hello", "hi ", "hey ", "thanks",
            "how are", "good morning", "can you help",
            "i need help", "please ",
            nullptr
        };
        for (int i = 0; chatPatterns[i]; ++i) {
            if (lower.find(chatPatterns[i]) != std::string::npos)
                return LLMTaskType::Chat;
        }
    }

    return LLMTaskType::General;
}

std::string Win32IDE::taskTypeString(LLMTaskType type) const {
    switch (type) {
        case LLMTaskType::Chat:           return "Chat";
        case LLMTaskType::CodeGeneration: return "CodeGeneration";
        case LLMTaskType::CodeReview:     return "CodeReview";
        case LLMTaskType::CodeEdit:       return "CodeEdit";
        case LLMTaskType::Planning:       return "Planning";
        case LLMTaskType::ToolExecution:  return "ToolExecution";
        case LLMTaskType::Research:       return "Research";
        case LLMTaskType::General:        return "General";
        default:                          return "Unknown";
    }
}

Win32IDE::LLMTaskType Win32IDE::taskTypeFromString(const std::string& name) const {
    if (name == "Chat")           return LLMTaskType::Chat;
    if (name == "CodeGeneration") return LLMTaskType::CodeGeneration;
    if (name == "CodeReview")     return LLMTaskType::CodeReview;
    if (name == "CodeEdit")       return LLMTaskType::CodeEdit;
    if (name == "Planning")       return LLMTaskType::Planning;
    if (name == "ToolExecution")  return LLMTaskType::ToolExecution;
    if (name == "Research")       return LLMTaskType::Research;
    if (name == "General")        return LLMTaskType::General;
    return LLMTaskType::General;
}

// ============================================================================
// CORE ROUTING — the heart of Phase 8C
// ============================================================================

Win32IDE::RoutingDecision Win32IDE::selectBackendForTask(LLMTaskType task,
                                                          const std::string& prompt) {
    RoutingDecision decision;
    decision.classifiedTask  = task;
    decision.decisionEpochMs = currentEpochMs();

    std::lock_guard<std::mutex> lock(m_routerMutex);

    // 0. Check if this task is pinned by the user (UX Enhancement)
    const auto& pin = m_taskPins[(size_t)task];
    if (pin.active && pin.pinnedBackend != AIBackendType::Count) {
        decision.selectedBackend = pin.pinnedBackend;
        decision.fallbackBackend = AIBackendType::Count;  // Pinned = no fallback
        decision.policyOverride  = true;
        decision.confidence      = 1.0f;
        decision.reason          = "PINNED by user: task '" + taskTypeString(task) +
                                   "' → backend '" + backendTypeString(pin.pinnedBackend) + "'";
        if (!pin.reason.empty()) {
            decision.reason += " (" + pin.reason + ")";
        }
        return decision;
    }

    // 1. Start with the task preference
    const auto& pref = m_taskPreferences[(size_t)task];
    AIBackendType preferred = pref.preferredBackend;
    AIBackendType fallback  = pref.fallbackBackend;

    // 2. Check if preferred backend is actually viable
    bool preferredViable = isBackendViableForTask(preferred, task);

    // 3. Failure-adjusted: if preferred has too many consecutive failures, demote it
    if (preferredViable) {
        preferred = getFailureAdjustedBackend(preferred, task);
        if (preferred != pref.preferredBackend) {
            decision.policyOverride = true;
            decision.reason = "Preferred backend '" + backendTypeString(pref.preferredBackend) +
                              "' demoted due to consecutive failures; promoted '" +
                              backendTypeString(preferred) + "'";
        }
    }

    // 4. If preferred is not viable, try fallback
    if (!preferredViable) {
        if (fallback != AIBackendType::Count && isBackendViableForTask(fallback, task)) {
            preferred = fallback;
            fallback  = AIBackendType::Count;
            decision.policyOverride = true;
            decision.reason = "Preferred backend '" + backendTypeString(pref.preferredBackend) +
                              "' not viable; routing to fallback '" +
                              backendTypeString(preferred) + "'";
        } else {
            // Last resort: use whatever the active backend is
            preferred = m_activeBackend;
            fallback  = AIBackendType::Count;
            decision.reason = "No preferred/fallback backend viable for task '" +
                              taskTypeString(task) + "'; using active backend '" +
                              backendTypeString(m_activeBackend) + "'";
        }
    }

    // 5. If we still haven't written a reason, write the normal one
    if (decision.reason.empty()) {
        decision.reason = "Task '" + taskTypeString(task) + "' → preferred backend '" +
                          backendTypeString(preferred) + "'";
        if (fallback != AIBackendType::Count) {
            decision.reason += " (fallback: '" + backendTypeString(fallback) + "')";
        }
    }

    // 6. Compute confidence based on how well the backend matches the task
    {
        const auto& cap = m_backendCapabilities[(size_t)preferred];
        float conf = 0.5f; // Base confidence
        // Bonus for capability alignment
        if (task == LLMTaskType::ToolExecution && cap.supportsFunctionCalling) conf += 0.2f;
        if (task == LLMTaskType::Research && cap.maxContextTokens >= 100000)   conf += 0.2f;
        if (task == LLMTaskType::Chat && cap.costTier == 0)                    conf += 0.1f;
        // Quality score contribution
        conf += cap.qualityScore * 0.2f;
        // Clamp
        if (conf > 1.0f) conf = 1.0f;
        decision.confidence = conf;
    }

    decision.selectedBackend = preferred;
    decision.fallbackBackend = pref.allowFallback ? fallback : AIBackendType::Count;

    return decision;
}

std::string Win32IDE::routeWithIntelligence(const std::string& prompt) {
    // If router is disabled, pass straight through to the basic backend switcher
    if (!m_routerEnabled || !m_routerInitialized) {
        return routeInferenceRequest(prompt);
    }

    // 1. Classify the task
    LLMTaskType task = classifyTask(prompt);

    // 1b. If ensemble mode is enabled, fan out to multiple backends
    if (m_ensembleEnabled) {
        return routeWithEnsemble(prompt);
    }

    // 2. Select backend
    RoutingDecision decision = selectBackendForTask(task, prompt);

    // 3. Temporarily switch to the selected backend, execute, then restore
    AIBackendType originalActive;
    {
        std::lock_guard<std::mutex> lock(m_backendMutex);
        originalActive = m_activeBackend;
        m_activeBackend = decision.selectedBackend;
    }

    auto startTime = std::chrono::steady_clock::now();
    std::string result = routeInferenceRequest(prompt);
    auto endTime = std::chrono::steady_clock::now();
    int latencyMs = (int)std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

    decision.primaryLatencyMs = latencyMs;

    bool primaryFailed = result.empty() ||
                         result.find("[BackendSwitcher] Error") != std::string::npos;

    // 4. If primary failed and fallback is available, try fallback
    if (primaryFailed && decision.fallbackBackend != AIBackendType::Count) {
        // Record failure on primary
        {
            std::lock_guard<std::mutex> lock(m_routerMutex);
            m_consecutiveFailures[(size_t)decision.selectedBackend]++;
        }

        logWarning("routeWithIntelligence",
                   "Primary backend '" + backendTypeString(decision.selectedBackend) +
                   "' failed for task '" + taskTypeString(task) + "' — trying fallback '" +
                   backendTypeString(decision.fallbackBackend) + "'");

        result = handleRoutingFallback(decision.selectedBackend,
                                        decision.fallbackBackend,
                                        prompt, decision);
    } else if (!primaryFailed) {
        // Reset consecutive failure counter on success
        std::lock_guard<std::mutex> lock(m_routerMutex);
        m_consecutiveFailures[(size_t)decision.selectedBackend] = 0;
    }

    // 5. Restore original active backend
    {
        std::lock_guard<std::mutex> lock(m_backendMutex);
        m_activeBackend = originalActive;
    }

    // 6. Update router stats
    {
        std::lock_guard<std::mutex> lock(m_routerMutex);
        m_routerStats.totalRouted++;
        m_routerStats.taskTypeCounts[(size_t)task]++;
        m_routerStats.backendSelections[(size_t)decision.selectedBackend]++;
        if (decision.fallbackUsed) {
            m_routerStats.totalFallbacksUsed++;
            m_routerStats.backendFallbacks[(size_t)decision.fallbackBackend]++;
        }
        if (decision.policyOverride) {
            m_routerStats.totalPolicyOverrides++;
        }
        m_lastRoutingDecision = decision;
    }

    // 6b. Record cost/latency for heatmap (UX Enhancement)
    recordCostLatency(decision);

    // 7. Log the routing decision for explainability
    logInfo("[LLMRouter] " + getRoutingDecisionExplanation(decision));

    return result;
}

std::string Win32IDE::handleRoutingFallback(AIBackendType primary, AIBackendType fallback,
                                              const std::string& prompt,
                                              RoutingDecision& decision) {
    decision.fallbackUsed = true;

    // Switch to fallback backend
    {
        std::lock_guard<std::mutex> lock(m_backendMutex);
        m_activeBackend = fallback;
    }

    auto startTime = std::chrono::steady_clock::now();
    std::string result = routeInferenceRequest(prompt);
    auto endTime = std::chrono::steady_clock::now();
    int latencyMs = (int)std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

    decision.fallbackLatencyMs = latencyMs;

    bool fallbackFailed = result.empty() ||
                          result.find("[BackendSwitcher] Error") != std::string::npos;

    if (fallbackFailed) {
        // Both primary and fallback failed
        std::lock_guard<std::mutex> lock(m_routerMutex);
        m_consecutiveFailures[(size_t)fallback]++;
        decision.reason += " | Fallback '" + backendTypeString(fallback) + "' also failed";

        appendToOutput("[LLMRouter] BOTH primary ('" + backendTypeString(primary) +
                       "') and fallback ('" + backendTypeString(fallback) +
                       "') failed. Response may be an error.",
                       "General", OutputSeverity::Warning);
    } else {
        // Fallback succeeded — reset its counter
        std::lock_guard<std::mutex> lock(m_routerMutex);
        m_consecutiveFailures[(size_t)fallback] = 0;

        appendToOutput("[LLMRouter] Primary '" + backendTypeString(primary) +
                       "' failed → fallback '" + backendTypeString(fallback) +
                       "' succeeded (" + std::to_string(latencyMs) + "ms).",
                       "General", OutputSeverity::Info);
    }

    return result;
}

// ============================================================================
// FAILURE-INFORMED ROUTING
// ============================================================================

Win32IDE::AIBackendType Win32IDE::getFailureAdjustedBackend(AIBackendType preferred,
                                                              LLMTaskType task) const {
    // If the preferred backend has too many consecutive failures, try to find
    // a better one from the same task preference's fallback chain.
    const auto& pref = m_taskPreferences[(size_t)task];
    int failures = m_consecutiveFailures[(size_t)preferred];

    if (failures >= pref.maxFailuresBeforeSkip) {
        // Preferred has been failing too much — try the fallback
        if (pref.fallbackBackend != AIBackendType::Count) {
            int fallbackFailures = m_consecutiveFailures[(size_t)pref.fallbackBackend];
            if (fallbackFailures < pref.maxFailuresBeforeSkip) {
                return pref.fallbackBackend;
            }
        }
        // If fallback is also failing, fall back to the globally active backend
        return m_activeBackend;
    }

    return preferred;
}

bool Win32IDE::isBackendViableForTask(AIBackendType type, LLMTaskType task) const {
    if (type >= AIBackendType::Count) return false;

    // Check if the backend is enabled
    const auto& cfg = m_backendConfigs[(size_t)type];
    if (!cfg.enabled) return false;

    // Check capability alignment for strict requirements
    const auto& cap = m_backendCapabilities[(size_t)type];

    // Tool execution requires function calling support
    if (task == LLMTaskType::ToolExecution && !cap.supportsFunctionCalling) {
        // Not a hard block — local models can still try — but note it
        // We return true but the confidence will be lower
    }

    return true;
}

int Win32IDE::getConsecutiveFailures(AIBackendType type) const {
    if (type >= AIBackendType::Count) return 0;
    return m_consecutiveFailures[(size_t)type];
}

// ============================================================================
// STATUS & EXPLAINABILITY
// ============================================================================

std::string Win32IDE::getRouterStatusString() const {
    std::ostringstream ss;
    ss << "[LLMRouter] Status:\n";
    ss << "  Enabled:     " << (m_routerEnabled ? "YES" : "NO") << "\n";
    ss << "  Initialized: " << (m_routerInitialized ? "YES" : "NO") << "\n";
    ss << "  ──────────────────────────────────────\n";

    ss << "  Task Preferences:\n";
    for (size_t i = 0; i < (size_t)LLMTaskType::Count; ++i) {
        const auto& pref = m_taskPreferences[i];
        ss << "    " << taskTypeString((LLMTaskType)i) << " → "
           << backendTypeString(pref.preferredBackend);
        if (pref.fallbackBackend != AIBackendType::Count) {
            ss << " (fallback: " << backendTypeString(pref.fallbackBackend) << ")";
        }
        ss << "\n";
    }

    ss << "  ──────────────────────────────────────\n";
    ss << "  Consecutive Failures:\n";
    for (size_t i = 0; i < (size_t)AIBackendType::Count; ++i) {
        if (m_consecutiveFailures[i] > 0) {
            ss << "    " << backendTypeString((AIBackendType)i) << ": "
               << m_consecutiveFailures[i] << "\n";
        }
    }

    return ss.str();
}

std::string Win32IDE::getRoutingDecisionExplanation(const RoutingDecision& decision) const {
    std::ostringstream ss;
    ss << "Task=" << taskTypeString(decision.classifiedTask)
       << " → Backend=" << backendTypeString(decision.selectedBackend)
       << " (conf=" << (int)(decision.confidence * 100) << "%)";

    if (decision.fallbackBackend != AIBackendType::Count) {
        ss << " fallback=" << backendTypeString(decision.fallbackBackend);
    }
    if (decision.policyOverride) {
        ss << " [POLICY-OVERRIDE]";
    }
    if (decision.fallbackUsed) {
        ss << " [FALLBACK-USED]";
    }
    if (decision.primaryLatencyMs >= 0) {
        ss << " primary=" << decision.primaryLatencyMs << "ms";
    }
    if (decision.fallbackLatencyMs >= 0) {
        ss << " fallback=" << decision.fallbackLatencyMs << "ms";
    }

    if (!decision.reason.empty()) {
        ss << " | " << decision.reason;
    }

    return ss.str();
}

std::string Win32IDE::getRouterStatsString() const {
    std::ostringstream ss;
    ss << "[LLMRouter] Statistics:\n";
    ss << "  Total Routed:          " << m_routerStats.totalRouted << "\n";
    ss << "  Total Fallbacks Used:  " << m_routerStats.totalFallbacksUsed << "\n";
    ss << "  Total Policy Overrides:" << m_routerStats.totalPolicyOverrides << "\n";
    ss << "  ──────────────────────────────────────\n";

    ss << "  By Task Type:\n";
    for (size_t i = 0; i < (size_t)LLMTaskType::Count; ++i) {
        uint64_t count = m_routerStats.taskTypeCounts[i];
        if (count > 0) {
            ss << "    " << taskTypeString((LLMTaskType)i) << ": " << count << "\n";
        }
    }

    ss << "  By Backend (selections):\n";
    for (size_t i = 0; i < (size_t)AIBackendType::Count; ++i) {
        uint64_t count = m_routerStats.backendSelections[i];
        if (count > 0) {
            ss << "    " << backendTypeString((AIBackendType)i) << ": " << count << "\n";
        }
    }

    ss << "  By Backend (fallbacks):\n";
    for (size_t i = 0; i < (size_t)AIBackendType::Count; ++i) {
        uint64_t count = m_routerStats.backendFallbacks[i];
        if (count > 0) {
            ss << "    " << backendTypeString((AIBackendType)i) << ": " << count << "\n";
        }
    }

    return ss.str();
}

std::string Win32IDE::getCapabilitiesString() const {
    std::ostringstream ss;
    ss << "[LLMRouter] Backend Capabilities:\n";
    ss << "  ──────────────────────────────────────\n";

    for (size_t i = 0; i < (size_t)AIBackendType::Count; ++i) {
        const auto& cap = m_backendCapabilities[i];
        ss << "  " << backendTypeString((AIBackendType)i) << ":\n";
        ss << "    Context:     " << cap.maxContextTokens << " tokens\n";
        ss << "    ToolCalls:   " << (cap.supportsToolCalls ? "yes" : "no") << "\n";
        ss << "    Streaming:   " << (cap.supportsStreaming ? "yes" : "no") << "\n";
        ss << "    FuncCalling: " << (cap.supportsFunctionCalling ? "yes" : "no") << "\n";
        ss << "    JSON Mode:   " << (cap.supportsJsonMode ? "yes" : "no") << "\n";
        ss << "    Cost Tier:   " << cap.costTier << "\n";
        ss << "    Quality:     " << (int)(cap.qualityScore * 100) << "%\n";
        if (!cap.notes.empty()) {
            ss << "    Notes:       " << cap.notes << "\n";
        }
    }

    return ss.str();
}

std::string Win32IDE::getFallbackChainString() const {
    std::ostringstream ss;
    ss << "[LLMRouter] Fallback Chains:\n";
    ss << "  ──────────────────────────────────────\n";

    for (size_t i = 0; i < (size_t)LLMTaskType::Count; ++i) {
        const auto& pref = m_taskPreferences[i];
        ss << "  " << taskTypeString((LLMTaskType)i) << ": "
           << backendTypeString(pref.preferredBackend);

        if (pref.fallbackBackend != AIBackendType::Count) {
            ss << " → " << backendTypeString(pref.fallbackBackend);
        } else {
            ss << " → (none)";
        }

        ss << "  [" << (pref.allowFallback ? "fallback-ok" : "no-fallback")
           << ", skip-after=" << pref.maxFailuresBeforeSkip << " fails]";
        ss << "\n";
    }

    return ss.str();
}

Win32IDE::RoutingDecision Win32IDE::getLastRoutingDecision() const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(m_routerMutex));
    return m_lastRoutingDecision;
}

// ============================================================================
// CONFIGURATION
// ============================================================================

void Win32IDE::setTaskPreference(LLMTaskType task, AIBackendType preferred, AIBackendType fallback) {
    if (task >= LLMTaskType::Count) return;
    std::lock_guard<std::mutex> lock(m_routerMutex);
    auto& pref = m_taskPreferences[(size_t)task];
    pref.preferredBackend = preferred;
    pref.fallbackBackend  = fallback;
    logInfo("[LLMRouter] Task '" + taskTypeString(task) + "' → preferred='" +
            backendTypeString(preferred) + "' fallback='" + backendTypeString(fallback) + "'");
}

Win32IDE::TaskRoutingPreference Win32IDE::getTaskPreference(LLMTaskType task) const {
    if (task >= LLMTaskType::Count) return TaskRoutingPreference{};
    return m_taskPreferences[(size_t)task];
}

void Win32IDE::setRouterEnabled(bool enabled) {
    m_routerEnabled = enabled;
    logInfo("[LLMRouter] Router " + std::string(enabled ? "ENABLED" : "DISABLED"));
    appendToOutput("[LLMRouter] Router " + std::string(enabled ? "ENABLED" : "DISABLED") +
                   ". " + (enabled ? "Prompts will be task-classified and routed intelligently."
                                   : "All prompts route to the active backend (passthrough)."),
                   "General", OutputSeverity::Info);
}

bool Win32IDE::isRouterEnabled() const {
    return m_routerEnabled;
}

void Win32IDE::resetRouterStats() {
    std::lock_guard<std::mutex> lock(m_routerMutex);
    m_routerStats = {};
    m_consecutiveFailures = {};
    m_lastRoutingDecision = {};
    logInfo("[LLMRouter] Stats reset");
}

// ============================================================================
// CONFIG PERSISTENCE (JSON)
// ============================================================================

std::string Win32IDE::getRouterConfigFilePath() const {
    std::string dir = getSessionFilePath();
    size_t pos = dir.find_last_of("/\\");
    if (pos != std::string::npos) {
        dir = dir.substr(0, pos + 1) + "router.json";
    } else {
        dir = "router.json";
    }
    return dir;
}

void Win32IDE::loadRouterConfig() {
    std::string path = getRouterConfigFilePath();
    std::ifstream ifs(path);
    if (!ifs.is_open()) {
        logInfo("[LLMRouter] No saved config at " + path + " — using defaults");
        return;
    }

    try {
        std::string fileContent((std::istreambuf_iterator<char>(ifs)),
                                 std::istreambuf_iterator<char>());
        nlohmann::json j = nlohmann::json::parse(fileContent);

        if (j.contains("enabled")) {
            m_routerEnabled = j["enabled"].get<bool>();
        }

        // Load task preferences
        if (j.contains("taskPreferences") && j["taskPreferences"].is_array()) {
            for (size_t pi = 0; pi < j["taskPreferences"].size(); ++pi) {
                const auto& pj = j["taskPreferences"][pi];
                std::string taskName = pj.contains("task") ? pj["task"].get<std::string>() : "";
                LLMTaskType task = taskTypeFromString(taskName);
                if (task >= LLMTaskType::Count) continue;

                auto& pref = m_taskPreferences[(size_t)task];
                if (pj.contains("preferred")) {
                    pref.preferredBackend = backendTypeFromString(pj["preferred"].get<std::string>());
                    if (pref.preferredBackend == AIBackendType::Count)
                        pref.preferredBackend = AIBackendType::LocalGGUF;
                }
                if (pj.contains("fallback")) {
                    std::string fb = pj["fallback"].get<std::string>();
                    pref.fallbackBackend = (fb == "none" || fb.empty())
                        ? AIBackendType::Count
                        : backendTypeFromString(fb);
                }
                if (pj.contains("allowFallback")) {
                    pref.allowFallback = pj["allowFallback"].get<bool>();
                }
                if (pj.contains("maxFailuresBeforeSkip")) {
                    pref.maxFailuresBeforeSkip = pj["maxFailuresBeforeSkip"].get<int>();
                }
            }
        }

        // Load capability overrides
        if (j.contains("capabilities") && j["capabilities"].is_array()) {
            for (size_t ci = 0; ci < j["capabilities"].size(); ++ci) {
                const auto& cj = j["capabilities"][ci];
                std::string backendName = cj.contains("backend") ? cj["backend"].get<std::string>() : "";
                AIBackendType bt = backendTypeFromString(backendName);
                if (bt == AIBackendType::Count) continue;

                auto& cap = m_backendCapabilities[(size_t)bt];
                if (cj.contains("maxContextTokens"))        cap.maxContextTokens = cj["maxContextTokens"].get<int>();
                if (cj.contains("supportsToolCalls"))       cap.supportsToolCalls = cj["supportsToolCalls"].get<bool>();
                if (cj.contains("supportsStreaming"))        cap.supportsStreaming = cj["supportsStreaming"].get<bool>();
                if (cj.contains("supportsFunctionCalling")) cap.supportsFunctionCalling = cj["supportsFunctionCalling"].get<bool>();
                if (cj.contains("supportsJsonMode"))         cap.supportsJsonMode = cj["supportsJsonMode"].get<bool>();
                if (cj.contains("costTier"))                 cap.costTier = cj["costTier"].get<int>();
                if (cj.contains("qualityScore"))             cap.qualityScore = cj["qualityScore"].get<float>();
                if (cj.contains("notes"))                    cap.notes = cj["notes"].get<std::string>();
            }
        }

        logInfo("[LLMRouter] Loaded config from " + path);
    } catch (const std::exception& e) {
        logError("loadRouterConfig", std::string("JSON parse error: ") + e.what());
    }
}

void Win32IDE::saveRouterConfig() {
    std::string path = getRouterConfigFilePath();

    std::filesystem::path p(path);
    if (p.has_parent_path()) {
        std::filesystem::create_directories(p.parent_path());
    }

    nlohmann::json j;
    j["enabled"] = m_routerEnabled;

    // Save task preferences
    nlohmann::json prefArr = nlohmann::json::array();
    for (size_t i = 0; i < (size_t)LLMTaskType::Count; ++i) {
        const auto& pref = m_taskPreferences[i];
        nlohmann::json pj;
        pj["task"]      = taskTypeString((LLMTaskType)i);
        pj["preferred"] = backendTypeString(pref.preferredBackend);
        pj["fallback"]  = (pref.fallbackBackend == AIBackendType::Count)
                           ? "none" : backendTypeString(pref.fallbackBackend);
        pj["allowFallback"]         = pref.allowFallback;
        pj["maxFailuresBeforeSkip"] = pref.maxFailuresBeforeSkip;
        prefArr.push_back(pj);
    }
    j["taskPreferences"] = prefArr;

    // Save capability profiles
    nlohmann::json capArr = nlohmann::json::array();
    for (size_t i = 0; i < (size_t)AIBackendType::Count; ++i) {
        const auto& cap = m_backendCapabilities[i];
        nlohmann::json cj;
        cj["backend"]                = backendTypeString((AIBackendType)i);
        cj["maxContextTokens"]       = cap.maxContextTokens;
        cj["supportsToolCalls"]      = cap.supportsToolCalls;
        cj["supportsStreaming"]       = cap.supportsStreaming;
        cj["supportsFunctionCalling"] = cap.supportsFunctionCalling;
        cj["supportsJsonMode"]        = cap.supportsJsonMode;
        cj["costTier"]                = cap.costTier;
        cj["qualityScore"]            = cap.qualityScore;
        cj["notes"]                   = cap.notes;
        capArr.push_back(cj);
    }
    j["capabilities"] = capArr;

    std::ofstream ofs(path);
    if (ofs.is_open()) {
        ofs << j.dump(2);
        logInfo("[LLMRouter] Saved config to " + path);
    } else {
        logError("saveRouterConfig", "Failed to write " + path);
    }
}

// ============================================================================
// HTTP ENDPOINTS — Phase 8C (called from Win32IDE_LocalServer.cpp)
// ============================================================================

void Win32IDE::handleRouterStatusEndpoint(SOCKET client) {
    nlohmann::json j;
    j["enabled"]     = m_routerEnabled;
    j["initialized"] = m_routerInitialized;

    // Task preferences
    nlohmann::json prefs = nlohmann::json::array();
    for (size_t i = 0; i < (size_t)LLMTaskType::Count; ++i) {
        const auto& pref = m_taskPreferences[i];
        nlohmann::json pj;
        pj["task"]      = taskTypeString((LLMTaskType)i);
        pj["preferred"] = backendTypeString(pref.preferredBackend);
        pj["fallback"]  = (pref.fallbackBackend == AIBackendType::Count)
                           ? "none" : backendTypeString(pref.fallbackBackend);
        pj["allowFallback"] = pref.allowFallback;
        prefs.push_back(pj);
    }
    j["taskPreferences"] = prefs;

    // Consecutive failures
    nlohmann::json failures;
    for (size_t i = 0; i < (size_t)AIBackendType::Count; ++i) {
        failures[backendTypeString((AIBackendType)i)] = m_consecutiveFailures[i];
    }
    j["consecutiveFailures"] = failures;

    // Stats
    nlohmann::json stats;
    stats["totalRouted"]        = m_routerStats.totalRouted;
    stats["totalFallbacksUsed"] = m_routerStats.totalFallbacksUsed;
    stats["totalPolicyOverrides"] = m_routerStats.totalPolicyOverrides;
    j["stats"] = stats;

    std::string body = j.dump(2);
    std::string resp = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
                       "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
    send(client, resp.c_str(), (int)resp.size(), 0);
}

void Win32IDE::handleRouterDecisionEndpoint(SOCKET client) {
    RoutingDecision last = getLastRoutingDecision();

    nlohmann::json j;
    j["classifiedTask"]    = taskTypeString(last.classifiedTask);
    j["selectedBackend"]   = backendTypeString(last.selectedBackend);
    j["fallbackBackend"]   = (last.fallbackBackend == AIBackendType::Count)
                              ? "none" : backendTypeString(last.fallbackBackend);
    j["confidence"]        = last.confidence;
    j["reason"]            = last.reason;
    j["policyOverride"]    = last.policyOverride;
    j["fallbackUsed"]      = last.fallbackUsed;
    j["decisionEpochMs"]   = last.decisionEpochMs;
    j["primaryLatencyMs"]  = last.primaryLatencyMs;
    j["fallbackLatencyMs"] = last.fallbackLatencyMs;

    std::string body = j.dump(2);
    std::string resp = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
                       "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
    send(client, resp.c_str(), (int)resp.size(), 0);
}

void Win32IDE::handleRouterCapabilitiesEndpoint(SOCKET client) {
    nlohmann::json j = nlohmann::json::array();
    for (size_t i = 0; i < (size_t)AIBackendType::Count; ++i) {
        const auto& cap = m_backendCapabilities[i];
        nlohmann::json cj;
        cj["backend"]                = backendTypeString((AIBackendType)i);
        cj["maxContextTokens"]       = cap.maxContextTokens;
        cj["supportsToolCalls"]      = cap.supportsToolCalls;
        cj["supportsStreaming"]       = cap.supportsStreaming;
        cj["supportsFunctionCalling"] = cap.supportsFunctionCalling;
        cj["supportsJsonMode"]        = cap.supportsJsonMode;
        cj["costTier"]                = cap.costTier;
        cj["qualityScore"]            = cap.qualityScore;
        cj["notes"]                   = cap.notes;
        j.push_back(cj);
    }

    std::string body = j.dump(2);
    std::string resp = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
                       "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
    send(client, resp.c_str(), (int)resp.size(), 0);
}

void Win32IDE::handleRouterRouteEndpoint(SOCKET client, const std::string& body) {
    try {
        nlohmann::json reqJson = nlohmann::json::parse(body);
        std::string prompt = reqJson.value("prompt", "");

        if (prompt.empty()) {
            std::string errBody = R"({"error":"Missing 'prompt' field"})";
            std::string resp = "HTTP/1.1 400 Bad Request\r\nContent-Type: application/json\r\n"
                               "Content-Length: " + std::to_string(errBody.size()) + "\r\n\r\n" + errBody;
            send(client, resp.c_str(), (int)resp.size(), 0);
            return;
        }

        // Classify + select (dry run — does not execute inference)
        LLMTaskType task = classifyTask(prompt);
        RoutingDecision decision = selectBackendForTask(task, prompt);

        nlohmann::json j;
        j["classifiedTask"]  = taskTypeString(decision.classifiedTask);
        j["selectedBackend"] = backendTypeString(decision.selectedBackend);
        j["fallbackBackend"] = (decision.fallbackBackend == AIBackendType::Count)
                                ? "none" : backendTypeString(decision.fallbackBackend);
        j["confidence"]      = decision.confidence;
        j["reason"]          = decision.reason;
        j["policyOverride"]  = decision.policyOverride;

        std::string respBody = j.dump(2);
        std::string resp = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
                           "Content-Length: " + std::to_string(respBody.size()) + "\r\n\r\n" + respBody;
        send(client, resp.c_str(), (int)resp.size(), 0);
    } catch (const std::exception& e) {
        std::string errBody = std::string(R"({"error":")") + e.what() + "\"}";
        std::string resp = "HTTP/1.1 400 Bad Request\r\nContent-Type: application/json\r\n"
                           "Content-Length: " + std::to_string(errBody.size()) + "\r\n\r\n" + errBody;
        send(client, resp.c_str(), (int)resp.size(), 0);
    }
}

// ============================================================================
// UX ENHANCEMENT: "Why This Backend?" — Detailed Explanation
// ============================================================================

std::string Win32IDE::generateWhyExplanation(const RoutingDecision& decision) const {
    std::ostringstream ss;

    ss << "╔══════════════════════════════════════════════════════════╗\n";
    ss << "║  WHY THIS BACKEND?  —  Routing Decision Explanation     ║\n";
    ss << "╚══════════════════════════════════════════════════════════╝\n\n";

    // 1. Task classification
    ss << "▸ Task Classification: " << taskTypeString(decision.classifiedTask) << "\n";
    ss << "  Confidence: " << (int)(decision.confidence * 100) << "%\n\n";

    // 2. Selected backend + capabilities
    ss << "▸ Selected Backend: " << backendTypeString(decision.selectedBackend) << "\n";
    if (decision.selectedBackend < AIBackendType::Count) {
        const auto& cap = m_backendCapabilities[(size_t)decision.selectedBackend];
        ss << "  Context Window:    " << cap.maxContextTokens << " tokens\n";
        ss << "  Quality Score:     " << (int)(cap.qualityScore * 100) << "%\n";
        ss << "  Cost Tier:         " << cap.costTier << " (0=free, 3=expensive)\n";
        ss << "  Tool Calls:        " << (cap.supportsToolCalls ? "✓" : "✗") << "\n";
        ss << "  Function Calling:  " << (cap.supportsFunctionCalling ? "✓" : "✗") << "\n";
        ss << "  Streaming:         " << (cap.supportsStreaming ? "✓" : "✗") << "\n";
        ss << "  JSON Mode:         " << (cap.supportsJsonMode ? "✓" : "✗") << "\n";
        if (!cap.notes.empty()) {
            ss << "  Notes:             " << cap.notes << "\n";
        }
    }
    ss << "\n";

    // 3. Routing rationale
    ss << "▸ Routing Rationale:\n";
    ss << "  " << decision.reason << "\n\n";

    // 4. Was the task pinned?
    const auto& pin = m_taskPins[(size_t)decision.classifiedTask];
    if (pin.active) {
        ss << "▸ Pin Status: PINNED to " << backendTypeString(pin.pinnedBackend) << "\n";
        if (!pin.reason.empty()) ss << "  Pin Reason: " << pin.reason << "\n";
        ss << "\n";
    } else {
        ss << "▸ Pin Status: Not pinned (routed by policy)\n\n";
    }

    // 5. Fallback info
    if (decision.fallbackBackend != AIBackendType::Count) {
        ss << "▸ Fallback Backend: " << backendTypeString(decision.fallbackBackend) << "\n";
        if (decision.fallbackUsed) {
            ss << "  ⚠ Fallback WAS used (primary failed)\n";
            ss << "  Primary Latency:  " << decision.primaryLatencyMs << " ms\n";
            ss << "  Fallback Latency: " << decision.fallbackLatencyMs << " ms\n";
        } else {
            ss << "  Fallback was NOT needed (primary succeeded)\n";
        }
    } else {
        ss << "▸ Fallback: None configured\n";
    }
    ss << "\n";

    // 6. Policy override
    if (decision.policyOverride) {
        ss << "▸ ⚠ Policy Override: The router's standard preference was overridden\n";
        ss << "  (due to failures, pin, or viability constraints).\n\n";
    }

    // 7. Performance
    if (decision.primaryLatencyMs >= 0) {
        ss << "▸ Performance: " << decision.primaryLatencyMs << " ms";
        if (decision.fallbackLatencyMs >= 0) {
            ss << " (fallback: " << decision.fallbackLatencyMs << " ms)";
        }
        ss << "\n";
    }

    return ss.str();
}

std::string Win32IDE::generateWhyExplanationForLast() const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(m_routerMutex));
    if (m_lastRoutingDecision.decisionEpochMs == 0) {
        return "[LLMRouter] No routing decisions recorded yet. Enable the router and send a prompt first.";
    }
    return generateWhyExplanation(m_lastRoutingDecision);
}

// ============================================================================
// UX ENHANCEMENT: Per-Task Backend Pinning
// ============================================================================

void Win32IDE::pinTaskToBackend(LLMTaskType task, AIBackendType backend, const std::string& reason) {
    if (task >= LLMTaskType::Count || backend >= AIBackendType::Count) return;

    std::lock_guard<std::mutex> lock(m_routerMutex);
    auto& pin         = m_taskPins[(size_t)task];
    pin.task           = task;
    pin.pinnedBackend  = backend;
    pin.active         = true;
    pin.reason         = reason.empty() ? "User pinned via palette" : reason;

    logInfo("[LLMRouter] Pinned task '" + taskTypeString(task) + "' → backend '" +
            backendTypeString(backend) + "' (" + pin.reason + ")");
    appendToOutput("[LLMRouter] Pinned: " + taskTypeString(task) + " → " +
                   backendTypeString(backend),
                   "General", OutputSeverity::Info);
}

void Win32IDE::unpinTask(LLMTaskType task) {
    if (task >= LLMTaskType::Count) return;

    std::lock_guard<std::mutex> lock(m_routerMutex);
    auto& pin   = m_taskPins[(size_t)task];
    std::string old = backendTypeString(pin.pinnedBackend);
    pin.active  = false;
    pin.pinnedBackend = AIBackendType::Count;
    pin.reason.clear();

    logInfo("[LLMRouter] Unpinned task '" + taskTypeString(task) + "' (was → '" + old + "')");
    appendToOutput("[LLMRouter] Unpinned: " + taskTypeString(task),
                   "General", OutputSeverity::Info);
}

void Win32IDE::clearAllPins() {
    std::lock_guard<std::mutex> lock(m_routerMutex);
    for (size_t i = 0; i < (size_t)LLMTaskType::Count; ++i) {
        m_taskPins[i] = {};
    }
    logInfo("[LLMRouter] All task pins cleared");
    appendToOutput("[LLMRouter] All task pins cleared.",
                   "General", OutputSeverity::Info);
}

bool Win32IDE::isTaskPinned(LLMTaskType task) const {
    if (task >= LLMTaskType::Count) return false;
    return m_taskPins[(size_t)task].active;
}

Win32IDE::TaskBackendPin Win32IDE::getTaskPin(LLMTaskType task) const {
    if (task >= LLMTaskType::Count) return TaskBackendPin{};
    return m_taskPins[(size_t)task];
}

std::string Win32IDE::getPinnedTasksString() const {
    std::ostringstream ss;
    ss << "[LLMRouter] Task Pins:\n";
    bool anyPinned = false;
    for (size_t i = 0; i < (size_t)LLMTaskType::Count; ++i) {
        const auto& pin = m_taskPins[i];
        if (pin.active) {
            ss << "  📌 " << taskTypeString((LLMTaskType)i)
               << " → " << backendTypeString(pin.pinnedBackend);
            if (!pin.reason.empty()) ss << " (" << pin.reason << ")";
            ss << "\n";
            anyPinned = true;
        }
    }
    if (!anyPinned) {
        ss << "  (no tasks pinned — all routing via policy)\n";
    }
    return ss.str();
}

// ============================================================================
// UX ENHANCEMENT: Cost / Latency Heatmap
// ============================================================================

void Win32IDE::recordCostLatency(const RoutingDecision& decision) {
    std::lock_guard<std::mutex> lock(m_routerMutex);

    // Build a record
    CostLatencyRecord rec;
    rec.task         = decision.classifiedTask;
    rec.backend      = decision.fallbackUsed ? decision.fallbackBackend : decision.selectedBackend;
    rec.latencyMs    = decision.fallbackUsed ? decision.fallbackLatencyMs : decision.primaryLatencyMs;
    rec.epochMs      = decision.decisionEpochMs;
    rec.fallbackUsed = decision.fallbackUsed;

    if (rec.backend < AIBackendType::Count) {
        rec.costTier     = m_backendCapabilities[(size_t)rec.backend].costTier;
        rec.qualityScore = m_backendCapabilities[(size_t)rec.backend].qualityScore;
    }

    // Append to rolling log
    if (m_costLatencyLog.size() >= MAX_COST_LATENCY_LOG) {
        m_costLatencyLog.erase(m_costLatencyLog.begin());
    }
    m_costLatencyLog.push_back(rec);

    // Update heatmap cell
    if (rec.task < LLMTaskType::Count && rec.backend < AIBackendType::Count) {
        auto& cell = m_heatmap[(size_t)rec.task][(size_t)rec.backend];
        cell.requestCount++;
        cell.totalLatencyMs += (double)rec.latencyMs;
        cell.avgLatencyMs    = cell.totalLatencyMs / (double)cell.requestCount;
        if (rec.latencyMs < cell.minLatencyMs) cell.minLatencyMs = rec.latencyMs;
        if (rec.latencyMs > cell.maxLatencyMs) cell.maxLatencyMs = rec.latencyMs;
        cell.avgCostTier = ((cell.avgCostTier * (cell.requestCount - 1)) + rec.costTier)
                           / (double)cell.requestCount;
        cell.avgQuality  = ((cell.avgQuality * (cell.requestCount - 1)) + rec.qualityScore)
                           / (double)cell.requestCount;
    }
}

Win32IDE::HeatmapCell Win32IDE::getHeatmapCell(LLMTaskType task, AIBackendType backend) const {
    if (task >= LLMTaskType::Count || backend >= AIBackendType::Count) return HeatmapCell{};
    return m_heatmap[(size_t)task][(size_t)backend];
}

std::string Win32IDE::getCostLatencyHeatmapString() const {
    std::ostringstream ss;
    ss << "[LLMRouter] Cost / Latency Heatmap:\n";
    ss << "  ──────────────────────────────────────────────────────────────\n";

    // Column headers: backends
    ss << "  Task\\Backend     ";
    for (size_t b = 0; b < (size_t)AIBackendType::Count; ++b) {
        std::string bname = backendTypeString((AIBackendType)b);
        // Pad to 14 chars
        while (bname.size() < 14) bname += ' ';
        ss << bname;
    }
    ss << "\n  ──────────────────────────────────────────────────────────────\n";

    for (size_t t = 0; t < (size_t)LLMTaskType::Count; ++t) {
        std::string tname = taskTypeString((LLMTaskType)t);
        while (tname.size() < 17) tname += ' ';
        ss << "  " << tname;

        for (size_t b = 0; b < (size_t)AIBackendType::Count; ++b) {
            const auto& cell = m_heatmap[t][b];
            if (cell.requestCount == 0) {
                ss << "     -        ";
            } else {
                std::ostringstream cellStr;
                cellStr << (int)cell.avgLatencyMs << "ms/"
                        << cell.requestCount << "r";
                std::string cs = cellStr.str();
                while (cs.size() < 14) cs += ' ';
                ss << cs;
            }
        }
        ss << "\n";
    }

    ss << "  ──────────────────────────────────────────────────────────────\n";
    ss << "  Legend: <avg latency>ms / <request count>r\n";
    return ss.str();
}

std::string Win32IDE::getCostStatsString() const {
    std::ostringstream ss;
    ss << "[LLMRouter] Cost & Performance Statistics:\n";
    ss << "  Total Records: " << m_costLatencyLog.size() << "\n\n";

    // Per-backend aggregation
    struct BStats {
        uint64_t count = 0;
        double totalLatency = 0;
        int minLat = INT_MAX;
        int maxLat = 0;
        double totalCost = 0;
    };
    std::array<BStats, (size_t)AIBackendType::Count> bstats = {};

    for (const auto& rec : m_costLatencyLog) {
        if (rec.backend >= AIBackendType::Count) continue;
        auto& bs = bstats[(size_t)rec.backend];
        bs.count++;
        bs.totalLatency += rec.latencyMs;
        if (rec.latencyMs < bs.minLat) bs.minLat = rec.latencyMs;
        if (rec.latencyMs > bs.maxLat) bs.maxLat = rec.latencyMs;
        bs.totalCost += rec.costTier;
    }

    for (size_t i = 0; i < (size_t)AIBackendType::Count; ++i) {
        const auto& bs = bstats[i];
        if (bs.count == 0) continue;
        ss << "  " << backendTypeString((AIBackendType)i) << ":\n";
        ss << "    Requests:     " << bs.count << "\n";
        ss << "    Avg Latency:  " << (int)(bs.totalLatency / bs.count) << " ms\n";
        ss << "    Min Latency:  " << bs.minLat << " ms\n";
        ss << "    Max Latency:  " << bs.maxLat << " ms\n";
        ss << "    Avg Cost Tier:" << std::fixed << std::setprecision(1)
           << (bs.totalCost / bs.count) << "\n\n";
    }

    return ss.str();
}

// ============================================================================
// RESEARCH: Ensemble Routing (Multi-Backend Fan-Out)
// ============================================================================

void Win32IDE::setEnsembleEnabled(bool enabled) {
    m_ensembleEnabled = enabled;
    logInfo("[LLMRouter] Ensemble mode " + std::string(enabled ? "ENABLED" : "DISABLED"));
    appendToOutput("[LLMRouter] Ensemble routing " +
                   std::string(enabled ? "ENABLED — prompts will be sent to multiple backends and the best response selected."
                                       : "DISABLED — standard single-backend routing restored."),
                   "General", OutputSeverity::Info);
}

bool Win32IDE::isEnsembleEnabled() const {
    return m_ensembleEnabled;
}

std::string Win32IDE::routeWithEnsemble(const std::string& prompt) {
    LLMTaskType task = classifyTask(prompt);
    EnsembleDecision ensemble = buildEnsembleDecision(task, prompt);

    // Store for later inspection
    {
        std::lock_guard<std::mutex> lock(m_routerMutex);
        m_lastEnsembleDecision = ensemble;
    }

    logInfo("[LLMRouter] Ensemble: " + std::to_string(ensemble.votes.size()) +
            " backends queried, winner='" + backendTypeString(ensemble.winnerBackend) +
            "' (conf=" + std::to_string((int)(ensemble.winnerConfidence * 100)) +
            "%, " + std::to_string(ensemble.totalLatencyMs) + "ms total)");

    return ensemble.mergedResponse;
}

Win32IDE::EnsembleDecision Win32IDE::buildEnsembleDecision(LLMTaskType task,
                                                            const std::string& prompt) {
    EnsembleDecision ensemble;
    ensemble.classifiedTask  = task;
    ensemble.decisionEpochMs = currentEpochMs();
    ensemble.strategy        = "confidence-weighted";

    auto wallStart = std::chrono::steady_clock::now();

    // Identify viable backends for this task
    std::vector<AIBackendType> candidates;
    for (size_t i = 0; i < (size_t)AIBackendType::Count; ++i) {
        AIBackendType bt = (AIBackendType)i;
        if (isBackendViableForTask(bt, task)) {
            candidates.push_back(bt);
        }
    }

    if (candidates.empty()) {
        // Fallback to active backend
        candidates.push_back(m_activeBackend);
    }

    // Query each candidate backend sequentially
    // (Could be parallelized in a future iteration with thread pool)
    AIBackendType originalActive;
    {
        std::lock_guard<std::mutex> lock(m_backendMutex);
        originalActive = m_activeBackend;
    }

    for (auto bt : candidates) {
        EnsembleVote vote;
        vote.backend = bt;

        // Compute confidence weight from capability profile
        {
            const auto& cap = m_backendCapabilities[(size_t)bt];
            vote.confidenceWeight = cap.qualityScore;
            // Bonus for task alignment
            if (task == LLMTaskType::ToolExecution && cap.supportsFunctionCalling) vote.confidenceWeight += 0.15f;
            if (task == LLMTaskType::Research && cap.maxContextTokens >= 100000)   vote.confidenceWeight += 0.15f;
            if (task == LLMTaskType::Chat && cap.costTier == 0)                    vote.confidenceWeight += 0.1f;
            if (vote.confidenceWeight > 1.0f) vote.confidenceWeight = 1.0f;
        }

        // Execute inference on this backend
        {
            std::lock_guard<std::mutex> lock(m_backendMutex);
            m_activeBackend = bt;
        }

        auto start = std::chrono::steady_clock::now();
        std::string result = routeInferenceRequest(prompt);
        auto end = std::chrono::steady_clock::now();
        vote.latencyMs = (int)std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        bool failed = result.empty() || result.find("[BackendSwitcher] Error") != std::string::npos;
        vote.succeeded = !failed;
        if (vote.succeeded) {
            vote.response = result;
        }

        ensemble.votes.push_back(vote);
    }

    // Restore original backend
    {
        std::lock_guard<std::mutex> lock(m_backendMutex);
        m_activeBackend = originalActive;
    }

    auto wallEnd = std::chrono::steady_clock::now();
    ensemble.totalLatencyMs = (int)std::chrono::duration_cast<std::chrono::milliseconds>(wallEnd - wallStart).count();

    // Select winner
    ensemble.mergedResponse = selectEnsembleWinner(ensemble);

    return ensemble;
}

std::string Win32IDE::selectEnsembleWinner(EnsembleDecision& decision) const {
    // Strategy: pick the successful response with the highest confidence weight
    float bestWeight = -1.0f;
    size_t bestIndex = 0;
    bool anySuccess = false;

    for (size_t i = 0; i < decision.votes.size(); ++i) {
        const auto& vote = decision.votes[i];
        if (vote.succeeded && vote.confidenceWeight > bestWeight) {
            bestWeight = vote.confidenceWeight;
            bestIndex  = i;
            anySuccess = true;
        }
    }

    if (anySuccess) {
        decision.winnerBackend    = decision.votes[bestIndex].backend;
        decision.winnerConfidence = decision.votes[bestIndex].confidenceWeight;
        return decision.votes[bestIndex].response;
    }

    // No backend succeeded
    decision.winnerBackend    = AIBackendType::Count;
    decision.winnerConfidence = 0.0f;
    return "[LLMRouter] Ensemble: All " + std::to_string(decision.votes.size()) +
           " backends failed to produce a response.";
}

std::string Win32IDE::getEnsembleStatusString() const {
    std::ostringstream ss;
    ss << "[LLMRouter] Ensemble Status:\n";
    ss << "  Mode: " << (m_ensembleEnabled ? "ENABLED" : "DISABLED") << "\n";

    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(m_routerMutex));
    if (m_lastEnsembleDecision.decisionEpochMs == 0) {
        ss << "  No ensemble decisions recorded yet.\n";
        return ss.str();
    }

    const auto& ens = m_lastEnsembleDecision;
    ss << "  Last Ensemble Decision:\n";
    ss << "    Task:        " << taskTypeString(ens.classifiedTask) << "\n";
    ss << "    Strategy:    " << ens.strategy << "\n";
    ss << "    Winner:      " << backendTypeString(ens.winnerBackend)
       << " (confidence " << (int)(ens.winnerConfidence * 100) << "%)\n";
    ss << "    Total Time:  " << ens.totalLatencyMs << " ms\n";
    ss << "    Votes:\n";

    for (const auto& vote : ens.votes) {
        ss << "      " << backendTypeString(vote.backend)
           << ": " << (vote.succeeded ? "OK" : "FAIL")
           << " (" << vote.latencyMs << "ms, weight="
           << (int)(vote.confidenceWeight * 100) << "%)"
           << "\n";
    }

    return ss.str();
}

// ============================================================================
// RESEARCH: Offline Routing Simulation
// ============================================================================

Win32IDE::SimulationResult Win32IDE::simulateRoutingOffline(
    const std::vector<SimulationInput>& inputs) const
{
    SimulationResult result;
    result.totalInputs = (int)inputs.size();
    result.inputs      = inputs;

    for (const auto& input : inputs) {
        SimulationResult::PerInput pi;
        pi.prompt = input.prompt;

        // Classify
        pi.classifiedTask = classifyTask(input.prompt);

        // Simulate selection (create a mutable copy of this for the call)
        // We use const_cast because selectBackendForTask takes a lock internally
        // but the simulation is read-only in spirit (we don't actually route)
        auto* mutableThis = const_cast<Win32IDE*>(this);
        RoutingDecision decision = mutableThis->selectBackendForTask(pi.classifiedTask, input.prompt);

        pi.selectedBackend = decision.selectedBackend;
        pi.confidence      = decision.confidence;
        pi.reason          = decision.reason;

        // Check if classification matches expected
        if (input.expectedTask != LLMTaskType::General ||
            input.prompt.empty() == false) {
            // Only count as "matched" if the user specified a non-General expected type
            if (input.expectedTask != LLMTaskType::General) {
                pi.matchedExpected = (pi.classifiedTask == input.expectedTask);
                if (pi.matchedExpected) result.correctClassifications++;
            }
        }

        result.results.push_back(pi);
    }

    if (result.totalInputs > 0) {
        // Count accuracy only over inputs where expectedTask != General
        int withExpected = 0;
        for (const auto& inp : inputs) {
            if (inp.expectedTask != LLMTaskType::General) withExpected++;
        }
        if (withExpected > 0) {
            result.classificationAccuracy = (float)result.correctClassifications / (float)withExpected;
        }
    }

    // Build summary
    std::ostringstream ss;
    ss << "Simulation: " << result.totalInputs << " prompts, "
       << result.correctClassifications << " correct classifications";
    if (result.classificationAccuracy > 0.0f) {
        ss << " (" << (int)(result.classificationAccuracy * 100) << "% accuracy)";
    }
    result.summaryText = ss.str();

    return result;
}

Win32IDE::SimulationResult Win32IDE::simulateFromHistory(int maxEvents) const {
    // Load agent history events that have prompts
    std::vector<AgentEvent> events = loadHistory(maxEvents);

    std::vector<SimulationInput> inputs;
    for (const auto& ev : events) {
        if (ev.prompt.empty()) continue;
        SimulationInput inp;
        inp.prompt       = ev.prompt;
        inp.expectedTask = LLMTaskType::General;  // No ground truth from history
        inputs.push_back(inp);
        if ((int)inputs.size() >= maxEvents) break;
    }

    if (inputs.empty()) {
        SimulationResult empty;
        empty.summaryText = "No prompts found in agent history for simulation.";
        return empty;
    }

    return simulateRoutingOffline(inputs);
}

std::string Win32IDE::getSimulationResultString(const SimulationResult& result) const {
    std::ostringstream ss;
    ss << "[LLMRouter] Routing Simulation Results:\n";
    ss << "  " << result.summaryText << "\n";
    ss << "  ──────────────────────────────────────\n";

    for (size_t i = 0; i < result.results.size() && i < 50; ++i) {
        const auto& r = result.results[i];
        std::string promptPreview = r.prompt.substr(0, 60);
        if (r.prompt.size() > 60) promptPreview += "...";

        ss << "  [" << (i + 1) << "] \"" << promptPreview << "\"\n";
        ss << "      Task: " << taskTypeString(r.classifiedTask)
           << " → " << backendTypeString(r.selectedBackend)
           << " (conf " << (int)(r.confidence * 100) << "%)";
        if (r.matchedExpected) ss << " ✓";
        ss << "\n";
    }

    if (result.results.size() > 50) {
        ss << "  ... and " << (result.results.size() - 50) << " more\n";
    }

    return ss.str();
}

// ============================================================================
// UX/RESEARCH HTTP ENDPOINTS
// ============================================================================

void Win32IDE::handleRouterWhyEndpoint(SOCKET client) {
    std::string explanation = generateWhyExplanationForLast();

    // Build JSON response with both text and structured data
    nlohmann::json j;
    j["explanation"] = explanation;

    {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(m_routerMutex));
        if (m_lastRoutingDecision.decisionEpochMs > 0) {
            const auto& d = m_lastRoutingDecision;
            j["classifiedTask"]    = taskTypeString(d.classifiedTask);
            j["selectedBackend"]   = backendTypeString(d.selectedBackend);
            j["confidence"]        = d.confidence;
            j["reason"]            = d.reason;
            j["policyOverride"]    = d.policyOverride;
            j["fallbackUsed"]      = d.fallbackUsed;
            j["primaryLatencyMs"]  = d.primaryLatencyMs;
            j["fallbackLatencyMs"] = d.fallbackLatencyMs;

            // Include capability snapshot
            if (d.selectedBackend < AIBackendType::Count) {
                const auto& cap = m_backendCapabilities[(size_t)d.selectedBackend];
                nlohmann::json cj;
                cj["maxContextTokens"]       = cap.maxContextTokens;
                cj["qualityScore"]           = cap.qualityScore;
                cj["costTier"]               = cap.costTier;
                cj["supportsToolCalls"]      = cap.supportsToolCalls;
                cj["supportsFunctionCalling"] = cap.supportsFunctionCalling;
                cj["supportsStreaming"]       = cap.supportsStreaming;
                cj["supportsJsonMode"]       = cap.supportsJsonMode;
                j["backendCapability"]       = cj;
            }

            // Pin status
            const auto& pin = m_taskPins[(size_t)d.classifiedTask];
            j["pinned"]        = pin.active;
            j["pinnedBackend"] = pin.active ? backendTypeString(pin.pinnedBackend) : "none";
            j["pinReason"]     = pin.reason;
        }
    }

    std::string body = j.dump(2);
    std::string resp = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
                       "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
    send(client, resp.c_str(), (int)resp.size(), 0);
}

void Win32IDE::handleRouterPinsEndpoint(SOCKET client) {
    nlohmann::json j = nlohmann::json::array();

    for (size_t i = 0; i < (size_t)LLMTaskType::Count; ++i) {
        const auto& pin = m_taskPins[i];
        nlohmann::json pj;
        pj["task"]          = taskTypeString((LLMTaskType)i);
        pj["active"]        = pin.active;
        pj["pinnedBackend"] = pin.active ? backendTypeString(pin.pinnedBackend) : "none";
        pj["reason"]        = pin.reason;
        j.push_back(pj);
    }

    std::string body = j.dump(2);
    std::string resp = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
                       "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
    send(client, resp.c_str(), (int)resp.size(), 0);
}

void Win32IDE::handleRouterHeatmapEndpoint(SOCKET client) {
    nlohmann::json j;

    // Heatmap grid
    nlohmann::json grid = nlohmann::json::array();
    for (size_t t = 0; t < (size_t)LLMTaskType::Count; ++t) {
        nlohmann::json row;
        row["task"] = taskTypeString((LLMTaskType)t);
        nlohmann::json backends;
        for (size_t b = 0; b < (size_t)AIBackendType::Count; ++b) {
            const auto& cell = m_heatmap[t][b];
            nlohmann::json cj;
            cj["requestCount"] = cell.requestCount;
            cj["avgLatencyMs"] = (int)cell.avgLatencyMs;
            cj["minLatencyMs"] = (cell.requestCount > 0) ? cell.minLatencyMs : 0;
            cj["maxLatencyMs"] = cell.maxLatencyMs;
            cj["avgCostTier"]  = cell.avgCostTier;
            cj["avgQuality"]   = cell.avgQuality;
            backends[backendTypeString((AIBackendType)b)] = cj;
        }
        row["backends"] = backends;
        grid.push_back(row);
    }
    j["heatmap"] = grid;

    // Summary stats
    j["totalRecords"] = (uint64_t)m_costLatencyLog.size();
    j["maxRecords"]   = (uint64_t)MAX_COST_LATENCY_LOG;

    std::string body = j.dump(2);
    std::string resp = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
                       "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
    send(client, resp.c_str(), (int)resp.size(), 0);
}

void Win32IDE::handleRouterEnsembleEndpoint(SOCKET client, const std::string& body) {
    try {
        nlohmann::json reqJson = nlohmann::json::parse(body);

        // Check for enable/disable action
        if (reqJson.contains("action")) {
            std::string action = reqJson["action"].get<std::string>();
            if (action == "enable") {
                setEnsembleEnabled(true);
            } else if (action == "disable") {
                setEnsembleEnabled(false);
            } else if (action == "status") {
                // Return status
            }

            nlohmann::json j;
            j["ensembleEnabled"] = m_ensembleEnabled;
            j["status"]          = getEnsembleStatusString();

            std::string respBody = j.dump(2);
            std::string resp = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
                               "Content-Length: " + std::to_string(respBody.size()) + "\r\n\r\n" + respBody;
            send(client, resp.c_str(), (int)resp.size(), 0);
            return;
        }

        // Otherwise, run an ensemble query
        std::string prompt = reqJson.value("prompt", "");
        if (prompt.empty()) {
            std::string errBody = R"({"error":"Missing 'prompt' or 'action' field"})";
            std::string resp = "HTTP/1.1 400 Bad Request\r\nContent-Type: application/json\r\n"
                               "Content-Length: " + std::to_string(errBody.size()) + "\r\n\r\n" + errBody;
            send(client, resp.c_str(), (int)resp.size(), 0);
            return;
        }

        LLMTaskType task = classifyTask(prompt);
        EnsembleDecision decision = buildEnsembleDecision(task, prompt);

        nlohmann::json j;
        j["classifiedTask"]   = taskTypeString(decision.classifiedTask);
        j["strategy"]         = decision.strategy;
        j["winnerBackend"]    = backendTypeString(decision.winnerBackend);
        j["winnerConfidence"] = decision.winnerConfidence;
        j["totalLatencyMs"]   = decision.totalLatencyMs;
        j["mergedResponse"]   = decision.mergedResponse;

        nlohmann::json votesArr = nlohmann::json::array();
        for (const auto& vote : decision.votes) {
            nlohmann::json vj;
            vj["backend"]          = backendTypeString(vote.backend);
            vj["succeeded"]        = vote.succeeded;
            vj["latencyMs"]        = vote.latencyMs;
            vj["confidenceWeight"] = vote.confidenceWeight;
            vj["responsePreview"]  = vote.response.substr(0, 200);
            votesArr.push_back(vj);
        }
        j["votes"] = votesArr;

        std::string respBody = j.dump(2);
        std::string resp = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
                           "Content-Length: " + std::to_string(respBody.size()) + "\r\n\r\n" + respBody;
        send(client, resp.c_str(), (int)resp.size(), 0);
    } catch (const std::exception& e) {
        std::string errBody = std::string(R"({"error":")") + e.what() + "\"}";
        std::string resp = "HTTP/1.1 400 Bad Request\r\nContent-Type: application/json\r\n"
                           "Content-Length: " + std::to_string(errBody.size()) + "\r\n\r\n" + errBody;
        send(client, resp.c_str(), (int)resp.size(), 0);
    }
}

void Win32IDE::handleRouterSimulateEndpoint(SOCKET client, const std::string& body) {
    try {
        nlohmann::json reqJson = nlohmann::json::parse(body);

        SimulationResult simResult;

        if (reqJson.contains("fromHistory") && reqJson["fromHistory"].get<bool>()) {
            int maxEvents = reqJson.value("maxEvents", 100);
            simResult = simulateFromHistory(maxEvents);
        } else if (reqJson.contains("prompts") && reqJson["prompts"].is_array()) {
            std::vector<SimulationInput> inputs;
            for (size_t pi = 0; pi < reqJson["prompts"].size(); ++pi) {
                const auto& pj = reqJson["prompts"][pi];
                SimulationInput inp;
                if (pj.is_string()) {
                    inp.prompt = pj.get<std::string>();
                } else if (pj.is_object()) {
                    inp.prompt       = pj.value("prompt", "");
                    std::string expTask = pj.value("expectedTask", "General");
                    inp.expectedTask = taskTypeFromString(expTask);
                }
                if (!inp.prompt.empty()) inputs.push_back(inp);
            }
            simResult = simulateRoutingOffline(inputs);
        } else {
            std::string errBody = R"({"error":"Provide 'prompts' array or 'fromHistory': true"})";
            std::string resp = "HTTP/1.1 400 Bad Request\r\nContent-Type: application/json\r\n"
                               "Content-Length: " + std::to_string(errBody.size()) + "\r\n\r\n" + errBody;
            send(client, resp.c_str(), (int)resp.size(), 0);
            return;
        }

        // Store last result
        {
            std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(m_routerMutex));
            const_cast<SimulationResult&>(m_lastSimulationResult) = simResult;
        }

        // Build JSON response
        nlohmann::json j;
        j["totalInputs"]             = simResult.totalInputs;
        j["correctClassifications"]  = simResult.correctClassifications;
        j["classificationAccuracy"]  = simResult.classificationAccuracy;
        j["summary"]                 = simResult.summaryText;

        nlohmann::json resultsArr = nlohmann::json::array();
        for (const auto& r : simResult.results) {
            nlohmann::json rj;
            rj["prompt"]          = r.prompt.substr(0, 120);
            rj["classifiedTask"]  = taskTypeString(r.classifiedTask);
            rj["selectedBackend"] = backendTypeString(r.selectedBackend);
            rj["confidence"]      = r.confidence;
            rj["reason"]          = r.reason;
            rj["matchedExpected"] = r.matchedExpected;
            resultsArr.push_back(rj);
        }
        j["results"] = resultsArr;

        std::string respBody = j.dump(2);
        std::string resp = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
                           "Content-Length: " + std::to_string(respBody.size()) + "\r\n\r\n" + respBody;
        send(client, resp.c_str(), (int)resp.size(), 0);
    } catch (const std::exception& e) {
        std::string errBody = std::string(R"({"error":")") + e.what() + "\"}";
        std::string resp = "HTTP/1.1 400 Bad Request\r\nContent-Type: application/json\r\n"
                           "Content-Length: " + std::to_string(errBody.size()) + "\r\n\r\n" + errBody;
        send(client, resp.c_str(), (int)resp.size(), 0);
    }
}
