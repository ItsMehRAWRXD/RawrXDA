<<<<<<< HEAD
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
=======
#include "semantic_diff_analyzer.hpp"


#include <algorithm>
#include <vector>

// Production-ready Myers diff algorithm implementation
namespace DiffAlgorithm {

struct DiffEdit {
    enum Type { KEEP, INSERT, DELETE };
    Type type;
    int oldIndex;
    int newIndex;
    std::string line;
};

// Myers diff algorithm for computing minimal edit script
std::vector<DiffEdit> computeMyersDiff(const std::vector<std::string>& oldLines, const std::vector<std::string>& newLines) {
    const int N = oldLines.size();
    const int M = newLines.size();
    const int MAX = N + M;
    
    std::vector<int> V(2 * MAX + 1, 0);
    std::vector<std::vector<int>> traces;
    
    // Forward search
    for (int D = 0; D <= MAX; ++D) {
        traces.push_back(V);
        
        for (int k = -D; k <= D; k += 2) {
            int x;
            if (k == -D || (k != D && V[MAX + k - 1] < V[MAX + k + 1])) {
                x = V[MAX + k + 1];
            } else {
                x = V[MAX + k - 1] + 1;
            }
            
            int y = x - k;
            
            // Follow diagonal
            while (x < N && y < M && oldLines[x] == newLines[y]) {
                ++x;
                ++y;
            }
            
            V[MAX + k] = x;
            
            if (x >= N && y >= M) {
                // Found solution
                std::vector<DiffEdit> edits;
                
                // Backtrack to build edit script
                int currentX = N;
                int currentY = M;
                
                for (int d = D; d > 0; --d) {
                    const auto& prevV = traces[d - 1];
                    int currentK = currentX - currentY;
                    
                    int prevK;
                    if (currentK == -d || (currentK != d && prevV[MAX + currentK - 1] < prevV[MAX + currentK + 1])) {
                        prevK = currentK + 1;
                    } else {
                        prevK = currentK - 1;
                    }
                    
                    int prevX = prevV[MAX + prevK];
                    int prevY = prevX - prevK;
                    
                    // Add diagonal moves (equals)
                    while (currentX > prevX && currentY > prevY) {
                        --currentX;
                        --currentY;
                        edits.push_back({DiffEdit::KEEP, currentX, currentY, oldLines[currentX]});
                    }
                    
                    // Add edit
                    if (currentX > prevX) {
                        --currentX;
                        edits.push_back({DiffEdit::DELETE, currentX, -1, oldLines[currentX]});
                    } else if (currentY > prevY) {
                        --currentY;
                        edits.push_back({DiffEdit::INSERT, -1, currentY, newLines[currentY]});
                    }
                }
                
                std::reverse(edits.begin(), edits.end());
                return edits;
            }
        }
    }
    
    return {};
}

std::string generateUnifiedDiff(const std::string& filePath, const std::vector<std::string>& oldLines, 
                           const std::vector<std::string>& newLines, int contextLines = 3) {
    auto edits = computeMyersDiff(oldLines, newLines);
    
    std::string result = std::string("--- a/%1\n+++ b/%1\n");
    
    // Group edits into hunks
    std::vector<std::pair<int, int>> hunks; // (start, end) indices in edits
    int hunkStart = 0;
    int lastEditIndex = -contextLines - 1;
    
    for (size_t i = 0; i < edits.size(); ++i) {
        if (edits[i].type != DiffEdit::KEEP) {
            if (i - lastEditIndex > 2 * contextLines) {
                if (lastEditIndex >= 0) {
                    hunks.push_back({hunkStart, lastEditIndex + contextLines});
                }
                hunkStart = std::max(0, static_cast<int>(i) - contextLines);
            }
            lastEditIndex = i;
        }
    }
    
    if (lastEditIndex >= 0) {
        hunks.push_back({hunkStart, std::min(static_cast<int>(edits.size()) - 1, 
                                              lastEditIndex + contextLines)});
    }
    
    // Generate hunk output
    for (const auto& hunk : hunks) {
        int oldStart = -1, newStart = -1;
        int oldCount = 0, newCount = 0;
        
        for (int i = hunk.first; i <= hunk.second; ++i) {
            if (edits[i].type == DiffEdit::DELETE || edits[i].type == DiffEdit::KEEP) {
                if (oldStart < 0) oldStart = edits[i].oldIndex;
                ++oldCount;
            }
            if (edits[i].type == DiffEdit::INSERT || edits[i].type == DiffEdit::KEEP) {
                if (newStart < 0) newStart = edits[i].newIndex;
                ++newCount;
            }
        }
        
        result += std::string("@@ -%1,%2 +%3,%4 @@\n")
                    
                    ;
        
        for (int i = hunk.first; i <= hunk.second; ++i) {
            switch (edits[i].type) {
                case DiffEdit::KEEP:
                    result += " " + edits[i].line + "\n";
                    break;
                case DiffEdit::DELETE:
                    result += "-" + edits[i].line + "\n";
                    break;
                case DiffEdit::INSERT:
                    result += "+" + edits[i].line + "\n";
                    break;
            }
        }
    }
    
    return result;
}

} // namespace DiffAlgorithm

