#pragma once

#include <string>
#include <memory>
#include <vector>

class ToolRegistry;
namespace RawrXD { 
    class PlanOrchestrator; 
    class UniversalModelRouter;
}

class ZeroDayAgenticEngine {
public:
    explicit ZeroDayAgenticEngine(RawrXD::UniversalModelRouter* router = nullptr,
                                  ToolRegistry* tools = nullptr,
                                  RawrXD::PlanOrchestrator* planner = nullptr,
                                  void* parent = nullptr);
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
