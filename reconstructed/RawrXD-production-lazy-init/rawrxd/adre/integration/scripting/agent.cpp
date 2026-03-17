#include "../agents/agent.h"
#include <iostream>

class ScriptingAgent : public Agent {
public:
    AgentID id() const override { return 100; }
    void run(const BeliefState& state) override {
        std::cout << "[ScriptingAgent] Executing script..." << std::endl;
        // ...execute user script on state...
    }
};
