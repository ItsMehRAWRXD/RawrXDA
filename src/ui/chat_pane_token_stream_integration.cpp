// ============================================================================
// chat_pane_token_stream_integration.cpp
// Example: Integrating TokenStreamFFBridge into Win32IDE ChatPane
// ============================================================================

#include "token_stream_fast_forward_bridge.h"
#include "fast_forward_controller.h"
#include <windows.h>
#include <richedit.h>
#include <string>
#include <memory>

// ============================================================================
// Example: ChatPane with integrated token streaming + FastForward
// ============================================================================

class ChatPaneTokenStreamIntegration {
public:
    struct Config {
        HWND hRichEdit = NULL;
        HWND hStatusBar = NULL;
        std::wstring apiHost = L"localhost";
        uint16_t apiPort = 11434; // Default Ollama port
        std::string apiModel = "codestral";
    };

    explicit ChatPaneTokenStreamIntegration(const Config& config)
        : config_(config)
    {
        // Create shared FF controller
        FastForwardConfig ffConfig;
        ffConfig.tlsTimeoutMs = 30000;      // 30s deadline
        ffConfig.slowTPSThreshold = 5.0f;   // Auto-FF below 5 TPS
        ffConfig.slowTPSDurationMs = 5000;  // For 5 seconds
        ffConfig.accelerationFactor = 3;    // Keep 1 of 3 tokens when FF active

        ffController_ = std::make_shared<RawrXD::UI::FastForwardController>(ffConfig);

        // Create bridge
        RawrXD::UI::BridgeConfig bridgeConfig;
        bridgeConfig.enforceSingleToken = true;
        bridgeConfig.tokenTimeoutMs = 2000;
        bridgeConfig.backpressureHigh = 200;
        bridgeConfig.backpressureLow = 50;
        bridgeConfig.stallDetectionMs = 5000;
        bridgeConfig.maxStallCount = 3;
        bridgeConfig.enableAutoFF = true;

        bridge_ = std::make_unique<RawrXD::UI::TokenStreamFFBridge>(
            ffController_, bridgeConfig);
    }

    ~ChatPaneTokenStreamIntegration() {
        bridge_->shutdown();
    }

    // ============================================================================
    // Send a message and stream the response
    // ============================================================================

    bool sendMessage(const std::string& userMessage) {
        if (bridge_->isRunning()) {
            // Already streaming - cancel previous
            bridge_->cancelStream("new_message");
        }

        // Generate message ID
        std::string messageId = generateMessageId();

        // Configure API
        RawrXD::UI::APIConfig apiConfig;
        apiConfig.host = config_.apiHost;
        apiConfig.port = config_.apiPort;
        apiConfig.path = L"/api/generate"; // Ollama endpoint
        apiConfig.model = config_.apiModel;
        apiConfig.temperature = 0.7f;
        apiConfig.maxTokens = 4096;
        apiConfig.timeoutMs = 120000;

        // Start streaming
        bool started = bridge_->startStream(
            messageId,
            userMessage,
            apiConfig,
            // Token callback - called for each KEPT token
            [this](const RawrXD::UI::EnrichedTokenInfo& info) {
                onTokenReceived(info);
            },
            // Complete callback
            [this](const RawrXD::UI::StreamSummary& summary) {
                onStreamComplete(summary);
            },
            // Error callback
            [this](const std::string& error) {
                onStreamError(error);
            }
        );

        if (started) {
            appendToChat("\n[Assistant]: ", RGB(0, 128, 0));
        }

        return started;
    }

    // ============================================================================
    // Cancel current stream
    // ============================================================================

    void cancelCurrentStream() {
        bridge_->cancelStream("user_cancel");
    }

    // ============================================================================
    // Get current stats for status bar
    // ============================================================================

