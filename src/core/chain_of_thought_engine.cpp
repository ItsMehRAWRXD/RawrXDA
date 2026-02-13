// ============================================================================
// chain_of_thought_engine.cpp — Chain-of-Thought Multi-Model Review Engine
// Phase 32A: Backend implementation
//
// Sequential multi-step reasoning chain with 12 roles, 6 presets, 1–8 steps.
// Each step receives the original query + all prior step outputs.
//
// NO exceptions. Structured results. Thread-safe.
// ============================================================================

#include "../include/chain_of_thought_engine.h"
#include <chrono>
#include <sstream>
#include <algorithm>
#include <cstring>

// ============================================================================
// Static role definitions — mirrors the JS COT_ROLES
// ============================================================================
static const std::vector<CoTRoleInfo>& buildRoleTable() {
    static std::vector<CoTRoleInfo> roles = {
        { CoTRoleId::Reviewer, "reviewer", "Reviewer", "[R]",
          "You are a code reviewer. Analyze the following carefully, identify issues, suggest improvements." },
        { CoTRoleId::Auditor, "auditor", "Auditor", "[A]",
          "You are a security/quality auditor. Check for vulnerabilities, correctness issues, edge cases, and compliance." },
        { CoTRoleId::Thinker, "thinker", "Thinker", "[T]",
          "You are a deep thinker. Reason step-by-step through the problem, consider alternatives, and explain your reasoning." },
        { CoTRoleId::Researcher, "researcher", "Researcher", "[Rs]",
          "You are a research assistant. Gather relevant context, find patterns, cross-reference information, and cite sources." },
        { CoTRoleId::DebaterFor, "debater_for", "Argue For", "[D+]",
          "You argue IN FAVOR of the proposed approach. Present the strongest possible case for why this is correct/optimal." },
        { CoTRoleId::DebaterAgainst, "debater_against", "Argue Against", "[D-]",
          "You argue AGAINST the proposed approach. Present the strongest possible counterarguments and alternatives." },
        { CoTRoleId::Critic, "critic", "Critic", "[Cr]",
          "You are a harsh critic. Find every flaw, weakness, and edge case. Be thorough and unforgiving." },
        { CoTRoleId::Synthesizer, "synthesizer", "Synthesizer", "[Sy]",
          "You are a synthesizer. Combine all previous analyses into a coherent, actionable final answer. Resolve conflicts and present the best path forward." },
        { CoTRoleId::Brainstorm, "brainstorm", "Brainstorm", "[B]",
          "You are a creative brainstormer. Generate multiple diverse approaches and ideas without filtering." },
        { CoTRoleId::Verifier, "verifier", "Verifier", "[V]",
          "You are a verifier. Check all previous claims for accuracy. Flag anything unverified or incorrect." },
        { CoTRoleId::Refiner, "refiner", "Refiner", "[Rf]",
          "You are a refiner. Take the previous output and improve its clarity, correctness, and completeness." },
        { CoTRoleId::Summarizer, "summarizer", "Summarizer", "[Su]",
          "You are a summarizer. Distill everything into a concise, actionable summary." },
    };
    return roles;
}

const CoTRoleInfo& getCoTRoleInfo(CoTRoleId id) {
    const auto& roles = buildRoleTable();
    int idx = static_cast<int>(id);
    if (idx >= 0 && idx < (int)roles.size()) return roles[idx];
    return roles[0]; // fallback to Reviewer
}

const CoTRoleInfo* getCoTRoleByName(const std::string& name) {
    const auto& roles = buildRoleTable();
    for (const auto& r : roles) {
        if (r.name == name) return &r;
    }
    return nullptr;
}

const std::vector<CoTRoleInfo>& getAllCoTRoles() {
    return buildRoleTable();
}