SemanticDiffAnalyzer::SemanticDiffAnalyzer(void* parent)
    : void(parent)
{
    logStructured("INFO", "SemanticDiffAnalyzer initializing", void*{{"component", "SemanticDiffAnalyzer"}});
    logStructured("INFO", "SemanticDiffAnalyzer initialized successfully", void*{{"component", "SemanticDiffAnalyzer"}});
}

SemanticDiffAnalyzer::~SemanticDiffAnalyzer()
{
    logStructured("INFO", "SemanticDiffAnalyzer shutting down", void*{{"component", "SemanticDiffAnalyzer"}});
}

void SemanticDiffAnalyzer::setConfig(const Config& config)
{
    std::lock_guard<std::mutex> locker(&m_configMutex);
    m_config = config;
    logStructured("INFO", "Configuration updated", void*{
        {"enableBreakingChangeDetection", config.enableBreakingChangeDetection},
        {"enableImpactAnalysis", config.enableImpactAnalysis},
        {"maxDiffSize", config.maxDiffSize},
        {"enableCaching", config.enableCaching}
    });
}

SemanticDiffAnalyzer::Config SemanticDiffAnalyzer::getConfig() const
{
    std::lock_guard<std::mutex> locker(&m_configMutex);
    return m_config;
}

SemanticDiffAnalyzer::DiffAnalysis SemanticDiffAnalyzer::analyzeDiff(const std::string& diff)
{
    auto startTime = std::chrono::steady_clock::now();
    DiffAnalysis analysis;
    
    try {
        if (!validateDiff(diff)) {
            logStructured("ERROR", "Invalid diff provided", void*{{"diffLength", diff.length()}});
            std::lock_guard<std::mutex> metricsLocker(&m_metricsMutex);
            m_metrics.errorCount++;
            errorOccurred("Invalid diff data");
            return analysis;
        }
        
        Config config;
        {
            std::lock_guard<std::mutex> configLocker(&m_configMutex);
            config = m_config;
        }
        
        // Check cache
        std::string diffHash = calculateDiffHash(diff);
        if (config.enableCaching) {
            DiffAnalysis cached = getCachedAnalysis(diffHash);
            if (!cached.summary.empty()) {
                std::lock_guard<std::mutex> metricsLocker(&m_metricsMutex);
                m_metrics.cacheHits++;
                logStructured("INFO", "Cache hit for diff analysis", void*{{"diffHash", diffHash}});
                return cached;
            }
            std::lock_guard<std::mutex> metricsLocker(&m_metricsMutex);
            m_metrics.cacheMisses++;
        }
        
        // Prepare AI request
        void* payload;
        payload["diff"] = diff;
        payload["enable_breaking_change_detection"] = config.enableBreakingChangeDetection;
        payload["enable_impact_analysis"] = config.enableImpactAnalysis;
        
        logStructured("DEBUG", "Sending diff analysis request to AI", void*{
            {"diffSize", diff.length()},
            {"diffHash", diffHash}
        });
        
        void* response = makeAiRequest(config.aiEndpoint + "/analyze-diff", payload);
        
        // Parse response
        analysis.summary = response["summary"].toString();
        analysis.breakingChangeCount = response["breaking_change_count"].toInt();
        analysis.overallImpactScore = response["overall_impact_score"].toDouble();
        analysis.metadata = response["metadata"].toObject();
        
        void* changesArray = response["changes"].toArray();
        for (const void*& value : changesArray) {
            void* changeObj = value.toObject();
            SemanticChange change;
            change.type = changeObj["type"].toString();
            change.name = changeObj["name"].toString();
            change.description = changeObj["description"].toString();
            change.file = changeObj["file"].toString();
            change.startLine = changeObj["start_line"].toInt();
            change.endLine = changeObj["end_line"].toInt();
            change.isBreaking = changeObj["is_breaking"].toBool();
            change.impactScore = changeObj["impact_score"].toDouble();
            
            void* affectedArray = changeObj["affected_files"].toArray();
            for (const void*& af : affectedArray) {
                change.affectedFiles.append(af.toString());
            }
            
            analysis.changes.append(change);
            
            // signals for breaking/high-impact changes
            if (change.isBreaking) {
                breakingChangeDetected(change);
            }
            if (change.impactScore >= 0.7) {
                highImpactChangeDetected(change);
            }
        }
        
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        recordLatency("diff_analysis", duration);
        
        {
            std::lock_guard<std::mutex> metricsLocker(&m_metricsMutex);
            m_metrics.diffsAnalyzed++;
            m_metrics.semanticChangesDetected += analysis.changes.size();
            m_metrics.breakingChangesDetected += analysis.breakingChangeCount;
            
            // Update average impact score
            if (analysis.changes.size() > 0) {
                m_metrics.avgImpactScore = 
                    (m_metrics.avgImpactScore * (m_metrics.semanticChangesDetected - analysis.changes.size()) 
                     + analysis.overallImpactScore * analysis.changes.size()) / m_metrics.semanticChangesDetected;
            }
        }
        
        logStructured("INFO", "Diff analysis completed", void*{
            {"changesDetected", analysis.changes.size()},
            {"breakingChanges", analysis.breakingChangeCount},
            {"overallImpactScore", analysis.overallImpactScore},
            {"latencyMs", duration.count()}
        });
        
        // Cache the result
        if (config.enableCaching) {
            cacheAnalysis(diffHash, analysis);
        }
        
        analysisCompleted(analysis);
        
    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> metricsLocker(&m_metricsMutex);
        m_metrics.errorCount++;
        logStructured("ERROR", "Diff analysis failed", void*{{"error", e.what()}});
        errorOccurred(std::string("Analysis failed: %1")));
    }
    
    return analysis;
}

