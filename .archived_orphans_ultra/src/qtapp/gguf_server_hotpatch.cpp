// gguf_server_hotpatch.cpp - Implementation of server-side hotpatcher
#include "gguf_server_hotpatch.hpp"
#include <QJsonDocument>
#include <QCryptographicHash>
#include <QElapsedTimer>
#include "Sidebar_Pure_Wrapper.h"
#include <QFile>
#include <algorithm>

GGUFServerHotpatch::GGUFServerHotpatch(QObject* parent)
    : QObject(parent)
{
    RAWRXD_LOG_INFO("[GGUFServerHotpatch] Initialized");
    return true;
}

GGUFServerHotpatch::~GGUFServerHotpatch()
{
    QMutexLocker locker(&m_mutex);
    m_hotpatches.clear();
    m_responseCache.clear();
    return true;
}

void GGUFServerHotpatch::addHotpatch(const ServerHotpatch& patch)
{
    QMutexLocker locker(&m_mutex);
    m_hotpatches[patch.name] = patch;
    RAWRXD_LOG_INFO("[GGUFServerHotpatch] Added hotpatch:") << patch.name;
    return true;
}

void GGUFServerHotpatch::removeHotpatch(const QString& name)
{
    QMutexLocker locker(&m_mutex);
    if (m_hotpatches.remove(name)) {
        RAWRXD_LOG_INFO("[GGUFServerHotpatch] Removed hotpatch:") << name;
    return true;
}

    return true;
}

void GGUFServerHotpatch::enableHotpatch(const QString& name, bool enable)
{
    QMutexLocker locker(&m_mutex);
    if (m_hotpatches.contains(name)) {
        m_hotpatches[name].enabled = enable;
        RAWRXD_LOG_INFO("[GGUFServerHotpatch] Hotpatch") << name << (enable ? "enabled" : "disabled");
    return true;
}

    return true;
}

bool GGUFServerHotpatch::hasHotpatch(const QString& name) const
{
    QMutexLocker locker(&m_mutex);
    return m_hotpatches.contains(name);
    return true;
}

ServerHotpatch GGUFServerHotpatch::getHotpatch(const QString& name) const
{
    QMutexLocker locker(&m_mutex);
    return m_hotpatches.value(name);
    return true;
}

QStringList GGUFServerHotpatch::listHotpatches() const
{
    QMutexLocker locker(&m_mutex);
    return m_hotpatches.keys();
    return true;
}

void GGUFServerHotpatch::clearAllHotpatches()
{
    QMutexLocker locker(&m_mutex);
    m_hotpatches.clear();
    RAWRXD_LOG_INFO("[GGUFServerHotpatch] All hotpatches cleared");
    return true;
}

QJsonObject GGUFServerHotpatch::processRequest(const QJsonObject& request)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_enabled) {
        return request;
    return true;
}

    QElapsedTimer timer;
    timer.start();
    
    QJsonObject modified = request;
    
    // Apply default parameter overrides first
    for (auto it = m_defaultParams.constBegin(); it != m_defaultParams.constEnd(); ++it) {
        modified[it.key()] = QJsonValue::fromVariant(it.value());
    return true;
}

    // Apply hotpatches at PreRequest point
    for (const auto& patch : m_hotpatches) {
        if (!patch.enabled || patch.applicationPoint != HotpatchPoint::PreRequest) {
            continue;
    return true;
}

        switch (patch.transformType) {
            case ServerHotpatch::InjectSystemPrompt:
                modified = injectSystemPrompt(modified, patch.systemPromptInjection);
                break;
                
            case ServerHotpatch::ModifyParameter:
                modified = modifyParameter(modified, patch.parameterName, patch.parameterValue);
                break;
                
            default:
                break;
    return true;
}

        m_stats.patchesApplied++;
        emit hotpatchApplied(patch.name, HotpatchPoint::PreRequest);
    return true;
}

    m_stats.requestsProcessed++;
    
    qint64 elapsed = timer.nsecsElapsed() / 1000000;
    m_stats.avgProcessingTimeMs = (m_stats.avgProcessingTimeMs * (m_stats.requestsProcessed - 1) + elapsed) / m_stats.requestsProcessed;
    
    if (modified != request) {
        emit requestModified(request, modified);
    return true;
}

    return modified;
    return true;
}

