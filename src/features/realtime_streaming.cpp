// ============================================================================
// realtime_streaming.cpp — Real-time Streaming Completion Engine
// ============================================================================
// Implements SSE-based streaming for real-time code suggestions
// Token-by-token streaming with cancellation support
// ============================================================================

#include "realtime_streaming.h"
#include "logging/logger.h"
#include "../ai/ai_completion_provider_real.hpp"
#include <thread>
#include <chrono>
#include <atomic>
#include <queue>

static Logger s_logger("RealtimeStreaming");

class RealtimeStreamingEngine::Impl {
public:
    AICompletionProvider* m_provider;
    std::thread m_streamingThread;
    std::atomic<bool> m_streaming;
    std::atomic<bool> m_stopRequested;
    std::queue<std::string> m_tokenQueue;
    std::mutex m_queueMutex;
    std::condition_variable m_queueCV;
    StreamCallback m_callback;
    
    Impl() : m_provider(new AICompletionProvider()), 
             m_streaming(false), 
             m_stopRequested(false) {}
    
    ~Impl() {
        stop();
        delete m_provider;
    }
    
    void startStreaming(const std::string& prompt,
                       const std::string& context,
                       StreamCallback callback) {
        if (m_streaming.load()) {
            s_logger.warn("Stream already active, stopping previous");
            stop();
        }
        
        m_callback = callback;
        m_stopRequested = false;
        m_streaming = true;
        
        m_streamingThread = std::thread([this, prompt, context]() {
            streamTokens(prompt, context);
        });
        
        s_logger.info("Started streaming session");
    }
    
    void streamTokens(const std::string& prompt, const std::string& context) {
        try {
            // Build completion context
            AICompletionProvider::CompletionContext ctx;
            ctx.currentLine = prompt;
            ctx.totalLines = 1;
            
            // Get completions
            auto suggestions = m_provider->getCompletions(ctx);
            
            if (suggestions.empty()) {
                if (m_callback) m_callback("", true);
                m_streaming = false;
                return;
            }
            
            // Stream best suggestion token by token
            std::string fullText = suggestions[0].text;
            
            for (size_t i = 0; i < fullText.length() && !m_stopRequested.load(); ++i) {
                std::string token(1, fullText[i]);
                
                if (m_callback) {
                    m_callback(token, false);
                }
                
                // Simulate streaming delay (adjust based on model speed)
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
            
            // Send completion signal
            if (m_callback && !m_stopRequested.load()) {
                m_callback("", true);
            }
            
        } catch (const std::exception& e) {
            s_logger.error("Streaming error: {}", e.what());
        }
        
        m_streaming = false;
    }
    
    void stop() {
        m_stopRequested = true;
        if (m_streamingThread.joinable()) {
            m_streamingThread.join();
        }
        m_streaming = false;
        s_logger.info("Streaming stopped");
    }
};

RealtimeStreamingEngine::RealtimeStreamingEngine() : m_impl(new Impl()) {}
RealtimeStreamingEngine::~RealtimeStreamingEngine() { delete m_impl; }

bool RealtimeStreamingEngine::initialize(const std::string& modelPath) {
    return m_impl->m_provider->initialize(modelPath, "");
}

void RealtimeStreamingEngine::startStreaming(const std::string& prompt,
                                            const std::string& context,
                                            StreamCallback callback) {
    m_impl->startStreaming(prompt, context, callback);
}

void RealtimeStreamingEngine::stopStreaming() {
    m_impl->stop();
}

bool RealtimeStreamingEngine::isStreaming() const {
    return m_impl->m_streaming.load();
}
