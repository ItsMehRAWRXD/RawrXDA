#include "../agent.h"
#include "../../core/hypothesis.h"
#include <iostream>

class FunctionRoleAgent : public Agent {
public:
    AgentID id() const override { return 3; }
    void run(const BeliefState& state) override {
        std::cout << "[FunctionRoleAgent] Inferring function roles..." << std::endl;
        // Example: Propose function role hypotheses
        for (auto hid : state.hypotheses) {
            // ...real inference logic here...
        }
    }
};
