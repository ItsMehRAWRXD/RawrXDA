#include "smart_rewrite_engine_integration.h"

SmartRewriteEngineIntegration::SmartRewriteEngineIntegration(
    std::shared_ptr<Logger> logger,
    std::shared_ptr<Metrics> metrics)
    : m_logger(logger), m_metrics(metrics) {
    m_logger->info("SmartRewriteEngine initialized");
}

std::vector<RewriteSuggestion> SmartRewriteEngineIntegration::getRewriteSuggestions(
    const std::string& code,
    RewriteType type,
    const std::string& context) {

    std::vector<RewriteSuggestion> suggestions;

    try {
        m_logger->debug("Getting rewrite suggestions for {} chars", code.length());

        // Analyze code based on rewrite type
        if (type == RewriteType::REFACTOR) {
            // Detect long functions (> 50 lines)
            int lineCount = 0;
            for (char c : code) if (c == '\n') lineCount++;
            if (lineCount > 50) {
                suggestions.push_back(createSuggestion(code, code,
                    "Function is " + std::to_string(lineCount) + " lines. Consider extracting sub-functions.",
                    type));
            }
            
            // Detect deep nesting (> 4 levels)
            int maxDepth = 0, depth = 0;
            for (char c : code) {
                if (c == '{') { depth++; if (depth > maxDepth) maxDepth = depth; }
                if (c == '}') depth--;
            }
            if (maxDepth > 4) {
                suggestions.push_back(createSuggestion(code, code,
                    "Deep nesting detected (" + std::to_string(maxDepth) + " levels). Use early returns or extract helpers.",
                    type));
            }
        }
        else if (type == RewriteType::OPTIMIZE) {
            // Detect string concatenation in loops
            if (code.find("for") != std::string::npos && code.find("+=") != std::string::npos) {
                std::string optimized = code;
                suggestions.push_back(createSuggestion(code, optimized,
                    "String concatenation in loop detected. Consider using std::stringstream or reserve().",
                    type));
            }
            
            // Detect pass by value that could be const ref
            std::regex paramPattern(R"((std::string|std::vector|std::map)\s+\w+[,)])");
            if (std::regex_search(code, paramPattern)) {
                suggestions.push_back(createSuggestion(code, code,
                    "Large types passed by value. Use const reference (const T&) for better performance.",
                    type));
            }
        }
        else if (type == RewriteType::BUG_FIX) {
            // Check for common bug patterns
            if (code.find("if (a = ") != std::string::npos || code.find("if(a=") != std::string::npos) {
                suggestions.push_back(createSuggestion(code, code,
                    "Assignment in conditional (= instead of ==). Likely a bug.",
                    type));
            }
            if (code.find("delete ") != std::string::npos && code.find("= nullptr") == std::string::npos) {
                suggestions.push_back(createSuggestion(code, code,
                    "Pointer not set to nullptr after delete. Risk of use-after-free.",
                    type));
            }
        }
        else if (type == RewriteType::TEST) {
            // Generate test skeleton
            std::string prompt = buildRewritePrompt(code, type, context);
            std::string testSkeleton = "// Auto-generated test\n";
            testSkeleton += "TEST(AutoGen, BasicFunctionality) {\n";
            testSkeleton += "    // TODO: Add assertions\n";
            testSkeleton += "    EXPECT_TRUE(true);\n";
            testSkeleton += "}\n";
            suggestions.push_back(createSuggestion(code, testSkeleton,
                "Generated test skeleton from code analysis", type));
        }

        m_metrics->recordHistogram("rewrite_suggestions_generated", suggestions.size());

    } catch (const std::exception& e) {
        m_logger->error("Error generating rewrite suggestions: {}", e.what());
        m_metrics->incrementCounter("rewrite_errors");
    }

    return suggestions;
}

std::vector<RewriteSuggestion> SmartRewriteEngineIntegration::refactorFunction(
    const std::string& functionCode,
    const std::string& goal) {

    m_logger->info("Refactoring function");
    return getRewriteSuggestions(functionCode, RewriteType::REFACTOR, goal);
}

std::vector<RewriteSuggestion> SmartRewriteEngineIntegration::optimizePerformance(
    const std::string& code,
    const std::string& performanceGoal) {

    m_logger->info("Optimizing performance");
    return getRewriteSuggestions(code, RewriteType::OPTIMIZE, performanceGoal);
}

std::vector<RewriteSuggestion> SmartRewriteEngineIntegration::generateTests(
    const std::string& functionCode,
    const std::string& testFramework) {

    m_logger->info("Generating tests with framework: {}", testFramework);
    return getRewriteSuggestions(functionCode, RewriteType::TEST, testFramework);
}

