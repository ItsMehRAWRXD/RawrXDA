// ============================================================================
// EventBus.cpp — EventBus implementation
// ============================================================================

#include "EventBus.h"

namespace RawrXD {

EventBus& EventBus::Instance() {
    static EventBus inst;
    return inst;
}

EventBus::Token EventBus::Subscribe(EventType type, Handler handler) {
    std::lock_guard<std::mutex> lock(m_mutex);
    Token tok = m_nextToken++;
    m_handlers[type].push_back({tok, std::move(handler)});
    return tok;
}

EventBus::Token EventBus::SubscribeAll(Handler handler) {
    std::lock_guard<std::mutex> lock(m_mutex);
    Token tok = m_nextToken++;
    m_allHandlers.push_back({tok, std::move(handler)});
    return tok;
}

void EventBus::Unsubscribe(Token token) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& [type, handlers] : m_handlers) {
        handlers.erase(
            std::remove_if(handlers.begin(), handlers.end(),
                [token](const auto& p) { return p.first == token; }),
            handlers.end());
    }
    m_allHandlers.erase(
        std::remove_if(m_allHandlers.begin(), m_allHandlers.end(),
            [token](const auto& p) { return p.first == token; }),
        m_allHandlers.end());
}

void EventBus::Publish(const Event& event) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_queue.push_back(event);
}

void EventBus::Publish(EventType type, const std::string& data) {
    Event ev;
    ev.type = type;
    ev.data = data;
    ev.timestamp = 0; // Could use time()
    Publish(ev);
}

void EventBus::ProcessQueue() {
    std::vector<Event> local;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        local.swap(m_queue);
    }
    for (const auto& event : local) {
        // Dispatch to type-specific handlers
        auto it = m_handlers.find(event.type);
        if (it != m_handlers.end()) {
            for (const auto& [tok, handler] : it->second) {
                if (handler) handler(event);
            }
        }
        // Dispatch to catch-all handlers
        for (const auto& [tok, handler] : m_allHandlers) {
            if (handler) handler(event);
        }
    }
}

} // namespace RawrXD
