#include "notification_manager.h"

namespace RawrXD::UI {
    NotificationManager& NotificationManager::getInstance() {
        static NotificationManager inst;
        return inst;
    }

    void NotificationManager::post(NotificationSeverity severity, const std::string& title, const std::string& message, int timeoutMs) {
        std::lock_guard<std::mutex> lock(m_mutex);
        Notification note;
        note.severity = severity;
        note.title = title;
        note.message = message;
        note.timestamp = std::chrono::steady_clock::now();
        note.timeoutMs = timeoutMs;

        m_queue.push_back(note);
        if (m_queue.size() > m_maxHistory) {
            m_queue.erase(m_queue.begin());
        }

        if (m_callback) m_callback(note);
    }

    std::vector<Notification> NotificationManager::getPending() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto now = std::chrono::steady_clock::now();
        std::vector<Notification> pending;
        for (const auto& note : m_queue) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - note.timestamp).count();
            if (elapsed < note.timeoutMs || note.timeoutMs == 0) {
                pending.push_back(note);
            }
        }
        return pending;
    }

    void NotificationManager::clearAll() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queue.clear();
    }
}
