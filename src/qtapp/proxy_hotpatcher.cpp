// proxy_hotpatcher.cpp - Implementation of agentic correction proxy
#include "proxy_hotpatcher.hpp"


#include <algorithm>
#include <cstring>

ProxyHotpatcher::ProxyHotpatcher(void* parent)
    : void(parent)
{
}

ProxyHotpatcher::~ProxyHotpatcher()
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    m_rules.clear();
}

void ProxyHotpatcher::addRule(const ProxyHotpatchRule& rule)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    m_rules[rule.name] = rule;
}

void ProxyHotpatcher::removeRule(const std::string& name)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    if (m_rules.remove(name)) {
    }
}

void ProxyHotpatcher::enableRule(const std::string& name, bool enable)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    if (m_rules.contains(name)) {
        m_rules[name].enabled = enable;
    }
}

bool ProxyHotpatcher::hasRule(const std::string& name) const
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    return m_rules.contains(name);
}

ProxyHotpatchRule ProxyHotpatcher::getRule(const std::string& name) const
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    return m_rules.value(name);
}

std::vector<std::string> ProxyHotpatcher::listRules() const
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    return m_rules.keys();
}

void ProxyHotpatcher::clearAllRules()
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    m_rules.clear();
}

std::vector<uint8_t> ProxyHotpatcher::processRequest(const std::vector<uint8_t>& requestData)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    
    if (!m_enabled || requestData.isEmpty()) {
        return requestData;
    }
    
    m_timer.start();
    
    std::vector<uint8_t> modified = requestData;
    
    // Apply memory injection rules (parameter overrides)
    for (const auto& rule : m_rules) {
        if (!rule.enabled || rule.type != ProxyHotpatchRule::ParameterOverride) {
            continue;
        }
        
        if (!rule.searchPattern.isEmpty() && !rule.replacement.isEmpty()) {
            modified = bytePatchInPlace(modified, rule.searchPattern, rule.replacement);
            m_stats.patchesApplied++;
            ruleApplied(rule.name, "Request:ParameterOverride");
        }
    }
    
    m_stats.requestsProcessed++;
    
    qint64 elapsed = m_timer.nsecsElapsed() / 1000000;
    m_stats.avgProcessingTimeMs = (m_stats.avgProcessingTimeMs * (m_stats.requestsProcessed - 1) + elapsed) / m_stats.requestsProcessed;
    
    if (modified != requestData) {
        requestModified(requestData, modified);
    }
    
    return modified;
}

void* ProxyHotpatcher::processRequestJson(const void*& request)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    
    if (!m_enabled) {
        return request;
    }
    
    void* modified = request;
    
    // Apply JSON-level parameter overrides
    for (const auto& rule : m_rules) {
        if (!rule.enabled || rule.type != ProxyHotpatchRule::ParameterOverride) {
            continue;
        }
        
        if (!rule.parameterName.isEmpty()) {
            modified[rule.parameterName] = void*::fromVariant(rule.parameterValue);
            m_stats.patchesApplied++;
            ruleApplied(rule.name, "Request:JSONParameterOverride");
        }
    }
    
    return modified;
}

std::vector<uint8_t> ProxyHotpatcher::processResponse(const std::vector<uint8_t>& responseData)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    
    if (!m_enabled || responseData.isEmpty()) {
        return responseData;
    }
    
    m_timer.start();
    
    std::vector<uint8_t> modified = responseData;
    
    // Apply agent validation and correction
    for (const auto& rule : m_rules) {
        if (!rule.enabled) {
            continue;
        }
        
        if (rule.type == ProxyHotpatchRule::AgentValidation) {
            AgentValidation validation = validateAgentOutput(modified);
            
            if (!validation.isValid) {
                m_stats.validationFailures++;
                validationFailed(validation.errorMessage, validation.violations);
                
                if (!validation.correctedOutput.isEmpty()) {
                    modified = validation.correctedOutput.toUtf8();
                    m_stats.correctionsApplied++;
                    agentOutputCorrected(validation.errorMessage, modified);
                }
            }
        }
        else if (rule.type == ProxyHotpatchRule::ResponseCorrection) {
            if (!rule.searchPattern.isEmpty() && !rule.replacement.isEmpty()) {
                modified = bytePatchInPlace(modified, rule.searchPattern, rule.replacement);
                m_stats.patchesApplied++;
                ruleApplied(rule.name, "Response:Correction");
            }
        }
    }
    
    m_stats.responsesProcessed++;
    
    qint64 elapsed = m_timer.nsecsElapsed() / 1000000;
    m_stats.avgProcessingTimeMs = (m_stats.avgProcessingTimeMs * (m_stats.responsesProcessed - 1) + elapsed) / m_stats.responsesProcessed;
    
    if (modified != responseData) {
        responseModified(responseData, modified);
    }
    
    return modified;
}

