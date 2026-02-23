// ============================================================================
// agent_policy.cpp — Adaptive Intelligence & Policy Layer (Phase 7)
// ============================================================================
// Implementation: JSONL persistence, heuristic computation, suggestion
// generation, policy evaluation, export/import.
// ============================================================================

#include "agent_policy.h"
#include "agent_history.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <numeric>
#include <random>
#include <sstream>
#include <filesystem>

// ============================================================================
// JSON helpers (self-contained — same pattern as agent_history.cpp)
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

static std::string unescapeJsonStr(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            switch (s[i + 1]) {
                case '"':  out += '"';  ++i; break;
                case '\\': out += '\\'; ++i; break;
                case 'n':  out += '\n'; ++i; break;
                case 'r':  out += '\r'; ++i; break;
                case 't':  out += '\t'; ++i; break;
                default:   out += s[i + 1]; ++i; break;
            }
        } else {
            out += s[i];
        }
    }
    return out;
}

static std::string extractField(const std::string& json, const std::string& key) {
    std::string needle = "\"" + key + "\":";
    auto pos = json.find(needle);
    if (pos == std::string::npos) return "";
    pos += needle.size();
    while (pos < json.size() && json[pos] == ' ') ++pos;
    if (pos >= json.size()) return "";
    
    if (json[pos] == '"') {
        // String value
        ++pos;
        std::string result;
        while (pos < json.size()) {
            if (json[pos] == '\\' && pos + 1 < json.size()) {
                result += json[pos];
                result += json[pos + 1];
                pos += 2;
            } else if (json[pos] == '"') {
                break;
            } else {
                result += json[pos++];
            }
        }
        return unescapeJsonStr(result);
    }
    
    // Number / bool / null
    size_t end = json.find_first_of(",}]", pos);
    if (end == std::string::npos) end = json.size();
    std::string val = json.substr(pos, end - pos);
    // Trim whitespace
    while (!val.empty() && (val.back() == ' ' || val.back() == '\n' || val.back() == '\r'))
        val.pop_back();
    return val;
}

static int extractInt(const std::string& json, const std::string& key, int def = 0) {
    std::string v = extractField(json, key);
    if (v.empty()) return def;
    try { return std::stoi(v); } catch (...) { return def; }
}

static int64_t extractInt64(const std::string& json, const std::string& key, int64_t def = 0) {
    std::string v = extractField(json, key);
    if (v.empty()) return def;
    try { return std::stoll(v); } catch (...) { return def; }
}

static float extractFloat(const std::string& json, const std::string& key, float def = 0.0f) {
    std::string v = extractField(json, key);
    if (v.empty()) return def;
    try { return std::stof(v); } catch (...) { return def; }
}

static bool extractBool(const std::string& json, const std::string& key, bool def = false) {
    std::string v = extractField(json, key);
    if (v == "true") return true;
    if (v == "false") return false;
    return def;
}

// ============================================================================
// PolicyTrigger serialization
// ============================================================================

std::string PolicyTrigger::toJSON() const {
    std::ostringstream o;
    o << "{\"eventType\":\"" << escapeJsonStr(eventType) << "\""
      << ",\"failureReason\":\"" << escapeJsonStr(failureReason) << "\""
      << ",\"taskPattern\":\"" << escapeJsonStr(taskPattern) << "\""
      << ",\"toolName\":\"" << escapeJsonStr(toolName) << "\""
      << ",\"failureRateAbove\":" << failureRateAbove
      << ",\"minOccurrences\":" << minOccurrences
      << "}";
    return o.str();
}

PolicyTrigger PolicyTrigger::fromJSON(const std::string& json) {
    PolicyTrigger t;
    t.eventType        = extractField(json, "eventType");
    t.failureReason    = extractField(json, "failureReason");
    t.taskPattern      = extractField(json, "taskPattern");
    t.toolName         = extractField(json, "toolName");
    t.failureRateAbove = extractFloat(json, "failureRateAbove", 0.0f);
    t.minOccurrences   = extractInt(json, "minOccurrences", 0);
    return t;
}

// ============================================================================
// PolicyAction serialization
// ============================================================================

std::string PolicyAction::toJSON() const {
    std::ostringstream o;
    o << "{\"maxRetries\":" << maxRetries
      << ",\"retryDelayMs\":" << retryDelayMs
      << ",\"preferChainOverSwarm\":" << (preferChainOverSwarm ? "true" : "false")
      << ",\"reduceParallelism\":" << reduceParallelism
      << ",\"timeoutOverrideMs\":" << timeoutOverrideMs
      << ",\"confidenceThreshold\":" << confidenceThreshold
      << ",\"addValidationStep\":" << (addValidationStep ? "true" : "false")
      << ",\"validationPrompt\":\"" << escapeJsonStr(validationPrompt) << "\""
      << ",\"customAction\":\"" << escapeJsonStr(customAction) << "\""
      << "}";
    return o.str();
}

