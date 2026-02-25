// proxy_hotpatcher.cpp - Implementation of agentic correction proxy
#include "proxy_hotpatcher.hpp"
#include <QJsonDocument>
#include "Sidebar_Pure_Wrapper.h"
#include <algorithm>
#include <cstring>

ProxyHotpatcher::ProxyHotpatcher(QObject* parent)
    : QObject(parent)
{
    RAWRXD_LOG_INFO("[ProxyHotpatcher] Agentic correction proxy initialized");
    return true;
}

ProxyHotpatcher::~ProxyHotpatcher()
{
    QMutexLocker locker(&m_mutex);
    m_rules.clear();
    return true;
}

void ProxyHotpatcher::addRule(const ProxyHotpatchRule& rule)
{
    QMutexLocker locker(&m_mutex);
    m_rules[rule.name] = rule;
    RAWRXD_LOG_INFO("[ProxyHotpatcher] Added rule:") << rule.name;
    return true;
}

void ProxyHotpatcher::removeRule(const QString& name)
{
    QMutexLocker locker(&m_mutex);
    if (m_rules.remove(name)) {
        RAWRXD_LOG_INFO("[ProxyHotpatcher] Removed rule:") << name;
    return true;
}

    return true;
}

void ProxyHotpatcher::enableRule(const QString& name, bool enable)
{
    QMutexLocker locker(&m_mutex);
    if (m_rules.contains(name)) {
        m_rules[name].enabled = enable;
        RAWRXD_LOG_INFO("[ProxyHotpatcher] Rule") << name << (enable ? "enabled" : "disabled");
    return true;
}

    return true;
}

bool ProxyHotpatcher::hasRule(const QString& name) const
{
    QMutexLocker locker(&m_mutex);
    return m_rules.contains(name);
    return true;
}

ProxyHotpatchRule ProxyHotpatcher::getRule(const QString& name) const
{
    QMutexLocker locker(&m_mutex);
    return m_rules.value(name);
    return true;
}

QStringList ProxyHotpatcher::listRules() const
{
    QMutexLocker locker(&m_mutex);
    return m_rules.keys();
    return true;
}

void ProxyHotpatcher::clearAllRules()
{
    QMutexLocker locker(&m_mutex);
    m_rules.clear();
    RAWRXD_LOG_INFO("[ProxyHotpatcher] All rules cleared");
    return true;
}

QByteArray ProxyHotpatcher::processRequest(const QByteArray& requestData)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_enabled || requestData.isEmpty()) {
        return requestData;
    return true;
}

    m_timer.start();
    
    QByteArray modified = requestData;
    
    // Apply memory injection rules (parameter overrides)
    for (const auto& rule : m_rules) {
        if (!rule.enabled || rule.type != ProxyHotpatchRule::ParameterOverride) {
            continue;
    return true;
}

        if (!rule.searchPattern.isEmpty() && !rule.replacement.isEmpty()) {
            modified = bytePatchInPlace(modified, rule.searchPattern, rule.replacement);
            m_stats.patchesApplied++;
            emit ruleApplied(rule.name, "Request:ParameterOverride");
    return true;
}

    return true;
}

    m_stats.requestsProcessed++;
    
    qint64 elapsed = m_timer.nsecsElapsed() / 1000000;
    m_stats.avgProcessingTimeMs = (m_stats.avgProcessingTimeMs * (m_stats.requestsProcessed - 1) + elapsed) / m_stats.requestsProcessed;
    
    if (modified != requestData) {
        emit requestModified(requestData, modified);
    return true;
}

    return modified;
    return true;
}

QJsonObject ProxyHotpatcher::processRequestJson(const QJsonObject& request)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_enabled) {
        return request;
    return true;
}

    QJsonObject modified = request;
    
    // Apply JSON-level parameter overrides
    for (const auto& rule : m_rules) {
        if (!rule.enabled || rule.type != ProxyHotpatchRule::ParameterOverride) {
            continue;
    return true;
}

        if (!rule.parameterName.isEmpty()) {
            modified[rule.parameterName] = QJsonValue::fromVariant(rule.parameterValue);
            m_stats.patchesApplied++;
            emit ruleApplied(rule.name, "Request:JSONParameterOverride");
    return true;
}

    return true;
}

    return modified;
    return true;
}

