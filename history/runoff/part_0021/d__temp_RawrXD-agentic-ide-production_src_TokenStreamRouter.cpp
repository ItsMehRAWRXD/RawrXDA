#include "TokenStreamRouter.h"
#include <QDebug>

TokenStreamRouter::TokenStreamRouter(QObject* parent)
    : QObject(parent) {
    qInfo() << "TokenStreamRouter initialized";
}

TokenStreamRouter::~TokenStreamRouter() {
    QMutexLocker lock(&m_mutex);
    m_consumers.clear();
}

void TokenStreamRouter::registerTokenConsumer(const QString& id, std::function<void(const QString&)> callback) {
    QMutexLocker lock(&m_mutex);
    
    // Check if already registered
    for (const auto& consumer : m_consumers) {
        if (consumer.id == id) {
            qWarning() << "Consumer already registered:" << id;
            return;
        }
    }
    
    m_consumers.push_back({id, callback});
    m_stats.totalConsumers = m_consumers.size();
    
    emit consumerRegistered(id);
    qInfo() << "Registered token consumer:" << id << "Total consumers:" << m_stats.totalConsumers;
}

void TokenStreamRouter::unregisterTokenConsumer(const QString& id) {
    QMutexLocker lock(&m_mutex);
    
    auto it = std::find_if(m_consumers.begin(), m_consumers.end(),
                          [&id](const Consumer& c) { return c.id == id; });
    
    if (it != m_consumers.end()) {
        m_consumers.erase(it);
        m_stats.totalConsumers = m_consumers.size();
        emit consumerUnregistered(id);
        qInfo() << "Unregistered token consumer:" << id;
    }
}

void TokenStreamRouter::routeToken(const QString& token) {
    QMutexLocker lock(&m_mutex);
    
    for (const auto& consumer : m_consumers) {
        if (consumer.callback) {
            consumer.callback(token);
        }
    }
    
    m_stats.totalTokensRouted++;
    emit tokenRouted(token);
}

void TokenStreamRouter::routeTokenTo(const QString& consumerId, const QString& token) {
    QMutexLocker lock(&m_mutex);
    
    for (const auto& consumer : m_consumers) {
        if (consumer.id == consumerId && consumer.callback) {
            consumer.callback(token);
            m_stats.totalTokensRouted++;
            emit tokenRouted(token);
            return;
        }
    }
    
    qWarning() << "Token consumer not found:" << consumerId;
}

TokenStreamRouter::StreamStats TokenStreamRouter::getStreamStatistics() const {
    QMutexLocker lock(&m_mutex);
    return m_stats;
}

void TokenStreamRouter::resetStatistics() {
    QMutexLocker lock(&m_mutex);
    m_stats.totalTokensRouted = 0;
    m_stats.avgLatencyMs = 0.0;
    qInfo() << "TokenStreamRouter statistics reset";
}
