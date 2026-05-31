// ============================================================================
// async_message_queue.h — Non-Blocking Message Queue for Chat
// ============================================================================
// Priority-based async message queue with streaming progress callbacks.
// Enables non-blocking chat: user sends message, focus returns to editor,
// AI processes in background, response streams into chat pane.
//
// Pattern: Thread-safe producer/consumer with event callbacks.
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <thread>
#include <functional>
#include <chrono>
#include <unordered_map>
#include <unordered_set>
#include <memory>

namespace RawrXD {
namespace UI {

// ============================================================================
// Message Priority
// ============================================================================

enum class MessagePriority : uint8_t {
    NORMAL  = 0,
    HIGH    = 1,
    URGENT  = 2
};

// ============================================================================
// Message Status
// ============================================================================

enum class MessageStatus : uint8_t {
    PENDING     = 0,
    PROCESSING  = 1,
    COMPLETED   = 2,
    ERROR       = 3,
    CANCELLED   = 4
};

const char* statusToString(MessageStatus s);
const char* priorityToString(MessagePriority p);

// ============================================================================
// Code Context — editor state captured at send time
// ============================================================================

struct CodeContext {
    std::string activeFile;
    std::string selectedText;
    std::string cursorPosition;   // "line:col"
    std::vector<std::string> openFiles;
    std::string language;

    bool isEmpty() const {
        return activeFile.empty() && selectedText.empty();
    }
};

// ============================================================================
// Queued Message
// ============================================================================

struct QueuedMessage {
    std::string       id;
    std::string       content;
    uint64_t          timestampMs;
    MessageStatus     status;
    MessagePriority   priority;
    CodeContext       context;
    std::string       response;
    float             progress;     // 0.0f - 100.0f
    std::string       errorMessage;
    uint32_t          promptTokens;
    uint32_t          completionTokens;
    float             latencyMs;

    QueuedMessage()
        : timestampMs(0)
        , status(MessageStatus::PENDING)
        , priority(MessagePriority::NORMAL)
        , progress(0.0f)
        , promptTokens(0)
        , completionTokens(0)
        , latencyMs(0.0f)
    {}
};

// ============================================================================
// Event Types
// ============================================================================

using MessageCallback    = std::function<void(const QueuedMessage&)>;
using ProgressCallback   = std::function<void(const QueuedMessage&)>;
using ErrorCallback      = std::function<void(const QueuedMessage&, const std::string&)>;
using StreamChunkCallback = std::function<void(const std::string& messageId, const std::string& chunk)>;

// ============================================================================
// Async Message Queue
// ============================================================================

class AsyncMessageQueue {
public:
    struct Config {
        uint32_t maxConcurrent = 3;
        uint32_t maxQueueSize  = 100;
        uint32_t workerThreads = 2;
        bool     enableStreaming = true;
    };

    explicit AsyncMessageQueue(const Config& config = {});
    ~AsyncMessageQueue();

    // ---- Enqueue / Dequeue ----

    // Enqueue a message. Returns message ID. Non-blocking.
    std::string enqueue(const std::string& content,
                        MessagePriority priority = MessagePriority::NORMAL,
                        const CodeContext& context = {});

    // Cancel a pending or processing message
    bool cancel(const std::string& messageId);

    // Reprioritize a queued message (move to front if urgent)
    bool prioritize(const std::string& messageId);

    // ---- Queries ----

    QueuedMessage* getMessage(const std::string& messageId);
    std::vector<QueuedMessage> getBacklog() const;
    std::vector<QueuedMessage> getActive() const;
    std::vector<QueuedMessage> getCompleted() const;
    size_t getPendingCount() const;
    size_t getActiveCount() const;

    // ---- Event Subscriptions ----

    void onQueued(MessageCallback cb);
    void onProcessing(MessageCallback cb);
    void onProgress(ProgressCallback cb);
    void onCompleted(MessageCallback cb);
    void onError(ErrorCallback cb);
    void onStreamChunk(StreamChunkCallback cb);

    // ---- Control ----

    void start();
    void stop();
    void clear();
    bool isRunning() const;

    // ---- Statistics ----

    struct Stats {
        uint64_t totalEnqueued;
        uint64_t totalCompleted;
        uint64_t totalErrors;
        uint64_t totalCancelled;
        float    avgLatencyMs;
        size_t   currentBacklog;
        size_t   currentActive;
    };
    Stats getStats() const;

private:
    Config config_;
    std::atomic<bool> running_{false};
    std::atomic<bool> shutdown_{false};

    // Queue storage
    std::deque<QueuedMessage> queue_;
    std::unordered_map<std::string, QueuedMessage> activeMessages_;
    std::unordered_map<std::string, QueuedMessage> completedMessages_;
    std::unordered_set<std::string> cancelledIds_;

    mutable std::mutex queueMutex_;
    std::condition_variable queueCv_;

    // Worker threads
    std::vector<std::thread> workers_;

    // Callbacks
    std::vector<MessageCallback>    onQueuedCbs_;
    std::vector<MessageCallback>    onProcessingCbs_;
    std::vector<ProgressCallback>   onProgressCbs_;
    std::vector<MessageCallback>    onCompletedCbs_;
    std::vector<ErrorCallback>      onErrorCbs_;
    std::vector<StreamChunkCallback> onStreamChunkCbs_;
    mutable std::mutex callbackMutex_;

    // Statistics
    std::atomic<uint64_t> totalEnqueued_{0};
    std::atomic<uint64_t> totalCompleted_{0};
    std::atomic<uint64_t> totalErrors_{0};
    std::atomic<uint64_t> totalCancelled_{0};
    std::atomic<uint64_t> totalLatencyMs_{0};

    // Internal
    void workerLoop();
    void processMessage(QueuedMessage& msg);
    void streamResponse(QueuedMessage& msg);
    std::string generateId() const;

    // Event dispatch
    void dispatchQueued(const QueuedMessage& msg);
    void dispatchProcessing(const QueuedMessage& msg);
    void dispatchProgress(const QueuedMessage& msg);
    void dispatchCompleted(const QueuedMessage& msg);
    void dispatchError(const QueuedMessage& msg, const std::string& error);
    void dispatchStreamChunk(const std::string& msgId, const std::string& chunk);
};

} // namespace UI
} // namespace RawrXD
