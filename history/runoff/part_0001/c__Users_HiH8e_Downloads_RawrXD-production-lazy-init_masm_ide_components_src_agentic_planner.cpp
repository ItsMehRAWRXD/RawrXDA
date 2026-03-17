#include "agentic_planner.h"
#include "ai_model_client.h"
#include "aws_bedrock_bridge.h"
#include <QJsonDocument>
#include <QDebug>

AgenticPlanner::AgenticPlanner(MasmToolRegistry* toolRegistry, ModelRouter* modelRouter, AIModelClient* aiClient, QObject* parent)
    : QObject(parent), m_toolRegistry(toolRegistry), m_modelRouter(modelRouter), m_aiClient(aiClient)
{
    // Real AWS Bedrock integration
    m_bedrockBridge = new AwsBedrockBridge(this);
    connect(m_bedrockBridge, &AwsBedrockBridge::responseReceived, this, &AgenticPlanner::onAIResponse);
    connect(m_bedrockBridge, &AwsBedrockBridge::errorOccurred, this, &AgenticPlanner::onAIError);
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
    
    // REAL AWS BEDROCK CALL
    m_bedrockBridge->callModel(prompt, "planning");
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
    
    // REAL AWS BEDROCK CALL for review
    m_bedrockBridge->callModel(prompt, "review");
}

void AgenticPlanner::handleStepFailure(int stepIndex, const QJsonObject& failureResult)
{
    // From MASM AgenticLoop_HandleStepFailure
    emit logMessage(QString("Step %1 failed. Entering correction loop...").arg(stepIndex + 1));
    
    QString prompt = buildCorrectionPrompt(stepIndex, failureResult);
    
    // REAL AWS BEDROCK CALL for correction
    m_bedrockBridge->callModel(prompt, "correction");
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

void AgenticPlanner::onAIResponse(const QString& response)
{
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(response.toUtf8(), &error);
    
    if (error.error != QJsonParseError::NoError) {
        emit errorOccurred("Invalid JSON response from AI: " + error.errorString());
        return;
    }

    switch (m_state) {
        case STATE_PLANNING:
            if (doc.isArray()) {
                m_currentPlan = doc.array();
                emit planGenerated(m_currentPlan);
                emit logMessage(QString("Plan generated with %1 steps").arg(m_currentPlan.size()));
                
                if (!m_currentPlan.isEmpty()) {
                    executeStep(m_currentPlan[0].toObject());
                }
            } else {
                emit errorOccurred("AI response is not a valid plan array");
            }
            break;
            
        case STATE_REVIEWING:
            emit taskFinished(response);
            setState(STATE_IDLE);
            break;
            
        default:
            // Correction response - parse and retry step
            if (doc.isObject()) {
                QJsonObject correctedStep = doc.object();
                executeStep(correctedStep);
            } else {
                emit errorOccurred("Invalid correction response from AI");
            }
            break;
    }
}

void AgenticPlanner::onAIError(const QString& error)
{
    emit errorOccurred("AI call failed: " + error);
    setState(STATE_ERROR);
}
