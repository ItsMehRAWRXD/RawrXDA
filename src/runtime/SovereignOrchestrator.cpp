#include "SovereignOrchestrator.h"
#include <iostream>
#include <mutex>

namespace RawrXD::Runtime {

static std::mutex g_orchMutex;

SovereignOrchestrator& SovereignOrchestrator::instance() {
    static SovereignOrchestrator instance;
    return instance;
}

uint32_t SovereignOrchestrator::planTask(const std::string& taskDescription) {
    std::lock_guard<std::mutex> lock(g_orchMutex);
    if (!m_initialized) return 0;

    // Record intention to memory (Three-Layer Indexing)
    SovereignMemoryBridge::instance().recordDecision("TASK_PLAN", taskDescription);

    // Initial simple mapping: Task -> Tool
    // In final Sovereign form, this will involve a routing step through QueryEngine for tool selection
    uint32_t taskId = SovereignToolBridge::instance().dispatchTool("PLANNER_TOOL", taskDescription, 10);
    
    if (taskId != 0) {
        m_tasks[taskId] = taskDescription;
        std::cout << "[Orchestrator] Task Planned and Dispatched: ID=" << taskId << std::endl;
    }

    return taskId;
}

bool SovereignOrchestrator::checkTask(uint32_t taskId, ToolStatus& status) {
    if (!m_initialized) return false;
    
    status = SovereignToolBridge::instance().getToolStatus(taskId);
    
    // Auto-consolidation into historical memory if completed
    if (status.state == 2 || status.state == 3) {
        std::string resultLabel = (status.state == 2) ? "TASK_COMPLETED" : "TASK_FAILED";
        SovereignMemoryBridge::instance().recordDecision(resultLabel, 
            "{ \"taskId\": " + std::to_string(taskId) + ", \"exitCode\": " + std::to_string(status.exitCode) + " }");
    }

    return true;
}

} // namespace RawrXD::Runtime