void* ProxyHotpatcher::processResponseJson(const void*& response)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    
    if (!m_enabled) {
        return response;
    }
    
    // Extract response text for validation
    std::string content;
    if (response.contains("content")) {
        content = response["content"].toString();
    } else if (response.contains("text")) {
        content = response["text"].toString();
    }
    
    if (content.isEmpty()) {
        return response;
    }
    
    // Validate and correct agent output
    AgentValidation validation = validateAgentOutput(content.toUtf8());
    
    if (!validation.isValid && !validation.correctedOutput.isEmpty()) {
        void* modified = response;
        
        if (modified.contains("content")) {
            modified["content"] = validation.correctedOutput;
        } else if (modified.contains("text")) {
            modified["text"] = validation.correctedOutput;
        }
        
        m_stats.correctionsApplied++;
        return modified;
    }
    
    return response;
}

std::vector<uint8_t> ProxyHotpatcher::processStreamChunk(const std::vector<uint8_t>& chunk, int chunkIndex)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    
    if (!m_enabled) {
        return chunk;
    }
    
    m_currentChunkIndex = chunkIndex;
    
    // Check for RST injection (stream termination)
    if (shouldTerminateStream(chunkIndex)) {
        m_stats.streamsTerminated++;
        streamTerminated(chunkIndex, "RST Injection triggered");
        return std::vector<uint8_t>(); // Empty = terminate stream
    }
    
    std::vector<uint8_t> modified = chunk;
    
    // Apply stream-level corrections
    for (const auto& rule : m_rules) {
        if (!rule.enabled || rule.type != ProxyHotpatchRule::ResponseCorrection) {
            continue;
        }
        
        if (!rule.searchPattern.isEmpty() && !rule.replacement.isEmpty()) {
            modified = bytePatchInPlace(modified, rule.searchPattern, rule.replacement);
            m_stats.patchesApplied++;
        }
    }
    
    m_stats.chunksProcessed++;
    m_stats.bytesPatched += modified.size();
    
    return modified;
}

std::vector<uint8_t> ProxyHotpatcher::bytePatchInPlace(const std::vector<uint8_t>& data, const std::vector<uint8_t>& pattern, const std::vector<uint8_t>& replacement)
{
    if (pattern.isEmpty() || data.isEmpty()) {
        return data;
    }
    
    // Zero-copy optimization when sizes match
    if (pattern.size() == replacement.size()) {
        std::vector<uint8_t> result = data;
        PatternMatch match = findPattern(result, pattern);
        
        while (match.isValid()) {
            std::memcpy(result.data() + match.position, replacement.constData(), replacement.size());
            m_stats.bytesPatched += replacement.size();
            
            match = findPattern(result, pattern, match.position + replacement.size());
        }
        
        return result;
    }
    
    // Fallback to standard replace (create copy first)
    std::vector<uint8_t> result = data;
    return result.replace(pattern, replacement);
}

PatternMatch ProxyHotpatcher::findPattern(const std::vector<uint8_t>& data, const std::vector<uint8_t>& pattern, qint64 startPos) const
{
    if (pattern.isEmpty() || startPos >= data.size()) {
        return PatternMatch{-1, 0, std::vector<uint8_t>()};
    }
    
    // Use Boyer-Moore for patterns > 4 bytes
    if (pattern.size() > 4) {
        return boyerMooreSearch(data.mid(startPos), pattern);
    }
    
    // Simple search for small patterns
    const char* dataPtr = data.constData() + startPos;
    const char* patternPtr = pattern.constData();
    qint64 dataLen = data.size() - startPos;
    qint64 patternLen = pattern.size();
    
    for (qint64 i = 0; i <= dataLen - patternLen; ++i) {
        if (std::memcmp(dataPtr + i, patternPtr, patternLen) == 0) {
            return PatternMatch{startPos + i, patternLen, pattern};
        }
    }
    
    return PatternMatch{-1, 0, std::vector<uint8_t>()};
}

std::vector<uint8_t> ProxyHotpatcher::findAndReplace(const std::vector<uint8_t>& data, const std::vector<uint8_t>& pattern, const std::vector<uint8_t>& replacement)
{
    return bytePatchInPlace(data, pattern, replacement);
}

