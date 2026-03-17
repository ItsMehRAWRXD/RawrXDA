#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include "action_executor.h"

struct PlanningTask {
    std::string id;
    std::string title;
    std::string status; // "pending", "in-progress", "completed", "failed"
    int priority;
    Action associatedAction;
};

class PlanningAgent {
public:
    explicit PlanningAgent();
    ~PlanningAgent();
    
    void initialize();
    
    // High level operations
    void createPlan(const std::string& goal);
    void executePlan();
    
    // Task management
    void addTask(const PlanningTask& task);
    std::vector<PlanningTask> getTasks() const;
    
    // Status
    bool isComplete() const;
    std::string generateSummary() const;

private:
    std::string generateUUID();
    std::vector<PlanningTask> m_tasks;
    std::unique_ptr<ActionExecutor> m_executor;
    
    // Internal logic for plan generation (template based for now)
    void generateCodePlan(const std::string& goal);
    void generateDebugPlan(const std::string& goal);
    void generateGenericPlan(const std::string& goal);
};
