#pragma once
#include <functional>
#include <QString>
#include <QDebug>
#include "ProdIntegration.h"

namespace RawrXD {
namespace Integration {

// Optional RAII wrapper for resource guards
template<typename Resource>
class ResourceGuard {
public:
    using Deleter = std::function<void(Resource)>;

    explicit ResourceGuard(Resource resource, Deleter deleter)
        : m_resource(resource), m_deleter(deleter), m_released(false) {
        if (Config::loggingEnabled()) {
            qInfo().noquote() << "[ResourceGuard] Acquired resource";
        }
    }

    ~ResourceGuard() {
        release();
    }

    // Prevent copying
    ResourceGuard(const ResourceGuard&) = delete;
    ResourceGuard& operator=(const ResourceGuard&) = delete;

    // Allow move
    ResourceGuard(ResourceGuard&& other) noexcept
        : m_resource(other.m_resource),
          m_deleter(std::move(other.m_deleter)),
          m_released(other.m_released) {
        other.m_released = true;
    }

    ResourceGuard& operator=(ResourceGuard&& other) noexcept {
        if (this != &other) {
            release();
            m_resource = other.m_resource;
            m_deleter = std::move(other.m_deleter);
            m_released = other.m_released;
            other.m_released = true;
        }
        return *this;
    }

    Resource get() const { return m_resource; }
    Resource operator*() const { return m_resource; }

    void release() {
        if (!m_released && m_deleter) {
            m_deleter(m_resource);
            m_released = true;
            if (Config::loggingEnabled()) {
                qInfo().noquote() << "[ResourceGuard] Released resource";
            }
        }
    }

private:
    Resource m_resource;
    Deleter m_deleter;
    bool m_released;
};

// Convenience factory for scoped function execution
class ScopedAction {
public:
    using Action = std::function<void()>;

    explicit ScopedAction(Action onDestroy)
        : m_action(std::move(onDestroy)) {}

    ~ScopedAction() {
        if (m_action) {
            m_action();
        }
    }

    ScopedAction(const ScopedAction&) = delete;
    ScopedAction& operator=(const ScopedAction&) = delete;

private:
    Action m_action;
};

} // namespace Integration
} // namespace RawrXD
