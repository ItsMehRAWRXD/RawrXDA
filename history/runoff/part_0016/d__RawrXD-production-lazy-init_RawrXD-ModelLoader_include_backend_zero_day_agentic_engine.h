#pragma once

#include <string>
#include <vector>
#include <memory>

namespace RawrXD {
namespace Backend {

// Zero-day agentic engine for autonomous operation
class ZeroDayAgenticEngine {
public:
    ZeroDayAgenticEngine();
    ~ZeroDayAgenticEngine();
    
    // Initialize the engine
    bool Initialize();
    
    // Start autonomous operation
    bool Start();
    
    // Stop autonomous operation
    void Stop();
    
    // Start a mission
    std::string StartMission(const std::string& description);
    
    // Check if mission is running
    bool IsMissionRunning() const;
    
    // Get mission update
    std::string GetMissionUpdate() const;
    
    // Get mission result
    std::string GetMissionResult() const;
    
    // Process a user request autonomously
    std::string ProcessRequest(const std::string& request);
    
    // Get engine status
    std::string GetStatus() const;
    
    // Start mission (alias for StartMission)
    std::string startMission(const std::string& description) { return StartMission(description); }
    
    // Check if mission is running (alias for IsMissionRunning)
    bool isMissionRunning() const { return IsMissionRunning(); }
    
    // Get mission update (alias for GetMissionUpdate)
    std::string getMissionUpdate() const { return GetMissionUpdate(); }
    
    // Get mission result (alias for GetMissionResult)
    std::string getMissionResult() const { return GetMissionResult(); }
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace Backend
} // namespace RawrXD