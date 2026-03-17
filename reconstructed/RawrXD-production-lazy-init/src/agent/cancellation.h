#ifndef AGENT_CANCELLATION_H
#define AGENT_CANCELLATION_H

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QAtomicInt>
#include <QSharedPointer>
#include <functional>

namespace RawrXD {

class CancellationToken {
public:
    CancellationToken() : cancelled_(false) {}
    
    bool isCancelled() const { return cancelled_.load(); }
    void cancel() { cancelled_.store(true); }
    void reset() { cancelled_.store(false); }
    
    void throwIfCancelled() {
        if (isCancelled()) {
            throw std::runtime_error("Operation cancelled");
        }
    }
    
    void waitForCancellation() {
        while (!isCancelled()) {
            QThread::msleep(100);
        }
    }
    
private:
    QAtomicInt cancelled_;
};

class AgentCancellationManager : public QObject {
    Q_OBJECT

public:
    static AgentCancellationManager& instance();
    
    void initialize();
    void shutdown();
    
    // Agent management
    QString registerAgent(const QString& agentType, const QString& description = QString());
    bool unregisterAgent(const QString& agentId);
    
    // Cancellation control
    bool cancelAgent(const QString& agentId, bool waitForCompletion = true, int timeoutMs = 5000);
    void cancelAllAgents(bool waitForCompletion = true, int timeoutMs = 5000);
    
    // Status queries
    bool isAgentRunning(const QString& agentId) const;
    QList<QString> getRunningAgents() const;
    int getAgentCount() const;
    
    // Token management
    QSharedPointer<CancellationToken> getToken(const QString& agentId);
    
    // Graceful shutdown helpers
    void setShutdownHandler(std::function<void()> handler);
    void setCleanupHandler(const QString& agentId, std::function<void()> handler);
    
signals:
    void agentStarted(const QString& agentId, const QString& agentType);
    void agentCompleted(const QString& agentId, bool success);
    void agentCancelled(const QString& agentId);
    void agentError(const QString& agentId, const QString& error);

private:
    AgentCancellationManager() = default;
    ~AgentCancellationManager();
    
    struct AgentInfo {
        QString id;
        QString type;
        QString description;
        QSharedPointer<CancellationToken> token;
        QDateTime startTime;
        QThread* thread;
        std::function<void()> cleanupHandler;
        bool running;
        
        AgentInfo() : thread(nullptr), running(false) {}
    };
    
    QString generateAgentId(const QString& agentType);
    void performCleanup(const QString& agentId);
    
    mutable QMutex mutex_;
    QHash<QString, AgentInfo> agents_;
    std::function<void()> shutdownHandler_;
    bool initialized_ = false;
};

// Convenience class for scoped agent registration
class ScopedAgent {
public:
    ScopedAgent(const QString& agentType, const QString& description = QString())
        : agentId_(AgentCancellationManager::instance().registerAgent(agentType, description))
        , token_(AgentCancellationManager::instance().getToken(agentId_)) {}
    
    ~ScopedAgent() {
        AgentCancellationManager::instance().unregisterAgent(agentId_);
    }
    
    const QString& id() const { return agentId_; }
    QSharedPointer<CancellationToken> token() const { return token_; }
    
    void checkCancellation() { token_->throwIfCancelled(); }
    
private:
    QString agentId_;
    QSharedPointer<CancellationToken> token_;
};

// Convenience macros
#define AGENT_REGISTER(type, desc) RawrXD::ScopedAgent agent##__LINE__(type, desc)
#define AGENT_CHECK_CANCELLATION() agent##__LINE__.checkCancellation()
#define AGENT_TOKEN agent##__LINE__.token()

#define AGENT_CANCEL(id) RawrXD::AgentCancellationManager::instance().cancelAgent(id)
#define AGENT_CANCEL_ALL() RawrXD::AgentCancellationManager::instance().cancelAllAgents()

} // namespace RawrXD

#endif // AGENT_CANCELLATION_H