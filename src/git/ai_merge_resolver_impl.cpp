<<<<<<< HEAD
// ============================================================================
// ai_merge_resolver_impl.cpp — Advanced Implementation Details
// ============================================================================
// Extended implementation for specialized conflict resolution scenarios,
// language-specific processing, and advanced merge strategies.
// ============================================================================

#include "ai_merge_resolver.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace RawrXD {
namespace Git {

// ============================================================================
// Language-Specific Conflict Resolution
// ============================================================================

enum class CodeLanguage : uint8_t {
    UNKNOWN = 0,
    PYTHON = 1,
    JAVASCRIPT = 2,
    CPLUSPLUS = 3,
    CSHARP = 4,
    JAVA = 5,
    RUST = 6,
    GO = 7,
    TYPESCRIPT = 8
};

static CodeLanguage detectLanguage(const std::string& filePath) {
    if (filePath.find(".py") != std::string::npos) return CodeLanguage::PYTHON;
    if (filePath.find(".js") != std::string::npos) return CodeLanguage::JAVASCRIPT;
    if (filePath.find(".ts") != std::string::npos) return CodeLanguage::TYPESCRIPT;
    if (filePath.find(".cpp") != std::string::npos ||
        filePath.find(".cc") != std::string::npos ||
        filePath.find(".cxx") != std::string::npos) return CodeLanguage::CPLUSPLUS;
    if (filePath.find(".cs") != std::string::npos) return CodeLanguage::CSHARP;
    if (filePath.find(".java") != std::string::npos) return CodeLanguage::JAVA;
    if (filePath.find(".rs") != std::string::npos) return CodeLanguage::RUST;
    if (filePath.find(".go") != std::string::npos) return CodeLanguage::GO;
    return CodeLanguage::UNKNOWN;
}

// ============================================================================
// AST-like Pattern Matching for Code Blocks
// ============================================================================

struct CodeBlock {
    std::string type;           // "function", "class", "import", "comment"
    std::string name;           // function/class name
    int startLine;
    int endLine;
    std::vector<std::string> lines;
};

static std::vector<CodeBlock> parseCodeBlocks(const std::string& content,
                                              CodeLanguage lang) {
    std::vector<CodeBlock> blocks;
    std::istringstream iss(content);
    std::string line;
    int lineNum = 0;

    CodeBlock currentBlock;
    currentBlock.startLine = 0;
    bool inBlock = false;

    while (std::getline(iss, line)) {
        lineNum++;
        currentBlock.lines.push_back(line);

        // Simple pattern matching for Python
        if (lang == CodeLanguage::PYTHON) {
            if (line.find("def ") != std::string::npos) {
                if (inBlock) blocks.push_back(currentBlock);
                currentBlock = CodeBlock();
                currentBlock.type = "function";
                currentBlock.startLine = lineNum;
                inBlock = true;
                // Extract function name
                size_t defPos = line.find("def ");
                size_t parenPos = line.find("(", defPos);
                if (parenPos != std::string::npos) {
                    currentBlock.name = line.substr(defPos + 4, parenPos - defPos - 4);
                }
            }
            else if (line.find("class ") != std::string::npos) {
                if (inBlock) blocks.push_back(currentBlock);
                currentBlock = CodeBlock();
                currentBlock.type = "class";
                currentBlock.startLine = lineNum;
                inBlock = true;
                size_t classPos = line.find("class ");
                size_t colonPos = line.find(":", classPos);
                if (colonPos != std::string::npos) {
                    currentBlock.name = line.substr(classPos + 6, colonPos - classPos - 6);
                }
            }
        }
        // Pattern matching for C++
        else if (lang == CodeLanguage::CPLUSPLUS) {
            // Look for function signatures
            if ((line.find("(") != std::string::npos && 
                 line.find(")") != std::string::npos) ||
                line.find("{") != std::string::npos) {
                if (inBlock && !currentBlock.lines.empty()) {
                    blocks.push_back(currentBlock);
                }
                currentBlock = CodeBlock();
                currentBlock.type = "block";
                currentBlock.startLine = lineNum;
                inBlock = true;
            }
        }
    }

    if (inBlock && !currentBlock.lines.empty()) {
        blocks.push_back(currentBlock);
    }

    return blocks;
}

// ============================================================================
// Diff-based Conflict Analysis
// ============================================================================

struct DiffMetrics {
    int oursAdditions;
    int oursRemovals;
    int theirsAdditions;
    int theirsRemovals;
    float oursComplexity;    // 0.0-1.0
    float theirsComplexity;
};

static DiffMetrics analyzeDifferences(const ConflictMarker& conflict) {
    DiffMetrics metrics;
    metrics.oursAdditions = 0;
    metrics.oursRemovals = 0;
    metrics.theirsAdditions = 0;
    metrics.theirsRemovals = 0;

    // Count lines (simple heuristic)
    std::istringstream oursIss(conflict.ours);
    std::string line;
    while (std::getline(oursIss, line)) {
        if (!line.empty()) metrics.oursAdditions++;
    }

    std::istringstream theirsIss(conflict.theirs);
    while (std::getline(theirsIss, line)) {
        if (!line.empty()) metrics.theirsAdditions++;
    }

    // Base removals
    if (conflict.is3Way) {
        std::istringstream baseIss(conflict.base);
        int baseLines = 0;
        while (std::getline(baseIss, line)) {
            if (!line.empty()) baseLines++;
        }
        metrics.oursRemovals = std::abs(metrics.oursAdditions - baseLines);
        metrics.theirsRemovals = std::abs(metrics.theirsAdditions - baseLines);
    }

    // Complexity: count special characters indicating code
    auto countComplexity = [](const std::string& content) -> float {
        int complexChars = 0;
        for (char c : content) {
            if (c == '{' || c == '}' || c == '(' || c == ')' ||
                c == '[' || c == ']' || c == ';' || c == ':') {
                complexChars++;
            }
        }
        return complexChars / std::max(1.0f, (float)content.length());
    };

    metrics.oursComplexity = countComplexity(conflict.ours);
    metrics.theirsComplexity = countComplexity(conflict.theirs);

    return metrics;
}

// ============================================================================
// Import Deduplication
// ============================================================================

static std::string deduplicateImports(const std::string& ours,
                                      const std::string& theirs) {
    std::unordered_set<std::string> seenImports;
    std::stringstream result;

    auto processLines = [&](const std::string& content) {
        std::istringstream iss(content);
        std::string line;
        while (std::getline(iss, line)) {
            std::string trimmed = line;
            // Simple trim
            trimmed.erase(0, trimmed.find_first_not_of(" \t"));
            trimmed.erase(trimmed.find_last_not_of(" \t") + 1);

            if (!trimmed.empty()) {
                if (seenImports.find(trimmed) == seenImports.end()) {
                    seenImports.insert(trimmed);
                    result << line << "\n";
                }
            }
        }
    };

    processLines(ours);
    processLines(theirs);

    return result.str();
}

// ============================================================================
// Bracket/Brace Matching for Code Blocks
// ============================================================================

struct BracketContext {
    int openCount;
    int closeCount;
    bool isBalanced() const { return openCount == closeCount; }
};

static BracketContext analyzeBrackets(const std::string& content) {
    BracketContext ctx;
    ctx.openCount = 0;
    ctx.closeCount = 0;

    for (char c : content) {
        if (c == '{' || c == '(' || c == '[') {
            ctx.openCount++;
        } else if (c == '}' || c == ')' || c == ']') {
            ctx.closeCount++;
        }
    }

    return ctx;
}

// ============================================================================
// Whitespace Management
// ============================================================================

static std::string normalizeWhitespace(const std::string& content) {
    std::string result;
    bool lastWasSpace = false;

    for (char c : content) {
        if (std::isspace(c)) {
            if (!lastWasSpace) {
                result += ' ';
                lastWasSpace = true;
            }
        } else {
            result += c;
            lastWasSpace = false;
        }
    }

    return result;
}

// ============================================================================
// Extended Resolution Strategies for Specialized Cases
// ============================================================================

class ConflictResolver {
public:
    static std::string resolveImportConflict(const ConflictMarker& conflict) {
        // Combine and deduplicate imports
        return deduplicateImports(conflict.ours, conflict.theirs);
    }

