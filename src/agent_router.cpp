// ============================================================================
// agent_router.cpp — Production Agent Router, Model Selector & Dispatch
// ============================================================================
// Routes requests to the optimal agent mode, model, and execution strategy.
// Context-aware dispatch with fallback chains, health checks, and metrics.
// Platform-independent. No Qt.
// ============================================================================

#include "agent_modes.h"
#include "agentic_engine.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <chrono>
#include <algorithm>
#include <functional>
#include <sstream>
#include <regex>
#include <cmath>
#include <deque>
#include <thread>
#include <iomanip>

// ============================================================================
// RouteResult — structured output from the router
// ============================================================================
struct RouteResult {
    AgentMode       mode           = ASK;
    std::string     modelId;             // e.g. "qwen2.5-coder:7b"
    std::string     systemPrompt;
    std::string     correlationId;       // unique per-request tracking ID
    int64_t         timestampMs    = 0;   // request timestamp (epoch ms)
    int             maxTokens      = 2048;
    float           temperature    = 0.7f;
    int             priority       = 0;   // 0=normal, 1=high, 2=critical
    bool            useChain       = false;
    bool            useSwarm       = false;
    int             swarmFanout    = 4;
    std::string     reason;              // why this route was chosen
};

// ============================================================================
// ModelHealth — tracks per-model availability and latency
// ============================================================================
struct ModelHealth {
    std::string     modelId;
    std::atomic<int> successCount{0};
    std::atomic<int> failureCount{0};
    std::atomic<int> timeoutCount{0};
    std::atomic<int64_t> totalLatencyMs{0};
    std::chrono::steady_clock::time_point lastSuccess;
    std::chrono::steady_clock::time_point lastFailure;
    bool            available = true;

    float successRate() const {
        int total = successCount.load() + failureCount.load() + timeoutCount.load();
        if (total == 0) return 1.0f;
        return (float)successCount.load() / (float)total;
    }

    int avgLatencyMs() const {
        int s = successCount.load();
        if (s == 0) return 0;
        return (int)(totalLatencyMs.load() / s);
    }
};

// ============================================================================
// ModelProfile — capability descriptor for a model
// ============================================================================
struct ModelProfile {
    std::string id;             // e.g. "qwen2.5-coder:7b"
    std::string family;         // e.g. "qwen", "phi", "gemma"
    int         paramCountB;    // billions of params
    int         contextWindow;  // max context tokens
    float       codingScore;    // 0.0-1.0 strength at coding tasks
    float       reasoningScore; // 0.0-1.0 strength at reasoning
    float       speedScore;     // 0.0-1.0 (1.0 = fastest)
    int         vramMB;         // estimated VRAM usage
    bool        available;      // currently loaded / accessible

    float scoreFor(AgentMode mode) const {
        switch (mode) {
            case EDIT:
            case CODESUGGEST: return codingScore * 0.7f + speedScore * 0.3f;
            case PLAN:        return reasoningScore * 0.6f + codingScore * 0.2f + speedScore * 0.2f;
            case BUGREPORT:   return reasoningScore * 0.5f + codingScore * 0.5f;
            case ASK:
            default:          return reasoningScore * 0.4f + codingScore * 0.3f + speedScore * 0.3f;
        }
    }
};

// ============================================================================
// RateLimiter — sliding-window token bucket per model
// ============================================================================
struct RateLimiter {
    int             maxRequestsPerMinute = 60;
    std::deque<std::chrono::steady_clock::time_point> timestamps;
    std::mutex      mtx;

    bool tryAcquire() {
        std::lock_guard<std::mutex> lock(mtx);
        auto now = std::chrono::steady_clock::now();
        auto windowStart = now - std::chrono::seconds(60);
        while (!timestamps.empty() && timestamps.front() < windowStart)
            timestamps.pop_front();
        if ((int)timestamps.size() >= maxRequestsPerMinute) return false;
        timestamps.push_back(now);
        return true;
    }

