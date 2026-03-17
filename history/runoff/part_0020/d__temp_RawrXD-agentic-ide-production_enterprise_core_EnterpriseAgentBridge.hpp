#ifndef ENTERPRISE_AGENT_BRIDGE_HPP
#define ENTERPRISE_AGENT_BRIDGE_HPP

#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <chrono>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <future>
#include "agentic_tools.hpp"

struct EnterpriseMission {
    std::string id;
    std::string description;
    std::unordered_map<std::string, std::string> parameters;
    std::chrono::system_clock::time_point createdAt;
    std::chrono::system_clock::time_point completedAt;
    std::string status; // "pending", "running", "completed", "failed"
    std::unordered_map<std::string, std::string> results;
    std::string errorMessage;
    int retryCount;
    int priority;
};

// Internal mission tracking structure
struct MissionData {
    std::string id;
    std::string description;
    std::unordered_map<std::string, std::string> parameters;
    std::chrono::system_clock::time_point createdAt;
    std::chrono::system_clock::time_point startedAt;
    std::chrono::system_clock::time_point completedAt;
    std::string status;
    std::unordered_map<std::string, std::string> results;
    std::string errorMessage;
    int retryCount;
    int priority;
};

// Forward declaration of private implementation
class EnterpriseAgentBridgePrivate;

class EnterpriseAgentBridge {
    
public:
    static EnterpriseAgentBridge* instance();
    
    // Enterprise mission execution
    std::string submitMission(const std::string& description, const std::unordered_map<std::string, std::string>& parameters);
    EnterpriseMission getMissionStatus(const std::string& missionId);
    std::vector<EnterpriseMission> getActiveMissions();
    
    // Enterprise tool orchestration
    bool executeToolChain(const std::vector<std::string>& tools, const std::unordered_map<std::string, std::string>& parameters);
    bool executeParallelTools(const std::vector<std::string>& tools, const std::unordered_map<std::string, std::string>& parameters);
    
    // Enterprise scheduling
    std::string scheduleMission(const std::string& description, const std::unordered_map<std::string, std::string>& parameters, 
                           const std::chrono::system_clock::time_point& scheduledTime);
    
    // Enterprise metrics
    int getConcurrentMissionCount() const;
    int getTotalMissionsProcessed() const;
    double getMissionSuccessRate() const;
    
    // Enterprise configuration
    void setMaxConcurrentMissions(int max);
    void setDefaultTimeout(int timeoutMs);
    void setRetryPolicy(const std::string& policy);
    
    // Enterprise security
    bool validateMissionParameters(const std::unordered_map<std::string, std::string>& parameters);
    bool auditMissionExecution(const std::string& missionId);
    
    // Enterprise cleanup
    void cleanupCompletedMissions();
    void emergencyStopAllMissions();
    
    // Event callbacks (replacing Qt signals)
    std::function<void(const std::string& missionId)> onMissionStarted;
    std::function<void(const std::string& missionId, const std::unordered_map<std::string, std::string>& results)> onMissionCompleted;
    std::function<void(const std::string& missionId, const std::string& error)> onMissionFailed;
    std::function<void(const std::string& chainId, const std::unordered_map<std::string, std::string>& results)> onToolChainCompleted;
    std::function<void(const std::string& alertType, const std::unordered_map<std::string, std::string>& details)> onEnterpriseAlert;
    
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
    void handleMissionFailure(const MissionData& mission, const std::string& error);
    ToolResult executeToolWithTimeout(const std::string& toolName, const std::vector<std::string>& parameters, int timeoutMs);
    
    // Disable copying
    EnterpriseAgentBridge(const EnterpriseAgentBridge&) = delete;
    EnterpriseAgentBridge& operator=(const EnterpriseAgentBridge&) = delete;
};

#endif // ENTERPRISE_AGENT_BRIDGE_HPP