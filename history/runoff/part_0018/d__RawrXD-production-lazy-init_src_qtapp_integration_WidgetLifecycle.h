#pragma once
#include "ProdIntegration.h"
#include <QWidget>
#include <QPointer>
#include <QStack>

namespace RawrXD {
namespace Integration {
namespace Widgets {

// Track widget creation/destruction for observability
class WidgetLifecycleTracker {
public:
    static WidgetLifecycleTracker& instance() {
        static WidgetLifecycleTracker tracker;
        return tracker;
    }

    void onWidgetCreated(QWidget* widget, const QString& widgetName) {
        if (!widget) return;
        
        const QString fullName = widgetName.isEmpty() 
            ? QString::fromUtf8(widget->metaObject()->className())
            : widgetName;
            
        m_creationStack.push(widget);
        
        if (Config::loggingEnabled()) {
            logDebug(QStringLiteral("WidgetLifecycle"), QStringLiteral("created"),
                     QStringLiteral("%1 (total: %2)").arg(fullName).arg(m_creationStack.size()));
        }
        
        if (Config::metricsEnabled()) {
            recordMetric("widgets_created", 1);
        }
    }

    void onWidgetDestroyed(QWidget* widget) {
        if (!widget) return;
        
        // Remove from stack (LIFO)
        while (!m_creationStack.isEmpty()) {
            QWidget* top = m_creationStack.pop();
            if (top == widget) break;
        }
        
        const QString className = QString::fromUtf8(widget->metaObject()->className());
        
        if (Config::loggingEnabled()) {
            logDebug(QStringLiteral("WidgetLifecycle"), QStringLiteral("destroyed"),
                     QStringLiteral("%1 (remaining: %2)").arg(className).arg(m_creationStack.size()));
        }
        
        if (Config::metricsEnabled()) {
            recordMetric("widgets_destroyed", 1);
        }
    }

    int activeWidgetCount() const { return m_creationStack.size(); }

    // Generate a snapshot of active widgets
    QStringList activeWidgets() const {
        QStringList result;
        for (QWidget* widget : m_creationStack) {
            if (widget) {
                result.append(QString::fromUtf8(widget->metaObject()->className()));
            }
        }
        return result;
    }

private:
    WidgetLifecycleTracker() = default;
    ~WidgetLifecycleTracker() = default;

    QStack<QPointer<QWidget>> m_creationStack;
};

// RAII widget guard that tracks lifecycle
template <typename T>
class WidgetGuard {
public:
    template <typename... Args>
    WidgetGuard(const QString& widgetName, Args&&... args)
        : m_widget(new T(std::forward<Args>(args)...)) {
        WidgetLifecycleTracker::instance().onWidgetCreated(m_widget, widgetName);
    }

    ~WidgetGuard() {
        if (m_widget) {
            WidgetLifecycleTracker::instance().onWidgetDestroyed(m_widget);
            m_widget->deleteLater();
        }
    }

    T* widget() { return m_widget; }
    const T* widget() const { return m_widget; }

    T* operator->() { return m_widget; }
    const T* operator->() const { return m_widget; }

    operator bool() const { return !m_widget.isNull(); }

    // Non-copyable, movable
    WidgetGuard(const WidgetGuard&) = delete;
    WidgetGuard& operator=(const WidgetGuard&) = delete;
    WidgetGuard(WidgetGuard&&) = default;
    WidgetGuard& operator=(WidgetGuard&&) = default;

private:
    QPointer<T> m_widget;
};

// Macro for easy widget lifecycle tracking
#define TRACK_WIDGET_CREATION(widget, name) \
    RawrXD::Integration::Widgets::WidgetLifecycleTracker::instance().onWidgetCreated(widget, name)

#define TRACK_WIDGET_DESTRUCTION(widget) \
    RawrXD::Integration::Widgets::WidgetLifecycleTracker::instance().onWidgetDestroyed(widget)

// Helper to create a tracked widget
template <typename T, typename... Args>
WidgetGuard<T> createTrackedWidget(const QString& name, Args&&... args) {
    return WidgetGuard<T>(name, std::forward<Args>(args)...);
}

} // namespace Widgets
} // namespace Integration
} // namespace RawrXD
