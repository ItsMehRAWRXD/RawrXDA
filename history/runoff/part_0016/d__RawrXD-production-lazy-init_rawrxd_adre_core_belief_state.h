#pragma once
#include "ids.h"
#include <vector>

struct BeliefState {
    BeliefStateID id;
    std::vector<FactID> facts;
    std::vector<HypothesisID> hypotheses;
};
