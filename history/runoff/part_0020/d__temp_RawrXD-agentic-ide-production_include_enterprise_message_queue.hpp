/**
 * @file message_queue.hpp
 * @brief Enterprise Message Queue System
 * 
 * Features:
 * - In-process async message queue
 * - Pub/Sub pattern
 * - Dead letter queue
 * - Message prioritization
 * - Retry with backoff
 * - Thread-safe operations
 */

#pragma once

#include <string>
#include <queue>
#include <unordered_map>
#include <functional>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <chrono>
#include <vector>
#include <optional>
#include <any>

namespace enterprise {

// =============================================================================
// Message Types
// =============================================================================

enum class MessagePriority {
    LOW = 0,
    NORMAL = 1,
    HIGH = 2,
    CRITICAL = 3
};

enum class MessageStatus {
    PENDING,
    PROCESSING,
    COMPLETED,
    FAILED,
    DEAD_LETTER
};

struct Message {
    std::string id;
    std::string topic;
    std::string payload;
    MessagePriority priority = MessagePriority::NORMAL;
    MessageStatus status = MessageStatus::PENDING;
    std::chrono::steady_clock::time_point createdAt;
    std::chrono::steady_clock::time_point processedAt;
    int retryCount = 0;
    int maxRetries = 3;
    std::string correlationId;
    std::unordered_map<std::string, std::string> headers;
    std::string errorMessage;
    
    // For priority queue ordering
    bool operator<(const Message& other) const {
        return priority < other.priority;  // Higher priority first
    }
};

// =============================================================================
// Message Handler
// =============================================================================

using MessageHandler = std::function<bool(const Message&)>;
using ErrorHandler = std::function<void(const Message&, const std::string& error)>;

struct Subscription {
    std::string id;
    std::string topic;
    MessageHandler handler;
    ErrorHandler errorHandler;
    bool active = true;
};

// =============================================================================
// Queue Configuration
// =============================================================================

struct QueueConfig {
    size_t maxQueueSize = 100000;
    int defaultMaxRetries = 3;
    int retryDelayMs = 1000;
    int retryBackoffMultiplier = 2;
    int maxRetryDelayMs = 60000;
    bool enableDeadLetter = true;
    int workerThreads = 4;
    bool persistMessages = false;
    std::string persistencePath;
};

// =============================================================================
// Queue Statistics
// =============================================================================

struct QueueStats {
    std::atomic<long long> messagesEnqueued{0};
    std::atomic<long long> messagesProcessed{0};
    std::atomic<long long> messagesFailed{0};
    std::atomic<long long> messagesRetried{0};
    std::atomic<long long> messagesDeadLettered{0};
    std::atomic<size_t> currentQueueSize{0};
    std::atomic<size_t> deadLetterQueueSize{0};

    QueueStats() = default;
    QueueStats(const QueueStats& other) {
        messagesEnqueued.store(other.messagesEnqueued.load());
        messagesProcessed.store(other.messagesProcessed.load());
        messagesFailed.store(other.messagesFailed.load());
        messagesRetried.store(other.messagesRetried.load());
        messagesDeadLettered.store(other.messagesDeadLettered.load());
        currentQueueSize.store(other.currentQueueSize.load());
        deadLetterQueueSize.store(other.deadLetterQueueSize.load());
    }
    QueueStats& operator=(const QueueStats& other) {
        if (this != &other) {
            messagesEnqueued.store(other.messagesEnqueued.load());
            messagesProcessed.store(other.messagesProcessed.load());
            messagesFailed.store(other.messagesFailed.load());
            messagesRetried.store(other.messagesRetried.load());
            messagesDeadLettered.store(other.messagesDeadLettered.load());
            currentQueueSize.store(other.currentQueueSize.load());
            deadLetterQueueSize.store(other.deadLetterQueueSize.load());
        }
        return *this;
    }
};

// =============================================================================
// Topic Queue
// =============================================================================

class TopicQueue {
public:
    explicit TopicQueue(const std::string& topic);
    
    void enqueue(Message msg);
    std::optional<Message> dequeue();
    std::optional<Message> peek() const;
    
    bool isEmpty() const;
    size_t size() const;
    void clear();
    
    std::string getTopic() const { return m_topic; }
    
private:
    std::string m_topic;
    std::priority_queue<Message> m_queue;
    mutable std::mutex m_mutex;
};

// =============================================================================
// Dead Letter Queue
// =============================================================================

class DeadLetterQueue {
public:
    void add(Message msg, const std::string& reason);
    std::optional<Message> pop();
    std::vector<Message> getAll() const;
    
    size_t size() const;
    void clear();
    
    // Requeue failed messages for retry
    std::vector<Message> requeueAll();
    bool requeue(const std::string& messageId);
    
private:
    std::queue<Message> m_queue;
    mutable std::mutex m_mutex;
};

// =============================================================================
// Message Queue System
// =============================================================================

class MessageQueue {
public:
    static MessageQueue& instance();
    
    // Configuration
    void configure(const QueueConfig& config);
    QueueConfig getConfig() const { return m_config; }
    
    // Lifecycle
    void start();
    void stop();
    bool isRunning() const { return m_running; }
    