SemanticDiffAnalyzer::DiffAnalysis SemanticDiffAnalyzer::compareFiles(const std::string& oldContent, const std::string& newContent, const std::string& filePath)
{
    auto startTime = std::chrono::steady_clock::now();
    
    try {
        Config config;
        {
            std::lock_guard<std::mutex> configLocker(&m_configMutex);
            config = m_config;
        }
        
        // Generate unified diff using production-ready Myers diff algorithm
        std::vector<std::string> oldLines = oldContent.split('\n');
        std::vector<std::string> newLines = newContent.split('\n');
        
        std::string diffContent = DiffAlgorithm::generateUnifiedDiff(filePath, oldLines, newLines, 3);
        
        logStructured("DEBUG", "Generated diff for file comparison", void*{
            {"filePath", filePath},
            {"oldContentSize", oldContent.length()},
            {"newContentSize", newContent.length()}
        });
        
        return analyzeDiff(diffContent);
        
    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> metricsLocker(&m_metricsMutex);
        m_metrics.errorCount++;
        logStructured("ERROR", "File comparison failed", void*{{"error", e.what()}});
        errorOccurred(std::string("File comparison failed: %1")));
        return DiffAnalysis();
    }
}

std::vector<std::string> SemanticDiffAnalyzer::detectBreakingChanges(const DiffAnalysis& analysis)
{
    std::vector<std::string> breakingChanges;
    
    for (const SemanticChange& change : analysis.changes) {
        if (change.isBreaking) {
            std::string description = std::string("%1: %2 in %3")


                ;
            breakingChanges.append(description);
        }
    }
    
    logStructured("INFO", "Breaking changes extracted", void*{
        {"count", breakingChanges.size()}
    });
    
    return breakingChanges;
}