// ============================================================================
// Static preset definitions — mirrors the JS COT_PRESETS
// ============================================================================
static const std::vector<CoTPreset>& buildPresetTable() {
    static std::vector<CoTPreset> presets;
    static bool built = false;
    if (!built) {
        built = true;

        // review: reviewer → critic → synthesizer
        presets.push_back({ "review", "Review", {
            { CoTRoleId::Reviewer,    "", "", false },
            { CoTRoleId::Critic,      "", "", false },
            { CoTRoleId::Synthesizer, "", "", false },
        }});

        // audit: auditor → verifier → summarizer
        presets.push_back({ "audit", "Audit", {
            { CoTRoleId::Auditor,     "", "", false },
            { CoTRoleId::Verifier,    "", "", false },
            { CoTRoleId::Summarizer,  "", "", false },
        }});

        // think: brainstorm → thinker → refiner → synthesizer
        presets.push_back({ "think", "Think", {
            { CoTRoleId::Brainstorm,  "", "", false },
            { CoTRoleId::Thinker,     "", "", false },
            { CoTRoleId::Refiner,     "", "", false },
            { CoTRoleId::Synthesizer, "", "", false },
        }});

        // research: researcher → thinker → verifier → synthesizer
        presets.push_back({ "research", "Research", {
            { CoTRoleId::Researcher,  "", "", false },
            { CoTRoleId::Thinker,     "", "", false },
            { CoTRoleId::Verifier,    "", "", false },
            { CoTRoleId::Synthesizer, "", "", false },
        }});

        // debate: debater_for → debater_against → synthesizer
        presets.push_back({ "debate", "Debate", {
            { CoTRoleId::DebaterFor,     "", "", false },
            { CoTRoleId::DebaterAgainst, "", "", false },
            { CoTRoleId::Synthesizer,    "", "", false },
        }});

        // custom: thinker (single step)
        presets.push_back({ "custom", "Custom", {
            { CoTRoleId::Thinker, "", "", false },
        }});
    }
    return presets;
}

const CoTPreset* getCoTPreset(const std::string& name) {
    const auto& presets = buildPresetTable();
    for (const auto& p : presets) {
        if (name == p.name) return &p;
    }
    return nullptr;
}

std::vector<std::string> getCoTPresetNames() {
    std::vector<std::string> names;
    const auto& presets = buildPresetTable();
    for (const auto& p : presets) {
        names.push_back(p.name);
    }
    return names;
}

// ============================================================================
// ChainOfThoughtEngine implementation
// ============================================================================

ChainOfThoughtEngine::ChainOfThoughtEngine() = default;
ChainOfThoughtEngine::~ChainOfThoughtEngine() {
    cancel();
}

ChainOfThoughtEngine& ChainOfThoughtEngine::instance() {
    static ChainOfThoughtEngine eng;
    return eng;
}

void ChainOfThoughtEngine::setInferenceCallback(CoTInferenceCallback cb) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_inferenceCallback = std::move(cb);
}

void ChainOfThoughtEngine::setStepCallback(CoTStepCallback cb) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stepCallback = std::move(cb);
}

void ChainOfThoughtEngine::setDefaultModel(const std::string& model) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_defaultModel = model;
}

void ChainOfThoughtEngine::setMaxSteps(int max) {
    if (max < 1) max = 1;
    if (max > 8) max = 8;
    m_maxSteps = max;
}

void ChainOfThoughtEngine::clearSteps() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_steps.clear();
}

void ChainOfThoughtEngine::addStep(const CoTStep& step) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if ((int)m_steps.size() < m_maxSteps) {
        m_steps.push_back(step);
    }
}

void ChainOfThoughtEngine::addStep(CoTRoleId role, const std::string& model,
                                    const std::string& instruction) {
    CoTStep s;
    s.role = role;
    s.model = model;
    s.instruction = instruction;
    s.skip = false;
    addStep(s);
}

void ChainOfThoughtEngine::setSteps(const std::vector<CoTStep>& steps) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_steps = steps;
    // Truncate to maxSteps
    if ((int)m_steps.size() > m_maxSteps) {
        m_steps.resize(m_maxSteps);
    }
}

bool ChainOfThoughtEngine::applyPreset(const std::string& presetName) {
    const CoTPreset* preset = getCoTPreset(presetName);
    if (!preset) return false;
    setSteps(preset->steps);
    return true;
}

// ============================================================================
// Build the system prompt for a step
// ============================================================================
std::string ChainOfThoughtEngine::buildSystemPrompt(const CoTStep& step) const {
    std::string instruction = step.instruction;
    if (instruction.empty()) {
        const CoTRoleInfo& info = getCoTRoleInfo(step.role);
        instruction = info.instruction;
    }
    return instruction;
}

