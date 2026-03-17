// AgenticIterativeReasoning Implementation
#include "agentic_iterative_reasoning.h"
#include "agentic_loop_state.h"
#include "agentic_engine.h"
#include "../src/qtapp/inference_engine.hpp"
#include <QDebug>
#include <QDateTime>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonArray>
#include <algorithm>

AgenticIterativeReasoning::AgenticIterativeReasoning(QObject* parent)
    : QObject(parent)
{
    qDebug() << "[AgenticIterativeReasoning] Initialized - Ready for iterative reasoning";
}

AgenticIterativeReasoning::~AgenticIterativeReasoning()
{
    qDebug() << "[AgenticIterativeReasoning] Destroyed";
}

void AgenticIterativeReasoning::initialize(
    AgenticEngine* engine,
    AgenticLoopState* state,
    InferenceEngine* inference)
{
    m_engine = engine;
    m_state = state;
    m_inferenceEngine = inference;

    qInfo() << "[AgenticIterativeReasoning] Initialized with AgenticEngine, state, and inference engine";
}

// ===== MAIN REASONING LOOP =====

AgenticIterativeReasoning::IterationResult AgenticIterativeReasoning::reason(
    const QString& goal,
    int maxIterations,
    int timeoutSeconds)
{
    qInfo() << "[AgenticIterativeReasoning] Starting reasoning loop for goal:" << goal;

    m_currentGoal = goal;
    m_loopStartTime = QDateTime::currentDateTime();
    m_currentIteration = 0;
    m_recentResults.clear();
    m_failedStrategies.clear();

    if (m_state) {
        m_state->setGoal(goal);
        m_state->startIteration(goal);
    }

    IterationResult finalResult;
    finalResult.success = false;
    finalResult.result = "Not completed";
    finalResult.confidence = 0.0f;
    finalResult.iterationCount = 0;

    QElapsedTimer timer;
    timer.start();

    // Main reasoning loop
    while (m_currentIteration < maxIterations) {
        m_currentIteration++;
        
        if (timer.elapsed() / 1000 > timeoutSeconds) {
            qWarning() << "[AgenticIterativeReasoning] Timeout exceeded";
            break;
        }

        emit iterationStarted(m_currentIteration);

        try {
            // PHASE 1: ANALYSIS
            if (m_state) m_state->setCurrentPhase(AgenticLoopState::ReasoningPhase::Analysis);
            
            QString analysis = executeAnalysisPhase(goal);
            emit phaseCompleted("Analysis", analysis);
            
            if (m_verboseLogging) {
                log(QString("Analysis phase completed - %1").arg(analysis));
            }

            // PHASE 2: PLANNING
            if (m_state) m_state->setCurrentPhase(AgenticLoopState::ReasoningPhase::Planning);
            
            std::vector<Strategy> strategies = executePlanningPhase(goal, analysis);
            if (strategies.empty()) {
                throw std::runtime_error("No valid strategies generated");
            }

            Strategy selectedStrategy = pickBestStrategy(strategies);
            emit strategySelected(selectedStrategy.name);

            if (m_verboseLogging) {
                log(QString("Selected strategy: %1").arg(selectedStrategy.name));
            }

            // PHASE 3: EXECUTION
            if (m_state) m_state->setCurrentPhase(AgenticLoopState::ReasoningPhase::Execution);
            
            QJsonObject executionPlan = executeExecutionPhase(selectedStrategy, goal);
            emit executionStarted(selectedStrategy.name);

            if (!executionPlan.value("success").toBool()) {
                if (shouldRetry(executionPlan.value("error").toString())) {
                    log("Execution failed, retrying...", "WARN");
                    m_currentIteration--;
                    continue;
                }
            }

            // PHASE 4: VERIFICATION
            if (m_state) m_state->setCurrentPhase(AgenticLoopState::ReasoningPhase::Verification);
            
            bool verified = executeVerificationPhase(goal, executionPlan);
            emit verificationResult(verified, executionPlan.value("result").toString());

            if (!verified && m_currentIteration < maxIterations - 1) {
                if (m_verboseLogging) {
                    log("Verification failed, continuing to next iteration");
                }
                continue;
            }

            // PHASE 5: REFLECTION (if enabled)
            if (m_enableReflection && m_state) {
                m_state->setCurrentPhase(AgenticLoopState::ReasoningPhase::Reflection);
                
                QJsonObject iterationData;
                iterationData["strategy"] = selectedStrategy.name;
                iterationData["verified"] = verified;
                iterationData["analysis"] = analysis;
                
                QString reflection = executeReflectionPhase(m_currentIteration, goal, iterationData);
                emit reflectionGenerated(reflection);

                if (m_verboseLogging) {
                    log(QString("Reflection: %1").arg(reflection));
                }

                // PHASE 6: ADJUSTMENT
                m_state->setCurrentPhase(AgenticLoopState::ReasoningPhase::Adjustment);
                
                QJsonObject adjustment = adjustStrategy(reflection);
                emit adjustmentApplied(adjustment.value("newStrategy").toString());

                if (m_verboseLogging) {
                    log(QString("Adjusted strategy: %1").arg(adjustment.value("newStrategy").toString()));
                }
            }

            // Check for convergence
            QString resultSummary = executionPlan.value("result").toString();
            m_recentResults.append(resultSummary);
            if (m_recentResults.size() > 3) m_recentResults.pop_front();

            if (verified && hasConverged(m_recentResults)) {
                finalResult.success = true;
                finalResult.result = resultSummary;
                finalResult.confidence = static_cast<float>(
                    executionPlan.value("confidence").toDouble(0.8)
                );
                finalResult.iterationCount = m_currentIteration;
                finalResult.totalTime = timer.elapsed();

                if (m_state) {
                    m_state->endIteration(AgenticLoopState::IterationStatus::Completed, resultSummary);
                }

                emit iterationCompleted(m_currentIteration, true);
                emit reasoningCompleted(resultSummary);

                qInfo() << "[AgenticIterativeReasoning] Reasoning completed successfully in"
                        << m_currentIteration << "iterations";

                break;
            }

            emit iterationCompleted(m_currentIteration, false);

        } catch (const std::exception& e) {
            QString error = QString::fromStdString(e.what());
            qWarning() << "[AgenticIterativeReasoning] Error in iteration" << m_currentIteration
                       << ":" << error;
            emit errorOccurred(error, m_currentIteration);

            if (m_state) {
                m_state->recordError("IterationError", error);
            }

            if (!handleReasoningError(error, m_currentIteration)) {
                break;
            }
        }
    }

    finalResult.reasoningTrace = m_reasoningTrace;
    return finalResult;
}

