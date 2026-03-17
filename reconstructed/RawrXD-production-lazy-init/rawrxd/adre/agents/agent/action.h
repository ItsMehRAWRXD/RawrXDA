#pragma once
#include "../core/ids.h"
#include <string>
#include <vector>

struct AgentAction {
    AgentID agent;
    BeliefStateID input;
    std::vector<FactID> factsAdded;
    std::vector<HypothesisID> hypothesesAdded;
    std::string rationale;
};