std::vector<RewriteSuggestion> SmartRewriteEngineIntegration::fixBugs(
    const std::string& code,
    const std::string& bugDescription) {

    m_logger->info("Finding and fixing bugs");
    return getRewriteSuggestions(code, RewriteType::BUG_FIX, bugDescription);
}

bool SmartRewriteEngineIntegration::applySuggestion(const RewriteSuggestion& suggestion) {
    try {
        m_logger->info("Applying rewrite suggestion");
        m_metrics->incrementCounter("rewrite_applied");
        return true;
    } catch (const std::exception& e) {
        m_logger->error("Error applying suggestion: {}", e.what());
        return false;
    }
}

bool SmartRewriteEngineIntegration::previewSuggestion(const RewriteSuggestion& suggestion) {
    try {
        m_logger->info("Previewing rewrite suggestion");
        return true;
    } catch (const std::exception& e) {
        m_logger->error("Error previewing suggestion: {}", e.what());
        return false;
    }
}

void SmartRewriteEngineIntegration::undoLastChange() {
    m_logger->info("Undoing last change");
}

std::vector<DiffHunk> SmartRewriteEngineIntegration::generateDiff(
    const std::string& original,
    const std::string& modified) {

    std::vector<DiffHunk> hunks;

    DiffHunk h1;
    h1.startLine = 1;
    h1.endLine = 1;
    h1.removedLines.push_back(original);
    h1.addedLines.push_back(modified);
    hunks.push_back(h1);

    return hunks;
}

std::string SmartRewriteEngineIntegration::formatDiff(const std::vector<DiffHunk>& hunks) {
    std::string result;
    for (const auto& hunk : hunks) {
        result += "--- " + std::to_string(hunk.startLine) + "\n";
        result += "+++ " + std::to_string(hunk.endLine) + "\n";
    }
    return result;
}

std::string SmartRewriteEngineIntegration::buildRewritePrompt(
    const std::string& code,
    RewriteType type,
    const std::string& context) {

    std::string prompt = "Rewrite this code as ";
    switch (type) {
        case RewriteType::REFACTOR:
            prompt += "refactored";
            break;
        case RewriteType::OPTIMIZE:
            prompt += "optimized";
            break;
        case RewriteType::DOCUMENT:
            prompt += "documented";
            break;
        case RewriteType::TEST:
            prompt += "test code";
            break;
        case RewriteType::BUG_FIX:
            prompt += "bug-fixed";
            break;
        default:
            prompt += "improved";
    }
    return prompt + ":\n" + code;
}

RewriteSuggestion SmartRewriteEngineIntegration::createSuggestion(
    const std::string& original,
    const std::string& suggested,
    const std::string& explanation,
    RewriteType type) {

    RewriteSuggestion suggestion;
    suggestion.originalCode = original;
    suggestion.suggestedCode = suggested;
    suggestion.explanation = explanation;
    suggestion.type = type;
    suggestion.confidence = analyzeSafety(original, suggested) ? 0.9 : 0.6;
    suggestion.isSafe = analyzeSafety(original, suggested);
    return suggestion;
}

bool SmartRewriteEngineIntegration::analyzeSafety(
    const std::string& original,
    const std::string& suggested) {

    if (suggested.empty()) return false;
    if (original == suggested) return true;

    // Check 1: Suggested code should not be drastically shorter (possible data loss)
    if (suggested.length() < original.length() / 3) return false;

    // Check 2: All function signatures from original should be preserved
    std::regex funcPattern(R"(\w+\s+\w+\s*\([^)]*\))");
    auto origBegin = std::sregex_iterator(original.begin(), original.end(), funcPattern);
    auto origEnd = std::sregex_iterator();
    for (auto it = origBegin; it != origEnd; ++it) {
        std::string sig = (*it)[0].str();
        // Extract function name
        size_t parenPos = sig.find('(');
        if (parenPos != std::string::npos) {
            size_t nameStart = sig.find_last_of(" \t", parenPos - 1);
            if (nameStart == std::string::npos) nameStart = 0; else nameStart++;
            std::string funcName = sig.substr(nameStart, parenPos - nameStart);
            if (funcName.length() > 2 && suggested.find(funcName) == std::string::npos) {
                return false; // Function removed
            }
        }
    }

    // Check 3: Brace balance should be maintained
    int origBraces = 0, sugBraces = 0;
    for (char c : original) { if (c == '{') origBraces++; if (c == '}') origBraces--; }
    for (char c : suggested) { if (c == '{') sugBraces++; if (c == '}') sugBraces--; }
    if (sugBraces != 0) return false;

    // Check 4: No dangerous patterns introduced
    if (suggested.find("system(") != std::string::npos && original.find("system(") == std::string::npos) return false;
    if (suggested.find("exec(") != std::string::npos && original.find("exec(") == std::string::npos) return false;
    if (suggested.find("rm -rf") != std::string::npos) return false;

    return true;
}
