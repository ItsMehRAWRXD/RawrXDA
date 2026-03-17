#include "../agent.h"
#include "../../core/hypothesis.h"
#include <iostream>

class ConsistencyAgent : public Agent {
public:
    AgentID id() const override { return 5; }
    void run(const BeliefState& state) override {
        std::cout << "[ConsistencyAgent] Validating consistency..." << std::endl;
        // Example: Check for contradicting facts
        for (auto hid : state.hypotheses) {
            // ...real consistency logic here...
        }
    }
};