    int currentLoad() {
        std::lock_guard<std::mutex> lock(mtx);
        auto now = std::chrono::steady_clock::now();
        auto windowStart = now - std::chrono::seconds(60);
        while (!timestamps.empty() && timestamps.front() < windowStart)
            timestamps.pop_front();
        return (int)timestamps.size();
    }
};

// ============================================================================
// Logging / Metrics injection
// ============================================================================
using RouterLogCallback    = std::function<void(int, const std::string&)>;
using RouterMetricCallback = std::function<void(const std::string&)>;

// ============================================================================
// AgentRouter — Production routing engine
// ============================================================================
class AgentRouter {
public:
    explicit AgentRouter(AgenticEngine* engine = nullptr)
        : m_engine(engine)
    {
        registerDefaultProfiles();
        registerKeywordRules();
    }

    void setEngine(AgenticEngine* engine) { m_engine = engine; }
    void setLogCallback(RouterLogCallback cb) { m_logCb = std::move(cb); }
    void setMetricCallback(RouterMetricCallback cb) { m_metricCb = std::move(cb); }

    // ──── Core Routing ────

    RouteResult route(const std::string& userInput, const std::string& context = "") {
        metric("router.route_calls");

        AgentMode mode = classifyMode(userInput);
        ModelProfile* best = selectModel(mode, userInput);

        RouteResult result;
        result.mode = mode;
        result.systemPrompt = buildSystemPrompt(mode, context);
        result.modelId = best ? best->id : "default";
        result.maxTokens = computeMaxTokens(mode, userInput);
        result.temperature = computeTemperature(mode);
        result.priority = computePriority(userInput);

        // Determine execution strategy
        auto strategy = selectStrategy(mode, userInput);
        result.useChain  = (strategy == "chain");
        result.useSwarm  = (strategy == "swarm");
        result.swarmFanout = result.useSwarm ? computeSwarmFanout(userInput) : 0;

        result.correlationId = generateCorrelationId();
        result.timestampMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        result.reason = "mode=" + modeToString(mode) +
                        " cid=" + result.correlationId +
                        " model=" + result.modelId +
                        " strategy=" + strategy +
                        " pri=" + std::to_string(result.priority);

        logInfo("Route: " + result.reason);
        metric("router.mode." + modeToString(mode));

        return result;
    }

    // ──── Mode Classification ────

    AgentMode classifyMode(const std::string& input) {
        std::string lower = toLower(input);

        // Priority score for each mode
        struct ModeScore { AgentMode mode; float score; };
        std::vector<ModeScore> scores;

        for (const auto& [mode, keywords] : m_keywordRules) {
            float score = 0.0f;
            for (const auto& kw : keywords) {
                if (lower.find(kw) != std::string::npos) {
                    score += 1.0f;
                    // Boost for early-position matches (intent signals)
                    size_t pos = lower.find(kw);
                    if (pos < 20) score += 0.5f;
                }
            }
            if (score > 0.0f) scores.push_back({mode, score});
        }

        if (scores.empty()) return ASK;

        std::sort(scores.begin(), scores.end(),
                  [](const ModeScore& a, const ModeScore& b) { return a.score > b.score; });

        return scores[0].mode;
    }

    // ──── Model Selection ────

    ModelProfile* selectModel(AgentMode mode, const std::string& input) {
        std::lock_guard<std::mutex> lock(m_profileMutex);

        ModelProfile* best = nullptr;
        float bestScore = -1.0f;

        for (auto& profile : m_profiles) {
            if (!profile.available) continue;

            // Check health
            auto hit = m_health.find(profile.id);
            if (hit != m_health.end() && hit->second.successRate() < 0.3f) {
                continue; // Skip unhealthy models
            }

            float score = profile.scoreFor(mode);

            // Bonus for input length vs context window
            int inputTokensEst = (int)(input.size() / 4);
            if (inputTokensEst > profile.contextWindow * 0.8f) {
                score *= 0.3f; // Penalize if input is too large for this model
            }

            // Bonus for VRAM fit
            if (profile.vramMB <= 12000) score += 0.1f; // Fits in single GPU easily

            if (score > bestScore) {
                bestScore = score;
                best = &profile;
            }
        }

        return best;
    }

