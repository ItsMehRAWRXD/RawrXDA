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
