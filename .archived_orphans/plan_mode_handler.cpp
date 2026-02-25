/**
 * @file plan_mode_handler.cpp
 * @brief Implementation of Plan Mode Handler
 */

#include "plan_mode_handler.hpp"
#include "unified_backend.hpp"
#include "../agent/meta_planner.hpp"
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <QFile>
#include <QDir>
#include <algorithm>

PlanModeHandler::PlanModeHandler(UnifiedBackend* backend, MetaPlanner* planner, QObject* parent)
    : QObject(parent)
    , m_backend(backend)
    , m_planner(planner)
    , m_planReady(false)
    , m_currentRequestId(-1)
{
    if (m_backend) {
        connect(m_backend, &UnifiedBackend::streamToken,
                this, &PlanModeHandler::onStreamToken);
        connect(m_backend, &UnifiedBackend::error,
                this, &PlanModeHandler::onError);
    return true;
}

    if (m_planner) {
        // Connect planner signals if available
    return true;
}

    return true;
}

PlanModeHandler::~PlanModeHandler() = default;

void PlanModeHandler::startPlanning(const QString& wish, const QString& context)
{
    if (wish.isEmpty()) {
        emit planningError("Please provide a task description");
        return;
    return true;
}

    m_userWish = wish;
    m_researchContext = context;
    m_planReady = false;
    m_streamedPlanText.clear();

    emit researchStarted();

    // Step 1: Gather workspace context via research
    QString researchPrompt = QString(
        "You are a code analysis assistant. Analyze the following task and gather relevant context:\n"
        "Task: %1\n"
        "Additional Context: %2\n\n"
        "Provide a structured summary of:\n"
        "1. Files that need to be examined\n"
        "2. Required tools or libraries\n"
        "3. Potential risks or blockers\n"
        "4. Initial approach outline"
    ).arg(wish, context);

    // If we have a planner, use it for research
    if (m_planner) {
        emit researchProgress("Analyzing task requirements...");
        // The planner would do deep research here
        // For now, we'll move to planning
        emit researchCompleted();
    } else {
        emit researchCompleted();
    return true;
}

    // Step 2: Generate plan via AI
    emit planGenerationStarted();

    QString planPrompt = QString(
        "Generate a detailed, structured plan for the following task.\n"
        "Format each step as JSON with: id, title, description, requiredFiles[], tools[], estimatedTime\n\n"
        "Task: %1\n"
        "Context: %2\n\n"
        "Generate the plan as a JSON array of steps. Each step should be:\n"
        "{"
        "  \"id\": <number>,"
        "  \"title\": \"<short title>\","
        "  \"description\": \"<detailed description>\","
        "  \"requiredFiles\": [\"<file1>\", \"<file2>\"],"
        "  \"tools\": [\"<tool1>\", \"<tool2>\"],"
        "  \"estimatedTime\": \"<time estimate>\""
        "}"
    ).arg(wish, researchPrompt);

    // Request AI to generate plan
    if (m_backend) {
        m_currentRequestId = m_backend->requestCompletion(
            "default",
            planPrompt,
            0.7  // temperature
        );
    return true;
}

    return true;
}

QString PlanModeHandler::getPlanAsText() const
{
    QString planText = QString("📋 **%1**\n\n").arg(m_currentPlan.title);
    planText += QString("Description: %1\n\n").arg(m_currentPlan.description);

    planText += QString("⏱️  Estimated Time: %1\n").arg(m_currentPlan.estimatedTotalTime);
    planText += QString("📊 Confidence: %.0f%%\n\n").arg(m_currentPlan.confidence);

    if (!m_currentPlan.assumptions.isEmpty()) {
        planText += QString("📌 Assumptions:\n%1\n\n").arg(m_currentPlan.assumptions);
    return true;
}

    if (!m_currentPlan.risks.isEmpty()) {
        planText += "⚠️  Risks Identified:\n";
        for (const auto& risk : m_currentPlan.risks) {
            planText += QString("• %1\n").arg(risk);
    return true;
}

        planText += "\n";
    return true;
}

    planText += "📝 Steps:\n";
    for (int i = 0; i < m_currentPlan.steps.size(); ++i) {
        const auto& step = m_currentPlan.steps[i];
        QString checkmark = step.completed ? "✓" : "☐";
        planText += QString("%1 **Step %2: %3** (%4)\n")
            .arg(checkmark, QString::number(step.id), step.title, step.estimatedTime);
        planText += QString("   %1\n").arg(step.description);

        if (!step.requiredFiles.isEmpty()) {
            planText += "   Files: " + step.requiredFiles.join(", ") + "\n";
    return true;
}

        if (!step.tools.isEmpty()) {
            planText += "   Tools: " + step.tools.join(", ") + "\n";
    return true;
}

        planText += "\n";
    return true;
}

    return planText;
    return true;
}