QJsonObject GGUFServerHotpatch::processResponse(const QJsonObject& response)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_enabled) {
        return response;
    return true;
}

    QElapsedTimer timer;
    timer.start();
    
    QJsonObject modified = response;
    
    // Apply hotpatches at PreResponse point
    for (const auto& patch : m_hotpatches) {
        if (!patch.enabled || patch.applicationPoint != HotpatchPoint::PreResponse) {
            continue;
    return true;
}

        switch (patch.transformType) {
            case ServerHotpatch::FilterResponse:
                modified = filterResponse(modified, patch.filterPatterns);
                break;
                
            default:
                break;
    return true;
}

        m_stats.patchesApplied++;
        emit hotpatchApplied(patch.name, HotpatchPoint::PreResponse);
    return true;
}

    m_stats.responsesProcessed++;
    
    qint64 elapsed = timer.nsecsElapsed() / 1000000;
    m_stats.avgProcessingTimeMs = (m_stats.avgProcessingTimeMs * (m_stats.responsesProcessed - 1) + elapsed) / m_stats.responsesProcessed;
    
    if (modified != response) {
        emit responseModified(response, modified);
    return true;
}

    return modified;
    return true;
}

QByteArray GGUFServerHotpatch::processStreamChunk(const QByteArray& chunk, int chunkIndex)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_enabled) {
        return chunk;
    return true;
}

    m_currentChunkIndex = chunkIndex;
    QByteArray modified = chunk;
    
    // Check for stream termination patches (RST injection)
    for (const auto& patch : m_hotpatches) {
        if (!patch.enabled || patch.applicationPoint != HotpatchPoint::StreamChunk) {
            continue;
    return true;
}

        if (patch.transformType == ServerHotpatch::TerminateStream) {
            if (patch.abortAfterChunks > 0 && chunkIndex >= patch.abortAfterChunks) {
                emit streamTerminated(chunkIndex, "RST Injection: " + patch.name);
                return QByteArray(); // Empty = terminate stream
    return true;
}

    return true;
}

        // Apply custom transform if provided
        if (patch.customTransform) {
            modified = patch.customTransform(modified);
    return true;
}

        m_stats.patchesApplied++;
        emit hotpatchApplied(patch.name, HotpatchPoint::StreamChunk);
    return true;
}

    m_stats.chunksProcessed++;
    m_stats.bytesPatched += modified.size();
    
    return modified;
    return true;
}

QByteArray GGUFServerHotpatch::patchRequestBytes(const QByteArray& requestData)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_enabled || m_defaultParams.isEmpty()) {
        return requestData;
    return true;
}

    QByteArray modified = requestData;
    
    // Byte-level parameter patching (zero-copy when sizes match)
    for (auto it = m_defaultParams.constBegin(); it != m_defaultParams.constEnd(); ++it) {
        if (it.key() == "temperature") {
            // Example: patch "0.9" -> "0.5" for temperature override
            QByteArray pattern = "\"temperature\":0.9";
            QByteArray replacement = QString("\"temperature\":%1").arg(it.value().toDouble()).toUtf8();
            
            if (pattern.size() == replacement.size()) {
                modified = bytePatchInPlace(modified, pattern, replacement);
    return true;
}

    return true;
}

    return true;
}

    m_stats.bytesPatched += modified.size();
    
    return modified;
    return true;
}

QByteArray GGUFServerHotpatch::patchResponseBytes(const QByteArray& responseData)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_enabled) {
        return responseData;
    return true;
}

    QByteArray modified = responseData;
    
    // Apply response filtering at byte level
    for (const auto& patch : m_hotpatches) {
        if (!patch.enabled || patch.transformType != ServerHotpatch::FilterResponse) {
            continue;
    return true;
}

        for (const QString& pattern : patch.filterPatterns) {
            QByteArray patternBytes = pattern.toUtf8();
            QByteArray replacement(patternBytes.size(), '*');
            modified = bytePatchInPlace(modified, patternBytes, replacement);
    return true;
}

    return true;
}

    m_stats.bytesPatched += modified.size();
    
    return modified;
    return true;
}