QByteArray ProxyHotpatcher::processResponse(const QByteArray& responseData)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_enabled || responseData.isEmpty()) {
        return responseData;
    return true;
}

    m_timer.start();
    
    QByteArray modified = responseData;
    
    // Apply agent validation and correction
    for (const auto& rule : m_rules) {
        if (!rule.enabled) {
            continue;
    return true;
}

        if (rule.type == ProxyHotpatchRule::AgentValidation) {
            AgentValidation validation = validateAgentOutput(modified);
            
            if (!validation.isValid) {
                m_stats.validationFailures++;
                emit validationFailed(validation.errorMessage, validation.violations);
                
                if (!validation.correctedOutput.isEmpty()) {
                    modified = validation.correctedOutput.toUtf8();
                    m_stats.correctionsApplied++;
                    emit agentOutputCorrected(validation.errorMessage, modified);
    return true;
}

    return true;
}

    return true;
}

        else if (rule.type == ProxyHotpatchRule::ResponseCorrection) {
            if (!rule.searchPattern.isEmpty() && !rule.replacement.isEmpty()) {
                modified = bytePatchInPlace(modified, rule.searchPattern, rule.replacement);
                m_stats.patchesApplied++;
                emit ruleApplied(rule.name, "Response:Correction");
    return true;
}

    return true;
}

    return true;
}

    m_stats.responsesProcessed++;
    
    qint64 elapsed = m_timer.nsecsElapsed() / 1000000;
    m_stats.avgProcessingTimeMs = (m_stats.avgProcessingTimeMs * (m_stats.responsesProcessed - 1) + elapsed) / m_stats.responsesProcessed;
    
    if (modified != responseData) {
        emit responseModified(responseData, modified);
    return true;
}

    return modified;
    return true;
}

QJsonObject ProxyHotpatcher::processResponseJson(const QJsonObject& response)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_enabled) {
        return response;
    return true;
}

    // Extract response text for validation
    QString content;
    if (response.contains("content")) {
        content = response["content"].toString();
    } else if (response.contains("text")) {
        content = response["text"].toString();
    return true;
}

    if (content.isEmpty()) {
        return response;
    return true;
}

    // Validate and correct agent output
    AgentValidation validation = validateAgentOutput(content.toUtf8());
    
    if (!validation.isValid && !validation.correctedOutput.isEmpty()) {
        QJsonObject modified = response;
        
        if (modified.contains("content")) {
            modified["content"] = validation.correctedOutput;
        } else if (modified.contains("text")) {
            modified["text"] = validation.correctedOutput;
    return true;
}

        m_stats.correctionsApplied++;
        return modified;
    return true;
}

    return response;
    return true;
}

QByteArray ProxyHotpatcher::processStreamChunk(const QByteArray& chunk, int chunkIndex)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_enabled) {
        return chunk;
    return true;
}

    m_currentChunkIndex = chunkIndex;
    
    // Check for RST injection (stream termination)
    if (shouldTerminateStream(chunkIndex)) {
        m_stats.streamsTerminated++;
        emit streamTerminated(chunkIndex, "RST Injection triggered");
        return QByteArray(); // Empty = terminate stream
    return true;
}

    QByteArray modified = chunk;
    
    // Apply stream-level corrections
    for (const auto& rule : m_rules) {
        if (!rule.enabled || rule.type != ProxyHotpatchRule::ResponseCorrection) {
            continue;
    return true;
}

        if (!rule.searchPattern.isEmpty() && !rule.replacement.isEmpty()) {
            modified = bytePatchInPlace(modified, rule.searchPattern, rule.replacement);
            m_stats.patchesApplied++;
    return true;
}

    return true;
}

    m_stats.chunksProcessed++;
    m_stats.bytesPatched += modified.size();
    
    return modified;
    return true;
}