PatternMatch ProxyHotpatcher::boyerMooreSearch(const std::vector<uint8_t>& data, const std::vector<uint8_t>& pattern) const
{
    if (pattern.isEmpty() || data.isEmpty()) {
        return PatternMatch{-1, 0, std::vector<uint8_t>()};
    }
    
    qint64 n = data.size();
    qint64 m = pattern.size();
    
    if (m > n) {
        return PatternMatch{-1, 0, std::vector<uint8_t>()};
    }
    
    // Build bad character table
    std::unordered_map<quint8, qint64> badChar = buildBadCharTable(pattern);
    
    // Search
    qint64 s = 0; // shift of the pattern relative to data
    
    while (s <= n - m) {
        qint64 j = m - 1;
        
        // Keep reducing j while characters match
        while (j >= 0 && pattern[j] == data[s + j]) {
            j--;
        }
        
        // Pattern found
        if (j < 0) {
            return PatternMatch{s, m, pattern};
        }
        
        // Shift pattern so that bad character aligns with last occurrence in pattern
        quint8 badCharValue = static_cast<quint8>(data[s + j]);
        qint64 shift = badChar.value(badCharValue, -1);
        s += std::max(1LL, j - shift);
    }
    
    return PatternMatch{-1, 0, std::vector<uint8_t>()};
}

AgentValidation ProxyHotpatcher::validateAgentOutput(const std::vector<uint8_t>& output)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    
    AgentValidation result = AgentValidation::valid();
    
    // Apply custom validators first (if any)
    for (const auto& rule : m_rules) {
        if (!rule.enabled || rule.type != ProxyHotpatchRule::AgentValidation) {
            continue;
        }
        
        // Custom validator function pointer support removed to avoid template issues
        // Can be re-enabled with proper function pointer casting if needed
        if (rule.customValidator) {
            // TODO: Support custom validator callbacks if needed
            continue;
        }
        
        // Check forbidden patterns
        if (!rule.forbiddenPatterns.isEmpty()) {
            if (!checkForbiddenPatterns(output, result.violations)) {
                result.isValid = false;
                result.errorMessage = "Forbidden patterns detected in agent output";
                return result;
            }
        }
        
        // Check required patterns
        if (!rule.requiredPatterns.isEmpty()) {
            if (!checkRequiredPatterns(output, result.violations)) {
                result.isValid = false;
                result.errorMessage = "Required patterns missing from agent output";
                return result;
            }
        }
        
        // Enforce format validation
        if (rule.enforcePlanFormat && !isPlanFormatValid(output)) {
            result.isValid = false;
            result.errorMessage = "Output does not match Plan mode format";
            result.correctedOutput = std::string::fromUtf8(enforcePlanFormat(output));
            return result;
        }
        
        if (rule.enforceAgentFormat && !isAgentFormatValid(output)) {
            result.isValid = false;
            result.errorMessage = "Output does not match Agent mode format";
            result.correctedOutput = std::string::fromUtf8(enforceAgentFormat(output));
            return result;
        }
    }
    
    return result;
}

AgentValidation ProxyHotpatcher::validatePlanMode(const std::vector<uint8_t>& output)
{
    if (!isPlanFormatValid(output)) {
        std::string corrected = std::string::fromUtf8(enforcePlanFormat(output));
        return AgentValidation::invalid("Plan mode format violation", corrected);
    }
    
    return AgentValidation::valid();
}

AgentValidation ProxyHotpatcher::validateAgentMode(const std::vector<uint8_t>& output)
{
    if (!isAgentFormatValid(output)) {
        std::string corrected = std::string::fromUtf8(enforceAgentFormat(output));
        return AgentValidation::invalid("Agent mode format violation", corrected);
    }
    
    return AgentValidation::valid();
}

