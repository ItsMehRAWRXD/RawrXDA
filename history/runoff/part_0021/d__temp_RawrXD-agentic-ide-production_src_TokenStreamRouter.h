#ifndef TOKENSTREAMROUTER_H
#define TOKENSTREAMROUTER_H

#include <QString>
#include <QObject>
#include <QMutex>
#include <QVector>
#include <functional>

class TokenStreamRouter : public QObject {
    Q_OBJECT

public:
    explicit TokenStreamRouter(QObject* parent = nullptr);
    ~TokenStreamRouter();

    // Register a consumer for token stream
    void registerTokenConsumer(const QString& id, std::function<void(const QString&)> callback);
    void unregisterTokenConsumer(const QString& id);

    // Route token to all registered consumers
    void routeToken(const QString& token);

    // Route token to specific consumer
    void routeTokenTo(const QString& consumerId, const QString& token);

    // Get stream statistics
    struct StreamStats {
        uint64_t totalTokensRouted = 0;
        uint64_t totalConsumers = 0;
        double avgLatencyMs = 0.0;
    };

    StreamStats getStreamStatistics() const;
    void resetStatistics();

signals:
    void tokenRouted(const QString& token);
    void consumerRegistered(const QString& id);
    void consumerUnregistered(const QString& id);

private:
    struct Consumer {
        QString id;
        std::function<void(const QString&)> callback;
    };

    mutable QMutex m_mutex;
    QVector<Consumer> m_consumers;
    StreamStats m_stats;

    Q_DISABLE_COPY(TokenStreamRouter)
};

#endif // TOKENSTREAMROUTER_H
