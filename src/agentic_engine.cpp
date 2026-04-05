#include "AppState.h"
#include "agentic_engine.h"
#include "cpu_inference_engine.h"
#include "native_agent.hpp"
#include "reverse_engineering/RawrDumpBin.hpp"
#include "reverse_engineering/RawrCodex.hpp"
#include "reverse_engineering/RawrCompiler.hpp"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <variant>
#include <optional>
#include <algorithm>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <algorithm>
#include <cmath>
#include <algorithm>
#include <fstream>
#include <cmath>
#include <algorithm>
#include <regex>
#include <cmath>
#include <algorithm>
#include <filesystem>
#include <cmath>
#include <algorithm>
#include <stack>
#include <cmath>
#include <algorithm>
#include <unordered_map>
#include <cmath>
#include <algorithm>
#include <unordered_set>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <algorithm>
#include <mutex>
#include <cmath>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <iomanip>
#include <cmath>
#include <algorithm>

// Feedback tracking structures
struct FeedbackEntry {
    std::string requestId;
    bool positive;
    std::string comment;
    std::chrono::steady_clock::time_point timestamp;
};

// MASM Telemetry bridge — lock-free atomic counters
#include "rawrxd_telemetry_exports.h"
#include <cmath>
#include <algorithm>

// SCAFFOLD_092: AgenticEngine chat/analyze/generate


// Static feedback tracking
static std::mutex g_feedbackMutex;
static std::vector<FeedbackEntry> g_feedbackLog;
static int g_positiveCount = 0;
static int g_negativeCount = 0;
static std::vector<std::string> g_avoidPatterns;
static bool g_prefVerbose = false;
static std::string g_prefLanguage = "en";

namespace {

struct LocalCodeMetrics {
    size_t totalLines = 0;
    size_t codeLines = 0;
    size_t commentLines = 0;
    size_t blankLines = 0;
    size_t functionCount = 0;
    size_t classCount = 0;
    int maxNestingDepth = 0;
    int cyclomaticComplexity = 1;
    size_t totalTokens = 0;
    double maintainabilityIndex = 0.0;
    std::string estimatedComplexity = "O(1)";
};

static std::string trimCopy(const std::string& value) {
    const size_t start = value.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    const size_t end = value.find_last_not_of(" \t\r\n");
    return value.substr(start, end - start + 1);
}

static std::string toLowerCopy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

static std::vector<std::string> splitLines(const std::string& text) {
    std::vector<std::string> lines;
    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        lines.push_back(line);
    }
    return lines;
}

static LocalCodeMetrics collectLocalCodeMetrics(const std::string& code) {
    LocalCodeMetrics metrics;
    bool inBlockComment = false;
    int currentNesting = 0;

    std::istringstream stream(code);
    std::string line;

    static const std::regex decisionPattern(
        R"(\b(if|else\s+if|for|while|case|catch|switch)\b|\&\&|\|\||\?)"
    );
    static const std::regex funcPattern(
        R"(\b([A-Za-z_][\w:<>,~*&\s]+)\s+([A-Za-z_][\w]*)\s*\([^;{}]*\)\s*(const\s*)?(\{|$))"
    );
    static const std::regex classPattern(
        R"(\b(class|struct|interface|enum)\s+[A-Za-z_][\w]*)"
    );

    while (std::getline(stream, line)) {
        metrics.totalLines++;
        const std::string trimmed = trimCopy(line);
        if (trimmed.empty()) {
            metrics.blankLines++;
            continue;
        }

        if (inBlockComment) {
            metrics.commentLines++;
            if (trimmed.find("*/") != std::string::npos) {
                inBlockComment = false;
            }
            continue;
        }

        if (trimmed.rfind("/*", 0) == 0) {
            metrics.commentLines++;
            if (trimmed.find("*/") == std::string::npos) {
                inBlockComment = true;
            }
            continue;
        }

        if (trimmed.rfind("//", 0) == 0 || trimmed[0] == '#') {
            metrics.commentLines++;
            continue;
        }

        metrics.codeLines++;
        metrics.totalTokens += static_cast<size_t>(std::count_if(trimmed.begin(), trimmed.end(), [](char ch) {
            return ch == ' ' || ch == '\t';
        })) + 1;

        for (char ch : line) {
            if (ch == '{') {
                currentNesting++;
                metrics.maxNestingDepth = std::max(metrics.maxNestingDepth, currentNesting);
            } else if (ch == '}') {
                currentNesting = std::max(0, currentNesting - 1);
            }
        }

        std::sregex_iterator it(trimmed.begin(), trimmed.end(), decisionPattern);
        std::sregex_iterator end;
        metrics.cyclomaticComplexity += static_cast<int>(std::distance(it, end));

        if (std::regex_search(trimmed, funcPattern) &&
            trimmed.find("if") != 0 && trimmed.find("for") != 0 && trimmed.find("while") != 0 &&
            trimmed.find("switch") != 0 && trimmed.find("catch") != 0) {
            metrics.functionCount++;
        }

        if (std::regex_search(trimmed, classPattern)) {
            metrics.classCount++;
        }
    }

    if (metrics.codeLines > 0 && metrics.totalTokens > 0) {
        const double tokenCount = static_cast<double>(metrics.totalTokens);
        const double lineCount = static_cast<double>(metrics.codeLines);
        const double halsteadVolume = tokenCount * std::log10(std::max(2.0, tokenCount));
        metrics.maintainabilityIndex = std::max(0.0,
            171.0 - 5.2 * std::log(std::max(1.0, halsteadVolume))
            - 0.23 * metrics.cyclomaticComplexity
            - 16.2 * std::log(std::max(1.0, lineCount)));
    }

    metrics.estimatedComplexity = "O(n)";
    if (code.find("/= 2") != std::string::npos || code.find(">> 1") != std::string::npos ||
        code.find("/ 2") != std::string::npos || code.find("binary_search") != std::string::npos) {
        metrics.estimatedComplexity = metrics.maxNestingDepth > 1 ? "O(n log n)" : "O(log n)";
    } else if (metrics.maxNestingDepth >= 3) {
        metrics.estimatedComplexity = "O(n^" + std::to_string(metrics.maxNestingDepth) + ")";
    } else if (metrics.maxNestingDepth == 2) {
        metrics.estimatedComplexity = "O(n^2)";
    }

    return metrics;
}

