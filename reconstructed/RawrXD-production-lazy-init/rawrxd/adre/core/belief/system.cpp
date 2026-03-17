// RawrXD ADRE - Belief System Implementation
// Version: 1.0 - 2026-01-22
#include "belief_state.h"
#include "hypothesis.h"
#include "fact.h"
#include <iostream>
#include <algorithm>

class BeliefSystem {
public:
    void reviseBelief(BeliefState& state, Hypothesis& hypo, bool accepted) {
        if (accepted) {
            hypo.status = HypothesisStatus::Accepted;
            hypo.confidence.increase(10);
        } else {
            hypo.status = HypothesisStatus::Rejected;
            hypo.confidence.decrease(10);
        }
        std::cout << "[BeliefSystem] Hypothesis " << hypo.id << " revised to " << (accepted ? "Accepted" : "Rejected") << std::endl;
    }

    void addFact(BeliefState& state, FactID fid) {
        if (std::find(state.facts.begin(), state.facts.end(), fid) == state.facts.end()) {
            state.facts.push_back(fid);
            std::cout << "[BeliefSystem] Fact " << fid << " added to belief state." << std::endl;
        }
    }
};
