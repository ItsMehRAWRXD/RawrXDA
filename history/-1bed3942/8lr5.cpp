#include "agent.h"
#include "../core/fact.h"
#include <iostream>

class CFGAnalyzer : public Agent {
public:
    AgentID id() const override { return 10; }
    void run(const BeliefState& state) override {
        std::cout << "[CFGAnalyzer] Analyzing control flow..." << std::endl;
        // ...real CFG analysis logic...
    }
};
