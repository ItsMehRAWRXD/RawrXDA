<<<<<<< HEAD
// ============================================================================
// ai_merge_resolver.cpp — AI-Assisted Merge Conflict Resolution
// ============================================================================
// Core implementation of merge conflict detection and resolution.
// ============================================================================

#include "ai_merge_resolver.hpp"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <sstream>
#include <cmath>

namespace RawrXD {
namespace Git {

// ============================================================================
// String utilities
// ============================================================================

static std::string trim(const std::string& str) {
    auto start = str.begin();
    while (start != str.end() && std::isspace(*start)) {
        start++;
    }
    auto end = str.end();
    do {
        end--;
    } while (std::distance(start, end) > 0 && std::isspace(*end));
    return std::string(start, end + 1);
}

static std::vector<std::string> split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;
    while (std::getline(ss, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

static bool startsWith(const std::string& str, const std::string& prefix) {
    return str.size() >= prefix.size() &&
           str.compare(0, prefix.size(), prefix) == 0;
}

static bool endsWith(const std::string& str, const std::string& suffix) {
    return str.size() >= suffix.size() &&
           str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// ============================================================================
// Constructor/Destructor
// ============================================================================

AIMergeResolver::AIMergeResolver() {
}

AIMergeResolver::~AIMergeResolver() {
}

// ============================================================================
// Conflict Detection and Parsing
// ============================================================================

std::vector<ConflictMarker> AIMergeResolver::parseConflicts(
    const std::string& fileContent,
    const std::string& filePath) {
    
    std::vector<ConflictMarker> markers;
    std::istringstream iss(fileContent);
    std::string line;
    int lineNum = 0;
    std::string buffer;
    int conflictStart = -1;
    std::string oursSection, theirsSection, baseSection;
    bool inOurs = false, inBase = false, inTheirs = false;

    while (std::getline(iss, line)) {
        lineNum++;

        // Check for conflict markers
        if (startsWith(line, "<<<<<<<")) {
            if (conflictStart >= 0) {
                // Nested conflict (shouldn't happen, but handle it)
                continue;
            }
            conflictStart = lineNum;
            oursSection.clear();
            baseSection.clear();
            theirsSection.clear();
            inOurs = true;
            inBase = false;
            inTheirs = false;
        }
        else if (startsWith(line, "||||||| ")) {
            // 3-way merge marker (merge base)
            if (inOurs) {
                inOurs = false;
                inBase = true;
                inTheirs = false;
            }
        }
        else if (startsWith(line, "=======")) {
            // Transition from ours to theirs
            if (inOurs) {
                inOurs = false;
                inTheirs = true;
            } else if (inBase) {
                inBase = false;
                inTheirs = true;
            }
        }
        else if (startsWith(line, ">>>>>>>")) {
            // Conflict end
            if (conflictStart >= 0) {
                ConflictMarker marker;
                marker.startLine = conflictStart;
                marker.endLine = lineNum;
                marker.ours = oursSection;
                marker.theirs = theirsSection;
                marker.base = baseSection;
                marker.filePath = filePath;
                marker.is3Way = !baseSection.empty();
                markers.push_back(marker);
                
                conflictStart = -1;
                inOurs = false;
                inBase = false;
                inTheirs = false;
            }
        }
        else if (conflictStart >= 0) {
            // Accumulate conflict content
            if (inOurs) {
                oursSection += line + "\n";
            } else if (inBase) {
                baseSection += line + "\n";
            } else if (inTheirs) {
                theirsSection += line + "\n";
            }
        }
    }

    return markers;
}

std::optional<ConflictMarker> AIMergeResolver::extractConflict(
    const std::string& content,
    int startLine) {
    
    auto conflicts = parseConflicts(content);
    for (const auto& conflict : conflicts) {
        if (conflict.startLine == startLine) {
            return conflict;
        }
    }
    return std::nullopt;
}

bool AIMergeResolver::hasConflicts(const std::string& content) const {
    return content.find("<<<<<<<") != std::string::npos &&
           content.find("=======") != std::string::npos &&
           content.find(">>>>>>>") != std::string::npos;
}

int AIMergeResolver::countConflicts(const std::string& content) const {
    int count = 0;
    size_t pos = 0;
    while ((pos = content.find("<<<<<<<", pos)) != std::string::npos) {
        count++;
        pos++;
    }
    return count;
}

// ============================================================================
// Simple Conflict Removal
// ============================================================================

std::string AIMergeResolver::keepOurs(const std::string& content) const {
    std::string result;
    std::istringstream iss(content);
    std::string line;
    bool inConflict = false;
    bool inOurs = false;

    while (std::getline(iss, line)) {
        if (startsWith(line, "<<<<<<<")) {
            inConflict = true;
            inOurs = true;
        }
        else if (startsWith(line, "||||||| ")) {
            inOurs = false;
        }
        else if (startsWith(line, "=======")) {
            inOurs = false;
        }
        else if (startsWith(line, ">>>>>>>")) {
            inConflict = false;
        }
        else if (!inConflict) {
            result += line + "\n";
        }
        else if (inOurs) {
            result += line + "\n";
        }
    }

    return result;
}

std::string AIMergeResolver::keepTheirs(const std::string& content) const {
    std::string result;
    std::istringstream iss(content);
    std::string line;
    bool inConflict = false;
    bool inTheirs = false;

    while (std::getline(iss, line)) {
        if (startsWith(line, "<<<<<<<")) {
            inConflict = true;
            inTheirs = false;
        }
        else if (startsWith(line, "=======")) {
            inTheirs = true;
        }
        else if (startsWith(line, ">>>>>>>")) {
            inConflict = false;
        }
        else if (!inConflict) {
            result += line + "\n";
        }
        else if (inTheirs) {
            result += line + "\n";
        }
    }

    return result;
}

// ============================================================================
// Semantic Analysis
// ============================================================================

std::vector<std::string> AIMergeResolver::extractMeaningfulLines(
    const std::string& content) const {
    
    std::vector<std::string> lines;
    std::istringstream iss(content);
    std::string line;

    while (std::getline(iss, line)) {
        std::string trimmed = trim(line);
        if (!trimmed.empty()) {
            lines.push_back(trimmed);
        }
    }

    return lines;
}

bool AIMergeResolver::isCodeContent(const std::string& content) const {
    // Very simple heuristic: check for common code patterns
    return content.find('{') != std::string::npos ||
           content.find('}') != std::string::npos ||
           content.find('(') != std::string::npos ||
           content.find(')') != std::string::npos ||
           content.find(';') != std::string::npos ||
           content.find("function") != std::string::npos ||
           content.find("class") != std::string::npos ||
           content.find("def ") != std::string::npos;
}

float AIMergeResolver::stringSimilarity(const std::string& a,
                                        const std::string& b) const {
    // Simplified similarity metric: count common substrings
    if (a == b) return 1.0f;
    if (a.empty() || b.empty()) return 0.0f;

    int matching = 0;
    int minLen = std::min(a.length(), b.length());
    for (int i = 0; i < minLen; ++i) {
        if (a[i] == b[i]) matching++;
    }

    return (float)matching / (float)std::max(a.length(), b.length());
}

AIMergeResolver::ConflictPattern AIMergeResolver::detectPattern(
    const ConflictMarker& conflict) const {
    
    ConflictPattern pattern;
    pattern.isImportConflict = false;
    pattern.isBlankLineConflict = false;
    pattern.isBothAddsToSameBlock = false;
    pattern.isSimpleLineChange = false;
    pattern.isCommentConflict = false;

    auto oursLines = extractMeaningfulLines(conflict.ours);
    auto theirsLines = extractMeaningfulLines(conflict.theirs);

    // Blank line conflict
    if (oursLines.empty() || theirsLines.empty()) {
        pattern.isBlankLineConflict = true;
        return pattern;
    }

    // Import conflict (Python, JavaScript, C++)
    if ((oursLines[0].find("import ") != std::string::npos ||
         oursLines[0].find("from ") != std::string::npos ||
         oursLines[0].find("#include") != std::string::npos) &&
        (theirsLines[0].find("import ") != std::string::npos ||
         theirsLines[0].find("from ") != std::string::npos ||
         theirsLines[0].find("#include") != std::string::npos)) {
        pattern.isImportConflict = true;
        return pattern;
    }

    // Comment conflict
    if ((oursLines[0][0] == '#' || oursLines[0][0] == '/' || oursLines[0][0] == '-') &&
        (theirsLines[0][0] == '#' || theirsLines[0][0] == '/' || theirsLines[0][0] == '-')) {
        pattern.isCommentConflict = true;
    }

    // Single-line change
    if (oursLines.size() == 1 && theirsLines.size() == 1) {
        pattern.isSimpleLineChange = true;
    }

    // Both additions to same logical block
    if (oursLines[0].find('{') != std::string::npos && 
        theirsLines[0].find('{') != std::string::npos) {
        pattern.isBothAddsToSameBlock = true;
    }

    return pattern;
}

std::string AIMergeResolver::findCommonContext(const std::string& ours,
                                               const std::string& theirs) const {
    // Find longest common substring prefix/suffix
    auto oursLines = extractMeaningfulLines(ours);
    auto theirsLines = extractMeaningfulLines(theirs);

    // For now, return a simple heuristic
    if (oursLines.empty() && theirsLines.empty()) {
        return "";
    }

    if (oursLines.empty()) return theirsLines[0];
    if (theirsLines.empty()) return oursLines[0];

    return oursLines[0];  // Simple default
}

std::string AIMergeResolver::merge3Way(const ConflictMarker& conflict) {
    // 3-way merge: compare ours and theirs against base
    if (conflict.base.empty()) {
        // Not actually 3-way
        return combineIntelligently(conflict, detectPattern(conflict));
    }

    auto baseLines = extractMeaningfulLines(conflict.base);
    auto oursLines = extractMeaningfulLines(conflict.ours);
    auto theirsLines = extractMeaningfulLines(conflict.theirs);

    // Simple heuristic: if ours == base, take theirs (they changed it)
    // If theirs == base, take ours (we changed it)
    if (trim(conflict.ours) == trim(conflict.base)) {
        return conflict.theirs;  // They made a change
    }
    if (trim(conflict.theirs) == trim(conflict.base)) {
        return conflict.ours;    // We made a change
    }

    // Both changed - auto-merge if possible
    return combineIntelligently(conflict, detectPattern(conflict));
}

std::string AIMergeResolver::combineIntelligently(
    const ConflictMarker& conflict,
    const ConflictPattern& pattern) {
    
    std::stringstream result;

    if (pattern.isBlankLineConflict) {
        // Just take the non-empty one
        if (!trim(conflict.ours).empty()) {
            return conflict.ours;
        }
        return conflict.theirs;
    }

    if (pattern.isImportConflict) {
        // Combine imports intelligently
        auto oursLines = split(conflict.ours, '\n');
        auto theirsLines = split(conflict.theirs, '\n');
        
        for (const auto& line : oursLines) {
            if (!trim(line).empty()) {
                result << trim(line) << "\n";
            }
        }
        for (const auto& line : theirsLines) {
            std::string trimmed = trim(line);
            if (!trimmed.empty()) {
                // Avoid duplicate imports
                std::string resultStr = result.str();
                if (resultStr.find(trimmed) == std::string::npos) {
                    result << trimmed << "\n";
                }
            }
        }
        return result.str();
    }

    if (pattern.isCommentConflict) {
        // Comments: try to include both
        result << conflict.ours << "\n" << conflict.theirs << "\n";
        return result.str();
    }

    if (pattern.isSimpleLineChange) {
        // Single line: check similarity
        float sim = stringSimilarity(conflict.ours, conflict.theirs);
        if (sim > 0.8f) {
            // Very similar - just take ours
            return conflict.ours;
        }
        // Different enough - return both (will need manual merge)
        result << conflict.ours << "\n" << conflict.theirs << "\n";
        return result.str();
    }

    // Default: combine with both versions separated by comment
    result << conflict.ours << "\n// --- alternative ---\n" << conflict.theirs << "\n";
    return result.str();
}

ResolutionStrategy AIMergeResolver::suggestStrategy(
    const ConflictMarker& conflict) const {
    
    auto pattern = detectPattern(conflict);

    if (pattern.isBlankLineConflict) {
        return ResolutionStrategy::COMBINED;
    }

    if (pattern.isImportConflict) {
        return ResolutionStrategy::SEMANTIC;
    }

    if (pattern.isCommentConflict) {
        return ResolutionStrategy::COMBINED;
    }

    // Check if they're identical
    if (trim(conflict.ours) == trim(conflict.theirs)) {
        return ResolutionStrategy::OURS;
    }

    // Check if it's 3-way with clear winner
    if (conflict.is3Way) {
        if (trim(conflict.ours) == trim(conflict.base)) {
            return ResolutionStrategy::THEIRS;
        }
        if (trim(conflict.theirs) == trim(conflict.base)) {
            return ResolutionStrategy::OURS;
        }
    }

    // Default to semantic analysis
    return ResolutionStrategy::SEMANTIC;
}

// ============================================================================
// Main Resolution Logic
// ============================================================================

ResolutionResult AIMergeResolver::resolveConflict(
    const ConflictMarker& conflict,
    const std::string& context) {
    
    ResolutionResult result;
    result.confidence = 0.0f;
    result.explanation = "";

    // Determine strategy
    result.strategy = suggestStrategy(conflict);

    switch (result.strategy) {
        case ResolutionStrategy::OURS: {
            if (trim(conflict.ours) == trim(conflict.theirs)) {
                result.resolved = conflict.ours;
                result.confidence = 1.0f;
                result.explanation = "Identical content in both versions";
            } else if (conflict.is3Way && 
                     trim(conflict.ours) == trim(conflict.base)) {
                result.resolved = conflict.theirs;
                result.confidence = 0.95f;
                result.explanation = "Theirs modified base, ours didn't";
            } else {
                result.resolved = conflict.ours;
                result.confidence = 0.6f;
                result.explanation = "Choosing our version by default";
            }
            break;
        }

        case ResolutionStrategy::THEIRS: {
            result.resolved = conflict.theirs;
            result.confidence = 0.95f;
            result.explanation = "Ours didn't change from base";
            break;
        }

        case ResolutionStrategy::COMBINED: {
            result.strategy = ResolutionStrategy::COMBINED;
            auto pattern = detectPattern(conflict);
            result.resolved = combineIntelligently(conflict, pattern);
            result.confidence = 0.75f;
            result.explanation = "Intelligently combined versions";
            break;
        }

        case ResolutionStrategy::SEMANTIC: {
            if (conflict.is3Way) {
                result.resolved = merge3Way(conflict);
                result.confidence = 0.80f;
                result.explanation = "3-way merge resolution";
            } else {
                auto pattern = detectPattern(conflict);
                result.resolved = combineIntelligently(conflict, pattern);
                // Check confidence based on similarity
                float sim = stringSimilarity(conflict.ours, conflict.theirs);
                result.confidence = 0.5f + (sim * 0.3f);  // 0.5-0.8 range
                result.explanation = "Semantic analysis with auto-combine";
            }
            break;
        }

        case ResolutionStrategy::MANUAL: {
            result.resolved = "";
            result.confidence = 0.0f;
            result.explanation = "Unable to auto-resolve, manual intervention required";
            break;
        }
    }

    return result;
}

bool AIMergeResolver::analyzeSemantic(const ConflictMarker& conflict,
                                     std::string& resolvedContent,
                                     float& confidence) {
    // This is where more sophisticated semantic analysis would go
    // For now, delegate to resolve logic
    auto result = resolveConflict(conflict);
    if (result.confidence > 0.0f) {
        resolvedContent = result.resolved;
        confidence = result.confidence;
        return true;
    }
    return false;
}

std::string AIMergeResolver::autoResolve(const std::string& fileContent,
                                        const std::string& filePath) {
    
    auto conflicts = parseConflicts(fileContent, filePath);
    
    if (conflicts.empty()) {
        return fileContent;  // No conflicts
    }

    // Process conflicts and rebuild content
    std::string result = fileContent;
    
    // Process in reverse order to maintain line numbers
    for (auto it = conflicts.rbegin(); it != conflicts.rend(); ++it) {
        auto resolution = resolveConflict(*it);
        
        // Only apply if confidence is reasonable
        if (resolution.confidence >= 0.6f) {
            // Find and replace the conflict marker section
            std::string conflictSection = "<<<<<<<";
            size_t pos = result.find(conflictSection);
            
            while (pos != std::string::npos) {
                // Find the end marker
                size_t endPos = result.find(">>>>>>>", pos);
                if (endPos != std::string::npos) {
                    // Find end of line after >>>>>>>
                    size_t endOfLine = result.find('\n', endPos);
                    if (endOfLine == std::string::npos) {
                        endOfLine = result.length();
                    }

                    // Replace entire conflict section with resolved content
                    result.replace(pos, endOfLine - pos, resolution.resolved);
                    break;
                } else {
                    break;
                }
            }
        }
    }

    return result;
}

} // namespace Git
} // namespace RawrXD
=======
#include "ai_merge_resolver.hpp"


AIMergeResolver::AIMergeResolver(void* parent)
    : void(parent)
{
    logStructured("INFO", "AIMergeResolver initializing", void*{{"component", "AIMergeResolver"}});
    logStructured("INFO", "AIMergeResolver initialized successfully", void*{{"component", "AIMergeResolver"}});
}

AIMergeResolver::~AIMergeResolver()
{
    logStructured("INFO", "AIMergeResolver shutting down", void*{{"component", "AIMergeResolver"}});
}

void AIMergeResolver::setConfig(const Config& config)
{
    std::lock_guard<std::mutex> locker(&m_configMutex);
    m_config = config;
    logStructured("INFO", "Configuration updated", void*{
        {"enableAutoResolve", config.enableAutoResolve},
        {"minConfidenceThreshold", config.minConfidenceThreshold},
        {"maxConflictSize", config.maxConflictSize}
    });
}

AIMergeResolver::Config AIMergeResolver::getConfig() const
{
    std::lock_guard<std::mutex> locker(&m_configMutex);
    return m_config;
}

std::vector<AIMergeResolver::ConflictBlock> AIMergeResolver::detectConflicts(const std::string& filePath)
{
    auto startTime = std::chrono::steady_clock::now();
    std::vector<ConflictBlock> conflicts;
    
    try {
        std::fstream file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            logStructured("ERROR", "Failed to open file for conflict detection", void*{
                {"filePath", filePath},
                {"error", file.errorString()}
            });
            std::lock_guard<std::mutex> metricsLocker(&m_metricsMutex);
            m_metrics.errorCount++;
            errorOccurred(std::string("Failed to open file: %1")));
            return conflicts;
        }
        
        QTextStream in(&file);
        std::string content = in.readAll();
        file.close();
        
        // Detect conflict markers
        std::regex conflictStart("^<<<<<<< (.+)$", std::regex::MultilineOption);
        std::regex conflictMid("^=======\\s*$", std::regex::MultilineOption);
        std::regex conflictEnd("^>>>>>>> (.+)$", std::regex::MultilineOption);
        
        std::vector<std::string> lines = content.split('\n');
        int i = 0;
        
        while (i < lines.size()) {
            std::smatch startMatch = conflictStart.match(lines[i]);
            if (startMatch.hasMatch()) {
                ConflictBlock conflict;
                conflict.file = filePath;
                conflict.startLine = i + 1;
                
                std::string currentBranch = startMatch"";
                std::vector<std::string> currentLines;
                i++;
                
                // Collect current version lines
                while (i < lines.size() && !conflictMid.match(lines[i]).hasMatch()) {
                    currentLines.append(lines[i]);
                    i++;
                }
                
                conflict.currentVersion = currentLines.join("\n");
                
                if (i < lines.size()) {
                    i++; // Skip =======
                }
                
                // Collect incoming version lines
                std::vector<std::string> incomingLines;
                while (i < lines.size() && !conflictEnd.match(lines[i]).hasMatch()) {
                    incomingLines.append(lines[i]);
                    i++;
                }
                
                conflict.incomingVersion = incomingLines.join("\n");
                
                if (i < lines.size()) {
                    std::smatch endMatch = conflictEnd.match(lines[i]);
                    if (endMatch.hasMatch()) {
                        std::string incomingBranch = endMatch"";
                        conflict.endLine = i + 1;
                        
                        // Extract context (5 lines before and after)
                        int contextStart = qMax(0, conflict.startLine - 6);
                        int contextEnd = qMin(lines.size(), conflict.endLine + 5);
                        conflict.context = lines.mid(contextStart, contextEnd - contextStart).join("\n");
                        
                        conflicts.append(conflict);
                    }
                }
            }
            i++;
        }
        
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        
        {
            std::lock_guard<std::mutex> metricsLocker(&m_metricsMutex);
            m_metrics.conflictsDetected += conflicts.size();
        }
        
        logStructured("INFO", "Conflicts detected", void*{
            {"filePath", filePath},
            {"conflictCount", conflicts.size()},
            {"latencyMs", duration.count()}
        });
        
        conflictsDetected(conflicts.size());
        
    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> metricsLocker(&m_metricsMutex);
        m_metrics.errorCount++;
        logStructured("ERROR", "Exception during conflict detection", void*{{"error", e.what()}});
        errorOccurred(std::string("Conflict detection failed: %1")));
    }
    
