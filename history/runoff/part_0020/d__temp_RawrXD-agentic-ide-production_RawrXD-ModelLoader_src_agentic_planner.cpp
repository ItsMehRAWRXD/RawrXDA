#include "agentic_planner.h"
#include <QJsonDocument>
#include <QDebug>

AgenticPlanner::AgenticPlanner(ToolRegistry* toolRegistry, ModelRouter* modelRouter, QObject* parent)
    : QObject(parent), m_toolRegistry(toolRegistry), m_modelRouter(modelRouter)
{
}

AgenticPlanner::~AgenticPlanner() = default;

void AgenticPlanner::executeTask(const QString& task)
{
    // From MASM AgenticLoop_ExecuteTask
    if (task.isEmpty()) {
        emit errorOccurred("Task is null");
        return;
    }

    m_originalTask = task;
    m_toolCallCount = 0;
    m_currentStepIndex = 0;
    m_currentPlan = QJsonArray();

    emit logMessage("Starting task: " + task);
    generatePlan(task);
}

void AgenticPlanner::generatePlan(const QString& task)
{
    // From MASM AgenticLoop_GeneratePlan
    setState(STATE_PLANNING);
    QString prompt = buildPlanningPrompt(task);
    
    emit logMessage("Generating execution plan...");
    
    // In a real implementation, we would call ModelRouter::selectPrimaryModel()
    // and send the prompt to the InferenceEngine.
    // For now, we emit a signal that the integration layer can catch.
    // emit requestModelCall(prompt, "planning");
    
    // MOCK for testing:
    // In a real test, the backend would call back with a JSON plan.
}

void AgenticPlanner::executeStep(const QJsonObject& step)
{
    // From MASM AgenticLoop_ExecuteStep
    if (m_state != STATE_EXECUTING) setState(STATE_EXECUTING);

    int stepNum = step["step"].toInt();
    QString action = step["action"].toString();
    
    emit stepStarted(m_currentStepIndex, step);
    emit logMessage(QString("Executing step %1: %2").arg(stepNum).arg(action));

    if (!m_toolRegistry) {
        emit errorOccurred("Tool registry not available");
        return;
    }

    // Dispatch to tool registry
    QJsonObject result = m_toolRegistry->executeToolCall(step);
    m_lastResult = result;

    emit stepFinished(m_currentStepIndex, result);

    // Check for failure (from MASM JsonExtractBool success)
    bool success = result.contains("success") ? result["success"].toBool() : !result.contains("error");
    
    if (!success) {
        handleStepFailure(m_currentStepIndex, result);
    } else {
        m_currentStepIndex++;
        m_toolCallCount++;

        if (m_toolCallCount > m_maxToolCalls) {
            emit errorOccurred("Max tool calls exceeded");
            setState(STATE_ERROR);
            return;
        }

        if (m_currentStepIndex < m_currentPlan.size()) {
            executeStep(m_currentPlan[m_currentStepIndex].toObject());
        } else {
            reviewResult(m_currentPlan, m_lastResult);
        }
    }
}

void AgenticPlanner::reviewResult(const QJsonArray& plan, const QJsonObject& finalResult)
{
    // From MASM AgenticLoop_ReviewResult
    setState(STATE_REVIEWING);
    QString prompt = buildReviewPrompt(plan, finalResult);
    
    emit logMessage("Reviewing final results...");
    // emit requestModelCall(prompt, "review");
}

void AgenticPlanner::handleStepFailure(int stepIndex, const QJsonObject& failureResult)
{
    // From MASM AgenticLoop_HandleStepFailure
    emit logMessage(QString("Step %1 failed. Entering correction loop...").arg(stepIndex + 1));
    
    QString prompt = buildCorrectionPrompt(stepIndex, failureResult);
    // emit requestModelCall(prompt, "correction");
}

void AgenticPlanner::setState(AgentState state)
{
    if (m_state != state) {
        m_state = state;
        emit stateChanged(state);
    }
}

QString AgenticPlanner::buildPlanningPrompt(const QString& task)
{
    // From MASM AGENT_PROMPT_FMT
    return QString(
        "You are an expert software development assistant. Create a detailed execution plan for this task:\n\n"
        "Task: %1\n\n"
        "Requirements:\n"
        "1. Break down into logical steps\n"
        "2. Each step must be tool-callable\n"
        "3. Include file paths and code locations\n"
        "4. Specify verification criteria\n\n"
        "Return JSON array of steps:\n"
        "[\n"
        "  {\n"
        "    \"step\": 1,\n"
        "    \"name\": \"file_read|file_write|execute_command|compile_project|grep_search|git_status\",\n"
        "    \"parameters\": { ... },\n"
        "    \"description\": \"...\",\n"
        "    \"verification\": \"...\"\n"
        "  }\n"
        "]"
    ).arg(task);
}

QString AgenticPlanner::buildCorrectionPrompt(int stepIndex, const QJsonObject& failure)
{
    // From MASM CORRECTION_FMT
    QJsonDocument doc(failure);
    return QString(
        "Step %1 failed. Error: %2\n\n"
        "Analyze the error and provide a corrected plan step."
    ).arg(stepIndex + 1).arg(QString(doc.toJson(QJsonDocument::Compact)));
}

QString AgenticPlanner::buildReviewPrompt(const QJsonArray& plan, const QJsonObject& result)
{
    // From MASM REVIEW_FMT
    QJsonDocument planDoc(plan);
    QJsonDocument resultDoc(result);
    return QString(
        "Review the completed task execution:\n\n"
        "Plan: %1\n\n"
        "Result: %2\n\n"
        "Verify:\n"
        "1. All requirements met\n"
        "2. No errors or issues\n"
        "3. Code quality acceptable\n"
        "4. Tests pass (if applicable)\n\n"
        "Provide final verification status."
    ).arg(QString(planDoc.toJson(QJsonDocument::Compact)))
     .arg(QString(resultDoc.toJson(QJsonDocument::Compact)));
}
