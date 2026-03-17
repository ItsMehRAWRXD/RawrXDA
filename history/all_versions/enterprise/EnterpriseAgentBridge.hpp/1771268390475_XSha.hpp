#ifndef ENTERPRISE_AGENT_BRIDGE_HPP
#define ENTERPRISE_AGENT_BRIDGE_HPP

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QDateTime>
#include <QMap>
#include <QQueue>
#include <QMutex>
#include <QWaitCondition>
#include <QThreadPool>
#include <QAtomicInt>
#include <QJsonObject>
#include <QJsonArray>
#include <QFuture>
#include "../test_suite/agentic_tools.hpp"

struct EnterpriseMission {
    QString id;
    QString description;
    QVariantMap parameters;
    QDateTime createdAt;
    QDateTime completedAt;
    QString status; // "pending", "running", "completed", "failed"
    QVariantMap results;
    QString errorMessage;
    int retryCount;
    int priority;
};

// Internal mission tracking structure
struct MissionData {
    QString id;
    QString description;
    QJsonObject parameters;
    QDateTime createdAt;
    QDateTime startedAt;
    QDateTime completedAt;
    QString status;
    QJsonObject results;
    QString errorMessage;
    int retryCount;
    int priority;
};

// Forward declaration of private implementation
class EnterpriseAgentBridgePrivate;

class EnterpriseAgentBridge : public QObject {
    Q_OBJECT
    
public:
    static EnterpriseAgentBridge* instance();
    
    // Enterprise mission execution
    QString submitMission(const QString& description, const QVariantMap& parameters);
    EnterpriseMission getMissionStatus(const QString& missionId);
    QList<EnterpriseMission> getActiveMissions();
    
    // Enterprise tool orchestration
    bool executeToolChain(const QStringList& tools, const QVariantMap& parameters);
    bool executeParallelTools(const QStringList& tools, const QVariantMap& parameters);
    
    // Enterprise scheduling
    QString scheduleMission(const QString& description, const QVariantMap& parameters, 
                           const QDateTime& scheduledTime);
    
    // Enterprise metrics
    int getConcurrentMissionCount() const;
    int getTotalMissionsProcessed() const;
    double getMissionSuccessRate() const;
    
    // Enterprise configuration
    void setMaxConcurrentMissions(int max);
    void setDefaultTimeout(int timeoutMs);
    void setRetryPolicy(const QString& policy);
    
    // Enterprise security
    bool validateMissionParameters(const QVariantMap& parameters);
    bool auditMissionExecution(const QString& missionId);
    
    // Enterprise cleanup
    void cleanupCompletedMissions();
    void emergencyStopAllMissions();
    
signals:
    void missionStarted(const QString& missionId);
    void missionCompleted(const QString& missionId, const QVariantMap& results);
    void missionFailed(const QString& missionId, const QString& error);
    void toolChainCompleted(const QString& chainId, const QVariantMap& results);
    void enterpriseAlert(const QString& alertType, const QVariantMap& details);
    
private:
    explicit EnterpriseAgentBridge(QObject *parent = nullptr);
    ~EnterpriseAgentBridge();
    
    QScopedPointer<EnterpriseAgentBridgePrivate> d_ptr;

    // Internal orchestration methods
    void processMissionQueue();
    void executeMission(const MissionData& mission);
    
    // Mission type executors
    ToolResult executeToolChainMission(const MissionData& mission);
    ToolResult executeParallelToolsMission(const MissionData& mission);
    ToolResult executeSingleToolMission(const MissionData& mission);
    
    // Error handling and helpers
    void handleMissionFailure(const MissionData& mission, const QString& error);
    ToolResult executeToolWithTimeout(const QString& toolName, const QStringList& parameters, int timeoutMs);
    
    Q_DISABLE_COPY(EnterpriseAgentBridge)
};

#endif // ENTERPRISE_AGENT_BRIDGE_HPP