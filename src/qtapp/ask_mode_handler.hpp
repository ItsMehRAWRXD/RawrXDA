/**
 * @file ask_mode_handler.hpp
 * @brief Ask Mode Handler - Simple Q&A with Verification
 * 
 * Ask Mode provides a straightforward question-answering interface:
 * 1. User asks a question in natural language
 * 2. AI researches and generates answer
 * 3. Citations/sources are provided
 * 4. Answer can be verified or refined
 */

#pragma once


#include <memory>

class UnifiedBackend;
class MetaPlanner;

/**
 * @struct Answer
 * @brief Represents an AI-generated answer with citations
 */
struct Answer {
    std::string text;                   ///< Main answer text
    std::vector<std::string> citations;          ///< Source citations
    std::vector<std::string> relevantFiles;      ///< Files examined for answer
    float confidence = 0.0f;        ///< Answer confidence (0-100)
    std::string followUpSuggestion;     ///< Suggested follow-up question
};

/**
 * @class AskModeHandler
 * @brief Handles simple Q&A interactions
 */
class AskModeHandler : public void {

public:
    explicit AskModeHandler(UnifiedBackend* backend, MetaPlanner* planner, void* parent = nullptr);
    ~AskModeHandler();

    /**
     * @brief Ask a question
     * @param question The question to ask
     * @param context Optional context (selected code, file content, etc.)
     */
    void askQuestion(const std::string& question, const std::string& context = "");

    /**
     * @brief Get the most recent answer
     */
    const Answer& getLastAnswer() const { return m_lastAnswer; }

    /**
     * @brief Check if currently answering a question
     */
    bool isAnswering() const { return m_isAnswering; }

    /**
     * @brief Refine the last answer with feedback
     * @param feedback User feedback or clarification
     */
    void refineAnswer(const std::string& feedback);

    /**
     * @brief Verify answer accuracy
     * @param verified True if answer is accurate
     */
    void verifyAnswer(bool verified);

    /// Question received and processing started
    void questionReceived(const std::string& question);

    /// Research phase started
    void researchStarted();

    /// Research progress
    void researchProgress(const std::string& message);

    /// Answer token received (streamed)
    void answerTokenReceived(const std::string& token);

    /// Citation/source found
    void citationFound(const std::string& citation);

    /// Answer generation completed
    void answerGenerated(const Answer& answer);

    /// Answer verified by user
    void answerVerified();

    /// Answer marked as incorrect
    void answerIncorrect();

    /// Error occurred during Q&A
    void qaError(const std::string& error);

private:
    /// Handle AI backend streaming
    void onStreamToken(int64_t reqId, const std::string& token);

    /// Handle AI backend error
    void onError(int64_t reqId, const std::string& error);

private:
    /**
     * @brief Parse answer from streamed tokens
     */
    void parseAnswer();

    /**
     * @brief Extract citations from answer text
     */
    void extractCitations(const std::string& text);

    /**
     * @brief Research relevant files for context
     */
    void researchRelevantFiles(const std::string& question);

    // Members
    UnifiedBackend* m_backend;
    MetaPlanner* m_planner;
    Answer m_lastAnswer;
    std::string m_accumulatedText;
    int64_t m_currentRequestId = -1;
    bool m_isAnswering = false;
};


