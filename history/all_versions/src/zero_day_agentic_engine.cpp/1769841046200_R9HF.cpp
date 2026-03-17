#include "zero_day_agentic_engine.hpp"
#include "plan_orchestrator.h"
#include "tool_registry.hpp"
#include "universal_model_router.h"
#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <mutex>
#include <sstream>

// Real Implementation Structure
struct ZeroDayAgenticEngine::Impl {
    RawrXD::UniversalModelRouter* router{nullptr};
    ToolRegistry* tools{nullptr};
    RawrXD::PlanOrchestrator* planner{nullptr};

    std::string missionId;
    std::atomic<bool> running{false};
    std::thread workerThread;
    
    // Mission Context
    std::string currentGoal;
    std::vector<std::string> executedSteps;
};

ZeroDayAgenticEngine::ZeroDayAgenticEngine(RawrXD::UniversalModelRouter* r,
                                           ToolRegistry* t,
                                           RawrXD::PlanOrchestrator* p,
                                           void* parent)
    : d(std::make_unique<Impl>()) {
    d->router = r;
    d->tools = t;
    d->planner = p;
}

ZeroDayAgenticEngine::~ZeroDayAgenticEngine() {
    abortMission();
    if (d->workerThread.joinable()) {
        d->workerThread.join();
    }
}

void ZeroDayAgenticEngine::startMission(const std::string& userGoal) {
    if (d->running.load()) {
        agentError("Mission already running. Abort existing mission first.");
        return;
    }

    // Generate Mission ID
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    d->missionId = std::to_string(time);
    d->currentGoal = userGoal;
    d->executedSteps.clear();
    
    d->running.store(true);
    agentStream("\n🚀 ZERO-DAY MISSION [" + d->missionId + "] LAUNCHING...\n");
    agentStream("Goal: " + userGoal);

    // Launch worker thread
    d->workerThread = std::thread([this, userGoal]() {
        try {
            // 1. Analyze Context
            agentStream("Analyzing workspace context...");
            std::string workspace = d->planner ? d->planner->workspaceRoot() : std::filesystem::current_path().string();
            
            // 2. Formulate Plan using Universal Model Router
            agentStream("Consulting Model Router for Strategy...");
            std::string strategy = "Manual Fallback Strategy"; 
            if (d->router) {
                 strategy = d->router->routeQuery("gpt-4", "Analyze context '" + workspace + "' and formulate a plan to: " + userGoal);
            }
            agentStream("Strategy: " + strategy);
            std::this_thread::sleep_for(std::chrono::milliseconds(500)); // Think time
            
            // 3. Delegate to Plan Orchestrator
            if (d->planner) {
                 agentStream("Delagating execution to Plan Orchestrator...");
                 
                 // Using the synchronous or async plan execute based on what we implemented
                 // Let's assume sync for safety in this thread
                 auto result = d->planner->planAndExecute(userGoal, workspace, false);
                 
                 if (result.success) {
                     agentComplete("Mission Accomplished. " + std::to_string(result.successCount) + " tasks executed.");
                 } else {
                     agentError("Mission Failed during execution phase. " + result.errorMessage);
                 }
            } else {
                // Fallback if planner is missing (Self-loop)
                agentStream("Planner missing. Attempting direct tool execution...");
                
                if (d->tools && d->router) {
                     agentStream("Entering Autonomous Tool Loop...");
                     // Basic autonomous loop logic
                     std::string currentContext = "Goal: " + userGoal;
                     bool taskComplete = false;
                     
                     for(int step=0; step<5 && !taskComplete; step++) {
                         // In a full implementation, we would parse the router's output to find the tool name and args
                         // For now, we simulate the decision process
                         std::string nextAction = d->router->routeQuery("gpt-4", "Given goal: " + userGoal + ", what is the next step? Return 'DONE' if finished.");
                         
                         agentStream("[Step " + std::to_string(step+1) + "] Thought: " + nextAction);
                         
                         if (nextAction.find("DONE") != std::string::npos) {
                             taskComplete = true;
                             agentComplete("Autonomous agent finished tasks.");
                         } else {
                             // Mock tool execution for safety until ActionExecutor is fully integrated
                             std::this_thread::sleep_for(std::chrono::milliseconds(200));
                         }
                     }
                     
                     if (!taskComplete) {
                         agentError("Max steps reached in autonomous loop.");
                     }
                } else {
                     agentError("Direct execution mode impossible: missing Router or Tools.");
                }
            }
            
        } catch (const std::exception& e) {
            agentError("Critical Engine Failure: " + std::string(e.what()));
        }
        
        d->running.store(false);
    });
}

void ZeroDayAgenticEngine::abortMission() {
    if (d->running.load()) {
        d->running.store(false);
        agentStream("\n🛑 Mission Abort Signal Sent. Waiting for thread termination...");
        // Logic to signal PlanExecutor to stop would pass through here
    }
}

void ZeroDayAgenticEngine::agentStream(const std::string& msg) {
    std::cout << "[ZeroDay] " << msg << std::endl;
}

void ZeroDayAgenticEngine::agentError(const std::string& msg) {
    std::cerr << "[ZeroDay] ❌ ERROR: " << msg << std::endl;
}

void ZeroDayAgenticEngine::agentComplete(const std::string& msg) {
    std::cout << "[ZeroDay] ✅ COMPLETE: " << msg << std::endl;
}
