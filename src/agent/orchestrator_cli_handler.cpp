// ============================================================================
// orchestrator_cli_handler.cpp — CLI Command Handler Implementation
// ============================================================================
#include "orchestrator_cli_handler.hpp"
#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>

namespace RawrXD {

OrchestratorCLI::OrchestratorCLI(AutonomousOrchestrator* orchestrator)
    : m_orchestrator(orchestrator)
{
}

OrchestratorCLI::~OrchestratorCLI() = default;

std::string OrchestratorCLI::processCommand(const std::string& commandLine) {
    auto args = parseCommandLine(commandLine);
    if (args.empty()) {
        return formatJson(json{{"error", "Empty command"}}, false);
    }

    std::string cmd = args[0];
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);

    try {
        if (cmd == "audit") {
            return handleAudit(args);
        } else if (cmd == "execute") {
            return handleExecute(args);
        } else if (cmd == "execute-top-difficult") {
            return handleExecuteTopDifficult(args);
        } else if (cmd == "execute-top-priority") {
            return handleExecuteTopPriority(args);
        } else if (cmd == "execute-category") {
            return handleExecuteCategory(args);
        } else if (cmd == "status") {
            return handleStatus();
        } else if (cmd == "auto-optimize") {
            return handleAutoOptimize();
        } else if (cmd == "load-config") {
            if (args.size() < 2) return formatJson(json{{"error", "Usage: load-config <file>"}}, false);
            return handleLoadConfig(args[1]);
        } else if (cmd == "save-progress") {
            std::string file = args.size() > 1 ? args[1] : "";
            return handleSaveProgress(file);
        } else if (cmd == "load-progress") {
            std::string file = args.size() > 1 ? args[1] : "";
            return handleLoadProgress(file);
        } else if (cmd == "set-quality-mode") {
            if (args.size() < 2) return formatJson(json{{"error", "Usage: set-quality-mode <Auto|Balance|Max>"}}, false);
            return handleSetQualityMode(args[1]);
        } else if (cmd == "set-agent-multiplier") {
            if (args.size() < 2) return formatJson(json{{"error", "Usage: set-agent-multiplier <1-99>"}}, false);
            return handleSetAgentMultiplier(std::stoi(args[1]));
        } else if (cmd == "set-agent-count") {
            if (args.size() < 2) return formatJson(json{{"error", "Usage: set-agent-count <1-99>"}}, false);
            return handleSetAgentCount(std::stoi(args[1]));
        } else if (cmd == "help") {
            return handleHelp();
        } else {
            return formatJson(json{{"error", "Unknown command: " + cmd}}, false);
        }
    } catch (const std::exception& e) {
        return formatJson(json{{"error", std::string("Exception: ") + e.what()}}, false);
    }
}

std::string OrchestratorCLI::processArgs(int argc, char** argv) {
    std::string commandLine;
    for (int i = 1; i < argc; ++i) {
        commandLine += argv[i];
        if (i < argc - 1) commandLine += " ";
    }
    return processCommand(commandLine);
}

std::string OrchestratorCLI::parseOrchestratorCommand(const std::string& cmd, const std::string& configFile) {
    // Load config if provided
    if (!configFile.empty()) {
        handleLoadConfig(configFile);
    }

    // Parse command format: "command:arg1:arg2:arg3"
    std::vector<std::string> parts;
    std::istringstream iss(cmd);
    std::string part;
    while (std::getline(iss, part, ':')) {
        parts.push_back(part);
    }

    if (parts.empty()) {
        return formatJson(json{{"error", "Empty orchestrator command"}}, false);
    }

    std::string mainCmd = parts[0];
    std::transform(mainCmd.begin(), mainCmd.end(), mainCmd.begin(), ::tolower);

    try {
        if (mainCmd == "audit") {
            std::vector<std::string> args = {"audit"};
            if (parts.size() > 1) args.push_back(parts[1]); // path
            if (parts.size() > 2 && parts[2] == "true") args.push_back("--deep");
            return handleAudit(args);
        } else if (mainCmd == "execute") {
            std::vector<std::string> args = {"execute"};
            if (parts.size() > 1) args.push_back(parts[1]); // mode
            return handleExecute(args);
        } else if (mainCmd == "execute-top-difficult") {
            std::vector<std::string> args = {"execute-top-difficult"};
            if (parts.size() > 1) args.push_back(parts[1]); // count
            return handleExecuteTopDifficult(args);
        } else if (mainCmd == "execute-top-priority") {
            std::vector<std::string> args = {"execute-top-priority"};
            if (parts.size() > 1) args.push_back(parts[1]); // count
            return handleExecuteTopPriority(args);
        } else if (mainCmd == "auto-optimize") {
            return handleAutoOptimize();
        } else if (mainCmd == "status") {
            return handleStatus();
        } else {
            return formatJson(json{{"error", "Unknown orchestrator command: " + mainCmd}}, false);
        }
    } catch (const std::exception& e) {
        return formatJson(json{{"error", std::string("Exception: ") + e.what()}}, false);
    }
}