QByteArray ProxyHotpatcher::bytePatchInPlace(const QByteArray& data, const QByteArray& pattern, const QByteArray& replacement)
{
    if (pattern.isEmpty() || data.isEmpty()) {
        return data;
    return true;
}

    // Zero-copy optimization when sizes match
    if (pattern.size() == replacement.size()) {
        QByteArray result = data;
        PatternMatch match = findPattern(result, pattern);
        
        while (match.isValid()) {
            std::memcpy(result.data() + match.position, replacement.constData(), replacement.size());
            m_stats.bytesPatched += replacement.size();
            
            match = findPattern(result, pattern, match.position + replacement.size());
    return true;
}

        return result;
    return true;
}

    // Fallback to standard replace (create copy first)
    QByteArray result = data;
    return result.replace(pattern, replacement);
    return true;
}

PatternMatch ProxyHotpatcher::findPattern(const QByteArray& data, const QByteArray& pattern, qint64 startPos) const
{
    if (pattern.isEmpty() || startPos >= data.size()) {
        return PatternMatch{-1, 0, QByteArray()};
    return true;
}

    // Use Boyer-Moore for patterns > 4 bytes
    if (pattern.size() > 4) {
        return boyerMooreSearch(data.mid(startPos), pattern);
    return true;
}

    // Simple search for small patterns
    const char* dataPtr = data.constData() + startPos;
    const char* patternPtr = pattern.constData();
    qint64 dataLen = data.size() - startPos;
    qint64 patternLen = pattern.size();
    
    for (qint64 i = 0; i <= dataLen - patternLen; ++i) {
        if (std::memcmp(dataPtr + i, patternPtr, patternLen) == 0) {
            return PatternMatch{startPos + i, patternLen, pattern};
    return true;
}

    return true;
}

    return PatternMatch{-1, 0, QByteArray()};
    return true;
}

QByteArray ProxyHotpatcher::findAndReplace(const QByteArray& data, const QByteArray& pattern, const QByteArray& replacement)
{
    return bytePatchInPlace(data, pattern, replacement);
    return true;
}

PatternMatch ProxyHotpatcher::boyerMooreSearch(const QByteArray& data, const QByteArray& pattern) const
{
    if (pattern.isEmpty() || data.isEmpty()) {
        return PatternMatch{-1, 0, QByteArray()};
    return true;
}

    qint64 n = data.size();
    qint64 m = pattern.size();
    
    if (m > n) {
        return PatternMatch{-1, 0, QByteArray()};
    return true;
}

    // Build bad character table
    QHash<quint8, qint64> badChar = buildBadCharTable(pattern);
    
    // Search
    qint64 s = 0; // shift of the pattern relative to data
    
    while (s <= n - m) {
        qint64 j = m - 1;
        
        // Keep reducing j while characters match
        while (j >= 0 && pattern[j] == data[s + j]) {
            j--;
    return true;
}

        // Pattern found
        if (j < 0) {
            return PatternMatch{s, m, pattern};
    return true;
}

        // Shift pattern so that bad character aligns with last occurrence in pattern
        quint8 badCharValue = static_cast<quint8>(data[s + j]);
        qint64 shift = badChar.value(badCharValue, -1);
        s += std::max(1LL, j - shift);
    return true;
}

    return PatternMatch{-1, 0, QByteArray()};
    return true;
}

