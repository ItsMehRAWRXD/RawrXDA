#include "planning_agent.h"
#include <iostream>
#include <sstream>
#include <chrono>
#include <random>

// Windows UUID
#ifdef _WIN32
#include <rpc.h>
#pragma comment(lib, "Rpcrt4.lib")
#endif

PlanningAgent::PlanningAgent() {
    m_executor = std::make_unique<ActionExecutor>();
}

PlanningAgent::~PlanningAgent() {
}

void PlanningAgent::initialize() {
    // Setup initial state if needed
}

std::string PlanningAgent::generateUUID() {
#ifdef _WIN32
    UUID uuid;
    UuidCreate(&uuid);
    unsigned char* str;
    UuidToStringA(&uuid, &str);
    std::string s((char*)str);
    RpcStringFreeA(&str);
    return s;
#else
    return "uuid-" + std::to_string(std::rand());
#endif
}

void PlanningAgent::createPlan(const std::string& goal) {
    m_tasks.clear();
    
    // Basic keyword analysis to determine plan strategy
    // In a real system, this would call an LLM
    std::string lowerGoal = goal;
    std::transform(lowerGoal.begin(), lowerGoal.end(), lowerGoal.begin(), ::tolower);
    
    if (lowerGoal.find("code") != std::string::npos || lowerGoal.find("implement") != std::string::npos) {
        generateCodePlan(goal);
    } else if (lowerGoal.find("debug") != std::string::npos || lowerGoal.find("fix") != std::string::npos) {
        generateDebugPlan(goal);
    } else {
        generateGenericPlan(goal);
    }
}

void PlanningAgent::addTask(const PlanningTask& task) {
    m_tasks.push_back(task);
}

std::vector<PlanningTask> PlanningAgent::getTasks() const {
    return m_tasks;
}

void PlanningAgent::executePlan() {
    ExecutionContext ctx;
    ctx.projectRoot = "D:\\rawrxd"; // Default
    m_executor->setContext(ctx);
    
    for (auto& task : m_tasks) {
        if (task.status == "completed") continue;
        
        task.status = "in-progress";
        
        // Execute the action associated with the task
        bool success = false;
        if (task.associatedAction.type != ActionType::Unknown) {
            success = m_executor->executeAction(task.associatedAction);
        } else {
            // Simulated success for abstract tasks
            success = true; 
        }
        
        if (success) {
            task.status = "completed";
        } else {
            task.status = "failed";
            // Stop on failure?
            // break;
        }
    }
}

// Plan Generators logic - converting abstract goals into concrete Actions where possible

void PlanningAgent::generateCodePlan(const std::string& goal) {
    // 1. Analyze
    PlanningTask t1;
    t1.id = generateUUID();
    t1.title = "Analyze Requirements";
    t1.status = "pending";
    t1.associatedAction.type = ActionType::SearchFiles; // Search for context
    t1.associatedAction.params["query"] = goal;
    m_tasks.push_back(t1);
    
    // 2. Implement
    PlanningTask t2;
    t2.id = generateUUID();
    t2.title = "Implement Code";
    t2.status = "pending";
    t2.associatedAction.type = ActionType::FileEdit; // Placeholder for edit
    t2.associatedAction.description = "Implement: " + goal;
    m_tasks.push_back(t2);
    
    // 3. Build Not included in default plan to avoid errors, 
    // but user can add it.
}

void PlanningAgent::generateDebugPlan(const std::string& goal) {
     // 1. Reproduce (Search)
    PlanningTask t1;
    t1.id = generateUUID();
    t1.title = "Search for Error Context";
    t1.status = "pending";
    t1.associatedAction.type = ActionType::SearchFiles; 
    t1.associatedAction.params["query"] = goal;
    m_tasks.push_back(t1);
}

void PlanningAgent::generateGenericPlan(const std::string& goal) {
    PlanningTask t1;
    t1.id = generateUUID();
    t1.title = "Research Strategy";
    t1.status = "pending";
    t1.associatedAction.type = ActionType::Unknown; // Abstract
    m_tasks.push_back(t1);
}

bool PlanningAgent::isComplete() const {
    for (const auto& task : m_tasks) {
        if (task.status != "completed") return false;
    }
    return !m_tasks.empty();
}

std::string PlanningAgent::generateSummary() const {
    std::stringstream ss;
    ss << "Plan Status:\n";
    for (const auto& task : m_tasks) {
        ss << " - " << task.title << ": " << task.status << "\n";
    }
    return ss.str();
}
