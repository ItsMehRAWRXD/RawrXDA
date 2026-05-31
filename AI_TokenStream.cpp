// ============================================================================
// AI_TokenStream.cpp
// Implementation of ProductionTokenStreamHandler and AsyncHTTPStreamReader
// ============================================================================

#include "AI_TokenStream.hpp"
#include <windows.h>
#include <stdio.h>

using namespace rawrxd::aistream;

// ============================================================================
// TokenRingBuffer implementation is header-only (inlined)
// SSEParser implementation is header-only (inlined)
// JSONContentExtractor implementation is header-only (inlined)
// TokenClassifier implementation is header-only (inlined)
// TokenSplitter implementation is header-only (inlined)
//
// This file provides:
// - WinHTTP async callback trampoline
// - Integration helpers for RawrXD editor
// - Debug logging
// ============================================================================

// ============================================================================
// Debug Logging
// ============================================================================

static void LogTokenEvent(const char* event, const TokenInfo& token, const StreamState& state) {
#ifdef _DEBUG
    char buf[512];
    const char* typeStr = "unknown";
    switch (token.token_type) {
        case TokenType::Text:        typeStr = "text"; break;
        case TokenType::Newline:     typeStr = "newline"; break;
        case TokenType::Whitespace:  typeStr = "whitespace"; break;
        case TokenType::Punctuation: typeStr = "punctuation"; break;
        case TokenType::Special:     typeStr = "special"; break;
        case TokenType::Unknown:     typeStr = "unknown"; break;
    }
    snprintf(buf, sizeof(buf),
        "[TokenStream] %s | msg=%s idx=%u type=%s latency=%ums value=%.20s%s",
        event,
        state.message_id.c_str(),
        token.index,
        typeStr,
        token.latency_ms,
        token.value.c_str(),
        token.value.length() > 20 ? "..." : ""
    );
    OutputDebugStringA(buf);
#endif
}

static void LogStateChange(const char* event, const StreamState& state) {
#ifdef _DEBUG
    char buf[256];
    const char* modeStr = "unknown";
    switch (state.mode) {
        case StreamMode::Single:   modeStr = "single"; break;
        case StreamMode::Batch:    modeStr = "batch"; break;
        case StreamMode::Adaptive: modeStr = "adaptive"; break;
    }
    snprintf(buf, sizeof(buf),
        "[TokenStream] %s | msg=%s tokens=%u mode=%s avg_latency=%ums",
        event,
        state.message_id.c_str(),
        state.total_tokens,
        modeStr,
        state.average_latency_ms
    );
    OutputDebugStringA(buf);
#endif
}

// ============================================================================
// RawrXD Editor Integration
// ============================================================================

// RawrXD Editor Integration (optional - only when RawrXD_TextEditor.h is available)
#ifdef RAWRXD_HAS_TEXT_EDITOR
#include "RawrXD_TextEditor.h"
#endif

class RawrXDTokenStreamIntegration {
public:
    struct Config {
        RawrXDTextEditor* editor = nullptr;
        HWND              status_window = NULL;
        bool              enable_visualization = true;
        uint32_t          max_tokens_display = 10000;
    };

    explicit RawrXDTokenStreamIntegration(const Config& config)
        : config_(config), processor_(SingleTokenAIProcessor::Config{}) {}

    ~RawrXDTokenStreamIntegration() {
        shutdown();
    }

    bool startStream(const std::string& message_id, const std::string& prompt) {
#ifdef RAWRXD_HAS_TEXT_EDITOR
        if (!config_.editor) return false;
#else
        (void)message_id; (void)prompt;
        return false;
#endif

        return processor_.start(
            message_id,
            prompt,
            // Token callback
            [this](const TokenInfo& token, const StreamState& state) {
                handleToken(token, state);
            },
            // Complete callback
            [this](const StreamState& state) {
                handleComplete(state);
            }
        );
    }

    void shutdown() {
        processor_.shutdown();
    }

    StreamState getState() const {
        return processor_.getState();
    }

private:
    Config config_;
    SingleTokenAIProcessor processor_;
    uint32_t tokens_inserted_ = 0;

