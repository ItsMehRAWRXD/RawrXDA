#pragma once

#include <QObject>
#include <QString>
#include <QJsonArray>
#include <QJsonObject>
#include <QPointer>
#include "tool_registry.h"
#include "model_router.h"
#include "ai_model_client.h"

class AwsBedrockBridge;

/**
 * @brief AgenticPlanner - Multi-step planning, execution, and review engine
 * 
 * Ported from agentic_loop.asm MASM implementation.
 * Implements the 3-phase agentic loop: Planning -> Executing -> Reviewing.
 */
class AgenticPlanner : public QObject {
    Q_OBJECT

public:
    enum AgentState {
        STATE_IDLE,
        STATE_PLANNING,
        STATE_EXECUTING,
        STATE_REVIEWING,
        STATE_ERROR
    };

    explicit AgenticPlanner(MasmToolRegistry* toolRegistry, ModelRouter* modelRouter, AIModelClient* aiClient, QObject* parent = nullptr);
    ~AgenticPlanner();

    // Main entry point (from MASM AgenticLoop_ExecuteTask)
    void executeTask(const QString& task);

    // State management
    AgentState getState() const { return m_state; }
    int getCurrentStepIndex() const { return m_currentStepIndex; }
    int getToolCallCount() const { return m_toolCallCount; }

    // Configuration
    void setMaxToolCalls(int max) { m_maxToolCalls = max; }
    void setMaxPlanSteps(int max) { m_maxPlanSteps = max; }

signals:
    void stateChanged(AgentState newState);
    void planGenerated(const QJsonArray& plan);
    void stepStarted(int index, const QJsonObject& step);
    void stepFinished(int index, const QJsonObject& result);
    void taskFinished(const QString& finalResult);
    void errorOccurred(const QString& error);
    void logMessage(const QString& message);

private slots:
    void onAIResponse(const QString& response);
    void onAIError(const QString& error);

private:
    // Phases (from MASM AgenticLoop_GeneratePlan, AgenticLoop_ExecuteStep, AgenticLoop_ReviewResult)
    void generatePlan(const QString& task);
    void executeStep(const QJsonObject& step);
    void reviewResult(const QJsonArray& plan, const QJsonObject& finalResult);
    void handleStepFailure(int stepIndex, const QJsonObject& failureResult);

    // Dependencies
    QPointer<MasmToolRegistry> m_toolRegistry;
    QPointer<ModelRouter> m_modelRouter;
    QPointer<AIModelClient> m_aiClient;
    AwsBedrockBridge* m_bedrockBridge;

    // State
    AgentState m_state{STATE_IDLE};
    QJsonArray m_currentPlan;
    int m_currentStepIndex{0};
    int m_toolCallCount{0};
    QJsonObject m_lastResult;
    QString m_originalTask;

    // Limits (from MASM)
    int m_maxToolCalls{50};
    int m_maxPlanSteps{10};

    // Helper methods
    void setState(AgentState state);
    QString buildPlanningPrompt(const QString& task);
    QString buildCorrectionPrompt(int stepIndex, const QJsonObject& failure);
    QString buildReviewPrompt(const QJsonArray& plan, const QJsonObject& result);
};