    // ──── Execution Strategy Selection ────

    std::string selectStrategy(AgentMode mode, const std::string& input) {
        std::string lower = toLower(input);

        // Explicit strategy requests
        if (lower.find("in parallel") != std::string::npos ||
            lower.find("fan out") != std::string::npos ||
            lower.find("swarm") != std::string::npos)
            return "swarm";

        if (lower.find("step by step") != std::string::npos ||
            lower.find("chain") != std::string::npos ||
            lower.find("pipeline") != std::string::npos)
            return "chain";

        // Mode-based defaults
        switch (mode) {
            case PLAN:
                // Multi-step goals benefit from chaining
                if (input.size() > 200) return "chain";
                return "direct";
            case EDIT:
                // Bulk edits or "all files" → swarm
                if (lower.find("all files") != std::string::npos ||
                    lower.find("every file") != std::string::npos ||
                    lower.find("across the") != std::string::npos)
                    return "swarm";
                return "direct";
            case BUGREPORT:
                return "chain"; // Reproduce → Diagnose → Fix → Verify
            case CODESUGGEST:
                return "direct";
            default:
                return "direct";
        }
    }

    // ──── System Prompt Builder ────

    std::string buildSystemPrompt(AgentMode mode, const std::string& context = "") {
        std::string prompt;

        switch (mode) {
            case PLAN:
                prompt = "You are an expert software architect and planner. "
                         "Break down complex goals into precise, actionable steps. "
                         "For each step, specify the action type (file_edit, search, build, test, command) "
                         "and all necessary parameters. Produce a JSON array of steps. "
                         "Never skip validation or testing steps.";
                break;
            case EDIT:
                prompt = "You are a senior code editing agent specialized in precise, minimal, correct edits. "
                         "Always preserve existing functionality unless explicitly asked to change it. "
                         "Show exact file paths and line numbers. Use search-and-replace style edits. "
                         "After edits, verify the code compiles and tests pass.";
                break;
            case BUGREPORT:
                prompt = "You are a senior debugging agent. Systematically analyze the bug: "
                         "1) Reproduce the issue, 2) Identify root cause via code analysis, "
                         "3) Propose a minimal fix, 4) Verify the fix doesn't introduce regressions. "
                         "Always show evidence (file paths, line numbers, stack traces).";
                break;
            case CODESUGGEST:
                prompt = "You are a code review and refactoring agent. "
                         "Analyze code quality metrics (complexity, duplication, coupling). "
                         "Propose refactoring suggestions ranked by impact. "
                         "Preserve public API contracts unless explicitly asked to change them. "
                         "Show before/after diffs for each suggestion.";
                break;
            case ASK:
            default:
                prompt = "You are a helpful, expert software engineering assistant. "
                         "Provide accurate, concise answers grounded in the user's codebase context. "
                         "When referencing code, always cite file paths and line numbers.";
                break;
        }

        // Inject workspace context if available
        if (!context.empty()) {
            prompt += "\n\nWorkspace context:\n" + context;
        }

        return prompt;
    }

    // ──── Health Tracking ────

    void recordSuccess(const std::string& modelId, int latencyMs) {
        std::lock_guard<std::mutex> lock(m_healthMutex);
        auto& h = m_health[modelId];
        h.modelId = modelId;
        h.successCount++;
        h.totalLatencyMs += latencyMs;
        h.lastSuccess = std::chrono::steady_clock::now();
        h.available = true;
        metric("router.model_success." + modelId);
    }

