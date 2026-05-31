// Win32IDE_PipelinePanel.cpp — Phase 13: Distributed Pipeline Orchestrator UI
// Win32 IDE panel for DAG-based task scheduling, compute node management,
// pipeline visualization, and work-stealing thread pool monitoring.
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
#include "Win32IDE.h"
#include "../core/distributed_pipeline_orchestrator.hpp"
#include <sstream>
#include <iomanip>

// ============================================================================
// Initialization
// ============================================================================

void Win32IDE::initPipelinePanel() {
    if (m_pipelinePanelInitialized) return;

    appendToOutput("[Pipeline] Phase 13 — Distributed Pipeline Orchestrator initialized.\n");
    m_pipelinePanelInitialized = true;
}

// ============================================================================
// Command Router
// ============================================================================

void Win32IDE::handlePipelineCommand(int commandId) {
    if (!m_pipelinePanelInitialized) initPipelinePanel();

    switch (commandId) {
        case IDM_PIPELINE_STATUS:          cmdPipelineStatus();           break;
        case IDM_PIPELINE_SUBMIT:          cmdPipelineSubmit();           break;
        case IDM_PIPELINE_CANCEL:          cmdPipelineCancel();           break;
        case IDM_PIPELINE_LIST_NODES:      cmdPipelineListNodes();        break;
        case IDM_PIPELINE_ADD_NODE:        cmdPipelineAddNode();          break;
        case IDM_PIPELINE_REMOVE_NODE:     cmdPipelineRemoveNode();       break;
        case IDM_PIPELINE_DAG_VIEW:        cmdPipelineDAGView();          break;
        case IDM_PIPELINE_STATS:           cmdPipelineShowStats();        break;
        case IDM_PIPELINE_SHUTDOWN:        cmdPipelineShutdown();         break;
        default:
            appendToOutput("[Pipeline] Unknown command: " + std::to_string(commandId) + "\n");
            break;
    }
}

// ============================================================================
// Command Handlers
// ============================================================================

void Win32IDE::cmdPipelineStatus() {
    auto& orch = DistributedPipelineOrchestrator::instance();
    auto& s = orch.getStats();

    std::ostringstream oss;
    oss << "╔══════════════════════════════════════════════════════════╗\n"
        << "║        DISTRIBUTED PIPELINE ORCHESTRATOR — STATUS       ║\n"
        << "╠══════════════════════════════════════════════════════════╣\n"
        << "║  Tasks Submitted:   " << std::setw(10) << s.tasksSubmitted.load()  << "                         ║\n"
        << "║  Tasks Completed:   " << std::setw(10) << s.tasksCompleted.load()  << "                         ║\n"
        << "║  Tasks Failed:      " << std::setw(10) << s.tasksFailed.load()     << "                         ║\n"
        << "║  Tasks Retried:     " << std::setw(10) << s.tasksRetried.load()    << "                         ║\n"
        << "║  Steals Attempted:  " << std::setw(10) << s.stealsAttempted.load() << "                         ║\n"
        << "║  Steals Succeeded:  " << std::setw(10) << s.stealsSucceeded.load() << "                         ║\n"
        << "║  Active Pipelines:  " << std::setw(10) << s.activePipelines.load() << "                         ║\n"
        << "╚══════════════════════════════════════════════════════════╝\n";

    appendToOutput(oss.str());
}

void Win32IDE::cmdPipelineSubmit() {
    auto& orch = DistributedPipelineOrchestrator::instance();

    PipelineTask task;
    task.name = "IDE-submitted-task";
    task.priority = TaskPriority::Normal;

    auto result = orch.submitTask(task);
    if (result.success) {
        appendToOutput("[Pipeline] Task submitted successfully (ID: " +
                       std::to_string(task.id) + ")\n");
    } else {
        appendToOutput("[Pipeline] Submit failed: " + std::string(result.detail) + "\n");
    }
}

void Win32IDE::cmdPipelineCancel() {
    appendToOutput("[Pipeline] Cancel requires a task ID. Use Pipeline: Status to view IDs.\n");
}

void Win32IDE::cmdPipelineListNodes() {
    auto& orch = DistributedPipelineOrchestrator::instance();
    auto nodes = orch.getNodeStatus();

    if (nodes.empty()) {
        appendToOutput("[Pipeline] No compute nodes registered.\n");
        return;
    }

    std::ostringstream oss;
    oss << "[Pipeline] Compute Nodes (" << nodes.size() << "):\n";
    for (auto& node : nodes) {
        oss << "  \u25b8 " << node.hostname
            << " [" << node.hostname << "]"
            << "  cores=" << node.totalCores
            << "  load=" << std::fixed << std::setprecision(1) << node.loadAverage
            << "  alive=" << (node.alive ? "YES" : "NO")
            << "\n";
    }
    appendToOutput(oss.str());
}

void Win32IDE::cmdPipelineAddNode() {
    ComputeNode node;
    node.hostname = "127.0.0.1";
    node.totalCores = 8;
    node.availableCores = 8;
    node.totalMemory = 16ULL * 1024 * 1024 * 1024;
    node.availableMemory = 8ULL * 1024 * 1024 * 1024;
    node.hasGPU = false;
    node.gpuCount = 0;
    node.loadAverage = 0.0;
    node.alive = true;

    auto& orch = DistributedPipelineOrchestrator::instance();
    auto result = orch.registerNode(node);
    if (result.success) {
        appendToOutput("[Pipeline] Node registered: " + node.hostname + "\n");
    } else {
        appendToOutput("[Pipeline] Registration failed: " + std::string(result.detail) + "\n");
    }
}

void Win32IDE::cmdPipelineRemoveNode() {
    appendToOutput("[Pipeline] Remove node requires a node ID. Use Pipeline: List Nodes first.\n");
}

void Win32IDE::cmdPipelineDAGView() {
    auto& orch = DistributedPipelineOrchestrator::instance();

    std::ostringstream oss;
    oss << "╔═══════════════════════════════════════════╗\n"
        << "║         PIPELINE DAG VISUALIZATION        ║\n"
        << "╠═══════════════════════════════════════════╣\n";

    auto pending = orch.getPendingTasks();
    if (pending.empty()) {
        oss << "\u2551  No active pipelines.                     \u2551\n";
    } else {
        for (auto taskId : pending) {
            oss << "\u2551  Task: " << std::left << std::setw(30) << taskId << "\u2551\n";
        }
    }
    oss << "╚═══════════════════════════════════════════╝\n";
    appendToOutput(oss.str());
}

void Win32IDE::cmdPipelineShowStats() {
    cmdPipelineStatus();  // Reuse status display
}

void Win32IDE::cmdPipelineShutdown() {
    auto& orch = DistributedPipelineOrchestrator::instance();
    orch.shutdown();
    appendToOutput("[Pipeline] Orchestrator shutdown complete.\n");
}