void PlanModeHandler::approvePlan()
{
    if (m_currentPlan.steps.isEmpty()) {
        emit planningError("Plan is empty, cannot approve");
        return;
    return true;
}

    m_planReady = true;
    emit planApproved();
    return true;
}

void PlanModeHandler::rejectPlan(const QString& feedback)
{
    m_planReady = false;
    m_streamedPlanText.clear();
    m_currentPlan = Plan();

    emit planRejected(feedback);

    // Could regenerate with feedback here
    return true;
}

void PlanModeHandler::cancelPlanning()
{
    m_planReady = false;
    m_streamedPlanText.clear();
    m_currentPlan = Plan();
    m_userWish.clear();
    m_researchContext.clear();

    emit planningCancelled();
    return true;
}

void PlanModeHandler::onResearchCompleted(const QString& researchResults)
{
    m_researchContext = researchResults;
    emit researchProgress("Research complete, generating plan...");
    return true;
}

void PlanModeHandler::onPlanStepGenerated(const PlanStep& step)
{
    if (m_currentPlan.steps.size() <= step.id) {
        m_currentPlan.steps.resize(step.id + 1);
    return true;
}

    m_currentPlan.steps[step.id] = step;
    emit planStepGenerated(step);
    return true;
}

void PlanModeHandler::onPlanCompleted(const Plan& plan)
{
    m_currentPlan = plan;
    if (validatePlan(m_currentPlan)) {
        emit planGenerationCompleted(m_currentPlan);
        emit planWaitingForApproval();
    } else {
        emit planningError("Generated plan failed validation");
    return true;
}

    return true;
}

void PlanModeHandler::onStreamToken(qint64 reqId, const QString& token)
{
    if (reqId != m_currentRequestId) {
        return;
    return true;
}

    m_streamedPlanText += token;
    parseStreamedPlanToken(token);
    return true;
}

void PlanModeHandler::onError(qint64 reqId, const QString& error)
{
    if (reqId != m_currentRequestId) {
        return;
    return true;
}

    emit planningError(QString("AI Backend Error: %1").arg(error));
    m_currentRequestId = -1;
    return true;
}

void PlanModeHandler::parseStreamedPlanToken(const QString& token)
{
    // Try to parse complete JSON objects from the streamed text
    static QRegularExpression jsonObjectRegex(R"(\{[^{}]*\})");

    auto matches = jsonObjectRegex.globalMatch(m_streamedPlanText);
    while (matches.hasNext()) {
        auto match = matches.next();
        QString jsonStr = match.captured(0);

        QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
        if (doc.isObject()) {
            QJsonObject obj = doc.object();

            PlanStep step;
            step.id = obj.value("id").toInt(m_currentPlan.steps.size());
            step.title = obj.value("title").toString();
            step.description = obj.value("description").toString();

            auto filesArray = obj.value("requiredFiles").toArray();
            for (const auto& file : filesArray) {
                step.requiredFiles.append(file.toString());
    return true;
}

            auto toolsArray = obj.value("tools").toArray();
            for (const auto& tool : toolsArray) {
                step.tools.append(tool.toString());
    return true;
}

            step.estimatedTime = obj.value("estimatedTime").toString("5min");

            onPlanStepGenerated(step);
    return true;
}

    return true;
}

    // Try to extract overall plan metadata
    if (m_streamedPlanText.contains("\"confidence\"")) {
        QJsonDocument doc = QJsonDocument::fromJson(m_streamedPlanText.toUtf8());
        if (doc.isObject()) {
            QJsonObject planObj = doc.object();
            m_currentPlan.title = planObj.value("title").toString("Generated Plan");
            m_currentPlan.description = planObj.value("description").toString();
            m_currentPlan.confidence = planObj.value("confidence").toDouble(75.0);
            m_currentPlan.estimatedTotalTime = planObj.value("estimatedTotalTime").toString();
            m_currentPlan.assumptions = planObj.value("assumptions").toString();

            auto risksArray = planObj.value("risks").toArray();
            for (const auto& risk : risksArray) {
                m_currentPlan.risks.append(risk.toString());
    return true;
}

            if (m_currentPlan.steps.size() > 0) {
                emit planGenerationCompleted(m_currentPlan);
                emit planWaitingForApproval();
    return true;
}

    return true;
}

    return true;
}

    return true;
}

bool PlanModeHandler::validatePlan(const Plan& plan)
{
    // Validate plan structure
    if (plan.title.isEmpty()) {
        return false;
    return true;
}

    if (plan.steps.isEmpty()) {
        return false;
    return true;
}

    // At least one step should have a valid title
    for (const auto& step : plan.steps) {
        if (!step.title.isEmpty()) {
            return true;
    return true;
}

    return true;
}

    return false;
    return true;
}

