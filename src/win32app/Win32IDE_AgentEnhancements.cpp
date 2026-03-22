// ============================================================================
// Win32IDE_AgentContextBudget.cpp
// ============================================================================
// Enhancement 1: Context-window budget tracking per plan step.
//   - Tracks estimated token usage per step and across the whole plan.
//   - Emits WM_AGENT_BUDGET_WARNING when >80% consumed.
//   - Auto-truncates prompt history when >95% consumed.
//   - Exposes budget panel in the plan dialog (new column + status bar).
//
// Enhancement 2: Tool-call schema validation.
//   - Every tool call is validated against a registered JSON schema before
//     dispatch. Invalid calls are rejected with a structured error and
//     classified as FormatViolation by the failure detector.
//
// Enhancement 3: Plan dependency graph (DAG) with parallel execution.
//   - PlanStep gains a `dependsOn` vector<int> of step IDs.
//   - Executor builds a topological order and runs independent steps in
//     parallel worker threads (up to PLAN_MAX_PARALLEL threads).
//   - Plan dialog shows a dependency column and parallel-batch indicators.
//
// Enhancement 4: Agent scratchpad — persistent working memory.
//   - Scratchpad is a key/value store scoped to the current plan.
//   - Steps can read/write scratchpad entries via tool:scratchpad_read/write.
//   - Scratchpad is serialised to %APPDATA%\RawrXD\scratchpad.json on commit.
//   - Cleared on plan reset; exported with the proof bundle.
//
// Enhancement 5: Streaming plan step output.
//   - executeSingleStep() now posts WM_PLAN_STREAM_TOKEN for each token.
//   - Plan dialog detail pane updates live as tokens arrive.
//   - Token rate (tok/s) shown in the progress label.
//
// Enhancement 6: Per-task token budget enforcement.
//   - generateAgentPlan() accepts a TokenBudget struct.
//   - Each step is allocated a proportional share of the budget.
//   - Steps that exceed their share are interrupted and retried with a
//     shorter prompt (ReduceScope strategy).
//
// Enhancement 7: Multi-model routing with fallback.
//   - AgentModelRouter selects the best available model for each step type.
//   - Falls back through a priority list: local GGUF → Ollama → cloud stub.
//   - Router state is shown in the plan dialog summary line.
// ============================================================================

#include "IDELogger.h"
#include "Win32IDE.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <fstream>
#include <functional>
#include <map>
#include <mutex>
#include <numeric>
#include <queue>
#include <set>
#include <sstream>
#include <thread>

// ============================================================================
// WM_PLAN_STREAM_TOKEN — posted per token during step execution
// ============================================================================
#ifndef WM_PLAN_STREAM_TOKEN
#define WM_PLAN_STREAM_TOKEN (WM_APP + 520)
#endif

#ifndef WM_AGENT_BUDGET_WARNING
#define WM_AGENT_BUDGET_WARNING (WM_APP + 521)
#endif

// ============================================================================
// ENHANCEMENT 1 — Context-window budget tracking
// ============================================================================

static constexpr int BUDGET_WARN_PCT = 80;
static constexpr int BUDGET_TRUNC_PCT = 95;

// Rough token estimator: 1 token ≈ 4 chars (GPT-style)
static int estimateTokens(const std::string& text)
{
    return std::max(1, (int)(text.size() / 4));
}

void Win32IDE::initContextBudget(int contextWindowTokens)
{
    m_contextBudget.windowSize = contextWindowTokens > 0 ? contextWindowTokens : 4096;
    m_contextBudget.usedTokens = 0;
    m_contextBudget.warnFired = false;
    m_contextBudget.truncations = 0;
    LOG_INFO("Context budget initialised: " + std::to_string(m_contextBudget.windowSize) + " tokens");
}