    void recordFailure(const std::string& modelId, bool isTimeout = false) {
        std::lock_guard<std::mutex> lock(m_healthMutex);
        auto& h = m_health[modelId];
        h.modelId = modelId;
        if (isTimeout) h.timeoutCount++;
        else h.failureCount++;
        h.lastFailure = std::chrono::steady_clock::now();

        // Circuit breaker: disable model if too many recent failures
        int total = h.successCount.load() + h.failureCount.load() + h.timeoutCount.load();
        if (total >= 5 && h.successRate() < 0.2f) {
            h.available = false;
            logWarn("Circuit breaker OPEN for model: " + modelId +
                    " (success rate: " + std::to_string(h.successRate()) + ")");
            metric("router.circuit_breaker." + modelId);
        }

        metric("router.model_failure." + modelId);
    }

    void resetHealth(const std::string& modelId) {
        std::lock_guard<std::mutex> lock(m_healthMutex);
        m_health.erase(modelId);
    }

    // ──── Fallback Chains ────

    std::vector<std::string> getFallbackChain(const std::string& modelId) {
        std::lock_guard<std::mutex> lock(m_profileMutex);
        std::vector<std::string> chain;
        chain.push_back(modelId);

        // Find same-family fallbacks, then cross-family
        std::string family;
        for (const auto& p : m_profiles) {
            if (p.id == modelId) { family = p.family; break; }
        }

        // Same family, different size
        for (const auto& p : m_profiles) {
            if (p.id != modelId && p.family == family && p.available) {
                chain.push_back(p.id);
            }
        }

        // Cross-family fallbacks
        for (const auto& p : m_profiles) {
            if (p.family != family && p.available) {
                chain.push_back(p.id);
            }
        }

        return chain;
    }

    // ──── Model Registration ────

    void registerModel(const ModelProfile& profile) {
        std::lock_guard<std::mutex> lock(m_profileMutex);
        // Replace if exists
        for (auto& p : m_profiles) {
            if (p.id == profile.id) { p = profile; return; }
        }
        m_profiles.push_back(profile);
    }

    void setModelAvailable(const std::string& modelId, bool available) {
        std::lock_guard<std::mutex> lock(m_profileMutex);
        for (auto& p : m_profiles) {
            if (p.id == modelId) { p.available = available; return; }
        }
    }

    // ──── Dispatch (high-level: route + execute) ────

    std::string dispatch(const std::string& userInput, const std::string& context = "") {
        RouteResult route_res = route(userInput, context);
        metric("router.dispatch_total");

        if (!m_engine) {
            logError("AgentRouter::dispatch — no engine set");
            return "[Router Error] No AgenticEngine available";
        }

        // Apply route config
        AgenticEngine::GenerationConfig genCfg;
        genCfg.maxTokens = route_res.maxTokens;
        genCfg.temperature = route_res.temperature;
        m_engine->updateConfig(genCfg);

        auto startTime = std::chrono::steady_clock::now();
        std::string result;

        // Execute with fallback chain
        auto fallbacks = getFallbackChain(route_res.modelId);
        bool success = false;

        for (const auto& modelId : fallbacks) {
            const int maxRetries = 2;
            for (int attempt = 0; attempt <= maxRetries; attempt++) {
                if (attempt > 0) {
                    int backoffMs = 100 * (1 << attempt); // 200ms, 400ms
                    std::this_thread::sleep_for(std::chrono::milliseconds(backoffMs));
                    logInfo("Retry " + std::to_string(attempt) + "/" +
                            std::to_string(maxRetries) + " on " + modelId);
                    metric("router.retry." + modelId);
                }
                if (!checkRateLimit(modelId)) {
                    logWarn("Rate limit hit for " + modelId);
                    metric("router.rate_limited." + modelId);
                    break; // Move to next fallback model
                }
                try {
                    std::string fullPrompt = route_res.systemPrompt + "\n\nUser: " + userInput;

                    if (route_res.useSwarm) {
                        std::vector<std::string> subPrompts = splitForSwarm(userInput, route_res.swarmFanout);
                        result = m_engine->executeSwarm(subPrompts, "summarize", route_res.swarmFanout);
                    } else if (route_res.useChain) {
                        std::vector<std::string> steps = decomposeForChain(userInput, route_res.mode);
                        result = m_engine->executeChain(steps, userInput);
                    } else {
                        result = m_engine->chat(fullPrompt);
                    }

                    if (!result.empty() && result.find("[Error]") == std::string::npos) {
                        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - startTime).count();
                        recordSuccess(modelId, (int)elapsed);
                        success = true;
                        break;
                    }
                } catch (const std::exception& e) {
                    logWarn("Model " + modelId + " attempt " +
                            std::to_string(attempt) + " failed: " + e.what());
                    if (attempt == maxRetries) recordFailure(modelId);
                }
            }
            if (success) break;
        }