    return conflicts;
}

AIMergeResolver::Resolution AIMergeResolver::resolveConflict(const ConflictBlock& conflict)
{
    auto startTime = std::chrono::steady_clock::now();
    Resolution resolution;
    
    try {
        Config config;
        {
            std::lock_guard<std::mutex> configLocker(&m_configMutex);
            config = m_config;
        }
        
        // Validate conflict size
        int conflictSize = conflict.currentVersion.length() + conflict.incomingVersion.length();
        if (conflictSize > config.maxConflictSize) {
            logStructured("WARN", "Conflict exceeds max size, requires manual resolution", void*{
                {"conflictSize", conflictSize},
                {"maxSize", config.maxConflictSize}
            });
            resolution.requiresManualReview = true;
            resolution.explanation = "Conflict too large for automated resolution";
            return resolution;
        }
        
        // Prepare AI request
        void* payload;
        payload["current_version"] = conflict.currentVersion;
        payload["incoming_version"] = conflict.incomingVersion;
        payload["base_version"] = conflict.baseVersion;
        payload["context"] = conflict.context;
        payload["file"] = conflict.file;
        
        logStructured("DEBUG", "Sending conflict resolution request to AI", void*{
            {"file", conflict.file},
            {"conflictSize", conflictSize}
        });
        
        void* response = makeAiRequest(config.aiEndpoint + "/resolve-conflict", payload);
        
        resolution.resolvedContent = response["resolved_content"].toString();
        resolution.confidence = response["confidence"].toDouble();
        resolution.strategy = response["strategy"].toString();
        resolution.explanation = response["explanation"].toString();
        resolution.requiresManualReview = resolution.confidence < config.minConfidenceThreshold;
        
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        recordLatency("conflict_resolution", duration);
        
        {
            std::lock_guard<std::mutex> metricsLocker(&m_metricsMutex);
            m_metrics.conflictsResolved++;
            if (!resolution.requiresManualReview) {
                m_metrics.autoResolved++;
            } else {
                m_metrics.manualResolved++;
            }
            
            // Update average confidence
            m_metrics.avgResolutionConfidence = 
                (m_metrics.avgResolutionConfidence * (m_metrics.conflictsResolved - 1) + resolution.confidence) 
                / m_metrics.conflictsResolved;
        }
        
        logStructured("INFO", "Conflict resolved", void*{
            {"file", conflict.file},
            {"confidence", resolution.confidence},
            {"strategy", resolution.strategy},
            {"requiresManualReview", resolution.requiresManualReview},
            {"latencyMs", duration.count()}
        });
        
        // Audit log
        if (config.enableAuditLog) {
            logAudit("conflict_resolved", void*{
                {"file", conflict.file},
                {"startLine", conflict.startLine},
                {"endLine", conflict.endLine},
                {"resolution", resolution.resolvedContent},
                {"confidence", resolution.confidence},
                {"strategy", resolution.strategy}
            });
        }
        
        conflictResolved(resolution);
        
    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> metricsLocker(&m_metricsMutex);
        m_metrics.errorCount++;
        logStructured("ERROR", "Conflict resolution failed", void*{{"error", e.what()}});
        errorOccurred(std::string("Resolution failed: %1")));
        resolution.requiresManualReview = true;
        resolution.explanation = std::string("AI resolution failed: %1"));
    }
    
    return resolution;
}