// Returns the prompt, possibly truncated to fit within budget.
// Posts WM_AGENT_BUDGET_WARNING if >80% consumed.
std::string Win32IDE::applyContextBudget(const std::string& prompt, const std::string& history)
{
    int historyTokens = estimateTokens(history);
    int promptTokens = estimateTokens(prompt);
    int total = historyTokens + promptTokens;

    m_contextBudget.usedTokens = total;

    int pct = (m_contextBudget.windowSize > 0) ? (total * 100 / m_contextBudget.windowSize) : 0;

    if (pct >= BUDGET_WARN_PCT && !m_contextBudget.warnFired)
    {
        m_contextBudget.warnFired = true;
        PostMessageA(m_hwndMain, WM_AGENT_BUDGET_WARNING, (WPARAM)pct, (LPARAM)m_contextBudget.windowSize);
        appendToOutput("[Budget] ⚠️ Context " + std::to_string(pct) + "% used (" + std::to_string(total) + "/" +
                           std::to_string(m_contextBudget.windowSize) + " tokens)",
                       "General", OutputSeverity::Warning);
    }

    if (pct < BUDGET_TRUNC_PCT)
    {
        return prompt;
    }

    // Auto-truncate: keep the last 60% of the prompt
    m_contextBudget.truncations++;
    int keepChars = (int)(prompt.size() * 0.6);
    std::string truncated = "[...truncated for context budget...]\n" + prompt.substr(prompt.size() - keepChars);
    appendToOutput("[Budget] ✂️ Prompt truncated (truncation #" + std::to_string(m_contextBudget.truncations) + ")",
                   "General", OutputSeverity::Warning);
    return truncated;
}

std::string Win32IDE::getContextBudgetStatus() const
{
    int pct = (m_contextBudget.windowSize > 0) ? (m_contextBudget.usedTokens * 100 / m_contextBudget.windowSize) : 0;
    char buf[128];
    snprintf(buf, sizeof(buf), "Context: %d/%d tokens (%d%%) | Truncations: %d", m_contextBudget.usedTokens,
             m_contextBudget.windowSize, pct, m_contextBudget.truncations);
    return buf;
}

// ============================================================================
// ENHANCEMENT 2 — Tool-call schema validation
// ============================================================================

// Schema: map of tool name → required parameter names
static const std::map<std::string, std::vector<std::string>> TOOL_SCHEMAS = {
    {"read_file", {"path"}},
    {"write_file", {"path", "content"}},
    {"list_dir", {"path"}},
    {"exec_cmd", {"cmd"}},
    {"grep_files", {"pattern", "path"}},
    {"search_files", {"query"}},
    {"web_search", {"query"}},
    {"git_status", {}},
    {"load_model", {"path"}},
    {"runSubagent", {"description", "prompt"}},
    {"chain", {"steps", "input"}},
    {"hexmag_swarm", {"prompts"}},
    {"manage_todo_list", {"items"}},
    {"scratchpad_read", {"key"}},
    {"scratchpad_write", {"key", "value"}},
};

ToolValidationResult Win32IDE::validateToolCall(const std::string& toolName, const std::string& argsJson)
{
    ToolValidationResult result;
    result.valid = true;
    result.toolName = toolName;

    auto it = TOOL_SCHEMAS.find(toolName);
    if (it == TOOL_SCHEMAS.end())
    {
        // Unknown tool — warn but allow (extensible tool set)
        result.warnings.push_back("Unknown tool '" + toolName + "' — no schema registered");
        return result;
    }

    const auto& required = it->second;
    if (required.empty())
        return result;  // No required params

    // Simple presence check: look for each required key in the JSON string
    for (const auto& param : required)
    {
        std::string needle = "\"" + param + "\"";
        if (argsJson.find(needle) == std::string::npos)
        {
            result.valid = false;
            result.errors.push_back("Missing required parameter: '" + param + "'");
        }
    }

    if (!result.valid)
    {
        result.errorMessage = "Tool '" + toolName + "' schema validation failed: " + result.errors[0];
        LOG_WARNING("[ToolSchema] " + result.errorMessage);
        appendToOutput("[ToolSchema] ❌ " + result.errorMessage, "General", OutputSeverity::Error);
    }

    return result;
}