// ===== PHASE IMPLEMENTATIONS =====

QString AgenticIterativeReasoning::executeAnalysisPhase(const QString& goal)
{
    if (!m_engine) return "No engine available";

    QString prompt = QString(
        "Analyze this goal and provide a comprehensive understanding:\n"
        "Goal: %1\n\n"
        "Provide:\n"
        "1. Problem decomposition\n"
        "2. Required resources\n"
        "3. Potential challenges\n"
        "4. Success criteria\n"
        "5. Risk assessment"
    ).arg(goal);

    return callModelForReasoning(prompt, m_state ? m_state->formatContextForModel() : "");
}

std::vector<AgenticIterativeReasoning::Strategy> AgenticIterativeReasoning::executePlanningPhase(
    const QString& goal,
    const QString& analysis)
{
    std::vector<Strategy> strategies;

    if (!m_engine) return strategies;

    QString prompt = QString(
        "Based on the analysis, generate multiple strategies:\n"
        "Goal: %1\n"
        "Analysis: %2\n\n"
        "For each strategy, provide:\n"
        "{\n"
        "  \"name\": \"strategy name\",\n"
        "  \"description\": \"detailed description\",\n"
        "  \"success_probability\": 0.0-1.0,\n"
        "  \"complexity\": 1-10,\n"
        "  \"required_tools\": [list]\n"
        "}\n\n"
        "Generate at least 3 diverse strategies."
    ).arg(goal, analysis);

    QString response = callModelForReasoning(prompt);
    auto structuredResponse = extractStructuredResponse(response);

    for (const auto& item : structuredResponse) {
        QJsonObject strategyObj = item.toObject();
        Strategy strategy;
        strategy.name = strategyObj.value("name").toString("Unknown");
        strategy.description = strategyObj.value("description").toString();
        strategy.estimatedSuccessProbability = 
            static_cast<float>(strategyObj.value("success_probability").toDouble(0.5));
        strategy.complexity = strategyObj.value("complexity").toInt(5);
        
        auto tools = strategyObj.value("required_tools").toArray();
        for (const auto& tool : tools) {
            strategy.requiredTools.append(tool.toString());
        }

        strategies.push_back(strategy);
    }

    return strategies;
}