// ============================================================================
// Command Handlers
// ============================================================================
std::string OrchestratorCLI::handleAudit(const std::vector<std::string>& args) {
    std::string path = args.size() > 1 ? args[1] : ".";
    bool deep = false;
    for (size_t i = 2; i < args.size(); ++i) {
        if (args[i] == "--deep" || args[i] == "-d") {
            deep = true;
        }
    }

    std::cout << "[Orchestrator] Auditing: " << path << " (deep=" << deep << ")\n";

    auto result = m_orchestrator->auditCodebase(path, deep);

    std::cout << "[Orchestrator] Audit complete. Found " << result.todos.size() << " todos.\n";

    return formatJson(result.toJSON());
}

std::string OrchestratorCLI::handleExecute(const std::vector<std::string>& args) {
    std::string mode = args.size() > 1 ? args[1] : "all";

    std::cout << "[Orchestrator] Executing in mode: " << mode << "\n";

    bool success = false;
    if (mode == "all") {
        success = m_orchestrator->execute();
    } else if (mode == "top-priority") {
        success = m_orchestrator->executeTopPriority(20);
    } else if (mode == "top-difficult") {
        success = m_orchestrator->executeTopDifficult(20);
    } else {
        success = m_orchestrator->executeCategory(mode);
    }

    auto stats = m_orchestrator->getStats();

    json result = stats.toJSON();
    result["success"] = success;

    std::cout << "[Orchestrator] Execution " << (success ? "completed" : "failed") << ".\n";

    return formatJson(result);
}

std::string OrchestratorCLI::handleExecuteTopDifficult(const std::vector<std::string>& args) {
    int count = 20;
    if (args.size() > 1) {
        count = std::stoi(args[1]);
    }

    std::cout << "[Orchestrator] Executing top " << count << " most difficult tasks...\n";

    bool success = m_orchestrator->executeTopDifficult(count);

    auto stats = m_orchestrator->getStats();

    json result = stats.toJSON();
    result["success"] = success;
    result["executedCount"] = count;

    return formatJson(result);
}

std::string OrchestratorCLI::handleExecuteTopPriority(const std::vector<std::string>& args) {
    int count = 20;
    if (args.size() > 1) {
        count = std::stoi(args[1]);
    }

    std::cout << "[Orchestrator] Executing top " << count << " priority tasks...\n";

    bool success = m_orchestrator->executeTopPriority(count);

    auto stats = m_orchestrator->getStats();

    json result = stats.toJSON();
    result["success"] = success;
    result["executedCount"] = count;

    return formatJson(result);
}

std::string OrchestratorCLI::handleExecuteCategory(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        return formatJson(json{{"error", "Usage: execute-category <category>"}}, false);
    }

    std::string category = args[1];

    std::cout << "[Orchestrator] Executing category: " << category << "\n";

    bool success = m_orchestrator->executeCategory(category);

    auto stats = m_orchestrator->getStats();

    json result = stats.toJSON();
    result["success"] = success;
    result["category"] = category;

    return formatJson(result);
}

std::string OrchestratorCLI::handleStatus() {
    auto status = m_orchestrator->getDetailedStatus();
    std::cout << "[Orchestrator] Status retrieved.\n";
    return formatJson(status);
}

std::string OrchestratorCLI::handleAutoOptimize() {
    std::cout << "[Orchestrator] Running auto-optimization...\n";

    m_orchestrator->autoOptimize();

    auto recommendations = m_orchestrator->getOptimizationRecommendations();

    std::cout << "[Orchestrator] Auto-optimization complete.\n";

    return formatJson(recommendations);
}

std::string OrchestratorCLI::handleLoadConfig(const std::string& configFile) {
    try {
        std::ifstream ifs(configFile);
        if (!ifs.good()) {
            return formatJson(json{{"error", "Config file not found: " + configFile}}, false);
        }

        json configJson = json::parse(ifs);
        loadConfigFromJson(configJson);

        std::cout << "[Orchestrator] Config loaded from: " << configFile << "\n";

        return formatJson(json{{"success", true, "configFile", configFile}});
    } catch (const std::exception& e) {
        return formatJson(json{{"error", std::string("Failed to load config: ") + e.what()}}, false);
    }
}

std::string OrchestratorCLI::handleSaveProgress(const std::string& filename) {
    bool success = m_orchestrator->saveProgress(filename);

    json result;
    result["success"] = success;
    if (success) {
        result["file"] = filename.empty() ? "orchestrator_progress.json" : filename;
        std::cout << "[Orchestrator] Progress saved.\n";
    } else {
        result["error"] = "Failed to save progress";
    }

    return formatJson(result);
}

