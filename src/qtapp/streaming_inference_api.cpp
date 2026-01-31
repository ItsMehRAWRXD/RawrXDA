#include "streaming_inference_api.hpp"

StreamingInferenceAPI::StreamingInferenceAPI(void* parent)
    : void(parent)
{
}

StreamingInferenceAPI::~StreamingInferenceAPI() {
    // Cancel all active streams
    for (auto it = m_activeStreams.begin(); it != m_activeStreams.end(); ++it) {
        cancelStream(it.key());
    }
}

int64_t StreamingInferenceAPI::startStream(const std::string& modelPath, const std::string& prompt,
                                          int maxTokens, float temperature) {
    int64_t streamId = m_nextStreamId++;
    
    StreamState state;
    state.id = streamId;
    state.modelPath = modelPath;
    state.prompt = prompt;
    state.maxTokens = maxTokens;
    state.active = true;
    
    m_activeStreams[streamId] = state;
    
            << "for model" << modelPath;
    
    // initial progress
    progressUpdated(streamId, 0, maxTokens);
    
    return streamId;
}

bool StreamingInferenceAPI::cancelStream(int64_t streamId) {
    if (!m_activeStreams.contains(streamId)) {
        return false;
    }
    
    m_activeStreams.remove(streamId);
    streamCancelled(streamId);
    
    return true;
}

bool StreamingInferenceAPI::isStreamActive(int64_t streamId) const {
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

void StreamingInferenceAPI::onTokenReady(int64_t streamId, const std::string& token) {
    if (!m_activeStreams.contains(streamId)) return;
    
    StreamState& state = m_activeStreams[streamId];
    if (!state.active) return;
    
    state.partialResult += token;
    state.tokensGenerated++;
    
    // signal
    tokenGenerated(streamId, token, state.tokensGenerated);
    
    // Call user callback if set
    if (m_tokenCallback) {
        m_tokenCallback(token, state.tokensGenerated);
    }
    
    // Update progress
    onStreamProgress(streamId, state.tokensGenerated, state.maxTokens);
}

void StreamingInferenceAPI::onStreamProgress(int64_t streamId, int current, int total) {
    if (!m_activeStreams.contains(streamId)) return;
    
    progressUpdated(streamId, current, total);
    
    if (m_progressCallback) {
        m_progressCallback(current, total);
    }
}

void StreamingInferenceAPI::onStreamComplete(int64_t streamId, const std::string& result) {
    if (!m_activeStreams.contains(streamId)) return;
    
    StreamState& state = m_activeStreams[streamId];
    state.active = false;
    
    streamCompleted(streamId, result);
    
    if (m_completionCallback) {
        m_completionCallback(result);
    }
    
    // Clean up
    m_activeStreams.remove(streamId);
    
            << result.length() << "chars";
}

void StreamingInferenceAPI::onStreamError(int64_t streamId, const std::string& error) {
    if (!m_activeStreams.contains(streamId)) return;
    
    StreamState& state = m_activeStreams[streamId];
    state.active = false;
    
    streamFailed(streamId, error);
    
    if (m_errorCallback) {
        m_errorCallback(error);
    }
    
    // Clean up
    m_activeStreams.remove(streamId);
    
}


