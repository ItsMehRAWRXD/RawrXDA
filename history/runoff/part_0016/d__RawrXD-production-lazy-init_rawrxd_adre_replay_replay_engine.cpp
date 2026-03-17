#include "action_log.h"
#include <iostream>

class ReplayEngine {
public:
    ReplayEngine(const ActionLog& log) : log(log) {}
    void replay() {
        for (const auto& action : log.actions) {
            std::cout << "[ReplayEngine] Agent " << action.agent << " rationale: " << action.rationale << std::endl;
            // ...apply facts/hypotheses to state...
        }
    }
private:
    const ActionLog& log;
};
