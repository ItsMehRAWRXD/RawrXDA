// ============================================================================
// token_router.h — Multi-Sink Token Router with Adaptive Backpressure
// ============================================================================
// Routes generated tokens to one or more output sinks (SSE, WebSocket,
// memory buffer, file log) with per-sink flow control.
//
// Backpressure model:
//   Each sink reports its available capacity (tokens it can accept).
//   When the slowest sink's capacity drops below the low-water mark,
//   the router signals the generator to pause.  When capacity rises
//   above the high-water mark, generation resumes.
//
// Token deduplication:
//   The router assigns monotonic sequence numbers; sinks that
//   reconnect can request replay from a given seqno.
//
// Threading: single producer (inference thread) → fan-out to N sink threads.
// ============================================================================
#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace rawrxd {

// ---------------------------------------------------------------------------
// Backpressure signal — returned to the generator
// ---------------------------------------------------------------------------
enum class FlowSignal : uint8_t {
    Continue = 0,   // keep generating
    Pause    = 1,   // slow down / yield
    Abort    = 2,   // cancel generation (all sinks disconnected)
};

// ---------------------------------------------------------------------------
// TokenEvent — a single routed token
// ---------------------------------------------------------------------------
struct TokenEvent {
    uint64_t    seqNo;
    std::string token;
    bool        isEOS;          // end of stream
    std::chrono::steady_clock::time_point timestamp;
};

// ---------------------------------------------------------------------------
// TokenSink — abstract interface for an output consumer
// ---------------------------------------------------------------------------
class TokenSink {
public:
    virtual ~TokenSink() = default;

    // Unique name for logging / debugging
    virtual const char* name() const = 0;

    // Deliver a token.  Returns false if the sink is dead/disconnected.
    virtual bool deliver(const TokenEvent& ev) = 0;

    // How many more tokens can this sink accept right now?
    virtual uint32_t availableCapacity() const = 0;

    // Is the sink still alive?
    virtual bool isConnected() const = 0;
};

// ---------------------------------------------------------------------------
// RouterConfig
// ---------------------------------------------------------------------------
struct RouterConfig {
    uint32_t replayBufferSize   = 512;      // ring buffer for replay-on-reconnect
    uint32_t lowWaterMark       = 4;        // pause when slowest sink ≤ this
    uint32_t highWaterMark      = 32;       // resume when slowest sink ≥ this
    bool     dropDisconnected   = true;     // auto-remove dead sinks
};

// ---------------------------------------------------------------------------
// RouterStats
// ---------------------------------------------------------------------------
struct RouterStats {
    uint64_t tokensRouted;
    uint64_t tokensPaused;         // pause signals issued
    uint64_t tokensDropped;        // tokens that couldn't be delivered
    uint32_t activeSinks;
    double   avgDeliveryUs;        // average per-token delivery latency
};

// ---------------------------------------------------------------------------
// TokenRouter
// ---------------------------------------------------------------------------
class TokenRouter {
public:
    explicit TokenRouter(const RouterConfig& cfg = {});
    ~TokenRouter();

    // Register / unregister sinks (thread-safe)
    void addSink(std::shared_ptr<TokenSink> sink);
    void removeSink(const char* sinkName);

    // Route a token to all sinks.  Returns a flow control signal.
    FlowSignal route(const std::string& token, bool isEOS = false);

    // Replay tokens from seqNo to current for a reconnecting sink.
    uint32_t replay(TokenSink* sink, uint64_t fromSeqNo) const;

    // Stats
    RouterStats getStats() const;

    // Block until backpressure clears (for generators that prefer blocking).
    // Returns false if all sinks disconnected (abort).
    bool waitForCapacity(uint32_t timeoutMs = 1000);

private:
    void pruneDeadSinks();

    RouterConfig                             m_cfg;
    mutable std::mutex                       m_mu;
    std::condition_variable                  m_capacityCv;

    std::vector<std::shared_ptr<TokenSink>>  m_sinks;
    std::atomic<uint64_t>                    m_seqNo{0};

    // Replay ring buffer
    std::vector<TokenEvent>                  m_replayBuf;
    uint64_t                                 m_replayHead = 0; // oldest seqNo in ring

    // Stats
    std::atomic<uint64_t>                    m_tokensRouted{0};
    std::atomic<uint64_t>                    m_tokensPaused{0};
    std::atomic<uint64_t>                    m_tokensDropped{0};
    std::atomic<uint64_t>                    m_deliveryUsSum{0};
};

// ============================================================================
// Concrete sinks
// ============================================================================

