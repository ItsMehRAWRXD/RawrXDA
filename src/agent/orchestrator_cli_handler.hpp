// ============================================================================
// orchestrator_cli_handler.hpp — CLI Command Handler for Autonomous Orchestrator
// ============================================================================
#pragma once

#include "autonomous_orchestrator.hpp"
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace RawrXD {

// ============================================================================
// OrchestratorCLI — Command-line interface for autonomous orchestrator
// ============================================================================
class OrchestratorCLI {
public:
    explicit OrchestratorCLI(AutonomousOrchestrator* orchestrator);
    ~OrchestratorCLI();

    /// Process a command line
    std::string processCommand(const std::string& commandLine);

    /// Process command from arguments
    std::string processArgs(int argc, char** argv);

    /// Parse orchestrator command (--orchestrator format)
    std::string parseOrchestratorCommand(const std::string& cmd, const std::string& configFile);

private:
    // Command handlers
    std::string handleAudit(const std::vector<std::string>& args);
    std::string handleExecute(const std::vector<std::string>& args);
    std::string handleExecuteTopDifficult(const std::vector<std::string>& args);
    std::string handleExecuteTopPriority(const std::vector<std::string>& args);
    std::string handleExecuteCategory(const std::vector<std::string>& args);
    std::string handleStatus();
    std::string handleAutoOptimize();
    std::string handleLoadConfig(const std::string& configFile);
    std::string handleSaveProgress(const std::string& filename);
    std::string handleLoadProgress(const std::string& filename);
    std::string handleSetQualityMode(const std::string& mode);
    std::string handleSetAgentMultiplier(int multiplier);
    std::string handleSetAgentCount(int count);
    std::string handleHelp();

    // Utilities
    std::vector<std::string> parseCommandLine(const std::string& commandLine);
    std::string formatJson(const json& j, bool pretty = true);
    void loadConfigFromJson(const json& j);

    AutonomousOrchestrator* m_orchestrator;
    bool m_verbose = false;
};

} // namespace RawrXD