// ============================================================================
// Build the user message: original query + all prior step outputs
// ============================================================================
std::string ChainOfThoughtEngine::buildUserMessage(
    const std::string& userQuery,
    const std::vector<CoTStepResult>& priorResults) const
{
    std::ostringstream oss;

    if (!priorResults.empty()) {
        oss << "=== Original Query ===\n" << userQuery << "\n\n";
        oss << "=== Prior Analysis Steps ===\n";
        for (const auto& pr : priorResults) {
            if (pr.skipped) continue;
            const CoTRoleInfo& info = getCoTRoleInfo(pr.role);
            oss << "--- Step " << (pr.stepIndex + 1) << " (" << info.label << ") ---\n";
            if (pr.success) {
                oss << pr.output << "\n\n";
            } else {
                oss << "[FAILED: " << pr.error << "]\n\n";
            }
        }
        oss << "=== Your Task ===\n"
            << "Building on the above analysis, provide your contribution.\n";
    } else {
        // First step — just the query
        oss << userQuery;
    }

    return oss.str();
}

// ============================================================================
// Execute the chain
// ============================================================================
CoTChainResult ChainOfThoughtEngine::executeChain(const std::string& userQuery) {
    CoTChainResult result;
    result.userQuery = userQuery;

    // Capture callbacks and steps under lock
    CoTInferenceCallback inferCb;
    CoTStepCallback stepCb;
    std::vector<CoTStep> steps;
    std::string defaultModel;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        inferCb = m_inferenceCallback;
        stepCb = m_stepCallback;
        steps = m_steps;
        defaultModel = m_defaultModel;
    }

    if (steps.empty()) {
        result.error = "No steps configured. Use applyPreset() or addStep() first.";
        return result;
    }

    if (!inferCb) {
        result.error = "No inference callback set. Use setInferenceCallback() first.";
        return result;
    }

    // Mark running
    bool expected = false;
    if (!m_running.compare_exchange_strong(expected, true)) {
        result.error = "Chain already running.";
        return result;
    }
    m_cancelled.store(false);

    auto chainStart = std::chrono::steady_clock::now();

    // Execute each step sequentially
    std::vector<CoTStepResult> completedResults;

    for (int i = 0; i < (int)steps.size() && !m_cancelled.load(); i++) {
        const CoTStep& step = steps[i];
        CoTStepResult sr;
        sr.stepIndex = i;
        sr.role = step.role;

        const CoTRoleInfo& roleInfo = getCoTRoleInfo(step.role);
        sr.roleName = roleInfo.name;

        if (step.skip) {
            sr.skipped = true;
            sr.success = true;
            result.stepsSkipped++;
            completedResults.push_back(sr);
            if (stepCb) stepCb(sr);
            continue;
        }

        // Determine model to use
        sr.model = step.model.empty() ? defaultModel : step.model;
        sr.instruction = buildSystemPrompt(step);

        // Build the user message with prior context
        std::string userMsg = buildUserMessage(userQuery, completedResults);

        // Execute inference
        auto stepStart = std::chrono::steady_clock::now();

        try {
            sr.output = inferCb(sr.instruction, userMsg, sr.model);
            sr.success = !sr.output.empty();
            if (!sr.success) {
                sr.error = "Empty response from inference";
                result.stepsFailed++;
            } else {
                result.stepsCompleted++;
                // Approximate token count (~4 chars per token)
                sr.tokenCount = (int)(sr.output.size() / 4);
            }
        } catch (...) {
            sr.success = false;
            sr.error = "Inference callback threw an exception";
            result.stepsFailed++;
        }

        auto stepEnd = std::chrono::steady_clock::now();
        sr.latencyMs = (int)std::chrono::duration_cast<std::chrono::milliseconds>(
            stepEnd - stepStart).count();

        completedResults.push_back(sr);
        if (stepCb) stepCb(sr);
    }

    auto chainEnd = std::chrono::steady_clock::now();
    result.totalLatencyMs = (int)std::chrono::duration_cast<std::chrono::milliseconds>(
        chainEnd - chainStart).count();

    // Copy step results
    result.stepResults = std::move(completedResults);

    // Final output = last successful non-skipped step
    for (int i = (int)result.stepResults.size() - 1; i >= 0; i--) {
        if (result.stepResults[i].success && !result.stepResults[i].skipped) {
            result.finalOutput = result.stepResults[i].output;
            break;
        }
    }

    result.success = (result.stepsCompleted > 0);

    // Update stats
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_stats.totalChains++;
        if (result.success) m_stats.successfulChains++;
        else m_stats.failedChains++;
        m_stats.totalStepsExecuted += result.stepsCompleted;
        m_stats.totalStepsSkipped  += result.stepsSkipped;
        m_stats.totalStepsFailed   += result.stepsFailed;

        // Update avg latency (running average)
        int total = m_stats.totalChains;
        m_stats.avgLatencyMs = ((m_stats.avgLatencyMs * (total - 1)) + result.totalLatencyMs) / total;

        // Track role usage
        for (const auto& sr : result.stepResults) {
            if (!sr.skipped) {
                m_stats.roleUsage[sr.role]++;
            }
        }
    }

    m_running.store(false);
    return result;
}