void* SemanticDiffAnalyzer::analyzeImpact(const SemanticChange& change)
{
    auto startTime = std::chrono::steady_clock::now();
    void* impactAnalysis;
    
    try {
        Config config;
        {
            std::lock_guard<std::mutex> configLocker(&m_configMutex);
            config = m_config;
        }
        
        if (!config.enableImpactAnalysis) {
            logStructured("DEBUG", "Impact analysis disabled", void*{});
            return impactAnalysis;
        }
        
        void* payload;
        payload["change_type"] = change.type;
        payload["change_name"] = change.name;
        payload["change_description"] = change.description;
        payload["file"] = change.file;
        payload["is_breaking"] = change.isBreaking;
        
        logStructured("DEBUG", "Requesting impact analysis", void*{
            {"changeType", change.type},
            {"changeName", change.name}
        });
        
        impactAnalysis = makeAiRequest(config.aiEndpoint + "/analyze-impact", payload);
        
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        
        {
            std::lock_guard<std::mutex> metricsLocker(&m_metricsMutex);
            m_metrics.impactAnalysesPerformed++;
        }
        
        logStructured("INFO", "Impact analysis completed", void*{
            {"impactScore", impactAnalysis["impact_score"].toDouble()},
            {"affectedComponents", impactAnalysis["affected_components"].toArray().size()},
            {"latencyMs", duration.count()}
        });
        
    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> metricsLocker(&m_metricsMutex);
        m_metrics.errorCount++;
        logStructured("ERROR", "Impact analysis failed", void*{{"error", e.what()}});
        errorOccurred(std::string("Impact analysis failed: %1")));
    }
    
    return impactAnalysis;
}

std::string SemanticDiffAnalyzer::enrichDiffContext(const std::string& diff)
{
    auto startTime = std::chrono::steady_clock::now();
    std::string enriched;
    
    try {
        Config config;
        {
            std::lock_guard<std::mutex> configLocker(&m_configMutex);
            config = m_config;
        }
        
        void* payload;
        payload["diff"] = diff;
        payload["enrichment_type"] = "context";
        
        logStructured("DEBUG", "Requesting diff context enrichment", void*{
            {"diffSize", diff.length()}
        });
        
        void* response = makeAiRequest(config.aiEndpoint + "/enrich-diff", payload);
        
        enriched = response["enriched_diff"].toString();
        
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        
        logStructured("INFO", "Diff enrichment completed", void*{
            {"originalSize", diff.length()},
            {"enrichedSize", enriched.length()},
            {"latencyMs", duration.count()}
        });
        
    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> metricsLocker(&m_metricsMutex);
        m_metrics.errorCount++;
        logStructured("ERROR", "Diff enrichment failed", void*{{"error", e.what()}});
        errorOccurred(std::string("Enrichment failed: %1")));
        enriched = diff; // Return original on error
    }
    
    return enriched;
}

SemanticDiffAnalyzer::Metrics SemanticDiffAnalyzer::getMetrics() const
{
    std::lock_guard<std::mutex> locker(&m_metricsMutex);
    return m_metrics;
}

void SemanticDiffAnalyzer::resetMetrics()
{
    std::lock_guard<std::mutex> locker(&m_metricsMutex);
    m_metrics = Metrics();
    logStructured("INFO", "Metrics reset", void*{});
}

