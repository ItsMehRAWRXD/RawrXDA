// ollama_hotpatch_proxy.cpp - Implementation of Ollama hotpatch proxy
#include "ollama_hotpatch_proxy.hpp"


#include <algorithm>

OllamaHotpatchProxy::OllamaHotpatchProxy(void* parent)
    : void(parent)
{
    
    m_statsReportTimer = new void*(this);
// Qt connect removed
    });
}

OllamaHotpatchProxy::~OllamaHotpatchProxy()
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    m_rules.clear();
    m_responseCache.clear();
    m_activeStreams.clear();
}

void OllamaHotpatchProxy::addRule(const OllamaHotpatchRule& rule)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    m_rules[rule.name] = rule;
    if (!m_ruleOrder.contains(rule.name)) {
        m_ruleOrder.append(rule.name);
    }
}

void OllamaHotpatchProxy::removeRule(const std::string& name)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    if (m_rules.remove(name)) {
        m_ruleOrder.removeAll(name);
    }
}

void OllamaHotpatchProxy::enableRule(const std::string& name, bool enable)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    if (m_rules.contains(name)) {
        m_rules[name].enabled = enable;
    }
}

bool OllamaHotpatchProxy::hasRule(const std::string& name) const
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    return m_rules.contains(name);
}

OllamaHotpatchRule OllamaHotpatchProxy::getRule(const std::string& name) const
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    return m_rules.value(name);
}

std::vector<std::string> OllamaHotpatchProxy::listRules(const std::string& modelPattern) const
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    if (modelPattern.empty()) {
        return m_ruleOrder;
    }
    
    std::vector<std::string> filtered;
    for (const auto& name : m_ruleOrder) {
        const auto& rule = m_rules[name];
        if (matchesModel(m_activeModel, rule.targetModel)) {
            filtered.append(name);
        }
    }
    return filtered;
}

void OllamaHotpatchProxy::clearAllRules()
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    m_rules.clear();
    m_ruleOrder.clear();
}

void OllamaHotpatchProxy::setPriorityOrder(const std::vector<std::string>& ruleNames)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    m_ruleOrder = ruleNames;
}

void* OllamaHotpatchProxy::processRequestJson(const void*& request)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    if (!m_enabled) return request;
    
    std::chrono::steady_clock timer;
    timer.start();
    
    m_stats.requestsProcessed++;
    void* result = request;
    
    for (const auto& ruleName : m_ruleOrder) {
        const auto& rule = m_rules[ruleName];
        if (!rule.enabled || !shouldApplyRule(rule, m_activeModel)) {
            continue;
        }
        
        switch (rule.ruleType) {
        case OllamaHotpatchRule::ParameterInjection:
            result = applyParameterInjection(result, rule);
            m_stats.rulesApplied++;
            break;
        case OllamaHotpatchRule::ContextInjection:
            result = applyContextInjection(result, rule.parameters.value("context").toString());
            m_stats.rulesApplied++;
            break;
        default:
            break;
        }
    }
    
    m_stats.avgProcessingTimeMs = (m_stats.avgProcessingTimeMs * (m_stats.requestsProcessed - 1) + timer.elapsed()) / m_stats.requestsProcessed;
    return result;
}

std::vector<uint8_t> OllamaHotpatchProxy::processRequestBytes(const std::vector<uint8_t>& requestData)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    if (!m_enabled) return requestData;
    
    void* doc = void*::fromJson(requestData);
    if (!doc.isObject()) return requestData;
    
    auto processed = processRequestJson(doc.object());
    return void*(processed).toJson(void*::Compact);
}

void* OllamaHotpatchProxy::processResponseJson(const void*& response)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    if (!m_enabled) return response;
    
    m_stats.responsesProcessed++;
    void* result = response;
    
    for (const auto& ruleName : m_ruleOrder) {
        const auto& rule = m_rules[ruleName];
        if (!rule.enabled || !shouldApplyRule(rule, m_activeModel)) {
            continue;
        }
        
        if (rule.ruleType == OllamaHotpatchRule::ResponseTransform) {
            result = applyResponseTransform(result, rule);
            m_stats.rulesApplied++;
        }
    }
    
    return result;
}