bool AIMergeResolver::applyResolution(const std::string& filePath, const Resolution& resolution, int lineStart, int lineEnd)
{
    auto startTime = std::chrono::steady_clock::now();
    
    try {
        std::fstream file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            logStructured("ERROR", "Failed to open file for applying resolution", void*{
                {"filePath", filePath},
                {"error", file.errorString()}
            });
            std::lock_guard<std::mutex> metricsLocker(&m_metricsMutex);
            m_metrics.errorCount++;
            errorOccurred(std::string("Failed to open file: %1")));
            return false;
        }
        
        QTextStream in(&file);
        std::vector<std::string> lines;
        while (!in.atEnd()) {
            lines.append(in.readLine());
        }
        file.close();
        
        // Replace conflict block with resolution
        if (lineStart < 1 || lineEnd > lines.size() || lineStart > lineEnd) {
            logStructured("ERROR", "Invalid line range for resolution", void*{
                {"lineStart", lineStart},
                {"lineEnd", lineEnd},
                {"fileLines", lines.size()}
            });
            std::lock_guard<std::mutex> metricsLocker(&m_metricsMutex);
            m_metrics.errorCount++;
            return false;
        }
        
        // Remove conflict lines
        lines.erase(lines.begin() + lineStart - 1, lines.begin() + lineEnd);
        
        // Insert resolved content
        std::vector<std::string> resolvedLines = resolution.resolvedContent.split('\n');
        for (int i = resolvedLines.size() - 1; i >= 0; i--) {
            lines.insert(lineStart - 1, resolvedLines[i]);
        }
        
        // Write back to file
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            logStructured("ERROR", "Failed to open file for writing resolution", void*{
                {"filePath", filePath},
                {"error", file.errorString()}
            });
            std::lock_guard<std::mutex> metricsLocker(&m_metricsMutex);
            m_metrics.errorCount++;
            errorOccurred(std::string("Failed to write file: %1")));
            return false;
        }
        
        QTextStream out(&file);
        for (const std::string& line : lines) {
            out << line << "\n";
        }
        file.close();
        
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        
        logStructured("INFO", "Resolution applied successfully", void*{
            {"filePath", filePath},
            {"lineStart", lineStart},
            {"lineEnd", lineEnd},
            {"latencyMs", duration.count()}
        });
        
        Config config;
        {
            std::lock_guard<std::mutex> configLocker(&m_configMutex);
            config = m_config;
        }
        
        if (config.enableAuditLog) {
            logAudit("resolution_applied", void*{
                {"file", filePath},
                {"lineStart", lineStart},
                {"lineEnd", lineEnd},
                {"resolution", resolution.resolvedContent}
            });
        }
        
        return true;
        
    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> metricsLocker(&m_metricsMutex);
        m_metrics.errorCount++;
        logStructured("ERROR", "Exception applying resolution", void*{{"error", e.what()}});
        errorOccurred(std::string("Apply resolution failed: %1")));
        return false;
    }
}

