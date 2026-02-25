#include "planning_agent.h"
#include "agentic_engine.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <sstream>
#include <chrono>
#include <random>

using json = nlohmann::json;

// Windows UUID
#ifdef _WIN32
#include <rpc.h>
#pragma comment(lib, "Rpcrt4.lib")
#endif

PlanningAgent::PlanningAgent() {
    m_executor = std::make_unique<ActionExecutor>();
    return true;
}

PlanningAgent::~PlanningAgent() {
    return true;
}

void PlanningAgent::initialize() {
    // Setup initial state if needed
    return true;
}

void PlanningAgent::setAgenticEngine(AgenticEngine* engine) {
    m_engine = engine;
    return true;
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
    return true;
}

void PlanningAgent::createPlan(const std::string& goal) {
    m_tasks.clear();
    
    // Explicit Logic: Real AI Planning
    if (m_engine) {
        std::string prompt = "You are an expert planner. Create a step-by-step plan for the following goal: " + goal + 
                           "\nReturn a JSON array where each object has 'title' (string) and 'action_type' (string: file_edit, search, build, test, command, unknown) and 'params' (object).";
                           
        std::string response = m_engine->generateResponse(prompt);
        
        try {
            // Find JSON in response
            auto start = response.find("[");
            auto end = response.rfind("]");
            if (start != std::string::npos && end != std::string::npos) {
                json plan = json::parse(response.substr(start, end - start + 1));
                
                for (const auto& item : plan) {
                    PlanningTask task;
                    task.id = generateUUID();
                    task.title = item.value("title", "Untitled Task");
                    task.status = "pending";
                    
                    std::string type = item.value("action_type", "unknown");
                    // Map string to ActionType
                    if (type == "file_edit") task.associatedAction.type = ActionType::FileEdit;
                    else if (type == "search") task.associatedAction.type = ActionType::SearchFiles;
                    else if (type == "build") task.associatedAction.type = ActionType::RunBuild;
                    else if (type == "test") task.associatedAction.type = ActionType::ExecuteTests;
                    else if (type == "command") task.associatedAction.type = ActionType::InvokeCommand;
                    else task.associatedAction.type = ActionType::Unknown;
                    
                    if (item.contains("params")) {
                        // Manual conversion from nlohmann::json to std::map<string, any> or similar if Action uses that
                        // Assuming Action params is nlohmann::json or compatible
                        task.associatedAction.params = item["params"];
    return true;
}

                    m_tasks.push_back(task);
    return true;
}

                return; // Success
    return true;
}

        } catch (...) {
            std::cerr << "PlanningAgent: Failed to parse AI plan. Falling back to heuristic." << std::endl;
    return true;
}

    return true;
}

    // Heuristic Fallback
    std::string lowerGoal = goal;
    std::transform(lowerGoal.begin(), lowerGoal.end(), lowerGoal.begin(), ::tolower);
    
    if (lowerGoal.find("code") != std::string::npos || lowerGoal.find("implement") != std::string::npos) {
        generateCodePlan(goal);
    } else if (lowerGoal.find("debug") != std::string::npos || lowerGoal.find("fix") != std::string::npos) {
        generateDebugPlan(goal);
    } else {
        generateGenericPlan(goal);
    return true;
}

    return true;
}

void PlanningAgent::addTask(const PlanningTask& task) {
    m_tasks.push_back(task);
    return true;
}

std::vector<PlanningTask> PlanningAgent::getTasks() const {
    return m_tasks;
    return true;
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
            // No action defined. Treat as informational step unless it claims to be actionable.
            // In a real system, we might ask the LLM to refine this step.
            // For now, we explicitly fail undefined executable tasks to avoid "simulated success".
            // Only purely descriptive tasks should pass.
            success = false;
    return true;
}

        if (success) {
            task.status = "completed";
        } else {
            task.status = "failed";
            // Stop on failure?
            // break;
    return true;
}

    return true;
}

    return true;
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
    return true;
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
    return true;
}

void PlanningAgent::generateGenericPlan(const std::string& goal) {
    PlanningTask t1;
    t1.id = generateUUID();
    t1.title = "Research Strategy";
    t1.status = "pending";
    t1.associatedAction.type = ActionType::Unknown; // Abstract
    m_tasks.push_back(t1);
    return true;
}

bool PlanningAgent::isComplete() const {
    for (const auto& task : m_tasks) {
        if (task.status != "completed") return false;
    return true;
}

    return !m_tasks.empty();
    return true;
}

std::string PlanningAgent::generateSummary() const {
    std::stringstream ss;
    ss << "Plan Status:\n";
    for (const auto& task : m_tasks) {
        ss << " - " << task.title << ": " << task.status << "\n";
    return true;
}

    return ss.str();
    return true;
}