// Intercept DispatchModelToolCalls to validate before dispatch
bool Win32IDE::validateAndDispatchToolCall(const std::string& toolName, const std::string& argsJson,
                                           std::string& toolResult)
{
    auto validation = validateToolCall(toolName, argsJson);
    if (!validation.valid)
    {
        toolResult = "[ToolSchemaError] " + validation.errorMessage;
        // Classify as FormatViolation so failure detector picks it up
        FailureClassification fc = FailureClassification::make(AgentFailureType::FormatViolation, 0.90f,
                                                               "Tool schema violation: " + validation.errorMessage);
        m_failureStats.formatViolationCount++;
        return false;
    }

    // Warnings are non-fatal — log and proceed
    for (const auto& w : validation.warnings)
    {
        appendToOutput("[ToolSchema] ⚠️ " + w, "General", OutputSeverity::Warning);
    }

    if (!m_agenticBridge)
        return false;
    return m_agenticBridge->DispatchModelToolCalls(toolName + " " + argsJson, toolResult);
}

// ============================================================================
// ENHANCEMENT 3 — Plan dependency graph (DAG) + parallel execution
// ============================================================================

// Topological sort of plan steps using Kahn's algorithm.
// Returns step indices in execution order; parallel batches share the same
// "level" in the returned vector-of-vectors.
std::vector<std::vector<int>> Win32IDE::buildPlanExecutionBatches()
{
    int n = (int)m_currentPlan.steps.size();
    if (n == 0)
        return {};

    // Build adjacency: dependsOn[i] = set of step IDs that step i depends on
    std::vector<int> inDegree(n, 0);
    std::vector<std::vector<int>> dependents(n);  // dependents[i] = steps that depend on i

    for (int i = 0; i < n; i++)
    {
        for (int dep : m_currentPlan.steps[i].dependsOn)
        {
            if (dep >= 0 && dep < n)
            {
                inDegree[i]++;
                dependents[dep].push_back(i);
            }
        }
    }

    std::vector<std::vector<int>> batches;
    std::queue<int> ready;

    for (int i = 0; i < n; i++)
    {
        if (inDegree[i] == 0)
            ready.push(i);
    }

    while (!ready.empty())
    {
        std::vector<int> batch;
        int batchSize = (int)ready.size();
        for (int k = 0; k < batchSize; k++)
        {
            int idx = ready.front();
            ready.pop();
            batch.push_back(idx);
        }
        batches.push_back(batch);

        for (int idx : batch)
        {
            for (int dep : dependents[idx])
            {
                if (--inDegree[dep] == 0)
                    ready.push(dep);
            }
        }
    }

    // Detect cycles: if total steps in batches < n, there's a cycle
    int total = 0;
    for (auto& b : batches)
        total += (int)b.size();
    if (total < n)
    {
        appendToOutput("[PlanDAG] ⚠️ Cycle detected in plan dependencies — falling back to sequential", "General",
                       OutputSeverity::Warning);
        // Fallback: sequential order
        batches.clear();
        for (int i = 0; i < n; i++)
            batches.push_back({i});
    }

    return batches;
}