        if (!success) {
            metric("router.dispatch_all_fallbacks_exhausted");
            logError("All fallback models exhausted for input: " +
                     userInput.substr(0, 80) + "...");
            return "[Router Error] All model fallbacks exhausted. Last result: " + result;
        }

        metric("router.dispatch_success");
        return result;
    }

    // ──── Status / Diagnostics ────

    std::string getStatusJSON() const {
        std::ostringstream oss;
        oss << "{\"models\":[";
        {
            std::lock_guard<std::mutex> lock(m_profileMutex);
            for (size_t i = 0; i < m_profiles.size(); i++) {
                if (i > 0) oss << ",";
                const auto& p = m_profiles[i];
                oss << "{\"id\":\"" << p.id << "\""
                    << ",\"family\":\"" << p.family << "\""
                    << ",\"params\":" << p.paramCountB
                    << ",\"available\":" << (p.available ? "true" : "false")
                    << ",\"codingScore\":" << p.codingScore
                    << ",\"reasoningScore\":" << p.reasoningScore
                    << ",\"speedScore\":" << p.speedScore;

                std::lock_guard<std::mutex> hlock(m_healthMutex);
                auto hit = m_health.find(p.id);
                if (hit != m_health.end()) {
                    oss << ",\"successRate\":" << hit->second.successRate()
                        << ",\"avgLatencyMs\":" << hit->second.avgLatencyMs()
                        << ",\"totalCalls\":" << (hit->second.successCount.load() +
                                                  hit->second.failureCount.load() +
                                                  hit->second.timeoutCount.load());
                }
                oss << "}";
            }
        }
        oss << "],\"totalRoutes\":" << m_totalRoutes.load() << "}";
        return oss.str();
    }

    std::string modeToString(AgentMode mode) const {
        switch (mode) {
            case ASK:         return "ask";
            case PLAN:        return "plan";
            case EDIT:        return "edit";
            case BUGREPORT:   return "bugreport";
            case CODESUGGEST: return "codesuggest";
            default:          return "unknown";
        }
    }

    // ──── Health Probe ────

    struct HealthStatus {
        bool healthy = false;
        int availableModels = 0;
        int totalModels = 0;
        int totalRoutes = 0;
        std::string details;
    };

    HealthStatus healthProbe() const {
        HealthStatus hs;
        {
            std::lock_guard<std::mutex> lock(m_profileMutex);
            hs.totalModels = (int)m_profiles.size();
            for (const auto& p : m_profiles) {
                if (p.available) hs.availableModels++;
            }
        }
        hs.totalRoutes = m_totalRoutes.load();
        hs.healthy = hs.availableModels > 0;
        hs.details = "models=" + std::to_string(hs.availableModels) + "/" +
                     std::to_string(hs.totalModels) +
                     " routes=" + std::to_string(hs.totalRoutes);
        return hs;
    }

    // ──── Batch Routing ────

    std::vector<RouteResult> batchRoute(const std::vector<std::string>& inputs,
                                         const std::string& sharedContext = "") {
        std::vector<RouteResult> results;
        results.reserve(inputs.size());
        for (const auto& input : inputs) {
            results.push_back(route(input, sharedContext));
        }
        return results;
    }

    std::vector<std::string> batchDispatch(const std::vector<std::string>& inputs,
                                            const std::string& sharedContext = "") {
        std::vector<std::string> results;
        results.reserve(inputs.size());
        for (const auto& input : inputs) {
            results.push_back(dispatch(input, sharedContext));
        }
        return results;
    }

    // ──── Config Hot-Reload from JSON string ────

    bool loadConfigJSON(const std::string& json) {
        size_t arrStart = json.find('[');
        if (arrStart == std::string::npos) return false;
        std::lock_guard<std::mutex> lock(m_profileMutex);
        std::vector<ModelProfile> newProfiles;
        size_t pos = arrStart;
        while (true) {
            size_t objStart = json.find('{', pos);
            if (objStart == std::string::npos) break;
            size_t objEnd = json.find('}', objStart);
            if (objEnd == std::string::npos) break;
            std::string obj = json.substr(objStart, objEnd - objStart + 1);
            ModelProfile p;
            auto extractStr = [&](const std::string& key) -> std::string {
                std::string pattern = "\"" + key + "\":\"";
                size_t kp = obj.find(pattern);
                if (kp == std::string::npos) { pattern = "\"" + key + "\": \""; kp = obj.find(pattern); }
                if (kp == std::string::npos) return "";
                size_t vs = kp + pattern.size();
                size_t ve = obj.find('"', vs);
                return (ve != std::string::npos) ? obj.substr(vs, ve - vs) : "";
            };
            auto extractInt = [&](const std::string& key) -> int {
                std::string pattern = "\"" + key + "\":";
                size_t kp = obj.find(pattern);
                if (kp == std::string::npos) { pattern = "\"" + key + "\": "; kp = obj.find(pattern); }
                if (kp == std::string::npos) return 0;
                size_t ns = kp + pattern.size();
                while (ns < obj.size() && !std::isdigit(obj[ns]) && obj[ns] != '-') ns++;
                std::string num;
                while (ns < obj.size() && (std::isdigit(obj[ns]) || obj[ns] == '-')) num += obj[ns++];
                return num.empty() ? 0 : std::stoi(num);
            };
            auto extractFloat = [&](const std::string& key) -> float {
                std::string pattern = "\"" + key + "\":";
                size_t kp = obj.find(pattern);
                if (kp == std::string::npos) { pattern = "\"" + key + "\": "; kp = obj.find(pattern); }
                if (kp == std::string::npos) return 0.0f;
                size_t ns = kp + pattern.size();
                while (ns < obj.size() && !std::isdigit(obj[ns]) && obj[ns] != '-' && obj[ns] != '.') ns++;
                std::string num;
                while (ns < obj.size() && (std::isdigit(obj[ns]) || obj[ns] == '-' || obj[ns] == '.')) num += obj[ns++];
                return num.empty() ? 0.0f : std::stof(num);
            };
            p.id = extractStr("id");
            p.family = extractStr("family");
            p.paramCountB = extractInt("params");
            p.contextWindow = extractInt("contextWindow");
            p.codingScore = extractFloat("codingScore");
            p.reasoningScore = extractFloat("reasoningScore");
            p.speedScore = extractFloat("speedScore");
            p.vramMB = extractInt("vramMB");
            p.available = (obj.find("\"available\":false") == std::string::npos);
            if (!p.id.empty()) newProfiles.push_back(p);
            pos = objEnd + 1;
        }
        if (!newProfiles.empty()) {
            m_profiles = std::move(newProfiles);
            logInfo("Config reloaded: " + std::to_string(m_profiles.size()) + " models");
            return true;
        }
        return false;
    }

    // ──── Reset / Clear ────

    void resetAllHealth() {
        std::lock_guard<std::mutex> lock(m_healthMutex);
        m_health.clear();
    }

    void clearModels() {
        std::lock_guard<std::mutex> lock(m_profileMutex);
        m_profiles.clear();
    }