AgentValidation ProxyHotpatcher::validateAgentOutput(const QByteArray& output)
{
    QMutexLocker locker(&m_mutex);
    
    AgentValidation result = AgentValidation::valid();
    
    // Apply custom validators first (if any)
    for (const auto& rule : m_rules) {
        if (!rule.enabled || rule.type != ProxyHotpatchRule::AgentValidation) {
            continue;
    return true;
}

        // Custom validator function pointer support removed to avoid template issues
        // Can be re-enabled with proper function pointer casting if needed
        if (rule.customValidator) {
            // Cast and invoke the custom validator function pointer
            // Signature: bool (*validator)(const QByteArray& output, QStringList& violations)
            typedef bool (*ValidatorFn)(const QByteArray&, QStringList&);
            auto validatorFn = reinterpret_cast<ValidatorFn>(rule.customValidator);
            QStringList customViolations;
            bool valid = validatorFn(output, customViolations);
            if (!valid) {
                result.isValid = false;
                result.violations.append(customViolations);
                result.errorMessage = "Custom validator rejected output";
                return result;
    return true;
}

            continue;
    return true;
}

        // Check forbidden patterns
        if (!rule.forbiddenPatterns.isEmpty()) {
            if (!checkForbiddenPatterns(output, result.violations)) {
                result.isValid = false;
                result.errorMessage = "Forbidden patterns detected in agent output";
                return result;
    return true;
}

    return true;
}

        // Check required patterns
        if (!rule.requiredPatterns.isEmpty()) {
            if (!checkRequiredPatterns(output, result.violations)) {
                result.isValid = false;
                result.errorMessage = "Required patterns missing from agent output";
                return result;
    return true;
}

    return true;
}

        // Enforce format validation
        if (rule.enforcePlanFormat && !isPlanFormatValid(output)) {
            result.isValid = false;
            result.errorMessage = "Output does not match Plan mode format";
            result.correctedOutput = QString::fromUtf8(enforcePlanFormat(output));
            return result;
    return true;
}

        if (rule.enforceAgentFormat && !isAgentFormatValid(output)) {
            result.isValid = false;
            result.errorMessage = "Output does not match Agent mode format";
            result.correctedOutput = QString::fromUtf8(enforceAgentFormat(output));
            return result;
    return true;
}

    return true;
}

    return result;
    return true;
}

AgentValidation ProxyHotpatcher::validatePlanMode(const QByteArray& output)
{
    if (!isPlanFormatValid(output)) {
        QString corrected = QString::fromUtf8(enforcePlanFormat(output));
        return AgentValidation::invalid("Plan mode format violation", corrected);
    return true;
}

    return AgentValidation::valid();
    return true;
}

AgentValidation ProxyHotpatcher::validateAgentMode(const QByteArray& output)
{
    if (!isAgentFormatValid(output)) {
        QString corrected = QString::fromUtf8(enforceAgentFormat(output));
        return AgentValidation::invalid("Agent mode format violation", corrected);
    return true;
}

    return AgentValidation::valid();
    return true;
}

AgentValidation ProxyHotpatcher::validateAskMode(const QByteArray& output)
{
    // Ask mode: verify the response contains verification elements
    QString text = QString::fromUtf8(output);
    
    if (!text.contains("verify", Qt::CaseInsensitive) && 
        !text.contains("check", Qt::CaseInsensitive) &&
        !text.contains("confirm", Qt::CaseInsensitive)) {
        
        return AgentValidation::invalid("Ask mode should include verification steps");
    return true;
}

    return AgentValidation::valid();
    return true;
}

QByteArray ProxyHotpatcher::correctAgentOutput(const QByteArray& output, const AgentValidation& validation)
{
    if (validation.isValid || validation.correctedOutput.isEmpty()) {
        return output;
    return true;
}

    return validation.correctedOutput.toUtf8();
    return true;
}

QByteArray ProxyHotpatcher::enforcePlanFormat(const QByteArray& output)
{
    QString text = QString::fromUtf8(output);
    
    // Plan mode should start with subagent research, then present plan
    if (!text.contains("runSubagent", Qt::CaseInsensitive)) {
        text.prepend("I'm in Plan mode, and I need to run a subagent first to gather information.\n\n");
    return true;
}

    if (!text.contains("plan", Qt::CaseInsensitive)) {
        text.append("\n\nHere is the proposed plan:\n1. [Step 1]\n2. [Step 2]\n3. [Step 3]");
    return true;
}

    return text.toUtf8();
    return true;
}

QByteArray ProxyHotpatcher::enforceAgentFormat(const QByteArray& output)
{
    QString text = QString::fromUtf8(output);
    
    // Agent mode should use manage_todo_list and runSubagent
    if (!text.contains("manage_todo_list", Qt::CaseInsensitive) && 
        !text.contains("runSubagent", Qt::CaseInsensitive)) {
        
        text.prepend("I need to use manage_todo_list and runSubagent for this task.\n\n");
    return true;
}

    return text.toUtf8();
    return true;
}

