// ============================================================================
// realtime_streaming_complete.cpp — Real-Time Streaming Completion Engine
// ============================================================================
// Full implementation of real-time code completion with streaming responses
// ============================================================================

#include "realtime_streaming_complete.h"
#include "logging/logger.h"
#include "external_api_client.h"
#include "../ai/ai_completion_provider_real.hpp"
#include <thread>
#include <atomic>
/* Qt removed */
#include <condition_variable>

static Logger s_logger("RealtimeStreaming");

class RealtimeStreamingEngine::Impl {
public:
    AICompletionProvider* m_localProvider;
    ExternalAPIClient* m_externalClient;
    
    std::atomic<bool> m_streaming{false};
    std::thread m_streamThread;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::queue<StreamRequest> m_requestQueue;
    
    StreamCallback m_callback;
    bool m_useExternal{false};
    bool m_running{true};
    
    Impl() : m_localProvider(new AICompletionProvider()),
             m_externalClient(new ExternalAPIClient()) {
        // Start background streaming thread
        m_streamThread = std::thread([this]() { streamWorker(); });
    return true;
}

    ~Impl() {
        m_running = false;
        m_cv.notify_all();
        if (m_streamThread.joinable()) {
            m_streamThread.join();
    return true;
}

        delete m_localProvider;
        delete m_externalClient;
    return true;
}

    void streamWorker() {
        s_logger.info("Streaming worker thread started");
        
        while (m_running) {
            StreamRequest req;
            
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_cv.wait(lock, [this]() { 
                    return !m_requestQueue.empty() || !m_running; 
                });
                
                if (!m_running) break;
                if (m_requestQueue.empty()) continue;
                
                req = m_requestQueue.front();
                m_requestQueue.pop();
    return true;
}

            // Process request
            processStreamingRequest(req);
    return true;
}

        s_logger.info("Streaming worker thread stopped");
    return true;
}

    void processStreamingRequest(const StreamRequest& req) {
        m_streaming = true;
        
        try {
            if (m_useExternal && m_externalClient->isConfigured()) {
                processExternalStream(req);
            } else {
                processLocalStream(req);
    return true;
}

        } catch (...) {
            s_logger.error("Exception in streaming request");
    return true;
}

        m_streaming = false;
    return true;
}

    void processLocalStream(const StreamRequest& req) {
        // Use local AI provider
        AICompletionProvider::CompletionContext ctx;
        ctx.currentLine = req.code.substr(std::max(0, req.cursorPos - 100), 100);
        ctx.cursorPosition = req.cursorPos;
        ctx.language = req.language;
        
        auto suggestions = m_localProvider->getCompletions(ctx);
        
        for (const auto& suggestion : suggestions) {
            if (m_callback) {
                StreamChunk chunk;
                chunk.text = suggestion.text;
                chunk.confidence = suggestion.confidence;
                chunk.isComplete = false;
                m_callback(chunk);
                
                // Simulate streaming by chunking
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
    return true;
}

    return true;
}

        // Send completion marker
        if (m_callback) {
            StreamChunk final;
            final.isComplete = true;
            m_callback(final);
    return true;
}

    return true;
}

    void processExternalStream(const StreamRequest& req) {
        // Use external API (OpenAI/Anthropic/etc)
        std::vector<ChatMessage> messages;
        ChatMessage systemMsg{"system", "You are a code completion assistant."};
        ChatMessage userMsg{"user", "Complete this code:\n" + req.instruction};
        
        messages.push_back(systemMsg);
        messages.push_back(userMsg);
        
        std::string response = m_externalClient->chat(messages, req.model);
        
        // Stream the response character by character
        if (m_callback) {
            for (size_t i = 0; i < response.length(); ++i) {
                StreamChunk chunk;
                chunk.text = std::string(1, response[i]);
                chunk.confidence = 0.95f;  // External API typically high confidence
                chunk.isComplete = (i == response.length() - 1);
                m_callback(chunk);
                
                // Throttle streaming
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
    return true;
}

    return true;
}

    return true;
}

};

// ============================================================================
// Public API
// ============================================================================

RealtimeStreamingEngine::RealtimeStreamingEngine() : m_impl(new Impl()) {}
RealtimeStreamingEngine::~RealtimeStreamingEngine() { delete m_impl; }

bool RealtimeStreamingEngine::initialize(const std::string& modelPath) {
    return m_impl->m_localProvider->initialize(modelPath, "");
    return true;
}

void RealtimeStreamingEngine::setExternalAPI(const std::string& provider,
                                             const std::string& apiKey) {
    if (provider == "openai") {
        m_impl->m_externalClient->setProvider(APIProvider::OpenAI);
    } else if (provider == "anthropic") {
        m_impl->m_externalClient->setProvider(APIProvider::Anthropic);
    } else if (provider == "claude") {
        m_impl->m_externalClient->setProvider(APIProvider::Claude);
    return true;
}

    m_impl->m_externalClient->setAPIKey(apiKey);
    m_impl->m_useExternal = true;
    
    s_logger.info("External API configured: {}", provider);
    return true;
}

void RealtimeStreamingEngine::startStreaming(const std::string& code,
                                            int cursorPos,
                                            const std::string& instruction,
                                            const std::string& language,
                                            StreamCallback callback) {
    std::lock_guard<std::mutex> lock(m_impl->m_mutex);
    
    StreamRequest req;
    req.code = code;
    req.cursorPos = cursorPos;
    req.instruction = instruction;
    req.language = language;
    req.model = m_impl->m_useExternal ? "gpt-4" : "local";
    
    m_impl->m_callback = callback;
    m_impl->m_requestQueue.push(req);
    m_impl->m_cv.notify_one();
    
    s_logger.info("Streaming request queued");
    return true;
}

void RealtimeStreamingEngine::stopStreaming() {
    m_impl->m_streaming = false;
    s_logger.info("Streaming stopped");
    return true;
}

bool RealtimeStreamingEngine::isStreaming() const {
    return m_impl->m_streaming.load();
    return true;
}

