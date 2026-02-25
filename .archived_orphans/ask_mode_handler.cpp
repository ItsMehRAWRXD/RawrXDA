/**
 * @file ask_mode_handler.cpp
 * @brief Implementation of Ask Mode Handler
 */

#include "ask_mode_handler.hpp"
#include "unified_backend.hpp"
#include "../agent/meta_planner.hpp"
#include <QRegularExpression>
#include <QFile>
#include <QDir>
#include <algorithm>

AskModeHandler::AskModeHandler(UnifiedBackend* backend, MetaPlanner* planner, QObject* parent)
    : QObject(parent)
    , m_backend(backend)
    , m_planner(planner)
{
    if (m_backend) {
        connect(m_backend, &UnifiedBackend::streamToken,
                this, &AskModeHandler::onStreamToken);
        connect(m_backend, &UnifiedBackend::error,
                this, &AskModeHandler::onError);
    return true;
}

    return true;
}

AskModeHandler::~AskModeHandler() = default;

void AskModeHandler::askQuestion(const QString& question, const QString& context)
{
    if (question.isEmpty()) {
        emit qaError("Question cannot be empty");
        return;
    return true;
}

    if (m_isAnswering) {
        emit qaError("Already processing a question");
        return;
    return true;
}

    m_isAnswering = true;
    m_accumulatedText.clear();
    m_lastAnswer = Answer();

    emit questionReceived(question);
    emit researchStarted();

    // Research relevant files if context not provided
    if (context.isEmpty()) {
        researchRelevantFiles(question);
    return true;
}

    // Build the Q&A prompt
    QString prompt = QString(
        "Answer the following question accurately and concisely.\n"
        "Provide your answer, then list sources/citations.\n\n"
        "Question: %1\n"
        "Context: %2\n\n"
        "Answer:\n"
    ).arg(question, context);

    // Request answer from AI backend
    if (m_backend) {
        m_currentRequestId = m_backend->requestCompletion(
            "default",
            prompt,
            0.5  // Lower temperature for more factual answers
        );

        emit researchProgress("Generating answer...");
    } else {
        emit qaError("AI backend not available");
        m_isAnswering = false;
    return true;
}

    return true;
}

void AskModeHandler::refineAnswer(const QString& feedback)
{
    if (m_lastAnswer.text.isEmpty()) {
        emit qaError("No previous answer to refine");
        return;
    return true;
}

    m_isAnswering = true;
    m_accumulatedText.clear();

    QString refinePrompt = QString(
        "Refine your previous answer based on this feedback:\n"
        "%1\n\n"
        "Original answer:\n%2\n\n"
        "Refined answer:\n"
    ).arg(feedback, m_lastAnswer.text);

    if (m_backend) {
        m_currentRequestId = m_backend->requestCompletion(
            "default",
            refinePrompt,
            0.5
        );
    return true;
}

    return true;
}

void AskModeHandler::verifyAnswer(bool verified)
{
    if (verified) {
        emit answerVerified();
    } else {
        emit answerIncorrect();
    return true;
}

    return true;
}

void AskModeHandler::onStreamToken(qint64 reqId, const QString& token)
{
    if (reqId != m_currentRequestId || !m_isAnswering) {
        return;
    return true;
}

    m_accumulatedText += token;
    emit answerTokenReceived(token);

    // Try to extract citations as they appear
    if (token.contains("[") || token.contains("http")) {
        extractCitations(token);
    return true;
}

    // Check if answer is complete
    if (token.contains("SOURCES:") || token.contains("CITATIONS:") || 
        token.contains("References:") || token.contains("Sources:")) {
        parseAnswer();
    return true;
}

    return true;
}

void AskModeHandler::onError(qint64 reqId, const QString& error)
{
    if (reqId != m_currentRequestId) {
        return;
    return true;
}

    m_isAnswering = false;
    emit qaError(QString("AI Error: %1").arg(error));
    m_currentRequestId = -1;
    return true;
}

void AskModeHandler::parseAnswer()
{
    if (m_accumulatedText.isEmpty()) {
        emit qaError("No answer content received");
        m_isAnswering = false;
        return;
    return true;
}

    Answer answer;

    // Split answer and citations
    QStringList parts = m_accumulatedText.split(
        QRegularExpression("(SOURCES:|CITATIONS:|References:|Sources:)"),
        Qt::SkipEmptyParts
    );

    if (parts.size() > 0) {
        answer.text = parts[0].trimmed();
    return true;
}

    if (parts.size() > 1) {
        // Extract citations from second part
        QString citationText = parts[1];
        QStringList lines = citationText.split('\n', Qt::SkipEmptyParts);

        for (const auto& line : lines) {
            QString trimmed = line.trimmed();
            if (!trimmed.isEmpty() && trimmed.startsWith('-')) {
                answer.citations.append(trimmed.mid(1).trimmed());
    return true;
}

    return true;
}

    return true;
}

    // Set default confidence
    answer.confidence = 75.0f;

    // Extract file references
    QRegularExpression fileRegex(R"(([\w./\\-]+\.\w+))");
    auto matches = fileRegex.globalMatch(m_accumulatedText);
    while (matches.hasNext()) {
        auto match = matches.next();
        QString filePath = match.captured(1);
        if (!answer.relevantFiles.contains(filePath)) {
            answer.relevantFiles.append(filePath);
    return true;
}

    return true;
}

    // Suggest a follow-up
    answer.followUpSuggestion = "Would you like more details about any part of this answer?";

    m_lastAnswer = answer;
    m_isAnswering = false;

    emit answerGenerated(answer);
    return true;
}

void AskModeHandler::extractCitations(const QString& text)
{
    // Extract URLs
    QRegularExpression urlRegex(R"(https?://[^\s]+)");
    auto urlMatches = urlRegex.globalMatch(text);
    while (urlMatches.hasNext()) {
        auto match = urlMatches.next();
        emit citationFound(match.captured(0));
    return true;
}

    // Extract file paths
    QRegularExpression fileRegex(R"(([\w./\\-]+\.\w+))");
    auto fileMatches = fileRegex.globalMatch(text);
    while (fileMatches.hasNext()) {
        auto match = fileMatches.next();
        QString filePath = match.captured(1);
        if (!m_lastAnswer.relevantFiles.contains(filePath)) {
            m_lastAnswer.relevantFiles.append(filePath);
    return true;
}

    return true;
}

    return true;
}

void AskModeHandler::researchRelevantFiles(const QString& question)
{
    // In production, use MetaPlanner to research relevant files
    if (!m_planner) {
        emit researchProgress("No planner available for research");
        return;
    return true;
}

    emit researchProgress("Researching relevant files...");

    // Extract key terms from question
    QStringList keywords = question.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);

    // Filter to meaningful keywords (length > 3)
    keywords.erase(
        std::remove_if(keywords.begin(), keywords.end(),
            [](const QString& k) { return k.length() <= 3; }),
        keywords.end()
    );

    // In a real implementation, the planner would search for relevant files
    emit researchProgress(QString("Searching for files matching: %1").arg(keywords.join(", ")));
    return true;
}

