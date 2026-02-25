/**
 * @file agent_task_parser.hpp
 * @brief Natural language task parser for autonomous agent operations
 *
 * Converts user requests into executable tool chains with:
 * - Intent classification
 * - Parameter extraction
 * - Tool chain construction
 * - Thermal-aware scheduling
 */

#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <memory>
#include <regex>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace RawrXD {

// ============================================================================
// Intent Types
// ============================================================================

enum class IntentType {
    // File Operations
    FILE_READ,
    FILE_WRITE,
    FILE_EDIT,
    FILE_DELETE,
    FILE_SEARCH,
    FILE_CREATE,
    
    // Build Operations
    BUILD_PROJECT,
    BUILD_TARGET,
    RUN_TESTS,
    DEBUG_START,
    
    // Git Operations
    GIT_STATUS,
    GIT_COMMIT,
    GIT_PUSH,
    GIT_PULL,
    GIT_BRANCH,
    GIT_DIFF,
    
    // Shell Operations
    SHELL_EXEC,
    PROCESS_LIST,
    PROCESS_KILL,
    
    // Analysis Operations
    CODE_ANALYZE,
    FIND_REFERENCES,
    FIND_DEFINITION,
    CODE_COMPLETE,
    
    // Thermal Operations
    THERMAL_STATUS,
    THERMAL_SELECT_DRIVE,
    THERMAL_CHECK_HEADROOM,
    
    // Compound Operations
    IMPLEMENT_FEATURE,
    FIX_BUG,
    REFACTOR_CODE,
    ADD_TESTS,
    
    // Meta Operations
    EXPLAIN_CODE,
    ANSWER_QUESTION,
    
    UNKNOWN
};

// ============================================================================
// Parsed Task Structure
// ============================================================================

struct ParsedTask {
    IntentType intent = IntentType::UNKNOWN;
    std::string rawQuery;
    float confidence = 0.0f;
    
    // Extracted parameters
    std::map<std::string, std::string> params;
    std::vector<std::string> filePaths;
    std::vector<std::string> symbols;
    
    // Tool chain to execute
    std::vector<std::string> toolChain;
    std::vector<json> toolParams;
    
    // Scheduling hints
    bool requiresThermalCheck = false;
    bool isHeavyIO = false;
    int estimatedTimeMs = 5000;
};

// ============================================================================
// Intent Pattern
// ============================================================================

struct IntentPattern {
    IntentType intent;
    std::vector<std::regex> patterns;
    std::vector<std::string> keywords;
    std::vector<std::string> requiredTools;
    bool heavyIO = false;
};

// ============================================================================
// Agent Task Parser
// ============================================================================

class AgentTaskParser {
public:
    AgentTaskParser();
    
    /**
     * @brief Parse a natural language request into a task
     * @param query The user's natural language request
     * @return ParsedTask with intent, parameters, and tool chain
     */
    ParsedTask parse(const std::string& query);
    
    /**
     * @brief Check if a query matches a specific intent
     * @param query The query to check
     * @param intent The intent to match against
     * @return Confidence score 0.0-1.0
     */
    float matchIntent(const std::string& query, IntentType intent);
    
    /**
     * @brief Extract file paths from a query
     * @param query The query to extract from
     * @return Vector of file paths found
     */
    std::vector<std::string> extractFilePaths(const std::string& query);
    
    /**
     * @brief Extract symbol names from a query
     * @param query The query to extract from
     * @return Vector of symbol names found
     */
    std::vector<std::string> extractSymbols(const std::string& query);
    
    /**
     * @brief Build tool chain for an intent
     * @param task The task with intent set
     * @return Vector of tool names to execute
     */
    std::vector<std::string> buildToolChain(const ParsedTask& task);
    
    /**
     * @brief Get intent name as string
     */
    static std::string intentToString(IntentType intent);
    
private:
    void initializePatterns();
    IntentType classifyIntent(const std::string& query, float& confidence);
    void extractParameters(const std::string& query, ParsedTask& task);
    
