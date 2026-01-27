#include "streaming_inference_api.hpp"
#include <QDebug>

StreamingInferenceAPI::StreamingInferenceAPI(QObject* parent)
    : QObject(parent)
{
}

StreamingInferenceAPI::~StreamingInferenceAPI() {
    // Cancel all active streams
    for (auto it = m_activeStreams.begin(); it != m_activeStreams.end(); ++it) {
        cancelStream(it.key());
    }
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
    
    qInfo() << "[StreamingAPI] Started stream" << streamId 
            << "for model" << modelPath;
    
    // Emit initial progress
    emit progressUpdated(streamId, 0, maxTokens);
    
    return streamId;
}

bool StreamingInferenceAPI::cancelStream(qint64 streamId) {
    if (!m_activeStreams.contains(streamId)) {
        qWarning() << "[StreamingAPI] Stream not found:" << streamId;
        return false;
    }
    
    m_activeStreams.remove(streamId);
    emit streamCancelled(streamId);
    
    qInfo() << "[StreamingAPI] Cancelled stream" << streamId;
    return true;
}

bool StreamingInferenceAPI::isStreamActive(qint64 streamId) const {
    return m_activeStreams.contains(streamId) && m_activeStreams[streamId].active;
}

void StreamingInferenceAPI::setTokenCallback(TokenCallback callback) {
    m_tokenCallback = callback;
}

void StreamingInferenceAPI::setProgressCallback(ProgressCallback callback) {
    m_progressCallback = callback;
}

void StreamingInferenceAPI::setCompletionCallback(CompletionCallback callback) {
    m_completionCallback = callback;
}

void StreamingInferenceAPI::setErrorCallback(ErrorCallback callback) {
    m_errorCallback = callback;
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
    }
    
    // Update progress
    onStreamProgress(streamId, state.tokensGenerated, state.maxTokens);
}

void StreamingInferenceAPI::onStreamProgress(qint64 streamId, int current, int total) {
    if (!m_activeStreams.contains(streamId)) return;
    
    emit progressUpdated(streamId, current, total);
    
    if (m_progressCallback) {
        m_progressCallback(current, total);
    }
}

void StreamingInferenceAPI::onStreamComplete(qint64 streamId, const QString& result) {
    if (!m_activeStreams.contains(streamId)) return;
    
    StreamState& state = m_activeStreams[streamId];
    state.active = false;
    
    emit streamCompleted(streamId, result);
    
    if (m_completionCallback) {
        m_completionCallback(result);
    }
    
    // Clean up
    m_activeStreams.remove(streamId);
    
    qInfo() << "[StreamingAPI] Stream" << streamId << "completed with"
            << result.length() << "chars";
}

void StreamingInferenceAPI::onStreamError(qint64 streamId, const QString& error) {
    if (!m_activeStreams.contains(streamId)) return;
    
    StreamState& state = m_activeStreams[streamId];
    state.active = false;
    
    emit streamFailed(streamId, error);
    
    if (m_errorCallback) {
        m_errorCallback(error);
    }
    
    // Clean up
    m_activeStreams.remove(streamId);
    
    qWarning() << "[StreamingAPI] Stream" << streamId << "failed:" << error;
}