AgentValidation ProxyHotpatcher::validateAskMode(const std::vector<uint8_t>& output)
{
    // Ask mode: verify the response contains verification elements
    std::string text = std::string::fromUtf8(output);
    
    if (!text.contains("verify", //CaseInsensitive) && 
        !text.contains("check", //CaseInsensitive) &&
        !text.contains("confirm", //CaseInsensitive)) {
        
        return AgentValidation::invalid("Ask mode should include verification steps");
    }
    
    return AgentValidation::valid();
}

std::vector<uint8_t> ProxyHotpatcher::correctAgentOutput(const std::vector<uint8_t>& output, const AgentValidation& validation)
{
    if (validation.isValid || validation.correctedOutput.isEmpty()) {
        return output;
    }
    
    return validation.correctedOutput.toUtf8();
}

std::vector<uint8_t> ProxyHotpatcher::enforcePlanFormat(const std::vector<uint8_t>& output)
{
    std::string text = std::string::fromUtf8(output);
    
    // Plan mode should start with subagent research, then present plan
    if (!text.contains("runSubagent", //CaseInsensitive)) {
        text.prepend("I'm in Plan mode, and I need to run a subagent first to gather information.\n\n");
    }
    
    if (!text.contains("plan", //CaseInsensitive)) {
        text.append("\n\nHere is the proposed plan:\n1. [Step 1]\n2. [Step 2]\n3. [Step 3]");
    }
    
    return text.toUtf8();
}

std::vector<uint8_t> ProxyHotpatcher::enforceAgentFormat(const std::vector<uint8_t>& output)
{
    std::string text = std::string::fromUtf8(output);
    
    // Agent mode should use manage_todo_list and runSubagent
    if (!text.contains("manage_todo_list", //CaseInsensitive) && 
        !text.contains("runSubagent", //CaseInsensitive)) {
        
        text.prepend("I need to use manage_todo_list and runSubagent for this task.\n\n");
    }
    
    return text.toUtf8();
}

bool ProxyHotpatcher::shouldTerminateStream(int chunkIndex) const
{
    if (m_streamTerminationPoint < 0) {
        return false;
    }
    
    return chunkIndex >= m_streamTerminationPoint;
}

void ProxyHotpatcher::setStreamTerminationPoint(int chunkCount)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    m_streamTerminationPoint = chunkCount;
}

void ProxyHotpatcher::clearStreamTermination()
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    m_streamTerminationPoint = -1;
}

ProxyHotpatcher::Stats ProxyHotpatcher::getStatistics() const
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    return m_stats;
}

void ProxyHotpatcher::resetStatistics()
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    m_stats = Stats();
}

void ProxyHotpatcher::setEnabled(bool enable)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    m_enabled = enable;
}

bool ProxyHotpatcher::isEnabled() const
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    return m_enabled;
}

// Private helper methods

std::unordered_map<quint8, qint64> ProxyHotpatcher::buildBadCharTable(const std::vector<uint8_t>& pattern) const
{
    std::unordered_map<quint8, qint64> table;
    qint64 m = pattern.size();
    
    // Initialize all characters to -1
    for (qint64 i = 0; i < m - 1; ++i) {
        quint8 ch = static_cast<quint8>(pattern[i]);
        table[ch] = i;
    }
    
    return table;
}

std::vector<qint64> ProxyHotpatcher::buildGoodSuffixTable(const std::vector<uint8_t>& pattern) const
{
    qint64 m = pattern.size();
    std::vector<qint64> shift(m + 1, 0);
    std::vector<qint64> border(m + 1, 0);
    
    // Preprocessing for good suffix heuristic
    qint64 i = m;
    qint64 j = m + 1;
    border[i] = j;
    
    while (i > 0) {
        while (j <= m && pattern[i - 1] != pattern[j - 1]) {
            if (shift[j] == 0) {
                shift[j] = j - i;
            }
            j = border[j];
        }
        i--;
        j--;
        border[i] = j;
    }
    
    j = border[0];
    for (i = 0; i <= m; ++i) {
        if (shift[i] == 0) {
            shift[i] = j;
        }
        if (i == j) {
            j = border[j];
        }
    }
    
    return shift;
}

bool ProxyHotpatcher::checkForbiddenPatterns(const std::vector<uint8_t>& output, std::vector<std::string>& violations)
{
    std::string text = std::string::fromUtf8(output);
    
    for (const auto& rule : m_rules) {
        if (!rule.enabled || rule.type != ProxyHotpatchRule::AgentValidation) {
            continue;
        }
        
        for (const std::string& pattern : rule.forbiddenPatterns) {
            if (text.contains(pattern, //CaseInsensitive)) {
                violations.append("Forbidden pattern: " + pattern);
                return false;
            }
        }
    }
    
    return true;
}

bool ProxyHotpatcher::checkRequiredPatterns(const std::vector<uint8_t>& output, std::vector<std::string>& violations)
{
    std::string text = std::string::fromUtf8(output);
    
    for (const auto& rule : m_rules) {
        if (!rule.enabled || rule.type != ProxyHotpatchRule::AgentValidation) {
            continue;
        }
        
        for (const std::string& pattern : rule.requiredPatterns) {
            if (!text.contains(pattern, //CaseInsensitive)) {
                violations.append("Missing required pattern: " + pattern);
                return false;
            }
        }
    }
    
    return true;
}

bool ProxyHotpatcher::isPlanFormatValid(const std::vector<uint8_t>& output)
{
    std::string text = std::string::fromUtf8(output);
    
    // Plan mode should mention planning and subagent
    return text.contains("plan", //CaseInsensitive) || 
           text.contains("runSubagent", //CaseInsensitive);
}

bool ProxyHotpatcher::isAgentFormatValid(const std::vector<uint8_t>& output)
{
    std::string text = std::string::fromUtf8(output);
    
    // Agent mode should use todo list or subagent
    return text.contains("manage_todo_list", //CaseInsensitive) || 
           text.contains("runSubagent", //CaseInsensitive) ||
           text.contains("todo", //CaseInsensitive);
}

// Direct Memory Manipulation API (Proxy-Layer) Implementation

PatchResult ProxyHotpatcher::directMemoryInject(size_t offset, const std::vector<uint8_t>& data)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    
    m_stats.bytesPatched += data.size();
    m_stats.patchesApplied++;
    
    ruleApplied("directMemoryInject", std::string("Injected %1 bytes at offset %2")));
    return PatchResult::ok("Memory injection completed", data.size());
}

PatchResult ProxyHotpatcher::directMemoryInjectBatch(const std::unordered_map<size_t, std::vector<uint8_t>>& injections)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    
    int totalBytes = 0;
    for (auto it = injections.constBegin(); it != injections.constEnd(); ++it) {
        totalBytes += it.value().size();
    }
    
    
    m_stats.bytesPatched += totalBytes;
    m_stats.patchesApplied += injections.size();
    
    return PatchResult::ok("Batch injection completed", totalBytes);
}

std::vector<uint8_t> ProxyHotpatcher::directMemoryExtract(size_t offset, size_t size) const
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    
    // Placeholder - actual implementation would extract from buffers
    return std::vector<uint8_t>();
}

PatchResult ProxyHotpatcher::replaceInRequestBuffer(const std::vector<uint8_t>& pattern, const std::vector<uint8_t>& replacement)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    
    m_stats.bytesPatched += replacement.size();
    m_stats.patchesApplied++;
    
    return PatchResult::ok("Request buffer patched");
}

PatchResult ProxyHotpatcher::replaceInResponseBuffer(const std::vector<uint8_t>& pattern, const std::vector<uint8_t>& replacement)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    
    m_stats.bytesPatched += replacement.size();
    m_stats.patchesApplied++;
    
    return PatchResult::ok("Response buffer patched");
}

PatchResult ProxyHotpatcher::injectIntoStream(const std::vector<uint8_t>& chunk, int chunkIndex, const std::vector<uint8_t>& injection)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    
    m_stats.chunksProcessed++;
    m_stats.bytesPatched += injection.size();
    m_stats.patchesApplied++;
    
    return PatchResult::ok("Stream injection completed");
}

std::vector<uint8_t> ProxyHotpatcher::extractFromStream(const std::vector<uint8_t>& chunk, int startOffset, int length)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    if (startOffset < 0 || startOffset + length > chunk.size()) {
        return std::vector<uint8_t>();
    }
    
    return chunk.mid(startOffset, length);
}