    void handleToken(const TokenInfo& token, const StreamState& state) {
        LogTokenEvent("RECV", token, state);

        if (tokens_inserted_ < config_.max_tokens_display) {
#ifdef RAWRXD_HAS_TEXT_EDITOR
            if (config_.editor) {
                config_.editor->InsertText(token.value.c_str());
            }
#endif
            ++tokens_inserted_;

            // Update status
            if (config_.status_window) {
                char buf[256];
                snprintf(buf, sizeof(buf), "Tokens: %u | Latency: %ums | Mode: %s",
                    state.total_tokens,
                    state.average_latency_ms,
                    state.mode == StreamMode::Single ? "single" :
                    state.mode == StreamMode::Batch ? "batch" : "adaptive"
                );
                SetWindowTextA(config_.status_window, buf);
            }
        }
    }

    void handleComplete(const StreamState& state) {
        LogStateChange("COMPLETE", state);

        if (config_.status_window) {
            char buf[256];
            snprintf(buf, sizeof(buf), "Complete: %u tokens | Avg latency: %ums",
                state.total_tokens,
                state.average_latency_ms
            );
            SetWindowTextA(config_.status_window, buf);
        }
    }
};

// ============================================================================
// Legacy AITokenStreamHandler Migration Wrapper
// Allows gradual migration from old handler to new production handler
// ============================================================================

class AITokenStreamHandlerMigration : public AITokenStreamHandler {
public:
    AITokenStreamHandlerMigration(RawrXDTextEditor* editor, HWND hStatus = NULL)
        : AITokenStreamHandler(editor, hStatus)
        , production_handler_(ProductionTokenStreamHandler::Config{})
        , integration_({editor, hStatus, true, 10000})
    {
        // Set up production handler callbacks
        production_handler_.onToken([this](const TokenInfo& token, const StreamState& state) {
            // Forward to legacy queue for compatibility
            this->QueueTokens(token.value);
        });
    }

    void Start() override {
        AITokenStreamHandler::Start();
        production_handler_.start();
    }

    void Stop() override {
        production_handler_.shutdown();
        AITokenStreamHandler::Stop();
    }

    // New method: Feed SSE chunk directly (for async HTTP)
    bool FeedSSEChunk(const std::string& message_id, const uint8_t* data, size_t len) {
        return production_handler_.feedChunk(message_id, data, len);
    }

    // New method: Get production state
    StreamState GetProductionState() const {
        return production_handler_.getState();
    }

private:
    ProductionTokenStreamHandler production_handler_;
    RawrXDTokenStreamIntegration integration_;
};

// ============================================================================
// C API for RawrXD integration (extern "C" wrappers)
// ============================================================================

extern "C" {

// Opaque handle
struct RawrXDTokenStream;

typedef void (*RawrXDTokenCallback)(const char* token_value, uint32_t token_index, uint32_t latency_ms);
typedef void (*RawrXDCompleteCallback)(uint32_t total_tokens, uint32_t avg_latency_ms);

RawrXDTokenStream* RawrXD_TokenStream_Create() {
    auto* stream = new ProductionTokenStreamHandler();
    return reinterpret_cast<RawrXDTokenStream*>(stream);
}

void RawrXD_TokenStream_Destroy(RawrXDTokenStream* handle) {
    if (!handle) return;
    auto* stream = reinterpret_cast<ProductionTokenStreamHandler*>(handle);
    stream->shutdown();
    delete stream;
}

void RawrXD_TokenStream_Start(RawrXDTokenStream* handle,
                              const char* message_id,
                              RawrXDTokenCallback on_token,
                              RawrXDCompleteCallback on_complete) {
    if (!handle) return;
    auto* stream = reinterpret_cast<ProductionTokenStreamHandler*>(handle);

    stream->reset(message_id ? message_id : "");

    stream->onToken([on_token](const TokenInfo& token, const StreamState&) {
        if (on_token) on_token(token.value.c_str(), token.index, token.latency_ms);
    });

    stream->onComplete([on_complete](const StreamState& state) {
        if (on_complete) on_complete(state.total_tokens, state.average_latency_ms);
    });

    stream->start();
}

void RawrXD_TokenStream_FeedChunk(RawrXDTokenStream* handle,
                                  const char* message_id,
                                  const uint8_t* data,
                                  size_t len) {
    if (!handle) return;
    auto* stream = reinterpret_cast<ProductionTokenStreamHandler*>(handle);
    stream->feedChunk(message_id ? message_id : "", data, len);
}

void RawrXD_TokenStream_Shutdown(RawrXDTokenStream* handle) {
    if (!handle) return;
    auto* stream = reinterpret_cast<ProductionTokenStreamHandler*>(handle);
    stream->shutdown();
}

} // extern "C"