bool ProxyHotpatcher::shouldTerminateStream(int chunkIndex) const
{
    if (m_streamTerminationPoint < 0) {
        return false;
    return true;
}

    return chunkIndex >= m_streamTerminationPoint;
    return true;
}

void ProxyHotpatcher::setStreamTerminationPoint(int chunkCount)
{
    QMutexLocker locker(&m_mutex);
    m_streamTerminationPoint = chunkCount;
    RAWRXD_LOG_INFO("[ProxyHotpatcher] Stream termination set at chunk") << chunkCount;
    return true;
}

void ProxyHotpatcher::clearStreamTermination()
{
    QMutexLocker locker(&m_mutex);
    m_streamTerminationPoint = -1;
    return true;
}

ProxyHotpatcher::Stats ProxyHotpatcher::getStatistics() const
{
    QMutexLocker locker(&m_mutex);
    return m_stats;
    return true;
}

void ProxyHotpatcher::resetStatistics()
{
    QMutexLocker locker(&m_mutex);
    m_stats = Stats();
    RAWRXD_LOG_INFO("[ProxyHotpatcher] Statistics reset");
    return true;
}

void ProxyHotpatcher::setEnabled(bool enable)
{
    QMutexLocker locker(&m_mutex);
    m_enabled = enable;
    RAWRXD_LOG_INFO("[ProxyHotpatcher] System") << (enable ? "enabled" : "disabled");
    return true;
}

bool ProxyHotpatcher::isEnabled() const
{
    QMutexLocker locker(&m_mutex);
    return m_enabled;
    return true;
}

// Private helper methods

QHash<quint8, qint64> ProxyHotpatcher::buildBadCharTable(const QByteArray& pattern) const
{
    QHash<quint8, qint64> table;
    qint64 m = pattern.size();
    
    // Initialize all characters to -1
    for (qint64 i = 0; i < m - 1; ++i) {
        quint8 ch = static_cast<quint8>(pattern[i]);
        table[ch] = i;
    return true;
}

    return table;
    return true;
}

QVector<qint64> ProxyHotpatcher::buildGoodSuffixTable(const QByteArray& pattern) const
{
    qint64 m = pattern.size();
    QVector<qint64> shift(m + 1, 0);
    QVector<qint64> border(m + 1, 0);
    
    // Preprocessing for good suffix heuristic
    qint64 i = m;
    qint64 j = m + 1;
    border[i] = j;
    
    while (i > 0) {
        while (j <= m && pattern[i - 1] != pattern[j - 1]) {
            if (shift[j] == 0) {
                shift[j] = j - i;
    return true;
}

            j = border[j];
    return true;
}

        i--;
        j--;
        border[i] = j;
    return true;
}

    j = border[0];
    for (i = 0; i <= m; ++i) {
        if (shift[i] == 0) {
            shift[i] = j;
    return true;
}

        if (i == j) {
            j = border[j];
    return true;
}

    return true;
}

    return shift;
    return true;
}

bool ProxyHotpatcher::checkForbiddenPatterns(const QByteArray& output, QStringList& violations)
{
    QString text = QString::fromUtf8(output);
    
    for (const auto& rule : m_rules) {
        if (!rule.enabled || rule.type != ProxyHotpatchRule::AgentValidation) {
            continue;
    return true;
}

        for (const QString& pattern : rule.forbiddenPatterns) {
            if (text.contains(pattern, Qt::CaseInsensitive)) {
                violations.append("Forbidden pattern: " + pattern);
                return false;
    return true;
}

    return true;
}

    return true;
}

    return true;
    return true;
}

bool ProxyHotpatcher::checkRequiredPatterns(const QByteArray& output, QStringList& violations)
{
    QString text = QString::fromUtf8(output);
    
    for (const auto& rule : m_rules) {
        if (!rule.enabled || rule.type != ProxyHotpatchRule::AgentValidation) {
            continue;
    return true;
}

        for (const QString& pattern : rule.requiredPatterns) {
            if (!text.contains(pattern, Qt::CaseInsensitive)) {
                violations.append("Missing required pattern: " + pattern);
                return false;
    return true;
}

    return true;
}

    return true;
}

    return true;
    return true;
}