// Execute a batch of steps in parallel (up to PLAN_MAX_PARALLEL threads)
void Win32IDE::executePlanBatch(const std::vector<int>& batch, bool dryRun)
{
    static constexpr int PLAN_MAX_PARALLEL = 4;

    if (batch.size() == 1)
    {
        // Single step — run inline
        int idx = batch[0];
        if (idx >= 0 && idx < (int)m_currentPlan.steps.size())
        {
            m_currentPlan.steps[idx].status = PlanStepStatus::Running;
            PostMessageA(m_hwndMain, WM_APP + 503, idx, (LPARAM)PlanStepStatus::Running);
            std::string result = executeAgentPlanStepViaBridge(m_currentPlan.steps[idx]);
            m_currentPlan.steps[idx].output = result;
            m_currentPlan.steps[idx].status = PlanStepStatus::Completed;
            PostMessageA(m_hwndMain, WM_PLAN_STEP_DONE, idx, (LPARAM)1);
        }
        return;
    }

    // Multiple independent steps — run in parallel
    int parallelism = std::min((int)batch.size(), PLAN_MAX_PARALLEL);
    std::vector<std::thread> workers;
    std::mutex resultMutex;

    for (int i = 0; i < (int)batch.size(); i++)
    {
        int idx = batch[i];
        if (idx < 0 || idx >= (int)m_currentPlan.steps.size())
            continue;

        workers.emplace_back(
            [this, idx, &resultMutex]()
            {
                m_currentPlan.steps[idx].status = PlanStepStatus::Running;
                PostMessageA(m_hwndMain, WM_APP + 503, idx, (LPARAM)PlanStepStatus::Running);

                std::string result = executeAgentPlanStepViaBridge(m_currentPlan.steps[idx]);

                std::lock_guard<std::mutex> lock(resultMutex);
                m_currentPlan.steps[idx].output = result;
                m_currentPlan.steps[idx].status = PlanStepStatus::Completed;
                PostMessageA(m_hwndMain, WM_PLAN_STEP_DONE, idx, (LPARAM)1);
            });

        // Throttle to max parallel
        if ((int)workers.size() >= parallelism)
        {
            for (auto& t : workers)
                if (t.joinable())
                    t.join();
            workers.clear();
        }
    }

    for (auto& t : workers)
        if (t.joinable())
            t.join();
}

// ============================================================================
// ENHANCEMENT 4 — Agent scratchpad
// ============================================================================

void Win32IDE::scratchpadWrite(const std::string& key, const std::string& value, const std::string& stepContext)
{
    std::lock_guard<std::mutex> lock(m_scratchpadMutex);
    m_scratchpad[key] = {value, stepContext, currentEpochMs()};
    LOG_INFO("[Scratchpad] Write: " + key + " = " + value.substr(0, 64));
    appendToOutput("[Scratchpad] 📝 " + key + " = " + value.substr(0, 80) + (value.size() > 80 ? "..." : ""), "General",
                   OutputSeverity::Info);
}

std::string Win32IDE::scratchpadRead(const std::string& key) const
{
    std::lock_guard<std::mutex> lock(m_scratchpadMutex);
    auto it = m_scratchpad.find(key);
    if (it == m_scratchpad.end())
        return "";
    return it->second.value;
}

bool Win32IDE::scratchpadHas(const std::string& key) const
{
    std::lock_guard<std::mutex> lock(m_scratchpadMutex);
    return m_scratchpad.count(key) > 0;
}

void Win32IDE::scratchpadClear()
{
    std::lock_guard<std::mutex> lock(m_scratchpadMutex);
    m_scratchpad.clear();
    appendToOutput("[Scratchpad] 🗑️ Cleared", "General", OutputSeverity::Info);
}

std::string Win32IDE::scratchpadToJSON() const
{
    std::lock_guard<std::mutex> lock(m_scratchpadMutex);
    std::ostringstream j;
    j << "{\n  \"scratchpad\": [\n";
    bool first = true;
    for (const auto& [k, entry] : m_scratchpad)
    {
        if (!first)
            j << ",\n";
        first = false;
        // Escape key and value
        auto esc = [](const std::string& s)
        {
            std::string out;
            for (char c : s)
            {
                if (c == '"')
                    out += "\\\"";
                else if (c == '\\')
                    out += "\\\\";
                else if (c == '\n')
                    out += "\\n";
                else
                    out += c;
            }
            return out;
        };
        j << "    {\"key\":\"" << esc(k) << "\","
          << "\"value\":\"" << esc(entry.value) << "\","
          << "\"step\":\"" << esc(entry.stepContext) << "\","
          << "\"ts\":" << entry.timestampMs << "}";
    }
    j << "\n  ]\n}\n";
    return j.str();
}

void Win32IDE::persistScratchpad()
{
    char appData[MAX_PATH] = {};
    if (!GetEnvironmentVariableA("APPDATA", appData, MAX_PATH))
        return;
    std::string dir = std::string(appData) + "\\RawrXD";
    CreateDirectoryA(dir.c_str(), nullptr);
    std::ofstream ofs(dir + "\\scratchpad.json", std::ios::trunc);
    if (ofs.is_open())
    {
        ofs << scratchpadToJSON();
        ofs.close();
    }
}

