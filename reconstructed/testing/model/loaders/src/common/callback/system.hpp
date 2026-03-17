// ============================================================================
// File: src/common/callback_system.hpp
// Purpose: Callback/event system replacing Qt signals/slots
// No Qt dependency - pure C++17
// ============================================================================
#pragma once

#include <functional>
#include <vector>
#include <mutex>
#include <algorithm>
#include <string>

/**
 * @brief Type-safe callback list replacing Qt signals
 * Usage:
 *   CallbackList<int, std::string> onEvent;
 *   auto id = onEvent.connect([](int a, const std::string& b) { ... });
 *   onEvent.emit(42, "hello");
 *   onEvent.disconnect(id);
 */
template<typename... Args>
class CallbackList {
public:
    using Callback = std::function<void(Args...)>;
    using CallbackId = int;

    CallbackId connect(Callback cb) {
        std::lock_guard<std::mutex> lock(m_mutex);
        int id = m_nextId++;
        m_callbacks.push_back({id, std::move(cb)});
        return id;
    }

    void disconnect(CallbackId id) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_callbacks.erase(
            std::remove_if(m_callbacks.begin(), m_callbacks.end(),
                [id](const Entry& e) { return e.id == id; }),
            m_callbacks.end()
        );
    }

    void emit(Args... args) const {
        // Copy callbacks to avoid holding lock during invocation
        std::vector<Entry> copy;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            copy = m_callbacks;
        }
        for (const auto& entry : copy) {
            if (entry.callback) {
                entry.callback(args...);
            }
        }
    }

    void clear() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_callbacks.clear();
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_callbacks.empty();
    }

private:
    struct Entry {
        CallbackId id;
        Callback callback;
    };

    mutable std::mutex m_mutex;
    std::vector<Entry> m_callbacks;
    int m_nextId = 1;
};