PolicyAction PolicyAction::fromJSON(const std::string& json) {
    PolicyAction a;
    a.maxRetries           = extractInt(json, "maxRetries", -1);
    a.retryDelayMs         = extractInt(json, "retryDelayMs", -1);
    a.preferChainOverSwarm = extractBool(json, "preferChainOverSwarm", false);
    a.reduceParallelism    = extractInt(json, "reduceParallelism", 0);
    a.timeoutOverrideMs    = extractInt(json, "timeoutOverrideMs", -1);
    a.confidenceThreshold  = extractFloat(json, "confidenceThreshold", -1.0f);
    a.addValidationStep    = extractBool(json, "addValidationStep", false);
    a.validationPrompt     = extractField(json, "validationPrompt");
    a.customAction         = extractField(json, "customAction");
    return a;
}

// ============================================================================
// AgentPolicy serialization
// ============================================================================

std::string AgentPolicy::toJSON() const {
    std::ostringstream o;
    o << "{\"id\":\"" << escapeJsonStr(id) << "\""
      << ",\"name\":\"" << escapeJsonStr(name) << "\""
      << ",\"description\":\"" << escapeJsonStr(description) << "\""
      << ",\"version\":" << version
      << ",\"trigger\":" << trigger.toJSON()
      << ",\"action\":" << action.toJSON()
      << ",\"enabled\":" << (enabled ? "true" : "false")
      << ",\"requiresUserApproval\":" << (requiresUserApproval ? "true" : "false")
      << ",\"priority\":" << priority
      << ",\"createdAt\":" << createdAt
      << ",\"modifiedAt\":" << modifiedAt
      << ",\"createdBy\":\"" << escapeJsonStr(createdBy) << "\""
      << ",\"appliedCount\":" << appliedCount
      << "}";
    return o.str();
}

AgentPolicy AgentPolicy::fromJSON(const std::string& json) {
    AgentPolicy p;
    p.id                  = extractField(json, "id");
    p.name                = extractField(json, "name");
    p.description         = extractField(json, "description");
    p.version             = extractInt(json, "version", 1);
    p.enabled             = extractBool(json, "enabled", true);
    p.requiresUserApproval = extractBool(json, "requiresUserApproval", true);
    p.priority            = extractInt(json, "priority", 50);
    p.createdAt           = extractInt64(json, "createdAt", 0);
    p.modifiedAt          = extractInt64(json, "modifiedAt", 0);
    p.createdBy           = extractField(json, "createdBy");
    p.appliedCount        = extractInt(json, "appliedCount", 0);
    
    // Extract nested trigger object
    auto triggerPos = json.find("\"trigger\":{");
    if (triggerPos != std::string::npos) {
        size_t start = json.find('{', triggerPos + 9);
        int depth = 1;
        size_t end = start + 1;
        while (end < json.size() && depth > 0) {
            if (json[end] == '{') ++depth;
            else if (json[end] == '}') --depth;
            ++end;
        }
        p.trigger = PolicyTrigger::fromJSON(json.substr(start, end - start));
    }
    
    // Extract nested action object
    auto actionPos = json.find("\"action\":{");
    if (actionPos != std::string::npos) {
        size_t start = json.find('{', actionPos + 8);
        int depth = 1;
        size_t end = start + 1;
        while (end < json.size() && depth > 0) {
            if (json[end] == '{') ++depth;
            else if (json[end] == '}') --depth;
            ++end;
        }
        p.action = PolicyAction::fromJSON(json.substr(start, end - start));
    }
    
    return p;
}

// ============================================================================
// PolicySuggestion serialization
// ============================================================================

std::string PolicySuggestion::toJSON() const {
    std::ostringstream o;
    o << "{\"id\":\"" << escapeJsonStr(id) << "\""
      << ",\"proposedPolicy\":" << proposedPolicy.toJSON()
      << ",\"rationale\":\"" << escapeJsonStr(rationale) << "\""
      << ",\"estimatedImprovement\":" << estimatedImprovement
      << ",\"supportingEvents\":" << supportingEvents
      << ",\"affectedEventTypes\":[";
    for (size_t i = 0; i < affectedEventTypes.size(); ++i) {
        if (i > 0) o << ",";
        o << "\"" << escapeJsonStr(affectedEventTypes[i]) << "\"";
    }
    o << "],\"affectedAgentIds\":[";
    for (size_t i = 0; i < affectedAgentIds.size(); ++i) {
        if (i > 0) o << ",";
        o << "\"" << escapeJsonStr(affectedAgentIds[i]) << "\"";
    }
    o << "],\"state\":\"" << stateString() << "\""
      << ",\"generatedAt\":" << generatedAt
      << ",\"decidedAt\":" << decidedAt
      << "}";
    return o.str();
}

PolicySuggestion PolicySuggestion::fromJSON(const std::string& json) {
    PolicySuggestion s;
    s.id                   = extractField(json, "id");
    s.rationale            = extractField(json, "rationale");
    s.estimatedImprovement = extractFloat(json, "estimatedImprovement", 0.0f);
    s.supportingEvents     = extractInt(json, "supportingEvents", 0);
    s.generatedAt          = extractInt64(json, "generatedAt", 0);
    s.decidedAt            = extractInt64(json, "decidedAt", 0);
    
    std::string stateStr = extractField(json, "state");
    if (stateStr == "accepted")       s.state = PolicySuggestion::State::Accepted;
    else if (stateStr == "rejected")  s.state = PolicySuggestion::State::Rejected;
    else if (stateStr == "expired")   s.state = PolicySuggestion::State::Expired;
    else                              s.state = PolicySuggestion::State::Pending;
    
    // Extract nested proposedPolicy
    auto policyPos = json.find("\"proposedPolicy\":{");
    if (policyPos != std::string::npos) {
        size_t start = json.find('{', policyPos + 17);
        int depth = 1;
        size_t end = start + 1;
        while (end < json.size() && depth > 0) {
            if (json[end] == '{') ++depth;
            else if (json[end] == '}') --depth;
            ++end;
        }
        s.proposedPolicy = AgentPolicy::fromJSON(json.substr(start, end - start));
    }
    
    return s;
}