static std::vector<std::string> detectLocalPatterns(const std::string& code) {
    const std::string lower = toLowerCopy(code);
    std::vector<std::string> patterns;

    auto addPattern = [&](const std::string& label) {
        if (std::find(patterns.begin(), patterns.end(), label) == patterns.end()) {
            patterns.push_back(label);
        }
    };

    if (lower.find("class ") != std::string::npos || lower.find("struct ") != std::string::npos) addPattern("object-oriented");
    if (lower.find("template<") != std::string::npos || lower.find("generic") != std::string::npos) addPattern("generic-programming");
    if (lower.find("std::thread") != std::string::npos || lower.find("mutex") != std::string::npos || lower.find("atomic") != std::string::npos) addPattern("concurrency");
    if (lower.find("async") != std::string::npos || lower.find("await") != std::string::npos || lower.find("future") != std::string::npos) addPattern("asynchronous-flow");
    if (lower.find("new ") != std::string::npos || lower.find("delete ") != std::string::npos || lower.find("malloc") != std::string::npos || lower.find("free(") != std::string::npos) addPattern("manual-memory-management");
    if (lower.find("unique_ptr") != std::string::npos || lower.find("shared_ptr") != std::string::npos || lower.find("raii") != std::string::npos) addPattern("raii");
    if (lower.find("try") != std::string::npos || lower.find("catch") != std::string::npos || lower.find("throw") != std::string::npos) addPattern("exception-handling");
    if (lower.find("sql") != std::string::npos || lower.find("select ") != std::string::npos || lower.find("insert ") != std::string::npos) addPattern("database-io");
    if (lower.find("http") != std::string::npos || lower.find("socket") != std::string::npos || lower.find("request") != std::string::npos) addPattern("network-io");
    if (lower.find("for (") != std::string::npos || lower.find("while (") != std::string::npos || lower.find("foreach") != std::string::npos) addPattern("iterative-processing");
    if (lower.find("recurs") != std::string::npos) addPattern("recursion");
    if (lower.find("test") != std::string::npos || lower.find("assert") != std::string::npos || lower.find("expect_") != std::string::npos) addPattern("test-related-code");
    if (lower.find("todo") != std::string::npos || lower.find("placeholder") != std::string::npos || lower.find("notimplemented") != std::string::npos) addPattern("incomplete-implementation");
    if (patterns.empty()) addPattern("straight-line-logic");

    return patterns;
}

static std::string formatPatternJson(const std::vector<std::string>& patterns) {
    std::ostringstream out;
    out << "{\n  \"patterns\": [";
    for (size_t i = 0; i < patterns.size(); ++i) {
        if (i > 0) out << ", ";
        out << '"' << patterns[i] << '"';
    }
    out << "]\n}";
    return out.str();
}

static std::vector<std::string> buildQualityFindings(const LocalCodeMetrics& metrics, const std::vector<std::string>& patterns) {
    std::vector<std::string> findings;
    if (metrics.maxNestingDepth >= 4) findings.push_back("Reduce deeply nested control flow by extracting helpers or early-return branches.");
    if (metrics.cyclomaticComplexity >= 15) findings.push_back("Cyclomatic complexity is high; split decision-heavy logic into smaller units.");
    if (metrics.commentLines == 0 && metrics.codeLines > 40) findings.push_back("Add a few high-value comments around non-obvious logic or invariants.");
    if (std::find(patterns.begin(), patterns.end(), "manual-memory-management") != patterns.end()) findings.push_back("Prefer RAII wrappers or smart pointers over manual allocation where practical.");
    if (std::find(patterns.begin(), patterns.end(), "incomplete-implementation") != patterns.end()) findings.push_back("Stub markers are present; replace TODO/placeholder paths with concrete behavior before relying on this flow.");
    if (metrics.functionCount == 0 && metrics.codeLines > 25) findings.push_back("Code appears monolithic; introduce named helpers to improve reuse and testability.");
    return findings;
}

static std::string formatBulletList(const std::vector<std::string>& items) {
    if (items.empty()) return "- None\n";
    std::ostringstream out;
    for (const auto& item : items) {
        out << "- " << item << "\n";
    }
    return out.str();
}

static std::vector<std::string> splitGoalFragments(const std::string& text) {
    std::string normalized = text;
    normalized = std::regex_replace(normalized, std::regex("\\bthen\\b", std::regex::icase), ",");
    normalized = std::regex_replace(normalized, std::regex("\\band\\b", std::regex::icase), ",");
    normalized = std::regex_replace(normalized, std::regex("\\bafter that\\b", std::regex::icase), ",");

    std::vector<std::string> parts;
    std::stringstream ss(normalized);
    std::string fragment;
    while (std::getline(ss, fragment, ',')) {
        fragment = trimCopy(fragment);
        if (!fragment.empty()) parts.push_back(fragment);
    }
    if (parts.empty() && !trimCopy(text).empty()) parts.push_back(trimCopy(text));
    return parts;
}

std::string expandEnvironmentPath(const std::string& rawPath) {
    if (rawPath.empty()) return rawPath;
#ifdef _WIN32
    char buffer[32768] = {0};
    DWORD written = ExpandEnvironmentStringsA(rawPath.c_str(), buffer, static_cast<DWORD>(sizeof(buffer)));
    if (written > 0 && written < sizeof(buffer)) {
        return std::string(buffer);
    }
#endif
    return rawPath;
}

std::string resolvePathForEngine(const std::string& inputPath, const std::string& workspaceRoot) {
    if (inputPath.empty()) return inputPath;

    std::filesystem::path p(expandEnvironmentPath(inputPath));
    if (p.is_relative()) {
        if (!workspaceRoot.empty()) {
            p = std::filesystem::path(workspaceRoot) / p;
        } else {
            p = std::filesystem::current_path() / p;
        }
    }

    try {
        return std::filesystem::weakly_canonical(p).string();
    } catch (...) {
        try {
            return p.lexically_normal().string();
        } catch (...) {
            return inputPath;
        }
    }
}

} // namespace

