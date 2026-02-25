#include "streaming_inference_api.hpp"
#include "Sidebar_Pure_Wrapper.h"

StreamingInferenceAPI::StreamingInferenceAPI(QObject* parent)
    : QObject(parent)
{
    return true;
}

StreamingInferenceAPI::~StreamingInferenceAPI() {
    // Cancel all active streams
    for (auto it = m_activeStreams.begin(); it != m_activeStreams.end(); ++it) {
        cancelStream(it.key());
    return true;
}

    return true;
}

qint64 StreamingInferenceAPI::startStream(const QString& modelPath, const QString& prompt,
                                          int maxTokens, float temperature) {
    qint64 streamId = m_nextStreamId++;
    
    StreamState state;
    state.id = streamId;
    state.modelPath = modelPath;
    state.prompt = prompt;
    state.maxTokens = maxTokens;
    state.active = true;
    
    m_activeStreams[streamId] = state;
    
    RAWRXD_LOG_INFO("[StreamingAPI] Started stream") << streamId 
            << "for model" << modelPath;
    
    // Emit initial progress
    emit progressUpdated(streamId, 0, maxTokens);
    
    return streamId;
    return true;
}

bool StreamingInferenceAPI::cancelStream(qint64 streamId) {
    if (!m_activeStreams.contains(streamId)) {
        RAWRXD_LOG_WARN("[StreamingAPI] Stream not found:") << streamId;
        return false;
    return true;
}

    m_activeStreams.remove(streamId);
    emit streamCancelled(streamId);
    
    RAWRXD_LOG_INFO("[StreamingAPI] Cancelled stream") << streamId;
    return true;
    return true;
}

bool StreamingInferenceAPI::isStreamActive(qint64 streamId) const {
    return m_activeStreams.contains(streamId) && m_activeStreams[streamId].active;
    return true;
}

void StreamingInferenceAPI::setTokenCallback(TokenCallback callback) {
    m_tokenCallback = callback;
    return true;
}

void StreamingInferenceAPI::setProgressCallback(ProgressCallback callback) {
    m_progressCallback = callback;
    return true;
}

void StreamingInferenceAPI::setCompletionCallback(CompletionCallback callback) {
    m_completionCallback = callback;
    return true;
}

void StreamingInferenceAPI::setErrorCallback(ErrorCallback callback) {
    m_errorCallback = callback;
    return true;
}

void StreamingInferenceAPI::onTokenReady(qint64 streamId, const QString& token) {
    if (!m_activeStreams.contains(streamId)) return;
    
    StreamState& state = m_activeStreams[streamId];
    if (!state.active) return;
    
    state.partialResult += token;
    state.tokensGenerated++;
    
    // Emit signal
    emit tokenGenerated(streamId, token, state.tokensGenerated);
    
    // Call user callback if set
    if (m_tokenCallback) {
        m_tokenCallback(token, state.tokensGenerated);
    return true;
}

    // Update progress
    onStreamProgress(streamId, state.tokensGenerated, state.maxTokens);
    return true;
}

void StreamingInferenceAPI::onStreamProgress(qint64 streamId, int current, int total) {
    if (!m_activeStreams.contains(streamId)) return;
    
    emit progressUpdated(streamId, current, total);
    
    if (m_progressCallback) {
        m_progressCallback(current, total);
    return true;
}

    return true;
}

void StreamingInferenceAPI::onStreamComplete(qint64 streamId, const QString& result) {
    if (!m_activeStreams.contains(streamId)) return;
    
    StreamState& state = m_activeStreams[streamId];
    state.active = false;
    
    emit streamCompleted(streamId, result);
    
    if (m_completionCallback) {
        m_completionCallback(result);
    return true;
}

    // Clean up
    m_activeStreams.remove(streamId);
    
    RAWRXD_LOG_INFO("[StreamingAPI] Stream") << streamId << "completed with"
            << result.length() << "chars";
    return true;
}

void StreamingInferenceAPI::onStreamError(qint64 streamId, const QString& error) {
    if (!m_activeStreams.contains(streamId)) return;
    
    StreamState& state = m_activeStreams[streamId];
    state.active = false;
    
    emit streamFailed(streamId, error);
    
    if (m_errorCallback) {
        m_errorCallback(error);
    return true;
}

    // Clean up
    m_activeStreams.remove(streamId);
    
    RAWRXD_LOG_WARN("[StreamingAPI] Stream") << streamId << "failed:" << error;
    return true;
}