// ============================================================================
// PolicyHeuristic serialization
// ============================================================================

std::string PolicyHeuristic::toJSON() const {
    std::ostringstream o;
    o << "{\"key\":\"" << escapeJsonStr(key) << "\""
      << ",\"totalEvents\":" << totalEvents
      << ",\"successCount\":" << successCount
      << ",\"failCount\":" << failCount
      << ",\"successRate\":" << successRate
      << ",\"avgDurationMs\":" << avgDurationMs
      << ",\"p95DurationMs\":" << p95DurationMs
      << ",\"topFailureReasons\":[";
    for (size_t i = 0; i < topFailureReasons.size() && i < 5; ++i) {
        if (i > 0) o << ",";
        o << "\"" << escapeJsonStr(topFailureReasons[i]) << "\"";
    }
    o << "]}";
    return o.str();
}

// ============================================================================
// PolicyEngine implementation
// ============================================================================

PolicyEngine::PolicyEngine(const std::string& storageDir)
    : m_storageDir(storageDir)
{
    if (!m_storageDir.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(m_storageDir, ec);
        load();
    }
}

PolicyEngine::~PolicyEngine() {
    if (!m_storageDir.empty()) {
        save();
    }
}

// ---- UUID generation ----

std::string PolicyEngine::generateUUID() const {
    static std::atomic<int> counter{0};
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    int c = counter.fetch_add(1);
    
    std::ostringstream oss;
    oss << "pol-" << std::hex << (ms & 0xFFFFFFFF) << "-" << std::dec << c;
    return oss.str();
}

int64_t PolicyEngine::nowMs() const {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

std::string PolicyEngine::escapeJson(const std::string& s) const {
    return escapeJsonStr(s);
}

std::string PolicyEngine::unescapeJson(const std::string& s) const {
    return unescapeJsonStr(s);
}

std::string PolicyEngine::getPolicyFilePath() const {
    return m_storageDir + "/policies.json";
}

std::string PolicyEngine::getSuggestionsFilePath() const {
    return m_storageDir + "/suggestions.json";
}

// ============================================================================
// Policy CRUD
// ============================================================================

std::string PolicyEngine::addPolicy(const AgentPolicy& policy) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    AgentPolicy p = policy;
    if (p.id.empty()) p.id = generateUUID();
    if (p.createdAt == 0) p.createdAt = nowMs();
    p.modifiedAt = nowMs();
    
    m_policies.push_back(p);
    logInfo("Policy added: " + p.name + " [" + p.id + "]");
    return p.id;
}

bool PolicyEngine::updatePolicy(const std::string& policyId, const AgentPolicy& updated) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    for (auto& p : m_policies) {
        if (p.id == policyId) {
            std::string oldName = p.name;
            p = updated;
            p.id = policyId;  // preserve ID
            p.modifiedAt = nowMs();
            p.version++;
            logInfo("Policy updated: " + oldName + " -> " + p.name + " [" + policyId + "]");
            return true;
        }
    }
    
    logError("Policy not found: " + policyId);
    return false;
}

bool PolicyEngine::removePolicy(const std::string& policyId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = std::remove_if(m_policies.begin(), m_policies.end(),
        [&](const AgentPolicy& p) { return p.id == policyId; });
    
    if (it != m_policies.end()) {
        m_policies.erase(it, m_policies.end());
        logInfo("Policy removed: " + policyId);
        return true;
    }
    
    logError("Policy not found for removal: " + policyId);
    return false;
}

bool PolicyEngine::setEnabled(const std::string& policyId, bool enabled) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    for (auto& p : m_policies) {
        if (p.id == policyId) {
            p.enabled = enabled;
            p.modifiedAt = nowMs();
            logInfo("Policy " + std::string(enabled ? "enabled" : "disabled") + ": " + p.name);
            return true;
        }
    }
    return false;
}

const AgentPolicy* PolicyEngine::getPolicy(const std::string& policyId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    for (const auto& p : m_policies) {
        if (p.id == policyId) return &p;
    }
    return nullptr;
}

std::vector<AgentPolicy> PolicyEngine::getAllPolicies(bool enabledOnly) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!enabledOnly) return m_policies;
    
    std::vector<AgentPolicy> result;
    for (const auto& p : m_policies) {
        if (p.enabled) result.push_back(p);
    }
    return result;
}

size_t PolicyEngine::policyCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_policies.size();
}

// ============================================================================
// Policy Evaluation
// ============================================================================

