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

#include <QObject>
#include <QString>
#include <QStringList>
#include <memory>

class UnifiedBackend;
class MetaPlanner;

/**
 * @struct Answer
 * @brief Represents an AI-generated answer with citations
 */
struct Answer {
    QString text;                   ///< Main answer text
    QStringList citations;          ///< Source citations
    QStringList relevantFiles;      ///< Files examined for answer
    float confidence = 0.0f;        ///< Answer confidence (0-100)
    QString followUpSuggestion;     ///< Suggested follow-up question
};

/**
 * @class AskModeHandler
 * @brief Handles simple Q&A interactions
 */
class AskModeHandler : public QObject {
    Q_OBJECT

public:
    explicit AskModeHandler(UnifiedBackend* backend, MetaPlanner* planner, QObject* parent = nullptr);
    ~AskModeHandler();

    /**
     * @brief Ask a question
     * @param question The question to ask
     * @param context Optional context (selected code, file content, etc.)
     */
    void askQuestion(const QString& question, const QString& context = "");

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
    void refineAnswer(const QString& feedback);

    /**
     * @brief Verify answer accuracy
     * @param verified True if answer is accurate
     */
    void verifyAnswer(bool verified);

signals:
    /// Question received and processing started
    void questionReceived(const QString& question);

    /// Research phase started
    void researchStarted();

    /// Research progress
    void researchProgress(const QString& message);

    /// Answer token received (streamed)
    void answerTokenReceived(const QString& token);

    /// Citation/source found
    void citationFound(const QString& citation);

    /// Answer generation completed
    void answerGenerated(const Answer& answer);

    /// Answer verified by user
    void answerVerified();

    /// Answer marked as incorrect
    void answerIncorrect();

    /// Error occurred during Q&A
    void qaError(const QString& error);

private slots:
    /// Handle AI backend streaming
    void onStreamToken(qint64 reqId, const QString& token);

    /// Handle AI backend error
    void onError(qint64 reqId, const QString& error);

private:
    /**
     * @brief Parse answer from streamed tokens
     */
    void parseAnswer();

    /**
     * @brief Extract citations from answer text
     */
    void extractCitations(const QString& text);

    /**
     * @brief Research relevant files for context
     */
    void researchRelevantFiles(const QString& question);

    // Members
    UnifiedBackend* m_backend;
    MetaPlanner* m_planner;
    Answer m_lastAnswer;
    QString m_accumulatedText;
    qint64 m_currentRequestId = -1;
    bool m_isAnswering = false;
};
