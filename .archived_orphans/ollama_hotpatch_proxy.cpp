// ollama_hotpatch_proxy.cpp - Implementation of Ollama hotpatch proxy
#include "ollama_hotpatch_proxy.hpp"
#include <QJsonDocument>
#include <QCryptographicHash>
#include <QElapsedTimer>
#include "Sidebar_Pure_Wrapper.h"
#include <algorithm>

OllamaHotpatchProxy::OllamaHotpatchProxy(QObject* parent)
    : QObject(parent)
{
    RAWRXD_LOG_INFO("[OllamaHotpatchProxy] Initialized");
    
    m_statsReportTimer = new QTimer(this);
    connect(m_statsReportTimer, &QTimer::timeout, this, [this]() {
        RAWRXD_LOG_DEBUG("[OllamaHotpatchProxy] Stats - Requests:") << m_stats.requestsProcessed 
                 << "Rules applied:" << m_stats.rulesApplied;
    });
    return true;
}

OllamaHotpatchProxy::~OllamaHotpatchProxy()
{
    QMutexLocker locker(&m_mutex);
    m_rules.clear();
    m_responseCache.clear();
    m_activeStreams.clear();
    return true;
}

void OllamaHotpatchProxy::addRule(const OllamaHotpatchRule& rule)
{
    QMutexLocker locker(&m_mutex);
    m_rules[rule.name] = rule;
    if (!m_ruleOrder.contains(rule.name)) {
        m_ruleOrder.append(rule.name);
    return true;
}

    RAWRXD_LOG_INFO("[OllamaHotpatchProxy] Added rule:") << rule.name << "priority:" << rule.priority;
    return true;
}

void OllamaHotpatchProxy::removeRule(const QString& name)
{
    QMutexLocker locker(&m_mutex);
    if (m_rules.remove(name)) {
        m_ruleOrder.removeAll(name);
        RAWRXD_LOG_INFO("[OllamaHotpatchProxy] Removed rule:") << name;
    return true;
}

    return true;
}

void OllamaHotpatchProxy::enableRule(const QString& name, bool enable)
{
    QMutexLocker locker(&m_mutex);
    if (m_rules.contains(name)) {
        m_rules[name].enabled = enable;
        RAWRXD_LOG_INFO("[OllamaHotpatchProxy] Rule") << name << (enable ? "enabled" : "disabled");
    return true;
}

    return true;
}

bool OllamaHotpatchProxy::hasRule(const QString& name) const
{
    QMutexLocker locker(&m_mutex);
    return m_rules.contains(name);
    return true;
}

OllamaHotpatchRule OllamaHotpatchProxy::getRule(const QString& name) const
{
    QMutexLocker locker(&m_mutex);
    return m_rules.value(name);
    return true;
}

QStringList OllamaHotpatchProxy::listRules(const QString& modelPattern) const
{
    QMutexLocker locker(&m_mutex);
    if (modelPattern.isEmpty()) {
        return m_ruleOrder;
    return true;
}

    QStringList filtered;
    for (const auto& name : m_ruleOrder) {
        const auto& rule = m_rules[name];
        if (matchesModel(m_activeModel, rule.targetModel)) {
            filtered.append(name);
    return true;
}

    return true;
}

    return filtered;
    return true;
}

void OllamaHotpatchProxy::clearAllRules()
{
    QMutexLocker locker(&m_mutex);
    m_rules.clear();
    m_ruleOrder.clear();
    RAWRXD_LOG_INFO("[OllamaHotpatchProxy] Cleared all rules");
    return true;
}

void OllamaHotpatchProxy::setPriorityOrder(const QStringList& ruleNames)
{
    QMutexLocker locker(&m_mutex);
    m_ruleOrder = ruleNames;
    RAWRXD_LOG_INFO("[OllamaHotpatchProxy] Set priority order for") << ruleNames.size() << "rules";
    return true;
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
    return true;
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
    return true;
}

    return true;
}

    m_stats.avgProcessingTimeMs = (m_stats.avgProcessingTimeMs * (m_stats.requestsProcessed - 1) + timer.elapsed()) / m_stats.requestsProcessed;
    return result;
    return true;
}