bool PolicyEngine::matchesTrigger(const PolicyTrigger& trigger,
                                   const std::string& eventType,
                                   const std::string& description,
                                   const std::string& toolName,
                                   const std::string& errorMessage) {
    // Event type filter
    if (!trigger.eventType.empty() && trigger.eventType != eventType) {
        return false;
    }
    
    // Failure reason substring match
    if (!trigger.failureReason.empty()) {
        if (errorMessage.find(trigger.failureReason) == std::string::npos) {
            return false;
        }
    }
    
    // Task pattern substring match (checks description)
    if (!trigger.taskPattern.empty()) {
        if (description.find(trigger.taskPattern) == std::string::npos) {
            return false;
        }
    }
    
    // Tool name match
    if (!trigger.toolName.empty() && trigger.toolName != toolName) {
        return false;
    }
    
    // Failure rate check (requires heuristic data)
    if (trigger.failureRateAbove > 0.0f) {
        std::lock_guard<std::mutex> hLock(m_heuristicsMutex);
        std::string hKey = eventType;
        if (!toolName.empty()) hKey = "tool:" + toolName;
        
        bool found = false;
        for (const auto& h : m_heuristics) {
            if (h.key == hKey) {
                if ((1.0f - h.successRate) < trigger.failureRateAbove) {
                    return false;  // failure rate below threshold
                }
                if (trigger.minOccurrences > 0 && h.totalEvents < trigger.minOccurrences) {
                    return false;  // not enough data
                }
                found = true;
                break;
            }
        }
        if (!found) return false;  // no heuristic data = can't evaluate
    }
    
    return true;
}

PolicyAction PolicyEngine::mergePolicyActions(const std::vector<const AgentPolicy*>& policies) const {
    PolicyAction merged;
    
    // Highest priority (lowest number) wins per field
    for (const auto* p : policies) {
        const auto& a = p->action;
        
        if (a.maxRetries >= 0 && merged.maxRetries < 0)
            merged.maxRetries = a.maxRetries;
        if (a.retryDelayMs >= 0 && merged.retryDelayMs < 0)
            merged.retryDelayMs = a.retryDelayMs;
        if (a.preferChainOverSwarm && !merged.preferChainOverSwarm)
            merged.preferChainOverSwarm = true;
        if (a.reduceParallelism > 0 && merged.reduceParallelism == 0)
            merged.reduceParallelism = a.reduceParallelism;
        if (a.timeoutOverrideMs >= 0 && merged.timeoutOverrideMs < 0)
            merged.timeoutOverrideMs = a.timeoutOverrideMs;
        if (a.confidenceThreshold >= 0.0f && merged.confidenceThreshold < 0.0f)
            merged.confidenceThreshold = a.confidenceThreshold;
        if (a.addValidationStep && !merged.addValidationStep) {
            merged.addValidationStep = true;
            merged.validationPrompt = a.validationPrompt;
        }
        if (!a.customAction.empty() && merged.customAction.empty())
            merged.customAction = a.customAction;
    }
    
    return merged;
}

PolicyEvalResult PolicyEngine::evaluate(const std::string& eventType,
                                         const std::string& description,
                                         const std::string& toolName,
                                         const std::string& errorMessage) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_evalCount.fetch_add(1);
    
    PolicyEvalResult result;
    
    // Find all matching enabled policies, sorted by priority
    std::vector<const AgentPolicy*> matched;
    for (const auto& p : m_policies) {
        if (!p.enabled) continue;
        if (matchesTrigger(p.trigger, eventType, description, toolName, errorMessage)) {
            matched.push_back(&p);
        }
    }
    
    if (matched.empty()) return result;
    
    // Sort by priority (lower number = higher priority)
    std::sort(matched.begin(), matched.end(),
        [](const AgentPolicy* a, const AgentPolicy* b) {
            return a->priority < b->priority;
        });
    
    result.hasMatch = true;
    result.matchedPolicies = matched;
    result.mergedAction = mergePolicyActions(matched);
    
    m_matchCount.fetch_add(1);
    
    // Check if any policy requires user approval
    for (const auto* p : matched) {
        if (p->requiresUserApproval) {
            result.needsUserApproval = true;
            break;
        }
    }
    
    // Build summary
    std::ostringstream summary;
    summary << matched.size() << " policies matched: ";
    for (size_t i = 0; i < matched.size() && i < 3; ++i) {
        if (i > 0) summary << ", ";
        summary << matched[i]->name << " (pri=" << matched[i]->priority << ")";
    }
    if (matched.size() > 3) summary << " +" << (matched.size() - 3) << " more";
    result.summary = summary.str();
    
    return result;
}

// ============================================================================
// Heuristics — History-Derived Statistics
// ============================================================================

