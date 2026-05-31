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
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// Real Implementation Structure
struct ZeroDayAgenticEngine::Impl {
    RawrXD::UniversalModelRouter* router{nullptr};
    RawrXD::ToolRegistry* tools{nullptr};
    RawrXD::PlanOrchestrator* planner{nullptr};

    std::string missionId;
    std::atomic<bool> running{false};
    std::thread workerThread;
    
    // Mission Context
    std::string currentGoal;
    std::vector<std::string> executedSteps;
};

ZeroDayAgenticEngine::ZeroDayAgenticEngine(RawrXD::UniversalModelRouter* r,
                                           RawrXD::ToolRegistry* t,
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
            if (!d->running.load()) return;
            agentStream("Analyzing workspace context...");
            // Use current path if planner doesn't provide root (assumed missing getter fallback)
            std::string workspace = std::filesystem::current_path().string();
            
            // 2. Formulate Plan using Universal Model Router
            if (!d->running.load()) return;
            agentStream("Consulting Model Router for Strategy...");
            std::string strategy = "Manual Fallback Strategy"; 
            if (d->router) {
                 strategy = d->router->routeQuery("gpt-4", "Analyze context '" + workspace + "' and formulate a plan to: " + userGoal);
            }
            agentStream("Strategy: " + strategy);
            std::this_thread::sleep_for(std::chrono::milliseconds(500)); 
            
            // 3. Delegate to Plan Orchestrator
            if (d->planner) {
                 if (!d->running.load()) return;
                 agentStream("Delagating execution to Plan Orchestrator...");
                 
                 // Using planAndExecute
                 auto result = d->planner->planAndExecute(userGoal, workspace, false);
                 
                 if (result.success) {
                     agentComplete("Mission Accomplished properly. " + std::to_string(result.successCount) + " tasks executed.");
                     for(const auto& f : result.successfulFiles) d->executedSteps.push_back("Edited: " + f);
                 } else {
                     agentError("Mission Failed during execution phase. " + result.errorMessage);
                 }
            } else {
                // Fallback if planner is missing (Self-loop)
                // This is the "Zero Day" Autonomous mode logic
                if (!d->running.load()) return;
                agentStream("Planner missing. Entering Zero-Day Autonomous Mode...");
                
                if (d->tools && d->router) {
                     // Build tool schemas context
                     json toolsJson = json::array();
                     for (const auto& tName : d->tools->getRegisteredTools()) {
                         auto tDef = d->tools->getTool(tName);
                         if (tDef) {
                             json schema;
                             schema["name"] = tDef->metadata.name;
                             schema["description"] = tDef->metadata.description;
                             
                             // Reconstruct JSON schema from metadata arguments
                             json props = json::object();
                             json req = json::array();
                             for(const auto& arg : tDef->metadata.arguments) {
                                 json p;
                                 p["description"] = arg.description;
                                 p["type"] = "string"; 
                                 // Basic mapping
                                 // if (arg.type == RawrXD::ToolArgType::INTEGER) p["type"] = "integer";
                                 props[arg.name] = p;
                                 if (arg.required) req.push_back(arg.name);
                             }
                             
                             schema["parameters"] = {
                                 {"type", "object"},
                                 {"properties", props},
                                 {"required", req}
                             };
                             
                             toolsJson.push_back(schema);
                         }
                     }
                     
                     std::string conversation = "Goal: " + userGoal + "\n"
                        "Available Tools: " + toolsJson.dump() + "\n"
                        "Instructions: You are an autonomous agent. "
                        "Reply with a JSON object containing { \"tool\": \"tool_name\", \"args\": { ... } } to execute a tool, "
                        "or { \"tool\": \"DONE\", \"args\": {} } if finished.\n";

                     bool taskComplete = false;
                     int maxSteps = 15; // Zero-Day Limit
                     
                     for(int step=0; step<maxSteps && !taskComplete && d->running.load(); step++) {
                         std::string prompt = conversation + "\nContext Step " + std::to_string(step+1) + ": What is the next action?";
                         std::string response = d->router->routeQuery("gpt-4", prompt);
                         
                         agentStream("[Step " + std::to_string(step+1) + "] Thought: " + response);
                         
                         std::string jsonStr = response;
                         size_t start = response.find("{");
                         size_t end = response.rfind("}");
                         if (start != std::string::npos && end != std::string::npos) jsonStr = response.substr(start, end - start + 1);

                         try {
                             auto action = json::parse(jsonStr);
                             std::string toolName = action.value("tool", "");
                             
                             if (toolName == "DONE") {
                                 taskComplete = true;
                                 agentComplete("Autonomous agent finished tasks.");
                             } else if (!toolName.empty()) {
                                 agentStream("Executing Tool: " + toolName);
                                 json args = action.value("args", json::object());
                                 
                                 RawrXD::ToolResult result = d->tools->executeTool(toolName, args);
                                 std::string toolOutput = result.success ? result.output : ("Error: " + result.error);
                                 
                                 d->executedSteps.push_back("Tool: " + toolName + " | Result: " + (result.success ? "Success" : "Fail"));
                                 if (toolOutput.length() > 500) toolOutput = toolOutput.substr(0, 500) + "... (truncated)";
                                 
                                 agentStream("Result: " + toolOutput);
                                 conversation += "\n[Action] " + toolName + " -> " + toolOutput;
                             }
                         } catch(...) {
                             conversation += "\n[System] Error: JSON Parse Failed.";
                             agentError("JSON Parse fail.");
                         }
                         std::this_thread::sleep_for(std::chrono::milliseconds(200));
                     }
                     if (!taskComplete && d->running.load()) agentError("Max steps reached in autonomous loop.");
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
        
        // Propagate abort to planner if active
        if (d->planner) {
            d->planner->abort();
        }
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