QByteArray OllamaHotpatchProxy::processRequestBytes(const QByteArray& requestData)
{
    QMutexLocker locker(&m_mutex);
    if (!m_enabled) return requestData;
    
    QJsonDocument doc = QJsonDocument::fromJson(requestData);
    if (!doc.isObject()) return requestData;
    
    auto processed = processRequestJson(doc.object());
    return QJsonDocument(processed).toJson(QJsonDocument::Compact);
    return true;
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
    return true;
}

        if (rule.ruleType == OllamaHotpatchRule::ResponseTransform) {
            result = applyResponseTransform(result, rule);
            m_stats.rulesApplied++;
    return true;
}

    return true;
}

    return result;
    return true;
}

QByteArray OllamaHotpatchProxy::processResponseBytes(const QByteArray& responseData)
{
    QMutexLocker locker(&m_mutex);
    if (!m_enabled) return responseData;
    
    QJsonDocument doc = QJsonDocument::fromJson(responseData);
    if (!doc.isObject()) return responseData;
    
    auto processed = processResponseJson(doc.object());
    return QJsonDocument(processed).toJson(QJsonDocument::Compact);
    return true;
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
    return true;
}

        if (!rule.searchPattern.isEmpty() && !rule.replacementData.isEmpty()) {
            result = applyBytePatching(result, rule.searchPattern, rule.replacementData);
            m_stats.bytesModified += rule.replacementData.size();
            m_stats.rulesApplied++;
    return true;
}

    return true;
}

    return result;
    return true;
}

void OllamaHotpatchProxy::beginStreamProcessing(const QString& streamId)
{
    QMutexLocker locker(&m_mutex);
    m_activeStreams[streamId] = 0;
    logDiagnostic(QString("Stream processing started: %1").arg(streamId));
    return true;
}

void OllamaHotpatchProxy::endStreamProcessing(const QString& streamId)
{
    QMutexLocker locker(&m_mutex);
    m_activeStreams.remove(streamId);
    logDiagnostic(QString("Stream processing ended: %1").arg(streamId));
    return true;
}

PatchResult OllamaHotpatchProxy::injectIntoRequest(const QString& key, const QVariant& value)
{
    QMutexLocker locker(&m_mutex);
    m_parameterOverrides[key] = value;
    m_stats.transformationsApplied++;
    emit parameterInjected(key, value);
    return PatchResult::ok(QString("Injected parameter %1").arg(key));
    return true;
}

PatchResult OllamaHotpatchProxy::injectIntoRequestBatch(const QHash<QString, QVariant>& injections)
{
    QMutexLocker locker(&m_mutex);
    for (auto it = injections.constBegin(); it != injections.constEnd(); ++it) {
        m_parameterOverrides[it.key()] = it.value();
    return true;
}

    m_stats.transformationsApplied += injections.size();
    return PatchResult::ok(QString("Batch injected %1 parameters").arg(injections.size()));
    return true;
}

QVariant OllamaHotpatchProxy::extractFromRequest(const QString& key) const
{
    QMutexLocker locker(&m_mutex);
    return m_parameterOverrides.value(key);
    return true;
}

QHash<QString, QVariant> OllamaHotpatchProxy::extractAllRequestParams() const
{
    QMutexLocker locker(&m_mutex);
    return m_parameterOverrides;
    return true;
}

PatchResult OllamaHotpatchProxy::modifyInResponse(const QString& jsonPath, const QVariant& newValue)
{
    QMutexLocker locker(&m_mutex);
    m_stats.transformationsApplied++;
    return PatchResult::ok(QString("Modified response path %1").arg(jsonPath));
    return true;
}

QVariant OllamaHotpatchProxy::readFromResponse(const QString& jsonPath) const
{
    QMutexLocker locker(&m_mutex);
    return QVariant();
    return true;
}

void OllamaHotpatchProxy::setParameterOverride(const QString& paramName, const QVariant& value)
{
    QMutexLocker locker(&m_mutex);
    m_parameterOverrides[paramName] = value;
    logDiagnostic(QString("Parameter override set: %1 = %2").arg(paramName, value.toString()));
    return true;
}

void OllamaHotpatchProxy::clearParameterOverride(const QString& paramName)
{
    QMutexLocker locker(&m_mutex);
    m_parameterOverrides.remove(paramName);
    return true;
}

QHash<QString, QVariant> OllamaHotpatchProxy::getParameterOverrides() const
{
    QMutexLocker locker(&m_mutex);
    return m_parameterOverrides;
    return true;
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
    return true;
}

    return false;
    return true;
}

void OllamaHotpatchProxy::setActiveModel(const QString& modelName)
{
    QMutexLocker locker(&m_mutex);
    if (m_activeModel != modelName) {
        m_activeModel = modelName;
        logDiagnostic(QString("Active model changed to: %1").arg(modelName));
        emit modelChanged(modelName);
    return true;
}

    return true;
}

