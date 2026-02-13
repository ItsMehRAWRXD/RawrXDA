/**
 * @file llm_http_client.hpp
 * @brief Portable async HTTP client for LLM backends (Qt-free)
 *
 * Post-Qt replacement: uses WinHTTP on Windows, curl on POSIX.
 * All methods return structured results (no exceptions).
 * Designed for both CLI and Win32 GUI interaction.
 *
 * Key fix: std::future is retained in HttpChainStep until completion,
 * preventing the premature destruction that Qt's event loop hid.
 *
 * Thread-safe. No STL allocators inside active I/O paths.
 */
#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

// ============================================================================
// HttpResponse — structured result, no exceptions
// ============================================================================
struct HttpResponse {
    bool        success    = false;
    int         statusCode = 0;
    std::string body;
    std::string error;
    int         latencyMs  = 0;  // wall-clock time

    static HttpResponse ok(int code, const std::string& b, int ms) {
        return {true, code, b, {}, ms};
    }
    static HttpResponse fail(const std::string& err, int ms = 0) {
        return {false, 0, {}, err, ms};
    }
};

// ============================================================================
// HttpRequest — all info needed for a single request
// ============================================================================
struct HttpRequest {
    std::string method  = "POST";
    std::string url;
    std::string body;
    std::string apiKey;            // Bearer token (optional)
    std::string extraHeaderKey;    // e.g. "x-api-key" for Claude
    std::string extraHeaderVal;
    int         timeoutMs = 30000;
};

// ============================================================================
// StlHttpClient — async-capable HTTP client with future retention
// ============================================================================
class StlHttpClient {
public:
    StlHttpClient();
    ~StlHttpClient();

    // ---- Configuration ----
    void setDefaultTimeout(int ms) { m_defaultTimeout = ms; }
    int  getDefaultTimeout() const { return m_defaultTimeout; }

    // ---- Synchronous ----
    HttpResponse send(const HttpRequest& req);

    // Convenience: POST JSON
    HttpResponse postJson(const std::string& url,
                          const std::string& jsonBody,
                          int timeoutMs = 0);

    // Convenience: GET
    HttpResponse get(const std::string& url, int timeoutMs = 0);

    // ---- Asynchronous (future-based) ----
    // CRITICAL: The returned future MUST be held alive until .get() is called.
    // Destroying the future before completion cancels the async operation.
    std::future<HttpResponse> sendAsync(const HttpRequest& req);
    std::future<HttpResponse> postJsonAsync(const std::string& url,
                                            const std::string& jsonBody,
                                            int timeoutMs = 0);

    // ---- Cancellation ----
    void cancelAll();

    // ---- Stats ----
    struct Stats {
        std::atomic<uint64_t> totalRequests{0};
        std::atomic<uint64_t> successCount{0};
        std::atomic<uint64_t> failCount{0};
        std::atomic<uint64_t> totalLatencyMs{0};
        std::atomic<uint64_t> activeRequests{0};
    };
    const Stats& stats() const { return m_stats; }
    void resetStats();

    // ---- Callbacks (GUI notification, optional) ----
    std::function<void(const std::string& url)>          onRequestStarted;
    std::function<void(const HttpResponse&)>             onRequestCompleted;
    std::function<void(const std::string&, bool)>        onError;

    // Singleton for convenience (both CLI and GUI share one instance)
    static StlHttpClient& instance();

private:
    HttpResponse platformSend(const HttpRequest& req);

    int                m_defaultTimeout = 30000;
    std::atomic<bool>  m_cancelled{false};
    Stats              m_stats;
    mutable std::mutex m_mutex;
};

// ============================================================================
// HttpChainStep — holds a future alive until the LLM response completes.
// This is the critical fix: Qt's signal-slot auto-managed reply lifetime;
// STL futures require explicit retention or they destruct-cancel the async op.
// ============================================================================
class HttpChainStep {
public:
    enum class State { Idle, Running, Completed, Failed, Cancelled };

    HttpChainStep() = default;
    explicit HttpChainStep(const std::string& name) : m_name(name) {}
    ~HttpChainStep();

    // Start an async LLM call — future is retained internally
    void start(const std::string& url,
               const std::string& jsonBody,
               int timeoutMs = 30000);

    // Poll for completion (non-blocking)
    bool poll();

    // Block until done (with optional timeout)
    bool waitFor(int timeoutMs = 0);

    // Get result (valid only after poll() returns true or waitFor succeeds)
    const HttpResponse& result() const { return m_result; }
    State state() const { return m_state; }
    const std::string& name() const { return m_name; }

    // Cancel this step
    void cancel();

private:
    std::string              m_name;
    std::future<HttpResponse> m_future;   // MUST stay alive until done
    HttpResponse             m_result;
    State                    m_state = State::Idle;
};

// ============================================================================
// HttpChainExecutor — runs a sequence of HttpChainSteps, holding all futures alive
// ============================================================================
class HttpChainExecutor {
public:
    HttpChainExecutor() = default;
    ~HttpChainExecutor();

    // Add a step to the chain
    void addStep(const std::string& name,
                 const std::string& url,
                 const std::string& jsonBody,
                 int timeoutMs = 30000);

    // Execute all steps sequentially (blocking)
    // Each step waits for the previous to complete and can use its result.
    bool executeAll(std::function<std::string(const std::string& prevResult,
                                              int stepIndex)> bodyBuilder = nullptr);

    // Execute all steps and collect results
    struct StepResult {
        std::string  name;
        HttpResponse response;
        int          index = 0;
    };
    const std::vector<StepResult>& results() const { return m_results; }

    // Cancel
    void cancel();
    bool isCancelled() const { return m_cancelled.load(); }

    // Callbacks
    std::function<void(int stepIndex, const std::string& name)> onStepStarted;
    std::function<void(int stepIndex, const StepResult&)>       onStepCompleted;

private:
    struct PendingStep {
        std::string name;
        std::string url;
        std::string jsonBody;
        int         timeoutMs;
    };
    std::vector<PendingStep> m_pending;
    std::vector<StepResult>  m_results;
    std::atomic<bool>        m_cancelled{false};
};