    static std::string resolveBracketConflict(const ConflictMarker& conflict) {
        auto oursCtx = analyzeBrackets(conflict.ours);
        auto theirsCtx = analyzeBrackets(conflict.theirs);

        // If one is balanced and the other isn't, use the balanced one
        if (oursCtx.isBalanced() && !theirsCtx.isBalanced()) {
            return conflict.ours;
        }
        if (theirsCtx.isBalanced() && !oursCtx.isBalanced()) {
            return conflict.theirs;
        }

        // Both balanced or both unbalanced: combine
        return conflict.ours + "\n" + conflict.theirs;
    }

    static std::string resolveWithinSameBlock(const ConflictMarker& conflict) {
        // Both are additions to the same logical block
        // Combine them, trying to maintain some order
        std::stringstream result;
        result << "{\n";
        result << conflict.ours << "\n";
        result << conflict.theirs << "\n";
        result << "}";
        return result.str();
    }

    static std::string resolveCommentConflict(const ConflictMarker& conflict) {
        // Include both comments
        std::stringstream result;
        result << "// " << conflict.ours << "\n";
        result << "// " << conflict.theirs << "\n";
        return result.str();
    }

    static std::string resolveLineLengthConflict(const ConflictMarker& conflict) {
        // Prefer shorter implementations
        if (conflict.ours.length() < conflict.theirs.length()) {
            return conflict.ours;
        }
        return conflict.theirs;
    }

