#pragma once

#include <string>
#include <vector>
#include <memory>

namespace RawrXD {
namespace Backend {

// Plan orchestrator for managing agentic execution plans
class PlanOrchestrator {
public:
    PlanOrchestrator();
    ~PlanOrchestrator();
    
    // Initialize the orchestrator
    bool Initialize();
    
    // Create a new execution plan
    std::string CreatePlan(const std::string& description);
    
    // Execute a plan
    bool ExecutePlan(const std::string& plan_id);
    
    // Execute the last plan
    bool ExecuteLastPlan();
    
    // Get plan status
    std::string GetPlanStatus(const std::string& plan_id) const;
    
    // Cancel a plan
    bool CancelPlan(const std::string& plan_id);
    
    // Create plan (alias for CreatePlan)
    std::string createPlan(const std::string& description) { return CreatePlan(description); }
    
    // Execute last plan (alias for ExecuteLastPlan)
    bool executeLastPlan() { return ExecuteLastPlan(); }
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace Backend
} // namespace RawrXD