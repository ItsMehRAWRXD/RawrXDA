// ============================================================================
// EventBus.h — Lightweight pub/sub event bus for RawrXD (header-only)
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <mutex>
#include <algorithm>

namespace RawrXD {

// ---------------------------------------------------------------------------
// Event types
// ---------------------------------------------------------------------------
enum class EventType {
    None,
    EditorChanged,
    CursorMoved,
    FileOpened,
    FileSaved,
    AIResponse,
    AIAccepted,
    AIRejected,
    ContextUpdated,
    ModelLoaded,
    InferenceStarted,
    InferenceCompleted,
    InferenceError,
    SettingsChanged,
    ThemeChanged,
    ExtensionActivated,
    ExtensionDeactivated,
};

// ---------------------------------------------------------------------------
// Event payload
// ---------------------------------------------------------------------------
struct Event {
    EventType type = EventType::None;
    std::string source;
    std::string data;
    uint64_t timestamp = 0;
    void* userData = nullptr;
};

// ---------------------------------------------------------------------------
// EventBus — thread-safe pub/sub (header-only implementation)
// ---------------------------------------------------------------------------
class EventBus {
public:
    using Handler = std::function<void(const Event&)>;
    using Token = size_t;

    static EventBus& Instance() {
        static EventBus inst;
        return inst;
    }
    static EventBus& Get() { return Instance(); }

    Token Subscribe(EventType type, Handler handler) {
        std::lock_guard<std::mutex> lock(m_mutex);
        Token tok = m_nextToken++;
        m_handlers[type].push_back({tok, std::move(handler)});
        return tok;
    }

    Token SubscribeAll(Handler handler) {
        std::lock_guard<std::mutex> lock(m_mutex);
        Token tok = m_nextToken++;
        m_allHandlers.push_back({tok, std::move(handler)});
        return tok;
    }

    void Unsubscribe(Token token) {
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

    void Publish(const Event& event) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queue.push_back(event);
    }

    void Publish(EventType type, const std::string& data = "") {
        Event ev;
        ev.type = type;
        ev.data = data;
        ev.timestamp = 0;
        Publish(ev);
    }

    void ProcessQueue() {
        std::vector<Event> local;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            local.swap(m_queue);
        }
        for (const auto& event : local) {
            auto it = m_handlers.find(event.type);
            if (it != m_handlers.end()) {
                for (const auto& [tok, handler] : it->second) {
                    if (handler) handler(event);
                }
            }
            for (const auto& [tok, handler] : m_allHandlers) {
                if (handler) handler(event);
            }
        }
    }

private:
    EventBus() = default;
    ~EventBus() = default;
    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;

    std::mutex m_mutex;
    std::unordered_map<EventType, std::vector<std::pair<Token, Handler>>> m_handlers;
    std::vector<std::pair<Token, Handler>> m_allHandlers;
    std::vector<Event> m_queue;
    Token m_nextToken = 1;
};

} // namespace RawrXD