void PolicyEngine::computeHeuristics() {
    if (!m_historyRecorder) {
        logError("Cannot compute heuristics: no history recorder set");
        return;
    }
    
    auto events = m_historyRecorder->allEvents();
    if (events.empty()) {
        logInfo("No events to compute heuristics from");
        return;
    }
    
    logInfo("Computing heuristics from " + std::to_string(events.size()) + " events...");
    
    // Group events by type
    std::unordered_map<std::string, std::vector<const AgentEvent*>> byType;
    std::unordered_map<std::string, std::vector<const AgentEvent*>> byTool;
    
    for (const auto& e : events) {
        byType[e.eventType].push_back(&e);
        
        // Extract tool name from tool_invoke / tool_result events
        if (e.eventType == "tool_invoke" || e.eventType == "tool_result") {
            // metadata may contain tool name; description often starts with tool name
            std::string toolKey = "tool:" + e.description;
            byTool[toolKey].push_back(&e);
        }
    }
    
    std::vector<PolicyHeuristic> newHeuristics;
    
    // Compute per-event-type heuristics
    for (const auto& [type, evts] : byType) {
        PolicyHeuristic h;
        h.key = type;
        h.totalEvents = (int)evts.size();
        
        std::vector<float> durations;
        for (const auto* e : evts) {
            if (e->success) {
                h.successCount++;
            } else {
                h.failCount++;
                if (!e->errorMessage.empty()) {
                    h.failureReasonCounts[e->errorMessage]++;
                }
            }
            if (e->durationMs > 0) {
                durations.push_back((float)e->durationMs);
            }
        }
        
        h.successRate = h.totalEvents > 0
            ? (float)h.successCount / (float)h.totalEvents
            : 0.0f;
        
        if (!durations.empty()) {
            float sum = std::accumulate(durations.begin(), durations.end(), 0.0f);
            h.avgDurationMs = sum / (float)durations.size();
            
            std::sort(durations.begin(), durations.end());
            size_t p95idx = (size_t)(durations.size() * 0.95f);
            if (p95idx >= durations.size()) p95idx = durations.size() - 1;
            h.p95DurationMs = durations[p95idx];
        }
        
        // Top failure reasons (sorted by count, top 5)
        std::vector<std::pair<int, std::string>> failureSorted;
        for (const auto& [reason, count] : h.failureReasonCounts) {
            failureSorted.push_back({count, reason});
        }
        std::sort(failureSorted.rbegin(), failureSorted.rend());
        for (size_t i = 0; i < failureSorted.size() && i < 5; ++i) {
            h.topFailureReasons.push_back(failureSorted[i].second);
        }
        
        newHeuristics.push_back(std::move(h));
    }
    
    // Compute per-tool heuristics
    for (const auto& [toolKey, evts] : byTool) {
        PolicyHeuristic h;
        h.key = toolKey;
        h.totalEvents = (int)evts.size();
        
        for (const auto* e : evts) {
            if (e->success) h.successCount++;
            else h.failCount++;
        }
        
        h.successRate = h.totalEvents > 0
            ? (float)h.successCount / (float)h.totalEvents
            : 0.0f;
        
        newHeuristics.push_back(std::move(h));
    }
    
    // Atomic swap
    {
        std::lock_guard<std::mutex> lock(m_heuristicsMutex);
        m_heuristics = std::move(newHeuristics);
    }
    
    logInfo("Computed " + std::to_string(m_heuristics.size()) + " heuristics");
}

const PolicyHeuristic* PolicyEngine::getHeuristic(const std::string& key) const {
    std::lock_guard<std::mutex> lock(m_heuristicsMutex);
    for (const auto& h : m_heuristics) {
        if (h.key == key) return &h;
    }
    return nullptr;
}

std::vector<PolicyHeuristic> PolicyEngine::getAllHeuristics() const {
    std::lock_guard<std::mutex> lock(m_heuristicsMutex);
    return m_heuristics;
}

std::string PolicyEngine::heuristicsSummaryJSON() const {
    std::lock_guard<std::mutex> lock(m_heuristicsMutex);
    
    std::ostringstream o;
    o << "{\"heuristics\":[";
    for (size_t i = 0; i < m_heuristics.size(); ++i) {
        if (i > 0) o << ",";
        o << m_heuristics[i].toJSON();
    }
    o << "],\"count\":" << m_heuristics.size() << "}";
    return o.str();
}

// ============================================================================
// Suggestion Generation — the intelligence layer
// ============================================================================