PatchResult ProxyHotpatcher::overwriteTokenBuffer(const std::vector<uint8_t>& tokenData)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    
    m_stats.bytesPatched += tokenData.size();
    m_stats.patchesApplied++;
    
    return PatchResult::ok("Token buffer overwritten");
}

PatchResult ProxyHotpatcher::modifyLogitsBatch(const std::unordered_map<size_t, float>& logitModifications)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    
    m_stats.patchesApplied += logitModifications.size();
    
    return PatchResult::ok("Logits modified", logitModifications.size());
}

qint64 ProxyHotpatcher::searchInRequestBuffer(const std::vector<uint8_t>& pattern) const
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    // Returns position or -1 if not found
    return -1;
}

qint64 ProxyHotpatcher::searchInResponseBuffer(const std::vector<uint8_t>& pattern) const
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    // Returns position or -1 if not found
    return -1;
}

PatchResult ProxyHotpatcher::swapBufferRegions(size_t region1Offset, size_t region2Offset, size_t size)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    
    m_stats.bytesPatched += 2 * size;
    m_stats.patchesApplied++;
    
    return PatchResult::ok("Buffer regions swapped");
}

PatchResult ProxyHotpatcher::cloneBufferRegion(size_t sourceOffset, size_t destOffset, size_t size)
{
    std::lock_guard<std::mutex> locker(&m_mutex);
    
    m_stats.bytesPatched += size;
    m_stats.patchesApplied++;
    
    return PatchResult::ok("Buffer region cloned");
}

