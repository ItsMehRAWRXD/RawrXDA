#pragma once

#include <QString>
#include <QStringList>
#include <vector>
#include <string>
#include <optional>
#include <algorithm>
#include <chrono>
#include <unordered_map>

namespace mem {

/**
 * @enum UserIntent
 * @brief Categorizes user requests into distinct intent types
 */
enum class UserIntent {
    CHAT_ONLY = 0,           ///< Pure conversational, no code modification
    AGENTIC_REQUEST = 1,     ///< Autonomous tool execution (find and fix, refactor, etc.)
    CODE_GENERATION = 2,     ///< Generate new code (create function, class, etc.)
    REFACTORING = 3,         ///< Rename, optimize, convert patterns
    EXPLANATION = 4,         ///< Educational content (explain, what is, why)
    DISABLE_AGENTIC = 5,     ///< Explicit disable directive (don't modify, no changes)
    PLANNING = 6,            ///< Multi-step task planning
    UNKNOWN = 7              ///< Ambiguous intent
};

/**
 * @struct ConversationContext
 * @brief Complete context about the user, workspace, and conversation state
 */
struct ConversationContext {
    QString currentFile;
    QStringList openFiles;
    QString selectedCode;
    QString recentEdits;
    QStringList conversationHistory;
    bool userPrefersChatOnly = false;
    
    // Learning & temporal context
    std::chrono::time_point<std::chrono::steady_clock> lastAgenticAction;
    int agenticActionsCount = 0;
    int chatOnlyRequestsCount = 0;
    std::unordered_map<std::string, int> userCorrections;  // pattern → correction count
    
    QString userId;           ///< Current user ID
    QString repoId;           ///< Current repository ID
};

/**
 * @struct IntentPattern
 * @brief Pattern matcher for intent detection with required/forbidden/context words
 */
struct IntentPattern {
    std::vector<std::string> requiredWords;    ///< All must be present
    std::vector<std::string> forbiddenWords;   ///< None should be present
    std::vector<std::string> contextWords;     ///< Increase confidence if present
    UserIntent intent;
    double confidenceThreshold = 0.7;
};

/**
 * @class ContextIntentAnalyzer
 * @brief Advanced intent analyzer with pattern matching, learning, and temporal awareness
 * 
 * Features:
 * - Multi-pattern keyword detection (required/forbidden/context words)
 * - Negation handling (don't, never, no → CHAT_ONLY)
 * - Temporal learning from user corrections
 * - Confidence scoring with threshold
 * - Explicit command override (/chat, /agentic)
 */
class ContextIntentAnalyzer {
public:
    ContextIntentAnalyzer();
    ~ContextIntentAnalyzer();

    /**
     * @brief Analyze user input and determine intent
     * @param userInput The raw user message
     * @param context The conversation and workspace context
     * @return Detected UserIntent
     */
    UserIntent analyzeIntent(const QString& userInput,
                           const ConversationContext& context);

    /**
     * @brief Record user correction for a similar pattern
     * @param pattern The recognized pattern
     * @param correctedIntent The intent user indicated was correct
     */
    void recordCorrection(const std::string& pattern, UserIntent correctedIntent);

    /**
     * @brief Check if user has corrected similar requests in past
     * @param input The current input
     * @param context The conversation context
     * @return True if similar correction pattern found with high count
     */
    bool hasUserCorrectedSimilarRequest(const QString& input,
                                       const ConversationContext& context);

    /**
     * @brief Get corrected intent based on user history
     * @param input The current input
     * @param context The conversation context
     * @return The corrected UserIntent from history
     */
    UserIntent getCorrectedIntent(const QString& input,
                                 const ConversationContext& context);

private:
    std::vector<IntentPattern> patterns;

    /**
     * @brief Convert input to lowercase for case-insensitive matching
     */
    static std::string toLower(const QString& input);

    /**
     * @brief Check if input starts with explicit command
     * @return Intent if explicit command found, nullopt otherwise
     */
    std::optional<UserIntent> checkExplicitCommands(const std::string& lowerInput);

    /**
     * @brief Calculate confidence score for a pattern
     * @param input Lowercase input string
     * @param pattern Pattern to test
     * @return Confidence score [0.0, 1.0]
     */
    double calculatePatternScore(const std::string& input,
                                const IntentPattern& pattern);

    /**
     * @brief Check if input contains negation keywords
     */
    bool containsNegation(const std::string& input);

    /**
     * @brief Check if input contains code-context words
     */
    bool hasCodeContext(const std::string& input);

    /**
     * @brief Initialize default intent patterns
     */
    void initializePatterns();

    /**
     * @brief Determine if request is ambiguous
     */
    bool isAmbiguous(const std::string& input);

    /**
     * @brief Resolve multiple matching intents based on temporal context
     */
    UserIntent resolveConflict(const QVector<QPair<UserIntent, double>>& scores,
                              const ConversationContext& context);
};

} // namespace mem
