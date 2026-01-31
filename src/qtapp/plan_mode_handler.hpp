/**
 * @file plan_mode_handler.hpp
 * @brief Plan Mode Handler - Research & Planning Phase
 * 
 * Plan Mode executes the planning phase of agentic operation:
 * 1. User provides a "wish" or task description
 * 2. runSubagent researches the task (file scanning, context gathering)
 * 3. AI generates a structured plan with checklist
 * 4. User reviews and approves the plan
 * 5. Transitions to Agent Mode for execution
 */

#pragma once


#include <memory>

class UnifiedBackend;
class MetaPlanner;

/**
 * @struct PlanStep
 * @brief Represents a single step in the generated plan
 */
struct PlanStep {
    int id;                        ///< Step ID (1-based)
    std::string title;                 ///< Short step description
    std::string description;           ///< Detailed description
    std::vector<std::string> requiredFiles;     ///< Files this step needs
    std::vector<std::string> tools;             ///< Tools/methods to use
    bool completed = false;        ///< Completion status
    std::string estimatedTime;         ///< Time estimate (e.g., "5min", "1h")
};

/**
 * @struct Plan
 * @brief Represents the complete generated plan
 */
struct Plan {
    std::string title;                 ///< Plan title
    std::string description;           ///< Overall description
    std::vector<PlanStep> steps;       ///< Ordered steps
    std::string estimatedTotalTime;    ///< Total time estimate
    float confidence;              ///< Plan confidence score (0-100)
    std::string assumptions;           ///< Assumptions the plan makes
    std::vector<std::string> risks;             ///< Identified risks
};

/**
 * @class PlanModeHandler
 * @brief Handles the Plan Mode phase of agentic operation
 */
class PlanModeHandler : public void {

public:
    explicit PlanModeHandler(UnifiedBackend* backend, MetaPlanner* planner, void* parent = nullptr);
    ~PlanModeHandler();

    /**
     * @brief Start plan mode with a user wish/task
     * @param wish The user's task description or goal
     * @param context Additional context (current file, selection, etc.)
     */
    void startPlanning(const std::string& wish, const std::string& context = "");

    /**
     * @brief Get the current plan being reviewed
     * @return Reference to current plan
     */
    const Plan& getCurrentPlan() const { return m_currentPlan; }

    /**
     * @brief Check if plan is complete and ready for execution
     * @return True if plan is generated and reviewed
     */
    bool isPlanReady() const { return m_planReady; }

    /**
     * @brief Get plan as formatted text for display
     * @return Human-readable plan text
     */
    std::string getPlanAsText() const;

    /**
     * @brief Mark plan as approved and ready to proceed
     */
    void approvePlan();

    /**
     * @brief Reject plan and request regeneration
     * @param feedback User feedback on why plan was rejected
     */
    void rejectPlan(const std::string& feedback);

    /**
     * @brief Cancel plan mode and return to idle
     */
    void cancelPlanning();

    /// Research phase started (gathering context from workspace)
    void researchStarted();

    /// Research progress update
    void researchProgress(const std::string& message);

    /// Research completed, planning AI now
    void researchCompleted();

    /// Plan generation started
    void planGenerationStarted();

    /// Plan step generated (streamed)
    void planStepGenerated(const PlanStep& step);

    /// Plan generation completed
    void planGenerationCompleted(const Plan& plan);

    /// Plan displayed and waiting for user approval
    void planWaitingForApproval();

    /// User approved the plan
    void planApproved();

    /// Plan rejected by user
    void planRejected(const std::string& feedback);

    /// Error occurred during planning
    void planningError(const std::string& errorMessage);

    /// Planning cancelled
    void planningCancelled();

private:
    /// Handle subagent research completion
    void onResearchCompleted(const std::string& researchResults);

    /// Handle planner generating a plan step
    void onPlanStepGenerated(const PlanStep& step);

    /// Handle planner completing plan generation
    void onPlanCompleted(const Plan& plan);

    /// Handle AI backend streaming a plan line
    void onStreamToken(int64_t reqId, const std::string& token);

    /// Handle AI backend error
    void onError(int64_t reqId, const std::string& error);

private:
    /**
     * @brief Parse streamed tokens into plan structure
     * @param token Token from AI stream
     */
    void parseStreamedPlanToken(const std::string& token);

    /**
     * @brief Validate plan structure
     * @return True if plan is well-formed
     */
    bool validatePlan(const Plan& plan);

    // Members
    UnifiedBackend* m_backend;          ///< AI backend for planning
    MetaPlanner* m_planner;             ///< Planning engine
    Plan m_currentPlan;                 ///< Currently generated plan
    bool m_planReady;                   ///< Plan approved and ready
    std::string m_userWish;                 ///< Original user request
    std::string m_researchContext;          ///< Context from workspace research
    std::string m_streamedPlanText;         ///< Accumulated streamed plan text
    int64_t m_currentRequestId;          ///< ID of current AI request
};