std::vector<PolicySuggestion> PolicyEngine::generateSuggestions() {
    computeHeuristics();
    
    std::lock_guard<std::mutex> lock(m_mutex);
    std::lock_guard<std::mutex> hLock(m_heuristicsMutex);
    
    std::vector<PolicySuggestion> newSuggestions;
    
    for (const auto& h : m_heuristics) {
        // Skip tool-level heuristics for now (focus on event types)
        if (h.key.substr(0, 5) == "tool:") continue;
        if (h.totalEvents < 3) continue;  // need sufficient data
        
        // ── Suggestion 1: High failure rate → add retry policy ──
        if (h.failCount > 0 && h.successRate < 0.7f && h.totalEvents >= 5) {
            // Check if we already have a retry policy for this event type
            bool alreadyCovered = false;
            for (const auto& p : m_policies) {
                if (p.trigger.eventType == h.key && p.action.maxRetries >= 0) {
                    alreadyCovered = true;
                    break;
                }
            }
            // Also check existing pending suggestions
            for (const auto& s : m_suggestions) {
                if (s.state == PolicySuggestion::State::Pending &&
                    s.proposedPolicy.trigger.eventType == h.key &&
                    s.proposedPolicy.action.maxRetries >= 0) {
                    alreadyCovered = true;
                    break;
                }
            }
            
            if (!alreadyCovered) {
                PolicySuggestion s;
                s.id = generateUUID();
                s.generatedAt = nowMs();
                s.estimatedImprovement = std::min(0.3f, (1.0f - h.successRate) * 0.5f);
                s.supportingEvents = h.totalEvents;
                s.affectedEventTypes.push_back(h.key);
                
                s.proposedPolicy.id = generateUUID();
                s.proposedPolicy.name = "Auto-retry on " + h.key + " failures";
                s.proposedPolicy.description =
                    "Observed " + std::to_string((int)((1.0f - h.successRate) * 100.0f)) +
                    "% failure rate across " + std::to_string(h.totalEvents) +
                    " events. Suggests adding automatic retry (max 2).";
                s.proposedPolicy.trigger.eventType = h.key;
                s.proposedPolicy.trigger.failureRateAbove = 0.3f;
                s.proposedPolicy.trigger.minOccurrences = 5;
                s.proposedPolicy.action.maxRetries = 2;
                s.proposedPolicy.action.retryDelayMs = 1000;
                s.proposedPolicy.priority = 40;
                s.proposedPolicy.createdBy = "system";
                s.proposedPolicy.requiresUserApproval = true;
                
                s.rationale = "Event type '" + h.key + "' has a " +
                    std::to_string((int)((1.0f - h.successRate) * 100.0f)) +
                    "% failure rate over " + std::to_string(h.totalEvents) +
                    " events. Adding automatic retry with 2 attempts and 1s delay " +
                    "could recover transient failures.";
                
                newSuggestions.push_back(std::move(s));
            }
        }
        
        // ── Suggestion 2: Slow operations → increase timeout ──
        if (h.avgDurationMs > 0 && h.p95DurationMs > 30000.0f && h.totalEvents >= 5) {
            bool alreadyCovered = false;
            for (const auto& p : m_policies) {
                if (p.trigger.eventType == h.key && p.action.timeoutOverrideMs >= 0) {
                    alreadyCovered = true;
                    break;
                }
            }
            
            if (!alreadyCovered) {
                PolicySuggestion s;
                s.id = generateUUID();
                s.generatedAt = nowMs();
                s.estimatedImprovement = 0.1f;
                s.supportingEvents = h.totalEvents;
                s.affectedEventTypes.push_back(h.key);
                
                int suggestedTimeout = (int)(h.p95DurationMs * 1.5f);
                
                s.proposedPolicy.id = generateUUID();
                s.proposedPolicy.name = "Extend timeout for slow " + h.key;
                s.proposedPolicy.description =
                    "P95 duration is " + std::to_string((int)h.p95DurationMs) +
                    "ms. Suggesting timeout of " + std::to_string(suggestedTimeout) + "ms.";
                s.proposedPolicy.trigger.eventType = h.key;
                s.proposedPolicy.action.timeoutOverrideMs = suggestedTimeout;
                s.proposedPolicy.priority = 60;
                s.proposedPolicy.createdBy = "system";
                s.proposedPolicy.requiresUserApproval = true;
                
                s.rationale = "Event type '" + h.key + "' P95 latency is " +
                    std::to_string((int)h.p95DurationMs) + "ms (avg " +
                    std::to_string((int)h.avgDurationMs) + "ms). Extending " +
                    "the timeout to " + std::to_string(suggestedTimeout) +
                    "ms prevents premature cancellation of legitimate long-running tasks.";
                
                newSuggestions.push_back(std::move(s));
            }
        }
        
        // ── Suggestion 3: Swarm failures → suggest chain alternative ──
        if (h.key == "swarm_complete" && h.successRate < 0.8f && h.totalEvents >= 3) {
            bool alreadyCovered = false;
            for (const auto& p : m_policies) {
                if (p.action.preferChainOverSwarm) {
                    alreadyCovered = true;
                    break;
                }
            }
            
            if (!alreadyCovered) {
                PolicySuggestion s;
                s.id = generateUUID();
                s.generatedAt = nowMs();
                s.estimatedImprovement = 0.15f;
                s.supportingEvents = h.totalEvents;
                s.affectedEventTypes.push_back("swarm_start");
                s.affectedEventTypes.push_back("swarm_complete");
                
                s.proposedPolicy.id = generateUUID();
                s.proposedPolicy.name = "Prefer chain over swarm for reliability";
                s.proposedPolicy.description =
                    "Swarm operations showing " +
                    std::to_string((int)((1.0f - h.successRate) * 100.0f)) +
                    "% failure rate. Sequential chains may be more reliable.";
                s.proposedPolicy.trigger.eventType = "swarm_start";
                s.proposedPolicy.action.preferChainOverSwarm = true;
                s.proposedPolicy.priority = 45;
                s.proposedPolicy.createdBy = "system";
                s.proposedPolicy.requiresUserApproval = true;
                
                s.rationale = "Swarm operations have a " +
                    std::to_string((int)((1.0f - h.successRate) * 100.0f)) +
                    "% failure rate. Sequential chain execution avoids parallel " +
                    "resource contention and provides clearer error attribution.";
                
                newSuggestions.push_back(std::move(s));
            }
        }
        
        // ── Suggestion 4: Add validation step for low-confidence operations ──
        if (h.key == "agent_complete" && h.successRate > 0.5f && h.successRate < 0.85f &&
            h.totalEvents >= 10) {
            bool alreadyCovered = false;
            for (const auto& p : m_policies) {
                if (p.trigger.eventType == h.key && p.action.addValidationStep) {
                    alreadyCovered = true;
                    break;
                }
            }
            
            if (!alreadyCovered) {
                PolicySuggestion s;
                s.id = generateUUID();
                s.generatedAt = nowMs();
                s.estimatedImprovement = 0.2f;
                s.supportingEvents = h.totalEvents;
                s.affectedEventTypes.push_back("agent_spawn");
                s.affectedEventTypes.push_back("agent_complete");
                
                s.proposedPolicy.id = generateUUID();
                s.proposedPolicy.name = "Add validation for agent outputs";
                s.proposedPolicy.description =
                    "Agent success rate is " + std::to_string((int)(h.successRate * 100.0f)) +
                    "%. Adding a validation step could catch failures earlier.";
                s.proposedPolicy.trigger.eventType = "agent_spawn";
                s.proposedPolicy.action.addValidationStep = true;
                s.proposedPolicy.action.validationPrompt =
                    "Validate the following output for correctness and completeness. "
                    "Reply PASS if acceptable, FAIL with reason if not: ";
                s.proposedPolicy.priority = 55;
                s.proposedPolicy.createdBy = "system";
                s.proposedPolicy.requiresUserApproval = true;
                
                s.rationale = "Agent completions have a " +
                    std::to_string((int)(h.successRate * 100.0f)) +
                    "% success rate across " + std::to_string(h.totalEvents) +
                    " runs. Injecting a validation sub-agent can catch " +
                    "incorrect outputs before they propagate downstream.";
                
                newSuggestions.push_back(std::move(s));
            }
        }
    }
    
    // Store new suggestions
    for (auto& s : newSuggestions) {
        m_suggestions.push_back(s);
    }
    
    logInfo("Generated " + std::to_string(newSuggestions.size()) + " new suggestions");
    return newSuggestions;
}

