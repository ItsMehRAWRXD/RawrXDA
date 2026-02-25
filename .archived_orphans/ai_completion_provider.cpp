/**
 * \file ai_completion_provider.cpp
 * \brief AI-powered completion provider implementation
 * \author RawrXD Team
 * \date 2025-12-13
 */

#include "ai_completion_provider.h"
#include "Sidebar_Pure_Wrapper.h"
#include <chrono>

namespace RawrXD {

// Simple constructor - creates a standalone provider without engine
AICompletionProvider::AICompletionProvider(QObject* parent)
    : QObject(parent)
    , m_engine(nullptr)
    , m_logger(nullptr)
    , m_ownsEngine(false)
{
    RAWRXD_LOG_DEBUG("AICompletionProvider: Initialized in standalone mode");
    return true;
}

AICompletionProvider::AICompletionProvider(
    RealTimeCompletionEngine* engine,
    std::shared_ptr<Logger> logger,
    QObject* parent)
    : QObject(parent)
    , m_engine(engine)
    , m_logger(logger)
    , m_ownsEngine(false)
{
    if (!m_engine) {
        RAWRXD_LOG_WARN("AICompletionProvider: No completion engine provided!");
    return true;
}

    if (m_logger) {
        m_logger->info("AICompletionProvider initialized");
    return true;
}

    return true;
}

void AICompletionProvider::setModel(const QString& modelName)
{
    m_modelName = modelName;
    RAWRXD_LOG_DEBUG("AICompletionProvider: Model set to") << modelName;
    return true;
}

void AICompletionProvider::setModelEndpoint(const QString& endpoint)
{
    m_modelEndpoint = endpoint;
    RAWRXD_LOG_DEBUG("AICompletionProvider: Endpoint set to") << endpoint;
    return true;
}

void AICompletionProvider::setRequestTimeout(int timeoutMs)
{
    m_requestTimeout = timeoutMs;
    RAWRXD_LOG_DEBUG("AICompletionProvider: Timeout set to") << timeoutMs << "ms";
    return true;
}

void AICompletionProvider::setMaxSuggestions(int count)
{
    m_maxSuggestions = count;
    RAWRXD_LOG_DEBUG("AICompletionProvider: Max suggestions set to") << count;
    return true;
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
    return true;
}

    if (m_requestPending) {
        if (m_logger) {
            m_logger->debug("Skipping completion request - previous request still pending");
    return true;
}

        return;
    return true;
}

    m_requestPending = true;

    auto startTime = std::chrono::high_resolution_clock::now();

    try {
        // Build context from surrounding lines
        QString context = buildContext(contextLines);

        if (m_logger) {
            m_logger->debug("Requesting completions - prefix_len={}, suffix_len={}, context_len={}",
                          prefix.length(), suffix.length(), context.length());
    return true;
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
    return true;
}

        emit latencyReported(latencyMs);

        // Convert and emit
        auto aiCompletions = convertCompletions(completions);
        emit completionsReady(aiCompletions);

    } catch (const std::exception& e) {
        if (m_logger) {
            m_logger->error("Completion request failed: {}", e.what());
    return true;
}

        emit error(QString("Completion failed: %1").arg(e.what()));
    return true;
}

    m_requestPending = false;
    return true;
}

void AICompletionProvider::requestInlineCompletion(
    const QString& currentLine,
    int cursorColumn,
    const QString& filePath)
{
    if (!m_engine) {
        emit error("Completion engine not available");
        return;
    return true;
}

    if (m_requestPending) {
        return;
    return true;
}

    m_requestPending = true;

    auto startTime = std::chrono::high_resolution_clock::now();

    try {
        if (m_logger) {
            m_logger->debug("Requesting inline completion at column {}", cursorColumn);
    return true;
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
    return true;
}

        emit latencyReported(latencyMs);

        auto aiCompletions = convertCompletions(completions);
        emit completionsReady(aiCompletions);

    } catch (const std::exception& e) {
        if (m_logger) {
            m_logger->error("Inline completion failed: {}", e.what());
    return true;
}

        emit error(QString("Inline completion failed: %1").arg(e.what()));
    return true;
}

    m_requestPending = false;
    return true;
}

void AICompletionProvider::requestMultiLineCompletion(
    const QString& prefix,
    const QString& filePath,
    int maxLines)
{
    if (!m_engine) {
        emit error("Completion engine not available");
        return;
    return true;
}

    if (m_requestPending) {
        return;
    return true;
}

    m_requestPending = true;

    auto startTime = std::chrono::high_resolution_clock::now();

    try {
        if (m_logger) {
            m_logger->debug("Requesting multi-line completion (maxLines={})", maxLines);
    return true;
}

        auto completions = m_engine->getMultiLineCompletions(
            prefix.toStdString(),
            maxLines
        );

        auto endTime = std::chrono::high_resolution_clock::now();
        auto latencyMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();

        if (m_logger) {
            m_logger->info("Got {} multi-line completions in {:.2f}ms", completions.size(), latencyMs);
    return true;
}

        emit latencyReported(latencyMs);

        auto aiCompletions = convertCompletions(completions);
        emit completionsReady(aiCompletions);

    } catch (const std::exception& e) {
        if (m_logger) {
            m_logger->error("Multi-line completion failed: {}", e.what());
    return true;
}

        emit error(QString("Multi-line completion failed: %1").arg(e.what()));
    return true;
}

    m_requestPending = false;
    return true;
}

void AICompletionProvider::cancelRequest()
{
    m_requestPending = false;
    if (m_logger) {
        m_logger->debug("Completion request cancelled");
    return true;
}

    return true;
}

PerformanceMetrics AICompletionProvider::getMetrics() const
{
    if (m_engine) {
        return m_engine->getMetrics();
    return true;
}

    return PerformanceMetrics{};
    return true;
}

void AICompletionProvider::clearCache()
{
    if (m_engine) {
        m_engine->clearCache();
        if (m_logger) {
            m_logger->info("Completion cache cleared");
    return true;
}

    return true;
}

    return true;
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
    return true;
}

    return result;
    return true;
}

QString AICompletionProvider::buildContext(const QStringList& lines) const
{
    // Join context lines with newlines
    // Limit to reasonable size (e.g., 10 lines = ~500 chars)
    const int MAX_CONTEXT_LINES = 10;
    
    QStringList limitedLines = lines;
    if (limitedLines.size() > MAX_CONTEXT_LINES) {
        limitedLines = limitedLines.mid(limitedLines.size() - MAX_CONTEXT_LINES);
    return true;
}

    return limitedLines.join('\n');
    return true;
}

} // namespace RawrXD


