#pragma once


class MetaPlanner {
public:
    // natural language  JSON task list
    void* plan(const std::string& humanWish);

    // decompose high-level goal into sub-tasks (simple wrapper for now)
    void* decomposeGoal(const std::string& goal) { return plan(goal); }

private:
    void* quantPlan(const std::string& wish);
    void* kernelPlan(const std::string& wish);
    void* releasePlan(const std::string& wish);
    void* fixPlan(const std::string& wish);
    void* perfPlan(const std::string& wish);
    void* testPlan(const std::string& wish);
    void* genericPlan(const std::string& wish);

    void* task(const std::string& type,
                     const std::string& target,
                     const void*& params = {});
};

