#pragma once
#include "copilot_gap_closer.h"
#include "autonomous_agentic_orchestrator.hpp"
#include <memory>
#include <string>
#include <vector>
#include <functional>

namespace RawrXD {

/**
 * @class AutonomousIDEInterface
 * @brief High-level interface for autonomous IDE operations
 * 
 * This class provides a simplified interface to the advanced autonomous
 * capabilities, making it easy to integrate complex multi-step operations
 * with safety gates into the IDE.
 */
class AutonomousIDEInterface {
public:
    AutonomousIDEInterface();
    ~AutonomousIDEInterface();
    
    // Initialization
    bool Initialize(const std::string& workspace_path);
    void Shutdown();
    bool IsReady() const;
    
    // High-level autonomous operations
    std::string AnalyzeCodebase(const std::string& path = "");
    std::string RefactorCode(const std::string& file_path, const std::string& requirements = "");
    std::string OptimizePerformance(const std::string& target = "");
    std::string FixBugs(const std::string& error_description = "");
    std::string GenerateTests(const std::string& code_path = "");
    std::string DocumentCode(const std::string& code_path = "");
    
    // Advanced autonomous features
    void EnableMaxAutonomy(bool enable = true);
    void SetSafetyLevel(int level); // 0=unrestricted, 4=maximum safety
    void EnableContinuousImprovement(bool enable = true);
    
    // Multi-step complex operations
    std::string ExecuteComplexWorkflow(const std::string& description);
    bool ExecuteMultiFileRefactoring(const std::vector<std::string>& files, 
                                   const std::string& refactoring_goal);
    bool PerformFullProjectAudit();
    
    // Real-time monitoring and feedback
    std::string GetCurrentActivity() const;
    double GetProgressPercentage() const;
    std::vector<std::string> GetActivePlans() const;
    
    // Callbacks for IDE integration
    std::function<void(const std::string&)> onStatusUpdate;
    std::function<void(const std::string&, const std::string&)> onTaskCompleted;
    std::function<void(const std::string&)> onError;
    std::function<bool(const std::string&)> onUserApprovalRequired;
    
    // Emergency controls
    void EmergencyStop();
    void PauseAllOperations();
    void ResumeOperations();
    
private:
    std::unique_ptr<CopilotGapCloser> gap_closer_;
    std::string workspace_path_;
    bool initialized_{false};
    bool emergency_stopped_{false};
    bool paused_{false};
    
    // Internal helper methods
    std::string CreatePlanFromDescription(const std::string& description);
    bool ValidateWorkspacePath(const std::string& path);
    void SetupCallbacks();
};

// Convenience functions for quick access
namespace Autonomous {
    // Quick autonomous operations
    inline std::string QuickAnalyze(const std::string& path) {
        static AutonomousIDEInterface interface;
        if (!interface.IsReady()) {
            interface.Initialize("d:/rawrxd");
        }
        return interface.AnalyzeCodebase(path);
    }
    
    inline std::string QuickRefactor(const std::string& file, const std::string& goal) {
        static AutonomousIDEInterface interface;
        if (!interface.IsReady()) {
            interface.Initialize("d:/rawrxd");
        }
        return interface.RefactorCode(file, goal);
    }
    
    inline std::string QuickOptimize(const std::string& target = "") {
        static AutonomousIDEInterface interface;
        if (!interface.IsReady()) {
            interface.Initialize("d:/rawrxd");
        }
        return interface.OptimizePerformance(target);
    }
    
    inline void EnableMaxMode() {
        static AutonomousIDEInterface interface;
        if (!interface.IsReady()) {
            interface.Initialize("d:/rawrxd");
        }
        interface.EnableMaxAutonomy(true);
        interface.SetSafetyLevel(0); // Unrestricted
        interface.EnableContinuousImprovement(true);
    }
}

} // namespace RawrXD