    // Publishing
    std::string publish(const std::string& topic, const std::string& payload,
                       MessagePriority priority = MessagePriority::NORMAL);
    std::string publish(Message msg);
    
    // Bulk publishing
    std::vector<std::string> publishBatch(const std::string& topic, 
                                          const std::vector<std::string>& payloads);
    
    // Subscribing
    std::string subscribe(const std::string& topic, MessageHandler handler);
    std::string subscribe(const std::string& topic, MessageHandler handler, 
                          ErrorHandler errorHandler);
    bool unsubscribe(const std::string& subscriptionId);
    
    // Topic management
    void createTopic(const std::string& topic);
    void deleteTopic(const std::string& topic);
    std::vector<std::string> listTopics() const;
    bool topicExists(const std::string& topic) const;
    
    // Queue inspection
    size_t getQueueSize(const std::string& topic) const;
    size_t getTotalQueueSize() const;
    
    // Dead letter queue
    DeadLetterQueue& deadLetterQueue() { return m_deadLetterQueue; }
    const DeadLetterQueue& deadLetterQueue() const { return m_deadLetterQueue; }
    
    // Statistics
    QueueStats getStats() const {
        QueueStats s;
        s.messagesEnqueued.store(m_stats.messagesEnqueued.load());
        s.messagesProcessed.store(m_stats.messagesProcessed.load());
        s.messagesFailed.store(m_stats.messagesFailed.load());
        s.messagesRetried.store(m_stats.messagesRetried.load());
        s.messagesDeadLettered.store(m_stats.messagesDeadLettered.load());
        s.currentQueueSize.store(m_stats.currentQueueSize.load());
        s.deadLetterQueueSize.store(m_stats.deadLetterQueueSize.load());
        return s;
    }
    void resetStats();
    
    // Synchronous processing (for testing)
    bool processNextMessage(const std::string& topic);
    void processAllPending();
    
private:
    MessageQueue();
    ~MessageQueue();
    
    MessageQueue(const MessageQueue&) = delete;
    MessageQueue& operator=(const MessageQueue&) = delete;
    
    void workerLoop();
    bool processMessage(Message& msg);
    void handleFailedMessage(Message& msg, const std::string& error);
    std::string generateMessageId();
    std::string generateSubscriptionId();
    
    QueueConfig m_config;
    std::atomic<bool> m_running{false};
    
    // Topics and queues
    std::unordered_map<std::string, std::unique_ptr<TopicQueue>> m_topics;
    mutable std::mutex m_topicsMutex;
    
    // Subscriptions
    std::unordered_map<std::string, Subscription> m_subscriptions;
    std::unordered_map<std::string, std::vector<std::string>> m_topicSubscriptions;  // topic -> subscription ids
    mutable std::mutex m_subscriptionsMutex;
    
    // Dead letter queue
    DeadLetterQueue m_deadLetterQueue;
    
    // Worker threads
    std::vector<std::thread> m_workers;
    std::condition_variable m_cv;
    std::mutex m_cvMutex;
    
    // Statistics
    QueueStats m_stats;
    
    // Message ID generation
    std::atomic<uint64_t> m_messageCounter{0};
    std::atomic<uint64_t> m_subscriptionCounter{0};
};

// =============================================================================
// Event Bus (Simplified Pub/Sub)
// =============================================================================

class EventBus {
public:
    static EventBus& instance();
    
    using EventHandler = std::function<void(const std::string& eventType, const std::any& data)>;
    
    // Subscribe to event type
    std::string on(const std::string& eventType, EventHandler handler);
    
    // Subscribe to event once
    std::string once(const std::string& eventType, EventHandler handler);
    
    // Unsubscribe
    void off(const std::string& subscriptionId);
    void offAll(const std::string& eventType);
    
    // Emit event
    void emit(const std::string& eventType, const std::any& data = {});
    
    // Async emit
    void emitAsync(const std::string& eventType, const std::any& data = {});
    
private:
    EventBus() = default;
    
    struct EventSubscription {
        std::string id;
        EventHandler handler;
        bool once = false;
    };
    
    std::unordered_map<std::string, std::vector<EventSubscription>> m_handlers;
    std::mutex m_mutex;
    std::atomic<uint64_t> m_counter{0};
};

// =============================================================================
// Request/Reply Pattern
// =============================================================================

class RequestReply {
public:
    static RequestReply& instance();
    
    using RequestHandler = std::function<std::string(const std::string& request)>;
    
    // Register handler for request type
    void registerHandler(const std::string& requestType, RequestHandler handler);
    void unregisterHandler(const std::string& requestType);
    
    // Send request and wait for reply (synchronous)
    std::optional<std::string> request(const std::string& requestType, 
                                        const std::string& payload,
                                        int timeoutMs = 5000);
    
    // Send request with callback (asynchronous)
    void requestAsync(const std::string& requestType,
                      const std::string& payload,
                      std::function<void(std::optional<std::string>)> callback);
    
private:
    RequestReply() = default;
    
    std::unordered_map<std::string, RequestHandler> m_handlers;
    std::mutex m_mutex;
};

} // namespace enterprise
