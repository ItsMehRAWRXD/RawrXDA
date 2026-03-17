#pragma once
#include <QObject>
#include <QString>
#include <QHash>

class AICompletionCache : public QObject {
    Q_OBJECT
public:
    explicit AICompletionCache(QObject* parent = nullptr);
    ~AICompletionCache();

    void setCache(const QString& key, const QString& value, int ttlMs = 300000);
    QString getCache(const QString& key) const;
    void clearCache();

private:
    QHash<QString, QPair<QString, qint64>> m_cache;
};