void* AIMergeResolver::analyzeSemanticMerge(const std::string& base, const std::string& current, const std::string& incoming)
{
    auto startTime = std::chrono::steady_clock::now();
    void* analysis;
    
    try {
        Config config;
        {
            std::lock_guard<std::mutex> configLocker(&m_configMutex);
            config = m_config;
        }
        
        void* payload;
        payload["base"] = base;
        payload["current"] = current;
        payload["incoming"] = incoming;
        payload["analysis_type"] = "semantic";
        
        logStructured("DEBUG", "Requesting semantic merge analysis", void*{
            {"baseSize", base.length()},
            {"currentSize", current.length()},
            {"incomingSize", incoming.length()}
        });
        
        analysis = makeAiRequest(config.aiEndpoint + "/analyze-semantic-merge", payload);
        
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        
        logStructured("INFO", "Semantic analysis completed", void*{
            {"hasConflicts", analysis["has_conflicts"].toBool()},
            {"semanticChanges", analysis["semantic_changes"].toArray().size()},
            {"latencyMs", duration.count()}
        });
        
    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> metricsLocker(&m_metricsMutex);
        m_metrics.errorCount++;
        logStructured("ERROR", "Semantic analysis failed", void*{{"error", e.what()}});
        errorOccurred(std::string("Semantic analysis failed: %1")));
    }
    
    return analysis;
}