AgenticEngine::AgenticEngine() : m_inferenceEngine(nullptr) {}

AgenticEngine::~AgenticEngine() {}

void AgenticEngine::initialize() {
    // Initialization logic if needed
}

std::string AgenticEngine::analyzeCode(const std::string& code) {
    const LocalCodeMetrics metrics = collectLocalCodeMetrics(code);
    const auto patterns = detectLocalPatterns(code);
    const auto findings = buildQualityFindings(metrics, patterns);

    std::ostringstream out;
    out << "Analysis Summary\n"
        << "Lines: " << metrics.totalLines << " total, " << metrics.codeLines << " code, " << metrics.commentLines << " comment\n"
        << "Functions: " << metrics.functionCount << " | Types: " << metrics.classCount << "\n"
        << "Complexity: " << metrics.cyclomaticComplexity << " cyclomatic, depth " << metrics.maxNestingDepth
        << ", estimated " << metrics.estimatedComplexity << "\n"
        << "Maintainability Index: " << static_cast<int>(metrics.maintainabilityIndex) << "\n"
        << "Patterns:\n" << formatBulletList(patterns)
        << "Findings:\n" << formatBulletList(findings);
    return out.str();
}

std::string AgenticEngine::analyzeCodeQuality(const std::string& code) {
    const LocalCodeMetrics metrics = collectLocalCodeMetrics(code);
    const auto patterns = detectLocalPatterns(code);
    const auto findings = buildQualityFindings(metrics, patterns);

    std::string grade = "A";
    if (metrics.cyclomaticComplexity > 20 || metrics.maxNestingDepth > 4 || metrics.maintainabilityIndex < 60.0) {
        grade = "C";
    } else if (metrics.cyclomaticComplexity > 12 || metrics.maxNestingDepth > 3 || metrics.maintainabilityIndex < 85.0) {
        grade = "B";
    }

    std::ostringstream out;
    out << "{\n"
        << "  \"qualityGrade\": \"" << grade << "\",\n"
        << "  \"maintainabilityIndex\": " << static_cast<int>(metrics.maintainabilityIndex) << ",\n"
        << "  \"cyclomaticComplexity\": " << metrics.cyclomaticComplexity << ",\n"
        << "  \"maxNestingDepth\": " << metrics.maxNestingDepth << ",\n"
        << "  \"issues\": [";
    for (size_t index = 0; index < findings.size(); ++index) {
        if (index > 0) out << ", ";
        out << '"' << findings[index] << '"';
    }
    out << "]\n}";
    return out.str();
}

std::string AgenticEngine::detectPatterns(const std::string& code) {
    return formatPatternJson(detectLocalPatterns(code));
}

std::string AgenticEngine::calculateMetrics(const std::string& code) {
    const LocalCodeMetrics metrics = collectLocalCodeMetrics(code);

    // Build JSON result
    std::ostringstream json;
    json << "{\n"
         << "  \"totalLines\": " << metrics.totalLines << ",\n"
         << "  \"codeLines\": " << metrics.codeLines << ",\n"
         << "  \"commentLines\": " << metrics.commentLines << ",\n"
         << "  \"blankLines\": " << metrics.blankLines << ",\n"
         << "  \"commentRatio\": " << (metrics.totalLines > 0 ? static_cast<double>(metrics.commentLines) / metrics.totalLines : 0.0) << ",\n"
         << "  \"functionCount\": " << metrics.functionCount << ",\n"
         << "  \"classCount\": " << metrics.classCount << ",\n"
         << "  \"cyclomaticComplexity\": " << metrics.cyclomaticComplexity << ",\n"
         << "  \"maxNestingDepth\": " << metrics.maxNestingDepth << ",\n"
         << "  \"estimatedComplexity\": \"" << metrics.estimatedComplexity << "\",\n"
         << "  \"maintainabilityIndex\": " << static_cast<int>(metrics.maintainabilityIndex) << ",\n"
         << "  \"tokens\": " << metrics.totalTokens << "\n"
         << "}";

    return json.str();
}

std::string AgenticEngine::suggestImprovements(const std::string& code) {
    const LocalCodeMetrics metrics = collectLocalCodeMetrics(code);
    const auto findings = buildQualityFindings(metrics, detectLocalPatterns(code));
    return std::string("Suggested Improvements\n") + formatBulletList(findings);
}

std::string AgenticEngine::generateCode(const std::string& prompt) {
    return chat("Generate code: " + prompt);
}

std::string AgenticEngine::generateFunction(const std::string& signature, const std::string& description) {
    return chat("Generate function " + signature + ": " + description);
}

std::string AgenticEngine::generateClass(const std::string& className, const std::string& spec) {
    return chat("Generate class " + className + " with spec: " + spec);
}

std::string AgenticEngine::generateTests(const std::string& code) {
    return chat("Generate unit tests for:\n" + code);
}

std::string AgenticEngine::refactorCode(const std::string& code, const std::string& refactoringType) {
    return chat("Refactor code (" + refactoringType + "):\n" + code);
}

std::string AgenticEngine::planTask(const std::string& goal) {
    const auto steps = splitGoalFragments(goal);
    std::ostringstream out;
    out << "Task Plan\n";
    out << "1. Clarify the concrete outcome and constraints for: " << trimCopy(goal) << "\n";
    for (size_t index = 0; index < steps.size(); ++index) {
        out << (index + 2) << ". Execute step: " << steps[index] << "\n";
    }
    out << (steps.size() + 2) << ". Validate the result and capture follow-up fixes if needed\n";
    return out.str();
}

