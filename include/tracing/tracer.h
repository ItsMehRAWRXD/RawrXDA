#pragma once

#include <string>
#include <chrono>
#include <memory>
#include <vector>
#include <unordered_map>
#include <mutex>

class Span {
private:
    std::string m_name;
    std::chrono::steady_clock::time_point m_startTime;
    std::unordered_map<std::string, std::string> m_attributes;
    std::string m_status = "unknown";
    std::string m_statusDescription;
    mutable std::mutex m_mutex;

public:
    Span(const std::string& name)
        : m_name(name), m_startTime(std::chrono::steady_clock::now()) {}

    void setAttribute(const std::string& key, const std::string& value) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_attributes[key] = value;
    }

    void setAttribute(const std::string& key, int64_t value) {
        setAttribute(key, std::to_string(value));
    }

    void setAttribute(const std::string& key, double value) {
        setAttribute(key, std::to_string(value));
    }

    void setStatus(const std::string& status, const std::string& description = "") {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_status = status;
        m_statusDescription = description;
    }

    std::string getName() const { return m_name; }
    std::string getStatus() const { return m_status; }
    std::string getStatusDescription() const { return m_statusDescription; }

    int64_t getDurationMs() const {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            now - m_startTime).count();
    }

    std::unordered_map<std::string, std::string> getAttributes() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_attributes;
    }
};

class Tracer {
private:
    std::vector<std::shared_ptr<Span>> m_activeSpans;
    mutable std::mutex m_mutex;
    bool m_enabled = true;

public:
    Tracer() = default;

    std::shared_ptr<Span> startSpan(const std::string& name) {
        if (!m_enabled) return nullptr;

        std::lock_guard<std::mutex> lock(m_mutex);
        auto span = std::make_shared<Span>(name);
        m_activeSpans.push_back(span);
        return span;
    }

    void endSpan(std::shared_ptr<Span> span) {
        if (!span) return;

        std::lock_guard<std::mutex> lock(m_mutex);
        // In a real implementation, would export span data
        // For now, just remove from active list
        auto it = std::find(m_activeSpans.begin(), m_activeSpans.end(), span);
        if (it != m_activeSpans.end()) {
            m_activeSpans.erase(it);
        }
    }

    void setEnabled(bool enabled) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_enabled = enabled;
    }

    bool isEnabled() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_enabled;
    }

    std::vector<std::shared_ptr<Span>> getActiveSpans() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_activeSpans;
    }
};
