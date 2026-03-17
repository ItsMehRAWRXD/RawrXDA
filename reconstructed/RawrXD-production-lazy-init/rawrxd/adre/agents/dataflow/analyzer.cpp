#include "agent.h"
#include "../core/fact.h"
#include <iostream>

class DataflowAnalyzer : public Agent {
public:
    AgentID id() const override { return 11; }
    void run(const BeliefState& state) override {
        std::cout << "[DataflowAnalyzer] Performing dataflow analysis..." << std::endl;
        // ...real dataflow analysis logic...
    }
};
