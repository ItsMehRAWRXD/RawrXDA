// UEC-X Microkernel - Event Bus
// Lock-free event distribution system

#pragma once

#include "uec_core.h"
#include <queue>
#include <unordered_map>
#include <shared_mutex>

namespace uec {

// =============================================================================
// Event Bus
// =============================================================================

class UEC_API EventBus {
public:
    EventBus();
    ~EventBus();

    // Non-copyable, non-movable
    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;
    EventBus(EventBus&&) = delete;
    EventBus& operator=(EventBus&&) = delete;

    // Lifecycle
    Result<void> Initialize(uint32_t maxQueueSize = 10000);
    Result<void> Shutdown();
    bool IsInitialized() const;

    // Event subscription
    Result<uint64_t> Subscribe(
        EventType type,
        EventHandler handler,
        ExtensionId subscriber = 0,
        EventFilter filter = nullptr
    );
    
    Result<void> SubscribeAll(
        EventHandler handler,
        ExtensionId subscriber = 0
    );
    
    Result<void> Unsubscribe(uint64_t subscriptionId);
    Result<void> UnsubscribeAll(ExtensionId subscriber);
    
    // Event emission
    Result<void> Emit(const Event& event);
    Result<void> Emit(EventType type, ExtensionId source, std::vector<uint8_t> payload);
    
    // Synchronous event emission (waits for all handlers)
    Result<void> EmitSync(const Event& event);

    // Event processing
    void ProcessEvents();
    Result<void> ProcessEventsWithTimeout(uint32_t timeoutMs);
    
    // Query
    size_t GetPendingEventCount() const;
    size_t GetSubscriberCount(EventType type) const;
    size_t GetTotalSubscriberCount() const;

private:
    struct Subscription {
        uint64_t id;
        EventType type;
        ExtensionId subscriber;
        EventHandler handler;
        EventFilter filter;
        Timestamp createdAt;
    };

    mutable std::shared_mutex m_mutex;
    std::unordered_map<EventType, std::vector<Subscription>> m_subscriptions;
    std::vector<Subscription> m_wildcardSubscriptions;
    std::queue<Event> m_eventQueue;
    std::atomic<uint64_t> m_nextSubscriptionId{1};
    std::atomic<bool> m_initialized{false};
    uint32_t m_maxQueueSize = 10000;
    std::condition_variable m_eventCondition;
    mutable std::mutex m_queueMutex;
};

} // namespace uec