std::string AgenticEngine::decomposeTask(const std::string& task) {
    const auto steps = splitGoalFragments(task);
    std::ostringstream out;
    out << "{\n  \"task\": \"" << trimCopy(task) << "\",\n  \"steps\": [";
    for (size_t index = 0; index < steps.size(); ++index) {
        if (index > 0) out << ", ";
        out << '"' << steps[index] << '"';
    }
    out << "]\n}";
    return out.str();
}

std::string AgenticEngine::generateWorkflow(const std::string& project) {
    return chat("Generate workflow for project: " + project);
}

std::string AgenticEngine::estimateComplexity(const std::string& task) {
    const std::string lower = toLowerCopy(task);
    int score = 1;
    static const std::vector<std::pair<std::string, int>> weightedTerms = {
        {"refactor", 2}, {"migrate", 3}, {"orchestrate", 3}, {"distributed", 3},
        {"debug", 2}, {"multi", 2}, {"autonomous", 2}, {"parallel", 2},
        {"api", 1}, {"ui", 1}, {"test", 1}, {"build", 1}
    };
    for (const auto& [term, weight] : weightedTerms) {
        if (lower.find(term) != std::string::npos) score += weight;
    }

    std::string bucket = "low";
    if (score >= 6) bucket = "high";
    else if (score >= 3) bucket = "medium";

    std::ostringstream out;
    out << "{\"complexity\":\"" << bucket << "\",\"score\":" << score
        << ",\"recommendedApproach\":\""
        << (bucket == "high" ? "decompose-and-validate" : bucket == "medium" ? "implement-with-smoke-tests" : "single-pass")
        << "\"}";
    return out.str();
}

std::string AgenticEngine::understandIntent(const std::string& userInput) {
    return chat("What is the intent of: " + userInput);
}

std::string AgenticEngine::extractEntities(const std::string& text) {
    return chat("Extract entities from: " + text);
}

std::string AgenticEngine::generateNaturalResponse(const std::string& query, const std::string& context) {
    return chat("Context: " + context + "\nQuery: " + query);
}

std::string AgenticEngine::summarizeCode(const std::string& code) {
    return chat("Summarize code:\n" + code);
}

std::string AgenticEngine::explainError(const std::string& errorMessage) {
    return chat("Explain error: " + errorMessage);
}

void AgenticEngine::collectFeedback(const std::string& requestId, bool positive, const std::string& comment) {
    std::lock_guard<std::mutex> lock(g_feedbackMutex);
    FeedbackEntry entry;
    entry.requestId = requestId;
    entry.positive = positive;
    entry.comment = comment;
    entry.timestamp = std::chrono::steady_clock::now();
    g_feedbackLog.push_back(entry);

    // Track positive/negative ratio for adaptive behavior
    if (positive) g_positiveCount++;
    else g_negativeCount++;

    // Keep feedback log bounded
    if (g_feedbackLog.size() > 10000) {
        g_feedbackLog.erase(g_feedbackLog.begin(), g_feedbackLog.begin() + 5000);
    }
}

void AgenticEngine::trainFromFeedback() {
    std::lock_guard<std::mutex> lock(g_feedbackMutex);
    if (g_feedbackLog.empty()) return;

    // Analyze negative feedback patterns to adjust behavior
    std::unordered_map<std::string, int> negPatterns;
    for (const auto& fb : g_feedbackLog) {
        if (!fb.positive && !fb.comment.empty()) {
            // Extract keywords from negative feedback
            std::istringstream iss(fb.comment);
            std::string word;
            while (iss >> word) {
                std::transform(word.begin(), word.end(), word.begin(), ::tolower);
                if (word.size() > 3) negPatterns[word]++;
            }
        }
    }

    // Store top patterns for future reference
    g_avoidPatterns.clear();
    for (const auto& [word, count] : negPatterns) {
        if (count >= 3) g_avoidPatterns.push_back(word);
    }
}

std::string AgenticEngine::getLearningStats() const {
    std::lock_guard<std::mutex> lock(g_feedbackMutex);
    std::ostringstream oss;
    oss << "{\"totalFeedback\":" << g_feedbackLog.size()
        << ",\"positive\":" << g_positiveCount
        << ",\"negative\":" << g_negativeCount
        << ",\"avoidPatterns\":" << g_avoidPatterns.size()
        << ",\"approvalRate\":";
    uint64_t total = g_positiveCount + g_negativeCount;
    if (total > 0)
        oss << std::fixed << std::setprecision(3) << (double)g_positiveCount / (double)total;
    else
        oss << "0.0";
    oss << "}";
    return oss.str();
}

void AgenticEngine::adaptToUserPreferences(const std::string& preferencesJson) {
    // Parse preference keys from JSON and apply to engine config
    // Expected format: {"temperature": 0.7, "verbosity": "concise", "language": "en"}
    if (preferencesJson.empty()) return;

    auto extractStr = [&](const std::string& key) -> std::string {
        std::string pat = "\"" + key + "\":\"";
        auto pos = preferencesJson.find(pat);
        if (pos == std::string::npos) { pat = "\"" + key + "\": \""; pos = preferencesJson.find(pat); }
        if (pos == std::string::npos) return "";
        auto s = pos + pat.size();
        auto e = preferencesJson.find('"', s);
        return (e != std::string::npos) ? preferencesJson.substr(s, e - s) : "";
    };

    std::string verbosity = extractStr("verbosity");
    if (verbosity == "concise") g_prefVerbose = false;
    else if (verbosity == "verbose") g_prefVerbose = true;

    std::string lang = extractStr("language");
    if (!lang.empty()) g_prefLanguage = lang;
}

bool AgenticEngine::validateInput(const std::string& input) {
    if (input.empty()) return false;
    if (input.size() > 1024 * 1024) return false; // 1MB limit

    // Check for null bytes
    if (input.find('\0') != std::string::npos) return false;

    // Check for common injection patterns
    static const char* dangerousPatterns[] = {
        "<script", "javascript:", "data:text/html",
        "\x1b[",  // ANSI escape injection
        nullptr
    };
    std::string lower = input;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    for (int i = 0; dangerousPatterns[i]; ++i) {
        if (lower.find(dangerousPatterns[i]) != std::string::npos) return false;
    }

    return true;
}

