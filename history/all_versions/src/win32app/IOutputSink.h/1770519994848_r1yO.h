#pragma once
// ============================================================================
// IOutputSink — Abstract output callback for headless / GUI-agnostic operation
// Phase 19C: Headless Surface Extraction
//
// Replaces PostMessage(WM_AGENT_OUTPUT) and direct HWND appendToOutput calls
// with a polymorphic sink that can route to:
//   1. Win32 GUI (PostMessage + RichEdit)       — Win32IDE
//   2. Console stdout/stderr                    — HeadlessIDE
//   3. HTTP response stream                     — LocalServer passthrough
//   4. Null sink                                — silent / benchmarking
//
// NO exceptions. All methods are noexcept.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdio>

// Severity levels (matches Win32IDE::OutputSeverity values)
enum class OutputSeverity : int {
    Debug   = 0,
    Info    = 1,
    Warning = 2,
    Error   = 3
};

// Streaming token origin
enum class TokenOrigin : int {
    Inference    = 0,   // Model inference token
    Agent        = 1,   // Agent framework output
    SubAgent     = 2,   // SubAgent / chain / swarm
    System       = 3    // Internal system message
};

class IOutputSink {
public:
    virtual ~IOutputSink() = default;

    // ---- Core output ----
    // General-purpose text output with severity classification
    virtual void appendOutput(const char* text, size_t length, OutputSeverity severity) noexcept = 0;

    // Convenience: null-terminated string
    virtual void appendOutput(const char* text, OutputSeverity severity) noexcept {
        if (text) appendOutput(text, strlen(text), severity);
    }

    // ---- Streaming tokens ----
    // Single token from inference / agent streaming
    virtual void onStreamingToken(const char* token, size_t length, TokenOrigin origin) noexcept = 0;

    // Stream started / completed lifecycle
    virtual void onStreamStart(const char* sourceId) noexcept = 0;
    virtual void onStreamEnd(const char* sourceId, bool success) noexcept = 0;

    // ---- Agent events ----
    // Agent started processing a prompt
    virtual void onAgentStarted(const char* agentId, const char* prompt) noexcept = 0;
    // Agent completed with a result
    virtual void onAgentCompleted(const char* agentId, const char* result, int durationMs) noexcept = 0;
    // Agent failed
    virtual void onAgentFailed(const char* agentId, const char* error) noexcept = 0;

    // ---- Status ----
    // Status bar text update (model name, backend, etc.)
    virtual void onStatusUpdate(const char* key, const char* value) noexcept = 0;

    // ---- Flush ----
    // Ensure all buffered output is delivered
    virtual void flush() noexcept = 0;
};

// ============================================================================
// ConsoleOutputSink — stdout/stderr implementation for headless mode
// ============================================================================
class ConsoleOutputSink : public IOutputSink {
public:
    void appendOutput(const char* text, size_t length, OutputSeverity severity) noexcept override;
    void onStreamingToken(const char* token, size_t length, TokenOrigin origin) noexcept override;
    void onStreamStart(const char* sourceId) noexcept override;
    void onStreamEnd(const char* sourceId, bool success) noexcept override;
    void onAgentStarted(const char* agentId, const char* prompt) noexcept override;
    void onAgentCompleted(const char* agentId, const char* result, int durationMs) noexcept override;
    void onAgentFailed(const char* agentId, const char* error) noexcept override;
    void onStatusUpdate(const char* key, const char* value) noexcept override;
    void flush() noexcept override;

    // Configuration
    void setVerbose(bool verbose) noexcept { m_verbose = verbose; }
    void setQuiet(bool quiet) noexcept { m_quiet = quiet; }
    void setJsonMode(bool json) noexcept { m_jsonMode = json; }

private:
    bool m_verbose  = false;   // Show debug-level output
    bool m_quiet    = false;   // Suppress info, show only warnings/errors
    bool m_jsonMode = false;   // Emit structured JSON lines instead of plain text
};

// ============================================================================
// NullOutputSink — silent sink for benchmarking / testing
// ============================================================================
class NullOutputSink : public IOutputSink {
public:
    void appendOutput(const char*, size_t, OutputSeverity) noexcept override {}
    void onStreamingToken(const char*, size_t, TokenOrigin) noexcept override {}
    void onStreamStart(const char*) noexcept override {}
    void onStreamEnd(const char*, bool) noexcept override {}
    void onAgentStarted(const char*, const char*) noexcept override {}
    void onAgentCompleted(const char*, const char*, int) noexcept override {}
    void onAgentFailed(const char*, const char*) noexcept override {}
    void onStatusUpdate(const char*, const char*) noexcept override {}
    void flush() noexcept override {}
};