void SemanticDiffAnalyzer::clearCache()
{
    std::lock_guard<std::mutex> locker(&m_cacheMutex);
    m_analysisCache.clear();
    logStructured("INFO", "Analysis cache cleared", void*{});
}

void SemanticDiffAnalyzer::logStructured(const std::string& level, const std::string& message, const void*& context)
{
    void* logEntry;
    logEntry["timestamp"] = std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate);
    logEntry["level"] = level;
    logEntry["component"] = "SemanticDiffAnalyzer";
    logEntry["message"] = message;
    logEntry["context"] = context;
    
    void* doc(logEntry);
}

void SemanticDiffAnalyzer::recordLatency(const std::string& operation, const std::chrono::milliseconds& duration)
{
    std::lock_guard<std::mutex> locker(&m_metricsMutex);
    
    if (operation == "diff_analysis") {
        m_metrics.avgAnalysisLatencyMs = 
            (m_metrics.avgAnalysisLatencyMs * (m_metrics.diffsAnalyzed - 1) + duration.count()) 
            / m_metrics.diffsAnalyzed;
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

void* SemanticDiffAnalyzer::makeAiRequest(const std::string& endpoint, const void*& payload)
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

std::string SemanticDiffAnalyzer::calculateDiffHash(const std::string& diff)
{
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(diff.toUtf8());
    return std::string(hash.result().toHex());
}

SemanticDiffAnalyzer::DiffAnalysis SemanticDiffAnalyzer::getCachedAnalysis(const std::string& diffHash)
{
    std::lock_guard<std::mutex> locker(&m_cacheMutex);
    return m_analysisCache.value(diffHash, DiffAnalysis());
}

void SemanticDiffAnalyzer::cacheAnalysis(const std::string& diffHash, const DiffAnalysis& analysis)
{
    std::lock_guard<std::mutex> locker(&m_cacheMutex);
    m_analysisCache[diffHash] = analysis;
    
    Config config;
    {
        std::lock_guard<std::mutex> configLocker(&m_configMutex);
        config = m_config;
    }
    
    // Persist to disk if cache directory configured
    if (!config.cacheDirectory.empty()) {
        std::filesystem::path cacheDir(config.cacheDirectory);
        if (!cacheDir.exists()) {
            cacheDir.mkpath(".");
        }
        
        std::fstream cacheFile(cacheDir.filePath(diffHash + ".json"));
        if (cacheFile.open(QIODevice::WriteOnly)) {
            void* cacheObj;
            cacheObj["summary"] = analysis.summary;
            cacheObj["breaking_change_count"] = analysis.breakingChangeCount;
            cacheObj["overall_impact_score"] = analysis.overallImpactScore;
            cacheObj["metadata"] = analysis.metadata;
            
            void* changesArray;
            for (const SemanticChange& change : analysis.changes) {
                void* changeObj;
                changeObj["type"] = change.type;
                changeObj["name"] = change.name;
                changeObj["description"] = change.description;
                changeObj["file"] = change.file;
                changeObj["start_line"] = change.startLine;
                changeObj["end_line"] = change.endLine;
                changeObj["is_breaking"] = change.isBreaking;
                changeObj["impact_score"] = change.impactScore;
                changesArray.append(changeObj);
            }
            cacheObj["changes"] = changesArray;
            
            void* doc(cacheObj);
            cacheFile.write(doc.toJson());
            cacheFile.close();
        }
    }
}

bool SemanticDiffAnalyzer::validateDiff(const std::string& diff)
{
    if (diff.empty()) {
        return false;
    }
    
    Config config;
    {
        std::lock_guard<std::mutex> configLocker(&m_configMutex);
        config = m_config;
    }
    
    if (diff.length() > config.maxDiffSize) {
        logStructured("WARN", "Diff exceeds maximum size", void*{
            {"diffSize", diff.length()},
            {"maxSize", config.maxDiffSize}
        });
        return false;
    }
    
    return true;
}


>>>>>>> origin/main