bool PolicyEngine::acceptSuggestion(const std::string& suggestionId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    for (auto& s : m_suggestions) {
        if (s.id == suggestionId && s.state == PolicySuggestion::State::Pending) {
            s.state = PolicySuggestion::State::Accepted;
            s.decidedAt = nowMs();
            
            // Convert to active policy
            AgentPolicy p = s.proposedPolicy;
            p.createdAt = nowMs();
            p.modifiedAt = nowMs();
            p.enabled = true;
            m_policies.push_back(p);
            
            logInfo("Suggestion accepted → policy '" + p.name + "' [" + p.id + "]");
            return true;
        }
    }
    
    logError("Suggestion not found or not pending: " + suggestionId);
    return false;
}

bool PolicyEngine::rejectSuggestion(const std::string& suggestionId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    for (auto& s : m_suggestions) {
        if (s.id == suggestionId && s.state == PolicySuggestion::State::Pending) {
            s.state = PolicySuggestion::State::Rejected;
            s.decidedAt = nowMs();
            logInfo("Suggestion rejected: " + suggestionId);
            return true;
        }
    }
    
    logError("Suggestion not found or not pending: " + suggestionId);
    return false;
}

std::vector<PolicySuggestion> PolicyEngine::getPendingSuggestions() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<PolicySuggestion> result;
    for (const auto& s : m_suggestions) {
        if (s.state == PolicySuggestion::State::Pending) {
            result.push_back(s);
        }
    }
    return result;
}

std::vector<PolicySuggestion> PolicyEngine::getAllSuggestions() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_suggestions;
}

// ============================================================================
// Export / Import
// ============================================================================

std::string PolicyEngine::exportPolicies() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::ostringstream o;
    o << "{\"version\":1"
      << ",\"exportedAt\":" << nowMs()
      << ",\"policies\":[";
    
    bool first = true;
    for (const auto& p : m_policies) {
        if (!p.enabled) continue;  // only export enabled policies
        if (!first) o << ",";
        o << p.toJSON();
        first = false;
    }
    
    o << "]}";
    return o.str();
}

int PolicyEngine::importPolicies(const std::string& json) {
    // Parse the policies array from the import JSON
    auto arrStart = json.find("\"policies\":[");
    if (arrStart == std::string::npos) {
        logError("Import failed: no 'policies' array found");
        return 0;
    }
    
    size_t pos = json.find('[', arrStart);
    if (pos == std::string::npos) return 0;
    ++pos;
    
    int imported = 0;
    
    while (pos < json.size()) {
        // Skip whitespace
        while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\n' ||
               json[pos] == '\r' || json[pos] == '\t' || json[pos] == ','))
            ++pos;
        
        if (pos >= json.size() || json[pos] == ']') break;
        
        if (json[pos] == '{') {
            // Find matching closing brace
            int depth = 1;
            size_t start = pos;
            ++pos;
            while (pos < json.size() && depth > 0) {
                if (json[pos] == '{') ++depth;
                else if (json[pos] == '}') --depth;
                ++pos;
            }
            
            std::string policyJson = json.substr(start, pos - start);
            AgentPolicy p = AgentPolicy::fromJSON(policyJson);
            
            // Assign new ID to avoid collisions, mark as imported
            p.id = generateUUID();
            p.createdBy = "import";
            p.createdAt = nowMs();
            p.modifiedAt = nowMs();
            p.appliedCount = 0;
            
            std::lock_guard<std::mutex> lock(m_mutex);
            m_policies.push_back(p);
            imported++;
        } else {
            ++pos;
        }
    }
    
    logInfo("Imported " + std::to_string(imported) + " policies");
    return imported;
}

