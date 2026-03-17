#include "FeatureToggleService.h"

FeatureToggleService::FeatureToggleService(QObject* parent) : QObject(parent) {}
FeatureToggleService::~FeatureToggleService() {}

bool FeatureToggleService::define(const Toggle& t) {
    QMutexLocker locker(&m_mutex);
    if (t.key.isEmpty()) return false;
    m_toggles[t.key] = t;
    emit toggleChanged(t.key, t.enabled);
    return true;
}

bool FeatureToggleService::set(const QString& key, bool enabled) {
    QMutexLocker locker(&m_mutex);
    if (!m_toggles.contains(key)) return false;
    m_toggles[key].enabled = enabled;
    emit toggleChanged(key, enabled);
    return true;
}

bool FeatureToggleService::remove(const QString& key) {
    QMutexLocker locker(&m_mutex);
    return m_toggles.remove(key) > 0;
}

bool FeatureToggleService::isEnabled(const QString& key) const {
    QMutexLocker locker(&m_mutex);
    return m_toggles.value(key).enabled;
}

FeatureToggleService::Toggle FeatureToggleService::get(const QString& key) const {
    QMutexLocker locker(&m_mutex);
    return m_toggles.value(key);
}

QList<FeatureToggleService::Toggle> FeatureToggleService::list(const QString& tagFilter) const {
    QMutexLocker locker(&m_mutex);
    QList<Toggle> toggles;
    for (const auto& t : m_toggles) {
        if (tagFilter.isEmpty() || t.tags.contains(tagFilter)) {
            toggles.append(t);
        }
    }
    return toggles;
}