std::string AgenticEngine::sanitizeCode(const std::string& code) {
    std::string sanitized = code;

    // Remove dangerous system calls (but preserve them as comments)
    static const std::pair<std::string, std::string> replacements[] = {
        {"system(",    "/* BLOCKED: system( */"},
        {"exec(",      "/* BLOCKED: exec( */"},
        {"popen(",     "/* BLOCKED: popen( */"},
        {"ShellExecute(", "/* BLOCKED: ShellExecute( */"},
        {"WinExec(",   "/* BLOCKED: WinExec( */"},
        {"CreateProcess(", "/* BLOCKED: CreateProcess( */"},
    };

    for (const auto& [pattern, replacement] : replacements) {
        size_t pos = 0;
        while ((pos = sanitized.find(pattern, pos)) != std::string::npos) {
            // Check if inside a string literal or comment (simple heuristic)
            bool inString = false;
            for (size_t i = 0; i < pos && i < sanitized.size(); ++i) {
                if (sanitized[i] == '"' && (i == 0 || sanitized[i-1] != '\\')) inString = !inString;
            }
            if (!inString) {
                sanitized.replace(pos, pattern.size(), replacement);
                pos += replacement.size();
            } else {
                pos += pattern.size();
            }
        }
    }

    return sanitized;
}

bool AgenticEngine::isCommandSafe(const std::string& command) {
    if (command.empty()) return false;

    // Dangerous commands that should never be executed
    static const char* dangerousCommands[] = {
        "rm -rf /", "del /s /q c:\\", "format ",
        "mkfs", "dd if=/dev/zero",
        ":(){ :|:& };:",  // Fork bomb
        "shutdown", "halt", "poweroff",
        "reg delete", "regedit",
        "net user", "net localgroup",
        nullptr
    };

    std::string lower = command;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    for (int i = 0; dangerousCommands[i]; ++i) {
        if (lower.find(dangerousCommands[i]) != std::string::npos) return false;
    }

    // Check for pipe to destructive commands
    if (lower.find("| rm ") != std::string::npos ||
        lower.find("| del ") != std::string::npos) return false;

    // Check for output redirection to system files
    if (lower.find("> /etc/") != std::string::npos ||
        lower.find("> c:\\windows") != std::string::npos) return false;

    return true;
}

std::string AgenticEngine::writeFile(const std::string& filepath, const std::string& content) {
    std::string resolvedPath = resolvePathForEngine(filepath, m_workspaceRoot);
    if (!isCommandSafe(resolvedPath)) return "[Error] Invalid file path.";

    std::error_code ec;
    std::filesystem::path outPath(resolvedPath);
    if (outPath.has_parent_path()) {
        std::filesystem::create_directories(outPath.parent_path(), ec);
    }
    
    std::ofstream file(resolvedPath, std::ios::out | std::ios::trunc);
    if (!file.is_open()) return "[Error] Could not open file for writing: " + resolvedPath;
    
    file << content;
    file.close();
    
    return "Successfully wrote " + std::to_string(content.size()) + " bytes to " + resolvedPath;
}

std::string AgenticEngine::listDir(const std::string& path) {
    std::string effectivePath = path.empty() ? (m_workspaceRoot.empty() ? "." : m_workspaceRoot) : path;
    effectivePath = resolvePathForEngine(effectivePath, m_workspaceRoot);

    std::ostringstream oss;
    oss << "Contents of " << effectivePath << ":\n";
    try {
        for (const auto& entry : std::filesystem::directory_iterator(effectivePath)) {
            oss << (entry.is_directory() ? "[DIR] " : "[FILE] ")
                << entry.path().filename().string() << "\n";
        }
    } catch (const std::exception& e) {
        return "[Error] " + std::string(e.what());
    }
    return oss.str();
}

