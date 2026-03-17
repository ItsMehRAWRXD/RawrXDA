#include "../agent.h"
#include "../../core/fact.h"
#include <iostream>

class CFGAgent : public Agent {
public:
    AgentID id() const override { return 1; }
    void run(const BeliefState& state) override {
        std::cout << "[CFGAgent] Running CFG analysis..." << std::endl;
        // Example: Add CFGEdge facts for each instruction
        for (auto fid : state.facts) {
            // ...real CFG logic here...
        }
    }
};
