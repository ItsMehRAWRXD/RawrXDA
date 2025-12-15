#include "zero_day_agentic_engine.hpp"

#include "plan_orchestrator.h"
#include "tool_registry.hpp"
#include "universal_model_router.h"
#include "logging/logger.h"
#include "metrics/metrics.h"

#include <QtConcurrent/QtConcurrent>
#include <QMetaObject>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>

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

    QString missionId;
    std::atomic<bool> running{false};
};

ZeroDayAgenticEngine::ZeroDayAgenticEngine(UniversalModelRouter* r,
                                           ToolRegistry* t,
                                           RawrXD::PlanOrchestrator* p,
                                           QObject* parent)
    : QObject(parent), d(std::make_unique<Impl>()) {
    d->router = r;
    d->tools = t;
    d->planner = p;
}

ZeroDayAgenticEngine::~ZeroDayAgenticEngine() = default;

void ZeroDayAgenticEngine::startMission(const QString& userGoal) {
    if (d->running.load()) {
        if (d->logger) d->logger->warn("Mission already running");
        return;
    }

    d->missionId = QDateTime::currentDateTime().toString("yyyyMMddhhmmss");
    d->running.store(true);

    emit agentStream("\n🚀 Mission " + d->missionId + " started.\n");

    // Execute mission asynchronously to keep UI responsive
    auto future = QtConcurrent::run([this, userGoal]() {
        QElapsedTimer timer; timer.start();

        const QString workspace = d->planner ? d->planner->workspaceRoot()
                                              : QDir::currentPath();

        if (d->logger) d->logger->info("Planning mission: {}", userGoal.toStdString());

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
                d->logger->info("Mission complete in {} ms", durationMs);
            } else {
                d->logger->error("Mission failed in {} ms: {}", durationMs, exec.errorMessage.toStdString());
            }
        }

        QMetaObject::invokeMethod(this, [this, ok, exec]() {
            if (!d->running.load()) return;

            if (ok) {
                emit agentComplete("Mission " + d->missionId + " finished.");
            } else {
                emit agentError(exec.errorMessage.isEmpty() ? QStringLiteral("Mission failed") : exec.errorMessage);
            }
            d->running.store(false);
        }, Qt::QueuedConnection);
    });
}

void ZeroDayAgenticEngine::abortMission() {
    d->running.store(false);
    emit agentStream("\n🛑 Mission aborted.\n");
}
