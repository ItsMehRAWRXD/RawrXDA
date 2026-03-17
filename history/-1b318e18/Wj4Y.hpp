#pragma once
#include <QObject>
#include <QJsonObject>
#include <memory>
#include <atomic>

class UniversalModelRouter;
class ToolRegistry;

namespace RawrXD {
class PlanOrchestrator;
struct ExecutionResult;
}
class Logger;
class Metrics;

/**
 * Enterprise-grade autonomous agent facade built on existing RawrXD systems.
 * Thread-safe, RAII, and instrumentation-ready. Uses synchronous PlanOrchestrator
 * and ToolRegistry while emitting Qt-friendly streaming signals.
 */
class ZeroDayAgenticEngine : public QObject {
    Q_OBJECT
public:
    explicit ZeroDayAgenticEngine(UniversalModelRouter* router,
                                  ToolRegistry* tools,
                                  PlanOrchestrator* planner,
                                  QObject* parent = nullptr);
    ~ZeroDayAgenticEngine();

    // Fire-and-forget mission start. Runs asynchronously without blocking UI.
    void startMission(const QString& userGoal);

    // Graceful abort; safe from any thread.
    void abortMission();

signals:
    void agentStream(const QString& token);
    void agentComplete(const QString& summary);
    void agentError(const QString& error);

private:
    struct Impl;
    std::unique_ptr<Impl> d;
};
