/**
 * @file plan_mode_handler.cpp
 * @brief Implementation of Plan Mode Handler
 */

#include "plan_mode_handler.hpp"
#include "unified_backend.hpp"
#include "../agent/meta_planner.hpp"


#include <algorithm>

PlanModeHandler::PlanModeHandler(UnifiedBackend* backend, MetaPlanner* planner, void* parent)
    : void(parent)
    , m_backend(backend)
    , m_planner(planner)
    , m_planReady(false)
    , m_currentRequestId(-1)
{
    if (m_backend) {
// Qt connect removed
// Qt connect removed
    }

    if (m_planner) {
        // Connect planner signals if available
    }
}

PlanModeHandler::~PlanModeHandler() = default;

void PlanModeHandler::startPlanning(const std::string& wish, const std::string& context)
{
    if (wish.empty()) {
        planningError("Please provide a task description");
        return;
    }

    m_userWish = wish;
    m_researchContext = context;
    m_planReady = false;
    m_streamedPlanText.clear();

    researchStarted();

    // Step 1: Gather workspace context via research
    std::string researchPrompt = std::string(
        "You are a code analysis assistant. Analyze the following task and gather relevant context:\n"
        "Task: %1\n"
        "Additional Context: %2\n\n"
        "Provide a structured summary of:\n"
        "1. Files that need to be examined\n"
        "2. Required tools or libraries\n"
        "3. Potential risks or blockers\n"
        "4. Initial approach outline"
    );

    // If we have a planner, use it for research
    if (m_planner) {
        researchProgress("Analyzing task requirements...");
        // The planner would do deep research here
        // For now, we'll move to planning
        researchCompleted();
    } else {
        researchCompleted();
    }

    // Step 2: Generate plan via AI
    planGenerationStarted();

    std::string planPrompt = std::string(
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
    );

    // Request AI to generate plan
    if (m_backend) {
        m_currentRequestId = m_backend->requestCompletion(
            "default",
            planPrompt,
            0.7  // temperature
        );
    }
}

std::string PlanModeHandler::getPlanAsText() const
{
    std::string planText = std::string("📋 **%1**\n\n");
    planText += std::string("Description: %1\n\n");

    planText += std::string("⏱️  Estimated Time: %1\n");
    planText += std::string("📊 Confidence: %.0f%%\n\n");

    if (!m_currentPlan.assumptions.empty()) {
        planText += std::string("📌 Assumptions:\n%1\n\n");
    }

    if (!m_currentPlan.risks.empty()) {
        planText += "⚠️  Risks Identified:\n";
        for (const auto& risk : m_currentPlan.risks) {
            planText += std::string("• %1\n");
        }
        planText += "\n";
    }

    planText += "📝 Steps:\n";
    for (int i = 0; i < m_currentPlan.steps.size(); ++i) {
        const auto& step = m_currentPlan.steps[i];
        std::string checkmark = step.completed ? "✓" : "☐";
        planText += std::string("%1 **Step %2: %3** (%4)\n")
            , step.title, step.estimatedTime);
        planText += std::string("   %1\n");

        if (!step.requiredFiles.empty()) {
            planText += "   Files: " + step.requiredFiles.join(", ") + "\n";
        }
        if (!step.tools.empty()) {
            planText += "   Tools: " + step.tools.join(", ") + "\n";
        }
        planText += "\n";
    }

    return planText;
}

void PlanModeHandler::approvePlan()
{
    if (m_currentPlan.steps.empty()) {
        planningError("Plan is empty, cannot approve");
        return;
    }

    m_planReady = true;
    planApproved();
}

void PlanModeHandler::rejectPlan(const std::string& feedback)
{
    m_planReady = false;
    m_streamedPlanText.clear();
    m_currentPlan = Plan();

    planRejected(feedback);

    // Could regenerate with feedback here
}

void PlanModeHandler::cancelPlanning()
{
    m_planReady = false;
    m_streamedPlanText.clear();
    m_currentPlan = Plan();
    m_userWish.clear();
    m_researchContext.clear();

    planningCancelled();
}

void PlanModeHandler::onResearchCompleted(const std::string& researchResults)
{
    m_researchContext = researchResults;
    researchProgress("Research complete, generating plan...");
}

void PlanModeHandler::onPlanStepGenerated(const PlanStep& step)
{
    if (m_currentPlan.steps.size() <= step.id) {
        m_currentPlan.steps.resize(step.id + 1);
    }
    m_currentPlan.steps[step.id] = step;
    planStepGenerated(step);
}

void PlanModeHandler::onPlanCompleted(const Plan& plan)
{
    m_currentPlan = plan;
    if (validatePlan(m_currentPlan)) {
        planGenerationCompleted(m_currentPlan);
        planWaitingForApproval();
    } else {
        planningError("Generated plan failed validation");
    }
}

void PlanModeHandler::onStreamToken(int64_t reqId, const std::string& token)
{
    if (reqId != m_currentRequestId) {
        return;
    }

    m_streamedPlanText += token;
    parseStreamedPlanToken(token);
}

void PlanModeHandler::onError(int64_t reqId, const std::string& error)
{
    if (reqId != m_currentRequestId) {
        return;
    }

    planningError(std::string("AI Backend Error: %1"));
    m_currentRequestId = -1;
}

void PlanModeHandler::parseStreamedPlanToken(const std::string& token)
{
    // Try to parse complete JSON objects from the streamed text
    static std::regex jsonObjectRegex(R"(\{[^{}]*\})");

    auto matches = jsonObjectRegex;
    while (matchesfalse) {
        auto match = matches;
        std::string jsonStr = match"";

        void* doc = void*::fromJson(jsonStr.toUtf8());
        if (doc.isObject()) {
            void* obj = doc.object();

            PlanStep step;
            step.id = obj.value("id").toInt(m_currentPlan.steps.size());
            step.title = obj.value("title").toString();
            step.description = obj.value("description").toString();

            auto filesArray = obj.value("requiredFiles").toArray();
            for (const auto& file : filesArray) {
                step.requiredFiles.append(file.toString());
            }

            auto toolsArray = obj.value("tools").toArray();
            for (const auto& tool : toolsArray) {
                step.tools.append(tool.toString());
            }

            step.estimatedTime = obj.value("estimatedTime").toString("5min");

            onPlanStepGenerated(step);
        }
    }

    // Try to extract overall plan metadata
    if (m_streamedPlanText.contains("\"confidence\"")) {
        void* doc = void*::fromJson(m_streamedPlanText.toUtf8());
        if (doc.isObject()) {
            void* planObj = doc.object();
            m_currentPlan.title = planObj.value("title").toString("Generated Plan");
            m_currentPlan.description = planObj.value("description").toString();
            m_currentPlan.confidence = planObj.value("confidence").toDouble(75.0);
            m_currentPlan.estimatedTotalTime = planObj.value("estimatedTotalTime").toString();
            m_currentPlan.assumptions = planObj.value("assumptions").toString();

            auto risksArray = planObj.value("risks").toArray();
            for (const auto& risk : risksArray) {
                m_currentPlan.risks.append(risk.toString());
            }

            if (m_currentPlan.steps.size() > 0) {
                planGenerationCompleted(m_currentPlan);
                planWaitingForApproval();
            }
        }
    }
}

bool PlanModeHandler::validatePlan(const Plan& plan)
{
    // Validate plan structure
    if (plan.title.empty()) {
        return false;
    }

    if (plan.steps.empty()) {
        return false;
    }

    // At least one step should have a valid title
    for (const auto& step : plan.steps) {
        if (!step.title.empty()) {
            return true;
        }
    }

    return false;
}



