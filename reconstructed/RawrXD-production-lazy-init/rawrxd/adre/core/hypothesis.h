#pragma once
#include "ids.h"
#include "confidence.h"
#include <vector>
#include <string>

enum class HypothesisStatus {
    Proposed,
    Accepted,
    Challenged,
    Rejected
};

struct Hypothesis {
    HypothesisID id;
    std::string claim;
    std::vector<FactID> supportingFacts;
    std::vector<FactID> contradictingFacts;
    Confidence confidence;
    HypothesisStatus status;
};