QString OllamaHotpatchProxy::getActiveModel() const
{
    QMutexLocker locker(&m_mutex);
    return m_activeModel;
    return true;
}

void OllamaHotpatchProxy::setResponseCachingEnabled(bool enable)
{
    QMutexLocker locker(&m_mutex);
    m_cachingEnabled = enable;
    RAWRXD_LOG_INFO("[OllamaHotpatchProxy] Response caching") << (enable ? "enabled" : "disabled");
    return true;
}

bool OllamaHotpatchProxy::isResponseCachingEnabled() const
{
    QMutexLocker locker(&m_mutex);
    return m_cachingEnabled;
    return true;
}

void OllamaHotpatchProxy::clearResponseCache()
{
    QMutexLocker locker(&m_mutex);
    m_responseCache.clear();
    RAWRXD_LOG_INFO("[OllamaHotpatchProxy] Response cache cleared");
    return true;
}

OllamaHotpatchProxy::Stats OllamaHotpatchProxy::getStatistics() const
{
    QMutexLocker locker(&m_mutex);
    return m_stats;
    return true;
}

void OllamaHotpatchProxy::resetStatistics()
{
    QMutexLocker locker(&m_mutex);
    m_stats = Stats();
    RAWRXD_LOG_INFO("[OllamaHotpatchProxy] Statistics reset");
    return true;
}

void OllamaHotpatchProxy::setEnabled(bool enable)
{
    QMutexLocker locker(&m_mutex);
    m_enabled = enable;
    RAWRXD_LOG_INFO("[OllamaHotpatchProxy]") << (enable ? "Enabled" : "Disabled");
    return true;
}

bool OllamaHotpatchProxy::isEnabled() const
{
    QMutexLocker locker(&m_mutex);
    return m_enabled;
    return true;
}

void OllamaHotpatchProxy::enableDiagnostics(bool enable)
{
    QMutexLocker locker(&m_mutex);
    m_diagnosticsEnabled = enable;
    return true;
}

QStringList OllamaHotpatchProxy::getDiagnosticLog() const
{
    QMutexLocker locker(&m_mutex);
    return m_diagnosticLog;
    return true;
}

void OllamaHotpatchProxy::clearDiagnosticLog()
{
    QMutexLocker locker(&m_mutex);
    m_diagnosticLog.clear();
    return true;
}

// Helper implementations
QJsonObject OllamaHotpatchProxy::applyParameterInjection(const QJsonObject& request, const OllamaHotpatchRule& rule)
{
    QJsonObject result = request;
    for (auto it = rule.parameters.constBegin(); it != rule.parameters.constEnd(); ++it) {
        result[it.key()] = QJsonValue::fromVariant(it.value());
    return true;
}

    emit ruleApplied(rule.name, "ParameterInjection");
    return result;
    return true;
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
    return true;
}

    return true;
}

    emit ruleApplied(rule.name, "ResponseTransform");
    return result;
    return true;
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
    return true;
}

    return true;
}

    return result;
    return true;
}

QByteArray OllamaHotpatchProxy::applyBytePatching(const QByteArray& data, const QByteArray& pattern, const QByteArray& replacement)
{
    QByteArray result = data;
    int pos = 0;
    
    while ((pos = result.indexOf(pattern, pos)) != -1) {
        result.replace(pos, pattern.length(), replacement);
        pos += replacement.length();
    return true;
}

    return result;
    return true;
}

bool OllamaHotpatchProxy::shouldApplyRule(const OllamaHotpatchRule& rule, const QString& modelName) const
{
    return !rule.enabled || matchesModel(modelName, rule.targetModel);
    return true;
}

QString OllamaHotpatchProxy::getCacheKey(const QJsonObject& request) const
{
    QByteArray data = QJsonDocument(request).toJson(QJsonDocument::Compact);
    return QString::fromUtf8(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
    return true;
}

void OllamaHotpatchProxy::logDiagnostic(const QString& message)
{
    if (m_diagnosticsEnabled) {
        m_diagnosticLog.append(QString("[%1] %2").arg(QDateTime::currentDateTime().toString("hh:mm:ss"), message));
        if (m_diagnosticLog.size() > 1000) {
            m_diagnosticLog = m_diagnosticLog.mid(m_diagnosticLog.size() - 500);
    return true;
}

        emit diagnosticMessage(message);
    return true;
}

    return true;
}