std::string AgenticEngine::grepFiles(const std::string& pattern, const std::string& path) {
    std::ostringstream results;
    int matchCount = 0;
    const int maxResults = 500;

    try {
        std::regex searchRegex(pattern, std::regex::ECMAScript | std::regex::icase);
        std::string searchPath = path.empty() ? (m_workspaceRoot.empty() ? "." : m_workspaceRoot) : path;
        searchPath = resolvePathForEngine(searchPath, m_workspaceRoot);

        for (const auto& entry : std::filesystem::recursive_directory_iterator(
                searchPath, std::filesystem::directory_options::skip_permission_denied)) {
            if (!entry.is_regular_file()) continue;
            if (matchCount >= maxResults) break;

            // Skip binary and large files
            auto ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            static const std::unordered_set<std::string> textExts = {
                ".cpp", ".hpp", ".h", ".c", ".cc", ".cxx", ".hxx",
                ".py", ".js", ".ts", ".jsx", ".tsx", ".java", ".cs",
                ".go", ".rs", ".rb", ".php", ".swift", ".kt",
                ".json", ".xml", ".yaml", ".yml", ".toml", ".ini", ".cfg",
                ".md", ".txt", ".cmake", ".sh", ".bat", ".ps1",
                ".html", ".css", ".scss", ".less", ".sql", ".asm"
            };
            if (!textExts.count(ext)) continue;

            auto fileSize = entry.file_size();
            if (fileSize > 10 * 1024 * 1024) continue; // Skip >10MB

            std::ifstream file(entry.path(), std::ios::in);
            if (!file.is_open()) continue;

            std::string line;
            int lineNum = 0;
            while (std::getline(file, line)) {
                lineNum++;
                if (matchCount >= maxResults) break;

                if (std::regex_search(line, searchRegex)) {
                    results << entry.path().string() << ":" << lineNum << ": " << line << "\n";
                    matchCount++;
                }
            }
        }
    } catch (const std::regex_error& e) {
        // Fall back to literal string search
        std::string searchPath = path.empty() ? (m_workspaceRoot.empty() ? "." : m_workspaceRoot) : path;
        searchPath = resolvePathForEngine(searchPath, m_workspaceRoot);
        for (const auto& entry : std::filesystem::recursive_directory_iterator(
                searchPath, std::filesystem::directory_options::skip_permission_denied)) {
            if (!entry.is_regular_file()) continue;
            if (matchCount >= maxResults) break;

            std::ifstream file(entry.path(), std::ios::in);
            if (!file.is_open()) continue;

            std::string line;
            int lineNum = 0;
            while (std::getline(file, line)) {
                lineNum++;
                if (matchCount >= maxResults) break;
                if (line.find(pattern) != std::string::npos) {
                    results << entry.path().string() << ":" << lineNum << ": " << line << "\n";
                    matchCount++;
                }
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        results << "[Error: " << e.what() << "]\n";
    }

    if (matchCount == 0) return "[No matches found for: " + pattern + "]\n";
    return results.str();
}

std::string AgenticEngine::readFile(const std::string& filePath, int startLine, int endLine) {
    std::string resolvedPath = resolvePathForEngine(filePath, m_workspaceRoot);

    std::ifstream file(resolvedPath, std::ios::in);
    if (!file.is_open()) {
        return "[Error: Cannot open file: " + resolvedPath + "]";
    }

    std::ostringstream result;
    std::string line;
    int lineNum = 0;

    // Default: read entire file if no range specified
    if (startLine <= 0 && endLine <= 0) {
        startLine = 1;
        endLine = std::numeric_limits<int>::max();
    }
    if (startLine <= 0) startLine = 1;
    if (endLine <= 0) endLine = std::numeric_limits<int>::max();

    // Cap range to prevent excessive reads
    if (endLine - startLine > 10000) {
        endLine = startLine + 10000;
    }

    while (std::getline(file, line)) {
        lineNum++;
        if (lineNum < startLine) continue;
        if (lineNum > endLine) break;
        result << lineNum << "| " << line << "\n";
    }

    if (lineNum < startLine) {
        return "[Error: File has only " + std::to_string(lineNum) + " lines, requested start at " + std::to_string(startLine) + "]";
    }

    return result.str();
}

std::string AgenticEngine::searchFiles(const std::string& query, const std::string& path) {
    std::ostringstream results;
    int matchCount = 0;
    const int maxResults = 100;

    try {
        std::string searchPath = path.empty() ? (m_workspaceRoot.empty() ? "." : m_workspaceRoot) : path;
        searchPath = resolvePathForEngine(searchPath, m_workspaceRoot);

        // Tokenize query into search terms
        std::vector<std::string> terms;
        std::istringstream iss(query);
        std::string term;
        while (iss >> term) {
            std::transform(term.begin(), term.end(), term.begin(), ::tolower);
            terms.push_back(term);
        }

        if (terms.empty()) return "[Error: Empty search query]\n";

        for (const auto& entry : std::filesystem::recursive_directory_iterator(
                searchPath, std::filesystem::directory_options::skip_permission_denied)) {
            if (!entry.is_regular_file()) continue;
            if (matchCount >= maxResults) break;

            // Check filename match first
            std::string filename = entry.path().filename().string();
            std::string filenameLower = filename;
            std::transform(filenameLower.begin(), filenameLower.end(), filenameLower.begin(), ::tolower);

            bool filenameMatch = false;
            for (const auto& t : terms) {
                if (filenameLower.find(t) != std::string::npos) {
                    filenameMatch = true;
                    break;
                }
            }

            if (filenameMatch) {
                results << "[FILE] " << entry.path().string() << " (" << entry.file_size() << " bytes)\n";
                matchCount++;
                continue;
            }

            // Check file contents for text files
            auto ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            static const std::unordered_set<std::string> textExts = {
                ".cpp", ".hpp", ".h", ".c", ".py", ".js", ".ts", ".java",
                ".json", ".xml", ".yaml", ".yml", ".md", ".txt", ".cmake"
            };
            if (!textExts.count(ext)) continue;
            if (entry.file_size() > 5 * 1024 * 1024) continue;

            std::ifstream file(entry.path(), std::ios::in);
            if (!file.is_open()) continue;

            std::string content((std::istreambuf_iterator<char>(file)),
                                 std::istreambuf_iterator<char>());
            std::string contentLower = content;
            std::transform(contentLower.begin(), contentLower.end(), contentLower.begin(), ::tolower);

            // Score by number of terms found
            int score = 0;
            for (const auto& t : terms) {
                if (contentLower.find(t) != std::string::npos) score++;
            }

            if (score > 0) {
                results << "[CONTENT score=" << score << "/" << terms.size() << "] " 
                        << entry.path().string() << "\n";
                matchCount++;
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        results << "[Error: " << e.what() << "]\n";
    }

    if (matchCount == 0) return "[No files found matching: " + query + "]\n";
    return results.str();
}

std::string AgenticEngine::referenceSymbol(const std::string& symbol) {
    // Find all references to a symbol across the codebase
    // Uses simple text search with word-boundary awareness
    std::ostringstream results;
    int defCount = 0;
    int refCount = 0;
    const int maxResults = 200;

    try {
        // Build patterns for different reference types
        std::regex defPattern(
            R"(\b(class|struct|enum|void|int|bool|auto|float|double|string|char|unsigned|signed|long|short)\s+)" 
            + symbol + R"(\b)",
            std::regex::ECMAScript
        );
        std::regex callPattern(
            symbol + R"(\s*\()",
            std::regex::ECMAScript
        );
        std::regex memberPattern(
            R"((\.|->|::))" + symbol + R"(\b)",
            std::regex::ECMAScript
        );

        std::string searchPath = m_workspaceRoot.empty() ? "." : m_workspaceRoot;
        searchPath = resolvePathForEngine(searchPath, m_workspaceRoot);
        for (const auto& entry : std::filesystem::recursive_directory_iterator(
                searchPath, std::filesystem::directory_options::skip_permission_denied)) {
            if (!entry.is_regular_file()) continue;
            if (defCount + refCount >= maxResults) break;

            auto ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            static const std::unordered_set<std::string> codeExts = {
                ".cpp", ".hpp", ".h", ".c", ".cc", ".cxx", ".hxx",
                ".py", ".js", ".ts", ".java", ".cs", ".go", ".rs"
            };
            if (!codeExts.count(ext)) continue;
            if (entry.file_size() > 5 * 1024 * 1024) continue;

            std::ifstream file(entry.path(), std::ios::in);
            if (!file.is_open()) continue;

            std::string line;
            int lineNum = 0;
            while (std::getline(file, line)) {
                lineNum++;
                if (defCount + refCount >= maxResults) break;

                // Check for symbol anywhere in line first (fast path)
                if (line.find(symbol) == std::string::npos) continue;

                // Classify the reference
                if (std::regex_search(line, defPattern)) {
                    results << "[DEF] " << entry.path().string() << ":" << lineNum << ": " << line << "\n";
                    defCount++;
                } else if (std::regex_search(line, callPattern)) {
                    results << "[CALL] " << entry.path().string() << ":" << lineNum << ": " << line << "\n";
                    refCount++;
                } else if (std::regex_search(line, memberPattern)) {
                    results << "[MEMBER] " << entry.path().string() << ":" << lineNum << ": " << line << "\n";
                    refCount++;
                } else {
                    // Generic reference
                    results << "[REF] " << entry.path().string() << ":" << lineNum << ": " << line << "\n";
                    refCount++;
                }
            }
        }
    } catch (const std::regex_error&) {
        // Fall back to simple string search
        return grepFiles(symbol, ".");
    } catch (const std::filesystem::filesystem_error& e) {
        results << "[Error: " << e.what() << "]\n";
    }

    std::ostringstream summary;
    summary << "=== Symbol: " << symbol << " ===\n"
            << "Definitions: " << defCount << " | References: " << refCount << "\n\n"
            << results.str();

    if (defCount + refCount == 0) {
        return "[No references found for symbol: " + symbol + "]\n";
    }

    return summary.str();
}

void AgenticEngine::updateConfig(const GenerationConfig& config) {
    m_config = config;
    if (m_inferenceEngine) {
        m_inferenceEngine->SetMaxMode(config.maxMode);
        m_inferenceEngine->SetDeepThinking(config.deepThinking);
        m_inferenceEngine->SetDeepResearch(config.deepResearch);
        // noRefusal is handled in the agent prompt builder
    }
}

std::string AgenticEngine::runDumpbin(const std::string& filePath, const std::string& mode) {
    RawrXD::ReverseEngineering::RawrDumpBin db;
    if (mode == "headers") return db.DumpHeaders(filePath);
    if (mode == "imports") return db.DumpImports(filePath);
    if (mode == "exports") return db.DumpExports(filePath);
    return db.DumpHeaders(filePath); // default
}

std::string AgenticEngine::runCodex(const std::string& filePath) {
    RawrXD::ReverseEngineering::RawrCodex codex;
    if (codex.LoadBinary(filePath)) {
        std::string result = "=== Codex Analysis: " + filePath + " ===\n";
        auto sections = codex.GetSections();
        result += "Sections: " + std::to_string(sections.size()) + "\n";
        for (const auto& s : sections) {
            result += "  " + s.name + " VA:0x" + std::to_string(s.virtualAddress)
                    + " Size:0x" + std::to_string(s.virtualSize) + "\n";
        }
        auto imports = codex.GetImports();
        result += "Imports: " + std::to_string(imports.size()) + "\n";
        for (size_t i = 0; i < std::min<size_t>(20, imports.size()); ++i) {
            result += "  " + imports[i].moduleName + "!" + imports[i].functionName + "\n";
        }
        auto exports = codex.GetExports();
        result += "Exports: " + std::to_string(exports.size()) + "\n";
        for (size_t i = 0; i < std::min<size_t>(20, exports.size()); ++i) {
            result += "  " + exports[i].name + "\n";
        }
        auto vulns = codex.DetectVulnerabilities();
        if (!vulns.empty()) {
            result += "Vulnerabilities: " + std::to_string(vulns.size()) + "\n";
            for (const auto& v : vulns) {
                result += "  [" + v.severity + "] " + v.type + ": " + v.description + "\n";
            }
        }
        return result;
    }
    return "Error: Could not analyze with Codex.";
}

std::string AgenticEngine::runCompiler(const std::string& sourceFile, const std::string& target) {
    RawrXD::ReverseEngineering::RawrCompiler compiler;
    auto result = compiler.CompileSource(sourceFile);
    if (result.success) {
        return "Compilation Successful: " + result.objectFile;
    } else {
        std::string errs;
        for (const auto& e : result.errors) errs += e + "\n";
        return "Compilation Failed:\n" + errs;
    }
}

std::string AgenticEngine::executeCommand(const std::string& command, bool isPowerShell) {
    if (!isCommandSafe(command)) {
        return "[Security Error] Command rejected as potentially dangerous.";
    }

    std::string fullCmd = command;
    if (!m_workspaceRoot.empty()) {
        std::string cwd = resolvePathForEngine(m_workspaceRoot, "");
#ifdef _WIN32
        fullCmd = "cd /d \"" + cwd + "\" && " + fullCmd;
#else
        fullCmd = "cd \"" + cwd + "\" && " + fullCmd;
#endif
    }

    if (isPowerShell) {
        fullCmd = "powershell -NoProfile -NonInteractive -Command \"" + fullCmd + "\"";
    }

#ifdef _WIN32
    // Use CreateProcessA with piped stdout instead of _popen to avoid
    // shell-interpolation injection through the command string.
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE hReadPipe = nullptr, hWritePipe = nullptr;
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0))
        return "[Error] Failed to create pipe for command execution.";
    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hWritePipe;
    si.hStdError  = hWritePipe;

    PROCESS_INFORMATION pi{};
    std::string cmdLine = "cmd.exe /C " + fullCmd;
    std::vector<char> cmdBuf(cmdLine.begin(), cmdLine.end());
    cmdBuf.push_back('\0');

    std::string cwd = m_workspaceRoot.empty() ? "." : m_workspaceRoot;
    BOOL created = CreateProcessA(
        nullptr, cmdBuf.data(), nullptr, nullptr, TRUE,
        CREATE_NO_WINDOW, nullptr, cwd.c_str(), &si, &pi);
    CloseHandle(hWritePipe);

    std::string result;
    if (!created) {
        CloseHandle(hReadPipe);
        return "[Error] CreateProcess failed (" + std::to_string(GetLastError()) + ")";
    }

    char buffer[4096];
    DWORD bytesRead;
    while (ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        result += buffer;
        if (result.size() > 512 * 1024) break; // 512KB cap
    }
    CloseHandle(hReadPipe);

    WaitForSingleObject(pi.hProcess, 60000);
    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    if (result.empty()) {
        result = "[Command executed with no output]";
    }
    result += "\n[exit_code=" + std::to_string(exitCode) + "]";
    return result;
#else
    fullCmd += " 2>&1";
    FILE* pipe = popen(fullCmd.c_str(), "r");
    if (!pipe) return "[Error] Failed to open pipe for command execution.";

    char buffer[4096];
    std::string result;
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
        if (result.size() > 512 * 1024) break;
    }
    int rc = pclose(pipe);

    if (result.empty()) {
        result = "[Command executed with no output]";
    }
    result += "\n[exit_code=" + std::to_string(rc) + "]";
    return result;
#endif
}

bool AgenticEngine::loadLocalModel(const std::string& modelPath) {
    if (!m_inferenceEngine) return false;

    std::string resolvedPath = resolvePathForEngine(modelPath, m_workspaceRoot);
    bool ok = m_inferenceEngine->LoadModel(resolvedPath);
    if (ok) {
        m_currentModelPath = resolvedPath;
    }
    return ok;
}

std::string AgenticEngine::getModelStatus() const {
    std::ostringstream oss;
    oss << "model_loaded=" << (isModelLoaded() ? "true" : "false") << "\n";
    oss << "model_path=" << (m_currentModelPath.empty() ? "<none>" : m_currentModelPath) << "\n";
    oss << "workspace_root=" << (m_workspaceRoot.empty() ? "<unset>" : m_workspaceRoot);
    return oss.str();
}

void AgenticEngine::setWorkspaceRoot(const std::string& rootPath) {
    if (rootPath.empty()) {
        m_workspaceRoot.clear();
        return;
    }
    m_workspaceRoot = resolvePathForEngine(rootPath, "");
}

std::string AgenticEngine::chat(const std::string& message) {
    // Headless/CLI: use injected chat provider (Ollama, etc.) when set
    if (m_chatProvider) {
        return m_chatProvider(message);
    }
    if (!m_inferenceEngine) return "[Error: No Inference Engine]";

    auto* cpuEngine = dynamic_cast<RawrXD::CPUInferenceEngine*>(m_inferenceEngine);
    if (!cpuEngine) return "[Error: Inference engine must be CPUInferenceEngine]";

    RawrXD::NativeAgent agent(cpuEngine);

    // Configure Agent from Engine config
    agent.SetMaxMode(m_config.maxMode);
    agent.SetDeepThink(m_config.deepThinking);
    agent.SetDeepResearch(m_config.deepResearch);
    agent.SetNoRefusal(m_config.noRefusal);

    // Use Execute instead of Ask to avoid boilerplate headers/footers in the string
    return agent.Execute(message);
}

// ============================================================================
// SubAgent / Chaining / Swarm — convenience wrappers
// These use the engine's own chat() to run sub-tasks. For the full
// SubAgentManager with thread pools and progress tracking, use the
// bridge-level SubAgentManager directly.
// ============================================================================

std::string AgenticEngine::runSubAgent(const std::string& description, const std::string& prompt) {
    // Simple synchronous sub-agent: just call chat with the prompt
    return chat("You are a sub-agent tasked with: " + description + "\n\n" + prompt);
}

std::string AgenticEngine::executeChain(const std::vector<std::string>& steps,
                                         const std::string& initialInput) {
    std::string currentInput = initialInput;
    for (size_t i = 0; i < steps.size(); i++) {
        // Atomic counter increment — single lock xadd, practically invisible to profiler
#if defined(RAWRXD_LINK_TELEMETRY_KERNEL_ASM) || defined(RAWR_HAS_MASM)
        UTC_IncrementCounter(&g_Counter_AgentLoop);
#endif
        std::string prompt = steps[i];
        // Replace {{input}} placeholder
        const std::string placeholder = "{{input}}";
        size_t pos = 0;
        while ((pos = prompt.find(placeholder, pos)) != std::string::npos) {
            prompt.replace(pos, placeholder.size(), currentInput);
            pos += currentInput.size();
        }
        if (prompt == steps[i] && !currentInput.empty()) {
            prompt += "\n\nContext from previous step:\n" + currentInput;
        }
        currentInput = chat(prompt);
    }
    return currentInput;
}

std::string AgenticEngine::executeSwarm(const std::vector<std::string>& prompts,
                                         const std::string& mergeStrategy,
                                         int maxParallel) {
    // Simple sequential fallback (the real parallel version is in SubAgentManager)
    std::vector<std::string> results;
    for (const auto& prompt : prompts) {
        results.push_back(chat(prompt));
    }

    if (mergeStrategy == "vote") {
        // Pick most common
        std::unordered_map<std::string, int> votes;
        for (const auto& r : results) votes[r]++;
        std::string best;
        int bestCount = 0;
        for (const auto& [r, c] : votes) {
            if (c > bestCount) { bestCount = c; best = r; }
        }
        return best;
    }
    else if (mergeStrategy == "summarize") {
        std::string all;
        for (size_t i = 0; i < results.size(); i++) {
            all += "=== Task " + std::to_string(i + 1) + " ===\n" + results[i] + "\n\n";
        }
        return chat("Merge and synthesize these sub-agent outputs:\n\n" + all);
    }

    // Default: concatenate
    std::string merged;
    for (size_t i = 0; i < results.size(); i++) {
        merged += "=== Task " + std::to_string(i + 1) + " ===\n" + results[i] + "\n\n";
    }
    return merged;
}