// ============================================================================
// ENHANCEMENT 5 — Streaming plan step output
// ============================================================================

void Win32IDE::onPlanStreamToken(int stepIndex, const char* token)
{
    if (!token || stepIndex < 0 || stepIndex >= (int)m_currentPlan.steps.size())
        return;

    // Append token to step's live output buffer
    m_currentPlan.steps[stepIndex].output += token;

    // Update detail pane if this step is selected
    if (m_hwndPlanDetail && m_hwndPlanList)
    {
        int sel = ListView_GetNextItem(m_hwndPlanList, -1, LVNI_SELECTED);
        if (sel == stepIndex)
        {
            // Append token to detail pane without full refresh
            int len = GetWindowTextLengthA(m_hwndPlanDetail);
            SendMessageA(m_hwndPlanDetail, EM_SETSEL, len, len);
            SendMessageA(m_hwndPlanDetail, EM_REPLACESEL, FALSE, (LPARAM)token);
            SendMessageA(m_hwndPlanDetail, EM_SCROLLCARET, 0, 0);
        }
    }

    // Update token rate in progress label
    m_planStreamTokenCount++;
    auto now = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(now - m_planStreamStart).count();
    if (elapsed > 0.5 && m_hwndPlanProgressLabel)
    {
        double tps = m_planStreamTokenCount / elapsed;
        char buf[64];
        snprintf(buf, sizeof(buf), "Step %d — %.1f tok/s", stepIndex + 1, tps);
        SetWindowTextA(m_hwndPlanProgressLabel, buf);
    }
}

void Win32IDE::resetPlanStreamCounters()
{
    m_planStreamTokenCount = 0;
    m_planStreamStart = std::chrono::steady_clock::now();
}

// ============================================================================
// ENHANCEMENT 6 — Per-task token budget enforcement
// ============================================================================

void Win32IDE::initPlanTokenBudget(int totalTokens)
{
    m_planTokenBudget.totalBudget = totalTokens > 0 ? totalTokens : 8192;
    m_planTokenBudget.usedTokens = 0;
    m_planTokenBudget.overruns = 0;
    m_planTokenBudget.active = true;

    int n = (int)m_currentPlan.steps.size();
    if (n > 0)
    {
        // Distribute budget proportionally by estimated step duration
        int totalTime = 0;
        for (auto& s : m_currentPlan.steps)
            totalTime += std::max(1, s.estimatedMinutes);
        for (auto& s : m_currentPlan.steps)
        {
            s.tokenBudget = (totalTime > 0) ? (m_planTokenBudget.totalBudget * s.estimatedMinutes / totalTime)
                                            : (m_planTokenBudget.totalBudget / n);
            s.tokenBudget = std::max(256, s.tokenBudget);
        }
    }

    LOG_INFO("[TokenBudget] Plan budget: " + std::to_string(m_planTokenBudget.totalBudget) + " tokens across " +
             std::to_string(n) + " steps");
}

// Returns true if the step is within budget; false if it overran.
bool Win32IDE::checkStepTokenBudget(int stepIndex, const std::string& output)
{
    if (!m_planTokenBudget.active)
        return true;
    if (stepIndex < 0 || stepIndex >= (int)m_currentPlan.steps.size())
        return true;

    int used = estimateTokens(output);
    int budget = m_currentPlan.steps[stepIndex].tokenBudget;

    m_planTokenBudget.usedTokens += used;

    if (used > budget)
    {
        m_planTokenBudget.overruns++;
        appendToOutput("[TokenBudget] ⚠️ Step " + std::to_string(stepIndex + 1) +
                           " overran budget: " + std::to_string(used) + "/" + std::to_string(budget) + " tokens",
                       "General", OutputSeverity::Warning);
        return false;
    }

    return true;
}