    static std::string resolveByLanguage(const ConflictMarker& conflict,
                                        CodeLanguage lang) {
        switch (lang) {
            case CodeLanguage::PYTHON:
                // Python-specific: check indentation
                // For simplicity, just use standard resolution
                break;
            case CodeLanguage::JAVASCRIPT:
            case CodeLanguage::TYPESCRIPT:
                // JS/TS: Handle async/await conflicts specially
                if (conflict.ours.find("async") != std::string::npos ||
                    conflict.theirs.find("async") != std::string::npos) {
                    // Take whichever has async
                    if (conflict.ours.find("async") != std::string::npos) {
                        return conflict.ours;
                    }
                    return conflict.theirs;
                }
                break;
            case CodeLanguage::CPLUSPLUS:
            case CodeLanguage::RUST:
                // Use bracket analysis
                return resolveBracketConflict(conflict);
            default:
                break;
        }
        return "";
    }
};

// ============================================================================
// Extended Analysis Helper
// ============================================================================

// This internal function extends the main resolver with specialized handling
extern "C" {
    struct AnalysisResult {
        bool shouldAutoResolve;
        float confidence;
        const char* suggestedResolution;
        const char* reason;
    };

    // Advanced analysis not exposed in public API but available for internal use
    AnalysisResult analyzeConflictAdvanced(const ConflictMarker* conflict,
                                          const char* filePath) {
        AnalysisResult result;
        result.shouldAutoResolve = false;
        result.confidence = 0.0f;
        result.suggestedResolution = nullptr;
        result.reason = "Analysis incomplete";

        if (!conflict || !filePath) {
            return result;
        }

        // Detect language
        CodeLanguage lang = detectLanguage(std::string(filePath));

        // Analyze differences
        DiffMetrics metrics = analyzeDifferences(*conflict);

        // Simple heuristic: if changes are similar size and complexity is low, safe to combine
        if (std::abs(metrics.oursAdditions - metrics.theirsAdditions) <= 2 &&
            metrics.oursComplexity < 0.3f && metrics.theirsComplexity < 0.3f) {
            result.shouldAutoResolve = true;
            result.confidence = 0.85f;
            result.reason = "Low-complexity, similar-sized changes";
        }

        return result;
    }
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
