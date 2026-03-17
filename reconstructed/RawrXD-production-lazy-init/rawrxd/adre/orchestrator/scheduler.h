#pragma once
#include "../agents/agent.h"
#include <queue>

class AgentScheduler {
public:
    void enqueue(Agent* agent);
    Agent* next();
private:
    std::queue<Agent*> queue;
};