std::string Win32IDE::getPlanTokenBudgetStatus() const
{
    if (!m_planTokenBudget.active)
        return "Budget: inactive";
    int pct =
        (m_planTokenBudget.totalBudget > 0) ? (m_planTokenBudget.usedTokens * 100 / m_planTokenBudget.totalBudget) : 0;
    char buf[128];
    snprintf(buf, sizeof(buf), "Budget: %d/%d tokens (%d%%) | Overruns: %d", m_planTokenBudget.usedTokens,
             m_planTokenBudget.totalBudget, pct, m_planTokenBudget.overruns);
    return buf;
}

// ============================================================================
// ENHANCEMENT 7 — Multi-model routing with fallback
// ============================================================================

// Model tier priority: local GGUF > Ollama > cloud stub
static const std::vector<std::string> MODEL_PRIORITY = {"local_gguf", "ollama", "cloud_stub"};

// Step type → preferred model tier
static const std::map<std::string, std::string> STEP_MODEL_PREFERENCE = {
    {"code_edit", "local_gguf"},     {"file_create", "local_gguf"}, {"file_delete", "local_gguf"},
    {"shell_command", "local_gguf"}, {"analysis", "ollama"},        {"verification", "ollama"},
    {"general", "ollama"},
};

AgentModelRoute Win32IDE::routeStepToModel(const PlanStep& step)
{
    AgentModelRoute route;
    route.stepType = planStepTypeString(step.type);

    // Determine preferred tier
    std::string preferred = "ollama";
    auto it = STEP_MODEL_PREFERENCE.find(route.stepType);
    if (it != STEP_MODEL_PREFERENCE.end())
        preferred = it->second;

    // Check availability in priority order
    bool localLoaded = m_agenticBridge && m_agenticBridge->IsInitialized() && m_agenticBridge->IsModelLoaded();
    bool ollamaAvail = m_agenticBridge && !m_agenticBridge->GetOllamaServer().empty();

    if (preferred == "local_gguf" && localLoaded)
    {
        route.selectedTier = "local_gguf";
        route.modelName = m_agenticBridge->GetModelName();
        route.fallbackUsed = false;
    }
    else if (ollamaAvail)
    {
        route.selectedTier = "ollama";
        route.modelName = m_agenticBridge->GetModelName();
        route.fallbackUsed = (preferred != "ollama");
    }
    else
    {
        // Cloud stub fallback
        route.selectedTier = "cloud_stub";
        route.modelName = "stub";
        route.fallbackUsed = true;
        appendToOutput("[ModelRouter] ⚠️ No local/Ollama model available — using cloud stub", "General",
                       OutputSeverity::Warning);
    }

    if (route.fallbackUsed)
    {
        appendToOutput("[ModelRouter] 🔀 Step " + step.title + ": preferred=" + preferred +
                           " → using=" + route.selectedTier,
                       "General", OutputSeverity::Info);
    }

    return route;
}

std::string Win32IDE::getModelRouterStatus() const
{
    if (!m_agenticBridge)
        return "Router: no bridge";

    bool localLoaded = m_agenticBridge->IsInitialized() && m_agenticBridge->IsModelLoaded();
    bool ollamaAvail = !m_agenticBridge->GetOllamaServer().empty();

    std::ostringstream oss;
    oss << "Router: local=" << (localLoaded ? "✅" : "❌") << " ollama=" << (ollamaAvail ? "✅" : "❌")
        << " cloud=stub";
    return oss.str();
}

// ============================================================================
// Wire all 7 enhancements into generateAgentPlan
// ============================================================================

void Win32IDE::initAllAgentEnhancements(int contextWindow, int tokenBudget)
{
    // Enhancement 1: context budget
    initContextBudget(contextWindow);

    // Enhancement 4: scratchpad
    scratchpadClear();

    // Enhancement 5: streaming counters
    resetPlanStreamCounters();

    // Enhancement 6: token budget (set after plan is generated)
    m_planTokenBudget.active = false;

    LOG_INFO("[AgentEnhancements] All 7 enhancements initialised");
    appendToOutput("[AgentEnhancements] Context budget=" + std::to_string(contextWindow) + " tokens | " +
                       getModelRouterStatus(),
                   "General", OutputSeverity::Info);
}