    std::vector<IntentPattern> m_patterns;
    
    // Regex patterns for extraction
    std::regex m_filePathRegex;
    std::regex m_symbolRegex;
    std::regex m_lineNumberRegex;
};

// ============================================================================
// Implementation
// ============================================================================

inline AgentTaskParser::AgentTaskParser() {
    initializePatterns();
    
    // Initialize extraction regexes
    // File paths: C:\..., D:\..., /path/to/file, ./relative, src/file.cpp
    m_filePathRegex = std::regex(R"((?:[A-Za-z]:\\|/|\.\.?/)[\w\-./\\]+\.\w+)", std::regex::icase);
    
    // Symbols: function names, class names, variable names
    m_symbolRegex = std::regex(R"(\b([A-Z][a-zA-Z0-9_]*|[a-z][a-zA-Z0-9_]*(?:_[a-zA-Z0-9_]+)+)\b)");
    
    // Line numbers: line 42, lines 10-20, L42
    m_lineNumberRegex = std::regex(R"(\b(?:line[s]?\s*)?(\d+)(?:\s*-\s*(\d+))?\b)", std::regex::icase);
}

inline void AgentTaskParser::initializePatterns() {
    m_patterns = {
        // File operations
        {IntentType::FILE_READ, 
         {std::regex(R"(read|open|show|display|view|cat|get\s+contents?)", std::regex::icase)},
         {"read", "open", "show", "view", "contents", "file"},
         {"readFile"}, false},
        
        {IntentType::FILE_WRITE,
         {std::regex(R"(write|save|create\s+file|output\s+to)", std::regex::icase)},
         {"write", "save", "create", "output"},
         {"writeFile"}, false},
        
        {IntentType::FILE_EDIT,
         {std::regex(R"(edit|modify|change|update|replace|fix)", std::regex::icase)},
         {"edit", "modify", "change", "update", "replace"},
         {"readFile", "writeFile"}, false},
        
        {IntentType::FILE_SEARCH,
         {std::regex(R"(search|find|grep|look\s+for|locate)", std::regex::icase)},
         {"search", "find", "grep", "locate"},
         {"grepSearch"}, true},
        
        // Build operations
        {IntentType::BUILD_PROJECT,
         {std::regex(R"(build|compile|make|cmake)", std::regex::icase)},
         {"build", "compile", "make"},
         {"checkThermalHeadroom", "shellExecute"}, true},
        
        {IntentType::RUN_TESTS,
         {std::regex(R"(test|run\s+tests?|verify|check)", std::regex::icase)},
         {"test", "tests", "verify"},
         {"checkThermalHeadroom", "runTests"}, true},
        
        // Git operations
        {IntentType::GIT_STATUS,
         {std::regex(R"(git\s+status|status|changes|modified)", std::regex::icase)},
         {"status", "changes", "modified"},
         {"gitStatus"}, false},
        
        {IntentType::GIT_COMMIT,
         {std::regex(R"(commit|save\s+changes|check\s*in)", std::regex::icase)},
         {"commit", "checkin"},
         {"gitStatus", "gitCommit"}, false},
        
        {IntentType::GIT_PUSH,
         {std::regex(R"(push|upload|sync\s+remote)", std::regex::icase)},
         {"push", "upload", "sync"},
         {"gitPush"}, false},
        
        // Thermal operations
        {IntentType::THERMAL_STATUS,
         {std::regex(R"(thermal|temperature|nvme\s+status|drive\s+temp)", std::regex::icase)},
         {"thermal", "temperature", "temp", "nvme"},
         {"thermalStatus"}, false},
        
        {IntentType::THERMAL_SELECT_DRIVE,
         {std::regex(R"(coolest\s+drive|select\s+drive|best\s+nvme)", std::regex::icase)},
         {"coolest", "best", "select", "drive"},
         {"selectCoolestDrive"}, false},
        
        // Compound operations
        {IntentType::IMPLEMENT_FEATURE,
         {std::regex(R"(implement|add\s+feature|create\s+(?:function|class|method))", std::regex::icase)},
         {"implement", "add", "create", "feature"},
         {"checkThermalHeadroom", "readFile", "writeFile", "runBuild", "runTests"}, true},
        
        {IntentType::FIX_BUG,
         {std::regex(R"(fix|bug|error|issue|problem|broken)", std::regex::icase)},
         {"fix", "bug", "error", "broken"},
         {"checkThermalHeadroom", "grepSearch", "readFile", "writeFile", "runBuild", "runTests"}, true},
        
        {IntentType::REFACTOR_CODE,
         {std::regex(R"(refactor|clean\s*up|improve|reorganize|restructure)", std::regex::icase)},
         {"refactor", "cleanup", "improve", "reorganize"},
         {"checkThermalHeadroom", "readFile", "detectCodeSmells", "writeFile", "runTests"}, true},
        
        // Analysis
        {IntentType::CODE_ANALYZE,
         {std::regex(R"(analyze|inspect|review|check\s+code)", std::regex::icase)},
         {"analyze", "inspect", "review"},
         {"readFile", "detectCodeSmells"}, false},
        
        {IntentType::EXPLAIN_CODE,
         {std::regex(R"(explain|what\s+does|how\s+does|understand)", std::regex::icase)},
         {"explain", "what", "how", "understand"},
         {"readFile"}, false}
    };
}