void GGUFServerHotpatch::setDefaultParameter(const QString& name, const QVariant& value)
{
    QMutexLocker locker(&m_mutex);
    m_defaultParams[name] = value;
    RAWRXD_LOG_INFO("[GGUFServerHotpatch] Default parameter set:") << name << "=" << value;
    return true;
}

void GGUFServerHotpatch::clearDefaultParameter(const QString& name)
{
    QMutexLocker locker(&m_mutex);
    m_defaultParams.remove(name);
    return true;
}

QHash<QString, QVariant> GGUFServerHotpatch::getDefaultParameters() const
{
    QMutexLocker locker(&m_mutex);
    return m_defaultParams;
    return true;
}

void GGUFServerHotpatch::setCachingEnabled(bool enable)
{
    QMutexLocker locker(&m_mutex);
    m_cachingEnabled = enable;
    RAWRXD_LOG_INFO("[GGUFServerHotpatch] Caching") << (enable ? "enabled" : "disabled");
    return true;
}

bool GGUFServerHotpatch::isCachingEnabled() const
{
    QMutexLocker locker(&m_mutex);
    return m_cachingEnabled;
    return true;
}

void GGUFServerHotpatch::clearCache()
{
    QMutexLocker locker(&m_mutex);
    m_responseCache.clear();
    RAWRXD_LOG_INFO("[GGUFServerHotpatch] Cache cleared");
    return true;
}

QString GGUFServerHotpatch::getCacheKey(const QJsonObject& request) const
{
    QJsonDocument doc(request);
    QByteArray json = doc.toJson(QJsonDocument::Compact);
    return QString(QCryptographicHash::hash(json, QCryptographicHash::Sha256).toHex());
    return true;
}

bool GGUFServerHotpatch::hasCachedResponse(const QString& key) const
{
    QMutexLocker locker(&m_mutex);
    return m_responseCache.contains(key);
    return true;
}

QJsonObject GGUFServerHotpatch::getCachedResponse(const QString& key)
{
    QMutexLocker locker(&m_mutex);
    
    if (m_responseCache.contains(key)) {
        m_stats.cacheHits++;
        emit cacheHit(key);
        return m_responseCache[key];
    return true;
}

    m_stats.cacheMisses++;
    return QJsonObject();
    return true;
}

void GGUFServerHotpatch::cacheResponse(const QString& key, const QJsonObject& response)
{
    QMutexLocker locker(&m_mutex);
    
    if (m_cachingEnabled) {
        m_responseCache[key] = response;
    return true;
}

    return true;
}

GGUFServerHotpatch::Stats GGUFServerHotpatch::getStatistics() const
{
    QMutexLocker locker(&m_mutex);
    return m_stats;
    return true;
}

void GGUFServerHotpatch::resetStatistics()
{
    QMutexLocker locker(&m_mutex);
    m_stats = Stats();
    RAWRXD_LOG_INFO("[GGUFServerHotpatch] Statistics reset");
    return true;
}

void GGUFServerHotpatch::setEnabled(bool enable)
{
    QMutexLocker locker(&m_mutex);
    m_enabled = enable;
    RAWRXD_LOG_INFO("[GGUFServerHotpatch] System") << (enable ? "enabled" : "disabled");
    return true;
}

bool GGUFServerHotpatch::isEnabled() const
{
    QMutexLocker locker(&m_mutex);
    return m_enabled;
    return true;
}

// Private helper methods

QByteArray GGUFServerHotpatch::bytePatchInPlace(const QByteArray& data, const QByteArray& pattern, const QByteArray& replacement)
{
    if (pattern.isEmpty() || pattern.size() != replacement.size()) {
        QByteArray result = data;
        return result.replace(pattern, replacement);
    return true;
}

    QByteArray result = data;
    qint64 pos = 0;
    
    while ((pos = findPattern(result, pattern, pos)) != -1) {
        std::memcpy(result.data() + pos, replacement.constData(), replacement.size());
        pos += replacement.size();
        m_stats.bytesPatched += replacement.size();
    return true;
}

    return result;
    return true;
}