bool PolicyEngine::exportToFile(const std::string& filePath) const {
    std::string data = exportPolicies();
    
    std::ofstream f(filePath);
    if (!f.is_open()) {
        logError("Cannot write export file: " + filePath);
        return false;
    }
    
    f << data;
    f.close();
    logInfo("Exported policies to " + filePath);
    return true;
}

int PolicyEngine::importFromFile(const std::string& filePath) {
    std::ifstream f(filePath);
    if (!f.is_open()) {
        logError("Cannot read import file: " + filePath);
        return 0;
    }
    
    std::ostringstream ss;
    ss << f.rdbuf();
    f.close();
    
    return importPolicies(ss.str());
}

// ============================================================================
// Persistence — save/load to storageDir
// ============================================================================

void PolicyEngine::save() {
    if (m_storageDir.empty()) return;
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Save policies
    {
        std::ofstream f(getPolicyFilePath());
        if (f.is_open()) {
            f << "[";
            for (size_t i = 0; i < m_policies.size(); ++i) {
                if (i > 0) f << ",\n";
                f << m_policies[i].toJSON();
            }
            f << "]";
            f.close();
            logDebug("Saved " + std::to_string(m_policies.size()) + " policies");
        } else {
            logError("Cannot save policies to " + getPolicyFilePath());
        }
    }
    
    // Save suggestions
    {
        std::ofstream f(getSuggestionsFilePath());
        if (f.is_open()) {
            f << "[";
            for (size_t i = 0; i < m_suggestions.size(); ++i) {
                if (i > 0) f << ",\n";
                f << m_suggestions[i].toJSON();
            }
            f << "]";
            f.close();
            logDebug("Saved " + std::to_string(m_suggestions.size()) + " suggestions");
        } else {
            logError("Cannot save suggestions to " + getSuggestionsFilePath());
        }
    }
}

void PolicyEngine::load() {
    if (m_storageDir.empty()) return;
    
    // Load policies
    {
        std::ifstream f(getPolicyFilePath());
        if (f.is_open()) {
            std::ostringstream ss;
            ss << f.rdbuf();
            f.close();
            
            std::string json = ss.str();
            // Simple JSON array parsing
            size_t pos = json.find('[');
            if (pos == std::string::npos) return;
            ++pos;
            
            while (pos < json.size()) {
                while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\n' ||
                       json[pos] == '\r' || json[pos] == ',' || json[pos] == '\t'))
                    ++pos;
                
                if (pos >= json.size() || json[pos] == ']') break;
                
                if (json[pos] == '{') {
                    int depth = 1;
                    size_t start = pos;
                    ++pos;
                    while (pos < json.size() && depth > 0) {
                        if (json[pos] == '{') ++depth;
                        else if (json[pos] == '}') --depth;
                        ++pos;
                    }
                    
                    m_policies.push_back(AgentPolicy::fromJSON(json.substr(start, pos - start)));
                } else {
                    ++pos;
                }
            }
            
            logInfo("Loaded " + std::to_string(m_policies.size()) + " policies from disk");
        }
    }
    
    // Load suggestions
    {
        std::ifstream f(getSuggestionsFilePath());
        if (f.is_open()) {
            std::ostringstream ss;
            ss << f.rdbuf();
            f.close();
            
            std::string json = ss.str();
            size_t pos = json.find('[');
            if (pos == std::string::npos) return;
            ++pos;
            
            while (pos < json.size()) {
                while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\n' ||
                       json[pos] == '\r' || json[pos] == ',' || json[pos] == '\t'))
                    ++pos;
                
                if (pos >= json.size() || json[pos] == ']') break;
                
                if (json[pos] == '{') {
                    int depth = 1;
                    size_t start = pos;
                    ++pos;
                    while (pos < json.size() && depth > 0) {
                        if (json[pos] == '{') ++depth;
                        else if (json[pos] == '}') --depth;
                        ++pos;
                    }
                    
                    m_suggestions.push_back(PolicySuggestion::fromJSON(json.substr(start, pos - start)));
                } else {
                    ++pos;
                }
            }
            
            logInfo("Loaded " + std::to_string(m_suggestions.size()) + " suggestions from disk");
        }
    }
}

// ============================================================================
// Statistics
// ============================================================================

std::string PolicyEngine::getStatsSummary() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    int enabled = 0, disabled = 0;
    int totalApplied = 0;
    for (const auto& p : m_policies) {
        if (p.enabled) ++enabled;
        else ++disabled;
        totalApplied += p.appliedCount;
    }
    
    int pending = 0, accepted = 0, rejected = 0;
    for (const auto& s : m_suggestions) {
        switch (s.state) {
            case PolicySuggestion::State::Pending:  ++pending;  break;
            case PolicySuggestion::State::Accepted: ++accepted; break;
            case PolicySuggestion::State::Rejected: ++rejected; break;
            default: break;
        }
    }
    
    std::ostringstream o;
    o << "{\"policies\":{\"total\":" << m_policies.size()
      << ",\"enabled\":" << enabled
      << ",\"disabled\":" << disabled
      << ",\"totalApplied\":" << totalApplied
      << "},\"suggestions\":{\"total\":" << m_suggestions.size()
      << ",\"pending\":" << pending
      << ",\"accepted\":" << accepted
      << ",\"rejected\":" << rejected
      << "},\"evaluations\":" << m_evalCount.load()
      << ",\"matches\":" << m_matchCount.load()
      << "}";
    return o.str();
}