std::string OrchestratorCLI::handleLoadProgress(const std::string& filename) {
    bool success = m_orchestrator->loadProgress(filename);

    json result;
    result["success"] = success;
    if (success) {
        result["file"] = filename.empty() ? "orchestrator_progress.json" : filename;
        std::cout << "[Orchestrator] Progress loaded.\n";
    } else {
        result["error"] = "Failed to load progress";
    }

    return formatJson(result);
}

std::string OrchestratorCLI::handleSetQualityMode(const std::string& mode) {
    QualityMode qm;
    if (mode == "Auto" || mode == "auto" || mode == "0") {
        qm = QualityMode::Auto;
    } else if (mode == "Balance" || mode == "balance" || mode == "1") {
        qm = QualityMode::Balance;
    } else if (mode == "Max" || mode == "max" || mode == "2") {
        qm = QualityMode::Max;
    } else {
        return formatJson(json{{"error", "Invalid quality mode: " + mode}}, false);
    }

    m_orchestrator->setQualityMode(qm);

    std::cout << "[Orchestrator] Quality mode set to: " << mode << "\n";

    return formatJson(json{{"success", true, "qualityMode", mode}});
}

std::string OrchestratorCLI::handleSetAgentMultiplier(int multiplier) {
    if (multiplier < 1 || multiplier > 99) {
        return formatJson(json{{"error", "Agent multiplier must be between 1 and 99"}}, false);
    }

    m_orchestrator->setAgentCycleMultiplier(multiplier);

    std::cout << "[Orchestrator] Agent multiplier set to: " << multiplier << "x\n";

    return formatJson(json{{"success", true, "agentMultiplier", multiplier}});
}

std::string OrchestratorCLI::handleSetAgentCount(int count) {
    if (count < 1 || count > 99) {
        return formatJson(json{{"error", "Agent count must be between 1 and 99"}}, false);
    }

    m_orchestrator->setAgentCount(count);

    std::cout << "[Orchestrator] Agent count set to: " << count << "\n";

    return formatJson(json{{"success", true, "agentCount", count}});
}

std::string OrchestratorCLI::handleHelp() {
    json help;
    help["commands"] = json::array({
        json{{"name", "audit"}, {"args", "[path] [--deep]"}, {"desc", "Audit codebase"}},
        json{{"name", "execute"}, {"args", "[mode]"}, {"desc", "Execute todos (modes: all, top-priority, top-difficult, <category>)"}},
        json{{"name", "execute-top-difficult"}, {"args", "[count]"}, {"desc", "Execute top N difficult tasks"}},
        json{{"name", "execute-top-priority"}, {"args", "[count]"}, {"desc", "Execute top N priority tasks"}},
        json{{"name", "execute-category"}, {"args", "<category>"}, {"desc", "Execute todos of specific category"}},
        json{{"name", "status"}, {"args", ""}, {"desc", "Show orchestrator status"}},
        json{{"name", "auto-optimize"}, {"args", ""}, {"desc", "Analyze and optimize settings"}},
        json{{"name", "load-config"}, {"args", "<file>"}, {"desc", "Load configuration from JSON"}},
        json{{"name", "save-progress"}, {"args", "[file]"}, {"desc", "Save current progress"}},
        json{{"name", "load-progress"}, {"args", "[file]"}, {"desc", "Load saved progress"}},
        json{{"name", "set-quality-mode"}, {"args", "<Auto|Balance|Max>"}, {"desc", "Set quality mode"}},
        json{{"name", "set-agent-multiplier"}, {"args", "<1-99>"}, {"desc", "Set agent cycle multiplier"}},
        json{{"name", "set-agent-count"}, {"args", "<1-99>"}, {"desc", "Set parallel agent count"}},
        json{{"name", "help"}, {"args", ""}, {"desc", "Show this help"}}
    });

    return formatJson(help);
}

// ============================================================================
// Utilities
// ============================================================================
std::vector<std::string> OrchestratorCLI::parseCommandLine(const std::string& commandLine) {
    std::vector<std::string> args;
    std::istringstream iss(commandLine);
    std::string arg;

    bool inQuotes = false;
    std::string currentArg;

    for (char ch : commandLine) {
        if (ch == '"') {
            inQuotes = !inQuotes;
        } else if (std::isspace(static_cast<unsigned char>(ch)) && !inQuotes) {
            if (!currentArg.empty()) {
                args.push_back(currentArg);
                currentArg.clear();
            }
        } else {
            currentArg += ch;
        }
    }

    if (!currentArg.empty()) {
        args.push_back(currentArg);
    }

    return args;
}

std::string OrchestratorCLI::formatJson(const json& j, bool pretty) {
    return j.dump(pretty ? 2 : -1);
}

void OrchestratorCLI::loadConfigFromJson(const json& j) {
    if (j.is_null()) return;

    auto config = OrchestratorConfig::fromJSON(j);
    m_orchestrator->setConfig(config);
}

} // namespace RawrXD