void ChainOfThoughtEngine::cancel() {
    m_cancelled.store(true);
}

ChainOfThoughtEngine::CoTStats ChainOfThoughtEngine::getStats() const {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    return m_stats;
}

void ChainOfThoughtEngine::resetStats() {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    m_stats = CoTStats{};
}

// ============================================================================
// JSON serialization helpers
// ============================================================================

static std::string escapeJsonStr(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 16);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;      break;
        }
    }
    return out;
}

std::string ChainOfThoughtEngine::getStatusJSON() const {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    std::ostringstream j;
    j << "{";
    j << "\"running\":" << (m_running.load() ? "true" : "false") << ",";
    j << "\"maxSteps\":" << m_maxSteps << ",";
    j << "\"currentStepCount\":" << m_steps.size() << ",";
    j << "\"defaultModel\":\"" << escapeJsonStr(m_defaultModel) << "\",";
    j << "\"stats\":{";
    j << "\"totalChains\":" << m_stats.totalChains << ",";
    j << "\"successfulChains\":" << m_stats.successfulChains << ",";
    j << "\"failedChains\":" << m_stats.failedChains << ",";
    j << "\"totalStepsExecuted\":" << m_stats.totalStepsExecuted << ",";
    j << "\"totalStepsSkipped\":" << m_stats.totalStepsSkipped << ",";
    j << "\"totalStepsFailed\":" << m_stats.totalStepsFailed << ",";
    j << "\"avgLatencyMs\":" << m_stats.avgLatencyMs;
    j << "}}";
    return j.str();
}

std::string ChainOfThoughtEngine::getStepsJSON() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::ostringstream j;
    j << "[";
    for (size_t i = 0; i < m_steps.size(); i++) {
        if (i > 0) j << ",";
        const auto& s = m_steps[i];
        const CoTRoleInfo& info = getCoTRoleInfo(s.role);
        j << "{";
        j << "\"index\":" << i << ",";
        j << "\"role\":\"" << info.name << "\",";
        j << "\"roleLabel\":\"" << info.label << "\",";
        j << "\"model\":\"" << escapeJsonStr(s.model) << "\",";
        j << "\"instruction\":\"" << escapeJsonStr(s.instruction.empty() ? info.instruction : s.instruction) << "\",";
        j << "\"skip\":" << (s.skip ? "true" : "false");
        j << "}";
    }
    j << "]";
    return j.str();
}

std::string ChainOfThoughtEngine::getPresetsJSON() const {
    std::ostringstream j;
    j << "{";
    const auto& presets = buildPresetTable();
    for (size_t pi = 0; pi < presets.size(); pi++) {
        if (pi > 0) j << ",";
        const auto& p = presets[pi];
        j << "\"" << p.name << "\":{";
        j << "\"label\":\"" << p.label << "\",";
        j << "\"steps\":[";
        for (size_t si = 0; si < p.steps.size(); si++) {
            if (si > 0) j << ",";
            const auto& s = p.steps[si];
            const CoTRoleInfo& info = getCoTRoleInfo(s.role);
            j << "{\"role\":\"" << info.name << "\"}";
        }
        j << "]}";
    }
    j << "}";
    return j.str();
}
