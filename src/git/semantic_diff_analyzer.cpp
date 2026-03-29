// ============================================================================
// semantic_diff_analyzer.cpp — Semantic Diff Analysis Implementation
// ============================================================================
// Semantic-level code diff analysis and impact assessment.
// ============================================================================

#include "semantic_diff_analyzer.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <cmath>
#include <unordered_set>

namespace RawrXD {
namespace Git {

// ============================================================================
// String Utilities
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

static bool contains(const std::string& str, const std::string& substr) {
    return str.find(substr) != std::string::npos;
}

// ============================================================================
// Constructor/Destructor
// ============================================================================

SemanticDiffAnalyzer::SemanticDiffAnalyzer() {
}

SemanticDiffAnalyzer::~SemanticDiffAnalyzer() {
}

// ============================================================================
// File Detection
// ============================================================================

bool SemanticDiffAnalyzer::isTestFile(const std::string& filePath) const {
    return contains(filePath, "test") ||
           contains(filePath, "spec") ||
           contains(filePath, "__test__") ||
           endsWith(filePath, ".test.js") ||
           endsWith(filePath, ".spec.js") ||
           endsWith(filePath, "Test.java") ||
           endsWith(filePath, "Tests.cs") ||
           contains(filePath, "/tests/") ||
           contains(filePath, "\\tests\\");
}

// ============================================================================
// Symbol Extraction
// ============================================================================

std::string SemanticDiffAnalyzer::extractSymbolName(const std::string& line) const {
    std::string s = trim(line);

    // Python: def function_name(
    if (startsWith(s, "def ")) {
        size_t parenPos = s.find('(');
        if (parenPos != std::string::npos) {
            return s.substr(4, parenPos - 4);
        }
    }

    // JavaScript/TypeScript: function name(...) or const name = (...)
    if (startsWith(s, "function ")) {
        size_t parenPos = s.find('(');
        if (parenPos != std::string::npos) {
            return trim(s.substr(9, parenPos - 9));
        }
    }

    // C++/C#/Java: type name(
    std::vector<char> delimiters = {'(', '{', '=', ':'};
    for (char delim : delimiters) {
        size_t pos = s.find(delim);
        if (pos != std::string::npos) {
            std::string candidate = trim(s.substr(0, pos));
            // Last word is likely the name
            size_t lastSpace = candidate.find_last_of(" \t");
            if (lastSpace != std::string::npos) {
                return candidate.substr(lastSpace + 1);
            }
            return candidate;
        }
    }

    return "";
}

std::vector<std::string> SemanticDiffAnalyzer::extractFunctionSignatures(
    const std::string& code,
    const std::string& fileName) {
    
    std::vector<std::string> signatures;
    std::istringstream iss(code);
    std::string line;

    while (std::getline(iss, line)) {
        std::string trimmed = trim(line);

        // Python
        if (startsWith(trimmed, "def ") && contains(trimmed, "(")) {
            signatures.push_back(trimmed);
        }
        // JavaScript/TypeScript
        else if (startsWith(trimmed, "function ") ||
                 (contains(trimmed, "=>") && !startsWith(trimmed, "//"))) {
            signatures.push_back(trimmed);
        }
        // C++/C#
        else if ((contains(trimmed, "(") && contains(trimmed, ")")) &&
                 !startsWith(trimmed, "//") &&
                 !startsWith(trimmed, "/*")) {
            // Could be function signature
            if (contains(trimmed, "{") || endsWith(trimmed, ";")) {
                signatures.push_back(trimmed);
            }
        }
    }

    return signatures;
}

std::string SemanticDiffAnalyzer::findFunctionAtLine(const std::string& content,
                                                    int lineNumber,
                                                    const std::string& fileName) {
    std::istringstream iss(content);
    std::string line;
    int currentLine = 0;
    std::string lastFunction;

    while (std::getline(iss, line)) {
        currentLine++;
        
        if (currentLine > lineNumber) {
            break;
        }

        std::string trimmed = trim(line);

        // Track function definitions
        if (startsWith(trimmed, "def ") || 
            startsWith(trimmed, "function ") ||
            (contains(trimmed, "()") && contains(trimmed, "{"))) {
            lastFunction = extractSymbolName(trimmed);
        }
    }

    return lastFunction;
}

// ============================================================================
// Keyword Detection
// ============================================================================

bool SemanticDiffAnalyzer::hasSecurityKeywords(const std::string& content) const {
    const std::vector<std::string> securityKeywords = {
        "password", "secret", "crypto", "encrypt", "decrypt",
        "hash", "token", "auth", "permission", "privilege",
        "ssl", "tls", "https", "security", "vulnerability",
        "sql_injection", "xss", "csrf", "sanitize", "validate"
    };

    std::string lower = content;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    for (const auto& keyword : securityKeywords) {
        if (contains(lower, keyword)) {
            return true;
        }
    }
    return false;
}

bool SemanticDiffAnalyzer::isDeadCode(const std::string& code) const {
    std::string lower = code;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    return contains(lower, "todo") ||
           contains(lower, "fixme") ||
           contains(lower, "deprecated") ||
           contains(lower, "unused") ||
           contains(lower, "remove") ||
           contains(lower, "delete this");
}

std::vector<std::string> SemanticDiffAnalyzer::extractImports(
    const std::string& code) const {
    
    std::vector<std::string> imports;
    std::istringstream iss(code);
    std::string line;

    while (std::getline(iss, line)) {
        std::string trimmed = trim(line);

        // Python
        if (startsWith(trimmed, "import ") || startsWith(trimmed, "from ")) {
            imports.push_back(trimmed);
        }
        // JavaScript
        else if (startsWith(trimmed, "import ") || startsWith(trimmed, "require(")) {
            imports.push_back(trimmed);
        }
        // C++
        else if (startsWith(trimmed, "#include")) {
            imports.push_back(trimmed);
        }
        // C#/Java
        else if (startsWith(trimmed, "using ") || startsWith(trimmed, "import ")) {
            imports.push_back(trimmed);
        }
    }

    return imports;
}

// ============================================================================
// Tokenization
// ============================================================================

std::vector<std::string> SemanticDiffAnalyzer::tokenize(const std::string& code) const {
    std::vector<std::string> tokens;
    std::string current;

    for (size_t i = 0; i < code.length(); ++i) {
        char c = code[i];

        if (std::isalnum(c) || c == '_') {
            current += c;
        } else {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
            if (c == '(' || c == ')' || c == '{' || c == '}' ||
                c == '[' || c == ']' || c == ';' || c == ':' || c == ',') {
                tokens.push_back(std::string(1, c));
            }
        }
    }

    if (!current.empty()) {
        tokens.push_back(current);
    }

    return tokens;
}

// ============================================================================
// Similarity Calculation
// ============================================================================

float SemanticDiffAnalyzer::calculateSimilarity(const std::string& a,
                                               const std::string& b) const {
    if (a == b) return 1.0f;
    if (a.empty() || b.empty()) return 0.0f;

    auto tokensA = tokenize(a);
    auto tokensB = tokenize(b);

    // Jaccard similarity
    std::unordered_set<std::string> union_set(tokensA.begin(), tokensA.end());
    union_set.insert(tokensB.begin(), tokensB.end());

    std::unordered_set<std::string> intersect;
    for (const auto& t : tokensA) {
        if (std::find(tokensB.begin(), tokensB.end(), t) != tokensB.end()) {
            intersect.insert(t);
        }
    }

    return (float)intersect.size() / (float)union_set.size();
}

// ============================================================================
// Pattern Detection
// ============================================================================

bool SemanticDiffAnalyzer::isBugFix(const std::string& oldCode,
                                    const std::string& newCode) const {
    // Heuristic: bug fixes often involve:
    // - Adding error checks
    // - Fixing conditions
    // - Adding safeguards

    std::string lower_old = oldCode;
    std::string lower_new = newCode;
    std::transform(lower_old.begin(), lower_old.end(), lower_old.begin(), ::tolower);
    std::transform(lower_new.begin(), lower_new.end(), lower_new.begin(), ::tolower);

    // Check for error handling additions
    if (contains(lower_new, "try") || contains(lower_new, "catch") ||
        contains(lower_new, "throw") || contains(lower_new, "error")) {
        if (!contains(lower_old, "try") && !contains(lower_old, "catch")) {
            return true;
        }
    }

    // Check for condition fixes
    if ((contains(lower_new, "!=") || contains(lower_new, "==")) &&
        !contains(lower_old, "!=") && !contains(lower_old, "==")) {
        return true;
    }

    // Check for null/undefined checks
    if (contains(lower_new, "null") || contains(lower_new, "undefined")) {
        if (!contains(lower_old, "null") && !contains(lower_old, "undefined")) {
            return true;
        }
    }

    return false;
}

bool SemanticDiffAnalyzer::isRefactoring(const std::string& oldCode,
                                        const std::string& newCode) const {
    // Refactoring: structure changes without behavior change
    // Often: variable renames, reordering, extraction

    float similarity = calculateSimilarity(oldCode, newCode);
    
    // High similarity but different structure suggests refactoring
    if (similarity > 0.6f && oldCode.length() != newCode.length()) {
        // Check if it's just rearrangement
        auto tokensOld = tokenize(oldCode);
        auto tokensNew = tokenize(newCode);
        
        if (tokensOld.size() == tokensNew.size()) {
            return true;  // Same tokens, different arrangement
        }
    }

    return false;
}

bool SemanticDiffAnalyzer::isPerformanceImprovement(const std::string& oldCode,
                                                    const std::string& newCode) const {
    // Performance improvements often involve:
    // - Loop optimizations
    // - Cache usage
    // - Algorithm changes

    std::string lower_new = newCode;
    std::transform(lower_new.begin(), lower_new.end(), lower_new.begin(), ::tolower);

    return contains(lower_new, "cache") ||
           contains(lower_new, "memoize") ||
           contains(lower_new, "optimize") ||
           contains(lower_new, "fast") ||
           (contains(lower_new, "for") && newCode.length() < oldCode.length());
}

bool SemanticDiffAnalyzer::detectAPIChange(const std::string& oldCode,
                                          const std::string& newCode) const {
    // Detect changes to public interfaces
    auto oldSigs = extractFunctionSignatures(oldCode, "");
    auto newSigs = extractFunctionSignatures(newCode, "");

    // If signatures changed, likely an API change
    if (oldSigs.size() != newSigs.size()) {
        return true;
    }

    // Check for parameter changes
    for (size_t i = 0; i < oldSigs.size() && i < newSigs.size(); ++i) {
        if (oldSigs[i] != newSigs[i]) {
            return true;
        }
    }

    return false;
}

// ============================================================================
// Main Analysis Functions
// ============================================================================

ChangeCategory SemanticDiffAnalyzer::classifyChange(const std::string& oldContent,
                                                    const std::string& newContent,
                                                    const std::string& filePath) {
    
    // Check cache first
    std::string cacheKey = oldContent + "|" + newContent;
    if (m_categoryCache.find(cacheKey) != m_categoryCache.end()) {
        return m_categoryCache[cacheKey];
    }

    ChangeCategory category = ChangeCategory::LOGIC_CHANGE;

    // Test file changes
    if (isTestFile(filePath)) {
        category = ChangeCategory::TEST;
        m_categoryCache[cacheKey] = category;
        return category;
    }

    // Purely formatting changes
    std::string oldNorm = oldContent;
    std::string newNorm = newContent;
    // Remove all whitespace for comparison
    oldNorm.erase(std::remove_if(oldNorm.begin(), oldNorm.end(), ::isspace),
                  oldNorm.end());
    newNorm.erase(std::remove_if(newNorm.begin(), newNorm.end(), ::isspace),
                  newNorm.end());
    if (oldNorm == newNorm) {
        category = ChangeCategory::FORMATTING;
        m_categoryCache[cacheKey] = category;
        return category;
    }

    // Comment changes
    if (startsWith(trim(oldContent), "//") || startsWith(trim(oldContent), "#")) {
        if (startsWith(trim(newContent), "//") || startsWith(trim(newContent), "#")) {
            category = ChangeCategory::COMMENT;
            m_categoryCache[cacheKey] = category;
            return category;
        }
    }

    // Documentation
    if (endsWith(filePath, ".md") || endsWith(filePath, ".txt") ||
        endsWith(filePath, ".rst") || endsWith(filePath, ".doc")) {
        category = ChangeCategory::DOCUMENTATION;
        m_categoryCache[cacheKey] = category;
        return category;
    }

    // Security
    if (hasSecurityKeywords(oldContent) || hasSecurityKeywords(newContent)) {
        category = ChangeCategory::SECURITY;
        m_categoryCache[cacheKey] = category;
        return category;
    }

    // Pattern detection
    if (isBugFix(oldContent, newContent)) {
        category = ChangeCategory::BUG_FIX;
        m_categoryCache[cacheKey] = category;
        return category;
    }

    if (isPerformanceImprovement(oldContent, newContent)) {
        category = ChangeCategory::PERFORMANCE;
        m_categoryCache[cacheKey] = category;
        return category;
    }

    if (isRefactoring(oldContent, newContent)) {
        category = ChangeCategory::REFACTOR;
        m_categoryCache[cacheKey] = category;
        return category;
    }

    if (detectAPIChange(oldContent, newContent)) {
        category = ChangeCategory::BREAKING_CHANGE;
        m_categoryCache[cacheKey] = category;
        return category;
    }

    m_categoryCache[cacheKey] = category;
    return category;
}

bool SemanticDiffAnalyzer::hasSecurityImplications(const std::string& oldContent,
                                                   const std::string& newContent) const {
    return hasSecurityKeywords(oldContent) || hasSecurityKeywords(newContent);
}

float SemanticDiffAnalyzer::assessRisk(const SemanticChange& change) const {
    float risk = 0.0f;

    switch (change.category) {
        case ChangeCategory::FORMATTING:
        case ChangeCategory::COMMENT:
        case ChangeCategory::DOCUMENTATION:
            risk = 0.0f;
            break;
        case ChangeCategory::VARIABLE_RENAME:
            risk = 0.2f;
            break;
        case ChangeCategory::BUG_FIX:
        case ChangeCategory::REFACTOR:
            risk = 0.3f;
            break;
        case ChangeCategory::NEW_FEATURE:
        case ChangeCategory::PERFORMANCE:
            risk = 0.4f;
            break;
        case ChangeCategory::LOGIC_CHANGE:
            risk = 0.6f;
            break;
        case ChangeCategory::SECURITY:
            risk = 0.7f;
            break;
        case ChangeCategory::BREAKING_CHANGE:
            risk = 0.9f;
            break;
        case ChangeCategory::TEST:
            risk = 0.2f;
            break;
    }

    // Adjust based on function scope
    if (change.functionName.empty()) {
        risk *= 1.1f;  // Unknown scope is riskier
    }

    return std::min(1.0f, risk);
}

bool SemanticDiffAnalyzer::isBreakingChange(const SemanticChange& change) const {
    return change.category == ChangeCategory::BREAKING_CHANGE ||
           change.isBreakingChange;
}

bool SemanticDiffAnalyzer::affectsTests(const SemanticChange& change) const {
    return change.category == ChangeCategory::TEST ||
           change.riskScore > 0.7f;
}

SemanticChange SemanticDiffAnalyzer::analyzeHunk(const std::string& hunkContent,
                                                const std::string& filePath,
                                                int startLine) {
    SemanticChange change;
    change.filePath = filePath;
    change.lineStart = startLine;
    change.lineEnd = startLine + 5;  // Rough estimate

    // Extract sections
    auto lines = split(hunkContent, '\n');
    std::string oldCode, newCode;

    for (const auto& line : lines) {
        if (startsWith(line, "-")) {
            oldCode += line.substr(1) + "\n";
        } else if (startsWith(line, "+")) {
            newCode += line.substr(1) + "\n";
        }
    }

    change.category = classifyChange(oldCode, newCode, filePath);
    change.functionName = findFunctionAtLine(oldCode + newCode, startLine, filePath);
    change.riskScore = assessRisk(change);
    change.isBreakingChange = isBreakingChange(change);

    return change;
}

std::vector<SemanticChange> SemanticDiffAnalyzer::analyzeDiff(
    const std::string& diffContent,
    const std::string& filePath) {
    
    std::vector<SemanticChange> changes;
    std::istringstream iss(diffContent);
    std::string line;
    std::string currentFile = filePath;
    int lineNum = 0;

    while (std::getline(iss, line)) {
        lineNum++;

        if (startsWith(line, "diff --git")) {
            // Extract filename
            size_t aPos = line.find(" a/");
            if (aPos != std::string::npos) {
                size_t bPos = line.find(" b/", aPos);
                if (bPos != std::string::npos) {
                    currentFile = line.substr(aPos + 3, bPos - aPos - 3);
                }
            }
        }
        else if (startsWith(line, "@@")) {
            // Hunk header - analyze this hunk
            // collect following lines
            std::string hunk = line + "\n";
            while (std::getline(iss, line)) {
                if (startsWith(line, "@@")) {
                    // Next hunk
                    break;
                }
                hunk += line + "\n";
            }

            auto change = analyzeHunk(hunk, currentFile, lineNum);
            changes.push_back(change);
        }
    }

    return changes;
}

DiffStats SemanticDiffAnalyzer::analyzeDiffStats(const std::string& diffContent) const {
    DiffStats stats;
    stats.totalLines = 0;
    stats.addedLines = 0;
    stats.removedLines = 0;
    stats.modifiedLines = 0;
    stats.fileCount = 0;

    std::istringstream iss(diffContent);
    std::string line;
    std::string currentFile;
    bool inDiff = false;

    while (std::getline(iss, line)) {
        if (startsWith(line, "diff --git")) {
            inDiff = true;
            currentFile = "";
            size_t aPos = line.find(" a/");
            if (aPos != std::string::npos) {
                size_t bPos = line.find(" b/", aPos);
                if (bPos != std::string::npos) {
                    currentFile = line.substr(aPos + 3, bPos - aPos - 3);
                    stats.fileCount++;
                }
            }
        }
        else if (inDiff) {
            if (startsWith(line, "+++")) {
                stats.newFiles.push_back(currentFile);
            }
            else if (startsWith(line, "---")) {
                // Check if it's /dev/null for new file
            }
            else if (startsWith(line, "+")) {
                stats.addedLines++;
            }
            else if (startsWith(line, "-")) {
                stats.removedLines++;
            }
        }
    }

    stats.totalLines = stats.addedLines + stats.removedLines;
    stats.modifiedLines = std::min(stats.addedLines, stats.removedLines);

    return stats;
}

} // namespace Git
} // namespace RawrXD
