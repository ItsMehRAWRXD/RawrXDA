/**
 * @file message_queue.cpp
 * @brief Enterprise Message Queue System Implementation
 */

#include "enterprise/message_queue.hpp"
#include <algorithm>
#include <random>
#include <sstream>
#include <iomanip>

namespace enterprise {

// =============================================================================
// TopicQueue Implementation
// =============================================================================

TopicQueue::TopicQueue(const std::string& topic) : m_topic(topic) {}

void TopicQueue::enqueue(Message msg) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_queue.push(std::move(msg));
}

std::optional<Message> TopicQueue::dequeue() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_queue.empty()) {
        return std::nullopt;
    }
    
    Message msg = m_queue.top();
    m_queue.pop();
    return msg;
}

std::optional<Message> TopicQueue::peek() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_queue.empty()) {
        return std::nullopt;
    }
    return m_queue.top();
}

bool TopicQueue::isEmpty() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_queue.empty();
}

size_t TopicQueue::size() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_queue.size();
}

void TopicQueue::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    while (!m_queue.empty()) {
        m_queue.pop();
    }
}

// =============================================================================
// DeadLetterQueue Implementation
// =============================================================================

void DeadLetterQueue::add(Message msg, const std::string& reason) {
    std::lock_guard<std::mutex> lock(m_mutex);
    msg.status = MessageStatus::DEAD_LETTER;
    msg.errorMessage = reason;
    m_queue.push(std::move(msg));
}

std::optional<Message> DeadLetterQueue::pop() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_queue.empty()) {
        return std::nullopt;
    }
    
    Message msg = m_queue.front();
    m_queue.pop();
    return msg;
}

std::vector<Message> DeadLetterQueue::getAll() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<Message> result;
    
    std::queue<Message> temp = m_queue;
    while (!temp.empty()) {
        result.push_back(temp.front());
        temp.pop();
    }
    
    return result;
}

size_t DeadLetterQueue::size() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_queue.size();
}

void DeadLetterQueue::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    while (!m_queue.empty()) {
        m_queue.pop();
    }
}

std::vector<Message> DeadLetterQueue::requeueAll() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<Message> messages;
    
    while (!m_queue.empty()) {
        Message msg = m_queue.front();
        m_queue.pop();
        msg.status = MessageStatus::PENDING;
        msg.retryCount = 0;
        msg.errorMessage.clear();
        messages.push_back(std::move(msg));
    }
    
    return messages;
}

bool DeadLetterQueue::requeue(const std::string& messageId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::queue<Message> temp;
    bool found = false;
    
    while (!m_queue.empty()) {
        Message msg = m_queue.front();
        m_queue.pop();
        
        if (msg.id == messageId) {
            found = true;
            // Message will be requeued by caller
        } else {
            temp.push(std::move(msg));
        }
    }
    
    m_queue = std::move(temp);
    return found;
}

// =============================================================================
// MessageQueue Implementation
// =============================================================================

MessageQueue& MessageQueue::instance() {
    static MessageQueue instance;
    return instance;
}

MessageQueue::MessageQueue() {
    m_config = QueueConfig{};
}

MessageQueue::~MessageQueue() {
    stop();
}

void MessageQueue::configure(const QueueConfig& config) {
    m_config = config;
}

void MessageQueue::start() {
    if (m_running) return;
    
    m_running = true;
    
    for (int i = 0; i < m_config.workerThreads; ++i) {
        m_workers.emplace_back(&MessageQueue::workerLoop, this);
    }
}