inline ParsedTask AgentTaskParser::parse(const std::string& query) {
    ParsedTask task;
    task.rawQuery = query;
    
    // Classify intent
    task.intent = classifyIntent(query, task.confidence);
    
    // Extract parameters
    extractParameters(query, task);
    
    // Build tool chain
    task.toolChain = buildToolChain(task);
    
    // Set scheduling hints
    for (const auto& pattern : m_patterns) {
        if (pattern.intent == task.intent) {
            task.isHeavyIO = pattern.heavyIO;
            task.requiresThermalCheck = pattern.heavyIO;
            break;
        }
    }
    
    return task;
}

inline IntentType AgentTaskParser::classifyIntent(const std::string& query, float& confidence) {
    std::string lowerQuery = query;
    std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(), ::tolower);
    
    IntentType bestIntent = IntentType::UNKNOWN;
    float bestScore = 0.0f;
    
    for (const auto& pattern : m_patterns) {
        float score = 0.0f;
        
        // Check regex patterns
        for (const auto& regex : pattern.patterns) {
            if (std::regex_search(lowerQuery, regex)) {
                score += 0.5f;
                break;
            }
        }
        
        // Check keywords
        int keywordMatches = 0;
        for (const auto& keyword : pattern.keywords) {
            if (lowerQuery.find(keyword) != std::string::npos) {
                keywordMatches++;
            }
        }
        score += 0.1f * std::min(keywordMatches, 5);
        
        if (score > bestScore) {
            bestScore = score;
            bestIntent = pattern.intent;
        }
    }
    
    confidence = std::min(bestScore, 1.0f);
    return bestIntent;
}

inline void AgentTaskParser::extractParameters(const std::string& query, ParsedTask& task) {
    // Extract file paths
    task.filePaths = extractFilePaths(query);
    
    // Extract symbols
    task.symbols = extractSymbols(query);
    
    // Extract line numbers
    std::smatch lineMatch;
    if (std::regex_search(query, lineMatch, m_lineNumberRegex)) {
        task.params["startLine"] = lineMatch[1].str();
        if (lineMatch[2].matched) {
            task.params["endLine"] = lineMatch[2].str();
        }
    }
}

inline std::vector<std::string> AgentTaskParser::extractFilePaths(const std::string& query) {
    std::vector<std::string> paths;
    std::sregex_iterator it(query.begin(), query.end(), m_filePathRegex);
    std::sregex_iterator end;
    
    while (it != end) {
        paths.push_back(it->str());
        ++it;
    }
    
    return paths;
}