    std::string getStatusText() const {
        auto stats = bridge_->getStats();

        char buf[256];
        snprintf(buf, sizeof(buf),
            "Tokens: %u/%u | Kept: %u | Skipped: %u | TPS: %.1f | Mode: %s",
            stats.totalTokensKept,
            stats.streamTotalTokens,
            stats.totalTokensKept,
            stats.totalTokensSkipped,
            stats.ffCurrentTPS,
            stats.streamMode == RawrXD::UI::StreamMode::Single ? "single" :
            stats.streamMode == RawrXD::UI::StreamMode::Batch ? "batch" : "adaptive"
        );
        return std::string(buf);
    }

private:
    Config config_;
    std::shared_ptr<RawrXD::UI::FastForwardController> ffController_;
    std::unique_ptr<RawrXD::UI::TokenStreamFFBridge> bridge_;

    // ============================================================================
    // Token received - append to RichEdit
    // ============================================================================

    void onTokenReceived(const RawrXD::UI::EnrichedTokenInfo& info) {
        if (!config_.hRichEdit) return;

        // Append token text to RichEdit
        appendToChat(info.token.value, RGB(0, 0, 0));

        // Update status bar
        if (config_.hStatusBar) {
            std::string status = getStatusText();
            SetWindowTextA(config_.hStatusBar, status.c_str());
        }

        // Optional: Visual feedback for skipped tokens
        if (info.tokensSkipped > 0 && info.tokensSkipped % 10 == 0) {
            // Flash status bar briefly to indicate skipping
            flashStatusBar();
        }
    }

    // ============================================================================
    // Stream complete
    // ============================================================================

    void onStreamComplete(const RawrXD::UI::StreamSummary& summary) {
        if (!config_.hRichEdit) return;

        // Append completion marker
        appendToChat("\n", RGB(0, 0, 0));

        // Log summary
        char buf[512];
        snprintf(buf, sizeof(buf),
            "[Stream Complete] Total: %u | Kept: %u | Skipped: %u | Avg Latency: %ums",
            summary.totalTokens,
            summary.tokensKept,
            summary.tokensSkipped,
            summary.averageLatencyMs
        );
        appendToChat(buf, RGB(128, 128, 128));
        appendToChat("\n", RGB(0, 0, 0));
    }

    // ============================================================================
    // Stream error
    // ============================================================================

    void onStreamError(const std::string& error) {
        if (!config_.hRichEdit) return;

        appendToChat("\n[Error: ", RGB(255, 0, 0));
        appendToChat(error, RGB(255, 0, 0));
        appendToChat("]\n", RGB(255, 0, 0));
    }

    // ============================================================================
    // UI Helpers
    // ============================================================================

    void appendToChat(const std::string& text, COLORREF color) {
        if (!config_.hRichEdit) return;

        // Set color
        CHARFORMAT2A cf = {};
        cf.cbSize = sizeof(cf);
        cf.dwMask = CFM_COLOR;
        cf.crTextColor = color;
        SendMessageA(config_.hRichEdit, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);

        // Append text
        SendMessageA(config_.hRichEdit, EM_REPLACESEL, FALSE, (LPARAM)text.c_str());

        // Scroll to bottom
        SendMessageA(config_.hRichEdit, EM_SCROLLCARET, 0, 0);
    }

    void flashStatusBar() {
        if (!config_.hStatusBar) return;

        // Briefly change background color
        SetWindowTextA(config_.hStatusBar, "[Skipping tokens...]");
        // Color would be set via WM_CTLCOLORSTATIC if needed
    }

    std::string generateMessageId() {
        static uint32_t counter = 0;
        char buf[32];
        snprintf(buf, sizeof(buf), "msg_%u_%lu", ++counter, GetTickCount());
        return std::string(buf);
    }
};

// ============================================================================
// Usage Example
// ============================================================================
/*
    // In your Win32IDE ChatPane setup:
    ChatPaneTokenStreamIntegration::Config config;
    config.hRichEdit = hRichEdit;      // Your RichEdit control
    config.hStatusBar = hStatusBar;    // Your status bar
    config.apiHost = L"localhost";
    config.apiPort = 11434;            // Ollama
    config.apiModel = "codestral";

    auto chatStream = std::make_unique<ChatPaneTokenStreamIntegration>(config);

    // When user sends a message:
    chatStream->sendMessage("Write a Python function to calculate fibonacci");

    // The response will stream in, with FastForward automatically:
    // - Skipping tokens when TPS drops below threshold
    // - Enforcing the 30s TLS deadline
    // - Showing stats in the status bar
*/