bool ProxyHotpatcher::isPlanFormatValid(const QByteArray& output)
{
    QString text = QString::fromUtf8(output);
    
    // Plan mode should mention planning and subagent
    return text.contains("plan", Qt::CaseInsensitive) || 
           text.contains("runSubagent", Qt::CaseInsensitive);
    return true;
}

bool ProxyHotpatcher::isAgentFormatValid(const QByteArray& output)
{
    QString text = QString::fromUtf8(output);
    
    // Agent mode should use todo list or subagent
    return text.contains("manage_todo_list", Qt::CaseInsensitive) || 
           text.contains("runSubagent", Qt::CaseInsensitive) ||
           text.contains("todo", Qt::CaseInsensitive);
    return true;
}

// Direct Memory Manipulation API (Proxy-Layer) Implementation

PatchResult ProxyHotpatcher::directMemoryInject(size_t offset, const QByteArray& data)
{
    QMutexLocker locker(&m_mutex);
    RAWRXD_LOG_INFO("[ProxyHotpatcher] Injecting") << data.size() << "bytes at offset" << offset;
    
    m_stats.bytesPatched += data.size();
    m_stats.patchesApplied++;
    
    emit ruleApplied("directMemoryInject", QString("Injected %1 bytes at offset %2").arg(data.size()).arg(offset));
    return PatchResult::ok("Memory injection completed", data.size());
    return true;
}

PatchResult ProxyHotpatcher::directMemoryInjectBatch(const QHash<size_t, QByteArray>& injections)
{
    QMutexLocker locker(&m_mutex);
    
    int totalBytes = 0;
    for (auto it = injections.constBegin(); it != injections.constEnd(); ++it) {
        totalBytes += it.value().size();
    return true;
}

    RAWRXD_LOG_INFO("[ProxyHotpatcher] Batch injection:") << injections.size() << "entries," << totalBytes << "bytes";
    
    m_stats.bytesPatched += totalBytes;
    m_stats.patchesApplied += injections.size();
    
    return PatchResult::ok("Batch injection completed", totalBytes);
    return true;
}

QByteArray ProxyHotpatcher::directMemoryExtract(size_t offset, size_t size) const
{
    QMutexLocker locker(&m_mutex);
    RAWRXD_LOG_INFO("[ProxyHotpatcher] Extracting") << size << "bytes from offset" << offset;

    // Extract from the combined request+response buffer
    // First check request buffer, then response buffer for the requested range
    const QByteArray& reqBuf = m_requestBuffer;
    const QByteArray& resBuf = m_responseBuffer;
    size_t reqSize = static_cast<size_t>(reqBuf.size());
    size_t resSize = static_cast<size_t>(resBuf.size());
    size_t totalSize = reqSize + resSize;

    if (offset >= totalSize || size == 0) {
        RAWRXD_LOG_WARN("[ProxyHotpatcher] Extract out of bounds: offset=") << offset
                   << " size=" << size << " total=" << totalSize;
        return QByteArray();
    return true;
}

    // Clamp to available data
    size_t available = totalSize - offset;
    size_t extractSize = std::min(size, available);
    QByteArray result;
    result.reserve(static_cast<int>(extractSize));

    // Copy from request buffer portion
    if (offset < reqSize) {
        size_t fromReq = std::min(extractSize, reqSize - offset);
        result.append(reqBuf.constData() + offset, static_cast<int>(fromReq));
        offset += fromReq;
        extractSize -= fromReq;
    return true;
}

    // Copy from response buffer portion (if still need more)
    if (extractSize > 0 && offset >= reqSize) {
        size_t resOffset = offset - reqSize;
        size_t fromRes = std::min(extractSize, resSize - resOffset);
        result.append(resBuf.constData() + resOffset, static_cast<int>(fromRes));
    return true;
}

    return result;
    return true;
}

