/**
 * @file AICompletionCache.cpp
 * @brief Implementation for AICompletionCache - AI completion caching system
 */

#include "AICompletionCache.h"
#include <QDateTime>
#include <QVBoxLayout>
#include <QLabel>
#include <QDebug>
#include "integration/ProdIntegration.h"

AICompletionCache::AICompletionCache(QWidget* parent) : QWidget(parent)
{
    setupUI();
    RawrXD::Integration::ScopedTimer timer("Cache", "AICompletionCache", "construct");
    RawrXD::Integration::logInfo("Cache", "create", "AICompletionCache created");
}

void AICompletionCache::setupUI() {
    QVBoxLayout* layout = new QVBoxLayout(this);
    
    m_statsLabel = new QLabel("Cache: 0 entries", this);
    layout->addWidget(m_statsLabel);
    
    m_cacheDisplay = new QPlainTextEdit(this);
    m_cacheDisplay->setReadOnly(true);
    layout->addWidget(m_cacheDisplay);
    
    m_clearButton = new QPushButton("Clear Cache", this);
    connect(m_clearButton, &QPushButton::clicked, this, &AICompletionCache::onClearCache);
    layout->addWidget(m_clearButton);
    
    setLayout(layout);
}

AICompletionCache::~AICompletionCache()
{
    RawrXD::Integration::logInfo("Cache", "destroy", "AICompletionCache destroyed");
}

void AICompletionCache::setCache(const QString& key, const QString& value, int ttlMs)
{
    RawrXD::Integration::ScopedTimer timer("Cache", "AICompletionCache", "setCache");
    qint64 expiryTime = QDateTime::currentMSecsSinceEpoch() + ttlMs;
    m_cache[key] = qMakePair(value, expiryTime);
    RawrXD::Integration::logInfo("Cache", "cache_set", QString("Cached value for key: %1").arg(key));
}

QString AICompletionCache::getCache(const QString& key) const
{
    RawrXD::Integration::ScopedTimer timer("Cache", "AICompletionCache", "getCache");
    if (m_cache.contains(key)) {
        auto& cacheEntry = m_cache[key];
        qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
        if (currentTime < cacheEntry.second) {
            RawrXD::Integration::logInfo("Cache", "cache_hit", QString("Cache hit for key: %1").arg(key));
            return cacheEntry.first;
        } else {
            RawrXD::Integration::logInfo("Cache", "cache_expired", QString("Cache expired for key: %1").arg(key));
            const_cast<AICompletionCache*>(this)->m_cache.remove(key);
        }
    }
    RawrXD::Integration::logInfo("Cache", "cache_miss", QString("Cache miss for key: %1").arg(key));
    return QString();
}

void AICompletionCache::clearCache()
{
    RawrXD::Integration::ScopedTimer timer("Cache", "AICompletionCache", "clearCache");
    m_cache.clear();
    updateStats();
    RawrXD::Integration::logInfo("Cache", "clear", "AICompletionCache cleared");
}

void AICompletionCache::onClearCache()
{
    clearCache();
}

void AICompletionCache::updateStats()
{
    if (m_statsLabel) {
        m_statsLabel->setText(QString("Cache: %1 entries").arg(m_cache.size()));
    }
    if (m_cacheDisplay) {
        m_cacheDisplay->setPlainText(QString("Cache entries: %1").arg(m_cache.size()));
    }
}
