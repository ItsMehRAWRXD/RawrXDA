#pragma once


struct Task {
    std::string id;
    std::string description;
    std::string status; // "pending", "in-progress", "completed", "failed"
    int priority;
    std::string assignedAgent;
};

class PlanningAgent : public void {

public:
    explicit PlanningAgent(void* parent = nullptr);
    virtual ~PlanningAgent() = default;
    
    void initialize();
    void createPlan(const std::string& goal);
    void executePlan();
    void addTask(const Task& task);
    std::vector<Task> getTasks() const;


    void planCreated(const std::string& plan);
    void taskStatusChanged(const std::string& taskId, const std::string& status);
    void planCompleted();
    void planFailed(const std::string& error);
    
private:
    void processNextTask();
    
private:
    std::vector<Task> tasks_;
    void** taskProcessor_;
    int currentTaskIndex_;
    
    std::string generatePlan(const std::string& goal);
    void updateTaskStatus(const std::string& taskId, const std::string& status);
};