AgenticIterativeReasoning::Strategy AgenticIterativeReasoning::pickBestStrategy(
    const std::vector<Strategy>& strategies)
{
    if (strategies.empty()) {
        Strategy fallback;
        fallback.name = "Fallback";
        fallback.description = "Default strategy";
        fallback.estimatedSuccessProbability = 0.5f;
        fallback.complexity = 5;
        return fallback;
    }

    // Select strategy with best balance of success probability and low complexity
    // Also avoid strategies that failed before
    Strategy best = strategies[0];
    float bestScore = -1.0f;

    for (const auto& strategy : strategies) {
        // Check if strategy was tried before and failed
        bool wasFailedBefore = std::any_of(
            m_failedStrategies.begin(),
            m_failedStrategies.end(),
            [&strategy](const Strategy& failed) { return failed.name == strategy.name; }
        );

        if (wasFailedBefore) continue;

        float score = (strategy.estimatedSuccessProbability * 10.0f) - (strategy.complexity * 0.5f);
        
        if (score > bestScore) {
            bestScore = score;
            best = strategy;
        }
    }

    return best;
}

QJsonObject AgenticIterativeReasoning::executeExecutionPhase(
    const Strategy& strategy,
    const QString& goal)
{
    QJsonObject result;
    result["strategy"] = strategy.name;

    if (!m_engine) {
        result["success"] = false;
        result["error"] = "No engine available";
        return result;
    }

    // Generate execution plan from strategy
    QString prompt = QString(
        "Create a detailed execution plan for this strategy:\n"
        "Strategy: %1\n"
        "Description: %2\n"
        "Goal: %3\n\n"
        "Provide step-by-step actions with parameters."
    ).arg(strategy.name, strategy.description, goal);

    QString plan = callModelForReasoning(prompt);

    // Execute the plan by parsing action steps and dispatching them
    bool executionSuccess = true;
    QStringList actionLog;

    if (!plan.isEmpty()) {
        // Parse the model's step-by-step plan and attempt execution
        QStringList steps = plan.split(QRegularExpression("\\n(?=\\d+\\.\\s|Step\\s)"), Qt::SkipEmptyParts);
        for (const QString& step : steps) {
            QString trimmedStep = step.trimmed();
            if (trimmedStep.isEmpty()) continue;

            // Dispatch step to the engine for tool execution
            if (m_engine) {
                QString stepResult = m_engine->executeAction(trimmedStep);
                actionLog.append(QString("[OK] %1 -> %2").arg(trimmedStep.left(80), stepResult.left(200)));
                if (stepResult.contains("error", Qt::CaseInsensitive) ||
                    stepResult.contains("failed", Qt::CaseInsensitive)) {
                    executionSuccess = false;
                }
            } else {
                actionLog.append(QString("[SKIP] %1 (no engine)").arg(trimmedStep.left(80)));
            }
        }
    } else {
        executionSuccess = false;
        actionLog.append("[ERROR] Model returned empty plan");
    }

    result["success"] = executionSuccess;
    result["result"] = executionSuccess ?
        "Strategy executed: " + strategy.name + "\n" + actionLog.join("\n") :
        "Strategy partially failed: " + strategy.name + "\n" + actionLog.join("\n");
    result["confidence"] = executionSuccess ?
        strategy.estimatedSuccessProbability :
        strategy.estimatedSuccessProbability * 0.5;

    if (m_state) {
        m_state->recordAppliedStrategy(strategy.name);
    }

    return result;
}

bool AgenticIterativeReasoning::executeVerificationPhase(
    const QString& expectedOutcome,
    const QJsonObject& actualResult)
{
    if (!m_engine) return false;

    QString prompt = QString(
        "Verify if the result meets the goal:\n"
        "Goal: %1\n"
        "Result: %2\n\n"
        "Answer with ONLY 'success' or 'failure'."
    ).arg(expectedOutcome, actualResult.value("result").toString());

    QString verification = callModelForReasoning(prompt);
    return verification.toLower().contains("success");
}