inline std::vector<std::string> AgentTaskParser::extractSymbols(const std::string& query) {
    std::vector<std::string> symbols;
    std::sregex_iterator it(query.begin(), query.end(), m_symbolRegex);
    std::sregex_iterator end;
    
    // Skip common words
    std::set<std::string> skipWords = {"the", "and", "for", "with", "from", "this", "that"};
    
    while (it != end) {
        std::string match = it->str();
        if (skipWords.find(match) == skipWords.end() && match.length() > 2) {
            symbols.push_back(match);
        }
        ++it;
    }
    
    return symbols;
}

inline std::vector<std::string> AgentTaskParser::buildToolChain(const ParsedTask& task) {
    std::vector<std::string> chain;
    
    for (const auto& pattern : m_patterns) {
        if (pattern.intent == task.intent) {
            chain = pattern.requiredTools;
            break;
        }
    }
    
    // If heavy I/O, prepend thermal check
    if (task.isHeavyIO && !chain.empty() && chain[0] != "checkThermalHeadroom") {
        chain.insert(chain.begin(), "checkThermalHeadroom");
    }
    
    return chain;
}

inline std::string AgentTaskParser::intentToString(IntentType intent) {
    static const std::map<IntentType, std::string> names = {
        {IntentType::FILE_READ, "FILE_READ"},
        {IntentType::FILE_WRITE, "FILE_WRITE"},
        {IntentType::FILE_EDIT, "FILE_EDIT"},
        {IntentType::FILE_DELETE, "FILE_DELETE"},
        {IntentType::FILE_SEARCH, "FILE_SEARCH"},
        {IntentType::FILE_CREATE, "FILE_CREATE"},
        {IntentType::BUILD_PROJECT, "BUILD_PROJECT"},
        {IntentType::BUILD_TARGET, "BUILD_TARGET"},
        {IntentType::RUN_TESTS, "RUN_TESTS"},
        {IntentType::DEBUG_START, "DEBUG_START"},
        {IntentType::GIT_STATUS, "GIT_STATUS"},
        {IntentType::GIT_COMMIT, "GIT_COMMIT"},
        {IntentType::GIT_PUSH, "GIT_PUSH"},
        {IntentType::GIT_PULL, "GIT_PULL"},
        {IntentType::GIT_BRANCH, "GIT_BRANCH"},
        {IntentType::GIT_DIFF, "GIT_DIFF"},
        {IntentType::SHELL_EXEC, "SHELL_EXEC"},
        {IntentType::PROCESS_LIST, "PROCESS_LIST"},
        {IntentType::PROCESS_KILL, "PROCESS_KILL"},
        {IntentType::CODE_ANALYZE, "CODE_ANALYZE"},
        {IntentType::FIND_REFERENCES, "FIND_REFERENCES"},
        {IntentType::FIND_DEFINITION, "FIND_DEFINITION"},
        {IntentType::CODE_COMPLETE, "CODE_COMPLETE"},
        {IntentType::THERMAL_STATUS, "THERMAL_STATUS"},
        {IntentType::THERMAL_SELECT_DRIVE, "THERMAL_SELECT_DRIVE"},
        {IntentType::THERMAL_CHECK_HEADROOM, "THERMAL_CHECK_HEADROOM"},
        {IntentType::IMPLEMENT_FEATURE, "IMPLEMENT_FEATURE"},
        {IntentType::FIX_BUG, "FIX_BUG"},
        {IntentType::REFACTOR_CODE, "REFACTOR_CODE"},
        {IntentType::ADD_TESTS, "ADD_TESTS"},
        {IntentType::EXPLAIN_CODE, "EXPLAIN_CODE"},
        {IntentType::ANSWER_QUESTION, "ANSWER_QUESTION"},
        {IntentType::UNKNOWN, "UNKNOWN"}
    };
    
    auto it = names.find(intent);
    return it != names.end() ? it->second : "UNKNOWN";
}

} // namespace RawrXD