std::vector<std::string> AIMergeResolver::detectBreakingChanges(const std::string& diff)
{
    auto startTime = std::chrono::steady_clock::now();
    std::vector<std::string> breakingChanges;
    
    try {
        Config config;
        {
            std::lock_guard<std::mutex> configLocker(&m_configMutex);
            config = m_config;
        }
        
        void* payload;
        payload["diff"] = diff;
        payload["detection_mode"] = "breaking_changes";
        
        logStructured("DEBUG", "Detecting breaking changes", void*{{"diffSize", diff.length()}});
        
        void* response = makeAiRequest(config.aiEndpoint + "/detect-breaking-changes", payload);
        
        void* changesArray = response["breaking_changes"].toArray();
        for (const void*& value : changesArray) {
            breakingChanges.append(value.toString());
        }
        
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        
        {
            std::lock_guard<std::mutex> metricsLocker(&m_metricsMutex);
            m_metrics.breakingChangesDetected += breakingChanges.size();
        }
        
        logStructured("INFO", "Breaking changes detected", void*{
            {"count", breakingChanges.size()},
            {"latencyMs", duration.count()}
        });
        
        for (const std::string& change : breakingChanges) {
            breakingChangeDetected(change);
        }
        
    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> metricsLocker(&m_metricsMutex);
        m_metrics.errorCount++;
        logStructured("ERROR", "Breaking change detection failed", void*{{"error", e.what()}});
        errorOccurred(std::string("Breaking change detection failed: %1")));
    }
    
    return breakingChanges;
}