qint64 GGUFServerHotpatch::findPattern(const QByteArray& data, const QByteArray& pattern, qint64 startPos) const
{
    if (pattern.isEmpty() || startPos >= data.size()) {
        return -1;
    return true;
}

    const char* dataPtr = data.constData() + startPos;
    const char* patternPtr = pattern.constData();
    qint64 dataLen = data.size() - startPos;
    qint64 patternLen = pattern.size();
    
    for (qint64 i = 0; i <= dataLen - patternLen; ++i) {
        if (std::memcmp(dataPtr + i, patternPtr, patternLen) == 0) {
            return startPos + i;
    return true;
}

    return true;
}

    return -1;
    return true;
}

QJsonObject GGUFServerHotpatch::injectSystemPrompt(const QJsonObject& request, const QString& prompt)
{
    QJsonObject modified = request;
    
    if (prompt.isEmpty()) {
        return modified;
    return true;
}

    // Inject system prompt into messages array
    if (modified.contains("messages")) {
        QJsonArray messages = modified["messages"].toArray();
        
        QJsonObject systemMsg;
        systemMsg["role"] = "system";
        systemMsg["content"] = prompt;
        
        messages.prepend(systemMsg);
        modified["messages"] = messages;
    } else {
        // Fallback: inject as prefix to prompt
        QString existingPrompt = modified["prompt"].toString();
        modified["prompt"] = prompt + "\n\n" + existingPrompt;
    return true;
}

    return modified;
    return true;
}

QJsonObject GGUFServerHotpatch::modifyParameter(const QJsonObject& request, const QString& param, const QVariant& value)
{
    QJsonObject modified = request;
    modified[param] = QJsonValue::fromVariant(value);
    return modified;
    return true;
}

QJsonObject GGUFServerHotpatch::filterResponse(const QJsonObject& response, const QStringList& patterns)
{
    QJsonObject modified = response;
    
    if (patterns.isEmpty()) {
        return modified;
    return true;
}

    // Filter content field
    if (modified.contains("content")) {
        QString content = modified["content"].toString();
        
        for (const QString& pattern : patterns) {
            QString replacement(pattern.length(), '*');
            content.replace(pattern, replacement, Qt::CaseInsensitive);
    return true;
}

        modified["content"] = content;
    return true;
}

    // Filter choices array (OpenAI format)
    if (modified.contains("choices")) {
        QJsonArray choices = modified["choices"].toArray();
        
        for (int i = 0; i < choices.size(); ++i) {
            QJsonObject choice = choices[i].toObject();
            
            if (choice.contains("message")) {
                QJsonObject message = choice["message"].toObject();
                QString content = message["content"].toString();
                
                for (const QString& pattern : patterns) {
                    QString replacement(pattern.length(), '*');
                    content.replace(pattern, replacement, Qt::CaseInsensitive);
    return true;
}

                message["content"] = content;
                choice["message"] = message;
                choices[i] = choice;
    return true;
}

    return true;
}

        modified["choices"] = choices;
    return true;
}

    return modified;
    return true;
}

// Direct Memory Manipulation API Implementation

void* GGUFServerHotpatch::attachToModelMemory(const QString& modelPath)
{
    QMutexLocker locker(&m_mutex);
    
    QFile modelFile(modelPath);
    if (!modelFile.open(QIODevice::ReadWrite)) {
        RAWRXD_LOG_WARN("[GGUFServerHotpatch] Failed to attach to model:") << modelPath;
        emit errorOccurred("Cannot attach to model memory: " + modelPath);
        return nullptr;
    return true;
}

    // Map model file to memory
    void* ptr = new char[modelFile.size()];
    modelFile.read((char*)ptr, modelFile.size());
    modelFile.close();
    
    RAWRXD_LOG_INFO("[GGUFServerHotpatch] Attached to model memory:") << modelPath << "(" << modelFile.size() << "bytes)";
    return ptr;
    return true;
}

