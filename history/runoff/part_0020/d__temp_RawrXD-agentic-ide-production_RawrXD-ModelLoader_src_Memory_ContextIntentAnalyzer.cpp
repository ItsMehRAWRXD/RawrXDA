#include "ContextIntentAnalyzer.hpp"
#include <QStringList>
#include <QRegularExpression>
#include <algorithm>
#include <cctype>

namespace mem {

ContextIntentAnalyzer::ContextIntentAnalyzer() {
    initializePatterns();
}

ContextIntentAnalyzer::~ContextIntentAnalyzer() = default;

void ContextIntentAnalyzer::initializePatterns() {
    // EXPLANATION pattern (chat only, educational)
    patterns.push_back({
        .requiredWords = {"explain", "what", "how"},
        .forbiddenWords = {"generate", "create", "write", "implement", "don't"},
        .contextWords = {"code", "function", "algorithm", "concept"},
        .intent = UserIntent::EXPLANATION,
        .confidenceThreshold = 0.7
    });

    // CHAT_ONLY explicit pattern
    patterns.push_back({
        .requiredWords = {"don't", "never", "no", "just"},
        .forbiddenWords = {},
        .contextWords = {"modify", "change", "touch", "edit", "execute"},
        .intent = UserIntent::CHAT_ONLY,
        .confidenceThreshold = 0.6
    });

    // CODE_GENERATION pattern
    patterns.push_back({
        .requiredWords = {"generate", "create", "write", "implement"},
        .forbiddenWords = {"explain", "how", "what"},
        .contextWords = {"function", "class", "file", "code", "method"},
        .intent = UserIntent::CODE_GENERATION,
        .confidenceThreshold = 0.8
    });

    // REFACTORING pattern
    patterns.push_back({
        .requiredWords = {"refactor", "rename", "optimize", "convert"},
        .forbiddenWords = {"explain"},
        .contextWords = {"code", "file", "function", "variable", "everywhere"},
        .intent = UserIntent::REFACTORING,
        .confidenceThreshold = 0.75
    });

    // AGENTIC_REQUEST pattern (find and fix, debug, analyze)
    patterns.push_back({
        .requiredWords = {"find", "fix", "debug", "analyze", "scan"},
        .forbiddenWords = {"explain", "how", "why"},
        .contextWords = {"bug", "issue", "problem", "error", "leak"},
        .intent = UserIntent::AGENTIC_REQUEST,
        .confidenceThreshold = 0.7
    });

    // PLANNING pattern
    patterns.push_back({
        .requiredWords = {"plan", "break down", "steps", "task"},
        .forbiddenWords = {},
        .contextWords = {"implement", "build", "project"},
        .intent = UserIntent::PLANNING,
        .confidenceThreshold = 0.65
    });

    // TOOL_USE pattern
    patterns.push_back({
        .requiredWords = {"compile", "build", "run", "test", "deploy"},
        .forbiddenWords = {"explain"},
        .contextWords = {"code", "project", "application"},
        .intent = UserIntent::AGENTIC_REQUEST,
        .confidenceThreshold = 0.7
    });
}

std::string ContextIntentAnalyzer::toLower(const QString& input) {
    std::string result = input.toLower().toStdString();
    return result;
}

std::optional<UserIntent> ContextIntentAnalyzer::checkExplicitCommands(const std::string& lowerInput) {
    if (lowerInput.find("/chat") == 0) return UserIntent::CHAT_ONLY;
    if (lowerInput.find("/agentic") == 0) return UserIntent::AGENTIC_REQUEST;
    if (lowerInput.find("/explain") == 0) return UserIntent::EXPLANATION;
    if (lowerInput.find("/generate") == 0) return UserIntent::CODE_GENERATION;
    if (lowerInput.find("/refactor") == 0) return UserIntent::REFACTORING;
    if (lowerInput.find("/plan") == 0) return UserIntent::PLANNING;
    return std::nullopt;
}

bool ContextIntentAnalyzer::containsNegation(const std::string& input) {
    static const std::vector<std::string> negations{
        "don't", "dont", "never", "no way", "nope", "don't modify",
        "don't change", "don't touch", "without changing", "no changes"
    };
    return std::any_of(negations.begin(), negations.end(),
        [&input](const std::string& neg) { return input.find(neg) != std::string::npos; });
}

bool ContextIntentAnalyzer::hasCodeContext(const std::string& input) {
    static const std::vector<std::string> codeWords{
        "code", "file", "project", "function", "class", "method", "variable",
        "loop", "condition", "algorithm", "bug", "error", "crash", "memory"
    };
    return std::any_of(codeWords.begin(), codeWords.end(),
        [&input](const std::string& word) { return input.find(word) != std::string::npos; });
}

double ContextIntentAnalyzer::calculatePatternScore(const std::string& input,
                                                   const IntentPattern& pattern) {
    double score = 0.0;

    // Check required words (all must be present)
    for (const auto& word : pattern.requiredWords) {
        if (input.find(word) == std::string::npos) {
            return 0.0;  // Required word missing → no match
        }
    }

    // Check forbidden words (none should be present)
    for (const auto& word : pattern.forbiddenWords) {
        if (input.find(word) != std::string::npos) {
            return 0.0;  // Forbidden word found → no match
        }
    }

    // All required present, no forbidden present → base score
    score = 0.5;

    // Boost score for context words
    int contextMatches = 0;
    for (const auto& word : pattern.contextWords) {
        if (input.find(word) != std::string::npos) {
            contextMatches++;
        }
    }
    if (!pattern.contextWords.empty()) {
        double contextBoost = (static_cast<double>(contextMatches) / pattern.contextWords.size()) * 0.5;
        score += contextBoost;
    }

    return std::min(score, 1.0);
}

bool ContextIntentAnalyzer::isAmbiguous(const std::string& input) {
    // Very short or no clear keywords
    return input.length() < 10 || input.find_first_of("?!") != std::string::npos;
}

UserIntent ContextIntentAnalyzer::resolveConflict(const QVector<QPair<UserIntent, double>>& scores,
                                                 const ConversationContext& context) {
    if (scores.empty()) return UserIntent::CHAT_ONLY;

    // Get highest score
    auto best = std::max_element(scores.begin(), scores.end(),
        [](const auto& a, const auto& b) { return a.second < b.second; });

    if (best->second < 0.5) {
        // Low confidence → default to chat
        return UserIntent::CHAT_ONLY;
    }

    // If user has been doing lots of agentic actions, bias toward agentic
    if (context.agenticActionsCount > 5 && context.chatOnlyRequestsCount == 0) {
        if (best->first == UserIntent::EXPLANATION) {
            return UserIntent::CODE_GENERATION;  // Assume they want to code after agentic session
        }
    }

    return best->first;
}

UserIntent ContextIntentAnalyzer::analyzeIntent(const QString& userInput,
                                              const ConversationContext& context) {
    std::string lowerInput = toLower(userInput);

    // 1. Check for explicit commands (/chat, /agentic, etc.)
    if (auto cmd = checkExplicitCommands(lowerInput)) {
        return *cmd;
    }

    // 2. Check for learned user corrections
    if (hasUserCorrectedSimilarRequest(userInput, context)) {
        UserIntent corrected = getCorrectedIntent(userInput, context);
        if (corrected != UserIntent::UNKNOWN) {
            return corrected;
        }
    }

    // 3. Check for explicit chat-only directives (negation)
    if (containsNegation(lowerInput)) {
        return UserIntent::CHAT_ONLY;
    }

    // 4. Check for user preference
    if (context.userPrefersChatOnly && isAmbiguous(lowerInput)) {
        return UserIntent::CHAT_ONLY;
    }

    // 5. Analyze with patterns
    QVector<QPair<UserIntent, double>> scores;
    for (const auto& pattern : patterns) {
        double score = calculatePatternScore(lowerInput, pattern);
        if (score >= pattern.confidenceThreshold) {
            scores.push_back({pattern.intent, score});
        }
    }

    // 6. Resolve conflicts
    if (!scores.empty()) {
        return resolveConflict(scores, context);
    }

    // 7. Default based on context
    if (context.userPrefersChatOnly) {
        return UserIntent::CHAT_ONLY;
    }

    return UserIntent::UNKNOWN;
}

bool ContextIntentAnalyzer::hasUserCorrectedSimilarRequest(const QString& input,
                                                         const ConversationContext& context) {
    std::string lower = toLower(input);
    for (const auto& [pattern, count] : context.userCorrections) {
        if (lower.find(pattern) != std::string::npos && count > 1) {
            return true;
        }
    }
    return false;
}

UserIntent ContextIntentAnalyzer::getCorrectedIntent(const QString& input,
                                                    const ConversationContext& context) {
    std::string lower = toLower(input);
    int maxCount = 0;
    UserIntent bestIntent = UserIntent::UNKNOWN;

    for (const auto& [pattern, count] : context.userCorrections) {
        if (lower.find(pattern) != std::string::npos && count > maxCount) {
            maxCount = count;
            // In real implementation, store the intent with correction too
            // For now, we infer it
        }
    }

    return bestIntent;
}

void ContextIntentAnalyzer::recordCorrection(const std::string& pattern, UserIntent correctedIntent) {
    // This would be called by UserPreferenceManager to record user corrections
    // Implementation delegates to user memory
}

} // namespace mem