AIMergeResolver::Metrics AIMergeResolver::getMetrics() const
{
    std::lock_guard<std::mutex> locker(&m_metricsMutex);
    return m_metrics;
}

void AIMergeResolver::resetMetrics()
{
    std::lock_guard<std::mutex> locker(&m_metricsMutex);
    m_metrics = Metrics();
    logStructured("INFO", "Metrics reset", void*{});
}

void AIMergeResolver::logStructured(const std::string& level, const std::string& message, const void*& context)
{
    void* logEntry;
    logEntry["timestamp"] = std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate);
    logEntry["level"] = level;
    logEntry["component"] = "AIMergeResolver";
    logEntry["message"] = message;
    logEntry["context"] = context;
    
    void* doc(logEntry);
}

void AIMergeResolver::recordLatency(const std::string& operation, const std::chrono::milliseconds& duration)
{
    std::lock_guard<std::mutex> locker(&m_metricsMutex);
    
    if (operation == "conflict_resolution") {
        m_metrics.avgResolutionLatencyMs = 
            (m_metrics.avgResolutionLatencyMs * (m_metrics.conflictsResolved - 1) + duration.count()) 
            / m_metrics.conflictsResolved;
    }
    
    Config config;
    {
        std::lock_guard<std::mutex> configLocker(&m_configMutex);
        config = m_config;
    }
    
    if (config.enableMetrics) {
        metricsUpdated(m_metrics);
    }
}

