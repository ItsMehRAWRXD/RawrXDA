/**
 * \file ai_completion_provider.cpp
 * \brief AI-powered completion provider implementation
 * \author RawrXD Team
 * \date 2025-12-13
 */

#include "ai_completion_provider.h"
#include <QDebug>
#include <chrono>

namespace RawrXD {

AICompletionProvider::AICompletionProvider(
    RealTimeCompletionEngine* engine,
    std::shared_ptr<Logger> logger,
    QObject* parent)
    : QObject(parent)
    , m_engine(engine)
    , m_logger(logger)
{
    if (!m_engine) {
        qWarning() << "AICompletionProvider: No completion engine provided!";
    }
    if (m_logger) {
        m_logger->info("AICompletionProvider initialized");
    }
}

void AICompletionProvider::requestCompletions(
    const QString& prefix,
    const QString& suffix,
    const QString& filePath,
    const QString& fileType,
    const QStringList& contextLines)
{
    if (!m_engine) {
        emit error("Completion engine not available");
        return;
    }

    if (m_requestPending) {
        if (m_logger) {
            m_logger->debug("Skipping completion request - previous request still pending");
        }
        return;
    }

    m_requestPending = true;

    auto startTime = std::chrono::high_resolution_clock::now();

    try {
        // Build context from surrounding lines
        QString context = buildContext(contextLines);

        if (m_logger) {
            m_logger->debug("Requesting completions - prefix_len={}, suffix_len={}, context_len={}",
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
            m_logger->info("Got {} completions in {:.2f}ms", completions.size(), latencyMs);
        }

        emit latencyReported(latencyMs);

        // Convert and emit
        auto aiCompletions = convertCompletions(completions);
        emit completionsReady(aiCompletions);

    } catch (const std::exception& e) {
        if (m_logger) {
            m_logger->error("Completion request failed: {}", e.what());
        }
        emit error(QString("Completion failed: %1").arg(e.what()));
    }

    m_requestPending = false;
}

void AICompletionProvider::requestInlineCompletion(
    const QString& currentLine,
    int cursorColumn,
    const QString& filePath)
{
    if (!m_engine) {
        emit error("Completion engine not available");
        return;
    }

    if (m_requestPending) {
        return;
    }

    m_requestPending = true;

    auto startTime = std::chrono::high_resolution_clock::now();

    try {
        if (m_logger) {
            m_logger->debug("Requesting inline completion at column {}", cursorColumn);
        }

        auto completions = m_engine->getInlineCompletions(
            currentLine.toStdString(),
            cursorColumn,
            filePath.toStdString()
        );

        auto endTime = std::chrono::high_resolution_clock::now();
        auto latencyMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();

        if (m_logger) {
            m_logger->info("Got {} inline completions in {:.2f}ms", completions.size(), latencyMs);
        }

        emit latencyReported(latencyMs);

        auto aiCompletions = convertCompletions(completions);
        emit completionsReady(aiCompletions);

    } catch (const std::exception& e) {
        if (m_logger) {
            m_logger->error("Inline completion failed: {}", e.what());
        }
        emit error(QString("Inline completion failed: %1").arg(e.what()));
    }

    m_requestPending = false;
}

void AICompletionProvider::requestMultiLineCompletion(
    const QString& prefix,
    const QString& filePath,
    int maxLines)
{
    if (!m_engine) {
        emit error("Completion engine not available");
        return;
    }

    if (m_requestPending) {
        return;
    }

    m_requestPending = true;

    auto startTime = std::chrono::high_resolution_clock::now();

    try {
        if (m_logger) {
            m_logger->debug("Requesting multi-line completion (maxLines={})", maxLines);
        }

        auto completions = m_engine->getMultiLineCompletions(
            prefix.toStdString(),
            maxLines
        );

        auto endTime = std::chrono::high_resolution_clock::now();
        auto latencyMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();

        if (m_logger) {
            m_logger->info("Got {} multi-line completions in {:.2f}ms", completions.size(), latencyMs);
        }

        emit latencyReported(latencyMs);

        auto aiCompletions = convertCompletions(completions);
        emit completionsReady(aiCompletions);

    } catch (const std::exception& e) {
        if (m_logger) {
            m_logger->error("Multi-line completion failed: {}", e.what());
        }
        emit error(QString("Multi-line completion failed: %1").arg(e.what()));
    }

    m_requestPending = false;
}

void AICompletionProvider::cancelRequest()
{
    m_requestPending = false;
    if (m_logger) {
        m_logger->debug("Completion request cancelled");
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
            m_logger->info("Completion cache cleared");
        }
    }
}

QVector<AICompletion> AICompletionProvider::convertCompletions(
    const std::vector<CodeCompletion>& completions)
{
    QVector<AICompletion> result;
    result.reserve(completions.size());

    for (const auto& comp : completions) {
        AICompletion ai;
        ai.text = QString::fromStdString(comp.text);
        ai.detail = QString::fromStdString(comp.detail);
        ai.confidence = comp.confidence;
        ai.kind = QString::fromStdString(comp.kind);
        ai.cursorOffset = comp.cursorOffset;
        ai.isMultiLine = (ai.text.count('\n') > 0);
        
        result.append(ai);
    }

    return result;
}

QString AICompletionProvider::buildContext(const QStringList& lines) const
{
    // Join context lines with newlines
    // Limit to reasonable size (e.g., 10 lines = ~500 chars)
    const int MAX_CONTEXT_LINES = 10;
    
    QStringList limitedLines = lines;
    if (limitedLines.size() > MAX_CONTEXT_LINES) {
        limitedLines = limitedLines.mid(limitedLines.size() - MAX_CONTEXT_LINES);
    }
    
    return limitedLines.join('\n');
}

} // namespace RawrXD