std::vector<uint8_t> OllamaHotpatchProxy::processResponseBytes(const std::vector<uint8_t>& responseData)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    if (!m_enabled) return responseData;
    
    void* doc = void*::fromJson(responseData);
    if (!doc.isObject()) return responseData;
    
    auto processed = processResponseJson(doc.object());
    return void*(processed).toJson(void*::Compact);
}

std::vector<uint8_t> OllamaHotpatchProxy::processStreamChunk(const std::vector<uint8_t>& chunk, int chunkIndex)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    if (!m_enabled) return chunk;
    
    m_stats.chunksProcessed++;
    std::vector<uint8_t> result = chunk;
    
    // Apply byte-level patching rules
    for (const auto& ruleName : m_ruleOrder) {
        const auto& rule = m_rules[ruleName];
        if (!rule.enabled || !shouldApplyRule(rule, m_activeModel)) {
            continue;
        }
        
        if (!rule.searchPattern.empty() && !rule.replacementData.empty()) {
            result = applyBytePatching(result, rule.searchPattern, rule.replacementData);
            m_stats.bytesModified += rule.replacementData.size();
            m_stats.rulesApplied++;
        }
    }
    
    return result;
}

void OllamaHotpatchProxy::beginStreamProcessing(const std::string& streamId)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    m_activeStreams[streamId] = 0;
    logDiagnostic(std::string("Stream processing started: %1"));
}

void OllamaHotpatchProxy::endStreamProcessing(const std::string& streamId)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    m_activeStreams.remove(streamId);
    logDiagnostic(std::string("Stream processing ended: %1"));
}

PatchResult OllamaHotpatchProxy::injectIntoRequest(const std::string& key, const std::any& value)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    m_parameterOverrides[key] = value;
    m_stats.transformationsApplied++;
    parameterInjected(key, value);
    return PatchResult::ok(std::string("Injected parameter %1"));
}

PatchResult OllamaHotpatchProxy::injectIntoRequestBatch(const std::unordered_map<std::string, std::any>& injections)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    for (auto it = injections.constBegin(); it != injections.constEnd(); ++it) {
        m_parameterOverrides[it.key()] = it.value();
    }
    m_stats.transformationsApplied += injections.size();
    return PatchResult::ok(std::string("Batch injected %1 parameters")));
}

std::any OllamaHotpatchProxy::extractFromRequest(const std::string& key) const
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    return m_parameterOverrides.value(key);
}

std::unordered_map<std::string, std::any> OllamaHotpatchProxy::extractAllRequestParams() const
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    return m_parameterOverrides;
}

PatchResult OllamaHotpatchProxy::modifyInResponse(const std::string& jsonPath, const std::any& newValue)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    m_stats.transformationsApplied++;
    return PatchResult::ok(std::string("Modified response path %1"));
}

std::any OllamaHotpatchProxy::readFromResponse(const std::string& jsonPath) const
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    return std::any();
}

void OllamaHotpatchProxy::setParameterOverride(const std::string& paramName, const std::any& value)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    m_parameterOverrides[paramName] = value;
    logDiagnostic(std::string("Parameter override set: %1 = %2")));
}

void OllamaHotpatchProxy::clearParameterOverride(const std::string& paramName)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    m_parameterOverrides.remove(paramName);
}

std::unordered_map<std::string, std::any> OllamaHotpatchProxy::getParameterOverrides() const
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    return m_parameterOverrides;
}

bool OllamaHotpatchProxy::matchesModel(const std::string& modelName, const std::string& pattern) const
{
    if (pattern.empty()) return true;
    if (modelName == pattern) return true;
    
    // Simple wildcard matching
    if (pattern.contains('*')) {
        std::string regexPattern = pattern;
        regexPattern.replace('*', ".*");
        regexPattern.replace('?', ".");
        std::regex regex(regexPattern);
        return regex.match(modelName).hasMatch();
    }
    
    return false;
}