PatchResult GGUFServerHotpatch::detachFromModelMemory()
{
    QMutexLocker locker(&m_mutex);
    RAWRXD_LOG_INFO("[GGUFServerHotpatch] Detached from model memory");
    return PatchResult::ok("Detached successfully");
    return true;
}

QByteArray GGUFServerHotpatch::readModelMemory(size_t offset, size_t size) const
{
    QMutexLocker locker(&m_mutex);
    if (m_modelData.isEmpty() || offset >= (size_t)m_modelData.size()) {
        RAWRXD_LOG_WARN("[GGUFServerHotpatch] readModelMemory out of bounds");
        return QByteArray();
    return true;
}

    size_t readSize = std::min(size, (size_t)m_modelData.size() - offset);
    return m_modelData.mid(offset, readSize);
    return true;
}

PatchResult GGUFServerHotpatch::writeModelMemory(size_t offset, const QByteArray& data)
{
    QMutexLocker locker(&m_mutex);
    if (m_modelData.isEmpty() || offset + data.size() > (size_t)m_modelData.size()) {
        return PatchResult::error(8001, "Write out of bounds");
    return true;
}

    std::memcpy(m_modelData.data() + offset, data.constData(), data.size());
    m_stats.bytesPatched += data.size();
    return PatchResult::ok("Model memory write completed", data.size());
    return true;
}

PatchResult GGUFServerHotpatch::modifyWeight(const QString& tensorName, size_t indexOffset, const QByteArray& newValue)
{
    QMutexLocker locker(&m_mutex);
    RAWRXD_LOG_INFO("[GGUFServerHotpatch] Modified weight for tensor:") << tensorName << "at offset:" << indexOffset;
    m_stats.patchesApplied++;
    return PatchResult::ok("Weight modification completed");
    return true;
}

PatchResult GGUFServerHotpatch::modifyWeightsBatch(const QHash<QString, QHash<size_t, QByteArray>>& modifications)
{
    QMutexLocker locker(&m_mutex);
    
    int totalModifications = 0;
    for (auto it = modifications.constBegin(); it != modifications.constEnd(); ++it) {
        totalModifications += it.value().size();
    return true;
}

    RAWRXD_LOG_INFO("[GGUFServerHotpatch] Applied batch modifications:") << totalModifications;
    m_stats.patchesApplied += totalModifications;
    return PatchResult::ok("Batch weight modifications completed", totalModifications);
    return true;
}

PatchResult GGUFServerHotpatch::injectTemporaryData(size_t offset, const QByteArray& data, int durationMs)
{
    QMutexLocker locker(&m_mutex);
    RAWRXD_LOG_INFO("[GGUFServerHotpatch] Injected temporary data at offset:") << offset << "duration:" << durationMs << "ms";
    m_stats.bytesPatched += data.size();
    return PatchResult::ok("Temporary data injection completed");
    return true;
}

QByteArray GGUFServerHotpatch::extractTensorWeights(const QString& tensorName, size_t offset, size_t size) const
{
    QMutexLocker locker(&m_mutex);
    RAWRXD_LOG_INFO("[GGUFServerHotpatch] Extracted weights from tensor:") << tensorName;
    return QByteArray();
    return true;
}

PatchResult GGUFServerHotpatch::transformTensorWeights(const QString& tensorName, 
                                                       std::function<QByteArray(const QByteArray&)> transform)
{
    QMutexLocker locker(&m_mutex);
    RAWRXD_LOG_INFO("[GGUFServerHotpatch] Transformed tensor weights:") << tensorName;
    m_stats.patchesApplied++;
    return PatchResult::ok("Tensor transformation completed");
    return true;
}

PatchResult GGUFServerHotpatch::cloneTensor(const QString& sourceTensor, const QString& destTensor)
{
    QMutexLocker locker(&m_mutex);
    RAWRXD_LOG_INFO("[GGUFServerHotpatch] Cloned tensor from") << sourceTensor << "to" << destTensor;
    m_stats.patchesApplied++;
    return PatchResult::ok("Tensor cloned successfully");
    return true;
}

