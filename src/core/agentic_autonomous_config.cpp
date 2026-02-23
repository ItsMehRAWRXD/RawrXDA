// ============================================================================
// agentic_autonomous_config.cpp — Agentic Autonomous Operations & Model Selection
// ============================================================================
// Production-ready: thread-safe, 1x–99x limits, audit iteration estimates,
// terminal/PWSH requirement hint for balance/speed/agenticness/autonomy.
// ============================================================================

#include "agentic_autonomous_config.h"
#include <algorithm>
#include <sstream>
#include <cstring>

namespace RawrXD {

AgenticAutonomousConfig& AgenticAutonomousConfig::instance() {
    static AgenticAutonomousConfig s;
    return s;
}

AgenticAutonomousConfig::AgenticAutonomousConfig() = default;

AgenticOperationMode AgenticAutonomousConfig::getOperationMode() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_operationMode;
}

void AgenticAutonomousConfig::setOperationMode(AgenticOperationMode mode) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_operationMode = mode;
}

bool AgenticAutonomousConfig::setOperationModeFromString(const std::string& s) {
    AgenticOperationMode mode = agentic_operation_mode_from_string(s);
    setOperationMode(mode);
    return true;
}

ModelSelectionMode AgenticAutonomousConfig::getModelSelectionMode() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_modelSelectionMode;
}

void AgenticAutonomousConfig::setModelSelectionMode(ModelSelectionMode mode) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_modelSelectionMode = mode;
}

bool AgenticAutonomousConfig::setModelSelectionModeFromString(const std::string& s) {
    ModelSelectionMode mode = model_selection_mode_from_string(s);
    setModelSelectionMode(mode);
    return true;
}

int AgenticAutonomousConfig::getPerModelInstanceCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_perModelInstanceCount;
}

void AgenticAutonomousConfig::setPerModelInstanceCount(int count) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_perModelInstanceCount = std::clamp(count, kMinPerModelInstances, kMaxPerModelInstances);
}

int AgenticAutonomousConfig::getMaxModelsInParallel() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_maxModelsInParallel;
}

void AgenticAutonomousConfig::setMaxModelsInParallel(int cap) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_maxModelsInParallel = std::clamp(cap, 1, kMaxModelsInParallel);
}

int AgenticAutonomousConfig::getCycleAgentCounter() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_cycleAgentCounter;
}

void AgenticAutonomousConfig::setCycleAgentCounter(int count) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_cycleAgentCounter = std::clamp(count, kMinCycleAgentCounter, kMaxCycleAgentCounter);
}

QualitySpeedBalance AgenticAutonomousConfig::getQualitySpeedBalance() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_qualitySpeedBalance;
}

void AgenticAutonomousConfig::setQualitySpeedBalance(QualitySpeedBalance q) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_qualitySpeedBalance = q;
}

bool AgenticAutonomousConfig::setQualitySpeedBalanceFromString(const std::string& s) {
    setQualitySpeedBalance(quality_speed_balance_from_string(s));
    return true;
}

int AgenticAutonomousConfig::estimateIterationRedoCount(const std::string& taskDescription, int complexityHint,
                                                        bool noTokenOrTimeConstraints) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    int base = 1;
    if (m_qualitySpeedBalance == QualitySpeedBalance::MAX_MODE || m_qualitySpeedBalance == QualitySpeedBalance::QualityBias)
        base = noTokenOrTimeConstraints ? 5 : 3;
    else if (m_qualitySpeedBalance == QualitySpeedBalance::SpeedBias)
        base = 1;
    else
        base = noTokenOrTimeConstraints ? 3 : 2;
    size_t len = taskDescription.size();
    int lenFactor = (len > 2000) ? 2 : (len > 500 ? 1 : 0);
    int complexity = std::clamp(complexityHint, 0, 10);
    int est = base + lenFactor + (complexity / 2) + (noTokenOrTimeConstraints ? 2 : 0);
    int capped = std::max(1, std::min(99, est * m_cycleAgentCounter));
    return capped;
}

std::string AgenticAutonomousConfig::getRecommendedTerminalRequirementHint() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_qualitySpeedBalance == QualitySpeedBalance::MAX_MODE) return "audit";
    if (m_qualitySpeedBalance == QualitySpeedBalance::SpeedBias) return "quick";
    if (m_operationMode == AgenticOperationMode::Agent || m_operationMode == AgenticOperationMode::Plan)
        return "agentic";
    return "default";
}