private:
    // ──── Internals ────

    void registerDefaultProfiles() {
        m_profiles = {
            {"phi:latest",            "phi",   2, 4096, 0.7f, 0.6f, 0.95f, 1600, true},
            {"gemma3:1b",             "gemma", 1, 8192, 0.5f, 0.5f, 0.98f, 800,  true},
            {"phi3:mini",             "phi",   4, 4096, 0.75f, 0.7f, 0.85f, 3800, true},
            {"qwen2.5-coder:latest",  "qwen",  7, 32768, 0.92f, 0.82f, 0.70f, 5500, true},
            {"qwen2.5:7b",            "qwen",  7, 32768, 0.80f, 0.85f, 0.72f, 5500, true},
            {"gpt-oss:20b",           "gpt",  20, 8192, 0.88f, 0.90f, 0.40f, 14000, true},
        };
    }

    void registerKeywordRules() {
        m_keywordRules[PLAN] = {
            "plan", "create a plan", "step by step", "break down", "decompose",
            "design", "architect", "roadmap", "strategy", "approach",
            "how should i", "what steps", "generate a plan"
        };
        m_keywordRules[EDIT] = {
            "edit", "change", "modify", "update", "fix the code",
            "implement", "add", "remove", "replace", "refactor",
            "write code", "create file", "create a file", "code for"
        };
        m_keywordRules[BUGREPORT] = {
            "bug", "error", "crash", "exception", "stack trace",
            "failing", "broken", "doesn't work", "not working",
            "debug", "diagnose", "segfault", "undefined", "null pointer"
        };
        m_keywordRules[CODESUGGEST] = {
            "suggest", "improve", "review", "refactor", "optimize",
            "code quality", "clean up", "simplify", "performance",
            "best practice", "code smell"
        };
    }

    int computeMaxTokens(AgentMode mode, const std::string& input) {
        int base;
        switch (mode) {
            case PLAN:        base = 4096; break;
            case EDIT:        base = 2048; break;
            case BUGREPORT:   base = 3072; break;
            case CODESUGGEST: base = 2048; break;
            default:          base = 2048; break;
        }
        // Scale up for long inputs
        if (input.size() > 2000) base = std::min(base + 1024, 8192);
        return base;
    }

    float computeTemperature(AgentMode mode) {
        switch (mode) {
            case PLAN:        return 0.4f;  // Structured, deterministic
            case EDIT:        return 0.2f;  // Precise edits
            case BUGREPORT:   return 0.3f;  // Analytical
            case CODESUGGEST: return 0.5f;  // Creative suggestions
            default:          return 0.7f;  // General
        }
    }

    int computePriority(const std::string& input) {
        std::string lower = toLower(input);
        if (lower.find("urgent") != std::string::npos ||
            lower.find("critical") != std::string::npos ||
            lower.find("asap") != std::string::npos ||
            lower.find("blocking") != std::string::npos)
            return 2;
        if (lower.find("important") != std::string::npos ||
            lower.find("priority") != std::string::npos)
            return 1;
        return 0;
    }

    int computeSwarmFanout(const std::string& input) {
        // Estimate number of parallel tasks from input
        int files = 0;
        for (size_t pos = 0; (pos = input.find("file", pos)) != std::string::npos; pos++) files++;
        if (files > 1) return std::min(files, 8);
        return 4; // Default fanout
    }

    std::vector<std::string> splitForSwarm(const std::string& input, int count) {
        std::vector<std::string> prompts;
        // Split numbered lists, paragraphs, or just replicate with task indices
        std::istringstream iss(input);
        std::string line;
        std::vector<std::string> lines;
        while (std::getline(iss, line)) {
            if (!line.empty()) lines.push_back(line);
        }

        if ((int)lines.size() >= count) {
            int chunkSize = (int)lines.size() / count;
            for (int i = 0; i < count; i++) {
                std::string chunk;
                int start = i * chunkSize;
                int end = (i == count - 1) ? (int)lines.size() : start + chunkSize;
                for (int j = start; j < end; j++) {
                    chunk += lines[j] + "\n";
                }
                prompts.push_back(chunk);
            }
        } else {
            // Fan-out: each worker gets the full prompt with a different focus
            for (int i = 0; i < count; i++) {
                prompts.push_back("Task " + std::to_string(i + 1) + "/" +
                                  std::to_string(count) + ": " + input);
            }
        }
        return prompts;
    }

    std::vector<std::string> decomposeForChain(const std::string& input, AgentMode mode) {
        std::vector<std::string> steps;
        switch (mode) {
            case PLAN:
                steps = {
                    "Analyze the goal and identify requirements:\n{{input}}",
                    "Create a detailed step-by-step execution plan from the analysis:\n{{input}}",
                    "Review the plan for completeness, add validation and testing steps:\n{{input}}"
                };
                break;
            case BUGREPORT:
                steps = {
                    "Reproduce and identify the bug. Search for related code and error patterns:\n{{input}}",
                    "Diagnose root cause. Trace through the code path and identify the exact failure point:\n{{input}}",
                    "Propose a minimal, correct fix. Show the exact code change needed:\n{{input}}",
                    "Verify: ensure the fix handles edge cases and doesn't break existing tests:\n{{input}}"
                };
                break;
            case EDIT:
                steps = {
                    "Understand the edit request and identify all affected files:\n{{input}}",
                    "Implement the changes with precise search-and-replace edits:\n{{input}}",
                    "Validate: check for compile errors, missing imports, broken references:\n{{input}}"
                };
                break;
            default:
                steps = {input};
                break;
        }
        return steps;
    }

    std::string toLower(const std::string& s) const {
        std::string r = s;
        std::transform(r.begin(), r.end(), r.begin(), ::tolower);
        return r;
    }

    void logInfo(const std::string& msg)  { if (m_logCb) m_logCb(1, "[Router] " + msg); }
    void logWarn(const std::string& msg)  { if (m_logCb) m_logCb(2, "[Router] " + msg); }
    void logError(const std::string& msg) { if (m_logCb) m_logCb(3, "[Router] " + msg); }
    void metric(const std::string& key)   { m_totalRoutes++; if (m_metricCb) m_metricCb(key); }

    AgenticEngine* m_engine = nullptr;
    RouterLogCallback m_logCb;
    RouterMetricCallback m_metricCb;

    std::vector<ModelProfile> m_profiles;
    mutable std::mutex m_profileMutex;

    std::unordered_map<std::string, ModelHealth> m_health;
    mutable std::mutex m_healthMutex;

    std::unordered_map<AgentMode, std::vector<std::string>> m_keywordRules;

    std::atomic<int> m_totalRoutes{0};
    std::atomic<uint64_t> m_correlationCounter{0};

    // Rate limiting
    std::unordered_map<std::string, std::unique_ptr<RateLimiter>> m_rateLimiters;
    std::mutex m_rateLimitMutex;

    std::string generateCorrelationId() {
        uint64_t seq = m_correlationCounter.fetch_add(1);
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        std::ostringstream oss;
        oss << std::hex << (now & 0xFFFFFFFF) << "-" << std::setw(4)
            << std::setfill('0') << (seq & 0xFFFF);
        return oss.str();
    }

    bool checkRateLimit(const std::string& modelId) {
        std::lock_guard<std::mutex> lock(m_rateLimitMutex);
        auto it = m_rateLimiters.find(modelId);
        if (it == m_rateLimiters.end()) {
            m_rateLimiters[modelId] = std::make_unique<RateLimiter>();
            it = m_rateLimiters.find(modelId);
        }
        return it->second->tryAcquire();
    }
};

// ============================================================================
// Legacy compat — build_system_prompt (original 13-line function)
// ============================================================================
std::string build_system_prompt(AgentMode mode) {
    static AgentRouter s_router;
    return s_router.buildSystemPrompt(mode);
}

// ============================================================================
// Global router instance (singleton)
// ============================================================================
static AgentRouter* g_routerInstance = nullptr;

AgentRouter& getGlobalRouter() {
    static AgentRouter instance;
    return instance;
}

void initGlobalRouter(AgenticEngine* engine) {
    auto& router = getGlobalRouter();
    router.setEngine(engine);
}

std::string routeAndDispatch(const std::string& input, const std::string& context) {
    return getGlobalRouter().dispatch(input, context);
}

RouteResult routeRequest(const std::string& input) {
    return getGlobalRouter().route(input);
}