// MemoryBufferSink — collects tokens into a string (for in-process consumers)
class MemoryBufferSink : public TokenSink {
public:
    explicit MemoryBufferSink(uint32_t maxTokens = 8192);
    const char* name() const override { return "MemoryBuffer"; }
    bool deliver(const TokenEvent& ev) override;
    uint32_t availableCapacity() const override;
    bool isConnected() const override { return m_connected.load(std::memory_order_relaxed); }

    std::string getBuffer() const;
    void disconnect();

private:
    mutable std::mutex    m_mu;
    std::string           m_buffer;
    uint32_t              m_maxTokens;
    uint32_t              m_tokenCount = 0;
    std::atomic<bool>     m_connected{true};
};

// CallbackSink — wraps a std::function callback
class CallbackSink : public TokenSink {
public:
    using Callback = std::function<bool(const std::string& token, bool eos)>;

    CallbackSink(const char* name, Callback cb, uint32_t capacity = 64);
    const char* name() const override { return m_name; }
    bool deliver(const TokenEvent& ev) override;
    uint32_t availableCapacity() const override;
    bool isConnected() const override { return m_connected.load(std::memory_order_relaxed); }

private:
    const char*           m_name;
    Callback              m_cb;
    uint32_t              m_capacity;
    std::atomic<uint32_t> m_pending{0};
    std::atomic<bool>     m_connected{true};
};

// WebSocketSink — delivers tokens over a WebSocket connection.
//
// Frame format (per RFC 6455):
//   - Text frame (opcode 0x1) containing JSON: {"seqNo":N,"token":"...","eos":false}
//   - On EOS, sends: {"seqNo":N,"token":"","eos":true}
//
// Connection lifecycle:
//   - Connects on demand when first deliver() is called
//   - Auto-reconnects with exponential backoff on transient failure
//   - Disconnected = isConnected() returns false, backpressure applies
//
class WebSocketSink : public TokenSink {
public:
    using SendFn = std::function<bool(const std::string& payload)>;

    // target_url: e.g. "ws://localhost:8080/stream/chat-123"
    WebSocketSink(const char* name, const std::string& target_url,
                  uint32_t bufferCapacity = 256, uint32_t maxRetries = 5,
                  SendFn send_fn = nullptr);
    ~WebSocketSink();

    const char* name() const override { return m_name; }
    bool deliver(const TokenEvent& ev) override;
    uint32_t availableCapacity() const override;
    bool isConnected() const override;

    // Stats
    uint64_t framesDropped() const { return m_framesDropped.load(std::memory_order_relaxed); }
    void resetStats() { m_framesDropped.store(0, std::memory_order_relaxed); }

private:
    void tryConnect();
    void disconnect();

    const char*           m_name;
    std::string           m_targetUrl;
    uint32_t              m_bufferCapacity;
    uint32_t              m_maxRetries;
    SendFn                m_sendFn;

    mutable std::mutex    m_mu;
    std::atomic<bool>     m_connected{false};
    std::atomic<uint64_t> m_framesDropped{0};

    // Opaque pointer to actual WebSocket handle (void* to avoid external library deps)
    void*                 m_wsHandle = nullptr;
};

// ServerSentEventsSink — delivers tokens as Server-Sent Events (SSE).
//
// Format (text/event-stream):
//   data: {"seqNo":N,"token":"...","eos":false}
//   \n\n
//
// Used for HTTP long-polling / streaming:
//   - Compatible with EventSource API in browsers
//   - One line per token (JSON format)
//   - On EOS, sends a :done marker or keeps-alive comment
//
class ServerSentEventsSink : public TokenSink {
public:
    // write_fn: lambda/callback that sends data bytes to the HTTP response stream
    //   Returns true on success, false on network error / file closed
    using WriteFn = std::function<bool(const std::string& data)>;

    ServerSentEventsSink(const char* name, WriteFn write_fn, uint32_t bufferCapacity = 256);
    const char* name() const override { return m_name; }
    bool deliver(const TokenEvent& ev) override;
    uint32_t availableCapacity() const override;
    bool isConnected() const override { return m_connected.load(std::memory_order_relaxed); }

    // Mark the stream as closed (e.g., when HTTP connection drops)
    void markClosed() { m_connected.store(false, std::memory_order_relaxed); }

private:
    const char*           m_name;
    WriteFn               m_writeFn;
    uint32_t              m_bufferCapacity;
    std::atomic<uint32_t> m_buffered{0};
    std::atomic<bool>     m_connected{true};
};

} // namespace rawrxd
