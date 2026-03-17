#pragma once

// C++20, no Qt. Enterprise autonomous agent facade; callbacks replace signals.

#include <string>
#include <memory>
#include <atomic>
#include <functional>

class UniversalModelRouter;
class ToolRegistry;
namespace RawrXD { class PlanOrchestrator; }
class Logger;
class Metrics;

class ZeroDayAgenticEngine
{
public:
    using AgentStreamFn  = std::function<void(const std::string& token)>;
    using AgentCompleteFn = std::function<void(const std::string& summary)>;
    using AgentErrorFn   = std::function<void(const std::string& error)>;

    explicit ZeroDayAgenticEngine(UniversalModelRouter* router,
                                  ToolRegistry* tools,
                                  RawrXD::PlanOrchestrator* planner);
    ~ZeroDayAgenticEngine();

    void setOnAgentStream(AgentStreamFn f)   { m_onStream = std::move(f); }
    void setOnAgentComplete(AgentCompleteFn f) { m_onComplete = std::move(f); }
    void setOnAgentError(AgentErrorFn f)     { m_onError = std::move(f); }

    void startMission(const std::string& userGoal);
    void abortMission();

private:
    struct Impl;
    std::unique_ptr<Impl> d;
    AgentStreamFn   m_onStream;
    AgentCompleteFn m_onComplete;
    AgentErrorFn    m_onError;
};