void MessageQueue::stop() {
    if (!m_running) return;
    
    m_running = false;
    m_cv.notify_all();
    
    for (auto& worker : m_workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    m_workers.clear();
}

std::string MessageQueue::publish(const std::string& topic, const std::string& payload,
                                   MessagePriority priority) {
    Message msg;
    msg.id = generateMessageId();
    msg.topic = topic;
    msg.payload = payload;
    msg.priority = priority;
    msg.createdAt = std::chrono::steady_clock::now();
    msg.maxRetries = m_config.defaultMaxRetries;
    
    return publish(std::move(msg));
}

std::string MessageQueue::publish(Message msg) {
    if (msg.id.empty()) {
        msg.id = generateMessageId();
    }
    if (msg.createdAt == std::chrono::steady_clock::time_point{}) {
        msg.createdAt = std::chrono::steady_clock::now();
    }
    
    std::string messageId = msg.id;
    std::string topic = msg.topic;
    
    {
        std::lock_guard<std::mutex> lock(m_topicsMutex);
        
        // Create topic if it doesn't exist
        if (m_topics.find(topic) == m_topics.end()) {
            m_topics[topic] = std::make_unique<TopicQueue>(topic);
        }
        
        // Check queue size limit
        if (m_topics[topic]->size() >= m_config.maxQueueSize) {
            // Queue is full
            return "";
        }
        
        m_topics[topic]->enqueue(std::move(msg));
    }
    
    m_stats.messagesEnqueued++;
    m_stats.currentQueueSize++;
    
    // Notify workers
    m_cv.notify_one();
    
    return messageId;
}

std::vector<std::string> MessageQueue::publishBatch(const std::string& topic,
                                                     const std::vector<std::string>& payloads) {
    std::vector<std::string> messageIds;
    messageIds.reserve(payloads.size());
    
    for (const auto& payload : payloads) {
        std::string id = publish(topic, payload);
        if (!id.empty()) {
            messageIds.push_back(id);
        }
    }
    
    return messageIds;
}

std::string MessageQueue::subscribe(const std::string& topic, MessageHandler handler) {
    return subscribe(topic, handler, nullptr);
}

std::string MessageQueue::subscribe(const std::string& topic, MessageHandler handler,
                                     ErrorHandler errorHandler) {
    std::lock_guard<std::mutex> lock(m_subscriptionsMutex);
    
    std::string subId = generateSubscriptionId();
    
    Subscription sub;
    sub.id = subId;
    sub.topic = topic;
    sub.handler = handler;
    sub.errorHandler = errorHandler;
    
    m_subscriptions[subId] = std::move(sub);
    m_topicSubscriptions[topic].push_back(subId);
    
    // Ensure topic exists
    {
        std::lock_guard<std::mutex> topicLock(m_topicsMutex);
        if (m_topics.find(topic) == m_topics.end()) {
            m_topics[topic] = std::make_unique<TopicQueue>(topic);
        }
    }
    
    return subId;
}

bool MessageQueue::unsubscribe(const std::string& subscriptionId) {
    std::lock_guard<std::mutex> lock(m_subscriptionsMutex);
    
    auto it = m_subscriptions.find(subscriptionId);
    if (it == m_subscriptions.end()) {
        return false;
    }
    
    std::string topic = it->second.topic;
    m_subscriptions.erase(it);
    
    // Remove from topic subscriptions
    auto& topicSubs = m_topicSubscriptions[topic];
    topicSubs.erase(std::remove(topicSubs.begin(), topicSubs.end(), subscriptionId),
                    topicSubs.end());
    
    return true;
}

void MessageQueue::createTopic(const std::string& topic) {
    std::lock_guard<std::mutex> lock(m_topicsMutex);
    
    if (m_topics.find(topic) == m_topics.end()) {
        m_topics[topic] = std::make_unique<TopicQueue>(topic);
    }
}

void MessageQueue::deleteTopic(const std::string& topic) {
    std::lock_guard<std::mutex> lock(m_topicsMutex);
    m_topics.erase(topic);
}

std::vector<std::string> MessageQueue::listTopics() const {
    std::lock_guard<std::mutex> lock(m_topicsMutex);
    
    std::vector<std::string> topics;
    topics.reserve(m_topics.size());
    
    for (const auto& [name, _] : m_topics) {
        topics.push_back(name);
    }
    
    return topics;
}

bool MessageQueue::topicExists(const std::string& topic) const {
    std::lock_guard<std::mutex> lock(m_topicsMutex);
    return m_topics.find(topic) != m_topics.end();
}

size_t MessageQueue::getQueueSize(const std::string& topic) const {
    std::lock_guard<std::mutex> lock(m_topicsMutex);
    
    auto it = m_topics.find(topic);
    if (it == m_topics.end()) {
        return 0;
    }
    
    return it->second->size();
}

size_t MessageQueue::getTotalQueueSize() const {
    std::lock_guard<std::mutex> lock(m_topicsMutex);
    
    size_t total = 0;
    for (const auto& [_, queue] : m_topics) {
        total += queue->size();
    }
    
    return total;
}

void MessageQueue::resetStats() {
    m_stats.messagesEnqueued = 0;
    m_stats.messagesProcessed = 0;
    m_stats.messagesFailed = 0;
    m_stats.messagesRetried = 0;
    m_stats.messagesDeadLettered = 0;
    m_stats.currentQueueSize = 0;
    m_stats.deadLetterQueueSize = 0;
}

bool MessageQueue::processNextMessage(const std::string& topic) {
    std::optional<Message> msgOpt;
    
    {
        std::lock_guard<std::mutex> lock(m_topicsMutex);
        auto it = m_topics.find(topic);
        if (it == m_topics.end()) {
            return false;
        }
        
        msgOpt = it->second->dequeue();
    }
    
    if (!msgOpt) {
        return false;
    }
    
    Message msg = std::move(*msgOpt);
    return processMessage(msg);
}

void MessageQueue::processAllPending() {
    std::vector<std::string> topics;
    
    {
        std::lock_guard<std::mutex> lock(m_topicsMutex);
        for (const auto& [name, _] : m_topics) {
            topics.push_back(name);
        }
    }
    
    for (const auto& topic : topics) {
        while (processNextMessage(topic)) {
            // Continue processing
        }
    }
}

void MessageQueue::workerLoop() {
    while (m_running) {
        std::optional<Message> msgOpt;
        std::string topic;
        
        {
            std::unique_lock<std::mutex> lock(m_cvMutex);
            m_cv.wait_for(lock, std::chrono::milliseconds(100), [this]() {
                return !m_running || getTotalQueueSize() > 0;
            });
        }
        
        if (!m_running) break;
        
        // Find a message to process
        {
            std::lock_guard<std::mutex> lock(m_topicsMutex);
            for (auto& [topicName, queue] : m_topics) {
                if (!queue->isEmpty()) {
                    msgOpt = queue->dequeue();
                    if (msgOpt) {
                        topic = topicName;
                        break;
                    }
                }
            }
        }
        
        if (msgOpt) {
            m_stats.currentQueueSize--;
            Message msg = std::move(*msgOpt);
            processMessage(msg);
        }
    }
}

bool MessageQueue::processMessage(Message& msg) {
    msg.status = MessageStatus::PROCESSING;
    msg.processedAt = std::chrono::steady_clock::now();
    
    // Get subscribers
    std::vector<Subscription> subscribers;
    {
        std::lock_guard<std::mutex> lock(m_subscriptionsMutex);
        auto it = m_topicSubscriptions.find(msg.topic);
        if (it != m_topicSubscriptions.end()) {
            for (const auto& subId : it->second) {
                auto subIt = m_subscriptions.find(subId);
                if (subIt != m_subscriptions.end() && subIt->second.active) {
                    subscribers.push_back(subIt->second);
                }
            }
        }
    }
    
    if (subscribers.empty()) {
        // No subscribers, message processed (dropped)
        msg.status = MessageStatus::COMPLETED;
        m_stats.messagesProcessed++;
        return true;
    }
    
    // Process with each subscriber
    bool allSucceeded = true;
    std::string lastError;
    
    for (const auto& sub : subscribers) {
        try {
            bool result = sub.handler(msg);
            if (!result) {
                allSucceeded = false;
                lastError = "Handler returned false";
            }
        } catch (const std::exception& e) {
            allSucceeded = false;
            lastError = e.what();
            
            if (sub.errorHandler) {
                sub.errorHandler(msg, e.what());
            }
        }
    }
    
    if (allSucceeded) {
        msg.status = MessageStatus::COMPLETED;
        m_stats.messagesProcessed++;
        return true;
    } else {
        handleFailedMessage(msg, lastError);
        return false;
    }
}

void MessageQueue::handleFailedMessage(Message& msg, const std::string& error) {
    msg.retryCount++;
    msg.errorMessage = error;
    
    if (msg.retryCount <= msg.maxRetries) {
        // Retry
        msg.status = MessageStatus::PENDING;
        m_stats.messagesRetried++;
        
        // Calculate backoff delay
        int delay = m_config.retryDelayMs;
        for (int i = 1; i < msg.retryCount; ++i) {
            delay *= m_config.retryBackoffMultiplier;
        }
        delay = std::min(delay, m_config.maxRetryDelayMs);
        
        // Delay then requeue
        std::this_thread::sleep_for(std::chrono::milliseconds(delay));
        publish(std::move(msg));
        
    } else {
        // Send to dead letter queue
        msg.status = MessageStatus::DEAD_LETTER;
        m_stats.messagesFailed++;
        
        if (m_config.enableDeadLetter) {
            m_deadLetterQueue.add(std::move(msg), error);
            m_stats.messagesDeadLettered++;
            m_stats.deadLetterQueueSize++;
        }
    }
}

std::string MessageQueue::generateMessageId() {
    uint64_t id = m_messageCounter.fetch_add(1) + 1;
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    
    std::stringstream ss;
    ss << "msg_" << std::hex << timestamp << "_" << id;
    return ss.str();
}

std::string MessageQueue::generateSubscriptionId() {
    uint64_t id = m_subscriptionCounter.fetch_add(1) + 1;
    
    std::stringstream ss;
    ss << "sub_" << std::hex << id;
    return ss.str();
}

// =============================================================================
// EventBus Implementation
// =============================================================================

EventBus& EventBus::instance() {
    static EventBus instance;
    return instance;
}

std::string EventBus::on(const std::string& eventType, EventHandler handler) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::string id = "evt_" + std::to_string(m_counter.fetch_add(1) + 1);
    
    EventSubscription sub;
    sub.id = id;
    sub.handler = handler;
    sub.once = false;
    
    m_handlers[eventType].push_back(std::move(sub));
    
    return id;
}

