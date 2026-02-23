// ide_integration_agent.cpp — Autonomous IDE orchestration agent
#include "ide_integration_agent.h"
#include <sstream>
#include <algorithm>
#include <iostream>

namespace rawrxd::agent {

IDEIntegrationAgent::IDEIntegrationAgent() : m_state(AgentState::Idle) {}

IDEIntegrationAgent::~IDEIntegrationAgent() {}

std::string IDEIntegrationAgent::getStateString() const {
    switch (m_state) {
        case AgentState::Idle:       return "Idle";
        case AgentState::Listening:  return "Listening";
        case AgentState::Processing: return "Processing";
        case AgentState::Executing:  return "Executing";
        case AgentState::Error:      return "Error";
        default: return "Unknown";
    }
}

void IDEIntegrationAgent::executeCommand(const std::string& command) {
    changeState(AgentState::Processing);

    if (m_onOutput) {
        m_onOutput("Processing: " + command);
    }

    ParsedCommand parsed = parseCommand(command);
    executeAction(parsed);

    changeState(AgentState::Idle);
}

void IDEIntegrationAgent::processVoiceCommand(const std::string& transcribedText) {
    // Voice commands would match against a more natural grammar
    // For example: "Install GitHub Copilot" -> action=install-extension, params={ext=github-copilot}
    executeCommand(transcribedText);
}

void IDEIntegrationAgent::executeTask(const AgentTask& task) {
    changeState(AgentState::Executing);

    if (m_onOutput) {
        m_onOutput("Starting task: " + task.description);
    }

    const AgentTask& mutableTask = const_cast<AgentTask&>(task);

    for (size_t i = 0; i < mutableTask.steps.size(); ++i) {
        mutableTask.currentStep = i;

        if (m_onOutput) {
            m_onOutput("Step " + std::to_string(i + 1) + ": " + mutableTask.steps[i]);
        }

        // Execute step
        executeCommand(mutableTask.steps[i]);
    }

    mutableTask.completed = true;
    changeState(AgentState::Idle);
}

bool IDEIntegrationAgent::autonomouslyInstallExtension(const std::string& extensionId) {
    if (m_onOutput) {
        m_onOutput("Installing extension: " + extensionId);
    }

    // This would call into the VsixLoader from the IDE window
    // For now, simulated success
    return true;
}

bool IDEIntegrationAgent::autonomouslySwitchProvider(const std::string& provider) {
    if (m_onOutput) {
        m_onOutput("Switching AI provider to: " + provider);
    }

    // This would call into the ChatPanelIntegration from the IDE window
    // For now, simulated success
    return true;
}

bool IDEIntegrationAgent::autonomouslyAnalyzeFile(const std::string& filePath) {
    if (m_onOutput) {
        m_onOutput("Analyzing file: " + filePath);
    }

    // Read file and send to AI for analysis
    // Send to chat panel's AI provider
    // Return analysis results
    return true;
}

bool IDEIntegrationAgent::autonomouslyCreateFile(const std::string& fileName, 
                                                 const std::string& description) {
    if (m_onOutput) {
        m_onOutput("Creating file: " + fileName + " - " + description);
    }

    // Use AI to generate file content based on description
    // Save to disk
    // Open in editor
    return true;
}

CommandResult IDEIntegrationAgent::autonomouslyRunCommand(const std::string& command) {
    if (m_onOutput) {
        m_onOutput("Running: " + command);
    }

    CommandResult result;
    result.exitCode = 0;

    // Execute in terminal
    // Capture output
    result.output = "[Command output would appear here]";
    result.success = true;

    return result;
}

bool IDEIntegrationAgent::autonomouslyRefactorCode(const std::string& filePath,
                                                   const std::string& refactoringType) {
    if (m_onOutput) {
        m_onOutput("Refactoring " + filePath + " - type: " + refactoringType);
    }

    // Send file to AI with refactoring request
    // Apply changes
    // Save file
    return true;
}

bool IDEIntegrationAgent::autonomouslyGenerateTests(const std::string& filePath) {
    if (m_onOutput) {
        m_onOutput("Generating tests for: " + filePath);
    }

    // Send file to AI
    // Generate test code
    // Create test file
    return true;
}

void IDEIntegrationAgent::changeState(AgentState newState) {
    if (m_state != newState) {
        AgentState oldState = m_state;
        m_state = newState;

        if (m_onStateChange) {
            m_onStateChange(oldState, newState);
        }
    }
}

IDEIntegrationAgent::ParsedCommand IDEIntegrationAgent::parseCommand(const std::string& command) {
    ParsedCommand result;

    // Simple command parser - in production would use NLP
    std::istringstream iss(command);
    std::string word;

    // Extract action (first meaningful word or compound)
    if (iss >> word) {
        std::transform(word.begin(), word.end(), word.begin(), ::tolower);

        if (word == "install") {
            result.action = "install-extension";
            if (iss >> word) {
                result.parameters["extension"] = word;
            }
        } else if (word == "switch") {
            result.action = "switch-provider";
            if (iss >> word && word == "to") {
                if (iss >> word) {
                    result.parameters["provider"] = word;
                }
            }
        } else if (word == "analyze") {
            result.action = "analyze-file";
            if (iss >> word) {
                result.parameters["file"] = word;
            }
        } else if (word == "create") {
            result.action = "create-file";
            if (iss >> word) {
                result.parameters["name"] = word;
            }
        } else if (word == "run") {
            result.action = "run-command";
            std::string rest;
            std::getline(iss, rest);
            result.parameters["command"] = rest;
        } else if (word == "refactor") {
            result.action = "refactor-code";
        } else if (word == "test") {
            result.action = "generate-tests";
        } else {
            result.action = "unknown";
        }
    }

    return result;
}

void IDEIntegrationAgent::executeAction(const ParsedCommand& cmd) {
    if (cmd.action == "install-extension") {
        auto it = cmd.parameters.find("extension");
        if (it != cmd.parameters.end()) {
            autonomouslyInstallExtension(it->second);
        }
    } else if (cmd.action == "switch-provider") {
        auto it = cmd.parameters.find("provider");
        if (it != cmd.parameters.end()) {
            autonomouslySwitchProvider(it->second);
        }
    } else if (cmd.action == "analyze-file") {
        auto it = cmd.parameters.find("file");
        if (it != cmd.parameters.end()) {
            autonomouslyAnalyzeFile(it->second);
        }
    } else if (cmd.action == "create-file") {
        auto nameIt = cmd.parameters.find("name");
        if (nameIt != cmd.parameters.end()) {
            autonomouslyCreateFile(nameIt->second, "");
        }
    } else if (cmd.action == "run-command") {
        auto it = cmd.parameters.find("command");
        if (it != cmd.parameters.end()) {
            autonomouslyRunCommand(it->second);
        }
    } else if (cmd.action == "refactor-code") {
        // Refactor action implementation
    } else if (cmd.action == "generate-tests") {
        // Test generation action implementation
    } else {
        if (m_onError) {
            m_onError("Unknown action: " + cmd.action);
        }
    }
}

}  // namespace rawrxd::agent
