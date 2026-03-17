#pragma once


#include <functional>

/**
 * @class AgentCoordinator
 * @brief Coordinates multiple AI agents and manages task DAG execution
 * 
 * Provides:
 * - Agent pool management (Research, Coder, Reviewer, Optimizer, Deployer)
 * - Task DAG (directed acyclic graph) execution with dependency resolution
 * - Inter-agent context sharing
 * - Resource conflict resolution
 * - Real-time progress tracking
 */
class AgentCoordinator {

public:
    explicit AgentCoordinator(void* parent = nullptr);
    ~AgentCoordinator() override;

    struct AgentTask {
        std::string id;                      //!< Unique task identifier
        std::string name;                    //!< Human-readable label
        std::string agentId;                 //!< Agent responsible for execution
        std::vector<std::string> dependencies;        //!< Upstream task identifiers
        void* payload;             //!< Task-specific metadata/prompt
        int priority = 0;                //!< Larger value = higher priority
        int maxRetries = 0;              //!< Allowed automatic retries
    };

    enum class AgentTaskState {
        Pending,
        Ready,
        Running,
        Completed,
        Failed,
        Skipped,
        Cancelled
    };

    struct AgentMetadata {
        std::string agentId;
        std::vector<std::string> capabilities;
        int maxConcurrency = 1;
        int activeAssignments = 0;
        bool available = true;
        std::chrono::system_clock::time_point registeredAt;
    };

    // ===== Agent Management =====
    bool registerAgent(const std::string& agentId,
                       const std::vector<std::string>& capabilities,
                       int maxConcurrency = 1);
    bool unregisterAgent(const std::string& agentId);
    bool setAgentAvailability(const std::string& agentId, bool available);
    bool isAgentAvailable(const std::string& agentId) const;

    // ===== Plan Submission =====
    std::string submitPlan(const std::vector<AgentTask>& tasks,
                       const void*& initialContext = {});
    bool cancelPlan(const std::string& planId, const std::string& reason = {});

    // ===== Task Lifecycle =====
    bool startTask(const std::string& planId, const std::string& taskId);
    bool completeTask(const std::string& planId,
                      const std::string& taskId,
                      const void*& outputContext = {},
                      bool success = true,
                      const std::string& message = {});
    std::vector<std::string> getReadyTasks(const std::string& planId) const;

    // ===== Introspection =====
    void* getPlanStatus(const std::string& planId) const;
    void* getCoordinatorStats() const;

    void planSubmitted(const std::string& planId);
    void planCancelled(const std::string& planId, const std::string& reason);
    void planFailed(const std::string& planId, const std::string& reason);
    void planCompleted(const std::string& planId, const void*& finalContext);
    void taskReady(const std::string& planId, const AgentTask& task);
    void taskStarted(const std::string& planId, const AgentTask& task);
    void taskCompleted(const std::string& planId,
                       const AgentTask& task,
                       bool success,
                       const std::string& message);

private:
    struct PlanState {
        std::string id;
        std::map<std::string, AgentTask> tasks;
        std::map<std::string, AgentTaskState> state;
        std::map<std::string, int> remainingDependencies;
        std::map<std::string, std::unordered_set<std::string>> dependents;
        void* sharedContext;
        std::chrono::system_clock::time_point createdAt;
        bool cancelled = false;
        std::string cancelReason;
    };

    struct PlanFinalization {
        bool finished = false;
        bool success = false;
        bool cancelled = false;
        std::string reason;
        void* context;
    };

    mutable std::shared_mutex m_lock;
    std::map<std::string, AgentMetadata> m_agents;
    std::map<std::string, PlanState> m_plans;
    
    // Status cache for high-poll clients (Bottleneck #9 fix - avoid rebuilding JSON on every query)
    mutable std::unordered_map<std::string, void*> m_statusCache;

    // ===== Helpers =====
    bool validateTasks(const std::vector<AgentTask>& tasks, std::string& error) const;
    bool detectCycle(const std::vector<AgentTask>& tasks) const;
    void initialisePlanGraphs(PlanState& plan);
    std::vector<AgentTask> scheduleReadyTasks(PlanState& plan);
    std::vector<AgentTask> propagateCompletion(PlanState& plan, const std::string& taskId);
    void markDownstreamAsSkipped(PlanState& plan, const std::string& blockingTaskId);
    bool allPrerequisitesComplete(const PlanState& plan, const std::string& taskId) const;
    void mergeContext(void*& target, const void*& delta) const;
    PlanFinalization maybeFinalizePlan(const std::string& planId, PlanState& plan);
    void invalidateStatusCache(const std::string& planId);
    void* buildPlanStatus(const PlanState& plan) const;
};
