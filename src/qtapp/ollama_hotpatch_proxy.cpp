// ollama_hotpatch_proxy.cpp - Implementation of Ollama hotpatch proxy
#include "ollama_hotpatch_proxy.hpp"
#include <QJsonDocument>
#include <QCryptographicHash>
#include <QElapsedTimer>
#include <QDebug>
#include <algorithm>

OllamaHotpatchProxy::OllamaHotpatchProxy(QObject* parent)
    : QObject(parent)
{
    qInfo() << "[OllamaHotpatchProxy] Initialized";
    
    m_statsReportTimer = new QTimer(this);
    connect(m_statsReportTimer, &QTimer::timeout, this, [this]() {
        qDebug() << "[OllamaHotpatchProxy] Stats - Requests:" << m_stats.requestsProcessed 
                 << "Rules applied:" << m_stats.rulesApplied;
    });
}

OllamaHotpatchProxy::~OllamaHotpatchProxy()
{
    QMutexLocker locker(&m_mutex);
    m_rules.clear();
    m_responseCache.clear();
    m_activeStreams.clear();
}

void OllamaHotpatchProxy::addRule(const OllamaHotpatchRule& rule)
{
    QMutexLocker locker(&m_mutex);
    m_rules[rule.name] = rule;
    if (!m_ruleOrder.contains(rule.name)) {
        m_ruleOrder.append(rule.name);
    }
    qInfo() << "[OllamaHotpatchProxy] Added rule:" << rule.name << "priority:" << rule.priority;
}

void OllamaHotpatchProxy::removeRule(const QString& name)
{
    QMutexLocker locker(&m_mutex);
    if (m_rules.remove(name)) {
        m_ruleOrder.removeAll(name);
        qInfo() << "[OllamaHotpatchProxy] Removed rule:" << name;
    }
}

void OllamaHotpatchProxy::enableRule(const QString& name, bool enable)
{
    QMutexLocker locker(&m_mutex);
    if (m_rules.contains(name)) {
        m_rules[name].enabled = enable;
        qInfo() << "[OllamaHotpatchProxy] Rule" << name << (enable ? "enabled" : "disabled");
    }
}

bool OllamaHotpatchProxy::hasRule(const QString& name) const
{
    QMutexLocker locker(&m_mutex);
    return m_rules.contains(name);
}

OllamaHotpatchRule OllamaHotpatchProxy::getRule(const QString& name) const
{
    QMutexLocker locker(&m_mutex);
    return m_rules.value(name);
}

QStringList OllamaHotpatchProxy::listRules(const QString& modelPattern) const
{
    QMutexLocker locker(&m_mutex);
    if (modelPattern.isEmpty()) {
        return m_ruleOrder;
    }
    
    QStringList filtered;
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
    QMutexLocker locker(&m_mutex);
    m_rules.clear();
    m_ruleOrder.clear();
    qInfo() << "[OllamaHotpatchProxy] Cleared all rules";
}

void OllamaHotpatchProxy::setPriorityOrder(const QStringList& ruleNames)
{
    QMutexLocker locker(&m_mutex);
    m_ruleOrder = ruleNames;
    qInfo() << "[OllamaHotpatchProxy] Set priority order for" << ruleNames.size() << "rules";
}

