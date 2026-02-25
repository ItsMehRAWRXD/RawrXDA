#pragma once

/**
 * @file proactive_agent_engine.hpp
 * @brief Proactive agent engine — forward decl stub (Qt impl in Win32_IDE_Complete).
 */

class UniversalModelRouter;
class ToolRegistry;
namespace RawrXD { class PlanOrchestrator; }

class ProactiveAgentEngine {
public:
    explicit ProactiveAgentEngine(UniversalModelRouter* router,
                                  ToolRegistry* tools,
                                  RawrXD::PlanOrchestrator* planner,
                                  void* parent = nullptr);
    ~ProactiveAgentEngine();
private:
    void* m_parent = nullptr;
};
