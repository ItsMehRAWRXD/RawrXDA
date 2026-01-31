/**
 * \file ai_completion_provider.cpp
 * \brief AI-powered completion provider implementation
 * \author RawrXD Team
 * \date 2025-12-13
 */

#include "ai_completion_provider.h"

#include <chrono>

namespace RawrXD {

AICompletionProvider::AICompletionProvider(
    RealTimeCompletionEngine* engine,
    std::shared_ptr<Logger> logger,
    void* parent)
    : void(parent)
    , m_engine(engine)
    , m_logger(logger)
{
    if (!m_engine) {
    }
    if (m_logger) {

    }
}

void AICompletionProvider::requestCompletions(
    const std::string& prefix,
    const std::string& suffix,
    const std::string& filePath,
    const std::string& fileType,
    const std::vector<std::string>& contextLines)
{
    if (!m_engine) {
        error("Completion engine not available");
        return;
    }

    if (m_requestPending) {
        if (m_logger) {

        }
        return;
    }

    m_requestPending = true;

    auto startTime = std::chrono::high_resolution_clock::now();

    try {
        // Build context from surrounding lines
        std::string context = buildContext(contextLines);

        if (m_logger) {

                          prefix.length(), suffix.length(), context.length());
        }

        // Call completion engine
        auto completions = m_engine->getCompletions(
            prefix.toStdString(),
            suffix.toStdString(),
            fileType.toStdString(),
            context.toStdString()
        );

        auto endTime = std::chrono::high_resolution_clock::now();
        auto latencyMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();

        if (m_logger) {

        }

        latencyReported(latencyMs);

        // Convert and auto aiCompletions = convertCompletions(completions);
        completionsReady(aiCompletions);

    } catch (const std::exception& e) {
        if (m_logger) {

        }
        error(std::string("Completion failed: %1")));
    }

    m_requestPending = false;
}

void AICompletionProvider::requestInlineCompletion(
    const std::string& currentLine,
    int cursorColumn,
    const std::string& filePath)
{
    if (!m_engine) {
        error("Completion engine not available");
        return;
    }

    if (m_requestPending) {
        return;
    }

    m_requestPending = true;

    auto startTime = std::chrono::high_resolution_clock::now();

    try {
        if (m_logger) {

        }

        auto completions = m_engine->getInlineCompletions(
            currentLine.toStdString(),
            cursorColumn,
            filePath.toStdString()
        );

        auto endTime = std::chrono::high_resolution_clock::now();
        auto latencyMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();

        if (m_logger) {

        }

        latencyReported(latencyMs);

        auto aiCompletions = convertCompletions(completions);
        completionsReady(aiCompletions);

    } catch (const std::exception& e) {
        if (m_logger) {

        }
        error(std::string("Inline completion failed: %1")));
    }

    m_requestPending = false;
}

void AICompletionProvider::requestMultiLineCompletion(
    const std::string& prefix,
    const std::string& filePath,
    int maxLines)
{
    if (!m_engine) {
        error("Completion engine not available");
        return;
    }

    if (m_requestPending) {
        return;
    }

    m_requestPending = true;

    auto startTime = std::chrono::high_resolution_clock::now();

    try {
        if (m_logger) {

        }

        auto completions = m_engine->getMultiLineCompletions(
            prefix.toStdString(),
            maxLines
        );

        auto endTime = std::chrono::high_resolution_clock::now();
        auto latencyMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();

        if (m_logger) {

        }

        latencyReported(latencyMs);

        auto aiCompletions = convertCompletions(completions);
        completionsReady(aiCompletions);

    } catch (const std::exception& e) {
        if (m_logger) {

        }
        error(std::string("Multi-line completion failed: %1")));
    }

    m_requestPending = false;
}

void AICompletionProvider::cancelRequest()
{
    m_requestPending = false;
    if (m_logger) {

    }
}

PerformanceMetrics AICompletionProvider::getMetrics() const
{
    if (m_engine) {
        return m_engine->getMetrics();
    }
    return PerformanceMetrics{};
}

void AICompletionProvider::clearCache()
{
    if (m_engine) {
        m_engine->clearCache();
        if (m_logger) {

        }
    }
}

std::vector<AICompletion> AICompletionProvider::convertCompletions(
    const std::vector<CodeCompletion>& completions)
{
    std::vector<AICompletion> result;
    result.reserve(completions.size());

    for (const auto& comp : completions) {
        AICompletion ai;
        ai.text = std::string::fromStdString(comp.text);
        ai.detail = std::string::fromStdString(comp.detail);
        ai.confidence = comp.confidence;
        ai.kind = std::string::fromStdString(comp.kind);
        ai.cursorOffset = comp.cursorOffset;
        ai.isMultiLine = (ai.text.count('\n') > 0);
        
        result.append(ai);
    }

    return result;
}

std::string AICompletionProvider::buildContext(const std::vector<std::string>& lines) const
{
    // Join context lines with newlines
    // Limit to reasonable size (e.g., 10 lines = ~500 chars)
    const int MAX_CONTEXT_LINES = 10;
    
    std::vector<std::string> limitedLines = lines;
    if (limitedLines.size() > MAX_CONTEXT_LINES) {
        limitedLines = limitedLines.mid(limitedLines.size() - MAX_CONTEXT_LINES);
    }
    
    return limitedLines.join('\n');
}

} // namespace RawrXD