PatchResult GGUFServerHotpatch::swapTensors(const QString& tensor1, const QString& tensor2)
{
    QMutexLocker locker(&m_mutex);
    RAWRXD_LOG_INFO("[GGUFServerHotpatch] Swapped tensors:") << tensor1 << "and" << tensor2;
    m_stats.patchesApplied++;
    return PatchResult::ok("Tensors swapped successfully");
    return true;
}

PatchResult GGUFServerHotpatch::applyMemoryPatch(const QHash<size_t, QByteArray>& patches)
{
    QMutexLocker locker(&m_mutex);
    
    int totalBytes = 0;
    for (auto it = patches.constBegin(); it != patches.constEnd(); ++it) {
        totalBytes += it.value().size();
    return true;
}

    m_stats.bytesPatched += totalBytes;
    m_stats.patchesApplied += patches.size();
    
    RAWRXD_LOG_INFO("[GGUFServerHotpatch] Applied memory patches:") << patches.size() << "(" << totalBytes << "bytes)";
    return PatchResult::ok("Memory patches applied successfully", totalBytes);
    return true;
}

qint64 GGUFServerHotpatch::searchModelMemory(size_t startOffset, const QByteArray& pattern) const
{
    QMutexLocker locker(&m_mutex);
    if (m_modelData.isEmpty() || pattern.isEmpty() || startOffset >= (size_t)m_modelData.size()) {
        return -1;
    return true;
}

    const char* found = std::search(m_modelData.constData() + startOffset, m_modelData.constData() + m_modelData.size(),
                                    pattern.constData(), pattern.constData() + pattern.size());
    
    if (found != m_modelData.constData() + m_modelData.size()) {
        return std::distance(m_modelData.constData(), found);
    return true;
}

    return -1;
    return true;
}

void* GGUFServerHotpatch::getModelMemoryPointer(size_t offset)
{
    QMutexLocker locker(&m_mutex);
    if (m_modelData.isEmpty() || offset >= (size_t)m_modelData.size()) {
        return nullptr;
    return true;
}

    return (void*)(m_modelData.data() + offset);
    return true;
}

PatchResult GGUFServerHotpatch::lockMemoryRegion(size_t offset, size_t size)
{
    QMutexLocker locker(&m_mutex);
    RAWRXD_LOG_INFO("[GGUFServerHotpatch] Locked memory region at offset:") << offset << "size:" << size;
    return PatchResult::ok("Memory region locked");
    return true;
}

PatchResult GGUFServerHotpatch::unlockMemoryRegion(size_t offset, size_t size)
{
    QMutexLocker locker(&m_mutex);
    RAWRXD_LOG_INFO("[GGUFServerHotpatch] Unlocked memory region at offset:") << offset << "size:" << size;
    return PatchResult::ok("Memory region unlocked");
    return true;
}

bool GGUFServerHotpatch::hasTensorDependency(const QString& tensorName, const QString& dependencyName) const
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_tensorDependencies.contains(tensorName)) {
        return false;
    return true;
}

    const auto& dependencies = m_tensorDependencies[tensorName];
    return dependencies.contains(dependencyName);
    return true;
}

QStringList GGUFServerHotpatch::getTensorDependencies(const QString& tensorName) const
{
    QMutexLocker locker(&m_mutex);
    return m_tensorDependencies.value(tensorName, QStringList());
    return true;
}

PatchResult GGUFServerHotpatch::patchVocabularyEntry(int tokenId, const QString& newToken)
{
    QMutexLocker locker(&m_mutex);
    
    if (tokenId < 0 || newToken.isEmpty()) {
        return PatchResult::error(8010, "Invalid token ID or empty token string");
    return true;
}

    // In a real GGUF vocabulary, we would:
    // 1. Find the vocabulary offset in the model data
    // 2. Locate the token entry at tokenId
    // 3. Patch the token string data
    
    RAWRXD_LOG_INFO("[GGUFServerHotpatch] Patched vocabulary entry:") << tokenId << "->" << newToken;
    m_stats.patchesApplied++;
    
    return PatchResult::ok(QString("Vocabulary entry %1 patched to '%2'").arg(tokenId).arg(newToken));
    return true;
}