void OllamaHotpatchProxy::setActiveModel(const std::string& modelName)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    if (m_activeModel != modelName) {
        m_activeModel = modelName;
        logDiagnostic(std::string("Active model changed to: %1"));
        modelChanged(modelName);
    }
}

std::string OllamaHotpatchProxy::getActiveModel() const
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    return m_activeModel;
}

void OllamaHotpatchProxy::setResponseCachingEnabled(bool enable)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    m_cachingEnabled = enable;
}

bool OllamaHotpatchProxy::isResponseCachingEnabled() const
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    return m_cachingEnabled;
}

void OllamaHotpatchProxy::clearResponseCache()
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    m_responseCache.clear();
}

OllamaHotpatchProxy::Stats OllamaHotpatchProxy::getStatistics() const
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    return m_stats;
}

void OllamaHotpatchProxy::resetStatistics()
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    m_stats = Stats();
}

void OllamaHotpatchProxy::setEnabled(bool enable)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    m_enabled = enable;
}

bool OllamaHotpatchProxy::isEnabled() const
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    return m_enabled;
}

void OllamaHotpatchProxy::enableDiagnostics(bool enable)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    m_diagnosticsEnabled = enable;
}

std::vector<std::string> OllamaHotpatchProxy::getDiagnosticLog() const
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    return m_diagnosticLog;
}

void OllamaHotpatchProxy::clearDiagnosticLog()
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    m_diagnosticLog.clear();
}

// Helper implementations
void* OllamaHotpatchProxy::applyParameterInjection(const void*& request, const OllamaHotpatchRule& rule)
{
    void* result = request;
    for (auto it = rule.parameters.constBegin(); it != rule.parameters.constEnd(); ++it) {
        result[it.key()] = void*::fromVariant(it.value());
    }
    ruleApplied(rule.name, "ParameterInjection");
    return result;
}

void* OllamaHotpatchProxy::applyResponseTransform(const void*& response, const OllamaHotpatchRule& rule)
{
    void* result = response;
    if (rule.customTransform) {
        std::vector<uint8_t> data = void*(response).toJson(void*::Compact);
        std::vector<uint8_t> transformed = rule.customTransform(data);
        void* doc = void*::fromJson(transformed);
        if (doc.isObject()) {
            result = doc.object();
        }
    }
    ruleApplied(rule.name, "ResponseTransform");
    return result;
}

void* OllamaHotpatchProxy::applyContextInjection(const void*& request, const std::string& context)
{
    void* result = request;
    
    if (result.contains("messages") && result["messages"].isArray()) {
        void* messages = result["messages"].toArray();
        if (!messages.empty()) {
            void* systemMsg;
            systemMsg["role"] = "system";
            systemMsg["content"] = context;
            messages.prepend(systemMsg);
            result["messages"] = messages;
        }
    }
    
    return result;
}

std::vector<uint8_t> OllamaHotpatchProxy::applyBytePatching(const std::vector<uint8_t>& data, const std::vector<uint8_t>& pattern, const std::vector<uint8_t>& replacement)
{
    std::vector<uint8_t> result = data;
    int pos = 0;
    
    while ((pos = result.indexOf(pattern, pos)) != -1) {
        result.replace(pos, pattern.length(), replacement);
        pos += replacement.length();
    }
    
    return result;
}

bool OllamaHotpatchProxy::shouldApplyRule(const OllamaHotpatchRule& rule, const std::string& modelName) const
{
    return !rule.enabled || matchesModel(modelName, rule.targetModel);
}

std::string OllamaHotpatchProxy::getCacheKey(const void*& request) const
{
    std::vector<uint8_t> data = void*(request).toJson(void*::Compact);
    return std::string::fromUtf8(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
}

void OllamaHotpatchProxy::logDiagnostic(const std::string& message)
{
    if (m_diagnosticsEnabled) {
        m_diagnosticLog.append(std::string("[%1] %2").toString("hh:mm:ss"), message));
        if (m_diagnosticLog.size() > 1000) {
            m_diagnosticLog = m_diagnosticLog.mid(m_diagnosticLog.size() - 500);
        }
        diagnosticMessage(message);
    }
}


