#ifndef ENTERPRISE_AGENT_BRIDGE_HPP
#define ENTERPRISE_AGENT_BRIDGE_HPP

#include <string>
#include <vector>
#include <map>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <any>
#include <future>
#include <QString>
#include <QStringList>
#include "../../test_suite/agentic_tools.hpp"

// Custom types to replace Qt
using String = QString;
using StringList = QStringList;
using VariantMap = std::map<std::string, std::any>;
using DateTime = std::chrono::system_clock::time_point;
using Map = std::map<std::string, std::any>;
using Queue = std::queue<std::any>;
using Mutex = std::mutex;
using WaitCondition = std::condition_variable;
using AtomicInt = std::atomic<int>;
using Future = std::future<std::any>;

struct EnterpriseMission {
    String id;
    String description;
    VariantMap parameters;
    DateTime createdAt;
    DateTime completedAt;
    String status; // "pending", "running", "completed", "failed"
    VariantMap results;
    QString errorMessage;
    int retryCount;
    int priority;
};

// Internal mission tracking structure
struct MissionData {
    String id;
    String description;
    std::map<std::string, std::any> parameters; // simplified
    DateTime createdAt;
    DateTime startedAt;
    DateTime completedAt;
    String status;
    std::map<std::string, std::any> results;
    String errorMessage;
    int retryCount;
    int priority;
};

// Forward declaration of private implementation
class EnterpriseAgentBridgePrivate;

class EnterpriseAgentBridge {
    
public:
    static EnterpriseAgentBridge* instance();
    
    // Enterprise mission execution
    String submitMission(const String& description, const VariantMap& parameters);
    EnterpriseMission getMissionStatus(const String& missionId);
    std::vector<EnterpriseMission> getActiveMissions();
    
    // Enterprise tool orchestration
    bool executeToolChain(const StringList& tools, const VariantMap& parameters);
    bool executeParallelTools(const StringList& tools, const VariantMap& parameters);
    
    // Enterprise scheduling
    String scheduleMission(const String& description, const VariantMap& parameters, 
                           const DateTime& scheduledTime);
    
    // Enterprise metrics
    int getConcurrentMissionCount() const;
    int getTotalMissionsProcessed() const;
    double getMissionSuccessRate() const;
    
    // Enterprise configuration
    void setMaxConcurrentMissions(int max);
    void setDefaultTimeout(int timeoutMs);
    void setRetryPolicy(const String& policy);
    
    // Enterprise security
    bool validateMissionParameters(const VariantMap& parameters);
    bool auditMissionExecution(const String& missionId);
    
    // Enterprise cleanup
    void cleanupCompletedMissions();
    void emergencyStopAllMissions();
    
private:
    explicit EnterpriseAgentBridge();
    ~EnterpriseAgentBridge();
    
    std::unique_ptr<EnterpriseAgentBridgePrivate> d_ptr;

    // Internal orchestration methods
    void processMissionQueue();
    void executeMission(const MissionData& mission);
    
    // Mission type executors
    ToolResult executeToolChainMission(const MissionData& mission);
    ToolResult executeParallelToolsMission(const MissionData& mission);
    ToolResult executeSingleToolMission(const MissionData& mission);
    
    // Error handling and helpers
    void handleMissionFailure(const MissionData& mission, const String& error);
    ToolResult executeToolWithTimeout(const String& toolName, const StringList& parameters, int timeoutMs);
};

#endif // ENTERPRISE_AGENT_BRIDGE_HPP