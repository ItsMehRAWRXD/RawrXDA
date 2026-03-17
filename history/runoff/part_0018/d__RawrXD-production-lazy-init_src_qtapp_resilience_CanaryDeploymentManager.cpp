#include "CanaryDeploymentManager.h"

CanaryDeploymentManager::CanaryDeploymentManager(QObject* parent) : QObject(parent) {}
CanaryDeploymentManager::~CanaryDeploymentManager() {}

bool CanaryDeploymentManager::start(const QString& env, const QString& version) {
    QMutexLocker locker(&m_mutex);
    Deployment d;
    d.env = env;
    d.version = version;
    d.started = QDateTime::currentDateTimeUtc();
    m_envs[env] = d;
    emit deploymentUpdated(d);
    return true;
}

bool CanaryDeploymentManager::recordMetrics(const QString& env, double errorRate, double latencyP95, int sampleSize, const QString& note) {
    QMutexLocker locker(&m_mutex);
    if (!m_envs.contains(env)) return false;
    Deployment& d = m_envs[env];
    d.errorRate = errorRate;
    d.latencyP95 = latencyP95;
    d.sampleSize = sampleSize;
    d.note = note;
    emit deploymentUpdated(d);
    return true;
}

bool CanaryDeploymentManager::promote(const QString& env) {
    QMutexLocker locker(&m_mutex);
    if (!m_envs.contains(env)) return false;
    Deployment& d = m_envs[env];
    d.promoted = true;
    d.blocked = false;
    emit deploymentPromoted(d);
    return true;
}

bool CanaryDeploymentManager::rollback(const QString& env, const QString& reason) {
    QMutexLocker locker(&m_mutex);
    if (!m_envs.contains(env)) return false;
    Deployment& d = m_envs[env];
    d.promoted = false;
    d.blocked = true;
    d.note = reason;
    emit deploymentRolledBack(d, reason);
    return true;
}

CanaryDeploymentManager::Deployment CanaryDeploymentManager::get(const QString& env) const {
    QMutexLocker locker(&m_mutex);
    return m_envs.value(env);
}

QList<CanaryDeploymentManager::Deployment> CanaryDeploymentManager::list() const {
    QMutexLocker locker(&m_mutex);
    return m_envs.values();
}

bool CanaryDeploymentManager::shouldPromote(const QString& env, double errThreshold, double latThreshold, int minSamples) const {
    QMutexLocker locker(&m_mutex);
    if (!m_envs.contains(env)) return false;
    const Deployment& d = m_envs[env];
    return !d.blocked && d.sampleSize >= minSamples && d.errorRate <= errThreshold && d.latencyP95 <= latThreshold;
}