std::string EventBus::once(const std::string& eventType, EventHandler handler) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::string id = "evt_" + std::to_string(m_counter.fetch_add(1) + 1);
    
    EventSubscription sub;
    sub.id = id;
    sub.handler = handler;
    sub.once = true;
    
    m_handlers[eventType].push_back(std::move(sub));
    
    return id;
}

void EventBus::off(const std::string& subscriptionId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    for (auto& [_, handlers] : m_handlers) {
        handlers.erase(
            std::remove_if(handlers.begin(), handlers.end(),
                [&](const EventSubscription& sub) { return sub.id == subscriptionId; }),
            handlers.end());
    }
}

void EventBus::offAll(const std::string& eventType) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_handlers.erase(eventType);
}

void EventBus::emit(const std::string& eventType, const std::any& data) {
    std::vector<EventSubscription> toCall;
    std::vector<std::string> toRemove;
    
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_handlers.find(eventType);
        if (it != m_handlers.end()) {
            for (const auto& sub : it->second) {
                toCall.push_back(sub);
                if (sub.once) {
                    toRemove.push_back(sub.id);
                }
            }
        }
    }
    
    // Call handlers
    for (const auto& sub : toCall) {
        try {
            sub.handler(eventType, data);
        } catch (...) {
            // Ignore handler exceptions
        }
    }
    
    // Remove one-time handlers
    for (const auto& id : toRemove) {
        off(id);
    }
}

void EventBus::emitAsync(const std::string& eventType, const std::any& data) {
    std::thread([this, eventType, data]() {
        emit(eventType, data);
    }).detach();
}

// =============================================================================
// RequestReply Implementation
// =============================================================================

RequestReply& RequestReply::instance() {
    static RequestReply instance;
    return instance;
}

void RequestReply::registerHandler(const std::string& requestType, RequestHandler handler) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_handlers[requestType] = handler;
}

void RequestReply::unregisterHandler(const std::string& requestType) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_handlers.erase(requestType);
}

std::optional<std::string> RequestReply::request(const std::string& requestType,
                                                   const std::string& payload,
                                                   int timeoutMs) {
    RequestHandler handler;
    
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_handlers.find(requestType);
        if (it == m_handlers.end()) {
            return std::nullopt;
        }
        handler = it->second;
    }
    
    try {
        return handler(payload);
    } catch (...) {
        return std::nullopt;
    }
}

void RequestReply::requestAsync(const std::string& requestType,
                                 const std::string& payload,
                                 std::function<void(std::optional<std::string>)> callback) {
    std::thread([this, requestType, payload, callback]() {
        auto result = request(requestType, payload);
        if (callback) {
            callback(result);
        }
    }).detach();
}

} // namespace enterprise
