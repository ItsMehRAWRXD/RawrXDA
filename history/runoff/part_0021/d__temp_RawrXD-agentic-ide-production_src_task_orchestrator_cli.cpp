#include "task_orchestrator.h"
#include <iostream>

int main() {
    std::vector<Task> tasks;
    ArchitectAgent architect(tasks);
    architect.analyzeGoal("Implement premium billing flow for JIRA-204");

    // Default 4 threads; configurable via RAWRXD_AGENT_THREADS
    architect.startOrchestra(4);

    std::cout << "Task orchestrator run complete. Metrics appended to d:/app_output.txt" << std::endl;
    return 0;
}
