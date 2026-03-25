<<<<<<< HEAD
#pragma once

// C++20, no Qt. Planning agent; callbacks replace signals.

#include <string>
#include <vector>
#include <functional>

struct Task {
    std::string id;
    std::string description;
    std::string status;
    int priority = 0;
    std::string assignedAgent;
};

class PlanningAgent
{
public:
    using PlanCreatedFn     = std::function<void(const std::string& plan)>;
    using TaskStatusChangedFn = std::function<void(const std::string& taskId, const std::string& status)>;
    using PlanCompletedFn   = std::function<void()>;
    using PlanFailedFn      = std::function<void(const std::string& error)>;

    PlanningAgent() = default;
    virtual ~PlanningAgent() = default;

    void setOnPlanCreated(PlanCreatedFn f)         { m_onPlanCreated = std::move(f); }
    void setOnTaskStatusChanged(TaskStatusChangedFn f) { m_onTaskStatusChanged = std::move(f); }
    void setOnPlanCompleted(PlanCompletedFn f)     { m_onPlanCompleted = std::move(f); }
    void setOnPlanFailed(PlanFailedFn f)           { m_onPlanFailed = std::move(f); }

    void initialize();
    void createPlan(const std::string& goal);
    void executePlan();
    void addTask(const Task& task);
    std::vector<Task> getTasks() const;

private:
    void processNextTask();
    std::string generatePlan(const std::string& goal);
    void updateTaskStatus(const std::string& taskId, const std::string& status);

    std::vector<Task> tasks_;
    int currentTaskIndex_ = 0;

    PlanCreatedFn       m_onPlanCreated;
    TaskStatusChangedFn m_onTaskStatusChanged;
    PlanCompletedFn     m_onPlanCompleted;
    PlanFailedFn        m_onPlanFailed;
};
=======
#pragma once

// C++20, no Qt. Planning agent; callbacks replace signals.

#include <string>
#include <vector>
#include <functional>

struct Task {
    std::string id;
    std::string description;
    std::string status;
    int priority = 0;
    std::string assignedAgent;
};

class PlanningAgent
{
public:
    using PlanCreatedFn     = std::function<void(const std::string& plan)>;
    using TaskStatusChangedFn = std::function<void(const std::string& taskId, const std::string& status)>;
    using PlanCompletedFn   = std::function<void()>;
    using PlanFailedFn      = std::function<void(const std::string& error)>;

    PlanningAgent() = default;
    virtual ~PlanningAgent() = default;

    void setOnPlanCreated(PlanCreatedFn f)         { m_onPlanCreated = std::move(f); }
    void setOnTaskStatusChanged(TaskStatusChangedFn f) { m_onTaskStatusChanged = std::move(f); }
    void setOnPlanCompleted(PlanCompletedFn f)     { m_onPlanCompleted = std::move(f); }
    void setOnPlanFailed(PlanFailedFn f)           { m_onPlanFailed = std::move(f); }

    void initialize();
    void createPlan(const std::string& goal);
    void executePlan();
    void addTask(const Task& task);
    std::vector<Task> getTasks() const;

private:
    void processNextTask();
    std::string generatePlan(const std::string& goal);
    void updateTaskStatus(const std::string& taskId, const std::string& status);

    std::vector<Task> tasks_;
    int currentTaskIndex_ = 0;

    PlanCreatedFn       m_onPlanCreated;
    TaskStatusChangedFn m_onTaskStatusChanged;
    PlanCompletedFn     m_onPlanCompleted;
    PlanFailedFn        m_onPlanFailed;
};
>>>>>>> origin/main