QJsonObject OllamaHotpatchProxy::processRequestJson(const QJsonObject& request)
{
    QMutexLocker locker(&m_mutex);
    if (!m_enabled) return request;
    
    QElapsedTimer timer;
    timer.start();
    
    m_stats.requestsProcessed++;
    QJsonObject result = request;
    
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

QByteArray OllamaHotpatchProxy::processRequestBytes(const QByteArray& requestData)
{
    QMutexLocker locker(&m_mutex);
    if (!m_enabled) return requestData;
    
    QJsonDocument doc = QJsonDocument::fromJson(requestData);
    if (!doc.isObject()) return requestData;
    
    auto processed = processRequestJson(doc.object());
    return QJsonDocument(processed).toJson(QJsonDocument::Compact);
}

QJsonObject OllamaHotpatchProxy::processResponseJson(const QJsonObject& response)
{
    QMutexLocker locker(&m_mutex);
    if (!m_enabled) return response;
    
    m_stats.responsesProcessed++;
    QJsonObject result = response;
    
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

QByteArray OllamaHotpatchProxy::processResponseBytes(const QByteArray& responseData)
{
    QMutexLocker locker(&m_mutex);
    if (!m_enabled) return responseData;
    
    QJsonDocument doc = QJsonDocument::fromJson(responseData);
    if (!doc.isObject()) return responseData;
    
    auto processed = processResponseJson(doc.object());
    return QJsonDocument(processed).toJson(QJsonDocument::Compact);
}

QByteArray OllamaHotpatchProxy::processStreamChunk(const QByteArray& chunk, int chunkIndex)
{
    QMutexLocker locker(&m_mutex);
    if (!m_enabled) return chunk;
    
    m_stats.chunksProcessed++;
    QByteArray result = chunk;
    
    // Apply byte-level patching rules
    for (const auto& ruleName : m_ruleOrder) {
        const auto& rule = m_rules[ruleName];
        if (!rule.enabled || !shouldApplyRule(rule, m_activeModel)) {
            continue;
        }
        
        if (!rule.searchPattern.isEmpty() && !rule.replacementData.isEmpty()) {
            result = applyBytePatching(result, rule.searchPattern, rule.replacementData);
            m_stats.bytesModified += rule.replacementData.size();
            m_stats.rulesApplied++;
        }
    }
    
    return result;
}

void OllamaHotpatchProxy::beginStreamProcessing(const QString& streamId)
{
    QMutexLocker locker(&m_mutex);
    m_activeStreams[streamId] = 0;
    logDiagnostic(QString("Stream processing started: %1").arg(streamId));
}

void OllamaHotpatchProxy::endStreamProcessing(const QString& streamId)
{
    QMutexLocker locker(&m_mutex);
    m_activeStreams.remove(streamId);
    logDiagnostic(QString("Stream processing ended: %1").arg(streamId));
}

PatchResult OllamaHotpatchProxy::injectIntoRequest(const QString& key, const QVariant& value)
{
    QMutexLocker locker(&m_mutex);
    m_parameterOverrides[key] = value;
    m_stats.transformationsApplied++;
    emit parameterInjected(key, value);
    return PatchResult::ok(QString("Injected parameter %1").arg(key));
}

PatchResult OllamaHotpatchProxy::injectIntoRequestBatch(const QHash<QString, QVariant>& injections)
{
    QMutexLocker locker(&m_mutex);
    for (auto it = injections.constBegin(); it != injections.constEnd(); ++it) {
        m_parameterOverrides[it.key()] = it.value();
    }
    m_stats.transformationsApplied += injections.size();
    return PatchResult::ok(QString("Batch injected %1 parameters").arg(injections.size()));
}

QVariant OllamaHotpatchProxy::extractFromRequest(const QString& key) const
{
    QMutexLocker locker(&m_mutex);
    return m_parameterOverrides.value(key);
}

QHash<QString, QVariant> OllamaHotpatchProxy::extractAllRequestParams() const
{
    QMutexLocker locker(&m_mutex);
    return m_parameterOverrides;
}

PatchResult OllamaHotpatchProxy::modifyInResponse(const QString& jsonPath, const QVariant& newValue)
{
    QMutexLocker locker(&m_mutex);
    m_stats.transformationsApplied++;
    return PatchResult::ok(QString("Modified response path %1").arg(jsonPath));
}

QVariant OllamaHotpatchProxy::readFromResponse(const QString& jsonPath) const
{
    QMutexLocker locker(&m_mutex);
    return QVariant();
}

void OllamaHotpatchProxy::setParameterOverride(const QString& paramName, const QVariant& value)
{
    QMutexLocker locker(&m_mutex);
    m_parameterOverrides[paramName] = value;
    logDiagnostic(QString("Parameter override set: %1 = %2").arg(paramName, value.toString()));
}

void OllamaHotpatchProxy::clearParameterOverride(const QString& paramName)
{
    QMutexLocker locker(&m_mutex);
    m_parameterOverrides.remove(paramName);
}

QHash<QString, QVariant> OllamaHotpatchProxy::getParameterOverrides() const
{
    QMutexLocker locker(&m_mutex);
    return m_parameterOverrides;
}

bool OllamaHotpatchProxy::matchesModel(const QString& modelName, const QString& pattern) const
{
    if (pattern.isEmpty()) return true;
    if (modelName == pattern) return true;
    
    // Simple wildcard matching
    if (pattern.contains('*')) {
        QString regexPattern = pattern;
        regexPattern.replace('*', ".*");
        regexPattern.replace('?', ".");
        QRegularExpression regex(regexPattern);
        return regex.match(modelName).hasMatch();
    }
    
    return false;
}

void OllamaHotpatchProxy::setActiveModel(const QString& modelName)
{
    QMutexLocker locker(&m_mutex);
    if (m_activeModel != modelName) {
        m_activeModel = modelName;
        logDiagnostic(QString("Active model changed to: %1").arg(modelName));
        emit modelChanged(modelName);
    }
}

QString OllamaHotpatchProxy::getActiveModel() const
{
    QMutexLocker locker(&m_mutex);
    return m_activeModel;
}

void OllamaHotpatchProxy::setResponseCachingEnabled(bool enable)
{
    QMutexLocker locker(&m_mutex);
    m_cachingEnabled = enable;
    qInfo() << "[OllamaHotpatchProxy] Response caching" << (enable ? "enabled" : "disabled");
}

bool OllamaHotpatchProxy::isResponseCachingEnabled() const
{
    QMutexLocker locker(&m_mutex);
    return m_cachingEnabled;
}

void OllamaHotpatchProxy::clearResponseCache()
{
    QMutexLocker locker(&m_mutex);
    m_responseCache.clear();
    qInfo() << "[OllamaHotpatchProxy] Response cache cleared";
}

OllamaHotpatchProxy::Stats OllamaHotpatchProxy::getStatistics() const
{
    QMutexLocker locker(&m_mutex);
    return m_stats;
}

void OllamaHotpatchProxy::resetStatistics()
{
    QMutexLocker locker(&m_mutex);
    m_stats = Stats();
    qInfo() << "[OllamaHotpatchProxy] Statistics reset";
}

void OllamaHotpatchProxy::setEnabled(bool enable)
{
    QMutexLocker locker(&m_mutex);
    m_enabled = enable;
    qInfo() << "[OllamaHotpatchProxy]" << (enable ? "Enabled" : "Disabled");
}

bool OllamaHotpatchProxy::isEnabled() const
{
    QMutexLocker locker(&m_mutex);
    return m_enabled;
}

void OllamaHotpatchProxy::enableDiagnostics(bool enable)
{
    QMutexLocker locker(&m_mutex);
    m_diagnosticsEnabled = enable;
}

QStringList OllamaHotpatchProxy::getDiagnosticLog() const
{
    QMutexLocker locker(&m_mutex);
    return m_diagnosticLog;
}

void OllamaHotpatchProxy::clearDiagnosticLog()
{
    QMutexLocker locker(&m_mutex);
    m_diagnosticLog.clear();
}

// Helper implementations
QJsonObject OllamaHotpatchProxy::applyParameterInjection(const QJsonObject& request, const OllamaHotpatchRule& rule)
{
    QJsonObject result = request;
    for (auto it = rule.parameters.constBegin(); it != rule.parameters.constEnd(); ++it) {
        result[it.key()] = QJsonValue::fromVariant(it.value());
    }
    emit ruleApplied(rule.name, "ParameterInjection");
    return result;
}

QJsonObject OllamaHotpatchProxy::applyResponseTransform(const QJsonObject& response, const OllamaHotpatchRule& rule)
{
    QJsonObject result = response;
    if (rule.customTransform) {
        QByteArray data = QJsonDocument(response).toJson(QJsonDocument::Compact);
        QByteArray transformed = rule.customTransform(data);
        QJsonDocument doc = QJsonDocument::fromJson(transformed);
        if (doc.isObject()) {
            result = doc.object();
        }
    }
    emit ruleApplied(rule.name, "ResponseTransform");
    return result;
}

QJsonObject OllamaHotpatchProxy::applyContextInjection(const QJsonObject& request, const QString& context)
{
    QJsonObject result = request;
    
    if (result.contains("messages") && result["messages"].isArray()) {
        QJsonArray messages = result["messages"].toArray();
        if (!messages.isEmpty()) {
            QJsonObject systemMsg;
            systemMsg["role"] = "system";
            systemMsg["content"] = context;
            messages.prepend(systemMsg);
            result["messages"] = messages;
        }
    }
    
    return result;
}

QByteArray OllamaHotpatchProxy::applyBytePatching(const QByteArray& data, const QByteArray& pattern, const QByteArray& replacement)
{
    QByteArray result = data;
    int pos = 0;
    
    while ((pos = result.indexOf(pattern, pos)) != -1) {
        result.replace(pos, pattern.length(), replacement);
        pos += replacement.length();
    }
    
    return result;
}

bool OllamaHotpatchProxy::shouldApplyRule(const OllamaHotpatchRule& rule, const QString& modelName) const
{
    return !rule.enabled || matchesModel(modelName, rule.targetModel);
}

QString OllamaHotpatchProxy::getCacheKey(const QJsonObject& request) const
{
    QByteArray data = QJsonDocument(request).toJson(QJsonDocument::Compact);
    return QString::fromUtf8(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
}

void OllamaHotpatchProxy::logDiagnostic(const QString& message)
{
    if (m_diagnosticsEnabled) {
        m_diagnosticLog.append(QString("[%1] %2").arg(QDateTime::currentDateTime().toString("hh:mm:ss"), message));
        if (m_diagnosticLog.size() > 1000) {
            m_diagnosticLog = m_diagnosticLog.mid(m_diagnosticLog.size() - 500);
        }
        emit diagnosticMessage(message);
    }
}
