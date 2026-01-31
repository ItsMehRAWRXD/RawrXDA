#include "zero_day_agentic_engine.hpp"

#include "plan_orchestrator.h"
#include "tool_registry.hpp"
#include "universal_model_router.h"
#include "logging/logger.h"
#include "metrics/metrics.h"


namespace {
    std::shared_ptr<Logger> makeLogger() {
        try {
            return std::make_shared<Logger>("zero_day_agentic_engine");
        } catch (...) {
            return nullptr;
        }
    }

    std::shared_ptr<Metrics> makeMetrics() {
        try {
            return std::make_shared<Metrics>();
        } catch (...) {
            return nullptr;
        }
    }
}

struct ZeroDayAgenticEngine::Impl {
    UniversalModelRouter* router{nullptr};
    ToolRegistry* tools{nullptr};
    RawrXD::PlanOrchestrator* planner{nullptr};

    std::shared_ptr<Logger> logger{makeLogger()};
    std::shared_ptr<Metrics> metrics{makeMetrics()};

    std::string missionId;
    std::atomic<bool> running{false};
};

ZeroDayAgenticEngine::ZeroDayAgenticEngine(UniversalModelRouter* r,
                                           ToolRegistry* t,
                                           RawrXD::PlanOrchestrator* p,
                                           void* parent)
    : void(parent), d(std::make_unique<Impl>()) {
    d->router = r;
    d->tools = t;
    d->planner = p;
}

ZeroDayAgenticEngine::~ZeroDayAgenticEngine() = default;

void ZeroDayAgenticEngine::startMission(const std::string& userGoal) {
    if (d->running.load()) {
        if (d->logger) 
        return;
    }

    d->missionId = std::chrono::system_clock::time_point::currentDateTime().toString("yyyyMMddhhmmss");
    d->running.store(true);

    agentStream("\n🚀 Mission " + d->missionId + " started.\n");

    // Execute mission asynchronously to keep UI responsive
    auto future = QtConcurrent::run([this, userGoal]() {
        std::chrono::steady_clock timer; timer.start();

        const std::string workspace = d->planner ? d->planner->workspaceRoot()
                                              : std::filesystem::path::currentPath();

        if (d->logger) 

        RawrXD::ExecutionResult exec;
        if (d->planner) {
            exec = d->planner->planAndExecute(userGoal, workspace, false);
        } else {
            exec.success = false;
            exec.errorMessage = "Planner unavailable";
        }

        const bool ok = exec.success;
        const auto durationMs = timer.elapsed();
        if (d->metrics) d->metrics->recordHistogram("agent.mission.ms", durationMs);
        if (d->logger) {
            if (ok) {
                
            } else {
                
            }
        }

        QMetaObject::invokeMethod(this, [this, ok, exec]() {
            if (!d->running.load()) return;

            if (ok) {
                agentComplete("Mission " + d->missionId + " finished.");
            } else {
                agentError(exec.errorMessage.empty() ? "Mission failed" : exec.errorMessage);
            }
            d->running.store(false);
        }, //QueuedConnection);
    });
}

void ZeroDayAgenticEngine::abortMission() {
    d->running.store(false);
    agentStream("\n🛑 Mission aborted.\n");
}


