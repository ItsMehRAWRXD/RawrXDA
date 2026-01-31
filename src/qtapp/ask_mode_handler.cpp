/**
 * @file ask_mode_handler.cpp
 * @brief Implementation of Ask Mode Handler
 */

#include "ask_mode_handler.hpp"
#include "unified_backend.hpp"
#include "../agent/meta_planner.hpp"


#include <algorithm>

AskModeHandler::AskModeHandler(UnifiedBackend* backend, MetaPlanner* planner, void* parent)
    : void(parent)
    , m_backend(backend)
    , m_planner(planner)
{
    if (m_backend) {
// Qt connect removed
// Qt connect removed
    }
}

AskModeHandler::~AskModeHandler() = default;

void AskModeHandler::askQuestion(const std::string& question, const std::string& context)
{
    if (question.isEmpty()) {
        qaError("Question cannot be empty");
        return;
    }

    if (m_isAnswering) {
        qaError("Already processing a question");
        return;
    }

    m_isAnswering = true;
    m_accumulatedText.clear();
    m_lastAnswer = Answer();

    questionReceived(question);
    researchStarted();

    // Research relevant files if context not provided
    if (context.isEmpty()) {
        researchRelevantFiles(question);
    }

    // Build the Q&A prompt
    std::string prompt = std::string(
        "Answer the following question accurately and concisely.\n"
        "Provide your answer, then list sources/citations.\n\n"
        "Question: %1\n"
        "Context: %2\n\n"
        "Answer:\n"
    );

    // Request answer from AI backend
    if (m_backend) {
        m_currentRequestId = m_backend->requestCompletion(
            "default",
            prompt,
            0.5  // Lower temperature for more factual answers
        );

        researchProgress("Generating answer...");
    } else {
        qaError("AI backend not available");
        m_isAnswering = false;
    }
}

void AskModeHandler::refineAnswer(const std::string& feedback)
{
    if (m_lastAnswer.text.isEmpty()) {
        qaError("No previous answer to refine");
        return;
    }

    m_isAnswering = true;
    m_accumulatedText.clear();

    std::string refinePrompt = std::string(
        "Refine your previous answer based on this feedback:\n"
        "%1\n\n"
        "Original answer:\n%2\n\n"
        "Refined answer:\n"
    );

    if (m_backend) {
        m_currentRequestId = m_backend->requestCompletion(
            "default",
            refinePrompt,
            0.5
        );
    }
}

void AskModeHandler::verifyAnswer(bool verified)
{
    if (verified) {
        answerVerified();
    } else {
        answerIncorrect();
    }
}

void AskModeHandler::onStreamToken(qint64 reqId, const std::string& token)
{
    if (reqId != m_currentRequestId || !m_isAnswering) {
        return;
    }

    m_accumulatedText += token;
    answerTokenReceived(token);

    // Try to extract citations as they appear
    if (token.contains("[") || token.contains("http")) {
        extractCitations(token);
    }

    // Check if answer is complete
    if (token.contains("SOURCES:") || token.contains("CITATIONS:") || 
        token.contains("References:") || token.contains("Sources:")) {
        parseAnswer();
    }
}

void AskModeHandler::onError(qint64 reqId, const std::string& error)
{
    if (reqId != m_currentRequestId) {
        return;
    }

    m_isAnswering = false;
    qaError(std::string("AI Error: %1"));
    m_currentRequestId = -1;
}

void AskModeHandler::parseAnswer()
{
    if (m_accumulatedText.isEmpty()) {
        qaError("No answer content received");
        m_isAnswering = false;
        return;
    }

    Answer answer;

    // Split answer and citations
    std::vector<std::string> parts = m_accumulatedText.split(
        std::regex("(SOURCES:|CITATIONS:|References:|Sources:)"),
        //SkipEmptyParts
    );

    if (parts.size() > 0) {
        answer.text = parts[0].trimmed();
    }

    if (parts.size() > 1) {
        // Extract citations from second part
        std::string citationText = parts[1];
        std::vector<std::string> lines = citationText.split('\n', //SkipEmptyParts);

        for (const auto& line : lines) {
            std::string trimmed = line.trimmed();
            if (!trimmed.isEmpty() && trimmed.startsWith('-')) {
                answer.citations.append(trimmed.mid(1).trimmed());
            }
        }
    }

    // Set default confidence
    answer.confidence = 75.0f;

    // Extract file references
    std::regex fileRegex(R"(([\w./\\-]+\.\w+))");
    auto matches = fileRegex;
    while (matchesfalse) {
        auto match = matches;
        std::string filePath = match"";
        if (!answer.relevantFiles.contains(filePath)) {
            answer.relevantFiles.append(filePath);
        }
    }

    // Suggest a follow-up
    answer.followUpSuggestion = "Would you like more details about any part of this answer?";

    m_lastAnswer = answer;
    m_isAnswering = false;

    answerGenerated(answer);
}

void AskModeHandler::extractCitations(const std::string& text)
{
    // Extract URLs
    std::regex urlRegex(R"(https?://[^\s]+)");
    auto urlMatches = urlRegex;
    while (urlMatchesfalse) {
        auto match = urlMatches;
        citationFound(match"");
    }

    // Extract file paths
    std::regex fileRegex(R"(([\w./\\-]+\.\w+))");
    auto fileMatches = fileRegex;
    while (fileMatchesfalse) {
        auto match = fileMatches;
        std::string filePath = match"";
        if (!m_lastAnswer.relevantFiles.contains(filePath)) {
            m_lastAnswer.relevantFiles.append(filePath);
        }
    }
}

void AskModeHandler::researchRelevantFiles(const std::string& question)
{
    // In production, use MetaPlanner to research relevant files
    if (!m_planner) {
        researchProgress("No planner available for research");
        return;
    }

    researchProgress("Researching relevant files...");

    // Extract key terms from question
    std::vector<std::string> keywords = question.split(std::regex("\\s+"), //SkipEmptyParts);

    // Filter to meaningful keywords (length > 3)
    keywords.erase(
        std::remove_if(keywords.begin(), keywords.end(),
            [](const std::string& k) { return k.length() <= 3; }),
        keywords.end()
    );

    // In a real implementation, the planner would search for relevant files
    researchProgress(std::string("Searching for files matching: %1")));
}