void AIMergeResolver::logAudit(const std::string& action, const void*& details)
{
    Config config;
    {
        std::lock_guard<std::mutex> configLocker(&m_configMutex);
        config = m_config;
    }
    
    if (!config.enableAuditLog || config.auditLogPath.empty()) {
        return;
    }
    
    void* auditEntry;
    auditEntry["timestamp"] = std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate);
    auditEntry["action"] = action;
    auditEntry["details"] = details;
    
    std::fstream auditFile(config.auditLogPath);
    if (auditFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&auditFile);
        void* doc(auditEntry);
        out << doc.toJson(void*::Compact) << "\n";
        auditFile.close();
    } else {
        logStructured("ERROR", "Failed to write audit log", void*{
            {"path", config.auditLogPath},
            {"error", auditFile.errorString()}
        });
    }
}

void* AIMergeResolver::makeAiRequest(const std::string& endpoint, const void*& payload)
{
    void* manager;
    void* request(endpoint);
    
    Config config;
    {
        std::lock_guard<std::mutex> configLocker(&m_configMutex);
        config = m_config;
    }
    
    request.setHeader(void*::ContentTypeHeader, "application/json");
    if (!config.apiKey.empty()) {
        request.setRawHeader("Authorization", std::string("Bearer %1").toUtf8());
    }
    
    void* doc(payload);
    std::vector<uint8_t> data = doc.toJson(void*::Compact);
    
    void* loop;
    void** reply = manager.post(request, data);
// Qt connect removed
    loop.exec();
    
    void* response;
    if (reply->error() == void*::NoError) {
        std::vector<uint8_t> responseData = reply->readAll();
        void* responseDoc = void*::fromJson(responseData);
        response = responseDoc.object();
    } else {
        logStructured("ERROR", "AI API request failed", void*{
            {"endpoint", endpoint},
            {"error", reply->errorString()}
        });
        throw std::runtime_error(reply->errorString().toStdString());
    }
    
    reply->deleteLater();
    return response;
}

bool AIMergeResolver::validateResolution(const Resolution& resolution, const ConflictBlock& conflict)
{
    if (resolution.resolvedContent.empty()) {
        logStructured("WARN", "Empty resolution content", void*{});
        return false;
    }
    
    Config config;
    {
        std::lock_guard<std::mutex> configLocker(&m_configMutex);
        config = m_config;
    }
    
    if (resolution.confidence < config.minConfidenceThreshold) {
        logStructured("WARN", "Resolution confidence below threshold", void*{
            {"confidence", resolution.confidence},
            {"threshold", config.minConfidenceThreshold}
        });
        return false;
    }
    
    return true;
}


>>>>>>> origin/main