void AgenticAutonomousConfig::estimateProductionAuditIterations(const std::string& codebaseHint, int topNDifficult,
                                                                int* estimatedIterationRedos,
                                                                int* taskCategoryCount) const {
    if (!estimatedIterationRedos) return;
    std::lock_guard<std::mutex> lock(m_mutex);
    int topN = std::max(1, std::min(99, topNDifficult));
    int categories = 8;
    if (codebaseHint == "ship" || codebaseHint == "Ship") categories = 6;
    else if (codebaseHint == "src") categories = 12;
    int perCategory = (m_qualitySpeedBalance == QualitySpeedBalance::MAX_MODE) ? 4 : 2;
    int redos = topN * perCategory * m_cycleAgentCounter;
    redos = std::max(topN, std::min(99 * 2, redos));
    *estimatedIterationRedos = redos;
    if (taskCategoryCount) *taskCategoryCount = categories;
}

int AgenticAutonomousConfig::getInstanceCountForModel(const std::string& modelId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_modelInstanceOverrides.find(modelId);
    if (it != m_modelInstanceOverrides.end()) {
        return std::clamp(it->second, kMinPerModelInstances, kMaxPerModelInstances);
    }
    return m_perModelInstanceCount;
}

void AgenticAutonomousConfig::setInstanceCountForModel(const std::string& modelId, int count) {
    std::lock_guard<std::mutex> lock(m_mutex);
    int c = std::clamp(count, kMinPerModelInstances, kMaxPerModelInstances);
    if (c == m_perModelInstanceCount) {
        m_modelInstanceOverrides.erase(modelId);
    } else {
        m_modelInstanceOverrides[modelId] = c;
    }
}

void AgenticAutonomousConfig::clearModelInstanceOverrides() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_modelInstanceOverrides.clear();
}

int AgenticAutonomousConfig::effectiveMaxParallel(int requestedMaxParallel) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    int cap = m_maxModelsInParallel;
    if (requestedMaxParallel <= 0) return std::min(4, cap);
    return std::min(requestedMaxParallel, cap);
}

std::string AgenticAutonomousConfig::toJson() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::ostringstream oss;
    oss << "{\"operationMode\":\"" << to_string(m_operationMode)
        << "\",\"modelSelectionMode\":\"" << to_string(m_modelSelectionMode)
        << "\",\"qualitySpeedBalance\":\"" << to_string(m_qualitySpeedBalance)
        << "\",\"perModelInstances\":" << m_perModelInstanceCount
        << ",\"maxModelsInParallel\":" << m_maxModelsInParallel
        << ",\"cycleAgentCounter\":" << m_cycleAgentCounter
        << ",\"modelInstanceOverrides\":{";
    bool first = true;
    for (const auto& kv : m_modelInstanceOverrides) {
        if (!first) oss << ",";
        first = false;
        oss << "\"";
        for (char c : kv.first) {
            if (c == '"' || c == '\\') oss << '\\';
            oss << c;
        }
        oss << "\":" << kv.second;
    }
    oss << "}}";
    return oss.str();
}

std::string AgenticAutonomousConfig::toDisplayString() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::ostringstream oss;
    oss << "Operation: " << to_string(m_operationMode)
        << " | Model: " << to_string(m_modelSelectionMode)
        << " | Q/S: " << to_string(m_qualitySpeedBalance)
        << " | Per-model: " << m_perModelInstanceCount
        << " | Max parallel: " << m_maxModelsInParallel
        << " | Cycle: " << m_cycleAgentCounter << "x";
    return oss.str();
}

namespace {
    std::string extractJsonString(const std::string& json, const char* key) {
        std::string search = "\"";
        search += key;
        search += "\":\"";
        size_t pos = json.find(search);
        if (pos == std::string::npos) return "";
        pos += search.size();
        size_t end = json.find('"', pos);
        if (end == std::string::npos) return "";
        return json.substr(pos, end - pos);
    }
    int extractJsonInt(const std::string& json, const char* key) {
        std::string search = "\"";
        search += key;
        search += "\":";
        size_t pos = json.find(search);
        if (pos == std::string::npos) return -1;
        pos += search.size();
        const char* p = json.c_str() + pos;
        return static_cast<int>(std::strtol(p, nullptr, 10));
    }
}

bool AgenticAutonomousConfig::fromJson(const std::string& json) {
    std::string op = extractJsonString(json, "operationMode");
    std::string model = extractJsonString(json, "modelSelectionMode");
    std::string qs = extractJsonString(json, "qualitySpeedBalance");
    int perModel = extractJsonInt(json, "perModelInstances");
    int maxParallel = extractJsonInt(json, "maxModelsInParallel");
    int cycle = extractJsonInt(json, "cycleAgentCounter");
    if (!op.empty()) setOperationMode(agentic_operation_mode_from_string(op));
    if (!model.empty()) setModelSelectionMode(model_selection_mode_from_string(model));
    if (!qs.empty()) setQualitySpeedBalance(quality_speed_balance_from_string(qs));
    if (perModel >= 0) setPerModelInstanceCount(perModel);
    if (maxParallel >= 0) setMaxModelsInParallel(maxParallel);
    if (cycle >= 0) setCycleAgentCounter(cycle);
    return true;
}

} // namespace RawrXD
