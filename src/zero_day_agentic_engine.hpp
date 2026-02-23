// SCAFFOLD_348: Zero day agentic engine void*

#pragma once

#include <string>
#include <memory>
#include <vector>

namespace RawrXD { 
    class ToolRegistry;
    class PlanOrchestrator; 
    class UniversalModelRouter;
}

class ZeroDayAgenticEngine {
public:
    explicit ZeroDayAgenticEngine(RawrXD::UniversalModelRouter* router = nullptr,
                                  RawrXD::ToolRegistry* tools = nullptr,
                                  RawrXD::PlanOrchestrator* planner = nullptr,
                                  void* parent = nullptr);  // Win32: HWND when provided
    ~ZeroDayAgenticEngine();

    void startMission(const std::string& userGoal);
    void abortMission();

private:
    struct Impl;
    std::unique_ptr<Impl> d;
    
    // Internal helpers
    void agentStream(const std::string& msg);
    void agentError(const std::string& msg);
    void agentComplete(const std::string& msg);
};
