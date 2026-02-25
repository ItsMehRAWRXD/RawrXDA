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
