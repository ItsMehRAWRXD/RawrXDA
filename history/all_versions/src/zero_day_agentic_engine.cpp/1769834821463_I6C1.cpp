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

// Stub logger/metrics if needed, or just use cout
// implementation details
struct ZeroDayAgenticEngine::Impl {
    UniversalModelRouter* router{nullptr};
    ToolRegistry* tools{nullptr};
    RawrXD::PlanOrchestrator* planner{nullptr};

    std::string missionId;
    std::atomic<bool> running{false};
    std::thread workerThread;
};

ZeroDayAgenticEngine::ZeroDayAgenticEngine(UniversalModelRouter* r,
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
        std::cerr << "[ZeroDay] Mission already running." << std::endl;
        return;
    }

    // Generate simple ID
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    d->missionId = std::to_string(time);
    
    d->running.store(true);
    agentStream("\n🚀 Mission " + d->missionId + " started.\n");

    // Launch thread
    d->workerThread = std::thread([this, userGoal]() {
        std::string workspace = d->planner ? d->planner->workspaceRoot() : std::filesystem::current_path().string();
        
        if (d->planner) {
             std::cout << "[ZeroDay] Planning for: " << userGoal << " in " << workspace << std::endl;
             
             auto result = d->planner->planAndExecute(userGoal, workspace, false);
             bool success = result.success;
             
             if (d->running.load()) {
                if (success) {
                    std::string summary = "Mission " + d->missionId + " finished.\n";
                    summary += "Successes: " + std::to_string(result.successCount) + ", Failures: " + std::to_string(result.failureCount);
                    agentComplete(summary);
                } else {
                    agentError("Mission failed: " + result.errorMessage);
                }
             }
        } else {
            agentError("Planner not available.");
        }
        d->running.store(false);
    });
    
    // Detach? No, we join in dtor or store it. 
    // But if we start multiple times, we need to join previous.
    // Simplified: one mission at a time, thread stored in d.
    d->workerThread.detach(); // For simplicity in this reconstruction to avoid blocking/zombies if we don't track carefully.
}

void ZeroDayAgenticEngine::abortMission() {
    d->running.store(false);
    agentStream("\n🛑 Mission aborted.\n");
}

void ZeroDayAgenticEngine::agentStream(const std::string& msg) {
    std::cout << "[ZeroDay] " << msg << std::endl;
}

void ZeroDayAgenticEngine::agentError(const std::string& msg) {
    std::cerr << "[ZeroDay] ERROR: " << msg << std::endl;
}

void ZeroDayAgenticEngine::agentComplete(const std::string& msg) {
    std::cout << "[ZeroDay] COMPLETE: " << msg << std::endl;
}
