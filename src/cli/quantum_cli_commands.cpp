// quantum_cli_commands.cpp — Implementation of Quantum CLI Commands

#include "quantum_cli_commands.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace RawrXD {
namespace Quantum {
namespace CLI {

// Helper to parse command
static std::vector<std::string> splitCommand(const std::string& input) {
    std::vector<std::string> tokens;
    std::istringstream iss(input);
    std::string token;
    while (iss >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

bool handleQuantumCommand(const std::string& input) {
    // Check if this is a quantum command
    if (input.find("!quantum") != 0) {
        return false;
    }
    
    auto tokens = splitCommand(input);
    if (tokens.size() < 2) {
        showQuantumHelp();
        return true;
    }
    
    std::string subcommand = tokens[1];
    std::transform(subcommand.begin(), subcommand.end(), subcommand.begin(), ::tolower);
    
    if (subcommand == "help") {
        showQuantumHelp();
    } else if (subcommand == "status") {
        showQuantumStatus();
    } else if (subcommand == "mode" && tokens.size() >= 3) {
        setQuantumMode(tokens[2]);
    } else if (subcommand == "models" && tokens.size() >= 3) {
        try {
            int count = std::stoi(tokens[2]);
            setModelCount(count);
        } catch (...) {
            std::cout << "❌ Invalid model count. Use: !quantum models <1-99>\n";
        }
    } else if (subcommand == "agents" && tokens.size() >= 3) {
        try {
            int count = std::stoi(tokens[2]);
            setAgentCycleCount(count);
        } catch (...) {
            std::cout << "❌ Invalid agent cycle count. Use: !quantum agents <1-99>\n";
        }
    } else if (subcommand == "exec" && tokens.size() >= 3) {
        // Reconstruct description from remaining tokens
        std::string description;
        for (size_t i = 2; i < tokens.size(); ++i) {
            if (i > 2) description += " ";
            description += tokens[i];
        }
        executeQuantumTask(description);
    } else if (subcommand == "audit" && tokens.size() >= 3) {
        auditProduction(tokens[2]);
    } else if (subcommand == "audit-exec" && tokens.size() >= 3) {
        try {
            int count = std::stoi(tokens[2]);
            executeTopAuditItems(count);
        } catch (...) {
            std::cout << "❌ Invalid count. Use: !quantum audit-exec <N>\n";
        }
    } else if (subcommand == "stats") {
        showStatistics();
    } else if (subcommand == "timeout" && tokens.size() >= 3) {
        configureTimeouts(tokens[2]);
    } else if (subcommand == "bypass" && tokens.size() >= 3) {
        std::string type = tokens[2];
        std::transform(type.begin(), type.end(), type.begin(), ::tolower);
        if (type == "token") {
            setBypassLimits(true, false, false);
        } else if (type == "complexity") {
            setBypassLimits(false, true, false);
        } else if (type == "time") {
            setBypassLimits(false, false, true);
        } else if (type == "all") {
            setBypassLimits(true, true, true);
        } else {
            std::cout << "❌ Unknown bypass type. Use: token, complexity, time, or all\n";
        }
    } else {
        std::cout << "❌ Unknown quantum command: " << subcommand << "\n";
        showQuantumHelp();
    }
    
    return true;
}

void showQuantumHelp() {
    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  QUANTUM AGENT ORCHESTRATOR — Multi-Model (1x-99x) Command Interface ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════════════╝\n";
    std::cout << "\n";
    std::cout << "CONFIGURATION COMMANDS:\n";
    std::cout << "  !quantum mode <auto|balance|max>   Set quality mode\n";
    std::cout << "                                      • auto    - Auto-select based on complexity\n";
    std::cout << "                                      • balance - Balance speed and quality\n";
    std::cout << "                                      • max     - Maximum quality (bypass limits)\n";
    std::cout << "\n";
    std::cout << "  !quantum models <1-99>              Set parallel model count (1x-99x)\n";
    std::cout << "  !quantum agents <1-99>              Set agent cycle count (1x-99x)\n";
    std::cout << "  !quantum timeout <auto|fixed>       Configure timeout adjustment\n";
    std::cout << "                                      • auto  - ML-based auto-adjustment\n";
    std::cout << "                                      • fixed - Use base timeout only\n";
    std::cout << "\n";
    std::cout << "BYPASS CONTROLS (MAX MODE):\n";
    std::cout << "  !quantum bypass <type>              Toggle bypass flags\n";
    std::cout << "                                      • token      - Bypass token limits\n";
    std::cout << "                                      • complexity - Bypass complexity limits\n";
    std::cout << "                                      • time       - Bypass time limits\n";
    std::cout << "                                      • all        - Bypass all limits\n";
    std::cout << "\n";
    std::cout << "EXECUTION COMMANDS:\n";
    std::cout << "  !quantum exec <description>         Execute task with current settings\n";
    std::cout << "  !quantum audit <path>               Audit codebase for production readiness\n";
    std::cout << "  !quantum audit-exec <N>             Execute top N audit items automatically\n";
    std::cout << "\n";
    std::cout << "STATUS & STATISTICS:\n";
    std::cout << "  !quantum status                     Show current configuration\n";
    std::cout << "  !quantum stats                      Show execution statistics\n";
    std::cout << "  !quantum help                       Show this help message\n";
    std::cout << "\n";
    std::cout << "EXAMPLE WORKFLOWS:\n";
    std::cout << "  # Maximum quality mode with 8x models and 8x agents\n";
    std::cout << "  !quantum mode max\n";
    std::cout << "  !quantum models 8\n";
    std::cout << "  !quantum agents 8\n";
    std::cout << "  !quantum bypass all\n";
    std::cout << "  !quantum exec Refactor the model loader with optimal architecture\n";
    std::cout << "\n";
    std::cout << "  # Production audit and auto-fix top 20 issues\n";
    std::cout << "  !quantum audit D:\\rawrxd\\src\n";
    std::cout << "  !quantum mode max\n";
    std::cout << "  !quantum models 8\n";
    std::cout << "  !quantum audit-exec 20\n";
    std::cout << "\n";
    std::cout << "  # Balance mode for everyday tasks\n";
    std::cout << "  !quantum mode balance\n";
    std::cout << "  !quantum models 3\n";
    std::cout << "  !quantum agents 3\n";
    std::cout << "  !quantum exec Add unit tests for the agent bridge\n";
    std::cout << "\n";
}

void showQuantumStatus() {
    auto& orchestrator = globalQuantumOrchestrator();
    auto strategy = orchestrator.getStrategy();
    auto stats = orchestrator.getStatistics();
    
    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  QUANTUM ORCHESTRATOR STATUS                                         ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════════════╝\n";
    std::cout << "\n";
    std::cout << "CONFIGURATION:\n";
    std::cout << "  Quality Mode:         ";
    switch (strategy.mode) {
        case QualityMode::Auto: std::cout << "AUTO (Adaptive)\n"; break;
        case QualityMode::Balance: std::cout << "BALANCE (Speed + Quality)\n"; break;
        case QualityMode::Max: std::cout << "MAX (Maximum Quality)\n"; break;
    }
    std::cout << "  Parallel Models:      " << strategy.modelCount << "x\n";
    std::cout << "  Agent Cycles:         " << strategy.agentCycleCount << "x\n";
    std::cout << "  Base Timeout:         " << strategy.baseTimeoutMs << "ms\n";
    std::cout << "  Auto-Adjust Timeout:  " << (strategy.autoAdjustTimeout ? "ENABLED" : "DISABLED") << "\n";
    std::cout << "\n";
    std::cout << "BYPASS FLAGS:\n";
    std::cout << "  Token Limits:         " << (strategy.bypassTokenLimits ? "BYPASSED ⚡" : "Enforced") << "\n";
    std::cout << "  Complexity Limits:    " << (strategy.bypassComplexityLimits ? "BYPASSED ⚡" : "Enforced") << "\n";
    std::cout << "  Time Limits:          " << (strategy.bypassTimeLimits ? "BYPASSED ⚡" : "Enforced") << "\n";
    std::cout << "\n";
    std::cout << "LIFETIME STATISTICS:\n";
    std::cout << "  Tasks Executed:       " << stats.totalTasksExecuted << "\n";
    std::cout << "  Total Iterations:     " << stats.totalIterations << "\n";
    std::cout << "  Models Used:          " << stats.totalModelsUsed << "\n";
    std::cout << "  Agent Cycles:         " << stats.totalAgentCycles << "\n";
    std::cout << "  Success Rate:         " << std::fixed << std::setprecision(1) 
              << (stats.successRate * 100) << "%\n";
    std::cout << "  Avg Iterations/Task:  " << stats.avgIterationsPerTask << "\n";
    std::cout << "\n";
}

void setQuantumMode(const std::string& mode) {
    auto& orchestrator = globalQuantumOrchestrator();
    
    std::string lower = mode;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    if (lower == "auto") {
        orchestrator.setQualityMode(QualityMode::Auto);
        std::cout << "✓ Quality mode set to AUTO (adaptive complexity-based selection)\n";
    } else if (lower == "balance") {
        orchestrator.setQualityMode(QualityMode::Balance);
        std::cout << "✓ Quality mode set to BALANCE (speed + quality)\n";
    } else if (lower == "max") {
        orchestrator.setQualityMode(QualityMode::Max);
        std::cout << "✓ Quality mode set to MAX (maximum quality, bypass limits)\n";
        std::cout << "  ⚡ Token and complexity limits automatically bypassed\n";
    } else {
        std::cout << "❌ Unknown mode: " << mode << "\n";
        std::cout << "   Valid modes: auto, balance, max\n";
    }
}

void setModelCount(int count) {
    auto& orchestrator = globalQuantumOrchestrator();
    
    if (count < 1 || count > 99) {
        std::cout << "❌ Model count must be between 1 and 99\n";
        return;
    }
    
    orchestrator.setModelCount(count);
    std::cout << "✓ Parallel model count set to " << count << "x\n";
    
    if (count >= 8) {
        std::cout << "  ⚡ QUANTUM MODE: " << count << "x parallel execution enabled\n";
    } else if (count >= 3) {
        std::cout << "  🔄 Multi-model consensus mode active\n";
    }
}

void setAgentCycleCount(int count) {
    auto& orchestrator = globalQuantumOrchestrator();
    
    if (count < 1 || count > 99) {
        std::cout << "❌ Agent cycle count must be between 1 and 99\n";
        return;
    }
    
    orchestrator.setAgentCycleCount(count);
    std::cout << "✓ Agent cycle count set to " << count << "x\n";
    
    if (count >= 8) {
        std::cout << "  ⚡ Deep iterative refinement enabled\n";
    }
}

void executeQuantumTask(const std::string& description) {
    auto& orchestrator = globalQuantumOrchestrator();
    
    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  EXECUTING QUANTUM TASK                                              ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════════════╝\n";
    std::cout << "\n";
    std::cout << "Task: " << description << "\n";
    std::cout << "\n";
    
    // Execute with auto strategy selection
    auto result = orchestrator.executeTaskAuto(description, {});
    
    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  EXECUTION RESULTS                                                   ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════════════╝\n";
    std::cout << "\n";
    std::cout << "Status:           " << (result.success ? "✅ SUCCESS" : "❌ FAILED") << "\n";
    std::cout << "Iterations:       " << result.iterationCount << "\n";
    std::cout << "Models Used:      " << result.modelCount << "x\n";
    std::cout << "Agent Cycles:     " << result.agentCycleCount << "x\n";
    std::cout << "Duration:         " << result.totalDurationMs << "ms\n";
    std::cout << "Mode Used:        ";
    switch (result.modeUsed) {
        case QualityMode::Auto: std::cout << "AUTO\n"; break;
        case QualityMode::Balance: std::cout << "BALANCE\n"; break;
        case QualityMode::Max: std::cout << "MAX\n"; break;
    }
    if (result.timeoutAdjusted) {
        std::cout << "Timeout Adjusted: " << result.adjustedTimeoutMs << "ms\n";
    }
    std::cout << "\n";
    std::cout << "Result: " << result.detail << "\n";
    std::cout << "\n";
}

void auditProduction(const std::string& rootPath) {
    auto& orchestrator = globalQuantumOrchestrator();
    
    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  PRODUCTION READINESS AUDIT                                          ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════════════╝\n";
    std::cout << "\n";
    std::cout << "Scanning: " << rootPath << "\n";
    std::cout << "\n";
    
    auto entries = orchestrator.auditProductionReadiness(rootPath);
    
    // Sort by priority
    std::sort(entries.begin(), entries.end(),
              [](const AuditEntry& a, const AuditEntry& b) {
                  return a.priorityScore > b.priorityScore;
              });
    
    std::cout << "╔══════════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  AUDIT RESULTS                                                       ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════════════╝\n";
    std::cout << "\n";
    std::cout << "Total Issues Found: " << entries.size() << "\n";
    std::cout << "\n";
    
    // Show top 20
    int count = 0;
    for (const auto& entry : entries) {
        if (count >= 20) break;
        
        std::cout << "[" << std::setw(3) << entry.priorityScore << "] ";
        
        if (entry.requiresImmediate) {
            std::cout << "🔴 CRITICAL ";
        } else if (entry.priorityScore >= 70) {
            std::cout << "🟠 HIGH     ";
        } else if (entry.priorityScore >= 40) {
            std::cout << "🟡 MEDIUM   ";
        } else {
            std::cout << "🟢 LOW      ";
        }
        
        std::cout << entry.subsystem << ": " << entry.detail << "\n";
        count++;
    }
    
    std::cout << "\n";
    std::cout << "Use '!quantum audit-exec <N>' to automatically fix top N issues\n";
    std::cout << "\n";
}

void executeTopAuditItems(int count) {
    auto& orchestrator = globalQuantumOrchestrator();
    auto strategy = orchestrator.getStrategy();
    
    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  EXECUTING TOP " << std::setw(2) << count << " AUDIT ITEMS                                        ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════════════╝\n";
    std::cout << "\n";
    
    // Get audit entries
    auto entries = orchestrator.auditProductionReadiness("D:\\rawrxd\\src");
    
    // Sort by priority
    std::sort(entries.begin(), entries.end(),
              [](const AuditEntry& a, const AuditEntry& b) {
                  return a.priorityScore > b.priorityScore;
              });
    
    // Execute top N
    auto result = orchestrator.executeAuditItems(entries, count, strategy);
    
    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  AUDIT EXECUTION COMPLETE                                            ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════════════╝\n";
    std::cout << "\n";
    std::cout << "Result: " << result.detail << "\n";
    std::cout << "\n";
}

void showStatistics() {
    auto& orchestrator = globalQuantumOrchestrator();
    auto stats = orchestrator.getStatistics();
    
    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  QUANTUM ORCHESTRATOR STATISTICS                                     ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════════════╝\n";
    std::cout << "\n";
    std::cout << "EXECUTION SUMMARY:\n";
    std::cout << "  Total Tasks:          " << stats.totalTasksExecuted << "\n";
    std::cout << "  Total Iterations:     " << stats.totalIterations << "\n";
    std::cout << "  Avg Iter/Task:        " << stats.avgIterationsPerTask << "\n";
    std::cout << "  Success Rate:         " << std::fixed << std::setprecision(1) 
              << (stats.successRate * 100) << "%\n";
    std::cout << "\n";
    std::cout << "RESOURCE USAGE:\n";
    std::cout << "  Models Used:          " << stats.totalModelsUsed << "\n";
    std::cout << "  Agent Cycles:         " << stats.totalAgentCycles << "\n";
    std::cout << "  Total Duration:       " << stats.totalDurationMs << "ms\n";
    std::cout << "\n";
    std::cout << "MODE DISTRIBUTION:\n";
    for (const auto& [mode, count] : stats.modeUsageCount) {
        std::cout << "  ";
        switch (mode) {
            case QualityMode::Auto: std::cout << "AUTO:    "; break;
            case QualityMode::Balance: std::cout << "BALANCE: "; break;
            case QualityMode::Max: std::cout << "MAX:     "; break;
        }
        std::cout << count << " tasks\n";
    }
    std::cout << "\n";
}

void configureTimeouts(const std::string& mode) {
    auto& orchestrator = globalQuantumOrchestrator();
    auto strategy = orchestrator.getStrategy();
    
    std::string lower = mode;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    if (lower == "auto") {
        strategy.autoAdjustTimeout = true;
        orchestrator.setStrategy(strategy);
        std::cout << "✓ Timeout mode set to AUTO (ML-based adjustment)\n";
        std::cout << "  ⚡ Timeouts will adapt based on task complexity and history\n";
    } else if (lower == "fixed") {
        strategy.autoAdjustTimeout = false;
        orchestrator.setStrategy(strategy);
        std::cout << "✓ Timeout mode set to FIXED (base timeout only)\n";
        std::cout << "  Using base timeout: " << strategy.baseTimeoutMs << "ms\n";
    } else {
        std::cout << "❌ Unknown timeout mode: " << mode << "\n";
        std::cout << "   Valid modes: auto, fixed\n";
    }
}

void setBypassLimits(bool token, bool complexity, bool time) {
    auto& orchestrator = globalQuantumOrchestrator();
    
    if (token) {
        orchestrator.setBypassTokenLimits(true);
        std::cout << "✓ Token limits BYPASSED ⚡\n";
    }
    if (complexity) {
        orchestrator.setBypassComplexityLimits(true);
        std::cout << "✓ Complexity limits BYPASSED ⚡\n";
    }
    if (time) {
        orchestrator.setBypassTimeLimits(true);
        std::cout << "✓ Time limits BYPASSED ⚡\n";
    }
    
    std::cout << "  ⚠️ WARNING: Bypassed limits may result in long execution times\n";
}

} // namespace CLI
} // namespace Quantum
} // namespace RawrXD