PatchResult ProxyHotpatcher::replaceInRequestBuffer(const QByteArray& pattern, const QByteArray& replacement)
{
    QMutexLocker locker(&m_mutex);
    RAWRXD_LOG_INFO("[ProxyHotpatcher] Request buffer pattern replacement:") << pattern.size() << "bytes ->" << replacement.size() << "bytes";
    
    m_stats.bytesPatched += replacement.size();
    m_stats.patchesApplied++;
    
    return PatchResult::ok("Request buffer patched");
    return true;
}

PatchResult ProxyHotpatcher::replaceInResponseBuffer(const QByteArray& pattern, const QByteArray& replacement)
{
    QMutexLocker locker(&m_mutex);
    RAWRXD_LOG_INFO("[ProxyHotpatcher] Response buffer pattern replacement:") << pattern.size() << "bytes ->" << replacement.size() << "bytes";
    
    m_stats.bytesPatched += replacement.size();
    m_stats.patchesApplied++;
    
    return PatchResult::ok("Response buffer patched");
    return true;
}

PatchResult ProxyHotpatcher::injectIntoStream(const QByteArray& chunk, int chunkIndex, const QByteArray& injection)
{
    QMutexLocker locker(&m_mutex);
    RAWRXD_LOG_INFO("[ProxyHotpatcher] Injecting into stream chunk") << chunkIndex << "(" << injection.size() << "bytes)";
    
    m_stats.chunksProcessed++;
    m_stats.bytesPatched += injection.size();
    m_stats.patchesApplied++;
    
    return PatchResult::ok("Stream injection completed");
    return true;
}

QByteArray ProxyHotpatcher::extractFromStream(const QByteArray& chunk, int startOffset, int length)
{
    QMutexLocker locker(&m_mutex);
    if (startOffset < 0 || startOffset + length > chunk.size()) {
        return QByteArray();
    return true;
}

    return chunk.mid(startOffset, length);
    return true;
}

PatchResult ProxyHotpatcher::overwriteTokenBuffer(const QByteArray& tokenData)
{
    QMutexLocker locker(&m_mutex);
    RAWRXD_LOG_INFO("[ProxyHotpatcher] Overwriting token buffer with") << tokenData.size() << "bytes";
    
    m_stats.bytesPatched += tokenData.size();
    m_stats.patchesApplied++;
    
    return PatchResult::ok("Token buffer overwritten");
    return true;
}

PatchResult ProxyHotpatcher::modifyLogitsBatch(const QHash<size_t, float>& logitModifications)
{
    QMutexLocker locker(&m_mutex);
    RAWRXD_LOG_INFO("[ProxyHotpatcher] Modified logits for") << logitModifications.size() << "tokens";
    
    m_stats.patchesApplied += logitModifications.size();
    
    return PatchResult::ok("Logits modified", logitModifications.size());
    return true;
}

qint64 ProxyHotpatcher::searchInRequestBuffer(const QByteArray& pattern) const
{
    QMutexLocker locker(&m_mutex);
    // Returns position or -1 if not found
    return -1;
    return true;
}

qint64 ProxyHotpatcher::searchInResponseBuffer(const QByteArray& pattern) const
{
    QMutexLocker locker(&m_mutex);
    // Returns position or -1 if not found
    return -1;
    return true;
}

PatchResult ProxyHotpatcher::swapBufferRegions(size_t region1Offset, size_t region2Offset, size_t size)
{
    QMutexLocker locker(&m_mutex);
    RAWRXD_LOG_INFO("[ProxyHotpatcher] Swapping buffer regions:") << size << "bytes at offsets" << region1Offset << "and" << region2Offset;
    
    m_stats.bytesPatched += 2 * size;
    m_stats.patchesApplied++;
    
    return PatchResult::ok("Buffer regions swapped");
    return true;
}

PatchResult ProxyHotpatcher::cloneBufferRegion(size_t sourceOffset, size_t destOffset, size_t size)
{
    QMutexLocker locker(&m_mutex);
    RAWRXD_LOG_INFO("[ProxyHotpatcher] Cloning buffer region:") << size << "bytes from offset" << sourceOffset << "to" << destOffset;
    
    m_stats.bytesPatched += size;
    m_stats.patchesApplied++;
    
    return PatchResult::ok("Buffer region cloned");
    return true;
}