QString AgenticIterativeReasoning::executeReflectionPhase(
    int iterationNumber,
    const QString& goal,
    const QJsonObject& iterationData)
{
    if (!m_engine) return "No reflection available";

    QString prompt = QString(
        "Reflect on this iteration:\n"
        "Iteration: %1\n"
        "Goal: %2\n"
        "Data: %3\n\n"
        "Provide insights on:\n"
        "1. What worked well\n"
        "2. What could improve\n"
        "3. Patterns observed\n"
        "4. Suggested adjustments"
    ).arg(QString::number(iterationNumber), goal, QString::fromUtf8(QJsonDocument(iterationData).toJson()));

    return callModelForReasoning(prompt);
}

// ===== HELPER METHODS =====

QString AgenticIterativeReasoning::callModelForReasoning(
    const QString& prompt,
    const QString& context)
{
    if (!m_inferenceEngine) {
        return "Model unavailable";
    }

    // Build full prompt with optional context prefix
    QString fullPrompt = context.isEmpty() ? prompt : context + "\n\n" + prompt;

    // Route through the loaded inference engine
    try {
        QString result = m_inferenceEngine->infer(fullPrompt);
        if (result.isEmpty()) {
            return "Model returned empty response";
        }
        return result;
    } catch (...) {
        return "Inference engine error during reasoning call";
    }
}

QJsonArray AgenticIterativeReasoning::extractStructuredResponse(const QString& modelResponse)
{
    QJsonArray result;

    // Parse JSON array from model response
    QRegularExpression jsonRegex("\\[\\s*\\{.*\\}\\s*\\]", 
                                 QRegularExpression::DotMatchesEverythingOption);
    QRegularExpressionMatch match = jsonRegex.match(modelResponse);

    if (match.hasMatch()) {
        QString jsonStr = match.captured(0);
        QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
        if (doc.isArray()) {
            result = doc.array();
        }
    }

    return result;
}

bool AgenticIterativeReasoning::hasConverged(const std::vector<QString>& recentResults)
{
    if (recentResults.size() < 3) return false;

    // Simple convergence check - if last 3 results are similar
    return recentResults[0] == recentResults[1] && recentResults[1] == recentResults[2];
}

bool AgenticIterativeReasoning::shouldRetry(const QString& error) const
{
    // Determine if error is retryable
    return error.contains("timeout", Qt::CaseInsensitive) ||
           error.contains("temporary", Qt::CaseInsensitive) ||
           error.contains("resource", Qt::CaseInsensitive);
}

bool AgenticIterativeReasoning::handleReasoningError(const QString& error, int& retryCount)
{
    if (retryCount >= m_maxIterations) {
        qCritical() << "[AgenticIterativeReasoning] Max iterations reached";
        return false;
    }

    if (shouldRetry(error)) {
        log(QString("Recoverable error, will retry: %1").arg(error), "WARN");
        return true;
    }

    log(QString("Fatal error: %1").arg(error), "ERROR");
    return false;
}

QJsonObject AgenticIterativeReasoning::adjustStrategy(const QString& reflection)
{
    QJsonObject adjustment;
    adjustment["reflection"] = reflection;
    adjustment["newStrategy"] = "Adjusted based on reflection";
    
    if (m_state) {
        m_state->recordAppliedStrategy("Adjusted strategy");
    }

    return adjustment;
}

void AgenticIterativeReasoning::log(const QString& message, const QString& level)
{
    if (!m_verboseLogging && level != "ERROR") return;

    qDebug() << QString("[AgenticIterativeReasoning %1] %2").arg(level, message);
}

QString AgenticIterativeReasoning::getReasoningExplanation() const
{
    QString explanation;
    explanation += "REASONING TRACE:\n";
    
    for (const auto& trace : m_reasoningTrace) {
        explanation += trace.toObject().value("description").toString() + "\n";
    }

    return explanation;
}

QJsonObject AgenticIterativeReasoning::getMetrics() const
{
    QJsonObject metrics;
    metrics["iterations"] = m_currentIteration;
    metrics["duration_ms"] = static_cast<int>(
        m_loopStartTime.msecsTo(QDateTime::currentDateTime())
    );
    metrics["strategy_count"] = m_failedStrategies.size();

    return metrics;
}
